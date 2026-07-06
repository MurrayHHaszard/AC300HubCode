
#ifndef __DRIVER_SPIFLASH_H__
#define __DRIVER_SPIFLASH_H__

#include "stdint.h"

#define PAGE_SIZE										64

#define SPIFLASH_Status_Busy		(0x1<<0)
#define SPIFLASH_Status_WEL			(0x1<<1)
#define SPIFLASH_Status_SRP			(0x1<<7)

/*********************************************

*********************************************/
#define SPIFLASH_WriteEnable            0x06    //Write Enable
#define SPIFLASH_WriteEnableVSR         0x50    //Write Enable for Volatile Status Register
#define SPIFLASH_WriteDisable           0x04    //Write Disable
#define SPIFLASH_ReadStatusReg1         0x05
//#define SPIFLASH_ReadStatusReg2         0x35
#define SPIFLASH_WriteStatusReg         0x01
#define SPIFLASH_PageProgram            0x02
#define SPIFLASH_SectorErase            0x20
#define SPIFLASH_BlockErase32K          0x52
#define SPIFLASH_BlockErase64K          0xD8
#define SPIFLASH_ChipErase              0xC7
//#define SPIFLASH_EraseSuspend           0x75
//#define SPIFLASH_EraseResume            0x7A
#define SPIFLASH_PowerDown              0xB9
#define SPIFLASH_ContinuousReadMode     0xFF

#define SPIFLASH_ReadData               0x03
#define SPIFLASH_FastReadData           0x0B
#define SPIFLASH_FastReadDual           0x3B

#define SPIFLASH_ReleasePowerDown       0xAB   
#define SPIFLASH_DeviceID               0xAB 
#define SPIFLASH_ManufactDeviceID       0x90 
#define SPIFLASH_JedecDeviceID          0x9F

extern uint32_t spiflash_read_id(void);
extern void spiflash_write_enable(void);
extern void spiflash_write_disable(void);
extern uint8_t spiflash_check_status(void);
extern void spiflash_chip_erase( void );
extern void spiflash_write_data(uint32_t WriteAddr, uint32_t numBytes, uint8_t *data);
extern void spiflash_read_data(uint32_t ReadAddr, uint32_t numBytes, uint8_t *data);


#endif  /* __DRIVER_SPIFLASH_H__ */
