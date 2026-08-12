/*
 * Biblioteca I2C Master
 *
 * Author: Miguel Donis 22993 - Ian Farrington 21952
 * Description: Interfaz TWI Master para SDA PC4 y SCL PC5
 */
/****************************************/
// Encabezado (Libraries)

#ifndef I2CLIB_H_
#define I2CLIB_H_

#include <stdint.h>

#define I2C_FRAME_SIZE 8U

typedef enum
{
    I2C_ERROR_NONE = 0,
    I2C_ERROR_NULL_POINTER,
    I2C_ERROR_START,
    I2C_ERROR_ADDRESS_NACK,
    I2C_ERROR_DATA_NACK,
    I2C_ERROR_READ,
    I2C_ERROR_TIMEOUT,
    I2C_ERROR_STOP_TIMEOUT,
    I2C_ERROR_RESPONSE_TIMEOUT
} I2CError;

typedef enum
{
    I2C_EXCHANGE_IMMEDIATE = 0,
    I2C_EXCHANGE_DEFERRED = 1
} I2CExchangeMode;

/****************************************/
// Function prototypes

// Configura SCL y el prescaler del modulo TWI.
void I2C_Master_Init(uint32_t scl_clock, uint8_t prescaler);

// Genera START y devuelve 1 si TWI lo acepta.
uint8_t I2C_Master_Start(void);
// Genera repeated START sin liberar el bus.
uint8_t I2C_Master_RepeatedStart(void);
// Genera STOP incluso despues de un error.
void I2C_Master_Stop(void);
// Escribe un byte y exige ACK.
uint8_t I2C_Master_Write(uint8_t data);
// Lee un byte y envia ACK cuando ack es distinto de cero.
uint8_t I2C_Master_Read(uint8_t *data, uint8_t ack);

// Detecta el ACK de una direccion sin intercambiar datos.
uint8_t I2C_Master_ProbeAddress(uint8_t address);
// Ejecuta PING mediante una solicitud y respuesta completas.
uint8_t I2C_Master_Ping(uint8_t address,
                        const uint8_t request[I2C_FRAME_SIZE],
                        uint8_t response[I2C_FRAME_SIZE]);
// Intercambia ocho bytes en modo inmediato o diferido.
uint8_t I2C_Master_Exchange(uint8_t address,
                            const uint8_t request[I2C_FRAME_SIZE],
                            uint8_t response[I2C_FRAME_SIZE],
                            I2CExchangeMode mode);

// Devuelve el ultimo estado del registro TWSR.
uint8_t I2C_Master_GetLastStatus(void);
// Devuelve el ultimo error clasificado por la biblioteca.
I2CError I2C_Master_GetLastError(void);

#endif /* I2CLIB_H_ */
