/*
 * Proyecto CarWash automatico
 *
 * Author: Miguel Donis 22993 - Ian Farrington 21952
 * Description: Control Master de sensores, actuadores, UART y LCD
 */
/****************************************/
// Encabezado (Libraries)

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <stdint.h>
#include <string.h>

#include "ESPUART/ESPUART.h"
#include "I2CLIB/I2CLIB.h"
#include "LCD/lcd.h"
#include "PROTOCOL/Protocol.h"
#include "TIMEBASE/Timebase.h"
#include "VL53L0X/VL53L0X.h"

#if I2C_FRAME_SIZE != PROTOCOL_FRAME_SIZE
#error "I2CLIB y Protocol deben usar tramas del mismo tamano"
#endif

#define UART_BAUD                       9600UL
#define UART_UBRR                       ((F_CPU / (16UL * UART_BAUD)) - 1UL)
#define UART_TX_BUFFER_SIZE             256U
#define ESP_COMMAND_BUFFER_SIZE          40U

#define SLAVE1_I2C_ADDRESS              0x11U
#define SLAVE2_I2C_ADDRESS              0x12U

#define MODE_BUTTON_DDR                 DDRB
#define MODE_BUTTON_PORT                PORTB
#define MODE_BUTTON_INPUT               PINB
#define MODE_BUTTON_PIN                 PB3
#define MODE_BUTTON_DEBOUNCE_MS          40UL
#define ESP_MODE_TELEMETRY_INTERVAL_MS 1000UL
#define MANUAL_SENSOR_INTERVAL_MS       250UL
#define MANUAL_VL_RETRY_MS             2000UL
#define MANUAL_STEPPER_MAX_SPEED        1000

#define CARWASH_WATER_CLOSED_ANGLE      0U
#define CARWASH_WATER_OPEN_ANGLE        90U
#define CARWASH_DOOR_CLOSED_ANGLE       145U
#define CARWASH_DOOR_OPEN_ANGLE         50U
#define CARWASH_STEPPER_SPEED           800U
#define CARWASH_DC_PWM                  128U

#define CARWASH_IR_INTERVAL_MS          100UL
#define CARWASH_HCSR04_INTERVAL_MS      100UL
#define CARWASH_VL_START_INTERVAL_MS    50UL
#define CARWASH_VL_READY_INTERVAL_MS    5UL
#define CARWASH_HEALTH_INTERVAL_MS      500UL
#define CARWASH_ERROR_INTERVAL_MS       500UL
#define CARWASH_ENTRY_DELAY_MS          3000UL
#define CARWASH_WASH_PHASE_MS           1000UL
#define CARWASH_FINISH_REVERSE_MS       5000UL
#define CARWASH_STARTUP_RETRY_MS         1000UL

#define CARWASH_IR_REQUIRED_SAMPLES     5U
#define CARWASH_RECOVERY_CYCLES         3U
#define CARWASH_VL_MIN_MM               20U
#define CARWASH_VL_MAX_MM               2000U
#define CARWASH_SOAP_MIN_MM             265U
#define CARWASH_SOAP_MAX_MM             270U
#define CARWASH_WASH_MIN_MM             125U
#define CARWASH_WASH_MAX_MM             130U
#define CARWASH_FINISH_MAX_MM           70U

typedef enum
{
    CARWASH_STARTUP_CHECK = 0,
    CARWASH_IDLE,
    CARWASH_ENTRY_WAIT,
    CARWASH_SEEK_SOAP,
    CARWASH_SOAP_REFERENCE,
    CARWASH_SOAP_WATER,
    CARWASH_SEEK_WASH,
    CARWASH_WASH,
    CARWASH_SEEK_FINISH,
    CARWASH_FINISH_REVERSE,
    CARWASH_FINISH_WAIT_CLEAR,
    CARWASH_ERROR_LINKS,
    CARWASH_ERROR_WAIT_CLEAR
} CarWashState;

typedef enum
{
    MASTER_SAMPLE_ERROR = 0,
    MASTER_SAMPLE_NO_SAMPLE,
    MASTER_SAMPLE_VALID
} MasterSampleResult;

typedef enum
{
    MASTER_VL_NO_RESULT = 0,
    MASTER_VL_VALID_RESULT,
    MASTER_VL_ERROR
} MasterVLResult;

typedef struct
{
    uint8_t available;
    uint16_t echo_us;
    uint8_t counter;
} MasterHCSR04Sample;

typedef struct
{
    uint8_t available;
    uint8_t raw_level;
    uint8_t counter;
} MasterInfraredSample;

typedef struct
{
    CarWashState state;
    uint32_t deadline_ms;
    uint32_t next_ir_ms;
    uint32_t next_hcsr04_ms;
    uint32_t next_vl_start_ms;
    uint32_t next_vl_ready_ms;
    uint32_t vl_started_ms;
    uint32_t next_health_ms;
    uint32_t next_error_ms;
    uint32_t next_startup_check_ms;
    uint16_t soap_reference_mm;
    uint16_t last_vl_mm;
    uint8_t last_vl_available;
    uint8_t ir_counter_known;
    uint8_t hcsr04_counter_known;
    uint8_t last_ir_counter;
    uint8_t last_hcsr04_counter;
    uint8_t ir_active_count;
    uint8_t ir_free_count;
    uint8_t wash_phase;
    uint8_t recovery_cycles;
    uint8_t vl_powered;
    uint8_t vl_pending;
} CarWashContext;

typedef struct
{
    uint8_t active;
    uint8_t address;
    uint8_t found;
} MasterI2CScan;

typedef struct
{
    uint8_t last_raw;
    uint8_t stable;
    uint32_t changed_ms;
} ModeButtonContext;

typedef struct
{
    uint32_t next_sensor_ms;
    uint32_t next_vl_retry_ms;
    int16_t dc_value;
    int16_t stepper_value;
    uint8_t water_angle;
    uint8_t door_angle;
    uint8_t sensor_phase;
} ManualModeContext;

static volatile uint8_t uart_tx_buffer[UART_TX_BUFFER_SIZE];
static volatile uint8_t uart_tx_head = 0U;
static volatile uint8_t uart_tx_tail = 0U;

static VL53L0X_t vl53l0x_sensor;
static MasterHCSR04Sample last_hcsr04_sample = {0U, 0U, 0U};
static MasterInfraredSample last_infrared_sample = {0U, 1U, 0U};
static CarWashContext carwash;
static MasterI2CScan i2c_scan = {0U, 0x08U, 0U};
static uint8_t status_report_step = 0U;
static ModeButtonContext mode_button = {1U, 1U, 0UL};
static ManualModeContext manual_context =
{
    0UL,
    0UL,
    0,
    0,
    CARWASH_WATER_CLOSED_ANGLE,
    CARWASH_DOOR_CLOSED_ANGLE,
    0U
};
static uint8_t manual_mode = 0U;
static uint32_t next_mode_telemetry_ms = 0UL;
static char esp_command_buffer[ESP_COMMAND_BUFFER_SIZE];
static uint8_t esp_command_length = 0U;

/****************************************/
// Function prototypes

static void CarWash_EnterError(uint32_t now, PGM_P reason);
static void CarWash_EnterIdle(uint32_t now);
static void CarWash_EnterStartupCheck(uint32_t now);

/****************************************/
// NON-Interrupt subroutines

// Inicializa la comunicacion UART.
static void UART_Init(void)
{
    UBRR0H = (uint8_t)(UART_UBRR >> 8U);
    UBRR0L = (uint8_t)UART_UBRR;
    UCSR0B = (1U << RXEN0) | (1U << TXEN0);
    UCSR0C = (1U << UCSZ01) | (1U << UCSZ00);
}

// Agrega un caracter al buffer UART.
static void UART_WriteChar(char data)
{
    uint8_t next_head;

    do
    {
        next_head = (uint8_t)(uart_tx_head + 1U);
    }
    while (next_head == uart_tx_tail);

    uart_tx_buffer[uart_tx_head] = (uint8_t)data;
    uart_tx_head = next_head;
    UCSR0B |= (1U << UDRIE0);
}

/****************************************/
// Interrupt routines

// Transmite el siguiente byte pendiente por UART.
ISR(USART_UDRE_vect)
{
    if (uart_tx_tail == uart_tx_head)
    {
        UCSR0B &= (uint8_t)~(1U << UDRIE0);
    }
    else
    {
        UDR0 = uart_tx_buffer[uart_tx_tail];
        uart_tx_tail = (uint8_t)(uart_tx_tail + 1U);
    }
}

/****************************************/
// NON-Interrupt subroutines

// Envia por UART un texto almacenado en Flash.
static void UART_WriteFlashString(PGM_P text)
{
    char character = (char)pgm_read_byte(text);

    while (character != '\0')
    {
        UART_WriteChar(character);
        text++;
        character = (char)pgm_read_byte(text);
    }
}

#define UART_WriteString(text) UART_WriteFlashString(PSTR(text))

// Indica si existe un byte recibido por UART.
static uint8_t UART_Available(void)
{
    return (uint8_t)((UCSR0A & (1U << RXC0)) != 0U);
}

// Lee un caracter recibido por UART.
static char UART_ReadChar(void)
{
    return (char)UDR0;
}

// Imprime un entero de 16 bits por UART.
static void UART_WriteUInt16(uint16_t value)
{
    char buffer[5];
    uint8_t index = 0U;

    if (value == 0U)
    {
        UART_WriteChar('0');
        return;
    }

    while (value > 0U)
    {
        buffer[index] = (char)('0' + (value % 10U));
        value /= 10U;
        index++;
    }

    while (index > 0U)
    {
        index--;
        UART_WriteChar(buffer[index]);
    }
}

// Imprime un entero de 16 bits con signo por UART.
static void UART_WriteInt16(int16_t value)
{
    uint16_t magnitude;

    if (value < 0)
    {
        UART_WriteChar('-');
        magnitude = (uint16_t)(-(value + 1)) + 1U;
    }
    else
    {
        magnitude = (uint16_t)value;
    }

    UART_WriteUInt16(magnitude);
}

// Imprime un nibble en hexadecimal.
static void UART_WriteHexNibble(uint8_t value)
{
    value &= 0x0FU;
    UART_WriteChar((value < 10U) ?
                   (char)('0' + value) :
                   (char)('A' + value - 10U));
}

// Imprime un byte en hexadecimal.
static void UART_WriteHexByte(uint8_t value)
{
    UART_WriteString("0x");
    UART_WriteHexNibble((uint8_t)(value >> 4U));
    UART_WriteHexNibble(value);
}

// Muestra los codigos de error del bus I2C.
static void Master_PrintI2CErrorValues(uint8_t error, uint8_t twi_status)
{
    UART_WriteString("ERROR (I2C ");
    UART_WriteUInt16(error);
    UART_WriteString(", TWI ");
    UART_WriteHexByte(twi_status);
    UART_WriteString(")");
}

// Muestra el ultimo error registrado por I2C.
static void Master_PrintCurrentI2CError(void)
{
    Master_PrintI2CErrorValues((uint8_t)I2C_Master_GetLastError(),
                               I2C_Master_GetLastStatus());
}

// Envia una telemetria numerica al ESP32.
static void Master_SendTelemetryInt(PGM_P key, int16_t value)
{
    UART_WriteString("@CW,T,");
    UART_WriteFlashString(key);
    UART_WriteChar(',');
    UART_WriteInt16(value);
    UART_WriteString("\r\n");
}

// Envia una telemetria de texto al ESP32.
static void Master_SendTelemetryText(PGM_P key, PGM_P value)
{
    UART_WriteString("@CW,T,");
    UART_WriteFlashString(key);
    UART_WriteChar(',');
    UART_WriteFlashString(value);
    UART_WriteString("\r\n");
}

// Escribe en LCD un texto almacenado en Flash.
static void LCD_WriteFlashString(PGM_P text)
{
    char character = (char)pgm_read_byte(text);

    while (character != '\0')
    {
        LCD_Write_Char(character);
        text++;
        character = (char)pgm_read_byte(text);
    }
}

// Actualiza las dos lineas de la LCD.
static void Master_SetLCD(PGM_P first_line, PGM_P second_line)
{
    LCD_Clear();
    LCD_Set_Cursor(1U, 1U);
    LCD_WriteFlashString(first_line);
    LCD_Set_Cursor(1U, 2U);
    LCD_WriteFlashString(second_line);
}

// Comprueba si se alcanzo un tiempo programado.
static uint8_t Master_TimeReached(uint32_t now, uint32_t deadline)
{
    return (uint8_t)((int32_t)(now - deadline) >= 0L);
}

// Valida si una muestra posee un contador nuevo.
static uint8_t Master_IsNewCounter(uint8_t counter,
                                   uint8_t *known,
                                   uint8_t *previous)
{
    if ((*known == 0U) || (counter != *previous))
    {
        *known = 1U;
        *previous = counter;
        return 1U;
    }

    return 0U;
}

// Convierte el tiempo de eco a milimetros.
static uint16_t Master_HCSR04EchoToMillimeters(uint16_t echo_us)
{
    return (uint16_t)(((uint32_t)echo_us * 10UL + 29UL) / 58UL);
}

// Intercambia y valida una trama Protocol por I2C.
static uint8_t Master_ExchangeProtocol(uint8_t address,
                                       const ProtocolRequest *request,
                                       ProtocolResponse *response)
{
    uint8_t request_frame[PROTOCOL_FRAME_SIZE];
    uint8_t response_frame[PROTOCOL_FRAME_SIZE];
    uint8_t transport_ok;

    if ((request == 0) || (response == 0))
    {
        return 0U;
    }

    Protocol_EncodeRequest(request, request_frame);

    if (request->command == (uint8_t)CMD_PING)
    {
        transport_ok = I2C_Master_Ping(address,
                                       request_frame,
                                       response_frame);
    }
    else
    {
        transport_ok = I2C_Master_Exchange(address,
                                           request_frame,
                                           response_frame,
                                           I2C_EXCHANGE_DEFERRED);
    }

    if (transport_ok == 0U)
    {
        return 0U;
    }

    if (Protocol_DecodeResponse(response_frame, response) != OK)
    {
        return 0U;
    }

    return (uint8_t)(Protocol_ValidateResponse(request, response) == OK);
}

// Crea y envia una solicitud Protocol.
static uint8_t Master_SendRequest(uint8_t address,
                                  uint8_t command,
                                  uint8_t device,
                                  uint8_t length,
                                  const uint8_t *data,
                                  ProtocolResponse *response)
{
    ProtocolRequest request;

    if (Protocol_CreateRequest(&request,
                               command,
                               device,
                               length,
                               data) != OK)
    {
        return 0U;
    }

    return Master_ExchangeProtocol(address, &request, response);
}

// Verifica un nodo mediante PING completo.
static uint8_t Master_NodePing(uint8_t address)
{
    ProtocolResponse response;

    if (Master_SendRequest(address,
                           (uint8_t)CMD_PING,
                           (uint8_t)ALL_LOCAL_ACTUATORS,
                           0U,
                           0,
                           &response) == 0U)
    {
        return 0U;
    }

    return (uint8_t)(response.status == (uint8_t)OK);
}

// Prueba un nodo y muestra el resultado inicial.
static uint8_t Master_PrintStartupNodeCheck(PGM_P name, uint8_t address)
{
    ProtocolResponse response;
    uint8_t exchanged;

    UART_WriteString("  ");
    UART_WriteFlashString(name);
    UART_WriteString(" ");
    UART_WriteHexByte(address);
    UART_WriteString(": ");

    exchanged = Master_SendRequest(address,
                                   (uint8_t)CMD_PING,
                                   (uint8_t)ALL_LOCAL_ACTUATORS,
                                   0U,
                                   0,
                                   &response);

    if ((exchanged != 0U) && (response.status == (uint8_t)OK))
    {
        UART_WriteString("OK\r\n");
        return 1U;
    }

    if (exchanged != 0U)
    {
        UART_WriteString("ERROR (Protocol estado ");
        UART_WriteUInt16(response.status);
        UART_WriteString(", TWI ");
        UART_WriteHexByte(I2C_Master_GetLastStatus());
        UART_WriteString(")\r\n");
    }
    else
    {
        Master_PrintCurrentI2CError();
        UART_WriteString("\r\n");
    }

    return 0U;
}

// Solicita la muestra cruda del HC-SR04.
static MasterSampleResult Master_GetHCSR04(uint16_t *echo_us,
                                           uint8_t *counter)
{
    ProtocolResponse response;

    if ((echo_us == 0) || (counter == 0))
    {
        return MASTER_SAMPLE_ERROR;
    }

    if (Master_SendRequest(SLAVE1_I2C_ADDRESS,
                           (uint8_t)CMD_READ_SENSOR,
                           (uint8_t)HCSR04,
                           0U,
                           0,
                           &response) == 0U)
    {
        return MASTER_SAMPLE_ERROR;
    }

    *echo_us = ((uint16_t)response.data[1] << 8U) |
               (uint16_t)response.data[2];
    *counter = response.data[3];

    if (response.status == (uint8_t)NO_SAMPLE)
    {
        return MASTER_SAMPLE_NO_SAMPLE;
    }

    if ((response.status != (uint8_t)OK) ||
        (response.data[0] == 0U))
    {
        return MASTER_SAMPLE_ERROR;
    }

    last_hcsr04_sample.available = 1U;
    last_hcsr04_sample.echo_us = *echo_us;
    last_hcsr04_sample.counter = *counter;
    return MASTER_SAMPLE_VALID;
}

// Solicita la muestra cruda del sensor infrarrojo.
static MasterSampleResult Master_GetInfrared(uint8_t *raw_level,
                                             uint8_t *counter)
{
    ProtocolResponse response;

    if ((raw_level == 0) || (counter == 0))
    {
        return MASTER_SAMPLE_ERROR;
    }

    if (Master_SendRequest(SLAVE2_I2C_ADDRESS,
                           (uint8_t)CMD_READ_SENSOR,
                           (uint8_t)IR,
                           0U,
                           0,
                           &response) == 0U)
    {
        return MASTER_SAMPLE_ERROR;
    }

    *raw_level = response.data[1];
    *counter = response.data[3];

    if (response.status == (uint8_t)NO_SAMPLE)
    {
        return MASTER_SAMPLE_NO_SAMPLE;
    }

    if ((response.status != (uint8_t)OK) ||
        (response.data[0] == 0U))
    {
        return MASTER_SAMPLE_ERROR;
    }

    last_infrared_sample.available = 1U;
    last_infrared_sample.raw_level = *raw_level;
    last_infrared_sample.counter = *counter;
    return MASTER_SAMPLE_VALID;
}

// Envia un comando y exige estado ACCEPTED.
static uint8_t Master_CommandAccepted(uint8_t address,
                                      uint8_t command,
                                      uint8_t device,
                                      uint8_t length,
                                      const uint8_t *data)
{
    ProtocolResponse response;

    if (Master_SendRequest(address,
                           command,
                           device,
                           length,
                           data,
                           &response) == 0U)
    {
        return 0U;
    }

    return (uint8_t)(response.status == (uint8_t)ACCEPTED);
}

// Ordena una posicion exacta a un servo.
static uint8_t Master_SetServo(uint8_t address,
                               uint8_t device,
                               uint8_t angle)
{
    return Master_CommandAccepted(address,
                                  (uint8_t)CMD_SERVO_POSITION,
                                  device,
                                  1U,
                                  &angle);
}

// Ordena direccion y PWM al motor DC.
static uint8_t Master_SetDCMotor(uint8_t direction, uint8_t pwm)
{
    const uint8_t data[2] = {direction, pwm};

    return Master_CommandAccepted(SLAVE1_I2C_ADDRESS,
                                  (uint8_t)CMD_DC_MOTOR,
                                  (uint8_t)DC_MOTOR,
                                  2U,
                                  data);
}

// Inicia el stepper continuamente con direccion y velocidad exactas.
static uint8_t Master_StartStepperAtSpeed(uint8_t direction, uint16_t speed)
{
    const uint8_t data[PROTOCOL_REQUEST_DATA_SIZE] =
    {
        direction,
        0U,
        0U,
        (uint8_t)(speed >> 8U),
        (uint8_t)speed
    };

    return Master_CommandAccepted(SLAVE2_I2C_ADDRESS,
                                  (uint8_t)CMD_STEPPER_MOVE,
                                  (uint8_t)STEPPER,
                                  PROTOCOL_REQUEST_DATA_SIZE,
                                  data);
}

// Inicia el stepper con la velocidad usada por el modo automatico.
static uint8_t Master_StartStepper(uint8_t direction)
{
    return Master_StartStepperAtSpeed(direction, CARWASH_STEPPER_SPEED);
}

// Detiene un dispositivo mediante Protocol.
static uint8_t Master_StopDevice(uint8_t address, uint8_t device)
{
    return Master_CommandAccepted(address,
                                  (uint8_t)CMD_STOP,
                                  device,
                                  0U,
                                  0);
}

// Detiene los actuadores de ambos Slaves.
static uint8_t Master_ApplySafeActions(void)
{
    uint8_t slave1_ok;
    uint8_t slave2_ok;
    uint8_t water_ok;
    uint8_t door_ok;

    slave1_ok = Master_StopDevice(SLAVE1_I2C_ADDRESS,
                                  (uint8_t)ALL_LOCAL_ACTUATORS);
    slave2_ok = Master_StopDevice(SLAVE2_I2C_ADDRESS,
                                  (uint8_t)ALL_LOCAL_ACTUATORS);
    water_ok = Master_SetServo(SLAVE1_I2C_ADDRESS,
                               (uint8_t)SERVO_WATER,
                               CARWASH_WATER_CLOSED_ANGLE);
    door_ok = Master_SetServo(SLAVE2_I2C_ADDRESS,
                              (uint8_t)SERVO_DOOR,
                              CARWASH_DOOR_CLOSED_ANGLE);

    return (uint8_t)((slave1_ok != 0U) &&
                     (slave2_ok != 0U) &&
                     (water_ok != 0U) &&
                     (door_ok != 0U));
}

// Apaga el VL53L0X mediante XSHUT.
static void Master_ShutdownVL53L0X(void)
{
    (void)VL53L0X_Shutdown(&vl53l0x_sensor);
    carwash.vl_powered = 0U;
    carwash.vl_pending = 0U;
}

// Enciende e inicializa el VL53L0X.
static uint8_t Master_InitializeVL53L0X(uint32_t now)
{
    if (VL53L0X_Init(&vl53l0x_sensor) != VL53L0X_OK)
    {
        Master_ShutdownVL53L0X();
        return 0U;
    }

    carwash.vl_powered = 1U;
    carwash.vl_pending = 0U;
    carwash.next_vl_start_ms = now;
    carwash.next_vl_ready_ms = now;
    return 1U;
}

// Ejecuta la revision I2C previa al CarWash.
static uint8_t Master_RunStartupI2CCheck(void)
{
    VL53L0X_Status_t vl_status;
    uint8_t slave1_ok;
    uint8_t slave2_ok;
    uint8_t vl_ok;
    uint8_t stop1_ok;
    uint8_t stop2_ok;
    uint8_t water_ok;
    uint8_t door_ok;
    uint8_t stop1_error;
    uint8_t stop1_twi;
    uint8_t stop2_error;
    uint8_t stop2_twi;
    uint8_t water_error;
    uint8_t water_twi;
    uint8_t door_error;
    uint8_t door_twi;

    // Primero exige una respuesta Protocol completa de ambos Slaves.
    UART_WriteString("Revision I2C inicial:\r\n");
    slave1_ok = Master_PrintStartupNodeCheck(PSTR("Slave1"),
                                              SLAVE1_I2C_ADDRESS);
    slave2_ok = Master_PrintStartupNodeCheck(PSTR("Slave2"),
                                              SLAVE2_I2C_ADDRESS);

    // Despierta e inicializa el VL53 para comprobar hardware y registros.
    UART_WriteString("  VL53L0X ");
    UART_WriteHexByte(VL53L0X_DEFAULT_ADDRESS);
    UART_WriteString(": ");
    vl_status = VL53L0X_Init(&vl53l0x_sensor);
    if (vl_status == VL53L0X_OK)
    {
        carwash.vl_powered = 1U;
        carwash.vl_pending = 0U;
        vl_ok = 1U;
        UART_WriteString("OK\r\n");
    }
    else
    {
        vl_ok = 0U;
        UART_WriteString("ERROR (VL ");
        UART_WriteUInt16((uint16_t)vl_status);
        UART_WriteString(", I2C ");
        UART_WriteUInt16((uint16_t)I2C_Master_GetLastError());
        UART_WriteString(", TWI ");
        UART_WriteHexByte(I2C_Master_GetLastStatus());
        UART_WriteString(")\r\n");
    }
    Master_ShutdownVL53L0X();

    // Antes de habilitar el proceso deja todos los actuadores detenidos.
    stop1_ok = Master_StopDevice(SLAVE1_I2C_ADDRESS,
                                 (uint8_t)ALL_LOCAL_ACTUATORS);
    stop1_error = (uint8_t)I2C_Master_GetLastError();
    stop1_twi = I2C_Master_GetLastStatus();
    stop2_ok = Master_StopDevice(SLAVE2_I2C_ADDRESS,
                                 (uint8_t)ALL_LOCAL_ACTUATORS);
    stop2_error = (uint8_t)I2C_Master_GetLastError();
    stop2_twi = I2C_Master_GetLastStatus();
    water_ok = Master_SetServo(SLAVE1_I2C_ADDRESS,
                               (uint8_t)SERVO_WATER,
                               CARWASH_WATER_CLOSED_ANGLE);
    water_error = (uint8_t)I2C_Master_GetLastError();
    water_twi = I2C_Master_GetLastStatus();
    door_ok = Master_SetServo(SLAVE2_I2C_ADDRESS,
                              (uint8_t)SERVO_DOOR,
                              CARWASH_DOOR_CLOSED_ANGLE);
    door_error = (uint8_t)I2C_Master_GetLastError();
    door_twi = I2C_Master_GetLastStatus();

    UART_WriteString("  STOP seguro: ");
    if ((stop1_ok != 0U) &&
        (stop2_ok != 0U) &&
        (water_ok != 0U) &&
        (door_ok != 0U))
    {
        UART_WriteString("OK\r\n");
    }
    else
    {
        UART_WriteString("ERROR\r\n");
        if (stop1_ok == 0U)
        {
            UART_WriteString("    Slave1 STOP: ");
            Master_PrintI2CErrorValues(stop1_error, stop1_twi);
            UART_WriteString("\r\n");
        }
        if (stop2_ok == 0U)
        {
            UART_WriteString("    Slave2 STOP: ");
            Master_PrintI2CErrorValues(stop2_error, stop2_twi);
            UART_WriteString("\r\n");
        }
        if (water_ok == 0U)
        {
            UART_WriteString("    Servo agua: ");
            Master_PrintI2CErrorValues(water_error, water_twi);
            UART_WriteString("\r\n");
        }
        if (door_ok == 0U)
        {
            UART_WriteString("    Servo puerta: ");
            Master_PrintI2CErrorValues(door_error, door_twi);
            UART_WriteString("\r\n");
        }
    }

    return (uint8_t)((slave1_ok != 0U) &&
                      (slave2_ok != 0U) &&
                      (vl_ok != 0U) &&
                      (stop1_ok != 0U) &&
                      (stop2_ok != 0U) &&
                      (water_ok != 0U) &&
                      (door_ok != 0U));
}

// Atiende una medicion no bloqueante del VL53L0X.
static MasterVLResult Master_PollVL53L0X(uint32_t now,
                                         uint16_t *distance_mm)
{
    VL53L0X_Status_t status;
    uint8_t ready;

    if ((distance_mm == 0) || (carwash.vl_powered == 0U))
    {
        return MASTER_VL_ERROR;
    }

    if (carwash.vl_pending == 0U)
    {
        if (Master_TimeReached(now, carwash.next_vl_start_ms) == 0U)
        {
            return MASTER_VL_NO_RESULT;
        }

        // Inicia una medicion y regresa para no bloquear el ciclo principal.
        status = VL53L0X_StartSingle(&vl53l0x_sensor);
        if (status != VL53L0X_OK)
        {
            return MASTER_VL_ERROR;
        }

        carwash.vl_pending = 1U;
        carwash.vl_started_ms = now;
        carwash.next_vl_start_ms = now + CARWASH_VL_START_INTERVAL_MS;
        carwash.next_vl_ready_ms = now + CARWASH_VL_READY_INTERVAL_MS;
        return MASTER_VL_NO_RESULT;
    }

    if (Master_TimeReached(now, carwash.next_vl_ready_ms) == 0U)
    {
        return MASTER_VL_NO_RESULT;
    }

    carwash.next_vl_ready_ms = now + CARWASH_VL_READY_INTERVAL_MS;
    // Consulta periodicamente hasta que el sensor anuncie un dato listo.
    status = VL53L0X_DataReady(&vl53l0x_sensor, &ready);
    if (status != VL53L0X_OK)
    {
        return MASTER_VL_ERROR;
    }

    if (ready != 0U)
    {
        status = VL53L0X_ReadResult(&vl53l0x_sensor, distance_mm);
        carwash.vl_pending = 0U;
        return (status == VL53L0X_OK) ?
               MASTER_VL_VALID_RESULT : MASTER_VL_ERROR;
    }

    if (Master_TimeReached(now,
                           carwash.vl_started_ms +
                           (uint32_t)vl53l0x_sensor.timeout_ms) != 0U)
    {
        status = VL53L0X_ClearInterrupt(&vl53l0x_sensor);
        carwash.vl_pending = 0U;
        carwash.next_vl_start_ms = now + CARWASH_VL_START_INTERVAL_MS;
        if (status == VL53L0X_ERROR_I2C)
        {
            return MASTER_VL_ERROR;
        }
    }

    return MASTER_VL_NO_RESULT;
}

// Imprime el estado actual del CarWash.
static void CarWash_PrintStateName(void)
{
    switch (carwash.state)
    {
        case CARWASH_STARTUP_CHECK:
            UART_WriteString("Revision I2C");
            break;
        case CARWASH_IDLE:
            UART_WriteString("Reposo");
            break;
        case CARWASH_ENTRY_WAIT:
            UART_WriteString("Entrada");
            break;
        case CARWASH_SEEK_SOAP:
            UART_WriteString("Buscando enjabonado");
            break;
        case CARWASH_SOAP_REFERENCE:
            UART_WriteString("Referencia HC-SR04");
            break;
        case CARWASH_SOAP_WATER:
            UART_WriteString("Enjabonado");
            break;
        case CARWASH_SEEK_WASH:
            UART_WriteString("Buscando lavado");
            break;
        case CARWASH_WASH:
            UART_WriteString("Lavado");
            break;
        case CARWASH_SEEK_FINISH:
            UART_WriteString("Buscando salida");
            break;
        case CARWASH_FINISH_REVERSE:
            UART_WriteString("Retorno final");
            break;
        case CARWASH_FINISH_WAIT_CLEAR:
            UART_WriteString("Esperando retiro");
            break;
        case CARWASH_ERROR_LINKS:
            UART_WriteString("Error: recuperando enlaces");
            break;
        case CARWASH_ERROR_WAIT_CLEAR:
            UART_WriteString("Error: esperando IR libre");
            break;
        default:
            UART_WriteString("Desconocido");
            break;
    }
}

// Muestra las ultimas lecturas almacenadas.
static void Master_PrintCachedStatus(void)
{
    UART_WriteString("\r\nModo: ");
    if (manual_mode != 0U)
    {
        UART_WriteString("MANUAL ADAFRUIT");
    }
    else
    {
        UART_WriteString("AUTOMATICO, estado: ");
        CarWash_PrintStateName();
    }
    UART_WriteString("\r\nIR ultima valida: ");
    if (last_infrared_sample.available == 0U)
    {
        UART_WriteString("sin muestra");
    }
    else
    {
        if (last_infrared_sample.raw_level == 0U)
        {
            UART_WriteString("detectado");
        }
        else
        {
            UART_WriteString("libre");
        }
        UART_WriteString(", #");
        UART_WriteUInt16(last_infrared_sample.counter);
    }

    UART_WriteString("\r\nHC ultima valida: ");
    if (last_hcsr04_sample.available == 0U)
    {
        UART_WriteString("sin muestra");
    }
    else
    {
        UART_WriteUInt16(last_hcsr04_sample.echo_us);
        UART_WriteString(" us, ");
        UART_WriteUInt16(Master_HCSR04EchoToMillimeters(
                         last_hcsr04_sample.echo_us));
        UART_WriteString(" mm, #");
        UART_WriteUInt16(last_hcsr04_sample.counter);
    }

    UART_WriteString("\r\nVL53: ");
    if (carwash.vl_powered != 0U)
    {
        UART_WriteString("ON");
    }
    else
    {
        UART_WriteString("XSHUT LOW");
    }
    UART_WriteString(", ultima valida ");
    if (carwash.last_vl_available != 0U)
    {
        UART_WriteUInt16(carwash.last_vl_mm);
        UART_WriteString(" mm");
    }
    else
    {
        UART_WriteString("sin muestra");
    }
    UART_WriteString("\r\nActuadores:\r\n");
}

// Consulta actuadores por pasos sin bloquear el ciclo.
static void Master_PrintActuatorReportStep(void)
{
    ProtocolResponse response;
    uint8_t address;
    uint8_t device;
    uint8_t exchanged;

    if (status_report_step == 0U)
    {
        return;
    }

    if (status_report_step == 1U)
    {
        address = SLAVE1_I2C_ADDRESS;
        device = (uint8_t)SERVO_WATER;
        UART_WriteString("  Agua: ");
    }
    else if (status_report_step == 2U)
    {
        address = SLAVE2_I2C_ADDRESS;
        device = (uint8_t)SERVO_DOOR;
        UART_WriteString("  Puerta: ");
    }
    else if (status_report_step == 3U)
    {
        address = SLAVE1_I2C_ADDRESS;
        device = (uint8_t)DC_MOTOR;
        UART_WriteString("  DC/L298N: ");
    }
    else
    {
        address = SLAVE2_I2C_ADDRESS;
        device = (uint8_t)STEPPER;
        UART_WriteString("  Stepper: ");
    }

    exchanged = Master_SendRequest(address,
                                   (uint8_t)CMD_PING,
                                   device,
                                   0U,
                                   0,
                                   &response);

    if ((exchanged == 0U) || (response.status != (uint8_t)OK))
    {
        UART_WriteString("no disponible\r\n");
    }
    else if ((device == (uint8_t)SERVO_WATER) ||
             (device == (uint8_t)SERVO_DOOR))
    {
        UART_WriteUInt16(response.data[0]);
        UART_WriteString(" deg\r\n");
    }
    else if (device == (uint8_t)DC_MOTOR)
    {
        UART_WriteString("dir ");
        UART_WriteUInt16(response.data[0]);
        UART_WriteString(", pwm ");
        UART_WriteUInt16(response.data[1]);
        UART_WriteString(", ENA ");
        UART_WriteUInt16(response.data[2]);
        UART_WriteString("\r\n");
    }
    else
    {
        UART_WriteString("run ");
        UART_WriteUInt16(response.data[0]);
        UART_WriteString(", dir ");
        UART_WriteUInt16(response.data[1]);
        UART_WriteString(", rem ");
        UART_WriteUInt16(((uint16_t)response.data[2] << 8U) |
                         (uint16_t)response.data[3]);
        UART_WriteString("\r\n");
    }

    if (status_report_step >= 4U)
    {
        status_report_step = 0U;
    }
    else
    {
        status_report_step++;
    }
}

// Inicia un escaneo manual del bus I2C.
static void Master_StartI2CScan(void)
{
    if (i2c_scan.active != 0U)
    {
        UART_WriteString("Escaneo I2C ya activo.\r\n");
        return;
    }

    i2c_scan.active = 1U;
    i2c_scan.address = 0x08U;
    i2c_scan.found = 0U;
    UART_WriteString("Escaneo I2C 0x08-0x77 iniciado.\r\n");
}

// Procesa una direccion del escaneo I2C.
static void Master_ServiceI2CScan(void)
{
    if (i2c_scan.active == 0U)
    {
        return;
    }

    if (I2C_Master_ProbeAddress(i2c_scan.address) != 0U)
    {
        UART_WriteString("  encontrado ");
        UART_WriteHexByte(i2c_scan.address);
        UART_WriteString("\r\n");
        i2c_scan.found = 1U;
    }

    if (i2c_scan.address >= 0x77U)
    {
        if (i2c_scan.found == 0U)
        {
            UART_WriteString("  ninguna direccion respondio\r\n");
        }
        if (carwash.vl_powered == 0U)
        {
            UART_WriteString("  0x29 esta apagado por XSHUT.\r\n");
        }
        UART_WriteString("Escaneo terminado.\r\n");
        i2c_scan.active = 0U;
    }
    else
    {
        i2c_scan.address++;
    }
}

// Configura D11 como boton con resistencia pull-up interna.
static void Master_ModeButtonInit(uint32_t now)
{
    uint8_t raw;

    MODE_BUTTON_DDR &= (uint8_t)~(1U << MODE_BUTTON_PIN);
    MODE_BUTTON_PORT |= (1U << MODE_BUTTON_PIN);
    raw = (uint8_t)((MODE_BUTTON_INPUT & (1U << MODE_BUTTON_PIN)) != 0U);
    mode_button.last_raw = raw;
    mode_button.stable = raw;
    mode_button.changed_ms = now;
}

// Convierte un texto decimal completo a entero con signo.
static uint8_t Master_ParseInt16(const char *text, int16_t *value)
{
    uint32_t magnitude = 0UL;
    uint8_t negative = 0U;

    if ((text == 0) || (value == 0) || (*text == '\0'))
    {
        return 0U;
    }

    if (*text == '-')
    {
        negative = 1U;
        text++;
    }
    else if (*text == '+')
    {
        text++;
    }

    if (*text == '\0')
    {
        return 0U;
    }

    while (*text != '\0')
    {
        if ((*text < '0') || (*text > '9'))
        {
            return 0U;
        }

        magnitude = (magnitude * 10UL) + (uint8_t)(*text - '0');
        if (magnitude > 32768UL)
        {
            return 0U;
        }
        text++;
    }

    if (negative != 0U)
    {
        if (magnitude == 32768UL)
        {
            *value = (int16_t)(-32767 - 1);
        }
        else
        {
            *value = (int16_t)-(int16_t)magnitude;
        }
    }
    else
    {
        if (magnitude > 32767UL)
        {
            return 0U;
        }
        *value = (int16_t)magnitude;
    }

    return 1U;
}

// Publica las posiciones seguras de los actuadores manuales.
static void Master_SendManualActuatorTelemetry(void)
{
    Master_SendTelemetryInt(PSTR("DC"), manual_context.dc_value);
    Master_SendTelemetryInt(PSTR("WA"), manual_context.water_angle);
    Master_SendTelemetryInt(PSTR("DO"), manual_context.door_angle);
    Master_SendTelemetryInt(PSTR("ST"), manual_context.stepper_value);
}

// Detiene todos los actuadores y actualiza su telemetria.
static uint8_t Master_StopManualActuators(void)
{
    if (Master_ApplySafeActions() == 0U)
    {
        return 0U;
    }

    manual_context.dc_value = 0;
    manual_context.stepper_value = 0;
    manual_context.water_angle = CARWASH_WATER_CLOSED_ANGLE;
    manual_context.door_angle = CARWASH_DOOR_CLOSED_ANGLE;
    Master_SendManualActuatorTelemetry();
    return 1U;
}

// Entra al control manual y deja una base segura.
static void Master_EnterManualMode(uint32_t now)
{
    uint8_t safe_ok;

    manual_mode = 1U;
    Master_ShutdownVL53L0X();
    safe_ok = Master_StopManualActuators();

    manual_context.next_sensor_ms = now;
    manual_context.next_vl_retry_ms = now;
    manual_context.sensor_phase = 0U;
    carwash.last_vl_available = 0U;

    Master_SetLCD(PSTR("Modo manual"), PSTR("Adafruit IO"));
    Master_SendTelemetryInt(PSTR("MODE"), 1);
    Master_SendTelemetryText(PSTR("LCD"), PSTR("Modo manual"));
    UART_WriteString("Modo MANUAL Adafruit habilitado.\r\n");

    if (safe_ok == 0U)
    {
        UART_WriteString("Manual: no se confirmo la parada segura.\r\n");
    }
}

// Sale del modo manual y reinicia la revision automatica.
static void Master_EnterAutomaticMode(uint32_t now)
{
    (void)Master_StopManualActuators();
    Master_ShutdownVL53L0X();
    manual_mode = 0U;
    Master_SendTelemetryInt(PSTR("MODE"), 0);
    UART_WriteString("Modo AUTOMATICO: reiniciando revision I2C.\r\n");
    CarWash_EnterStartupCheck(now);
}

// Aplica antirrebote y alterna el modo al presionar D11.
static void Master_ServiceModeButton(uint32_t now)
{
    uint8_t raw = (uint8_t)((MODE_BUTTON_INPUT &
                             (1U << MODE_BUTTON_PIN)) != 0U);

    if (raw != mode_button.last_raw)
    {
        mode_button.last_raw = raw;
        mode_button.changed_ms = now;
        return;
    }

    if ((raw != mode_button.stable) &&
        (Master_TimeReached(now,
                            mode_button.changed_ms +
                            MODE_BUTTON_DEBOUNCE_MS) != 0U))
    {
        mode_button.stable = raw;
        if (raw == 0U)
        {
            if (manual_mode == 0U)
            {
                Master_EnterManualMode(now);
            }
            else
            {
                Master_EnterAutomaticMode(now);
            }
        }
    }
}

// Procesa una orden @CW,C,DISPOSITIVO,VALOR del ESP32.
static void Master_ProcessESPCommand(char *line)
{
    char *device;
    char *separator;
    int16_t value;
    uint8_t accepted = 0U;

    if ((line == 0) || (strncmp(line, "@CW,C,", 6U) != 0))
    {
        return;
    }

    device = &line[6];
    separator = strchr(device, ',');
    if (separator == 0)
    {
        return;
    }

    *separator = '\0';
    if (Master_ParseInt16(separator + 1, &value) == 0U)
    {
        UART_WriteString("Manual: valor recibido invalido.\r\n");
        return;
    }

    if (manual_mode == 0U)
    {
        UART_WriteString("Orden Adafruit ignorada en modo automatico.\r\n");
        return;
    }

    if ((strcmp(device, "DC") == 0) &&
        (value >= -255) && (value <= 255))
    {
        if (value == 0)
        {
            accepted = Master_StopDevice(SLAVE1_I2C_ADDRESS,
                                         (uint8_t)DC_MOTOR);
        }
        else
        {
            uint8_t direction = (value > 0) ?
                (uint8_t)PROTOCOL_DIRECTION_FORWARD :
                (uint8_t)PROTOCOL_DIRECTION_REVERSE;
            uint8_t pwm = (uint8_t)((value > 0) ? value : -value);

            accepted = Master_SetDCMotor(direction, pwm);
        }

        if (accepted != 0U)
        {
            manual_context.dc_value = value;
            Master_SendTelemetryInt(PSTR("DC"), value);
        }
    }
    else if ((strcmp(device, "WA") == 0) &&
             (value >= 0) && (value <= 180))
    {
        accepted = Master_SetServo(SLAVE1_I2C_ADDRESS,
                                   (uint8_t)SERVO_WATER,
                                   (uint8_t)value);
        if (accepted != 0U)
        {
            manual_context.water_angle = (uint8_t)value;
            Master_SendTelemetryInt(PSTR("WA"), value);
        }
    }
    else if ((strcmp(device, "DO") == 0) &&
             (value >= 0) && (value <= 180))
    {
        accepted = Master_SetServo(SLAVE2_I2C_ADDRESS,
                                   (uint8_t)SERVO_DOOR,
                                   (uint8_t)value);
        if (accepted != 0U)
        {
            manual_context.door_angle = (uint8_t)value;
            Master_SendTelemetryInt(PSTR("DO"), value);
        }
    }
    else if ((strcmp(device, "ST") == 0) &&
             (value >= -MANUAL_STEPPER_MAX_SPEED) &&
             (value <= MANUAL_STEPPER_MAX_SPEED))
    {
        if (value == 0)
        {
            accepted = Master_StopDevice(SLAVE2_I2C_ADDRESS,
                                         (uint8_t)STEPPER);
        }
        else
        {
            uint8_t direction = (value > 0) ?
                (uint8_t)PROTOCOL_DIRECTION_FORWARD :
                (uint8_t)PROTOCOL_DIRECTION_REVERSE;
            uint16_t speed = (uint16_t)((value > 0) ? value : -value);

            accepted = Master_StartStepperAtSpeed(direction, speed);
        }

        if (accepted != 0U)
        {
            manual_context.stepper_value = value;
            Master_SendTelemetryInt(PSTR("ST"), value);
        }
    }
    else if ((strcmp(device, "STOP") == 0) && (value == 0))
    {
        accepted = Master_StopManualActuators();
        if (accepted != 0U)
        {
            Master_SendTelemetryText(PSTR("LCD"), PSTR("Detenido"));
        }
    }
    else
    {
        UART_WriteString("Manual: dispositivo o rango invalido.\r\n");
        return;
    }

    if (accepted == 0U)
    {
        UART_WriteString("Manual: orden rechazada por el Slave.\r\n");
        Master_SendTelemetryText(PSTR("LCD"), PSTR("Error comando"));
    }
}

// Forma lineas completas recibidas por la UART de D12.
static void Master_ServiceESPCommands(void)
{
    if (ESPUART_GetAndClearOverflow() != 0U)
    {
        esp_command_length = 0U;
        UART_WriteString("ESP UART: buffer desbordado.\r\n");
    }

    while (ESPUART_Available() != 0U)
    {
        char data = ESPUART_ReadChar();

        if (data == '\r')
        {
            continue;
        }

        if (data == '\n')
        {
            esp_command_buffer[esp_command_length] = '\0';
            if (esp_command_length > 0U)
            {
                Master_ProcessESPCommand(esp_command_buffer);
            }
            esp_command_length = 0U;
        }
        else if (esp_command_length < (ESP_COMMAND_BUFFER_SIZE - 1U))
        {
            esp_command_buffer[esp_command_length] = data;
            esp_command_length++;
        }
        else
        {
            esp_command_length = 0U;
        }
    }
}

// Mantiene actualizados los sensores visibles en Adafruit durante manual.
static void Master_ServiceManualSensors(uint32_t now)
{
    MasterSampleResult sample_result;
    MasterVLResult vl_result;
    uint16_t sensor_value;
    uint8_t counter;

    if (Master_TimeReached(now, manual_context.next_sensor_ms) != 0U)
    {
        manual_context.next_sensor_ms = now + MANUAL_SENSOR_INTERVAL_MS;

        if (manual_context.sensor_phase == 0U)
        {
            sample_result = Master_GetHCSR04(&sensor_value, &counter);
            if (sample_result == MASTER_SAMPLE_VALID)
            {
                Master_SendTelemetryInt(
                    PSTR("HCR"),
                    (int16_t)Master_HCSR04EchoToMillimeters(sensor_value));
            }
            else if (sample_result == MASTER_SAMPLE_NO_SAMPLE)
            {
                Master_SendTelemetryInt(PSTR("HCR"), -1);
            }
        }
        else
        {
            uint8_t raw_level;

            sample_result = Master_GetInfrared(&raw_level, &counter);
            if (sample_result == MASTER_SAMPLE_VALID)
            {
                Master_SendTelemetryInt(PSTR("IR"), raw_level);
            }
            else if (sample_result == MASTER_SAMPLE_NO_SAMPLE)
            {
                Master_SendTelemetryInt(PSTR("IR"), -1);
            }
        }

        manual_context.sensor_phase ^= 1U;
    }

    if (carwash.vl_powered == 0U)
    {
        if (Master_TimeReached(now, manual_context.next_vl_retry_ms) != 0U)
        {
            if (Master_InitializeVL53L0X(now) == 0U)
            {
                manual_context.next_vl_retry_ms = now + MANUAL_VL_RETRY_MS;
                Master_SendTelemetryInt(PSTR("VL"), -1);
            }
        }
        return;
    }

    vl_result = Master_PollVL53L0X(now, &sensor_value);
    if (vl_result == MASTER_VL_VALID_RESULT)
    {
        if ((sensor_value >= CARWASH_VL_MIN_MM) &&
            (sensor_value <= CARWASH_VL_MAX_MM))
        {
            carwash.last_vl_mm = sensor_value;
            carwash.last_vl_available = 1U;
            Master_SendTelemetryInt(PSTR("VL"), (int16_t)sensor_value);
        }
    }
    else if (vl_result == MASTER_VL_ERROR)
    {
        Master_ShutdownVL53L0X();
        manual_context.next_vl_retry_ms = now + MANUAL_VL_RETRY_MS;
        Master_SendTelemetryInt(PSTR("VL"), -1);
    }
}

// Repite el modo para que el ESP32 pueda recuperarlo tras reiniciarse.
static void Master_ServiceModeTelemetry(uint32_t now)
{
    if (Master_TimeReached(now, next_mode_telemetry_ms) != 0U)
    {
        next_mode_telemetry_ms = now + ESP_MODE_TELEMETRY_INTERVAL_MS;
        Master_SendTelemetryInt(PSTR("MODE"), manual_mode);
    }
}

// Registra una transicion de estado por UART.
static void CarWash_LogTransition(PGM_P state_name)
{
    UART_WriteString("CarWash -> ");
    UART_WriteFlashString(state_name);
    UART_WriteString("\r\n");
}

// Entra a la revision inicial de conexiones.
static void CarWash_EnterStartupCheck(uint32_t now)
{
    carwash.state = CARWASH_STARTUP_CHECK;
    carwash.next_startup_check_ms = now;
    Master_ShutdownVL53L0X();
    Master_SetLCD(PSTR("Revisando I2C"), PSTR("Espere..."));
}

// Reintenta la revision inicial cuando falla.
static void CarWash_ServiceStartupCheck(uint32_t now)
{
    if (Master_TimeReached(now, carwash.next_startup_check_ms) == 0U)
    {
        return;
    }

    Master_SetLCD(PSTR("Revisando I2C"), PSTR("Espere..."));
    if (Master_RunStartupI2CCheck() != 0U)
    {
        UART_WriteString("Conexion I2C: EXITOSA\r\n");
        UART_WriteString("CarWash automatico habilitado.\r\n");
        CarWash_EnterIdle(Timebase_Millis());
        return;
    }

    Master_ShutdownVL53L0X();
    Master_SetLCD(PSTR("Error I2C"), PSTR("Reintentando"));
    UART_WriteString("Conexion I2C: ERROR\r\n");
    UART_WriteString("Reintentando en 1000 ms...\r\n");
    carwash.next_startup_check_ms = Timebase_Millis() +
                                    CARWASH_STARTUP_RETRY_MS;
}

// Lleva el sistema al estado seguro de error.
static void CarWash_EnterError(uint32_t now, PGM_P reason)
{
    UART_WriteString("ERROR: ");
    UART_WriteFlashString(reason);
    UART_WriteString("\r\n");

    carwash.state = CARWASH_ERROR_LINKS;
    carwash.recovery_cycles = 0U;
    carwash.ir_free_count = 0U;
    carwash.ir_counter_known = 0U;
    Master_ShutdownVL53L0X();
    (void)Master_ApplySafeActions();
    Master_SetLCD(PSTR("Error I2C"), PSTR("Esperando..."));
    carwash.next_error_ms = now;
    carwash.next_health_ms = now + CARWASH_HEALTH_INTERVAL_MS;
}

// Prepara el sistema para esperar un vehiculo.
static void CarWash_EnterIdle(uint32_t now)
{
    carwash.state = CARWASH_IDLE;
    carwash.ir_active_count = 0U;
    carwash.ir_free_count = 0U;
    carwash.ir_counter_known = 0U;
    carwash.hcsr04_counter_known = 0U;
    carwash.soap_reference_mm = 0U;
    carwash.wash_phase = 0U;
    Master_ShutdownVL53L0X();

    if (Master_ApplySafeActions() == 0U)
    {
        CarWash_EnterError(Timebase_Millis(),
                           PSTR("no se pudo asegurar el reposo"));
        return;
    }

    now = Timebase_Millis();
    carwash.next_ir_ms = now;
    carwash.next_health_ms = now + CARWASH_HEALTH_INTERVAL_MS;
    Master_SetLCD(PSTR("Bienvenido al"), PSTR("CarWash"));
    CarWash_LogTransition(PSTR("Reposo"));
}

// Abre la puerta e inicia la etapa de entrada.
static void CarWash_StartEntry(uint32_t now)
{
    if (Master_SetServo(SLAVE2_I2C_ADDRESS,
                        (uint8_t)SERVO_DOOR,
                        CARWASH_DOOR_OPEN_ANGLE) == 0U)
    {
        CarWash_EnterError(Timebase_Millis(),
                           PSTR("puerta no acepto 50 grados"));
        return;
    }

    carwash.state = CARWASH_ENTRY_WAIT;
    carwash.deadline_ms = Timebase_Millis() + CARWASH_ENTRY_DELAY_MS;
    carwash.ir_active_count = 0U;
    Master_SetLCD(PSTR("Ingresando"), PSTR("tu auto"));
    CarWash_LogTransition(PSTR("Entrada"));
    (void)now;
}

// Enciende el sensor e inicia el transporte.
static void CarWash_FinishEntry(void)
{
    uint32_t now = Timebase_Millis();

    if (Master_InitializeVL53L0X(now) == 0U)
    {
        CarWash_EnterError(Timebase_Millis(),
                           PSTR("fallo al inicializar VL53L0X"));
        return;
    }

    if (Master_StartStepper((uint8_t)PROTOCOL_DIRECTION_FORWARD) == 0U)
    {
        CarWash_EnterError(Timebase_Millis(),
                           PSTR("stepper no acepto avance"));
        return;
    }

    carwash.state = CARWASH_SEEK_SOAP;
    carwash.next_vl_start_ms = Timebase_Millis();
    CarWash_LogTransition(PSTR("Buscando enjabonado"));
}

// Detiene el vehiculo e inicia el enjabonado.
static void CarWash_EnterSoap(void)
{
    uint8_t stepper_ok;
    uint8_t door_ok;
    uint8_t water_ok;

    stepper_ok = Master_StopDevice(SLAVE2_I2C_ADDRESS,
                                   (uint8_t)STEPPER);
    door_ok = Master_SetServo(SLAVE2_I2C_ADDRESS,
                              (uint8_t)SERVO_DOOR,
                              CARWASH_DOOR_CLOSED_ANGLE);
    water_ok = Master_SetServo(SLAVE1_I2C_ADDRESS,
                               (uint8_t)SERVO_WATER,
                               CARWASH_WATER_CLOSED_ANGLE);

    if ((stepper_ok == 0U) || (door_ok == 0U) || (water_ok == 0U))
    {
        CarWash_EnterError(Timebase_Millis(),
                           PSTR("fallo al preparar enjabonado"));
        return;
    }

    carwash.state = CARWASH_SOAP_REFERENCE;
    carwash.soap_reference_mm = 0U;
    carwash.hcsr04_counter_known = 0U;
    carwash.vl_pending = 0U;
    carwash.next_hcsr04_ms = Timebase_Millis();
    Master_SetLCD(PSTR("Enjabonando"), PSTR("tu auto"));
    CarWash_LogTransition(PSTR("Enjabonado"));
}

// Cierra el agua y termina el enjabonado.
static void CarWash_CompleteSoap(void)
{
    if (Master_SetServo(SLAVE1_I2C_ADDRESS,
                        (uint8_t)SERVO_WATER,
                        CARWASH_WATER_CLOSED_ANGLE) == 0U)
    {
        CarWash_EnterError(Timebase_Millis(),
                           PSTR("agua no cerro"));
        return;
    }

    if (Master_StartStepper((uint8_t)PROTOCOL_DIRECTION_FORWARD) == 0U)
    {
        CarWash_EnterError(Timebase_Millis(),
                           PSTR("stepper no reinicio"));
        return;
    }

    carwash.state = CARWASH_SEEK_WASH;
    carwash.next_vl_start_ms = Timebase_Millis();
    carwash.vl_pending = 0U;
    CarWash_LogTransition(PSTR("Buscando lavado"));
}

// Inicia el motor DC hacia adelante con velocidad media fija.
static void CarWash_EnterWash(void)
{
    if (Master_StopDevice(SLAVE2_I2C_ADDRESS,
                          (uint8_t)STEPPER) == 0U)
    {
        CarWash_EnterError(Timebase_Millis(),
                           PSTR("stepper no se detuvo"));
        return;
    }

    if (Master_SetDCMotor((uint8_t)PROTOCOL_DIRECTION_FORWARD,
                          CARWASH_DC_PWM) == 0U)
    {
        CarWash_EnterError(Timebase_Millis(),
                           PSTR("motor DC no acepto orden"));
        return;
    }

    carwash.state = CARWASH_WASH;
    carwash.wash_phase = 0U;
    carwash.vl_pending = 0U;
    carwash.deadline_ms = Timebase_Millis() + CARWASH_WASH_PHASE_MS;
    Master_SetLCD(PSTR("Lavando"), PSTR("tu auto"));
    CarWash_LogTransition(PSTR("Lavado"));
}

// Detiene el motor y termina el lavado.
static void CarWash_CompleteWash(void)
{
    VL53L0X_Status_t vl_status;
    uint32_t now;

    if (Master_StopDevice(SLAVE1_I2C_ADDRESS,
                          (uint8_t)DC_MOTOR) == 0U)
    {
        CarWash_EnterError(Timebase_Millis(),
                           PSTR("motor DC no se detuvo"));
        return;
    }

    // Reinicia el ciclo de medicion antes de buscar la salida.
    vl_status = VL53L0X_ClearInterrupt(&vl53l0x_sensor);
    if (vl_status != VL53L0X_OK)
    {
        CarWash_EnterError(Timebase_Millis(),
                           PSTR("VL53 no reinicio tras lavado"));
        return;
    }

    if (Master_StartStepper((uint8_t)PROTOCOL_DIRECTION_FORWARD) == 0U)
    {
        CarWash_EnterError(Timebase_Millis(),
                           PSTR("stepper no reinicio"));
        return;
    }

    vl_status = VL53L0X_StartSingle(&vl53l0x_sensor);
    if (vl_status != VL53L0X_OK)
    {
        CarWash_EnterError(Timebase_Millis(),
                           PSTR("VL53 no inicio tras lavado"));
        return;
    }

    now = Timebase_Millis();
    carwash.state = CARWASH_SEEK_FINISH;
    carwash.vl_pending = 1U;
    carwash.vl_started_ms = now;
    carwash.next_vl_start_ms = now + CARWASH_VL_START_INTERVAL_MS;
    carwash.next_vl_ready_ms = now + CARWASH_VL_READY_INTERVAL_MS;
}

// Asegura actuadores y devuelve el vehiculo durante cinco segundos.
static void CarWash_EnterFinal(void)
{
    if (Master_ApplySafeActions() == 0U)
    {
        CarWash_EnterError(Timebase_Millis(),
                           PSTR("fallo en parada final"));
        return;
    }

    Master_ShutdownVL53L0X();
    if (Master_StartStepper((uint8_t)PROTOCOL_DIRECTION_REVERSE) == 0U)
    {
        CarWash_EnterError(Timebase_Millis(),
                           PSTR("stepper no acepto retorno"));
        return;
    }

    carwash.state = CARWASH_FINISH_REVERSE;
    carwash.deadline_ms = Timebase_Millis() +
                          CARWASH_FINISH_REVERSE_MS;
    Master_SetLCD(PSTR("Gracias por"), PSTR("elegirnos"));
    CarWash_LogTransition(PSTR("Retorno final 5 s"));
}

// Comprueba periodicamente ambos enlaces I2C.
static uint8_t CarWash_CheckNormalHealth(uint32_t now)
{
    uint8_t slave1_ok;
    uint8_t slave2_ok;

    if (Master_TimeReached(now, carwash.next_health_ms) == 0U)
    {
        return 1U;
    }

    carwash.next_health_ms = now + CARWASH_HEALTH_INTERVAL_MS;
    slave1_ok = Master_NodePing(SLAVE1_I2C_ADDRESS);
    slave2_ok = Master_NodePing(SLAVE2_I2C_ADDRESS);

    if ((slave1_ok == 0U) || (slave2_ok == 0U))
    {
        CarWash_EnterError(Timebase_Millis(),
                           PSTR("se perdio un Slave"));
        return 0U;
    }

    return 1U;
}

// Cuenta detecciones consecutivas en reposo.
static void CarWash_ServiceIdleIR(uint32_t now)
{
    MasterSampleResult result;
    uint8_t raw_level;
    uint8_t counter;

    if (Master_TimeReached(now, carwash.next_ir_ms) == 0U)
    {
        return;
    }
    carwash.next_ir_ms = now + CARWASH_IR_INTERVAL_MS;

    result = Master_GetInfrared(&raw_level, &counter);
    if (result == MASTER_SAMPLE_ERROR)
    {
        CarWash_EnterError(Timebase_Millis(),
                           PSTR("lectura IR invalida"));
        return;
    }

    if (Master_IsNewCounter(counter,
                            &carwash.ir_counter_known,
                            &carwash.last_ir_counter) == 0U)
    {
        return;
    }

    // El IR es activo en bajo; una muestra libre reinicia el conteo.
    if ((result == MASTER_SAMPLE_VALID) && (raw_level == 0U))
    {
        if (carwash.ir_active_count < CARWASH_IR_REQUIRED_SAMPLES)
        {
            carwash.ir_active_count++;
        }
    }
    else
    {
        carwash.ir_active_count = 0U;
    }

    if (carwash.ir_active_count >= CARWASH_IR_REQUIRED_SAMPLES)
    {
        CarWash_StartEntry(Timebase_Millis());
    }
}

// Controla el agua segun el HC-SR04.
static void CarWash_ServiceHCSR04(uint32_t now)
{
    MasterSampleResult result;
    uint16_t echo_us;
    uint16_t distance_mm;
    uint8_t counter;

    if (Master_TimeReached(now, carwash.next_hcsr04_ms) == 0U)
    {
        return;
    }
    carwash.next_hcsr04_ms = now + CARWASH_HCSR04_INTERVAL_MS;

    result = Master_GetHCSR04(&echo_us, &counter);
    if (result == MASTER_SAMPLE_ERROR)
    {
        CarWash_EnterError(Timebase_Millis(),
                           PSTR("respuesta HC-SR04 invalida"));
        return;
    }

    if (Master_IsNewCounter(counter,
                            &carwash.hcsr04_counter_known,
                            &carwash.last_hcsr04_counter) == 0U)
    {
        return;
    }

    // Una medicion perdida obliga a cerrar agua y buscar nueva referencia.
    if (result == MASTER_SAMPLE_NO_SAMPLE)
    {
        if (carwash.state == CARWASH_SOAP_WATER)
        {
            if (Master_SetServo(SLAVE1_I2C_ADDRESS,
                                (uint8_t)SERVO_WATER,
                                CARWASH_WATER_CLOSED_ANGLE) == 0U)
            {
                CarWash_EnterError(Timebase_Millis(),
                                   PSTR("agua no cerro tras NO_SAMPLE"));
                return;
            }
            carwash.state = CARWASH_SOAP_REFERENCE;
            carwash.soap_reference_mm = 0U;
        }
        return;
    }

    distance_mm = Master_HCSR04EchoToMillimeters(echo_us);
    // La primera distancia valida se guarda antes de abrir el servo de agua.
    if (carwash.state == CARWASH_SOAP_REFERENCE)
    {
        carwash.soap_reference_mm = distance_mm;
        if (Master_SetServo(SLAVE1_I2C_ADDRESS,
                            (uint8_t)SERVO_WATER,
                            CARWASH_WATER_OPEN_ANGLE) == 0U)
        {
            CarWash_EnterError(Timebase_Millis(),
                               PSTR("agua no abrio"));
            return;
        }
        carwash.state = CARWASH_SOAP_WATER;
    }
    // Un aumento de 20 mm cierra el agua y reinicia el transporte.
    else if ((uint32_t)distance_mm >=
             ((uint32_t)carwash.soap_reference_mm + 20UL))
    {
        CarWash_CompleteSoap();
    }
}

// Evalua la distancia VL53 para cambiar de etapa.
static void CarWash_ServiceVL(uint32_t now)
{
    MasterVLResult result;
    uint16_t distance_mm;

    result = Master_PollVL53L0X(now, &distance_mm);
    if (result == MASTER_VL_ERROR)
    {
        CarWash_EnterError(Timebase_Millis(),
                           PSTR("error I2C del VL53L0X"));
        return;
    }

    if (result != MASTER_VL_VALID_RESULT)
    {
        return;
    }

    // Mantiene visible la distancia durante todas las busquedas.
    UART_WriteString("VL53L0X: ");
    UART_WriteUInt16(distance_mm);
    UART_WriteString(" mm\r\n");

    // Los valores fisicamente no validos se muestran, pero no cambian estado.
    if ((distance_mm < CARWASH_VL_MIN_MM) ||
        (distance_mm > CARWASH_VL_MAX_MM))
    {
        return;
    }

    carwash.last_vl_available = 1U;
    carwash.last_vl_mm = distance_mm;

    // Solo se evalua la ventana correspondiente a la etapa actual.
    if (carwash.state == CARWASH_SEEK_SOAP)
    {
        if ((distance_mm >= CARWASH_SOAP_MIN_MM) &&
            (distance_mm <= CARWASH_SOAP_MAX_MM))
        {
            CarWash_EnterSoap();
        }
    }
    else if (carwash.state == CARWASH_SEEK_WASH)
    {
        if ((distance_mm >= CARWASH_WASH_MIN_MM) &&
            (distance_mm <= CARWASH_WASH_MAX_MM))
        {
            CarWash_EnterWash();
        }
    }
    else if ((carwash.state == CARWASH_SEEK_FINISH) &&
             (distance_mm <= CARWASH_FINISH_MAX_MM))
    {
        UART_WriteString("FIN\r\n");
        CarWash_EnterFinal();
    }
}

// Mantiene el motor a velocidad fija durante cinco segundos.
static void CarWash_ServiceWash(uint32_t now)
{
    if (Master_TimeReached(now, carwash.deadline_ms) == 0U)
    {
        return;
    }

    // Cada fase dura un segundo; cinco fases completan el lavado.
    carwash.wash_phase++;
    if (carwash.wash_phase >= 5U)
    {
        CarWash_CompleteWash();
        return;
    }

    carwash.deadline_ms = Timebase_Millis() + CARWASH_WASH_PHASE_MS;
}

// Espera la salida del vehiculo al finalizar.
static void CarWash_ServiceFinishWait(uint32_t now)
{
    MasterSampleResult result;
    uint8_t raw_level;
    uint8_t counter;

    if (Master_TimeReached(now, carwash.next_ir_ms) == 0U)
    {
        return;
    }
    carwash.next_ir_ms = now + CARWASH_IR_INTERVAL_MS;

    result = Master_GetInfrared(&raw_level, &counter);
    if (result == MASTER_SAMPLE_ERROR)
    {
        CarWash_EnterError(Timebase_Millis(),
                           PSTR("se perdio IR al finalizar"));
        return;
    }

    if (Master_IsNewCounter(counter,
                            &carwash.ir_counter_known,
                            &carwash.last_ir_counter) == 0U)
    {
        return;
    }

    if ((result == MASTER_SAMPLE_VALID) && (raw_level != 0U))
    {
        if (carwash.ir_free_count < CARWASH_IR_REQUIRED_SAMPLES)
        {
            carwash.ir_free_count++;
        }
    }
    else
    {
        carwash.ir_free_count = 0U;
    }

    if (carwash.ir_free_count >= CARWASH_IR_REQUIRED_SAMPLES)
    {
        CarWash_EnterIdle(Timebase_Millis());
    }
}

// Comprueba la recuperacion de todos los enlaces.
static void CarWash_ServiceErrorLinks(uint32_t now)
{
    uint8_t slave1_ok;
    uint8_t slave2_ok;

    if (Master_TimeReached(now, carwash.next_error_ms) == 0U)
    {
        return;
    }
    carwash.next_error_ms = now + CARWASH_ERROR_INTERVAL_MS;

    // Mantiene la parada mientras comprueba tres ciclos de PING completos.
    (void)Master_ApplySafeActions();
    slave1_ok = Master_NodePing(SLAVE1_I2C_ADDRESS);
    slave2_ok = Master_NodePing(SLAVE2_I2C_ADDRESS);

    if ((slave1_ok == 0U) || (slave2_ok == 0U))
    {
        carwash.recovery_cycles = 0U;
        return;
    }

    carwash.recovery_cycles++;
    if (carwash.recovery_cycles < CARWASH_RECOVERY_CYCLES)
    {
        return;
    }

    // Tambien exige que el VL53 pueda inicializarse antes de recuperarse.
    if (Master_InitializeVL53L0X(Timebase_Millis()) == 0U)
    {
        carwash.recovery_cycles = 0U;
        UART_WriteString("VL53L0X aun no disponible.\r\n");
        return;
    }

    Master_ShutdownVL53L0X();
    if (Master_ApplySafeActions() == 0U)
    {
        carwash.recovery_cycles = 0U;
        return;
    }

    carwash.state = CARWASH_ERROR_WAIT_CLEAR;
    carwash.ir_free_count = 0U;
    carwash.ir_counter_known = 0U;
    carwash.next_ir_ms = Timebase_Millis();
    carwash.next_health_ms = Timebase_Millis() +
                             CARWASH_HEALTH_INTERVAL_MS;
    UART_WriteString("Enlaces recuperados; esperando IR libre.\r\n");
}

// Espera que el IR quede libre tras un error.
static void CarWash_ServiceErrorWaitClear(uint32_t now)
{
    uint8_t slave1_ok;
    uint8_t slave2_ok;

    if (Master_TimeReached(now, carwash.next_health_ms) != 0U)
    {
        carwash.next_health_ms = now + CARWASH_HEALTH_INTERVAL_MS;
        slave1_ok = Master_NodePing(SLAVE1_I2C_ADDRESS);
        slave2_ok = Master_NodePing(SLAVE2_I2C_ADDRESS);
        if ((slave1_ok == 0U) || (slave2_ok == 0U))
        {
            CarWash_EnterError(Timebase_Millis(),
                               PSTR("enlace perdido durante recuperacion"));
            return;
        }
    }

    CarWash_ServiceFinishWait(now);
}

// Ejecuta el servicio correspondiente al estado actual.
static void CarWash_Service(uint32_t now)
{
    if ((carwash.state != CARWASH_STARTUP_CHECK) &&
        (carwash.state != CARWASH_ERROR_LINKS) &&
        (carwash.state != CARWASH_ERROR_WAIT_CLEAR))
    {
        if (CarWash_CheckNormalHealth(now) == 0U)
        {
            return;
        }
    }

    // Cada estado ejecuta solo una tarea corta y devuelve el control a main.
    switch (carwash.state)
    {
        case CARWASH_STARTUP_CHECK:
            CarWash_ServiceStartupCheck(now);
            break;

        case CARWASH_IDLE:
            CarWash_ServiceIdleIR(now);
            break;

        case CARWASH_ENTRY_WAIT:
            if (Master_TimeReached(now, carwash.deadline_ms) != 0U)
            {
                CarWash_FinishEntry();
            }
            break;

        case CARWASH_SEEK_SOAP:
        case CARWASH_SEEK_WASH:
        case CARWASH_SEEK_FINISH:
            CarWash_ServiceVL(now);
            break;

        case CARWASH_SOAP_REFERENCE:
        case CARWASH_SOAP_WATER:
            CarWash_ServiceHCSR04(now);
            break;

        case CARWASH_WASH:
            CarWash_ServiceWash(now);
            break;

        case CARWASH_FINISH_REVERSE:
            if (Master_TimeReached(now, carwash.deadline_ms) != 0U)
            {
                if (Master_StopDevice(SLAVE2_I2C_ADDRESS,
                                      (uint8_t)STEPPER) == 0U)
                {
                    CarWash_EnterError(Timebase_Millis(),
                                       PSTR("stepper no detuvo retorno"));
                    return;
                }

                carwash.state = CARWASH_FINISH_WAIT_CLEAR;
                carwash.ir_free_count = 0U;
                carwash.ir_counter_known = 0U;
                carwash.next_ir_ms = now;
                CarWash_LogTransition(PSTR("Esperando retiro"));
            }
            break;

        case CARWASH_FINISH_WAIT_CLEAR:
            CarWash_ServiceFinishWait(now);
            break;

        case CARWASH_ERROR_LINKS:
            CarWash_ServiceErrorLinks(now);
            break;

        case CARWASH_ERROR_WAIT_CLEAR:
            CarWash_ServiceErrorWaitClear(now);
            break;

        default:
            CarWash_EnterError(Timebase_Millis(),
                               PSTR("estado interno desconocido"));
            break;
    }
}

// Atiende los comandos de diagnostico UART.
static void Master_ServiceUART(void)
{
    char command;

    if (UART_Available() == 0U)
    {
        return;
    }

    command = UART_ReadChar();
    if ((command == '\r') || (command == '\n'))
    {
        return;
    }

    if ((command == 'p') || (command == 'P'))
    {
        Master_PrintCachedStatus();
        status_report_step = 1U;
    }
    else if ((command == 's') || (command == 'S'))
    {
        Master_StartI2CScan();
    }
    else
    {
        UART_WriteString("Use p=estado, s=escaneo o boton D11 para modo.\r\n");
    }
}

// Ejecuta diagnosticos pendientes sin bloquear.
static void Master_ServiceDiagnostics(void)
{
    if (status_report_step != 0U)
    {
        Master_PrintActuatorReportStep();
    }
    else
    {
        Master_ServiceI2CScan();
    }
}

/****************************************/
// Main Function

// Inicializa perifericos y ejecuta el ciclo principal.
int main(void)
{
    uint8_t reset_cause = MCUSR;
    uint32_t now;

    MCUSR = 0U;

    // Configura interfaces, sensor ToF apagado y base de tiempo.
    UART_Init();
    initLCD8bits();
    I2C_Master_Init(100000UL, 1U);
    VL53L0X_ObjectInit(&vl53l0x_sensor);
    (void)VL53L0X_AttachXSHUT(&vl53l0x_sensor,
                              &DDRB,
                              &PORTB,
                              PB2);
    (void)VL53L0X_Shutdown(&vl53l0x_sensor);
    Timebase_Init();
    ESPUART_Init();
    Master_ModeButtonInit(Timebase_Millis());
    sei();

    UART_WriteString("MASTER12C listo.\r\n");
    UART_WriteString("Reset MCUSR: ");
    UART_WriteHexByte(reset_cause);
    UART_WriteString("\r\n");
    UART_WriteString("p: estado  s: escaneo  D11: automatico/manual\r\n");
    UART_WriteString("ESP RX comandos: D12 a 9600 baudios.\r\n");
    Master_SendTelemetryInt(PSTR("MODE"), 0);
    CarWash_EnterStartupCheck(Timebase_Millis());

    // Atiende continuamente modo, ESP, control, UART y diagnosticos.
    while (1)
    {
        now = Timebase_Millis();
        Master_ServiceModeButton(now);
        Master_ServiceESPCommands();
        Master_ServiceModeTelemetry(now);

        if (manual_mode != 0U)
        {
            Master_ServiceManualSensors(now);
        }
        else
        {
            CarWash_Service(now);
        }

        Master_ServiceUART();
        Master_ServiceDiagnostics();
    }
}
