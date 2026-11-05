#ifndef PARQUEO_I2C_H
#define PARQUEO_I2C_H

#include <stdint.h>

/* Direccion de 7 bits de la STM esclavo 2. El ESP32 usa este valor sin desplazar. */
#define PARQUEO_DIRECCION_I2C 0x21U
#define PARQUEO_GRUPO_SIN_DATOS 0xFFU

/* Un byte: bits 0-3 = los cuatro parqueos locales, 1 ocupado y 0 libre.
 * Los bits 4-7 siempre son cero. El maestro debe solicitar exactamente un byte. */
extern volatile uint8_t i2c_mascara_ocupados;
extern volatile uint32_t i2c_lecturas;
extern volatile uint32_t i2c_errores;
extern volatile uint32_t i2c_ultimo_error;

void ParqueoI2C_Init(void);
void ParqueoI2C_Update(void);
uint8_t ParqueoI2C_MascaraRemota(void);

#endif /* PARQUEO_I2C_H */
