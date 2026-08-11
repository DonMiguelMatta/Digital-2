/*
 * Biblioteca Sensor Infrarrojo
 *
 * Author: Miguel Donis 22993 - Ian Farrington 21952
 * Description: Lectura cruda del nivel digital del sensor IR
 */
/****************************************/
// Encabezado (Libraries)

#include "InfraRojo.h"

#include <stddef.h>

static volatile uint8_t *infrared_pin_register = NULL;
static uint8_t infrared_pin_mask = 0u;

/****************************************/
// NON-Interrupt subroutines

// Configura el pin como entrada y habilita el pull-up si se solicita.
uint8_t InfraRojo_Init(volatile uint8_t *ddr,
                       volatile uint8_t *port,
                       volatile uint8_t *pin_register,
                       uint8_t pin,
                       uint8_t enable_pullup)
{
    if ((ddr == NULL) || (port == NULL) ||
        (pin_register == NULL) || (pin > 7u))
    {
        infrared_pin_register = NULL;
        return 0u;
    }

    infrared_pin_register = pin_register;
    infrared_pin_mask = (uint8_t)(1u << pin);

    *ddr &= (uint8_t)~infrared_pin_mask;
    if (enable_pullup != 0u)
    {
        *port |= infrared_pin_mask;
    }
    else
    {
        *port &= (uint8_t)~infrared_pin_mask;
    }

    return 1u;
}

// Devuelve 1 para nivel alto y 0 para nivel bajo, sin interpretarlo.
uint8_t InfraRojo_ReadPinLevel(void)
{
    if (infrared_pin_register == NULL)
    {
        return 0u;
    }

    return ((*infrared_pin_register & infrared_pin_mask) != 0u) ?
           1u : 0u;
}

/****************************************/
// Interrupt routines
// Esta biblioteca lee el pin por consulta; no usa interrupciones.
