#include "main.h"
#include "parqueo.h"
#include "parqueo_i2c.h"
#include "display_7seg.h"

extern I2C_HandleTypeDef hi2c1;
volatile uint8_t i2c_mascara_ocupados;
volatile uint32_t i2c_lecturas;
volatile uint32_t i2c_errores;
volatile uint32_t i2c_ultimo_error;
static uint8_t byte_tx;
static uint8_t byte_rx;
static volatile uint8_t total_recibido = 0xFFU;
static volatile uint32_t ultima_escritura;

void ParqueoI2C_Update(void)
{
  uint8_t mascara = 0U;
  for (uint8_t i = 0U; i < PARQUEO_CANTIDAD; ++i)
  {
    if (parqueo_ocupado[i]) mascara |= (uint8_t)(1U << i);
  }
  /* Publicacion de un byte: la interrupcion lee un estado completo. */
  i2c_mascara_ocupados = mascara;

  /* Copiar juntos el dato y su marca de tiempo escritos en la interrupcion. */
  const uint32_t primask = __get_PRIMASK();
  __disable_irq();
  const uint8_t total = total_recibido;
  const uint32_t recibido_ms = ultima_escritura;
  __set_PRIMASK(primask);
  Display7_Mostrar((uint32_t)(HAL_GetTick() - recibido_ms) < 1500U ? total : 0xFFU);

  /* Recuperar la escucha despues de un STOP o un error de transferencia. */
  if (HAL_I2C_GetState(&hi2c1) == HAL_I2C_STATE_READY)
  {
    (void)HAL_I2C_EnableListen_IT(&hi2c1);
  }
}

void ParqueoI2C_Init(void)
{
  /* CubeMX/HAL guardan la direccion desplazada; el ESP32 usa 0x20. */
  if (hi2c1.Init.OwnAddress1 != (PARQUEO_DIRECCION_I2C << 1U)) Error_Handler();
  ParqueoI2C_Update();
}

void HAL_I2C_AddrCallback(I2C_HandleTypeDef *hi2c, uint8_t direccion, uint16_t direccion_detectada)
{
  (void)direccion_detectada;
  if (hi2c->Instance != I2C1) return;
  HAL_StatusTypeDef resultado;
  if (direccion == I2C_DIRECTION_RECEIVE)
  {
    /* El maestro lee: conservar el buffer estable durante la transferencia. */
    byte_tx = i2c_mascara_ocupados;
    resultado = HAL_I2C_Slave_Seq_Transmit_IT(hi2c, &byte_tx, 1U, I2C_FIRST_AND_LAST_FRAME);
  }
  else
  {
    resultado = HAL_I2C_Slave_Seq_Receive_IT(hi2c, &byte_rx, 1U, I2C_FIRST_AND_LAST_FRAME);
  }
  if (resultado != HAL_OK) ++i2c_errores;
}

void HAL_I2C_SlaveRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
  if (hi2c->Instance != I2C1) return;
  if (byte_rx <= 8U || byte_rx == 0xFFU)
  {
    total_recibido = byte_rx;
    ultima_escritura = HAL_GetTick();
  }
}

void HAL_I2C_SlaveTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
  if (hi2c->Instance == I2C1) ++i2c_lecturas;
}

void HAL_I2C_ListenCpltCallback(I2C_HandleTypeDef *hi2c)
{
  if (hi2c->Instance == I2C1) (void)HAL_I2C_EnableListen_IT(hi2c);
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
  if (hi2c->Instance != I2C1) return;
  i2c_ultimo_error = HAL_I2C_GetError(hi2c);
  /* El NACK del maestro al terminar una lectura no es un fallo del enlace. */
  if ((i2c_ultimo_error & ~HAL_I2C_ERROR_AF) != HAL_I2C_ERROR_NONE) ++i2c_errores;
  /* El ciclo principal rearmara la escucha cuando HAL quede READY. */
}
