/*
 * Hub_flashlog.c
 *
 *  Created on: 6/10/2023
 *      Author: OEM
 */


#include "Hub_flashlog.h"

#include "ac300Enc_ssp.h"
#include "AC300_hub.h"

#ifdef MH_FLASH_RECORD

//----------------------------------------------------------------------------------------------------
/* This data structure to be held in MRAM, offset 512 as 256 used for plog. Could be 256 + 64??
 *  If accelerometer diags active then this structure would be created on hub initialisation.
 *  It would need to be updated as the flash data is written.
 *
 *  The data_ptr can have 2 components. First byte = wrap count, last 3 bytes = flash address offset
 *
 */

#define FLOG_MRAM_HEAD_OFFSET		256+64
#define FLOG_MRAM_HEAD_LEN			(sizeof(Flog_mram_head))
#define FLOG_MRAM_MAGIC				12345


#define FLOG_MRAM_INDEX_OFFSET		512
#define FLOG_MRAM_INDEX_SPACE		256
#define FLOG_MRAM_INDEX_LEN		(sizeof(Flog_index))

#define FLOG_SECTOR_SIZE			65536

bool Flog_diags_enabled;

typedef struct
{
	uint16_t magic;
	uint16_t ac200_run;
	uint16_t ac200_secs;
	uint16_t index_ptr;
	uint32_t data_ptr;
	uint32_t last_byte_erased;
} td_FLOG_MRAM_HEAD;
td_FLOG_MRAM_HEAD Flog_mram_head;

// This structure is used in in MRAM, bytes 512 to 512+256, one for each new run being recorded. Basically an index
// to the main data area.
// When the flash data is requested from the hub, the run will need to be specified.
typedef struct
{
	uint16_t ac200_run;
	uint16_t ac200_secs;			// Not needed, but so we can align structure on data boundaries. At 8 bytes per entry, max of 32 index entries.
	uint32_t data_ptr;
} td_FLOG_INDEX;
td_FLOG_INDEX Flog_index;

/*
 *  This data to be saved once a second. Data to be supplied by ac200. Recorded once every second from timer interrupt.
 *  Preceded by a value of 1. Internally, could just increment ac200 sec value once a second and only get from ac200 at start of flash record.
 */

typedef struct
{
	uint16_t ac200_run;
	uint16_t ac200_secs;
	uint16_t ac200_rpm;
} td_FLOG_FLASH_DATA_HD;
td_FLOG_FLASH_DATA_HD Flog_flash_data_head;

typedef struct
{
	uint16_t ac200_run;
	uint16_t ac200_secs;
	uint16_t ac200_rpm;
	int16_t adxl_z_axis;
} td_FLOG_FLASH_DATA_HD_ADXL375;
td_FLOG_FLASH_DATA_HD_ADXL375 Flog_flash_data_head_adxl375;

bool Flog_save_data_flag;
#ifdef MH_XXX
typedef struct
{
	uint32_t head_ptr;
	uint32_t tail_ptr;
	uint32_t overrun;
	uint8_t data[FLOG_DATA_SIZE];
} td_FLOG_FLASH_DATA;
#endif
td_FLOG_FLASH_DATA Flog_data;

//-------------------------------------------------------------------------------------------------
#define SSP_CMD_SECTOR_ERASE		0xD8
void SSP_Erase_Sector(uint32_t sector)
{
// The default settings for the S25FL127S are for 64K erase.
// Since the maximum number of programmable bytes is 16Mb, there is a maximum of 256 possible 64K sectors.
// sector 0 to Sector 255.

// Since we are providing a full byte address then the sector number is multiplied by 64K to give 3 byte address.

#define SIXTYFOUR_K		(64 * 1024)

	PRINTF("SSP_Erase_sector:%d\r\n",sector);

	SSP_Flash_Wait_Ready(SPI_FLASH_CS);
//	SSP_Flash_Write_Enable();
	SSP_Write_Enable(SPI_FLASH_CS);

	Tx_Buf[0] = SSP_CMD_SECTOR_ERASE;		// Sector Erase instruction
	uint32_t byte_address = sector * SIXTYFOUR_K;
	SSP_copy_address(byte_address);
//	uint32_t t1 = us_ticker_read();
	SSP_RW_Poll_Mode(SPI_FLASH_CS,4);
	SSP_flash_wait_ready = true;
/*
	for(;;)
	{
		uint8_t sr1 = SSP_Flash_Read_Status();
		if(sr1 == 0) break;	// Note: Could also look for error
		wait_ms(1);
	}
	uint32_t t2 = us_ticker_read();
	uint32_t elapsed_us = t2 - t1;
	printf("Erase time:%d (ms)\r\n",elapsed_us/1000);
//	ReturnToContinue();
 *
 */
}

// This allows access to sector zero
//This is trickier than the read as the write will page wrap
#define SSP_FLASH_PAGE_SIZE	256
uint32_t Hub_raw_flash_write_count;
uint32_t Hub_raw_flash_write_bytes;
void Hub_ssp_raw_flash_write(uint32_t byte_address,uint8_t *buff, uint32_t len)
{
	Hub_raw_flash_write_count++;
	Hub_raw_flash_write_bytes += len;

	uint32_t pos = byte_address;
	uint8_t *bp = buff;
	uint32_t bytes_left = len;

	while(bytes_left > 0)
	{
//		uint32_t offset = pos % SSP_FLASH_PAGE_SIZE;
		uint32_t offset = (pos & (SSP_FLASH_PAGE_SIZE-1));		// AS FLASH_PAGE_SIZE = 256
		uint32_t bytes_avail = SSP_FLASH_PAGE_SIZE - offset;
		uint32_t wlen = MIN(bytes_left,bytes_avail);
		pos &= 0xffffff;		// MHH:08/12/2023. Must fit in 16 Megabyte range
		SSP_WriteBuffer(SPI_FLASH_CS,pos,bp,wlen);
		bytes_left -= wlen;
		pos += wlen;
		bp += wlen;
	}
}

/*
 *  Flash bytes are coded as follows:
 *
 *   Value	Meaning
 *   255	Data has not been written to this byte (default state after erase).
 *   254	Data outside range. The Outside Range pin is active.
 *   1		Flash data head, see structure above
 *   2		Could be used for repeat of data. Eg 2 bytes following could contain repeat count. Leave for now.
 *
 */


/*
 *  So, the plan is that the AC200 will tell Hub to start recording by sending a "flash" data pkt with run, secs and rpm. If the rpm value is zero then that
 *  would signal stopping recording data.
 *
 *  Once the Hub receives a flash data pkt, it will initialise the flash data recording, but only start writing on a second boundary,
 *  so the first entry in the flash data should always be a header.
 *
 *  May have to fake the data packet for initial testing.
 *
 */

int Hub_flog_write_mram_head(void)
{
	uint8_t* p_buf = (uint8_t *)&Flog_mram_head;

	ac200Enc_ssp_raw_mram_write(SPI_MRAM_CS,FLOG_MRAM_HEAD_OFFSET,p_buf,FLOG_MRAM_HEAD_LEN,456);
	return 0;
}

void Hub_flog_reset_head(void)
{
	Hub_watchdog_active = false;		// MHH:26/12/2023
	uint32_t last_sector_erased = Flog_mram_head.data_ptr >> 16;		// 65536 bytes per sector
	last_sector_erased++;		// to be sure...
	PRINTF("Erasing all used sectors\r\n")
	SSP_Erase_Sector(0);		// Sector zero can take a long time to erase...
	wait_ms(3000);
	last_sector_erased = 255;		// MHH:26/12/2023
	for(int sector=1;sector<=last_sector_erased;sector++)
	{
		if(sector > 4)		// Arbitrary
		{
			Flog_getc_ptr = sector * 65536;		// MHH:26/12/2023
			int b = Flog_getc2();
			if(b == 255) break;
		}
		SSP_Erase_Sector(sector);
	}
	Flog_mram_head.magic = FLOG_MRAM_MAGIC;
	Flog_mram_head.ac200_run = Enc_data.ac200_run;
	Flog_mram_head.ac200_secs = 1;		// For now, once we get command from ac200 use that value
	Flog_mram_head.index_ptr = 0;
	Flog_mram_head.data_ptr = 0;
	Flog_mram_head.last_byte_erased = FLOG_SECTOR_SIZE - 1;
	Hub_flog_write_mram_head();
	Hub_watchdog_active = true;

//	SSP_Erase_Sector(0);
}
int Hub_flog_read_mram_head(void)
{
	uint8_t* p_buf = (uint8_t *)&Flog_mram_head;

	if(ac200Enc_ssp_mram_read(SPI_MRAM_CS,FLOG_MRAM_HEAD_OFFSET,p_buf, FLOG_MRAM_HEAD_LEN))
	{
		DebugAbort("Hub_flog_read_mram_head:ac200Enc_ssp_mram_read");
		return -1;
	}
	if(Flog_mram_head.magic != FLOG_MRAM_MAGIC)
	{
		L2PRINTF("Flog_magic not valid, creating new Flog_mram_head\r\n");
		Hub_flog_reset_head();

	}
	return 0;
}
/*
 * typedef struct
{
	uint16_t ac200_run;		// 0.
	uint16_t ac200_secs;	// 2
	uint16_t prop_rpm;		// 4
} AC200_v_data_td;			// 6.
AC200_v_data_td AC200_v_data;
 */
uint16_t V_ac200_secs;
uint16_t V_prop_rpm;
int Hub_flog_write_mram_index(void)
{
	Flog_index.ac200_run = Enc_data.ac200_run;
	Flog_index.ac200_secs = V_ac200_secs;		// For now, once we get command from ac200 use that value
	Flog_index.data_ptr = Flog_mram_head.data_ptr;
	uint8_t* p_buf = (uint8_t *)&Flog_index;

	uint32_t pos_offset = Flog_mram_head.index_ptr;
	pos_offset &= (FLOG_MRAM_INDEX_SPACE-1);		// Only works because power of 2
	uint32_t pos = FLOG_MRAM_INDEX_OFFSET + pos_offset;

	ac200Enc_ssp_raw_mram_write(SPI_MRAM_CS,pos,p_buf,FLOG_MRAM_INDEX_LEN,456);	// Update index

	Flog_mram_head.index_ptr += FLOG_MRAM_INDEX_LEN;
//	Flog_mram_head.index_ptr &= (FLOG_MRAM_INDEX_SPACE-1);		// Will wrap when > 256 bytes

	Hub_flog_write_mram_head();		// Update header
	return 0;
}
/*
 * // This structure is used in in MRAM, bytes 512 to 512+256, one for each new run being recorded. Basically an index
// to the main data area.
// When the flash data is requested from the hub, the run will need to be specified.
typedef struct
{
	uint16_t ac200_run;
	uint16_t ac200_secs;			// Not needed, but so we can align structure on data boundaries. At 8 bytes per entry, max of 32 index entries.
	uint32_t data_ptr;
} td_FLOG_INDEX;
td_FLOG_INDEX Flog_index;
 *
 */
void Hub_flog_list_mram_index(void)
{
	td_FLOG_INDEX flog_index;

	PRINTF("\r\nHub_flog_list_mram_index:\r\n");
	Hub_watchdog_active = false;

	uint32_t next_index_ptr = Flog_mram_head.index_ptr;		// points to first byte for next index
	uint32_t first_index_ptr = 0;
	if(next_index_ptr > FLOG_MRAM_INDEX_SPACE)	// Have we wrapped?
	{
		first_index_ptr = next_index_ptr - FLOG_MRAM_INDEX_SPACE;
	}
	uint32_t index_ptr;
	int inum=0;
	for(index_ptr=first_index_ptr;index_ptr<next_index_ptr;index_ptr+=FLOG_MRAM_INDEX_LEN)
	{
		uint8_t* p_buf = (uint8_t *)&flog_index;
		uint32_t pos_offset = index_ptr;
		pos_offset &= (FLOG_MRAM_INDEX_SPACE-1);		// Only works because power of 2
		uint32_t pos = FLOG_MRAM_INDEX_OFFSET + pos_offset;
		if(ac200Enc_ssp_mram_read(SPI_MRAM_CS,pos,p_buf, FLOG_MRAM_INDEX_LEN))	// Could have read 256 bytes into buffer too
		{
			DebugAbort("Hub_flog_list_mram_index:ac200Enc_ssp_mram_read");
			return;
		}
		inum++;
		PRINTF("%2d ac200_run:%d, ac200_secs:%3d, data_ptr:%d\r\n",inum,flog_index.ac200_run,flog_index.ac200_secs,flog_index.data_ptr);
	}
	PRINTF("\r\nFlog_mram_head.data_ptr:%d\r\n",Flog_mram_head.data_ptr);
	PRINTF("\r\nEnd of list\r\n");
}


bool Flog_outside_range_pin;		// Note: Could configure as an interrupt, then woul not need to read each time
extern volatile uint32_t Adc_accel_val;
uint32_t Flog_time_count;
uint32_t Flog_seconds;

uint32_t Flog_putc_count;
void Flog_putc(uint8_t b)
{
	Flog_putc_count++;
	if((Flog_data.head_ptr - Flog_data.tail_ptr) >= (FLOG_DATA_SIZE-1))	// Not sure about -1, but should not cause error
	{
		Flog_data.overrun++;
//		L2PRINTF("Flog_putc:OVR\r\n");
		return;
	}
	uint32_t head_offset = Flog_data.head_ptr++;
	head_offset &= (FLOG_DATA_SIZE-1);
	Flog_data.data[head_offset] = b;
}

#define FLOG_GETC_BUFFSIZE	64
uint8_t Flog_getc_buff[FLOG_GETC_BUFFSIZE];
uint32_t Flog_getc_ptr;
uint32_t Flog_getc_sector=-1;
#define FLOG_SPACE_MASK		0xffffff

int Flog_getc2(void)
{
	Flog_getc_ptr &= FLOG_SPACE_MASK;
	uint32_t sector = Flog_getc_ptr >> 6;		// /64
	uint32_t offset = Flog_getc_ptr & (FLOG_GETC_BUFFSIZE-1);
	if(sector != Flog_getc_sector)
	{
		Flog_getc_sector = sector;
		uint32_t sector_pos = sector << 6;	// * 64
		SSP_ReadBuffer(SPI_FLASH_CS,sector_pos,Flog_getc_buff,FLOG_GETC_BUFFSIZE);
	}
	uint8_t b = Flog_getc_buff[offset];
	Flog_getc_ptr++;
	return (int)b;
}

int Flog_getc(void)
{
	Flog_getc_ptr &= FLOG_SPACE_MASK;
	if(Flog_getc_ptr >= Flog_mram_head.data_ptr)
	{
		return -1;
	}
	return Flog_getc2();
}



/*
 * typedef struct
{
	uint16_t ac200_run;
	uint16_t ac200_secs;
	uint16_t ac200_rpm;
} td_FLOG_FLASH_DATA_HD;
td_FLOG_FLASH_DATA_HD Flog_flash_data_head;
 *
 */

// Note: Could use 0 if needed
#define FLOG_SEC_HEADER				1
#define FLOG_SEC_HEADER_ADXL375		2
#define FLOG_SEC_HEADER_12500		3

#define FLOG_PIN_OUTSIDE_RANGE		254
#define FLOG_OUTSIDE_RANGE_LOW		253
#define FLOG_OUTSIDE_RANGE_HIGH		252
//#define FLOG_OUTSIDE_RANGE_SOFT		251

uint16_t AC200_start_seconds;		// Need to get this from AC200
//uint16_t AC200_rpm = 5000;			// This too
uint16_t Flog_header_count;
#ifdef MH_ADXL1001
void Flog_output_header(void)
{
	Flog_flash_data_head.ac200_run = Enc_data.ac200_run;
	Flog_flash_data_head.ac200_secs = Flog_seconds + AC200_start_seconds;
	Flog_flash_data_head.ac200_rpm  = V_prop_rpm;
	uint8_t *p_head = (uint8_t *)&Flog_flash_data_head;
	uint32_t len = sizeof(Flog_flash_data_head);
	Flog_putc(FLOG_SEC_HEADER_12500);
	for(int i=0;i<len;i++)
	{
		uint8_t b = *p_head++;
		Flog_putc(b);
	}
}
#endif
#ifdef MH_ADXL375
void Flog_output_header_adxl375(void)
{
	Flog_flash_data_head_adxl375.ac200_run = Enc_data.ac200_run;
	Flog_flash_data_head_adxl375.ac200_secs = Flog_seconds + AC200_start_seconds;
	Flog_flash_data_head_adxl375.ac200_rpm  = V_prop_rpm;
	Flog_flash_data_head_adxl375.adxl_z_axis  = Adxl375_z_tot / Adxl375_z_count;		// Should be sample rate, eg 1600
	int x_second_avg = Adxl375_x_sec_tot / Adxl375_z_count;
	PRINTF("x=%d,xtot=%d\r\n",x_second_avg,Adxl375_x_sec_tot);
	PRINTF("z=%d,ztot=%d,zcount=%d\r\n",Flog_flash_data_head_adxl375.adxl_z_axis,Adxl375_z_tot,Adxl375_z_count);

	Adxl375_z_tot = 0;
	Adxl375_x_sec_tot = 0;
	Adxl375_z_count = 0;;

	uint8_t *p_head = (uint8_t *)&Flog_flash_data_head_adxl375;
	uint32_t len = sizeof(Flog_flash_data_head_adxl375);
	Flog_putc(FLOG_SEC_HEADER_ADXL375);
	for(int i=0;i<len;i++)
	{
		uint8_t b = *p_head++;
		Flog_putc(b);
	}
}
#endif

uint32_t Flog_outside_range_count;
uint16_t Adc_accel_max;
uint16_t Adc_accel_min=4096;
uint16_t Flog_outside_range_soft_low;
uint16_t Flog_outside_range_soft_high;

extern uint16_t Ctimer_accel_avg;


//#define ADC_ACCEL_LOW		(1024-128)
//#define ADC_ACCEL_HIGH		(1024+128)
#define ADC_ACCEL_LOW		(512-128)
#define ADC_ACCEL_HIGH		(512+128)

#define ADC_ACCEL_SHIFT		2			// Divide by 4
uint32_t Flog_put_ring_data_count;
uint32_t Flog_put_ring_data_count2;
void Flog_put_ring_data(void)
{
#ifdef MH_ADXL1001
	if(Flog_save_data_flag == false) return;

	Flog_put_ring_data_count++;

	if(Flog_time_count >= 12500)
	{
		Flog_time_count = 0;
		Flog_seconds++;
	}
	if(Flog_time_count++ == 0)
	{
		Flog_output_header();
	}
	uint8_t data_byte=0;
	if(Flog_outside_range_pin)
	{
		data_byte = FLOG_PIN_OUTSIDE_RANGE;	// Special code
		Flog_putc(data_byte);
		Flog_outside_range_count++;
	}
	else
	{
//		uint16_t adc_val = (uint16_t) Adc_accel_val;
		uint16_t adc_val = (uint16_t) Ctimer_accel_avg;
		if(adc_val > Adc_accel_max) Adc_accel_max = adc_val;
		if(adc_val < Adc_accel_min) Adc_accel_min = adc_val;

//		adc_val >>= 1;		// Divide by 2. Retain as much sensitivity as possible. 1g = about 16
		adc_val >>= ADC_ACCEL_SHIFT;		// Divide by 4. Retain as much sensitivity as possible. 1g = about 16

		uint8_t prefix_byte=0;	// If outside normal range, use prefix to extend allowable range

		if(adc_val < (ADC_ACCEL_LOW + 5))
		{
			Flog_outside_range_soft_low++;
			prefix_byte = FLOG_OUTSIDE_RANGE_LOW;
			adc_val += 200;
//			data_byte = 253;
		}
		if(adc_val > (ADC_ACCEL_HIGH -5))
		{
			Flog_outside_range_soft_high++;
			prefix_byte = FLOG_OUTSIDE_RANGE_HIGH;
			adc_val -= 200;
		}
		if(prefix_byte != 0)
		{
			Flog_putc(prefix_byte);
		}
		adc_val -= ADC_ACCEL_LOW;
		data_byte = (uint8_t)(adc_val & 0xff);
		Flog_putc(data_byte);
	}
#endif
}
#ifdef MH_ADXL375
void Flog_put_ring_data_adxl375(int adxl375_x_tot)
{
	if(Flog_save_data_flag == false) return;

	Flog_put_ring_data_count++;

	if(Flog_time_count >= 800)
	{
		Flog_time_count = 0;
		Flog_seconds++;
	}
	if(Flog_time_count++ == 0)
	{
		Flog_output_header_adxl375();
	}
	uint8_t data_byte=0;

	/*
	 * 	  int Adxl375_tot = -20;
	  int adxl375_avg = (Adxl375_tot >> 3);
	  printf("adxl375_avg = %d\r\n",adxl375_avg);
	 *
	 */
	//		uint16_t adc_val = (uint16_t) Adc_accel_val;
	int adxl375_avg;

	uint8_t shift_val = Adxl375_x_count_max >> 1;
	shift_val += 2;
	adxl375_avg = adxl375_x_tot >> shift_val;		// Divide


	/*
	 *  Sample_rate	Adxl375_x_tot_max	hift_val t	ot_shift_val		Divide by
	 *  	800			1					0				0+2		4
	 *  	1600		2					1				1+2		8
	 *  	3200		4					2				2+2		16
	 */

#ifdef MH_XXX
	if(Adxl375_sample_rate == 1600)
	{
		adxl375_avg = adxl375_x_tot >> 3;		// Divide by 8
	}
	else	// 3200
	{
		adxl375_avg = adxl375_x_tot >> 4;		// Divide by 16
	}
#endif
	if(adxl375_avg < -327 || adxl375_avg > 327)
	{
		data_byte = FLOG_PIN_OUTSIDE_RANGE;	// Special code
		Flog_putc(data_byte);
		Flog_outside_range_count++;
	}

	//		uint16_t adc_val = (uint16_t) Ctimer_accel_avg;

	uint16_t adc_val = (uint16_t) (512 + adxl375_avg);		// Get in same format as ADXL1001
	if(adc_val > Adc_accel_max) Adc_accel_max = adc_val;
	if(adc_val < Adc_accel_min) Adc_accel_min = adc_val;

	//		adc_val >>= 1;		// Divide by 2. Retain as much sensitivity as possible. 1g = about 16
	//		adc_val >>= ADC_ACCEL_SHIFT;		// Already shifted. Divide by 4. Retain as much sensitivity as possible. 1g = about 16

	uint8_t prefix_byte=0;	// If outside normal range, use prefix to extend allowable range

	if(adc_val < (ADC_ACCEL_LOW + 5))
	{
		Flog_outside_range_soft_low++;
		prefix_byte = FLOG_OUTSIDE_RANGE_LOW;
		adc_val += 200;
		//			data_byte = 253;
	}
	if(adc_val > (ADC_ACCEL_HIGH -5))
	{
		Flog_outside_range_soft_high++;
		prefix_byte = FLOG_OUTSIDE_RANGE_HIGH;
		adc_val -= 200;
	}
	if(prefix_byte != 0)
	{
		Flog_putc(prefix_byte);
	}
	adc_val -= ADC_ACCEL_LOW;
	data_byte = (uint8_t)(adc_val & 0xff);
	Flog_putc(data_byte);
}
#endif


int Flog_get_ring_data(void)
{
	if(Flog_data.tail_ptr == Flog_data.head_ptr)	// Anything in buffer?
	{
		return -1;	// No
	}
	uint32_t tail_offset = Flog_data.tail_ptr++;
	tail_offset &= (FLOG_DATA_SIZE-1);
	uint8_t data_byte = Flog_data.data[tail_offset];
	int rv = (int) data_byte;
	return rv;
}

int Flog_ring_data_count(void)
{
	int count = (Flog_data.head_ptr - Flog_data.tail_ptr);
	return count;
}
uint8_t Flog_data_out[FLOG_DATA_SIZE];
void Flog_write_ring_data()
{
#ifdef MH_ERASE_ON_FLY

	if(Flog_mram_head.last_byte_erased < (Flog_mram_head.data_ptr + 32768))	// Half a sector
	{
		Flog_mram_head.last_byte_erased += FLOG_SECTOR_SIZE;
		Hub_flog_write_mram_head();		// Update header
		uint32_t erase_pos = Flog_mram_head.last_byte_erased;
		erase_pos &= FLOG_SPACE_MASK;		// MHH:08/12/2023
		uint32_t sector = (erase_pos >> 16);	// Divide by sector size
		SSP_Erase_Sector(sector);
		return;			// May take a while, do something else
	}
#endif

//	if(Flog_ring_data_count() == 0) return;

//	int data_v;
//	uint32_t data_ix = 0;

	uint32_t head_ptr = Flog_data.head_ptr;
	uint32_t tail_ptr = Flog_data.tail_ptr;

	uint32_t head_offset = head_ptr & (FLOG_DATA_SIZE-1);
	uint32_t tail_offset = tail_ptr & (FLOG_DATA_SIZE-1);

	uint8_t *p_src = Flog_data.data + tail_offset;
	uint8_t *p_dst = Flog_data_out;
	uint32_t data_len;
	if(tail_offset < head_offset)
	{
		uint32_t len = head_offset - tail_offset;
		len &= (FLOG_DATA_SIZE-1);
		memcpy(p_dst,p_src,len);
//		Flog_data.tail_ptr += len;
		data_len = len;
	}
	else		// Must have a wrap
	{
		uint32_t len = FLOG_DATA_SIZE - tail_offset;
		memcpy(p_dst,p_src,len);
//		Flog_data.tail_ptr += len;
		p_dst += len;
		uint32_t len2 = head_offset;
		p_src = Flog_data.data;
		memcpy(p_dst,p_src,len2);
//		Flog_data.tail_ptr += len2;
		data_len = len + len2;
	}
	Flog_data.tail_ptr += data_len;


#ifdef MH_BYTE_BY_BYTE


	for(;data_ix<FLOG_DATA_SIZE;)
	{
		data_v = Flog_get_ring_data();
		if(data_v < 0) break;
		Flog_data_out[data_ix++] = (uint8_t)data_v;
	}
	uint32_t data_len = data_ix;
#endif
	uint32_t pos = Flog_mram_head.data_ptr;
	Flog_mram_head.data_ptr += data_len;
//	pos &= 0xffffff;		// Must fit in 16 Megabyte range
	pos &= FLOG_SPACE_MASK;		// MHH:08/12/2023. Must fit in 16 Megabyte range
	Hub_ssp_raw_flash_write(pos,Flog_data_out,data_len);
}



#define MH_FLASH_TEST
#ifdef MH_FLASH_TEST
#define SSP_FLASH_PAGE_SIZE	256
static uint8_t TestWriteBuffer[SSP_FLASH_PAGE_SIZE+2],TestReadBuffer[SSP_FLASH_PAGE_SIZE];
#ifdef MH_SSP_FLASH_TEST
static void SSP_WritePage(uint32_t page,uint32_t len, uint8_t *pBuffer)
{
	uint32_t byte_address = page * SSP_FLASH_PAGE_SIZE;
	SSP_WriteBuffer(SPI_FLASH_CS,byte_address,TestWriteBuffer,80);
}

void SSP_Flash_Test(void)
{
	printf("SSP_Flash_Test:\n\r");
	printf("Writing at %d byte pages, starting from position %d\r\n",SSP_FLASH_PAGE_SIZE,SIXTYFOUR_K);

	uint32_t n=1;
	uint32_t t1 = us_ticker_read();
	for(int pg=256;pg<512;pg++)
	{
		char *bp = (char *)TestWriteBuffer;
		for(int i=0;i<32;i++)
		{
			sprintf(bp,"%8d",n);
			bp+= 8;
			n++;
		}
		SSP_WritePage(pg,256,TestWriteBuffer);
		if(pg%16 == 0) printf("\r\n");
		printf("%6d",pg);
	}
	uint32_t t2 = us_ticker_read();
	uint32_t elapsed_us = t2 - t1;
	printf("\r\nFinished writing 256 pages, elapsed = %d (ms)\r\n",elapsed_us/1000);
	printf("Now checking\r\n");

	n = 1;
	for(int pg=256;pg<512;pg++)
	{
		char *bp = (char *)TestWriteBuffer;
		for(int i=0;i<32;i++)
		{
			sprintf(bp,"%8d",n);
			bp+= 8;
			n++;
		}
		SSP_ReadPage(pg,256,TestReadBuffer);
		if(pg%16 == 0) printf("\r\n");
		printf("%6d",pg);
		if(memcmp(TestReadBuffer,TestWriteBuffer,256))
		{
			printf("\r\nPage:%d different\r\n",pg);
		}
	}
	uint32_t t3 = us_ticker_read();
	elapsed_us = t3 - t2;
	printf("\r\nFinished reading 256 pages, elapsed = %d (ms)\r\n",elapsed_us/1000);
}
#endif
#endif

//------------------------------------------------------------------------------
//#define TEST_LOOPS	4
#define TEST_LOOPS	819
/*
 * MHH:16/12/2023
 *
 * With MCU at 30 Mhz, time for 65520 bytes (819 * 80):
 *
 * Write = 2052 millisecs, = 32 bytes/millisec
 * Read = 741 millisecs, = 88 bytes/millisec
 *
 * So, maximum write rate for flash memory is 32,000 bytes/sec
 *     maximum read rate for flash memory is 88,000 bytes/sec
 *
 */


void SSP_Flash_Test2(void)
{
	PRINTF("SSP_Flash_Test2:\n\r");
	PRINTF("Writing 80 byte pages, starting from position %d\r\n",SIXTYFOUR_K);

	uint32_t n=1;
	uint32_t t1 = us_ticker_read();
	uint32_t pos = SIXTYFOUR_K;
//	for(int r=0;r<819;r++)


	char *bp = (char *)TestWriteBuffer;
	for(int i=0;i<10;i++)
	{
		sprintf(bp,"%8d",n);
		bp+= 8;
		n++;
	}


	for(int r=0;r<TEST_LOOPS;r++)
	{
//		wait_ms(100);
//		SSP_Flash_WriteBuffer(SPI_FLASH_CS,pos,TestWriteBuffer,80);
		Hub_ssp_raw_flash_write(pos,TestWriteBuffer,80);
//		SSP_WriteBuffer(SPI_FLASH_CS,pos,TestWriteBuffer,80);
//		AC210_ssp_flash_write(SPI_FLASH_CS,pos,TestWriteBuffer,80);
//		SSP_WriteBuffer(pos,TestWriteBuffer,80);
#ifdef MH_YYY
		if(r%20 == 0) PRINTF("\r\n");
		PRINTF("%6d",r);
#endif
		pos += 80;
	}
	uint32_t t2 = us_ticker_read();
	uint32_t elapsed_us = t2 - t1;
	PRINTF("\r\nFinished writing 256 pages, elapsed = %d (ms)\r\n",elapsed_us/1000);
	PRINTF("Now checking\r\n");

	n = 1;
	pos = SIXTYFOUR_K;

//	wait_ms(100);

//	for(int r=0;r<819;r++)
	for(int r=0;r<TEST_LOOPS;r++)
	{
#ifdef MH_YYY
		char *bp = (char *)TestWriteBuffer;
		for(int i=0;i<10;i++)
		{
			sprintf(bp,"%8d",n);
			bp+= 8;
			n++;
		}
#endif
		SSP_ReadBuffer(SPI_FLASH_CS,pos,TestReadBuffer,80);
//		AC210_ssp_flash_read(pos,TestReadBuffer,80);
//		SSP_ReadBuffer(pos,TestReadBuffer,80);
#ifdef MH_YYY
		if(r%20 == 0) PRINTF("\r\n");
		PRINTF("%6d",r);
		if(memcmp(TestReadBuffer,TestWriteBuffer,80))
		{
			PRINTF("\r\nBuffer:%d different\r\n",r);
		}
#endif
		pos += 80;
	}
	uint32_t t3 = us_ticker_read();
	elapsed_us = t3 - t2;
	PRINTF("\r\nFinished reading 256 pages, elapsed = %d (ms)\r\n",elapsed_us/1000);
	PRINTF_FLUSH;
}
//--------------------------------------------------------------------------------------------------------------------
void Hub_flog_init(void)
{
	Hub_flog_read_mram_head();
}
void Hub_flog_reset(void)
{
	PRINTF("Hub_flog_reset\r\n");
	Hub_flog_reset_head();

}

bool Hub_flog_check_data_ptr(void)	// MHH:15/12/2023. Appears that data area may not be erased. Check it...
{
	Flog_getc_ptr = Flog_mram_head.data_ptr;
	int b;
	for(int i=0;i<16;i++)	// check first 16 bytes are erased
	{
		b = Flog_getc2();
		if(b != 255)
		{
			L2PRINTF("Hub_flog_check_data_ptr:Data NOT erased, ptr=%d\r\n",Flog_getc_ptr);
			return true;
		}
	}
	return false;
}


void Hub_flog_start_record(void)
{
#ifdef MH_FLASH_RECORD
	if(Hub_flog_check_data_ptr())
	{
		return;	// So we don't overwrite anything, and can look at data
	}

	if(Enc_data.ac200_run == 0)	Enc_data.ac200_run = 123;		// Test only
	AC200_start_seconds = V_ac200_secs;
	Hub_flog_write_mram_index();
	Flog_time_count = 0;
	Flog_save_data_flag = true;

	Timer_interrupt_init();

#ifdef MH_ADXL375
	Adxl375_start_measure();
#endif
#endif
}
// This routine to be called whenever time at low priority
// Note: we should consider updating at least data_ptr regularly...
uint16_t Flog_update_seconds = 10;
uint16_t Flog_display_secs;
int Hub_flog_check_ring_buffer(void)
{
#ifdef MH_FLASH_RECORD
	if(Flog_save_data_flag == false) return 0;

	if(Flog_put_ring_data_count >= 12500)
	{
		Flog_put_ring_data_count2 += Flog_put_ring_data_count;
		Flog_put_ring_data_count = 0;
//		PRINTF("Flog_count:%d\r\n",Flog_put_ring_data_count2);
	}

	if((Flog_data.head_ptr - Flog_data.tail_ptr) != 0)
//	if(Flog_data.head_ptr != 0)
	{
		Flog_write_ring_data();
	}

	if(Flog_seconds != Flog_display_secs)
	{
		Flog_display_secs = Flog_seconds;
//		PRINTF("AC200_rpm:%d\r\n",V_prop_rpm);
		if((Flog_seconds - Flog_update_seconds) > 10)	// Update data_ptr every 10 seconds
		{
			Flog_update_seconds = Flog_seconds;
			// Note: Could just update data_ptr here
			Hub_flog_write_mram_head();			// Make sure we have latest value for data_ptr
		}
	}
	return (Flog_data.head_ptr - Flog_data.tail_ptr);
#endif
}
// This will be called when receive a pkt from AC200 saying rpm = 0, or from test logic
void Hub_flog_end_record(void)
{
	Flog_save_data_flag = false;		// Disable recording
	Timer_interrupt_disable();
#ifdef MH_ADXL375
	Adxl375_stop_measure();
#endif
	Flog_write_ring_data();			// Flush ring buffer
	Hub_flog_write_mram_head();			// Make sure we have latest value for data_ptr
}
/*
 *typedef struct
{
	uint16_t magic;
	uint16_t ac200_run;
	uint16_t ac200_secs;
	uint16_t index_ptr;
	uint32_t data_ptr;
	uint32_t last_byte_erased;
} td_FLOG_MRAM_HEAD;
td_FLOG_MRAM_HEAD Flog_mram_head;
*
 */
extern uint32_t Ctimer_count;
extern uint32_t Adc_count;
void Hub_flog_display(void)
{
	PRINTF("Hub_flog_display\r\n");
	PRINTF("ac200_run:%d\r\n",Flog_mram_head.ac200_run);
	PRINTF("ac200_secs:%d\r\n",Flog_mram_head.ac200_secs);
	PRINTF("index_ptr :%d\r\n",Flog_mram_head.index_ptr);
	PRINTF("data_ptr  :%d\r\n",Flog_mram_head.data_ptr);
	PRINTF("erased_ptr:%d\r\n",Flog_mram_head.last_byte_erased);
	PRINTF("\r\n");
	PRINTF("Flog_putc_count:%d\r\n",Flog_putc_count);
	PRINTF("Flog_seconds   :%d\r\n",Flog_seconds);

	PRINTF("Ctimer_count   :%d\r\n",Ctimer_count);
	PRINTF("Adc_count      :%d\r\n",Adc_count);
	PRINTF("Adc_accel_max  :%d\r\n",Adc_accel_max);
	PRINTF("Adc_accel_min  :%d\r\n",Adc_accel_min);
	PRINTF("Ring overrun   :%d\r\n",Flog_data.overrun);
	PRINTF("Outside_range_Pin :%d\r\n",Flog_outside_range_count);
	PRINTF("Outside_range_Low :%d\r\n",Flog_outside_range_soft_low);
	PRINTF("Outside_range_High:%d\r\n",Flog_outside_range_soft_high);

	PRINTF("Raw_flash_write_count:%d\r\n",Hub_raw_flash_write_count);
	PRINTF("Raw_flash_write_bytes:%d\r\n",Hub_raw_flash_write_bytes);
}
void Hub_flog_display_data(void)
{
	PRINTF("Hub_flog_display_data:\r\n");
	Flog_getc_ptr = 0;
	int b;
	for(;;)
	{
		b = Flog_getc();
		if(b < 0) break;
		PRINTF("%5d:%3d\r\n",Flog_getc_ptr,b);
	}
}

// A copy of Hub_flog_list_mram_index, modified to scan

td_FLOG_INDEX Flog_scan_index;
td_FLOG_INDEX Flog_scan_index2;


uint32_t Hub_flog_get_pos(uint32_t index_ptr)
{
	uint32_t pos_offset = index_ptr;
	pos_offset &= (FLOG_MRAM_INDEX_SPACE-1);		// Only works because power of 2
	uint32_t pos = FLOG_MRAM_INDEX_OFFSET + pos_offset;
	return pos;

}
bool Hub_flog_scan_mram_index(int scan_number)
{

	uint32_t next_index_ptr = Flog_mram_head.index_ptr;		// points to first byte for next index
	uint32_t first_index_ptr = 0;
	if(next_index_ptr > FLOG_MRAM_INDEX_SPACE)	// Have we wrapped?
	{
		first_index_ptr = next_index_ptr - FLOG_MRAM_INDEX_SPACE;
	}
	uint32_t index_ptr;
	int inum=0;
	uint8_t* p_buf = (uint8_t *)&Flog_scan_index;
	uint32_t pos;
	bool found_entry = false;
	for(index_ptr=first_index_ptr;index_ptr<next_index_ptr;index_ptr+=FLOG_MRAM_INDEX_LEN)
	{
		pos = Hub_flog_get_pos(index_ptr);
		if(ac200Enc_ssp_mram_read(SPI_MRAM_CS,pos,p_buf, FLOG_MRAM_INDEX_LEN))	// Could have read 256 bytes into buffer too
		{
			DebugAbort("Hub_flog_scan_mram_index:ac200Enc_ssp_mram_read");
			return false;
		}
		inum++;
		if(inum == scan_number)
		{
			found_entry = true;
			break;
		}
	}
	if(found_entry == false) return false;

// At this stage we have found the index entry for scan_number, but we also need to know how long the file is, so look at next entry

	if(index_ptr == Flog_mram_head.index_ptr - FLOG_MRAM_INDEX_LEN)	// Last entry?
	{
		Flog_scan_index2.data_ptr = Flog_mram_head.data_ptr;		// Then last byte = eof
	}
	else
	{
		index_ptr+=FLOG_MRAM_INDEX_LEN;
		p_buf = (uint8_t *)&Flog_scan_index2;
		pos = Hub_flog_get_pos(index_ptr);
		if(ac200Enc_ssp_mram_read(SPI_MRAM_CS,pos,p_buf, FLOG_MRAM_INDEX_LEN))	// Could have read 256 bytes into buffer too
		{
			DebugAbort("Hub_flog_scan_mram_index:ac200Enc_ssp_mram_read(2)");
			return false;
		}
	}
	return true;
}



void Hub_flog_send_binary(void)
{
//	PRINTF("Hub_send:\r\n");
	Hub_watchdog_active = false;
#ifndef MH_YYY		// debug!!
	char buff[10];
	int ix=0;
	int c;

	for(int i=0;i<10;i++)
	{
		c = Board_UARTGetChar_wait_RB(20);
		if(c >= 0)
		{
			buff[ix++] = (char)c;
			if(c == 10) break;
			if(ix >= 10) break;
		}
	}


// Expect string to identify which index to use, in format "=nn\r\n"

	if(buff[0] != '=')
	{
		PRINTF("ERR=1\r\n");
		return;
	}
	int val=0;
	for(int i=1;i<ix;i++)
	{
		c = buff[i];
		if(c == 10 || c == 15) break;
		if(c <'0' || c>'9')
		{
			PRINTF("ERR=2\r\n");
			return;
		}
		c -= '0';
		val = val * 10 + c;
	}

#else
	int val = 3;
#endif


	if(Hub_flog_scan_mram_index(val) == false)
	{
		PRINTF("ERR=3\r\n");
		return;

	}

	// At this point we should have the index details saved in Flog_scan_index structure, and the start of the next file in index2

	Flog_getc_ptr = Flog_scan_index.data_ptr;
	uint32_t next_data_ptr = Flog_scan_index2.data_ptr;
	uint32_t file_size = next_data_ptr - Flog_getc_ptr;
	PRINTF("File starts at position:%d, length:%d\r\n",Flog_getc_ptr,file_size);
	Dputc('{');
//	Flog_getc_ptr = 0;
	int b;
	uint32_t file_pos;
	Flog_getc_sector = -1;	// MHH:16/12/2023
	for(file_pos=0;file_pos<file_size;file_pos++)
	{
		b = Flog_getc();
		if(b < 0) break;
//	PRINTF("%5d:%3d\r\n",Flog_getc_ptr,b);
		Dputc((char)b);
	}
	Dputc('}');	// EOF
}
/*
 * typedef struct
{
	uint16_t ac200_run;
	uint16_t ac200_secs;
	uint16_t ac200_rpm;
} td_FLOG_FLASH_DATA_HD;
td_FLOG_FLASH_DATA_HD Flog_flash_data_head;

 *
 */
td_FLOG_FLASH_DATA_HD Header;

uint16_t Flash_ReadUInt16(void)
{
	uint8_t b0 = Flog_getc();
	uint8_t b1 = Flog_getc();
	uint16_t rv = b0 | (b1 << 8);
	return rv;
}
void Flash_load_second_header()
{
	Header.ac200_run = Flash_ReadUInt16();
	Header.ac200_secs = Flash_ReadUInt16();
	Header.ac200_rpm = Flash_ReadUInt16();
}

void Hub_flog_verify(void)
{
	PRINTF("Hub_flog_verify:\r\n");
	Hub_watchdog_active = false;
	Flog_getc_ptr = 0;
	Flog_getc_sector = -1;
	int b;
	int header_count=0;
	int data_count=0;
	int byte_start_count=0;
	int OR_count=0;
	int LO_count = 0;
	int HI_count = 0;
	int byte_count=0;
	for(;;)
	{
		b = Flog_getc();
		if(b < 0) break;
		//	PRINTF("%5d:%3d\r\n",Flog_getc_ptr,b);

		// First deal with usual case...
		if (b > 5 && b < 251)
		{
			data_count++;
			continue;
		}
		// If we drop through then it is a special case
		switch (b)
		{
		default:
			PRINTF("Invalid byte:%d at pos:%d\r\n",b,Flog_getc_ptr);
			return;

		case 1:
		case 3:
			byte_count = Flog_getc_ptr - byte_start_count - 1;
			Flash_load_second_header();
			header_count++;
			PRINTF("%3d Header:ac200_run=%d,ac200_secs=%3d,ac200_rpm=%d,",header_count,Header.ac200_run,Header.ac200_secs,Header.ac200_rpm);
			PRINTF("data_bytes:%d,byte_count:%d,OR=%d,LO=%d,HI=%d\r\n",data_count,byte_count,OR_count,LO_count,HI_count);
			data_count = 0;
			byte_start_count = Flog_getc_ptr;
			OR_count = 0;
			LO_count = 0;
			HI_count = 0;
			break;
		case 254:   // We have an outside range error
			OR_count++;
			break;
		case 253: // Have a low value outside normal range
			b = Flog_getc();
			LO_count++;
			break;
		case 252: // Have a high value outside normal range
			b = Flog_getc();
			HI_count++;
			break;
		}
	}

	PRINTF("\r\ntotal_bytes:%d\r\n",Flog_getc_ptr);
}
#endif
//#endif
