/*
 * Proyecto CarWash automatico - Slave 1
 *
 * Author: Miguel Donis 22993 - Ian Farrington 21952
 * Description: Lectura HC-SR04 y control de servo de agua y motor DC
 */
/****************************************/
// Encabezado (Libraries)

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>
#include <util/delay.h>

#include "HCR/HC_SR04.h"
#include "I2CLIB/I2CLIB.h"
#include "L298N/L298N.h"
#include "PROTOCOL/Protocol.h"
#include "SERVO/Servo.h"

#define SLAVE1_I2C_ADDRESS 0x11U
#define SLAVE1_L298N_ENA_PIN PB1
#define SLAVE1_L298N_IN1_PIN PD7
#define SLAVE1_L298N_IN2_PIN PB0
#define SLAVE1_SAMPLE_INTERVAL_MS 60U

typedef struct
{
    uint8_t valid;
    uint16_t echo_us;
    uint8_t counter;
} HCSR04RawSample;

static HCSR04RawSample hcsr04_sample =
{
    0U,
    HCSR04_INVALID_PULSE_US,
    0U
};

static uint8_t dc_motor_initialized = 0U;

/****************************************/
// Function prototypes
// No se requieren prototipos locales.

/****************************************/
// NON-Interrupt subroutines

// Codifica y publica una respuesta I2C.
static uint8_t Slave1_SendResponse(const ProtocolResponse *response)
{
    uint8_t frame[PROTOCOL_FRAME_SIZE];

    Protocol_EncodeResponse(response, frame);
    return I2C_Slave_SetResponse(frame);
}

// Responde unicamente con un estado Protocol.
static uint8_t Slave1_SendStatus(const ProtocolRequest *request,
                                 ProtocolStatus status)
{
    ProtocolResponse response;

    (void)Protocol_CreateResponse(&response,
                                  request->command,
                                  request->device,
                                  status,
                                  0U,
                                  0);
    return Slave1_SendResponse(&response);
}

// Devuelve el estado local solicitado.
static void Slave1_HandlePing(const ProtocolRequest *request)
{
    ProtocolResponse response;
    L298NState dc_state;
    uint8_t data[PROTOCOL_RESPONSE_DATA_SIZE] = {0U, 0U, 0U, 0U};
    uint8_t length = 0U;
    ProtocolStatus status = OK;

    switch ((ProtocolDevice)request->device)
    {
        case HCSR04:
        case ALL_LOCAL_ACTUATORS:
            break;

        case SERVO_WATER:
            data[0] = Servo_GetAngle();
            length = 1U;
            break;

        case DC_MOTOR:
            if (dc_motor_initialized == 0U)
            {
                status = UNSUPPORTED;
            }
            else
            {
                L298N_GetState(&dc_state);
                data[0] = dc_state.direction;
                data[1] = dc_state.pwm;
                data[2] = dc_state.enabled;
                length = 3U;
            }
            break;

        case SERVO_DOOR:
            status = UNSUPPORTED;
            break;

        case IR:
        case STEPPER:
        default:
            status = UNKNOWN_DEVICE;
            break;
    }

    (void)Protocol_CreateResponse(&response,
                                  request->command,
                                  request->device,
                                  status,
                                  length,
                                  data);
    (void)Slave1_SendResponse(&response);
}

// Entrega la ultima muestra cruda del HC-SR04.
static void Slave1_HandleSensorRead(const ProtocolRequest *request)
{
    ProtocolResponse response;
    uint8_t data[PROTOCOL_RESPONSE_DATA_SIZE];
    ProtocolStatus status;

    if (request->device != (uint8_t)HCSR04)
    {
        (void)Slave1_SendStatus(request, UNKNOWN_DEVICE);
        return;
    }

    data[0] = hcsr04_sample.valid;
    data[1] = (uint8_t)(hcsr04_sample.echo_us >> 8U);
    data[2] = (uint8_t)hcsr04_sample.echo_us;
    data[3] = hcsr04_sample.counter;
    status = (hcsr04_sample.valid != 0U) ? OK : NO_SAMPLE;

    (void)Protocol_CreateResponse(&response,
                                  request->command,
                                  request->device,
                                  status,
                                  PROTOCOL_RESPONSE_DATA_SIZE,
                                  data);
    (void)Slave1_SendResponse(&response);
}

// Ejecuta una solicitud Protocol ya validada.
static void Slave1_HandleValidatedRequest(const ProtocolRequest *request)
{
    // Selecciona sensor o actuador sin ejecutar trabajo dentro de la ISR TWI.
    switch ((ProtocolCommand)request->command)
    {
        case CMD_PING:
            Slave1_HandlePing(request);
            break;

        case CMD_READ_SENSOR:
            Slave1_HandleSensorRead(request);
            break;

        case CMD_SERVO_POSITION:
            if (request->device == (uint8_t)SERVO_DOOR)
            {
                (void)Slave1_SendStatus(request, UNSUPPORTED);
            }
            else if (request->device != (uint8_t)SERVO_WATER)
            {
                (void)Slave1_SendStatus(request, UNKNOWN_DEVICE);
            }
            // Publica ACCEPTED antes de aplicar fisicamente el nuevo angulo.
            else if (Slave1_SendStatus(request, ACCEPTED) != 0U)
            {
                (void)Servo_SetAngle(request->data[0]);
            }
            break;

        case CMD_DC_MOTOR:
            if (request->device != (uint8_t)DC_MOTOR)
            {
                (void)Slave1_SendStatus(request, UNKNOWN_DEVICE);
            }
            else if (dc_motor_initialized == 0U)
            {
                (void)Slave1_SendStatus(request, UNSUPPORTED);
            }
            else if (Slave1_SendStatus(request, ACCEPTED) != 0U)
            {
                (void)L298N_Set(
                    (request->data[0] ==
                     (uint8_t)PROTOCOL_DIRECTION_FORWARD) ?
                    L298N_DIRECTION_FORWARD : L298N_DIRECTION_REVERSE,
                    request->data[1]);
            }
            break;

        case CMD_STEPPER_MOVE:
            (void)Slave1_SendStatus(request, UNKNOWN_DEVICE);
            break;

        case CMD_STOP:
            if ((request->device == (uint8_t)DC_MOTOR) ||
                (request->device == (uint8_t)ALL_LOCAL_ACTUATORS))
            {
                if (Slave1_SendStatus(request, ACCEPTED) != 0U)
                {
                    L298N_Stop();
                }
            }
            else
            {
                (void)Slave1_SendStatus(request, UNKNOWN_DEVICE);
            }
            break;

        default:
            (void)Slave1_SendStatus(request, UNKNOWN_COMMAND);
            break;
    }
}

// Recibe, valida y responde solicitudes I2C.
static void Slave1_ServiceI2C(void)
{
    uint8_t frame[PROTOCOL_FRAME_SIZE];
    I2CSlaveRequestState request_state;
    ProtocolRequest request;
    ProtocolStatus validation;

    if (I2C_Slave_RequestAvailable() == 0U)
    {
        return;
    }

    request_state = I2C_Slave_GetRequest(frame);
    if (request_state == I2C_SLAVE_REQUEST_NONE)
    {
        return;
    }

    // Decodifica y valida la trama congelada fuera de la interrupcion.
    validation = Protocol_DecodeRequest(frame, &request);

    // Una segunda solicitud conservada recibe BUSY y no reemplaza la primera.
    if (request_state == I2C_SLAVE_REQUEST_REJECTED_BUSY)
    {
        (void)Slave1_SendStatus(&request, BUSY);
        return;
    }

    if (validation != OK)
    {
        (void)Slave1_SendStatus(&request, validation);
        return;
    }

    Slave1_HandleValidatedRequest(&request);
}

// Actualiza periodicamente la muestra HC-SR04.
static void Slave1_UpdateSensor(void)
{
    uint16_t echo_us = HCSR04_INVALID_PULSE_US;

    if (HCSR04_ReadEchoPulseUS(&echo_us) != 0U)
    {
        hcsr04_sample.valid = 1U;
        hcsr04_sample.echo_us = echo_us;
    }
    else
    {
        hcsr04_sample.valid = 0U;
        hcsr04_sample.echo_us = HCSR04_INVALID_PULSE_US;
    }

    // El contador permite al Master distinguir una muestra nueva.
    hcsr04_sample.counter++;
}

/****************************************/
// Main Function

// Inicializa el Slave1 y atiende sensor e I2C.
int main(void)
{
    uint8_t delay_ms;

    // Configura HC-SR04, servo de agua, puente H e I2C Slave.
    (void)HCSR04_Init(&DDRC,
                      &PORTC,
                      PC3,
                      &DDRC,
                      &PINC,
                      PC2);

    (void)Servo_Init(&DDRB, &PORTB, PB3);

    // L298N: ENA=D9, IN1=D7 e IN2=D8 en la placa Arduino Nano.
    dc_motor_initialized = L298N_Init(&DDRB,
                                     &PORTB,
                                     SLAVE1_L298N_ENA_PIN,
                                     &DDRD,
                                     &PORTD,
                                     SLAVE1_L298N_IN1_PIN,
                                     &DDRB,
                                     &PORTB,
                                     SLAVE1_L298N_IN2_PIN);
    L298N_Stop();

    (void)I2C_Slave_Init(SLAVE1_I2C_ADDRESS);
    sei();

    // Mide localmente y procesa solicitudes entre cada adquisicion.
    while (1)
    {
        Slave1_ServiceI2C();
        Slave1_UpdateSensor();
        Slave1_ServiceI2C();

        for (delay_ms = 0U;
             delay_ms < SLAVE1_SAMPLE_INTERVAL_MS;
             delay_ms++)
        {
            Slave1_ServiceI2C();
            _delay_ms(1);
        }
    }
}

/****************************************/
// Interrupt routines
// La interrupcion TWI se encuentra dentro de I2CLIB.c.
