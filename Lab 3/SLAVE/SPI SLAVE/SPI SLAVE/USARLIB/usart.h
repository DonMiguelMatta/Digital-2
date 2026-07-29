/*
 * usart.h
 *
 * Created: 23/07/2026 06:34:04 p. m.
 *  Author: migue
 */

#ifndef USART_H_
#define USART_H_

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <stdint.h>

#define USART_BAUDRATE 9600UL

void USART_Init(void);
void USART_Transmit(char data);
char USART_Receive(void);
uint8_t USART_Available(void);
void USART_SendString(const char *text);
void USART_SendUint16(uint16_t numero);
void USART_SendInt16(int16_t numero);

#endif /* USART_H_ */
