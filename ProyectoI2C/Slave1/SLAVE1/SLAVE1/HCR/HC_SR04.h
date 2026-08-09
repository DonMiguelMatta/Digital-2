/*
 * HC_SR04.h
 *
 * Adquisicion modular del ancho crudo de ECHO.
 */

#ifndef HC_SR04_H_
#define HC_SR04_H_

#include <stdint.h>

#define HCSR04_INVALID_PULSE_US 0xFFFFu

uint8_t HCSR04_Init(volatile uint8_t *trig_ddr,
                    volatile uint8_t *trig_port,
                    uint8_t trig_pin,
                    volatile uint8_t *echo_ddr,
                    volatile uint8_t *echo_pin_register,
                    uint8_t echo_pin);
uint8_t HCSR04_ReadEchoPulseUS(uint16_t *echo_us);

#endif /* HC_SR04_H_ */
