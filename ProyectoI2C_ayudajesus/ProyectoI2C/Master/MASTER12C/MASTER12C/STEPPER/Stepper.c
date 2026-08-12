/*
 * Stepper.c
 *
 * Secuenciador no bloqueante por Timer2 para ULN2003 y motor 28BYJ-48.
 */

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include "Stepper.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <stddef.h>
#include <util/atomic.h>

static const uint8_t half_step_sequence[8] =
{
    0x01u, 0x03u, 0x02u, 0x06u,
    0x04u, 0x0Cu, 0x08u, 0x09u
};

static const uint8_t full_step_sequence[4] =
{
    0x09u, 0x0Cu, 0x06u, 0x03u
};

static StepperConfig stepper_config;
static volatile StepperState stepper_state;
static volatile uint8_t stepper_index = 0u;
static uint8_t stepper_sequence_length = 8u;
static StepperMode stepper_mode = STEPPER_MODE_HALF_STEP;
static uint8_t stepper_initialized = 0u;

static uint8_t Stepper_ConfigureRate(uint16_t steps_per_second)
{
    static const uint16_t prescalers[7] =
    {
        1u, 8u, 32u, 64u, 128u, 256u, 1024u
    };
    static const uint8_t clock_bits[7] =
    {
        (1u << CS20),
        (1u << CS21),
        (1u << CS21) | (1u << CS20),
        (1u << CS22),
        (1u << CS22) | (1u << CS20),
        (1u << CS22) | (1u << CS21),
        (1u << CS22) | (1u << CS21) | (1u << CS20)
    };
    uint8_t index;

    if (steps_per_second == 0u)
    {
        return 0u;
    }

    for (index = 0u; index < 7u; index++)
    {
        uint32_t divider =
            (uint32_t)prescalers[index] * steps_per_second;
        uint32_t counts = F_CPU / divider;

        if ((counts >= 2UL) && (counts <= 256UL))
        {
            OCR2A = (uint8_t)(counts - 1UL);
            TCCR2B = clock_bits[index];
            return 1u;
        }
    }

    return 0u;
}

static void Stepper_WritePattern(uint8_t pattern)
{
    uint8_t phase;

    for (phase = 0u; phase < 4u; phase++)
    {
        uint8_t mask = (uint8_t)(1u << stepper_config.pin[phase]);

        if ((pattern & (1u << phase)) != 0u)
        {
            *stepper_config.port[phase] |= mask;
        }
        else
        {
            *stepper_config.port[phase] &= (uint8_t)~mask;
        }
    }
}

static void Stepper_ApplyCurrentStep(void)
{
    uint8_t pattern =
        (stepper_mode == STEPPER_MODE_HALF_STEP) ?
        half_step_sequence[stepper_index] :
        full_step_sequence[stepper_index];

    Stepper_WritePattern(pattern);
}

static uint8_t Stepper_Start(StepperDirection direction,
                             uint16_t steps,
                             uint16_t steps_per_second,
                             uint8_t continuous)
{
    if ((stepper_initialized == 0u) ||
        ((direction != STEPPER_DIRECTION_FORWARD) &&
         (direction != STEPPER_DIRECTION_REVERSE)) ||
        ((continuous == 0u) && (steps == 0u)))
    {
        return 0u;
    }

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        TIMSK2 &= (uint8_t)~(1u << OCIE2A);
        TCCR2B = 0u;
        TCNT2 = 0u;

        if (Stepper_ConfigureRate(steps_per_second) == 0u)
        {
            return 0u;
        }

        stepper_state.direction = direction;
        stepper_state.steps_per_second = steps_per_second;
        stepper_state.remaining_steps = steps;
        stepper_state.continuous = continuous;
        stepper_state.running = 1u;
        TIFR2 = (1u << OCF2A);
        TIMSK2 |= (1u << OCIE2A);
    }

    return 1u;
}

uint8_t Stepper_Init(const StepperConfig *config, StepperMode mode)
{
    uint8_t phase;

    if ((config == NULL) ||
        ((mode != STEPPER_MODE_FULL_STEP) &&
         (mode != STEPPER_MODE_HALF_STEP)))
    {
        return 0u;
    }

    for (phase = 0u; phase < 4u; phase++)
    {
        if ((config->ddr[phase] == NULL) ||
            (config->port[phase] == NULL) ||
            (config->pin[phase] > 7u))
        {
            return 0u;
        }
    }

    stepper_config = *config;
    stepper_mode = mode;
    stepper_sequence_length =
        (mode == STEPPER_MODE_HALF_STEP) ? 8u : 4u;

    for (phase = 0u; phase < 4u; phase++)
    {
        *stepper_config.ddr[phase] |=
            (uint8_t)(1u << stepper_config.pin[phase]);
    }

    TCCR2A = (1u << WGM21);
    TCCR2B = 0u;
    TCNT2 = 0u;
    TIMSK2 &= (uint8_t)~(1u << OCIE2A);
    Stepper_WritePattern(0u);

    stepper_index = 0u;
    stepper_state.direction = STEPPER_DIRECTION_FORWARD;
    stepper_state.steps_per_second = 0u;
    stepper_state.remaining_steps = 0u;
    stepper_state.running = 0u;
    stepper_state.continuous = 0u;
    stepper_initialized = 1u;
    return 1u;
}

uint8_t Stepper_RunContinuous(StepperDirection direction,
                              uint16_t steps_per_second)
{
    return Stepper_Start(direction, 0u, steps_per_second, 1u);
}

uint8_t Stepper_Move(StepperDirection direction,
                     uint16_t steps,
                     uint16_t steps_per_second)
{
    return Stepper_Start(direction, steps, steps_per_second, 0u);
}

void Stepper_Stop(uint8_t release_coils)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        TIMSK2 &= (uint8_t)~(1u << OCIE2A);
        TCCR2B = 0u;
        stepper_state.running = 0u;
        stepper_state.continuous = 0u;
        stepper_state.remaining_steps = 0u;
        stepper_state.steps_per_second = 0u;
    }

    if ((stepper_initialized != 0u) && (release_coils != 0u))
    {
        Stepper_WritePattern(0u);
    }
}

uint8_t Stepper_IsRunning(void)
{
    uint8_t running;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        running = stepper_state.running;
    }

    return running;
}

void Stepper_GetState(StepperState *state)
{
    if (state == NULL)
    {
        return;
    }

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        *state = stepper_state;
    }
}

ISR(TIMER2_COMPA_vect)
{
    if (stepper_state.running == 0u)
    {
        return;
    }

    if (stepper_state.direction == STEPPER_DIRECTION_FORWARD)
    {
        stepper_index++;
        if (stepper_index >= stepper_sequence_length)
        {
            stepper_index = 0u;
        }
    }
    else if (stepper_index == 0u)
    {
        stepper_index = (uint8_t)(stepper_sequence_length - 1u);
    }
    else
    {
        stepper_index--;
    }

    Stepper_ApplyCurrentStep();

    if (stepper_state.continuous == 0u)
    {
        if (stepper_state.remaining_steps > 0u)
        {
            stepper_state.remaining_steps--;
        }

        if (stepper_state.remaining_steps == 0u)
        {
            TIMSK2 &= (uint8_t)~(1u << OCIE2A);
            TCCR2B = 0u;
            stepper_state.running = 0u;
            stepper_state.steps_per_second = 0u;
        }
    }
}
