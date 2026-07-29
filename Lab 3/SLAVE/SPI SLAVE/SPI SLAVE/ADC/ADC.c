/*
 * ADC.c
 * Created: 23-jul
 * Author: Miguel Donis
 * Description: Libreria ADC
 */

/****************************************/
// Encabezado (Libraries)

#include <avr/io.h>
#include <stdint.h>

#include "ADC.h"

/****************************************/
// Function prototypes

// Prototipos en ADC.h.

/****************************************/
// Main Function

// No aplica en libreria.

/****************************************/
// NON-Interrupt subroutines

// Inicializa ADC.
void ADC_Init(void)
{
    ADMUX = (1 << REFS0);
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
    DIDR0 = (1 << ADC0D) | (1 << ADC1D);
}

// Lee canal ADC.
uint16_t ADC_Read(uint8_t channel)
{
    channel &= 0x07;
    ADMUX = (ADMUX & 0xF0) | channel;
    
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC))
    {
    }
    
    return ADC;
}

/****************************************/
// Interrupt routines

// No se usan interrupciones.
