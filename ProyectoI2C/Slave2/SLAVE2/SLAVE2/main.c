#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>
#include <util/atomic.h>
#include <util/delay.h>

#include "I2CLIB/I2CLIB.h"
#include "IR-FLYINGFISH/InfraRojo.h"
#include "STEPPER/Stepper.h"

#define SLAVE2_I2C_ADDRESS           0x12U
#define SLAVE2_RESPONSE_BYTES        4U
#define SLAVE2_STATUS_IR_OK          0x01U
#define SLAVE2_STATUS_STEPPER_OK     0x02U
#define SLAVE2_STEPS_PER_SECOND      500U
#define SLAVE2_CMD_STEPPER_FORWARD   'f'
#define SLAVE2_CMD_STEPPER_REVERSE   'b'
#define SLAVE2_CMD_STEPPER_STOP      'x'

#define SLAVE2_TWI_ACK() \
    do { \
        TWCR = (1U << TWINT) | (1U << TWEA) | \
               (1U << TWEN)  | (1U << TWIE); \
    } while (0)

static volatile uint8_t slave2_tx_buffer[SLAVE2_RESPONSE_BYTES] =
{
    0U,
    1U,
    0U,
    (uint8_t)STEPPER_DIRECTION_FORWARD
};

static volatile uint8_t slave2_tx_index = 0U;
static volatile uint8_t slave2_command = 0U;
static volatile uint8_t slave2_command_pending = 0U;

static void Slave2_UpdateResponse(uint8_t status,
                                  uint8_t infrared_level,
                                  const StepperState *stepper_state)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        slave2_tx_buffer[0] = status;
        slave2_tx_buffer[1] = infrared_level;
        slave2_tx_buffer[2] = stepper_state->running;
        slave2_tx_buffer[3] = (uint8_t)stepper_state->direction;
    }
}

static void Slave2_ExecutePendingCommand(void)
{
    uint8_t command = 0U;
    uint8_t has_command = 0U;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        if (slave2_command_pending != 0U)
        {
            command = slave2_command;
            slave2_command_pending = 0U;
            has_command = 1U;
        }
    }

    if (has_command == 0U)
    {
        return;
    }

    if ((command == (uint8_t)SLAVE2_CMD_STEPPER_FORWARD) ||
        (command == (uint8_t)'F'))
    {
        (void)Stepper_RunContinuous(
            STEPPER_DIRECTION_FORWARD,
            SLAVE2_STEPS_PER_SECOND);
    }
    else if ((command == (uint8_t)SLAVE2_CMD_STEPPER_REVERSE) ||
             (command == (uint8_t)'B'))
    {
        (void)Stepper_RunContinuous(
            STEPPER_DIRECTION_REVERSE,
            SLAVE2_STEPS_PER_SECOND);
    }
    else if ((command == (uint8_t)SLAVE2_CMD_STEPPER_STOP) ||
             (command == (uint8_t)'X'))
    {
        Stepper_Stop(1U);
    }
}

int main(void)
{
    const StepperConfig stepper_config =
    {
        { &DDRD, &DDRD, &DDRD, &DDRD },
        { &PORTD, &PORTD, &PORTD, &PORTD },
        { PD2, PD3, PD4, PD5 }
    };
    StepperState stepper_state;
    uint8_t status = 0U;
    uint8_t infrared_level = 1U;

    if (InfraRojo_Init(
            &DDRC,
            &PORTC,
            &PINC,
            PC0,
            0U) != 0U)
    {
        status |= SLAVE2_STATUS_IR_OK;
    }

    if (Stepper_Init(&stepper_config, STEPPER_MODE_HALF_STEP) != 0U)
    {
        status |= SLAVE2_STATUS_STEPPER_OK;
    }

    (void)I2C_Slave_Init(SLAVE2_I2C_ADDRESS);
    sei();

    while (1)
    {
        Slave2_ExecutePendingCommand();

        if ((status & SLAVE2_STATUS_IR_OK) != 0U)
        {
            infrared_level = InfraRojo_ReadPinLevel();
        }

        Stepper_GetState(&stepper_state);
        Slave2_UpdateResponse(status, infrared_level, &stepper_state);
        _delay_ms(10);
    }
}

ISR(TWI_vect)
{
    uint8_t twi_status = (uint8_t)(TWSR & 0xF8U);

    switch (twi_status)
    {
        case 0x60U:
        case 0x68U:
        case 0x70U:
        case 0x78U:
            slave2_tx_index = 0U;
            SLAVE2_TWI_ACK();
            break;

        case 0x80U:
        case 0x90U:
            slave2_command = TWDR;
            slave2_command_pending = 1U;
            SLAVE2_TWI_ACK();
            break;

        case 0xA0U:
            SLAVE2_TWI_ACK();
            break;

        case 0xA8U:
        case 0xB0U:
            slave2_tx_index = 0U;
            TWDR = slave2_tx_buffer[slave2_tx_index];
            slave2_tx_index++;
            SLAVE2_TWI_ACK();
            break;

        case 0xB8U:
            if (slave2_tx_index < SLAVE2_RESPONSE_BYTES)
            {
                TWDR = slave2_tx_buffer[slave2_tx_index];
                slave2_tx_index++;
            }
            else
            {
                TWDR = 0U;
            }
            SLAVE2_TWI_ACK();
            break;

        case 0xC0U:
        case 0xC8U:
        default:
            SLAVE2_TWI_ACK();
            break;
    }
}
