#include "main.h"
#include "parqueo.h"

#define ANTIRREBOTE_MS 30U

typedef struct
{
  GPIO_TypeDef *boton_puerto;
  uint16_t boton_pin;
  GPIO_TypeDef *led_puerto;
  uint16_t led_pin;
} Parqueo_Conexion;

typedef struct
{
  GPIO_PinState ultima_lectura;
  GPIO_PinState estado_estable;
  uint32_t ultimo_cambio_ms;
} Parqueo_Antirrebote;

/* Cada fila relaciona un boton con su LED; usa los nombres de CubeMX. */
static const Parqueo_Conexion conexiones[PARQUEO_CANTIDAD] =
{
  {BOTON_PARQUEO_GPIO_Port, BOTON_PARQUEO_Pin, LED_LIBRE_GPIO_Port, LED_LIBRE_Pin},
  {BOTON_PARQUEO_2_GPIO_Port, BOTON_PARQUEO_2_Pin, LED_LIBRE_2_GPIO_Port, LED_LIBRE_2_Pin},
  {BOTON_PARQUEO_3_GPIO_Port, BOTON_PARQUEO_3_Pin, LED_LIBRE_3_GPIO_Port, LED_LIBRE_3_Pin},
  {BOTON_PARQUEO_4_GPIO_Port, BOTON_PARQUEO_4_Pin, LED_LIBRE_4_GPIO_Port, LED_LIBRE_4_Pin}
};

volatile uint8_t parqueo_ocupado[PARQUEO_CANTIDAD];
volatile uint8_t espacios_disponibles;
volatile uint8_t espacios_ocupados;
static Parqueo_Antirrebote antirrebote[PARQUEO_CANTIDAD];

static void ActualizarIndicadores(void)
{
  uint8_t ocupados = 0U;

  for (uint8_t i = 0U; i < PARQUEO_CANTIDAD; ++i)
  {
    /* Boton a GND: presionado = ocupado; suelto = libre. */
    parqueo_ocupado[i] = (antirrebote[i].estado_estable == GPIO_PIN_RESET) ? 1U : 0U;
    ocupados += parqueo_ocupado[i];
    HAL_GPIO_WritePin(conexiones[i].led_puerto, conexiones[i].led_pin,
                     parqueo_ocupado[i] ? GPIO_PIN_RESET : GPIO_PIN_SET);
  }

  /* Recalcular evita perder la cuenta con pulsaciones simultaneas. */
  espacios_ocupados = ocupados;
  espacios_disponibles = PARQUEO_CANTIDAD - ocupados;
}

void Parqueo_Init(void)
{
  const uint32_t ahora = HAL_GetTick();

  /* Reconocer tambien los botones que ya esten presionados al encender. */
  for (uint8_t i = 0U; i < PARQUEO_CANTIDAD; ++i)
  {
    GPIO_PinState lectura = HAL_GPIO_ReadPin(conexiones[i].boton_puerto,
                                            conexiones[i].boton_pin);
    antirrebote[i].ultima_lectura = lectura;
    antirrebote[i].estado_estable = lectura;
    antirrebote[i].ultimo_cambio_ms = ahora;
  }
  ActualizarIndicadores();
}

void Parqueo_Update(void)
{
  const uint32_t ahora = HAL_GetTick();
  uint8_t hubo_cambio = 0U;

  for (uint8_t i = 0U; i < PARQUEO_CANTIDAD; ++i)
  {
    GPIO_PinState lectura = HAL_GPIO_ReadPin(conexiones[i].boton_puerto,
                                            conexiones[i].boton_pin);

    if (lectura != antirrebote[i].ultima_lectura)
    {
      antirrebote[i].ultima_lectura = lectura;
      antirrebote[i].ultimo_cambio_ms = ahora;
    }

    /* Cada boton tiene su propio temporizador; ninguno bloquea a los otros. */
    if ((lectura != antirrebote[i].estado_estable) &&
        ((uint32_t)(ahora - antirrebote[i].ultimo_cambio_ms) >= ANTIRREBOTE_MS))
    {
      antirrebote[i].estado_estable = lectura;
      hubo_cambio = 1U;
    }
  }

  if (hubo_cambio)
  {
    ActualizarIndicadores();
  }
}
