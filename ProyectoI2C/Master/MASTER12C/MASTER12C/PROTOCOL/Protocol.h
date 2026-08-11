/*
 * Biblioteca Protocol
 *
 * Author: Miguel Donis 22993 - Ian Farrington 21952
 * Description: Contrato comun de tramas I2C de ocho bytes
 */
/****************************************/
// Encabezado (Libraries)

#ifndef PROTOCOL_H_
#define PROTOCOL_H_

#include <stdint.h>

#define PROTOCOL_FRAME_SIZE          8U
#define PROTOCOL_REQUEST_DATA_SIZE   5U
#define PROTOCOL_RESPONSE_DATA_SIZE  4U

typedef enum
{
    CMD_PING = 0x01U,
    CMD_READ_SENSOR = 0x02U,
    CMD_SERVO_POSITION = 0x03U,
    CMD_DC_MOTOR = 0x04U,
    CMD_STEPPER_MOVE = 0x05U,
    CMD_STOP = 0x06U
} ProtocolCommand;

typedef enum
{
    HCSR04 = 0x01U,
    IR = 0x02U,
    SERVO_WATER = 0x03U,
    SERVO_DOOR = 0x04U,
    DC_MOTOR = 0x05U,
    STEPPER = 0x06U,
    ALL_LOCAL_ACTUATORS = 0x7FU
} ProtocolDevice;

typedef enum
{
    OK = 0x00U,
    ACCEPTED = 0x01U,
    BUSY = 0x02U,
    UNKNOWN_COMMAND = 0x03U,
    UNKNOWN_DEVICE = 0x04U,
    INVALID_LENGTH = 0x05U,
    INVALID_VALUE = 0x06U,
    NO_SAMPLE = 0x07U,
    UNSUPPORTED = 0x08U
} ProtocolStatus;

typedef enum
{
    PROTOCOL_DIRECTION_REVERSE = 0U,
    PROTOCOL_DIRECTION_FORWARD = 1U
} ProtocolDirection;

typedef struct
{
    uint8_t command;
    uint8_t device;
    uint8_t length;
    uint8_t data[PROTOCOL_REQUEST_DATA_SIZE];
} ProtocolRequest;

typedef struct
{
    uint8_t command;
    uint8_t device;
    uint8_t status;
    uint8_t length;
    uint8_t data[PROTOCOL_RESPONSE_DATA_SIZE];
} ProtocolResponse;

/****************************************/
// Function prototypes

// Devuelve 1 cuando el comando pertenece al contrato.
uint8_t Protocol_IsKnownCommand(uint8_t command);
// Devuelve 1 cuando el dispositivo pertenece al contrato.
uint8_t Protocol_IsKnownDevice(uint8_t device);
// Devuelve 1 cuando el estado pertenece al contrato.
uint8_t Protocol_IsKnownStatus(uint8_t status);

// Crea y valida una solicitud con hasta cinco bytes de datos.
ProtocolStatus Protocol_CreateRequest(
    ProtocolRequest *request,
    uint8_t command,
    uint8_t device,
    uint8_t length,
    const uint8_t *data);

// Crea una respuesta con estado y hasta cuatro bytes de datos.
ProtocolStatus Protocol_CreateResponse(
    ProtocolResponse *response,
    uint8_t command,
    uint8_t device,
    ProtocolStatus status,
    uint8_t length,
    const uint8_t *data);

// Empaca una solicitud en la trama fija enviada por I2C.
void Protocol_EncodeRequest(
    const ProtocolRequest *request,
    uint8_t frame[PROTOCOL_FRAME_SIZE]);

// Empaca una respuesta en la trama fija enviada por I2C.
void Protocol_EncodeResponse(
    const ProtocolResponse *response,
    uint8_t frame[PROTOCOL_FRAME_SIZE]);

// Decodifica y valida los ocho bytes de una solicitud recibida.
ProtocolStatus Protocol_DecodeRequest(
    const uint8_t frame[PROTOCOL_FRAME_SIZE],
    ProtocolRequest *request);

// Decodifica y valida el formato de una respuesta recibida.
ProtocolStatus Protocol_DecodeResponse(
    const uint8_t frame[PROTOCOL_FRAME_SIZE],
    ProtocolResponse *response);

// Revisa la semantica y parametros de una solicitud.
ProtocolStatus Protocol_ValidateRequest(
    const ProtocolRequest *request);

// Comprueba que la respuesta corresponde a su solicitud.
ProtocolStatus Protocol_ValidateResponse(
    const ProtocolRequest *request,
    const ProtocolResponse *response);

#endif /* PROTOCOL_H_ */
