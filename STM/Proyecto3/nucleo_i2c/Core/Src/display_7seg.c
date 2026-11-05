#include "display_7seg.h"
#include "main.h"

void Display7_Mostrar(uint8_t libres)
{
  /* Bit 0 = A, bit 6 = G. El ultimo patron es un guion. */
  static const uint8_t patrones[] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x40
  };
  static const struct
  {
    GPIO_TypeDef *puerto;
    uint16_t pin;
  } segmentos[] = {
    {SEG_A_GPIO_Port, SEG_A_Pin}, {SEG_B_GPIO_Port, SEG_B_Pin},
    {SEG_C_GPIO_Port, SEG_C_Pin}, {SEG_D_GPIO_Port, SEG_D_Pin},
    {SEG_E_GPIO_Port, SEG_E_Pin}, {SEG_F_GPIO_Port, SEG_F_Pin},
    {SEG_G_GPIO_Port, SEG_G_Pin}
  };
  static uint8_t ultimo_patron;
  const uint8_t patron = patrones[libres <= 8U ? libres : 9U];
  if (patron == ultimo_patron) return;
  ultimo_patron = patron;

  for (uint8_t i = 0U; i < 7U; ++i)
  {
    /* Anodo comun a 3.3 V: el segmento enciende con un cero. */
    HAL_GPIO_WritePin(segmentos[i].puerto, segmentos[i].pin,
                     (patron & (1U << i)) ? GPIO_PIN_RESET : GPIO_PIN_SET);
  }
}
