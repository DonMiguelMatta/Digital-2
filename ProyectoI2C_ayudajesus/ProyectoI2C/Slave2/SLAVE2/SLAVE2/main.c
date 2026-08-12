/*
 * Proyecto CarWash automatico - Slave 2
 *
 * Author: Miguel Donis 22993 - Ian Farrington 21952
 * Description: Lectura IR y control de stepper y servo de puerta
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

#include "I2CLIB/I2CLIB.h"
#include "IR-FLYINGFISH/InfraRojo.h"
#include "PROTOCOL/Protocol.h"
#include "SERVO/Servo.h"
#include "STEPPER/Stepper.h"

#define SLAVE2_I2C_ADDRESS 0x12U
#define SLAVE2_SAMPLE_INTERVAL_MS 10U

typedef struct
{
    uint8_t valid;
    uint8_t raw_level;
    uint8_t counter;
} InfraredRawSample;

static InfraredRawSample infrared_sample = {0U, 1U, 0U};
static uint8_t infrared_initialized = 0U;
static uint8_t stepper_initialized = 0U;
static uint8_t servo_door_initialized = 0U;

/****************************************/
// Function prototypes
// No se requieren prototipos locales.

/****************************************/
// NON-Interrupt subroutines

// Codifica y publica una respuesta I2C.
static uint8_t Slave2_SendResponse(const ProtocolResponse *response)
{
    uint8_t frame[PROTOCOL_FRAME_SIZE];

    Protocol_EncodeResponse(response, frame);
    return I2C_Slave_SetResponse(frame);
}

// Responde unicamente con un estado Protocol.
static uint8_t Slave2_SendStatus(const ProtocolRequest *request,
                                 ProtocolStatus status)
{
    ProtocolResponse response;

    (void)Protocol_CreateResponse(&response,
                                  request->command,
                                  request->device,
                                  status,
                                  0U,
                                  0);
    return Slave2_SendResponse(&response);
}

// Devuelve el estado local solicitado.
static void Slave2_HandlePing(const ProtocolRequest *request)
{
    ProtocolResponse response;
    StepperState stepper_state;
    uint8_t data[PROTOCOL_RESPONSE_DATA_SIZE] = {0U, 0U, 0U, 0U};
    uint8_t length = 0U;
    ProtocolStatus status = OK;

    switch ((ProtocolDevice)request->device)
    {
        case IR:
        case ALL_LOCAL_ACTUATORS:
            break;

        case STEPPER:
            if (stepper_initialized == 0U)
            {
                status = UNSUPPORTED;
                break;
            }

            Stepper_GetState(&stepper_state);
            data[0] = stepper_state.running;
            data[1] = (uint8_t)stepper_state.direction;
            data[2] = (uint8_t)(stepper_state.remaining_steps >> 8U);
            data[3] = (uint8_t)stepper_state.remaining_steps;
            length = PROTOCOL_RESPONSE_DATA_SIZE;
            break;

        case SERVO_DOOR:
            if (servo_door_initialized == 0U)
            {
                status = UNSUPPORTED;
                break;
            }

            data[0] = Servo_GetAngle();
            length = 1U;
            break;

        case HCSR04:
        case SERVO_WATER:
        case DC_MOTOR:
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
    (void)Slave2_SendResponse(&response);
}

// Entrega la ultima muestra cruda del sensor IR.
static void Slave2_HandleSensorRead(const ProtocolRequest *request)
{
    ProtocolResponse response;
    uint8_t data[PROTOCOL_RESPONSE_DATA_SIZE];
    ProtocolStatus status;

    if (request->device != (uint8_t)IR)
    {
        (void)Slave2_SendStatus(request, UNKNOWN_DEVICE);
        return;
    }

    data[0] = infrared_sample.valid;
    data[1] = infrared_sample.raw_level;
    data[2] = 0U;
    data[3] = infrared_sample.counter;
    status = (infrared_sample.valid != 0U) ? OK : NO_SAMPLE;

    (void)Protocol_CreateResponse(&response,
                                  request->command,
                                  request->device,
                                  status,
                                  PROTOCOL_RESPONSE_DATA_SIZE,
                                  data);
    (void)Slave2_SendResponse(&response);
}

// Ejecuta el movimiento solicitado al stepper.
static void Slave2_HandleStepperMove(const ProtocolRequest *request)
{
    uint16_t steps;
    uint16_t speed;
    StepperDirection direction;

    if (stepper_initialized == 0U)
    {
        (void)Slave2_SendStatus(request, UNSUPPORTED);
        return;
    }

    if (Slave2_SendStatus(request, ACCEPTED) == 0U)
    {
        return;
    }

    direction = (request->data[0] ==
                 (uint8_t)PROTOCOL_DIRECTION_FORWARD) ?
                STEPPER_DIRECTION_FORWARD : STEPPER_DIRECTION_REVERSE;
    steps = ((uint16_t)request->data[1] << 8U) |
            (uint16_t)request->data[2];
    speed = ((uint16_t)request->data[3] << 8U) |
            (uint16_t)request->data[4];

    if (steps == 0U)
    {
        (void)Stepper_RunContinuous(direction, speed);
    }
    else
    {
        (void)Stepper_Move(direction, steps, speed);
    }
}

// Ejecuta una solicitud Protocol ya validada.
static void Slave2_HandleValidatedRequest(const ProtocolRequest *request)
{
    // Selecciona sensor o actuador sin ejecutar trabajo dentro de la ISR TWI.
    switch ((ProtocolCommand)request->command)
    {
        case CMD_PING:
            Slave2_HandlePing(request);
            break;

        case CMD_READ_SENSOR:
            Slave2_HandleSensorRead(request);
            break;

        case CMD_SERVO_POSITION:
            if (request->device == (uint8_t)SERVO_DOOR)
            {
                if (servo_door_initialized == 0U)
                {
                    (void)Slave2_SendStatus(request, UNSUPPORTED);
                }
                // Publica ACCEPTED antes de aplicar fisicamente el nuevo angulo.
                else if (Slave2_SendStatus(request, ACCEPTED) != 0U)
                {
                    (void)Servo_SetAngle(request->data[0]);
                }
            }
            else
            {
                (void)Slave2_SendStatus(request, UNKNOWN_DEVICE);
            }
            break;

        case CMD_DC_MOTOR:
            (void)Slave2_SendStatus(request, UNKNOWN_DEVICE);
            break;

        case CMD_STEPPER_MOVE:
            Slave2_HandleStepperMove(request);
            break;

        case CMD_STOP:
            if ((request->device == (uint8_t)STEPPER) ||
                (request->device == (uint8_t)ALL_LOCAL_ACTUATORS))
            {
                if (Slave2_SendStatus(request, ACCEPTED) != 0U)
                {
                    Stepper_Stop(1U);
                }
            }
            else
            {
                (void)Slave2_SendStatus(request, UNKNOWN_DEVICE);
            }
            break;

        default:
            (void)Slave2_SendStatus(request, UNKNOWN_COMMAND);
            break;
    }
}

// Recibe, valida y responde solicitudes I2C.
static void Slave2_ServiceI2C(void)
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
        (void)Slave2_SendStatus(&request, BUSY);
        return;
    }

    if (validation != OK)
    {
        (void)Slave2_SendStatus(&request, validation);
        return;
    }

    Slave2_HandleValidatedRequest(&request);
}

// Actualiza periodicamente la muestra infrarroja.
static void Slave2_UpdateSensor(void)
{
    infrared_sample.valid = infrared_initialized;
    if (infrared_initialized != 0U)
    {
        infrared_sample.raw_level = InfraRojo_ReadPinLevel();
    }
    // El contador permite al Master distinguir una muestra nueva.
    infrared_sample.counter++;
}

/****************************************/
// Main Function

// Inicializa el Slave2 y atiende sensor e I2C.
int main(void)
{
    const StepperConfig stepper_config =
    {
        {&DDRD, &DDRD, &DDRD, &DDRD},
        {&PORTD, &PORTD, &PORTD, &PORTD},
        {PD2, PD3, PD4, PD5}
    };
    uint8_t delay_ms;

    // Configura IR, stepper, servo de puerta e I2C Slave.
    infrared_initialized = InfraRojo_Init(&DDRC,
                                          &PORTC,
                                          &PINC,
                                          PC0,
                                          0U);
    stepper_initialized = Stepper_Init(&stepper_config,
                                       STEPPER_MODE_HALF_STEP);
    servo_door_initialized = Servo_Init(&DDRD, &PORTD, PD6);

    (void)I2C_Slave_Init(SLAVE2_I2C_ADDRESS);
    sei();

    // Actualiza el IR y atiende solicitudes sin bloquear el stepper.
    while (1)
    {
        Slave2_ServiceI2C();
        Slave2_UpdateSensor();

        for (delay_ms = 0U;
             delay_ms < SLAVE2_SAMPLE_INTERVAL_MS;
             delay_ms++)
        {
            Slave2_ServiceI2C();
            _delay_ms(1);
        }
    }
}

/****************************************/
// Interrupt routines
// Las interrupciones TWI y Timer2 estan en sus librerias.
