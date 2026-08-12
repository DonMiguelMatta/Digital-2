/*
 * Biblioteca ESPUART
 *
 * Author: Miguel Donis 22993 - Ian Farrington 21952
 * Description: Recepcion UART por software desde el ESP32
 */
/****************************************/
// Encabezado (Libraries)

#ifndef ESPUART_H_
#define ESPUART_H_

#include <stdint.h>

/****************************************/
// Function prototypes

// Configura D12 como receptor UART a 9600 baudios.
void ESPUART_Init(void);
// Indica si existe un byte recibido desde el ESP32.
uint8_t ESPUART_Available(void);
// Lee el siguiente byte recibido sin bloquear.
char ESPUART_ReadChar(void);
// Informa y limpia un desbordamiento del buffer.
uint8_t ESPUART_GetAndClearOverflow(void);

#endif /* ESPUART_H_ */
