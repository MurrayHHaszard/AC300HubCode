/*
 * Hub_adc.c
 *
 *  Created on: 29/06/2023
 *      Author: OEM
 */

#include "LPC8xx.h"
#include "rom_api.h"
//#include "LPC8xx_swm.h"
//#include "LPC8xx.h"
#include "core_cm0plus.h"
#include "DAC.h"
#include "iocon.h"

//#include "Multi_Timer_Blinky.h"
#include "syscon.h"
//#include "wkt.h"
//#include "mrt.h"
#include "utilities.h"


//#include "LPC8xx.h"
#include "swm.h"
#include "syscon.h"

#include "adc.h"
#include "ctimer.h"

#include "AC300_hub.h"
#include "Hub_flashlog.h"

uint8_t ADC_adjusted_voltage;
uint8_t ADC_adjusted_temp;
int ADC_temp_tot;
uint8_t ADC_temp_count;
int ADC_avg_temp;

extern volatile uint32_t Systick_secs;
//=======================================================================================================
uint32_t current_seqa_ctrl, current_seqb_ctrl;
//volatile enum {False, True} seqa_handshake, seqb_handshake;
volatile enum {FFalse, TTrue} seqa_handshake;
volatile uint32_t Adc_temp,Adc_voltage;
volatile uint32_t Adc_accel_val;


//#define MR_TIME_INTERVAL	1000			// 1 ms
//#define MR_TIME_INTERVAL	125				// * 8
//#define MR_TIME_INTERVAL	50				// MHH:14/12/2023. Allow 20,000 samples per second
//#define MR_TIME_INTERVAL	10				// MHH:14/12/2023. Allow 100,000 samples per second
// If we average over 8 samples, then 12,500 sample per sec

uint32_t MR_time_val = MR_TIME_INTERVAL;
uint32_t Ctimer_count;
uint16_t Ctimer_avg_count;
uint32_t Ctimer_avg_tot;
uint32_t Adc_accel_tot;
uint16_t Adc_accel_count;
uint16_t Ctimer_accel_avg;
//uint16_t Ctimer_accel_tbl[8];
uint32_t Ctimer_false_handshake_count;

#ifdef MH_XXX	// This did not work. Suspect divide may be too slow.
uint16_t Ctimer_get_avg_val(void)
{
	uint16_t avg_val =  Ctimer_avg_tot >> 3;		// Divide by 8
	uint32_t new_avg_tot = 0;
	uint16_t new_avg_count=0;
	for(int i=0;i<8;i++)
	{
		uint16_t v = Ctimer_accel_tbl[i];
		if(v > avg_val - 10 && v < avg_val + 10)
		{
			new_avg_tot += v;
			new_avg_count++;
		}
	}
	uint16_t new_avg = avg_val;
	if(new_avg_count > 0)
	{
		new_avg = new_avg_tot / new_avg_count;
	}
	return new_avg;
}
#endif
uint32_t Ctimer_count2;
uint32_t Ctimer_secs;
void ADC_sample2(void);
void CTIMER0_IRQHandler(void) {

  LPC_CTIMER0->IR = 1<<MR0INT;              // Clear the interrupt flag
  LPC_CTIMER0->MR[0] = LPC_CTIMER0->TC + (MR_TIME_INTERVAL-2);
//  LPC_CTIMER0->MR[0] += MR_TIME_INTERVAL;
  Ctimer_count++;
  if(++Ctimer_count2 >= 100)				// 1 tenth second
  {
	  Ctimer_count2 = 0;
	  Ctimer_secs++;
//	  PRINTF("CT:%d\r\n",Ctimer_secs);
  }
  ADC_sample2();
}

void CTIMER_idle_loop(int ival)
{
	return;
#ifdef MH_XXX
	uint32_t ctimer_secs = Ctimer_secs;
	PRINTF("CTIMER_idle_loop:%d\r\n",ival);
	int cnt_max = 5;
	if(ival < 10) cnt_max = 2;
	for(int cnt=0;cnt<cnt_max;)
	{
		if(Ctimer_secs != ctimer_secs)
		{
			cnt++;
			ctimer_secs = Ctimer_secs;
			PRINTF("CT:%d\r\n",ctimer_secs);
		}
	}
#endif
}
#ifdef MH_ADXL1001
uint32_t Ctimer_count2;
uint32_t Ctimer_usecs;
uint32_t Ctimer_usecs_elapsed;
//#define MH_TEST_CTIMER
#ifdef MH_TEST_CTIMER
uint16_t Ctimer_test_val=2048;
//int16_t Ctimer_change_val=4;
bool Ctimer_incrementing=true;
uint16_t Ctimer_change_count;
#endif
void CTIMER0_IRQHandler(void) {

  LPC_CTIMER0->IR = 1<<MR0INT;              // Clear the interrupt flag
  LPC_CTIMER0->MR[0] = LPC_CTIMER0->TC + (MR_TIME_INTERVAL-2);
  if(seqa_handshake == TTrue)	// Should always be true I think
  {
	  seqa_handshake = FFalse;
	  LPC_ADC->SEQA_CTRL = current_seqa_ctrl;	// Restart sequence
  }
  else
  {
	  Ctimer_false_handshake_count++;
  }
  Ctimer_count++;
#ifdef MH_TIME_INTERRUPT
  if(++Ctimer_count2 >= 100000)				// 1 second
  {
	  uint32_t usecs = LPC_CTIMER0->TC;
	  if(Ctimer_usecs)
	  {
		  Ctimer_usecs_elapsed = usecs - Ctimer_usecs;
	  }
	  Ctimer_usecs = usecs;
	  Ctimer_count2 = 0;
  }
#endif

//  MR_time_val += MR_TIME_INTERVAL;			// Add to MR so we can still use CTIMER as timer
//  LPC_CTIMER0->MR[0] = MR_time_val;
//  Ctimer_accel_tbl[Ctimer_avg_count++] = Adc_accel_val;

  if(Board_pin_read(DIAGS_OUTSIDE_RANGE_PIN))
  {
	  Flog_outside_range_pin = true;
  }
  else
  {
	  Ctimer_avg_tot += Adc_accel_val;
	  Ctimer_accel_avg = Adc_accel_val;
  }

  if(++Ctimer_avg_count >= 2)
  {
	  Ctimer_avg_count = 0;
	  Ctimer_accel_avg = Ctimer_avg_tot >> 1;		// Divide by 2
//	  Ctimer_accel_avg = Ctimer_get_avg_val();
	  Ctimer_avg_tot = 0;

#ifdef MH_TEST_CTIMER
	  Ctimer_accel_avg =  Ctimer_test_val;
	  if(++Ctimer_change_count >= 10)
	  {
		  Ctimer_change_count = 0;
		  if(Ctimer_incrementing)
		  {
			  Ctimer_test_val += 4;
			  if(Ctimer_test_val > (2048 + 1600))
			  {
				  Ctimer_incrementing = false;
			  }
		  }
		  else
		  {
			  Ctimer_test_val -= 4;
			  if(Ctimer_test_val < (2048 - 1600))
			  {
				  Ctimer_incrementing = true;
			  }
		  }
	  }
#endif
	  Flog_put_ring_data();
	  Flog_outside_range_pin = false;
  }
} // end of Ctimer0 ISR
#endif
// ============================================================================
// ADC Sequence A ISR
// Reads the data registers for the ADC channels assigned to sequence A
// and stores the contents in an array in SRAM. Then sets a handshake flag
// for main() and returns.
// ============================================================================
uint16_t Adc_val;
uint32_t Adc_count;
uint32_t Adc_count2;

void ADC_SEQA_IRQHandler(void) {
  uint32_t temp_flags,temp_data;

#ifdef MH_ADXL1001
//  if(Flog_diags_enabled)
  if(Flog_save_data_flag)
  {
	  Adc_count++;
	  temp_data = LPC_ADC->DAT[2];
	  Adc_accel_val = (temp_data>>4) & 0x00000FFF;
	  if(++Adc_count2 >= 1000)
	  {
		  Adc_count2 = 0;
		  temp_data = LPC_ADC->DAT[7];
		  Adc_temp = (temp_data>>4) & 0x00000FFF;
		  temp_data = LPC_ADC->DAT[8];
		  Adc_voltage = (temp_data>>4) & 0x00000FFF;
	  }
  }
  else
  {
	  temp_data = LPC_ADC->DAT[7];
	  Adc_temp = (temp_data>>4) & 0x00000FFF;
	  temp_data = LPC_ADC->DAT[8];
	  Adc_voltage = (temp_data>>4) & 0x00000FFF;
  }
#else
  temp_data = LPC_ADC->DAT[7];
  Adc_temp = (temp_data>>4) & 0x00000FFF;
  temp_data = LPC_ADC->DAT[8];
  Adc_voltage = (temp_data>>4) & 0x00000FFF;
#endif

  // Clear the interrupt flag
  temp_flags = LPC_ADC->FLAGS;
  temp_flags |= 1<<ADC_SEQA_INT;
  LPC_ADC->FLAGS = temp_flags;

  // Set the handshake flag for main
  seqa_handshake = TTrue;

  // Return from interrupt
  return;
}
void ADC_init(void)
{
//	  uint32_t current_clkdiv, val, temp_data, n;
	  uint32_t current_clkdiv;

	 // Step 1. Power up and reset the ADC, and enable clocks to peripherals
	  LPC_SYSCON->PDRUNCFG &= ~(ADC_PD);
	  LPC_SYSCON->PRESETCTRL0 &= (ADC_RST_N);
	  LPC_SYSCON->PRESETCTRL0 |= ~(ADC_RST_N);
	  LPC_SYSCON->SYSAHBCLKCTRL0 |= (ADC | GPIO0 | GPIO_INT | SWM);
	  LPC_SYSCON->ADCCLKDIV = 1;                 // Enable clock, and divide-by-1 at this clock divider
	  LPC_SYSCON->ADCCLKSEL = ADCCLKSEL_FRO_CLK; // Use fro_clk as source for ADC async clock

	  // Step 2. Perform a self-calibration
	  // Choose a CLKDIV divider value that yields about 500 KHz.
	  SystemCoreClockUpdate();                   // Get the fro_clk frequency
	  current_clkdiv = (fro_clk / 500000) - 1;

	  // Start the self-calibration
	  // Calibration mode = true, low power mode = false, CLKDIV = appropriate for 500,000 Hz
	  LPC_ADC->CTRL = ( (1<<ADC_CALMODE) | (0<<ADC_LPWRMODE) | (current_clkdiv<<ADC_CLKDIV) );

	  // Poll the calibration mode bit until it is cleared
	  while (LPC_ADC->CTRL & (1<<ADC_CALMODE));


	  // Step 3. Configure the external pins we will use
	  // Disable any fixed-pin functions on pins we are using as digital outs and ins (P0.11 and P0.10 I2C functions)
	  //LPC_SWM->PINENABLE0 |= (I2C0_SDA | I2C0_SCL); // P0.11 and P0.10 disable fixed pin functions if using them

	  // ADC1 - ADC6, ADC7, ADC9 - ADC11 pins to be ADC analog functions
	  // We won't use ADC0 or ADC8 (no affirmative action in this institution)
//	  LPC_SWM->PINENABLE0 &= ~(ADC_1|ADC_2|ADC_3|ADC_4|ADC_5|ADC_6|ADC_7|ADC_9|ADC_10|ADC_11); // Fixed pin analog functions enabled
//	  LPC_SWM->PINENABLE0 |= (ADC_0|ADC_8);                                                    // Movable digital functions enabled


	  LPC_IOCON->PIO0_18         &= (IOCON_MODE_MASK | MODE_INACTIVE);  // Disable pull-up and pull-down (ADC_8)
	  LPC_IOCON->PIO0_19         &= (IOCON_MODE_MASK | MODE_INACTIVE);  // Disable pull-up and pull-down (ADC_7)

	  // ADC7, ADC8 pins to be ADC analog functions
	  // We won't use any others (no affirmative action in this institution)

#ifdef MH_ADXL1001
	  if(Flog_diags_enabled)
	  {
		  LPC_IOCON->PIO0_14         &= (IOCON_MODE_MASK | MODE_INACTIVE);  // Disable pull-up and pull-down (ADC_2)
		  LPC_SWM->PINENABLE0 &= ~(ADC_2|ADC_7|ADC_8); // Fixed pin analog functions enabled
		  LPC_SWM->PINENABLE0 |= (ADC_0|ADC_1|ADC_3|ADC_4|ADC_5|ADC_6|ADC_9|ADC_10|ADC_11);
	  }
#else
//	  else
	  {
		  LPC_SWM->PINENABLE0 &= ~(ADC_7|ADC_8); // Fixed pin analog functions enabled
		  LPC_SWM->PINENABLE0 |= (ADC_0|ADC_1|ADC_2|ADC_3|ADC_4|ADC_5|ADC_6|ADC_9|ADC_10|ADC_11);
	  }
#endif

	  // Step 4. Configure the ADC for the appropriate analog supply voltage using the TRM register
	  // For a sampling rate higher than 1 Msamples/s, VDDA must be higher than 2.7 V (on the Max board it is 3.3 V)
	  LPC_ADC->TRM &= ~(1<<ADC_VRANGE); // '0' for high voltage

//#define desired_sample_rate 1200000
//#define desired_sample_rate 16000
#define desired_sample_rate 1200000	// MHH:14/12/2023

	  current_clkdiv = (fro_clk / (25 * desired_sample_rate)) - 1;

// Calibration mode = false, low power mode = false, CLKDIV = appropriate for desired sample rate.
	  LPC_ADC->CTRL = ( (0<<ADC_CALMODE) | (0<<ADC_LPWRMODE) | (current_clkdiv<<ADC_CLKDIV) );

	  // Step 5. Assign some ADC channels to each sequence
	  // Let sequence A = channels 1, 2, 3, 4, 5, 6
	  // Let sequence B = channels 7, 9, 10, 11
#ifdef MH_ADXL1001
	  current_seqa_ctrl = ((1<<2)|(1<<7)|(1<<8))<<ADC_CHANNELS;
#else
	  current_seqa_ctrl = ((1<<7)|(1<<8))<<ADC_CHANNELS;
#endif
//	  current_seqb_ctrl = ((1<<7)|(1<<9)|(1<<10)|(1<<11))<<ADC_CHANNELS;
	  current_seqb_ctrl = 0;

	  current_seqa_ctrl |= NO_TRIGGER<<ADC_TRIGGER;    // Use for software-only triggering
	  current_seqb_ctrl |= NO_TRIGGER<<ADC_TRIGGER;    // Use for software-only triggering

	  // Step 9. Choose burst mode (1) or no burst mode (0) for each of the sequences
	  // Let sequences A and B use no burst mode
	  current_seqa_ctrl |= 0<<ADC_BURST;
	  current_seqb_ctrl |= 0<<ADC_BURST;

	  // Step 10. Choose single step (1), or not (0), for each sequence.
	  // Let sequences A and B use no single step
	  current_seqa_ctrl |= 0<<ADC_SINGLESTEP;
	  current_seqb_ctrl |= 0<<ADC_SINGLESTEP;

	  // Step 11. Choose A/B sequence priority
	  // Use default
	  current_seqa_ctrl |= 0<<ADC_LOWPRIO;

	  // Step 12. Choose end-of-sequence mode (1), or end-of-conversion mode (0), for each sequence
	  // Let sequences A and B use end-of-sequence mode
	  current_seqa_ctrl |= 1<<ADC_MODE;
	  current_seqb_ctrl |= 1<<ADC_MODE;


	  // Step 15. Choose which interrupt conditions will contribute to the ADC interrupt/DMA triggers
	  // Only the sequence A and sequence B interrupts are enabled for this example
	  // SEQA_INTEN = true
	  // SEQB_INTEN = false
	  // OVR_INTEN = false
	  // ADCMPINTEN0 to ADCMPEN11 = false
	  LPC_ADC->INTEN = (1<<SEQA_INTEN)  |
	                   (0<<SEQB_INTEN)  |
	                   (0<<OVR_INTEN)   |
	                   (0<<ADCMPINTEN0) |
	                   (0<<ADCMPINTEN1) |
	                   (0<<ADCMPINTEN2) |
	                   (0<<ADCMPINTEN3) |
	                   (0<<ADCMPINTEN4) |
	                   (0<<ADCMPINTEN5) |
	                   (0<<ADCMPINTEN6) |
	                   (0<<ADCMPINTEN7) |
	                   (0<<ADCMPINTEN8) |
	                   (0<<ADCMPINTEN9) |
	                   (0<<ADCMPINTEN10)|
	                   (0<<ADCMPINTEN11);


	  // Write the sequence control word with enable bit set for both sequences
	  current_seqa_ctrl |= 1U<<ADC_SEQ_ENA;
	  LPC_ADC->SEQA_CTRL = current_seqa_ctrl;

//	  current_seqb_ctrl |= 1U<<ADC_SEQ_ENA;
//	  LPC_ADC->SEQB_CTRL = current_seqb_ctrl;

	  // Enable ADC interrupts in the NVIC
	  NVIC_EnableIRQ(ADC_SEQA_IRQn);

//	  seqa_handshake = True;		// To get ADC_sample() to start
//	  seqa_handshake = TTrue;		// To get ADC_sample() to start
	  //	  NVIC_EnableIRQ(ADC_SEQB_IRQn);



	  // Launch sequence A with software
	  current_seqa_ctrl |= 1<<ADC_START;
	  LPC_ADC->SEQA_CTRL = current_seqa_ctrl;
//	  seqa_handshake = FFalse;
	  seqa_handshake = TTrue;

//#define MH_TEST_ADC
#ifdef MH_TEST_ADC
	  Hub_watchdog_active = false;
	  uint32_t systick_secs = Systick_secs;
	  // The main loop
	  while(1)
	  {
		 while(systick_secs == Systick_secs);
		 systick_secs = Systick_secs;

#ifdef MH_XXX
		 wait_ms(1000);
	      seqa_handshake = False;

	      // Launch sequence A with software
	      current_seqa_ctrl |= 1<<ADC_START;
	      LPC_ADC->SEQA_CTRL = current_seqa_ctrl;
	      // Wait for return from ISR
	      while (!seqa_handshake);
#endif

//	      uint32_t val = Adc_val;
	      uint32_t tot = Adc_accel_tot;
	      uint32_t adc_count = Adc_count;
	      Adc_accel_tot = 0;
	      Adc_count = 0;
	      uint32_t accel_avg = tot / adc_count;
	      uint32_t timer_count = Ctimer_count;
	      printf("accel_avg=%d, Adc_count=%d,Ctimer_count=%d\r\n",accel_avg,adc_count,timer_count);


	      uint32_t v = (Adc_voltage + 60)/120;
//	      PRINTF("Adc_accel_val = %d\r\n",Adc_accel_val);
	      PRINTF("Adc_voltage = %d, v = %d\r\n",Adc_voltage,v);

	  }
#endif
}
//----------------------------------------------------------------------------------------------
int ADC_voltage_tot;
uint8_t ADC_voltage_count;
uint32_t ADC_systick;
uint32_t ADC_systick_start;
//uint8_t ADC_low_count;
bool ADC_voltage_disabled;
void ADC_sample(void){};		// Dummy as we move to sample2
void ADC_sample2(void)
{
	if(seqa_handshake == FFalse) return;

	ADC_voltage_tot += Adc_voltage;
	ADC_voltage_count++;
	if(ADC_voltage_count >= 8)	// This logic to smooth voltage
	{
		int voltage_average = ADC_voltage_tot >> 3;	// /8
		ADC_adjusted_voltage = (uint8_t)(voltage_average >> 4);		// Divide by 16, as max of 4096 / 16 = 256 which fits in a byte
		ADC_voltage_tot = 0;
		ADC_voltage_count = 0;
//		PRINTF("V:%d\r\n",ADC_adjusted_voltage);
//		PRINTF("V:%d\r\n",Adc_voltage);
	}
#ifdef MH_XXX
	if(ADC_systick != Systick_tick)
	{
		ADC_systick = Systick_tick;
		PRINTF("V:%d\r\n",Adc_voltage);
	}
#endif
	if(Adc_voltage < Enc_misc.adc_corrected_min_voltage)
	{
		if(ADC_voltage_disabled == false)
		{
//			if(ADC_low_count++ > 2)
			if(ADC_systick)
			{
				ADC_voltage_disabled = true;
				int pos = Enc_Pos.pos;
				if(Motor_enabled)
				{
					Disable_Motor(600);
					ADC_systick_start = Systick_tick;
					L2PRINTF("Vlow:%d,Disable Motor, pos=%d,tick=%d\r\n",Adc_voltage,pos,ADC_systick);
				}
				else
				{
					L2PRINTF("Vlow:%d,No disable, pos=%d,tick=%d\r\n",Adc_voltage,pos,ADC_systick);
					ADC_systick_start = 0;
				}
			}
		}
	}
	else
	{
//		ADC_low_count = 0;
		ADC_voltage_disabled = false;
	}
	if(ADC_systick != Systick_tick)
	{
		ADC_systick = Systick_tick;
		if(ADC_voltage_disabled)
		{
			if(ADC_systick_start)
			{
				if((Systick_tick - ADC_systick_start) > 1)
				{
					Enc_adjust_pos_when_low_voltage();
					Put_encoder_pos(150);
//					L2PRINTF("Vlow:Adjust pos,rpm = %d\r\n");
					ADC_systick_start = 0;
				}
			}
		}
	}

	if(ADC_voltage_disabled)
	{
//		PRINTF("P250:%d\r\n",Enc_Pos.pos);
		Put_encoder_pos(250);
	}

//	uint32_t v = ((Adc_voltage)>>4);	// Divide by 16, as max of 4096 / 16 = 256 which fits in a byte
//	ADC_adjusted_voltage = (uint8_t)v;


	ADC_temp_tot += Adc_temp;
	ADC_temp_count++;
	if(ADC_temp_count >= 64)	// MHH:11/04/2023
	{
		ADC_avg_temp = (ADC_temp_tot / 64);
//		PRINTF("T:%d,",ADC_avg_temp);
		ADC_temp_count = 0;
		ADC_temp_tot = 0;
	}

//	v = ((Adc_temp)>>4);	// Divide by 16, as max of 4096 / 16 = 256 which fits in a byte
//	ADC_adjusted_temp = (uint8_t)v;
//	PRINTF("T:%d,",Adc_temp);

	// Note: Will do temperature here too.

	seqa_handshake = FFalse;

    // Launch sequence A with software
    current_seqa_ctrl |= 1<<ADC_START;
    LPC_ADC->SEQA_CTRL = current_seqa_ctrl;

}


