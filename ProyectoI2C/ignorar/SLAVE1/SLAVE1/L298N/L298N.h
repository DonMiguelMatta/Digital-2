/*
 * Biblioteca L298N
 *
 * Author: Miguel Donis 22993 - Ian Farrington 21952
 * Description: Interfaz para direccion y velocidad de un motor DC
 */
/****************************************/
// Encabezado (Libraries)

#ifndef L298N_H_
#define L298N_H_

#include <stdint.h>

typedef enum
{
    L298N_DIRECTION_REVERSE = 0U,
    L298N_DIRECTION_FORWARD = 1U
} L298NDirection;

typedef struct
{
    uint8_t direction;
    uint8_t pwm;
    uint8_t enabled;
} L298NState;

/****************************************/
// Function prototypes

// Asocia ENA, IN1 e IN2 e inicializa Timer0; devuelve 1 si son validos.
uint8_t L298N_Init(volatile uint8_t *enable_ddr,
                   volatile uint8_t *enable_port,
                   uint8_t enable_pin,
                   volatile uint8_t *in1_ddr,
                   volatile uint8_t *in1_port,
                   uint8_t in1_pin,
                   volatile uint8_t *in2_ddr,
                   volatile uint8_t *in2_port,
                   uint8_t in2_pin);

// Aplica direccion y PWM; acepta valores de velocidad entre 0 y 255.
uint8_t L298N_Set(L298NDirection direction, uint8_t pwm);

// Deshabilita ENA y coloca ambas entradas de direccion en bajo.
void L298N_Stop(void);

// Copia de forma atomica la ultima direccion, PWM y estado de ENA.
void L298N_GetState(L298NState *state);

#endif /* L298N_H_ */
