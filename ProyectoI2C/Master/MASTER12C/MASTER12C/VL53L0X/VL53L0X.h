/*
 * VL53L0X.h
 *
 * Libreria sencilla para VL53L0X
 * ATmega328P - Microchip Studio - C
 *
 * NO requiere STSW-IMG005 ni Arduino.
 * Depende unicamente de I2C.h / I2C.c.
 *
 * La inicializacion compacta sigue la secuencia de configuracion
 * comunmente utilizada para VL53L0X, derivada de la API de ST.
 */

#ifndef VL53L0X_H_
#define VL53L0X_H_

#include <avr/io.h>
#include <stdint.h>

/* Direccion I2C de fabrica, formato 7 bits */
#define VL53L0X_DEFAULT_ADDRESS      0x29U

/* ID esperado del VL53L0X */
#define VL53L0X_EXPECTED_MODEL_ID    0xEEU

/* Timeout por defecto */
#define VL53L0X_DEFAULT_TIMEOUT_MS   200U


typedef enum
{
    VL53L0X_OK = 0,
    VL53L0X_ERROR_I2C,
    VL53L0X_ERROR_ID,
    VL53L0X_ERROR_TIMEOUT,
    VL53L0X_ERROR_PARAMETER,
    VL53L0X_ERROR_NOT_INITIALIZED

} VL53L0X_Status_t;


/*
 * Estructura generica para asociar un pin del ATmega328P.
 * Permite utilizar PORTB, PORTC o PORTD sin modificar la libreria.
 */
typedef struct
{
    volatile uint8_t *ddr;
    volatile uint8_t *port;
    volatile uint8_t *pin;

    uint8_t mask;
    uint8_t enabled;

} VL53L0X_Pin_t;


/*
 * Objeto principal del sensor.
 */
typedef struct
{
    uint8_t address;
    uint8_t stop_variable;
    uint8_t initialized;

    uint16_t timeout_ms;

    VL53L0X_Pin_t xshut;
    VL53L0X_Pin_t gpio1;

} VL53L0X_t;


/******************************************************************************
 * Inicializa solamente la estructura en RAM.
 * Todavia no comunica con el sensor.
 ******************************************************************************/
void VL53L0X_ObjectInit(VL53L0X_t *sensor);


/******************************************************************************
 * Asociar pin XSHUT.
 *
 * Ejemplo PD6:
 *
 * VL53L0X_AttachXSHUT(&tof, &DDRD, &PORTD, PD6);
 ******************************************************************************/
VL53L0X_Status_t VL53L0X_AttachXSHUT(
    VL53L0X_t *sensor,
    volatile uint8_t *ddr,
    volatile uint8_t *port,
    uint8_t bit);


/******************************************************************************
 * Asociar pin GPIO1.
 *
 * Ejemplo PD2:
 *
 * VL53L0X_AttachGPIO1(&tof, &DDRD, &PORTD, &PIND, PD2);
 ******************************************************************************/
VL53L0X_Status_t VL53L0X_AttachGPIO1(
    VL53L0X_t *sensor,
    volatile uint8_t *ddr,
    volatile uint8_t *port,
    volatile uint8_t *pin,
    uint8_t bit);


/******************************************************************************
 * Reset fisico mediante XSHUT.
 *
 * Si XSHUT no fue configurado, solamente realiza una espera de arranque.
 ******************************************************************************/
VL53L0X_Status_t VL53L0X_Reset(VL53L0X_t *sensor);


/******************************************************************************
 * Comprueba el registro de identificacion del sensor.
 ******************************************************************************/
VL53L0X_Status_t VL53L0X_IsConnected(VL53L0X_t *sensor);


/******************************************************************************
 * Inicializacion completa del VL53L0X.
 *
 * Antes de llamar esta funcion se debe inicializar I2C:
 *
 * I2C_Master_Init(100000, 1);
 ******************************************************************************/
VL53L0X_Status_t VL53L0X_Init(VL53L0X_t *sensor);


/******************************************************************************
 * Medicion bloqueante.
 *
 * Devuelve la distancia en milimetros mediante distance_mm.
 ******************************************************************************/
VL53L0X_Status_t VL53L0X_ReadDistance(
    VL53L0X_t *sensor,
    uint16_t *distance_mm);


/******************************************************************************
 * Inicia una medicion single sin esperar a que termine.
 ******************************************************************************/
VL53L0X_Status_t VL53L0X_StartSingle(VL53L0X_t *sensor);


/******************************************************************************
 * Comprueba si existe una medicion nueva.
 *
 * Si GPIO1 esta conectado, lee directamente GPIO1.
 * Si no, consulta RESULT_INTERRUPT_STATUS por I2C.
 ******************************************************************************/
VL53L0X_Status_t VL53L0X_DataReady(
    VL53L0X_t *sensor,
    uint8_t *ready);


/******************************************************************************
 * Lee una medicion previamente iniciada con VL53L0X_StartSingle().
 ******************************************************************************/
VL53L0X_Status_t VL53L0X_ReadResult(
    VL53L0X_t *sensor,
    uint16_t *distance_mm);


/******************************************************************************
 * Limpia la interrupcion de nueva medicion.
 ******************************************************************************/
VL53L0X_Status_t VL53L0X_ClearInterrupt(VL53L0X_t *sensor);


/******************************************************************************
 * Cambia la direccion I2C del sensor.
 *
 * new_address debe ser direccion de 7 bits.
 * Ejemplo: 0x30
 ******************************************************************************/
VL53L0X_Status_t VL53L0X_SetAddress(
    VL53L0X_t *sensor,
    uint8_t new_address);


/******************************************************************************
 * Cambia el timeout utilizado en operaciones bloqueantes.
 ******************************************************************************/
void VL53L0X_SetTimeout(
    VL53L0X_t *sensor,
    uint16_t timeout_ms);


#endif /* VL53L0X_H_ */