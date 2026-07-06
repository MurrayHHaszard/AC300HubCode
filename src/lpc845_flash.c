/*
===============================================================================
 Name        : ac210_flash.c
 Author      : $(author)
 Version     :
 Copyright   : $(copyright)
 Description : main definition
===============================================================================
*/
// Derived from mh_flash_iap.c

#if defined (__USE_LPCOPEN)
#if defined(NO_BOARD_LIB)
#include "chip.h"
#else
#include "board.h"
#endif
#endif

#include <stdio.h>
#include "LPC8xx.h"
#include "chip_setup.h"
#include "iap.h"
#include "FlashFaimPrg.h"                      // FlashOS Structures


#include <cr_section_macros.h>

#include "AC300_hub.h"
#include "ac300Enc_ssp.h"

// TODO: insert other include files here

// TODO: insert other definitions and declarations here

/* Last sector address */
#define START_ADDR_LAST_SECTOR  0x00000000
//#define START_ADDR_LAST_SECTOR  0x00078000

/* Size of each sector */
//#define SECTOR_SIZE             1024

/* LAST SECTOR */
#define IAP_LAST_SECTOR         0

/* Number of bytes to be written to the last sector */

#define LPC845_PAGE_SIZE		64		// bytes

#define IAP_NUM_BYTES_TO_WRITE  LPC845_PAGE_SIZE
#define NUM_BYTES				LPC845_PAGE_SIZE

//#define IAP_NUM_BYTES_TO_WRITE  32	// Does not work!!

/* Number elements in array */
#define ARRAY_ELEMENTS          (IAP_NUM_BYTES_TO_WRITE / sizeof(uint32_t))

/* Data array to write to flash */
static uint32_t src_iap_array_data[ARRAY_ELEMENTS];

/*
#define WHICH_SECTOR END_SECTOR // 63
#define WHICH_PAGE   15
#define ADDR ((WHICH_SECTOR * SECTOR_SIZE) + (WHICH_PAGE * PAGE_SIZE))
*/

#define ADDR	0

#define WHICH_SECTOR END_SECTOR // 63
#define WHICH_PAGE   0
#define MY_ADDR ((WHICH_SECTOR * SECTOR_SIZE) + (WHICH_PAGE * PAGE_SIZE))



// Note: This is a good way to producr a hardware memory fault if the flash page has not been initialised
uint32_t LPC845_read_flash_address(int offset)
{
	uint32_t *p_dword;

	p_dword = (uint32_t *)(MY_ADDR + offset*4);
	uint32_t dw = *p_dword;
	return dw;


//	= (uint32_t*)(63 * SECTOR_SIZE + offset_sector_63);
	//return *p_dword;
}
void LPC845_read_flash(void)
{
	uint32_t v;
	PRINTF("LPC845_read_flash:")
	for(int i=0;i<4;i++)
	{
		v = LPC845_read_flash_address(i);
		PRINTF("[%d]",v);
	}
	PRINTF("\r\n");
}
extern uint8_t Hub_data[64];
//bool LPC845_flash_data_loaded;
Boolean LPC845_flash_data_loaded;
//int Mh_reset_page_zero = 123;
int Mh_reset_page_zero;
int LPC845_read_hub_data_from_flash(void)
{
	uint32_t *p_dword_src;
	uint32_t *p_dword_dst;


	LPC845_flash_data_loaded = False;	// Will use this to decide how much to load from SSP

	if(Mh_reset_page_zero == 123)	// Test resetting page zero
	{
		return 3;
	}

	// First copy in existing sector zero

    p_dword_src = (uint32_t *)MY_ADDR;
    p_dword_dst = (uint32_t *)Hub_data;
	for (int i = 0; i < ARRAY_ELEMENTS; i++) {
//		src_iap_array_data[i] = *p_dword++;
		p_dword_dst[i] = *p_dword_src++;
	}
	uint32_t flash_crc32 = p_dword_dst[15];		// 16 dwords per flash page
	uint32_t check_crc32 = Wiki_CRC32(Hub_data,60);
	if(check_crc32 != flash_crc32)
	{
		L2PRINTF("Flash Enc_data load FAIL\r\n")
				return 1;		// Error
	}
	// Now to update Enc_data structure...

	if(SSP_copy_Hub_data_to_Enc_data())
	{
		return 2;
	}

	PRINTF("Enc_data loaded from flash\r\n");
	LPC845_flash_data_loaded = True;
	return 0;
}


int LPC845_write_hub_data_to_flash(void)
{

	uint32_t ret_code;
	// Disable all interrupts during IAP calls
		  __disable_irq();

		  // IAP Prepare Sectors for Write command can be skipped since we are using
		  // the drivers in FlashFaimPrg.c which do the Prepare.

		  // IAP Erase Page command (pass the start address of the last page in the last sector; sector 63, page 15)
		  // The ending address, or any other address in the page will work also. The driver handles the math.
		  if ((ret_code = ErasePage(MY_ADDR))) {
		    PRINTF ("ErasePage failed. Return code was 0x%X\n\r", ret_code);
		      __enable_irq();
		      return 1;
		  }

		  // This does depend on Hub_data being aligned on dword boundary. If it isn't then will need to copy to array.

		  // IAP Copy RAM to Flash command
		  if ((ret_code = Program(MY_ADDR, NUM_BYTES, (uint8_t *)Hub_data)))
		  {
		    PRINTF ("Copy RAM to Flash failed. Return code was 0x%X\n\r", ret_code);
		      __enable_irq();
		      return 1;
		  }

		  // IAP Compare command
		   if ((ret_code = Compare(MY_ADDR, (uint32_t)Hub_data, NUM_BYTES)))
		   {
		        PRINTF ("Compare failed. Return code was 0x%X\n\r", ret_code);
			      __enable_irq();
			      return 1;
		      }

		      // Reenable interrupts
	      __enable_irq();

	      PRINTF("Programming succeeded.\n\n\r");
	      return 0 ;




}
int LPC845_write_last_sector(void)
{
	for(int i=0;i<8;i++)	// Should be 16 dwords in page
	{
		src_iap_array_data[i]=i+1;
	}
	uint32_t ret_code;
	// Disable all interrupts during IAP calls
		  __disable_irq();

		  // IAP Prepare Sectors for Write command can be skipped since we are using
		  // the drivers in FlashFaimPrg.c which do the Prepare.

		  // IAP Erase Page command (pass the start address of the last page in the last sector; sector 63, page 15)
		  // The ending address, or any other address in the page will work also. The driver handles the math.
		  if ((ret_code = ErasePage(MY_ADDR))) {
		    PRINTF ("ErasePage failed. Return code was 0x%X\n\r", ret_code);
		      __enable_irq();
		      return 1;
		  }

		  // IAP Copy RAM to Flash command
		  if ((ret_code = Program(MY_ADDR, NUM_BYTES, (uint8_t *)src_iap_array_data)))
		  {
		    PRINTF ("Copy RAM to Flash failed. Return code was 0x%X\n\r", ret_code);
		      __enable_irq();
		      return 1;
		  }

		  // IAP Compare command
		   if ((ret_code = Compare(MY_ADDR, (uint32_t)src_iap_array_data, NUM_BYTES)))
		   {
		        PRINTF ("Compare failed. Return code was 0x%X\n\r", ret_code);
			      __enable_irq();
			      return 1;
		      }

		      // Reenable interrupts
	      __enable_irq();

	      PRINTF("Programming succeeded.\n\n\r");
	      return 0 ;

}
int LPC845_flash(void)
{
	uint32_t *p_dword;
	uint8_t ret_code;
//	uint32_t part_id;

    PRINTF("LPC845_flash\r\n");

    // First copy in existing sector zero

    p_dword = (uint32_t *)0;

	for (int i = 0; i < ARRAY_ELEMENTS; i++) {
		src_iap_array_data[i] = *p_dword++;
	}
	for(int i=0;i<8;i++)
	{
		PRINTF("Address:%8x,value:%8x\r\n",i*4,src_iap_array_data[i]);
	}
	if(src_iap_array_data[7] == 0)
	{
		PRINTF("Checksum already zero\r\n");
		return 0;
	}

	src_iap_array_data[7] = 0;		// This is where checksum is kept

/*
 * Following code from Flash_IAP_main.c
 */

	// Disable all interrupts during IAP calls
	  __disable_irq();

	  // IAP Prepare Sectors for Write command can be skipped since we are using
	  // the drivers in FlashFaimPrg.c which do the Prepare.

	  // IAP Erase Page command (pass the start address of the last page in the last sector; sector 63, page 15)
	  // The ending address, or any other address in the page will work also. The driver handles the math.
	  if ((ret_code = ErasePage(ADDR))) {
	    PRINTF ("ErasePage failed. Return code was 0x%X\n\r", ret_code);
	      __enable_irq();
	      return 1;
	  }

	  // IAP Copy RAM to Flash command
	  if ((ret_code = Program(ADDR, NUM_BYTES, (uint8_t *)src_iap_array_data)))
	  {
	    PRINTF ("Copy RAM to Flash failed. Return code was 0x%X\n\r", ret_code);
	      __enable_irq();
	      return 1;
	  }

	  // IAP Compare command
	   if ((ret_code = Compare(ADDR, (uint32_t)src_iap_array_data, NUM_BYTES)))
	   {
	        PRINTF ("Compare failed. Return code was 0x%X\n\r", ret_code);
		      __enable_irq();
		      return 1;
	      }

	      // Reenable interrupts
      __enable_irq();

      PRINTF("Programming succeeded.\n\n\r");
      return 0 ;

#ifdef MH_WRITE_FLASH2
	src_iap_array_data[7] = 0;		// This is where checksum is kept

	/* Read Part Identification Number*/
	part_id = Chip_IAP_ReadPID();
	PRINTF("Part ID is: %x\r\n", part_id);
#endif
	/* Disable interrupt mode so it doesn't fire during FLASH updates */

#ifdef MH_WRITE_FLASH


	__disable_irq();

	/* IAP Flash programming */
	/* Prepare to write/erase the last sector */
	ret_code = Chip_IAP_PreSectorForReadWrite(IAP_LAST_SECTOR, IAP_LAST_SECTOR);

	/* Error checking */
	if (ret_code != IAP_CMD_SUCCESS) {
		PRINTF("Chip_IAP_PreSectorForReadWrite() failed to execute, return code is: %x\r\n", ret_code);
		return 1;
	}

#endif

#ifdef MH_ERASE_SECTOR
	/* Erase the last sector */
	ret_code = Chip_IAP_EraseSector(IAP_LAST_SECTOR, IAP_LAST_SECTOR);

	/* Error checking */
	if (ret_code != IAP_CMD_SUCCESS) {
		DEBUGOUT("Chip_IAP_EraseSector() failed to execute, return code is: %x\r\n", ret_code);
	}

	/* Prepare to write/erase the last sector */
	ret_code = Chip_IAP_PreSectorForReadWrite(IAP_LAST_SECTOR, IAP_LAST_SECTOR);

	/* Error checking */
	if (ret_code != IAP_CMD_SUCCESS) {
		DEBUGOUT("Chip_IAP_PreSectorForReadWrite() failed to execute, return code is: %x\r\n", ret_code);
	}
#endif


#ifdef MH_WRITE_FLASH


	/* Write to the last sector */
	ret_code = Chip_IAP_CopyRamToFlash(START_ADDR_LAST_SECTOR, src_iap_array_data, IAP_NUM_BYTES_TO_WRITE);

	/* Error checking */
	if (ret_code != IAP_CMD_SUCCESS) {
		printf("Chip_IAP_CopyRamToFlash() failed to execute, return code is: %x\r\n", ret_code);
		return 1;
	}
	/* Re-enable interrupt mode */
	__enable_irq();
#endif


#ifdef MH_WRITE_FLASH2

	/* Start the signature generator for the last sector */
	Chip_FMC_ComputeSignatureBlocks(START_ADDR_LAST_SECTOR, (SECTOR_SIZE / 16));

	/* Check for signature generation completion */
	while (Chip_FMC_IsSignatureBusy()) {}

	/* Get the generated FLASH signature value */
	PRINTF("Generated signature for the last sector is: %x \r\n", Chip_FMC_GetSignature(0));
    return 0 ;
#endif

}
