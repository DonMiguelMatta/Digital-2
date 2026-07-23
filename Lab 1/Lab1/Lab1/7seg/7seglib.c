/*
 * 7seglib.c
 *
 */

#include "7seglib.h"

/****************************************/
// Tabla de numeros hexadecimales

const uint8_t hexTable[16] =
{
	0x3F,	
	0x06,	
	0x5B,	
	0x4F,	
	0x66,	
	0x6D,	
	0x7D,	
	0x07,	
	0x7F,	
	0x6F,	
	0x77,	
	0x7C,	
	0x39,	
	0x5E,	
	0x79,	
	0x71	
};

/****************************************/
// Inicializar display

void display_init(void)
{
	
	
	
	DDRC |= DISPLAY_SEGMENTS_MASK;

	
	display_clear();
}

/****************************************/
// Escribir

void display_write_raw(uint8_t pattern)
{
	
	pattern &= DISPLAY_SEGMENTS_MASK;

#if DISPLAY_COMMON_ANODE

	pattern = (~pattern) & DISPLAY_SEGMENTS_MASK;

#endif

	
	PORTC =
		(PORTC & ~DISPLAY_SEGMENTS_MASK) |
		pattern;
}

/****************************************/
// Mostrar numero

void display_show(uint8_t num)
{
	
	if (num > 15)
	{
		display_clear();
		return;
	}

	display_write_raw(hexTable[num]);
}

/****************************************/
// Apagar display

void display_clear(void)
{
#if DISPLAY_COMMON_ANODE

	
	PORTC |= DISPLAY_SEGMENTS_MASK;

#else

	
	PORTC &= ~DISPLAY_SEGMENTS_MASK;

#endif
}

/****************************************/
// Encender todos los segmentos

void display_all_on(void)
{
#if DISPLAY_COMMON_ANODE

	/*
	 * En anodo comun, un 0 enciende el segmento.
	 */
	PORTC &= ~DISPLAY_SEGMENTS_MASK;

#else

	/*
	 * En catodo comun, un 1 enciende el segmento.
	 */
	PORTC |= DISPLAY_SEGMENTS_MASK;

#endif
}
