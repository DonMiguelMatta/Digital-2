/*
 * Biblioteca USART
 *
 * Author: Miguel Donis 22993 - Ian Farrington 21952
 * Description: Interfaz serial bloqueante para USART0
 */
/****************************************/
// Encabezado (Libraries)

#ifndef USART_H_
#define USART_H_

#include <avr/io.h>
#include <stdint.h>

/****************************************/
// Function prototypes

// Inicializa USART0 a 9600 baudios en formato 8N1.
void USART_Init(void);
// Transmite un caracter.
void USART_Transmit(char data);
// Espera y devuelve un caracter recibido.
char USART_Receive(void);
// Indica si existe un caracter disponible.
uint8_t USART_Available(void);
// Transmite una cadena terminada en nulo.
void USART_SendString(const char *text);
// Transmite un entero sin signo en decimal.
void USART_SendUint16(uint16_t numero);
// Transmite un entero con signo en decimal.
void USART_SendInt16(int16_t numero);

#endif
