#include <string.h>

#include "font.h"
#include "ili9341.h"
#include "lcd_registers.h"
#include "pgmspace.h"

extern const uint8_t smallFont[1140];
extern const uint16_t bigFont[1520];

static uint8_t ILI9341_TimeReached(uint32_t now_ms, uint32_t deadline)
{
  return ((int32_t)(now_ms - deadline) >= 0) ? 1U : 0U;
}

static void ILI9341_Select(const ILI9341_t *lcd)
{
  HAL_GPIO_WritePin(lcd->config.cs_port, lcd->config.cs_pin, GPIO_PIN_RESET);
}

static void ILI9341_Deselect(const ILI9341_t *lcd)
{
  HAL_GPIO_WritePin(lcd->config.cs_port, lcd->config.cs_pin, GPIO_PIN_SET);
}

static void ILI9341_SetDataMode(const ILI9341_t *lcd, GPIO_PinState state)
{
  HAL_GPIO_WritePin(lcd->config.dc_port, lcd->config.dc_pin, state);
}

static void ILI9341_WriteByte(const ILI9341_t *lcd, uint8_t value)
{
  (void)HAL_SPI_Transmit(lcd->config.spi, &value, 1U, 1U);
}

static void ILI9341_WriteCommand(const ILI9341_t *lcd, uint8_t command)
{
  ILI9341_Select(lcd);
  ILI9341_SetDataMode(lcd, GPIO_PIN_RESET);
  ILI9341_WriteByte(lcd, command);
  ILI9341_Deselect(lcd);
}

static void ILI9341_WriteData(const ILI9341_t *lcd, uint8_t data)
{
  ILI9341_Select(lcd);
  ILI9341_SetDataMode(lcd, GPIO_PIN_SET);
  ILI9341_WriteByte(lcd, data);
  ILI9341_Deselect(lcd);
}

static void ILI9341_BeginMemoryWrite(const ILI9341_t *lcd,
                                     uint16_t x0,
                                     uint16_t y0,
                                     uint16_t x1,
                                     uint16_t y1)
{
  ILI9341_WriteCommand(lcd, ILI9341_CASET);
  ILI9341_WriteData(lcd, (uint8_t)(x0 >> 8));
  ILI9341_WriteData(lcd, (uint8_t)x0);
  ILI9341_WriteData(lcd, (uint8_t)(x1 >> 8));
  ILI9341_WriteData(lcd, (uint8_t)x1);

  ILI9341_WriteCommand(lcd, ILI9341_PASET);
  ILI9341_WriteData(lcd, (uint8_t)(y0 >> 8));
  ILI9341_WriteData(lcd, (uint8_t)y0);
  ILI9341_WriteData(lcd, (uint8_t)(y1 >> 8));
  ILI9341_WriteData(lcd, (uint8_t)y1);

  ILI9341_WriteCommand(lcd, ILI9341_RAMWR);
  ILI9341_Select(lcd);
  ILI9341_SetDataMode(lcd, GPIO_PIN_SET);
}

static void ILI9341_EndMemoryWrite(const ILI9341_t *lcd)
{
  ILI9341_Deselect(lcd);
}

static void ILI9341_WriteColor(const ILI9341_t *lcd, uint16_t color)
{
  ILI9341_WriteByte(lcd, (uint8_t)(color >> 8));
  ILI9341_WriteByte(lcd, (uint8_t)color);
}

static void ILI9341_ConfigureController(const ILI9341_t *lcd)
{
  ILI9341_WriteCommand(lcd, 0xD1U);
  ILI9341_WriteData(lcd, 0x00U);
  ILI9341_WriteData(lcd, 0x71U);
  ILI9341_WriteData(lcd, 0x19U);

  ILI9341_WriteCommand(lcd, 0xD0U);
  ILI9341_WriteData(lcd, 0x07U);
  ILI9341_WriteData(lcd, 0x01U);
  ILI9341_WriteData(lcd, 0x08U);

  ILI9341_WriteCommand(lcd, ILI9341_MADCTL);
  ILI9341_WriteData(lcd, lcd->config.memory_access_control);
  ILI9341_WriteCommand(lcd, ILI9341_PIXFMT);
  ILI9341_WriteData(lcd, 0x05U);

  ILI9341_WriteCommand(lcd, ILI9341_PWCTR2);
  ILI9341_WriteData(lcd, 0x10U);
  ILI9341_WriteData(lcd, 0x10U);
  ILI9341_WriteData(lcd, 0x02U);
  ILI9341_WriteData(lcd, 0x02U);

  ILI9341_WriteCommand(lcd, ILI9341_PWCTR1);
  ILI9341_WriteData(lcd, 0x00U);
  ILI9341_WriteData(lcd, 0x35U);
  ILI9341_WriteData(lcd, 0x00U);
  ILI9341_WriteData(lcd, 0x00U);
  ILI9341_WriteData(lcd, 0x01U);
  ILI9341_WriteData(lcd, 0x02U);

  ILI9341_WriteCommand(lcd, ILI9341_VMCTR1);
  ILI9341_WriteData(lcd, 0x04U);
  ILI9341_WriteCommand(lcd, 0xD2U);
  ILI9341_WriteData(lcd, 0x01U);
  ILI9341_WriteData(lcd, 0x44U);

  ILI9341_WriteCommand(lcd, 0xC8U);
  ILI9341_WriteData(lcd, 0x04U);
  ILI9341_WriteData(lcd, 0x67U);
  ILI9341_WriteData(lcd, 0x35U);
  ILI9341_WriteData(lcd, 0x04U);
  ILI9341_WriteData(lcd, 0x08U);
  ILI9341_WriteData(lcd, 0x06U);
  ILI9341_WriteData(lcd, 0x24U);
  ILI9341_WriteData(lcd, 0x01U);
  ILI9341_WriteData(lcd, 0x37U);
  ILI9341_WriteData(lcd, 0x40U);
  ILI9341_WriteData(lcd, 0x03U);
  ILI9341_WriteData(lcd, 0x10U);
  ILI9341_WriteData(lcd, 0x08U);
  ILI9341_WriteData(lcd, 0x80U);
  ILI9341_WriteData(lcd, 0x00U);

  ILI9341_WriteCommand(lcd, ILI9341_CASET);
  ILI9341_WriteData(lcd, 0x00U);
  ILI9341_WriteData(lcd, 0x00U);
  ILI9341_WriteData(lcd, 0x01U);
  ILI9341_WriteData(lcd, 0x3FU);
  ILI9341_WriteCommand(lcd, ILI9341_PASET);
  ILI9341_WriteData(lcd, 0x00U);
  ILI9341_WriteData(lcd, 0x00U);
  ILI9341_WriteData(lcd, 0x01U);
  ILI9341_WriteData(lcd, 0xE0U);
  ILI9341_WriteCommand(lcd, ILI9341_DISPON);
  ILI9341_WriteCommand(lcd, ILI9341_RAMWR);
}

void ILI9341_BeginInit(ILI9341_t *lcd,
                       const ILI9341_Config *config,
                       uint32_t now_ms)
{
  if (lcd == 0 || config == 0 || config->spi == 0 ||
      config->reset_port == 0 || config->dc_port == 0 ||
      config->cs_port == 0)
  {
    return;
  }

  lcd->config = *config;
  HAL_GPIO_WritePin(lcd->config.cs_port, lcd->config.cs_pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(lcd->config.dc_port, lcd->config.dc_pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(lcd->config.reset_port, lcd->config.reset_pin, GPIO_PIN_SET);
  lcd->init_state = ILI9341_INIT_RESET_HIGH;
  lcd->init_deadline = now_ms + 5U;
}

uint8_t ILI9341_UpdateInit(ILI9341_t *lcd, uint32_t now_ms)
{
  if (lcd == 0 || lcd->init_state == ILI9341_INIT_ERROR)
  {
    return 0U;
  }

  if (lcd->init_state == ILI9341_INIT_READY)
  {
    return 1U;
  }

  if (ILI9341_TimeReached(now_ms, lcd->init_deadline) == 0U)
  {
    return 0U;
  }

  switch (lcd->init_state)
  {
    case ILI9341_INIT_RESET_HIGH:
      HAL_GPIO_WritePin(lcd->config.reset_port, lcd->config.reset_pin, GPIO_PIN_RESET);
      lcd->init_state = ILI9341_INIT_RESET_LOW;
      lcd->init_deadline = now_ms + 20U;
      break;

    case ILI9341_INIT_RESET_LOW:
      HAL_GPIO_WritePin(lcd->config.reset_port, lcd->config.reset_pin, GPIO_PIN_SET);
      lcd->init_state = ILI9341_INIT_RESET_RELEASED;
      lcd->init_deadline = now_ms + 150U;
      break;

    case ILI9341_INIT_RESET_RELEASED:
      ILI9341_WriteCommand(lcd, 0xE9U);
      ILI9341_WriteData(lcd, 0x20U);
      ILI9341_WriteCommand(lcd, ILI9341_SLPOUT);
      lcd->init_state = ILI9341_INIT_SLEEP_OUT;
      lcd->init_deadline = now_ms + 100U;
      break;

    case ILI9341_INIT_SLEEP_OUT:
      ILI9341_ConfigureController(lcd);
      ILI9341_WriteCommand(lcd, ILI9341_INVOFF);
      lcd->init_state = ILI9341_INIT_INVERSION_OFF;
      lcd->init_deadline = now_ms + 120U;
      break;

    case ILI9341_INIT_INVERSION_OFF:
      ILI9341_WriteCommand(lcd, ILI9341_SLPOUT);
      lcd->init_state = ILI9341_INIT_DISPLAY_ON;
      lcd->init_deadline = now_ms + 120U;
      break;

    case ILI9341_INIT_DISPLAY_ON:
      ILI9341_WriteCommand(lcd, ILI9341_DISPON);
      ILI9341_Deselect(lcd);
      lcd->init_state = ILI9341_INIT_READY;
      break;

    default:
      lcd->init_state = ILI9341_INIT_ERROR;
      break;
  }

  return ILI9341_IsReady(lcd);
}

uint8_t ILI9341_IsReady(const ILI9341_t *lcd)
{
  return (lcd != 0 && lcd->init_state == ILI9341_INIT_READY) ? 1U : 0U;
}

void ILI9341_Clear(ILI9341_t *lcd, uint16_t color)
{
  ILI9341_FillRect(lcd, 0U, 0U, ILI9341_WIDTH, ILI9341_HEIGHT, color);
}

void ILI9341_DrawHLine(ILI9341_t *lcd,
                       uint16_t x,
                       uint16_t y,
                       uint16_t length,
                       uint16_t color)
{
  ILI9341_FillRect(lcd, x, y, length, 1U, color);
}

void ILI9341_DrawVLine(ILI9341_t *lcd,
                       uint16_t x,
                       uint16_t y,
                       uint16_t length,
                       uint16_t color)
{
  ILI9341_FillRect(lcd, x, y, 1U, length, color);
}

void ILI9341_DrawRect(ILI9341_t *lcd,
                      uint16_t x,
                      uint16_t y,
                      uint16_t width,
                      uint16_t height,
                      uint16_t color)
{
  if (width == 0U || height == 0U)
  {
    return;
  }

  ILI9341_DrawHLine(lcd, x, y, width, color);
  ILI9341_DrawHLine(lcd, x, (uint16_t)(y + height - 1U), width, color);
  ILI9341_DrawVLine(lcd, x, y, height, color);
  ILI9341_DrawVLine(lcd, (uint16_t)(x + width - 1U), y, height, color);
}

void ILI9341_FillRect(ILI9341_t *lcd,
                      uint16_t x,
                      uint16_t y,
                      uint16_t width,
                      uint16_t height,
                      uint16_t color)
{
  uint32_t pixels;

  if (ILI9341_IsReady(lcd) == 0U || width == 0U || height == 0U)
  {
    return;
  }

  ILI9341_BeginMemoryWrite(lcd, x, y,
                           (uint16_t)(x + width - 1U),
                           (uint16_t)(y + height - 1U));
  pixels = (uint32_t)width * height;
  while (pixels-- > 0U)
  {
    ILI9341_WriteColor(lcd, color);
  }
  ILI9341_EndMemoryWrite(lcd);
}

void ILI9341_DrawText(ILI9341_t *lcd,
                      const char *text,
                      uint16_t x,
                      uint16_t y,
                      uint8_t font_size,
                      uint16_t color,
                      uint16_t background)
{
  uint16_t font_width;
  uint16_t font_height;
  uint16_t character;
  uint16_t row;
  uint16_t column;
  uint16_t text_index;

  if (ILI9341_IsReady(lcd) == 0U || text == 0)
  {
    return;
  }

  if (font_size == 1U)
  {
    font_width = fontXSizeSmal;
    font_height = fontYSizeSmal;
  }
  else if (font_size == 2U)
  {
    font_width = fontXSizeBig;
    font_height = fontYSizeBig;
  }
  else
  {
    return;
  }

  for (text_index = 0U; text[text_index] != '\0'; text_index++)
  {
    uint8_t input = (uint8_t)text[text_index];

    if (input < 32U || input > 126U)
    {
      continue;
    }

    ILI9341_BeginMemoryWrite(lcd,
                             (uint16_t)(x + text_index * font_width), y,
                             (uint16_t)(x + text_index * font_width + font_width - 1U),
                             (uint16_t)(y + font_height - 1U));
    character = (uint16_t)(input - 32U);

    for (row = 0U; row < font_height; row++)
    {
      uint32_t bits;

      if (font_size == 1U)
      {
        bits = pgm_read_word_near(smallFont + character * font_height + row);
      }
      else
      {
        bits = pgm_read_word_near(bigFont + character * font_height + row);
      }

      for (column = 0U; column < font_width; column++)
      {
        uint32_t mask = 1UL << (font_width - column - 1U);
        ILI9341_WriteColor(lcd, (bits & mask) != 0U ? color : background);
      }
    }
    ILI9341_EndMemoryWrite(lcd);
  }
}

void ILI9341_DrawBitmap(ILI9341_t *lcd,
                        uint16_t x,
                        uint16_t y,
                        uint16_t width,
                        uint16_t height,
                        const uint16_t *bitmap)
{
  uint32_t pixel_index;
  uint32_t pixels;

  if (ILI9341_IsReady(lcd) == 0U || bitmap == 0 || width == 0U || height == 0U)
  {
    return;
  }

  ILI9341_BeginMemoryWrite(lcd, x, y,
                           (uint16_t)(x + width - 1U),
                           (uint16_t)(y + height - 1U));
  pixels = (uint32_t)width * height;
  for (pixel_index = 0U; pixel_index < pixels; pixel_index++)
  {
    ILI9341_WriteColor(lcd, bitmap[pixel_index]);
  }
  ILI9341_EndMemoryWrite(lcd);
}

void ILI9341_DrawBitmapTransparent(ILI9341_t *lcd,
                                   uint16_t x,
                                   uint16_t y,
                                   uint16_t width,
                                   uint16_t height,
                                   const uint16_t *bitmap,
                                   uint16_t transparent_color)
{
  uint16_t row;
  uint16_t column;

  if (ILI9341_IsReady(lcd) == 0U || bitmap == 0)
  {
    return;
  }

  for (row = 0U; row < height; row++)
  {
    for (column = 0U; column < width; column++)
    {
      uint16_t pixel = bitmap[(uint32_t)row * width + column];

      if (pixel != transparent_color)
      {
        ILI9341_BeginMemoryWrite(lcd,
                                 (uint16_t)(x + column),
                                 (uint16_t)(y + row),
                                 (uint16_t)(x + column),
                                 (uint16_t)(y + row));
        ILI9341_WriteColor(lcd, pixel);
        ILI9341_EndMemoryWrite(lcd);
      }
    }
  }
}

void ILI9341_DrawSprite(ILI9341_t *lcd,
                        int16_t x,
                        int16_t y,
                        uint16_t width,
                        uint16_t height,
                        const uint16_t *bitmap,
                        uint16_t columns,
                        uint16_t index,
                        uint8_t flip,
                        int16_t offset)
{
  uint16_t row;
  uint16_t column;
  int32_t stride;

  if (ILI9341_IsReady(lcd) == 0U || bitmap == 0 || width == 0U ||
      height == 0U || columns == 0U || index >= columns || x < 0 || y < 0)
  {
    return;
  }

  ILI9341_BeginMemoryWrite(lcd, (uint16_t)x, (uint16_t)y,
                           (uint16_t)(x + (int16_t)width - 1),
                           (uint16_t)(y + (int16_t)height - 1));
  stride = (int32_t)width * columns;

  for (row = 0U; row < height; row++)
  {
    int32_t pixel_index;

    if (flip != 0U)
    {
      pixel_index = (int32_t)row * stride + (int32_t)index * width +
                    width - 1 - offset;
      for (column = 0U; column < width; column++)
      {
        ILI9341_WriteColor(lcd, bitmap[pixel_index--]);
      }
    }
    else
    {
      pixel_index = (int32_t)row * stride + (int32_t)index * width + offset;
      for (column = 0U; column < width; column++)
      {
        ILI9341_WriteColor(lcd, bitmap[pixel_index++]);
      }
    }
  }
  ILI9341_EndMemoryWrite(lcd);
}
