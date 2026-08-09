#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>
#include <util/atomic.h>
#include <util/delay.h>

#include "HCR/HC_SR04.h"
#include "I2CLIB/I2CLIB.h"
#include "SERVO/Servo.h"

#define SLAVE1_I2C_ADDRESS          0x11U
#define SLAVE1_RESPONSE_BYTES       4U
#define SLAVE1_STATUS_HCSR04_OK     0x01U
#define SLAVE1_SERVO_CLOSED_ANGLE   0U
#define SLAVE1_SERVO_OPEN_ANGLE     90U
#define SLAVE1_CMD_OPEN_SERVO       'o'
#define SLAVE1_CMD_CLOSE_SERVO      'c'

#define SLAVE1_TWI_ACK() \
    do { \
        TWCR = (1U << TWINT) | (1U << TWEA) | \
               (1U << TWEN)  | (1U << TWIE); \
    } while (0)

static volatile uint8_t slave1_tx_buffer[SLAVE1_RESPONSE_BYTES] =
{
    0U,
    (uint8_t)HCSR04_INVALID_PULSE_US,
    (uint8_t)(HCSR04_INVALID_PULSE_US >> 8),
    SLAVE1_SERVO_CLOSED_ANGLE
};

static volatile uint8_t slave1_tx_index = 0U;
static volatile uint8_t slave1_command = 0U;
static volatile uint8_t slave1_command_pending = 0U;

static void Slave1_UpdateResponse(
    uint8_t status,
    uint16_t echo_us,
    uint8_t servo_angle);

static void Slave1_ExecutePendingCommand(void);

static void Slave1_UpdateResponse(
    uint8_t status,
    uint16_t echo_us,
    uint8_t servo_angle)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        slave1_tx_buffer[0] = status;
        slave1_tx_buffer[1] = (uint8_t)echo_us;
        slave1_tx_buffer[2] = (uint8_t)(echo_us >> 8);
        slave1_tx_buffer[3] = servo_angle;
    }
}

static void Slave1_ExecutePendingCommand(void)
{
    uint8_t command = 0U;
    uint8_t has_command = 0U;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        if (slave1_command_pending != 0U)
        {
            command = slave1_command;
            slave1_command_pending = 0U;
            has_command = 1U;
        }
    }

    if (has_command == 0U)
    {
        return;
    }

    if ((command == SLAVE1_CMD_OPEN_SERVO) ||
        (command == (uint8_t)'O'))
    {
        (void)Servo_SetAngle(SLAVE1_SERVO_OPEN_ANGLE);
    }
    else if ((command == SLAVE1_CMD_CLOSE_SERVO) ||
             (command == (uint8_t)'C'))
    {
        (void)Servo_SetAngle(SLAVE1_SERVO_CLOSED_ANGLE);
    }
}

int main(void)
{
    uint16_t echo_us = HCSR04_INVALID_PULSE_US;
    uint8_t status = 0U;

    (void)HCSR04_Init(
        &DDRC,
        &PORTC,
        PC3,
        &DDRC,
        &PINC,
        PC2);

    (void)Servo_Init(&DDRB, &PORTB, PB3);
    (void)Servo_SetAngle(SLAVE1_SERVO_CLOSED_ANGLE);

    I2C_Slave_Init(SLAVE1_I2C_ADDRESS);
    sei();

    while (1)
    {
        Slave1_ExecutePendingCommand();

        if (HCSR04_ReadEchoPulseUS(&echo_us) != 0U)
        {
            status = SLAVE1_STATUS_HCSR04_OK;
        }
        else
        {
            status = 0U;
            echo_us = HCSR04_INVALID_PULSE_US;
        }

        Slave1_UpdateResponse(status, echo_us, Servo_GetAngle());
        _delay_ms(60);
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
            slave1_tx_index = 0U;
            SLAVE1_TWI_ACK();
            break;

        case 0x80U:
        case 0x90U:
            slave1_command = TWDR;
            slave1_command_pending = 1U;
            SLAVE1_TWI_ACK();
            break;

        case 0xA0U:
            SLAVE1_TWI_ACK();
            break;

        case 0xA8U:
        case 0xB0U:
            slave1_tx_index = 0U;
            TWDR = slave1_tx_buffer[slave1_tx_index];
            slave1_tx_index++;
            SLAVE1_TWI_ACK();
            break;

        case 0xB8U:
            if (slave1_tx_index < SLAVE1_RESPONSE_BYTES)
            {
                TWDR = slave1_tx_buffer[slave1_tx_index];
                slave1_tx_index++;
            }
            else
            {
                TWDR = 0U;
            }
            SLAVE1_TWI_ACK();
            break;

        case 0xC0U:
        case 0xC8U:
        default:
            SLAVE1_TWI_ACK();
            break;
    }
}
