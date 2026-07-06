/*
 * board.c
 *
 *  Created on: 19/05/2018
 *      Author: Murray
 */

/*
 *  Similar to board.c for LPC1768 routines
 */
#include "LPC8xx.h"
#include "syscon.h"
#include "iocon.h"

#include "board.h"

#include "AC300_hub.h"

void Config_Output_Pin(uint32_t port)
{
    if (port <= 31) {
      LPC_SYSCON->SYSAHBCLKCTRL[0] |= GPIO0;   // Turn on clock to GPIO0
//      LPC_GPIO_PORT->SET[0] = 1<<port;         // Make the port bit an output driving '1'
      LPC_GPIO_PORT->DIRSET[0]= 1<<port;
    }
    else if (port <= 63) {
      LPC_SYSCON->SYSAHBCLKCTRL[0] |= GPIO1;   // Turn on clock to GPIO1
//      LPC_GPIO_PORT->SET[1] = 1<<(port-32);    // Make the port bit an output driving '1'
      LPC_GPIO_PORT->DIRSET[1]= 1<<(port-32);
    }
}

void Config_Input_Pin(uint32_t port)
{
    if (port <= 31) {
      LPC_SYSCON->SYSAHBCLKCTRL[0] |= GPIO0;   // Turn on clock to GPIO0
//      LPC_GPIO_PORT->SET[0] = 1<<port;         // Make the port bit an output driving '1'
      LPC_GPIO_PORT->DIRCLR[0]= 1<<port;
    }
    else if (port <= 63) {
      LPC_SYSCON->SYSAHBCLKCTRL[0] |= GPIO1;   // Turn on clock to GPIO1
//      LPC_GPIO_PORT->SET[1] = 1<<(port-32);    // Make the port bit an output driving '1'
      LPC_GPIO_PORT->DIRCLR[1]= 1<<(port-32);
    }
}
//****************************************************************************
// Function name: LED_On. Makes a port bit drive '1' (for LED off).
//****************************************************************************
void Pin_On(uint32_t port)
{
	  if (port <= 31) {
	    LPC_GPIO_PORT->SET[0] = 1<<port;
	  }
	  else if (port <= 63) {
	    LPC_GPIO_PORT->SET[1] = 1<<(port-32);
	  }
}


//****************************************************************************
// Function name: LED_Off. Makes a port bit drive '0' (for LED off).
//****************************************************************************
void Pin_Off(uint32_t port)
{
    if (port <= 31) {
      LPC_GPIO_PORT->CLR[0] = 1<<port;
    }
    else if (port <= 63) {
      LPC_GPIO_PORT->CLR[1] = 1<<(port-32);
    }

}
//----------------------------------------------------------------------------
uint32_t Pin_Read(uint32_t port)
{
	uint32_t rv;
	if(port <= 31)
	{
		rv = ((LPC_GPIO_PORT->PIN[0]) >> port) & 1;
	}
	else
	{
		if(port <= 63)
		{
			rv = ((LPC_GPIO_PORT->PIN[1]) >> (port-32)) & 1;
		}
	}
	return rv;
}
//----------------------------------------------------------------------------
int Board_pin_read(uint32_t pin_def)
{
	return (Pin_Read(pin_def));
}
//---------------------------------------------------------------------------
void Board_pin_write(uint32_t pin_def,bool pinval)
{
	if(pinval)
	{
		Pin_On(pin_def);
	}
	else
	{
		Pin_Off(pin_def);
	}
}
//===================================================================================================
void Sig100_HDC_Set(bool On)
{
	Board_pin_write(SIG100_HDC_PIN,On);
}
//====================================================================================================
/* Sets the state of a board LED to on or off */
void Board_LED_Set(uint32_t port_def, bool On)
{
	Board_pin_write(port_def,On);
}

/* Returns the current state of a board LED */
bool Board_LED_Test(uint32_t pin_def)
{
	return Pin_Read(pin_def);
}

void Board_LED_Toggle(uint32_t pindef)
{
	Board_LED_Set(pindef, !Board_LED_Test(pindef));
}
//=====================================================================================================
uint8_t Hub_hardware_version;
bool Hub_maxon_driver;
void Board_turn_heater_off(void)
{
//#ifndef MH_DIAGS
//	if(Flog_diags_enabled) return;
#ifndef MH_FLASH_RECORD
	if(Hub_hardware_version == 0) return;	// MHH:06/09/2023.

	if(Board_pin_read(HEATER_PIN) == false && Board_pin_read(HEATER2_PIN) == true) return;

	Dputs("Heater OFF\r\n");
	Board_pin_write(HEATER_PIN,false);
	Config_Input_Pin(HEATER2_PIN);
#endif
	// MHH:02/09/2023. May be losing pullup mode doing this, but seems to work when Rdis=2K. Note Heater disable not working when Rdis=10K.
//#endif
}
void Board_turn_heater_on(void)
{
//#ifndef MH_DIAGS
//	if(Flog_diags_enabled) return;
#ifndef MH_FLASH_RECORD
	if(Hub_hardware_version == 0) return;	// MHH:06/09/2023.
	Dputs("Heater ON\r\n");
	Board_pin_write(HEATER_PIN,true);
	Config_Output_Pin(HEATER2_PIN);
	Board_pin_write(HEATER2_PIN,false);
#endif
//#endif
}

void interrupts_init(void)	// MHH:26/06/2026
{
//   return;

	// Set GPIO IRQ to higher priority (numerically lower) Used by Hall effect pins
    NVIC_SetPriority(PININT0_IRQn,    0);   // 0 = highest priority
    NVIC_SetPriority(PININT1_IRQn,    0);   // 0 = highest priority
    NVIC_SetPriority(PININT2_IRQn,    0);   // 0 = highest priority

    NVIC_SetPriority(CTIMER0_IRQn,    1);
    NVIC_SetPriority(ADC_SEQA_IRQn,   3);

    // Set RIT IRQ to lower priority (numerically higher)
    NVIC_SetPriority(UART0_IRQn,2);		// SIG100
    NVIC_SetPriority(UART1_IRQn,3);		// Debug serial
}
void Board_init(void)
{
	LPC_SYSCON->SYSAHBCLKCTRL[0] |= IOCON;		// Enable IOCON. Note: Could turn off to conserve power

	Hub_maxon_driver = Board_pin_read(HARDWARE_MAXON_PIN);

// MHH:23/11/2025. Pull down DIG1_DIG2. If Maxon control enabled PCB then the Maxon controller should pull up the pin, so we will know it is enabled.
	if(Hub_maxon_driver)
	{
		LPC_IOCON->DIG1_DIG2_MODE &= (IOCON_MODE_MASK);	// Clear mode field
		LPC_IOCON->DIG1_DIG2_MODE |= MODE_PULLDOWN;  // Pull-down, DIG1_DIG2_PIN
	}
	else
	{
		Config_Output_Pin(NSLEEP_PIN);
		Pin_On(NSLEEP_PIN);		// MHH:10/06/2026. Will have to reverse if we get rid of NPN
	}
	//	LPC_IOCON->PIO0_10         &= (IOCON_MODE_MASK | MODE_INACTIVE);  // Disable pull-up and pull-down, READY pin
	LPC_IOCON->READY_MODE         &= (IOCON_MODE_MASK | MODE_INACTIVE);  // Disable pull-up and pull-down, READY pin

//	Config_Output_Pin(READY_PIN);
//	Board_pin_write(READY_PIN,true);	// Just to test ISR PULLDOWN


/*
#define HALL_1_PIN				P0_22		// Yellow wire, LPCXpresso pin 7, LPC845 Pin 43
#define HALL_2_PIN				P0_30		// Brown wire, LPCXpresso Pin 6, LPC845 Pin 42
#define HALL_3_PIN				P0_23		// Grey wire, LPCXpresso Pin 5, LPC845 Pin 39
 *
 *
 */

/*	 These are pulled up by default, and anyway think logic wrong...
	LPC_IOCON->PIO0_22         &= (IOCON_MODE_MASK | MODE_PULLUP);  // Pull up H1
	LPC_IOCON->PIO0_30         &= (IOCON_MODE_MASK | MODE_PULLUP);  // Pull up H2
	LPC_IOCON->PIO0_23         &= (IOCON_MODE_MASK | MODE_PULLUP);  // Pull up H3
*/

	/*
	 *  MHH:31/08/203. Hardware version pins. Invert values to get version. Eg P1_8 grounded = version 1 (other pins pulled up).
	 *  Note that the internal hardware version is not the same as the PCB version. Internal hardware version should only be changed when
	 *  there is change to PCB hardware logic.
	 */
	int v1 = (Board_pin_read(HARDWARE_VERSION_1_PIN) == 0);
	int v2 = (Board_pin_read(HARDWARE_VERSION_2_PIN) == 0);
	int v3 = (Board_pin_read(HARDWARE_VERSION_3_PIN) == 0);

	Hub_hardware_version = v1 + (v2 << 1) + (v3 << 2);

#ifdef MH_MCT8316_DEBUG2
	if(Hub_maxon_driver == false)	// MHH:13/06/2026 Test!!!!. Test board returning zero!!!!
	{
		if(Hub_hardware_version == 0) Hub_hardware_version = 1;
	}
#endif

	Config_Output_Pin(ENABLE_PIN);	// Enable pin referenced in Enc_Init(), so ini first
	Board_pin_write(ENABLE_PIN,true);	// Disable
	Config_Output_Pin(ENABLE_PIN2);	// and, from Hub7 board, a second enable pin.
	Board_pin_write(ENABLE_PIN2,true);	// Disable

	if(Hub_hardware_version == 0)
	{
		Config_Output_Pin(OVERRIDE_FINE_MS_PIN);			// As Fine MS high disables ENABLE, this should allow override when on
		Board_pin_write(OVERRIDE_FINE_MS_PIN,false);		// initialise
	}
/*
 *  Note: From Hub_hardware_version = 1, we will have on state for OVERRIDE_FINE_MS_PIN represented by read mode pullup,
 *        and off state by write mode zero.
 */

	Config_Output_Pin(DIR_PIN);

	/*
	 *  MHH:31/08/2023. With Hub_hardware_version zero, the heater could be on when pins were in default mode (pulled up), which meant when loading program via debugger
	 *  or when in debug before starting.
	 *
	 *  To fix this, HEATER_PIN (P0_14) needs to be on (1) to heat, and HEATER_2_PIN (P0_1) needs to be off (0). Because there is no resistor for HEATER2_PIN, the
	 *  ON state is represented by READ mode pullup.
	 *
	 */

//	if(Flog_diags_enabled == false)
#ifndef MH_FLASH_RECORD
	{
		Config_Output_Pin(HEATER_PIN);
		Board_pin_write(HEATER_PIN,false);	// To be sure...
	}
#endif
	if(Hub_hardware_version == 0)
	{
//#ifndef MH_DIAGS

//		if(Flog_diags_enabled == false)
#ifndef MH_FLASH_RECORD
		{
			Config_Output_Pin(HEATER2_PIN);
			Board_pin_write(HEATER2_PIN,false);	// MHH:06/09/2023
		}
#endif
//#endif
		int dig1_dig2_pin = Board_pin_read(DIG1_DIG2_PIN);
		//	PRINTF("Dig1_dig2_pin = %d,\r\n",dig1_dig2_pin);


			if(dig1_dig2_pin == 1)
			{
				Speed_opamp = true;
				Config_Output_Pin(DIG1_DIG2_PIN);

#define MH_FORCE_MAXON_SPEED

#ifdef MH_FORCE_MAXON_SPEED
				Speed_raw_mode = false;		// from board revision 15..
				Board_pin_write(DIG1_DIG2_PIN,true);	// Maxon speed mode
#else
				Speed_raw_mode = true;		// from board revision 15..
				Board_pin_write(DIG1_DIG2_PIN,false);	// Try putting in raw mode...
		//		PRINTF("Dig1_dig2 = 1, so Speed_raw_mode = true\r\n");
#endif
			}

			Speed_raw_mode = false;		// Test only, so we can see how hub reacts to a hard stop under maxon speed control
	}
#ifdef MH_XXX	// MHH:19/11/2025
	else
	{
		Speed_opamp = true;
		Speed_raw_mode = true;
		Config_Output_Pin(DIG1_DIG2_PIN);		// MHH:18/11/2025
		Board_pin_write(DIG1_DIG2_PIN,false);	// Raw speed mode

	}
#endif

	interrupts_init();				// MHH:25/06/2026
}

