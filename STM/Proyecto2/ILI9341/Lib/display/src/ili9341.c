#include "ili9341.h"
#include "font.h"

#define ILI9341_SLPOUT 0x11U
#define ILI9341_INVOFF 0x20U
#define ILI9341_DISPON 0x29U
#define ILI9341_CASET  0x2AU
#define ILI9341_PASET  0x2BU
#define ILI9341_RAMWR  0x2CU
#define ILI9341_MADCTL 0x36U
#define ILI9341_PIXFMT 0x3AU

extern const uint8_t smallFont[1140];

static uint8_t ILI9341_TimeReached(uint32_t now_ms, uint32_t deadline)
{
  return ((int32_t)(now_ms - deadline) >= 0) ? 1U : 0U;
}

static void ILI9341_SetPin(const ILI9341_Pin *pin, uint8_t high)
{
  if (high != 0U)
  {
    pin->port->BSRR = pin->pin;
  }
  else
  {
    pin->port->BSRR = (uint32_t)pin->pin << 16U;
  }
}

static uint8_t ILI9341_FindDataPort(const ILI9341_t *lcd,
                                    GPIO_TypeDef *port)
{
  uint8_t index;

  for (index = 0U; index < lcd->data_port_count; index++)
  {
    if (lcd->data_ports[index] == port)
    {
      return index;
    }
  }

  return ILI9341_MAX_DATA_PORTS;
}

static uint8_t ILI9341_BuildLookup(ILI9341_t *lcd)
{
  uint16_t value;
  uint8_t bit;
  uint8_t port_index;

  lcd->data_port_count = 0U;
  for (value = 0U; value < 256U; value++)
  {
    for (port_index = 0U; port_index < ILI9341_MAX_DATA_PORTS; port_index++)
    {
      lcd->lookup[value][port_index] = 0U;
    }
  }

  for (bit = 0U; bit < 8U; bit++)
  {
    GPIO_TypeDef *port = lcd->config.data[bit].port;

    if (ILI9341_FindDataPort(lcd, port) == ILI9341_MAX_DATA_PORTS)
    {
      if (lcd->data_port_count >= ILI9341_MAX_DATA_PORTS)
      {
        return 0U;
      }

      lcd->data_ports[lcd->data_port_count] = port;
      lcd->data_port_count++;
    }
  }

  for (value = 0U; value < 256U; value++)
  {
    for (bit = 0U; bit < 8U; bit++)
    {
      const ILI9341_Pin *data_pin = &lcd->config.data[bit];
      uint8_t port_index = ILI9341_FindDataPort(lcd, data_pin->port);

      if ((value & (1U << bit)) != 0U)
      {
        lcd->lookup[value][port_index] |= data_pin->pin;
      }
      else
      {
        lcd->lookup[value][port_index] |= (uint32_t)data_pin->pin << 16U;
      }
    }
  }

  return 1U;
}

static void ILI9341_WriteBus(const ILI9341_t *lcd, uint8_t value)
{
  uint8_t index;

  for (index = 0U; index < lcd->data_port_count; index++)
  {
    lcd->data_ports[index]->BSRR = lcd->lookup[value][index];
  }
}

static void ILI9341_WriteByteSelected(const ILI9341_t *lcd, uint8_t value)
{
  ILI9341_SetPin(&lcd->config.write, 0U);
  ILI9341_WriteBus(lcd, value);
  ILI9341_SetPin(&lcd->config.write, 1U);
}

static void ILI9341_WriteCommandSelected(const ILI9341_t *lcd, uint8_t command)
{
  ILI9341_SetPin(&lcd->config.register_select, 0U);
  ILI9341_WriteByteSelected(lcd, command);
}

static void ILI9341_WriteDataSelected(const ILI9341_t *lcd, uint8_t data)
{
  ILI9341_SetPin(&lcd->config.register_select, 1U);
  ILI9341_WriteByteSelected(lcd, data);
}

static void ILI9341_BeginWindowWrite(const ILI9341_t *lcd,
                                     uint16_t x0,
                                     uint16_t y0,
                                     uint16_t x1,
                                     uint16_t y1)
{
  ILI9341_SetPin(&lcd->config.chip_select, 0U);
  ILI9341_WriteCommandSelected(lcd, ILI9341_CASET);
  ILI9341_WriteDataSelected(lcd, (uint8_t)(x0 >> 8));
  ILI9341_WriteDataSelected(lcd, (uint8_t)x0);
  ILI9341_WriteDataSelected(lcd, (uint8_t)(x1 >> 8));
  ILI9341_WriteDataSelected(lcd, (uint8_t)x1);
  ILI9341_WriteCommandSelected(lcd, ILI9341_PASET);
  ILI9341_WriteDataSelected(lcd, (uint8_t)(y0 >> 8));
  ILI9341_WriteDataSelected(lcd, (uint8_t)y0);
  ILI9341_WriteDataSelected(lcd, (uint8_t)(y1 >> 8));
  ILI9341_WriteDataSelected(lcd, (uint8_t)y1);
  ILI9341_WriteCommandSelected(lcd, ILI9341_RAMWR);
  ILI9341_SetPin(&lcd->config.register_select, 1U);
}

static void ILI9341_WriteColorSelected(const ILI9341_t *lcd, uint16_t color)
{
  ILI9341_WriteByteSelected(lcd, (uint8_t)(color >> 8));
  ILI9341_WriteByteSelected(lcd, (uint8_t)color);
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
  ILI9341_WriteCommand(lcd, 0xC1U);
  ILI9341_WriteData(lcd, 0x10U);
  ILI9341_WriteData(lcd, 0x10U);
  ILI9341_WriteData(lcd, 0x02U);
  ILI9341_WriteData(lcd, 0x02U);
  ILI9341_WriteCommand(lcd, 0xC0U);
  ILI9341_WriteData(lcd, 0x00U);
  ILI9341_WriteData(lcd, 0x35U);
  ILI9341_WriteData(lcd, 0x00U);
  ILI9341_WriteData(lcd, 0x00U);
  ILI9341_WriteData(lcd, 0x01U);
  ILI9341_WriteData(lcd, 0x02U);
  ILI9341_WriteCommand(lcd, 0xC5U);
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
}

void ILI9341_BeginInit(ILI9341_t *lcd,
                       const ILI9341_Parallel8Config *config,
                       uint32_t now_ms)
{
  uint8_t bit;

  if (lcd == 0 || config == 0 || config->reset.port == 0 ||
      config->read.port == 0 || config->write.port == 0 ||
      config->register_select.port == 0 || config->chip_select.port == 0)
  {
    return;
  }

  for (bit = 0U; bit < 8U; bit++)
  {
    if (config->data[bit].port == 0)
    {
      return;
    }
  }

  lcd->config = *config;
  if (ILI9341_BuildLookup(lcd) == 0U)
  {
    lcd->init_state = ILI9341_INIT_ERROR;
    return;
  }
  ILI9341_SetPin(&lcd->config.chip_select, 1U);
  ILI9341_SetPin(&lcd->config.read, 1U);
  ILI9341_SetPin(&lcd->config.write, 1U);
  ILI9341_SetPin(&lcd->config.register_select, 1U);
  ILI9341_SetPin(&lcd->config.reset, 1U);
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
      ILI9341_SetPin(&lcd->config.reset, 0U);
      lcd->init_state = ILI9341_INIT_RESET_LOW;
      lcd->init_deadline = now_ms + 20U;
      break;

    case ILI9341_INIT_RESET_LOW:
      ILI9341_SetPin(&lcd->config.reset, 1U);
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

void ILI9341_WriteCommand(ILI9341_t *lcd, uint8_t command)
{
  if (lcd == 0)
  {
    return;
  }

  ILI9341_SetPin(&lcd->config.chip_select, 0U);
  ILI9341_WriteCommandSelected(lcd, command);
  ILI9341_SetPin(&lcd->config.chip_select, 1U);
}

void ILI9341_WriteData(ILI9341_t *lcd, uint8_t data)
{
  if (lcd == 0)
  {
    return;
  }

  ILI9341_SetPin(&lcd->config.chip_select, 0U);
  ILI9341_WriteDataSelected(lcd, data);
  ILI9341_SetPin(&lcd->config.chip_select, 1U);
}

void ILI9341_SetWindow(ILI9341_t *lcd,
                       uint16_t x0,
                       uint16_t y0,
                       uint16_t x1,
                       uint16_t y1)
{
  if (ILI9341_IsReady(lcd) == 0U)
  {
    return;
  }

  ILI9341_BeginWindowWrite(lcd, x0, y0, x1, y1);
  ILI9341_SetPin(&lcd->config.chip_select, 1U);
}

void ILI9341_DrawPixel(ILI9341_t *lcd,
                       uint16_t x,
                       uint16_t y,
                       uint16_t color)
{
  ILI9341_FillRect(lcd, x, y, 1U, 1U, color);
}

void ILI9341_FillRect(ILI9341_t *lcd,
                      uint16_t x,
                      uint16_t y,
                      uint16_t width,
                      uint16_t height,
                      uint16_t color)
{
  uint32_t pixels;

  if (ILI9341_IsReady(lcd) == 0U || width == 0U || height == 0U ||
      x >= ILI9341_WIDTH || y >= ILI9341_HEIGHT)
  {
    return;
  }

  if ((uint32_t)x + width > ILI9341_WIDTH)
  {
    width = (uint16_t)(ILI9341_WIDTH - x);
  }
  if ((uint32_t)y + height > ILI9341_HEIGHT)
  {
    height = (uint16_t)(ILI9341_HEIGHT - y);
  }

  ILI9341_BeginWindowWrite(lcd, x, y,
                           (uint16_t)(x + width - 1U),
                           (uint16_t)(y + height - 1U));
  pixels = (uint32_t)width * height;
  while (pixels-- > 0U)
  {
    ILI9341_WriteColorSelected(lcd, color);
  }
  ILI9341_SetPin(&lcd->config.chip_select, 1U);
}

void ILI9341_Clear(ILI9341_t *lcd, uint16_t color)
{
  ILI9341_FillRect(lcd, 0U, 0U, ILI9341_WIDTH, ILI9341_HEIGHT, color);
}

void ILI9341_DrawBitmap(ILI9341_t *lcd,
                        uint16_t x,
                        uint16_t y,
                        uint16_t width,
                        uint16_t height,
                        const uint16_t *bitmap)
{
  ILI9341_DrawBitmapRegion(lcd, x, y, width, height, bitmap, width);
}

void ILI9341_DrawBitmapRegion(ILI9341_t *lcd,
                              uint16_t x,
                              uint16_t y,
                              uint16_t width,
                              uint16_t height,
                              const uint16_t *bitmap,
                              uint16_t source_width)
{
  uint16_t row;
  uint16_t column;

  if (ILI9341_IsReady(lcd) == 0U || bitmap == 0 || width == 0U ||
      height == 0U || source_width < width || x >= ILI9341_WIDTH ||
      y >= ILI9341_HEIGHT)
  {
    return;
  }

  if ((uint32_t)x + width > ILI9341_WIDTH)
  {
    width = (uint16_t)(ILI9341_WIDTH - x);
  }
  if ((uint32_t)y + height > ILI9341_HEIGHT)
  {
    height = (uint16_t)(ILI9341_HEIGHT - y);
  }

  ILI9341_BeginWindowWrite(lcd, x, y,
                           (uint16_t)(x + width - 1U),
                           (uint16_t)(y + height - 1U));
  for (row = 0U; row < height; row++)
  {
    for (column = 0U; column < width; column++)
    {
      ILI9341_WriteColorSelected(lcd, bitmap[(uint32_t)row * source_width + column]);
    }
  }
  ILI9341_SetPin(&lcd->config.chip_select, 1U);
}

void ILI9341_DrawPackedBitmapRGB565(ILI9341_t *lcd,
                                    uint16_t x,
                                    uint16_t y,
                                    uint16_t width,
                                    uint16_t height,
                                    const uint32_t *bitmap)
{
  ILI9341_DrawPackedBitmapRGB565Region(lcd,
                                       x,
                                       y,
                                       width,
                                       height,
                                       bitmap,
                                       width,
                                       0U,
                                       0U);
}

void ILI9341_DrawPackedBitmapRGB565Region(ILI9341_t *lcd,
                                          uint16_t x,
                                          uint16_t y,
                                          uint16_t width,
                                          uint16_t height,
                                          const uint32_t *bitmap,
                                          uint16_t source_width,
                                          uint16_t source_x,
                                          uint16_t source_y)
{
  uint16_t row;
  uint16_t column;

  if (ILI9341_IsReady(lcd) == 0U || bitmap == 0 || width == 0U ||
      height == 0U || source_width == 0U || x >= ILI9341_WIDTH ||
      y >= ILI9341_HEIGHT)
  {
    return;
  }

  if ((uint32_t)x + width > ILI9341_WIDTH)
  {
    width = (uint16_t)(ILI9341_WIDTH - x);
  }
  if ((uint32_t)y + height > ILI9341_HEIGHT)
  {
    height = (uint16_t)(ILI9341_HEIGHT - y);
  }

  ILI9341_BeginWindowWrite(lcd, x, y,
                           (uint16_t)(x + width - 1U),
                           (uint16_t)(y + height - 1U));
  for (row = 0U; row < height; row++)
  {
    for (column = 0U; column < width; column++)
    {
      uint32_t pixel_index =
          (uint32_t)(source_y + row) * source_width + source_x + column;
      uint32_t packed_color = bitmap[pixel_index / 2U];
      uint16_t color = ((pixel_index & 1U) == 0U)
                           ? (uint16_t)(packed_color >> 16U)
                           : (uint16_t)packed_color;

      ILI9341_WriteColorSelected(lcd, color);
    }
  }
  ILI9341_SetPin(&lcd->config.chip_select, 1U);
}

void ILI9341_DrawPackedBitmapRGB565Keyed(ILI9341_t *lcd,
                                         uint16_t x,
                                         uint16_t y,
                                         uint16_t width,
                                         uint16_t height,
                                         const uint32_t *background,
                                         const uint32_t *overlay,
                                         uint16_t transparent_color)
{
  ILI9341_DrawPackedBitmapRGB565KeyedRegion(lcd,
                                             x,
                                             y,
                                             width,
                                             height,
                                             background,
                                             overlay,
                                             width,
                                             0U,
                                             0U,
                                             transparent_color);
}

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
                                               uint16_t transparent_color)
{
  uint16_t row;
  uint16_t column;

  if (ILI9341_IsReady(lcd) == 0U || background == 0 || overlay == 0 ||
      width == 0U || height == 0U || source_width == 0U ||
      x >= ILI9341_WIDTH || y >= ILI9341_HEIGHT)
  {
    return;
  }

  if ((uint32_t)x + width > ILI9341_WIDTH)
  {
    width = (uint16_t)(ILI9341_WIDTH - x);
  }
  if ((uint32_t)y + height > ILI9341_HEIGHT)
  {
    height = (uint16_t)(ILI9341_HEIGHT - y);
  }

  ILI9341_BeginWindowWrite(lcd, x, y,
                           (uint16_t)(x + width - 1U),
                           (uint16_t)(y + height - 1U));
  for (row = 0U; row < height; row++)
  {
    for (column = 0U; column < width; column++)
    {
      uint32_t pixel_index =
          (uint32_t)(source_y + row) * source_width + source_x + column;
      uint32_t packed_overlay = overlay[pixel_index / 2U];
      uint16_t overlay_color = ((pixel_index & 1U) == 0U)
                                   ? (uint16_t)(packed_overlay >> 16U)
                                   : (uint16_t)packed_overlay;
      uint16_t color = overlay_color;

      if (overlay_color == transparent_color)
      {
        uint32_t packed_background = background[pixel_index / 2U];
        color = ((pixel_index & 1U) == 0U)
                    ? (uint16_t)(packed_background >> 16U)
                    : (uint16_t)packed_background;
      }

      ILI9341_WriteColorSelected(lcd, color);
    }
  }
  ILI9341_SetPin(&lcd->config.chip_select, 1U);
}

void ILI9341_DrawText(ILI9341_t *lcd,
                      const char *text,
                      uint16_t x,
                      uint16_t y,
                      uint16_t color,
                      uint16_t background)
{
  uint16_t text_index;

  if (ILI9341_IsReady(lcd) == 0U || text == 0 || y > ILI9341_HEIGHT - 12U)
  {
    return;
  }

  for (text_index = 0U; text[text_index] != '\0'; text_index++)
  {
    uint8_t input = (uint8_t)text[text_index];
    uint16_t char_x = (uint16_t)(x + text_index * 8U);
    uint8_t row;

    if (input < 32U || input > 126U || char_x > ILI9341_WIDTH - 8U)
    {
      return;
    }

    ILI9341_BeginWindowWrite(lcd, char_x, y,
                             (uint16_t)(char_x + 7U),
                             (uint16_t)(y + 11U));
    for (row = 0U; row < 12U; row++)
    {
      uint8_t bits = smallFont[(uint16_t)(input - 32U) * 12U + row];
      uint8_t column;

      for (column = 0U; column < 8U; column++)
      {
        uint8_t mask = (uint8_t)(1U << (7U - column));
        ILI9341_WriteColorSelected(lcd, (bits & mask) != 0U ? color : background);
      }
    }
    ILI9341_SetPin(&lcd->config.chip_select, 1U);
  }
}
