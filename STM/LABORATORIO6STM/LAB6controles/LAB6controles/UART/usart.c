/*
 * Biblioteca USART
 *
 * Author: Miguel Donis 22993 - Ian Farrington 21952
 * Description: Comunicacion serial a 9600 baudios en formato 8N1
 */
/****************************************/
// Encabezado (Libraries)

#include "usart.h"

/****************************************/
// NON-Interrupt subroutines

// Configura USART0 a 9600 baudios, ocho bits, sin paridad y un stop.
void USART_Init(void)
{
    UBRR0H = 0;
    UBRR0L = 103;
    UCSR0A = 0;
    UCSR0B = (1 << RXEN0) | (1 << TXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

// Espera que el registro este libre y transmite un caracter.
void USART_Transmit(char data)
{
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = data;
}

// Espera un dato completo y devuelve el caracter recibido.
char USART_Receive(void)
{
    while (!(UCSR0A & (1 << RXC0)));
    return UDR0;
}

// Devuelve 1 cuando USART0 posee un byte listo para leer.
uint8_t USART_Available(void)
{
    return ((UCSR0A & (1 << RXC0)) != 0);
}

// Transmite una cadena completa hasta el terminador nulo.
void USART_SendString(const char *text)
{
    while (*text)
    {
        USART_Transmit(*text);
        text++;
    }
}

// Convierte un entero sin signo a decimal y lo transmite.
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

// Transmite signo y magnitud decimal de un entero con signo.
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
// Esta interfaz USART trabaja por consulta y no usa interrupciones.
