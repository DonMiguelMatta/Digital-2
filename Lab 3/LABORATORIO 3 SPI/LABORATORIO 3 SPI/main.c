/*
 * Laboratorio 3 SPI MASTER
 * Created: 23-jul
 * Author: Miguel Donis
 * Description: UART - SPI - ADC - contador LEDs
 */

/****************************************/
// Encabezado (Libraries)

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <stdint.h>
#include <util/delay.h>

#include "SPIMASTER/SPIMASTER.h"
#include "UART/UART.h"

#define MODE_UART_VALUE 0
#define MODE_ADC_POTS   1

/****************************************/
// Function prototypes

static void MASTER_LEDS_Init(void);
static void MASTER_LEDS_Write(uint8_t value);
static void BUTTON_Init(void);
static uint8_t BUTTON_WasPressed(void);
static void MASTER_PrintMode(uint8_t mode);
static void MASTER_RunUartMode(void);
static void MASTER_RunAdcMode(void);

/****************************************/
// Main Function

int main(void)
{
    uint8_t mode = MODE_UART_VALUE;
    
    UART_Init(9600);
    SPIMASTER_Init();
    MASTER_LEDS_Init();
    BUTTON_Init();
    
    MASTER_PrintMode(mode);
    
    while (1)
    {
        if (BUTTON_WasPressed())
        {
            mode ^= 1;
            UART_RxUInt8Reset();
            MASTER_PrintMode(mode);
        }
        
        if (mode == MODE_UART_VALUE)
        {
            MASTER_RunUartMode();
        }
        else
        {
            MASTER_RunAdcMode();
        }
        
        _delay_ms(10);
    }
}

/****************************************/
// NON-Interrupt subroutines

// Configura D2-D9 como salidas.
static void MASTER_LEDS_Init(void)
{
    DDRD |= (1 << DDD2) | (1 << DDD3) | (1 << DDD4) | (1 << DDD5) | (1 << DDD6) | (1 << DDD7);
    DDRB |= (1 << DDB0) | (1 << DDB1);
}

// Muestra el valor en LEDs.
static void MASTER_LEDS_Write(uint8_t value)
{
    PORTD = (PORTD & 0x03) | ((value & 0x3F) << 2);
    PORTB = (PORTB & 0xFC) | ((value >> 6) & 0x03);
}

// Configura boton en A4.
static void BUTTON_Init(void)
{
    DDRC &= ~(1 << PC4);
    PORTC |= (1 << PC4);
}

// Detecta presion del boton.
static uint8_t BUTTON_WasPressed(void)
{
    static uint8_t button_released = 1;
    
    if ((!(PINC & (1 << PC4))) && (button_released))
    {
        _delay_ms(25);
        
        if (!(PINC & (1 << PC4)))
        {
            button_released = 0;
            return 1;
        }
    }
    
    if (PINC & (1 << PC4))
    {
        button_released = 1;
    }
    
    return 0;
}

// Muestra modo actual.
static void MASTER_PrintMode(uint8_t mode)
{
    if (mode == MODE_UART_VALUE)
    {
        UART_TxString("\r\nModo UART -> LEDs/SPI\r\nIngrese un numero de 0 a 255:\r\n");
    }
    else
    {
        UART_TxString("\r\nModo ADC -> Potenciometros\r\n");
    }
}

// Lee numero de terminal.
static void MASTER_RunUartMode(void)
{
    uint8_t value = 0;
    uint8_t status = UART_RxUInt8Task(&value);
    
    if (status == UART_RX_READY)
    {
        MASTER_LEDS_Write(value);
        SPIMASTER_SendValue(value);
        
        UART_TxString("Valor enviado: ");
        UART_TxUInt16(value);
        UART_TxString("\r\nIngrese un numero de 0 a 255:\r\n");
    }
    else if (status == UART_RX_ERROR)
    {
        UART_TxString("Valor invalido. Ingrese un numero de 0 a 255:\r\n");
    }
}

// Lee potenciometros por SPI.
static void MASTER_RunAdcMode(void)
{
    static uint8_t sample_counter = 0;
    uint16_t pot1 = 0;
    uint16_t pot2 = 0;
    
    sample_counter++;
    
    if (sample_counter < 25)
    {
        return;
    }
    
    sample_counter = 0;
    pot1 = SPIMASTER_ReadPot(1);
    pot2 = SPIMASTER_ReadPot(2);
    
    UART_TxString("POT1: ");
    UART_TxUInt16(pot1);
    UART_TxString("\tPOT2: ");
    UART_TxUInt16(pot2);
    UART_TxString("\r\n");
}

/****************************************/
// Interrupt routines

// No se usan interrupciones.
