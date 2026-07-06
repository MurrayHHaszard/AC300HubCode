/*
 * ac210_ssp.h
 *
 *  Created on: 3/01/2017
 *      Author: Murray
 */

#ifndef AC300ENC_SSP_H_
#define AC300ENC_SSP_H_
#include "./board.h"

int ac200Enc_SSP_Init(void);
int SSP_read_pos(void);
void ac200Enc_SSP_save_pos(void);
void ac200Enc_SSP_write_pos(int ifrom);
void ac200Enc_SSP_save_coarse_stop(int coarse_stop);
void ac200Enc_SSP_save_ud_coarse_stop(uint16_t ud_coarse_stop);
void ac200Enc_SSP_save_stop_flags(uint8_t user_stops);
void ac200Enc_SSP_save_ini_data(void);

int ac200Enc_ssp_mram_write(int spi_num,uint32_t pos,uint8_t *buff, uint32_t len);
void ac200Enc_ssp_raw_mram_write(int spi_num,uint32_t byte_address,uint8_t *buff, uint32_t len,int ifrom);
int ac200Enc_ssp_mram_read(int spi_num,uint32_t pos,uint8_t *buff, uint32_t len);

void ac200Enc_display_enc_data(void);
int ac200Enc_SSP_save_hub_data(void);

int SSP_copy_Hub_data_to_Enc_data(void);
void ac200Enc_write_Enc_data(uint8_t *data_p,int len,int ifrom);

#define SSP_MAGIC_1			11111
#define SSP_MAGIC_2			22222
#define SSP_MAGIC_3         33333

#define	SSP_MAGIC_1_POS		0
//#define	SSP_MAGIC_2_POS		64
//#define SSP_MAGIC_3_POS		114


void SSP_Flash_Wait_Ready(int spi_num);
void SSP_Write_Enable(int spi_num);
void SSP_copy_address(uint32_t byte_address);
void SSP_RW_Poll_Mode(int spi_num,int length);
int SSP_ReadBuffer(int spi_num,uint32_t byte_address, uint8_t *pBuffer, uint32_t len);
void SSP_WriteBuffer(int spi_num,uint32_t byte_address, uint8_t *pBuffer, uint32_t len);

#define SSP_BUFFER_SIZE                         (0x110)

extern bool SSP_flash_wait_ready;
extern uint8_t Tx_Buf[SSP_BUFFER_SIZE];
extern uint8_t Rx_Buf[SSP_BUFFER_SIZE];



#endif /* AC300ENC_SSP_H_ */
