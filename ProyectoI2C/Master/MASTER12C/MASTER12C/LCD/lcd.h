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

void initLCD8bits(void);
void LCD_Port(uint8_t data);
void LCD_CMD(uint8_t command);
void LCD_Write_Char(char data);
void LCD_Write_String(const char *text);
void LCD_Set_Cursor(uint8_t column, uint8_t row);
void LCD_Clear(void);
void LCD_WriteUint16(uint16_t numero);
void LCD_WriteInt16(int16_t numero);

#endif
