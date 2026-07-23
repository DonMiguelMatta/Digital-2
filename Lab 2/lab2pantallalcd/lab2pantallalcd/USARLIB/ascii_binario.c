/*
 * ascii_binario.c
 *
 * Created: 25/04/2026 06:53:57 p. m.
 *  Author: migue
 */ 
#include "ascii_binario.h"

//convertir a ascii
uint8_t ASCII_a_Binario(char caracter)
{
	return (uint8_t)caracter;
}

//enviar 
void ASCII_En_LEDs_D2_D9(char caracter)
{
	uint8_t dato;

	dato = ASCII_a_Binario(caracter);

	// Apaga todos los leds
	PORTD &= ~((1 << PORTD2) | (1 << PORTD3) | (1 << PORTD4) |
	(1 << PORTD5) | (1 << PORTD6) | (1 << PORTD7));
	PORTB &= ~((1 << PORTB0) | (1 << PORTB1));

	// Espera 15 ms
	Delay_ms_simple(15);

	// Muestra nuevo ASCII
	PORTD |= ((dato & 0x3F) << 2);
	PORTB |= ((dato >> 6) & 0x03);
}