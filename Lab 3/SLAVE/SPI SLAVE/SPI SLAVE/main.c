/*
 * Laboratorio 3 SPI SLAVE
 * Created: 23-jul
 * Author: Miguel Donis
 * Description: SPI - ADC - contador LEDs
 */

/****************************************/
// Encabezado (Libraries)

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <util/delay.h>

#include "ADC/ADC.h"
#include "SPISLAVE/SPISLAVE.h"

/****************************************/
// Function prototypes

static void SLAVE_LEDS_Init(void);
static void SLAVE_LEDS_Write(uint8_t value);

/****************************************/
// Main Function

int main(void)
{
    ADC_Init();
    SPISLAVE_Init();
    SLAVE_LEDS_Init();
    sei();
    
    while (1)
    {
        uint16_t pot1 = ADC_Read(0);
        uint16_t pot2 = ADC_Read(1);
        
        SPISLAVE_SetReadings(pot1, pot2);
        SLAVE_LEDS_Write(SPISLAVE_GetReceivedValue());
        _delay_ms(10);
    }
}

/****************************************/
// NON-Interrupt subroutines

// Configura D2-D9 como salidas.
static void SLAVE_LEDS_Init(void)
{
    DDRD |= (1 << DDD2) | (1 << DDD3) | (1 << DDD4) | (1 << DDD5) | (1 << DDD6) | (1 << DDD7);
    DDRB |= (1 << DDB0) | (1 << DDB1);
}

// Muestra el valor en LEDs.
static void SLAVE_LEDS_Write(uint8_t value)
{
    PORTD = (PORTD & 0x03) | ((value & 0x3F) << 2);
    PORTB = (PORTB & 0xFC) | ((value >> 6) & 0x03);
}

/****************************************/
// Interrupt routines

// Interrupcion en libreria SPI.
