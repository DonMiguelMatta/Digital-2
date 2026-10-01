#ifndef ILI9341_H
#define ILI9341_H

#include <stdint.h>

#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ILI9341_WIDTH  320U
#define ILI9341_HEIGHT 240U

typedef struct
{
  SPI_HandleTypeDef *spi;
  GPIO_TypeDef *reset_port;
  uint16_t reset_pin;
  GPIO_TypeDef *dc_port;
  uint16_t dc_pin;
  GPIO_TypeDef *cs_port;
  uint16_t cs_pin;
  uint8_t memory_access_control;
} ILI9341_Config;

typedef enum
{
  ILI9341_INIT_RESET_HIGH,
  ILI9341_INIT_RESET_LOW,
  ILI9341_INIT_RESET_RELEASED,
  ILI9341_INIT_SLEEP_OUT,
  ILI9341_INIT_INVERSION_OFF,
  ILI9341_INIT_DISPLAY_ON,
  ILI9341_INIT_READY,
  ILI9341_INIT_ERROR
} ILI9341_InitState;

typedef struct
{
  ILI9341_Config config;
  ILI9341_InitState init_state;
  uint32_t init_deadline;
} ILI9341_t;

/* Starts the non-blocking initialization sequence. */
void ILI9341_BeginInit(ILI9341_t *lcd,
                       const ILI9341_Config *config,
                       uint32_t now_ms);

/* Advances initialization. Returns 1 only after the display is ready to draw. */
uint8_t ILI9341_UpdateInit(ILI9341_t *lcd, uint32_t now_ms);
uint8_t ILI9341_IsReady(const ILI9341_t *lcd);

void ILI9341_Clear(ILI9341_t *lcd, uint16_t color);
void ILI9341_DrawHLine(ILI9341_t *lcd,
                       uint16_t x,
                       uint16_t y,
                       uint16_t length,
                       uint16_t color);
void ILI9341_DrawVLine(ILI9341_t *lcd,
                       uint16_t x,
                       uint16_t y,
                       uint16_t length,
                       uint16_t color);
void ILI9341_DrawRect(ILI9341_t *lcd,
                      uint16_t x,
                      uint16_t y,
                      uint16_t width,
                      uint16_t height,
                      uint16_t color);
void ILI9341_FillRect(ILI9341_t *lcd,
                      uint16_t x,
                      uint16_t y,
                      uint16_t width,
                      uint16_t height,
                      uint16_t color);
void ILI9341_DrawText(ILI9341_t *lcd,
                      const char *text,
                      uint16_t x,
                      uint16_t y,
                      uint8_t font_size,
                      uint16_t color,
                      uint16_t background);
void ILI9341_DrawBitmap(ILI9341_t *lcd,
                        uint16_t x,
                        uint16_t y,
                        uint16_t width,
                        uint16_t height,
                        const uint16_t *bitmap);
void ILI9341_DrawBitmapTransparent(ILI9341_t *lcd,
                                   uint16_t x,
                                   uint16_t y,
                                   uint16_t width,
                                   uint16_t height,
                                   const uint16_t *bitmap,
                                   uint16_t transparent_color);
void ILI9341_DrawSprite(ILI9341_t *lcd,
                        int16_t x,
                        int16_t y,
                        uint16_t width,
                        uint16_t height,
                        const uint16_t *bitmap,
                        uint16_t columns,
                        uint16_t index,
                        uint8_t flip,
                        int16_t offset);

#ifdef __cplusplus
}
#endif

#endif /* ILI9341_H */
