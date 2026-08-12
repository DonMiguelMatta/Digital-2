/*
 * Biblioteca L298N
 *
 * Author: Miguel Donis 22993 - Ian Farrington 21952
 * Description: Control de un canal L298N con direccion y PWM por Timer0
 */
/****************************************/
// Encabezado (Libraries)

#include "L298N.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <stddef.h>
#include <util/atomic.h>

#define L298N_PWM_TIMER_PRESCALER_BITS \
    ((1U << CS01) | (1U << CS00))

static volatile uint8_t *l298n_enable_port = NULL;
static volatile uint8_t *l298n_in1_port = NULL;
static volatile uint8_t *l298n_in2_port = NULL;

static uint8_t l298n_enable_mask = 0U;
static uint8_t l298n_in1_mask = 0U;
static uint8_t l298n_in2_mask = 0U;

static volatile uint8_t l298n_direction =
    (uint8_t)L298N_DIRECTION_FORWARD;
static volatile uint8_t l298n_pwm = 0U;
static volatile uint8_t l298n_initialized = 0U;

// Comprueba que dos funciones no intenten utilizar exactamente el mismo pin.
static uint8_t L298N_PinIsRepeated(volatile uint8_t *first_ddr,
                                  uint8_t first_pin,
                                  volatile uint8_t *second_ddr,
                                  uint8_t second_pin)
{
    return (uint8_t)((first_ddr == second_ddr) &&
                     (first_pin == second_pin));
}

// Detiene Timer0 y fuerza ENA a nivel bajo.
static void L298N_DisablePWM(void)
{
    TCCR0B = 0U;
    TIMSK0 &= (uint8_t)~((1U << TOIE0) | (1U << OCIE0A));
    *l298n_enable_port &= (uint8_t)~l298n_enable_mask;
}

// Aplica nivel fijo para 0/255 o PWM de 976 Hz para valores intermedios.
static void L298N_ApplyPWM(uint8_t pwm)
{
    L298N_DisablePWM();
    TCCR0A = 0U;
    TCNT0 = 0U;
    OCR0A = pwm;
    TIFR0 = (1U << TOV0) | (1U << OCF0A);

    if (pwm == 0U)
    {
        return;
    }

    if (pwm == 255U)
    {
        *l298n_enable_port |= l298n_enable_mask;
        return;
    }

    *l298n_enable_port |= l298n_enable_mask;
    TIMSK0 |= (1U << TOIE0) | (1U << OCIE0A);
    TCCR0B = L298N_PWM_TIMER_PRESCALER_BITS;
}

// Cambia IN1 e IN2 segun el sentido de giro solicitado.
static void L298N_ApplyDirection(L298NDirection direction)
{
    if (direction == L298N_DIRECTION_FORWARD)
    {
        *l298n_in1_port |= l298n_in1_mask;
        *l298n_in2_port &= (uint8_t)~l298n_in2_mask;
    }
    else
    {
        *l298n_in1_port &= (uint8_t)~l298n_in1_mask;
        *l298n_in2_port |= l298n_in2_mask;
    }
}

/****************************************/
// NON-Interrupt subroutines

// Configura los tres pines como salida y reserva Timer0 para el PWM.
uint8_t L298N_Init(volatile uint8_t *enable_ddr,
                   volatile uint8_t *enable_port,
                   uint8_t enable_pin,
                   volatile uint8_t *in1_ddr,
                   volatile uint8_t *in1_port,
                   uint8_t in1_pin,
                   volatile uint8_t *in2_ddr,
                   volatile uint8_t *in2_port,
                   uint8_t in2_pin)
{
    if ((enable_ddr == NULL) || (enable_port == NULL) ||
        (in1_ddr == NULL) || (in1_port == NULL) ||
        (in2_ddr == NULL) || (in2_port == NULL) ||
        (enable_pin > 7U) || (in1_pin > 7U) || (in2_pin > 7U))
    {
        return 0U;
    }

    if ((L298N_PinIsRepeated(enable_ddr, enable_pin,
                             in1_ddr, in1_pin) != 0U) ||
        (L298N_PinIsRepeated(enable_ddr, enable_pin,
                             in2_ddr, in2_pin) != 0U) ||
        (L298N_PinIsRepeated(in1_ddr, in1_pin,
                             in2_ddr, in2_pin) != 0U))
    {
        return 0U;
    }

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        l298n_enable_port = enable_port;
        l298n_in1_port = in1_port;
        l298n_in2_port = in2_port;
        l298n_enable_mask = (uint8_t)(1U << enable_pin);
        l298n_in1_mask = (uint8_t)(1U << in1_pin);
        l298n_in2_mask = (uint8_t)(1U << in2_pin);

        *enable_ddr |= l298n_enable_mask;
        *in1_ddr |= l298n_in1_mask;
        *in2_ddr |= l298n_in2_mask;

        *l298n_enable_port &= (uint8_t)~l298n_enable_mask;
        *l298n_in1_port &= (uint8_t)~l298n_in1_mask;
        *l298n_in2_port &= (uint8_t)~l298n_in2_mask;

        TCCR0A = 0U;
        TCCR0B = 0U;
        TCNT0 = 0U;
        TIMSK0 &= (uint8_t)~((1U << TOIE0) | (1U << OCIE0A));
        TIFR0 = (1U << TOV0) | (1U << OCF0A);

        l298n_direction = (uint8_t)L298N_DIRECTION_FORWARD;
        l298n_pwm = 0U;
        l298n_initialized = 1U;
    }

    return 1U;
}

// Deshabilita ENA, cambia la direccion y luego aplica la nueva velocidad.
uint8_t L298N_Set(L298NDirection direction, uint8_t pwm)
{
    if ((l298n_initialized == 0U) ||
        ((direction != L298N_DIRECTION_FORWARD) &&
         (direction != L298N_DIRECTION_REVERSE)))
    {
        return 0U;
    }

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        L298N_DisablePWM();
        L298N_ApplyDirection(direction);
        l298n_direction = (uint8_t)direction;
        l298n_pwm = pwm;
        L298N_ApplyPWM(pwm);
    }

    return 1U;
}

// Detiene el PWM y deja IN1 e IN2 en bajo para una parada libre.
void L298N_Stop(void)
{
    if (l298n_initialized == 0U)
    {
        return;
    }

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        l298n_pwm = 0U;
        L298N_DisablePWM();
        *l298n_in1_port &= (uint8_t)~l298n_in1_mask;
        *l298n_in2_port &= (uint8_t)~l298n_in2_mask;
    }
}

// Entrega el estado logico sin exponer registros del hardware.
void L298N_GetState(L298NState *state)
{
    if (state == NULL)
    {
        return;
    }

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        state->direction = l298n_direction;
        state->pwm = l298n_pwm;
        state->enabled = (uint8_t)(l298n_pwm != 0U);
    }
}

/****************************************/
// Interrupt routines

// Coloca ENA en alto al comenzar cada periodo de PWM intermedio.
ISR(TIMER0_OVF_vect)
{
    if ((l298n_initialized != 0U) &&
        (l298n_pwm > 0U) && (l298n_pwm < 255U))
    {
        *l298n_enable_port |= l298n_enable_mask;
    }
}

// Coloca ENA en bajo cuando el contador alcanza el ciclo de trabajo.
ISR(TIMER0_COMPA_vect)
{
    if ((l298n_initialized != 0U) && (l298n_pwm < 255U))
    {
        *l298n_enable_port &= (uint8_t)~l298n_enable_mask;
    }
}
