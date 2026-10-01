#ifndef NIEVE_H
#define NIEVE_H

#include <stdint.h>

#include "ili9341.h"

#define NIEVE_PARTICLE_COUNT 30U

typedef struct
{
  uint16_t x;
  uint16_t y;
  uint8_t size;
} NieveParticle_t;

typedef struct
{
  NieveParticle_t particles[NIEVE_PARTICLE_COUNT];
  uint32_t random_state;
  uint32_t last_update_ms;
  uint8_t initialized;
} Nieve_t;

void Nieve_Init(Nieve_t *nieve, uint32_t seed, uint32_t now_ms);
uint8_t Nieve_Update(Nieve_t *nieve,
                     ILI9341_t *lcd,
                     const uint16_t *background,
                     uint16_t background_width,
                     uint16_t background_height,
                     uint32_t now_ms);
void Nieve_Clear(Nieve_t *nieve,
                 ILI9341_t *lcd,
                 const uint16_t *background,
                 uint16_t background_width,
                 uint16_t background_height);

#endif /* NIEVE_H */
