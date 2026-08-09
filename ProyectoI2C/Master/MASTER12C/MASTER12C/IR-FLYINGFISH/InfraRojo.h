/*
 * InfraRojo.h
 *
 * Lectura electrica cruda de un pin digital.
 */

#ifndef INFRAROJO_H_
#define INFRAROJO_H_

#include <stdint.h>

uint8_t InfraRojo_Init(volatile uint8_t *ddr,
                       volatile uint8_t *port,
                       volatile uint8_t *pin_register,
                       uint8_t pin,
                       uint8_t enable_pullup);
uint8_t InfraRojo_ReadPinLevel(void);

#endif /* INFRAROJO_H_ */
