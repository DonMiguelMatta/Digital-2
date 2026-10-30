#ifndef CARRO_TEXTURA_H
#define CARRO_TEXTURA_H

#include <stdint.h>

#define CARRO_ANCHO 61U
#define CARRO_ALTO 61U
#define CARRO_COLOR_TRANSPARENTE 0xF819U

/* RGB565, orden por filas. La textura constante se almacena en Flash. */
extern const uint16_t carro_textura[CARRO_ANCHO * CARRO_ALTO];

#endif /* CARRO_TEXTURA_H */
