/*
 * Binary_io.c
 *
 *  Created on: 4/03/2023
 *      Author: MHH
 */

// May be called by Calibrator or AC210
#include <stdio.h>
#include <string.h>

#include "LPC8xx.h"
#include "uart.h"
#include "syscon.h"
#include "swm.h"

#include "ac300Enc_ssp.h"
#include "chip_setup.h"

#include "AC300_hub.h"

void Sig100_send_binary_data(uint8_t *data_p,size_t data_size)
{
	uint8_t *p = data_p;
	for(int i=0;i<data_size;i++)
	{
		uint8_t c = *p++;
#ifdef MH_SIG100_RB
		SIG100_RB_UARTPutchar(c);
#else
		Sig100_UARTPutChar((char)c);
#endif
//		wait_us(100);
	}
}
#ifdef MH_SEND_DELAY
void Sig100_send_binary_data2(uint8_t *data_p,size_t data_size)
{
	uint8_t *p = data_p;
	for(int i=0;i<data_size;i++)
	{
		uint8_t c = *p++;
		Sig100_UARTPutChar((char)c);
		wait_ms(1);
	}
}
#endif
bool Hub_AT_debug;
void Sig100_flush_and_wait(int millisecs)
{
#ifdef MH_SIG100_RB
	Sig100_RB_flush_output();
#endif
	wait_ms(millisecs);
}
void Sig100_send_hub_data(void)
{
//	Hub_AT_debug = true;
//	size_t data_size = sizeof(Enc_data);
	size_t data_size = 84;	// MHH:19/07/2023
	int pos = Enc_Pos.pos;
	if(pos < 0) pos = 0;
	Enc_data.calibrate_pos = pos;		// MHH:21/12/2023
//	Enc_data.calibrate_pos = Enc_Pos.pos;
	uint8_t *data_p = (uint8_t *)&Enc_data;	// Possibly need & ?
	uint32_t crc32 = Wiki_CRC32(data_p,data_size);
	size_t crc32_size = sizeof(crc32);
	// Perhaps could send header and pkt length?
#ifdef MH_SIG100_RB
	SIG100_RB_UARTPutchar('{');
	uint8_t c = (uint8_t) data_size + (uint8_t)crc32_size;
	SIG100_RB_UARTPutchar((char)c);		// Should not exceed 255
#else
	Sig100_UARTPutChar('{');
	uint8_t c = (uint8_t) data_size + (uint8_t)crc32_size;
	Sig100_UARTPutChar((char)c);		// Should not exceed 255
#endif

//	Sig100_send_binary_data2(data_p,data_size);
	Sig100_send_binary_data(data_p,data_size);
	Sig100_flush_and_wait(5);
	data_p = (uint8_t *)&crc32;	// Now send crc32
	Sig100_send_binary_data(data_p,crc32_size);
	Sig100_flush_and_wait(5);
//	Sig100_send_binary_data2(data_p,crc32_size);
#ifdef MH_SIG100_RB
	SIG100_RB_UARTPutchar('}');
#else
	Sig100_UARTPutChar('}');
#endif
	Sig100_flush_and_wait(5);
//	Sig100_UARTPutSTR("12345678901234567890");
//	wait_ms(5);
}
uint32_t Get_uint32(uint8_t *data_p)
{
	// Assume litle endian
	uint32_t result = data_p[0] | (data_p[1] << 8) | (data_p[2] << 16) | (data_p[3] << 24);
	return result;

//            Int32 result = bdata[start_byte] + (bdata[start_byte + 1] << 8) + (bdata[start_byte + 2] << 16) + (bdata[start_byte + 3] << 24);

}


void Sig100_receive_error(int v)
{
	PRINTF("Error:%d\r\n",v);
}
uint8_t Hub_data[64];	// So can fit in 1 page of flash memory
int Hub_data_len;

int Sig100_getc_with_timeout(int millisecs)
{
#ifdef MH_SIG100_RB
	return Sig100_RB_getc_timeout(millisecs);
#else
	return Sig100_getc_timeout(millisecs);
#endif
}

bool Sig100_receive_hub_data(void)
{
	int c;
	PRINTF("Sig100_receive_hub_data:\r\n");
	Hub_data_len = 0;
	c = Sig100_getc_with_timeout(100);
	if(c != '{')
	{
		Sig100_receive_error(1);
		return false;
	}
	c = Sig100_getc_with_timeout(100);
	if(c < 0)
	{
		Sig100_receive_error(2);
		return false;
	}
	int data_and_crc_len = c;
	if(data_and_crc_len > sizeof(Hub_data))
	{
		Sig100_receive_error(3);
		return false;
	}
	uint8_t *data_p = Hub_data;		// May need a cast or & ?
	for(int i=0;i<data_and_crc_len;i++)
	{
		c = Sig100_getc_with_timeout(100);
		if(c < 0)
		{
			Sig100_receive_error(4);
			return false;
		}

		*data_p++ = (uint8_t) c;
	}
	c = Sig100_getc_with_timeout(100);
	if(c != '}')
	{
		Sig100_receive_error(5);
		return false;

	}

	// If we drop through then have received the correct number of bytes + crc32. Now to verify the data against the crc

	int data_size = data_and_crc_len - 4;
	uint32_t data_crc32 = Get_uint32(Hub_data + data_size);

	uint32_t crc32 = Wiki_CRC32(Hub_data,data_size);

	if(crc32 != data_crc32)
	{
		PRINTF("Sig100_receive_hub_data:crc32 error");
		return false;
	}
	PRINTF("crc32 OK\r\n");
	Hub_data_len = data_size;
	return true;		// Can update memory once we know this works...
	// Make sure that we do not write any more bytes than sizeof structure.
}
int LPC845_write_hub_data_to_flash(void);

int Sig100_update_hub_data(void)
{
// OK, we have the data in Hub_data, 60 bytes + 4 byte crc32. Should fit into 1 page of flash memory...
//	and the length in Hub_data_len
// First job is to write it all to ssp memory...
// Try flash instead...
	int result = LPC845_write_hub_data_to_flash();
	if(result) return result;

	result= ac200Enc_SSP_save_hub_data();
	if(result)
	{
		L2PRINTF("Sig100_update_hub_data:Failed to update SSP\r\n");
	}
	return result;
}
//====================================================================================================================================
void Sig100_X_putc(uint8_t c)
{
#ifdef MH_SIG100_RB
	SIG100_RB_UARTPutchar(c);
#else
	Sig100_UARTPutChar(c);
#endif
}


void Sig100_X_puts(char * string)
{
#ifdef MH_SIG100_RB
	SIG100_RB_puts(string);
#else
	Sig100_UARTPutSTR(string);
#endif

}
void Sig100_send_test_data(void)
{

	Sig100_X_putc('{');
	for(int i=0;i<128;i++)
	{
		char c = '0' + i;
		if(c > 127) c-= 64;
		Sig100_X_putc(c);
	}
	Sig100_X_putc('{');

}

int After_AT_commands=0;
bool Sig100_process_command(void)
{
	PRINTF("Command:%s\r\n",Command_string);
	//                        12345678901
	if(memcmp(Command_string,"!ATREBOOT=1",11) == 0)	// MHH:21/04/2023
	{
		Sig100_X_puts("OK\r\n");
		PRINTF("OK\r\n");
		PRINTF("Rebooting...\r\n");	// Quick and dirty way of getting back to 'normal'....
		wait_ms(200);
		LPC845_reboot();
		return false;
	}


	//                        123456789
	if(memcmp(Command_string,"!ATCODE=1",9) == 0)
	{
		Sig100_X_puts("OK\r\n");
		PRINTF("OK\r\n");
		return false;
	}
	//                        1234567890123
	if(memcmp(Command_string,"!ATHUBPLOG=1",12) == 0)
	{
		Hub_plog_display(1);
		PRINTF("Rebooting...\r\n");		// Quick and dirty way of getting back to 'normal'....
		wait_ms(100);
		LPC845_reboot();
		return false;
	}


	if(memcmp(Command_string,"!ATHUBDATA=0",12) == 0)
	{
		Sig100_send_test_data();
		return false;
	}

//                            123456789012

	if(memcmp(Command_string,"!ATHUBDATA=1",12) == 0)
	{
		Sig100_send_hub_data();
		PRINTF("??Not Rebooting...\r\n");		// Quick and dirty way of getting back to 'normal'....
		wait_ms(100);
		After_AT_commands++;
//		LPC845_reboot();		// MHH:21/12/2023 Do we need to??
		return true;
	}
	//                          12345678901234
	if (memcmp(Command_string, "!ATHUBUPDATE=1", 14) == 0)
	{
		Sig100_X_puts("OK\r\n");
		PRINTF("OK\r\n");
		if(Sig100_receive_hub_data())
		{
			Sig100_update_hub_data();
			Sig100_X_puts("DATA GOOD\r\n");
			L2PRINTF("Hub data update, DATA GOOD\r\n");
		}
		else
		{
			Sig100_X_puts("DATA BAD\r\n");
			L2PRINTF("Hub data update, DATA BAD\r\n");
		}
//		Sig100_UARTPutSTR("OK\r\n");


		PRINTF("Rebooting...\r\n");	// Quick and dirty way of getting back to 'normal'....
		wait_ms(200);
		LPC845_reboot();
		return false;
	}

#ifdef MH_XXX
	//                          12345678901234
	if (memcmp(Command_string, "!ATCALIBRATE=1", 14) == 0) {
		Sig100_UARTPutSTR("OK\r\n");
		Hub_calibrate();
		PRINTF("OK\r\n");
		return;
	}
#endif
	//                        1234567890123456
	if(memcmp(Command_string,"!ATFLASH=2476",13) == 0)
	{
		LPC845_flash();		// Prepare for reboot and ISP commands
		PRINTF("Rebooting...\r\n");
		wait_ms(200);
		LPC845_reboot();
		return false;
	}
	return false;
}
//-----------------------------------------------------------------------------------------------
void Sig100_get_command(void)
{
	int ci;
	Command_len = 0;
//	for(int i=0;i<10;i++) Command_string[i]=0;		// Debug...
	for(;;)
	{
#ifdef MH_SIG100_RB
		ci = Sig100_RB_getc();
#else
		ci = Sig100_getc();
#endif
//		ci = Sig100_UARTGetChar();	// Currently using 'no wait". Consider using wait one millisec
		if(ci == 13) continue;	// ignore CR
		if(ci == 10) break;		// We have a command
		if(Command_len < COMMAND_MAX_LEN)
		{
			Command_string[Command_len++] = (char)ci;
		}
	}
	if(Command_len >= COMMAND_MAX_LEN) Command_len = COMMAND_MAX_LEN - 1;	// min(len,max_len-1)

	Command_string[Command_len] = 0;	// null terminate string.

}
//-----------------------------------------------------------------------------------------------
void Check_command_mode(void)
{
	// Entering command mode only caused by AC200 receiving HubLink command
	//                        123456789

#ifdef MH_DDD
	if(memcmp(Command_string,"!CMode=1",8) != 0) return;
#endif
	if(memcmp(Command_string,"C!mode=1",8) != 0) return;

	Hub_watchdog_active = false;			// MHH:04/09/2023

	PRINTF("\r\nCommand mode:\r\n");
	for(;;)
	{
		Sig100_get_command();
		if(Sig100_process_command())
		{
			return;		// MHH:21/12/2023
		}
	}
}

