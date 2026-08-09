/*
 * HC_SR04.c
 *
 * Mide ECHO en microsegundos. No calcula distancia ni nivel.
 */

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include "HC_SR04.h"

#include <avr/io.h>
#include <stddef.h>
#include <util/delay.h>

#define HCSR04_TIMEOUT_US       30000UL
#define HCSR04_TIMER_PRESCALER  8UL
#define HCSR04_TICKS_PER_US \
    (F_CPU / HCSR04_TIMER_PRESCALER / 1000000UL)
#define HCSR04_TIMEOUT_TICKS \
    (HCSR04_TIMEOUT_US * HCSR04_TICKS_PER_US)

static volatile uint8_t *hcsr04_trig_ddr = NULL;
static volatile uint8_t *hcsr04_trig_port = NULL;
static volatile uint8_t *hcsr04_echo_ddr = NULL;
static volatile uint8_t *hcsr04_echo_pin_register = NULL;
static uint8_t hcsr04_trig_pin = 0u;
static uint8_t hcsr04_echo_pin = 0u;
static uint8_t hcsr04_initialized = 0u;

static void HCSR04_TimerStart(void)
{
    TCCR2A = 0u;
    TCCR2B = 0u;
    TCNT2 = 0u;
    TIFR2 = (1u << TOV2);
    TCCR2B = (1u << CS21);
}

static void HCSR04_TimerStop(void)
{
    TCCR2B = 0u;
}

static uint8_t HCSR04_EchoIsHigh(void)
{
    return ((*hcsr04_echo_pin_register &
             (1u << hcsr04_echo_pin)) != 0u) ? 1u : 0u;
}

static uint8_t HCSR04_WaitForState(uint8_t expected_high,
                                   uint32_t max_ticks,
                                   uint32_t *elapsed_ticks)
{
    uint32_t overflow_ticks = 0UL;

    while (HCSR04_EchoIsHigh() != expected_high)
    {
        uint32_t current_ticks;

        if ((TIFR2 & (1u << TOV2)) != 0u)
        {
            TIFR2 = (1u << TOV2);
            overflow_ticks += 256UL;
        }

        current_ticks = overflow_ticks + TCNT2;
        if (current_ticks >= max_ticks)
        {
            *elapsed_ticks = current_ticks;
            return 0u;
        }
    }

    *elapsed_ticks = overflow_ticks + TCNT2;
    return 1u;
}

uint8_t HCSR04_Init(volatile uint8_t *trig_ddr,
                    volatile uint8_t *trig_port,
                    uint8_t trig_pin,
                    volatile uint8_t *echo_ddr,
                    volatile uint8_t *echo_pin_register,
                    uint8_t echo_pin)
{
    if ((trig_ddr == NULL) || (trig_port == NULL) ||
        (echo_ddr == NULL) || (echo_pin_register == NULL) ||
        (trig_pin > 7u) || (echo_pin > 7u) ||
        (HCSR04_TICKS_PER_US == 0UL))
    {
        hcsr04_initialized = 0u;
        return 0u;
    }

    hcsr04_trig_ddr = trig_ddr;
    hcsr04_trig_port = trig_port;
    hcsr04_echo_ddr = echo_ddr;
    hcsr04_echo_pin_register = echo_pin_register;
    hcsr04_trig_pin = trig_pin;
    hcsr04_echo_pin = echo_pin;

    *hcsr04_trig_ddr |= (1u << hcsr04_trig_pin);
    *hcsr04_trig_port &= (uint8_t)~(1u << hcsr04_trig_pin);
    *hcsr04_echo_ddr &= (uint8_t)~(1u << hcsr04_echo_pin);
    hcsr04_initialized = 1u;
    return 1u;
}

uint8_t HCSR04_ReadEchoPulseUS(uint16_t *echo_us)
{
    uint32_t unused_ticks = 0UL;
    uint32_t pulse_ticks = 0UL;
    uint32_t pulse_us;

    if (echo_us == NULL)
    {
        return 0u;
    }

    *echo_us = HCSR04_INVALID_PULSE_US;

    if (hcsr04_initialized == 0u)
    {
        return 0u;
    }

    *hcsr04_trig_port &= (uint8_t)~(1u << hcsr04_trig_pin);
    _delay_us(2);
    *hcsr04_trig_port |= (1u << hcsr04_trig_pin);
    _delay_us(10);
    *hcsr04_trig_port &= (uint8_t)~(1u << hcsr04_trig_pin);

    HCSR04_TimerStart();

    if (HCSR04_WaitForState(1u, HCSR04_TIMEOUT_TICKS,
                            &unused_ticks) == 0u)
    {
        HCSR04_TimerStop();
        return 0u;
    }

    TCNT2 = 0u;
    TIFR2 = (1u << TOV2);

    if (HCSR04_WaitForState(0u, HCSR04_TIMEOUT_TICKS,
                            &pulse_ticks) == 0u)
    {
        HCSR04_TimerStop();
        return 0u;
    }

    HCSR04_TimerStop();
    pulse_us = pulse_ticks / HCSR04_TICKS_PER_US;

    if ((pulse_us == 0UL) || (pulse_us > UINT16_MAX))
    {
        return 0u;
    }

    *echo_us = (uint16_t)pulse_us;
    return 1u;
}
