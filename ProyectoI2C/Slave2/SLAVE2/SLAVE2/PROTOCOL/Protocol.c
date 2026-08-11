/*
 * Biblioteca Protocol
 *
 * Author: Miguel Donis 22993 - Ian Farrington 21952
 * Description: Creacion, validacion y codificacion de tramas I2C
 */
/****************************************/
// Encabezado (Libraries)

#include "Protocol.h"

#include <stddef.h>

/****************************************/
// NON-Interrupt subroutines

// Rellena con cero una zona de datos de la trama.
static void Protocol_Clear(uint8_t *data, uint8_t count)
{
    uint8_t index;

    for (index = 0U; index < count; index++)
    {
        data[index] = 0U;
    }
}

// Copia datos y completa con cero los bytes no utilizados.
static void Protocol_Copy(
    uint8_t *destination,
    const uint8_t *source,
    uint8_t count)
{
    uint8_t index;

    for (index = 0U; index < count; index++)
    {
        destination[index] = source[index];
    }
}

// Comprueba si el byte corresponde a un comando definido.
uint8_t Protocol_IsKnownCommand(uint8_t command)
{
    return ((command >= (uint8_t)CMD_PING) &&
            (command <= (uint8_t)CMD_STOP)) ? 1U : 0U;
}

// Comprueba si el byte corresponde a un dispositivo definido.
uint8_t Protocol_IsKnownDevice(uint8_t device)
{
    if ((device >= (uint8_t)HCSR04) &&
        (device <= (uint8_t)STEPPER))
    {
        return 1U;
    }

    return (device == (uint8_t)ALL_LOCAL_ACTUATORS) ? 1U : 0U;
}

// Comprueba si el byte corresponde a un estado definido.
uint8_t Protocol_IsKnownStatus(uint8_t status)
{
    return (status <= (uint8_t)UNSUPPORTED) ? 1U : 0U;
}

// Valida comando, dispositivo, longitud y parametros de una solicitud.
ProtocolStatus Protocol_ValidateRequest(
    const ProtocolRequest *request)
{
    uint16_t speed;
    uint8_t index;

    if (request == NULL)
    {
        return INVALID_VALUE;
    }

    if (Protocol_IsKnownCommand(request->command) == 0U)
    {
        return UNKNOWN_COMMAND;
    }

    if (Protocol_IsKnownDevice(request->device) == 0U)
    {
        return UNKNOWN_DEVICE;
    }

    if (request->length > PROTOCOL_REQUEST_DATA_SIZE)
    {
        return INVALID_LENGTH;
    }

    for (index = request->length;
         index < PROTOCOL_REQUEST_DATA_SIZE;
         index++)
    {
        if (request->data[index] != 0U)
        {
            return INVALID_VALUE;
        }
    }

    switch ((ProtocolCommand)request->command)
    {
        case CMD_PING:
            return (request->length == 0U) ? OK : INVALID_LENGTH;

        case CMD_READ_SENSOR:
            if ((request->device != (uint8_t)HCSR04) &&
                (request->device != (uint8_t)IR))
            {
                return UNKNOWN_DEVICE;
            }

            return (request->length == 0U) ? OK : INVALID_LENGTH;

        case CMD_SERVO_POSITION:
            if ((request->device != (uint8_t)SERVO_WATER) &&
                (request->device != (uint8_t)SERVO_DOOR))
            {
                return UNKNOWN_DEVICE;
            }

            if (request->length != 1U)
            {
                return INVALID_LENGTH;
            }

            return (request->data[0] <= 180U) ? OK : INVALID_VALUE;

        case CMD_DC_MOTOR:
            if (request->device != (uint8_t)DC_MOTOR)
            {
                return UNKNOWN_DEVICE;
            }

            if (request->length != 2U)
            {
                return INVALID_LENGTH;
            }

            return (request->data[0] <=
                    (uint8_t)PROTOCOL_DIRECTION_FORWARD) ?
                   OK : INVALID_VALUE;

        case CMD_STEPPER_MOVE:
            if (request->device != (uint8_t)STEPPER)
            {
                return UNKNOWN_DEVICE;
            }

            if (request->length != 5U)
            {
                return INVALID_LENGTH;
            }

            if (request->data[0] >
                (uint8_t)PROTOCOL_DIRECTION_FORWARD)
            {
                return INVALID_VALUE;
            }

            speed = ((uint16_t)request->data[3] << 8) |
                    (uint16_t)request->data[4];
            return (speed != 0U) ? OK : INVALID_VALUE;

        case CMD_STOP:
            if ((request->device != (uint8_t)DC_MOTOR) &&
                (request->device != (uint8_t)STEPPER) &&
                (request->device !=
                 (uint8_t)ALL_LOCAL_ACTUATORS))
            {
                return UNKNOWN_DEVICE;
            }

            return (request->length == 0U) ? OK : INVALID_LENGTH;

        default:
            return UNKNOWN_COMMAND;
    }
}

// Construye una solicitud limpia y devuelve el resultado de validarla.
ProtocolStatus Protocol_CreateRequest(
    ProtocolRequest *request,
    uint8_t command,
    uint8_t device,
    uint8_t length,
    const uint8_t *data)
{
    if (request == NULL)
    {
        return INVALID_VALUE;
    }

    if (length > PROTOCOL_REQUEST_DATA_SIZE)
    {
        return INVALID_LENGTH;
    }

    if ((length > 0U) && (data == NULL))
    {
        return INVALID_VALUE;
    }

    request->command = command;
    request->device = device;
    request->length = length;
    Protocol_Clear(request->data, PROTOCOL_REQUEST_DATA_SIZE);

    if (length > 0U)
    {
        Protocol_Copy(request->data, data, length);
    }

    return Protocol_ValidateRequest(request);
}

// Construye una respuesta limpia con estado y datos limitados a cuatro bytes.
ProtocolStatus Protocol_CreateResponse(
    ProtocolResponse *response,
    uint8_t command,
    uint8_t device,
    ProtocolStatus status,
    uint8_t length,
    const uint8_t *data)
{
    if (response == NULL)
    {
        return INVALID_VALUE;
    }

    if (Protocol_IsKnownStatus((uint8_t)status) == 0U)
    {
        return INVALID_VALUE;
    }

    if (length > PROTOCOL_RESPONSE_DATA_SIZE)
    {
        return INVALID_LENGTH;
    }

    if ((length > 0U) && (data == NULL))
    {
        return INVALID_VALUE;
    }

    response->command = command;
    response->device = device;
    response->status = (uint8_t)status;
    response->length = length;
    Protocol_Clear(response->data, PROTOCOL_RESPONSE_DATA_SIZE);

    if (length > 0U)
    {
        Protocol_Copy(response->data, data, length);
    }

    return OK;
}

// Convierte una solicitud estructurada en ocho bytes para transporte.
void Protocol_EncodeRequest(
    const ProtocolRequest *request,
    uint8_t frame[PROTOCOL_FRAME_SIZE])
{
    uint8_t index;

    if ((request == NULL) || (frame == NULL))
    {
        return;
    }

    frame[0] = request->command;
    frame[1] = request->device;
    frame[2] = request->length;

    for (index = 0U; index < PROTOCOL_REQUEST_DATA_SIZE; index++)
    {
        frame[index + 3U] = request->data[index];
    }
}

// Convierte una respuesta estructurada en ocho bytes para transporte.
void Protocol_EncodeResponse(
    const ProtocolResponse *response,
    uint8_t frame[PROTOCOL_FRAME_SIZE])
{
    uint8_t index;

    if ((response == NULL) || (frame == NULL))
    {
        return;
    }

    frame[0] = response->command;
    frame[1] = response->device;
    frame[2] = response->status;
    frame[3] = response->length;

    for (index = 0U; index < PROTOCOL_RESPONSE_DATA_SIZE; index++)
    {
        frame[index + 4U] = response->data[index];
    }
}

// Separa los ocho bytes recibidos y valida la solicitud obtenida.
ProtocolStatus Protocol_DecodeRequest(
    const uint8_t frame[PROTOCOL_FRAME_SIZE],
    ProtocolRequest *request)
{
    uint8_t index;

    if ((frame == NULL) || (request == NULL))
    {
        return INVALID_VALUE;
    }

    request->command = frame[0];
    request->device = frame[1];
    request->length = frame[2];

    for (index = 0U; index < PROTOCOL_REQUEST_DATA_SIZE; index++)
    {
        request->data[index] = frame[index + 3U];
    }

    return Protocol_ValidateRequest(request);
}

// Separa los ocho bytes recibidos y valida su formato de respuesta.
ProtocolStatus Protocol_DecodeResponse(
    const uint8_t frame[PROTOCOL_FRAME_SIZE],
    ProtocolResponse *response)
{
    uint8_t index;

    if ((frame == NULL) || (response == NULL))
    {
        return INVALID_VALUE;
    }

    response->command = frame[0];
    response->device = frame[1];
    response->status = frame[2];
    response->length = frame[3];

    for (index = 0U; index < PROTOCOL_RESPONSE_DATA_SIZE; index++)
    {
        response->data[index] = frame[index + 4U];
    }

    if (Protocol_IsKnownStatus(response->status) == 0U)
    {
        return INVALID_VALUE;
    }

    if (response->length > PROTOCOL_RESPONSE_DATA_SIZE)
    {
        return INVALID_LENGTH;
    }

    return OK;
}

// Confirma formato, comando y dispositivo contra la solicitud original.
ProtocolStatus Protocol_ValidateResponse(
    const ProtocolRequest *request,
    const ProtocolResponse *response)
{
    uint8_t expected_length;
    uint8_t index;

    if ((request == NULL) || (response == NULL))
    {
        return INVALID_VALUE;
    }

    if ((response->command != request->command) ||
        (response->device != request->device))
    {
        return INVALID_VALUE;
    }

    if (Protocol_IsKnownStatus(response->status) == 0U)
    {
        return INVALID_VALUE;
    }

    if (response->length > PROTOCOL_RESPONSE_DATA_SIZE)
    {
        return INVALID_LENGTH;
    }

    for (index = response->length;
         index < PROTOCOL_RESPONSE_DATA_SIZE;
         index++)
    {
        if (response->data[index] != 0U)
        {
            return INVALID_VALUE;
        }
    }

    switch ((ProtocolStatus)response->status)
    {
        case BUSY:
        case UNKNOWN_COMMAND:
        case UNKNOWN_DEVICE:
        case INVALID_LENGTH:
        case INVALID_VALUE:
        case UNSUPPORTED:
            return (response->length == 0U) ? OK : INVALID_LENGTH;

        case NO_SAMPLE:
            if ((request->command != (uint8_t)CMD_READ_SENSOR) ||
                (response->length != 4U))
            {
                return INVALID_LENGTH;
            }

            return (response->data[0] == 0U) ? OK : INVALID_VALUE;

        case ACCEPTED:
            if ((request->command != (uint8_t)CMD_SERVO_POSITION) &&
                (request->command != (uint8_t)CMD_DC_MOTOR) &&
                (request->command != (uint8_t)CMD_STEPPER_MOVE) &&
                (request->command != (uint8_t)CMD_STOP))
            {
                return INVALID_VALUE;
            }

            return (response->length == 0U) ? OK : INVALID_LENGTH;

        case OK:
            break;

        default:
            return INVALID_VALUE;
    }

    if (request->command == (uint8_t)CMD_READ_SENSOR)
    {
        if (response->length != 4U)
        {
            return INVALID_LENGTH;
        }

        if (response->data[0] != 1U)
        {
            return INVALID_VALUE;
        }

        if ((request->device == (uint8_t)IR) &&
            (response->data[1] > 1U))
        {
            return INVALID_VALUE;
        }

        return OK;
    }

    if (request->command != (uint8_t)CMD_PING)
    {
        return INVALID_VALUE;
    }

    switch ((ProtocolDevice)request->device)
    {
        case SERVO_WATER:
        case SERVO_DOOR:
            expected_length = 1U;
            break;

        case DC_MOTOR:
            expected_length = 3U;
            break;

        case STEPPER:
            expected_length = 4U;
            break;

        default:
            expected_length = 0U;
            break;
    }

    if (response->length != expected_length)
    {
        return INVALID_LENGTH;
    }

    if (((request->device == (uint8_t)SERVO_WATER) ||
         (request->device == (uint8_t)SERVO_DOOR)) &&
        (response->data[0] > 180U))
    {
        return INVALID_VALUE;
    }

    if ((request->device == (uint8_t)DC_MOTOR) &&
        ((response->data[0] >
          (uint8_t)PROTOCOL_DIRECTION_FORWARD) ||
         (response->data[2] > 1U)))
    {
        return INVALID_VALUE;
    }

    if ((request->device == (uint8_t)STEPPER) &&
        ((response->data[0] > 1U) ||
         (response->data[1] >
          (uint8_t)PROTOCOL_DIRECTION_FORWARD)))
    {
        return INVALID_VALUE;
    }

    return OK;
}
