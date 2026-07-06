/*
 * mhencoder.h
 *
 *  Created on: 12/05/2018
 *      Author: Murray
 */

#ifndef AC300_HUB_H_
#define AC300_HUB_H_

#include <stdio.h>
#include <string.h>


#include "./board.h"
#include "timer.h"

#define POSITION_TOLERANCE	0	// MHH:07/06/2026
#define MH_SIG100_RB
#define MR_TIME_INTERVAL	1000			// MHH:25/06/2025

#define SPI_MRAM_CS			0		// SPI0
#define SPI_FLASH_CS		1		// SPI0
#define SPI_ACCEL_CS		0		// SPI1

#define HUB_SOFTWARE_REVISION		155		// MHH:08/06/2026
/*
 * MHH:25/06/2026. Added void interrupts_init(void) to set ISR priorities.
 */
//#define HUB_SOFTWARE_REVISION		154		// MHH:08/06/2026
/*
 * MHH:08/06/2026. Modify positioning logic so accurate to encoder value, plus can do this with or without load. See pos_rpm in motor.c
 */

//#define HUB_SOFTWARE_REVISION		153		// MHH:19/05/2026
/*
 * MHH:19/05/2026. Modify 'p' positioning logic so that target position can be between feather stop and reverse stop.
 */
//#define HUB_SOFTWARE_REVISION		152		// MHH:23/04/2026
/*
 * MHH:23/04/2026. Change default Enc_data.cal_tolerance = 10; (1 mm)  was 5 (0.5).
 */


//#define HUB_SOFTWARE_REVISION		151		// MHH:12/04/2026
/*
 * MHH:12/04/2026. Allow speed char to be in % range (0 to 100) by setting high bit (128) to flag byte as new format.
 *                 range of '0' to '9'+1
 */
//#define HUB_SOFTWARE_REVISION		150		// MHH:28/03/2026

/*
 *  MHH:28/03/2026. When testing BETA, and moving from COARSE stop to REVERSE stop, as the blade angle was passed the fine pitch stop the motor would
 *                  occasionally slow as if disabled, then restart.
 *                  Fix: In Enable_Motor(void) routine, add local boolean variable "fine_stop_override" and only set global boolean variable
 *                  "Motor_fine_stop_override" once with local value instead of setting off then on under conditions. Problem was that in short time
 *                  it was turned off a Hall effect pin change would trigger an ISR which would disable motor if fine stop on and override off.
 */

//#define HUB_SOFTWARE_REVISION		149		// MHH:10/01/2026

/*
 *  MHH:10/01/2026. Noticed that occasionally a L2PRINTF statement was outputting data that was past the last null. This is because the p_BufferLen
 *                  was being modified inside the Hub_plog_puts_common() function, adding to the length for the call to Board_UART0PutSTR()
 *                  which used p_BufferLen.
 *
 *                  Solution: Get rid of p_BufferLen and use trailing null to define string length. Rename Board_UART0PutSTR() function to
 *                  Board_UART1PutSTR() as it is dealing with UART1, not UART0.
 */

/*
 *  MHH:10/01/2026. Problem: Occasionally calibration would not start or would end prematurely, especially with hub Russell supplied. Multiple causes:
 *                  a) Current may have been non-zero at start, noticed it was hovering between 0 and 25. Fix: in hub, if AC200_current < 50 then set to zero.
 *                  b) When calibrating at stopping at CHS the calibrating stopped. This is because it was waiting for encoder rpm to be zero, but the routine
 *                     inside the SysTick interrupt was not working because if the Systick_save_pos was negative then it ignored setting the rpm. At CHS it is
 *                     valid for the encoder_pos to be negative.
 *                     Fix: Set the initial value for the encoder position to 999999 instead of -1
 */

/*
 * // MHH:07/01/2026. Not clearing SIG100 input buffer can result in getting out of sync with AC300 current when Calibrating and under debug, causing
					  motor to not stop at hard limits. To fix, make sure SIG100 ring buffer cleared after every command received.
					  	Sig100_RB_clear_input();
 */

//#define HUB_SOFTWARE_REVISION		148		// MHH:16/12/2025

/*
 * MHH:16/12/2025 Change format of BL for '+' and '-' so that instead of "+p" or "-m" with optional speed appended,it is now in format
 *                "+sc" or "-sc" where s is an ascii character representing speed and c is the checksum for the preceding 2 bytes.
 *                The reason for this change is that occasionally when the speed byte was corrupted the hub assumed full speed.
 *                Note AC300 software was changed from 10.141 to 10.142. So that Hub software is compatible with earlier versions, the AC300 version
 *                number is now passed on initialisation as part of the 'i' packet. The AC200 already knows the hub software version, so it choses
 *                format to send to hub based on software revision.
*/

//#define HUB_SOFTWARE_REVISION		147		// MHH:18/11/2025
/*
 *  MHH:27/11/2025. Changes to positioning logic to take advantage of improved speed control.
 */
/*
 * MHH:18/11/2025. Changes to allow Maxon Speed Control. Redefine DIG1_DIG2_PIN from P1_2 to P0_21. Default still raw mode.
 * Improvements to raw speed mode to try and match Maxon speed control.
 */

//#define HUB_SOFTWARE_REVISION		146		// MHH:19/10/2025
/*
 * MHH:19/10/2025. Changes to Enc_raw_speed_control(void) so that for pole_count of 4 it checks speed every 40 ms, not every hundreth to make more responsive.
 *                 Also set DAC0_v to 205 if gt 205 when disabling.
 */
//#define HUB_SOFTWARE_REVISION		145		// MHH:01/10/2025

/* MHH:01/10/2025. When in beta manual control and overcurrent then command changes from 'R' to '-', so need to disable override.(motor.c)
 * 	if(Motor_fine_stop_override)
		{
			Set_fine_stop_override(false);
		}
 */

//#define HUB_SOFTWARE_REVISION		144		// MHH:22/09/2025
/*
 * 	  MHH:22/09/2025.
 * 	  Problem: Change calculation of Enc_misc.adc_corrected_min_voltage to include natural difference between Adc_voltage and real voltage.
 * 	  Eg Real voltage of 13 corresponds with Adc_val of 1431, which is about 1.1 times real voltage.
 *
 *
 * 	  voltage_min *= 11;		// MHH:22/09/2025. Because Adc_voltage = 1431 corresponds with real voltage of 13, approx * 1.1
	  voltage_min /= 10;
	  Enc_misc.adc_corrected_min_voltage = voltage_min;
 *
 */

//#define HUB_SOFTWARE_REVISION		143		// MHH:03/09/2025

/*
 * MHH:03/09/2025. Add calibration parameter cal_current_max to allow customising maximum current when calibrating.
 *
 * Also, to avoid starting motor at full speed, use Set_AOUT to initialise DAC to low value so that speed ramps slowly when setting rpm to 8000.
 * This setting is done whenever are hard stop is encountered, and also at the start of calibrating.
 */

//#define HUB_SOFTWARE_REVISION		142		// MHH:14/08/2025

/*
 * MHH:14/08/2025. Enc_rpm not being set to zero, added logic to set to zero in Sig100_send_response(void) but it should not have persisted as the
 * systick logic should at least set to zero every 20 ms.???. Also, need to make sure AC300 sets Enc_rpm to zero if a short pkt.
 * Think this is why Enc_rpm not set to zero. In '*' pkt, add following lines:
 *     			Spkt[4] = (uint8_t)(Enc_rpm >> 5);	// MHH:14/08/2025. Divide by 32
 *     			Spkt[5] = (uint8_t)(DAC0_v >> 2);	// Max val 1023, so divide by 4
 */

//#define HUB_SOFTWARE_REVISION		141		// MHH:12/08/2025

/*
 * MHH:12/08/2025.
 *
 * Added hub diagnostics mode (see Calibrator). If enabled, sends 9 byte pkt with '^' header and enc_rpm and dac0_v included.
 *
 * Diagnostics program sees values and displays.
 *
 */

//#define HUB_SOFTWARE_REVISION		140		// MHH:10/08/2025
/*
 * MHH:10/08/2025. Added CTIMER0 interrupt at 1000 Hz to allow finer sampling of voltage, using ADC_sample2()
 */

//#define HUB_SOFTWARE_REVISION		139		// MHH:08/07/2025
/*
 * MHH: 07/07/2025
 *
 * 07/07/2025
 *
 * In Set_Motor_RPM(int rpm,int ifrom) minimum aout_val now set to 200, not 100
 *
 * if(aout_val < 200) aout_val = 200;	// MHH:07/07/2025
 *
 * If driver NOT_READY pin active then log to Hub Data:
 *
 *        		L3PRINTF("RDY=0");	// MHH:07/07/2025
 *
 */
//=================================================================================================
//#define HUB_SOFTWARE_REVISION		138		// MHH:29/06/2025
/*
 * MHH:28/06/2025
 *		Numerous changes to try and allow power to be turned off without losing position.
 *		Many centre on the ADC_voltage_disabled boolean value, but other changes are to improve hub data logging
 *		so that calibrations are listed, along with the first fine rise which should allow watching for position creep.
 *		The "DA:" log line is listed every time the voltage drops below the trigger voltage which is nominally set to 22 V for a 24V system.
 */
//#define HUB_SOFTWARE_REVISION		137		// MHH:05/06/2025

/*
 * MHH:05/06/2025
 *
 * When in reverse mode, set motor RPM to max value, as per Martin's request
 *
 * 			Set_Motor_RPM(MOTOR_HI_RPM,1040);		// MHH:05/06/2025
 */
//#define HUB_SOFTWARE_REVISION		136	// MHH:23/05/2025
/*
 * MHH:23/05/2025.
 * When a position error has occurred, the position was not being updated in MRAM. Now update. Put_encoder_pos(int ifrom)
 */
/*
 * MHH:20/05/2025
 * Disable setting Enc_feather_limit_reached flag when a position error is detected. This should allow AC300 to control feathering by monitoring current
 * if a position error is detected, without needing to ignore feather flag or have the hub stop the motor if it thinks feather reached.
 * MHH:20/05/2025
 * Modify both Enc_check_fine_microswitch_on() and Enc_check_coarse_microswitch_on() so that if the Enc.Pos is within tolerance of actual stop position then
 * either Enc_fine_microswitch_on or Enc_coarse_microswitch_on flags are set on instead of waiting for tick_count > 5.
 *
 */

/*
 * Change default cal_tolerance from 0.05 mm to 0.5 mm for gear ration 190, 4 pole
 //	Enc_data.cal_tolerance = 16;		// just for ini.
	Enc_data.cal_tolerance = 120;		// MHH:19/05/2025. (0.5 * 4 * 190)/3.175 just for ini.
 */
//#define HUB_SOFTWARE_REVISION		135	// MHH:23/04/2025
/*
  * //#define MOTOR_STANDARD_RPM	5000		// MHH:14/01/2025
#define MOTOR_STANDARD_RPM	6870		// MHH:23/04/2025. As per Xoar request. Nominal speed, as defined by Maxon for 24V motor, code=539487
 */

//#define MH_IGNORE_ERROR
//#define HUB_SOFTWARE_REVISION		134	// MHH:31/03/2025
/*
 * This version sets Enc_positioning flag to false for any command that is not positioning. I noticed that after a position command a move FINE at speed 1
 * was jerky.
 *
 */
//=============================================================================================================
//#define HUB_SOFTWARE_REVISION		133	// MHH:19/02/2025
/*
 * This version tries to prevent a sudden change in direction by only allowing a new direction if not rotating at > 2000 rpm in
 * the opposite direction. If it is then Motor is disabled and the rest of the command ignored.
 *
 * Note: Also improved position accuracy by hard coding position_tolerance to 2 (was pole_pairs).
 */
//=============================================================================================================
//#define HUB_SOFTWARE_REVISION		132	// MHH:13/02/2025
/*
 *  This version fixed the Xoar melting hub PCB problem by replacing the code which changed direction at full speed when calibrating with disable, wait then
 *  new direction.
 */
//=============================================================================================================
//#define HUB_SOFTWARE_REVISION		131	// MHH:03/02/2025
/*
 *  Problem: When coming out of FINE stop and under Auto control, Hub was setting speed to MOTOR_HI_RPM instead of Control Speed (1-10).
 *           The same logic applies for coming out of Coarse.
 *  Fix: Only set MOTOR_HI_RPM when coming out of FINE stop or COARSE stop if Control Speed = 10 (top speed)
 *
 *			if(Board_pin_read(FINE_MICROSWITCH_PIN) && (Get_command_speed() == 10))	// MHH:03/02/2025 Coming out of reverse?
			{
				Set_Motor_RPM(MOTOR_HI_RPM,1030);		// MHH:14/01/2025
			}
			else
			{
				Motor_set_speed();
			}
*/
//=============================================================================================================
//#define HUB_SOFTWARE_REVISION		130	// MHH:14/01/2025
/*
 *  Problem: Speed controller attempting to run the motor at 8000 rpm by default, but there is little torque on brushless Maxon motors at this RPM
 *           so change of blade angle speed could depend on load.
 *  Fix: Change default speed from 8000 rpm to 5000 rpm. Note: Still need to run some operations at 8000 rpm. Eg Feathering and Calibrating.
 *       Also, maybe getting out of reverse/beta?

//#define MOTOR_STANDARD_RPM	8000		// Try. 02/12/2022
#define MOTOR_STANDARD_RPM	5000		// MHH:14/01/2025
*/
//=============================================================================================================
//#define HUB_SOFTWARE_REVISION		129	// MHH:12/01/2025
/*
 *  Problem: When running AC300RemoteTest_10_003 in "Test Calibrate and Pos" mode, with loops set to 10, and the command  = "RC_SET_POS=n.n" eventually the
 *           hub would go to full feather mode at full speed and only limit on the coarse hard stop.
 *
 *  Solution: It seems that there was a situation where Motor_mode could be set to FINE but Motor_dir still be set to COARSE in the positioning logic in
 *  		Motor_process_command(), so this section was rewritten. See #ifdef MH_OLD_POS.
 */
//=============================================================================================================
//#define HUB_SOFTWARE_REVISION		128	// MHH:30/11/2024
/*
 *  Problem: If DAC_v is set to (say) 205 as in previous revision, then it is not possible for a fast start.
 *
 *  Solution: Comment out previous "fix" where DAC_v is set to 205 in Disable_Motor routine
 */
//==============================================================================================================
//#define HUB_SOFTWARE_REVISION		127	// MHH:22/11/2024
/*
 * Problem: When fine tuning AC300 Brushless_control() routine I noticed that the initial current was dependent on the value of DAC0_v before it is set,
 * This could be any value, as it was left at value when the motor was disabled.
 *
 * Solution: Set DAC_v to a known value (205=1024/5) when disabling.
 *
 * In motor.c: void Disable_Motor(int ifrom)
 *
 * 	if(DAC0_v!=205)	// MHH:22/11/2024
	{
		Set_AOUT(205,4);
	}
 */

//===============================================================================================================
//#define HUB_SOFTWARE_REVISION		126	// MHH:25/10/2024
/*
 * MHH:25/10/2024.
 * According to Martin 1.25 had the position verification logic disabled. Think source code in MCUXpresso 6.1
 * Version on my PC was 1.24, 1.25 was commented out, so have gone to 1.26 for bug fix.
 * Bug: Out of position error (double red flash on feather led) when moving from fine stop in coarse direction and back to fine stop in manual mode.
 * Fix: In Hub_encoder.c:
 * bool Enc_check_fine_position_ok(void)
{
	//	if((Enc_Pos.pos < Enc_data.cal_fine_stop - tolerance) || (Enc_Pos.pos > Enc_data.cal_fine_stop2 + tolerance)) return false;
	//	if((Enc_Pos.pos < Enc_data.cal_fine_stop - Enc_data.cal_tolerance) || (Enc_Pos.pos > Enc_data.cal_fine_stop2 + tolerance)) return false;	// MHH:10/01/2024
	// MHH:24/10/2024. Replace above line with following to try and fix bug. Ignore cal_fine_stop2 as this was not set correctly.

	if(Enc_Pos.pos < (Enc_data.cal_fine_stop - Enc_data.cal_tolerance))
	{
		return false;
	}
	int tolerance = (Enc_data.cal_pole_pairs << 2);	// MHH:14/12/2023
	if(Enc_Pos.pos > (Enc_data.cal_fine_stop + Enc_data.cal_tolerance + tolerance))
	{
		return false;
	}
	return true;
}
 * And:
 * Beef up error logging, adding this code to Enc_check_fine_microswitch_on_ini():
 * 	if(Enc_Pos.pos < (Enc_data.cal_fine_stop - Enc_data.cal_tolerance))
	{
		L2PRINTF("?FR31:%d,FS=%d,T=%d\r\n",Enc_fine_check_position,Enc_data.cal_fine_stop,Enc_data.cal_tolerance);
	}
	int tolerance = (Enc_data.cal_pole_pairs << 2);
	if(Enc_Pos.pos > (Enc_data.cal_fine_stop + Enc_data.cal_tolerance + tolerance))
	{
		L2PRINTF("?FR32:%d,FS=%d,T=%d\r\n",Enc_fine_check_position,Enc_data.cal_fine_stop,Enc_data.cal_tolerance);
	}
 *
 *
 */



//#define HUB_SOFTWARE_REVISION		125		// MHH:08/04/2024
//#define HUB_SOFTWARE_REVISION		124		// MHH:24/01/2024
//#define HUB_SOFTWARE_REVISION		123		// MHH:10/01/2024
//#define HUB_SOFTWARE_REVISION		122		// MHH:31/12/2023
//#define HUB_SOFTWARE_REVISION		121		// MHH:22/12/2023
//#define HUB_SOFTWARE_REVISION		120		// MHH:20/12/2023
//#define HUB_SOFTWARE_REVISION		119		// MHH:19/12/2023
//#define HUB_SOFTWARE_REVISION		118		// MHH:03/12/2023
//#define HUB_SOFTWARE_REVISION		117		// MHH:03/12/2023
//#define HUB_SOFTWARE_REVISION		116		// MHH:22/11/2023
//#define HUB_SOFTWARE_REVISION		115		// MHH:24/10/2023
//#define HUB_SOFTWARE_REVISION		114		// MHH:18/09/2023
//#define HUB_SOFTWARE_REVISION		113		// MHH:13/09/2023
//#define HUB_SOFTWARE_REVISION		112		// MHH:08/09/2023
//#define HUB_SOFTWARE_REVISION		111		// MHH:06/09/2023
//#define HUB_SOFTWARE_REVISION		110		// MHH:02/09/2023
//#define HUB_SOFTWARE_REVISION		109		// MHH:29/08/2023
//#define HUB_SOFTWARE_REVISION		108		// MHH:03/08/2023
//#define HUB_SOFTWARE_REVISION		107		// MHH:24/07/2023
//#define HUB_SOFTWARE_REVISION		106		// MHH:24/05/2023
//#define HUB_SOFTWARE_REVISION		105		// MHH:12/05/2023
//#define HUB_SOFTWARE_REVISION		104		// MHH:21/04/2023
//#define HUB_SOFTWARE_REVISION		103		// MHH:27/03/2023
//#define HUB_SOFTWARE_REVISION		102

#define false 0
#define true (!false)
#define FALSE	(0)

#define WD_AC200HUBL	1000		// Watchdog source files
#define WD_MOTOR		2000

//void Hub_watchIt2(uint16_t from);
void Hub_watchIt(uint16_t from);

#define MOTOR_STANDARD_RPM	6870		// MHH:23/04/2025. As per Xoar request. Nominal RPM, as defined by Maxon for 24V motor, code=539487

#define MH_MCT8316_DEBUG	// MHH:13/06/2026. Special code for development board!!!!
#define MH_MCT8316_DEBUG2	// MHH:14/06/2026. Special code for development board!!!!

//#define MOTOR_HI_RPM		5000		// MHH:14/06/2026
#define MOTOR_HI_RPM		8000		// MHH:14/01/2025

#define STOP_FINE	1
#define STOP_COARSE	2
#define STOP_FEATHER	4
#define STOP_REVERSE	8

//#define CALIBRATE_POS_BIT		128
#define NO_FEATHER_BIT					128		// Only set if position looks invalid at fine stop
#define NOT_READY_BIT					64		// Opposite value of READY_PIN
#define MAN_CALIBRATE_COMPLETE_BIT		32		// MHH:28/08/2023
#define MOTOR_ENABLED_BIT				16		// Used for testing if hub has msg from AC200


#define HUB_ERR_POS_FINESTOP			1
#define HUB_ERR_POS_COARSESTOP			2
#define HUB_ERR_RPM_ZERO				3

#define HUB_ABORT_INVALID_POLEPAIR		1

void Sig100_format_error_pkt(uint8_t error_type,uint16_t ecode,int16_t eval);

//#define MH_ENCODER_MAX_PRECISION

int abs(int ival);


#define DIR_STOPPED 	0
#define DIR_COARSE    	1
#define DIR_FINE   		2
#define DIR_UNKNOWN 	3

extern int8_t Motor_dir;

void Enc_Init(void);

void Disable_Motor(int ifrom);
void Motor_check_pos(int incr);

void Config_Input_Pin(uint32_t port);	// MHH:31/08/2023

extern uint8_t Hub_hardware_version;	// MHH:31/08/2023
extern bool Hub_maxon_driver;			// MHH:24/04/2026

extern volatile uint32_t Adc_voltage;
extern uint8_t ADC_adjusted_voltage;
extern uint8_t ADC_adjusted_temp;
extern int ADC_avg_temp;
extern bool ADC_voltage_disabled;

extern char Command_string[20];	// Could make these into a structure...
extern char Command;
extern char Arg1,Arg2;

extern bool Calibration_command;
extern bool Calibrating;

extern bool Enc_fine_microswitch_on;
extern bool Enc_fine_microswitch_pos_set;
extern bool Enc_coarse_microswitch_on;

extern bool Hub_watchdog_active;

extern bool Speed_raw_mode;
extern bool Speed_opamp;

//extern bool Enc_update_coarse_stop;
//extern int Enc_coarse_ms_pos;

extern int Motor_direction;
extern bool Motor_fine_stop_override;
extern bool Motor_diags;
extern bool Motor_verify;

extern int Enc_dir;
//extern int Enc_max_fine_rpm;
//extern int Enc_pos;
extern int Enc_rpm;
extern int Enc_posx;
extern bool Enc_posx_set;
extern int Enc_fine_ms_stop_pos;
extern int Enc_coarse_ms_stop_pos;
extern bool Enc_sense_hardstop;
//extern int Enc_usecs;
extern int Enc_max_usecs;
extern int Enc_hardstop_posx;
extern int Enc_ecount;
extern int Enc_last_err;
extern int Enc_ready_count;

extern bool Enc_fine_rise_error;
extern bool Enc_coarse_rise_error;

//extern uint8_t Enc_coarse_stop_ticks;
//extern bool Enc_systick_check_coarse_stop;

//extern uint8_t	Enc_fine_rise_stop_ticks;
extern uint8_t	Enc_fine_fall_stop_ticks;
extern bool Enc_systick_check_fine_rise;
//extern bool Enc_systick_check_fine_fall;

extern bool Enc_update_fine_rise_stop;
extern bool Enc_update_fine_fall_stop;

extern int Enc_fine_ms_pos_rise;
extern int Enc_coarse_ms_pos_rise;
//extern int Enc_fine_ms_pos_fall;

extern bool Enc_ud_coarse_stop;
extern bool Enc_ud_feather_stop;
extern bool Enc_ud_reverse_stop;

extern bool Enc_coarse_limit_reached;
extern bool Enc_feather_limit_reached;
extern bool Enc_reverse_limit_reached;

extern bool Enc_positioning;
extern int Enc_target_position;
extern bool Motor_enabled;

extern uint32_t Systick_new_rpm_count;	// MHH:19/02/2025
extern uint32_t Systick_rpm_count;

extern TIMER_t Enc_timer;
extern TIMER_t Motor_timer;

#define POS_MAGIC1		11111
#define POS_MAGIC2		22222

typedef struct		// MHH:08/09/2023
{
	uint16_t magic1;
	int pos;
	int pos_check;
	uint16_t magic2;
} Enc_pos_td;

typedef struct		// MHH:26/06/2025
{
	uint16_t adc_corrected_min_voltage;
	uint16_t cal_tolerance_enc_units;

} Enc_misc_td;
extern Enc_misc_td Enc_misc;
extern Enc_pos_td Enc_Pos;
bool Enc_pos_check(uint16_t ifrom);
void Enc_pos_report_error(void);
void Enc_pos_save2(void);	// Called after loading from ssp

void Enc_check_fine_microswitch_on(void);
void Enc_check_coarse_microswitch_on(void);

typedef struct
{
	uint16_t magic;				// 0.
	uint16_t creation_time;		// 2. May use for creation time
	uint16_t hub_id;			// 4.
	uint16_t update_count;		// 6. Incremented by AC200HubCalibrator only
	uint16_t modified_date;		// 8.
	uint16_t creation_date;		// 10.

	uint16_t cal_tolerance;		// 12. In Ecount units, equivalent to +/- 0.5 mm, provided by Calibrator.
	uint16_t cal_motor_ratio;	// 14. Test hub uses 294.6
	uint16_t cal_cam_length;	// 16. In tenths of a mm, usually 26.0
	int16_t cal_blade_offset;	// 18. In degrees, usually 30.0
	uint16_t cal_leadscrew_mm;	// 20. In mm, 3 implied decimal places. Default is 3.175 (25.4/8)

	uint8_t cal_pole_pairs;		// 22.
	uint8_t user_defined_stops;	// 23. Bit flags of user defined stops (feather and reverse)
	uint16_t cal_coarse_stop;	// 24. Was user defined, but now is provided by calibrator.
	uint16_t ud_feather_stop;	// 26. User defined coarse stops
	uint16_t ud_reverse_stop;	// 28.

	uint16_t cal_fine_stop;		// 30.
	uint16_t cal_tdc_pos_offset;		// 32. MHH:04/07/2023. Same value, different name.
	uint16_t cal_fine_hard_stop;		// 34.
	uint16_t ref_max;				// 36.Could be 1 byte. These fields build picture of reference stops
	uint16_t ref_start[3];			// 38.
	uint16_t ref_end[3];			// 44.
	uint8_t voltage_min;			// 50. MHH:29/06/2025
	uint8_t cal_current_max;		// 51. MHH:03/09/2025
	uint16_t cal_fine_stop2;		// 52. There can be play depending on if cam is pushing or pulling. Since the fine stop is used to
									//	   check the accuracy of the position, it is valid inside the range fine_stop to fine_stop2, +/- tolerance
	uint8_t hw_flags;				// 54 hardware flags eg op_amp
	uint8_t hw_version;				// 55. 18/07/2023
	uint8_t sw_flags;				// 56. MHH:23/06/2025 eg 24V
	uint8_t voltage_correct_pct;	// 57. MHH:25/06/2025

	uint16_t cal_pos;				// 58. Returned from calibrator, could be moved out
	uint32_t crc32;					// 60.
									// 64.Should fit in 1 flash page

/*
*	Update following data to memory if an error, but limit rate it can be updated to (say) once every 10 seconds
*/
	uint16_t magic2;				// 64.0.When just loading from this area
	uint8_t run_flags;				// 66.2 Eg 1 = not allowed to FEATHER
	uint8_t update_count2;			// 67.3 So we can update AC200 with copy of data following magic3 on init if different to AC200
	uint16_t ac200_run;				// 68.4. Use for error reporting
	uint16_t hub_run;				// 70.6
	uint16_t ac300_software_version;// 72.8	MHH:16/12/2025
	uint16_t hub_software_version;	// 74.10
	uint16_t calibrate_pos;			// 76.12
	uint8_t run_set_from;			// 78.14	// MHH:04/08/2023.
	uint8_t fill1;					// 79.15

	uint16_t error_count;			// 80.16
	uint8_t error_type;				// 82.18	eg 'E'
	uint8_t error_command;			// 83.19
	uint16_t error_number;			// 84.20
	uint16_t error_ac200_run;		// 86.22
	uint16_t error_data;			// 88.24 Anything else that helps define an error
	uint8_t error_badpos1_count;	// 90.26 Increment if position is invalid on loading
	uint8_t error_badpos2_count;	// 91.27
	uint32_t log_pos;				// 92.28	// MHH:05/08/2023.
	uint16_t error_hub_run;			// 92.30	// MHH:06/09/2023

	uint16_t error_end_data;		// 96.32	Total of 32 bytes in data 2 area

	uint16_t pos1;					// 98.34 These 3 updated together, so we know if we have an rpm restart (if motor was going when hub mcu stopped)
	uint16_t pos1_check;			// 100.36 pos complement, to check pos correct
	uint16_t rpm;					// 102.38
	uint16_t pos2;					// 104.40
	uint16_t pos2_check;			// 106.42
	uint16_t rpm2;					// 108.44
	uint16_t fill2[2];				// 110.46
	uint16_t magic3;				// 114.50
	uint16_t end_data2;				// 116.52
	// Note: Copy until up to rpm from hub, as maintaining data below in struct is handled by ac200
	// We may not need any of the data below in the hub. Have decided that it is simpler and more robust to send any error data from the hub to ac200
	// rather than trying to store, as the storing may be causing the error.

}Enc_data_td;

#define ENC_RUN_FLAGS_NO_FEATHER		1


extern Enc_data_td Enc_data;
extern uint8_t Hub_data[];
extern uint8_t Spkt[];
//extern int MH_trace;

typedef enum
{
	IDLE=0,
	FINE,
	COARSE,
    FEATHER,
	REVERSE,
//	CALIBRATE_COARSE,
//	CALIBRATE_FINE,
} motor_Mode;

extern motor_Mode Motor_mode;


typedef enum
{
	MAN_IDLE=0,
	MAN_START,
	MAN_COARSE_TO_HARD_STOP,
	MAN_FINE_TO_HARD_STOP,
	MAN_COARSE_TO_FINE_MS,
	MAN_FINE_TO_FINE_MS,
	MAN_FINISH,
	MAN_ERROR,		// MHH:13/09/2023
} manual_Mode;
extern manual_Mode Manual_mode;

typedef enum
{
    CAL_IDLE=0,
	CAL_START,
	CAL_MAIN,
	CAL_FINISH,
	CAL_ERROR,
} CAL_state;

typedef enum	// If memory is overwritten then true is anything other than zero. This may help.
{
	True=3,
	False,
}Boolean;
extern bool IsTrue(Boolean b,int ifrom);

#define AC200ENC_SSP	1000

extern volatile uint32_t Systick_tick;
extern CAL_state Hub_calibrate_state;
extern uint8_t Hub_calibrate_type;		// 1 = AC200 requested calibrate
bool Process_calibration_command(CAL_state new_command);

void ADC_init(void);
void ac200Enc_update_pos(int ifrom);

extern int Enc_calibrate_rpm;
extern int Enc2_pos;
void Set_Direction_Coarse(int ifrom);
void Set_Direction_Fine(int ifrom);
void Set_Motor_RPM(int rpm,int ifrom);
void Set_Motor_RPM2(int rpm);

//extern int p_BufferLen;
extern char p_Buffer[];

void Board_UART1PutSTR(const char *buff);
void Hub_log_puts(const char *s);
void Hub_plog_puts3(const char *s);

//#define PRINTF(...) {p_BufferLen = sprintf(p_Buffer,__VA_ARGS__);Board_UART0PutSTR(p_Buffer);}

#define PRINTF_FLUSH		{Board_output_wait_until_rb_empty();}
//#define PRINTF(...) {sprintf(p_Buffer,__VA_ARGS__);Board_UART1PutSTR(p_Buffer);PRINTF_FLUSH;}	// Note: Causes Hub to freeze when PRINTF inside ISR
#define PRINTF(...) {sprintf(p_Buffer,__VA_ARGS__);Board_UART1PutSTR(p_Buffer);}

//#define LPRINTF(...) {p_BufferLen = sprintf(p_Buffer,__VA_ARGS__);Hub_plog_puts(p_Buffer);}
#define LPRINTF(...) {sprintf(p_Buffer,__VA_ARGS__);Hub_plog_puts(p_Buffer);}

//#define L2PRINTF(...) {p_BufferLen = sprintf(p_Buffer,__VA_ARGS__);Hub_plog_puts(p_Buffer);Board_UART0PutSTR(p_Buffer);}
#define L2PRINTF(...) {sprintf(p_Buffer,__VA_ARGS__);Hub_plog_puts(p_Buffer);Dputs(p_Buffer);}	// MHH:10/01/2026
//#define L3PRINTF(...) {p_BufferLen = sprintf(p_Buffer,__VA_ARGS__);Hub_plog_puts3(p_Buffer);}
#define L3PRINTF(...) {sprintf(p_Buffer,__VA_ARGS__);Hub_plog_puts3(p_Buffer);}	// MHH:10/01/2026


//#define SIG100_PRINTF(...) {p_BufferLen = sprintf(p_Buffer,__VA_ARGS__);SIG100_RB_puts(p_Buffer);Sig100_flush_and_wait(10);}
//#define SIG100_PRINTF(...) {p_BufferLen = sprintf(p_Buffer,__VA_ARGS__);SIG100_RB_puts_and_wait(p_Buffer);}
//#define SIG100_PRINTF(...) {p_BufferLen = sprintf(p_Buffer,__VA_ARGS__);SIG100_RB_puts(p_Buffer);}	// MHH:05/12/2023
#define SIG100_PRINTF(...) {sprintf(p_Buffer,__VA_ARGS__);SIG100_RB_puts(p_Buffer);}	// MHH:10/01/2026

void DebugAbort(char *msg);
void Serial_rb_init(void);
void Timer_interrupt_init(void);
void Timer_interrupt_disable(void);

void Board_UARTPutchar_RB(uint8_t c);
int Board_UARTGetChar_wait_RB(int millisecs);
void Board_output_wait_until_rb_empty(void);

int Board_UARTGetChar_RB(void);

void Checksum_and_send_pkt(const int plen);

void Dputc(const char c);
void Dputs(const char *buff);
void Dputs2(const char *buff);

void Enc_raw_speed_control(void);

void Enc_ssp_update_enc_run_data(void);
void Enc_ssp_update_enc_run_flags(void);

void Enc_adjust_pos_when_low_voltage(void);	// MHH:29/06/2025
void Enc_pos_save(void);

int16_t Get_int16(char *cptr);
uint16_t Get_uint16(char *cptr);
void Put_uint16(uint8_t *dst_p,uint16_t v);

//void Hub_flog_init(void);
//void Hub_flog_start_record(void);
//int Hub_flog_check_ring_buffer(void);
//void Hub_flog_end_record(void);
//void Hub_flog_display(void);
//void Hub_flog_reset(void);
//void Hub_flog_display_data(void);
//void Hub_flog_send_binary(void);
//void Hub_flog_verify(void);
//void Hub_flog_list_mram_index(void);

void Hub_plog_init(void);
void Hub_plog_puts(const char *s);
void Hub_plog_display(int val);

void Sig100_send_calibration_response(void);

#ifdef MH_SIG100_RB
void SIG100_rb_init(void);
int Sig100_RB_bytes_avail(void);
int Sig100_RB_getc_timeout(int millisecs);
void Sig100_RB_clear_input(void);
int Sig100_RB_getc(void);
void SIG100_RB_puts(char *string);
int Sig100_RB_getc_nowait(void);
void SIG100_RB_UARTPutchar(uint8_t c);
void Sig100_RB_flush_output(void);
void Sig100_flush_and_wait(int millisecs);
void SIG100_RB_puts_and_wait(char *string);
void Sig100_X_putc(uint8_t c);
void Sig100_X_puts(char * string);
int Sig100_X_getc_timeout(int millisecs);
#else
void Sig100_UARTPutChar(char ch);
int Sig100_getc_timeout(int millisecs);
int Sig100_getc(void);
int Sig100_UARTGetChar(void);
void Sig100_UARTPutSTR(char *str);
#endif


void Sig100_HDC_Set(bool On);


void SSP_get_encoder_pos(void);
void SSP_initialise_page_zero();

int Sig100_update_hub_data();

void LPC845_reboot(void);
int LPC845_flash(void);
int LPC845_read_hub_data_from_flash(void);
void LPC845_read_flash(void);
int LPC845_write_last_sector(void);

void Put_encoder_pos(int ifrom);
void SSP_set_position_error(int ifrom);

//extern bool LPC845_flash_data_loaded;
extern Boolean LPC845_flash_data_loaded;

#define COMMAND_MAX_LEN	20
#define NO_COMMAND	0
extern char Command_string[20];
extern char Command,Command_speed;
extern uint8_t Command_len;
extern char Arg1,Arg2;
extern uint32_t Command_total;

void Check_command_mode(void);

bool MCT8316_check_reset(int ifrom);
void MCT8316_driver_reset(int ifrom);


bool Sig100_receive_hub_data(void);
void Sig100_send_hub_data(void);
void Sig100_send_error_pkt(uint8_t error_type,uint16_t ecode,int16_t eval);

extern uint8_t Hub_data[64];	// So can fit in 1 page of flash memory

uint32_t Wiki_CRC32(const uint8_t data[],size_t data_length);

void Enc_raw_speed_check_rpm(void);
void Set_AOUT(int val,int ifrom);
void Set_AOUT2(int val);

extern int Motor_systick;
extern bool Motor_main_mode;
extern uint32_t Motor_target_rpm;
extern uint16_t Enc_max_current;
extern int DAC0_v;

extern int16_t AC200_current;

#ifdef MH_XXX	// MHH:29/06/2026

extern bool Flog_outside_range_pin;
extern bool Flog_diags_enabled;

void SPI_ADXL375_init(void);
extern int Adxl375_sample_rate;
extern int Adxl375_x_count_max;
extern int Adxl375_x_sec_tot;
//#define MH_ADXL1001

extern uint32_t Flog_putc_count;
#define FLOG_DATA_SIZE		512
typedef struct
{
	uint32_t head_ptr;
	uint32_t tail_ptr;
	uint32_t overrun;
	uint8_t data[FLOG_DATA_SIZE];
} td_FLOG_FLASH_DATA;
extern td_FLOG_FLASH_DATA Flog_data;
#endif

//extern int After_AT_commands;

//void PWM_SetDutyPercent(int dutyPercent);
void PWM_SetDutyTenBit(int duty_10bit);
extern int PWM_duty_10bit;

#endif /* AC300_HUB_H_ */
