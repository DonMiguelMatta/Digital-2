/*
 * Biblioteca I2C Slave
 *
 * Author: Miguel Donis 22993 - Ian Farrington 21952
 * Description: Transporte TWI de tramas fijas para un Slave ATmega328P
 */
/****************************************/
// Encabezado (Libraries)

#include "I2CLIB.h"

#include <avr/interrupt.h>
#include <avr/io.h>

static volatile uint8_t rx_buffer[I2C_FRAME_SIZE];
static volatile uint8_t pending_request[I2C_FRAME_SIZE];
static volatile uint8_t rejected_request[I2C_FRAME_SIZE];
static volatile uint8_t staged_response[I2C_FRAME_SIZE];
static volatile uint8_t active_response[I2C_FRAME_SIZE];

static volatile uint8_t rx_index;
static volatile uint8_t rx_overflow;
static volatile uint8_t request_pending;
static volatile uint8_t request_in_progress;
static volatile uint8_t rejected_pending;
static volatile uint8_t response_ready;
static volatile uint8_t active_response_valid;
static volatile uint8_t tx_index;

// Reactiva TWI con ACK e interrupcion para continuar la transaccion.
static void I2C_Slave_Continue(void)
{
    TWCR = (1U << TWINT) | (1U << TWEA) | (1U << TWEN) | (1U << TWIE);
}

// Congela una trama completa o la guarda como rechazada si existe otra pendiente.
static void I2C_Slave_FinalizeReception(void)
{
    uint8_t index;

    if ((rx_index != I2C_FRAME_SIZE) || (rx_overflow != 0U))
    {
        return;
    }

    if ((request_pending != 0U) || (request_in_progress != 0U))
    {
        if (rejected_pending == 0U)
        {
            for (index = 0U; index < I2C_FRAME_SIZE; index++)
            {
                rejected_request[index] = rx_buffer[index];
            }
            rejected_pending = 1U;
        }
        return;
    }

    for (index = 0U; index < I2C_FRAME_SIZE; index++)
    {
        pending_request[index] = rx_buffer[index];
    }
    request_pending = 1U;
    response_ready = 0U;
}

/****************************************/
// NON-Interrupt subroutines

// Limpia los buffers y configura la direccion propia del Slave.
uint8_t I2C_Slave_Init(uint8_t address)
{
    uint8_t saved_sreg;

    if ((address == 0U) || (address > 0x7FU))
    {
        return 0U;
    }

    saved_sreg = SREG;
    cli();

    rx_index = 0U;
    rx_overflow = 0U;
    request_pending = 0U;
    request_in_progress = 0U;
    rejected_pending = 0U;
    response_ready = 0U;
    active_response_valid = 0U;
    tx_index = 0U;

    DDRC &= (uint8_t)~((1U << DDC4) | (1U << DDC5));
    TWAR = (uint8_t)(address << 1U);
    TWCR = (1U << TWEA) | (1U << TWEN) | (1U << TWIE);

    SREG = saved_sreg;
    return 1U;
}

// Indica si main puede retirar una solicitud normal o rechazada por BUSY.
uint8_t I2C_Slave_RequestAvailable(void)
{
    uint8_t saved_sreg = SREG;
    uint8_t available;

    cli();
    available = request_pending;
    if ((available == 0U) && (rejected_pending != 0U) &&
        (request_in_progress == 0U) && (response_ready == 0U))
    {
        available = 1U;
    }
    SREG = saved_sreg;

    return available;
}

// Copia una solicitud congelada y marca que main la esta procesando.
I2CSlaveRequestState I2C_Slave_GetRequest(uint8_t frame[I2C_FRAME_SIZE])
{
    I2CSlaveRequestState state = I2C_SLAVE_REQUEST_NONE;
    uint8_t saved_sreg;
    uint8_t index;

    if (frame == 0)
    {
        return I2C_SLAVE_REQUEST_NONE;
    }

    saved_sreg = SREG;
    cli();

    if ((request_pending != 0U) && (request_in_progress == 0U))
    {
        for (index = 0U; index < I2C_FRAME_SIZE; index++)
        {
            frame[index] = pending_request[index];
        }
        request_pending = 0U;
        request_in_progress = 1U;
        state = I2C_SLAVE_REQUEST_READY;
    }
    else if ((rejected_pending != 0U) && (request_in_progress == 0U) &&
             (response_ready == 0U))
    {
        for (index = 0U; index < I2C_FRAME_SIZE; index++)
        {
            frame[index] = rejected_request[index];
        }
        rejected_pending = 0U;
        request_in_progress = 1U;
        state = I2C_SLAVE_REQUEST_REJECTED_BUSY;
    }

    SREG = saved_sreg;
    return state;
}

// Publica una respuesta completa sin modificar la copia actualmente transmitida.
uint8_t I2C_Slave_SetResponse(const uint8_t frame[I2C_FRAME_SIZE])
{
    uint8_t saved_sreg;
    uint8_t index;

    if (frame == 0)
    {
        return 0U;
    }

    saved_sreg = SREG;
    cli();

    if ((request_in_progress == 0U) || (response_ready != 0U))
    {
        SREG = saved_sreg;
        return 0U;
    }

    for (index = 0U; index < I2C_FRAME_SIZE; index++)
    {
        staged_response[index] = frame[index];
    }
    response_ready = 1U;
    request_in_progress = 0U;

    SREG = saved_sreg;
    return 1U;
}

/****************************************/
// Interrupt routines

// Recibe solicitudes y transmite una copia congelada segun el estado TWI.
ISR(TWI_vect)
{
    uint8_t status = (uint8_t)(TWSR & 0xF8U);
    uint8_t index;

    switch (status)
    {
        case 0x00U:
            rx_index = 0U;
            rx_overflow = 0U;
            active_response_valid = 0U;
            tx_index = 0U;
            TWCR = (1U << TWINT) | (1U << TWSTO) | (1U << TWEA) |
                   (1U << TWEN) | (1U << TWIE);
            break;

        case 0x60U:
        case 0x68U:
            rx_index = 0U;
            rx_overflow = 0U;
            I2C_Slave_Continue();
            break;

        case 0x80U:
        case 0x88U:
            if (rx_index < I2C_FRAME_SIZE)
            {
                rx_buffer[rx_index] = TWDR;
                rx_index++;
            }
            else
            {
                rx_overflow = 1U;
            }
            I2C_Slave_Continue();
            break;

        case 0xA0U:
            I2C_Slave_FinalizeReception();
            rx_index = 0U;
            rx_overflow = 0U;
            I2C_Slave_Continue();
            break;

        case 0xA8U:
        case 0xB0U:
            active_response_valid = response_ready;
            for (index = 0U; index < I2C_FRAME_SIZE; index++)
            {
                active_response[index] = (active_response_valid != 0U) ?
                                         staged_response[index] : 0U;
            }
            tx_index = 1U;
            TWDR = active_response[0];
            I2C_Slave_Continue();
            break;

        case 0xB8U:
            if (tx_index < I2C_FRAME_SIZE)
            {
                TWDR = active_response[tx_index];
                tx_index++;
            }
            else
            {
                TWDR = 0U;
            }
            I2C_Slave_Continue();
            break;

        case 0xC0U:
        case 0xC8U:
            if ((active_response_valid != 0U) &&
                (tx_index >= I2C_FRAME_SIZE))
            {
                response_ready = 0U;
            }
            active_response_valid = 0U;
            tx_index = 0U;
            I2C_Slave_Continue();
            break;

        default:
            rx_index = 0U;
            rx_overflow = 0U;
            active_response_valid = 0U;
            tx_index = 0U;
            I2C_Slave_Continue();
            break;
    }
}
