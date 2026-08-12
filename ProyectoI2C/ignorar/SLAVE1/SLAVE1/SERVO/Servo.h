/*
 * Biblioteca Servo
 *
 * Author: Miguel Donis 22993 - Ian Farrington 21952
 * Description: Interfaz de posicion para un servo de 0 a 180 grados
 */
/****************************************/
// Encabezado (Libraries)

#ifndef SERVO_H_
#define SERVO_H_

#include <stdint.h>

#define SERVO_MIN_ANGLE 0u
#define SERVO_MAX_ANGLE 180u

/****************************************/
// Function prototypes

// Asocia el pin de salida e inicia Timer1; devuelve 1 si es valido.
uint8_t Servo_Init(volatile uint8_t *ddr,
                   volatile uint8_t *port,
                   uint8_t pin);
// Programa el angulo solicitado; rechaza valores mayores a 180 grados.
uint8_t Servo_SetAngle(uint8_t angle);
// Devuelve el ultimo angulo aceptado.
uint8_t Servo_GetAngle(void);

#endif /* SERVO_H_ */
