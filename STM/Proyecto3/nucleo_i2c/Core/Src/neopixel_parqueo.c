#include "main.h"
#include "neopixel_parqueo.h"
#include "parqueo.h"

#define BRILLO 32U
#define REFRESCO_MS 50U

static uint32_t ultimo_envio;

void NeoPixelParqueo_Init(void)
{
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_SPI1_CLK_ENABLE();

  /* PA7/D11 es MOSI. El NeoPixel solo recibe esta linea. */
  GPIO_InitTypeDef gpio = {
    .Pin = GPIO_PIN_7,
    .Mode = GPIO_MODE_AF_PP,
    .Pull = GPIO_PULLDOWN,
    .Speed = GPIO_SPEED_FREQ_HIGH,
    .Alternate = GPIO_AF5_SPI1
  };
  HAL_GPIO_Init(GPIOA, &gpio);

  /* APB2 a 16 MHz, divisor 4. SPI maestro de 8 bits, solo transmision. */
  SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_BR_0 |
              SPI_CR1_CPHA | SPI_CR1_BIDIMODE | SPI_CR1_BIDIOE;
  SET_BIT(SPI1->CR1, SPI_CR1_SPE);
  ultimo_envio = HAL_GetTick();
}

static void Transmitir(const uint8_t *trama, uint8_t longitud)
{
  /* Evitar huecos entre bytes durante la trama de aproximadamente 100 us. */
  const uint32_t primask = __get_PRIMASK();
  __disable_irq();
  for (uint8_t i = 0U; i < longitud; ++i)
  {
    uint32_t intentos = 256U;
    while (!(SPI1->SR & SPI_SR_TXE) && intentos > 0U) --intentos;
    if (intentos == 0U) break;
    *(__IO uint8_t *)&SPI1->DR = trama[i];
  }
  __set_PRIMASK(primask);
}

void NeoPixelParqueo_Update(void)
{
  const uint32_t ahora = HAL_GetTick();
  if ((uint32_t)(ahora - ultimo_envio) < REFRESCO_MS) return;
  ultimo_envio = ahora;

  uint8_t trama[1U + PARQUEO_CANTIDAD * 12U] = {0};
  uint8_t indice = 1U;
  for (uint8_t led = 0U; led < PARQUEO_CANTIDAD; ++led)
  {
    const uint8_t ocupado = parqueo_ocupado[led];
    const uint8_t grb[] = {ocupado ? 0U : BRILLO, ocupado ? BRILLO : 0U, 0U};
    for (uint8_t canal = 0U; canal < 3U; ++canal)
    {
      for (uint8_t mascara = 0x80U; mascara != 0U; mascara >>= 2U)
      {
        /* Cada bit SPI dura 250 ns: 1000 representa 0 y 1110 representa 1. */
        const uint8_t alto = (grb[canal] & mascara) ? 0xEU : 0x8U;
        const uint8_t bajo = (grb[canal] & (mascara >> 1U)) ? 0xEU : 0x8U;
        trama[indice++] = (uint8_t)((alto << 4U) | bajo);
      }
    }
  }
  /* El ultimo bit SPI es bajo. Los 50 ms entre tramas incluyen el reset. */
  Transmitir(trama, sizeof(trama));
}
