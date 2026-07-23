/*
 * ascii_binario.h
 *
 * Created: 25/04/2026 06:54:11 p. m.
 *  Author: migue
 */ 


#ifndef ASCII_BINARIO_H_
#define ASCII_BINARIO_H_

#include <avr/io.h>

uint8_t ASCII_a_Binario(char caracter);
void ASCII_En_LEDs_D2_D9(char caracter);

#endif /* ASCII_BINARIO_H_ */