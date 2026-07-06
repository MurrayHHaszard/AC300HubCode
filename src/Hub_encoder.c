/*
 * encoder.c
 *
 *  Created on: 15/12/2017
 *      Author: Murray
 */
#include <stdio.h>
#include <stdlib.h>

#include "syscon.h"

#include "ac300Enc_ssp.h"
#include "board.h"
#include "AC300_hub.h"

//=============================================================================================================
// This logic from mbed mh_emg49.cpp

TIMER_t Enc_timer;

//----------------------------------------------------------
// 8000
//int Encoder_Pos;	// Here only for SIG60 routines
int Enc_dir=DIR_STOPPED;
//int Enc_prev=-1;
//int Enc_this;
//int Enc_start;
//int Enc_usecs; ?FR31


Enc_pos_td Enc_Pos;	// MHH:08/09/2023

//int Dac_per_rpmd2;
int Enc_rpm;
uint8_t Enc_rpm_cnt;
//int Enc_rpm_change;
int Enc_calibrate_rpm;
bool Enc_calibrate_limit;
//int Enc_cnt;
int Enc_errors;
int Enc_ix=-1;
int Enc_hall_count;
int Enc_dir_pos=0;
//int Enc_pos;
//int Enc_rpm_count;
//int Enc_rpm_count2;
bool Enc_ignore;
bool Enc_H1_init=true;


#define ENC_HALL_1		1
#define ENC_HALL_2		2
#define ENC_HALL_3		3

//------------------------------------------------------------------------
bool Enc_feather_limit_reached;
bool Enc_reverse_limit_reached;


bool Enc_fine_microswitch_pos_set;
//bool Enc_coarse_microswitch_pos_set;
//int Enc_max_fine_rpm=10000;	// High value = 10,000

int Enc_coarse_ms_pos;
int Enc_fine_ms_pos_rise;
//int Enc_fine_ms_pos_fall;
bool Enc_fine_rise_error;
bool Enc_coarse_rise_error;
int Enc_coarse_ms_pos_rise;

int Enc_reset_count;
//int Enc_ISR_fine_count=0;
//uint8_t	Enc_fine_rise_stop_ticks;
//uint8_t	Enc_fine_fall_stop_ticks;
//bool Enc_systick_check_fine_rise;
//bool Enc_systick_check_fine_fall;

//bool Enc_update_coarse_stop;
//bool Enc_update_fine_rise_stop;
//bool Enc_update_fine_fall_stop;

//bool Enc_systick_check_coarse_stop;
//uint8_t Enc_coarse_stop_ticks;
//int Enc_ISR_coarse_count;

void Enc_ISR_fine_microswitch(void)
{
}
//----------------------------------------------------------------------------------------------------------
void Enc_ISR_coarse_microswitch(void)
{
}
//------------------------------------------------------------------------
#define ENC_ERROR_BAD_READ		101
#define ENC_ERROR_FINE_MS		102
#define ENC_ERROR_FINE_MS2		103
#define ENC_ERROR_FINE_MS3		104
void Enc_H1_error(int ival)
{
	PRINTF("\r\nEnc_H1_error:%d\r\n",ival);
}

int Enc_h123;
int Enc2_h123;

/*
 *  Sequence is: F100,R101,F001,R011,F010,R110,...
 */
#define F100	4
#define R101	5
#define F001	1
#define R011	3
#define F010	2
#define R110	6

#define ENC_H_A		F100
#define ENC_H_B		F001
#define ENC_H_C		F010


extern bool Motor_main_mode;
int Enc_count;
//int Motor_systick;
//int Enc_rpm_prev;

bool Enc_fine_microswitch_on;


int Enc_calibrate_last_ms_stop;
//int Enc_calibrate_flags;
//int Enc_raw_speed_count = 0;
bool Enc_new_rpm = false;
#ifdef MH_RESTORE_OLD_LOGIC
void Enc_raw_speed_control(void)
{
//	static uint8_t i_count;
//	static int8_t i_val;

	// Either on a new rev, or every 10th of a sec, whichever comes first.
	//			if((Enc_rpm_count != motor_rpm_count) || (Systick_tick != motor_systick))
	//	if((Enc_rpm_count != motor_rpm_count) || (Systick_tick - motor_systick > 10))
//	Motor_systick = Systick_tick;

#ifdef MH_XXX	// MHH:19/11/2025
	if(Enc_data.cal_pole_pairs == 4)
	{
		if(Enc_rpm_count2 == 0)
		{
			Enc_rpm = 0;
		}
	}
	else
	{
		if(Enc_rpm_count == 0)	// This is incremented for every revolution of motor
		{
			Enc_rpm = 0;
		}
	}
#endif
	Enc_rpm_count = 0;		// MHH:18/02/2025. Reset or it doesn't work
	int rval = 0;
	if(Motor_target_rpm == 0 || Motor_enabled == false)
	{
		Enc_raw_speed_count = 0;
//		Enc_rpm_prev = 0;
		rval=1;
	}
//	Enc_raw_speed_count++;
	if(Enc_rpm == 0)
	{
//		Enc_rpm_prev = 0;
		Enc_rpm_count2 = 0;
		if(Enc_raw_speed_count++ < 1) rval=2;	//MHH:18/02/2025. Think that if rpm=0 then first time ignore changing DAC
	}
	else
	{
		if(Enc_new_rpm == false)
		{
			rval=3;
		}
	}
//	if(rval != 2) PRINTF("R%d",rval);
	if(rval)
	{
		return;
	}
	Enc_new_rpm = false;
//	int p_diff = Motor_target_rpm - Enc_rpm;
//	int change_aout = p_diff / 64;	// ??
//	int p_val = p_diff / P_FACTOR;

#ifdef MH_YYY

	int d_diff = Enc_rpm - Enc_rpm_prev;
	Enc_rpm_prev = Enc_rpm;

	d_diff = 0;

	int next_rpm = Enc_rpm + d_diff;
	int p_diff = Motor_target_rpm - next_rpm;

#endif
//	int change_aout = p_diff / 64;
//	int change_aout = p_diff / 32;

	int p_diff = Motor_target_rpm - Enc_rpm;

//	int change_aout = p_diff / 16;
//	int change_aout = p_diff / 8;		// MHH:13/01/2025
//	int change_aout = p_diff / 64;		// MHH:19/10/2025
//	int change_aout = p_diff / 32;		// MHH:19/11/2025
	int change_aout = p_diff / 32;		// MHH:20/11/2025

	if(change_aout == 0)
	{
		if(Motor_target_rpm > Enc_rpm) change_aout++;
		if(Motor_target_rpm < Enc_rpm) change_aout--;

//		if(Motor_target_rpm > Enc_rpm) i_val++;
//		if(Motor_target_rpm < Enc_rpm) i_val--;
//		change_aout = i_val * 8;
#ifdef MH_XXX
		if(i_count++ >= 8)
		{
			if(i_val >= 8) change_aout = 1;
			if(i_val <= -8) change_aout = -1;
			i_count = 0;
			i_val = 0;
		}
#endif
	}
	else
	{
		if(change_aout > 1)	change_aout *= 2;
	}
#ifdef MH_XXX
	else
	{

		if(Enc_rpm == 0)
		{
			change_aout *= 4;
		}
//		i_count = 0;
		i_val = 0;
	}
#endif
	//				int change_aout = diff / 32;	// ??
	int aout_val = DAC0_v;
	int new_aout_val = aout_val + change_aout;
//	PRINTF("T=%d,R=%d,D=%d,S=%d\r\n",Motor_target_rpm,Enc_rpm,DAC0_v,Systick_rpm_count);
//	PRINTF("T=%d,R=%d,D=%d\r\n",Motor_target_rpm,Enc_rpm,DAC0_v);

	//	aout_val += change_aout;
//	if(new_aout_val < 50) new_aout_val = 50;		// MHH:19/11/2025

//	aout_val = 500;
#ifdef MH_YYY
	if(Enc_rpm)
	{
		PRINTF("%d,%d,%d,%d\r\n",Motor_target_rpm,Enc_rpm,aout_val,new_aout_val);
	}
#endif
//#ifdef MH_XXX
	if(Enc_rpm == 0)	// MHH:19/11/2025
	{
//		if(new_aout_val < 100) new_aout_val = 100;
		new_aout_val += 100;
	}
//#endif
	if(new_aout_val > 1023) new_aout_val = 1023;
	if(new_aout_val != DAC0_v)
	{
		Set_AOUT(new_aout_val,1);
	}
}
#endif
extern uint32_t RS_systick_start;
//extern int Dac_per_rpmd2;
void Enc_raw_speed_control(void)
{
	if((Systick_tick - RS_systick_start) < 20)	// allow 2 tenths after change of rpm before changing
	{
		return;
	}
	int rval = 0;
	if(Motor_target_rpm == 0 || Motor_enabled == false)
	{
		rval=1;
	}
#ifdef MH_YYY
	if(Hub_maxon_driver == false)	// MHH:08/05/2026
	{
		rval = 2;
	}
#endif
	if(Enc_new_rpm == false)
	{
		rval=3;
	}

	if(rval)
	{
		return;
	}
	Enc_new_rpm = false;
	int p_diff = Motor_target_rpm - Enc_rpm;
	int change_aout = p_diff / 16;		// MHH:28/03/2026
//	int change_aout = p_diff / 12;		// MHH:16/03/2026
#ifdef M_XXX
	if(Dac_per_rpmd2 > 1000)
	{
		change_aout = (p_diff * Dac_per_rpmd2)/10000;
	}
#endif
//	int change_aout = p_diff / 32;		// MHH:20/11/2025

#ifdef MH_XXX		// MHH:26/11/2025
	if(change_aout == 0)
	{
		if(Motor_target_rpm > (Enc_rpm + 8)) change_aout++;
		if(Motor_target_rpm < (Enc_rpm - 8)) change_aout--;
	}
	else
	{
//		change_aout *= 2;
	}
#endif
	int aout_val = DAC0_v;
	int new_aout_val = aout_val + change_aout;
#ifdef MH_YYY
	if(Enc_rpm)
	{
		PRINTF("%d,%d,%d,%d\r\n",Motor_target_rpm,Enc_rpm,aout_val,new_aout_val);
	}
#endif
	if(new_aout_val > 1023) new_aout_val = 1023;
	if(new_aout_val != DAC0_v)
	{
		Set_AOUT(new_aout_val,1);
	}
}

// If motor not moving, then do raw_speed control. Called from Systick routine
// Note that if (say) DAC0_v > 1000 or (say) more than call then could conclude stalled.
// Another approach is to set DAC0_v to (say) 1000, then if no movement on next call then it is stalled.
// Leave details until we start testing stall logic.

void Enc_raw_speed_check_rpm(void)
{
//	int max_ticks = 10;
//	if(Enc_data.cal_pole_pairs == 4) max_ticks = 2;
//	if(Systick_tick - Motor_systick >= max_ticks)	// More than a tenth of a second since last raw speed control?
	if(Speed_raw_mode == false) return;

	if((Systick_tick & 7) == 0)
//	if((Systick_tick & 3) == 0)	// MHH:15/03/2026
	{
//		Motor_systick = Systick_tick;
		Enc_raw_speed_control();
	}
}

void Enc_adjust_pos_when_low_voltage(void)
{
	L2PRINTF("Enc_adjust pos,rpm=%d\r\n",Enc_rpm);
	if(Enc_rpm > 2000)
	{
		if(Enc_dir == DIR_COARSE)
		{
			Enc_Pos.pos--;
		}
		else
		{
			Enc_Pos.pos++;
		}
		Enc_pos_save();
	}
}

void Enc_get_rpm(int dir)
{

	if(dir != Enc_dir)
	{
		if(dir == DIR_COARSE) Dputs2("#C\r\n");
		if(dir == DIR_FINE) Dputs2("#F\r\n");
		if(dir != Motor_dir)
		{
			Dputs2("?DIR?\r\n");
		}
		Enc_dir = dir;
		Enc_new_rpm = true;
		Enc_rpm = 0;
		//		Enc_rpm_count2 = 0;			// MHH:24/11/2025
		Timer_reset(&Enc_timer);
		Enc_rpm = 0;
		Enc_dir_pos = Enc_Pos.pos;
		return;
	}
	int pos_change = abs(Enc_Pos.pos - Enc_dir_pos);
	if(pos_change < Enc_data.cal_pole_pairs)
	{
		uint32_t enc_usecs = Timer_read_us(&Enc_timer);
		Enc_rpm = 60000000/enc_usecs;
		Enc_rpm = (Enc_rpm * pos_change) / Enc_data.cal_pole_pairs;
		Enc_new_rpm = true;
		return;
	}
	bool full_rev = false;
	switch(Enc_data.cal_pole_pairs)
	{
	case 1:
		full_rev = true;
		break;
	case 2:
		if((Enc_Pos.pos & 1) == 0) full_rev = true;
		break;
	case 4:
		if((Enc_Pos.pos & 3) == 0) full_rev = true;
		break;
		// Could put a default in and abort...

	}

	if(full_rev)
	{
		uint32_t enc_usecs = Timer_read_us(&Enc_timer);
		Timer_reset(&Enc_timer);
		Enc_rpm = 60000000/enc_usecs;
		Enc_new_rpm = true;
#ifdef MH_XXX
		if(Enc_rpm > 300)
		{
			Dac_per_rpmd2 = (DAC0_v * 10000) / Enc_rpm;
			if((Enc_rpm_cnt++ & 7) == 0)		// Show every 8th time
			{
				PRINTF("Dp:%d,t:%d,r:%d,D:%d\r\n",Dac_per_rpmd2,Motor_target_rpm,Enc_rpm,DAC0_v);
			}
		}
#endif
	}

}
//========================================================================================
/*
 *  Sequence is: F100,R101,F001,R011,F010,R110,...
#define F100	4
#define R101	5
#define F001	1
#define R011	3
#define F010	2
#define R110	6

#define ENC_H_A		F100
#define ENC_H_B		F001
#define ENC_H_C		F010
 */

//int Enc_posx;
//int Enc_fine_ms_stop_posx;
//int Enc_coarse_ms_stop_posx;
int Enc_fine_ms_stop_pos;
int Enc_coarse_ms_stop_pos;
bool Enc_fine_microswitch_on;
bool Enc_coarse_microswitch_on;
int Enc_h123x;
int Enc_dirx;
//int Enc_ecount;
//int Enc_last_err;
int Enc_ready_count;
#ifdef MH_YYY	// MHH:19/12/2023
void Enc_check_fine_pos(void)
{
	if(Enc_data.run_flags & ENC_RUN_FLAGS_NO_FEATHER) return;	// Already know
	if(Enc_fine_rise_error) return;

	int tolerance = (Enc_data.cal_pole_pairs << 1);
	if(Enc_Pos.pos > Enc_data.cal_fine_stop2 + tolerance)
	{
		L2PRINTF("!FE:%d\r\n",Enc_Pos.pos);
		Enc_data.run_flags |= ENC_RUN_FLAGS_NO_FEATHER;
		Enc_fine_ms_pos_rise = Enc_Pos.pos;		// Use this for error
		Enc_fine_rise_error = true;			// So we can send error packet to AC200, and update ram memory.
		Enc_data.run_set_from = 250;
	}
}
#endif

bool Enc_check_fine_position_ok(void)
{
	//	if((Enc_Pos.pos < Enc_data.cal_fine_stop - tolerance) || (Enc_Pos.pos > Enc_data.cal_fine_stop2 + tolerance)) return false;
	//	if((Enc_Pos.pos < Enc_data.cal_fine_stop - Enc_data.cal_tolerance) || (Enc_Pos.pos > Enc_data.cal_fine_stop2 + tolerance)) return false;	// MHH:10/01/2024
	// MHH:24/10/2024. Replace above line with following to try and fix bug. Ignore cal_fine_stop2 as this was not set correctly.

//	return false;		// MHH:23/05/2025. Test ONLY!!!

	if(Enc_Pos.pos < (Enc_data.cal_fine_stop - Enc_misc.cal_tolerance_enc_units))
	{
		return false;
	}
//	int tolerance = (Enc_data.cal_pole_pairs << 2);	// MHH:14/12/2023
	if(Enc_Pos.pos > (Enc_data.cal_fine_stop + Enc_misc.cal_tolerance_enc_units))
	{
		return false;
	}
	return true;
}
bool Enc_first_fine_rise=false;
void Enc_report_fine_rise(int ifrom)
{
	if(Enc_first_fine_rise == false)
	{
		Enc_first_fine_rise = true;
//		L2PRINTF("FR(%d):%d\r\n",ifrom,Enc_Pos.pos);		// MHH:28/06/2025. So we can check log to see if position of FINE rise changes
		L3PRINTF("FR(%d):%d\r\n",ifrom,Enc_Pos.pos);		// MHH:05/07/2025. So we can check log to see if position of FINE rise changes and see time in diagnostics
	}
	else
	{
		PRINTF("FR(%d):%d\r\n",ifrom,Enc_Pos.pos);
	}

}
void Enc_check_fine_rise(int ifrom)
{
#ifndef MH_IGNORE_ERROR
	if((Enc_data.run_flags & ENC_RUN_FLAGS_NO_FEATHER) == 0)
	{	// May need to accept greater tolerance.
//		int tolerance = (Enc_data.cal_pole_pairs << 1);
//		int tolerance = (Enc_data.cal_pole_pairs << 2);	// MHH:14/12/2023
//		if((Enc_Pos.pos < Enc_data.cal_fine_stop - tolerance) || (Enc_Pos.pos > Enc_data.cal_fine_stop2 + tolerance))

		if(Enc_check_fine_position_ok() == false)
		{
			L2PRINTF("!FR(%d):%d\r\n",ifrom,Enc_Pos.pos);
			Enc_data.run_flags |= ENC_RUN_FLAGS_NO_FEATHER;
			Enc_fine_ms_pos_rise = Enc_Pos.pos;		// Use this for error
			Enc_fine_rise_error = true;			// So we can send error packet to AC200, and update ram memory.
			Enc_data.run_set_from = 50;
		}
		else
		{
			Enc_report_fine_rise(100+ifrom);
		}

	}
#endif
}
//----------------------------------------------------------------------------------------------------------------------------------
/*
 *  MHH:08/09/2023. The idea is to store Enc_pos in 2 places, along with a check in case it is being overwritten.
 *
 *  If it is overwritten then hopefully can get correct value from backup and record error to help find how it is overwritten.
 */

typedef struct		// MHH:08/09/2023
{
	bool error_to_report;
	uint16_t count;
	uint16_t ifrom;
	uint16_t ier;
	Enc_pos_td Enc_Pos;
	Enc_pos_td Enc_Pos2;
} Enc_pos_err_td;
Enc_pos_err_td Enc_pos_err;
Enc_pos_td Enc_Pos2;

void Enc_pos_report_error2(int ipos,Enc_pos_td *p)
{
	L2PRINTF("Pos%d:Magic1=%d,pos=%d,check=%d,magic2=%d\r\n",ipos,p->magic1,p->pos,p->pos_check,p->magic2);
}

void Enc_pos_report_error(void)	// To be called from a non ISR
{
	if(Enc_pos_err.error_to_report == false) return;

	Enc_pos_err.error_to_report = false;

	L2PRINTF("Enc_pos_report_error:\r\n");
	L2PRINTF("ier=%d,ifrom=%d\r\n",Enc_pos_err.ier,Enc_pos_err.ifrom);
	Enc_pos_report_error2(1,&Enc_pos_err.Enc_Pos);
	Enc_pos_report_error2(2,&Enc_pos_err.Enc_Pos2);

#ifndef MH_IGNORE_ERROR
	SSP_set_position_error(100);
#endif
}

bool Enc_pos_check_error(uint16_t ier,uint16_t ifrom)
{

	if(Enc_data.run_flags & ENC_RUN_FLAGS_NO_FEATHER)	// MHH:08/09/2023. Do not try and recover at this stage...
	{
		return true;
	}
	if(Enc_pos_err.error_to_report)		// One at a time...
	{
		return true;;
	}

#ifndef MH_IGNORE_ERROR

	Enc_data.run_flags |= ENC_RUN_FLAGS_NO_FEATHER;
	Enc_data.run_set_from = 150;

	Enc_pos_err.error_to_report = true;		// So we can report when not in ISR routine
	Enc_pos_err.count++;	// First save details
	Enc_pos_err.ier = ier;
	Enc_pos_err.ifrom = ifrom;
	Enc_pos_err.Enc_Pos = Enc_Pos;
	Enc_pos_err.Enc_Pos2 = Enc_Pos2;
#endif
	// Now see if we can recover...

	bool fixed = false;

	int pos_check = Enc_Pos.pos ^ -1;	// Does pos look OK?
	if(pos_check == Enc_Pos.pos_check)
	{
		fixed = true;
	}
	else
	{
		int pos2_check = Enc_Pos2.pos ^ -1;	// Does pos 2 look OK?
		if(pos2_check == Enc_Pos2.pos_check)
		{
			Enc_Pos.pos = Enc_Pos2.pos;		// yes, then restore pos from pos2
			fixed = true;
		}
	}
	Enc_Pos.magic1 = POS_MAGIC1;
	Enc_Pos.magic2 = POS_MAGIC2;

	if(fixed == false)
	{
		return true;		// flag error
	}


	pos_check = Enc_Pos.pos ^ -1;	// Does pos look OK?
	Enc_Pos.pos_check = pos_check;
	Enc_Pos2 = Enc_Pos;		// Copy structure
	return false;			// recovered from error.
}

bool Enc_pos_check(uint16_t ifrom)
{
	if(Enc_Pos.magic1 != POS_MAGIC1)
	{
		if(Enc_pos_check_error(1,ifrom)) return true;
	}
	if(Enc_Pos.magic2 != POS_MAGIC2)
	{
		if(Enc_pos_check_error(2,ifrom)) return true;
	}
	if(Enc_Pos.pos != Enc_Pos2.pos)
	{
		if(Enc_pos_check_error(3,ifrom)) return true;
	}
	int pos_check = Enc_Pos.pos ^ -1;
	if(pos_check != Enc_Pos.pos_check)
	{
		if(Enc_pos_check_error(4,ifrom)) return true;
	}

	return false;		// No error
}

void Enc_pos_save(void)
{
	int pos_check = Enc_Pos.pos ^ -1;
	Enc_Pos.pos_check = pos_check;
	Enc_Pos2.pos = Enc_Pos.pos;
	Enc_Pos2.pos_check = Enc_Pos.pos_check;
}

void Enc_pos_save2(void)	// Called after loading from ssp
{
	int pos_check = Enc_Pos.pos ^ -1;
	Enc_Pos.pos_check = pos_check;
	Enc_Pos.magic1 = POS_MAGIC1;
	Enc_Pos.magic2 = POS_MAGIC2;

	Enc_Pos2 = Enc_Pos;			// Copy structure.

}


void Enc_increment_pos(void)
{
	Enc_pos_check(100);
//	Systick_new_rpm_count++;	// MHH:19/02/2025
	Enc_Pos.pos++;
	Enc_pos_save();
}
void Enc_decrement_pos(void)
{
	Enc_pos_check(200);
//	Systick_new_rpm_count++;	// MHH:19/02/2025
	Enc_Pos.pos--;
	Enc_pos_save();
}
//----------------------------------------------------------------------------------------------------------------
// MHH:18/12/2023. Fine rise was happening outside valid range when motor running. Assume vibration may have caused MS to temporarily open
// which caused fine rise. This logic to check it rise position is outside valid range then to wait a few ticks and if still on then report the error
// Another way of doing this may be to set the fine MS pin up as an interrupt.

uint8_t Enc_fine_tick_count;
bool Enc_fine_check;
int Enc_fine_check_position;


#ifdef MH_XXX
void Enc_check_fine_rise2(void)		// OK, if we get here then we know that the fine rise was outside the valid range, os we have to report error...
{
	if((Enc_data.run_flags & ENC_RUN_FLAGS_NO_FEATHER) == 0)
	{	// May need to accept greater tolerance.
		L2PRINTF("!FR4:%d\r\n",Enc_fine_check_position);
		Enc_data.run_flags |= ENC_RUN_FLAGS_NO_FEATHER;
		Enc_fine_ms_pos_rise = Enc_fine_check_position;		// Use this for error
		Enc_fine_rise_error = true;			// So we can send error packet to AC200, and update ram memory.
		Enc_data.run_set_from = 30;
	}
}
#endif


void Enc_check_fine_microswitch_on_ini(void)
{

	if(Enc_check_fine_position_ok())	// Are we within tolerance?
	{
		Enc_fine_microswitch_on = true;		// Yes, then assume on
		Enc_report_fine_rise(3);;
//		PRINTF("FR3:%d\r\n",Enc_Pos.pos);
		return;
	}
/*
 *  Here if a pin test indicates that FINE microswitch is on, but it is not in ON position. We want it to be on continuously for at least 2 ticks
 */
	if(Enc_fine_check) return;		// Already checking
	Enc_fine_check_position = Enc_Pos.pos;
	if(Enc_Pos.pos < (Enc_data.cal_fine_stop - Enc_misc.cal_tolerance_enc_units))
	{
		L2PRINTF("?FR31:%d,FS=%d,T=%d\r\n",Enc_fine_check_position,Enc_data.cal_fine_stop,Enc_misc.cal_tolerance_enc_units);
	}

	if(Enc_Pos.pos > (Enc_data.cal_fine_stop + Enc_misc.cal_tolerance_enc_units))
	{
		L2PRINTF("?FR32:%d,FS=%d,T=%d\r\n",Enc_fine_check_position,Enc_data.cal_fine_stop,Enc_misc.cal_tolerance_enc_units);
	}
	L2PRINTF("?FR3:%d\r\n",Enc_fine_check_position);
	Enc_fine_tick_count = 0;
	Enc_fine_check = true;
}
void Enc_check_fine_microswitch_on(void)
{
	if(Enc_fine_check == false) return;	// Not checking
/*
 *  Called from main receive logic, at least once after a receive
 */
	bool fine_ms_on = Board_pin_read(FINE_MICROSWITCH_PIN);
	if(fine_ms_on == false)	// Pin still on?
	{
		Enc_fine_check = false;	// No, probably false alarm. Stop checking
		return;
	}
	if(Enc_check_fine_position_ok())	// MHH:20/05/2025. Are we within tolerance?
	{
		Enc_fine_microswitch_on = true;		// Yes, then assume on
		Disable_Motor(50);		// MHH:19/11/2025
		Enc_fine_check = false;	// Stop checking
		Enc_report_fine_rise(4);
//		PRINTF("FR4:%d\r\n",Enc_Pos.pos);
		return;
	}
	if(++Enc_fine_tick_count >= 5)		// Has it been on for at least 5 checks?
	{
		Enc_fine_check = false;				// No more checking
		Enc_fine_microswitch_on = true;		// OK, it is on.
		Disable_Motor(51);		// MHH:19/11/2025
		// Now to see if the current position is valid for on
		Enc_check_fine_rise(3);
	}
}
uint8_t Enc_coarse_tick_count;
bool Enc_coarse_check;
int Enc_coarse_check_position;

bool Enc_check_coarse_position_ok(void)
{
//	int tolerance = (Enc_data.cal_pole_pairs << 3);	// MHH:22/12/2023. Greater tolerance than fine as we are traveling in different
													// direction to calibrate and we don't have 2 values like fine
//	if((Enc_Pos.pos < Enc_data.cal_coarse_stop - (tolerance+tolerance)) || (Enc_Pos.pos > Enc_data.cal_coarse_stop + tolerance)) return false;
	if((Enc_Pos.pos < Enc_data.cal_coarse_stop - Enc_misc.cal_tolerance_enc_units) || (Enc_Pos.pos > Enc_data.cal_coarse_stop + Enc_misc.cal_tolerance_enc_units)) return false;
		// MHH:10/01/2024
	return true;
}

void Enc_check_coarse_rise(int ifrom)
{
#ifndef MH_IGNORE_ERROR
	if((Enc_data.run_flags & ENC_RUN_FLAGS_NO_FEATHER) == 0)
	{
		if(Enc_check_coarse_position_ok() == false)
		{
			L2PRINTF("!CR(%d):%d\r\n",ifrom,Enc_Pos.pos);
			Enc_data.run_flags |= ENC_RUN_FLAGS_NO_FEATHER;
			Enc_coarse_ms_pos_rise = Enc_Pos.pos;		// Use this for error
			Enc_coarse_rise_error = true;			// So we can send error packet to AC200, and update ram memory.
			Enc_data.run_set_from = 60;
		}
		else
		{
			PRINTF("CR:%d\r\n",Enc_Pos.pos);
		}
	}
#endif
}

void Enc_check_coarse_microswitch_on_ini(void)
{

	if(Enc_check_coarse_position_ok())	// Are we within tolerance?
	{
		Enc_coarse_microswitch_on = true;		// Yes, then assume on
		if(Motor_mode != FEATHER)	// MHH:18/05/2026
		{
			Disable_Motor(60);		// MHH:19/11/2025
		}
		PRINTF("CR3:%d\r\n",Enc_Pos.pos);
		return;
	}
/*
 *  Here if a pin test indicates that FINE microswitch is on, but it is not in ON position. We want it to be on continuously for at least 2 ticks
 */
	if(Enc_coarse_check) return;		// Already checking
	Enc_coarse_check_position = Enc_Pos.pos;
	L2PRINTF("?CR3:%d\r\n",Enc_coarse_check_position);
	Enc_coarse_tick_count = 0;
	Enc_coarse_check = true;
}
void Enc_check_coarse_microswitch_on(void)
{
	if(Enc_coarse_check == false) return;	// Not checking
/*
 *  Called from main receive logic, at least once after a receive
 */
	bool coarse_ms_on = Board_pin_read(COARSE_MICROSWITCH_PIN);
	if(coarse_ms_on == false)	// Pin still on?
	{
		Enc_coarse_check = false;	// No, probably false alarm. Stop checking
		return;
	}
	if(Enc_check_coarse_position_ok())	// MHH:20/05/2025. Are we within tolerance?
	{
		Enc_coarse_microswitch_on = true;		// Yes, then assume on
		Disable_Motor(61);		// MHH:19/11/2025
		Enc_coarse_check = false;				// No more checking
		PRINTF("CR4:%d\r\n",Enc_Pos.pos);
		return;
	}
	if(++Enc_coarse_tick_count >= 5)		// Has it been on for at least 5 checks?
	{
		Enc_coarse_check = false;				// No more checking
		Enc_coarse_microswitch_on = true;		// OK, it is on.
		Disable_Motor(62);		// MHH:19/11/2025

		// Now to see if the current position is valid for on
		Enc_check_coarse_rise(3);
	}
}
#define HALL_READ_MAX	32
uint8_t Hall_read[HALL_READ_MAX];
int Hall_read_ix;
void Enc_H123_ISR2(int pin, bool rising)
{

    if(Enc_ignore) return;

    int pin0 = LPC_GPIO_PORT->PIN0;    // Read the pin register
    int h1 = (pin0 >> (HALL_1_PIN) & 1);
    int h2 = (pin0 >> (HALL_2_PIN) & 1);
    int h3 = (pin0 >> (HALL_3_PIN) & 1);

    int h123 = (h1<<2) | (h2 <<1) | h3;

#ifdef MH_XXX
    if(Enc_posx_set)
    {
        Enc_H123_ISRx(h123,pin,rising);
    }
#endif
    if(h123 == Enc_h123)
    {
//    	PRINTF("?DUP");
    	return;
    }
    Hall_read_ix &= 31;
    Hall_read[Hall_read_ix++] = h123;

#ifdef MH_XXX
    if(Hall_read_ix == HALL_READ_MAX)
    {
        PRINTF("Enc:%03d\r\n",h123);
    }
#endif
    int dir=0;

    Enc_count++;
#ifdef MH_XXX
    if(Enc_count % 128 == 0)
    {
    	PRINTF("Ec:%d,p=%d,r=%d\r\n",Enc_count,Enc_Pos.pos,Enc_rpm);
    }
#endif
//    int posx;
    switch(h123)
    {
    default:
    	return;

    case ENC_H_B:	// (B)
    	if(Enc_h123 == ENC_H_A)		// A->B ?
    	{
    		dir = DIR_FINE;
    		Enc_increment_pos();	// MHH:08/09/2023
//    		Enc_Pos.pos++;		// FINE
    	}
		break;

    case ENC_H_A:	// (A)
    	if(Enc_h123 == ENC_H_B)		// B->A?
    	{
    		dir = DIR_COARSE;
    		Enc_decrement_pos();	// MHH:08/09/2023
//    		Enc_Pos.pos--;		// COARSE
    	}
		break;

    case ENC_H_C:
    	break;

    }
    Enc_h123 = h123;
    if(dir == 0) return;

#ifdef MH_DISPLAY_HALL_PINS
    Board_UARTPutchar(' ');
        Board_UARTPutchar(h1+'0');
        Board_UARTPutchar(h2+'0');
        Board_UARTPutchar(h3+'0');
        return;
#endif

#ifdef MH_YYY

   	bool fine_ms_on = Enc_fine_microswitch_on;
   	bool coarse_ms_on = Enc_coarse_microswitch_on;
   	Enc_fine_microswitch_on = Board_pin_read(FINE_MICROSWITCH_PIN);
   	Enc_coarse_microswitch_on = Board_pin_read(COARSE_MICROSWITCH_PIN);
#else

   	bool fine_ms_on = Board_pin_read(FINE_MICROSWITCH_PIN);
   	bool coarse_ms_on = Board_pin_read(COARSE_MICROSWITCH_PIN);
//   	Enc_fine_microswitch_on = Board_pin_read(FINE_MICROSWITCH_PIN);
//   	Enc_coarse_microswitch_on = Board_pin_read(COARSE_MICROSWITCH_PIN);

#endif

    if(Motor_main_mode)
    {
    	Enc_get_rpm(dir);
    	if(fine_ms_on != Enc_fine_microswitch_on)
    	{
    		Enc_fine_microswitch_on = fine_ms_on;		// MHH:18/12/2023
    		if(dir  == DIR_FINE)
    		{
    			if(Enc_fine_microswitch_on)
    			{
    				Enc_fine_ms_stop_pos = Enc_Pos.pos;	// Only set when going fine, same as calibrator
           			Enc_calibrate_last_ms_stop = Enc_Pos.pos;
           			if(Motor_verify)
           			{
            			Enc_check_fine_rise(1);
           			}
//    				if(Motor_diags) PRINTF("\r\nEx:F=%d\r\n",Enc_Pos.pos);
    			}
    			else
    			{
    				Dputs2("\r\nEx:F off\r\n");
    			}
//        		Enc_fine_ms_stop_pos = Enc_pos;	// Only set when going fine, same as calibrator
    		}
    		else
    		{
    			if(Enc_fine_microswitch_on)
    			{
    				Dputs2("\r\nEx:F on\r\n");
    			}
    			else
    			{
    				Dputs2("\r\nEx:F off\r\n");
    			}
    		}
    	}
    	if(coarse_ms_on != Enc_coarse_microswitch_on)
    	{
       		Enc_coarse_microswitch_on = coarse_ms_on;		// MHH:18/12/2023
    		if(dir  == DIR_FINE)	// Only set when going fine, same as calibrator
       		{
       			if(Enc_coarse_microswitch_on == false)
       			{
           			Enc_coarse_ms_stop_pos = Enc_Pos.pos;
           			Enc_calibrate_last_ms_stop = Enc_Pos.pos;
           			if(Motor_diags) PRINTF("\r\nEx:C=%d\r\n",Enc_Pos.pos);
       			}
       		}
       		else
       		{
        		if(Enc_coarse_microswitch_on)
        		{
        			Dputs2("\r\nEx:C on\r\n");
        		}
        		else
        		{
        			Dputs2("\r\nEx:C off\r\n");
        		}
       		}
    	}
    	return;
    }
    Enc_check_fine_microswitch_on();		// Just in case
    Enc_check_coarse_microswitch_on();		// Just in case

    // This logic to save Enc_pos on a rise or fall, so we know where stops are.
    if(dir == DIR_FINE) // Fine?
    {
       	if(fine_ms_on != Enc_fine_microswitch_on)			// MHH:18/12/2023
       	{
       		if(fine_ms_on)
       		{
       			Enc_check_fine_microswitch_on_ini();		// Only test rise for now...
       		}
       		else
       		{
       			Enc_fine_microswitch_on = fine_ms_on;		// which is false;
       		}
       	}

       	if(Enc_fine_microswitch_on)
    	{
    		if(Motor_fine_stop_override == false)
    		{
    			Disable_Motor(1000);	// To be sure, but will need to handle differently if reversing
    		}
#ifdef MH_YYY    	// MHH:18/12/2023. I think not needed, with alternative check.
    		if(fine_ms_on != Enc_fine_microswitch_on)
    		{
    			Enc_check_fine_rise(2);
    		}
#endif
    	}
#ifdef MH_YYY    // MHH:19/12/2023. Think this check redundant
    	else	// MHH:13/09/2023
    	{
    		if(Motor_fine_stop_override == false)
    		{
    			Enc_check_fine_pos();
    		}
    	}
#endif
    }
    else
    {		// MHH:22/12/2023. Same logic as for fine, as noticed when swapping hub pcbs between differently geared hubs that they would not pick up on
    		// difference so the "no-feather" flag not set.

       	if(coarse_ms_on != Enc_coarse_microswitch_on)			// MHH:22/12/2023
       	{
       		if(coarse_ms_on)
       		{
       			Enc_check_coarse_microswitch_on_ini();		// Only test rise for now...
       		}
       		else
       		{
       			Enc_coarse_microswitch_on = coarse_ms_on;		// which is false;
       		}
       	}
//    	Enc_coarse_microswitch_on = coarse_ms_on;
    }
//    Enc_data.pos = Enc_pos;
//    ac200Enc_SSP_save_pos();	// Just save, do not write. Leave that to Sys_tick.

    Enc_get_rpm(dir);

//    int flags;

    switch(Motor_mode)
    {
    case COARSE:
		if(Enc_coarse_microswitch_on)
		{
    		Disable_Motor(1300);
		}
    	if(Enc_positioning)
    	{
#ifdef MH_XXX	// MHH:26/11/2025
    		int position_tolerance = Enc_data.cal_pole_pairs;	// MHH:24/01/2024
    		if(Enc_Pos.pos < Enc_target_position + position_tolerance)
#endif
    		if(Enc_Pos.pos <= Enc_target_position)
//    		if(Enc_Pos.pos <= Enc_target_position + Enc_data.cal_tolerance)
    		{
				if(Motor_enabled)
				{
					Disable_Motor(1400);
				}
    		}
    	}
    	break;

    case FEATHER:
//    	Enc_feather_limit_reached = (Enc_pos > Enc_data.ud_feather_stop); // Also can be set in Sys_tick
    	if((Enc_data.run_flags & ENC_RUN_FLAGS_NO_FEATHER) == 0)	// MHH:20/05/2025
    	{
    		Enc_feather_limit_reached = (Enc_Pos.pos < Enc_data.ud_feather_stop); // MHH:05/07/2023.
    		if(Enc_feather_limit_reached)
    		{
    			Disable_Motor(1500);
    		}
    	}
    	break;

    case REVERSE:
    	if(Enc_ud_reverse_stop)
    	{
//        	Enc_reverse_limit_reached = (Enc_pos < Enc_data.ud_reverse_stop); // Also can be set in Sys_tick
        	Enc_reverse_limit_reached = (Enc_Pos.pos > Enc_data.ud_reverse_stop); // MHH:05/07/2023
        	if(Enc_reverse_limit_reached)
        	{
        		Disable_Motor(1600);
        	}
    	}
    	break;

    case FINE:
    	if(Enc_positioning)
    	{
#ifdef MH_XXX
    		int position_tolerance = Enc_data.cal_pole_pairs;	// MHH:24/01/2024
    		if(Enc_Pos.pos > Enc_target_position - position_tolerance)
#endif
//    		if(Enc_Pos.pos >= Enc_target_position - Enc_data.cal_tolerance)
       		if(Enc_Pos.pos >= Enc_target_position)
    		{
				if(Motor_enabled)
				{
					Disable_Motor(1700);
				}
    		}
    	}
    	break;
#ifdef MH_XXX
    case CALIBRATE_COARSE:	// Could check that Calibration_command is true
    	Enc_calibrate_last_ms_stop = 0;
    	Enc_calibrate_flags = 0;
    	break;

    case CALIBRATE_FINE:
    	flags = 0;
    	if(fine_microswitch_on) flags |= 1;
    	if(coarse_microswitch_on) flags |= 2;
    	if(Enc_calibrate_flags != flags)
    	{
    		Enc_calibrate_flags = flags;
    		Enc_calibrate_last_ms_stop = Enc_pos;
    	}

    	break;
#endif
    default:
    	break;
    }

//    Motor_check_pos(dir);

	return;

}

void Enc_Init(void)
{
	int h1 = Board_pin_read(HALL_1_PIN);
	int h2 = Board_pin_read(HALL_2_PIN);
	int h3 = Board_pin_read(HALL_3_PIN);

	Enc_h123 = (h1<<2) | (h2 <<1) | h3;
	Enc2_h123 = Enc_h123;	// Just used as a check while testing...

	// Enable clock to pin interrupt modules.
	LPC_SYSCON->SYSAHBCLKCTRL0 |= GPIO_INT;

	// Configure as pin interrupts 0 by writing to the PINTSEL in SYSCON
	LPC_SYSCON->PINTSEL[0] = HALL_1_PIN;
	LPC_SYSCON->PINTSEL[1] = HALL_2_PIN;
	LPC_SYSCON->PINTSEL[2] = HALL_3_PIN;
//	LPC_SYSCON->PINTSEL[3] = READY_PIN;


	// Configure the Pin interrupt mode register (a.k.a ISEL) for edge-sensitive on PINTSEL1,0
	LPC_PIN_INT->ISEL = 0x0;

	// Configure the IENF (pin interrupt enable falling) for falling edges on PINTSEL0,1,2
	//	  LPC_PIN_INT->IENF = 0x1;		// Falling only for HALL_1

//	LPC_PIN_INT->IENR = 0x7;		// Rising for H1,H2,H3
	//	  LPC_PIN_INT->IENR = 0x0;		// No Rising for H1,H2,H3
	LPC_PIN_INT->IENF = 0x7;		// Falling for H1,H2,H3
//	LPC_PIN_INT->IENF = 0xf;		// Falling for H1,H2,H3 and READY_PIN

	// Clear any pending or left-over interrupt flags
	LPC_PIN_INT->IST = 0xFF;

	Enc_fine_microswitch_on = Board_pin_read(FINE_MICROSWITCH_PIN);		// Initialise Enc_fine_microswitch_on

	Enc_coarse_microswitch_on= Board_pin_read(COARSE_MICROSWITCH_PIN);	// MHH:22/12/20223. Was commented out. Initialise Enc_fine_microswitch_on


	// Enable pin interrupts 0 - 2 in the NVIC (see core_cm0plus.h)
	NVIC_EnableIRQ(PININT0_IRQn);		// HALL_1_PIN
	NVIC_EnableIRQ(PININT1_IRQn);		// HALL_2_PIN
	NVIC_EnableIRQ(PININT2_IRQn);		// HALL_3_PIN
//	NVIC_EnableIRQ(PININT3_IRQn);		// READY_PIN


	//	  NVIC_EnableIRQ(PININT1_IRQn);		// FINE_MICROSWITCH_PIN
	//	  NVIC_EnableIRQ(PININT2_IRQn);		// COARSE_MICROSWITCH_PIN
	Enc_H1_init = false;
}
//-----------------------------------------------------------------------------------------------
//int Enc2_pos;

//-----------------------------------------------------------------------------------------------
#define PIN_INT_MASK0		(1<<0)
#define PIN_INT_MASK1		(1<<1)
#define PIN_INT_MASK2		(1<<2)
#define PIN_INT_MASK3		(1<<3)

#define MH_RISING_INT

//#define MH_DIAG_RPM
#ifdef MH_DIAG_RPM
// This routine for Hall effect sensor on test electric motor
// Use Enc_rpm for now
extern uint16_t AC200_rpm;
void Enc_diag_rpm(void)
{
	uint32_t enc_usecs = Timer_read_us(&Enc_timer);
    Timer_reset(&Enc_timer);
	Enc_rpm_count++;
	Enc_rpm = 60000000/enc_usecs;
	AC200_rpm = (uint16_t)Enc_rpm;
}
#endif
void PININT0_IRQHandler(void)
{
#ifdef MH_RISING_INT
#ifdef MH_XXX
	bool rising = false;
	if (LPC_PIN_INT->RISE & PIN_INT_MASK0) {
		rising = true;
		LPC_PIN_INT->RISE = PIN_INT_MASK0;       // Clear the interrupt flag
	}
#endif
	if (LPC_PIN_INT->FALL & PIN_INT_MASK0) {
		LPC_PIN_INT->FALL = PIN_INT_MASK0;       // Clear the interrupt flag
	}

#ifdef MH_DIAG_RPM
	Enc_diag_rpm();
#else
	Enc_H123_ISR2(1,false);
#endif
#else
	if (LPC_PIN_INT->FALL & PIN_INT_MASK0) {
		LPC_PIN_INT->FALL = PIN_INT_MASK0;       // Clear the interrupt flag
		Enc_H123_ISR();
//		Enc_H123_ISR2();
	}
#endif

}
void PININT1_IRQHandler(void)
 {
#ifdef MH_RISING_INT
#ifdef MH_XXX
	bool rising = false;
	if (LPC_PIN_INT->RISE & PIN_INT_MASK1) {
		// Note: These could be cleared by &= ~PIN_INT_MASK1.
		LPC_PIN_INT->RISE = PIN_INT_MASK1;       // Clear the interrupt flag
		rising = true;
	}
#endif
	if (LPC_PIN_INT->FALL & PIN_INT_MASK1) {
		LPC_PIN_INT->FALL = PIN_INT_MASK1;       // Clear the interrupt flag
	}
	Enc_H123_ISR2(2, false);
#else
	if (LPC_PIN_INT->FALL & PIN_INT_MASK1) {
		LPC_PIN_INT->FALL = PIN_INT_MASK1;       // Clear the interrupt flag
		Enc_H123_ISR();
//		Enc_H123_ISR2();
	}
#endif
	//  Enc_ISR_fine_microswitch();
}
void PININT2_IRQHandler(void)
 {
#ifdef MH_RISING_INT
#ifdef MH_XXX
	bool rising = false;
	if (LPC_PIN_INT->RISE & PIN_INT_MASK2) {
		// Note: These could be cleared by &= ~PIN_INT_MASK2.
		LPC_PIN_INT->RISE = PIN_INT_MASK2;       // Clear the interrupt flag
		rising = true;
	}
#endif
	if (LPC_PIN_INT->FALL & PIN_INT_MASK2) {
		LPC_PIN_INT->FALL = PIN_INT_MASK2;       // Clear the interrupt flag
	}
	Enc_H123_ISR2(3, false);
#else
	if (LPC_PIN_INT->FALL & PIN_INT_MASK2) {
		LPC_PIN_INT->FALL = PIN_INT_MASK2;       // Clear the interrupt flag
		Enc_H123_ISR();
//		Enc_H123_ISR2();
	}
#endif

	//  Enc_ISR_coarse_microswitch();
}

void PININT3_IRQHandler(void)	// MHH:22/07/2023 Unused
 {

	if (LPC_PIN_INT->FALL & PIN_INT_MASK3)
	{
		LPC_PIN_INT->FALL = PIN_INT_MASK3;       // Clear the interrupt flag
		Enc_ready_count++;
		PRINTF("ISR3:RDY=0");
//		Enc_H123_ISR();
	}
}
