#include "main.h"
#include "parqueo.h"
#include "parqueo_i2c.h"

extern I2C_HandleTypeDef hi2c1;

volatile uint8_t i2c_mascara_ocupados;
volatile uint32_t i2c_lecturas;
volatile uint32_t i2c_errores;
volatile uint32_t i2c_ultimo_error;
static uint8_t byte_tx;
static uint8_t byte_rx;
static volatile uint8_t mascara_remota = PARQUEO_GRUPO_SIN_DATOS;
static volatile uint32_t ultima_escritura;
static volatile uint8_t recuperar_periferico;
static uint32_t ultimo_reintento;

static void RearmarEscucha(I2C_HandleTypeDef *hi2c)
{
  if (!recuperar_periferico && HAL_I2C_GetState(hi2c) == HAL_I2C_STATE_READY)
  {
    if (HAL_I2C_EnableListen_IT(hi2c) != HAL_OK)
    {
      ++i2c_errores;
      recuperar_periferico = 1U;
    }
  }
}

void ParqueoI2C_Update(void)
{
  uint8_t mascara = 0U;
  for (uint8_t i = 0U; i < PARQUEO_CANTIDAD; ++i)
  {
    if (parqueo_ocupado[i]) mascara |= (uint8_t)(1U << i);
  }
  /* Publicacion atomica de un byte; el buffer TX solo cambia al iniciar la lectura. */
  i2c_mascara_ocupados = mascara;

  /* Un error real puede dejar HAL en LISTEN con las interrupciones deshabilitadas.
   * Recuperar desde el ciclo principal, sin dibujar ni esperar dentro de la ISR. */
  if (recuperar_periferico)
  {
    const uint32_t ahora = HAL_GetTick();
    if ((uint32_t)(ahora - ultimo_reintento) < 20U) return;
    ultimo_reintento = ahora;
    if (HAL_I2C_DeInit(&hi2c1) != HAL_OK || HAL_I2C_Init(&hi2c1) != HAL_OK)
    {
      ++i2c_errores;
      return;
    }
    recuperar_periferico = 0U;
  }
  RearmarEscucha(&hi2c1);
}

void ParqueoI2C_Init(void)
{
  /* HAL usa la direccion desplazada; Wire usa 0x21. */
  if (hi2c1.Init.OwnAddress1 != (PARQUEO_DIRECCION_I2C << 1U)) Error_Handler();
  ParqueoI2C_Update();
}

void HAL_I2C_AddrCallback(I2C_HandleTypeDef *hi2c, uint8_t direccion,
                         uint16_t direccion_detectada)
{
  (void)direccion_detectada;
  if (hi2c->Instance != I2C1) return;
  HAL_StatusTypeDef resultado;
  if (direccion == I2C_DIRECTION_RECEIVE)
  {
    /* El maestro lee: conservar el buffer estable durante toda la transferencia. */
    byte_tx = i2c_mascara_ocupados;
    resultado = HAL_I2C_Slave_Seq_Transmit_IT(hi2c, &byte_tx, 1U,
                                             I2C_FIRST_AND_LAST_FRAME);
  }
  else
  {
    resultado = HAL_I2C_Slave_Seq_Receive_IT(hi2c, &byte_rx, 1U,
                                            I2C_FIRST_AND_LAST_FRAME);
  }
  if (resultado != HAL_OK)
  {
    ++i2c_errores;
    recuperar_periferico = 1U;
  }
}

void HAL_I2C_SlaveRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
  if (hi2c->Instance != I2C1) return;
  if (byte_rx <= 0x0FU || byte_rx == PARQUEO_GRUPO_SIN_DATOS)
  {
    mascara_remota = byte_rx;
    ultima_escritura = HAL_GetTick();
  }
}

uint8_t ParqueoI2C_MascaraRemota(void)
{
  const uint32_t primask = __get_PRIMASK();
  __disable_irq();
  const uint8_t mascara = mascara_remota;
  const uint32_t recibido_ms = ultima_escritura;
  __set_PRIMASK(primask);
  return (uint32_t)(HAL_GetTick() - recibido_ms) < 1500U
         ? mascara : PARQUEO_GRUPO_SIN_DATOS;
}

void HAL_I2C_SlaveTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
  if (hi2c->Instance == I2C1) ++i2c_lecturas;
}

void HAL_I2C_ListenCpltCallback(I2C_HandleTypeDef *hi2c)
{
  if (hi2c->Instance == I2C1) RearmarEscucha(hi2c);
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
  if (hi2c->Instance != I2C1) return;
  i2c_ultimo_error = HAL_I2C_GetError(hi2c);
  /* El NACK de fin de lectura no se cuenta como error del enlace. */
  if ((i2c_ultimo_error & ~HAL_I2C_ERROR_AF) != HAL_I2C_ERROR_NONE)
  {
    ++i2c_errores;
    recuperar_periferico = 1U;
  }
  /* Rearmar despues de que termine el manejador HAL, desde el ciclo principal. */
}
