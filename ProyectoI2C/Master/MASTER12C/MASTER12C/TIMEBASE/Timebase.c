/*
 * Biblioteca Timebase
 *
 * Author: Miguel Donis 22993 - Ian Farrington 21952
 * Description: Base de tiempo de un milisegundo mediante Timer0
 */
/****************************************/
// Encabezado (Libraries)

#include "Timebase.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/atomic.h>

static volatile uint32_t timebase_milliseconds = 0UL;

/****************************************/
// NON-Interrupt subroutines

// Configura Timer0 en CTC para generar una interrupcion cada 1 ms.
void Timebase_Init(void)
{
    TCCR0A = (1U << WGM01);
    TCCR0B = (1U << CS01) | (1U << CS00);
    OCR0A = 249U;
    TCNT0 = 0U;
    TIMSK0 |= (1U << OCIE0A);
}

// Copia atomicamente los milisegundos transcurridos desde el inicio.
uint32_t Timebase_Millis(void)
{
    uint32_t milliseconds;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        milliseconds = timebase_milliseconds;
    }

    return milliseconds;
}

/****************************************/
// Interrupt routines

// Incrementa el contador global en cada milisegundo.
ISR(TIMER0_COMPA_vect)
{
    timebase_milliseconds++;
}
