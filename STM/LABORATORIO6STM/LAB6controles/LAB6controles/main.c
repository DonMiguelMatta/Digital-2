/*
 * Botones A0-A5 con debounce por 5 muestras
 * ATmega328P - Arduino Nano
 */

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <stdint.h>

#include "UART/usart.h"

#define NUM_BOTONES          6
#define MASCARA_BOTONES      0x3F
#define MUESTRAS_ESTABLES    5

volatile uint8_t eventosPendientes = 0;

static uint8_t ultimaMuestra = MASCARA_BOTONES;
static uint8_t estadoEstable = MASCARA_BOTONES;
static uint8_t contador[NUM_BOTONES] = {0};

/* Envía b1-b6 */
static void enviarBoton(uint8_t boton)
{
    USART_Transmit('b');
    USART_Transmit((char)('1' + boton));
    USART_SendString("\r\n");
}

/* Timer0 genera un pulso cada 1 ms */
static void Timer0_Init(void)
{
    TCCR0A = (1 << WGM01);

    /* Prescaler de 64 */
    TCCR0B = (1 << CS01) | (1 << CS00);

    /* 1 ms para 16 MHz */
    OCR0A = 249;
    TCNT0 = 0;

    /* Interrupción por comparación */
    TIMSK0 = (1 << OCIE0A);
}

/* Revisa la estabilidad de los botones */
ISR(TIMER0_COMPA_vect)
{
    uint8_t muestra;
    uint8_t mascara;
    uint8_t boton;
    uint8_t lecturaActual;
    uint8_t lecturaAnterior;
    uint8_t lecturaEstable;

    muestra = PINC & MASCARA_BOTONES;

    for (boton = 0; boton < NUM_BOTONES; boton++)
    {
        mascara = (1 << boton);

        lecturaActual =
            (muestra & mascara) ? 1 : 0;

        lecturaAnterior =
            (ultimaMuestra & mascara) ? 1 : 0;

        /* La lectura continúa igual */
        if (lecturaActual == lecturaAnterior)
        {
            if (contador[boton] < MUESTRAS_ESTABLES)
            {
                contador[boton]++;
            }
        }
        else
        {
            /* Comienza una nueva secuencia */
            contador[boton] = 1;

            if (lecturaActual)
            {
                ultimaMuestra |= mascara;
            }
            else
            {
                ultimaMuestra &= ~mascara;
            }
        }

        /* Confirma el cambio después de 5 muestras */
        if (contador[boton] >= MUESTRAS_ESTABLES)
        {
            lecturaEstable =
                (estadoEstable & mascara) ? 1 : 0;

            if (lecturaActual != lecturaEstable)
            {
                if (lecturaActual)
                {
                    /* Botón liberado */
                    estadoEstable |= mascara;
                }
                else
                {
                    /* Botón presionado */
                    estadoEstable &= ~mascara;
                    eventosPendientes |= mascara;
                }
            }
        }
    }
}

int main(void)
{
    uint8_t eventos;
    uint8_t boton;

    /* A0-A5 como entradas */
    DDRC &= ~MASCARA_BOTONES;

    /* Pull-up interno en A0-A5 */
    PORTC |= MASCARA_BOTONES;

    /* UART0 a 9600 baudios */
    USART_Init();

    /* Inicia temporizador */
    Timer0_Init();

    /* Habilita interrupciones */
    sei();

    /* Mensaje para comprobar UART */
    USART_SendString("\r\n - \r\n");

    while (1)
    {
        /* Copia y limpia los eventos de forma segura */
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
        {
            eventos = eventosPendientes;
            eventosPendientes = 0;
        }

        /* Transmite los botones detectados */
        for (boton = 0; boton < NUM_BOTONES; boton++)
        {
            if (eventos & (1 << boton))
            {
                enviarBoton(boton);
            }
        }
    }
}