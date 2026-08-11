/*
 * Biblioteca LCD
 *
 * Author: Miguel Donis 22993 - Ian Farrington 21952
 * Description: Interfaz de pantalla LCD paralela de ocho bits
 */
/****************************************/
// Encabezado (Libraries)

#ifndef LCD_H_
#define LCD_H_

#include <avr/io.h>
#include <stdint.h>

#define LCD_RS_DDR DDRC
#define LCD_RS_PORT PORTC
#define LCD_RS_PIN PC1

#define LCD_E_DDR DDRC
#define LCD_E_PORT PORTC
#define LCD_E_PIN PC0

/****************************************/
// Function prototypes

// Inicializa pines y secuencia de arranque de la pantalla.
void initLCD8bits(void);
// Presenta un byte sobre D0-D7 de la LCD.
void LCD_Port(uint8_t data);
// Envia una instruccion al controlador LCD.
void LCD_CMD(uint8_t command);
// Escribe un caracter visible.
void LCD_Write_Char(char data);
// Escribe una cadena terminada en nulo.
void LCD_Write_String(const char *text);
// Posiciona el cursor usando columna y fila.
void LCD_Set_Cursor(uint8_t column, uint8_t row);
// Borra ambas lineas de la pantalla.
void LCD_Clear(void);
// Escribe un entero sin signo en decimal.
void LCD_WriteUint16(uint16_t numero);
// Escribe un entero con signo en decimal.
void LCD_WriteInt16(int16_t numero);

#endif
