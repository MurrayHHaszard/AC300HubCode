/*
 * motor.c
 *
 *  Created on: 19/06/2018
 *      Author: Murray
 */

#include <stdio.h>
#include <math.h>

//#include "AC300_hub.h"
#include "ac300Enc_ssp.h"
#include "board.h"
#include "AC300_hub.h"
#include "Hub_flashlog.h"

#define		false		0
#define		true		(!false)

int8_t Motor_dir;
bool Motor_keyboard_command;

int Debug_print_val=0;
void Debug_print(char *s)
{
	if(Debug_print_val > 0)
	{
		Board_UART1PutSTR(s);
	}
}
#define SLEEP_TOGGLE_USECS	65	// Note 50 too small
//#define SLEEP_TOGGLE_USECS	100	// Note 50 too small
void MCT8316_driver_reset(int ifrom)
{
//	return;
	if(Hub_maxon_driver) return;
	Pin_Off(NSLEEP_PIN);			// MHH:10/06/2026. Will have to reverse if don't use NPN
	wait_us(SLEEP_TOGGLE_USECS);
	Pin_On(NSLEEP_PIN);		// MHH:10/06/2026. Will have to reverse if don't use NPN
	wait_ms(5);				// ???
	PRINTF("MCT8316_driver_reset from %d\r\n",ifrom);
//	printf("Sleeping for %d usecs\r\n",NSLEEP_TOGGLE_USECS);
}
#ifdef MH_YYY
void MCT8316_driver_reset2(void)
{
//	return;
	if(Hub_maxon_driver) return;
	Pin_On(SLEEP_PIN);			// MHH:10/06/2026. Will have to reverse if don't use NPN
	wait_us(40);
	Pin_Off(SLEEP_PIN);		// MHH:10/06/2026. Will have to reverse if don't use NPN
//	printf("Sleeping for %d usecs\r\n",NSLEEP_TOGGLE_USECS);
}
#endif
bool MCT8316_check_reset(int ifrom)
{
  	return false;
	if(Hub_maxon_driver) return false;
	if(Board_pin_read(READY_PIN) == 0)		// Actually nFAULT pin
	{
		MCT8316_driver_reset(10000+ifrom);		// Should we disable or set pwm to zero?
		if(Board_pin_read(READY_PIN) == 0)
		{
			PRINTF("nFAULT still ON after reset, from:%d\r\n",ifrom);
			return true;
		}
	}
	return false;
}
//----------------------------------------------------------------------
void Set_Direction_Fine(int ifrom)
{
	Board_pin_write(DIR_PIN,true);	// MHH:30/11/2022

	if(Motor_dir != DIR_FINE)
	{
		PRINTF("Set_Direction_Fine from:%d\r\n",ifrom);
		Motor_dir = DIR_FINE;
		if(Hub_maxon_driver == false)
		{
//			MCT8316_driver_reset(100);
		}
		//		PRINTF("SF:%d\r\n",ifrom);
		//		Debug_print("\r\ndir=FINE\r\n");
	}
}
//----------------------------------------------------------------------
void Set_Direction_Coarse(int ifrom)
{

	Board_pin_write(DIR_PIN,false);	// MHH:30/11/2022

	if(Motor_dir != DIR_COARSE)
	{
		PRINTF("Set_Direction_Coarse from:%d\r\n",ifrom);
		Motor_dir = DIR_COARSE;
		if(Hub_maxon_driver == false)
		{
//			MCT8316_driver_reset(200);
		}
//		PRINTF("SC:%d\r\n",ifrom);
//		Debug_print("\r\ndir=COARSE\r\n");
	}
}
//----------------------------------------------------------------------
bool Motor_fine_stop_override;
void Set_fine_stop_override_1(bool bval,int ifrom)
{
	if(bval)
	{
		Config_Output_Pin(OVERRIDE_FINE_MS_PIN);
		Board_pin_write(OVERRIDE_FINE_MS_PIN,false);	// Then stop Fine microswitch disabling ENABLE
		if(Motor_fine_stop_override == false)
		{
			Motor_fine_stop_override = true;
			PRINTF("[%d]FINE override ON\r\n",ifrom);
		}
//		Debug_print("\r\nFINE override pin ON\r\n");
	}
	else
	{
		Config_Input_Pin(OVERRIDE_FINE_MS_PIN);		// This should put in read mode + pullup.
		if(Motor_fine_stop_override)
		{
			Motor_fine_stop_override = false;
			PRINTF("[%d]FINE override OFF\r\n",ifrom);
		}
//		Debug_print("\r\nFINE override pin OFF\r\n");
	}
}

void Set_fine_stop_override(bool bval,int ifrom)
{
//	return;			// Test!!!MHH:11/06/2026


	if(Hub_hardware_version != 0)
	{
		Set_fine_stop_override_1(bval,ifrom);
		return;
	}

	if(bval)
	{
		Board_pin_write(OVERRIDE_FINE_MS_PIN,true);	// Then stop Fine microswitch disabling ENABLE
		Motor_fine_stop_override = true;
		PRINTF("FINE override pin ON from %d\r\n",ifrom);
			// Note: Should disable this pin the moment that fine microswitch is off. Systick? or Enc_H1_ISR?
	}
	else
	{
		Board_pin_write(OVERRIDE_FINE_MS_PIN,false);
		Motor_fine_stop_override = false;
		PRINTF("FINE override pin OFF from %d\r\n",ifrom);
	}

}
//----------------------------------------------------------------------
bool Motor_enabled=true;
int Motor_enabled_count;
void Enable_Motor(void)
{
//	Debug_print("\r\nEnabling motor:");
#ifdef MH_YYY
	if(Board_pin_read(READY_PIN) == 0)
	{
		L2PRINTF("Enable:RDY=0,Motor_enabled=%d\r\n",Motor_enabled);
		return;			// Not sure about this, need to somehow flag this situation so that we know we should be enabled but are waiting for READY_PIN.
	}
#endif

	MCT8316_check_reset(300);		// If any nFAULT error then try and clear.
// Could set a flag and print these outside interrupt
//	Board_pin_write(ENABLE_PIN,true);
	Board_pin_write(ENABLE_PIN,false);		// From Hub7 board, must be off to enable
	Board_pin_write(ENABLE_PIN2,false);		// From Hub7 board, must be off to enable
//	Board_pin_write(OVERRIDE_FINE_MS_PIN,false);	// No FINE microswitch override by default
//	Motor_fine_stop_override = false;
//	Set_fine_stop_override(false,100);

//	if(Motor_mode == REVERSE || Motor_mode == CALIBRATE_FINE)
	bool fine_stop_override = false;		// MHH:28/03/2026
	if(Motor_mode == REVERSE)
	{
		fine_stop_override = true;
//		Set_fine_stop_override(true,200);
	}
	else
	{
		if(Board_pin_read(FINE_MICROSWITCH_PIN))	// Fine microswitch on?
		{
			if(Board_pin_read(DIR_PIN) == false)	//  MHH:30/11/2022 Direction COARSE?
			{
				fine_stop_override = true;
//				Set_fine_stop_override(true,300);
			}
		}
	}
	Set_fine_stop_override(fine_stop_override,310);

#ifdef MH_YYY
	if(Board_pin_read(READY_PIN) == 0)
	{
		PRINTF("Enable:RDY=0,Motor_enabled=%d\r\n",Motor_enabled);
		return;			// Not sure about this, need to somehow flag this situation so that we know we should be enabled but are waiting for READY_PIN.
	}
#endif
	Motor_enabled = true;
	Motor_enabled_count++;

}
//----------------------------------------------------------------------
void Set_AOUT2(int val)
{
	if(Hub_maxon_driver &&  Speed_raw_mode)
	{
		Set_AOUT(val,1234);
	}
	else
	{
		Set_AOUT(0,1235);
		Motor_target_rpm = 0;		// MHH:14/06/2026

	}
}

int Debug_motor=0;	// Note: If non zero, on hitting FINE stop the AC200 times out on receive.
extern char AC300_Tick;
void Disable_Motor(int ifrom)	// Note: If this occurs under an interrupt, the PRINTF can disrupt an existing PRINTF
{
	if(Motor_enabled)
	{
#ifdef MH_XXX		// MHH:15/06/2026. Following lines cause loss of communication to AC300 if Debug_motor non zero and called from ISR
		if(Debug_motor > 0)
		{
			PRINTF("\r\nDisable from:%d\r\n",ifrom);	// This can disrupt connecting with Calibrator
		}
#endif
	}
	if(Command == '#')		// MHH:15/10/2025
	{
		if(Motor_enabled)
		{
//			PRINTF("Dis:%d\r\n",AC300_Tick);
		}
	}
//	Board_pin_write(ENABLE_PIN,false);
	Motor_mode = IDLE;
	if(Motor_dir != DIR_STOPPED)
	{
		Motor_dir = DIR_STOPPED;
//		PRINTF("DS:%d\r\n",ifrom);
	}
	Board_pin_write(ENABLE_PIN,true);		// From Hub7 board, must be on to disable
	Board_pin_write(ENABLE_PIN2,true);		// From Hub7 board, must be on to disable
//	Board_pin_write(OVERRIDE_FINE_MS_PIN,false);
//	Motor_fine_stop_override = false;
	Set_fine_stop_override(false,400);
	Motor_enabled = false;
	Set_AOUT2(100);
//	Set_AOUT(100,4);

#ifdef MH_YYY

	if(Speed_raw_mode)
	{
//		Set_AOUT(0,4);	// MHH:24/11/2025


		if(Hub_maxon_driver)
		{
			Set_AOUT(100,4);	// MHH:24/11/2025
		}
		else
		{
			Set_AOUT(0,40);
		}
#ifdef MH_XXX
		if(DAC0_v > 205)	// MHH:06/02/2025. To try and remove 'bumpy' start when using manual fine/coarse.
		{
			Set_AOUT(205,4);
		}
#endif
	}
	else
	{
		Set_AOUT(0,41);	// MHH:24/11/2025
//		Set_AOUT(100,41);
	}
#endif
	Motor_target_rpm = 0;		// MHH:19/11/2025. Because when Voltage dropped below minimum voltage then restored and not raw_mode then speed didn't change

}
//----------------------------------------------------------------------
void mh_debug(void)
{

}
int DAC0_v;
void Set_AOUT(int val,int ifrom)
{
	if(DAC0_v == val) return;

	uint32_t v = val & 1023;
//	PRINTF("Set_AOUT(%d):%d\r\n",ifrom,val);
	DAC0_v = val;

	if(Hub_maxon_driver)
	{
		v <<= 6;		// * 64
		v |= 1 << 16;
		LPC_DAC0->CR = v;
	}
	else
	{
		PWM_SetDutyTenBit(DAC0_v);
//		int duty_percent = (DAC0_v * 100) << 10;		// Get percent
//		PWM_SetDutyPercent(duty_percent);
	}
}
//----------------------------------------------------------------------
uint32_t Motor_target_rpm;
//extern int Enc_rpm;
//static int save_rpm;
//----------------------------------------------------------------------
/*
 *  As per DEC 50/5, with DigiN1 = 0, DigiN2 = 1 and 1 pole pair
 *
 *  	0V = 500 RPM
 *  	5V = 20,000 RPM
 *
 *  	So 1 RPM = 5V/20,00)
 *
 *  	Since Aout has a maximum value of 3.3V, we have to set a val of between 0 and 1023 to get desired voltage.
 *
 *  	So, if we want (say) 6000 RPM, then desired output voltage is 6000/20,000 * 5V = 1.5V
 *
 *		So, if val = (Vout / 3.3) * 1023 then if Vout = 1.5V
 *
 *		val = (1.5/3.3) * 1023 = 465
 *
 *
 */
//#define FUDGE		117
//#define FUDGE		100
int Get_aout_val_for_rpm(int rpm)
{
	int val;
	int percent;
//	int divisor;
	int pole_pairs = Enc_data.cal_pole_pairs;
	switch(pole_pairs)
	{
	case 1:
		val = rpm / 50;
		break;

	case 2:	// Pure guess..
		val = rpm / 25;
		break;

	case 4:
		val = rpm / 16;
		percent = 121;		// default
		if(rpm < 2000) percent = 128;
		if(rpm < 1000) percent = 142;	// MHH:21/11/2025 Trial and error
		val = (val * percent) / 100;
#ifdef MH_XXX
		divisor = 65;
		if(rpm >= 4000) divisor = 66;
		if(rpm >= 6000) divisor = 67;
		val = (rpm * 10) / divisor;		// Not sure why change???
#endif
		break;
	default:
		Sig100_format_error_pkt('A',HUB_ABORT_INVALID_POLEPAIR,1);
		val = 0;
		break;
	}
	if(Speed_opamp)
	{
		val = (val * 2)/3;		// Because opamp boosts DAC pin by a third.
	}

	return val;
}

#ifdef MH_XXX	// MHH:18/02/2025. Not used
void Set_Motor_RPM2(int rpm)	// Used by calibration routine. Allows '0' speed (crawl).
{
	Motor_target_rpm = rpm;
	int aout_val = Get_aout_val_for_rpm(rpm);
	Set_AOUT(aout_val,2);
	Enable_Motor();
//	Motor_systick = 0;		// Start speed control
}
#endif
#ifdef MH_XXX		// MHH:22/11/2025
void Set_Motor_RPM(int rpm,int ifrom)
{
	if(Motor_target_rpm != rpm)	//MHH:21/11/2025.  MHH:24/05/2023.
	{
		int aout_val;
		if(Speed_raw_mode)
		{
			aout_val = 200;
			if(rpm > Motor_target_rpm)
			{
				aout_val = (1024 * Motor_target_rpm) / 10000;
	//			if(aout_val < 100) aout_val = 100;
				if(aout_val < 200) aout_val = 200;	// MHH:07/07/2025
			}
		}
		else
		{
			aout_val = Get_aout_val_for_rpm(rpm);
			// Note: If not using raw mode, and we have opamp, should multiply by 2/3 to compensate
		}
		Motor_target_rpm = rpm;
		if(aout_val > 1023) aout_val = 1023;
		Set_AOUT(aout_val,3);
//		Motor_systick = 0;		// Start speed control

#ifdef MH_XXX
		if(Debug_motor > 0)
		{
			PRINTF("\r\nSetting RPM to: %d, from: %d\r\n",rpm,ifrom);
		}
#endif
	}
	Enable_Motor();
#ifdef MH_XXX		// MHH:1/4/2023. Try allowing speed to be zero. May be good for braking.
	if(rpm == 0)
	{
		Disable_Motor(2000);
	}
	else
	{
		Enable_Motor();
	}
#endif
}
#endif
//int RS_rpm_per_aout_unit_2dec = (6000 * 100)/1024;		// Ini. Say at 100% we are getting 6000 rpm
#define RS_RPM_PER_AOUT_UNIT_2DEC 	((6000 * 100)/1024)		// MHH:16/03/2026. Ini. Say at 100% we are getting 6400 rpm
uint32_t RS_systick_start;
void Set_Motor_RPM(int rpm,int ifrom)
{
	if(Motor_target_rpm != rpm)	//MHH:21/11/2025.  MHH:24/05/2023.
	{
		PRINTF("Setting RPM to: %d, from: %d\r\n",rpm,ifrom);
		int aout_val;
		if(Speed_raw_mode)
		{
			aout_val = (rpm * 100)/RS_RPM_PER_AOUT_UNIT_2DEC;
			RS_systick_start = Systick_tick;
		}
		else
		{
			aout_val = Get_aout_val_for_rpm(rpm);
			// Note: If not using raw mode, and we have opamp, should multiply by 2/3 to compensate
		}
		Motor_target_rpm = rpm;
		if(aout_val > 1023) aout_val = 1023;
		Set_AOUT(aout_val,3);
//		Motor_systick = 0;		// Start speed control

#ifdef MH_XXX
		if(Debug_motor > 0)
		{
			PRINTF("\r\nSetting RPM to: %d, from: %d\r\n",rpm,ifrom);
		}
#endif
	}
	Enable_Motor();
#ifdef MH_XXX		// MHH:1/4/2023. Try allowing speed to be zero. May be good for braking.Setting rpm to:
	if(rpm == 0)
	{
		Disable_Motor(2000);
	}
	else
	{
		Enable_Motor();
	}
#endif
}
char Motor_command;
//#define MOTOR_STANDARD_RPM	8000		// Try. 02/12/2022
//#define MOTOR_STANDARD_RPM	5000		// MHH:14/01/2025
//#define MOTOR_STANDARD_RPM	6870		// MHH:23/04/2025. As per Xoar request. Nominal RPM, as defined by Maxon for 24V motor, code=539487
//#define MOTOR_HI_RPM		8000		// MHH:14/01/2025

int Get_speed_pct(char arg)
{
	if(arg == 0) return 100;	// Normal case
	if(arg >= '0' && arg <= ('9'+1))
	{
		return (arg - '0')*10;
	}
	uint8_t a = (uint8_t)arg;
	if(a >=128)
	{
		a &= 127;
		if(a >= 10 && a <= 100)
		{
			return a;
		}
	}
	return 0;	// Case of invalid arg
}


void Motor_set_speed(void)
{
	int speed_pct,rpm;
	speed_pct = Get_speed_pct(Command_speed);
	rpm = (MOTOR_STANDARD_RPM * speed_pct)/100;
	Set_Motor_RPM(rpm,100);
}

int Get_command_speed_pct(void)	// MHH:03/02/2025
{
	int speed_pct = Get_speed_pct(Command_speed);	// Value between 1 and 10
	return speed_pct;
}
motor_Mode Motor_mode;
int Enc_target_position;
bool Enc_positioning;
char Enc_last_arg1;
//int abs(int ival);
uint8_t Cal_flags;
int16_t Cal_pos;
uint8_t Cal_fine_wait_count;
uint8_t Cal_coarse_wait_count;
CAL_state Run_calibrate_state;
//#define CAL_SPEED	1000
#ifdef MH_XXX
#define CAL_SPEED	4000
bool Process_calibration_command(CAL_state new_command)
{
	if(new_command == Run_calibrate_state) return true;	// Repeat? Note will need timeout on commands.
	Run_calibrate_state = new_command;
	switch(Run_calibrate_state)
	{
	default:		// Should never get here
		PRINTF("???\r\n");
		return false;

	case CAL_COARSE_HARDSTOP:
		Motor_mode = CALIBRATE_COARSE;
		Set_Direction_Coarse();
		Set_Motor_RPM2(CAL_SPEED);
		Cal_fine_wait_count = 0;
		return true;

	case CAL_FINE_HARDSTOP:
		if(Cal_fine_wait_count++ < 50)	// This logic to get motor to pause after hitting coarse hard stop so we may be able to get a better position measurement
		{
			if(Cal_fine_wait_count == 1)
			{
				Motor_mode = IDLE;
				Disable_Motor(2051);
				Motor_command = 0;	//
			}
			Run_calibrate_state = CAL_IDLE;
			return true;
		}
		Motor_mode = CALIBRATE_FINE;
		Set_Direction_Fine();
		Set_Motor_RPM2(CAL_SPEED);
		Cal_coarse_wait_count = 0;
		return true;

	case CAL_RETURN_FINE_STOP:
		if(Cal_coarse_wait_count++ < 50)	// This logic to get motor to pause after hitting coarse hard stop so we may be able to get a better position measurement
		{
			if(Cal_coarse_wait_count == 1)
			{
				Motor_mode = IDLE;
				Disable_Motor(2052);
				Motor_command = 0;	//
			}
			Run_calibrate_state = CAL_IDLE;
			return true;
		}
		Motor_mode = CALIBRATE_COARSE;
		Set_Direction_Coarse();	// CCW
		Set_Motor_RPM2(8000);
		Motor_command = 0;	//
		return true;

	case CAL_FINISH:
		Motor_mode = IDLE;
		Disable_Motor(2050);
		Motor_command = 0;	//
		return true;
	}
}
#endif
// We could format and send response here...

manual_Mode Manual_mode;
bool Manual_calibration_complete;
//int Manual_ready_pin=1;
uint16_t Enc_max_current;
uint16_t Motor_wait_count;
int Motor_rpm=4000;
int Motor_fine_hard_stop_pos;
int Motor_fine_ms_stop_pos;
int Motor_play;
int Motor_repeat_count;
int Motor_repeat_val;
int Motor_wait_max = 1;
//int Motor_wait_max_limit = 20;
int Motor_wait_max_limit = 0;
bool Motor_diags;

extern int Enc_calibrate_last_ms_stop;
uint8_t Manual_mode_error;		// MHH:13/09/2023
void Sig100_send_calibration_response(void)
{
	uint8_t flags = 0;
	if(Board_pin_read(FINE_MICROSWITCH_PIN)) flags |= STOP_FINE;
	if(Board_pin_read(COARSE_MICROSWITCH_PIN)) flags |= STOP_COARSE;
	MCT8316_check_reset(400);		// If any nFAULT error then try and clear.


//	flags |= CALIBRATE_POS_BIT;		// So AC200 knows that bytes 3 and 4 are for exact microswitch positions
	if(Board_pin_read(READY_PIN) == 0) flags |= NOT_READY_BIT;

	if(Manual_calibration_complete) flags |= MAN_CALIBRATE_COMPLETE_BIT;

	Spkt[0] = '@';
	Spkt[1] = flags;	// The 2 bytes common to 4 byte pkt and 8 byte pkt
	Spkt[2] = Manual_mode_error;		// MHH:13/09/2023
	Spkt[3] = (uint8_t)Manual_mode;

//	Spkt[2] = ADC_adjusted_voltage;	// Reserved for voltage. This may help with detecting brush wear and other problems
	/*
	 * 		Hub_calibrate_type = SIG100_input_packet[2];
*
 * 			Field		Offset	length	Description
 * 			id			0		1		Identifier = '@'
 * 			flags		1		1		flags. eg FINE_STOP, NOT_READY
 * 			voltage		2		1
 * 			type		3		1
 * 										0 = Enc_Position
 * 										1 = Coarse_hard_stop
 * 										2 = Coarse MS stop
 * 										3 = Fine MS stop (1)
 * 										4 = Fine hard stop
 * 										5 = Fine MS stop (2)
 * 										6 = finished calibrate
 * 										Note: If needed, could just use 4 bits, and other 4 bits for something else
 *			stop_pos	4		2		2 byte unsigned integer
 *			pos			6		2		2 byte unsigned integer
 *			checksum 	8		1
 *

//		Encoder_Pos = (int16_t)(SIG100_input_packet[5] | (SIG100_input_packet[6] << 8));
// Note: Sometimes Encoder position will actually be position of a stop
		Hub_calibrate_last_ms_stop_pos = Get_uint16(SIG100_input_packet+3);
		Encoder_Pos = Get_uint16(SIG100_input_packet+5);

	 */

	// MHH:20/06/2023. Send exact microswitch positions

	int pos = 0;

	switch(Manual_mode)
	{
	default:
		break;
	case MAN_FINE_TO_HARD_STOP:
		if(Enc_calibrate_last_ms_stop)	// MHH:05/07/2023
		{
			if(Enc_Pos.pos > Enc_calibrate_last_ms_stop + 20)
			{
				Enc_calibrate_last_ms_stop = 0;
			}
		}
		pos = Enc_calibrate_last_ms_stop;
		break;
	case MAN_COARSE_TO_FINE_MS:
		pos = Motor_fine_hard_stop_pos;
		break;
	case MAN_FINE_TO_FINE_MS:
	case MAN_FINISH:
		pos = Enc_fine_ms_stop_pos;
		break;

	}

	Put_uint16(Spkt+4,pos);

	pos = Enc_Pos.pos;
	if(pos < 0) pos = 0;
	Put_uint16(Spkt+6,pos);

	Checksum_and_send_pkt(9);
}

void Motor_manual_mode(void);
void Motor_manual2(bool kb_input);
bool bCalibration_finished_msg;
bool Process_calibration_command(CAL_state new_command)
{
	char *from_text;

	Hub_watchIt(WD_MOTOR);
	Run_calibrate_state = new_command;
	switch(Run_calibrate_state)
	{
	default:		// Should never get here
		L2PRINTF("Process_calibration_command:Invalid state:%d\r\n",(int)Run_calibrate_state);
		return false;

	case CAL_START:
		if(Hub_calibrate_type == 0)
		{
			from_text = "serial command";
		}
		else
		{
			from_text = "AC200 special keys";
		}


		L2PRINTF("Calibration start:from %s\r\n",from_text)

		Manual_mode = MAN_START;

//		Set_AOUT2(100);	// Start slow
		Set_AOUT(100,3000);		// Start slow
		Motor_rpm = MOTOR_HI_RPM;
		Motor_repeat_val = 0;
		Motor_repeat_count = 0;
//		Enc_max_current = 1500;
		Motor_wait_max = 20;
		Enc_calibrate_last_ms_stop = 0;
		Motor_keyboard_command = false;
		bCalibration_finished_msg = false;
		return true;

	case CAL_MAIN:
		Motor_manual2(false);
		return true;

	case CAL_FINISH:
		if(Calibrating)
		{
			Motor_manual2(false);		// Wait for position stop changing
		}

		Motor_command = 0;	//
		if(bCalibration_finished_msg == false)
		{
			bCalibration_finished_msg = true;
			L2PRINTF("Calibration finished\r\n");
		}

		return true;

	case CAL_ERROR:
		Motor_command = 0;
		Manual_mode = MAN_IDLE;
		return true;

	}
}
//--------------------------------------------------------------------------------------------------------
bool Motor_fast_coarse(void)
{
	if(Enc_dir != DIR_COARSE) return false;	// Already rotating in FINE direction?

	// If we drop through then motor is rotating in COARSE direction. How fast are we rotating?

	if(Enc_rpm < 1000) return false;		// Not too fast, allow change

	PRINTF("Motor_fast_coarse:Disabling,rpm=%d\r\n",Enc_rpm);

	Disable_Motor(3100);
	Set_AOUT2(200);
#ifdef MH_YYY
	if(Hub_maxon_driver)
	{
		if(Speed_raw_mode)
		{
			Set_AOUT(200,10);		// We don't wait until rpm=0 because this may slow positioning, but start slowly
		}
	}
#endif
	return true;
}
//--------------------------------------------------------------------------------------------------------
bool Motor_fast_fine(void)
{
	if(Enc_dir != DIR_FINE) return false;	// Already rotating in FINE direction?

	// If we drop through then motor is rotating in COARSE direction. How fast are we rotating?

	if(Enc_rpm < 1000) return false;		// Not too fast, allow change

	PRINTF("Motor_fast_fine:Disabling,rpm=%d\r\n",Enc_rpm);

	Disable_Motor(3200);
	Set_AOUT2(200);
#ifdef MH_YYY
	if(Hub_maxon_driver)
	{
		if(Speed_raw_mode)
		{
			Set_AOUT(200,11);		// We don't wait until rpm=0 because this may slow positioning, but start slowly
		}
	}
#endif
	return true;
}
//--------------------------------------------------------------------------------------------------------
//char Motor_speed;
TIMER_t Motor_timer;
int Pos_last_epos;
bool Motor_process_command(char new_command)
{
#ifdef MH_XXX	// MHH:10/07/2023. Always do command, in case READY_PIN = 0
	if(new_command != 'P' && new_command != 'p')
	{
		if(new_command == Motor_command && Command_speed == Motor_speed) return true;	// Repeat? Note will need timeout on commands.
		Enc_positioning = false;
	}
#endif
	Motor_command = new_command;
//	Motor_speed = Command_speed;
	char c1,c2,c3,c4,nc3,nc4;
//	bool timer_on = false;

	if(Motor_command != 'P' && Motor_command != 'p')	// MHH:31/03/2025
	{
		Enc_positioning = false;
	}
	switch(Motor_command)
	{
	case '.':
	case '#':		// MHH:15/10/2025
//		Motor_mode = IDLE;	// Set in disable
		Disable_Motor(2100);
		return true;

	case '-':
		if(Motor_fast_coarse())		// MHH:19/02/2025
		{
			return true;
		}
		Motor_mode = FINE;
		if(Motor_fine_stop_override)	// MHH:01/10/2025. When in beta manual control and overcurrent then command changes from 'R' to '-', so need to disable override.
		{
			Set_fine_stop_override(false,500);
		}
		if(Board_pin_read(FINE_MICROSWITCH_PIN)== false)	// Because it takes 10 ticks for flag to set
		{
#ifdef MH_TIMER
			if(Motor_dir != DIR_FINE)
			{
				timer_on = true;
				Timer_reset(&Motor_timer);
			}
#endif
			Set_Direction_Fine(100);	// CW

			if(Board_pin_read(COARSE_MICROSWITCH_PIN) && (Get_command_speed_pct() == 100))	// Coming out of feather?
			{
				Set_Motor_RPM(MOTOR_HI_RPM,1020);		// MHH:14/01/2025
			}
			else
			{
				Motor_set_speed();
			}

#ifdef MH_TIMER
			if(timer_on)
			{
				int msecs = Timer_read_ms(&Motor_timer);
				PRINTF("F:msecs=%d\r\n",msecs);
			}
#endif
		}
		return true;

	case '+':
		if(Motor_fast_fine())		// MHH:19/02/2025
		{
			return true;
		}

		Motor_mode = COARSE;
		if(Board_pin_read(COARSE_MICROSWITCH_PIN)== false)	// Because it takes 10 ticks for flag to set
//		if(Enc_coarse_limit_reached == false)
		{
#ifdef MH_TIMER
			if(Motor_dir != DIR_COARSE)
			{
				timer_on = true;
				Timer_reset(&Motor_timer);
			}
#endif
			Set_Direction_Coarse(100);	// CCW
//			Enc_max_fine_rpm = MOTOR_STANDARD_RPM;
			if(Board_pin_read(FINE_MICROSWITCH_PIN) && (Get_command_speed_pct() == 100))	// MHH:03/02/2025 Coming out of reverse?
			{
				Set_Motor_RPM(MOTOR_HI_RPM,1030);		// MHH:14/01/2025
			}
			else
			{
				Motor_set_speed();
			}

#ifdef MH_TIMER
			if(timer_on)
			{
				int msecs = Timer_read_ms(&Motor_timer);
				PRINTF("C:msecs=%d\r\n",msecs);
			}
#endif
		}
		return true;
	case 'F':
		if(Motor_fast_fine())		// MHH:19/02/2025
		{
			return true;
		}
		Motor_mode = FEATHER;
		if(Enc_feather_limit_reached == false)
		{
			Set_Direction_Coarse(200);	// CCW
//			Enc_max_fine_rpm = MOTOR_STANDARD_RPM;
//			Motor_set_speed();
			Set_Motor_RPM(MOTOR_HI_RPM,1010);		// MHH:14/01/2025
		}
		return true;

	case 'P':
	case 'p':
		// Format is 'P', low order pos, high order pos, not low order pos, not high order pos
		if(Enc_positioning == false)	// First time?
		{
			Enc_positioning = true;
			Pos_last_epos = Enc_Pos.pos;
		}

		c1 = Command_string[1];
		c2 = Command_string[2];
		c3 = Command_string[3];
		c4 = Command_string[4];

		nc3 = ~c3;
		nc4 = ~c4;

		if(c1 != nc3) return false;
		if(c2 != nc4) return false;

		int target_position = Get_uint16(Command_string+1);
		if(target_position != Enc_target_position)
		{
			Enc_target_position = target_position;
			//			PRINTF("PT:%d,Pos=%d\r\n",target_position,Enc_Pos.pos);
		}
		int enc_diff = Enc_target_position - Enc_Pos.pos;
		//		int enc_movement = Enc_Pos.pos - Pos_last_epos;
		Pos_last_epos = Enc_Pos.pos;
		//		PRINTF(":D=%d,P=%d,em=%d,T=%d\r\n",Motor_dir,Pos_last_epos,enc_movement,Motor_target_rpm);

#ifdef MH_POS_DIAG		// MHH:12/01/2025
		if(Motor_dir == DIR_FINE)
		{
			if(enc_movement < 0)
			{
				PRINTF("?DIR_FINE and em=%d\r\n",enc_movement);
			}
		}
		else
		{
			if(Motor_dir == DIR_COARSE)
			{
				if(enc_movement > 0)
				{
					PRINTF("?DIR_COARSE and em=%d\r\n",enc_movement);
				}

			}
		}
#endif

		int abs_diff = abs(enc_diff);

		int position_tolerance = Enc_data.cal_pole_pairs;	// MHH:24/01/2024
		position_tolerance=POSITION_TOLERANCE;		// MHH:19/02/2025?
		if(abs_diff <= position_tolerance)	// May need to make sure does not go passed target by keeping a count?
			//		if(abs_diff < Enc_data.cal_tolerance)	// May need to make sure does not go passed target by keeping a count?
		{
			if(Motor_enabled)
			{
				//				PRINTF("\r\nDisableMotor\r\n");
				Disable_Motor(2200);
			}
			return true;		// Close enough, but may want to take into account pole pairs.
		}

#ifdef MH_YYY
		Command_speed = 0;	// Use this to control speed


		abs_diff = abs(enc_diff);	// MHH:22/07/2023

		//		int speed = (abs_diff / Enc_data.cal_pole_pairs) >> 1;
		int speed = ((abs_diff / Enc_data.cal_pole_pairs) * 35)/ 100;	// MHH:26/11/2025
		if(speed == 0) speed = 1;
		if(speed >= 10)
		{
			Command_speed = 0;
		}
		else
		{
			Command_speed = speed + '0';		// Convert to ascii
		}

		if(Command_speed != Enc_last_arg1)
		{
			Enc_last_arg1 = Command_speed;		// clumsy, but get working then tidy...
			Motor_set_speed();
		}
#endif

		int pos_rpm = (abs_diff * 400)/ Enc_data.cal_pole_pairs;	// MHH:07/06/2026
		if(pos_rpm < 100) pos_rpm = 100;
		if(pos_rpm > 6500) pos_rpm = 6500;
//		Set_Motor_RPM(pos_rpm,1234);

		if(Motor_command == 'P')
		{
			if(enc_diff > 0)	// Same logic as FINE. May need to slow speed if getting close to target position
			{
				if(Motor_fast_coarse())		// MHH:19/02/2025
				{
					return true;
				}
				Motor_mode = FINE;
				Set_Direction_Fine(200);	// CW
				if(Board_pin_read(FINE_MICROSWITCH_PIN)== false)	// Because it takes 5 ticks for flag to set
				{
					Set_Motor_RPM(pos_rpm,1234);
//					Motor_set_speed();		// This enables too.
				}
				else
				{
					Disable_Motor(2210);
				}
			}
			else
			{
				if(Motor_fast_fine())		// MHH:19/02/2025
				{
					return true;
				}

				Motor_mode = COARSE;
				Set_Direction_Coarse(300);	// CCW
				if(Board_pin_read(COARSE_MICROSWITCH_PIN)== false)	// Because it takes 5 ticks for flag to set
				{
					Set_Motor_RPM(pos_rpm,1234);
//					Motor_set_speed();
				}
				else
				{
					Disable_Motor(2215);

				}
			}
			//			PRINTF("MM=%d,Sp=%d\r\n",Motor_mode,Command_speed);
			return true;
		}
		//		PRINTF("D=%d,",enc_diff);
		if(enc_diff > 0)	// Same logic as FINE. May need to slow speed if getting close to target position
		{
			if(Motor_fast_coarse())		// MHH:19/02/2025
			{
				return true;
			}
			Motor_mode = REVERSE;
			Set_Direction_Fine(300);	// CCW
			if(Enc_reverse_limit_reached == false)
			{
				Set_Motor_RPM(pos_rpm,1234);
//				Motor_set_speed();
			}
			else
			{
				Disable_Motor(2220);

			}
		}
		else
		{	// Do not allow positioning in feather zone for now
#ifdef MH_POS_FEATHER
			if(Motor_mode != FEATHER)
			{
				Motor_mode = FEATHER;
				if(Enc_feather_limit_reached == false)
				{
					Set_Direction_Coarse();	// CCW
					Enc_max_fine_rpm = MOTOR_STANDARD_RPM;
					Motor_set_speed();
				}
				return true;
#endif
				if(Motor_fast_fine())		// MHH:19/02/2025
				{
					return true;
				}
//				Motor_mode = COARSE;
				Motor_mode = FEATHER;		// MHH:19/05/2026
				Set_Direction_Coarse(400);	// CCW
#ifdef MH_XXX	// MHH:19/05/2026
				if(Board_pin_read(COARSE_MICROSWITCH_PIN)== false)	// Because it takes 5 ticks for flag to set
				{
					Motor_set_speed();
				}
				else
				{
					Disable_Motor(2225);
				}
#else
				Set_Motor_RPM(pos_rpm,1234);

//				Motor_set_speed();
#endif
			}
			return true;

	case 'R':
		if(Motor_fast_coarse())		// MHH:19/02/2025
		{
			return true;
		}
		Motor_mode = REVERSE;
		if(Enc_reverse_limit_reached == false)
		{
			Set_Direction_Fine(400);	// CCW
//			Enc_max_fine_rpm = MOTOR_STANDARD_RPM;
			Set_Motor_RPM(MOTOR_HI_RPM,1040);		// MHH:05/06/2025
//			Motor_set_speed();
		}
		return true;

	case 'V':
		return true;

	default:
		return false;	// Invalid command
	}
}
//------------------------------------------------------------------------------------------------------
// Called from Enc_ISR routine
#ifdef MH_AAA
void Motor_check_pos(int incr)
{
	if(Motor_dir == DIR_FINE)	// Fine direction?
	{
		if(Enc_fine_microswitch_on == false)
		{
			if(Enc_fine_microswitch_pos_set)	// And fine position set
			{
				if(Enc_pos < 20)
				{
					if(Enc_max_fine_rpm > 1000)
					{
						Enc_max_fine_rpm = 1000;
						Set_Motor_RPM(1000);
					}
				}
			}
		}
	}
}
#endif
//------------------------------------------------------------------------------------------------------
extern volatile bool Display_rpm;
bool Motor_main_mode;
bool Speed_raw_mode;
bool Speed_opamp;
extern int Enc_rpm_count;
void Hub_calibrate(void);
void Hub_calibrate2(void);
void Hub_calibrate2a(void);
void Hub_calibrate3(void);

bool Verify_Enc_pos(int ifrom);

//extern uint16_t AC200_current;

void Hub_update_enc_data(void)	// Only called if calibrate requested by AC200 special keys
{
	L2PRINTF("Hub_update_enc_data:\r\n");
	Enc_data.update_count++;		// Force AC200 to reload Enc_data
	Enc_data.cal_coarse_stop = (uint16_t)Enc_coarse_ms_stop_pos;
	Enc_data.cal_fine_stop = (uint16_t)Motor_fine_ms_stop_pos;
	Enc_data.cal_fine_stop2 = (uint16_t)Enc_fine_ms_stop_pos;
	Enc_data.cal_fine_hard_stop = (uint16_t)Motor_fine_hard_stop_pos;
	Enc_data.cal_pos = (uint16_t)Enc_Pos.pos;

	uint8_t *p_src = (uint8_t *)&Enc_data;
	uint32_t check_crc32 = Wiki_CRC32(p_src,60);
	Enc_data.crc32 = check_crc32;
	uint8_t *p_dst = (uint8_t *)&Hub_data;
	memcpy(p_dst,p_src,64);

	int result = Sig100_update_hub_data();	// Write to flash memory and spi memory
	if(result)
	{
		L2PRINTF("Sig100_update_hub_data fail, result=%d",result);
		return;
	}

	Enc_data.run_flags &= ~ENC_RUN_FLAGS_NO_FEATHER;	// Turn no feather flag off
	Enc_ssp_update_enc_run_flags();
	Manual_calibration_complete = true;
	L2PRINTF("Calibration completed successfully\r\n");


#ifdef MH_XXX
	PRINTF("Motor COARSE MS stop   = %d\r\n",Enc_coarse_ms_stop_pos);
	PRINTF("Motor FINE MS stop (1) = %d\r\n",Motor_fine_ms_stop_pos);
	PRINTF("Motor FINE MS stop (2) = %d\r\n",Enc_fine_ms_stop_pos);
	Motor_play =  Enc_fine_ms_stop_pos - Motor_fine_ms_stop_pos;
	PRINTF("Motor play             = %d\r\n",Motor_play);
	PRINTF("Motor FINE hard stop   = %d\r\n",Motor_fine_hard_stop_pos);
#endif
}

#define MANUAL_MODE_ERROR_NO_FINE_SWITCH		1
#define MANUAL_MODE_ERROR_FINE_SWITCH_TOO_CLOSE 2
#define MANUAL_MODE_ERROR_NO_COARSE_SWITCH		3
TIMER_t Interval_timer;
//Current:%d,pos:%d
void Motor_manual_mode(void)
{
	static int Save_enc_pos;
	static int Enc_pos_count;
	static int Motor_wait_count;
//	static int Motor_slow_count;
//	int ready_pin;
//	if(MH_trace)
	{
//		PRINTF("\r\n:Manual_mode:%d,",(int)Manual_mode);
	}
	switch(Manual_mode)
	{
	case MAN_IDLE:
		return;

	case MAN_START:
		if(Motor_repeat_val > 0)
		{
			Motor_repeat_count++;
			Dputs("\r\n\r\n================================\r\n");
			PRINTF("Repeat_count:%d,wait_max:%d\r\n",Motor_repeat_count,Motor_wait_max);
		}
		Dputs("Starting calibrate\r\n");
//		Dputs2("Setting direction COARSE\r\n");
		Set_Direction_Coarse(500);
		Motor_rpm = MOTOR_HI_RPM;
		PRINTF("Setting rpm to:%d\r\n",Motor_rpm);
		Set_Motor_RPM(Motor_rpm,331);
		Set_fine_stop_override(true,600);
		Manual_mode = MAN_COARSE_TO_HARD_STOP;
		Manual_calibration_complete = false;
		Enc_fine_ms_stop_pos = 0;		// MHH:12/09/2023.
		Enc_coarse_ms_stop_pos = 0;
		Manual_mode_error = 0;
//		Motor_slow_count = 0;
		return;


	case MAN_COARSE_TO_HARD_STOP:
#ifdef MH_YYY
		if(Motor_slow_count++ == 25)
		{
			Motor_rpm = 8000;			// Start slowly
			PRINTF("Setting rpm to:%d\r\n",Motor_rpm);
			Set_Motor_RPM(Motor_rpm,333);
		}
#endif
		if(Motor_enabled == false)
		{
			PRINTF("Reached COARSE hard stop, moving to FINE hard stop, Enc_pos =%d\r\n",Enc_Pos.pos);
			if((Motor_verify == false) && (Motor_repeat_count == 0))	// Only set if this is AC200 Calibrate
//			if(Motor_repeat_count == 0)	// Only set if this is AC200 Calibrate
			{
				Enc_Pos.pos = 0;	// Should have stopped moving because of motor wait
				Enc_pos_save2();		// So checks and backup are changed too
				Enc_data.run_flags &= ~ENC_RUN_FLAGS_NO_FEATHER;		// MHH:08/09/2023. Turn off no feather flag
				Dputs("Setting Enc_pos to ZERO\r\n");
			}
			// Could have special motor pos, but gets complicated...
//			Enc_pos = 0;	// Should have stopped moving because of motor wait
//			Enc_posx = 0;
//			Enc_posx_set = false;
			Dputs("Setting direction FINE\r\n");
			Set_Direction_Fine(500);
			PRINTF("Setting rpm to:%d\r\n",Motor_rpm);
			Motor_rpm = MOTOR_HI_RPM;
			Set_Motor_RPM(Motor_rpm,332);
			Set_fine_stop_override(true,700);

			Manual_mode = MAN_FINE_TO_HARD_STOP;
//			Motor_slow_count = 0;
		}
		break;

	case MAN_FINE_TO_HARD_STOP:
#ifdef MH_YYY
		if(Motor_slow_count++ == 25)
		{
			Motor_rpm = 8000;			// Start slowly
			PRINTF("Setting rpm to:%d\r\n",Motor_rpm);
			Set_Motor_RPM(Motor_rpm,333);
		}
#endif
		if(Motor_enabled == false)
		{
			Motor_fine_hard_stop_pos = Enc_Pos.pos;	// Should have stopped moving because of motor wait
			PRINTF("Reached FINE hard stop at pos:%d\r\n",Motor_fine_hard_stop_pos);

			if(Enc_fine_ms_stop_pos == 0)
			{
				L2PRINTF("Motor_manual_mode:No FINE MS stop, stopping calibrate\r\n");
				Manual_mode_error = MANUAL_MODE_ERROR_NO_FINE_SWITCH;
			}
			int tolerance = (Enc_data.cal_pole_pairs *50);
			if(Enc_fine_ms_stop_pos + tolerance > Motor_fine_hard_stop_pos)
			{

				L2PRINTF("Motor_manual_mode:FINE MS stop too close to FINE hard stop, stopping calibrate\r\n");
				Manual_mode_error = MANUAL_MODE_ERROR_FINE_SWITCH_TOO_CLOSE;
			}
			if(Enc_coarse_ms_stop_pos == 0)
			{
				L2PRINTF("Motor_manual_mode:No COARSE MS stop, stopping calibrate\r\n");
				Manual_mode_error = MANUAL_MODE_ERROR_NO_COARSE_SWITCH;
			}
			if(Manual_mode_error != 0)
			{
				Manual_mode = MAN_ERROR;
				Enc_pos_count = 0;
				return;
			}


			// Could have special motor pos, but gets complicated...
			Dputs("Setting direction COARSE\r\n");
			Set_Direction_Coarse(600);
			Motor_rpm = MOTOR_HI_RPM;
			PRINTF("Setting rpm to:%d\r\n",Motor_rpm);
			Set_Motor_RPM(Motor_rpm,333);
//			Set_fine_stop_override(true);	// MHH:02/09/2025. Should not be needed as FINE MS ON and direction COARSE (See enable).
			Manual_mode = MAN_COARSE_TO_FINE_MS;
			Motor_fine_ms_stop_pos = Enc_fine_ms_stop_pos;
		}
		break;

	case MAN_COARSE_TO_FINE_MS:
#ifdef MH_YYY
		ready_pin = Board_pin_read(READY_PIN);
		if(Motor_enabled)
		{
			if(ready_pin == 0)
			{
				Dputs("M2:RDY = 0,Disabling motor\r\n");
				Board_pin_write(ENABLE_PIN,true);		// From Hub7 board, must be on to disable
				Board_pin_write(ENABLE_PIN2,true);		// From Hub7 board, must be on to disable
				Motor_enabled = false;
				return;
			}
		}
		else
		{
			if(ready_pin == 0)
			{
				return;
			}
			else
			{
				Set_Motor_RPM(Motor_rpm,340);		// Should enable motor;
			}
		}
#endif
		if(Enc_fine_microswitch_on == false)
		{
			Dputs("Reached FINE MS stop\r\n");
//			Dputs2("Setting direction FINE\r\n");

/*
 * MHH:13/02/2025
 */

			Dputs("Stopping motor\r\n");
			PRINTF("RPM:%d,P:%d\r\n",Enc_rpm,Enc_Pos.pos);
			Timer_start(&Interval_timer);
			Disable_Motor(1236);
			Set_AOUT(0,2);		// MHH:14/06/2026
#ifdef MH_YYY
			if(Speed_raw_mode)
			{
				Set_AOUT(0,2);			// MHH:18/02/2025. So we get a softstart in the other direction.
			}
#endif
//			Set_Motor_RPM(0,334);	// MHH:18/02/2025. This enables motor, which we don't want. Set DAC to 0 instead
			Motor_wait_count = 0;


//			Set_Direction_Fine();

			Manual_mode = MAN_FINE_TO_FINE_MS;
			Enc_fine_ms_stop_pos = 0;
		}
		break;

	case MAN_FINE_TO_FINE_MS:
//		Motor_wait_count++;
		PRINTF("MWC:%d,PWM:%d,Pos:%d\r\n",Motor_wait_count,PWM_duty_10bit,Enc_Pos.pos);
		if(Motor_wait_count++ == 0)
		{
			uint32_t millisecs = Timer_read_ms(&Interval_timer);
			Timer_reset(&Interval_timer);
			PRINTF("R(%d):%d,P:%d,M:%d\r\n",Motor_wait_count,Enc_rpm,Enc_Pos.pos,millisecs);	// MHH:18/02/2025:Debug
			if(Enc_rpm > 0)
			{
				L2PRINTF("??MAN_FINE_TO_FINE_MS:RPM > 0\r\n",Enc_rpm);
			}
		}
#ifdef MH_XXX		// MHH:18/02/2025. Already pause for Motor_wait_max ticks if motor disabled and current
		if(Motor_wait_count <20)
		{
			uint32_t millisecs = Timer_read_ms(&Interval_timer);
			Timer_reset(&Interval_timer);
			PRINTF("R(%d):%d,P:%d,M:%d\r\n",Motor_wait_count,Enc_rpm,Enc_Pos.pos,millisecs);	// MHH:18/02/2025:Debug
			break;
		}

		if(Motor_wait_count == 20)
		{
			Set_Direction_Fine();		// MHH:18/02/2025. Set direction fine before setting RPM which also enables motor.
			Set_Motor_RPM(1000,334);
		}
#else
		Set_Direction_Fine(600);		// MHH:18/02/2025. Set direction fine before setting RPM which also enables motor.
		Set_Motor_RPM(1000,334);
#endif

//		if(Enc_fine_microswitch_on)
		if(Board_pin_read(FINE_MICROSWITCH_PIN))	// MHH:15/06/2026
		{
			Disable_Motor(1235);
			Dputs("Reached FINE MS stop\r\n");
			PRINTF("Motor COARSE MS stop   = %d\r\n",Enc_coarse_ms_stop_pos);
			PRINTF("Motor FINE MS stop (1) = %d\r\n",Motor_fine_ms_stop_pos);
			PRINTF("Motor FINE MS stop (2) = %d\r\n",Enc_fine_ms_stop_pos);
			Motor_play =  Enc_fine_ms_stop_pos - Motor_fine_ms_stop_pos;
			PRINTF("Motor play             = %d\r\n",Motor_play);
			PRINTF("Motor FINE hard stop   = %d\r\n",Motor_fine_hard_stop_pos);
			if(Enc_data.run_flags & ENC_RUN_FLAGS_NO_FEATHER)
			{
				Dputs("NO_FEATHER_FLAG ON!!\r\n");
			}
			else
			{
				Dputs("NO_FEATHER_FLAG off\r\n");
			}

//			Dputs("Update Enc_pos in memory\r\n");
//			ac200Enc_update_pos(300);
//			int pos = SSP_read_pos();
//			PRINTF("pos from SSP_read_pos = %d\r\n",pos);
			Manual_mode = MAN_FINISH;
			Save_enc_pos = Enc_Pos.pos;
			Enc_pos_count = 0;
		}
		break;
	case MAN_FINISH:
		if(Enc_Pos.pos == Save_enc_pos)
		{
			if(Enc_pos_count++ >= 10)
			{
				Dputs("Finished\r\n");
				if(Verify_Enc_pos(200))
				{
					if(Motor_repeat_val > 0)
					{
						if(Motor_repeat_count < Motor_repeat_val)
						{
							Manual_mode = MAN_START;
							return;
						}
					}
					if(Hub_calibrate_type == 1)	// MHH:28/08/2023. Comment out for now, but good to be able to update locally, just not when called from HubCalibrator
					{
						Hub_update_enc_data();
						//						Verify_Enc_pos(300);
					}
				}
				Manual_mode = MAN_IDLE;
				Calibrating = false;
			}
		}
		else
		{
			Save_enc_pos = Enc_Pos.pos;
			Enc_pos_count = 0;
		}
		break;

	case MAN_ERROR:
		if(Enc_pos_count++ == 0)
		{
			L2PRINTF("MAN_ERROR:Setting NO_FEATHER_FLAG on\r\n");
			SSP_set_position_error(150);
			return;
		}

		PRINTF("Enc_pos_count:%d\r\n",Enc_pos_count);
		if(Enc_pos_count > 10)
		{
			PRINTF("MAN_IDLE\r\n");
			Manual_mode = MAN_IDLE;
			Calibrating = false;
		}

#ifdef MH_XXX
		if(Enc_pos_count >= 10)	// Use count to make sure that pkt is sent at least 10 times
		{
			Manual_mode = MAN_IDLE;
			Calibrating = false;
			Dputs("MAN_ERROR:Finished\r\n");
		}
#endif
		break;
	}
}

int Enc_cal_last_pos;
bool Motor_verify;


bool Verify_Enc_pos(int ifrom)
{
	int pos = Enc_Pos.pos;
	SSP_get_encoder_pos();
	if(pos != Enc_Pos.pos)
	{
		L2PRINTF("Verify_Enc_pos:ERROR:Enc_pos = d%,pos = %d,from:%d\r\n,",Enc_Pos.pos,pos,ifrom);
		return false;
	}
	PRINTF("Verify_Enc_pos:%d OK\r\n",Enc_Pos.pos);
	return true;
}
#ifdef MH_FLASH_RECORD
void Hub_flashlog_menu(void)	// Here if we have an '_' prefix.
{
	if(Flog_diags_enabled == false) return;
	int c = Board_UARTGetChar_wait_RB(500);	// Allow half second in case Teraterm input
	switch(c)
	{
	case 'S':
		Hub_flog_send_binary();
		break;

	case 'L':
		Hub_flog_list_mram_index();
		break;

	case 'V':
		Hub_flog_verify();
		break;

	case 'Z':
		Hub_flog_reset();
		break;
	}
}
#endif
//#define MOTOR_WAIT_COUNT
bool Kb_control_commands_allowed;
void Hub_log_display();
extern int Motor_enabled_count;
#ifdef MH_XXX
int MH_trace;
void MH_trace_return(int ival)
{

	if(MH_trace)
	{
		PRINTF("[%d]\r\n",ival);
	}
}
#endif

void Motor_manual2(bool kb_input)
{
//	int rpm;
//	static int ecount=0;
//	return;		// MHH:13/06/2026
	static int kb_last;
	int ready_pin;
	static int motor_pos2;

	Motor_verify = false;
	if(kb_input)
	{
		Motor_verify = true;
	}

//	Motor_main_mode = true;
	if(Motor_repeat_val > 0)
	{
		Motor_diags = false;
	}
	else
	{
//		Motor_diags = true;
	}
	if(Enc_data.cal_pole_pairs == 0)
	{
		Enc_data.cal_pole_pairs = 4;
	}
	MCT8316_check_reset(500);		// If any nFAULT error then try and clear.

	ready_pin = Board_pin_read(READY_PIN);
	if(ready_pin == 0)
	{
		Dputs("M1:RDY = 0,Disabling motor\r\n");
		Board_pin_write(ENABLE_PIN,true);		// From Hub7 board, must be on to disable
		Board_pin_write(ENABLE_PIN2,true);		// From Hub7 board, must be on to disable
		Motor_enabled = false;
		Set_AOUT2(100);
#ifdef MH_YYY
		if(Speed_raw_mode)		// MHH:20/11/2025
		{
			Set_AOUT(100,4000);		// Start slow
		}
#endif
		return;
	}
	if(AC200_current > Enc_max_current)
	{
		if(Motor_enabled)
		{
			Disable_Motor(5678);
			PRINTF("Current > %d,disabling\r\n",Enc_max_current);
//			MH_trace = 1;
		}
		Set_AOUT2(100);
#ifdef MH_YYY
		if(Speed_raw_mode)		// MHH:20/11/2025
		{
			Set_AOUT(100,5000);		// Start slow
		}
#endif
		return;
	}

	//#ifdef MH_DIAGS
	bool b_check_motor = false;
#ifdef MH_FLASH_RECORD
//	if(Flog_diags_enabled)
	{
		b_check_motor = true;
	}
//	else
#else
	{
		if(Board_pin_read(HEATER_PIN) == false && Board_pin_read(HEATER2_PIN) == true)	// Heater off?
		{
			b_check_motor = true;
		}
	}
#endif
	//#else
	//#endif

#ifdef MH_YYY		// MHH:18/02/2025. Replaced with logic below which waits for RPM to be zero before waiting. Maybe the wait not necessary if RPM=0?
	if(b_check_motor)
	{
		if(Motor_enabled == false && AC200_current > 100)
		{
			if(Motor_diags)
			{
				PRINTF("Current:%d,pos:%d\r\n",AC200_current,Enc_Pos.pos);
#ifdef MH_XXX
				int enable_pin = Board_pin_read(ENABLE_PIN);
				int enable_pin2 = Board_pin_read(ENABLE_PIN2);
				int motor_enabled = 0;
				if(Motor_enabled) motor_enabled = 1;
				PRINTF("ME=%d,EP=%d,EP2=%d\r\n",motor_enabled,enable_pin,enable_pin2);
#endif
			}
			Motor_wait_count = Motor_wait_max;
			Enc_cal_last_pos = Enc_Pos.pos;
			return;
		}
	}
#endif
	if(b_check_motor)
	{
		if(Motor_enabled == false)
		{
			if((Enc_rpm > 0) || (AC200_current > 100))
			{
//				if(Motor_diags)
				{
					PRINTF("Current:%d,pos:%d,rpm:%d\r\n",AC200_current,Enc_Pos.pos,Enc_rpm);
				}
//				Motor_wait_count = Motor_wait_max;
				Motor_wait_count = 1;	// MHH:18/02/2025. Is there any need to wait once RPM = 0?
				Enc_cal_last_pos = Enc_Pos.pos;
//				MH_trace_return(200);
				return;
			}
		}
	}

	if(Motor_wait_count > 0)
	{
		Motor_wait_count--;
//		MH_trace_return(300);
		return;
	}
	if(kb_input == false)	// When calibrating from AC200
	{
		Motor_manual_mode();
		return;
	}

// If we drop through then we have a type 'A' command which is only allowed when Brushless Diags parameter = 123

	if((Systick_tick & 127) == 0)
		//		if((Enc_rpm_count & 127) == 0)
	{
		if(Enc_Pos.pos != motor_pos2 && Manual_mode == MAN_IDLE)
		{
			motor_pos2 = Enc_Pos.pos;
//				PRINTF("Enc_pos:%d,Enc_rpm:%d,DAC0_v:%d\r\n",Enc_pos,Enc_rpm,DAC0_v);
			PRINTF("Enc_pos:%d,Enc_rpm:%d,Curr:%d\r\n",Enc_Pos.pos,Enc_rpm,AC200_current);
		}
	}

	Motor_manual_mode();		// MHH:08/09/2023

	int c = Board_UARTGetChar_RB();
	if(c < 0)
	{
//		Motor_manual_mode();
		return;
	}
	Dputc(c);
	if(c >= 'a' && c <= 'z') c -= ('a' - 'A');	// toupper

	if(Calibrating)
	{
		if(c != 'S') return;	// Only allow Stop when Verifying, as occasionally getting spurious serial data
	}
	if(c == '}' && kb_last == '{')	// MHH:03/08/2023. Noticed that UL hub somehow was moving at '0' speed after reprogramming and recalibrating.
	{								// This logic to only allow control commands if we get a {} keyboard input sequence.
		Kb_control_commands_allowed = true;
		Dputs("Kb control commands allowed\r\n");
	}
	kb_last = c;

	switch(c)		// First handle commands that will not cause a problem if accidentally generated.
	{
	case '[':		// Allow handshake with windows software (MH_Vibrator_console)
		Dputc(']');
		break;
	case '?':
		if(Kb_control_commands_allowed == false)
		{
			Dputs("\r\n");
			Dputs("{} Enable keyboard control commands\r\n");
		}
		break;
	}

	if(Kb_control_commands_allowed == false)	// MHH:03/08/2023
	{
		return;
	}

	if(c >= '0' && c <= '9')
	{
		Motor_rpm = (c - '0') * 1000;
		PRINTF("Setting rpm to:%d\r\n",Motor_rpm);
		Set_Motor_RPM(Motor_rpm,2222);
		Set_fine_stop_override(true,800);
		return;
	}

	switch(c)
	{
	case '?':
		Dputs("\r\n");
		Dputs("?  List of commands\r\n");
		Dputs("{} Enable keyboard control commands\r\n");
		Dputs("*  Display Hub data\r\n");
		Dputs("+  Increase maximum current by 100 mA\r\n");
		Dputs("-  Decrease maximum current by 100 mA\r\n");
		Dputs("#  Reboot hub\r\n");
		Dputs("0-9 Set RPM to digit x 1000\r\n");
		Dputs("C  Set direction COARSE\r\n");
		Dputs("D  Disable motor\r\n");
		Dputs("E  Enable motor\r\n");
		Dputs("F  Set direction FINE\r\n");
		Dputs("G  Generate hub error\r\n");
		Dputs("H  Toggle Heater on/off\r\n");
		Dputs("I  Initialise hub page zero\r\n");
		Dputs("L  Log display\r\n");
		Dputs("R  Set Repeat count for Verify logic\r\n");
		Dputs("S  STOP\r\n");
		Dputs("T  Toggle NO_FEATHER run flag\r\n");
		Dputs("V  Verify position logic\r\n");
		Dputs("X  Reset Log Display file");
		break;

	case '*':
		/*
int Enc_ecount;
int Enc_last_err;
int Enc_ready_count;
		 */
		Dputs("\r\n");
		PRINTF("Enc_data.run_flags      : %d\r\n",Enc_data.run_flags);
		PRINTF("Enc_data.error_count    : %d\r\n",Enc_data.error_count);
		PRINTF("Enc_data.error_type     : %d (%c)\r\n",Enc_data.error_type,Enc_data.error_type);
		PRINTF("Enc_data.error_command  : %d (%c)\r\n",Enc_data.error_command,Enc_data.error_command);
		PRINTF("Enc_data.error_number   : %d\r\n",Enc_data.error_number);
		PRINTF("Enc_data.error_ac200_run: %d\r\n",Enc_data.error_ac200_run);
		PRINTF("Enc_data.error_data     : %d\r\n",Enc_data.error_data);
//		PRINTF("Enc_ready_count: %d\r\n",Enc_ready_count);
		Dputs("\r\n");
//		PRINTF("Enc_posx:        %d\r\n",Enc_posx);
		PRINTF("Enc_data.cal_coarse_stop : %d\r\n",Enc_data.cal_coarse_stop);
		PRINTF("Enc_data.cal_fine_stop(1): %d\r\n",Enc_data.cal_fine_stop);
		PRINTF("Enc_data.cal_fine_stop(2): %d\r\n",Enc_data.cal_fine_stop2);
		if(Enc_data.user_defined_stops & 4)
		{
			PRINTF("Enc_data.ud_feather_stop : %d\r\n",Enc_data.ud_feather_stop);
		}
		if(Enc_data.user_defined_stops & 8)
		{
			PRINTF("Enc_data.ud_reverse_stop : %d\r\n",Enc_data.ud_reverse_stop);
		}
		Dputs("\r\n");
		PRINTF("Enc_pos      : %d\r\n",Enc_Pos.pos);
		PRINTF("AC200 current: %d\r\n",AC200_current);
		MCT8316_check_reset(600);		// If any nFAULT error then try and clear.
		ready_pin = Board_pin_read(READY_PIN);
		PRINTF("Enc_ready_pin: %d",ready_pin);
		if(ready_pin == 0)
		{
			Dputs(" - NOT READY");
		}
		Dputs("\r\n");
		if(Enc_fine_microswitch_on)
		{
			Dputs("Enc_fine_microswitch ON\r\n");
		}
		if(Enc_coarse_microswitch_on)
		{
			Dputs("Enc_coarse_microswitch ON\r\n");
		}
		break;

	case 'L':
		PRINTF("Log display\r\n")
		Hub_plog_display(0);
		break;

	case '+':
		Enc_max_current+=100;
		PRINTF("Enc_max_current:%d\r\n",Enc_max_current);
		break;

	case '-':
		Enc_max_current-=100;
		if(Enc_max_current < 100) Enc_max_current = 100;
		PRINTF("Enc_max_current:%d\r\n",Enc_max_current);
		break;

	case '#':
		Dputs("Rebooting...\r\n");
		wait_ms(200);
		LPC845_reboot();
		break;

	case 'C':
		Dputs("Setting direction COARSE\r\n");
		Set_Direction_Coarse(700);
		break;

	case 'D':
		Dputs("Disabling motor\r\n");
		Board_pin_write(ENABLE_PIN,true);		// From Hub7 board, must be on to disable
		Board_pin_write(ENABLE_PIN2,true);		// From Hub7 board, must be on to disable
		Motor_enabled = false;
		break;

	case 'E':
		Dputs("Enabling motor\r\n");
		Board_pin_write(ENABLE_PIN,false);		// From Hub7 board, must be on to disable
		Board_pin_write(ENABLE_PIN2,false);		// From Hub7 board, must be on to disable
		Motor_enabled = true;
		break;

	case 'F':
		Dputs("Setting direction FINE\r\n");
		Set_Direction_Fine(700);
		break;

	case 'G':
		Dputs("Generating hub error\r\n");
		Sig100_format_error_pkt('E',HUB_ERR_POS_FINESTOP,1234);
		break;

	case 'H':
//#ifndef MH_DIAGS
#ifndef MH_FLASH_RECORD
//		if(Flog_diags_enabled == false)
		{
			Dputs("\r\nHeater:");
			if(Board_pin_read(HEATER_PIN))
			{
				Board_turn_heater_off();
			}
			else
			{
				Board_turn_heater_on();
			}
		}
#endif
		break;

	case 'I':
		Dputs("Initialise page zero\r\n");
		SSP_initialise_page_zero();
		break;

#ifdef MH_XXX
	case 'M':
		Motor_rpm += 1000;
		if(Motor_rpm > 8000) Motor_rpm = 1000;
		PRINTF("Setting motor calibration rpm value to:%d\r\n",Motor_rpm);
		break;


	case 'P':
		Dputs("Need to recode Test_epos\r\n");
//		Test_epos();
		break;
#endif


	case 'R':
		switch(Motor_repeat_val)
		{
		case 0:
			Motor_repeat_val = 5;
			break;
		case 5:
			Motor_repeat_val = 10;
			break;
		case 10:
			Motor_repeat_val = 100;
			break;
		case 100:
			Motor_repeat_val = 1000;
			break;
		default:
			Motor_repeat_val = 0;
			break;
		}
		PRINTF("Setting motor repeat value to:%d\r\n",Motor_repeat_val);
		break;

	case 'S':
		Dputs("Stop\r\n");
			//		Motor_rpm = 0;
		Disable_Motor(1234);
		Manual_mode = MAN_IDLE;		// Just in case...
		Motor_repeat_val = 0;	// Just in case...
			//  		Set_Motor_RPM(rpm,230);
		break;

	case 'T':
		Dputs("Toggle NO_FEATHER run flag\r\n");
		if(Enc_data.run_flags & ENC_RUN_FLAGS_NO_FEATHER)
		{
			Dputs("Turning NO_FEATHER flag OFF\r\n");
			Enc_data.run_flags &= ~ENC_RUN_FLAGS_NO_FEATHER;	// Clear NO_FEATHER bit
		}
		else
		{
			Dputs("Turning NO_FEATHER flag ON\r\n");
			Enc_data.run_flags |= ENC_RUN_FLAGS_NO_FEATHER;
		}
		break;

#ifdef MH_XXX
	case 'U':
		Dputs("Update Enc_pos in memory\r\n");
		Put_encoder_pos(400);
//		Put_encoder_pos(1,410);

//		ac200Enc_update_pos(400);
		break;
#endif

	case 'V':	// Was Go calibrate, now Verify
		//		Dputs("Find play\r\n");
		if(Verify_Enc_pos(100))
		{
			Manual_mode = MAN_START;
			Motor_repeat_count = 0;
			Motor_keyboard_command = true;
			Motor_rpm = MOTOR_HI_RPM;
			Motor_wait_max = 20;
			Calibrating = true;
		}
		break;

	case 'X':
		PRINTF("Reset Log display\r\n")
		Hub_plog_display(123);
		break;

	case 'Z':
		L2PRINTF("MAN Z:Setting NO_FEATHER_FLAG on\r\n");
		SSP_set_position_error(200);
		break;

//#ifdef MH_DIAGS
#ifdef MH_FLASH_RECORD
	case '_':		// Special vibrator prefix
		Hub_flashlog_menu();
		break;
#endif
//#endif

#ifdef MH_XXX
	case 'W':
		Motor_wait_max++;
		if(Motor_wait_max > 20) Motor_wait_max = 1;
		PRINTF("Setting motor wait count to:%d\r\n",Motor_wait_max);
		break;
#endif
	}

}
#ifdef MH_XXX
void Motor_manual(void)
{
	Timer_reset(&Motor_timer);
	for(;;)
	{
		int msecs = Timer_read_ms(&Motor_timer);
		if(msecs <= 5)
		{
			Motor_manual2();
		}
	}
}
#endif
bool Flash_recording;
void Sig100_ini(void);
void SIG100_rb_init(void);
extern uint32_t Ctimer_count;
extern uint32_t Ctimer_usecs_elapsed;
int Motor_main(void)
{

	int c;
	int msecs;
	static int kb_last;

	PRINTF("Motor_main\r\n");
#ifdef MH_TEST_SIG100_RB

	Sig100_ini();


#ifdef MH_XXX
	Sig100_UARTPutSTR("Hello from SIG100 UART, enter some data to echo...\r\n");
	for(;;)
	{
		c = Sig100_getc_nowait();
		if(c != -1)
		{
			Sig100_UARTPutChar(c);
		}
	}
#endif



	SIG100_rb_init();
	SIG100_RB_puts("Hello from SIG100_RB_puts, enter some data to echo...\r\n");
	for(;;)
	{
		c = Sig100_RB_getc_nowait();
		if(c != -1)
		{
			SIG100_RB_UARTPutchar(c);
		}
	}
#endif
	//    Enc_Init();		// Already done

	//	SysTick_Config(SystemCoreClock / 100);	// 100 Hz

	Motor_main_mode = true;
	if(Enc_data.cal_pole_pairs == 0)
	{
		Enc_data.cal_pole_pairs = 4;
	}

	int rpm = 0;
	uint32_t motor_systick;
	int motor_change;
	int motor_pos=0;
	int motor_pos2=0;
	//    Set_Motor_RPM(rpm,200);
	//    Set_Direction_Fine();

	//    Enable_Motor();


//	Timer_start(&Motor_timer);
	bool timer_set = false;
	bool bval=false;
	int start_pos;
//	int motor_rpm_count = 0;
	int epos;
//	int ctimer_count = 0;
//	uint32_t overrun_count = Flog_data.overrun;
//	uint32_t save_flog_putc_count = 0;
	while(true)
	{
		//#ifdef MH_DIAGS

#ifdef MH_XXX	// MHH:08/05/2026
		if(Flog_save_data_flag)
		{
#ifdef MH_ADXL375
			Adxl375_read_xyz_if_ready();
#endif
			for(int i=0;i<10;i++)
			{
				if(Hub_flog_check_ring_buffer() < 256) break;
//				save_flog_putc_count = Flog_putc_count;
				if(Flog_data.overrun > overrun_count + 10)
				{
					overrun_count = Flog_data.overrun;
					Dputc('?');
				}
			}
//			while(Hub_flog_check_ring_buffer() >= 128);


			//			continue;
		}
		else
#endif
		{
#ifdef MH_XXX
			if(Ctimer_usecs_elapsed)
			{
				uint32_t usecs = Ctimer_usecs_elapsed;
				Ctimer_usecs_elapsed = 0;
				PRINTF("Ctimer_usecs_elapsed = %d\r\n",usecs);

			}
			if(Ctimer_count - ctimer_count >= 100000)
			{
				ctimer_count = Ctimer_count;
				PRINTF("Ct:%d\r\n",ctimer_count)
			}
#endif
			if((Systick_tick & 255) == 0)
				//		if((Enc_rpm_count & 127) == 0)
			{
				if(Enc_Pos.pos != motor_pos2)
				{
					motor_pos2 = Enc_Pos.pos;
					//				PRINTF("Enc_pos:%d,Enc_rpm:%d,DAC0_v:%d\r\n",Enc_pos,Enc_rpm,DAC0_v);
					PRINTF("Enc_pos:%d,Enc_rpm:%d,DAC0_v:%d\r\n",Enc_Pos.pos,Enc_rpm,DAC0_v);
				}
			}
		}
		if((Systick_tick & 15) == 0)
		{
			MCT8316_check_reset(700);		// If any nFAULT error then try and clear.
			if(Board_pin_read(READY_PIN) == 0)
			{
				//PRINTF("RDY=0\r\n");
			}
		}

		//#endif
		c = Board_UARTGetChar_RB();
		if(c < 0) continue;

		if(c == '}' && kb_last == '{')	// MHH:03/08/2023. Noticed that UL hub somehow was moving at '0' speed after reprogramming and recalibrating.
		{								// This logic to only allow control commands if we get a {} keyboard input sequence.
//			Kb_control_commands_allowed = true;
			Dputs("Kb control commands allowed\r\n");	// Just for compatability when not using Motor_main()
		}
		kb_last = c;
		if(c >= '0' && c <= '9')
		{
			rpm = (c - '0') * 1000;
			PRINTF("Setting rpm to:%d\r\n",rpm);
			Set_Motor_RPM(rpm,2221);
			Set_fine_stop_override(true,900);
			continue;
		}
		if(c >= 'a' && c <= 'z') c -= ('a' - 'A');	// toupper
		switch(c)
		{
#ifdef MH_FLASH_RECORD
		case '_':		// Special vibrator prefix
			Hub_flashlog_menu();
			break;
#endif
		case '#':
			PRINTF("Resetting nSLEEP pin\r\n");
			Pin_Off(NSLEEP_PIN);		// MHH:10/06/2026. Reverse if not using NPN
			wait_ms(100);
			Pin_On(NSLEEP_PIN);
			//			Hub_calibrate2();
			break;

		case '*':
			/*
int Enc_ecount;
int Enc_last_err;
int Enc_ready_count;
			 */
			PRINTF("\r\n");
			PRINTF("Enc_pos:         %d\r\n",Enc_Pos.pos);
#ifdef MH_XXX
			PRINTF("Enc_ecount:      %d\r\n",Enc_ecount);
			PRINTF("Enc_last_err:    %d\r\n",Enc_last_err);
			PRINTF("Enc_ready_count: %d\r\n",Enc_ready_count);
			//			PRINTF("Enc_posx:        %d\r\n",Enc_posx);
			PRINTF("Enc_pos:         %d\r\n",Enc_pos);
			PRINTF("Enc_coarse_ms_stop_pos: %d\r\n",Enc_coarse_ms_stop_pos);
			PRINTF("Enc_fine_ms_stop_pos:   %d\r\n",Enc_fine_ms_stop_pos);
			int fine_to_coarse = Enc_coarse_ms_stop_pos - Enc_fine_ms_stop_pos;
			PRINTF("fine_to_coarse:         %d\r\n",fine_to_coarse)
#endif
#ifdef MH_XXX
			if(Enc_pos != Enc_posx/6)
			{
				PRINTF("Enc_posx/6 != Enc_posx\r\n");
			}
#endif
			if(Enc_fine_microswitch_on)
			{
				PRINTF("Enc_fine_microswitch ON\r\n");
			}
			if(Enc_coarse_microswitch_on)
			{
				PRINTF("Enc_coarse_microswitch ON\r\n");
			}
			MCT8316_check_reset(800);		// If any nFAULT error then try and clear.

			int ready_pin = Board_pin_read(READY_PIN);
			PRINTF("Enc_ready_pin: %d\r\n",ready_pin);
			if(ready_pin == 0)
			{
				PRINTF(" - NOT READY\r\n");
			}
			break;

		case 32:
			rpm += 1000;
			if(rpm > 8000) rpm = 1000;
			PRINTF("Setting rpm to:%d\r\n",rpm);
			Set_Motor_RPM(rpm,220);
			break;

#ifdef MH_XXX
		case 'B':
			PRINTF("Brake\r\n");
			timer_set = true;
			Timer_reset(&Motor_timer);
			start_pos = Enc_Pos.pos;
			Set_Motor_RPM(0,230);
			if(Motor_dir == DIR_FINE)
			{
				Set_Direction_Coarse(800);
			}
			else
			{
				Set_Direction_Fine(800);
			}
			break;
#endif

		case 'C':
			PRINTF("Setting direction COARSE\r\n");
			Set_Direction_Coarse(900);
			break;

		case 'D':
			PRINTF("Disabling motor\r\n");
			Board_pin_write(ENABLE_PIN,true);		// From Hub7 board, must be on to disable
			Board_pin_write(ENABLE_PIN2,true);		// From Hub7 board, must be on to disable
			Motor_enabled = false;
			break;

		case 'E':
			PRINTF("Enabling motor\r\n");
			Board_pin_write(ENABLE_PIN,false);		// From Hub7 board, must be on to disable
			Board_pin_write(ENABLE_PIN2,false);		// From Hub7 board, must be on to disable
			Motor_enabled = true;
			break;

		case 'F':
			PRINTF("Setting direction FINE\r\n");
			Set_Direction_Fine(900);
			break;

		case 'O':
			PRINTF("Toggling override pin\r\n");
			if(Motor_fine_stop_override)
			{
				bval=false;
				PRINTF("Turning override off\r\n");
			}
			else
			{
				bval = true;
				PRINTF("Turning override on\r\n")
			}
			Set_fine_stop_override(bval,1234);
			break;

		case 'P':
			PRINTF("Setting Enc_pos to 1234\r\n");
			Enc_Pos.pos = 1234;
			//			ac200Enc_update_pos(200);
			epos = SSP_read_pos();
			PRINTF("epos = %d\r\n",epos);
			break;
		case 'R':
			MCT8316_driver_reset(400);
#ifdef MH_XXX
			Board_pin_write(NSLEEP_PIN,false);
			wait_ms(20);
			Board_pin_write(NSLEEP_PIN,true);
#endif
			PRINTF("nSLEEP reset\r\n");
			break;
		case 'S':
			PRINTF("Stop\r\n");
			rpm = 0;
#ifdef MH_SET_TIMER
			timer_set = true;
			Timer_reset(&Motor_timer);
			start_pos = Enc_pos;
#endif
			Disable_Motor(1234);
			//  		Set_Motor_RPM(rpm,230);
			break;

		case 'T':		// for test
			PRINTF("Test\r\n");
			Set_Direction_Coarse(1000);
			//			timer_set = true;
			Timer_reset(&Motor_timer);
			start_pos = Enc_Pos.pos;
			Set_Motor_RPM(MOTOR_HI_RPM,230);
			while(Enc_Pos.pos <= start_pos+1);
			msecs = Timer_read_ms(&Motor_timer);
			//					PRINTF("usecs:%d\r\n",usecs);

			Disable_Motor(5678);

			PRINTF("msecs:%d\r\n",msecs);
			break;

//		case 'U':
//			PRINTF("Update Enc_pos in memory\r\n");
			//			ac200Enc_update_pos(500);
			break;

		}
#ifdef MH_FLASH_RECORD
//		if(Flog_diags_enabled)
		{
			switch(c)
			{
			case '[':		// Used for handshake with MH_vibrator_console
				Dputc(']');
				break;

			case 'W':
				Hub_flog_display_data();
				break;

			case 'X':
				if(Flash_recording == false)
				{
					Flash_recording = true;
					PRINTF("Starting Flash record\r\n");
					Hub_flog_start_record();
				}
				else
				{
					Flash_recording = false;
					PRINTF("Ending Flash record\r\n");
					Hub_flog_end_record();
				}
				break;
			case 'Y':
				Hub_flog_display();
				break;

			case 'Z':
				Hub_flog_reset();
				break;
					//#endif


			case '_':		// Special vibrator prefix

				c = Board_UARTGetChar_wait_RB(1000);
				switch(c)
				{
				case 'S':
					Hub_flog_send_binary();
					break;

				case 'L':
					Hub_flog_list_mram_index();
					break;

				case 'V':
					Hub_flog_verify();
					break;
				}
				break;
			}
		}
#endif
	}
	if(timer_set)
	{
		if(Systick_tick != motor_systick)
		{
			motor_systick = Systick_tick;
			Board_UARTPutchar_RB('.');
			motor_change = Enc_Pos.pos - motor_pos;
			motor_pos = Enc_Pos.pos;
			if(motor_change == 0)
			{
				timer_set = false;
				int pos_change = Enc_Pos.pos - start_pos;
				//					int usecs = Timer_read_us(&Motor_timer);
				int msecs = Timer_read_ms(&Motor_timer);
				//					PRINTF("usecs:%d\r\n",usecs);
				PRINTF("msecs:%d,pos_change:%d\r\n",msecs,pos_change);
			}
		}
	}
#ifdef MH_XXX
	if(Enc_rpm != save_rpm)
	{
		save_rpm = Enc_rpm;
		//			int drpm = Enc_rpm / Enc_data.cal_pole_pairs;
		PRINTF("RPM:%d\r\n",Enc_rpm);
	}
#endif
}
