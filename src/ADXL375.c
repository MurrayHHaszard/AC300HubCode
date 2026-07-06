/*
 * ADXL375.c
 *
 *  Created on: 28/11/2023
 *      Author: OEM
 */

#include "LPC8xx.h"
#include "rom_api.h"
//#include "LPC8xx_swm.h"
//#include "LPC8xx.h"
#include "core_cm0plus.h"
#include "iocon.h"
#include "syscon.h"
#include "utilities.h"
#include "swm.h"
#include "spi.h"

#include "driver_spiflash.h"

//#include "board.h"
#include "ctimer.h"

#include "ac300Enc_ssp.h"
#include "timer.h"

#include "AC300_hub.h"
#include "Hub_flashlog.h"

#ifdef MH_ADXL375

//#define LPC_SPI1BAUDRate		4000000
#define LPC_SPI1BAUDRate		5000000


bool Time_this;
TIMER_t SPI_timer;
TIMER_t SPI_timer2;
TIMER_t SPI_timer3;
int SPI_timer2_count;

void SPI_start_timer(int chip_select)
{
	if(Time_this == false) return;
	Timer_start(&SPI_timer);
}

void SPI_stop_timer(int chip_select)
{
	if(Time_this == false) return;
	Timer_stop(&SPI_timer);
}



//#define LPC_SPI1BAUDRate		1000000
void SPImasterWriteRead1( int chip_select,uint8_t *WrBuf, uint8_t *RdBuf, uint32_t WrLen )
{
	uint32_t i = 0;

	LPC_SPI1->TXCTL &= ~(SPI_CTL_EOT);          // Start a new transfer, clear the EOT bit
	//	SPI_FLASH_CS0();	/* Drive SPI CS low. */

	uint8_t chip_select_bits = 3;				// Could be SSEL0 or SSEL1
	chip_select_bits ^= (1 << chip_select);		// We want to turn off the chip select bits that we want to use. 0 = active, 1 = inactive
	uint32_t txdatctl = (chip_select_bits << 16) | (SPI_CTL_LEN(DATA_WIDTH));

	SPI_start_timer(chip_select);


	//  	SPI_flash_cs0(chip_select);
	if ( WrLen == 1 ) {
		while ( (LPC_SPI1->STAT & SPI_STAT_TXRDY) == 0 );
		//		LPC_SPI1->TXDATCTL = SPI_CTL_LEN(DATA_WIDTH) | SPI_CTL_EOT | *WrBuf;
		LPC_SPI1->TXDATCTL = txdatctl | SPI_CTL_EOT | *WrBuf;
		while ( (LPC_SPI1->STAT & SPI_STAT_RXRDY) == 0 );
		*RdBuf = LPC_SPI1->RXDAT;
	}
	else
	{
		while ( (LPC_SPI1->STAT & SPI_STAT_TXRDY) == 0 );
		LPC_SPI1->TXDATCTL = txdatctl | *WrBuf++;
		while ( (LPC_SPI1->STAT & SPI_STAT_RXRDY) == 0 );
		*RdBuf++ = LPC_SPI1->RXDAT;
		uint32_t ilast = WrLen - 1;
		for(i=1;i<ilast;i++)
		{
			while ( (LPC_SPI1->STAT & SPI_STAT_TXRDY) == 0 );
			LPC_SPI1->TXDAT = *WrBuf++;
			while ( (LPC_SPI1->STAT & SPI_STAT_RXRDY) == 0 );
			*RdBuf++ = LPC_SPI1->RXDAT;
		}
		while ( (LPC_SPI1->STAT & SPI_STAT_TXRDY) == 0 );
		LPC_SPI1->TXDATCTL = txdatctl | SPI_CTL_EOT | *WrBuf++;
		while ( (LPC_SPI1->STAT & SPI_STAT_RXRDY) == 0 );
		*RdBuf++ = LPC_SPI1->RXDAT;
	}
	SPI_stop_timer(chip_select);
}
#ifdef MH_OLD_SPI_READ_WRITE1
void SPImasterWriteRead1( int spi_num,uint8_t *WrBuf, uint8_t *RdBuf, uint32_t WrLen )
{
	uint32_t i = 0;

  LPC_SPI1->TXCTL &= ~(SPI_CTL_EOT);                 // Start a new transfer, clear the EOT bit
//	SPI_FLASH_CS0();	/* Drive SPI CS low. */
//	SPI_flash_cs0(spi_num);
	SPI_ACCEL_CS0();

	if ( WrLen == 1 ) {
		while ( (LPC_SPI1->STAT & SPI_STAT_TXRDY) == 0 );
		LPC_SPI1->TXDATCTL = SPI_CTL_LEN(DATA_WIDTH) | SPI_CTL_EOT | *WrBuf;
		while ( (LPC_SPI1->STAT & SPI_STAT_RXRDY) == 0 );
		*RdBuf = LPC_SPI1->RXDAT;
	}
	else {
		while ( i < WrLen ) {
			/* Move only if TXRDY is ready */
			while ( (LPC_SPI1->STAT & SPI_STAT_TXRDY) == 0 );
			if ( i == 0 ) {
				LPC_SPI1->TXDATCTL = SPI_CTL_LEN(DATA_WIDTH) | *WrBuf++;
			}
			else if ( i == WrLen-1 ) {
				LPC_SPI1->TXDATCTL = SPI_CTL_LEN(DATA_WIDTH) | SPI_CTL_EOT | *WrBuf++;
			}
			else {
				LPC_SPI1->TXDAT = *WrBuf++;
			}
			while ( (LPC_SPI1->STAT & SPI_STAT_RXRDY) == 0 );
			*RdBuf++ = LPC_SPI1->RXDAT;
			i++;
		}
	}
//	SPI_flash_cs1(spi_num);
	SPI_ACCEL_CS1();

//	SPI_FLASH_CS1();	/* Drive SPI CS high */
	return;
}
#endif
int16_t Data_xyz[3];
int8_t Offset_xyz[3];
int32_t Tot_xyz[3];

#define REG_DEVID			0

#define REG_THRESH_SHOCK	0x1D
#define REG_OFSX			0x1E
#define REG_OFSY			0x1F
#define REG_OFSZ			0x20

#define REG_BW_RATE			0x2C
#define REG_POWER_CTL		0x2D

#define REG_DATAX0			0x32

#define REG_FIFO_CTL		0x38
#define REG_FIFO_STATUS		0x39

#define ADXL375_SAMPLE_RATE			100

const int Adxl375_sample_rate_t[]=
{
100,	// 10
200,	// 11
400,	// 12
800,	// 13
1600,	// 14
3200,	// 15
};

int Adxl375_sample_rate;
int Adxl375_x_count_max;
//#define ADXL375_BIT_RATE_CODE			10	// Default
//#define ADXL375_BIT_RATE_CODE			13

#define ADXL375_BIT_RATE_CODE			15
//#define ADXL375_BIT_RATE_CODE			14

#define MH_ADXL375

const int16_t Adxl_correct_xyz[3]={-1,0,1};

int SSP_read_ACCEL_byte(int byte_address)
{
	Tx_Buf[0] = 0x80 | (byte_address & 0xff);
//	Tx_Buf[0] = (byte_address & 0xff);
	SPImasterWriteRead1(SPI_ACCEL_CS,Tx_Buf,Rx_Buf,2);
	return Rx_Buf[1];
}
int SSP_read_ACCEL_bytes(int byte_address,int number_bytes)
{
	uint8_t b = (uint8_t) byte_address;
//	for(int i=0;i<30;i++) Rx_Buf[i] = 254;

	b |= 0x80;		// As a read
	if(number_bytes > 1) b |= 0x40;
	Tx_Buf[0] = b;
	SPImasterWriteRead1(SPI_ACCEL_CS,Tx_Buf,Rx_Buf,number_bytes+1);
	return 0;
}

int SSP_write_ACCEL_byte(uint8_t byte_address,uint8_t byte_val)
{
//	Tx_Buf[0] = 0x80 | (byte_address & 0xff);
	Tx_Buf[0] = byte_address;
	Tx_Buf[1] = byte_val;
//	SPImasterWriteOnly1(SPI_ACCEL_CS,Tx_Buf,2);
	SPImasterWriteRead1(SPI_ACCEL_CS,Tx_Buf,Rx_Buf,2);
	return 0;
}

int SSP_write_ACCEL_bytes(uint8_t byte_address,uint8_t *wbuf,uint8_t number_bytes)
{
	uint8_t b = byte_address;
//	for(int i=0;i<30;i++) Rx_Buf[i] = 254;

#ifdef MH_ADXL375
//	b |= 0x80;		// As a read
	if(number_bytes > 1) b |= 0x40;
#else
//	b = (b << 1) | 1;
#endif
	Tx_Buf[0] = b;
	memcpy(Tx_Buf+1,wbuf,number_bytes);
//	SPImasterWriteOnly(SPI_ACCEL_CS,Tx_Buf,number_bytes+1);
	SPImasterWriteRead1(SPI_ACCEL_CS,Tx_Buf,Rx_Buf,number_bytes+1);
	return 0;
}

TIMER_t SPI_timer;
void SPI_ADXL375_init(void)
{
	LPC_SYSCON->SYSAHBCLKCTRL0 |= ( SWM | GPIO | SPI1 | IOCON);

	// Provide main_clk as function clock to SPI0
  LPC_SYSCON->SPI1CLKSEL = FCLKSEL_MAIN_CLK;
  // Configure the SWM (see peripherals_lib and swm.h)

  // Configure the SWM (see peripherals_lib and swm.h)
  ConfigSWM(SPI1_SCK,   P0_8);
  ConfigSWM(SPI1_MISO,  P1_5);
  ConfigSWM(SPI1_MOSI,  P1_6);
  ConfigSWM(SPI1_SSEL0, P0_9);


  LPC_SYSCON->SYSAHBCLKCTRL[0] |= GPIO1;   // Turn on clock to GPIO1
//	SPI_ACCEL_CS1();

//  SPI_flash_cs1(SPI_ACCEL_CS);
//  Config_Output_Pin(SPI_ACCEL_CS_PIN);

  // Setup the SPIs ...
  LPC_SYSCON->PRESETCTRL0 &=  (SPI1_RST_N);
  LPC_SYSCON->PRESETCTRL0 |= ~(SPI1_RST_N);
  // Configure the SPI master's clock divider, slave's value meaningless. (value written to DIV divides by value+1)
  SystemCoreClockUpdate();                // Get main_clk frequency
  LPC_SPI1->DIV = (main_clk/LPC_SPI1BAUDRate) - 1; //(2-1);
  // Configure the CFG registers:
  // Enable=true, master/slave, no LSB first, CPHA=0, CPOL=0, no loop-back, SSEL active low
#ifdef MH_ADXL375
  LPC_SPI1->CFG = SPI_CFG_ENABLE | SPI_CFG_MASTER | SPI_CFG_CPHA | SPI_CFG_CPOL;
#else
  LPC_SPI1->CFG = SPI_CFG_ENABLE | SPI_CFG_MASTER;
#endif



  // Configure the master SPI delay register (DLY), slave's value meaningless.
  // Pre-delay = 0 clocks, post-delay = 0 clocks, frame-delay = 0 clocks, transfer-delay = 0 clocks
  LPC_SPI1->DLY = 0x0000;
//  LPC_SPI1->DLY = 0x0007;

	/* Polling is used in this SPI flash access example. */
//  LPC_SPI1->INTENSET = SPI_RXRDYEN;                    // Master interrupt only on received data
//  NVIC_EnableIRQ(SPI0_IRQn);

  	int result = SSP_read_ACCEL_byte(REG_DEVID);
  	PRINTF("ADXL_REG_DEVID=%d\r\n",result);

  	SSP_write_ACCEL_byte(REG_POWER_CTL,0);		// Try turning measure mode OFF
//  	wait_us(5);

#ifdef MH_SPI_TEST1
	Time_this = true;
	for(;;)
	{
		Timer_reset(&SPI_timer);
		for(int i=0;i<10000;i++)
	  	{
	  	  	SSP_read_ACCEL_bytes(REG_THRESH_SHOCK,29);
	  	}
  		uint32_t usecs = Timer_read_us(&SPI_timer);
  		printf("usecs:%d\r\n",usecs);
	}
#endif

#define MH_SET_OFFSET
#ifdef MH_SET_OFFSET
  	  	Offset_xyz[0] = -3;		// Try and init offsets
  	  	Offset_xyz[1] = 1;
//  	  	Offset_xyz[2] = 0;
//  	  	Offset_xyz[2] = 1;
  	  	Offset_xyz[2] = -2;
#else
  	  	Offset_xyz[0] =  0;		// Try and init offsets
  	  	Offset_xyz[1] =  0;
  	  	Offset_xyz[2] =  0;
#endif

  	SSP_write_ACCEL_bytes(REG_OFSX,(uint8_t *)Offset_xyz,3);


  	Adxl375_sample_rate = Adxl375_sample_rate_t[ADXL375_BIT_RATE_CODE-10];
  	Adxl375_x_count_max = Adxl375_sample_rate / 800;
//  	Adxl375_x_count_max = 2;		// Default for 1600 Hz
//  	if(Adxl375_sample_rate == 3200) Adxl375_x_count_max = 4;
  	PRINTF("Adxl375_sample_rate:%d\r\n",Adxl375_sample_rate);
  	SSP_write_ACCEL_byte(REG_BW_RATE,ADXL375_BIT_RATE_CODE);

#ifdef MH_READ_ADXL375_REGISTERS
  	SSP_read_ACCEL_bytes(REG_THRESH_SHOCK,29);
  	for(int i=0;i<29;i++)
  	{
  		PRINTF("%2d,%2x %d\r\n",i+29,i+29,Rx_Buf[i+1]);
  	}
#endif
  	SSP_write_ACCEL_byte(REG_FIFO_CTL,128);		// Try turning STREAM mode on
//  	SSP_write_ACCEL_byte(0x38,64);		// Try turning FIFO mode on
#ifdef MH_ADXL375_CHECK_STREAM_MODE
  	PRINTF("Tried to turn STREAM mode on\r\n");
  	result = SSP_read_ACCEL_byte(REG_FIFO_CTL);
  	PRINTF("result = %d\r\n",result);
#endif

#ifdef MH_ADXL375_TEST_SAMPLING
  	SSP_write_ACCEL_byte(REG_POWER_CTL,8);		// Try turning measure mode on
  	result = SSP_read_ACCEL_byte(REG_POWER_CTL);
  	PRINTF("Measure mode = %d\r\n",result);

  	for(int i=0;i<1;i++)
  	{
  		for(int j=0;j<=2;j++) Tot_xyz[j] = 0;
  		Timer_reset(&SPI_timer);
  	  	SPI_timer2_count=0;
//  	  	Timer_stop(&SPI_timer2);		// In case was used
  	  	Timer_reset(&SPI_timer2);
  	  	Timer_reset(&SPI_timer3);
  	  	Timer_start(&SPI_timer3);

  		for(int j=0;j<Adxl375_sample_rate;)
  		{
  	  		Time_this = false;

  	  		SPI_timer2_count++;
  			Timer_start(&SPI_timer2);
  	  		int fifo_status_register = SSP_read_ACCEL_byte(REG_FIFO_STATUS);
  	  		Timer_stop(&SPI_timer2);
  			if(fifo_status_register > 0)
  			{
  				//  			PRINTF("fifo_status_register:%d\r\n",fifo_status_register);
  				j++;
  				Time_this = true;
  				SSP_read_ACCEL_bytes(REG_DATAX0,6);
  				memcpy((uint8_t *)Data_xyz,Rx_Buf+1,6);
  				Tot_xyz[0] += Data_xyz[0] + Adxl_correct_xyz[0];
  				Tot_xyz[1] += Data_xyz[1] + Adxl_correct_xyz[1];
  				Tot_xyz[2] += Data_xyz[2] + Adxl_correct_xyz[2];

//  				for(int k=0;k<=2;k++) Tot_xyz[k] += Data_xyz[k];

  				//  	  			PRINTF("%d,%d,%d\r\n",Data_xyz[0],Data_xyz[1],Data_xyz[2]);
  			}
  		}
  		Timer_stop(&SPI_timer3);
  		uint32_t usecs = Timer_read_us(&SPI_timer);
  		uint32_t usecs3 = Timer_read_us(&SPI_timer3);
  		PRINTF("%d,%d,%d,%d,%d,%d\r\n",usecs3,usecs,usecs/Adxl375_sample_rate,Tot_xyz[0],Tot_xyz[1],Tot_xyz[2]);
  		PRINTF("x=%d,y=%d,z=%d\r\n",Tot_xyz[0]/Adxl375_sample_rate,Tot_xyz[1]/Adxl375_sample_rate,Tot_xyz[2]/Adxl375_sample_rate);
  		usecs = Timer_read_us(&SPI_timer2);
  		PRINTF("SPI_timer2_count:%d, usecs:%d, usecs_per_read:%d\r\n",SPI_timer2_count,usecs,usecs/SPI_timer2_count);
  	}

  	PRINTF("\r\nFinished, turning measure mode off\r\n");
  	SSP_write_ACCEL_byte(REG_POWER_CTL,0);		// Try turning measure mode OFF
#endif

}
int Adxl375_x_tot;
int Adxl375_x_count;

int Adxl375_x_sec_tot;		// Use this to verify averages over a second
int Adxl375_z_tot;
int Adxl375_z_count;

void Adxl375_start_measure(void)
{
	SSP_write_ACCEL_byte(REG_POWER_CTL,8);		// Try turning measure mode on
	Adxl375_x_tot = 0;		// To be sure...
	Adxl375_x_count = 0;
	Adxl375_z_tot = 0;		// To be sure...
	Adxl375_z_count = 0;
	Adxl375_x_sec_tot = 0;
//	Flog_save_data_flag = true;
}
void Adxl375_stop_measure(void)
{
  	SSP_write_ACCEL_byte(REG_POWER_CTL,0);		// turn measure mode OFF
//  	Flog_save_data_flag = false;
}
int Adxl375_read_fifo_status_register(void)
{
	return SSP_read_ACCEL_byte(REG_FIFO_STATUS);
}
void Adxl375_read_xyz_registers(void)
{
	SSP_read_ACCEL_bytes(REG_DATAX0,6);
	memcpy((uint8_t *)Data_xyz,Rx_Buf+1,6);
	Data_xyz[0] += Adxl_correct_xyz[0];
	Data_xyz[1] += Adxl_correct_xyz[1];
	Data_xyz[2] += Adxl_correct_xyz[2];
}
void Adxl375_read_xyz_if_ready(void)
{
	if(Flog_save_data_flag == false) return;
	if(Adxl375_read_fifo_status_register() > 0)
	{
		Adxl375_read_xyz_registers();
		Adxl375_x_tot += Data_xyz[0];
		Adxl375_x_sec_tot += Data_xyz[0];
		Adxl375_x_count++;
		Adxl375_z_tot += Data_xyz[2];
		Adxl375_z_count++;

		if(Adxl375_x_count >= Adxl375_x_count_max)		// This test for 1600 samples per second. 1600 >= 2, 3200 then would be >=4
		{
			Flog_put_ring_data_adxl375(Adxl375_x_tot);
			Adxl375_x_tot = 0;
			Adxl375_x_count = 0;
		}

	}
}
#endif
