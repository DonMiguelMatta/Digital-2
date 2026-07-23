/*
 * TIMER.c
 *
 * Created:
 * Author:
 * Description: Libreria general para configuracion de timers en ATmega328P
 */

/****************************************/
// Librerias
#include "timer.h"

/****************************************/
// Prototipos internos

static uint8_t getTimer0PrescalerBits(uint16_t prescaler);
static uint8_t getTimer1PrescalerBits(uint16_t prescaler);

/****************************************/
// Funciones

void initTimer0_CTC(uint8_t comparacion, uint16_t prescaler)
{
	// Configurar Timer0 en modo CTC
	TCCR0A = 0;
	TCCR0B = 0;
	TCNT0 = 0;

	OCR0A = comparacion;

	TCCR0A |= (1 << WGM01);

	TCCR0B |= getTimer0PrescalerBits(prescaler);

	timer0ClearCompareAFlag();
}

void initTimer1_CTC(uint16_t comparacion, uint16_t prescaler)
{
	// Configurar Timer1 en modo CTC
	TCCR1A = 0;
	TCCR1B = 0;
	TCNT1 = 0;

	OCR1A = comparacion;

	TCCR1B |= (1 << WGM12);

	TCCR1B |= getTimer1PrescalerBits(prescaler);

	timer1ClearCompareAFlag();
}

void timer0SetCompareA(uint8_t comparacion)
{
	OCR0A = comparacion;
}

void timer1SetCompareA(uint16_t comparacion)
{
	OCR1A = comparacion;
}

void timer0Reset(void)
{
	TCNT0 = 0;
}

void timer1Reset(void)
{
	TCNT1 = 0;
}

void timer0Stop(void)
{
	TCCR0B &= ~((1 << CS02) | (1 << CS01) | (1 << CS00));
}

void timer1Stop(void)
{
	TCCR1B &= ~((1 << CS12) | (1 << CS11) | (1 << CS10));
}

uint8_t timer0CompareAFlag(void)
{
	if (TIFR0 & (1 << OCF0A))
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

uint8_t timer1CompareAFlag(void)
{
	if (TIFR1 & (1 << OCF1A))
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

void timer0ClearCompareAFlag(void)
{
	TIFR0 |= (1 << OCF0A);
}

void timer1ClearCompareAFlag(void)
{
	TIFR1 |= (1 << OCF1A);
}

uint8_t timer0_usToTicks(uint32_t tiempo_us, uint16_t prescaler)
{
	uint32_t ticks;

	// Convertir microsegundos a cuentas de Timer0
	ticks = ((tiempo_us * (F_CPU / prescaler)) / 1000000UL);

	if (ticks > 0)
	{
		ticks = ticks - 1;
	}

	if (ticks > 255)
	{
		ticks = 255;
	}

	return (uint8_t)ticks;
}

uint16_t timer1_usToTicks(uint32_t tiempo_us, uint16_t prescaler)
{
	uint32_t ticks;

	// Convertir microsegundos a cuentas de Timer1
	ticks = ((tiempo_us * (F_CPU / prescaler)) / 1000000UL);

	if (ticks > 0)
	{
		ticks = ticks - 1;
	}

	if (ticks > 65535)
	{
		ticks = 65535;
	}

	return (uint16_t)ticks;
}

static uint8_t getTimer0PrescalerBits(uint16_t prescaler)
{
	switch (prescaler)
	{
		case TIMER_PRESCALER_1:
			return (1 << CS00);

		case TIMER_PRESCALER_8:
			return (1 << CS01);

		case TIMER_PRESCALER_64:
			return (1 << CS01) | (1 << CS00);

		case TIMER_PRESCALER_256:
			return (1 << CS02);

		case TIMER_PRESCALER_1024:
			return (1 << CS02) | (1 << CS00);

		default:
			return 0;
	}
}

static uint8_t getTimer1PrescalerBits(uint16_t prescaler)
{
	switch (prescaler)
	{
		case TIMER_PRESCALER_1:
			return (1 << CS10);

		case TIMER_PRESCALER_8:
			return (1 << CS11);

		case TIMER_PRESCALER_64:
			return (1 << CS11) | (1 << CS10);

		case TIMER_PRESCALER_256:
			return (1 << CS12);

		case TIMER_PRESCALER_1024:
			return (1 << CS12) | (1 << CS10);

		default:
			return 0;
	}
}