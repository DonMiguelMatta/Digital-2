#ifndef USART_H_
#define USART_H_

#include <avr/io.h>
#include <stdint.h>

void USART_Init(void);
void USART_Transmit(char data);
char USART_Receive(void);
uint8_t USART_Available(void);
void USART_SendString(const char *text);
void USART_SendUint16(uint16_t numero);
void USART_SendInt16(int16_t numero);

#endif
