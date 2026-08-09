/*
 * I2CLIB.h
 *
 * Inicializacion TWI en modo esclavo para ATmega328P.
 */

#ifndef I2CLIB_H_
#define I2CLIB_H_

#include <stdint.h>

uint8_t I2C_Slave_Init(uint8_t address);

#endif /* I2CLIB_H_ */
