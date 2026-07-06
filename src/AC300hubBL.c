// derived from ROM_divide.c

//#include <stdio.h>
//#include <string.h>

#include "LPC8xx.h"
#include "rom_api.h"
//#include "LPC8xx_swm.h"
#include "core_cm0plus.h"
#include "DAC.h"
#include "iocon.h"

#include "syscon.h"
#include "utilities.h"

#include "swm.h"
#include "syscon.h"
#include "spi.h"
#include "sct.h"

#include "driver_spiflash.h"

//#include "adc.h"

//#define MANUFACTURE_ID		0xef3012	/* This may change depending on the SPI flash device on the board. */
#define MANUFACTURE_ID		0x120184d	// MHH:29/11/2023

//#include "utilities.h" target

//#include "board.h"
#include "ctimer.h"
#include "timer.h"

#include "ac300Enc_ssp.h"
#include "AC300_hub.h"

//const unsigned char Version_id[]={'z','z',':','V',':',0,HUB_SOFTWARE_REVISION/10,HUB_SOFTWARE_REVISION%10,':',':'};	// May be able to scan for it
const unsigned char Version_id[]={'z','z',':','V',':',0,HUB_SOFTWARE_REVISION/100,HUB_SOFTWARE_REVISION%100,':',':'};	// MHH:04/09/2023

void setup_debug_uart(void);
//5000 "!FR "?FR 1:RDY

//extern TIMER_t Enc_timer;
//extern int Enc_usecs;
extern int Enc_rpm;
extern int Enc_rpm_count;
extern int DAC0_v;

#define SYSTICK_HZ		100
//#define LPC845_HZ		15000000			// 15 million
#define LPC845_HZ		12000000			// MHH:04/10/2023 12 million
#define SYSTICK_TIME ((LPC845_HZ/SYSTICK_HZ)-1)

int Systick_dummy;
volatile uint32_t Systick_tick;
volatile uint32_t Systick_secs;
volatile int Systick_count;
volatile  int Systick_flag;


//#define false	(0)
//#define true	(!false)

int Display_ticker = 100;		// Once a second
volatile bool Display_rpm=false;

uint32_t Motor_rpm_count;
int Systick_rpm;
bool Systick_disabled;
#define COMMAND_TIMEOUT_TICKS	10	// One tenth of a second at 100 Hz
void Motor_RPM_ISR(void);


//uint8_t Systick_coarse_tick_count;
//uint8_t Systick_fine_tick_count;
void ADC_sample(void);

#ifdef MH_MOTOR_TIME_TEST
char Last_command;
int Last_pos;
bool Motor_time_test;
int Motor_msecs;
extern TIMER_t Motor_timer;
#endif
//extern int DAC0_v;
//extern uint32_t Motor_target_rpm;
//extern bool Motor_main_mode;
//void Set_AOUT(int val);
//CAL_START ,D2=%d Current:%d Enc_pos:%d Enc_rpm "AC200 Run MH_trace_return
//--------------------------------------------------------------------------------------------------------------
int16_t Hub_watchdog_count;
uint8_t Hub_watchdog_reset_count;
uint16_t Hub_watchdog_from;
bool Hub_watchdog_active;
void Hub_Watchdog_Reset(void)
{
//#define MH_DISABLE_WATCHDOG
#ifdef MH_DISABLE_WATCHDOG
	printf("\r\nHub_Watchdog_Reset:DISABLED FOR TESTING!!!\r\n");
	Hub_watchdog_active = false;
	return;
#endif


	if(Hub_watchdog_reset_count++ == 0)
	{
		L2PRINTF("\r\nHub_Watchdog_Reset:%05d\r\n",Hub_watchdog_from);
		wait_ms(200);
	}
	LPC845_reboot();
}
//--------------------------------------------------------------------------------------------------------------
void Hub_watchIt(uint16_t from)
{
	Hub_watchdog_active = true;
	Hub_watchdog_from = from;
	Hub_watchdog_count  = 200;	// 2 seconds
}
//--------------------------------------------------------------------------------------------------------------
#ifdef MH_XXX
void Hub_watchIt2(uint16_t from)
{
	Hub_watchdog_from = from;
}
#endif
int Hub_Force_Hardfault(void)
{
	int *ptr=0;
	int v;
	ptr+=1000000;
	v=*ptr;
	return v;
}

//--------------------------------------------------------------------------------------------------------------
static int HardFault_count;
// If we don't have a hardfault handler then default behaviour is an infinite loop.
void HardFault_Handler(void)
{
	if(HardFault_count++ == 0)
	{
		L2PRINTF("Hub_Hardfault_Handler:%05d\r\n",Hub_watchdog_from);
		wait_ms(200);
	}
	LPC845_reboot();
//	NVIC_SystemReset();
}
#define SYSTICK_HZ		100
volatile uint32_t Systick_sec100_count;
//uint32_t Systick_new_rpm_count;	// MHH:19/02/2025
//uint32_t Systick_rpm_count;
//extern int Enc_rpm_count2;
#define POS_FIRST_TIME	999999		// Invalid pos
int Systick_save_pos=POS_FIRST_TIME;
extern bool Enc_new_rpm;
void SysTick_Handler(void)
{
	// Clear the interrupt flag by reading the SysTick_CTRL register
	Systick_dummy = SysTick->CTRL;
	Systick_flag = 0;
	Systick_tick++;
	if(Systick_sec100_count++ >= (SYSTICK_HZ-1))
	{
		Systick_sec100_count = 0;
		Systick_secs++;
	}
//	if(Motor_main_mode) return;	// MHH:02/09/2023. Comment out because we want Hub Diagnostics mode to work always. Maybe need to distinguish between Calibrating and not.

	if(Hub_watchdog_active)
	{
		if(--Hub_watchdog_count <= 0)
		{
			Hub_Watchdog_Reset();
		}
	}
//	if((Systick_tick & 1) == 0)	// MHH:19/11/2025. Every 20 ms.
	if((Systick_tick & 3) == 0)	// MHH:19/11/2025. Every 40 ms.
	{
		if(Systick_save_pos == POS_FIRST_TIME)		// First time
		{
			if(Enc_Pos.pos) Systick_save_pos = Enc_Pos.pos;
		}
		else
		{
			int pos_change = abs(Systick_save_pos - Enc_Pos.pos);
			Systick_save_pos = Enc_Pos.pos;
			if(pos_change == 0)
			{
				Enc_new_rpm = true;
				Enc_rpm = 0;
//				Enc_rpm_count2 = 0;			// MHH:24/11/2025
			}
#ifdef MH_XXX	// MHH:24/11/2025
			else
			{
				if(Enc_rpm == 0)
				{
					//					Enc_rpm = (pos_change * SYSTICK_HZ * 60)/(4*4);	// 4 pos_change per revolution (4 pole_pairs), and sampling is once every 4th systick interrupt
					Enc_rpm = (pos_change * SYSTICK_HZ * 15)/4;
					Enc_new_rpm = true;
				}
			}
#endif
		}

	}


#ifdef MH_YYY
	if(++Systick_check_rpm_count >= 10)		// MHH:27/12/2023. Every tenth of a second
	{
		Systick_check_rpm_count = 0;
		Enc_raw_speed_control();
	}
#endif
//#if MH_YYY
	if(Speed_raw_mode)
	{
		Enc_raw_speed_check_rpm();
	}
//#endif
	if(Enc_ud_feather_stop  || 	Enc_feather_limit_reached)	// Enc_feather_limit_reached can be triggered by closeness to hard stop
	{
//		Enc_feather_limit_reached = (Enc_pos > Enc_data.ud_feather_stop); // Normally set in Enc_ISR but in case passed stop and not moving
    	if((Enc_data.run_flags & ENC_RUN_FLAGS_NO_FEATHER) == 0)	// MHH:20/05/2025
    	{
    		Enc_feather_limit_reached = (Enc_Pos.pos < Enc_data.ud_feather_stop); // MHH:05/07/2023. Normally set in Enc_ISR but in case passed stop and not moving
    	}
	}
	if(Enc_ud_reverse_stop || Enc_reverse_limit_reached)
	{
//		Enc_reverse_limit_reached = (Enc_pos < Enc_data.ud_reverse_stop); // Normally set in Enc_ISR but in case passed stop and not moving
		Enc_reverse_limit_reached = (Enc_Pos.pos > Enc_data.ud_reverse_stop); // MHH:05/07/2023. Normally set in Enc_ISR but in case passed stop and not moving
	}
#ifdef MH_XXX
	if(Systick_tick % 100 == 0)// Once a second
	{
		Systick_secs++;
		Board_turn_heater_off();
//		Board_pin_write(HEATER_PIN,false);	// Turn heater off, in case we lose contact with AC200
	}
#endif

//#ifndef MH_DIAGS
	if(Systick_sec100_count == 0)// Once a second
	{
		Board_turn_heater_off();
//		Board_pin_write(HEATER_PIN,false);	// Turn heater off, in case we lose contact with AC200
	}
//#endif
	if(Systick_count < COMMAND_TIMEOUT_TICKS)
	{
		Systick_count++;
		Systick_disabled = false;
	}
	else
	{
		if(Systick_disabled == false)	// We do not want to disable when a valid command has come through
		{
			Systick_disabled = true;
			Enc_positioning = false;
			Disable_Motor(100);
		}
	}
#ifdef MH_YYY
	if(Display_ticker)
	{
		if(Systick_tick % Display_ticker == 0)
		{
			if(Enc_rpm_count == 0) Enc_rpm = 0;
			Systick_rpm = Enc_rpm_count * 60;	// Rough check of instant RPM
			Enc_rpm_count = 0;
			Display_rpm = true;
		}
	}
#endif
}
//----------------------------------------------------------------------------------------------
#ifdef MH_YYY
void SPI_flash_cs1(int spi_num)
{
	switch(spi_num)
	{
	case SPI_MRAM_NUM:
		SPI_MRAM_CS1(); /* Make the port bit an output driving '1' */
		return;
	case SPI_FLASH_CS:
		SPI_FLASH127_CS1(); /* Make the port bit an output driving '1' */
		return;

	default:
		L2PRINTF("SPI_flash_cs1:Invalid spi_num:%d\r\n",spi_num);
		return;		// Should abort...
	}
}
void SPI_flash_cs0(int spi_num)
{
	switch(spi_num)
	{
	case SPI_MRAM_NUM:
		SPI_MRAM_CS0(); /* Make the port bit an output driving '0' */
		return;
	case SPI_FLASH_CS:
		SPI_FLASH127_CS0(); /* Make the port bit an output driving '0' */
		return;
	default:
		L2PRINTF("SPI_flash_cs0:Invalid spi_num:%d\r\n",spi_num);
		return;		// Should abort...
	}
}
#endif
void SPI_init(void)
{

	LPC_SYSCON->SYSAHBCLKCTRL0 |= ( SWM | GPIO | SPI0 | IOCON);

	// Provide main_clk as function clock to SPI0
	LPC_SYSCON->SPI0CLKSEL = FCLKSEL_MAIN_CLK;
	LPC_SYSCON->SYSAHBCLKCTRL[0] |= GPIO1;   // Turn on clock to GPIO1


	// Configure the SWM (see peripherals_lib and swm.h)
	ConfigSWM(SPI0_SCK,   P0_0);
	ConfigSWM(SPI0_MISO,  P0_6);
	ConfigSWM(SPI0_MOSI,  P0_7);
	ConfigSWM(SPI0_SSEL0, P1_7);		// CS MRAM

#ifdef MH_FLASH_RECORD

//	if(Flog_diags_enabled)
	{
		ConfigSWM(SPI0_SSEL1, P0_21);	// CS S25FL127
	}

#endif

	// SPI Flash CS Pin Initialize
//  SPI_FLASH_CS1(); /* Make the port bit an output driving '1' */
  //
//	Config_Output_Pin(SPI_CS_PIN);
//  SPI_FLASH_CS0(); /* Test */


#ifdef MH_YYY
#ifdef MH_XXX

  SPI_FLASH_CS1(); /* Make the port bit an output driving '1' */
  LPC_GPIO_PORT->DIRSET[1] = 1<<(SPI_CS_PIN-32);
#else
  SPI_flash_cs1(SPI_MRAM_NUM);
  Config_Output_Pin(SPI_MRAM_CS_PIN);
//#ifdef MH_DIAGS
  if(Flog_diags_enabled)
  {
	  SPI_flash_cs1(SPI_FLASH_CS);
	  Config_Output_Pin(SPI_FLASH_CS_PIN);
  }
  #endif
#endif
//  SPI_FLASH_CS0(); /* Test */
//  SPI_FLASH_CS1(); /* Test */

  // Setup the SPIs ...
  LPC_SYSCON->PRESETCTRL0 &=  (SPI0_RST_N);
  LPC_SYSCON->PRESETCTRL0 |= ~(SPI0_RST_N);
  // Configure the SPI master's clock divider, slave's value meaningless. (value written to DIV divides by value+1)
  SystemCoreClockUpdate();                // Get main_clk frequency
  LPC_SPI0->DIV = (main_clk/LPC_SPI0BAUDRate) - 1; //(2-1);
  // Configure the CFG registers:
  // Enable=true, master/slave, no LSB first, CPHA=0, CPOL=0, no loop-back, SSEL active low
  LPC_SPI0->CFG = SPI_CFG_ENABLE | SPI_CFG_MASTER;
  // Configure the master SPI delay register (DLY), slave's value meaningless.
  // Pre-delay = 0 clocks, post-delay = 0 clocks, frame-delay = 0 clocks, transfer-delay = 0 clocks
  LPC_SPI0->DLY = 0x0000;

	/* Polling is used in this SPI flash access example. */
//  LPC_SPI0->INTENSET = SPI_RXRDYEN;                    // Master interrupt only on received data
//  NVIC_EnableIRQ(SPI0_IRQn);


//#ifdef MH_CHECK_FLASH_ID
#ifdef MH_FLASH_RECORD
//	if(Flog_diags_enabled)	// MHH:02/12/2023
	{
	  	uint8_t spi_flash_status = SSP_Flash_Read_Status(SPI_FLASH_CS);
	//  	uint8_t spi_status = spiflash_check_status();
	  	PRINTF("spi_flash_status:%d\r\n",spi_flash_status);
	  	uint32_t flash_id = SSP_Flash_Read_ID();
	  	if(flash_id != MANUFACTURE_ID)
	  	{
	  		PRINTF("flash_id=%d,MANUFACTURE_ID=%d\r\n",flash_id,MANUFACTURE_ID);
	  	}
	}
#endif
//#endif

//#define MH_TEST_SSP_FLASH
#ifdef MH_TEST_SSP_FLASH
  	uint32_t erase_sector = 1;

  	SSP_Erase_Sector(erase_sector);
//  	wait_ms(1000);	// Test.
  	SSP_Flash_Test2();
//  	ReturnToContinue();
#endif


	ac200Enc_SSP_Init();
//	SSP_Mram_Test();

}
//-----------------------------------------------------------------------------------------------------------
void setup_sig100_uart(void);
int Sig100_register[8];
bool Motor_process_command(int command);
int Sig100_err;
void SIG100_error(int ival)
{
	Sig100_err = ival;
	PRINTF("\r\nSIG100_error:%d\r\n",ival);
}
void SIG100_test(void)
{
	Board_UARTPutchar_RB('>');

//	wait_ms(1);
	for(int i=0;i<10;i++)
	{
		char ch = '0' + i;
		Sig100_X_putc(ch);
	}
	Board_UARTPutchar_RB('<');
//	wait_ms(5);
	for(int i=0;i<10;i++)
	{
		int ci = Sig100_X_getc_timeout(2);
		if(ci > 0)
		{
			Board_UARTPutchar_RB((char) ci);
		}
		if(ci != 'a' + i)
		{
//			SIG100_error(1);
			PRINTF("\r\nci = %d,i=%d\r\n",ci,i);
			break;
		}
	}
//	wait_ms(1);
	Sig100_X_putc('x');
	Board_UARTPutchar_RB('x');
}
//-------------------------------------------------------------------------------------------------------


#define SPACKET_SIZE	16
uint8_t Spkt[SPACKET_SIZE];

// Note: Unless requested to send the longer packet, we could make do with a short packet of 4 bytes. eg:
/*
	Spkt[0] = '_';
	Spkt[1] = flags;	// So we can check data valid
	Spkt[2] = voltage;
	Spkt[3] = checksum
*/
// Hub_data


void Checksum_and_send_pkt(const int plen)
{
	if(plen > SPACKET_SIZE)
	{
		DebugAbort("Checksum_and_send_pkt:plen > sizeof(Spkt)");
	}
	uint8_t checksum = 0;
	int last_ix = plen -1;
	for(int i=0;i<last_ix;i++)
	{
		checksum += Spkt[i];
	}
	uint8_t checksum_byte = -checksum;	// Idea is that total of all bytes = 0
	Spkt[last_ix] = checksum_byte;

	for(int i=0;i<plen;i++)
	{
		char ch = (char)Spkt[i];
		Sig100_X_putc(ch);
	}

}
//---------------------------------------------------------------------------------------------------------------
void Put_uint16(uint8_t *dst_p,uint16_t v)
{
	dst_p[0] = (uint8_t)(v & 0xff);	// low order byte
	dst_p[1] = (uint8_t)(v >> 8);	// high order byte
}
void Put_int16(uint8_t *dst_p,int16_t v)
{
	dst_p[0] = (uint8_t)(v & 0xff);	// low order byte
	dst_p[1] = (uint8_t)(v >> 8);	// high order byte
}

void Sig100_test_error(void)
{
	Sig100_send_error_pkt('A',12345,-5);
}


bool Sig100_hub_error_ready;
uint16_t Sig100_hub_error_count;
uint8_t Epkt[10];

void Sig100_send_error(void)
{
	memcpy(Spkt,Epkt,8);
	Checksum_and_send_pkt(9);
	PRINTF("Sig100_send_error:\r\n")

}

void Sig100_format_error_pkt(uint8_t error_type,uint16_t ecode,int16_t eval)
{
	// Have to ignore if we are not connected too.
	if(Command_total < 10) return;
	if(Sig100_hub_error_count++ > 5) return;	// Make sure we don't get in a loop

	//--------------------------------------------------------------------------------------------------
	/*  *  Packet '#' return
	*
	*  	0			'#' Header
	*  	1			'e'
	*  	2			Error type 'E','A',"W'
		3			Command
	*  	4-5			Error code
	*   6   		checksum
	*/
	/*
	 * typedef struct
	{
		uint16_t code;			// 0.
		uint16_t command;		// 2. Could be byte
		uint16_t ac200_run;		// 4.
		uint16_t count;			// 6. of this code, if multiple occurrences of error.
		uint16_t from;			// 8. where it was called from
		uint16_t total;			// 10.
	} Enc_err_td;				// 12.
	 *
	 */
	Epkt[0] = '#';
	Epkt[1] = 'e';
	Epkt[2] = error_type;
	Epkt[3] = (uint8_t)Command;
	Put_uint16(Epkt+4,ecode);
	Put_int16(Epkt+6,eval);

	// from may not be needed as the ecode should be unique for each error.
	// We should not need to send the run as ac200 will know it.

//	Checksum_and_send_pkt(9);
	Sig100_hub_error_ready = true;		// Don't send until we receive a msg, as may conflict with message sent


	Enc_data.error_count++;
	Enc_data.error_type = error_type;
	Enc_data.error_command = (uint8_t)Command;
	Enc_data.error_ac200_run = Enc_data.ac200_run;
	Enc_data.error_number = ecode;
	Enc_data.error_data = eval;
	Enc_data.error_hub_run = Enc_data.hub_run;	// MHH:06/09/2023

	Enc_ssp_update_enc_run_data();

	PRINTF("\r\nFormatError:Type:%c,Command:%c,code:%d,val:%d\r\n",error_type,Command,ecode,eval);

}
//---------------------------------------------------------------------------------------------------------------

//#define POS_CHANGE_BIT		128
uint8_t Sig100_get_pkt_flags(void)
{
	uint8_t flags=0;	// 8 bits. Bits 0 to 3 contain logical microswitch status (FINE, COARSE, FEATHER, REVERSE)
					// High 4 bits reserved for later use (eg starting, fine position verified)
//	if(Enc_fine_microswitch_on) flags |= STOP_FINE;

	if(Board_pin_read(FINE_MICROSWITCH_PIN)) flags |= STOP_FINE;
	if(Board_pin_read(COARSE_MICROSWITCH_PIN)) flags |= STOP_COARSE;
	if(Enc_feather_limit_reached) flags |= STOP_FEATHER;
	if(Enc_reverse_limit_reached) flags |= STOP_REVERSE;

	if(Motor_enabled) flags |= MOTOR_ENABLED_BIT;
	if(Board_pin_read(READY_PIN) == 0) flags |= NOT_READY_BIT;
	if(Enc_data.run_flags & ENC_RUN_FLAGS_NO_FEATHER)
	{
		flags |= NO_FEATHER_BIT;
	}
	return flags;
}
//---------------------------------------------------------------------------------------------------------------
int Response_short_count;
//int Enc_save_pos;

void Sig100_send_response(void)
{

	// Send a short response by default. Exceptions: Every tenth response, or if Master asks for long response

	Spkt[1] = Sig100_get_pkt_flags();	// The 2 bytes common to 4 byte pkt and 8 byte pkt
	Spkt[2] = ADC_adjusted_voltage;		// Reserved for voltage. This may help with detecting brush wear and other problems

	bool diagnostics = ((Enc_data.sw_flags & 2) != 0);
//	diagnostics = false;		// Test!!!


	if(diagnostics == false)
	{
//		if(Enc_Pos.pos == Enc_save_pos && Response_short_count++ < 20)
		if((Enc_rpm == 0) && (Response_short_count++ < 20))	// MHH:22/11/2025
		{
			Spkt[0] = '_';
			Checksum_and_send_pkt(4);
			return;
		}
	}

	Response_short_count = 0;
	uint8_t pkt_header = '&';

	if(diagnostics) pkt_header = '^';
	Spkt[0] = pkt_header;

// Others to be added here...
	ADC_adjusted_temp = ADC_avg_temp >> 4;	// Divide by 16 to fit into a byte
	Spkt[3] = ADC_adjusted_temp;	// Reserved for temperature
	Spkt[4] = (uint8_t)(Enc_rpm >> 5);	// MHH:12/08/2025. Divide by 32
	Put_uint16(Spkt+5,Enc_Pos.pos);
//	Spkt[5] = (uint8_t)(pos & 0xff);	// low order byte
//	Spkt[6] = (uint8_t)(pos >> 8);	// high order byte
	if(diagnostics)
	{
		Spkt[7] = (uint8_t)(DAC0_v >> 2);	// Max val 1023, so divide by 4
		Checksum_and_send_pkt(9);
	}
	else
	{
		Checksum_and_send_pkt(8);
	}
}

//--------------------------------------------------------------------------------------------------------
#define COMMAND_MAX_LEN	20
#define NO_COMMAND	0
char Command_string[20];
char Reply_string[20];
uint8_t Reply_len;
char Command;
char AC300_Tick;
char Command_speed;
uint8_t Command_len;
//char Arg1,Arg2;


//--------------------------------------------------------------------------------------------
bool Checksum_command_pkt(const int plen)
{
	uint8_t checksum = 0;
	for(int i=0;i<plen;i++)
	{
		checksum += Command_string[i];
	}
	if(checksum == 0) return true;
	return false;
}
bool Checksum_reply_pkt(const int plen)
{
	uint8_t checksum = 0;
	for(int i=0;i<plen;i++)
	{
		checksum += Reply_string[i];
	}
	if(checksum == 0) return true;
	return false;
}
//--------------------------------------------------------------------------------------------
int16_t Get_int16(char *cptr)
{
	uint8_t *src = (uint8_t *)cptr;
	int16_t v = *src++;
	v |= (*src << 8);
	return v;
}
uint16_t Get_uint16(char *cptr)
{
	uint8_t *src = (uint8_t *)cptr;
	uint16_t v = *src++;
	v |= (*src << 8);
	return v;
}
//--------------------------------------------------------------------------------------------
void Checksum_and_send_reply_pkt(const int plen)
{
	Reply_len = plen;
	uint8_t checksum = 0;
	int last_ix = plen -1;
	for(int i=0;i<last_ix;i++)
	{
		checksum += Reply_string[i];
	}
	uint8_t checksum_byte = -checksum;	// Idea is that total of all bytes = 0
	Reply_string[last_ix] = checksum_byte;

	for(int i=0;i<plen;i++)
	{
		char ch = (char)Reply_string[i];
		Sig100_X_putc(ch);
	}
}

//--------------------------------------------------------------------------------------------
void Send_reply_pkt(const int plen)
{
	Reply_len = plen;
	for(int i=0;i<plen;i++)
	{
		char ch = (char)Reply_string[i];
		Sig100_X_putc(ch);
	}
}

//--------------------------------------------------------------------------------------------
/*
 *
 *  Type 'I' pkt
 *
 *  Format:
 *  	Offset		Desc
 *  	0			'!'		Packet header, data to follow
 *  	1			'I'		Type of data is Initial data
 *  	2			User define stop flags
 *  	3			unused
 *  	4-5			Hub user defined coarse stop, relative to fine stop in encoder increments
 *  	6-7			Hub user defined feather stop
 *  	8-9			Hub user defined reverse stop
 *  	10			unused
 *  	11			checksum
 */

#define SIG100_I_PKT_LEN		14
#define SIG100_J_PKT_LEN		8

//#define STOP_HIVAL_POS	32767
/*
 *  Format:
 *  	Offset		Desc
 *  	0			'!'		Packet header, data to follow
 *  	1			'I'		Type of data is Initial data
 *  	2			User define stop flags
 *  	3			Motor pole pairs (only used for soft FINE stop)
 *  	4-5			Hub user defined coarse stop, relative to fine stop in encoder increments
 *  	6-7			Hub user defined feather stop
 *  	8-9			Hub user defined reverse stop
 *  	10-11		Motor gear ratio (only used for soft FINE stop)
 *  	12-13		actual coarse stop, return only
 *  	14			spare
 *  	15			checksum
 *
 */
//--------------------------------------------------------------------------------------------
/*
 *
 *#define SIG100_i_PKT_LEN 5
void Sig100_send_init_i(void)
{
	Sig100_send_pkt[0] = '!';	// Tell hub some new arguments coming
	Sig100_send_pkt[1] = 'i';	// Initial data

	Put_int2(Sig100_send_pkt+2,Stat_Rec.run_number);

	Sig100_checksum_and_send_pkt(SIG100_i_PKT_LEN);
}
 *
 *
 */
#define SIG100_i_PKT_LEN		5
#define SIG100_i_PKT_LEN2		7

void Put_int2(char *cdst,uint16_t v);

void Process_i_data(void)
{
	Board_UARTPutchar_RB('i');

	if(Command_len < SIG100_i_PKT_LEN) return;

	if(Command_len == SIG100_i_PKT_LEN)	// MHH:16/12/2025
	{
		if(Checksum_command_pkt(SIG100_i_PKT_LEN) == false)
		{
			return;
		}
	}
	else	// MHH:16/12/2025. Allow for longer pkt which includes ac300 software version
	{
		if(Command_len == SIG100_i_PKT_LEN2)
		{
			if(Checksum_command_pkt(SIG100_i_PKT_LEN2) == false)
			{
				return;
			}
			else
			{
				Enc_data.ac300_software_version = Get_int16(Command_string+4);	// Save ac300_software version
			}
		}
		else
		{
			return;
		}
	}



	/*
	 *  *  Packet 'i' return
	 *
	 *  	0			'>' Header
	 *  	1			'i'
	 *  	2-3			Hub ID
	 *  	4-5			Update count
	 *  	6-7			Hub software version
	 *      8			checksum
*/

	Enc_data.ac200_run = Get_int16(Command_string+2);	// Save ac200_run

	Reply_string[0] = '>';

	Reply_string[1] = 'I';
	Put_int2(Reply_string+2,Enc_data.creation_time);
	Put_int2(Reply_string+4,Enc_data.hub_id);
	Put_int2(Reply_string+6,Enc_data.update_count);
	Put_int2(Reply_string+8,Enc_data.hub_software_version);
	Checksum_and_send_reply_pkt(11);

	ac200Enc_write_Enc_data((uint8_t *)&Enc_data.ac200_run,2,400);
	if(Command_len == SIG100_i_PKT_LEN2)	// MHH:16/12/2025. We save it in case lpc845 reboots without ac300 handshake
	{
		ac200Enc_write_Enc_data((uint8_t *)&Enc_data.ac300_software_version,2,410);
	}
	L3PRINTF("AC300 Run:%d, Version:%d\r\n",Enc_data.ac200_run,Enc_data.ac300_software_version);

	//	PRINTF("Version:%d",Enc_data.hub_software_version);
}

//------------------------------------------------------------------------------------------------------------------
/*
Sig100_send_pkt[0] = '!';	// Tell hub we want Enc_data  (128 bytes)
	Sig100_send_pkt[1] = 'j';	// Initial data
	Sig100_send_pkt[2] = 'J';	// This is a big request, make sure pkt not corrupt

	Sig100_checksum_and_send_pkt(SIG100_j_PKT_LEN);
	*/
//#define SIG100_j_PKT_LEN		4
#define SIG100_j_PKT_LEN		3
void Sig100_send_binary_data(uint8_t *data_p,size_t data_size);
void Process_j_data(void)
{
	Board_UARTPutchar_RB('j');

	if(Command_len < SIG100_j_PKT_LEN) return;

	if(Checksum_command_pkt(SIG100_j_PKT_LEN) == false)
	{
		return;
	}
//	if(Command_string[2] != 'J') return;

	/*
	 *	// OK, going to break the rules here and wait for a response.
	// Expect header of ">j"
	// Then "{ndddd...cccc}
	// where n = number of bytes expected, including crc32
	// d = data bytes
	// cccc = 4 byte crc32
	 *
	 *
	 *
	 */


	Sig100_X_putc('>');
	Sig100_X_putc('j');
	Sig100_X_putc('{');

	uint8_t *data_p = (uint8_t *)&Enc_data;
//	int data_size = 128;
	int data_size = 80;

	uint32_t crc32;
	size_t crc32_size = sizeof(crc32);
	uint8_t c = (uint8_t) (data_size + crc32_size);	// Should be 132
	Sig100_X_putc((char)c);		// Should not exceed 255

	Sig100_send_binary_data(data_p,data_size);

	crc32 = Wiki_CRC32(data_p,data_size);
	data_p = (uint8_t *)&crc32;	// Now send crc32
	Sig100_send_binary_data(data_p,crc32_size);
	Sig100_X_putc('}');

}
//--------------------------------------------------------------------------------------------
#define SIG100_k_PKT_LEN		7
void Process_k_data(void)
{
	Board_UARTPutchar_RB('k');

	if(Command_len < SIG100_k_PKT_LEN) return;

	if(Checksum_command_pkt(SIG100_k_PKT_LEN) == false)
	{
		return;
	}

	Enc_data.ac300_software_version = Get_int16(Command_string+2);	// Save ac300_software version
	ac200Enc_write_Enc_data((uint8_t *)&Enc_data.ac300_software_version,2,420);

	Reply_string[0] = '>';

	Reply_string[1] = 'k';
	Put_int2(Reply_string+2,Enc_Pos.pos);
	Put_int2(Reply_string+4,0);
	Checksum_and_send_reply_pkt(7);
	LPRINTF("AC300 Version(k):%d\r\n",Enc_data.ac300_software_version);

}
//--------------------------------------------------------------------------------------------
void Put_int2(char *cdst,uint16_t v)
{
	uint8_t *dst = (uint8_t *)cdst;
	*dst++ = v & 0xff;
	*dst   = (v >> 8);
}

extern uint16_t V_ac200_secs;
extern uint16_t V_prop_rpm;
#define SIG100_V_PKT_LEN	7
/*
 * 	Expect this command to be sent once a second from AC200 if vibration logic enabled
 */
#ifdef MH_FLASH_RECORD
extern bool Flog_save_data_flag;
void Process_V_data(void)	// Vibration command
{
	Command = NO_COMMAND;	// In case invalid
	if(Command_len != SIG100_V_PKT_LEN) return;
	if(Checksum_command_pkt(SIG100_V_PKT_LEN) == false)
	{
		return;
	}
	V_ac200_secs = Get_uint16(Command_string+2);
	V_prop_rpm   = Get_uint16(Command_string+4);

	if(V_prop_rpm != 0)
	{
		if(Flog_save_data_flag == false)
		{
			PRINTF("Starting Flash record\r\n");
			Hub_flog_start_record();
		}
	}
	else
	{
		if(Flog_save_data_flag)
		{
			PRINTF("Ending Flash record\r\n");
			Hub_flog_end_record();
		}
	}


	Command = 'V';
}
#endif


bool Calibration_command;
bool Calibrating;
CAL_state Hub_calibrate_state;
uint8_t Hub_calibrate_type;		// 1 = AC200 requested calibrate
int16_t AC200_current;

#define SIG100_X_PKT_LEN	6
//#define SIG100_X_PKT_LEN	5
char Show_buffer[32];
void Show_string(char *string,int string_len)
{
//	return;
//	if(slen == 0) return;
	char *bps = string;
	char *bpd = Show_buffer;
	for(int i=0;i<string_len;i++)
	{
		uint8_t c = *bps++;
		if(c > 32 && c < 127)
		{
			*bpd++ = c;
		}
		else
		{
			int slen = sprintf(bpd,"<%d>",c);
			bpd+= slen;
		}
		if(bpd - Show_buffer >= 30) break;
	}
	*bpd++ = 0;
	PRINTF("SL:%d,%s\r\n",string_len,Show_buffer);
}
void Show_command(void)
{
	return;
	if(Command_len == 0) return;
	PRINTF("Cmd:");
	Show_string(Command_string,Command_len);
}

void Process_X_data(void)
{
	Command = NO_COMMAND;	// In case invalid
//	Show_command();
	if(Command_len != SIG100_X_PKT_LEN) return;
	if(Checksum_command_pkt(SIG100_X_PKT_LEN) == false)
	{
		return;
	}
	Hub_calibrate_state = (CAL_state)Command_string[2];
	if(Hub_calibrate_state == CAL_START)
	{
		Enc_data.cal_pole_pairs = Command_string[3];
		Hub_calibrate_type = Command_string[4];		// MHH:26/08/2023
	}
	else
	{
		AC200_current = Get_uint16(Command_string+3);

		if(AC200_current < 50) AC200_current = 0;	// MHH:09/01/2026
#ifdef MH_XXX
		if(AC200_current > 100)	// DEBUG!!! MHH:09/01/2026
		{
			AC200_current -= 150;
			if(AC200_current < 0) AC200_current = 0;
		}
#endif
#ifdef MH_XXX

		int rpin = Board_pin_read(READY_PIN);
		int e1 = Board_pin_read(ENABLE_PIN);
		int e2 = Board_pin_read(ENABLE_PIN2);
		PRINTF(",%d:%d:%d:%d:%d",AC200_current,DAC0_v,e1,e2,rpin);
#endif
	}
	Command = 'X';
	Calibration_command = true;
}
//-----------------------------------------------------------------------------------------------------------
#ifndef MH_SIG100_RB
void Sig100_clear_input(void)
{
	int ci;

	for(;;)
	{
		ci = Sig100_getc_timeout(2);
		if(ci == EOF) return;
	}
}
#endif
//------------------------------------------------------------------------------------------------------------
void Sig100_rcv_pkt(void)
{
	int ci;
	for(;;)
	{
		ci = Sig100_X_getc_timeout(2);
		if(ci == EOF)
		{
			break;
		}
		if(Command_len < COMMAND_MAX_LEN)
		{
			Command_string[Command_len++] = (char)ci;
		}
	}
	if(Command_len >= COMMAND_MAX_LEN) Command_len = COMMAND_MAX_LEN - 1;	// min(len,max_len-1)
}
//---------------------------------------------------------------------------------------------------------------
void Set_command_speed(char cspeed,int ifrom)
{
	if(cspeed == Command_speed) return;
	Command_speed = cspeed;
#ifdef MH_XXX
	int speed = 10;
	if(cspeed >= '0' && cspeed <= '9') speed = cspeed - '0';
	PRINTF("speed:%d from:%d\r\n",speed,ifrom);
#endif
}
#define COMMAND_PREFIX	'|'


int Sig100_X_getc_timeout(int millisecs)
{
#ifdef MH_SIG100_RB
		return Sig100_RB_getc_timeout(millisecs);
#else
		return Sig100_getc_timeout(millisecs);		// Not getting RPM display. Try shortening timeout???
#endif

}
void Get_movement_command(char command)
{
	Command_string[0] = command;
	Command_len = 1;
	Sig100_rcv_pkt();
	if(Command_len == 3)
	{
		if(Checksum_command_pkt(3))
		{
			Command = command;
			char speed = Command_string[1];
			Set_command_speed(speed,210);
		}
	}
}
void Get_command(void)
{

	int ci;
	int speed;
	Command_len = 0;
	Command = 0;
//	Command_speed = 0;
//	Arg1 = 0;
//	Arg2 = 0;

	Calibration_command = false;
//	for(int i=0;i<10;i++) Command_string[i]=0;		// Debug...
#ifdef MH_XXX
	if(Sig100_RB_bytes_avail() == 0)
	{
#ifdef MH_ADXL375
		// Note: Can put any routine in here that takes less than a millisec.
		Adxl375_read_xyz_if_ready();
		if(Sig100_RB_bytes_avail() == 0) return;

#endif
	}
#endif
	for(;;)
	{
//		ci = Sig100_getc_timeout(10);
//		ci = Sig100_X_getc_timeout(5);
		ci = Sig100_X_getc_timeout(10);		// MHH:15/12/2023. Try 10 now that we have ring buffer and can do things while we wait

		if(ci == '.')
		{
			Command = '.';
#ifdef MH_SIG100_RB
			Sig100_RB_clear_input();
#else
			Sig100_clear_input();
#endif
			return;
		}
		if(ci == '#')		// MHH:15/10/2025. Attempt to see exactly when disable happens
		{
			AC300_Tick = Sig100_X_getc_timeout(2);
			Command = '#';
			Sig100_RB_clear_input();
			return;
		}
		if(ci == COMMAND_PREFIX) break;
		if(ci == EOF)
		{
			return;
		}
	}

// The only way we can get here is if we have a command prefix. Now to identify the command...
	if(Enc_data.ac300_software_version <= 10141)
	{
		Set_command_speed(0,100);
	}

	ci = Sig100_X_getc_timeout(2);
	char command = (char)ci;
	switch(ci)
	{
	default:	// Only here if bad command
		break;

	case EOF:
		break;				// MHH:07/01/2026
//		return;

	case '+':
		if(Enc_data.ac300_software_version <= 10141)
		{
			ci = Sig100_X_getc_timeout(2);
			speed = Sig100_X_getc_timeout(2);		// May not be there.
			if(ci == 'p')
			{
				Command = command;
				if(speed > 0) Set_command_speed(speed,200);
			}
		}
		else
		{
			Get_movement_command(command);
		}
		break;

	case '-':
		if(Enc_data.ac300_software_version <= 10141)
		{
			ci = Sig100_X_getc_timeout(2);
			speed = Sig100_X_getc_timeout(2);		// May not be there.
			if(ci == 'm')
			{
				Command = command;
				if(speed > 0) Set_command_speed(speed,300);
			}
		}
		else
		{
			Get_movement_command(command);
		}
		break;

	case 'A':	// MHH:30/06/2023. A=Amps. Used for testing in manual mode.
		Command_string[0] = command;
		Command_len = 1;
		Sig100_rcv_pkt();
//		if(Command_len == 4)
		if(Command_len == 5)
		{
			Command = command;
			return;
		}
		break;


	case 'C':	// Command mode
		ci = Sig100_X_getc_timeout(2);
		if(ci != '!') break;
//		AC200_SIG100_putstr("C!mode=1");
		Command_string[0] = command;
		Command_string[1] = '!';
		Command_len = 2;
		Sig100_rcv_pkt();
		Check_command_mode();	// May not come back from this...
//		return;
		break;				// MHH:07/01/2026

	case 'F':
		ci = Sig100_X_getc_timeout(2);
		if(ci == 'e')
		{
			Command = command;
			Set_command_speed(0,110);
		}
		break;

	case 'H':
		ci = Sig100_X_getc_timeout(2);
		if(ci == 't') Command = command;
		break;

	case 'R':
		ci = Sig100_X_getc_timeout(2);
		if(ci == 'v')
		{
			Command = command;
			Set_command_speed(0,120);
		}
		break;


	case 'P':	// Position command
	case 'p':
		Command_string[0] = command;
		Command_len = 1;
		Sig100_rcv_pkt();
		if(Command_len == 5)
		{
			Command = command;
//			return;		// MHH:07/01/2026
		}
		break;

#ifdef MH_FLASH_RECORD
	case 'V':			// Vibration command
		ci = Sig100_X_getc_timeout(2);
		if(ci != 'i') break;
		Command_string[0] = command;
		Command_string[1] = 'i';
		Command_len = 2;
		Sig100_rcv_pkt();
		Process_V_data();
		return;
#endif

	case 'X':	// Calibrate command
		ci = Sig100_X_getc_timeout(2);
		if(ci != 'c') break;
		Command_string[0] = command;
		Command_string[1] = 'c';
		Command_len = 2;
		Sig100_rcv_pkt();
		Process_X_data();
//		return;
		break;		// MHH:07/01/2026. Not clearing input buffer can result in getting out of sync with AC300 current when under debug, causing
					// motor to not stop at hard limits.

	case 'i':
		ci = Sig100_X_getc_timeout(2);
		if(ci != 'I') break;
		Command_string[0] = 'i';
		Command_string[1] = 'I';
		Command_len = 2;
		Sig100_rcv_pkt();
		Process_i_data();
//		return;
		break;				// MHH:07/01/2026

	case 'j':
		ci = Sig100_X_getc_timeout(2);
		if(ci != 'J') break;
		Command_string[0] = 'j';
		Command_string[1] = 'J';
		Command_len = 2;
		Sig100_rcv_pkt();
		Process_j_data();
//		return;
		break;				// MHH:07/01/2026

	case 'k':
		ci = Sig100_X_getc_timeout(2);
		if(ci != 'K') break;
		Command_string[0] = 'k';
		Command_string[1] = 'K';
		Command_len = 2;
		Sig100_rcv_pkt();
		Process_k_data();
//		return;
		break;				// MHH:07/01/2026


	}
#ifdef MH_SIG100_RB
	Sig100_RB_clear_input();
#else
	Sig100_clear_input();
#endif
	return;
}
//--------------------------------------------------------------------------------------------------------
//bool  MH_test_error=true;
void Receive_check_fine_stop(void)
{
	if(Calibration_command) return;

	if(Enc_fine_rise_error)
	{
		Enc_fine_rise_error = false;
		Sig100_format_error_pkt('E',HUB_ERR_POS_FINESTOP,Enc_fine_ms_pos_rise);
	}
}
void Receive_check_coarse_stop(void)
{
	if(Calibration_command) return;

	if(Enc_coarse_rise_error)
	{
		Enc_coarse_rise_error = false;
		Sig100_format_error_pkt('E',HUB_ERR_POS_COARSESTOP,Enc_coarse_ms_pos_rise);
	}
}
//--------------------------------------------------------------------------------------------------------
bool Heater_on;
bool Display_command;
uint32_t Command_total;
uint32_t Put_systick_last;
extern char Motor_command;
void Motor_manual(void);
void Motor_manual2(bool kb_input);
//bool Force_error;

void Test_encoder_update_speed(void)
{
    PRINTF("\r\nTest_encoder_update_speed:\r\n");
    Timer_reset(&Enc_timer);

    for(int i=0;i<100;i++)
	{
    	if(i < 50)
    	{
    		Enc_Pos.pos++;
    	}
    	else
    	{
    		Enc_Pos.pos--;
    	}
    	Put_encoder_pos(51);

	}
	uint32_t enc_usecs = Timer_read_us(&Enc_timer);

	PRINTF("usecs for 1 loop:%d\r\n",enc_usecs/100);
}
//bool E_coarse_ms_on;
//bool Coarse_ms_on;
void Receive_SIG100(void)
{
//    static int save_rpm;

//	Test_encoder_update_speed();

    PRINTF("\r\nReceive_SIG100:\r\n");

    for(;;)
    {
    	Sig100_RB_clear_input();		// MHH:09/01/2026
//    	c = Sig100_UARTGetChar();
    	Hub_watchIt(WD_AC200HUBL);

//#ifdef MH_DIAGS
#ifdef MH_XXX
    	if(Flog_diags_enabled)
    	{
        	Hub_flog_check_ring_buffer();
    	}
#endif
//#endif
    	Receive_check_fine_stop();
    	Receive_check_coarse_stop();  				// MHH:22/12/2023
    	Enc_pos_report_error();							// Only if error...
//    	if(Systick_tick - Put_systick_last >= 8)		// MHH:26/06/2025. Remove condition. Only takes 125 usecs to update 6 bytes. Once every 8th systick, about 12 times a second
    	{
    		Put_systick_last = Systick_tick;
    		Put_encoder_pos(50);
    	}

    	Get_command();
    	if(Command == 0) continue;		// MHH:26/06/2026

    	Show_command();
    	Hub_watchIt(WD_AC200HUBL+50);

#ifdef MH_XXX
    	if(Enc_coarse_microswitch_on != E_coarse_ms_on)
    	{
    		E_coarse_ms_on = Enc_coarse_microswitch_on;
    		PRINTF("E_CS != ECS:");
    		if(E_coarse_ms_on)
    		{
    			PRINTF("ON\r\n");
    		}
    		else
    		{
    			PRINTF("OFF\r\n");
    		}
    	}
    	bool coarse_ms_on = Board_pin_read(COARSE_MICROSWITCH_PIN);
    	if(coarse_ms_on != Coarse_ms_on)
    	{
    		Coarse_ms_on = coarse_ms_on;
    		if(coarse_ms_on)
    		{
    			PRINTF("C_on\r\n");
    		}
    		else
    		{
    			PRINTF("C_off\r\n");
    		}
    	}
#endif
    	Enc_check_fine_microswitch_on();
    	Enc_check_coarse_microswitch_on();

#ifdef MH_WATCHDOG_TEST
    	if(Enc_data.hub_run & 1)		// Test watchdog
    	{
    		PRINTF("Watchdog test\r\n");
    		wait_ms(100);
    		while(1);		// infinite loop
    	}
#endif
#ifdef MH_HARDFAULT_TEST
    	if(Enc_data.hub_run & 1)		// Test hard fault
    	{
    		PRINTF("Hard fault test\r\n");
    		wait_ms(100);
        	Hub_Force_Hardfault();
    	}
#endif

    	if(Sig100_hub_error_ready)
    	{
    		Sig100_hub_error_ready = false;
    		Sig100_send_error();
    		continue;
    	}

    	/*
    	 *  MHH:04/08/2023. Noticed that UL Hub was moving at zero speed (very slowly) after reprogramming and recalibrating.
    	 *  Following logic to try and trap error if it happens again.
    	 */

    	if(Board_pin_read(ENABLE_PIN) == false && Board_pin_read(ENABLE_PIN2) == false)	// MHH:04/08/2023. Test pins for enabled state
    	{
    		if(DAC0_v == 0)		// speed = 0
    		{
    			Disable_Motor(400);
    			L2PRINTF("ReceiveSIG100:Enabled & zero speed\r\n");
    			Sig100_format_error_pkt('E',HUB_ERR_RPM_ZERO,400);
    			continue;
    		}
    	}

    	if(Manual_mode != MAN_IDLE)
    	{
    		if(Calibrating == false)
    		{
    			Dputs("Calibrating==false\r\n");
    		}
    		if(Motor_main_mode == false)
    		{
    			Dputs("Motor_main_mode==false\r\n");
    		}
    	}


#ifdef MH_XXX    // MHH:30/06/2023. Now have ISR3 to detect not ready.
    	int ready_pin = Board_pin_read(READY_PIN);
    	if(ready_pin == 0)
    	{
    		PRINTF("ready_pin=0\r\n");
    	}
#endif
    	if(Calibration_command)
    	{
//    		Display_command = true;
//    		Board_UARTPutchar_RB('X');
//    		Board_UARTPutchar_RB(Command);
    		Calibrating = true;
   	    	Motor_main_mode = true;
    		if(Process_calibration_command(Hub_calibrate_state))
    		{
    			Systick_count = 0;		// Valid command
  //   	    	ADC_sample();		// Don't think voltage needed for calibration
    			Sig100_send_calibration_response();
    		}
    		continue;
    	}


    	/*
    	if(Display_command)
    	{
    		Board_UARTPutchar_RB(Command);
    	}
*/
    	if(Command != NO_COMMAND)
    	{
    		Hub_watchIt(WD_AC200HUBL+100);
    		Command_total++;
#ifdef MH_MOTOR_TIME_TEST
        	Last_command = Command;
#endif
#ifdef MH_BBB
        	if(Command_total == 100)	// DEBUG!!!!
        	{
        		Sig100_test_error();
        	}
#endif

//    		Calibrating = false;

     		if(Command == 'A')
    		{
//    			AC200_current = Get_uint16(Command_string+1);

//    	    	Motor_main_mode = true;	// MHH:06/07/2023, commented out on 02/09/2023
    			Disable_Motor(500);		// MHH:18/10/2023
    			AC200_current = Get_uint16(Command_string+2);
    			if(AC200_current < 50) AC200_current = 0;	// MHH:14/06/2026
    			/*
    			 * 			Field		Offset	length	Description
    			 * 			id			0		1		Identifier = '@'
    			 * 			flags		1		1		flags. eg FINE_STOP, NOT_READY
    			 * 			voltage		2		1		usual format
    			 * 			type		3		1		0 = Enc_Position
    			 * 										1 = Coarse_hard_stop
    			 * 										2 = Coarse MS stop
    			 * 										3 = Fine MS stop (1)
    			 * 										4 = Fine hard stop
    			 * 										5 = Fine MS stop (2)
    			 * 										6 = finished calibrate
    			 * 										Note: If needed, could just use 4 bits, and other 4 bits for something else
    			 *			stop_pos	4		2
    			 *			pos			6		2		2 byte integer
    			 *			checksum 	8		1
    			 *
    			 */
//    			Spkt[0] = '@';
    			Spkt[0] = '*';		// MHH:01/09/2023
#ifdef MH_XXX
    			int flags = 0;
    			if(Board_pin_read(FINE_MICROSWITCH_PIN)) flags |= STOP_FINE;
    			if(Board_pin_read(COARSE_MICROSWITCH_PIN)) flags |= STOP_COARSE;
    			if(Board_pin_read(READY_PIN) == 0) flags |= NOT_READY_BIT;
#endif
    			Spkt[1] = Sig100_get_pkt_flags();
    			Spkt[2] = ADC_adjusted_voltage;
    			ADC_adjusted_temp = ADC_avg_temp >> 4;	// Divide by 16 to fit into a byte
    			Spkt[3] = ADC_adjusted_temp;	// Reserved for temperature
//    			Spkt[3] = 0;		// For now
//    			Spkt[4] = 0;		// stop pos
    			Spkt[4] = (uint8_t)(Enc_rpm >> 5);	// MHH:14/08/2025. Divide by 32
//    			Spkt[5] = 0;
    			Spkt[5] = (uint8_t)(DAC0_v >> 2);	// Max val 1023, so divide by 4
    			int pos = Enc_Pos.pos;	// Will want to change this later, depending on type
    			if(pos < 0) pos = 0;			// MHH:22/07/2023. Can happen if verifying.
    			Put_uint16(Spkt+6,pos);
//    			Spkt[5] = (uint8_t)(pos & 0xff);	// low order byte
//    			Spkt[6] = (uint8_t)(pos >> 8);	// high order byte

    			Checksum_and_send_pkt(9);
    			Motor_manual2(true);
    			continue;
    		}
    		Hub_watchIt(WD_AC200HUBL+200);


			if(Hub_maxon_driver)
			{
	    		if(Board_pin_read(READY_PIN) == 0)
	    		{
					Disable_Motor(200);
	    			Motor_command = 0;
	        		Sig100_send_response();
	        		L2PRINTF("A:RDY=0");	// MHH:07/07/2025
	        		continue;
	    		}
			}
			if(MCT8316_check_reset(50))
			{
				Disable_Motor(210);
    			Motor_command = 0;
				continue;
			}
#ifdef MH_XXX    // MHH:30/06/2023. Think this is redundant...
    		if(Command != '.')
    		{
//    			if(Command != 'p') Board_UARTPutchar_RB(Command);
    			if(Command == '!')
    			{
    				Disable_Motor(200);
    				Command = '!';	// Debug
    			}
    			Board_UARTPutchar_RB(Command);
    		}
#endif
//    		Board_UARTPutchar_RB((char) c);

    		if(Command == 'H')	// Heater?
    		{
    			Board_turn_heater_on();
 //   			Board_pin_write(HEATER_PIN,true);
 //  			Board_pin_write(HEATER2_PIN,true);
    			Command = '.';
    		}
    		else
    		{
    			Board_turn_heater_off();
//    			Board_pin_write(HEATER_PIN,false);
//    			Board_pin_write(HEATER2_PIN,false);
    		}

    		if(Manual_mode != MAN_IDLE)
    		{
    			Dputs("Mm!=idle\r\n");
    			PRINTF("Command:%c,%d\r\n",Command,Command);
    		}
    		Hub_watchIt(WD_AC200HUBL+300);
    		Motor_main_mode = false;	// MHH:06/07/2023
    		if(Motor_process_command(Command))
    		{
    			Systick_count = 0;		// Valid command
    			Sig100_send_response();
    		}
    	}
    }
}

void Send_SIG100(void)
{
    printf("\r\nSend_SIG100:\r\n");
    char send_char = 'a';
    for(;;)
    {
    	Sig100_X_putc(send_char);
    	send_char++;
    	if(send_char > 'z')
    	{
    		send_char = 'a';
        	Sig100_X_putc(13);
        	Sig100_X_putc(10);
    	}

      	int kb = Board_UARTGetChar_RB();	// Could do this once every (say) 10 loops
		if(kb == 'X' || kb == 'x')
		{
			return;
		}
    }
}

void Sig100_ini(void)
{
	setup_sig100_uart();
//	Sig100_UARTPutSTR("\r\nSIG100 Hello World\r\n");
    SIG100_rb_init();
//    Sig100_X_puts("\r\nSIG100 Hello World\r\n");	// MHH:09/01/2026


}
//----------------------------------------------------------------------------------------------
void DAC_init(void)
{
	//	  printf("DAC example\n\r");
	LPC_SYSCON->SYSAHBCLKCTRL[0] |= SWM|IOCON|DAC0;

	//	  Enable_Periph_Clock(CLK_DAC0);

	// Enable DACOUT on its pin
	LPC_SWM->PINENABLE0 &= ~(DACOUT0);

	// Configure the DACOUT pin. Inactive mode (no pullups/pulldowns), DAC function enabled
	uint32_t temp = (LPC_IOCON->DACOUT0_PIN) & (IOCON_MODE_MASK) & (IOCON_DACEN_MASK);
	temp |= (0<<IOCON_MODE)|(1<<IOCON_DAC_ENABLE);
	LPC_IOCON->DACOUT0_PIN = temp;

	// Power to the DAC!
	LPC_SYSCON->PDRUNCFG &= ~(DAC0_PD);

}
//==============================================================================================
//const int AC200Enc_version = 201;		// 2 implied decimal places. Not working!!
//#define AC200ENC_VERSION  102		// 2 implied decimal places
void Enc_Init(void);
void Motor_main(void);

//--------------------------------------------------------------------------------------------------------------
void LPC845_reboot(void)
{
//	for(;;);

// What do we do if in an Abort loop, eg if logging not working?
// Could test a pin (DIM pin or similar) and if active then put in loop and turn off watchdog.
// This would allow me to load debugger.

	NVIC_SystemReset();
}

void dummy_call(const unsigned char *version)
{
}
// If data is in old format (tdc relative) convert to CHS relative
#ifdef MH_XXX
void Convert_tdc_to_chs(uint16_t *u16_p)
{
	uint16_t u16 = *u16_p;
	int v1 = (int16_t)u16;	// Get signed value
	int v2 = Enc_data.cal_tdc_pos_offset;
	int v3 = v2 - v1;
	u16 = (uint16_t)v3;
	*u16_p = u16;
}

void Enc_data_init(void)
{
	if(Enc_data.cal_fine_stop < 60000) return;

	Dputs("Converting tdc data\r\n");
/*
 * uint16_t ud_coarse_stop;	// 24. User defined coarse stop
	uint16_t ud_feather_stop;	// 26.
	uint16_t ud_reverse_stop;	// 28.

	uint16_t cal_fine_stop;		// 30.
//	uint16_t cal_coarse_hard_stop;	// 32.
	uint16_t cal_tdc_pos_offset;		// 32. MHH:04/07/2023. Same value, different name.
	uint16_t cal_fine_hard_stop;
 */
	Convert_tdc_to_chs(&Enc_data.cal_fine_stop);
	Convert_tdc_to_chs(&Enc_data.cal_fine_hard_stop);
	Convert_tdc_to_chs(&Enc_data.cal_coarse_stop);
	Convert_tdc_to_chs(&Enc_data.ud_feather_stop);
	Convert_tdc_to_chs(&Enc_data.ud_reverse_stop);
}
#endif
bool IsTrue(Boolean b,int ifrom)
{
	if(b == True) return true;
	if(b == False) return false;

	PRINTF("IsTrue:Invalid bool value from:%d\r\n",ifrom);
	DebugAbort("IsTrue: Bad value");
	return false;
}
extern uint32_t MR_time_val;

#ifdef MH_XXX		// MHH:26/06/2026
void Timer_interrupt_init(void)
{
//	LPC_CTIMER0->TCR &= ~(1<<CEN);		// Disable timer. Not sure if necessary
//	MR_time_val = LPC_CTIMER0->TC + MR_TIME_INTERVAL;
	LPC_CTIMER0->TCR = 2;		// Disable timer and reset. Note: This is likely to upset any timing logic currently happening.

//	LPC_CTIMER0->TC = 0;
//	MR_time_val = LPC_CTIMER0->TC + MR_TIME_INTERVAL;
	LPC_CTIMER0->MR[0] = MR_TIME_INTERVAL;

		// Configure the Match Control register to interrupt and reset on match in MR0
//    LPC_CTIMER0->MCR = (1<<MR0I)|(1<<MR0R);	// Try interrupt and reset...??
	LPC_CTIMER0->MCR = (1<<MR0I);	// Just interrupt, because we want to use CTIMER as timer too.

		// Enable the Ctimer0 interrupt
	NVIC_EnableIRQ(CTIMER0_IRQn);
	LPC_CTIMER0->TCR = 1<<CEN;		// Enable
//	MR_time_val = LPC_CTIMER0->TC + MR_TIME_INTERVAL;
//	LPC_CTIMER0->MR[0] = LPC_CTIMER0->TC + MR_TIME_INTERVAL;
//	MR_time_val = LPC_CTIMER0->MR[0];

}

void Timer_interrupt_disable(void)
{
	NVIC_DisableIRQ(CTIMER0_IRQn);
}

#endif

void Timer_init(void)
{
	/*
	 *  This relies on SystemCoreClockUpdate(); executed in setup_debug_uart.
	 */
	SystemCoreClockUpdate();	// main_clk and fro_clk obtained from this call

	LPC_SYSCON->SYSAHBCLKCTRL[0] |= CTIMER0;		// Enable Timer
	uint32_t prescale_register = (system_ahb_clk / 1000000) - 1;
	LPC_CTIMER0->PR = prescale_register;
//	LPC_CTIMER0->PR = 14;		// Test???
	//	  LPC_CTIMER0->PR = 14;		// Set Prescale Register so that we get 1 Mhz. Assume APB bus clock = 15 Mhz
	//	  LPC_CTIMER0->PR = 29;		// Set Prescale Register so that we get 1 Mhz. Assume APB bus clock = 30 Mhz
	//	  LPC_CTIMER0->TCR = 1;		// Enable counter

//#ifdef MH_XXX
//#ifdef MH_ADXL1001
//	if(Flog_diags_enabled)	// Note: We may be able to postpone this until first AC200 diagnostic pkt
	{
		// Set the PWM period in counts/cycle of the TC
		LPC_CTIMER0->MR[0] = MR_TIME_INTERVAL;

		// Configure the Match Control register to interrupt and reset on match in MR0
//	    LPC_CTIMER0->MCR = (1<<MR0I)|(1<<MR0R);
		LPC_CTIMER0->MCR = (1<<MR0I);	// Just interrupt.

		// Enable the Ctimer0 interrupt
		NVIC_EnableIRQ(CTIMER0_IRQn);
	}
//#endif
//#endif
	// Start the action
	LPC_CTIMER0->TCR = 1<<CEN;
}

//#define PWM_RES	100
#ifdef MH_XXX

#define PWM_PERIOD        500
#define C_match_red_pin_OFF   100
#define C_match_red_pin_ON  600
#else
#define PWM_PERIOD        1500
#define C_match_red_pin_OFF   100
#define C_match_red_pin_ON  (PWM_PERIOD+1)
#endif
enum sct_state {blink_red = 0, blink_green = 1};
void Init_SCTimer_PWM(void)
{
	  // Enable clocks to relevant peripherals
	  LPC_SYSCON->SYSAHBCLKCTRL0 |= (SWM|SCT);

	  // Configure the SWM (see utilities_lib and lpc8xx_swm.h)
	#ifdef MH_IN_PORT
	  ConfigSWM(SCT_PIN0, IN_PORT);   // SCT input 0 on port pin, for input transition events (default internal pull-up remains on)
	#endif
	//  ConfigSWM(SCT_OUT0, LED_GREEN); // SCT output 0 on green LED port pin (0=on, 1=off)
	//  ConfigSWM(SCT_OUT1, LED_RED);   // SCT output 1 on red LED port pin (0=on, 1=off)
	  ConfigSWM(SCT_OUT0, P0_17); // SCT output 0 on green LED port pin (0=on, 1=off)
//	  ConfigSWM(SCT_OUT1, LED_GREEN);   // SCT output 1 on red LED port pin (0=on, 1=off)

	#ifdef MH_IN_PORT
	  // Write to SCT0_INMUX[0] to select SCT_PIN0 function (which was connected to IN_PORT in the switch matrix) as SCT input SCT_IN0
	  LPC_INMUX_TRIGMUX->SCT0_INMUX0 = SCT_INMUX_SCT_PIN0;
	#endif
	  // Configure the SCT ...
	  // Give the module a reset
	  LPC_SYSCON->PRESETCTRL0 &= (SCT0_RST_N);
	  LPC_SYSCON->PRESETCTRL0 |= ~(SCT0_RST_N);

	  // Configure the CONFIG register
	  // UNIFY counter, CLKMODE=busclock, CKSEL=unused(default), NORELOADL/H=unused(default), INSYNC=unused(default), AUTOLIMIT=true
	  LPC_SCT->CONFIG |= (1<<UNIFY) |
	                     (Bus_clock<<CLKMODE) |
	                     (1<<AUTOLIMIT_L);

	  // Configure the CTRL register
	  // Don't run yet, clear the counter, BIDIR=0(default,unidirectional up-count), PRE=0(default,div-by-1)
	  LPC_SCT->CTRL   |= (0<<Stop_L) |       // Stay in halt mode until SCT setup is complete
	                     (1<<Halt_L) |       // Stay in halt mode until SCT setup is complete
	                     (1<<CLRCTR_L) |     // Clear the counter (good practice)
	                     (0<<BIDIR_L) |      // Unidirectional mode (Up-count)
	                     (0<<PRE_L);         // Prescaler = div-by-1

	  // Setup the LIMIT register
	  // No events serve as counter limits because we are using the AUTOLIMIT feature of match0 (see the CONFIG reg. config.)
	  LPC_SCT->LIMIT_L = 0;

	  // Setup the HALT register
	  // No events will set the HALT_L bit in the CTRL reg.
	  LPC_SCT->HALT_L = 0;

	  // Setup the STOP register
	  // No events will set the STOP_L bit in the CTRL reg.
	  LPC_SCT->STOP_L = 0;

	  // Setup the START register
	  // No events will clear the STOP_L bit in the CTRL reg.
	  LPC_SCT->START_L = 0;

	  // Initialize the COUNT register
	  // Start counting at '0'
	  LPC_SCT->COUNT = 0;

	  // Initialize the STATE register
	  // Start in state 0
	//  LPC_SCT->STATE_L = blink_green;
	  LPC_SCT->STATE_L = blink_red;

	  // Setup the REGMODE register
	  // All Match/Capture registers act as match registers
	  LPC_SCT->REGMODE_L = 0;

	  // Configure the OUTPUT register
	  // Initialize CTOUT_0 and CTOUT_1 to '1' so the LEDs are off to begin with
	  LPC_SCT->OUTPUT = (1<<0)|(1<<1);

	  // Configure the OUTPUTDIRCTRL register
	  // The counting direction has no impact on the meaning of set and clear for all outputs
	  LPC_SCT->OUTPUTDIRCTRL = 0;

	  // Configure the RES register
	  // Simultaneous set and clear (which would be a programming error, and won't happen here) has no effect for all outputs
	  LPC_SCT->RES = 0;

	  // Configure the EVEN register
	  // This example does not use interrupts, so don't enable any event flags to interrupt.
	  LPC_SCT->EVEN = 0;

	  // Clear any pending event flags by writing '1's to the EVFLAG register
	  LPC_SCT->EVFLAG = 0x3F;

	  // Configure the CONEN register
	  // This example does not use interrupts, so don't enable any 'no-change conflict' event flags to interrupt.
	  LPC_SCT->CONEN = 0;

	  // Clear any pending 'no-change conflict' event flags, and BUSSERR flags, by writing '1's to the CONLAG register
	  LPC_SCT->CONFLAG = 0xFFFFFFFF;

	  // Configure the match registers (and their associated match reload registers, which will be the same for this example)
	  // for the PWM duty cycles desired
	  LPC_SCT->MATCH[0].U =    PWM_PERIOD;         // Match0 is the AUTOLIMIT event, determines the period of the PWM
	  LPC_SCT->MATCHREL[0].U = PWM_PERIOD;         // "
	  LPC_SCT->MATCH[1].U =    C_match_red_pin_OFF;   // green LED on, CTOUT_0/P0.17 = '0'
	  LPC_SCT->MATCHREL[1].U = C_match_red_pin_OFF;   // "
	  LPC_SCT->MATCH[2].U =    C_match_red_pin_ON;  // green LED off, CTOUT_0/P0.17 = '1'
	  LPC_SCT->MATCHREL[2].U = C_match_red_pin_ON;  // "
	#ifdef MH_GREEN_LED
	  LPC_SCT->MATCH[3].U =    C_match_green_ON;     // red LED on, CTOUT_1/P0.7 = '0'
	  LPC_SCT->MATCHREL[3].U = C_match_green_ON;     // "
	  LPC_SCT->MATCH[4].U =    C_match_green_OFF;    // red LED off, CTOUT_1/P0.7 = '1'
	  LPC_SCT->MATCHREL[4].U = C_match_green_OFF;    // "
	#endif
	  // Configure the EVENT_STATE and EVENT_CTRL registers for all events
	  //
	  // Event EVENT_STATE                        EVENT_CTRL
	  // ----- ---------------------------------  -------------------------------------------------------------------------------------------
	  // EV0   Enabled in State 0 (blink_green).  A match in Match1 is associated with this event (green on), no effect on state.
	  // EV1   Enabled in State 0 (blink_green).  A match in Match2 is associated with this event (green off), no effect on state.
	  // EV2   Enabled in State 0 (blink_green).  A match in Match0 AND CTIN_0 low is associated with this event, changes state from 0 to 1.
	  // EV3   Enabled in State 1 (blink_red).    A match in Match3 is associated with this event (red on), no effect on state.
	  // EV4   Enabled in State 1 (blink_red).    A match in Match4 is associated with this event (red off), no effect on state.
	  // EV5   Enabled in State 1 (blink red).    A match in Match0 AND CTIN_0 high is associated with this event, changes state from 1 to 0.
	#ifdef MH_XXX
	  LPC_SCT->EVENT[0].STATE = 1<<blink_green; // Event0 is enabled in state 0
	  LPC_SCT->EVENT[1].STATE = 1<<blink_green; // Event1 is enabled in state 0
	  LPC_SCT->EVENT[2].STATE = 1<<blink_green; // Event2 is enabled in state 0
	  LPC_SCT->EVENT[3].STATE = 1<<blink_red;   // Event3 is enabled in state 1
	  LPC_SCT->EVENT[4].STATE = 1<<blink_red;   // Event4 is enabled in state 1
	  LPC_SCT->EVENT[5].STATE = 1<<blink_red;   // Event5 is enabled in state 1
	#else
	  LPC_SCT->EVENT[0].STATE = 1<<blink_red; // Event0 is enabled in state 0
	  LPC_SCT->EVENT[1].STATE = 1<<blink_red; // Event1 is enabled in state 0
	#ifdef MH_GREEN_LED
	  LPC_SCT->EVENT[2].STATE = 1<<blink_red; // Event2 is enabled in state 0
	  LPC_SCT->EVENT[3].STATE = 1<<blink_green;   // Event3 is enabled in state 1
	  LPC_SCT->EVENT[4].STATE = 1<<blink_green;   // Event4 is enabled in state 1
	  LPC_SCT->EVENT[5].STATE = 1<<blink_green;   // Event5 is enabled in state 1
	#endif
	#endif
	  LPC_SCT->EVENT[0].CTRL  = (0x1<<MATCHSEL)|(Match_Only<<COMBMODE)|(0<<STATELD)|(0<<STATEV);
	  LPC_SCT->EVENT[1].CTRL  = (0x2<<MATCHSEL)|(Match_Only<<COMBMODE)|(0<<STATELD)|(0<<STATEV);
	//  LPC_SCT->EVENT[2].CTRL  = (0x0<<MATCHSEL)|(0<<OUTSEL)|(0<<IOSEL)|(LOW<<IOCOND)|(Match_and_IO<<COMBMODE)|(1<<STATELD)|(blink_red<<STATEV);

	#ifdef MH_GREEN_LED
	  LPC_SCT->EVENT[3].CTRL  = (0x3<<MATCHSEL)|(Match_Only<<COMBMODE)|(0<<STATELD)|(0<<STATEV);
	  LPC_SCT->EVENT[4].CTRL  = (0x4<<MATCHSEL)|(Match_Only<<COMBMODE)|(0<<STATELD)|(0<<STATEV);
	  LPC_SCT->EVENT[5].CTRL  = (0x0<<MATCHSEL)|(0<<OUTSEL)|(0<<IOSEL)|(HIGH<<IOCOND)|(Match_and_IO<<COMBMODE)|(1<<STATELD)|(blink_green<<STATEV);
	#endif

	  // Configure the OUT registers for the 4 SCT outputs
	  // Event 0 clears output CTOUT_0 to '0'   (green LED on)
	  // Event 1 sets output CTOUT_0 to '1' (green LED off)
	  // Event 3 clears output CTOUT_1 to '0'   (red LED on)
	  // Event 4 sets output CTOUT_1 to '1' (red LED off)
	  LPC_SCT->OUT[0].SET = 1<<event1;
	  LPC_SCT->OUT[0].CLR = 1<<event0;

	  LPC_SCT->OUT[3].SET = 0; // Unused output, set by no event (default)
	  LPC_SCT->OUT[3].CLR = 0; // Unused output, cleared by no event (default)
	  // FINALLY ... now let's run it. Clearing bit 2 of the CTRL register takes it out of HALT.
	  LPC_SCT->CTRL_L &= ~(1<<Halt_L);
}
int PWM_duty_10bit;
void PWM_SetDutyTenBit(int pwm_duty_10bit)
{
	int red_pin_off = 0;
	int red_pin_on = 0;

	PWM_duty_10bit = pwm_duty_10bit;


	if(pwm_duty_10bit == 0)
	{
//		red_pin_on = 0;
		red_pin_off = PWM_PERIOD + 1;		// never
	}
	else
	{
		if(pwm_duty_10bit >= 1023)
		{
//			red_pin_off = 0;
			red_pin_on = PWM_PERIOD + 1;	// never;
		}
		else
		{	// Normal case
//			red_pin_off = 0;
			red_pin_on = (PWM_PERIOD * pwm_duty_10bit) >> 10;
		}
	}
	LPC_SCT->MATCHREL[1].U = red_pin_on;
	LPC_SCT->MATCHREL[2].U = red_pin_off;
}


void MH_set_debug_uart_to_115k(void);

// 8000 ATCODE "P:%d "DA:%d DATA GOOD CAL_START: "FR3: Current:%d Enc_pos_report_error: " Calibration start Enc_pos:%d "Current:%d,pos
// Starting calibrate LPC_CTIMER0->MR DAC0 FINE override pin Reset_MCT8316 Put_encoder_pos1:Error RDY=0 Disable from: M1:RDY = 8000
// Set_AOUT MCT8316_driver_reset ReceiveSIG100:Enabled Disable from: M1:RDY = 0 Calibration finished "Setting RPM to:Set_Direction_Fine(
// 1235 2221
Enc_misc_td Enc_misc;
void CTIMER_idle_loop(int ival);

int main(void)
{
	  Board_init();
#ifdef MH_XXX
	  Init_SCTimer_PWM();
	  PWM_SetDutyTenBit(256);
	  while(1);
#endif
	// Configure the debug uart (see Serial.c)
 	  setup_debug_uart();

// 	 MH_set_debug_uart_to_115k();

	  printf("\r\nAC200hubBL: Hello World!\r\n");

//	  wait_ms(10000);
//	  for(;;){};
	  dummy_call(Version_id);

	  Timer_init();
//	  Timer_interrupt_init();	// MHH:25/06/2025

#ifdef MH_XXX

	  LPC_SYSCON->SYSAHBCLKCTRL[0] |= CTIMER0;		// Enable Timer
	  LPC_CTIMER0->PR = 14;		// Set Prescale Register so that we get 1 Mhz. Assume APB bus clock = 15 Mhz
	  //	  LPC_CTIMER0->PR = 29;		// Set Prescale Register so that we get 1 Mhz. Assume APB bus clock = 30 Mhz
	  LPC_CTIMER0->TCR = 1;		// Enable counter

#endif
	  // Configure the SYSTICK
	  // clock = system_clock, tick interrupt enabled
	  SysTick->CTRL = (1<<SysTick_CTRL_CLKSOURCE_Pos) | (1<<SysTick_CTRL_TICKINT_Pos);
	  uint32_t systick_time = (main_clk/SYSTICK_HZ)-1;
	  // 	  SysTick->LOAD = SYSTICK_TIME;
	  SysTick->LOAD = systick_time;

	  // Reload value
	  //	  SysTick->LOAD = SYSTICK_TIME;
	  // Clear the counter and the countflag bit by writing any value to SysTick_VAL
	  SysTick->VAL = 0;
	  // Enable the SYSTICK interrupt in the NVIC
	  SysTick->CTRL |= 1<<SysTick_CTRL_ENABLE_Pos;		// Enable counter
	  NVIC_EnableIRQ(SysTick_IRQn);


	  // Enable the USART0 RX Ready Interrupt
	  Serial_rb_init();

	  PRINTF("RingBuffer active\r\n");
	  CTIMER_idle_loop(1);


//	  Timer_interrupt_init();


	  PRINTF("Heater enabled\r\n");

#ifdef MH_USE_LPC845_CRC	// Could not get it to work. Maybe needs multiples of 4 bytes?
	  CRC32_init();

	  uint32_t crc = CRC32_bytes((uint8_t *)"ABC",3);
	  PRINTF("CRC=%d\r\n",crc);
#endif
//	  uint32_t crc2 = Wiki_CRC32((uint8_t *)"ABC",3);
//	  PRINTF("CRC2=%u\r\n",crc2);
	  int v = sizeof(Enc_data);
	  PRINTF("Enc_data size:%d\r\n",v);
	  uint8_t *start_64 = (uint8_t *)&Enc_data.magic;
	  uint8_t *end_64 = (uint8_t *) &Enc_data.magic2;
	  v = end_64 - start_64;
	  PRINTF("Enc_data 64 size:%d\r\n",v);

	  if(LPC845_read_hub_data_from_flash())
	  {
		  PRINTF("Failed to read flash data!!\r\n");
		  // We could still read from SSP and possibly restore to flash?
	  }
	  CTIMER_idle_loop(2);
	  SPI_init();

	  CTIMER_idle_loop(3);
	  Hub_plog_init();

	  CTIMER_idle_loop(4);
	  Hub_watchIt(0);	// MHH:29/08/2023
	  CTIMER_idle_loop(5);

	  Disable_Motor(300);

	  Speed_raw_mode = true;	// Default.
	  if(Hub_maxon_driver)
	  {
		  Speed_opamp = true;
		  if(Enc_data.sw_flags & 4)
		  {
			  if(Board_pin_read(DIG1_DIG2_PIN))
			  {
				  Speed_raw_mode = false;
				  L2PRINTF("Speed MaxonCtl\r\n");
				  LPC_IOCON->DIG1_DIG2_MODE &= (IOCON_MODE_MASK);	// Clear mode field
				  LPC_IOCON->DIG1_DIG2_MODE |= MODE_PULLUP;  // Pull-up, DIG1_DIG2_PIN
			  }
			  else		// PCB is not configured for Maxon control
			  {
				  L2PRINTF("Hub PCB not configured Speed MaxonCtl\r\n");
			  }
		  }

		  if(Speed_raw_mode)	// from board revision 15..
		  {
			  Config_Output_Pin(DIG1_DIG2_PIN);		// MHH:23/11/2025. Dig1 + Dig2 = 0 which is raw speed mode
			  Board_pin_write(DIG1_DIG2_PIN,false);	// Raw speed mode
			  L2PRINTF("Speed_raw_mode true\r\n");
		  }
		  if(Speed_opamp)
		  {
			  L2PRINTF("Speed_opamp true\r\n");
		  }
		  DAC_init();
	  }
	  else
	  {
		  L2PRINTF("TI Driver\r\n");
		  Init_SCTimer_PWM();
		  PWM_SetDutyTenBit(0);
	  }

	  CTIMER_idle_loop(6);
	  ADC_init();
	  CTIMER_idle_loop(7);
//#define MH_TEST_ADC
#ifdef MH_TEST_ADC
	  Hub_watchdog_active = false;
	  for(;;)
	  {
		  wait_ms(20);
		  ADC_sample();
	  }
#endif

//	  wait_ms(100);	// Debug!!


	  if(Enc_data.cal_pole_pairs == 0)
	  {
		  Enc_data.cal_pole_pairs = 4;
		  L2PRINTF("!Setting pole_pairs to 4\r\n");
	  }


	  Enc_Init();
	  CTIMER_idle_loop(8);
	  int v_nominal;
	  int voltage_min = Enc_data.voltage_min;
	  if(Enc_data.sw_flags & 1)
	  {
		  v_nominal = 24;
		  if(voltage_min == 0) voltage_min = 22;	// default;
	  }
	  else
	  {
		  v_nominal = 12;
		  if(voltage_min == 0) voltage_min = 1;		// Off by default for 12V
	  }
	  int divisor = Enc_data.voltage_correct_pct;
	  if(divisor == 0) divisor = 100;
	  L2PRINTF("%dV system, min V=%d, Vcorrect=%d\r\n",v_nominal,voltage_min,divisor);
	  voltage_min *= 10000;
	  voltage_min /= divisor;
	  voltage_min *= 11;		// MHH:22/09/2025. Because Adc_voltage = 1431 corresponds with real voltage of 13, approx * 1.1
	  voltage_min /= 10;
	  Enc_misc.adc_corrected_min_voltage = voltage_min;

	  Enc_max_current = Enc_data.cal_current_max * 100;		// MHH:03/09/2025
	  if(Enc_max_current == 0) Enc_max_current = 2500;
	  L2PRINTF("Enc_max_current = %d\r\n",Enc_max_current);

	  int cal_tolerance = Enc_data.cal_tolerance;		// MHH:29/06/2025
	  if(cal_tolerance < 1 || cal_tolerance > 10) cal_tolerance = 5;		// default to 0.5 mm.
	  Enc_misc.cal_tolerance_enc_units = (cal_tolerance * Enc_data.cal_pole_pairs * Enc_data.cal_motor_ratio) / Enc_data.cal_leadscrew_mm;
	  L2PRINTF("Encoder_tolerance:%d.%d mm, %d enc_units\r\n",cal_tolerance/10,cal_tolerance%10,Enc_misc.cal_tolerance_enc_units);
	  CTIMER_idle_loop(9);
	  Timer_start(&Enc_timer);
	  Timer_start(&Motor_timer);	// May not need Motor_timer when in receive mode.

#ifdef MH_XXX
	  if(Hub_maxon_driver == false)	// MHH:13/06/2026 Test!!!!. Test board returning zero!!!!
	  {
		  if(Board_pin_read(FINE_MICROSWITCH_PIN))
		  {
			  if(Enc_Pos.pos < Enc_data.cal_fine_stop) Enc_Pos.pos = Enc_data.cal_fine_stop;
		  }
		  if(Board_pin_read(COARSE_MICROSWITCH_PIN))
		  {
			  if(Enc_Pos.pos > Enc_data.cal_coarse_stop) Enc_Pos.pos = Enc_data.cal_coarse_stop;
		  }
	  }
#endif

	  L2PRINTF("!Run:%d,Fine=%d,Pos=%d,Flags=%d,HWver=%d,SWver=%d\r\n",Enc_data.hub_run,Enc_data.cal_fine_stop,Enc_Pos.pos,Enc_data.run_flags,Hub_hardware_version
			  ,Enc_data.hub_software_version);
#ifdef MH_FLASH_RECORD
	  L2PRINTF("DIAGNOSTIC VERSION ONLY!!\r\n")
//AC300 Run: ??Not Rebooting
#endif


	  Sig100_ini();
	  CTIMER_idle_loop(10);

//	  PWM_init_20khz();		// MHH:05/03/2026
//	  PWM_SetDutyPercent(25);		// test
//	int ready_pin = Board_pin_read(READY_PIN);
//	PRINTF("READY_PIN:%d\r\n",ready_pin);


//	  CTIMER_idle_loop();

#define MH_SIG100
#ifdef MH_SIG100
	  Receive_SIG100();
#else
	  PRINTF("Testing, SIG100 receive not active\r\n");
	  Hub_watchdog_active = false;
	  Motor_main();
#endif

	  L2PRINTF("AC200HUBL:After Receive_SIG100, rebooting\r\n");
	  wait_ms(200);
	  LPC845_reboot();
	  while(1);
}  // end of main
