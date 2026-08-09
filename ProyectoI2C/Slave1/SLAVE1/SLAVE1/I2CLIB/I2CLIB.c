/*
 * I2C.c
 *
 * Libreria I2C/TWI para ATmega328P
 * Microchip Studio - C
 */

#include "I2CLIB.h"

#ifndef F_CPU
#error "Debe definir F_CPU globalmente en el proyecto. Ejemplo: F_CPU=16000000UL"
#endif

/******************************************************************************/
// Funcion para inicializar I2C Maestro
/******************************************************************************/
void I2C_Master_Init(unsigned long SCL_Clock, uint8_t Prescaler)
{
    // Pines I2C como entradas:
    // PC4 = SDA
    // PC5 = SCL
    DDRC &= ~((1 << DDC4) | (1 << DDC5));

    // Seleccionamos el valor de los bits del prescaler en TWSR
    switch (Prescaler)
    {
        case 1:
            TWSR &= ~((1 << TWPS1) | (1 << TWPS0));
            break;

        case 4:
            TWSR &= ~(1 << TWPS1);
            TWSR |=  (1 << TWPS0);
            break;

        case 16:
            TWSR &= ~(1 << TWPS0);
            TWSR |=  (1 << TWPS1);
            break;

        case 64:
            TWSR |= ((1 << TWPS1) | (1 << TWPS0));
            break;

        default:
            TWSR &= ~((1 << TWPS1) | (1 << TWPS0));
            Prescaler = 1;
            break;
    }

    // Calcular velocidad del bus I2C
    TWBR = (uint8_t)(((F_CPU / SCL_Clock) - 16UL) / (2UL * Prescaler));

    // Activar interfaz TWI/I2C
    TWCR = (1 << TWEN);
}

/******************************************************************************/
// Funcion de inicio de la comunicacion I2C
/******************************************************************************/
uint8_t I2C_Master_Start(void)
{
    // Enviar condicion START
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);

    // Esperar a que termine la operacion
    while (!(TWCR & (1 << TWINT)));

    // Estado 0x08 = START transmitido
    return ((TWSR & 0xF8) == 0x08);
}

/******************************************************************************/
// Funcion de reinicio de la comunicacion I2C
/******************************************************************************/
uint8_t I2C_Master_RepeatedStart(void)
{
    // Enviar condicion Repeated START
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);

    // Esperar a que termine la operacion
    while (!(TWCR & (1 << TWINT)));

    // Estado 0x10 = Repeated START transmitido
    return ((TWSR & 0xF8) == 0x10);
}

/******************************************************************************/
// Funcion de parada de la comunicacion I2C
/******************************************************************************/
void I2C_Master_Stop(void)
{
    // Enviar condicion STOP
    TWCR = (1 << TWEN) | (1 << TWINT) | (1 << TWSTO);

    // Esperar a que TWSTO vuelva a cero
    while (TWCR & (1 << TWSTO));
}

/******************************************************************************/
// Funcion de transmision de datos del maestro al esclavo
//
// Sirve tanto para:
// - SLA+W
// - SLA+R
// - Datos
//
// Retorna 1 cuando se recibe ACK.
/******************************************************************************/
uint8_t I2C_Master_Write(uint8_t dato)
{
    uint8_t estado;

    // Cargar byte a transmitir
    TWDR = dato;

    // Iniciar transmision
    TWCR = (1 << TWEN) | (1 << TWINT);

    // Esperar a que termine
    while (!(TWCR & (1 << TWINT)));

    // Obtener solamente los bits de estado TWI
    estado = TWSR & 0xF8;

    /*
     * Estados de exito mas comunes:
     *
     * 0x18 = SLA+W transmitido, ACK recibido
     * 0x28 = dato transmitido, ACK recibido
     * 0x40 = SLA+R transmitido, ACK recibido
     */
    if ((estado == 0x18) ||
        (estado == 0x28) ||
        (estado == 0x40))
    {
        return 1;
    }

    return 0;
}

/******************************************************************************/
// Funcion de recepcion de datos enviados por el esclavo al maestro
//
// ack = 1:
//   El maestro envia ACK despues de recibir el byte.
//
// ack = 0:
//   El maestro envia NACK porque es el ultimo byte.
/******************************************************************************/
uint8_t I2C_Master_Read(uint8_t *buffer, uint8_t ack)
{
    uint8_t estado;

    if (buffer == 0)
    {
        return 0;
    }

    if (ack)
    {
        // Recibir byte y responder ACK
        TWCR = (1 << TWINT) |
               (1 << TWEN)  |
               (1 << TWEA);
    }
    else
    {
        // Recibir ultimo byte y responder NACK
        TWCR = (1 << TWINT) |
               (1 << TWEN);
    }

    // Esperar a que termine la recepcion
    while (!(TWCR & (1 << TWINT)));

    estado = TWSR & 0xF8;

    // 0x50 = dato recibido, ACK transmitido
    if (ack && (estado != 0x50))
    {
        return 0;
    }

    // 0x58 = dato recibido, NACK transmitido
    if (!ack && (estado != 0x58))
    {
        return 0;
    }

    // Obtener dato recibido
    *buffer = TWDR;

    return 1;
}

/******************************************************************************/
// Funcion para inicializar I2C Esclavo
/******************************************************************************/
void I2C_Slave_Init(uint8_t address)
{
    // SDA y SCL como entradas
    DDRC &= ~((1 << DDC4) | (1 << DDC5));

    // Direccion I2C de 7 bits
    TWAR = (address << 1);

    // Habilitar TWI, ACK automatico e interrupcion TWI
    TWCR = (1 << TWEA) |
           (1 << TWEN) |
           (1 << TWIE);
}
