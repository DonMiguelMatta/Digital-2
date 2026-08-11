/*
 * Biblioteca Stepper
 *
 * Author: Miguel Donis 22993 - Ian Farrington 21952
 * Description: Interfaz para motores unipolares de cuatro fases
 */
/****************************************/
// Encabezado (Libraries)

#ifndef STEPPER_H_
#define STEPPER_H_

#include <stdint.h>

typedef enum
{
    STEPPER_DIRECTION_REVERSE = 0u,
    STEPPER_DIRECTION_FORWARD = 1u
} StepperDirection;

typedef enum
{
    STEPPER_MODE_FULL_STEP = 0u,
    STEPPER_MODE_HALF_STEP
} StepperMode;

typedef struct
{
    volatile uint8_t *ddr[4];
    volatile uint8_t *port[4];
    uint8_t pin[4];
} StepperConfig;

typedef struct
{
    StepperDirection direction;
    uint16_t steps_per_second;
    uint16_t remaining_steps;
    uint8_t running;
    uint8_t continuous;
} StepperState;

/****************************************/
// Function prototypes

// Recibe los cuatro pines y configura el modo de paso; devuelve 1 al iniciar.
uint8_t Stepper_Init(const StepperConfig *config, StepperMode mode);
// Gira sin limite de pasos hasta llamar Stepper_Stop.
uint8_t Stepper_RunContinuous(StepperDirection direction,
                              uint16_t steps_per_second);
// Gira la cantidad indicada de pasos a la velocidad solicitada.
uint8_t Stepper_Move(StepperDirection direction,
                     uint16_t steps,
                     uint16_t steps_per_second);
// Detiene el giro y permite conservar o liberar las bobinas.
void Stepper_Stop(uint8_t release_coils);
// Devuelve 1 mientras el Timer2 mantiene el movimiento.
uint8_t Stepper_IsRunning(void);
// Copia movimiento, direccion, pasos restantes y velocidad.
void Stepper_GetState(StepperState *state);

#endif /* STEPPER_H_ */
