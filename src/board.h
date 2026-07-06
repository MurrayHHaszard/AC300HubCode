/*
 * board.h
 *
 *  Created on: 19/05/2018
 *      Author: Murray
 */

#ifndef SRC_BOARD_H_
#define SRC_BOARD_H_

#include <stdint.h>
//#include "LPC8xx.h"
#include "swm.h"

#define		bool		_Bool
#if !defined(MAX)
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif
#if !defined(MIN)
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

void Config_Output_Pin(uint32_t port);
void Pin_Off(uint32_t port);
void Pin_On(uint32_t port);

void Board_turn_heater_off(void);
void Board_turn_heater_on(void);

//#define DACOUT_PIN PIO0_19
/*
 *  MHH:31/08/203. Hardware version pins. Invert values to get version. Eg P1_8 grounded = version 1 (other pins pulled up).
 *  Note that the internal hardware version is not the same as the PCB version. Internal hardware version should only be changed when
 *  there is change to PCB hardware logic.
 */
#define HARDWARE_VERSION_1_PIN	P1_8
#define HARDWARE_VERSION_2_PIN	P0_13
#define HARDWARE_VERSION_3_PIN	P1_9

#define HARDWARE_MAXON_PIN		P0_8		// If Grounded then not Maxon

#define HALL_1_PIN				P0_22		// Yellow wire, LPCXpresso pin 7, LPC845 Pin 43
#define HALL_2_PIN				P0_30		// Brown wire, LPCXpresso Pin 6, LPC845 Pin 42
#define HALL_3_PIN				P0_23		// Grey wire, LPCXpresso Pin 5, LPC845 Pin 39

#define ENABLE_PIN				P1_1		// Blue, pull-down, LPCXpresso Pin 23, LPC845 Pin 14
#define ENABLE_PIN2				P0_31		// As from Hub7 board, need 2 enable pins, both low, to turn on enable.

#define MR25H_CS_PIN			P1_7		// Chip select for MR25H.
	// Note this is called SPI_CS_PIN in spi.c and is initialised as output and used on every spi operation.

#define OVERRIDE_FINE_MS_PIN	P0_28

//#define DIR_PIN					P1_2		// Grey, LPCXpresso Pin 24, LPC845 Pin Pin 16
#define DIR_PIN					P0_16		// For hub pcb

#define AOUT_PIN				P0_17		// Blue, no pull-down, LPCXpresso Pin 18, LPC845 Pin 48
#define READY_PIN				P0_10		// Not used in breadboard, only hub pcb.
#define READY_MODE				PIO0_10


//#define DIR_PIN					P0_16		// For hub pcb
//#define BRAKE_PIN				P1_1		// (Use ENABLE_PIN) LPCXpresso Pin 23, LPC845 Pin 14
#define NSLEEP_PIN				P1_2		// No exact match on Maxon
//#define SLEEP_PIN				P1_2		// MHH:10/06/2026. As using NPN. Revert to NSLEEP_PIN if connect directly

//#define AOUT_PIN				P0_17		// Blue, no pull-down, LPCXpresso Pin 18, LPC845 Pin 48
//#define NFAULT_PIN				P0_10		// Not used in breadboard, only hub pcb.
//#define NFAULT_MODE				PIO0_10		// Use READY_PIN

#define FINE_MICROSWITCH_PIN				P1_0	// Will try and connect to interrupt logic
#define COARSE_MICROSWITCH_PIN				P0_11

/*
 *  MHH:31/08/2023. With Hub_hardware_version zero, the heater could be on when pins were in default mode (pulled up), which meant when loading program via debugger
 *  or when in debug before starting.
 *
 *  To fix this, HEATER_PIN (P0_14) needs to be on (1) to heat, and HEATER_2_PIN (P0_1) needs to be off (0). Because there is no resistor for HEATER2_PIN, the
 *  ON state is represented by READ mode pullup.
 *
 */


#define HEATER_PIN				P0_14
#define HEATER2_PIN				P0_1		// Version 11 has 2 heater switches

//#define DIAGS_OUTSIDE_RANGE_PIN		P0_1		// Uses same pin as HEATER2_PIN. Should have an #ifdef here, but board.h cannot see MH_DIAGS def
//#define DIAGS_OUTSIDE_RANGE_MODE	PIO0_1

#define MH_MAXON_CTL_ENABLED_HUB			// MHH:18/11/2025
#ifdef MH_MAXON_CTL_ENABLED_HUB
#define DIG1_DIG2_PIN			P0_21
#define DIG1_DIG2_MODE			PIO0_21
#else
#define DIG1_DIG2_PIN			P1_2
#define DIG1_DIG2_MODE			PIO1_2
#endif

#define SIG100_UART	0			// Changed from 1 on 12/12/2022 so we can look at flash programming through SIG100 pins
#define SIG100_HDC_PIN		P1_3
#define SIG100_TXPIN		P0_25
#define SIG100_RXPIN		P0_24


int Board_pin_read(uint32_t pin_def);
void Board_pin_write(uint32_t pin_def,bool pinval);
//int Board_UARTGetChar(void);
void Board_UARTPutchar(int ch);
void Board_LED_Toggle(uint32_t pindef);

void Board_init(void);

#define	PC_PUTC		Board_UARTPutchar

#endif /* SRC_BOARD_H_ */
