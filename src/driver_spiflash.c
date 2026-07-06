#include <string.h>
#include "LPC8xx.h"
#include "syscon.h"
#include "swm.h"
#include "spi.h"
#include "chip_setup.h"
#include "driver_spiflash.h"

#ifdef MH_SPI_INTERFACE
uint8_t  WriteDataBuf[BUFFER_SIZE];
uint8_t  ReadDataBuf[BUFFER_SIZE];
uint32_t WrRdLength;

#define FLASH_BYTE_ADDR1(address) ((address >> 16) & 0xf)
#define FLASH_BYTE_ADDR2(address) ((address >> 8) & 0xf)
#define FLASH_BYTE_ADDR3(address) (address & 0xf)

void spiflash_write_enable(void)
{
	WriteDataBuf[0] = SPIFLASH_WriteEnable;
	WrRdLength = 1;
	SPImasterWriteOnly( WriteDataBuf, WrRdLength );
}

void spiflash_write_disable(void)
{
	WriteDataBuf[0] = SPIFLASH_WriteDisable;
	WrRdLength = 1;
	SPImasterWriteOnly( WriteDataBuf, WrRdLength );
}

/* Check Busy */
uint8_t spiflash_check_status(void)
{
	WriteDataBuf[0] = SPIFLASH_ReadStatusReg1;
	WriteDataBuf[1] = DUMMY_BYTE;
	WrRdLength = 2;	
	SPImasterWriteRead( WriteDataBuf, ReadDataBuf, WrRdLength );
	return (  ReadDataBuf[1] );
}

void spiflash_chip_erase( void )
{
	WriteDataBuf[0] = SPIFLASH_ChipErase;
	WrRdLength = 1;	
	
	SPImasterWriteOnly( WriteDataBuf, WrRdLength ); 
}

void spiflash_write_data(uint32_t WriteAddr, uint32_t numBytes, uint8_t *data)
{
	WriteDataBuf[0] = SPIFLASH_PageProgram;
	WriteDataBuf[1] = FLASH_BYTE_ADDR1(WriteAddr);
	WriteDataBuf[2] = FLASH_BYTE_ADDR2(WriteAddr);
	WriteDataBuf[3] = FLASH_BYTE_ADDR3(WriteAddr);
	memcpy ( &WriteDataBuf[4], data, numBytes );
	WrRdLength = numBytes + 4;	
	
	SPImasterWriteOnly( WriteDataBuf, WrRdLength );
}

void spiflash_read_data(uint32_t ReadAddr, uint32_t numBytes, uint8_t *data)
{
	WriteDataBuf[0] = SPIFLASH_ReadData;
	WriteDataBuf[1] = FLASH_BYTE_ADDR1(ReadAddr);
	WriteDataBuf[2] = FLASH_BYTE_ADDR2(ReadAddr);
	WriteDataBuf[3] = FLASH_BYTE_ADDR3(ReadAddr);
	WrRdLength = numBytes + 4;	
	
	SPImasterWriteRead( WriteDataBuf, ReadDataBuf, WrRdLength+4 );
	memcpy ( data, &ReadDataBuf[4], numBytes );
}

uint32_t spiflash_read_id(void)
{
	WriteDataBuf[0] = SPIFLASH_JedecDeviceID;
	WriteDataBuf[1] = DUMMY_BYTE;
	WriteDataBuf[2] = DUMMY_BYTE;
	WriteDataBuf[3] = DUMMY_BYTE;
	WrRdLength = 4;	
	SPImasterWriteRead( WriteDataBuf, ReadDataBuf, WrRdLength );
	return ( (uint32_t)((ReadDataBuf[1] << 16) | (ReadDataBuf[2] << 8) | (ReadDataBuf[3])) );
}

#endif
// end file
