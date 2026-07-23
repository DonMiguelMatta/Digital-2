/*
 * Laboratorio 2 pantalla LCD
 * Created: 16-jul
 * Author: Miguel Donis
 * Description: Pantalla - uart - potenciometros
 */
/****************************************/
// Encabezado (Libraries)

#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/delay.h>
#include "LCDLIB/lcd.h"
#include "USARLIB/usart.h"

#define POT1_CHANNEL 0
#define POT2_CHANNEL 1

/****************************************/
// Function prototypes

void ADC_Init(void);
uint16_t ADC_Read(uint8_t channel);
void USART_SendData(uint16_t pot1, uint16_t pot2, int16_t contador);
void USART_SendVoltage(uint16_t adcValue);
void USART_UpdateCounter(int16_t *contador);
void LCD_ShowData(uint16_t pot1, uint16_t pot2, int16_t contador);
void LCD_WriteVoltage(uint16_t adcValue);

/****************************************/
// Main Function

int main(void)
{
    uint16_t pot1;
    uint16_t pot2;
    int16_t contador = 0;

    USART_Init();
    initLCD8bits();
    ADC_Init();

    while (1)
    {
        pot1 = ADC_Read(POT1_CHANNEL);
        pot2 = ADC_Read(POT2_CHANNEL);

        USART_UpdateCounter(&contador);
        USART_SendData(pot1, pot2, contador);
        LCD_ShowData(pot1, pot2, contador);
        _delay_ms(250);
    }
}

/****************************************/
// NON-Interrupt subroutines

// Inicializa ADC
void ADC_Init(void)
{
    ADMUX = (1 << REFS0);
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

// Lee canal ADC
uint16_t ADC_Read(uint8_t channel)
{
    ADMUX &= 0xF0;
    ADMUX |= (channel & 0x0F);
    ADCSRA |= (1 << ADSC);

    while (ADCSRA & (1 << ADSC));

    return ADC;
}

// Envia datos por UART
void USART_SendData(uint16_t pot1, uint16_t pot2, int16_t contador)
{
    USART_SendString("S1=");
    USART_SendVoltage(pot1);
    USART_SendString(" S2=");
    USART_SendUint16(pot2);
    USART_SendString(" S3=");
    USART_SendInt16(contador);
    USART_SendString("\r\n");
}

// Envia voltaje por UART
void USART_SendVoltage(uint16_t adcValue)
{
    uint16_t centivolts;

    centivolts = (uint16_t)(((uint32_t)adcValue * 500 + 511) / 1023);
    USART_SendUint16(centivolts / 100);
    USART_Transmit('.');
    USART_Transmit(((centivolts / 10) % 10) + '0');
    USART_Transmit((centivolts % 10) + '0');
    USART_Transmit('V');
}

// Actualiza contador por UART
void USART_UpdateCounter(int16_t *contador)
{
    char data;

    if (USART_Available())
    {
        data = USART_Receive();

        if (data == '+')
        {
            (*contador)++;
        }
        else if (data == '-')
        {
            (*contador)--;
        }
    }
}

// Muestra S1 S2 S3 en LCD
void LCD_ShowData(uint16_t pot1, uint16_t pot2, int16_t contador)
{
    LCD_Set_Cursor(1, 1);
    LCD_Write_String("S1:   S2:  S3:");

    LCD_Set_Cursor(1, 2);
    LCD_Write_String("                ");
    LCD_Set_Cursor(1, 2);
    LCD_WriteVoltage(pot1);

    LCD_Set_Cursor(7, 2);
    LCD_WriteUint16(pot2);

    LCD_Set_Cursor(12, 2);
    LCD_WriteInt16(contador);
}

// Escribe voltaje
void LCD_WriteVoltage(uint16_t adcValue)
{
    uint16_t centivolts;

    centivolts = (uint16_t)(((uint32_t)adcValue * 500 + 511) / 1023);
    LCD_WriteUint16(centivolts / 100);
    LCD_Write_Char('.');
    LCD_Write_Char(((centivolts / 10) % 10) + '0');
    LCD_Write_Char((centivolts % 10) + '0');
    LCD_Write_Char('V');
}

/****************************************/
// Interrupt routines
