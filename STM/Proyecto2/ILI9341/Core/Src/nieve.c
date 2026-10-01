#include "nieve.h"

#define NIEVE_UPDATE_MS 50U
#define NIEVE_COLOR 0xFFFFU

static uint32_t Nieve_NextRandom(Nieve_t *nieve)
{
  nieve->random_state = nieve->random_state * 1664525U + 1013904223U;
  return nieve->random_state;
}

static void Nieve_PlaceParticle(Nieve_t *nieve,
                                NieveParticle_t *particle,
                                uint16_t background_width,
                                uint16_t background_height,
                                uint8_t from_top)
{
  particle->x = (uint16_t)(Nieve_NextRandom(nieve) % background_width);
  particle->y = (from_top != 0U)
                    ? 0U
                    : (uint16_t)(Nieve_NextRandom(nieve) % background_height);
  particle->size = (uint8_t)(1U + (Nieve_NextRandom(nieve) % 4U));
}

static void Nieve_RestoreParticle(ILI9341_t *lcd,
                                  const NieveParticle_t *particle,
                                  const uint16_t *background,
                                  uint16_t background_width)
{
  const uint16_t *background_region =
      &background[(uint32_t)particle->y * background_width + particle->x];

  ILI9341_DrawBitmapRegion(lcd,
                            particle->x,
                            particle->y,
                            particle->size,
                            particle->size,
                            background_region,
                            background_width);
}

void Nieve_Init(Nieve_t *nieve, uint32_t seed, uint32_t now_ms)
{
  uint8_t index;

  if (nieve == 0)
  {
    return;
  }

  nieve->random_state = (seed == 0U) ? 1U : seed;
  nieve->last_update_ms = now_ms;
  nieve->initialized = 1U;

  for (index = 0U; index < NIEVE_PARTICLE_COUNT; index++)
  {
    Nieve_PlaceParticle(nieve,
                        &nieve->particles[index],
                        ILI9341_WIDTH,
                        ILI9341_HEIGHT,
                        0U);
  }
}

uint8_t Nieve_Update(Nieve_t *nieve,
                     ILI9341_t *lcd,
                     const uint16_t *background,
                     uint16_t background_width,
                     uint16_t background_height,
                     uint32_t now_ms)
{
  uint8_t index;

  if (nieve == 0 || lcd == 0 || background == 0 || background_width == 0U ||
      background_height == 0U || nieve->initialized == 0U ||
      (now_ms - nieve->last_update_ms) < NIEVE_UPDATE_MS)
  {
    return 0U;
  }

  nieve->last_update_ms = now_ms;

  for (index = 0U; index < NIEVE_PARTICLE_COUNT; index++)
  {
    Nieve_RestoreParticle(lcd,
                          &nieve->particles[index],
                          background,
                          background_width);
  }

  for (index = 0U; index < NIEVE_PARTICLE_COUNT; index++)
  {
    NieveParticle_t *particle = &nieve->particles[index];

    particle->x++;
    particle->y = (uint16_t)(particle->y + 4U);
    if (particle->x >= background_width || particle->y >= background_height)
    {
      Nieve_PlaceParticle(nieve,
                          particle,
                          background_width,
                          background_height,
                          1U);
    }

    ILI9341_FillRect(lcd,
                     particle->x,
                     particle->y,
                     particle->size,
                     particle->size,
                     NIEVE_COLOR);
  }

  return 1U;
}

void Nieve_Clear(Nieve_t *nieve,
                 ILI9341_t *lcd,
                 const uint16_t *background,
                 uint16_t background_width,
                 uint16_t background_height)
{
  uint8_t index;

  if (nieve == 0 || lcd == 0 || background == 0 || background_width == 0U ||
      background_height == 0U || nieve->initialized == 0U)
  {
    return;
  }

  for (index = 0U; index < NIEVE_PARTICLE_COUNT; index++)
  {
    Nieve_RestoreParticle(lcd,
                          &nieve->particles[index],
                          background,
                          background_width);
  }
}
