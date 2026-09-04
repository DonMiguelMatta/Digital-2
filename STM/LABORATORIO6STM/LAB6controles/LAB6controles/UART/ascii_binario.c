/*
 * Biblioteca ASCII Binario
 *
 * Author: Miguel Donis 22993 - Ian Farrington 21952
 * Description: Conversion ASCII y visualizacion binaria mediante LED
 */
/****************************************/
// Encabezado (Libraries)

#include "ascii_binario.h"
#include <util/delay.h>

/****************************************/
// NON-Interrupt subroutines

// Devuelve directamente el valor binario de un caracter ASCII.
uint8_t ASCII_a_Binario(char caracter)
{
	return (uint8_t)caracter;
}

// Presenta los ocho bits del caracter en los pines D2-D9.
void ASCII_En_LEDs_D2_D9(char caracter)
{
	uint8_t dato;

	dato = ASCII_a_Binario(caracter);

	// Apaga todos los leds
	PORTD &= ~((1 << PORTD2) | (1 << PORTD3) | (1 << PORTD4) |
	(1 << PORTD5) | (1 << PORTD6) | (1 << PORTD7));
	PORTB &= ~((1 << PORTB0) | (1 << PORTB1));

	// Espera 15 ms
	_delay_ms(15);

	// Muestra nuevo ASCII
	PORTD |= ((dato & 0x3F) << 2);
	PORTB |= ((dato >> 6) & 0x03);
}

/****************************************/
// Interrupt routines
// Esta biblioteca no utiliza interrupciones.
