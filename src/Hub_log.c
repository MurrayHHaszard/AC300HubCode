/*
 * Hub_log.c
 *
 *  Created on: 5/08/2023
 *      Author: MHH
 */


#include "ac300Enc_ssp.h"
#include "AC300_hub.h"

/*
 *  The idea is to use pages 1 to 15 of the MRAM to hold a log of events, which can be requested by AC200Diagnostics.
 *
 *  Data will be appended on a ring basis, so when page 15 is filled page 1 will be filled next.
 *
 *  Data to be in readable form, probably added by a routine such as LPRINTF (Log printf), similar to PRINTF but with output directed
 *  to the log file, instead of to a serial port. We could also use routines such as LPUTS. We could even use the PRINTF buffer, copying
 *  to the log file until a null character is found.
 *
 *  EOF could be represented visually, with something like \r\n[eof]\r\n
 *  Alternatively, we could use a special character, such as 255, 254 or null.
 *
 *  Data could by copied using a similar function to putc(c), so that when a buffer is filled it is written, but the buffer should always
 *  be flushed when the terminating null is found.
 *
 *  On initialisation, the pointer to the next byte to be written (Enc_data.log_pos) will be validated.
 *
 *
 */
#define PLOG_DATA_OFFSET		1024
#define PLOG_HEAD_OFFSET		256		// May need to change
#define PLOG_HEAD_LEN			12			// We do not want to overwrite enabled
#define PLOG_SIZE				(1024*8)		// Max is 15*1024

#define PLOG_HEAD_MAGIC		123456789

const uint32_t UINT32_HIVAL=0xffffffff;

//----------------------------------------------------------------------------------------------------
typedef struct
{
	uint32_t magic;
	uint32_t data_ptr;
	uint32_t data_check;	// Idea is data_check + data_ptr = 0
	bool enabled;
} td_PLOG_HEAD;
td_PLOG_HEAD Plog_head;

//


int Hub_plog_read_head(void)
{
	uint8_t* p_buf = (uint8_t *)&Plog_head;

	if(ac200Enc_ssp_mram_read(SPI_MRAM_CS,PLOG_HEAD_OFFSET,p_buf, PLOG_HEAD_LEN))
	{
		DebugAbort("Hub_plog_read_head:ac200Enc_ssp_mram_read");
		return -1;
	}
	return 0;
}
int Hub_plog_write_head(void)
{
#ifdef MH_XXX
	if(Plog_head.magic != PLOG_HEAD_MAGIC)	// Corrupt?
	{
		Plog_head.enabled = false;
		return -1;
	}
#endif
	/*
	 *  MHH:05/09/20232. Even if corrupt may be better to fix and save data.
	 */


	Plog_head.magic = PLOG_HEAD_MAGIC;
	uint8_t* p_buf = (uint8_t *)&Plog_head;
	Plog_head.data_check = Plog_head.data_ptr ^ UINT32_HIVAL;	// Idea is pos + check = -1

	ac200Enc_ssp_raw_mram_write(SPI_MRAM_CS,PLOG_HEAD_OFFSET,p_buf,PLOG_HEAD_LEN,456);
	return 0;
}

int Hub_plog_write_data(uint32_t data_pos,uint8_t *pdata,uint32_t len)
{
	ac200Enc_ssp_raw_mram_write(SPI_MRAM_CS,PLOG_DATA_OFFSET + data_pos,pdata,len,123);	// Update log
	return 0;
}

int Hub_plog_new_head(void)
{
	Plog_head.magic = PLOG_HEAD_MAGIC;
	Plog_head.data_ptr = 0;
//	Plog_head.data_check = -1;
	return Hub_plog_write_head();
}
void Hub_plog_puts2(const char *s)
{
//	if(Plog_head.enabled == false) return;

	int s_len = strlen(s);

	uint32_t data_check_val = (Plog_head.data_ptr ^ Plog_head.data_check);
	if((data_check_val) != UINT32_HIVAL) return;	// Corrupt!!

	uint32_t data_offset = Plog_head.data_ptr % PLOG_SIZE;	// ptr does not wrap, so find offset
	uint32_t data_len = s_len;
	uint8_t *byte_address = (uint8_t *)s;

	int end_p = data_offset + s_len - 1;
	if(end_p >= PLOG_SIZE)		// handle possible wrap
	{
		end_p = PLOG_SIZE - 1;
		data_len = PLOG_SIZE - data_offset;
	}
	if(Hub_plog_write_data(data_offset,byte_address,data_len))	// Update log
	{
		Plog_head.enabled = false;
		return;		// error
	}

	if(data_len < s_len)		// wrap?
	{
		data_offset = 0;
		byte_address += data_len;
		data_len = s_len - data_len;		// Write remaining bytes
		if(Hub_plog_write_data(data_offset,byte_address,data_len))	// Update log
		{
			Plog_head.enabled = false;
			return;		// error
		}
	}
	Plog_head.data_ptr += s_len;
//	Plog_head.data_check = Plog_head.data_ptr ^ UINT32_HIVAL;	// Idea is pos + check = -1
	Hub_plog_write_head();
}

static uint8_t Plog_count;
extern volatile int Systick_secs;



void Hub_plog_puts_common(int ifrom,const char *s)
{
	if(Plog_head.enabled == false) return;

	char plog_line[160];

//	uint32_t tsecs = TimeInSeconds;
	uint32_t tsecs = Systick_secs;

	int ss = tsecs % 60;
	int tmins = tsecs / 60;
	int mm = tmins % 60;
	int hh = tmins / 60;

	char *p_line = plog_line;

	if(Plog_count == 0)
	{
		Plog_count++;
		*p_line++ = '\r';
		*p_line++ = '\n';
	}
	sprintf(p_line,"%02d:%02d:%02d %s",hh,mm,ss,s);	// Need p_Bufferlen in case ifrom == 3
	Hub_plog_puts2(plog_line);
	if(ifrom == 3)
	{
		Board_UART1PutSTR(plog_line);
	}
}

void Hub_plog_puts(const char *s)
{
	Hub_plog_puts_common(0,s);
}

void Hub_plog_puts3(const char *s)
{
	Hub_plog_puts_common(3,s);
}
void Hub_plog_init(void)
{
	Plog_head.enabled = false;	// Turn print logging off.

	if(Hub_plog_read_head() != 0) return;

	uint32_t data_check_val = (Plog_head.data_ptr ^ Plog_head.data_check);

	bool data_check = (data_check_val == UINT32_HIVAL);
	bool magic_check = (Plog_head.magic == PLOG_HEAD_MAGIC);
	if(data_check == false && magic_check == false)	// Assume first time
	{
		if(Hub_plog_new_head() != 0) return;
		Plog_head.enabled = true;
		L2PRINTF("Hub_plog_init:New hub log\r\n");
	}
	else
	{
		Plog_head.enabled = true;
		if(magic_check == false)
		{
			L2PRINTF("Hub_plog_init:Magic check fail\r\n");
		}
		if(data_check == false)
		{
			L2PRINTF("Hub_plog_init:Data check fail\r\n");
		}
	}
}
void Dputc(const char c);
#ifdef MH_XXX
void Hub_log_display(void)
{
	int c;
	int imax = Hub_log_putc_pos;	// This doesn't take into account wrap, just for test.
	for(int i=0;i<imax;i++)	// Could add a limit
	{
		c = Hub_log_getc();
		if(c <= 0 || c >= 128) break;
		Dputc(c);
	}
}
#endif
#define PLOG_PAGESIZE	128
uint8_t Plog_buffer[PLOG_PAGESIZE];
int Plog_getc_pos;
int Plog_page = -1;	// Force read on page 0

int Plog_getpage(int page)
{
	Plog_page = page;
	uint32_t page_pos = PLOG_DATA_OFFSET + (Plog_page * PLOG_PAGESIZE);

	uint8_t* p_buf = (uint8_t *)&Plog_buffer;

	if(ac200Enc_ssp_mram_read(SPI_MRAM_CS,page_pos,p_buf, PLOG_PAGESIZE))
	{
		DebugAbort("Hub_plog_getpage:ac200Enc_ssp_mram_read");
		return -1;
	}
	return 0;
}
int Plog_getc(void)
{
	if(Plog_getc_pos >= PLOG_SIZE) Plog_getc_pos = 0;	// Handle wrap
	if(Plog_getc_pos < 0) Plog_getc_pos = PLOG_SIZE  + Plog_getc_pos;	// How can it be negative?

	int page = Plog_getc_pos / PLOG_PAGESIZE;
	int offset = Plog_getc_pos % PLOG_PAGESIZE;
	int rv;
	if(page != Plog_page)
	{
		rv = Plog_getpage(page);
		if(rv != 0) return rv;
	}
	rv = Plog_buffer[offset];
	Plog_getc_pos++;
	return rv;
}

void Hub_return_hub_data(void)
{
	SIG100_PRINTF("\r\nHub Enc_data:\r\n");

	SIG100_PRINTF("Enc_data.run_flags      : %d\r\n",Enc_data.run_flags);
	SIG100_PRINTF("Enc_data.error_count    : %d\r\n",Enc_data.error_count);
	SIG100_PRINTF("Enc_data.error_type     : %d (%c)\r\n",Enc_data.error_type,Enc_data.error_type);
	uint8_t command = Enc_data.error_command;
	if(command < 32 || command > 125) command = '?';	// Because a null caused loss of CRNL
	SIG100_PRINTF("Enc_data.error_command  : %d (%c)\r\n",Enc_data.error_command,command);
	SIG100_PRINTF("Enc_data.error_number   : %d\r\n",Enc_data.error_number);
	SIG100_PRINTF("Enc_data.error_ac200_run: %d\r\n",Enc_data.error_ac200_run);
	SIG100_PRINTF("Enc_data.error_hub_run  : %d\r\n",Enc_data.error_hub_run);
	SIG100_PRINTF("Enc_data.error_data     : %d\r\n",Enc_data.error_data);
	SIG100_PRINTF("Enc_data.run_set_from   : %d\r\n",Enc_data.run_set_from);	// MHH:05/07/2025
	//		PRINTF("Enc_ready_count: %d\r\n",Enc_ready_count);
	SIG100_PRINTF("\r\n");
	//		PRINTF("Enc_posx:        %d\r\n",Enc_posx);
	SIG100_PRINTF("Enc_data.coarse_stop    : %d\r\n",Enc_data.cal_coarse_stop);
	SIG100_PRINTF("Enc_data.fine_stop(1)   : %d\r\n",Enc_data.cal_fine_stop);
	SIG100_PRINTF("Enc_data.fine_stop(2)   : %d\r\n",Enc_data.cal_fine_stop2);
	SIG100_PRINTF("Enc_data.fine_hard_stop : %d\r\n",Enc_data.cal_fine_hard_stop);

	if(Enc_data.user_defined_stops & 4)
	{
		SIG100_PRINTF("Enc_data.ud_feather_stop : %d\r\n",Enc_data.ud_feather_stop);
	}
	if(Enc_data.user_defined_stops & 8)
	{
		SIG100_PRINTF("Enc_data.ud_reverse_stop : %d\r\n",Enc_data.ud_reverse_stop);
	}
	SIG100_PRINTF("\r\n");
	SIG100_PRINTF("Enc_pos      : %d\r\n",Enc_Pos.pos);
	SIG100_PRINTF("AC200 current: %d\r\n",AC200_current);
	int ready_pin = Board_pin_read(READY_PIN);
	SIG100_PRINTF("Enc_ready_pin: %d",ready_pin);
	if(ready_pin == 0)
	{
		SIG100_PRINTF(" - NOT READY");
	}
	SIG100_PRINTF("\r\n");
	if(Enc_fine_microswitch_on)
	{
		SIG100_PRINTF("Enc_fine_microswitch ON\r\n");
	}
	if(Enc_coarse_microswitch_on)
	{
		SIG100_PRINTF("Enc_coarse_microswitch ON\r\n");
	}
}
int Hub_plog_debug;
void Hub_plog_display(int val)
{
	int c;
	if(val == 123)
	{
//		Dputs("Resetting plog file\r\n");
		Hub_plog_new_head();
		return;
	}

	Hub_plog_debug=1;

	Hub_watchdog_active = false;		// MHH:06/09/2023. This could take a while...

	if(val == 1)
	{
		Sig100_X_puts("{start}\r\n");
//		PRINTF("{start}\r\n");			// Debug
	}
	int plog_size;
	if(Plog_head.data_ptr < PLOG_SIZE)
	{
		Plog_getc_pos = 0;
		plog_size = Plog_head.data_ptr;
	}
	else
	{
		Plog_getc_pos = Plog_head.data_ptr % PLOG_SIZE;
		plog_size = PLOG_SIZE;
	}

//	uint32_t imax = Plog_head.data_ptr % EE_PLOG_SIZE;	// This doesn't take into account wrap, just for test.
	for(int i=0;i<plog_size;i++)
	{
		c = Plog_getc();
//		if(c <= 0 || c >= 128) break;
		if(c < 0) break;
#ifdef MH_XXX
		if(c > 125)	// MHH:06/09/2023
		{
			c = '?';
		}
		else
		{
			if(c < 32)
			{
				if(c != 10 && c != 13) c = '?';
			}
		}
#endif
		if(val == 1)
		{

#ifdef MH_FLUSH_WAIT	// MHH:05/12/2023. Have added logic to

			if(c == 10)
			{
//				Dputs("<10>");
				Sig100_flush_and_wait(10);
			}
#endif
//				wait_ms(10);	// SIG100 has a problem if too many consecutive bytes sent in a row.
//			Dputc(c);		// Debug
			Sig100_X_putc((char)c);
		}
		else
		{
			Dputc(c);
		}
	}
	if(val == 1)
	{
		Hub_return_hub_data();
		Sig100_X_puts("{end}\r\n");
		Sig100_flush_and_wait(10);

		//		PRINTF("{end}\r\n");		// Debug
	}

//	Hub_watchdog_active = true;	// Do not turn back on as may be called from Sig100_process_command()
}

#ifdef MH_XXX
void Hub_log_init(void)
{
	Hub_log_getc_pos = 0;
//	Hub_log_putc_pos = Enc_data.log_pos;
	Hub_log_putc_pos = 0;	// Test
	Hub_log_page = -1;
}
#endif
