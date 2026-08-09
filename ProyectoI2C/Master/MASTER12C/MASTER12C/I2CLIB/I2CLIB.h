/*
 * I2C.h
 *
 * Libreria I2C/TWI para ATmega328P
 * Microchip Studio - C
 *
 * SDA = PC4
 * SCL = PC5
 */

#ifndef I2C_H_
#define I2C_H_

#include <avr/io.h>
#include <stdint.h>

/******************************************************************************/
// Funcion para inicializar I2C Maestro
/******************************************************************************/
void I2C_Master_Init(unsigned long SCL_Clock, uint8_t Prescaler);

/******************************************************************************/
// Funcion de inicio de la comunicacion I2C
/******************************************************************************/
uint8_t I2C_Master_Start(void);

/******************************************************************************/
// Funcion de reinicio de la comunicacion I2C
/******************************************************************************/
uint8_t I2C_Master_RepeatedStart(void);

/******************************************************************************/
// Funcion de parada de la comunicacion I2C
/******************************************************************************/
void I2C_Master_Stop(void);

/******************************************************************************/
// Funcion de transmision de datos del maestro al esclavo
// Retorna 1 si el byte fue transmitido correctamente con ACK.
/******************************************************************************/
uint8_t I2C_Master_Write(uint8_t dato);

/******************************************************************************/
// Funcion de recepcion de datos enviados por el esclavo al maestro
//
// ack = 1 -> el maestro responde ACK porque desea recibir mas datos
// ack = 0 -> el maestro responde NACK porque es el ultimo byte
//
// Retorna 1 si la lectura fue correcta.
/******************************************************************************/
uint8_t I2C_Master_Read(uint8_t *buffer, uint8_t ack);

/******************************************************************************/
// Ultimo estado TWI observado por la libreria.
/******************************************************************************/
uint8_t I2C_Master_GetLastStatus(void);

/******************************************************************************/
// Funcion para inicializar I2C Esclavo
/******************************************************************************/
void I2C_Slave_Init(uint8_t address);

#endif /* I2C_H_ */	
