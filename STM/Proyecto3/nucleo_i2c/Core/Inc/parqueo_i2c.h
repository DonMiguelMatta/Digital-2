#ifndef PARQUEO_I2C_H
#define PARQUEO_I2C_H
#include <stdint.h>

#define PARQUEO_DIRECCION_I2C 0x20U
extern volatile uint8_t i2c_mascara_ocupados;
extern volatile uint32_t i2c_lecturas;
extern volatile uint32_t i2c_errores;
extern volatile uint32_t i2c_ultimo_error;

void ParqueoI2C_Init(void);
void ParqueoI2C_Update(void);
#endif
