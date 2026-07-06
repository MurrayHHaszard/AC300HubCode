/*
 * ac210_ssp.c
 *
 *  Created on: 3/01/2017
 *      Author: Murray
 */

//#include "lpc17xx.h"

#include <stdio.h>
#include "board.h"
#ifndef MH_LPC_1768
#include <string.h>
#include "spi.h"
#include "timer.h"
#include "AC300_hub.h"

//#define MH_AC200_SPI
//#ifdef MH_AC200_SPI

#include "ac300Enc_ssp.h"

//#ifdef MH_DIAGS
#include "Hub_flashlog.h"
//#endif

#define		false		0
#define		true		(!false)

/*****************************************************************************
 * Private types/enumerations/variables
 ****************************************************************************/

// FIXME: SSP0 not working on LPCXpresso LPC1769.  There seems to be some sort
//        of contention on the MISO signal.  The contention originates on the
//		  LPCXpresso side of the signal.
//

//-----------------------------------------------------------------------------
#define MRAM_SIZE 		16384		// 128K bits
#define MRAM_START			0
#define MRAM_TEST_START		0
#define MRAM_80BYTE_PAGES	(MRAM_SIZE/80)
//#define MRAM_80BYTE_PAGES	(1)
//-----------------------------------------------------------------------------

//#define SSP_COMMAND_ADDRESS_SIZE			3		// Command is 1 byte, address 2 bytes
													// Different for FLASH, command + address = 4 bytes.

/* Tx buffer */
uint8_t Tx_Buf[SSP_BUFFER_SIZE];

/* Rx buffer */
uint8_t Rx_Buf[SSP_BUFFER_SIZE];

#define MH_MR25H			// MR25H Flash only needs 1 write enable.
//-------------------------------------------------------------------------------------------------
void DebugAbort(char *msg)
{
	PRINTF("DebugAbort:%s",msg);
	PRINTF_FLUSH;
	while(true);	// infinite loop
}
//-------------------------------------------------------------------------------------------------
void SSP_RW_Poll_Mode(int spi_num,int length)
{
	SPImasterWriteRead(spi_num,Tx_Buf,Rx_Buf,length);
}
//-------------------------------------------------------------------------------------------------
uint8_t SSP_Flash_Read_Status(int spi_num)
{
	if(spi_num != SPI_FLASH_CS) return 255;

//	printf("SSP_Flash_Read_Status:\r\n");
	Tx_Buf[0] = 5;		// Read status command
	Tx_Buf[1] = 0;		// dummy

	SSP_RW_Poll_Mode(spi_num,2);	// Send and Receive
	uint8_t status = Rx_Buf[1];
	return status;
}
//-------------------------------------------------------------------------------------------------
uint32_t SSP_Flash_Read_ID(void)
{
//	printf("SSP_Flash_Read_ID\r\n");
	Tx_Buf[0] = 0x9F;		// Read ID command
	Tx_Buf[1] = 0;		// dummy

	SSP_RW_Poll_Mode(SPI_FLASH_CS,5);	// Send and Receive
	uint32_t flash_id=0;
	for(int i=1;i<=4;i++)
	{
		flash_id <<= 8;
		uint8_t b=Rx_Buf[i];
		flash_id |= b;
	}
	PRINTF("flash_id=%04x\r\n",flash_id);
	return flash_id;
}
//-------------------------------------------------------------------------------------------------
void SSP_Write_Enable(int spi_num)
{
	Tx_Buf[0] = 6;		// Write Enable
	SSP_RW_Poll_Mode(spi_num,1);	// Send and Receive
}
//-------------------------------------------------------------------------------------------------
void SSP_copy_address(uint32_t byte_address)
{
	uint32_t a = byte_address;

	switch(SPI_command_address_size)
	{
	case 3:
		Tx_Buf[2] = (uint8_t) (a & 0xff);
		a >>= 8;
		Tx_Buf[1] = (uint8_t) (a & 0xff);
		return;
	case 4:
		Tx_Buf[3] = (uint8_t) (a & 0xff);
		a >>= 8;
		Tx_Buf[2] = (uint8_t) (a & 0xff);
		a >>= 8;
		Tx_Buf[1] = (uint8_t) (a & 0xff);
		return;
	default:
		DebugAbort("SSP_copy_address: error");
		break;

	}
#ifdef MH_XXX
	for(int i=(SSP_COMMAND_ADDRESS_SIZE-1);i>=1;i--)
	{
		Tx_Buf[i] = (uint8_t) (a & 0xff);
		a >>=8;
	}
#endif
}
//-------------------------------------------------------------------------------------------------
#define SSP_CMD_WRITE				0x2
#define SSP_CMD_READ				0x3

#define SSP_BUFF_SIZE	256

#ifdef MH_SPI_WRITE_POLL_MODE
void SSP_Write_Poll_Mode(int spi_num,int length)		// Test write only
{
	SPImasterWriteOnly(spi_num,Tx_Buf,length);
}
#endif
//-------------------------------------------------------------------------------------------------
// Note: Could put a limit on loops here, in case of problem.
bool SSP_flash_wait_ready;
uint32_t SPI_command_address_size;
void SSP_Flash_Wait_Ready(int spi_num)
{
// Note: MRAM does not need to wait for Ready.
	switch(spi_num)
	{
	case SPI_MRAM_CS:
		SPI_command_address_size = 3;
		return;
	case SPI_FLASH_CS:
		SPI_command_address_size = 4;
		break;
	default:
		DebugAbort("SSP_Flash_Wait_Ready: error");
		break;
	}

	if(SSP_flash_wait_ready == false) return;
	SSP_flash_wait_ready = false;
	for(int i=0;i<1000;i++)	// MHH:20/02/2019. Put limit in as if flash not working didn't exit loop
	{
		uint8_t sr1 = SSP_Flash_Read_Status(spi_num);
		if(sr1 == 0) return;
		if(sr1 == 2) return;	// Just write enabled
//		PRINTF("sr1=%d\r\n",sr1);
		wait_ms(1);
	}
	DebugAbort("SSP_Flash_Wait_Ready(2): error");
}
//-------------------------------------------------------------------------------------------------
void SSP_WriteBuffer(int spi_num,uint32_t byte_address, uint8_t *pBuffer, uint32_t len)
{
	SSP_Flash_Wait_Ready(spi_num);

#ifndef MH_MR25H
	SSP_Write_Enable();
#endif

	SSP_Write_Enable(spi_num);		// MHH:07/07/2023

	Tx_Buf[0] = SSP_CMD_WRITE;		// write instruction
	SSP_copy_address(byte_address);
	memcpy(Tx_Buf+SPI_command_address_size,pBuffer,len);
//	memcpy(Tx_Buf+SSP_COMMAND_ADDRESS_SIZE,pBuffer,len);

//	SSP_RW_Poll_Mode(SSP_COMMAND_ADDRESS_SIZE+len);
//	SSP_Write_Poll_Mode(spi_num,SSP_COMMAND_ADDRESS_SIZE+len);
//	SSP_Write_Poll_Mode(spi_num,SPI_command_address_size+len);
	SSP_RW_Poll_Mode(spi_num,SPI_command_address_size+len);		// Could get bytes_read from here
	if(spi_num == SPI_FLASH_CS)
	{
		SSP_flash_wait_ready = true;
	}
//	SSP_Flash_Wait_Ready();
}
//------------------------------------------------------------------------------
// The idea is to make the i/o independent of page size, for portability
static int ssp_error;

//-------------------------------------------------------------------------------------------------
int SSP_ReadBuffer(int spi_num,uint32_t byte_address, uint8_t *pBuffer, uint32_t len)
{
	SSP_Flash_Wait_Ready(spi_num);

	Tx_Buf[0] = SSP_CMD_READ;		// read instruction
	SSP_copy_address(byte_address);
//	SSP_RW_Poll_Mode(spi_num,SSP_COMMAND_ADDRESS_SIZE+len);		// Could get bytes_read from here
	SSP_RW_Poll_Mode(spi_num,SPI_command_address_size+len);		// Could get bytes_read from here
//	memcpy(pBuffer,Rx_Buf+SSP_COMMAND_ADDRESS_SIZE,len);
	memcpy(pBuffer,Rx_Buf+SPI_command_address_size,len);
	return 0;
}
//-------------------------------------------------------------------------------------------------

//=================================================================================
// Note: These routines assume that there is only one eeprom we are accessing, and we
// have initialised its details. Just makes it simpler, otherwise have to remember multiple
// ids, slave addresses and positions

//------------------------------------------------------------------------------
// The idea is to make the i/o independent of page size, for portability
int ac200Enc_ssp_mram_read(int spi_num,uint32_t byte_address,uint8_t *buff, uint32_t len)
{
/*
	if(len > SSP_BUFF_SIZE)
	{
		return -2;
	}
*/
	uint32_t pos = byte_address;
	uint8_t *bp = buff;
	int bytes_left = len;

	while(bytes_left > 0)
	{
		int rlen = MIN(bytes_left,SSP_BUFF_SIZE);
		rlen = MIN(rlen,MRAM_SIZE - pos);
		if(rlen <= 0)
		{
			return -1;		// eof
		}
		SSP_ReadBuffer(spi_num,pos,bp,rlen);
		bytes_left -= rlen;
		pos += rlen;
		bp += rlen;
	}

//	SSP_ReadBuffer(pos,buff,len);
	return 0;
}
//------------------------------------------------------------------------------
// The idea is to make the i/o independent of page size, for portability
static int ssp_error;

// This allows access to sector zero

void ac200Enc_ssp_raw_mram_write(int spi_num,uint32_t byte_address,uint8_t *buff, uint32_t len,int ifrom)
{
	uint32_t pos = byte_address;
	uint8_t *bp = buff;
	int bytes_left = len;

//	PRINTF("ssp:w:F=%d,A=%d,L=%d\r\n",ifrom,byte_address,len);

	while(bytes_left > 0)
	{
		uint32_t wlen = MIN(bytes_left,SSP_BUFF_SIZE);
		SSP_WriteBuffer(spi_num,pos,bp,wlen);
		bytes_left -= wlen;
		pos += wlen;
		bp += wlen;
	}
}
//------------------------------------------------------------------------------
int ac200Enc_ssp_mram_write(int spi_num,uint32_t byte_address,uint8_t *buff, uint32_t len)
{
//This is trickier than the read as the write will page wrap

	if(byte_address + len > MRAM_SIZE)
	{
		ssp_error = 1;
		return -1;
	}
	if(byte_address < MRAM_START)
	{
		ssp_error = 2;
		return -1;
	}

	ac200Enc_ssp_raw_mram_write(spi_num,byte_address,buff,len,100);

	return 0;
}
//-------------------------------------------------------------------------------------------------
#define ENCODER_POS_HIGHVAL		99999		// must be impossible
#define SSP_ENC_DATA_POS		8
#define SSP_ENC_DATA_LEN		4


//int ssp_encoder_pos=ENCODER_POS_HIGHVAL;
//int ssp_count;

Enc_data_td Enc_data;
//bool Enc_ud_coarse_stop;
bool Enc_ud_feather_stop;
bool Enc_ud_reverse_stop;
//void ac200Enc_update_pos(void);

void ac200_Enc_set_ud_flags(void)
{
//	Enc_ud_coarse_stop = ((Enc_data.user_defined_stops & 2) != 0);
	Enc_ud_feather_stop = ((Enc_data.user_defined_stops & 4) != 0);
	Enc_ud_reverse_stop = ((Enc_data.user_defined_stops & 8) != 0);
}
uint8_t Hub_data2[64];	// Could probably use a scratch buffer...


int SSP_copy_Hub_data_to_Enc_data(void)
{
	uint8_t *dst_p = (uint8_t *)&Enc_data;		// Do we need & ?
	for(int i=0;i<64;i++)
	{
		*dst_p++ = Hub_data[i];
	}
	if(Enc_data.magic != SSP_MAGIC_1)
	{
		L2PRINTF("Copy_Hub_data_to_Enc_data:magic FAIL\r\n");
		return 1;
	}
	return 0;
}
void Enc_ssp_update_enc_run_data(void)
{
	// Maybe avoid this if we have an SSP error?

	ac200Enc_write_Enc_data((uint8_t *)&Enc_data.run_flags,30,100);
}
void Enc_ssp_update_enc_run_flags(void)
{
//	ac200Enc_write_Enc_data((uint8_t *)&Enc_data.run_flags,1,100);
	ac200Enc_write_Enc_data((uint8_t *)&Enc_data.run_flags,16,150);	// MHH:04/08/2023. Include run_set_from
}
int ac200Enc_SSP_save_hub_data(void)
{
	ac200Enc_ssp_raw_mram_write(SPI_MRAM_CS,0,(uint8_t *)Hub_data,64,200);

// Since this is only called when updating from Calibrator, check that it wrote OK
// Note: Another way would be to set Hub_data to zero, read into it and check crc32
	int result = ac200Enc_ssp_mram_read(SPI_MRAM_CS,0,(uint8_t *)Hub_data2,64);
	if(result)
	{
		L2PRINTF("mram_read_error:1\r\n");		// Maybe should just go into loop after, as this is really bad
		return 1;
	}
	for(int i=0;i<64;i++)
	{
		if(Hub_data[i] != Hub_data2[i])
		{
			L2PRINTF("mram read/write error at byte:%d\r\n",i);
			return 2;
		}
	}
	// Now to update Enc_data structure...
	if(SSP_copy_Hub_data_to_Enc_data())
	{
		return 3;
	}
	Enc_data.run_flags &= ~ENC_RUN_FLAGS_NO_FEATHER;	// Clear NO_FEATHER bit
	Enc_data.run_set_from = 0;
	Enc_ssp_update_enc_run_flags();
	// Now update position.
//	Enc_data.pos = Enc_data.cal_pos;
	Enc_Pos.pos = Enc_data.cal_pos;
	Put_encoder_pos(100);
//	Put_encoder_pos(1,110);
//	ac200Enc_update_pos(100);

	return 0;
}

#ifdef MH_XXX
bool Enc_write_pos_needed;

void ac200Enc_SSP_save_pos(void)
{
	if(Enc_pos == Enc_data.pos) return;

	//Note: Could limit to (say) ten times per sec, ie at least 10 systicks between

	Enc_data.pos = Enc_pos;
//	Enc_data.rpm = Enc_rpm;		// Now used to see if Hub restarted when propeller turning
	Enc_write_pos_needed = true;		// Don't want to update spi memory every time motor makes a revolution, get Systick to do it up to 5 times a sec
}
#endif

#ifdef MH_XXX
int Enc_last_pos_written;
void ac200Enc_update_pos(int ifrom)
{
//	PRINTF("UP:%d\r\n",ifrom);
	Enc_data.pos = Enc_pos;		// To be sure..
	Enc_data.rpm = Enc_rpm;

	if(Enc_pos == Enc_last_pos_written)
	{
		return;
	}
	Enc_last_pos_written = Enc_pos;
	ac200Enc_write_Enc_data((uint8_t *)&Enc_data.pos,4,100);
//	ac200Enc_write_Enc_data((uint8_t *)&Enc_data,80,100);
}
#endif
#ifdef MH_XXX
void ac200Enc_SSP_write_pos(int ifrom)
{
//	if(Enc_write_pos_needed == false) return;

//	PRINTF("WP:%d\r\n",ifrom);

	Enc_write_pos_needed = false;

	ac200Enc_update_pos(200);
}
#endif
//-------------------------------------------------------------------------------------------------
//static uint8_t buffer[68];	// Note: May be able to use another buffer for this...
//static const uint16_t magic_val_tbl[3] = {SSP_MAGIC_1,SSP_MAGIC_2,SSP_MAGIC_3};
//static const uint16_t magic_pos_tbl[3] = {SSP_MAGIC_1_POS,SSP_MAGIC_2_POS,SSP_MAGIC_3_POS};Set_AOUT
void SSP_initialise_page_zero()
{
	Dputs("SSP:Initialising SSP..");

	uint8_t *enc_data_p = (uint8_t *)&Enc_data;
	memset(enc_data_p,0,sizeof(Enc_data));

	Enc_data.run_flags |= ENC_RUN_FLAGS_NO_FEATHER;	// As we don't know position
	Enc_data.run_set_from = 10;

	Enc_data.magic = SSP_MAGIC_1;
	Enc_data.magic2 = SSP_MAGIC_2;
	Enc_data.magic3 = SSP_MAGIC_3;

	Enc_data.cal_pole_pairs = 4;		// Default
	Enc_data.cal_cam_length = 260;		// 26.0 mm
	Enc_data.cal_blade_offset = 300;	// In tenths of a degree
	Enc_data.cal_leadscrew_mm = 3175;	// in thousands of a mm.
	Enc_data.cal_motor_ratio = 19011;	// 190.11F;
//	Enc_data.cal_tolerance = 16;		// just for ini.
//	Enc_data.cal_tolerance = 5;			// MHH:29/06/2025. Now using Enc_misc.cal_tolerance_enc_units. (0.5 * 4 * 190)/3.175 just for ini.
	Enc_data.cal_tolerance = 10;		// MHH:23/04/2026. Requested by Russell.

	Enc_data.cal_coarse_stop = 4681;		// From test calibrate
	Enc_data.cal_fine_stop = 6654;		// From test calibrate
	Enc_data.cal_fine_stop2 = 6660;
	Enc_data.cal_fine_hard_stop = 7850;	// From test calibrate

	Enc_data.hub_software_version = HUB_SOFTWARE_REVISION;
	Enc_data.cal_tdc_pos_offset = 5269;	// (22.0mm / 3.175) * 190.11 * pole_pairs

	Enc_data.pos1 = Enc_data.cal_fine_stop;	// Default to fine stop
	Enc_data.pos1_check = ~Enc_data.pos1;

	Enc_Pos.pos = Enc_data.cal_fine_stop;		// Useful when testing with simulator.

	Enc_data.pos2 = Enc_data.cal_fine_stop;	// Default to fine stop
	Enc_data.pos2_check = ~Enc_data.pos2;

	Enc_data.crc32 = Wiki_CRC32(enc_data_p,60);		// Think 60, not 64

	ac200Enc_ssp_mram_write(SPI_MRAM_CS,0,enc_data_p,sizeof(Enc_data));

#ifdef MH_XXX

	memset(Hub_data2,0,sizeof(Hub_data2));
	for(int pos=0;pos < 1024;pos+=64)
	{
		ac200Enc_ssp_mram_write(pos,Hub_data2,64);
	}

	uint16_t magic;
	uint8_t  *p_data = (uint8_t *)&magic;
	int magic_pos;
	for(int i=0;i<3;i++)
	{
		magic = magic_val_tbl[i];
		magic_pos = magic_pos_tbl[i];
		ac200Enc_ssp_raw_mram_write(magic_pos,p_data,2,300);
	}
#endif
	PRINTF("done\r\n");
}
int SSP_read_pos(void)
{
	uint8_t pos_t[2];

	uint8_t *p_data = pos_t;
	int result = ac200Enc_ssp_mram_read(SPI_MRAM_CS,76,p_data,2);
	if(result)
	{
		Dputs("mram_read_error:2\r\n");		// Maybe should just go into loop after, as this is really bad
		return 0;
	}
	int pos = pos_t[0] | (pos_t[1] << 8);
	return pos;
}
//-------------------------------------------------------------------------------------------------
void SSP_read_Enc_data(void)
{
	Dputs("SSP_read_Enc_data:\r\n");

	uint32_t len = sizeof(Enc_data);
	uint8_t *p_data = (uint8_t *)&Enc_data;

	int result = ac200Enc_ssp_mram_read(SPI_MRAM_CS,SSP_MAGIC_1_POS,p_data,len);
	if(result)
	{
		Dputs("mram_read_error:1\r\n");		// Maybe should just go into loop after, as this is really bad
		return;
	}

	if(Enc_data.magic != SSP_MAGIC_1)
	{
		if(Enc_data.magic2 != SSP_MAGIC_2)
		{
			SSP_initialise_page_zero();
			return;			// Need both to be invalid before we initialise.
		}
	}
	//}

	if(Enc_data.magic3 != SSP_MAGIC_3)
	{
		{
			Dputs("Enc_data.magic3 FAIL\r\n");		// This should not happen
			SSP_initialise_page_zero();				// 15/07/2023:For now....
		}
	}
}
//------------------------------------------------------------------------------------------------------------
/*
 *  Enc_pos is stored in 2 bytes, along with a complement of value to ensure integrity.
 *  It is stored in 2 places at different times, so if one becomes corrupt then the backup can be used.
 *  The valid range for the Enc_pos is about -500 (in case of movement into coarse hard stop) to 65000.
 */
#define ENC_POS_ERROR_RANGE1		1
#define ENC_POS_ERROR_RANGE2		2
#define ENC_POS_ERROR_RANGE3		3

#define ENC_POS_GETPOS				4

#ifdef MH_YYY

int Put_pos_lastwritten[2];
int Put_encoder_pos2(int option)
{
// Check pos is in valid range.
	int pos = Enc_Pos.pos;		// Can be changed by ISR
	if(pos == Put_pos_lastwritten[option]) return 0;
	Put_pos_lastwritten[option] = pos;
//	PRINTF("P:%d.",pos);	// MHH:25/06/2025

	if(pos < 0)
	{
		if(pos < -200) return ENC_POS_ERROR_RANGE1;
		pos = 0;		// If less than zero, could be valid if calibrating and moving into coarse hard stop,but zero is at rest CHS position
	}
	else
	{
		if(pos > 65535)	return ENC_POS_ERROR_RANGE2;
		if(Calibrating == false)
		{
			if(pos > Enc_data.cal_fine_hard_stop)
			{
				if(pos < Enc_data.cal_fine_hard_stop + 200)	// Possible if moving into fine hard stop
				{
					pos = Enc_data.cal_fine_hard_stop;	// same logic as for negative, do not store past range
				}
				else
				{
					return ENC_POS_ERROR_RANGE3;
				}
			}
		}
	}
	uint16_t pos16 = (uint16_t)pos;
	uint16_t pos16_check = ~pos16;
	uint8_t *data_p;
	if(option == 0)
	{
		Enc_data.pos1 = pos16;
		Enc_data.pos1_check = pos16_check;
		Enc_data.rpm = Enc_rpm;
		data_p = (uint8_t *) &Enc_data.pos1;
	}
	else
	{
		Enc_data.pos2 = pos16;
		Enc_data.pos2_check = pos16_check;
		Enc_data.rpm2 = Enc_rpm;
		data_p = (uint8_t *) &Enc_data.pos2;
	}
	ac200Enc_write_Enc_data(data_p,6,50);
	return 0;
}
void Put_encoder_pos1(int option)
{
//	if(Enc_pos_check(300)) return;		// MHH:09/09/2023. Cannot check here as if ISR occurs then it will cause an error.

	int e = Put_encoder_pos2(option);
#ifndef MH_IGNORE_ERROR
	if(e)
	{
		L2PRINTF("Put_encoder_pos1:Error:%d\r\n",e);
		Enc_data.run_flags |= ENC_RUN_FLAGS_NO_FEATHER;
		Enc_data.run_set_from = 20;
		Enc_ssp_update_enc_run_flags();	// MHH:04/08/2023
	}
#endif
}
int Put_pos_ix;
void Put_encoder_pos(int ifrom)
{
#ifdef MH_XXX		// MHH:23/05/2025. Probably better to update.
	if(Enc_data.run_flags & ENC_RUN_FLAGS_NO_FEATHER) return;	// MHH:08/09/2023. Do not update position if position error
#endif
	Put_encoder_pos1(Put_pos_ix);
	Put_pos_ix ^= 1;		// toggle between 0 and 1

}
#endif
int Put_pos_lastwritten;
int Put_pos_cnt;
int Put_encoder_pos2(void)
{
// Check pos is in valid range.
	int pos = Enc_Pos.pos;		// Can be changed by ISR
//	PRINTF("P:%d.",pos);	// MHH:25/06/2025

	if(pos < 0)
	{
		if(pos < -200) return ENC_POS_ERROR_RANGE1;
		pos = 0;		// If less than zero, could be valid if calibrating and moving into coarse hard stop,but zero is at rest CHS position
	}
	else
	{
		if(pos > 65535)	return ENC_POS_ERROR_RANGE2;
		if(Calibrating == false)
		{
			if(pos > Enc_data.cal_fine_hard_stop)
			{
				if(pos < Enc_data.cal_fine_hard_stop + 200)	// Possible if moving into fine hard stop
				{
					pos = Enc_data.cal_fine_hard_stop;	// same logic as for negative, do not store past range
				}
				else
				{
					return ENC_POS_ERROR_RANGE3;
				}
			}
		}
	}
	Put_pos_cnt &= 0xf;
	int option = (Put_pos_cnt & 1);
	uint16_t pos_cnt = Put_pos_cnt++;
	uint16_t cnt_check = (0xee00 | pos_cnt);

	uint16_t pos16 = (uint16_t)pos;
	uint16_t pos16_check = ~pos16;
	uint8_t *data_p;

	if(option == 0)
	{
		Enc_data.pos1 = pos16;
		Enc_data.pos1_check = pos16_check;
//		Enc_data.rpm = Enc_rpm;
		Enc_data.rpm = cnt_check;
		data_p = (uint8_t *) &Enc_data.pos1;
	}
	else
	{
		Enc_data.pos2 = pos16;
		Enc_data.pos2_check = pos16_check;
//		Enc_data.rpm2 = Enc_rpm;
		Enc_data.rpm2 = cnt_check;
		data_p = (uint8_t *) &Enc_data.pos2;
	}
	ac200Enc_write_Enc_data(data_p,6,50);
	return 0;
}

void Put_encoder_pos1(void)
{
//	if(Enc_pos_check(300)) return;		// MHH:09/09/2023. Cannot check here as if ISR occurs then it will cause an error.

	int e = Put_encoder_pos2();
#ifndef MH_IGNORE_ERROR
	if(e)
	{
		L2PRINTF("Put_encoder_pos1:Error:%d\r\n",e);
		SSP_set_position_error(20);
	}
#endif
}

void Put_encoder_pos(int ifrom)
{
#ifdef MH_XXX		// MHH:23/05/2025. Probably better to update.
	if(Enc_data.run_flags & ENC_RUN_FLAGS_NO_FEATHER) return;	// MHH:08/09/2023. Do not update position if position error
#endif

	int pos = Enc_Pos.pos;		// Can be changed by ISR
	if(pos == Put_pos_lastwritten)
	{
		return;
	}
	Put_pos_lastwritten = pos;
	Put_encoder_pos1();
	if(ADC_voltage_disabled)
	{
		L2PRINTF("V:%d,%d,%d\r\n",Adc_voltage,pos,Systick_tick);
	}
}

//------------------------------------------------------------------------------------
uint8_t Encoder_write_count[2];
uint16_t Encoder_pos_t[2];
int Encoder_check_pos3(uint16_t *p16,int ix)
{
	uint16_t pos = *p16;
	uint16_t pos_check = *(p16+1);
	uint16_t rpm = *(p16+2);

	uint16_t pos_complement = ~pos_check;

	uint8_t write_count;
	if(pos == pos_complement)
	{
		if(rpm)
		{
			if((rpm & 0xff00) == 0xee00)
			{
				Encoder_pos_t[ix] = pos;
				write_count = rpm & 0xff;	// lsb for write count
				if(write_count > 15)	// Valid range = 0 to 15
				{
					write_count = 50;		// error
				}
				else
				{
					uint8_t small_bit = write_count & 1;
					if(small_bit != ix)	// Must match ix. eg cnt=0, ix=0. cnt=3,ix=1
					{
						write_count = 51;		// error
					}
				}
				if(write_count >= 50)
				{
					L2PRINTF("Cnt error:%d for ix=%d\r\n",write_count,ix);
				}
				Encoder_write_count[ix] = write_count;	// lsb for write count
				return 1;
			}
			else
			{
				Enc_Pos.pos = pos;	// MHH:26/06/2025 Old format
				return 0;
			}
		}
		else
		{
			Enc_Pos.pos = pos;	// MHH:26/06/2025 Old format
			return 0;
		}
	}
	else
	{
		// Integrity check fail
		Encoder_pos_t[ix] = 60000;		// Bad value
		L2PRINTF("Encoder_check_pos3: pos != pos_check for ix=%d\r\n");
		return 2;
	}
}
int Get_encoder_pos2(void)
{
	int result = Encoder_check_pos3(&Enc_data.pos1,0);
	if(result == 0) return 0;		// Old format, position OK.
	if(result == 2)
	{
		Enc_data.error_badpos1_count++;
	}

	int result2 = Encoder_check_pos3(&Enc_data.pos2,1);
	if(result2 == 0) return 0;		// Old format, position OK.
	if(result2 == 2)
	{
		Enc_data.error_badpos2_count++;
	}
	if(result == 2 && result2 == 2)	// Both positions invalid?
	{
		return ENC_POS_GETPOS;
	}
	if(result == 2 || result2 == 2)	// Only one position valid?
	{
		if(result == 1)
		{
			Enc_Pos.pos = Encoder_pos_t[0];
		}
		else
		{
			Enc_Pos.pos = Encoder_pos_t[1];
		}
		return 0;
	}
	// OK, if we drop through then need to chose position based on which was written most recently

	if(Encoder_write_count[0] >= 50 && Encoder_write_count[1] >= 50)	// Count error in both?
	{
		L2PRINTF("Get_encoder_pos2:Cnt error in both\r\n");
		Enc_Pos.pos = Encoder_pos_t[0];
	}
	if(Encoder_write_count[0] >= 50 || Encoder_write_count[1] >= 50)	// Count error in either?
	{
		if(Encoder_write_count[0] < 16)		// Then chose the pos without error
		{
			Enc_Pos.pos = Encoder_pos_t[0];
		}
		else
		{
			Enc_Pos.pos = Encoder_pos_t[1];
		}
		return 0;
	}

/*
 * OK, if we drop through then both cnts look valid, and should be in range 0-15.
 * Normally should be in sequence, and cnt[0] can only be 0,2,4,6,8,10,12,14. Cnt[1] can only be 1,3,5, etc...
 */

	int ix = 0;
	if(Encoder_write_count[0] < Encoder_write_count[1])
	{
		if(Encoder_write_count[0] != 0)
		{
			ix = 1;
		}
	}
	else
	{
		if(Encoder_write_count[1] == 0)
		{
			ix = 1;
		}
	}
	Enc_Pos.pos = Encoder_pos_t[ix];
	Put_pos_cnt = Encoder_write_count[ix];
	Put_pos_cnt++;		// We want to start on next count
	return 0;

#ifdef MH_XXX
		uint16_t pos_complement = ~Enc_data.pos1_check;
//	if(Enc_data.pos1 == (~Enc_data.pos1_check))		// MHH:15/07/2023 - This test fails!!Normal path
	if(Enc_data.pos1 == pos_complement)		// Normal path
	{
		if(Enc_data.rpm)
		{
			if((Enc_data.rpm & 0xf0) == 0xe0)	// MHH:26/06/2025 New format?
			{
				Encoder_write_count[0] = Enc_data.rpm & 0xff;		// Use lsb for write count;
			}
			else
			{
				Enc_Pos.pos = Enc_data.pos1;
				return 0;
			}
			//			Enc_data.rpm_starts++;
		}
		else
		{
			Enc_Pos.pos = Enc_data.pos1;
			return 0;
		}
	}
	else
	{
		Encoder_pos_t[0] = 60000;		// Bad value
	}
	Dputs("pos1 != pos1_check\r\n");
	Enc_data.error_badpos1_count++;

	pos_complement = ~Enc_data.pos2_check;
//	if(Enc_data.pos2 == ~Enc_data.pos2_check)	// MHH:15/07/2023 - This test fails!!
	if(Enc_data.pos2 == pos_complement)
	{
		Enc_Pos.pos = Enc_data.pos2;
		if(Enc_data.rpm2)
		{
			Enc_data.rpm_starts++;
		}
		return 0;
	}
	Dputs("pos2 != pos2_check\r\n");
	Enc_data.error_badpos2_count++;

// Error if we drop through.

	return ENC_POS_GETPOS;
#endif

}

void SSP_set_position_error(int ifrom)
{
	Enc_data.run_flags |= ENC_RUN_FLAGS_NO_FEATHER;
	Enc_data.run_set_from = ifrom;
	Enc_ssp_update_enc_run_flags();	// MHH:04/08/2023
}

void SSP_get_encoder_pos(void)
{
	if(Get_encoder_pos2())
	{
		// Perhaps should set a flag here indicating invalid position?
		// No more position information until calibrate?
		// Could set Enc_pos to (say) fine ms stop, if fine_ms on, but not allow any further updates until calibration?
#ifndef MH_IGNORE_ERROR
		L2PRINTF("Get_encoder_pos fail, setting position error flag\r\n");
//		Enc_Pos.pos = Enc_data.cal_fine_stop;		// This is most likely position, but we need to flag an error.
		SSP_set_position_error(30);
#endif
	}
	else
	{
		Enc_pos_save2();		// MHH:08/09/2023. To try and find if pos is being overwritten.

		if(Calibrating == false)
		{
			if(Enc_Pos.pos > Enc_data.cal_fine_hard_stop + 200)
			{
#ifndef MH_IGNORE_ERROR
				L2PRINTF("Get_encoder_pos:%d > fine hard stop\r\n",Enc_Pos.pos);
				Enc_Pos.pos = Enc_data.cal_fine_stop;		// This is most likely position, but we need to flag an error.
				SSP_set_position_error(40);
#endif
			}
		}
	}
}
//------------------------------------------------------------------------------------------------------------
void SSP_ini_update_page_zero(void)
{
	Enc_data.hub_run++;
	SSP_get_encoder_pos();	// Enc_pos
	ac200_Enc_set_ud_flags();
//	if(Enc_ud_coarse_stop)  PRINTF("UD Coarse stop:%d\r\n",Enc_data.ud_coarse_stop);
	if(Enc_ud_feather_stop) PRINTF("UD feather stop:%d\r\n",Enc_data.ud_feather_stop);
	if(Enc_ud_reverse_stop) PRINTF("UD reverse stop:%d\r\n",Enc_data.ud_reverse_stop);


//	printf("SSP:run:%d,pos:%d,rpm:%d\r\n",Enc_data.run,Enc_data.pos,Enc_data.rpm);

// Update run and rpm_starts. May be useful diagnostics later.

	ac200Enc_write_Enc_data((uint8_t *)&Enc_data.hub_run,4,200);

#ifdef MH_FORCE_NEW_HUB_REVISION
	  // Force new software version every time hub restarts to test AC200 version update

	  uint16_t test_version = HUB_SOFTWARE_REVISION + Enc_data.hub_run;
	  if(Enc_data.hub_software_version != test_version)
	  {
		  Enc_data.hub_software_version = test_version;	// After loading from SPI data, as will get overwritten
		  Enc_data.rpm_starts = 0;		// Reset this
		  uint8_t *data_p = (uint8_t *)&Enc_data.rpm_starts;
		  ac200Enc_write_Enc_data(data_p,4);		// update
		  PRINTF("Version:%d\r\n",Enc_data.hub_software_version);
	  }
#else
	  if(Enc_data.hub_software_version != HUB_SOFTWARE_REVISION)
	  {
		  Enc_data.hub_software_version = HUB_SOFTWARE_REVISION;	// After loading from SSP data, as will get overwritten
//		  Enc_data.rpm_starts = 0;		// Reset this
//		  uint8_t *data_p = (uint8_t *)&Enc_data.rpm_starts;
		  uint8_t *data_p = (uint8_t *)&Enc_data.hub_software_version;	// MHH:16/12/2025

		  ac200Enc_write_Enc_data(data_p,2,300);		// update
		  PRINTF("Updating software version to:%d\r\n",HUB_SOFTWARE_REVISION);
	  }
#endif


//	p_data = (uint8_t *)&Enc_data.hub_run;
//	ac200Enc_ssp_raw_mram_write(SSP_RUN_POS,p_data,4);

}
//------------------------------------------------------------------------------------------------------------------------
#define SSP_OFFSET_64		64
extern int Mh_reset_page_zero;

void SSP_read_page_zero(void)
{
//	uint32_t magic_1;
	uint16_t magic_1;	// MHH:18/06/2023

	if(Mh_reset_page_zero == 123)	// Allow test under debug
	{
		SSP_initialise_page_zero();
		return;			// Need both to be invalid before we initialise.
	}

	uint8_t *p_data;
	bool pos_data_loaded = false;

	if(IsTrue(LPC845_flash_data_loaded,AC200ENC_SSP + 10))
	{
		p_data = (uint8_t *)&magic_1;
		int result = ac200Enc_ssp_mram_read(SPI_MRAM_CS,SSP_MAGIC_1_POS,p_data,2);	// MHH:18/06/2023
		if(result)
		{
			PRINTF("mram_read_error:A\r\n");		// Maybe should just go into loop after, as this is really bad
			return;
		}

		// Not sure why I am reading magic for SSP rec, except if it is valid then not loading first 64 bytes from ssp.

//		if(Enc_data.magic == SSP_MAGIC_1)
		if(magic_1 == SSP_MAGIC_1)		// MHH:18/06/2023
		{
// Looking OK, just need to read last data2 bytes into Enc_data structure...
			p_data = (uint8_t *)&Enc_data.magic2;
			uint8_t *p_data_end = (uint8_t *)&Enc_data.end_data2;
			int len = p_data_end - p_data;
			int result = ac200Enc_ssp_mram_read(SPI_MRAM_CS,SSP_OFFSET_64,p_data,len);
			if(result)
			{
				Dputs("mram_read_error:B\r\n");		// Maybe should just go into loop after, as this is really bad
				return;
			}
			if(Enc_data.magic2 == SSP_MAGIC_2)
			{
				pos_data_loaded = true;
			}
		}
	}
	if(pos_data_loaded == false)
	{
		SSP_read_Enc_data();
	}
}
//-------------------------------------------------------------------------------------------------
// Save data to ssp file
void ac200Enc_write_Enc_data(uint8_t *data_p,int len,int ifrom)
{
//	PRINTF("WD:%d\r\n",ifrom);
	uint8_t *start_struct_p = (uint8_t *)&Enc_data;
	uint32_t pos = data_p - start_struct_p;
	ac200Enc_ssp_raw_mram_write(SPI_MRAM_CS,pos,data_p,len,500);
}
//-------------------------------------------------------------------------------------------------
void ac200Enc_display_enc_data(void)
{
	PRINTF("Enc_data:\r\n");
	PRINTF("pos   :%d\r\n",Enc_data.pos1);
	PRINTF("UDS   :%d\r\n",Enc_data.user_defined_stops);
	PRINTF("Fine stop:%d\r\n",Enc_data.cal_fine_stop);
	PRINTF("User defined:\r\n");
	if(Enc_data.cal_coarse_stop)  PRINTF("Coarse stop : %d\r\n",Enc_data.cal_coarse_stop);
	if(Enc_data.ud_feather_stop) PRINTF("Feather stop: %d\r\n",Enc_data.ud_feather_stop);
	if(Enc_data.ud_reverse_stop) PRINTF("Reverse stop: %d\r\n",Enc_data.ud_reverse_stop);
}
//-------------------------------------------------------------------------------------------------
/**
 * @brief	Main routine for SSP example
 * @return	Nothing
 */
void SSP_Mram_Test(void);

int ac200Enc_SSP_Init(void)
{
	SSP_Flash_Read_Status(SPI_MRAM_CS);
//	SSP_Mram_Test();
	SSP_read_page_zero();
	SSP_ini_update_page_zero();
	return 0;
}
#endif
//#define MH_TEST_SSP
#ifdef MH_TEST_SSP
static uint8_t TestReadBuffer[128];
//#define MRAM_TEST_START2		160
void SSP_Mram_Test(void)
{
	PRINTF("SSP_Mram_Test:\n\r");
	PRINTF("Reading 100 x 100 byte pages, starting from position %d\r\n",MRAM_TEST_START);

	uint32_t t1 = us_ticker_read();
	uint32_t pos = MRAM_TEST_START;

	//	for(int r=0;r<MRAM_80BYTE_PAGES;r++)
		for(int r=0;r<100;r++)
	//	for(int r=0;r<5;r++)
		{
			ac200Enc_ssp_mram_read(SPI_MRAM_CS,pos,TestReadBuffer,100);
	//		SSP_ReadBuffer(pos,TestReadBuffer,80);
	#ifdef MH_PRINT_PROGRESS
			if(r%20 == 0) printf("\r\n");
			printf("%6d",r);
	#endif
			pos += 100;
		}

	uint32_t t2 = us_ticker_read();
	uint32_t elapsed_us = t2 - t1;
	PRINTF("\r\nFinished reading test pages, elapsed = %d (ms)\r\n",elapsed_us/1000);
}
static uint8_t TestWriteBuffer[128],TestReadBuffer[128];
//#define MRAM_TEST_START2		160
void SSP_Mram_Test(void)
{
	PRINTF("SSP_Mram_Test:\n\r");
	PRINTF("Writing 80 byte pages, starting from position %d\r\n",MRAM_TEST_START);

	uint32_t n=1;
	uint32_t t1 = us_ticker_read();
	uint32_t pos = MRAM_TEST_START;
//	for(int r=0;r<MRAM_80BYTE_PAGES;r++)
	for(int r=0;r<5;r++)
	{
		char *bp = (char *)TestWriteBuffer;
		for(int i=0;i<10;i++)
		{
			sprintf(bp,"%8d",n);
			bp+= 8;
			n++;
		}
		ac200Enc_ssp_mram_write(pos,TestWriteBuffer,80);
		SSP_Flash_Read_Status();
//		SSP_WriteBuffer(pos,TestWriteBuffer,80);
#ifdef MH_PRINT_PROGRESS
		if(r%20 == 0) printf("\r\n");
		printf("%6d",r);
#endif
		pos += 80;
	}
	uint32_t t2 = us_ticker_read();
	uint32_t elapsed_us = t2 - t1;
	PRINTF("\r\nFinished writing test pages, elapsed = %d (ms)\r\n",elapsed_us/1000);
	PRINTF("Now checking\r\n");

	n = 1;
	pos = MRAM_TEST_START;

//	for(int r=0;r<MRAM_80BYTE_PAGES;r++)
	for(int r=0;r<5;r++)
//	for(int r=0;r<5;r++)
	{
//		memset(TestWriteBuffer,0,sizeof(TestWriteBuffer));
		char *bp = (char *)TestWriteBuffer;
		for(int i=0;i<10;i++)
		{
			sprintf(bp,"%8d",n);
			bp+= 8;
			n++;
		}
		ac200Enc_ssp_mram_read(pos,TestReadBuffer,80);
//		SSP_ReadBuffer(pos,TestReadBuffer,80);
#ifdef MH_PRINT_PROGRESS
		if(r%20 == 0) printf("\r\n");
		printf("%6d",r);
#endif
		if(memcmp(TestReadBuffer,TestWriteBuffer,80))
		{
			PRINTF("\r\nBuffer:%d different\r\n",r);
		}
		pos += 80;
	}
	uint32_t t3 = us_ticker_read();
	elapsed_us = t3 - t2;
	PRINTF("\r\nFinished reading test pages, elapsed = %d (ms)\r\n",elapsed_us/1000);
}
#endif


/**
 * @}
 */
