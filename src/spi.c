/*
 * spi.c
 *
 *  Created on: Apr 5, 2016
 *
 */

#include "LPC8xx.h"
#include "utilities.h"
#include "spi.h"

uint32_t SPI_RXCount, SPI_TXCount;

void SPI0_IRQHandler(void)
{
  uint32_t active = LPC_SPI0->STAT & LPC_SPI0->INTENSET;

  if(active & SPI_STAT_RXRDY) {
		SPI_RXCount++;
		LPC_SPI0->INTENCLR = SPI_STAT_RXRDY;
  }
  if(active & SPI_STAT_TXRDY) {
		SPI_TXCount++;
		LPC_SPI0->INTENCLR = SPI_STAT_TXRDY;
  }
  return;
}

/*****************************************************************************
** Function name:		SPImasterWriteOnly
**
** Description:		  Write a block of data to the SPI slave using polling.
**									In this example, data width is limited to 8 bits only.
**
** parameters:      pointer to the write buffer.
**                  Length of the write data block 
** Returned value:		None
**
*****************************************************************************/
#ifdef MH_SPI_MASTER_WRITE_ONLY
void SPImasterWriteOnly(int chip_select, uint8_t *WrBuf, uint32_t WrLen )
{	
    uint32_t i = 0;
	volatile uint32_t temp;

	LPC_SPI0->TXCTL &= ~(SPI_CTL_EOT);                 // Start a new transfer, clear the EOT bit

//	SPI_flash_cs0(spi_num);
//	SPI_FLASH_CS0();	/* Drive SPI CS low. */

	uint8_t chip_select_bits = 3;				// Could be SSEL0 or SSEL1
	chip_select_bits ^= (1 << chip_select);		// We want to turn off the chip select bits that we want to use. 0 = active, 1 = inactive
	uint32_t txdatctl = (chip_select_bits << 16) | (SPI_CTL_LEN(DATA_WIDTH));

	if ( WrLen == 1 ) {
		while ( (LPC_SPI0->STAT & SPI_STAT_TXRDY) == 0 );
//		LPC_SPI0->TXDATCTL = SPI_CTL_LEN(DATA_WIDTH) | SPI_CTL_EOT | *WrBuf;
		LPC_SPI0->TXDATCTL = txdatctl | SPI_CTL_EOT | *WrBuf;
	}
	else {
		while ( i < WrLen ) {
			/* Move only if TXRDY is ready */
			while ( (LPC_SPI0->STAT & SPI_STAT_TXRDY) == 0 );
			if ( i == 0 ) {
//				LPC_SPI0->TXDATCTL = SPI_CTL_LEN(DATA_WIDTH) | SPI_TXDATCTL_RX_IGNORE | *WrBuf++;
				LPC_SPI0->TXDATCTL =  txdatctl | SPI_TXDATCTL_RX_IGNORE | *WrBuf++;
			}
			else if ( i == WrLen-1 ) {
//				LPC_SPI0->TXDATCTL = SPI_CTL_LEN(DATA_WIDTH) | SPI_CTL_EOT | *WrBuf++;
				LPC_SPI0->TXDATCTL = txdatctl | SPI_CTL_EOT | *WrBuf++;
			}
			else {
				LPC_SPI0->TXDAT = *WrBuf++;
			}
			i++;
		}
	}
	while ( (LPC_SPI0->STAT & SPI_STAT_RXRDY) == 0 );	/* For last frame to be sent, wait until RX on MISO arrives before raising CS. */
	temp = LPC_SPI0->RXDAT;
//	SPI_flash_cs1(spi_num);
//	SPI_FLASH_CS1();	/* Drive SPI CS high */
}
#endif
/*****************************************************************************
** Function name:		SPImasterWriteRead
**
** Description:		  Write data on the SPI MOSI line while reading data from MISO.
**									In this example, data width is limited to 8 bits only.
**
** parameters:      pointer to the write buffer
                    pointer to the read buffer
                    length of the write/read data buffer
** Returned value:	None
**
*****************************************************************************/
void SPImasterWriteRead( int chip_select,uint8_t *WrBuf, uint8_t *RdBuf, uint32_t WrLen )
{
	uint32_t i = 0;

	LPC_SPI0->TXCTL &= ~(SPI_CTL_EOT);          // Start a new transfer, clear the EOT bit
	//	SPI_FLASH_CS0();	/* Drive SPI CS low. */

	uint8_t chip_select_bits = 3;				// Could be SSEL0 or SSEL1
	chip_select_bits ^= (1 << chip_select);		// We want to turn off the chip select bits that we want to use. 0 = active, 1 = inactive
	uint32_t txdatctl = (chip_select_bits << 16) | (SPI_CTL_LEN(DATA_WIDTH));

	//  	SPI_flash_cs0(chip_select);
	if ( WrLen == 1 ) {
		while ( (LPC_SPI0->STAT & SPI_STAT_TXRDY) == 0 );
		//		LPC_SPI0->TXDATCTL = SPI_CTL_LEN(DATA_WIDTH) | SPI_CTL_EOT | *WrBuf;
		LPC_SPI0->TXDATCTL = txdatctl | SPI_CTL_EOT | *WrBuf;
		while ( (LPC_SPI0->STAT & SPI_STAT_RXRDY) == 0 );
		*RdBuf = LPC_SPI0->RXDAT;
	}
	else
	{
		while ( (LPC_SPI0->STAT & SPI_STAT_TXRDY) == 0 );
		LPC_SPI0->TXDATCTL = txdatctl | *WrBuf++;
		while ( (LPC_SPI0->STAT & SPI_STAT_RXRDY) == 0 );
		*RdBuf++ = LPC_SPI0->RXDAT;
		uint32_t ilast = WrLen - 1;
		for(i=1;i<ilast;i++)
		{
			while ( (LPC_SPI0->STAT & SPI_STAT_TXRDY) == 0 );
			LPC_SPI0->TXDAT = *WrBuf++;
			while ( (LPC_SPI0->STAT & SPI_STAT_RXRDY) == 0 );
			*RdBuf++ = LPC_SPI0->RXDAT;
		}
		while ( (LPC_SPI0->STAT & SPI_STAT_TXRDY) == 0 );
		LPC_SPI0->TXDATCTL = txdatctl | SPI_CTL_EOT | *WrBuf++;
		while ( (LPC_SPI0->STAT & SPI_STAT_RXRDY) == 0 );
		*RdBuf++ = LPC_SPI0->RXDAT;
	}
#ifdef MH_SLOWER_SPI
		while ( i < WrLen ) {
			/* Move only if TXRDY is ready */
			while ( (LPC_SPI0->STAT & SPI_STAT_TXRDY) == 0 );
			if ( i == 0 ) {
				//				LPC_SPI0->TXDATCTL = SPI_CTL_LEN(DATA_WIDTH) | *WrBuf++;
				LPC_SPI0->TXDATCTL = txdatctl | *WrBuf++;
			}
			else if ( i == WrLen-1 ) {
				//				LPC_SPI0->TXDATCTL = SPI_CTL_LEN(DATA_WIDTH) | SPI_CTL_EOT | *WrBuf++;
				LPC_SPI0->TXDATCTL = txdatctl | SPI_CTL_EOT | *WrBuf++;
			}
			else {
				LPC_SPI0->TXDAT = *WrBuf++;
			}
			while ( (LPC_SPI0->STAT & SPI_STAT_RXRDY) == 0 );
			*RdBuf++ = LPC_SPI0->RXDAT;
			i++;
		}
	}
#endif
//	SPI_flash_cs1(chip_select);
	//	SPI_FLASH_CS1();	/* Drive SPI CS high */
//	return;
}
#ifdef MH_YYY
void SPImasterWriteRead( int spi_cs,uint8_t *WrBuf, uint8_t *RdBuf, uint32_t WrLen )
{	
	uint32_t i = 0;

  LPC_SPI0->TXCTL &= ~(SPI_CTL_EOT);                 // Start a new transfer, clear the EOT bit	
  LPC_SPI0->TXCTL |= (1 << (16+spi_cs));			// MHH:29/11/2023. Specify Chip Select. See definition of SPI_CTL_TXSSEL0_N
#ifdef MH_YYY
  switch(spi_cs)		// May be able to do this before this routine....
  {
  case 0:
	  LPC_SPI0->TXCTL |= SPI_CTL_TXSSEL0_N;
	  break;
  case 1:
	  LPC_SPI0->TXCTL |= SPI_CTL_TXSSEL1_N;
	  break;
  default:
	  return;		// Should never get here.
  }
#endif
//	SPI_FLASH_CS0();	/* Drive SPI CS low. */
//	SPI_flash_cs0(spi_num);
	if ( WrLen == 1 ) {
		while ( (LPC_SPI0->STAT & SPI_STAT_TXRDY) == 0 );
		LPC_SPI0->TXDATCTL = SPI_CTL_LEN(DATA_WIDTH) | SPI_CTL_EOT | *WrBuf;
		while ( (LPC_SPI0->STAT & SPI_STAT_RXRDY) == 0 );
		*RdBuf = LPC_SPI0->RXDAT;
	}
	else {
		while ( i < WrLen ) {
			/* Move only if TXRDY is ready */
			while ( (LPC_SPI0->STAT & SPI_STAT_TXRDY) == 0 );
			if ( i == 0 ) {
				LPC_SPI0->TXDATCTL = SPI_CTL_LEN(DATA_WIDTH) | *WrBuf++;
			}
			else if ( i == WrLen-1 ) {
				LPC_SPI0->TXDATCTL = SPI_CTL_LEN(DATA_WIDTH) | SPI_CTL_EOT | *WrBuf++;
			}
			else {
				LPC_SPI0->TXDAT = *WrBuf++;
			}
			while ( (LPC_SPI0->STAT & SPI_STAT_RXRDY) == 0 );
			*RdBuf++ = LPC_SPI0->RXDAT;
			i++;
		}
	}
//	SPI_flash_cs1(spi_num);
//	SPI_FLASH_CS1();	/* Drive SPI CS high */
	return;
}
#endif
/*****************************************************************************
** Function name:		SPImasterReadOnly
**
** Description:		  Read a block of data to the SPI slave using polling. It's almost
**                  the same as SPImasterWriteRead() except that SPI master writes 
**                  dummy data the MOSI.
**									In this example, data width is limited to 8 bits only.
**                  
** parameters:      pointer to the read buffer.
**                  Length of the write data block 
** Returned value:		None
**
*****************************************************************************/
#ifdef MH_SPI_MASTER_READONLY
void SPImasterReadOnly(int chip_select, uint8_t *RdBuf, uint32_t RdLen )
{	
	uint32_t i = 0;

	LPC_SPI0->TXCTL &= ~(SPI_CTL_EOT);                 // Start a new transfer, clear the EOT bit
	//  SPI_flash_cs0(spi_num);
	//	SPI_FLASH_CS0();	/* Drive SPI CS low. */

	uint8_t chip_select_bits = 3;				// Could be SSEL0 or SSEL1
	chip_select_bits ^= (1 << chip_select);		// We want to turn off the chip select bits that we want to use. 0 = active, 1 = inactive
	uint32_t txdatctl = (chip_select_bits << 16) | (SPI_CTL_LEN(DATA_WIDTH));

	if ( RdLen == 1 ) {
		while ( (LPC_SPI0->STAT & SPI_STAT_TXRDY) == 0 );
		//		LPC_SPI0->TXDATCTL = SPI_CTL_LEN(DATA_WIDTH) | SPI_CTL_EOT | DUMMY_BYTE;
		LPC_SPI0->TXDATCTL = txdatctl | SPI_CTL_EOT | DUMMY_BYTE;
		while ( (LPC_SPI0->STAT & SPI_STAT_RXRDY) == 0 );
		*RdBuf = LPC_SPI0->RXDAT;
	}
	else {
		while ( i < RdLen ) {
			/* Move only if TXRDY is ready */
			while ( (LPC_SPI0->STAT & SPI_STAT_TXRDY) == 0 );
			if ( i == 0 ) {
				//				LPC_SPI0->TXDATCTL = SPI_CTL_LEN(DATA_WIDTH) | DUMMY_BYTE;
				LPC_SPI0->TXDATCTL = txdatctl | DUMMY_BYTE;
			}
			else if ( i == RdLen-1 ) {
				//				LPC_SPI0->TXDATCTL = SPI_CTL_LEN(DATA_WIDTH) | SPI_CTL_EOT | DUMMY_BYTE;
				LPC_SPI0->TXDATCTL = txdatctl | SPI_CTL_EOT | DUMMY_BYTE;
			}
			else {
				LPC_SPI0->TXDAT = DUMMY_BYTE;
			}
			while ( (LPC_SPI0->STAT & SPI_STAT_RXRDY) == 0 );
			*RdBuf++ = LPC_SPI0->RXDAT;
			i++;
		}
	}
	//	  SPI_flash_cs1(spi_num);
	//	SPI_FLASH_CS1();	/* Drive SPI CS high */
	return;
}
#endif
