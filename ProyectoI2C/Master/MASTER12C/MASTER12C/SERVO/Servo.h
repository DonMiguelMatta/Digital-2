/*
 * Servo.h
 *
 * Servo modular de 0 a 180 grados mediante Timer1.
 */

#ifndef SERVO_H_
#define SERVO_H_

#include <stdint.h>

#define SERVO_MIN_ANGLE 0u
#define SERVO_MAX_ANGLE 180u

uint8_t Servo_Init(volatile uint8_t *ddr,
                   volatile uint8_t *port,
                   uint8_t pin);
uint8_t Servo_SetAngle(uint8_t angle);
uint8_t Servo_GetAngle(void);

#endif /* SERVO_H_ */
