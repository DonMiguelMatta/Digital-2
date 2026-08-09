/*
 * Servo.c
 *
 * Timer1 genera una senal de 50 Hz sobre un pin digital configurable.
 */

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include "Servo.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <stddef.h>
#include <util/atomic.h>

#define SERVO_TIMER_TOP       39999u
#define SERVO_MIN_PULSE_TICKS 2000u
#define SERVO_MAX_PULSE_TICKS 4000u

static volatile uint8_t *servo_port = NULL;
static uint8_t servo_pin_mask = 0u;
static volatile uint8_t servo_angle = SERVO_MIN_ANGLE;
static volatile uint8_t servo_initialized = 0u;

static uint16_t Servo_AngleToTicks(uint8_t angle)
{
    uint32_t pulse_ticks = SERVO_MIN_PULSE_TICKS;

    pulse_ticks +=
        ((uint32_t)(SERVO_MAX_PULSE_TICKS -
                    SERVO_MIN_PULSE_TICKS) * angle) /
        SERVO_MAX_ANGLE;

    return (uint16_t)pulse_ticks;
}

uint8_t Servo_Init(volatile uint8_t *ddr,
                   volatile uint8_t *port,
                   uint8_t pin)
{
    if ((ddr == NULL) || (port == NULL) || (pin > 7u))
    {
        return 0u;
    }

    servo_port = port;
    servo_pin_mask = (uint8_t)(1u << pin);
    *ddr |= servo_pin_mask;
    *servo_port &= (uint8_t)~servo_pin_mask;

    TCCR1A = 0u;
    TCCR1B = 0u;
    TCNT1 = 0u;
    ICR1 = SERVO_TIMER_TOP;
    OCR1A = SERVO_MIN_PULSE_TICKS;
    TIFR1 = (1u << TOV1) | (1u << OCF1A);

    TCCR1A = (1u << WGM11);
    TCCR1B = (1u << WGM13) | (1u << WGM12) |
             (1u << CS11);
    TIMSK1 = (1u << TOIE1) | (1u << OCIE1A);

    servo_angle = SERVO_MIN_ANGLE;
    servo_initialized = 1u;
    return 1u;
}

uint8_t Servo_SetAngle(uint8_t angle)
{
    uint16_t ticks;

    if ((servo_initialized == 0u) || (angle > SERVO_MAX_ANGLE))
    {
        return 0u;
    }

    ticks = Servo_AngleToTicks(angle);

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        OCR1A = ticks;
        servo_angle = angle;
    }

    return 1u;
}

uint8_t Servo_GetAngle(void)
{
    uint8_t angle;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        angle = servo_angle;
    }

    return angle;
}

ISR(TIMER1_OVF_vect)
{
    if (servo_port != NULL)
    {
        *servo_port |= servo_pin_mask;
    }
}

ISR(TIMER1_COMPA_vect)
{
    if (servo_port != NULL)
    {
        *servo_port &= (uint8_t)~servo_pin_mask;
    }
}
