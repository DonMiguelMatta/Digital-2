/*
 * I2CLIB.c
 *
 * TWI esclavo local para Slave2.
 */

#include "I2CLIB.h"

#include <avr/io.h>

uint8_t I2C_Slave_Init(uint8_t address)
{
    if ((address < 0x08U) || (address > 0x77U))
    {
        return 0U;
    }

    /* A4/PC4 = SDA y A5/PC5 = SCL. */
    DDRC &= (uint8_t)~((1U << DDC4) | (1U << DDC5));
    PORTC &= (uint8_t)~((1U << PORTC4) | (1U << PORTC5));

    TWAMR = 0U;
    TWAR = (uint8_t)(address << 1);
    TWCR = (1U << TWINT) |
           (1U << TWEA)  |
           (1U << TWEN)  |
           (1U << TWIE);

    return 1U;
}
