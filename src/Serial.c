#include <stdio.h>

#include "LPC8xx.h"
#include "uart.h"
#include "syscon.h"
#include "swm.h"

#include "AC300_hub.h"
#include "chip_setup.h"


// Implementation of sendchar (used by printf)
// This is for Keil and MCUXpresso projects.
int sendchar (int ch) {
  while (!((pDBGU->STAT) & TXRDY));   // Wait for TX Ready
  return (pDBGU->TXDAT  = ch);        // Write one character to TX data register
}


// Implementation of MyLowLevelPutchar (used by printf)
// This is for IAR projects. Must include locally modified __write in the project.
int MyLowLevelPutchar(int ch) {
  while (!((pDBGU->STAT) & TXRDY));   // Wait for TX Ready
  return (pDBGU->TXDAT  = ch);        // Write one character to TX data register
}


// Implementation of getkey (used by scanf)
// This is for Keil and MCUXpresso projects.
int getkey (void) {
  while (!((pDBGU->STAT) & RXRDY));   // Wait for RX Ready
  return (pDBGU->RXDAT );             // Read one character from RX data register
}


// Implementation of MyLowLevelGetchar (used by scanf)
// This is for IAR projects. Must include locally modified __read in the project.
int MyLowLevelGetchar(void){
  while (!((pDBGU->STAT) & RXRDY));   // Wait for RX Ready
  return (pDBGU->RXDAT );             // Read one character from RX data register
}


/* Gets a character from the UART, returns EOF if no character is ready */

#ifdef MH_NO_RB
int Board_UARTGetChar(void)
{

	  if((pDBGU->STAT) & RXRDY)
	  {
		  return (pDBGU->RXDAT );             // Read one character from RX data register
	  }
		return EOF;
}

int Board_UARTGetChar_wait(int millisecs)
{
	uint32_t start_usecs = us_ticker_read();
	uint32_t max_usecs = millisecs * 1000;
	while(true)
	{
		uint32_t elapsed_usecs = us_ticker_read() - start_usecs;	// This logic to try and handle wrap around (when us_ticker_read goes to zero)
		if(elapsed_usecs >= max_usecs)
		{
			return EOF;
		}

		if((pDBGU->STAT) & RXRDY)
		{
			return (pDBGU->RXDAT );             // Read one character from RX data register
		}
	}
}
#endif

void Board_UARTPutchar(int ch)
{
	  while (!((pDBGU->STAT) & TXRDY));   // Wait for TX Ready
	  pDBGU->TXDAT  = ch;        // Write one character to TX data register

}
//==============================================================================================================
//#define UART_WRITEABLE(puart)	(puart->FIFOLVL/256 < 16)
#define UART_WRITEABLE(puart)	((puart->STAT) & TXRDY)
//#define UART_NOT_WRITEABLE(puart)	(puart->FIFOLVL/256 == 16)
//#define UART_PUTC(puart,c)		puart->THR = c
#define UART_PUTC(puart,c)		puart->TXDAT = c
//#define UART_READABLE(puart)	(puart->LSR & UART_LSR_RDR)
//#define UART_GETC(puart)		(puart->RBR)


#define P_BUFF_MAX		128
//int p_BufferLen;
char p_Buffer[P_BUFF_MAX];

//---------------------------------------------------------------------------------
typedef struct
{
    volatile uint32_t head;		// MHH: 15/03/2018. Added "volatile" as had a problem with mbed compiler serial(Encoder7) without volatile
    volatile uint32_t tail;
//    volatile int busy;       // Only for output so no conflict with IRQ
    uint32_t buffsize;
    uint8_t *pbuff;
} UartRingBuf_t;

//#define SBUFFSIZE_OUT	4	// Test only, should be 128
#define SBUFFSIZE_OUT	128
#define SBUFFSIZE_IN	32
uint8_t Uart1_buff_out[SBUFFSIZE_OUT];
uint8_t Uart1_buff_in[SBUFFSIZE_IN];

UartRingBuf_t Uart1OutputRB;
UartRingBuf_t Uart1InputRB;
#define TXRDYEN		(1<<2)		// 17.6.4 in manual
#define RXRDYEN		(1<<0)
void Serial_rb_init(void)
{
	Uart1OutputRB.buffsize = SBUFFSIZE_OUT;
	Uart1OutputRB.pbuff = Uart1_buff_out;

	Uart1InputRB.buffsize = SBUFFSIZE_IN;
	Uart1InputRB.pbuff = Uart1_buff_in;

	LPC_USART1->INTENSET = (RXRDYEN | TXRDYEN);		// Not sure this is needed

	NVIC_EnableIRQ(UART1_IRQn);

}
#define SIG100_BUFFSIZE_OUT	64
#define SIG100_BUFFSIZE_IN	32

uint8_t Uart0_buff_out[SIG100_BUFFSIZE_OUT];
uint8_t Uart0_buff_in[SIG100_BUFFSIZE_IN];

UartRingBuf_t Uart0OutputRB;
UartRingBuf_t Uart0InputRB;
TIMER_t SIG100_rb_empty_timer;
void SIG100_rb_init(void)
{
	Uart0OutputRB.buffsize = SIG100_BUFFSIZE_OUT;
	Uart0OutputRB.pbuff = Uart0_buff_out;

	Uart0InputRB.buffsize = SIG100_BUFFSIZE_IN;
	Uart0InputRB.pbuff = Uart0_buff_in;

	LPC_USART0->INTENSET = (RXRDYEN | TXRDYEN);		// Not sure this is needed

	Timer_start(&SIG100_rb_empty_timer);

	NVIC_EnableIRQ(UART0_IRQn);

}
//==========================================================================
#define LPC_USART_T	LPC_USART_TypeDef
#define LPC_UART1	pDBGU

/*
 * Following example from Bing. MHH:02/12/2023
 */
/*
#include "board.h"
#include "fsl_usart.h"

// Define the size of the ring buffer
#define RING_BUFFER_SIZE 64

// Declare the ring buffer
static uint8_t ringBuffer[RING_BUFFER_SIZE];
static volatile uint32_t ringBufferHead = 0;
static volatile uint32_t ringBufferTail = 0;

// USART interrupt handler
void USART0_IRQHandler(void) {
    uint8_t data;

    // Check if data is received
    if (USART_GetStatusFlags(USART0) & kUSART_RxReady) {
        data = USART_ReadByte(USART0);

        // Add data to the ring buffer
        ringBuffer[ringBufferHead] = data;
        ringBufferHead = (ringBufferHead + 1) % RING_BUFFER_SIZE;
    }
}

// Initialize USART0 for serial communication
void USART_Init(void) {
    usart_config_t config;
    USART_GetDefaultConfig(&config);
    config.baudRate_Bps = 115200;
    config.enableTx = true;
    config.enableRx = true;
    USART_Init(USART0, &config, CLOCK_GetFreq(kCLOCK_Fro12M));
    USART_EnableInterrupts(USART0, kUSART_RxReadyInterruptEnable);
    NVIC_EnableIRQ(USART0_IRQn);
}

// Read data from the ring buffer
bool USART_Read(uint8_t *data) {
    if (ringBufferHead != ringBufferTail) {
        *data = ringBuffer[ringBufferTail];
        ringBufferTail = (ringBufferTail + 1) % RING_BUFFER_SIZE;
        return true;
    }
    return false;
}

int main(void) {
    BOARD_InitPins();
    BOARD_BootClockFROHF48M();
    USART_Init();

    uint8_t receivedData;

    while (1) {
        if (USART_Read(&receivedData)) {
            // Process received data
            // ...
        }
    }
}

 */



void UartAddRingByte(LPC_USART_T *pUART,UartRingBuf_t *urb,uint8_t uc)
{
	pUART->INTENCLR = TXRDYEN;				// Disable interrupt
//	UART0_ISR_disabled = true;

#ifdef MH_DEBUG
	if(pUART == LPC_USART0)		// Debug!!
	{
		if(uc >= 32 && uc < 127)
		{
			Dputc(uc);
		}
		else
		{
			PRINTF("[x%x]",uc);
		}
	}
#endif
	uint32_t head = urb->head;
	uint32_t tail = urb->tail;
	uint32_t buffsize = urb->buffsize;
	uint32_t buffsizemask = urb->buffsize -1;
	uint32_t next_head = (head+1);

	uint32_t head_ix = head & buffsizemask;
	urb->pbuff[head_ix] = uc;
	if(next_head - tail < buffsize)
//	if(next_head != tail)	// Normal path if buffer not full
	{
		urb->head++;
//		UART0_ISR_disabled = false;
		pUART->INTENSET = TXRDYEN;	// Enable interrupt, so ISR will send another byte when ready
		return;
	}
	uint32_t tail_ix = tail & buffsizemask;
	uint8_t c = urb->pbuff[tail_ix];
	while(UART_WRITEABLE(pUART) == false);	// Wait here until we can send the byte
	UART_PUTC(pUART,c);			// Send tail byte

// Could put a check in here that next_head = tail + 1
	urb->tail++;
	urb->head++;
	pUART->INTENSET = TXRDYEN;	// Enable interrupt, so ISR will send another byte when ready
}
//-----------------------------------------------------------------------------
void Board_UARTPutchar_RB(uint8_t c)
{
	UartAddRingByte(LPC_USART1,&Uart1OutputRB,c);
//	LPC_USART0->INTENSET = TXRDYEN;	// Enable interrupt when ready to send another byte
}

//----------------------------------------------------------------------------------
void UartCopyToRingBuffer(UartRingBuf_t *urb,LPC_USART_T *pUART,uint8_t *ubp,int ilen)
{
	uint8_t *bp = ubp;
//	pUART->INTENCLR = TXRDYEN;	// Disable interrupts for UART
	for(int i=0;i<ilen;i++)
	{
		UartAddRingByte(pUART,urb,*bp++);
	}
}
//-------------------------------------------------------------------------
// This routine only called from ISR and will overwrite any bytes not popped in time.
void RingBuffer_Insert(UartRingBuf_t *urb,uint8_t uc)
{
	uint32_t head = urb->head++;
	uint32_t buffsizemask = urb->buffsize -1;

	uint32_t head_ix = head & buffsizemask;
	urb->pbuff[head_ix] = uc;

/*
 * 		Consider:
 * 		Tail = 0, Head = 7, buffsize = 8, and a byte is added.
 *
 * 		Then:
 *
 * 		Head = 8, and tail (0) is overwritten
 *
 * 		In this logic, head = 7, as is previous value, and buffsizemask = 7, so:
 *
 * 		tail_last = head (7) - buffsizemask (7) = 0 = tail (0)
 * 		since tail (0) <= tail_last, then tail = tail_last + 1 = 1		which is first tail byte not overwritten
 *
 */
//#ifdef MH_XXX

	if(head >= buffsizemask)		// Make sure tail_last not negative
	{
		uint32_t tail_last = head - buffsizemask;	// MHH:05/12/2023. These lines added in case bytes not popped in time, as they have been overwritten.
		if(urb->tail <= tail_last)
		{
			urb->tail = tail_last + 1;
		}
	}

//#endif
}
/* Pop single item from Ring Buffer */
int RingBuffer_Pop(UartRingBuf_t *urb)
{
	if(urb->head > urb->tail)	// Anything in buffer?
	{
		uint32_t buffsizemask = urb->buffsize - 1;
		uint32_t tail_ix = urb->tail++ & buffsizemask;
		uint8_t ch = urb->pbuff[tail_ix];
#ifdef MH_DEBUG
		if(ch > 32 && ch < 126)	// Debug!!!
		{
			Dputc(ch);
		}
		else
		{
			PRINTF("[x%x]",ch);
		}
#endif
		return ch;
	}
	return -1;			// Nothing in ring buffer
}

extern int Hub_plog_debug;

void UART0_IRQHandler(void)
{
//	LPC_USART0->INTENCLR = TXRDYEN;				// Disable interrupt
	LPC_USART0->INTENCLR = RXRDYEN | TXRDYEN;	// Disable interrupts

// Should we disable RX interrupt too?

	while((LPC_USART0->STAT) & RXRDY)
	{
		uint8_t ch = LPC_USART0->RXDAT;
		RingBuffer_Insert(&Uart0InputRB,ch);
	}

	while(Uart0OutputRB.head > Uart0OutputRB.tail)	// Anything in buffer?
	{
		if((LPC_USART0->STAT & TXRDY) == false)	// Can we write to UART?
		{
			LPC_USART0->INTENSET = RXRDYEN | TXRDYEN;	// No, enable both as more to send
			return;
		}

		// OK, we can send a byte. Get it, send it and update tail pointer

		uint32_t buffsizemask = Uart0OutputRB.buffsize - 1;

		uint32_t tail_ix = Uart0OutputRB.tail & buffsizemask;
		uint8_t c = Uart0OutputRB.pbuff[tail_ix];
		UART_PUTC(LPC_USART0,c);
		Uart0OutputRB.tail++;
#ifdef MH_DEBUG_SERIAL
		if(Hub_plog_debug == 2)
		{
			Dputc(c);
		}
#endif
	}
	LPC_USART0->INTENSET = RXRDYEN;	// Enable only receive because nothing in output buffer
	Timer_reset(&SIG100_rb_empty_timer);

}
//-------------------------------------------------------------------------
void UART1_IRQHandler(void)
{
//	LPC_UART1->INTENCLR = TXRDYEN;				// Disable interrupt
	LPC_USART1->INTENCLR = RXRDYEN | TXRDYEN;	// Disable interrupts


	while((LPC_USART1->STAT) & RXRDY)
	{
		uint8_t ch = LPC_USART1->RXDAT;
		RingBuffer_Insert(&Uart1InputRB,ch);
	}

	while(Uart1OutputRB.head > Uart1OutputRB.tail)	// Anything in buffer?
	{
		if((LPC_USART1->STAT & TXRDY) == false)	// Can we write to UART?
		{
			LPC_UART1->INTENSET = RXRDYEN |TXRDYEN;	// No, enable interrupt when ready to send another byte
			return;
		}

		// OK, we can send a byte. Get it, send it and update tail pointer

		uint32_t buffsizemask = Uart1OutputRB.buffsize - 1;

		uint32_t tail_ix = Uart1OutputRB.tail & buffsizemask;
		uint8_t c = Uart1OutputRB.pbuff[tail_ix];
		UART_PUTC(LPC_UART1,c);
		Uart1OutputRB.tail++;
	}
	LPC_USART1->INTENSET = RXRDYEN;	// Enable only receive because nothing in output buffer

}
//----------------------------------------------------------------------------
int Board_RB_bytes_avail(void)
{
	return (Uart1InputRB.head - Uart1InputRB.tail);
}

int Board_UARTGetChar_RB(void)
{
	return RingBuffer_Pop(&Uart1InputRB);
}
void Adxl375_read_xyz_if_ready(void);

int Board_UARTGetChar_wait_RB(int millisecs)
{
	if(Board_RB_bytes_avail() > 0)	// Fast track
	{
		return RingBuffer_Pop(&Uart1InputRB);
	}
	uint32_t start_usecs = us_ticker_read();
	uint32_t max_usecs = millisecs * 1000;
	for (;;)
	{
		if(Board_RB_bytes_avail() > 0)
		{
			return RingBuffer_Pop(&Uart1InputRB);
		}
		uint32_t usecs_elapsed = us_ticker_read() - start_usecs;	// Note: This should handle wrap around of timer
		if(usecs_elapsed >= max_usecs)
		{
			return -1;
		}
#ifdef MH_ADXL375
		// Note: Can put any routine in here that takes less than a millisec.
		Adxl375_read_xyz_if_ready();
#endif
	}
}
//----------------------------------------------------------------------------
void p_send(const char *buf,uint32_t numChar)
{
	//void UartCopyToRingBuffer(UartRingBuf_t *urb,LPC_USART_T *pUART,const uint8_t *ubp,int ilen)
    UartCopyToRingBuffer(&Uart1OutputRB,LPC_UART1,(uint8_t *)buf,(int)numChar);
}

void Board_UART1PutSTR(const char *buff)
{
#ifdef MH_XXX	// MHH:10/01/2026
	if(p_BufferLen <= 0) return;	// Ignore errors
	if(p_BufferLen >= P_BUFF_MAX-1)
	{
		DebugAbort("Board_UART0PutSTR:len > 64");
	}
	p_send(buff,p_BufferLen);
#endif
	int slen = strlen(buff);
	p_send(buff,slen);
}

int Board_output_RB_bytes_avail(void)
{
	return (Uart1OutputRB.head - Uart1OutputRB.tail);
}
void Board_output_wait_until_rb_empty(void)	// MHH:16/12/2023Uart1_buff_out
{
	while(Board_output_RB_bytes_avail());
}



//bool Dputs_active=true;
void Dputs(const char *buff)
{
//	if(Dputs_active != true) return;
	uint8_t *bp = (uint8_t *)buff;
	uint8_t c;
	for(;;)
	{
		c = *bp++;
		if(c == 0) return;

		UartAddRingByte(LPC_USART1,&Uart1OutputRB,c);
	}
}
void Dputs2(const char *buff)
{
	if(Motor_diags)
	{
		Dputs(buff);
	}
}

void Dputc(const char c)
{
	UartAddRingByte(LPC_USART1,&Uart1OutputRB,c);
}


#define CLOCK_UART0		(1<<14)
#define CLOCK_UART1		(1<<15)

#define UART0_RESET_FLAG	(1<<14)

// Index by uart_ix, only handle UART0,UART1 and UART2 for now.
const uint8_t Uart_func_num[3]={U0_TXD,U1_TXD,U2_TXD};

const LPC_USART_TypeDef *pLPC_UART_tbl[3]={LPC_USART0,LPC_USART1,LPC_USART2};
//----------------------------------------------------------------------------------------------------
void LPC845_putc(uint32_t uart_ix,int ch)
{
	LPC_USART_TypeDef *pLPC_uart;
	pLPC_uart = (LPC_USART_TypeDef *)pLPC_UART_tbl[uart_ix];
	while (!((pLPC_uart->STAT) & TXRDY));   // Wait for TX Ready
	pLPC_uart->TXDAT  = ch;        // Write one character to TX data register
}
//----------------------------------------------------------------------------------------------------
bool LPC845_is_UART_readable(uint32_t uart_ix)
{
	LPC_USART_TypeDef *pLPC_uart;
	pLPC_uart = (LPC_USART_TypeDef *)pLPC_UART_tbl[uart_ix];
	if((pLPC_uart->STAT) & RXRDY) return true;
	return false;
}
//----------------------------------------------------------------------------------------------------
int LPC845_getc(uint32_t uart_ix)
{
	LPC_USART_TypeDef *pLPC_uart;
	pLPC_uart = (LPC_USART_TypeDef *)pLPC_UART_tbl[uart_ix];
	while (!((pLPC_uart->STAT) & RXRDY));   // Wait for RX Ready
	return (pLPC_uart->RXDAT );             // Read one character from RX data register
}
//-------------------------------------------------------------------------------
/* Gets a character from the UART, returns EOF if no character is ready */
int LPC845_getc_nowait(uint32_t uart_ix)
{
	LPC_USART_TypeDef *pLPC_uart;
	pLPC_uart = (LPC_USART_TypeDef *)pLPC_UART_tbl[uart_ix];

	if((pLPC_uart->STAT) & RXRDY)
	{
		return (pLPC_uart->RXDAT );             // Read one character from RX data register
	}
	return EOF;
}
//-------------------------------------------------------------------------------
int LPC845_getc_timeout(uint32_t uart_ix,uint32_t millisecs)
{
	LPC_USART_TypeDef *pLPC_uart;
	pLPC_uart = (LPC_USART_TypeDef *)pLPC_UART_tbl[uart_ix];
	uint32_t start_usecs = us_ticker_read();
	uint32_t max_usecs = millisecs * 1000;
	for (;;)
	{
		if ((pLPC_uart->STAT) & RXRDY)
		{
			return (pLPC_uart->RXDAT); // Read one character from RX data register
		}
		uint32_t usecs_elapsed = us_ticker_read() - start_usecs;	// Note: This should handle wrap around of timer
		if(usecs_elapsed >= max_usecs)
		{
			return -1;
		}
	}
}
//----------------------------------------------------------------------------------------------------
void LPC845_uart_init(uint32_t uart_ix,uint32_t uart_txpin,uint32_t uart_rxpin,uint32_t uart_baud_rate)
{

	LPC_USART_TypeDef *pLPC_uart;
	pLPC_uart = (LPC_USART_TypeDef *)pLPC_UART_tbl[uart_ix];

	// Turn on relevant peripheral APB/AHB clocks

	uint32_t	clock_flag = (CLOCK_UART0 << uart_ix);
	LPC_SYSCON->SYSAHBCLKCTRL[0] |= (clock_flag | SWM);

	// Connect USART TXD, RXD signals to port pins

#ifdef MH_OLD_CONFIG_SWM
	ConfigSWM(DBGUTXD, DBGTXPIN);
	ConfigSWM(DBGURXD, DBGRXPIN);
#else
	uint32_t func = Uart_func_num[uart_ix];
	ConfigSWM(func, uart_txpin);
	ConfigSWM(func+1, uart_rxpin);
#endif

	SystemCoreClockUpdate();	// main_clk and fro_clk obtained from this call

	// Note: This only works for UART0 to UART2. UART3 and UART4 are bits 30 and 31.
	uint32_t	reset_flag = (UART0_RESET_FLAG << uart_ix);
	LPC_SYSCON->PRESETCTRL[0] &= ~(reset_flag);		// Set to zero to reset
	LPC_SYSCON->PRESETCTRL[0] |= reset_flag;			// And on again

	if(uart_baud_rate > 19200)	// If using 115K then need to use this logic derived from UART0_Terminal.c.
	{
		LPC_SYSCON->FCLKSEL[uart_ix] = FCLKSEL_FRG0CLK;     // Select FRG0 as fclk to this USART

#define MH_USE_MAIN_CLOCK
#ifdef MH_USE_MAIN_CLOCK
		int clock_hz = main_clk;
		LPC_SYSCON->FRG0CLKSEL = FRGCLKSEL_MAIN_CLK;
#else
		// Note: fro_clock can be changed with call to system API: void set_fro_frequency(uint32_t iFreq);
		int clock_hz = fro_clk;
		LPC_SYSCON->FRG0CLKSEL = FRGCLKSEL_FRO_CLK;
#endif
		int brg = ((clock_hz)/(16 * uart_baud_rate)) - 1;
		int frm = -256 + (clock_hz*16)/(uart_baud_rate*(brg+1));
		LPC_SYSCON->FRG0DIV = 255;
		LPC_SYSCON->FRG0MULT = frm;
		pLPC_uart->BRG = brg;
	}
	else
	{
		LPC_SYSCON->FCLKSEL[uart_ix] = FCLKSEL_MAIN_CLK;
		pLPC_uart->BRG = (main_clk / (16 * uart_baud_rate)) - 1;
	}

	// Configure the USART CFG register:
	// 8 data bits, no parity, one stop bit, no flow control, asynchronous mode
	pLPC_uart->CFG = DATA_LENG_8|PARITY_NONE|STOP_BIT_1;

	// Configure the USART CTL register (nothing to be done here)
	// No continuous break, no address detect, no Tx disable, no CC, no CLRCC
	pLPC_uart->CTL = 0;

	// Clear any pending flags (for illustration, isn't necessary after the peripheral reset)
	pLPC_uart->STAT = 0xFFFF;

	// Enable the USART RX Ready Interrupt (add these lines to main if the project assumes an interrupt-driven use case)
	//pDBGU->INTENSET = RXRDY;
	//NVIC_EnableIRQ(DBGUIRQ);

	// Enable USART
	pLPC_uart->CFG |= UART_EN;


#ifdef MH_ENABLE_IRQ		// Not wanted for Motor test
	if(uart_ix == 0)		// Initially, just enable RX interrupt for UART0
	{
		// Enable the USART0 RX Ready Interrupt
		LPC_USART0->INTENSET = RXRDY;
		NVIC_EnableIRQ(UART0_IRQn);
	}
#endif

	// Turn off SWM clock before returning
	LPC_SYSCON->SYSAHBCLKCTRL[0] &= ~(SWM);

}
//=============================================================================================
void setup_sig100_uart(void)
{
	  LPC845_uart_init(SIG100_UART,SIG100_TXPIN,SIG100_RXPIN,19200);
}
//---------------------------------------------------------------------------------------------
char Number_string[10];
extern bool Hub_AT_debug;
int Sig100_put_count;
#ifndef MH_SIG100_RB
void Sig100_UARTPutChar(char ch)
{
	LPC845_putc(SIG100_UART,ch);
//	wait_us(500);
//	wait_us(200);	// This does NOT!!
	if(Hub_AT_debug)
	{
		if(ch >= 32 && ch <= 125)
		{
			Board_UARTPutchar_RB(ch);
		}
		else
		{
			PRINTF("<%d>",(int)ch);
		}
	}
}
#endif
//---------------------------------------------------------------------------------------------
#define SIG100_OUTPUT_DELAY_MSECS			2			// Try reducing when we get working
//#define SIG100_OUTPUT_DELAY_MSECS			3			// MHH:18/12/2023
#define SIG100_MAX_CONSECUTIVE_BYTES_OUT	64			// Try increasing when we get working
uint32_t SIG100_consecutive_bytes_out;

void SIG100_RB_UARTPutchar(uint8_t c)
{

/*
 * 	MHH:05/12/2023
 *
 * 	The SIG100 can only send a limited number of bytes consecutively before it loses track of where the bytes start.
 * 	The solution has been to pause periodically, usually at the end of a print statement, but not efficient. This logic an alternative
 */

	if(Uart0OutputRB.head == Uart0OutputRB.tail)		// Ring buffer empty?
	{
		int msecs = Timer_read_ms(&SIG100_rb_empty_timer);
		if(msecs >= SIG100_OUTPUT_DELAY_MSECS)
		{
			SIG100_consecutive_bytes_out = 0;
		}
	}


	if(SIG100_consecutive_bytes_out++ >= SIG100_MAX_CONSECUTIVE_BYTES_OUT)
	{
		Sig100_RB_flush_output();
		wait_ms(SIG100_OUTPUT_DELAY_MSECS);
		SIG100_consecutive_bytes_out = 1;
	}


	UartAddRingByte(LPC_USART0,&Uart0OutputRB,c);
#ifdef MH_DEBUG_SERIAL
	if(Hub_plog_debug==1)
	{
		Dputc(c);
	}
#endif
}
void Sig100_RB_flush_output(void)
{
	for(;;)
	{
		if(Uart0OutputRB.head == Uart0OutputRB.tail) return;
		// Could put a time limit, and/or do something while waiting
	}
}
//-----------------------------------------------------------------------------
void SIG100_RB_puts(char *string)
{
	uint8_t *p = (uint8_t *)string;
	uint8_t c;
	for(;;)
	{
		c = *p++;
		if(c == 0) return;
		SIG100_RB_UARTPutchar(c);
//		UartAddRingByte(LPC_USART0,&Uart0OutputRB,c);
	}
}
#ifdef MH_SIG100_PUTS_WAIT
void SIG100_RB_puts_and_wait(char *string)	// MHH:05/12/2023. Was used by SIG100_PRINTF, but have added logic to SIG100_RB_UARTPutchar(c) to handle
											// too many consecutive bytes
{
	SIG100_RB_puts(string);
	Sig100_flush_and_wait(10);
}
#endif
/* Gets a character from the UART, returns EOF if no character is ready */
#ifndef MH_SIG100_RB
int Sig100_getc_nowait(void)
{
	return LPC845_getc_nowait(SIG100_UART);
}
#endif
//-----------------------------------------------------------------------------
int Sig100_RB_getc_nowait(void)
{
	return RingBuffer_Pop(&Uart0InputRB);
}
int Sig100_RB_bytes_avail(void)
{
	return (Uart0InputRB.head - Uart0InputRB.tail);
}
uint32_t Flog_data_overrun;
int Sig100_RB_getc_timeout(int millisecs)
{
	if(Sig100_RB_bytes_avail() > 0)	// Fast track
	{
		return RingBuffer_Pop(&Uart0InputRB);
	}
	uint32_t start_usecs = us_ticker_read();
	uint32_t max_usecs = millisecs * 1000;
	for (;;)
	{
		if(Sig100_RB_bytes_avail() > 0)
		{
			return RingBuffer_Pop(&Uart0InputRB);
		}
#ifdef MH_XXX
		if(ADC_voltage_disabled)
		{
	//		PRINTF("P250:%d\r\n",Enc_Pos.pos);
			Put_encoder_pos(150);
		}
#endif
		uint32_t usecs_elapsed = us_ticker_read() - start_usecs;	// Note: This should handle wrap around of timer
		if(usecs_elapsed >= max_usecs)
		{
			return -1;
		}
#ifdef MH_ADXL1001		// MHH:15/12/2023
		Hub_flog_check_ring_buffer();
		if(Flog_data.overrun > Flog_data_overrun + 10)
		{
			Flog_data_overrun = Flog_data.overrun;
			Dputc('?');
		}
#endif
#ifdef MH_ADXL375
		// Note: Can put any routine in here that takes less than a millisec.
		Adxl375_read_xyz_if_ready();
#endif
	}
}
int Sig100_RB_getc(void)
{
	for(;;)
	{
		if(Sig100_RB_bytes_avail() > 0)
		{
			return RingBuffer_Pop(&Uart0InputRB);
		}
#ifdef MH_ADXL375
		// Note: Can put any routine in here that takes less than a millisec.
		Adxl375_read_xyz_if_ready();
#endif
	}
}
void Sig100_RB_clear_input(void)
{
	Uart0InputRB.head = 0;	// Possibly should disable interrupts?
	Uart0InputRB.tail = 0;
}
//---------------------------------------------------------------------------------------------
#ifndef MH_SIG100_RB
int Sig100_getc(void)
{
	return LPC845_getc(SIG100_UART);
}
//-------------------------------------------------------------------------------
int Sig100_getc_timeout(int millisecs)
{
	return LPC845_getc_timeout(SIG100_UART,millisecs);
}
//---------------------------------------------------------------------------------------------
void Sig100_UARTPutSTR(char *pString)
{
	char c;
	char *pCh = pString;
	for(;;)
	{
		c = *pCh++;
		if(c == 0)
		{
			wait_ms(10);	// MHH:06/09/2023
			return;
		}
		Sig100_UARTPutChar(c);
	}
}
#endif

//---------------------------------------------------------------------------------------------

//
// Function: setup_debug_uart
//
// UART BRG calculation:
// For asynchronous mode (UART mode) the BRG formula is:
// (BRG + 1) * (1 + (m/256)) * (16 * baudrate Hz.) = FRG_in Hz.
// For this example, we set m = 0 (so FRG = 1). 
// We choose FRG_in = main_clk, using FRG0CLKSEL mux setup below.
// Then, we use the global main_clk variable, as set by the function 
// SystemCoreClockUpdate(), in our BRG calculation as follows:
// BRG = (main_clk Hz. / (16 * desired_baud Hz.)) - 1

void setup_debug_uart()
{
  // Note: Using same pins (TX=p0_25, RX=p0_24) for SIG100 as I hope to be able to flash copy using boot initialise logic through SIG100.
//  LPC845_uart_init(DBGUART,DBGTXPIN,DBGRXPIN,19200);
  LPC845_uart_init(DBGUART,DBGTXPIN,DBGRXPIN,115200);
//  LPC845_uart_init(DBGUART,DBGTXPIN,DBGRXPIN,230400);
}

//--------------------------------------------------------------------------------------------
//#define MH_CHANGE_BAUD
#ifdef MH_CHANGE_BAUD
// This is untested on LPC845 and was written for LPC804. Leave here in case we need to go to 115K
// MHH:18/09/203. See LPC845_uart_init() for baudrate > 19200 for way to change baud above 19200

//#define FRG_IN_HZ		15000000
#define MH_FRACTIONAL_BR
#ifdef MH_FRACTIONAL_BR
int BRG_val;
int FRM_val;
void Set_baud_rate(LPC_USART_TypeDef *pUSART,int baudrate)
{
// Since both UARTs share fractional divider, only useful if either 1 UART or both have same baud rate.
// It is optional anyway, possibly only useful for a certain range of baud rates
// The higher the baudrate, the lower the BRG_val, so more need for fractional divider.
// Eg if baudrate == 115,200 then BRG_val = 7 and FRM_val = 5


	int brg = ((FRG_IN_HZ)/(16 * baudrate)) - 1;
	int v1 = (16 * baudrate) * (brg + 1);
	int v2 = v1 / 256;
	FRM_val = (FRG_IN_HZ - v1 + (v2 - 1)) / v2;	// Round up
	BRG_val = brg;
	if(pUSART == LPC_USART0)
	{
		LPC_SYSCON->FRG0MULT = FRM_val;
	}
	else
	{
		LPC_SYSCON->FRG1MULT = FRM_val;
	}
//    LPC_USART0->BRG = brg;
    pUSART->BRG = brg;
}
#endif
int Get_BRG_val(int baudrate)
{
	int v1 = 16 * baudrate;
	int brg = ((FRG_IN_HZ + v1 - 1)/v1) - 1;	// Rounding up.
	return brg;
}

void MH_set_debug_uart_to_115k(void)
{
	Set_baud_rate(LPC_USART1,115200);
//	wait_ms(100);
}

#endif
