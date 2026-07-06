/*
 * timer.c
 *
 *  Created on: 19/05/2018
 *      Author: Murray
 */


/*
 * These routines adapted from LPC1768 routines
 */
#include "LPC8xx.h"
#include "timer.h"
//#define US_TICKER_TIMER		LPC_CTIMER0


#ifdef MH_USE_ROUTINE
uint32_t us_ticker_read(void)
{
    if (!us_ticker_inited)
        us_ticker_init();

    return US_TICKER_TIMER->TC;
}
#else
//#define us_ticker_read()	LPC_CTIMER0->TC
uint32_t us_ticker_read(void)
{
	return LPC_CTIMER0->TC;
}
#endif


void wait_us(uint32_t usecs)
{
	uint32_t start_usecs = us_ticker_read();
	uint32_t end_usecs   = start_usecs + usecs;
	if(end_usecs < start_usecs) {
		while(us_ticker_read() > start_usecs);	// wait until wrap around
	}
	while(us_ticker_read() < end_usecs);		// loop until end

}
void wait_ms(uint32_t msecs)
{
	wait_us(msecs*1000);
}

void Timer_start(TIMER_t *pTIMER) {
    pTIMER->_start = us_ticker_read();
    pTIMER->_running = 1;
}

void Timer_stop(TIMER_t *pTIMER) {
    pTIMER->_time += Timer_slicetime(pTIMER);
    pTIMER->_running = 0;
}

uint32_t Timer_read_us(TIMER_t *pTIMER) {
    return pTIMER->_time + Timer_slicetime(pTIMER);
}

float Timer_read(TIMER_t *pTIMER) {
    return (float)Timer_read_us(pTIMER) / 1000000.0f;
}

uint32_t Timer_read_ms(TIMER_t *pTIMER) {
    return Timer_read_us(pTIMER) / 1000;
}

uint32_t Timer_slicetime(TIMER_t *pTIMER) {
    if (pTIMER->_running) {
        return us_ticker_read() - pTIMER->_start;
    } else {
        return 0;
    }
}

void Timer_reset(TIMER_t *pTIMER) {
    pTIMER->_start = us_ticker_read();
    pTIMER->_time = 0;
}



