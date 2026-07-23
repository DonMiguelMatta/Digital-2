/*
 * 7seglib.h
 */




/****************************************/
// Librerias
#ifndef SEVENSEGLIB_H_
#define SEVENSEGLIB_H_
#include <avr/io.h>
#include <stdint.h>

/****************************************/
// Configuracion

#define DISPLAY_COMMON_ANODE 0

#define DISPLAY_SEGMENTS_MASK 0x7F

/****************************************/
// Tabla hexadecimal

extern const uint8_t hexTable[16];

/****************************************/
// Prototipos


void display_init(void);

void display_show(uint8_t num);

void display_clear(void);

void display_all_on(void);

void display_write_raw(uint8_t pattern);

#endif 
