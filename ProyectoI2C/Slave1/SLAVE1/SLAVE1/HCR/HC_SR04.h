/*
 * Biblioteca HC-SR04
 *
 * Author: Miguel Donis 22993 - Ian Farrington 21952
 * Description: Interfaz para adquirir el ancho crudo de ECHO
 */
/****************************************/
// Encabezado (Libraries)

#ifndef HC_SR04_H_
#define HC_SR04_H_

#include <stdint.h>

#define HCSR04_INVALID_PULSE_US 0xFFFFu

/****************************************/
// Function prototypes

// Recibe registros y pines de TRIG/ECHO; devuelve 1 si son validos.
uint8_t HCSR04_Init(volatile uint8_t *trig_ddr,
                    volatile uint8_t *trig_port,
                    uint8_t trig_pin,
                    volatile uint8_t *echo_ddr,
                    volatile uint8_t *echo_pin_register,
                    uint8_t echo_pin);
// Guarda el pulso ECHO en microsegundos; devuelve 0 ante timeout.
uint8_t HCSR04_ReadEchoPulseUS(uint16_t *echo_us);

#endif /* HC_SR04_H_ */
