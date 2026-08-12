/*
 * Biblioteca ESPUART
 *
 * Author: Miguel Donis 22993 - Ian Farrington 21952
 * Description: Recepcion UART por software desde el ESP32
 */
/****************************************/
// Encabezado (Libraries)

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include "ESPUART.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/atomic.h>

#define ESPUART_RX_DDR              DDRB
#define ESPUART_RX_PORT             PORTB
#define ESPUART_RX_INPUT            PINB
#define ESPUART_RX_PIN              PB4
#define ESPUART_RX_PCINT            PCINT4

#define ESPUART_BAUD                9600UL
#define ESPUART_TIMER_PRESCALER     8UL
#define ESPUART_BIT_TICKS           ((F_CPU + ((ESPUART_BAUD * ESPUART_TIMER_PRESCALER) / 2UL)) / \
                                     (ESPUART_BAUD * ESPUART_TIMER_PRESCALER))
#define ESPUART_FIRST_SAMPLE_TICKS  ((ESPUART_BIT_TICKS * 3UL) / 2UL)
#define ESPUART_BUFFER_SIZE         64U
#define ESPUART_BUFFER_MASK         (ESPUART_BUFFER_SIZE - 1U)

#if (ESPUART_BUFFER_SIZE & ESPUART_BUFFER_MASK) != 0U
#error "ESPUART_BUFFER_SIZE debe ser potencia de dos"
#endif

static volatile uint8_t esp_uart_buffer[ESPUART_BUFFER_SIZE];
static volatile uint8_t esp_uart_head = 0U;
static volatile uint8_t esp_uart_tail = 0U;
static volatile uint8_t esp_uart_overflow = 0U;
static volatile uint8_t esp_uart_receiving = 0U;
static volatile uint8_t esp_uart_bit_index = 0U;
static volatile uint8_t esp_uart_rx_byte = 0U;

/****************************************/
// NON-Interrupt subroutines

// Configura Timer1 y la interrupcion por cambio de D12.
void ESPUART_Init(void)
{
    ESPUART_RX_DDR &= (uint8_t)~(1U << ESPUART_RX_PIN);
    ESPUART_RX_PORT |= (1U << ESPUART_RX_PIN);

    TCCR1A = 0U;
    TCCR1B = (1U << CS11);
    TCNT1 = 0U;
    TIMSK1 &= (uint8_t)~(1U << OCIE1A);

    PCMSK0 |= (1U << ESPUART_RX_PCINT);
    PCIFR = (1U << PCIF0);
    PCICR |= (1U << PCIE0);
}

// Comprueba si el buffer contiene datos.
uint8_t ESPUART_Available(void)
{
    return (uint8_t)(esp_uart_head != esp_uart_tail);
}

// Extrae un byte del buffer circular.
char ESPUART_ReadChar(void)
{
    char data = '\0';

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        if (esp_uart_head != esp_uart_tail)
        {
            data = (char)esp_uart_buffer[esp_uart_tail];
            esp_uart_tail = (uint8_t)((esp_uart_tail + 1U) &
                                      ESPUART_BUFFER_MASK);
        }
    }

    return data;
}

// Devuelve el indicador de desbordamiento y lo limpia.
uint8_t ESPUART_GetAndClearOverflow(void)
{
    uint8_t overflow;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        overflow = esp_uart_overflow;
        esp_uart_overflow = 0U;
    }

    return overflow;
}

/****************************************/
// Interrupt routines

// Detecta el flanco de inicio de un byte UART.
ISR(PCINT0_vect)
{
    uint16_t start_ticks = TCNT1;

    if ((esp_uart_receiving == 0U) &&
        ((ESPUART_RX_INPUT & (1U << ESPUART_RX_PIN)) == 0U))
    {
        esp_uart_receiving = 1U;
        esp_uart_bit_index = 0U;
        esp_uart_rx_byte = 0U;
        PCMSK0 &= (uint8_t)~(1U << ESPUART_RX_PCINT);
        OCR1A = (uint16_t)(start_ticks + ESPUART_FIRST_SAMPLE_TICKS);
        TIFR1 = (1U << OCF1A);
        TIMSK1 |= (1U << OCIE1A);
    }
}

// Muestrea los ocho bits y valida el bit de parada.
ISR(TIMER1_COMPA_vect)
{
    if (esp_uart_bit_index < 8U)
    {
        if ((ESPUART_RX_INPUT & (1U << ESPUART_RX_PIN)) != 0U)
        {
            esp_uart_rx_byte |= (uint8_t)(1U << esp_uart_bit_index);
        }

        esp_uart_bit_index++;
        OCR1A = (uint16_t)(OCR1A + ESPUART_BIT_TICKS);
        return;
    }

    if ((ESPUART_RX_INPUT & (1U << ESPUART_RX_PIN)) != 0U)
    {
        uint8_t next_head = (uint8_t)((esp_uart_head + 1U) &
                                      ESPUART_BUFFER_MASK);

        if (next_head != esp_uart_tail)
        {
            esp_uart_buffer[esp_uart_head] = esp_uart_rx_byte;
            esp_uart_head = next_head;
        }
        else
        {
            esp_uart_overflow = 1U;
        }
    }

    TIMSK1 &= (uint8_t)~(1U << OCIE1A);
    esp_uart_receiving = 0U;
    PCIFR = (1U << PCIF0);
    PCMSK0 |= (1U << ESPUART_RX_PCINT);
}
