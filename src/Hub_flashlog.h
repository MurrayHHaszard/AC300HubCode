/*
 * Hub_flashlog.h
 *
 *  Created on: 6/10/2023
 *      Author: OEM
 */

#ifndef HUB_FLASHLOG_H_
#define HUB_FLASHLOG_H_

#include "AC300_hub.h"

void Hub_ssp_raw_flash_write(uint32_t byte_address,uint8_t *buff, uint32_t len);
void Flog_put_ring_data(void);
void Flog_put_ring_data_adxl375(int adxl375_x_tot);
int Flog_getc2(void);

void Adxl375_start_measure(void);
void Adxl375_stop_measure(void);
void Adxl375_read_xyz_if_ready(void);

extern int Adxl375_z_tot;
extern int Adxl375_z_count;

extern bool Flog_save_data_flag;
extern uint32_t Flog_getc_ptr;

#endif /* HUB_FLASHLOG_H_ */
