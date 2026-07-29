/*
 * SPIMASTER.c
 * Created: 23-jul
 * Author: Miguel Donis
 * Description: Libreria SPI maestro
 */

/****************************************/
// Encabezado (Libraries)

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <stdint.h>
#include <util/delay.h>

#include "SPIMASTER.h"

/****************************************/
// Function prototypes

static void SPIMASTER_SelectSlave(void);
static void SPIMASTER_ReleaseSlave(void);

/****************************************/
// Main Function

// No aplica en libreria.

/****************************************/
// NON-Interrupt subroutines

// Activa el esclavo.
static void SPIMASTER_SelectSlave(void)
{
    PORTB &= ~(1 << PORTB2);
}

// Desactiva el esclavo.
static void SPIMASTER_ReleaseSlave(void)
{
    PORTB |= (1 << PORTB2);
}

// Inicializa SPI maestro.
void SPIMASTER_Init(void)
{
    DDRB |= (1 << DDB2) | (1 << DDB3) | (1 << DDB5);
    DDRB &= ~(1 << DDB4);
    
    PORTB |= (1 << PORTB2);
    
    SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR1) | (1 << SPR0);
    SPSR &= ~(1 << SPI2X);
}

// Envia y recibe un byte.
uint8_t SPIMASTER_Transfer(uint8_t data)
{
    SPDR = data;
    while (!(SPSR & (1 << SPIF)))
    {
    }
    
    return SPDR;
}

// Lee un byte por comando.
uint8_t SPIMASTER_ReadByte(uint8_t command)
{
    uint8_t data = 0;
    
    SPIMASTER_SelectSlave();
    (void)SPIMASTER_Transfer(command);
    _delay_us(10);
    data = SPIMASTER_Transfer(SPI_DUMMY_BYTE);
    SPIMASTER_ReleaseSlave();
    
    return data;
}

// Envia valor al esclavo.
void SPIMASTER_SendValue(uint8_t value)
{
    SPIMASTER_SelectSlave();
    _delay_us(10);
    (void)SPIMASTER_Transfer(SPI_CMD_SET_LEDS);
    _delay_us(10);
    (void)SPIMASTER_Transfer(value);
    _delay_us(10);
    SPIMASTER_ReleaseSlave();
}

// Lee potenciometro remoto.
uint16_t SPIMASTER_ReadPot(uint8_t pot)
{
    uint8_t high = 0;
    uint8_t low = 0;
    
    if (pot == 1)
    {
        high = SPIMASTER_ReadByte(SPI_CMD_POT1_HIGH);
        low = SPIMASTER_ReadByte(SPI_CMD_POT1_LOW);
    }
    else
    {
        high = SPIMASTER_ReadByte(SPI_CMD_POT2_HIGH);
        low = SPIMASTER_ReadByte(SPI_CMD_POT2_LOW);
    }
    
    return (((uint16_t)high << 8) | low) & 0x03FF;
}

/****************************************/
// Interrupt routines

// No se usan interrupciones.
