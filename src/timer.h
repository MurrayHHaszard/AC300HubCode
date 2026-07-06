/*
 * timer.h
 *
 *  Created on: 19/05/2018
 *      Author: Murray
 */

#ifndef SRC_TIMER_H_
#define SRC_TIMER_H_

/*
 * Adapted from LP1768 routines
 */

typedef struct
{
	uint32_t	_start;
	uint32_t	_running;
	uint32_t	_time;
} TIMER_t;

void wait_us(uint32_t);
void wait_ms(uint32_t);

void Timer_start(TIMER_t *);
void Timer_stop(TIMER_t *);
uint32_t Timer_read_us(TIMER_t *);
float Timer_read(TIMER_t *);
uint32_t Timer_read_ms(TIMER_t *);
uint32_t Timer_slicetime(TIMER_t *);
void Timer_reset(TIMER_t *);
extern uint32_t us_ticker_read(void);


#endif /* SRC_TIMER_H_ */
