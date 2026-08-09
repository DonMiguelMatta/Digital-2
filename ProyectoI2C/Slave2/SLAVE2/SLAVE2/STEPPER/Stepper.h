/*
 * Stepper.h
 *
 * Driver reutilizable para motores unipolares de cuatro fases.
 */

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

uint8_t Stepper_Init(const StepperConfig *config, StepperMode mode);
uint8_t Stepper_RunContinuous(StepperDirection direction,
                              uint16_t steps_per_second);
uint8_t Stepper_Move(StepperDirection direction,
                     uint16_t steps,
                     uint16_t steps_per_second);
void Stepper_Stop(uint8_t release_coils);
uint8_t Stepper_IsRunning(void);
void Stepper_GetState(StepperState *state);

#endif /* STEPPER_H_ */
