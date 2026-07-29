/*
 * UART.c
 * Created: 23-jul
 * Author: Miguel Donis
 * Description: Libreria UART
 */

/****************************************/
// Encabezado (Libraries)

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <stdint.h>

#include "UART.h"

/****************************************/
// Function prototypes

// Prototipos en UART.h.

/****************************************/
// Main Function

// No aplica en libreria.

/****************************************/
// NON-Interrupt subroutines

static uint16_t uart_rx_number = 0;
static uint8_t uart_rx_has_digits = 0;

// Inicializa UART.
void UART_Init(uint32_t baudrate)
{
    uint16_t ubrr = (uint16_t)((F_CPU / (16UL * baudrate)) - 1UL);
    
    UBRR0H = (uint8_t)(ubrr >> 8);
    UBRR0L = (uint8_t)ubrr;
    
    UCSR0A = 0;
    UCSR0B = (1 << RXEN0) | (1 << TXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

// Envia un caracter.
void UART_TxChar(char data)
{
    while (!(UCSR0A & (1 << UDRE0)))
    {
    }
    
    UDR0 = data;
}

// Recibe un caracter.
char UART_RxChar(void)
{
    while (!(UCSR0A & (1 << RXC0)))
    {
    }
    
    return UDR0;
}

// Revisa dato recibido.
uint8_t UART_Available(void)
{
    return ((UCSR0A & (1 << RXC0)) != 0);
}

// Envia texto.
void UART_TxString(const char *text)
{
    while (*text)
    {
        UART_TxChar(*text);
        text++;
    }
}

// Envia numero entero.
void UART_TxUInt16(uint16_t value)
{
    char digits[5];
    uint8_t index = 0;
    
    if (value == 0)
    {
        UART_TxChar('0');
        return;
    }
    
    while ((value > 0) && (index < sizeof(digits)))
    {
        digits[index] = (char)('0' + (value % 10));
        value /= 10;
        index++;
    }
    
    while (index > 0)
    {
        index--;
        UART_TxChar(digits[index]);
    }
}

// Recibe numero 0-255.
uint8_t UART_RxUInt8(uint8_t *value)
{
    while (1)
    {
        uint8_t status = UART_RxUInt8Task(value);
        
        if (status == UART_RX_READY)
        {
            return 1;
        }
        else if (status == UART_RX_ERROR)
        {
            return 0;
        }
    }
}

// Reinicia numero recibido.
void UART_RxUInt8Reset(void)
{
    uart_rx_number = 0;
    uart_rx_has_digits = 0;
}

// Lee numero sin bloquear.
uint8_t UART_RxUInt8Task(uint8_t *value)
{
    char data;
    
    if (!UART_Available())
    {
        return UART_RX_WAITING;
    }
    
    data = UART_RxChar();
    
    if ((data >= '0') && (data <= '9'))
    {
        UART_TxChar(data);
        uart_rx_number = (uint16_t)((uart_rx_number * 10) + (data - '0'));
        uart_rx_has_digits = 1;
        
        if (uart_rx_number > 255)
        {
            UART_TxString("\r\n");
            UART_RxUInt8Reset();
            return UART_RX_ERROR;
        }
        
        return UART_RX_WAITING;
    }
    else if ((data == '\r') || (data == '\n'))
    {
        if (uart_rx_has_digits)
        {
            UART_TxString("\r\n");
            *value = (uint8_t)uart_rx_number;
            UART_RxUInt8Reset();
            return UART_RX_READY;
        }
        
        return UART_RX_WAITING;
    }
    else if ((data == 8) || (data == 127))
    {
        UART_TxString("\r\n");
        UART_RxUInt8Reset();
        return UART_RX_ERROR;
    }
    
    UART_TxString("\r\n");
    UART_RxUInt8Reset();
    return UART_RX_ERROR;
}

/****************************************/
// Interrupt routines

// No se usan interrupciones.
