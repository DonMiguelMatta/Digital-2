#ifndef ILI9341_H
#define ILI9341_H

#include <stdint.h>

#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ILI9341_WIDTH  240U
#define ILI9341_HEIGHT 320U
#define ILI9341_MAX_DATA_PORTS 3U

typedef struct
{
  GPIO_TypeDef *port;
  uint16_t pin;
} ILI9341_Pin;

typedef struct
{
  ILI9341_Pin reset;
  ILI9341_Pin read;
  ILI9341_Pin write;
  ILI9341_Pin register_select;
  ILI9341_Pin chip_select;
  ILI9341_Pin data[8];
  uint8_t memory_access_control;
} ILI9341_Parallel8Config;

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
  ILI9341_Parallel8Config config;
  GPIO_TypeDef *data_ports[ILI9341_MAX_DATA_PORTS];
  uint32_t lookup[256][ILI9341_MAX_DATA_PORTS];
  uint8_t data_port_count;
  ILI9341_InitState init_state;
  uint32_t init_deadline;
} ILI9341_t;

void ILI9341_BeginInit(ILI9341_t *lcd,
                       const ILI9341_Parallel8Config *config,
                       uint32_t now_ms);
uint8_t ILI9341_UpdateInit(ILI9341_t *lcd, uint32_t now_ms);
uint8_t ILI9341_IsReady(const ILI9341_t *lcd);

void ILI9341_WriteCommand(ILI9341_t *lcd, uint8_t command);
void ILI9341_WriteData(ILI9341_t *lcd, uint8_t data);
void ILI9341_SetWindow(ILI9341_t *lcd,
                       uint16_t x0,
                       uint16_t y0,
                       uint16_t x1,
                       uint16_t y1);
void ILI9341_DrawPixel(ILI9341_t *lcd,
                       uint16_t x,
                       uint16_t y,
                       uint16_t color);
void ILI9341_FillRect(ILI9341_t *lcd,
                      uint16_t x,
                      uint16_t y,
                      uint16_t width,
                      uint16_t height,
                      uint16_t color);
void ILI9341_Clear(ILI9341_t *lcd, uint16_t color);
void ILI9341_DrawBitmap(ILI9341_t *lcd,
                        uint16_t x,
                        uint16_t y,
                        uint16_t width,
                        uint16_t height,
                        const uint16_t *bitmap);
void ILI9341_DrawBitmapRegion(ILI9341_t *lcd,
                              uint16_t x,
                              uint16_t y,
                              uint16_t width,
                              uint16_t height,
                              const uint16_t *bitmap,
                              uint16_t source_width);
void ILI9341_DrawPackedBitmapRGB565(ILI9341_t *lcd,
                                    uint16_t x,
                                    uint16_t y,
                                    uint16_t width,
                                    uint16_t height,
                                    const uint32_t *bitmap);
void ILI9341_DrawPackedBitmapRGB565Region(ILI9341_t *lcd,
                                          uint16_t x,
                                          uint16_t y,
                                          uint16_t width,
                                          uint16_t height,
                                          const uint32_t *bitmap,
                                          uint16_t source_width,
                                          uint16_t source_x,
                                          uint16_t source_y);
void ILI9341_DrawPackedBitmapRGB565Keyed(ILI9341_t *lcd,
                                         uint16_t x,
                                         uint16_t y,
                                         uint16_t width,
                                         uint16_t height,
                                         const uint32_t *background,
                                         const uint32_t *overlay,
                                         uint16_t transparent_color);
void ILI9341_DrawPackedBitmapRGB565KeyedRegion(ILI9341_t *lcd,
                                               uint16_t x,
                                               uint16_t y,
                                               uint16_t width,
                                               uint16_t height,
                                               const uint32_t *background,
                                               const uint32_t *overlay,
                                               uint16_t source_width,
                                               uint16_t source_x,
                                               uint16_t source_y,
                                               uint16_t transparent_color);
void ILI9341_DrawText(ILI9341_t *lcd,
                      const char *text,
                      uint16_t x,
                      uint16_t y,
                      uint16_t color,
                      uint16_t background);

#ifdef __cplusplus
}
#endif

#endif /* ILI9341_H */
