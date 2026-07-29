/*
 * UART.h
 *
 * Created: 23/07/2026
 *  Author: migue
 */

#ifndef UART_H_
#define UART_H_

#include <stdint.h>

#define UART_RX_WAITING 0
#define UART_RX_READY   1
#define UART_RX_ERROR   2

void UART_Init(uint32_t baudrate);
void UART_TxChar(char data);
char UART_RxChar(void);
uint8_t UART_Available(void);
void UART_TxString(const char *text);
void UART_TxUInt16(uint16_t value);
uint8_t UART_RxUInt8(uint8_t *value);
void UART_RxUInt8Reset(void);
uint8_t UART_RxUInt8Task(uint8_t *value);

#endif /* UART_H_ */
