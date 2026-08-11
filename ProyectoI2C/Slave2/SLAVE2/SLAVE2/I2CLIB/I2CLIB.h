/*
 * Biblioteca I2C Slave
 *
 * Author: Miguel Donis 22993 - Ian Farrington 21952
 * Description: Interfaz de transporte TWI para tramas fijas de ocho bytes
 */
/****************************************/
// Encabezado (Libraries)

#ifndef I2CLIB_H_
#define I2CLIB_H_

#include <stdint.h>

#define I2C_FRAME_SIZE 8U

typedef enum
{
    I2C_SLAVE_REQUEST_NONE = 0,
    I2C_SLAVE_REQUEST_READY,
    I2C_SLAVE_REQUEST_REJECTED_BUSY
} I2CSlaveRequestState;

/****************************************/
// Function prototypes

// Configura TWI con la direccion de siete bits indicada.
uint8_t I2C_Slave_Init(uint8_t address);
// Devuelve 1 cuando existe una solicitud lista para main.
uint8_t I2C_Slave_RequestAvailable(void);
// Copia la solicitud e indica si fue normal o rechazada por BUSY.
I2CSlaveRequestState I2C_Slave_GetRequest(uint8_t frame[I2C_FRAME_SIZE]);
// Deja preparada una respuesta completa para la siguiente lectura del Master.
uint8_t I2C_Slave_SetResponse(const uint8_t frame[I2C_FRAME_SIZE]);

#endif /* I2CLIB_H_ */
