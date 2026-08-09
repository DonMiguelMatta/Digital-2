/*
 * main_example_terminal.c
 *
 * ATmega328P + VL53L0X + Microchip Studio
 *
 * Envia la distancia medida por UART a la terminal serial.
 *
 * Configuracion:
 *      F_CPU = 16 MHz
 *      UART  = 9600 baudios
 *      I2C   = 100 kHz
 *
 * ATmega328P:
 *      SDA = PC4
 *      SCL = PC5
 *      TX  = PD1 / TXD
 */

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

#include "I2CLIB/I2CLIB.h"
#include "LCD/lcd.h"
#include "VL53L0X/VL53L0X.h"


/******************************************************************************
 * UART
 ******************************************************************************/

#define UART_BAUD       9600UL
#define UART_UBRR       ((F_CPU / (16UL * UART_BAUD)) - 1UL)


void UART_Init(void)
{
    /* Configurar velocidad */
    UBRR0H = (uint8_t)(UART_UBRR >> 8);
    UBRR0L = (uint8_t)UART_UBRR;

    /* Habilitar recepcion y transmision */
    UCSR0B = (1 << RXEN0) | (1 << TXEN0);

    /* 8 bits, 1 stop bit, sin paridad */
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}


void UART_WriteChar(char data)
{
    /* Esperar hasta que el buffer de transmision este libre */
    while (!(UCSR0A & (1 << UDRE0)));

    UDR0 = data;
}


void UART_WriteString(const char *text)
{
    while (*text != '\0')
    {
        UART_WriteChar(*text);
        text++;
    }
}


uint8_t UART_Available(void)
{
    return ((UCSR0A & (1 << RXC0)) != 0U) ? 1U : 0U;
}


char UART_ReadChar(void)
{
    return (char)UDR0;
}


char UART_ReadCharBlocking(void)
{
    while (!UART_Available())
    {
    }

    return UART_ReadChar();
}


/*
 * Envia un uint16_t sin utilizar sprintf().
 * Esto reduce el uso de memoria en el ATmega328P.
 */
void UART_WriteUInt16(uint16_t value)
{
    char buffer[6];
    uint8_t i = 0;

    if (value == 0)
    {
        UART_WriteChar('0');
        return;
    }

    while ((value > 0) && (i < sizeof(buffer)))
    {
        buffer[i] = (char)('0' + (value % 10));
        value /= 10;
        i++;
    }

    while (i > 0)
    {
        i--;
        UART_WriteChar(buffer[i]);
    }
}


void UART_WriteHexNibble(uint8_t value)
{
    value &= 0x0FU;

    if (value < 10U)
    {
        UART_WriteChar((char)('0' + value));
    }
    else
    {
        UART_WriteChar((char)('A' + (value - 10U)));
    }
}


void UART_WriteHexByte(uint8_t value)
{
    UART_WriteString("0x");
    UART_WriteHexNibble((uint8_t)(value >> 4));
    UART_WriteHexNibble(value);
}


/******************************************************************************
 * SLAVE 1
 ******************************************************************************/

#define SLAVE1_I2C_ADDRESS          0x11U
#define SLAVE1_RESPONSE_BYTES       4U
#define SLAVE1_STATUS_HCSR04_OK     0x01U
#define SLAVE1_HCSR04_INVALID_US    0xFFFFU
#define SLAVE1_CMD_OPEN_SERVO       'o'
#define SLAVE1_CMD_CLOSE_SERVO      'c'

#define SLAVE2_I2C_ADDRESS          0x12U
#define SLAVE2_RESPONSE_BYTES       4U
#define SLAVE2_STATUS_IR_OK         0x01U
#define SLAVE2_STATUS_STEPPER_OK    0x02U
#define SLAVE2_STEPPER_REVERSE      0U
#define SLAVE2_STEPPER_FORWARD      1U
#define SLAVE2_CMD_STEPPER_FORWARD  'f'
#define SLAVE2_CMD_STEPPER_REVERSE  'b'
#define SLAVE2_CMD_STEPPER_STOP     'x'


static uint8_t Master_I2C_WriteByte(
    uint8_t address,
    uint8_t data)
{
    if (!I2C_Master_Start())
    {
        return 0U;
    }

    if (!I2C_Master_Write((uint8_t)(address << 1)))
    {
        I2C_Master_Stop();
        return 0U;
    }

    if (!I2C_Master_Write(data))
    {
        I2C_Master_Stop();
        return 0U;
    }

    I2C_Master_Stop();
    return 1U;
}


static uint8_t Master_I2C_Ping(uint8_t address)
{
    uint8_t ok = 0U;

    if (I2C_Master_Start())
    {
        ok = I2C_Master_Write((uint8_t)(address << 1));
        I2C_Master_Stop();
    }

    return ok;
}


static uint8_t Master_I2C_ReadBytes(
    uint8_t address,
    uint8_t *data,
    uint8_t count)
{
    uint8_t i;

    if ((data == 0) || (count == 0U))
    {
        return 0U;
    }

    if (!I2C_Master_Start())
    {
        return 0U;
    }

    if (!I2C_Master_Write((uint8_t)((address << 1) | 0x01U)))
    {
        I2C_Master_Stop();
        return 0U;
    }

    for (i = 0U; i < count; i++)
    {
        uint8_t ack = (i < (uint8_t)(count - 1U)) ? 1U : 0U;

        if (!I2C_Master_Read(&data[i], ack))
        {
            I2C_Master_Stop();
            return 0U;
        }
    }

    I2C_Master_Stop();
    return 1U;
}


static uint8_t Master_SendSlave1Command(uint8_t command)
{
    return Master_I2C_WriteByte(SLAVE1_I2C_ADDRESS, command);
}


static uint8_t Master_SendSlave2Command(uint8_t command)
{
    return Master_I2C_WriteByte(SLAVE2_I2C_ADDRESS, command);
}


static uint8_t Master_ReadSlave1(
    uint8_t *status,
    uint16_t *echo_us,
    uint8_t *servo_angle)
{
    uint8_t data[SLAVE1_RESPONSE_BYTES];

    if ((status == 0) || (echo_us == 0) || (servo_angle == 0))
    {
        return 0U;
    }

    if (!Master_I2C_ReadBytes(
            SLAVE1_I2C_ADDRESS,
            data,
            SLAVE1_RESPONSE_BYTES))
    {
        return 0U;
    }

    *status = data[0];
    *echo_us = ((uint16_t)data[2] << 8) | (uint16_t)data[1];
    *servo_angle = data[3];

    return 1U;
}


static uint8_t Master_ReadSlave2(
    uint8_t *status,
    uint8_t *infrared_level,
    uint8_t *stepper_running,
    uint8_t *stepper_direction)
{
    uint8_t data[SLAVE2_RESPONSE_BYTES];

    if ((status == 0) || (infrared_level == 0) ||
        (stepper_running == 0) || (stepper_direction == 0))
    {
        return 0U;
    }

    if (!Master_I2C_ReadBytes(
            SLAVE2_I2C_ADDRESS,
            data,
            SLAVE2_RESPONSE_BYTES))
    {
        return 0U;
    }

    *status = data[0];
    *infrared_level = data[1];
    *stepper_running = data[2];
    *stepper_direction = data[3];

    return 1U;
}


static uint16_t Master_HCSR04_EchoToCm(uint16_t echo_us)
{
    return (uint16_t)((uint32_t)echo_us / 58UL);
}


static void Master_PrintSlave1Reading(
    uint8_t status,
    uint16_t echo_us,
    uint8_t servo_angle)
{
    UART_WriteString("Slave1 HC-SR04: ");

    if (((status & SLAVE1_STATUS_HCSR04_OK) != 0U) &&
        (echo_us != SLAVE1_HCSR04_INVALID_US))
    {
        UART_WriteUInt16(Master_HCSR04_EchoToCm(echo_us));
        UART_WriteString(" cm (echo ");
        UART_WriteUInt16(echo_us);
        UART_WriteString(" us)");
    }
    else
    {
        UART_WriteString("sin eco");
    }

    UART_WriteString(", servo ");
    UART_WriteUInt16((uint16_t)servo_angle);
    UART_WriteString(" deg\r\n");
}


static void Master_PrintSlave2Reading(
    uint8_t status,
    uint8_t infrared_level,
    uint8_t stepper_running,
    uint8_t stepper_direction)
{
    UART_WriteString("Slave2 IR: ");

    if ((status & SLAVE2_STATUS_IR_OK) != 0U)
    {
        if (infrared_level == 0U)
        {
            UART_WriteString("objeto detectado");
        }
        else
        {
            UART_WriteString("libre");
        }

        UART_WriteString(" (nivel ");
        UART_WriteUInt16((uint16_t)infrared_level);
        UART_WriteString(")");
    }
    else
    {
        UART_WriteString("no inicializado");
    }

    UART_WriteString(", stepper: ");

    if ((status & SLAVE2_STATUS_STEPPER_OK) == 0U)
    {
        UART_WriteString("no inicializado");
    }
    else if (stepper_running == 0U)
    {
        UART_WriteString("detenido");
    }
    else if (stepper_direction == SLAVE2_STEPPER_FORWARD)
    {
        UART_WriteString("adelante");
    }
    else
    {
        UART_WriteString("atras");
    }

    UART_WriteString("\r\n");
}


static void Master_PrintI2CPing(
    const char *name,
    uint8_t address)
{
    UART_WriteString(name);
    UART_WriteString(" ");
    UART_WriteHexByte(address);
    UART_WriteString(": ");

    if (Master_I2C_Ping(address))
    {
        UART_WriteString("OK");
    }
    else
    {
        UART_WriteString("sin respuesta");
    }

    UART_WriteString(" (TWI ");
    UART_WriteHexByte(I2C_Master_GetLastStatus());
    UART_WriteString(")\r\n");
}


static void Master_ScanI2CBus(void)
{
    uint8_t address;
    uint8_t found = 0U;

    UART_WriteString("Escaneo I2C 0x08-0x77:\r\n");

    for (address = 0x08U; address <= 0x77U; address++)
    {
        if (Master_I2C_Ping(address))
        {
            UART_WriteString("  encontrado ");
            UART_WriteHexByte(address);
            UART_WriteString(" (TWI ");
            UART_WriteHexByte(I2C_Master_GetLastStatus());
            UART_WriteString(")\r\n");
            found = 1U;
        }
    }

    if (found == 0U)
    {
        UART_WriteString("  ninguna direccion respondio\r\n");
    }
}


static void Master_PrintSlaveStatus(void)
{
    UART_WriteString("Confirmando dispositivos por I2C...\r\n");
    Master_PrintI2CPing("Slave1", SLAVE1_I2C_ADDRESS);
    Master_PrintI2CPing("Slave2", SLAVE2_I2C_ADDRESS);
    Master_PrintI2CPing("VL53L0X", VL53L0X_DEFAULT_ADDRESS);
}


static void Master_PrintMenu(void)
{
    UART_WriteString("\r\nQue deseas hacer?\r\n");
    UART_WriteString("h: leer HC-SR04 en Slave1\r\n");
    UART_WriteString("v: leer VL53L0X\r\n");
    UART_WriteString("o: abrir servo a 90 deg\r\n");
    UART_WriteString("c: cerrar servo a 0 deg\r\n");
    UART_WriteString("i: leer sensor IR en Slave2\r\n");
    UART_WriteString("f: mover stepper adelante\r\n");
    UART_WriteString("b: mover stepper atras\r\n");
    UART_WriteString("x: detener stepper\r\n");
    UART_WriteString("s: escanear bus I2C\r\n");
    UART_WriteString("p: confirmar slaves por I2C\r\n");
    UART_WriteString("> ");
}


static uint8_t Master_IsOpenCommand(char command)
{
    return ((command == SLAVE1_CMD_OPEN_SERVO) ||
            (command == 'O')) ? 1U : 0U;
}


static uint8_t Master_IsCloseCommand(char command)
{
    return ((command == SLAVE1_CMD_CLOSE_SERVO) ||
            (command == 'C')) ? 1U : 0U;
}


/******************************************************************************
 * MAIN
 ******************************************************************************/

int main(void)
{
    VL53L0X_t tof;

    uint16_t distancia_mm = 0;
    VL53L0X_Status_t estado;
    uint16_t slave1_echo_us = SLAVE1_HCSR04_INVALID_US;
    uint16_t slave1_last_echo_us = SLAVE1_HCSR04_INVALID_US;
    uint8_t slave1_status = 0U;
    uint8_t slave1_servo_angle = 0U;
    uint8_t slave1_last_status = 0U;
    uint8_t slave1_last_servo_angle = 0U;
    uint8_t slave1_has_last_response = 0U;
    uint8_t slave2_status = 0U;
    uint8_t slave2_infrared_level = 1U;
    uint8_t slave2_stepper_running = 0U;
    uint8_t slave2_stepper_direction = SLAVE2_STEPPER_FORWARD;
    uint8_t slave2_last_status = 0U;
    uint8_t slave2_last_infrared_level = 1U;
    uint8_t slave2_last_stepper_running = 0U;
    uint8_t slave2_last_stepper_direction = SLAVE2_STEPPER_FORWARD;
    uint8_t slave2_has_last_response = 0U;
    uint8_t vl53_initialized = 0U;
    char command;


    /**************************************************************************
     * Inicializar UART
     **************************************************************************/
    UART_Init();

    UART_WriteString("\r\n");
    UART_WriteString("MASTER12C listo.\r\n");

    initLCD8bits();
    LCD_Set_Cursor(1U, 1U);
    LCD_Write_String("conectado");


    /**************************************************************************
     * Inicializar I2C Master
     *
     * SCL = 100 kHz
     * Prescaler = 1
     **************************************************************************/
    I2C_Master_Init(100000UL, 1U);


    /**************************************************************************
     * Crear objeto VL53L0X
     **************************************************************************/
    VL53L0X_ObjectInit(&tof);


    /**************************************************************************
     * XSHUT OPCIONAL
     *
     * Si tienes XSHUT conectado, descomenta estas lineas.
     *
     * Ejemplo:
     *      XSHUT -> PD6
     **************************************************************************/

    /*
    VL53L0X_AttachXSHUT(
        &tof,
        &DDRD,
        &PORTD,
        PD6);
    */


    /**************************************************************************
     * GPIO1 OPCIONAL
     *
     * Para esta prueba NO es necesario.
     * VL53L0X_ReadDistance() puede comprobar el estado mediante I2C.
     *
     * Si deseas utilizar GPIO1 en PD2:
     **************************************************************************/

    /*
    VL53L0X_AttachGPIO1(
        &tof,
        &DDRD,
        &PORTD,
        &PIND,
        PD2);
    */


    Master_PrintSlaveStatus();
    Master_ScanI2CBus();
    UART_WriteString("-------------------------\r\n");


    /**************************************************************************
     * Loop principal
     **************************************************************************/
    while (1)
    {
        Master_PrintMenu();
        command = UART_ReadCharBlocking();
        UART_WriteChar(command);
        UART_WriteString("\r\n");

        if ((command == 'h') || (command == 'H'))
        {
            if (Master_ReadSlave1(
                    &slave1_status,
                    &slave1_echo_us,
                    &slave1_servo_angle))
            {
                slave1_last_status = slave1_status;
                slave1_last_echo_us = slave1_echo_us;
                slave1_last_servo_angle = slave1_servo_angle;
                slave1_has_last_response = 1U;

                Master_PrintSlave1Reading(
                    slave1_status,
                    slave1_echo_us,
                    slave1_servo_angle);
            }
            else
            {
                UART_WriteString("Slave1: sin respuesta I2C\r\n");

                if (slave1_has_last_response != 0U)
                {
                    UART_WriteString("Ultima respuesta valida -> ");
                    Master_PrintSlave1Reading(
                        slave1_last_status,
                        slave1_last_echo_us,
                        slave1_last_servo_angle);
                }
            }
        }
        else if ((command == 'i') || (command == 'I'))
        {
            if (Master_ReadSlave2(
                    &slave2_status,
                    &slave2_infrared_level,
                    &slave2_stepper_running,
                    &slave2_stepper_direction))
            {
                slave2_last_status = slave2_status;
                slave2_last_infrared_level = slave2_infrared_level;
                slave2_last_stepper_running = slave2_stepper_running;
                slave2_last_stepper_direction = slave2_stepper_direction;
                slave2_has_last_response = 1U;

                Master_PrintSlave2Reading(
                    slave2_status,
                    slave2_infrared_level,
                    slave2_stepper_running,
                    slave2_stepper_direction);
            }
            else
            {
                UART_WriteString("Slave2: sin respuesta I2C\r\n");

                if (slave2_has_last_response != 0U)
                {
                    UART_WriteString("Ultima respuesta valida -> ");
                    Master_PrintSlave2Reading(
                        slave2_last_status,
                        slave2_last_infrared_level,
                        slave2_last_stepper_running,
                        slave2_last_stepper_direction);
                }
            }
        }
        else if ((command == 'v') || (command == 'V'))
        {
            if (vl53_initialized == 0U)
            {
                UART_WriteString("Inicializando VL53L0X...\r\n");
                VL53L0X_ObjectInit(&tof);
                estado = VL53L0X_Init(&tof);

                if (estado == VL53L0X_OK)
                {
                    vl53_initialized = 1U;
                    UART_WriteString("VL53L0X listo.\r\n");
                }
                else
                {
                    UART_WriteString("ERROR: no se pudo inicializar el VL53L0X. Codigo: ");
                    UART_WriteUInt16((uint16_t)estado);
                    UART_WriteString("\r\n");
                }
            }

            if (vl53_initialized != 0U)
            {
                estado = VL53L0X_ReadDistance(
                    &tof,
                    &distancia_mm);

                if (estado == VL53L0X_OK)
                {
                    UART_WriteString("VL53L0X: ");
                    UART_WriteUInt16(distancia_mm);
                    UART_WriteString(" mm\r\n");
                }
                else
                {
                    UART_WriteString("ERROR de lectura VL53L0X. Codigo: ");
                    UART_WriteUInt16((uint16_t)estado);
                    UART_WriteString("\r\n");
                }
            }
        }
        else if (Master_IsOpenCommand(command))
        {
            if (Master_SendSlave1Command((uint8_t)SLAVE1_CMD_OPEN_SERVO))
            {
                UART_WriteString("Comando Slave1: abrir servo a 90 deg\r\n");
            }
            else
            {
                UART_WriteString("ERROR I2C enviando abrir servo\r\n");
            }
        }
        else if (Master_IsCloseCommand(command))
        {
            if (Master_SendSlave1Command((uint8_t)SLAVE1_CMD_CLOSE_SERVO))
            {
                UART_WriteString("Comando Slave1: cerrar servo a 0 deg\r\n");
            }
            else
            {
                UART_WriteString("ERROR I2C enviando cerrar servo\r\n");
            }
        }
        else if ((command == SLAVE2_CMD_STEPPER_FORWARD) ||
                 (command == 'F'))
        {
            if (Master_SendSlave2Command(
                    (uint8_t)SLAVE2_CMD_STEPPER_FORWARD))
            {
                UART_WriteString("Comando Slave2: stepper adelante\r\n");
            }
            else
            {
                UART_WriteString("ERROR I2C enviando stepper adelante\r\n");
            }
        }
        else if ((command == SLAVE2_CMD_STEPPER_REVERSE) ||
                 (command == 'B'))
        {
            if (Master_SendSlave2Command(
                    (uint8_t)SLAVE2_CMD_STEPPER_REVERSE))
            {
                UART_WriteString("Comando Slave2: stepper atras\r\n");
            }
            else
            {
                UART_WriteString("ERROR I2C enviando stepper atras\r\n");
            }
        }
        else if ((command == SLAVE2_CMD_STEPPER_STOP) ||
                 (command == 'X'))
        {
            if (Master_SendSlave2Command(
                    (uint8_t)SLAVE2_CMD_STEPPER_STOP))
            {
                UART_WriteString("Comando Slave2: detener stepper\r\n");
            }
            else
            {
                UART_WriteString("ERROR I2C deteniendo stepper\r\n");
            }
        }
        else if ((command == 'p') || (command == 'P'))
        {
            Master_PrintSlaveStatus();
        }
        else if ((command == 's') || (command == 'S'))
        {
            Master_PrintSlaveStatus();
            Master_ScanI2CBus();
        }
        else
        {
            UART_WriteString("Opcion no valida.\r\n");
        }
    }
}
