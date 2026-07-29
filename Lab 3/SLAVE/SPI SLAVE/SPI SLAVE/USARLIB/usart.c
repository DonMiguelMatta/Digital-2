/*
 * usart.c
 * Created: 23-jul
 * Author: Miguel Donis
 * Description: Libreria USART
 */

/****************************************/
// Encabezado (Libraries)

#include "usart.h"

/****************************************/
// Function prototypes

// Prototipos en usart.h.

/****************************************/
// Main Function

// No aplica en libreria.

/****************************************/
// NON-Interrupt subroutines

// Inicializa USART.
void USART_Init(void)
{
    uint16_t ubrr = (uint16_t)((F_CPU / (16UL * USART_BAUDRATE)) - 1UL);
    
    UBRR0H = (uint8_t)(ubrr >> 8);
    UBRR0L = (uint8_t)ubrr;
    UCSR0A = 0;
    UCSR0B = (1 << RXEN0) | (1 << TXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

// Envia un caracter.
void USART_Transmit(char data)
{
    while (!(UCSR0A & (1 << UDRE0)))
    {
    }
    
    UDR0 = data;
}

// Recibe un caracter.
char USART_Receive(void)
{
    while (!(UCSR0A & (1 << RXC0)))
    {
    }
    
    return UDR0;
}

// Revisa dato recibido.
uint8_t USART_Available(void)
{
    return ((UCSR0A & (1 << RXC0)) != 0);
}

// Envia texto.
void USART_SendString(const char *text)
{
    while (*text)
    {
        USART_Transmit(*text);
        text++;
    }
}

// Envia numero entero.
void USART_SendUint16(uint16_t numero)
{
    char buffer[6];
    uint8_t i = 0;
    uint8_t j;
    char temp;

    if (numero == 0)
    {
        USART_Transmit('0');
        return;
    }

    while (numero > 0)
    {
        buffer[i] = (numero % 10) + '0';
        numero /= 10;
        i++;
    }

    for (j = 0; j < i / 2; j++)
    {
        temp = buffer[j];
        buffer[j] = buffer[i - 1 - j];
        buffer[i - 1 - j] = temp;
    }

    buffer[i] = '\0';
    USART_SendString(buffer);
}

// Envia numero con signo.
void USART_SendInt16(int16_t numero)
{
    uint16_t valor;

    if (numero < 0)
    {
        USART_Transmit('-');
        valor = (uint16_t)(-(numero + 1)) + 1;
    }
    else
    {
        valor = (uint16_t)numero;
    }

    USART_SendUint16(valor);
}

/****************************************/
// Interrupt routines

// No se usan interrupciones.
