#ifndef PARQUEO_H
#define PARQUEO_H
#include <stdint.h>

#define PARQUEO_CANTIDAD 4U

/* Variables visibles en Live Expressions del depurador. */
/* Indices 0, 1, 2 y 3 corresponden a los espacios 1, 2, 3 y 4. */
extern volatile uint8_t parqueo_ocupado[PARQUEO_CANTIDAD];
extern volatile uint8_t espacios_disponibles;
extern volatile uint8_t espacios_ocupados;

void Parqueo_Init(void);
void Parqueo_Update(void);
#endif
