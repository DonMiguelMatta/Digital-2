/*
 * Biblioteca I2C Master
 *
 * Author: Miguel Donis 22993 - Ian Farrington 21952
 * Description: Transporte TWI con timeout para tramas y registros I2C
 */
/****************************************/
// Encabezado (Libraries)

#include "I2CLIB.h"

#include <avr/io.h>
#include <util/delay.h>

#ifndef F_CPU
#error "Debe definir F_CPU=16000000UL en el proyecto"
#endif

#define I2C_WAIT_TIMEOUT_US 5000U
#define I2C_DEFERRED_ATTEMPTS 40U

static uint8_t i2c_last_status = 0xF8U;
static I2CError i2c_last_error = I2C_ERROR_NONE;

// Espera que TWI termine una operacion o registra timeout tras unos 5 ms.
static uint8_t I2C_WaitForTWINT(void)
{
    uint16_t remaining = I2C_WAIT_TIMEOUT_US;

    while ((TWCR & (1U << TWINT)) == 0U)
    {
        if (remaining == 0U)
        {
            i2c_last_status = (uint8_t)(TWSR & 0xF8U);
            i2c_last_error = I2C_ERROR_TIMEOUT;
            TWCR = (1U << TWEN);
            return 0U;
        }

        remaining--;
        _delay_us(1);
    }

    return 1U;
}

// Envia direccion de siete bits junto con el bit de lectura o escritura.
static uint8_t I2C_SendAddress(uint8_t address, uint8_t read)
{
    uint8_t address_byte = (uint8_t)(address << 1U);

    if (read != 0U)
    {
        address_byte |= 1U;
    }

    if (I2C_Master_Write(address_byte) == 0U)
    {
        i2c_last_error = I2C_ERROR_ADDRESS_NACK;
        return 0U;
    }

    return 1U;
}

// Escribe los ocho bytes completos de una solicitud y garantiza STOP.
static uint8_t I2C_WriteFrame(uint8_t address,
                              const uint8_t frame[I2C_FRAME_SIZE])
{
    uint8_t index;

    if (I2C_Master_Start() == 0U)
    {
        return 0U;
    }

    if (I2C_SendAddress(address, 0U) == 0U)
    {
        return 0U;
    }

    for (index = 0U; index < I2C_FRAME_SIZE; index++)
    {
        if (I2C_Master_Write(frame[index]) == 0U)
        {
            i2c_last_error = I2C_ERROR_DATA_NACK;
            return 0U;
        }
    }

    return 1U;
}

// Lee ocho bytes usando ACK excepto en el ultimo byte, que usa NACK.
static uint8_t I2C_ReadFrame(uint8_t address,
                             uint8_t frame[I2C_FRAME_SIZE],
                             uint8_t repeated_start)
{
    uint8_t index;

    if (repeated_start != 0U)
    {
        if (I2C_Master_RepeatedStart() == 0U)
        {
            return 0U;
        }
    }
    else if (I2C_Master_Start() == 0U)
    {
        return 0U;
    }

    if (I2C_SendAddress(address, 1U) == 0U)
    {
        return 0U;
    }

    for (index = 0U; index < I2C_FRAME_SIZE; index++)
    {
        uint8_t send_ack = (uint8_t)(index < (I2C_FRAME_SIZE - 1U));

        if (I2C_Master_Read(&frame[index], send_ack) == 0U)
        {
            i2c_last_error = I2C_ERROR_READ;
            return 0U;
        }
    }

    return 1U;
}

/****************************************/
// NON-Interrupt subroutines

// Configura la frecuencia SCL, el prescaler y habilita el modulo TWI.
void I2C_Master_Init(uint32_t scl_clock, uint8_t prescaler)
{
    DDRC &= (uint8_t)~((1U << DDC4) | (1U << DDC5));

    switch (prescaler)
    {
        case 1U:
            TWSR &= (uint8_t)~((1U << TWPS1) | (1U << TWPS0));
            break;

        case 4U:
            TWSR &= (uint8_t)~(1U << TWPS1);
            TWSR |= (1U << TWPS0);
            break;

        case 16U:
            TWSR &= (uint8_t)~(1U << TWPS0);
            TWSR |= (1U << TWPS1);
            break;

        case 64U:
            TWSR |= (1U << TWPS1) | (1U << TWPS0);
            break;

        default:
            TWSR &= (uint8_t)~((1U << TWPS1) | (1U << TWPS0));
            prescaler = 1U;
            break;
    }

    if (scl_clock == 0UL)
    {
        scl_clock = 100000UL;
    }

    TWBR = (uint8_t)(((F_CPU / scl_clock) - 16UL) /
                     (2UL * (uint32_t)prescaler));
    TWCR = (1U << TWEN);
    i2c_last_status = (uint8_t)(TWSR & 0xF8U);
    i2c_last_error = I2C_ERROR_NONE;
}

// Genera START y verifica el estado devuelto por TWI.
uint8_t I2C_Master_Start(void)
{
    i2c_last_error = I2C_ERROR_NONE;
    TWCR = (1U << TWINT) | (1U << TWSTA) | (1U << TWEN);

    if (I2C_WaitForTWINT() == 0U)
    {
        return 0U;
    }

    i2c_last_status = (uint8_t)(TWSR & 0xF8U);
    if (i2c_last_status != 0x08U)
    {
        i2c_last_error = I2C_ERROR_START;
        return 0U;
    }

    return 1U;
}

// Genera repeated START sin liberar previamente el bus.
uint8_t I2C_Master_RepeatedStart(void)
{
    TWCR = (1U << TWINT) | (1U << TWSTA) | (1U << TWEN);

    if (I2C_WaitForTWINT() == 0U)
    {
        return 0U;
    }

    i2c_last_status = (uint8_t)(TWSR & 0xF8U);
    if (i2c_last_status != 0x10U)
    {
        i2c_last_error = I2C_ERROR_START;
        return 0U;
    }

    return 1U;
}

// Genera STOP y espera que el hardware libere el bus.
void I2C_Master_Stop(void)
{
    uint16_t remaining = I2C_WAIT_TIMEOUT_US;

    TWCR = (1U << TWEN) | (1U << TWINT) | (1U << TWSTO);

    while ((TWCR & (1U << TWSTO)) != 0U)
    {
        if (remaining == 0U)
        {
            i2c_last_status = (uint8_t)(TWSR & 0xF8U);
            i2c_last_error = I2C_ERROR_STOP_TIMEOUT;
            TWCR = 0U;
            TWCR = (1U << TWEN);
            return;
        }

        remaining--;
        _delay_us(1);
    }
}

// Transmite un byte y valida el ACK recibido.
uint8_t I2C_Master_Write(uint8_t data)
{
    uint8_t status;

    TWDR = data;
    TWCR = (1U << TWEN) | (1U << TWINT);

    if (I2C_WaitForTWINT() == 0U)
    {
        return 0U;
    }

    status = (uint8_t)(TWSR & 0xF8U);
    i2c_last_status = status;

    if ((status == 0x18U) || (status == 0x28U) || (status == 0x40U))
    {
        return 1U;
    }

    i2c_last_error = I2C_ERROR_DATA_NACK;
    return 0U;
}

// Lee un byte y decide si responder con ACK o NACK.
uint8_t I2C_Master_Read(uint8_t *data, uint8_t ack)
{
    uint8_t status;

    if (data == 0)
    {
        i2c_last_error = I2C_ERROR_NULL_POINTER;
        return 0U;
    }

    if (ack != 0U)
    {
        TWCR = (1U << TWINT) | (1U << TWEN) | (1U << TWEA);
    }
    else
    {
        TWCR = (1U << TWINT) | (1U << TWEN);
    }

    if (I2C_WaitForTWINT() == 0U)
    {
        return 0U;
    }

    status = (uint8_t)(TWSR & 0xF8U);
    i2c_last_status = status;

    if (((ack != 0U) && (status != 0x50U)) ||
        ((ack == 0U) && (status != 0x58U)))
    {
        i2c_last_error = I2C_ERROR_READ;
        return 0U;
    }

    *data = TWDR;
    return 1U;
}

// Comprueba solamente si una direccion responde con ACK.
uint8_t I2C_Master_ProbeAddress(uint8_t address)
{
    uint8_t success;

    i2c_last_error = I2C_ERROR_NONE;
    success = I2C_Master_Start();
    if (success != 0U)
    {
        success = I2C_SendAddress(address, 0U);
    }

    I2C_Master_Stop();
    if (i2c_last_error == I2C_ERROR_STOP_TIMEOUT)
    {
        return 0U;
    }

    return success;
}

// Intercambia una trama PING completa con espera diferida.
uint8_t I2C_Master_Ping(uint8_t address,
                        const uint8_t request[I2C_FRAME_SIZE],
                        uint8_t response[I2C_FRAME_SIZE])
{
    return I2C_Master_Exchange(address, request, response,
                               I2C_EXCHANGE_DEFERRED);
}

// Escribe una solicitud y obtiene la respuesta inmediata o diferida.
uint8_t I2C_Master_Exchange(uint8_t address,
                            const uint8_t request[I2C_FRAME_SIZE],
                            uint8_t response[I2C_FRAME_SIZE],
                            I2CExchangeMode mode)
{
    uint8_t attempt;

    if ((request == 0) || (response == 0))
    {
        i2c_last_error = I2C_ERROR_NULL_POINTER;
        return 0U;
    }

    i2c_last_error = I2C_ERROR_NONE;
    if (I2C_WriteFrame(address, request) == 0U)
    {
        I2C_Master_Stop();
        return 0U;
    }

    if (mode == I2C_EXCHANGE_IMMEDIATE)
    {
        if (I2C_ReadFrame(address, response, 1U) == 0U)
        {
            I2C_Master_Stop();
            return 0U;
        }

        I2C_Master_Stop();
        if (i2c_last_error == I2C_ERROR_STOP_TIMEOUT)
        {
            return 0U;
        }

        if ((response[0] != request[0]) || (response[1] != request[1]))
        {
            i2c_last_error = I2C_ERROR_RESPONSE_TIMEOUT;
            return 0U;
        }

        i2c_last_error = I2C_ERROR_NONE;
        return 1U;
    }

    I2C_Master_Stop();
    if (i2c_last_error == I2C_ERROR_STOP_TIMEOUT)
    {
        return 0U;
    }

    for (attempt = 0U; attempt < I2C_DEFERRED_ATTEMPTS; attempt++)
    {
        _delay_ms(1);

        if (I2C_ReadFrame(address, response, 0U) == 0U)
        {
            I2C_Master_Stop();
            if (i2c_last_error == I2C_ERROR_STOP_TIMEOUT)
            {
                return 0U;
            }
            continue;
        }

        I2C_Master_Stop();
        if (i2c_last_error == I2C_ERROR_STOP_TIMEOUT)
        {
            return 0U;
        }

        if ((response[0] == request[0]) && (response[1] == request[1]))
        {
            i2c_last_error = I2C_ERROR_NONE;
            return 1U;
        }
    }

    i2c_last_error = I2C_ERROR_RESPONSE_TIMEOUT;
    return 0U;
}

// Devuelve el ultimo codigo TWSR sin bits de prescaler.
uint8_t I2C_Master_GetLastStatus(void)
{
    return i2c_last_status;
}

// Devuelve la ultima categoria de error del transporte.
I2CError I2C_Master_GetLastError(void)
{
    return i2c_last_error;
}
