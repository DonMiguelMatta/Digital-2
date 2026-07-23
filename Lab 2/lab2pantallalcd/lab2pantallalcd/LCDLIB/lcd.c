#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include "lcd.h"
#include <util/delay.h>

static void LCD_EnablePulse(void);

void initLCD8bits(void)
{
    DDRD |= (1 << DDD2) | (1 << DDD3) | (1 << DDD4) | (1 << DDD5) | (1 << DDD6) | (1 << DDD7);
    DDRB |= (1 << DDB0) | (1 << DDB1);
    LCD_RS_DDR |= (1 << LCD_RS_PIN);
    LCD_E_DDR |= (1 << LCD_E_PIN);

    LCD_RS_PORT &= ~(1 << LCD_RS_PIN);
    LCD_E_PORT &= ~(1 << LCD_E_PIN);
    PORTD &= ~((1 << PORTD2) | (1 << PORTD3) | (1 << PORTD4) | (1 << PORTD5) | (1 << PORTD6) | (1 << PORTD7));
    PORTB &= ~((1 << PORTB0) | (1 << PORTB1));

    _delay_ms(20);
    LCD_CMD(0x38);
    LCD_CMD(0x0C);
    LCD_CMD(0x06);
    LCD_Clear();
}

void LCD_Port(uint8_t data)
{
    PORTD &= ~((1 << PORTD2) | (1 << PORTD3) | (1 << PORTD4) | (1 << PORTD5) | (1 << PORTD6) | (1 << PORTD7));
    PORTB &= ~((1 << PORTB0) | (1 << PORTB1));

    if (data & (1 << 7))
    {
        PORTD |= (1 << PORTD2);
    }
    if (data & (1 << 6))
    {
        PORTD |= (1 << PORTD3);
    }
    if (data & (1 << 5))
    {
        PORTD |= (1 << PORTD4);
    }
    if (data & (1 << 4))
    {
        PORTD |= (1 << PORTD5);
    }
    if (data & (1 << 3))
    {
        PORTD |= (1 << PORTD6);
    }
    if (data & (1 << 2))
    {
        PORTD |= (1 << PORTD7);
    }
    if (data & (1 << 1))
    {
        PORTB |= (1 << PORTB0);
    }
    if (data & (1 << 0))
    {
        PORTB |= (1 << PORTB1);
    }
}

void LCD_CMD(uint8_t command)
{
    LCD_RS_PORT &= ~(1 << LCD_RS_PIN);
    LCD_Port(command);
    LCD_EnablePulse();
    _delay_ms(2);
}

void LCD_Write_Char(char data)
{
    LCD_RS_PORT |= (1 << LCD_RS_PIN);
    LCD_Port(data);
    LCD_EnablePulse();
    _delay_us(100);
}

void LCD_Write_String(const char *text)
{
    while (*text)
    {
        LCD_Write_Char(*text);
        text++;
    }
}

void LCD_Set_Cursor(uint8_t column, uint8_t row)
{
    if (row == 1)
    {
        LCD_CMD(0x80 + column - 1);
    }
    else
    {
        LCD_CMD(0xC0 + column - 1);
    }
}

void LCD_Clear(void)
{
    LCD_CMD(0x01);
    _delay_ms(2);
}

void LCD_WriteUint16(uint16_t numero)
{
    char buffer[6];
    uint8_t i = 0;
    uint8_t j;
    char temp;

    if (numero == 0)
    {
        LCD_Write_Char('0');
        return;
    }

    while (numero > 0)
    {
        buffer[i] = (numero % 10) + '0';
        numero /= 10;
        i++;
    }

    for (j = 0; j < i / 2; j++)
    {
        temp = buffer[j];
        buffer[j] = buffer[i - 1 - j];
        buffer[i - 1 - j] = temp;
    }

    buffer[i] = '\0';
    LCD_Write_String(buffer);
}

void LCD_WriteInt16(int16_t numero)
{
    uint16_t valor;

    if (numero < 0)
    {
        LCD_Write_Char('-');
        valor = (uint16_t)(-(numero + 1)) + 1;
    }
    else
    {
        valor = (uint16_t)numero;
    }

    LCD_WriteUint16(valor);
}

static void LCD_EnablePulse(void)
{
    LCD_E_PORT |= (1 << LCD_E_PIN);
    _delay_us(1);
    LCD_E_PORT &= ~(1 << LCD_E_PIN);
    _delay_us(100);
}
