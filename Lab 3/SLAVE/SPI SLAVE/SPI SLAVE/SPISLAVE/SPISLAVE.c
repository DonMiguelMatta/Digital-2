/*
 * SPISLAVE.c
 * Created: 23-jul
 * Author: Miguel Donis 22993 - Ian Farrington 21952
 * Description: Libreria SPI esclavo
 */

/****************************************/
// Encabezado (Libraries)

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>

#include "SPISLAVE.h"

/****************************************/
// Function prototypes

static void spiSetClock(Spi_type stype);

/****************************************/
// Main Function

// No aplica en libreria.

/****************************************/
// NON-Interrupt subroutines

static volatile uint16_t spi_pot1 = 0;
static volatile uint16_t spi_pot2 = 0;
static volatile uint16_t spi_latched_pot1 = 0;
static volatile uint16_t spi_latched_pot2 = 0;
static volatile uint8_t spi_received_value = 0;
static volatile uint8_t spi_wait_led_value = 0;

// Configura divisor SPI.
static void spiSetClock(Spi_type stype)
{
    uint8_t clock = stype & 0x07;
    
    SPCR &= ~((1 << SPR1) | (1 << SPR0));
    SPSR &= ~(1 << SPI2X);
    
    switch (clock)
    {
        case 0:
            SPSR |= (1 << SPI2X);
            break;
            
        case 1:
            break;
            
        case 2:
            SPCR |= (1 << SPR0);
            SPSR |= (1 << SPI2X);
            break;
            
        case 3:
            SPCR |= (1 << SPR0);
            break;
            
        case 4:
            SPCR |= (1 << SPR1);
            SPSR |= (1 << SPI2X);
            break;
            
        case 5:
            SPCR |= (1 << SPR1);
            break;
            
        default:
            SPCR |= (1 << SPR1) | (1 << SPR0);
            break;
    }
}

// Inicializa SPI.
void spiInit(Spi_type stype, SPI_DATA_ORDER sDataOrder, Spi_Clock_Polarity sClockPolarity, Spi_Clock_Phase sClockPhase)
{
    SPCR = 0;
    SPSR &= ~(1 << SPI2X);
    
    if (stype & 0x10)
    {
        DDRB |= (1 << DDB2) | (1 << DDB3) | (1 << DDB5);
        DDRB &= ~(1 << DDB4);
        PORTB |= (1 << PORTB2);
        
        SPCR |= (1 << MSTR);
        spiSetClock(stype);
    }
    else
    {
        DDRB |= (1 << DDB4);
        DDRB &= ~((1 << DDB2) | (1 << DDB3) | (1 << DDB5));
    }
    
    SPCR |= (1 << SPE) | sDataOrder | sClockPolarity | sClockPhase;
}

// Carga dato SPI.
void spiWrite(uint8_t dat)
{
    SPDR = dat;
}

// Revisa dato SPI.
unsigned spiDataReady(void)
{
    if (SPSR & (1 << SPIF))
    {
        return 1;
    }
    
    return 0;
}

// Lee dato SPI.
char spiRead(void)
{
    while (!(SPSR & (1 << SPIF)))
    {
    }
    
    return SPDR;
}

// Transfiere un byte.
uint8_t spiTransfer(uint8_t dat)
{
    SPDR = dat;
    while (!(SPSR & (1 << SPIF)))
    {
    }
    
    return SPDR;
}

// Inicializa SPI esclavo.
void SPISLAVE_Init(void)
{
    spiInit(SPI_SLAVE_MODE, SPI_DATA_ORDER_MSB, SPI_CLOCK_POLARITY_LOW, SPI_CLOCK_PHASE_LEADING);
    SPCR |= (1 << SPIE);
    SPDR = 0x00;
}

// Guarda lecturas ADC.
void SPISLAVE_SetReadings(uint16_t pot1, uint16_t pot2)
{
    uint8_t sreg = SREG;
    cli();
    
    spi_pot1 = pot1 & 0x03FF;
    spi_pot2 = pot2 & 0x03FF;
    
    SREG = sreg;
}

// Devuelve dato recibido.
uint8_t SPISLAVE_GetReceivedValue(void)
{
    uint8_t value = 0;
    uint8_t sreg = SREG;
    cli();
    
    value = spi_received_value;
    
    SREG = sreg;
    return value;
}

/****************************************/
// Interrupt routines

// Recibe byte del maestro.
ISR(SPI_STC_vect)
{
    uint8_t data = SPDR;
    
    if (spi_wait_led_value)
    {
        spi_received_value = data;
        spi_wait_led_value = 0;
        SPDR = spi_received_value;
    }
    else if (data == SPI_CMD_SET_LEDS)
    {
        spi_wait_led_value = 1;
        SPDR = spi_received_value;
    }
    else if (data == SPI_CMD_POT1_HIGH)
    {
        spi_latched_pot1 = spi_pot1;
        SPDR = (uint8_t)((spi_latched_pot1 >> 8) & 0x03);
    }
    else if (data == SPI_CMD_POT1_LOW)
    {
        SPDR = (uint8_t)(spi_latched_pot1 & 0xFF);
    }
    else if (data == SPI_CMD_POT2_HIGH)
    {
        spi_latched_pot2 = spi_pot2;
        SPDR = (uint8_t)((spi_latched_pot2 >> 8) & 0x03);
    }
    else if (data == SPI_CMD_POT2_LOW)
    {
        SPDR = (uint8_t)(spi_latched_pot2 & 0xFF);
    }
    else
    {
        SPDR = spi_received_value;
    }
}
