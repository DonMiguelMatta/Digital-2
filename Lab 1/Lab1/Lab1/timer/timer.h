/*
 * TIMER.h
 *
 * Created:
 * Author:
 * Description: Libreria general para configuracion de timers en ATmega328P
 */

#ifndef TIMER_H_
#define TIMER_H_

/****************************************/
// Librerias
#include <avr/io.h>
#include <stdint.h>

/****************************************/
// Constantes

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#define TIMER_STOPPED        0
#define TIMER_PRESCALER_1    1
#define TIMER_PRESCALER_8    8
#define TIMER_PRESCALER_64   64
#define TIMER_PRESCALER_256  256
#define TIMER_PRESCALER_1024 1024

/****************************************/
// Prototipos

void initTimer0_CTC(uint8_t comparacion, uint16_t prescaler);
void initTimer1_CTC(uint16_t comparacion, uint16_t prescaler);

void timer0SetCompareA(uint8_t comparacion);
void timer1SetCompareA(uint16_t comparacion);

void timer0Reset(void);
void timer1Reset(void);

void timer0Stop(void);
void timer1Stop(void);

uint8_t timer0CompareAFlag(void);
uint8_t timer1CompareAFlag(void);

void timer0ClearCompareAFlag(void);
void timer1ClearCompareAFlag(void);

uint8_t timer0_usToTicks(uint32_t tiempo_us, uint16_t prescaler);
uint16_t timer1_usToTicks(uint32_t tiempo_us, uint16_t prescaler);

#endif /* TIMER_H_ */