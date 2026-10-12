#ifndef CONTROLES_JUGADORES_H
#define CONTROLES_JUGADORES_H

#include <stdint.h>
#include "stm32f4xx_hal.h"

/* Identificadores usados para separar los dos controles. */
#define CONTROLES_JUGADOR_1 1U
#define CONTROLES_JUGADOR_2 2U

/* Cada bit representa un botón y permite combinaciones. */
#define CONTROLES_IZQUIERDA (1U << 0)
#define CONTROLES_DERECHA   (1U << 1)
#define CONTROLES_ARRIBA    (1U << 2)
#define CONTROLES_ABAJO     (1U << 3)
#define CONTROLES_SALTO     (1U << 4)
#define CONTROLES_SALTO_2   (1U << 5)
#define CONTROLES_INICIO    (1U << 6)

/* Contadores para encontrar pérdidas o tramas inválidas. */
typedef struct
{
  uint32_t rx_bytes;
  uint32_t valid_frames;
  uint32_t crc_errors;
  uint32_t id_errors;
  uint32_t format_errors;
  uint32_t uart_errors;
  uint32_t overwritten_frames;
  uint32_t sync_restarts;
  uint32_t timeout_releases;
  uint32_t raw_debug_dropped;
} ControlesDiagnostico_t;

void Controles_Init(void);
void Controles_SetDebugUart(UART_HandleTypeDef *uart);
void Controles_Update(uint32_t now_ms);
void Controles_UART_RxCpltCallback(UART_HandleTypeDef *uart);
void Controles_UART_ErrorCallback(UART_HandleTypeDef *uart);
uint8_t Controles_Held(uint8_t player, uint8_t mask);
uint8_t Controles_TakePressed(uint8_t player, uint8_t mask);
uint8_t Controles_IsConnected(uint8_t player);
const ControlesDiagnostico_t *Controles_GetDiagnostics(uint8_t player);

#endif /* CONTROLES_JUGADORES_H */
