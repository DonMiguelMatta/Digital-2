/*
 * Biblioteca ASCII Binario
 *
 * Author: Miguel Donis 22993 - Ian Farrington 21952
 * Description: Interfaz de conversion y salida binaria en LED
 */
/****************************************/
// Encabezado (Libraries)

#ifndef ASCII_BINARIO_H_
#define ASCII_BINARIO_H_

#include <avr/io.h>

/****************************************/
// Function prototypes

// Convierte el caracter al byte que representa su codigo ASCII.
uint8_t ASCII_a_Binario(char caracter);
// Muestra ese byte en ocho LED conectados entre D2 y D9.
void ASCII_En_LEDs_D2_D9(char caracter);

#endif /* ASCII_BINARIO_H_ */
