/*
 * junk.c
 *
 *  Created on: 20/03/2023
 *      Author: OEM
 */

#ifdef MH_JUNK
//--------------------------------------------------------------------------------------------
#ifdef MH_DDD
void Get_command(void)
{
	int ci;
	Command_len = 0;
	Calibration_command = false;
//	for(int i=0;i<10;i++) Command_string[i]=0;		// Debug...
	for(;;)
	{
		ci = Sig100_getc_timeout(2);
//		ci = Sig100_UARTGetChar();	// Currently using 'no wait". Consider using wait one millisec
		if(ci == EOF)
		{
			break;
		}
		if(Command_len < COMMAND_MAX_LEN)
		{
			Command_string[Command_len++] = (char)ci;
		}
	}
	if(Command_len >= COMMAND_MAX_LEN) Command_len = COMMAND_MAX_LEN - 1;	// min(len,max_len-1)

	Command_string[Command_len] = 0;	// null terminate string.

	Arg1 = 0;
	Arg2 = 0;
	Command = 0;
	if(Command_len == 0)
	{
		return;
	}
	Command = Command_string[0];

	if(Command == 'P' || Command == 'p')
	{
		if(Command_len != 5)
		{
			Command = 0;
		}
		return;
	}
	/*
	if(Command_len == 1)
	{
		return;
	}
	*/
	if(Command != '!')
	{
		if(Command_len > 1)	Arg1 = Command_string[1];
		return;
	}



// If we drop through then we have a special command
	uint8_t type = Command_string[1];

	switch(type)
	{
	case 'C':		// Command mode?
//		Process_AT_data();
		Check_command_mode();	// May not come back from this...
		return;

	case 'i':
		Process_i_data();
		return;

	case 'j':
		Process_j_data();
		return;
#ifdef MH_BBB
	case 'k':
		Process_k_data();
		return;
#endif
	case 'X':
		// Calibration command
		Process_X_data();
		return;

	default:	// Only here if bad command
		Command = NO_COMMAND;
		return;
	}
}
#endif

//------------------------------------------------------------------------------------------------------------------
#ifdef MH_BBB
#define SIG100_k_PKT_LEN		4
void Process_k_data(void)
{
	Board_UARTPutchar_RB('k');

	if(Command_len < SIG100_k_PKT_LEN) return;

	if(Checksum_command_pkt(SIG100_k_PKT_LEN) == false)
	{
		return;
	}
	if(Command_string[2] != 'K') return;

	/*
	 *	// OK, going to break the rules here and wait for a response.
	// Expect header of ">j"
	// Then "{ndddd...cccc}
	// where n = number of bytes expected, including crc32
	// d = data bytes
	// cccc = 4 byte crc32
	 *
	 *
	 *
	 */

	Sig100_UARTPutChar('>');
	Sig100_UARTPutChar('k');
	Sig100_UARTPutChar('{');

	uint8_t *data_p = (uint8_t *)&Enc_data.magic3;
	int data_size = 64;

	uint32_t crc32;
	size_t crc32_size = sizeof(crc32);
	uint8_t c = (uint8_t) (data_size + crc32_size);	// Should be 68
	Sig100_UARTPutChar((char)c);		// Should not exceed 255

	Sig100_send_binary_data(data_p,data_size);

	crc32 = Wiki_CRC32(data_p,data_size);
	data_p = (uint8_t *)&crc32;	// Now send crc32
	Sig100_send_binary_data(data_p,crc32_size);
	Sig100_UARTPutChar('}');

}
#endif
//--------------------------------------------------------------------------------------------
//void LPC845_reboot(void);
//void LPC845_flash(void);
#ifdef MH_XXX
void Hub_calibrate(void)
{
	PRINTF("Hub_calibrate:\r\n");

	/* First move to coarse physical limit, ignoring any stops, but getting ready stop if reaches limit.
	 * We will need to control speed and also monitor speed. Start by moving to the fine stop, so we know where we are.
	 */
	PRINTF("Moving to FINE stop...\r\n");

	for(;;)
	{
		if(Board_pin_read(FINE_MICROSWITCH_PIN)) Enc_fine_microswitch_on = true;
		if(Enc_fine_microswitch_on) break;
		Motor_mode = FINE;
		Set_Direction_Fine();	// CW
		Set_Motor_RPM(8000);
		wait_ms(50);
	}
	PRINTF("At fine stop, Enc_pos:%d\r\n",Enc_pos);

	PRINTF("Moving to COARSE hard stop...\r\n");
	Enc_calibrate_rpm = 0;
	Enc_calibrate_limit = false;

	for(int i=0;;i++)
	{
		if(Enc_calibrate_limit) break;
		if(Enc_calibrate_rpm != 1000)
		{
			if(Enc_rpm > Enc_calibrate_rpm) Enc_calibrate_rpm = Enc_rpm;
			if(Enc_calibrate_rpm > 1000) Enc_calibrate_rpm = 1000;
		}
		Motor_mode = CALIBRATE;
		Set_Direction_Coarse();	// CW
		Set_Motor_RPM(2000);		// May have to adjust this for pole pairs??
		wait_ms(50);
		if((i & 7) == 0)
		{
			PRINTF("Enc_pos:%d,Enc_rpm:%d\r\n",Enc_pos,Enc_rpm);
		}
	}
	PRINTF("At COARSE hard stop, Enc_pos:%d\r\n",Enc_pos);

}
#endif
#ifdef MH_AAA
void Process_J_data(void)
{

//	add actual coarse stop
	/*
	 *  *  Packet 'J' return
	 *
	 *  	0			'>' Header
	 *  	1			'J'
	 *  	2-3			actual coarse stop, return only
	 *  	4-5			Hub ID
	 *  	6			Hub software revision
	 *      7			checksum
	 *
	 */

	Board_UARTPutchar_RB('J');

	Command_string[0] = '>';

	int16_t fine_stop = Enc_data.cal_fine_stop;
	PRINTF(">%d;",fine_stop);
	Put_int2(Command_string+2,fine_stop);
	Put_int2(Command_string+4,Enc_data.hub_id);
	Command_string[6] = HUB_SOFTWARE_REVISION;
	Checksum_and_send_cmd_pkt(SIG100_J_PKT_LEN);
}
#endif
#ifdef MH_YYY	// This is now provided by Calibrator
	Enc_data.user_defined_stops = Command_string[2];
//	Enc_data.pole_pairs = Command_string[3];
	Enc_data.ud_coarse_stop = Get_int16(Command_string+4);
	Enc_data.ud_feather_stop = Get_int16(Command_string+6);
	Enc_data.ud_reverse_stop = Get_int16(Command_string+8);
//	Enc_data.motor_ratio = Get_int16(Command_string+10);
#endif
#ifdef MH_AAA
void Process_I_data(void)
{
	Board_UARTPutchar_RB('I');

	if(Command_len < SIG100_I_PKT_LEN) return;

	if(Checksum_command_pkt(SIG100_I_PKT_LEN) == false)
	{
		return;
	}
#ifdef MH_YYY	// This is now provided by Calibrator
	Enc_data.user_defined_stops = Command_string[2];
//	Enc_data.pole_pairs = Command_string[3];
	Enc_data.ud_coarse_stop = Get_int16(Command_string+4);
	Enc_data.ud_feather_stop = Get_int16(Command_string+6);
	Enc_data.ud_reverse_stop = Get_int16(Command_string+8);
//	Enc_data.motor_ratio = Get_int16(Command_string+10);
#endif
	// Going to reply immediately with same packet, just change first byte
	Command_string[0] = '>';

	Checksum_and_send_cmd_pkt(SIG100_I_PKT_LEN);

	ac200Enc_SSP_save_ini_data();
}
#endif
#ifdef MH_AAA
		if(Enc_systick_check_fine_fall)
		{
			if(fine_stop_pin)	// FINE stop pin still low?
			{
				Enc_systick_check_fine_fall = false;	// No, forget it
			}
			else
			{
				if(Enc_fine_fall_stop_ticks++ > 20)	// More than 20 ticks elapsed?
				{
					Enc_update_fine_fall_stop = true;	// Yes, update fine fall position
					Enc_systick_check_fine_fall = false;	// and stop checking
				}
			}
		}
#endif
#ifdef MH_DDD
	for(;;)
	{
		ci = Sig100_getc_timeout(2);
//		ci = Sig100_UARTGetChar();	// Currently using 'no wait". Consider using wait one millisec
		if(ci == EOF)
		{
			break;
		}
		if(Command_len < COMMAND_MAX_LEN)
		{
			Command_string[Command_len++] = (char)ci;
		}
	}
	if(Command_len >= COMMAND_MAX_LEN) Command_len = COMMAND_MAX_LEN - 1;	// min(len,max_len-1)

	Command_string[Command_len] = 0;	// null terminate string.

	Arg1 = 0;
	Arg2 = 0;
	Command = 0;
	if(Command_len == 0)
	{
		return;
	}
	Command = Command_string[0];

	if(Command == 'P' || Command == 'p')
	{
		if(Command_len != 5)
		{
			Command = 0;
		}
		return;
	}
	/*
	if(Command_len == 1)
	{
		return;
	}
	*/
	if(Command != '!')
	{
		if(Command_len > 1)	Arg1 = Command_string[1];
		return;
	}



// If we drop through then we have a special command
	uint8_t type = Command_string[1];

	switch(type)
	{
	case 'C':		// Command mode?
//		Process_AT_data();
		Check_command_mode();	// May not come back from this...
		return;

	case 'i':
		Process_i_data();
		return;

	case 'j':
		Process_j_data();
		return;
#ifdef MH_BBB
	case 'k':
		Process_k_data();
		return;
#endif
	case 'X':
		// Calibration command
		Process_X_data();
		return;

	default:	// Only here if bad command
		Command = NO_COMMAND;
		return;
	}
#endif
#ifdef MH_AAA
	if(Enc_update_fine_fall_stop)
	{
		Enc_update_fine_fall_stop = false;
		int diff = Enc_fine_ms_pos_fall - Enc_data.cal_fine_stop;
//		if(diff < -1 || diff >1)
//		if(Enc_fine_ms_pos_fall < -1 || Enc_fine_ms_pos_fall >1)
		if(abs(diff) > Enc_data.cal_tolerance)		// equivalent to 0.05 mm, calculated by Calibrator
		{

// 			Disable_Motor();		// DEBUG!!


			char c = '%';
			if(Enc_fine_microswitch_pos_set)	// Have we already set it in this run?
			{
				c = '?';		// Should NOT need to reset if have already done it this run!!
			}
//			PRINTF("%c Fine fall pos update:%d\r\n",c,Enc_fine_ms_pos_fall);
			PRINTF("%c Fine fall pos update:%d\r\n",c,diff);
//			Enc_pos -= Enc_fine_ms_pos_fall;		// adjust encoder position
			Enc_pos -= diff;		// adjust encoder position
			ac200Enc_SSP_save_pos();
		}
		Enc_fine_microswitch_pos_set = true;	// Set or verified
	}
#endif



	//	  Sig100_puts("Sig100_puts\r\n");

		Config_Output_Pin(SIG100_HDC_PIN);
		Sig100_HDC_Set(true);
		return;



		Sig100_HDC_Set(false);
		wait_us(1);
	#ifdef MH_SET_19200
		Sig100_UARTPutChar(0xf5);	// Tell SIG100 to write register 0 data
		Sig100_UARTPutChar(0);
	//	Sig100_UARTPutChar(0x28);		// Set register 0 to 0x28 (9600 baud)
		Sig100_UARTPutChar(0x2a);		// Set register 0 to 0x2a (19200 baud). 9/11/2022
		wait_us(1);
	#endif
		int sig100_register;
		for(int r=0;r<7;r++)
		{
	//		PRINTF("Trying to read SIG100 register:%d\r\n",r);
			char rc = (char)r;
			Sig100_UARTPutChar(0xfd);	// Tell SIG100 to send register[rc] data
			Sig100_UARTPutChar(rc);
			for(int i=0;i<100000;i++)
			{
				wait_us(1);
			    sig100_register = Sig100_getc_nowait();
			    if(sig100_register != EOF) break;
			 }
			 if(sig100_register == EOF)
			 {
				 PRINTF("Failed to read register: %d\r\n",r);
			     break;
			 }
			 Sig100_register[r] = sig100_register;
			 wait_us(1);
		}
		Sig100_HDC_Set(true);
		wait_us(1);
		for(int r=0;r<7;r++)
		{
	//		PRINTF("Register[%d]:%02x\r\n",r,Sig100_register[r]);
		}
#ifdef MH_ADC_PIN_INT
	  // Movable digital functions enabled

	  // Make the ouput ports GPIO outputs driving '1', connect to the input ports externally.
	  // Make the input ports GPIO inputs.
	  LPC_GPIO_PORT->SET0  = (1<<OUTPORT_B)|(1<<OUTPORT_A);  // Output GPIOs initially driving '1'
	  LPC_GPIO_PORT->DIR0 |= (1<<OUTPORT_B)|(1<<OUTPORT_A);  // to output mode
	  LPC_GPIO_PORT->DIR0 &= ~((1<<INPORT_B)|(1<<INPORT_A)); // Input GPIOs to input mode

	  // Assign the ADC pin trigger functions to the input port pins
	  // For LPC82x and LPC83x : ADC pin triggers are movable digital functions assignable through the SWM
	  // For LPC84x (thank you, side Birns) :  ADC and DAC pin triggers driven by GPIO_INT
	  // Configure pinint0_irq and pinint1_irq for high level sensitive, then assign them in seq_ctrl[14:12]
	  // Important Note! We must configure the pin interrupts to generate the ADC pin triggers, but we don't use the pin interrupts as interrupts.

	  // Assign the input ports to be pin interrupts 1, 0 by writing to the PINTSELs in SYSCON
	  LPC_SYSCON->PINTSEL[1] = INPORT_B;
	  LPC_SYSCON->PINTSEL[0] = INPORT_A;

	  // Configure the pin interrupt mode register (a.k.a ISEL) for level-sensitive on PINTSEL1,0 ('1' = level-sensitive)
	  LPC_PIN_INT->ISEL |= 0x3; // level-sensitive

	  // Configure the active level for PINTSEL1,0 to active-high ('1' = active-high)
	  LPC_PIN_INT->IENF |= 0x3;  // active high

	  // Enable interrupt generation for PINTSEL1,0
	  LPC_PIN_INT->SIENR = 0x3;
#endif
	  //#define MH_TEMPERATURE_KLUGE	// 05/12/2022
	  #ifdef MH_TEMPERATURE_KLUGE
	  	Adc_temp = 2930;		// Had sensor back to front, but cannot re-solder a new one. Use this kluge until put surface mount version on.
	  #endif
#ifdef MH_AAA
	if(Enc_ud_coarse_stop)
	{
		if(Enc_data.ud_coarse_stop > 0)	// MHH:07/02/2023. Ignore if less than fine stop
		{
			Enc_coarse_limit_reached = (Enc_pos > Enc_data.ud_coarse_stop); // Normally set in Enc_ISR but in case passed stop and not moving
		}
	}

#else
	if(Enc_ud_coarse_stop)	// What happens if we don't define coarse or feather stop? May be useful for testing.
	{
		Enc_coarse_limit_reached = (Enc_pos > Enc_data.ud_coarse_stop); // Normally set in Enc_ISR but in case passed stop and not moving
	}
#endif
#ifdef MH_AAA
	else	// This not used because now using coarse stop pin as reference pin.
	{
		coarse_stop_pin = Board_pin_read(COARSE_MICROSWITCH_PIN);

		if(Enc_coarse_microswitch_on != coarse_stop_pin)	// new value?
		{
			if(Systick_coarse_tick_count++ > 10)
			{
				Enc_coarse_microswitch_on = coarse_stop_pin;
				Enc_coarse_limit_reached = Enc_coarse_microswitch_on;
				if(Enc_coarse_limit_reached && Motor_mode == COARSE)	// Not for feather
				{
					Disable_Motor();		// MHH:30/11/2022
				}
			}
		}
		else
		{
			Systick_coarse_tick_count = 0;
		}

/*
		if(Enc_systick_check_coarse_stop)
		{
			if(coarse_stop_pin == false)	// Coarse stop pin still high?
			{
				Enc_systick_check_coarse_stop = false;	// No, forget it
			}
			else
			{
				if(Enc_coarse_stop_ticks++ > 20)	// More than 20 ticks elapsed?
				{
					Enc_update_coarse_stop = true;	// Yes, update coarse stop position
					Enc_systick_check_coarse_stop = false;	// and stop checking
				}
			}
		}
*/
	}
#endif
#ifdef MH_AAA
void ac200Enc_SSP_save_ini_data(void)
{
	ac200_Enc_set_ud_flags();

	uint8_t *p_start_data = (uint8_t *)&Enc_data.user_defined_stops;
	uint8_t *p_end_data = (uint8_t *)&Enc_data.ud_reverse_stop;

	uint32_t pos = p_start_data - (uint8_t *) &Enc_data;
	uint32_t len = p_end_data - p_start_data;

	ac200Enc_ssp_raw_mram_write(pos,p_start_data,len);

}
#endif
#ifdef MH_OLD_LOGIC
void ac200Enc_SSP_save_stop_flags(uint8_t user_stops)
{
	if(user_stops == Enc_data.user_defined_stops)	// But should have been set on page zero read?
	{
		ac200_Enc_set_ud_flags();
		return;
	}

	Enc_data.user_defined_stops = user_stops;
	ac200_Enc_set_ud_flags();

	uint8_t *p_data = (uint8_t *)&Enc_data.user_defined_stops;
	uint32_t pos = p_data - (uint8_t *) &Enc_data;
	ac200Enc_ssp_raw_mram_write(pos,p_data,1);
}
#endif
#ifdef MH_AAA
void ac200Enc_SSP_save_ud_coarse_stop(uint16_t ud_coarse_stop)
{
	if(ud_coarse_stop == Enc_data.ud_coarse_stop) return;	// Need to update?

	Enc_data.ud_coarse_stop = ud_coarse_stop;

	uint8_t *p_data = (uint8_t *)&Enc_data.ud_coarse_stop;
	uint32_t pos = p_data - (uint8_t *) &Enc_data;
	ac200Enc_ssp_raw_mram_write(pos,p_data,2);
}
#endif
/*
void ac200Enc_SSP_save_coarse_stop(int coarse_stop)
{
	if(coarse_stop == Enc_data.coarse_stop) return;	// Need to update?

	PRINTF("ssp_save_coarse:%d\r\n",coarse_stop);
#ifdef MH_CHECK_RANGE
	if(coarse_stop < 340 || coarse_stop > 400)
	{
		PRINTF("COARSE_STOP out of range, not updating\r\n");
		return;
	}
#endif
	Enc_data.coarse_stop = coarse_stop;

	uint8_t *p_data = (uint8_t *)&Enc_data.coarse_stop;
	uint32_t pos = p_data - (uint8_t *) &Enc_data;
	ac200Enc_ssp_raw_mram_write(pos,p_data,2);
}
*/

//------------------------------------------------------------------------------
#ifdef MH_FLASH_TEST
static uint8_t TestWriteBuffer[SSP_BUFF_SIZE+2],TestReadBuffer[SSP_BUFF_SIZE];
void SSP_Flash_Test(void)
{
	printf("SSP_Flash_Test:\n\r");
	printf("Writing at %d byte pages, starting from position %d\r\n",SSP_BUFF_SIZE,MRAM_TEST_START);

	uint32_t n=1;
	uint32_t t1 = us_ticker_read();
	for(int pg=256;pg<512;pg++)
	{
		char *bp = (char *)TestWriteBuffer;
		for(int i=0;i<32;i++)
		{
			sprintf(bp,"%8d",n);
			bp+= 8;
			n++;
		}
		SSP_WritePage(pg,256,TestWriteBuffer);
		if(pg%16 == 0) printf("\r\n");
		printf("%6d",pg);
	}
	uint32_t t2 = us_ticker_read();
	uint32_t elapsed_us = t2 - t1;
	printf("\r\nFinished writing 256 pages, elapsed = %d (ms)\r\n",elapsed_us/1000);
	printf("Now checking\r\n");

	n = 1;
	for(int pg=256;pg<512;pg++)
	{
		char *bp = (char *)TestWriteBuffer;
		for(int i=0;i<32;i++)
		{
			sprintf(bp,"%8d",n);
			bp+= 8;
			n++;
		}
		SSP_ReadPage(pg,256,TestReadBuffer);
		if(pg%16 == 0) printf("\r\n");
		printf("%6d",pg);
		if(memcmp(TestReadBuffer,TestWriteBuffer,256))
		{
			printf("\r\nPage:%d different\r\n",pg);
		}
	}
	uint32_t t3 = us_ticker_read();
	elapsed_us = t3 - t2;
	printf("\r\nFinished reading 256 pages, elapsed = %d (ms)\r\n",elapsed_us/1000);
}
#endif
//------------------------------------------------------------------------------
#define MH_TEST_SSP
#ifdef MH_TEST_SSP
static uint8_t TestWriteBuffer[128],TestReadBuffer[128];

void SSP_Mram_Test(void)
{
	PRINTF("SSP_Mram_Test:\n\r");
	PRINTF("Writing 80 byte pages, starting from position %d\r\n",MRAM_TEST_START);

	uint32_t n=1;
	uint32_t t1 = us_ticker_read();
	uint32_t pos = MRAM_TEST_START;
	for(int r=0;r<MRAM_80BYTE_PAGES;r++)
//	for(int r=0;r<5;r++)
	{
		char *bp = (char *)TestWriteBuffer;
		for(int i=0;i<10;i++)
		{
			sprintf(bp,"%8d",n);
			bp+= 8;
			n++;
		}
		ac200Enc_ssp_mram_write(pos,TestWriteBuffer,80);
		SSP_Flash_Read_Status();
//		SSP_WriteBuffer(pos,TestWriteBuffer,80);
#ifdef MH_PRINT_PROGRESS
		if(r%20 == 0) printf("\r\n");
		printf("%6d",r);
#endif
		pos += 80;
	}
	uint32_t t2 = us_ticker_read();
	uint32_t elapsed_us = t2 - t1;
	PRINTF("\r\nFinished writing test pages, elapsed = %d (ms)\r\n",elapsed_us/1000);
	PRINTF("Now checking\r\n");

	n = 1;
	pos = MRAM_TEST_START;

	for(int r=0;r<MRAM_80BYTE_PAGES;r++)
//	for(int r=0;r<5;r++)
	{
//		memset(TestWriteBuffer,0,sizeof(TestWriteBuffer));
		char *bp = (char *)TestWriteBuffer;
		for(int i=0;i<10;i++)
		{
			sprintf(bp,"%8d",n);
			bp+= 8;
			n++;
		}
		ac200Enc_ssp_mram_read(pos,TestReadBuffer,80);
//		SSP_ReadBuffer(pos,TestReadBuffer,80);
#ifdef MH_PRINT_PROGRESS
		if(r%20 == 0) printf("\r\n");
		printf("%6d",r);
#endif
		if(memcmp(TestReadBuffer,TestWriteBuffer,80))
		{
			PRINTF("\r\nBuffer:%d different\r\n",r);
		}
		pos += 80;
	}
	uint32_t t3 = us_ticker_read();
	elapsed_us = t3 - t2;
	PRINTF("\r\nFinished reading test pages, elapsed = %d (ms)\r\n",elapsed_us/1000);
}
#endif

//-------------------------------------------------------------------------------------------------
#ifdef MH_SSP_PAGE_LOGIC
static void SSP_WritePage(uint32_t page,uint32_t len, uint8_t *pBuffer)
{
//	printf("SSP_WritePage\r\n");

	SSP_Flash_Wait_Ready();
	SSP_Write_Enable();

	Tx_Buf[0] = SSP_CMD_WRITE;		// write instruction
	uint32_t byte_address = page * SSP_BUFF_SIZE;
	SSP_copy_address(byte_address);
	memcpy(Tx_Buf+SSP_COMMAND_ADDRESS_SIZE,pBuffer,len);

//	SSP_RW_Poll_Mode(SSP_COMMAND_ADDRESS_SIZE+len);
	SSP_Write_Poll_Mode(SSP_COMMAND_ADDRESS_SIZE+len);
	SSP_wait_ready = true;
//	SSP_Flash_Wait_Ready();
}
//-------------------------------------------------------------------------------------------------
static void SSP_ReadPage(uint32_t page,uint32_t len, uint8_t *pBuffer)
{
//	printf("SSP_ReadPage\r\n");

	SSP_Flash_Wait_Ready();

	Tx_Buf[0] = SSP_CMD_READ;		// read instruction
	uint32_t byte_address = page * SSP_BUFF_SIZE;
	SSP_copy_address(byte_address);
	SSP_RW_Poll_Mode(SSP_COMMAND_ADDRESS_SIZE+len);

	memcpy(pBuffer,Rx_Buf+SSP_COMMAND_ADDRESS_SIZE,len);
}
#endif


#ifdef MH_SSP_READ_WRITEBYTE
void SSP_WriteByte(uint32_t byte_address,uint8_t byte_val)
{
	printf("SSP_WriteByte\r\n");

	SSP_Write_Enable();

	Tx_Buf[0] = SSP_CMD_WRITE;		// write instruction

	SSP_copy_address(byte_address);
	Tx_Buf[4] = byte_val;
	SSP_RW_Poll_Mode(5);
	for(;;)
	{
		uint8_t sr1 = SSP_Flash_Read_Status();
		if(sr1 == 0) break;
		wait_ms(1);
	}
}
//-------------------------------------------------------------------------------------------------
uint8_t SSP_ReadByte(uint32_t byte_address)
{
	printf("SSP_ReadByte\r\n");

	Tx_Buf[0] = SSP_CMD_READ;		// read instruction

	SSP_copy_address(byte_address);
	SSP_RW_Poll_Mode(5);
	return Rx_Buf[4];	// ???
}
#endif
//-------------------------------------------------------------------------------------------------
#ifdef MH_LPC_1768
static void SSP_Write_Poll_Mode(int length)		// Test write only
{
	//	dmaChSSPTx = Chip_GPDMA_GetFreeChannel(LPC_GPDMA, GPDMA_CONN_SSP1_Tx);
	//	dmaChSSPRx = Chip_GPDMA_GetFreeChannel(LPC_GPDMA, GPDMA_CONN_SSP1_Rx);

	SS_N_LO;				// Set !CS line to LO state
	if(Chip_SSP_WriteFrames_Blocking(LPC_SSP1, Tx_Buf, length) == 0)
	{
		DebugAbort("SSP_Write_Poll_Mode: error");
	}
	SS_N_HI;				// Set !CS line to HI state
}
#else
static void SSP_Write_Poll_Mode(int length)		// Test write only
{
	SPImasterWriteOnly(Tx_Buf,length);
}
#endif
//-------------------------------------------------------------------------------------------------
#ifdef MH_LPC_1768
static void SSP_RW_Poll_Mode(int length)
{
	//	dmaChSSPTx = Chip_GPDMA_GetFreeChannel(LPC_GPDMA, GPDMA_CONN_SSP1_Tx);
	//	dmaChSSPRx = Chip_GPDMA_GetFreeChannel(LPC_GPDMA, GPDMA_CONN_SSP1_Rx);

	xf_setup.length = length;
	xf_setup.tx_data = Tx_Buf;
	xf_setup.rx_data = Rx_Buf;

	//	Buffer_Init();

	xf_setup.rx_cnt = xf_setup.tx_cnt = 0;
	SS_N_LO;				// Set !CS line to LO state
	if(Chip_SSP_RWFrames_Blocking(LPC_SSP1, &xf_setup) == 0)
	{
		DebugAbort("SSP_RW_Poll_Mode: error");
	}
	SS_N_HI;				// Set !CS line to HI state
//	printf("xf_setup.rx_cnt =%d, xf_setup.tx_cnt = %d\r\n",xf_setup.rx_cnt, xf_setup.tx_cnt);
}
#else
static void SSP_RW_Poll_Mode(int length)
{
	SPImasterWriteRead(Tx_Buf,Rx_Buf,length);
}
#endif
#ifdef MH_LPC_1768
#define LPC_SC				LPC_SYSCTL
#define PCLKSEL0			PCLKSEL[0]
#define PINSEL0				PINSEL[0]
#define	LPC_PINCON			LPC_IOCON
#define	LPC_GPIO0 			(LPC_GPIO+0)
#define FIOCLR				CLR
#define FIOSET				SET
#define SS_N_LO	LPC_GPIO0->FIOCLR |= (1<<6);	// GPIO pin P0.6 as a Slave Select (SS) pin is in the low level
#define SS_N_HI	LPC_GPIO0->FIOSET |= (1<<6);	// GPIO pin P0.6 as a Slave Select (SS) pin is in the low level

static Chip_SSP_DATA_SETUP_T xf_setup;

#endif


//#define SS_N_LO	LPC_GPIO0->FIOCLR |= (1<<16);	// GPIO pin P0.16 as a Slave Select (SS) pin is in the low level
//#define SS_N_HI	LPC_GPIO0->FIOSET |= (1<<16);	// GPIO pin P0.16 as a Slave Select (SS) pin is in the low level

//#define SS_N_LO	;	// GPIO pin P0.6 as a Slave Select (SS) pin is in the low level
//#define SS_N_HI	;	// GPIO pin P0.6 as a Slave Select (SS) pin is in the low level

#ifdef MH_WAIT_READY
	if(SSP_wait_ready == false) return;
	SSP_wait_ready = false;
	for(;;)
	{
		uint8_t sr1 = SSP_Flash_Read_Status();
		if(sr1 == 0) break;
		if(sr1 == 2) break;	// Just write enabled
		wait_ms(1);
	}
#endif
#ifdef MH_OLD_ENCODER
    int dir;

	if(Enc_H1_init) return;	// Sometimes showing error when ISR enabled

    // Could read all pins in one go.
    int h1 = Board_pin_read(HALL_1_PIN);
    int h2 = Board_pin_read(HALL_2_PIN);
    int h3 = Board_pin_read(HALL_3_PIN);

    int h123 = (h1<<2) | (h2 <<1) | h3;

    switch (h123)
	{
    default:
    	Enc_H1_error(ENC_ERROR_BAD_READ);
    	return;
    case 1:	//001
//    case 6:	//110, only on rising edge
//    	dir = DIR_COARSE;
    	dir = DIR_FINE;	// MHH:30/11/2022
    	break;

    case 2:	// 010
//    case 5:	// 101, only on rising edge
//    	dir = DIR_FINE;
    	dir = DIR_COARSE;	// MHH:30/11/2022
    	break;
	}
#endif
#ifdef MH_XXX
	if(Response_short_count++ < 5) // Normal path
	{
		Spkt[0] = '_';
		Checksum_and_send_pkt(4);
		return;
	}
#endif

#ifdef MH_ENC_ISRX
void Enc_H123_ISRx(int h123,int pin,bool rising)
{
    if(h123 == Enc_h123x)
    {
    	Dputs2("?DUPx");
    	return;
    }

    // Note: Could do a check on pin rising/falling against h123...

    int dirx=0;
    int err=0;
    switch(Enc_h123x)
    {
    default:
    	err = 1;
    	break;

    case F100:
    	if(h123 == R101)
    	{
      		dirx = DIR_FINE;
      		break;
    	}
    	if(h123 == R110)
    	{
      		dirx = DIR_COARSE;
      		break;
    	}
    	err=2;
    	break;

    case R101:
    	if(h123 == F001)
    	{
      		dirx = DIR_FINE;
      		break;
    	}
    	if(h123 == F100)
    	{
      		dirx = DIR_COARSE;
      		break;
    	}
    	err=3;
    	break;

    case F001:
    	if(h123 == R011)
    	{
      		dirx = DIR_FINE;
      		break;
    	}
    	if(h123 == R101)
    	{
      		dirx = DIR_COARSE;
      		break;
    	}
    	err=4;
    	break;

    case R011:
    	if(h123 == F010)
    	{
      		dirx = DIR_FINE;
      		break;
    	}
    	if(h123 == F001)
    	{
      		dirx = DIR_COARSE;
      		break;
    	}
    	err=5;
    	break;

    case F010:
    	if(h123 == R110)
    	{
      		dirx = DIR_FINE;
      		break;
    	}
    	if(h123 == R011)
    	{
      		dirx = DIR_COARSE;
      		break;
    	}
    	err=6;
    	break;

    case R110:
    	if(h123 == F100)
    	{
      		dirx = DIR_FINE;
      		break;
    	}
    	if(h123 == F010)
    	{
      		dirx = DIR_COARSE;
      		break;
    	}
    	err=6;
    	break;
    }
    if(err != 0)
    {
    	PRINTF("Ex:%d\r\n",err);
    	Enc_ecount++;
    	Enc_last_err = err;
    	if(err == 1) return;		// Invalid pin value
    }
    Enc_h123x = h123;
    if(err) return;

#ifdef MH_XXX
    uint32_t us_time =  us_ticker_read();
    uint32_t us_elapsed = us_time - Enc_us_time;
    Enc_us_time = us_time;
#endif
    switch(dirx)
    {
    case DIR_COARSE:
//		Enc_posx++;		// COARSE
		Enc_posx--;		// COARSE
		if(dirx != Enc_dirx)
		{
			Enc_dirx = dirx;
			Dputs2("Dx:C\r\n");
		}
		break;
    case DIR_FINE:
//		Enc_posx--;		// FINE
		Enc_posx++;		// FINE
		if(dirx != Enc_dirx)
		{
			Enc_dirx = dirx;
			Dputs2("Dx:F\r\n");
		}
		break;
    }
#ifdef MH_XXX
    Posx_ix &= 255;
    Posx_t[Posx_ix] = Enc_posx;
    Usecs_t[Posx_ix++] = us_elapsed;
    Enc_usecs = us_elapsed;
    if(Enc_sense_hardstop)
    {
        if(Enc_usecs > Enc_max_usecs)
        {
        	Enc_sense_hardstop = false;
        	Enc_hardstop_posx = Enc_posx;
        	Disable_Motor(9100);
        }
    }
#endif
}
#endif
#ifdef MH_XXX
    		posx = Enc_pos * 6;
        	if(Enc_posx_set)
        	{
        		if(Enc_posx != posx)
        		{
        			Disable_Motor(2000);
        			PRINTF("\r\n:F:Enc_posx=%d,Enc_pos=%d,posx=%d\r\n",Enc_posx,Enc_pos,posx);
//        			Dputs("\r\nEnc_posx != Enc_pos:F\r\n");
        		}
        	}
        	else
        	{
        		Enc_posx = posx;
        		Enc_h123x = h123;
        		PRINTF("\r\nEset:F:%d\r\n",posx);
        		Enc_posx_set = true;
        	}
#endif
#ifdef MH_XXX
    		posx = Enc_pos * 6 - 2;
        	if(Enc_posx_set)
        	{
        		if(Enc_posx != posx)
        		{
        			Disable_Motor(2100);
        			PRINTF("\r\n:C:Enc_posx=%d,Enc_pos=%d,posx=%d\r\n",Enc_posx,Enc_pos,posx);
//        			Dputs("\r\nEnc_posx != Enc_pos:C\r\n");
        		}
        	}
        	else
        	{
        		Enc_posx = posx;
        		Enc_h123x = h123;
        		PRINTF("\r\nEset:C:%d\r\n",posx);
        		Enc_posx_set = true;
        	}
#endif
#ifdef MH_XXX
void Hub_calibrate(void)
{
	PRINTF("Hub_calibrate:\r\n");

	Set_Direction_Coarse();
	Set_Motor_RPM(4000,510);
	Set_fine_stop_override(true);	// perhaps should turn off at end?
	int count;
	int tcount=0;
	int msecs;
//	int pos = Enc_pos;
	int enc_count = Enc_count;
	int coarse_hard_stop;
	int fine_hard_stop;
	wait_ms(200);
	Timer_reset(&Motor_timer);
	for(count=0;;count++)
	{
#ifdef MH_RPM
		if(Enc_rpm < 1000)
		{
			PRINTF("Enc_rpm = %d, exiting\r\n",Enc_rpm);
			break;
		}
#endif
		coarse_hard_stop = Enc_pos;
		msecs = Timer_read_ms(&Motor_timer);
		if(msecs >= 10)
//		if(msecs >= 5)
		{
//			if(pos == Enc_pos)
			int diff = Enc_count - enc_count;
			enc_count = Enc_count;
			Timer_reset(&Motor_timer);
#ifdef MH_DDD
			D_ix = tcount & 31;
			Diff_t[D_ix] = diff;
#endif
//			Rpm_t[D_ix] = Enc_rpm;
			tcount++;
//			if(diff <= 2)		// Could use pole pairs?
			if(diff <= Enc_data.cal_pole_pairs)		// Could use pole pairs?
			{
				PRINTF("msecs >=10, diff=%d,rpm=%d,tcount=%d,exiting\r\n",diff,Enc_rpm,tcount);
				break;
			}
		}
	}
#ifdef MH_DDD
	Disable_Motor(5100);
	return;
#endif

	int ready_pin = Board_pin_read(READY_PIN);
	Set_Direction_Fine();
	PRINTF("ready_pin:%d\r\n",ready_pin);
	if(ready_pin == 0)
	{
		PRINTF("Disabling and Enabling motor\r\n")
		Disable_Motor(5100);
		wait_ms(10);
		Enable_Motor();
	}
//	return;

//	Set_Motor_RPM(4000,510);

	PRINTF("coarse_hard_stop = %d\r\n",coarse_hard_stop);
	int fine_ms_stop = 0;
	int coarse_ms_stop = 0;
//	bool fine_microswitch_on;
	bool coarse_microswitch_on=false;
	wait_ms(1000);
	Timer_reset(&Motor_timer);
	for(count=0;;count++)
	{
#ifdef MH_RPM
		if(Enc_rpm < 2000)
		{
			PRINTF("Enc_rpm = %d, exiting\r\n",Enc_rpm);
			break;
		}
#endif
		fine_hard_stop = Enc_pos;
		if(coarse_ms_stop == 0)
		{
			if(coarse_microswitch_on)
			{
				if(Board_pin_read(COARSE_MICROSWITCH_PIN) == false)
				{
					coarse_ms_stop = Enc_pos;
				}
			}
			else
			{
				coarse_microswitch_on = Board_pin_read(COARSE_MICROSWITCH_PIN);
			}
		}
		if(fine_ms_stop == 0)
		{
			if(Board_pin_read(FINE_MICROSWITCH_PIN))
			{
				fine_ms_stop = Enc_pos;
			}
		}
		msecs = Timer_read_ms(&Motor_timer);
		if(msecs >= 10)
		{
			int diff = Enc_count - enc_count;
			enc_count = Enc_count;
			Timer_reset(&Motor_timer);
			tcount++;
			if(diff <= Enc_data.cal_pole_pairs)		// Could use pole pairs?
			{
				PRINTF("msecs >=10, diff=%d,rpm=%d,tcount=%d,exiting\r\n",diff,Enc_rpm,tcount);
				break;
			}
		}

	}
	Set_Direction_Coarse();
	ready_pin = Board_pin_read(READY_PIN);
	PRINTF("ready_pin(2):%d\r\n",ready_pin);
	if(ready_pin == 0)
	{
		PRINTF("Disabling and Enabling motor\r\n")
		Disable_Motor(5200);
		wait_ms(10);
		Enable_Motor();
	}

	PRINTF("coarse_hard_stop = %d\r\n",coarse_hard_stop);
	PRINTF("coarse_ms_stop   = %d\r\n",coarse_ms_stop);
	PRINTF("fine_ms_stop     = %d\r\n",fine_ms_stop);
	PRINTF("fine_hard_stop   = %d\r\n",fine_hard_stop);
	int pos_size = coarse_hard_stop - fine_hard_stop;
	PRINTF("Size in pos incr = %d\r\n",pos_size);
	wait_ms(1000);

	// Note: Could just position back to known fine ms pos.

	for(count=0;;count++)
	{
		if(Board_pin_read(FINE_MICROSWITCH_PIN) == false)
		{
			Disable_Motor(5000);
			PRINTF("fine_ms off, exiting\r\n");
			break;
		}
	}
#ifdef MH_XXX
	Timer_reset(&Motor_timer);
	pos = Enc_pos;
	for(count=0;;count++)
	{
		msecs = Timer_read_ms(&Motor_timer);
		if(msecs >= 200)
		{
			if(pos == Enc_pos)
			{
				PRINTF("msecs >=200, pos==Enc_pos,exiting\r\n");
				break;
			}
			pos = Enc_pos;
			Timer_reset(&Motor_timer);
		}
	}
#endif
	PRINTF("coarse_ms_pos from CHS = %d\r\n",coarse_hard_stop - coarse_ms_stop);
	PRINTF("fine_ms_pos from CHS   = %d\r\n",coarse_hard_stop - fine_ms_stop);
	PRINTF("fine_hs_pos from CHS   = %d\r\n",coarse_hard_stop - fine_hard_stop);
	wait_ms(100);
	PRINTF("Enc_pos:%d\r\n",Enc_pos);

	PRINTF("Finished calibrate\r\n");
}
#endif
#ifdef MH_CAL2
uint8_t Posx_change_t[64];
uint8_t Posx_change_ix;
void Hub_calibrate2(void)
{
	PRINTF("Hub_calibrate2:\r\n");
	Set_Direction_Coarse();
//	Set_fine_stop_override(true);	// perhaps should turn off at end?
	Set_Motor_RPM(4000,610);

	PRINTF("Moving coarse until fine microswitch off");
	for(;;)
	{
		if(Board_pin_read(FINE_MICROSWITCH_PIN) == false) break;
	}
	wait_ms(1000);	// Deliberately pass

	Disable_Motor(6100);	// This probably not needed..
	wait_ms(100);

	Set_Direction_Fine();
	Set_Motor_RPM(4000,620);
	Set_fine_stop_override(true);	// perhaps should turn off at end?
	int posx = Enc_posx;
	int posx_change=0;
	for(int i=0;;i++)
	{
		wait_ms(10);
		if(Enc_fine_microswitch_on) break;
		posx_change = posx - Enc_posx;
		posx = Enc_posx;
		Posx_change_ix &= 63;
		Posx_change_t[Posx_change_ix++] = posx_change;
	}
	int fine_ms_pos = Enc_fine_ms_stop_pos;
//	PRINTF("fine_ms_posx = %d,posx_change = %d\r\n",fine_ms_posx,posx_change);

	int posx_change2=0;
	posx = Enc_posx;
	Enc_max_usecs = 1000;
	Enc_sense_hardstop = true;
	int posx_exit2=0;
	for(int i=0;i<500;i++)
	{
		wait_ms(10);
		posx_change2 = posx - Enc_posx;
		posx = Enc_posx;
		Posx_change_ix &= 63;
		Posx_change_t[Posx_change_ix++] = posx_change2;
//		if(posx_change2 >= 30) break;
		if(Enc_sense_hardstop == false)
		{
			PRINTF("Enc_sense_hardstop:Enc_hardstop_posx=%d\r\n",Enc_hardstop_posx);
			break;
		}

		if(posx_change2 <= 15)
		{
			posx_exit2 = posx;
			break;
		}

	}
	int posx_from_fine;
	if(Enc_sense_hardstop == false)
	{
		posx_from_fine = Enc_hardstop_posx - fine_ms_posx;
		PRINTF("Enc_hardstop_posx:%d, posx from fine:%d\r\n",Enc_hardstop_posx,posx_from_fine)
		PRINTF("posx_from_fine = %d,posx_change2=%d\r\n",posx_from_fine,posx_change2);
	}
	else
	{
		int ready_pin = Board_pin_read(READY_PIN);
		Disable_Motor(6200);
		PRINTF("ready_pin=%d\r\n",ready_pin);
		posx_from_fine = posx_exit2 - fine_ms_posx;
		PRINTF("posx_exit2     = %d\r\n",posx_exit2);
		PRINTF("posx_from_fine = %d,posx_change2=%d\r\n",posx_from_fine,posx_change2);
	}

#ifdef MH_XXX
	if(Motor_fine_stop_override)
	{
		PRINTF("FS override on\r\n");
	}
	else
	{
		PRINTF("FS override off\r\n");
	}
	if(Motor_enabled )
	{
		PRINTF("Motor enabled\r\n");
	}
	else
	{
		PRINTF("Motor not enabled\r\n");
	}
#endif

//	int posx_from_fine = posx - fine_ms_posx;
//	int fine_hard_posx = Enc_posx;
//	PRINTF("fine_hard_posx = %d,posx_change2=%d\r\n",fine_hard_posx,posx_change2);

	wait_ms(200);

	posx = Enc_posx;		// dummy for debug
	PRINTF("Enc_posx = %d\r\n",Enc_posx);
	for(int i=0;i<20;i++)
	{
		if(i > 1)
		{
			Set_Direction_Fine();
		}
		else
		{
			Set_Direction_Coarse();
		}
	//	Set_Motor_RPM(1000,620);
		Set_AOUT(15);
		Enable_Motor();
		Set_fine_stop_override(true);
		wait_ms(150);
		Disable_Motor(6300);
		PRINTF("(%d)Enc_posx=%d\r\n",i,Enc_posx);
		wait_ms(100);
	}
	posx_from_fine = Enc_posx - fine_ms_posx;
	PRINTF("posx_from_fine = %d\r\n",posx_from_fine);
}

#endif
#ifdef MH_XXX
void Hub_calibrate3(void)
{
	PRINTF("Hub_calibrate3:\r\n");
	Set_Direction_Coarse();
	Set_fine_stop_override(true);	// perhaps should turn off at end?
	Set_Motor_RPM(4000,610);

	PRINTF("Moving coarse until coarse microswitch on");
	for(;;)
	{
		wait_ms(10);
		if(Enc_coarse_microswitch_on) break;
	}
//	int coarse_ms_posx = Enc_coarse_ms_stop_posx;
	wait_ms(100);	// Deliberately pass

	Disable_Motor(6100);	// This probably not needed..
	wait_ms(100);

	PRINTF("Moving fine until coarse microswitch off\r\n");

	Set_Direction_Fine();
	Set_Motor_RPM(4000,620);
	Set_fine_stop_override(true);
	int posx_change=0;
	int posx = Enc_posx;
	for(;;)
	{
		wait_ms(10);
		if(Enc_coarse_microswitch_on == false) break;
		posx_change = posx - Enc_posx;
		posx = Enc_posx;
		Posx_change_ix &= 63;
		Posx_change_t[Posx_change_ix++] = posx_change;
		if(posx_change > 30)
		{
			PRINTF("posx_change>30\r\n");
		}
	}
	int coarse_ms_pos = Enc_coarse_ms_stop_pos;

	PRINTF("Enc_coarse_ms_stop_pos = %d\r\n",Enc_coarse_ms_stop_pos);
	PRINTF("Enc_pos = %d\r\n",Enc_pos);
	wait_ms(200);

	posx = Enc_posx;
	posx_change=0;
//	int posx_zero_count = 0;
//	int posx_non_zero_count = 0;
	int fine_ms_count=0;
	int fine_ms_stop_pos = 0;
	for(int i=0;i<1000;i++)
	{
		wait_ms(10);
		//		if(Enc_fine_microswitch_on) break;
		posx_change = posx - Enc_posx;
		posx = Enc_posx;
		Posx_change_ix &= 63;
		Posx_change_t[Posx_change_ix++] = posx_change;
		if(Enc_fine_microswitch_on)
		{
			if(fine_ms_stop_pos == 0) fine_ms_stop_pos = Enc_fine_ms_stop_pos;
			if(fine_ms_count++ > 100)
			{
				PRINTF("fine_ms_count > 100\r\n");
				break;
			}
		}
/*
		if(posx_change == 0)
		{
			posx_zero_count++;
		}
		else
		{
			posx_non_zero_count++;
		}
*/
		if(posx_change > 30)
		{
			int posx_from_coarse = posx - coarse_ms_posx;
			PRINTF("pfcx:%d\r\n",posx_from_coarse)
		}
	}
	Disable_Motor(6200);
//	PRINTF("posx_zero_count     = %d\r\n",posx_zero_count);
//	PRINTF("posx_non_zero_count = %d\r\n",posx_non_zero_count);

	if(Enc_fine_microswitch_on)
	{
		PRINTF("Fine ms on, Enc_fine_ms_stop_posx = %d\r\n",Enc_fine_ms_stop_posx);
		PRINTF("finex to coarsex = %d\r\n",Enc_coarse_ms_stop_posx - Enc_fine_ms_stop_posx);
	}
//	PRINTF("fine_hard_posx = %d,posx_change2=%d\r\n",fine_hard_posx,posx_change2);
//	PRINTF("posx_from_fine = %d,posx_change2=%d\r\n",posx_from_fine,posx_change2);
	wait_ms(200);

	posx = Enc_posx;		// dummy for debug

}
#endif
#ifdef MH_XXX
	if(next_head != urb->tail)	// Normal path if buffer not full
	{

//		urb->pbuff[next_head] = uc;	// This bug took most of day to solve. MHH 16/11/2022!!!!
		urb->pbuff[head] = uc;
		urb->head = next_head;
//		UART0_ISR_disabled = false;
		pUART->INTENSET = TXRDYEN;	// Enable interrupt, so ISR will send another byte when ready
		return;
	}




//	pUART->INTENSET = TXRDYEN;	// Enable interrupt, so ISR will send another byte when ready, and hopefully update tail...

//	while(next_head == urb->tail)	// Wait until tail changes. Could output directly from here as an alternative
//	while(next_head == Uart_get_tail(pUART,urb))	// Wait until tail changes. Could output directly from here as an alternative


	// If we just wait for the ISR to output this may not happen if (say) PRINTF called from an ISR with higher priority than UART ISR.


	int tail  = urb->tail;
	uint8_t c = urb->pbuff[tail];

	while(UART_WRITEABLE(pUART) == false);	// Wait here until we can send the byte

	UART_PUTC(pUART,c);			// Send tail byte
	urb->tail = (tail+1) % urb->buffsize;	// Update tail pointer

/*
	ARB_ix %= ARB_BUFF_SIZE;
	ARB_tail[ARB_ix] = urb->tail;
	ARB_sent[ARB_ix++] = c;

	ARB_stats.last_sent = c;
	ARB_stats.sent++;
*/
//	urb->pbuff[next_head] = uc;
	urb->pbuff[head] = uc;	// This bug took most of day to solve. MHH 16/11/2022!!!!
	urb->head = next_head;
//	UART0_ISR_disabled = false;
	pUART->INTENSET = TXRDYEN;	// Enable interrupt, so ISR will send another byte when ready
#else
#endif

	//-------------------------------------------------------------------------
	#ifdef MH_OUTPUT_IRQ_BUFF
	static void UartOutputIRQRingBuffer(UartRingBuf_t *urb,LPC_USART_T *pUART)
	{
	    if(urb->busy) return;
	    if(urb->head == urb->tail) return;  // Anything in buffer?
	    UartOutputRingBuffer(urb,pUART);      // yes, output
	}
	#endif

#ifdef MH_DEBUG_UART0_ISR
struct
{
	int count;
	int not_ready;
	int bytes_avail;
	int sent;
	int last_sent;
} UART0_stats;

#define ISR_BUFF_SIZE	16
int ISR_ix;
uint8_t ISR_sent[ISR_BUFF_SIZE];
uint8_t ISR_tail[ISR_BUFF_SIZE];

void ISR_debug(){};
#endif

#ifdef MH_XXX
void Board_putchar(char c)
{
	uint8_t buf[2];

	buf[0] = (uint8_t)c;
    UartCopyToRingBuffer(&Uart1OutputRB,LPC_UART1,buf,1);
}
#endif
//-----------------------------------------------------------------------------
/*
static int UartBytesTransmitted;
static void UartOutputRingBuffer(UartRingBuf_t *urb,LPC_USART_T *pUART)
{
    int tail = urb->tail;
//	pUART->INTENCLR = TXRDYEN;	// So we don't get interrupted in this routine.

    while(urb->head != tail) {
//        if(pUART->writeable()== false) return;
        if(UART_WRITEABLE(pUART) == false)
        {
        	pUART->INTENSET = TXRDYEN;	// Enable interrupt when ready to send another byte
        	return;
        }

        uint8_t c = urb->pbuff[tail];
//        pUART->putc(c);
        UART_PUTC(pUART,c);
        UartBytesTransmitted++;
        tail = (tail+1) % urb->buffsize;    // Note: tail is used in while test!
        urb->tail = tail;
    }

}
*/
//-----------------------------------------------------------------------------
/*
int Uart_get_tail(LPC_USART_T *pUART,UartRingBuf_t *urb)
{
	pUART->INTENSET = TXRDYEN;	// Enable interrupt, so ISR will send another byte when ready, and hopefully update tail...
	return urb->tail;
}
*/
/*
#define USTATS_BUFF_SIZE	8
struct
{
	uint8_t io;
	uint8_t c;
	uint8_t head;
	uint8_t tail;
} Ustats[8];
int Ustats_ix;
*/
/*
#define ARB_BUFF_SIZE	16
int ARB_ix;
uint8_t ARB_sent[ARB_BUFF_SIZE];
uint8_t ARB_tail[ARB_BUFF_SIZE];
bool UART0_ISR_disabled;
*/
#endif
