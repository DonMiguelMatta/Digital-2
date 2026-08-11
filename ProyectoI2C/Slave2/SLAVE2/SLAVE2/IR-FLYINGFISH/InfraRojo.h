/*
 * Biblioteca Sensor Infrarrojo
 *
 * Author: Miguel Donis 22993 - Ian Farrington 21952
 * Description: Interfaz para leer el nivel electrico crudo del IR
 */
/****************************************/
// Encabezado (Libraries)

#ifndef INFRAROJO_H_
#define INFRAROJO_H_

#include <stdint.h>

/****************************************/
// Function prototypes

// Asocia los registros del pin y configura opcionalmente su pull-up.
uint8_t InfraRojo_Init(volatile uint8_t *ddr,
                       volatile uint8_t *port,
                       volatile uint8_t *pin_register,
                       uint8_t pin,
                       uint8_t enable_pullup);
// Devuelve directamente el nivel logico actual del pin.
uint8_t InfraRojo_ReadPinLevel(void);

#endif /* INFRAROJO_H_ */
