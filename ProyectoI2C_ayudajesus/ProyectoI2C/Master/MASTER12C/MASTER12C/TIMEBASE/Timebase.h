/*
 * Biblioteca Timebase
 *
 * Author: Miguel Donis 22993 - Ian Farrington 21952
 * Description: Interfaz de tiempo no bloqueante en milisegundos
 */
/****************************************/
// Encabezado (Libraries)

#ifndef TIMEBASE_H_
#define TIMEBASE_H_

#include <stdint.h>

/****************************************/
// Function prototypes

// Inicia Timer0 con periodo de un milisegundo.
void Timebase_Init(void);
// Devuelve los milisegundos acumulados en 32 bits.
uint32_t Timebase_Millis(void);

#endif
