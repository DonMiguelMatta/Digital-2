#include "controles_jugadores.h"

#include <string.h>

/* Formato y valores válidos de una trama del control. */
#define CONTROLES_SYNC 0xA5U
#define CONTROLES_FRAME_SIZE 5U
#define CONTROLES_QUEUE_DEPTH 8U
#define CONTROLES_RAW_QUEUE_DEPTH 64U
#define CONTROLES_STATUS_DISCONNECTED 0xA0U
#define CONTROLES_STATUS_CONNECTED 0xA1U
#define CONTROLES_BUTTON_MASK 0x7FU
#define CONTROLES_STALE_TIMEOUT_MS 300U
#define CONTROLES_DEBUG_REPEAT_MS 500U
#define CONTROLES_DEBUG_RAW_PERIOD_MS 100U

extern UART_HandleTypeDef huart5;
extern UART_HandleTypeDef huart3;

/* Estado independiente de cada control y su cola de recepción. */
typedef struct
{
  UART_HandleTypeDef *uart;
  uint8_t expected_id;
  uint8_t buttons;
  uint8_t pressed_events;
  uint8_t connected;
  uint8_t has_valid_frame;
  uint32_t last_valid_ms;
  uint8_t rx_byte;
  uint8_t rx_frame[CONTROLES_FRAME_SIZE];
  uint8_t rx_index;
  uint8_t frame_queue[CONTROLES_QUEUE_DEPTH][CONTROLES_FRAME_SIZE];
  uint32_t frame_time_queue[CONTROLES_QUEUE_DEPTH];
  volatile uint8_t queue_head;
  volatile uint8_t queue_tail;
  uint8_t raw_debug_queue[CONTROLES_RAW_QUEUE_DEPTH];
  volatile uint8_t raw_debug_head;
  volatile uint8_t raw_debug_tail;
  uint8_t debug_has_frame;
  uint8_t debug_last_buttons;
  uint8_t debug_last_status;
  uint32_t debug_last_ms;
  uint32_t debug_raw_last_ms;
  ControlesDiagnostico_t diagnostics;
} ControlesCanal_t;

static ControlesCanal_t controls[2];
static UART_HandleTypeDef *debug_uart;

/* Busca el canal usando el número de jugador. */
static ControlesCanal_t *Controles_GetChannel(uint8_t player)
{
  if (player == CONTROLES_JUGADOR_1)
  {
    return &controls[0];
  }
  if (player == CONTROLES_JUGADOR_2)
  {
    return &controls[1];
  }

  return 0;
}

/* Busca qué jugador pertenece a la UART que generó el evento. */
static ControlesCanal_t *Controles_GetUartChannel(UART_HandleTypeDef *uart)
{
  uint8_t index;

  for (index = 0U; index < 2U; index++)
  {
    if (controls[index].uart == uart)
    {
      return &controls[index];
    }
  }

  return 0;
}

/* CRC-8/SMBUS: polinomio 0x07, valor inicial 0x00. */
static uint8_t Controles_Crc8(const uint8_t *data, uint8_t length)
{
  uint8_t crc = 0U;
  uint8_t index;

  while (length-- != 0U)
  {
    crc ^= *data++;
    for (index = 0U; index < 8U; index++)
    {
      crc = (crc & 0x80U) != 0U
                ? (uint8_t)((crc << 1U) ^ 0x07U)
                : (uint8_t)(crc << 1U);
    }
  }

  return crc;
}

/* Convierte un nibble para mostrarlo en la terminal. */
static char Controles_HexDigit(uint8_t value)
{
  value &= 0x0FU;
  return (value < 10U) ? (char)('0' + value)
                       : (char)('A' + value - 10U);
}

/* Envía un mensaje corto con el número de jugador. */
static void Controles_DebugText(ControlesCanal_t *channel,
                                const char *prefix,
                                const char *result)
{
  char message[32];
  uint8_t length = 0U;

  if (debug_uart == 0)
  {
    return;
  }

  while (*prefix != '\0' && length < (uint8_t)(sizeof(message) - 2U))
  {
    message[length++] = *prefix++;
  }
  message[length++] = 'J';
  message[length++] = (char)('0' + channel->expected_id);
  message[length++] = ' ';
  while (*result != '\0' && length < (uint8_t)(sizeof(message) - 2U))
  {
    message[length++] = *result++;
  }
  message[length++] = '\r';
  message[length++] = '\n';

  (void)HAL_UART_Transmit(debug_uart, (uint8_t *)message, length, 20U);
}

/* Guarda bytes crudos sin frenar la interrupción. */
static void Controles_QueueRawByte(ControlesCanal_t *channel, uint8_t byte)
{
  uint8_t next_head = (uint8_t)((channel->raw_debug_head + 1U) %
                                CONTROLES_RAW_QUEUE_DEPTH);

  if (next_head == channel->raw_debug_tail)
  {
    channel->diagnostics.raw_debug_dropped++;
    return;
  }

  channel->raw_debug_queue[channel->raw_debug_head] = byte;
  channel->raw_debug_head = next_head;
}

/* Saca un byte de depuración de forma segura. */
static uint8_t Controles_TakeRawByte(ControlesCanal_t *channel, uint8_t *byte)
{
  uint32_t primask = __get_PRIMASK();
  uint8_t has_byte = 0U;

  __disable_irq();
  if (channel->raw_debug_tail != channel->raw_debug_head)
  {
    *byte = channel->raw_debug_queue[channel->raw_debug_tail];
    channel->raw_debug_tail = (uint8_t)((channel->raw_debug_tail + 1U) %
                                        CONTROLES_RAW_QUEUE_DEPTH);
    has_byte = 1U;
  }
  if (primask == 0U)
  {
    __enable_irq();
  }

  return has_byte;
}

/* Saca los bytes antes de validar sincronizacion, ID o CRC. */
static void Controles_DebugRaw(ControlesCanal_t *channel, uint32_t now_ms)
{
  char message[64];
  uint8_t byte;

  if (debug_uart == 0 ||
      (now_ms - channel->debug_raw_last_ms) < CONTROLES_DEBUG_RAW_PERIOD_MS)
  {
    return;
  }
  channel->debug_raw_last_ms = now_ms;

  while (channel->raw_debug_tail != channel->raw_debug_head)
  {
    uint8_t length = 0U;
    uint8_t count = 0U;

    message[length++] = 'R';
    message[length++] = 'A';
    message[length++] = 'W';
    message[length++] = ' ';
    message[length++] = 'J';
    message[length++] = (char)('0' + channel->expected_id);

    while (count < 16U && Controles_TakeRawByte(channel, &byte) != 0U)
    {
      message[length++] = ' ';
      message[length++] = Controles_HexDigit((uint8_t)(byte >> 4U));
      message[length++] = Controles_HexDigit(byte);
      count++;
    }
    message[length++] = '\r';
    message[length++] = '\n';
    (void)HAL_UART_Transmit(debug_uart, (uint8_t *)message, length, 20U);
  }
}

/* Muestra la trama cuando cambia y periodicamente confirma que sigue llegando. */
static void Controles_DebugFrame(ControlesCanal_t *channel,
                                 const uint8_t *frame,
                                 uint32_t received_ms,
                                 const char *result,
                                 uint8_t force)
{
  char message[32];
  uint8_t length = 0U;
  uint8_t index;

  if (debug_uart == 0)
  {
    return;
  }

  if (force == 0U && channel->debug_has_frame != 0U &&
      channel->debug_last_buttons == frame[2] &&
      channel->debug_last_status == frame[3] &&
      (received_ms - channel->debug_last_ms) < CONTROLES_DEBUG_REPEAT_MS)
  {
    return;
  }

  message[length++] = 'J';
  message[length++] = (char)('0' + channel->expected_id);
  for (index = 0U; index < CONTROLES_FRAME_SIZE; index++)
  {
    message[length++] = ' ';
    message[length++] = Controles_HexDigit((uint8_t)(frame[index] >> 4U));
    message[length++] = Controles_HexDigit(frame[index]);
  }
  message[length++] = ' ';
  while (*result != '\0' && length < (uint8_t)(sizeof(message) - 2U))
  {
    message[length++] = *result++;
  }
  message[length++] = '\r';
  message[length++] = '\n';

  (void)HAL_UART_Transmit(debug_uart, (uint8_t *)message, length, 20U);

  if (force == 0U)
  {
    channel->debug_has_frame = 1U;
    channel->debug_last_buttons = frame[2];
    channel->debug_last_status = frame[3];
    channel->debug_last_ms = received_ms;
  }
}

/* Guarda una trama completa para procesarla fuera de la interrupción. */
static void Controles_QueueFrame(ControlesCanal_t *channel)
{
  uint8_t next_head =
      (uint8_t)((channel->queue_head + 1U) % CONTROLES_QUEUE_DEPTH);

  if (next_head == channel->queue_tail)
  {
    channel->queue_tail =
        (uint8_t)((channel->queue_tail + 1U) % CONTROLES_QUEUE_DEPTH);
    channel->diagnostics.overwritten_frames++;
  }

  memcpy(channel->frame_queue[channel->queue_head],
         channel->rx_frame,
         CONTROLES_FRAME_SIZE);
  channel->frame_time_queue[channel->queue_head] = HAL_GetTick();
  channel->queue_head = next_head;
}

/* Se llama desde la interrupcion y reconstruye tramas usando 0xA5. */
static void Controles_PushByte(ControlesCanal_t *channel, uint8_t byte)
{
  if (channel->rx_index == 0U)
  {
    if (byte == CONTROLES_SYNC)
    {
      channel->rx_frame[0] = byte;
      channel->rx_index = 1U;
    }
    return;
  }

  if (byte == CONTROLES_SYNC &&
      channel->rx_index < (CONTROLES_FRAME_SIZE - 1U))
  {
    channel->rx_frame[0] = byte;
    channel->rx_index = 1U;
    channel->diagnostics.sync_restarts++;
    return;
  }

  channel->rx_frame[channel->rx_index++] = byte;
  if (channel->rx_index == CONTROLES_FRAME_SIZE)
  {
    Controles_QueueFrame(channel);
    channel->rx_index = 0U;
  }
}

/* Extrae la trama más antigua de la cola. */
static uint8_t Controles_TakeFrame(ControlesCanal_t *channel,
                                   uint8_t *frame,
                                   uint32_t *received_ms)
{
  uint32_t primask = __get_PRIMASK();
  uint8_t has_frame = 0U;

  __disable_irq();
  if (channel->queue_tail != channel->queue_head)
  {
    memcpy(frame,
           channel->frame_queue[channel->queue_tail],
           CONTROLES_FRAME_SIZE);
    *received_ms = channel->frame_time_queue[channel->queue_tail];
    channel->queue_tail =
        (uint8_t)((channel->queue_tail + 1U) % CONTROLES_QUEUE_DEPTH);
    has_frame = 1U;
  }
  if (primask == 0U)
  {
    __enable_irq();
  }

  return has_frame;
}

/* Valida la trama y actualiza botones, conexión y eventos. */
static void Controles_ApplyFrame(ControlesCanal_t *channel,
                                 const uint8_t *frame,
                                 uint32_t received_ms)
{
  uint8_t new_buttons;

  if (frame[0] != CONTROLES_SYNC ||
      (frame[2] & (uint8_t)~CONTROLES_BUTTON_MASK) != 0U ||
      (frame[3] != CONTROLES_STATUS_CONNECTED &&
       frame[3] != CONTROLES_STATUS_DISCONNECTED))
  {
    Controles_DebugFrame(channel, frame, received_ms, "FMT", 1U);
    channel->diagnostics.format_errors++;
    return;
  }
  if (frame[4] != Controles_Crc8(frame, 4U))
  {
    Controles_DebugFrame(channel, frame, received_ms, "CRC", 1U);
    channel->diagnostics.crc_errors++;
    return;
  }
  if (frame[1] != channel->expected_id)
  {
    Controles_DebugFrame(channel, frame, received_ms, "ID", 1U);
    channel->diagnostics.id_errors++;
    return;
  }

  Controles_DebugFrame(channel, frame, received_ms, "OK", 0U);
  channel->diagnostics.valid_frames++;
  channel->has_valid_frame = 1U;
  channel->last_valid_ms = received_ms;

  if (frame[3] == CONTROLES_STATUS_DISCONNECTED)
  {
    channel->connected = 0U;
    channel->buttons = 0U;
    channel->pressed_events = 0U;
    return;
  }

  new_buttons = (uint8_t)(frame[2] & CONTROLES_BUTTON_MASK);
  /* Sólo genera eventos para botones que acaban de presionarse. */
  channel->pressed_events |=
      (uint8_t)(new_buttons & (uint8_t)~channel->buttons);
  channel->buttons = new_buttons;
  channel->connected = 1U;
}

/* Procesa tramas pendientes y libera botones por tiempo agotado. */
static void Controles_UpdateChannel(ControlesCanal_t *channel,
                                    uint32_t now_ms)
{
  uint8_t frame[CONTROLES_FRAME_SIZE];
  uint32_t received_ms;

  Controles_DebugRaw(channel, now_ms);

  while (Controles_TakeFrame(channel, frame, &received_ms) != 0U)
  {
    Controles_ApplyFrame(channel, frame, received_ms);
  }

  if (channel->has_valid_frame != 0U &&
      (channel->buttons != 0U || channel->pressed_events != 0U) &&
      (now_ms - channel->last_valid_ms) >= CONTROLES_STALE_TIMEOUT_MS)
  {
    channel->buttons = 0U;
    channel->pressed_events = 0U;
    channel->diagnostics.timeout_releases++;
  }
}

/* Deja armada la recepción del siguiente byte. */
static HAL_StatusTypeDef Controles_StartReception(ControlesCanal_t *channel)
{
  HAL_StatusTypeDef status =
      HAL_UART_Receive_IT(channel->uart, &channel->rx_byte, 1U);

  if (status != HAL_OK)
  {
    channel->diagnostics.uart_errors++;
  }

  return status;
}

/* Asigna una UART a cada jugador e inicia ambas recepciones. */
void Controles_Init(void)
{
  HAL_StatusTypeDef player_1_status;
  HAL_StatusTypeDef player_2_status;

  memset(controls, 0, sizeof(controls));

  controls[0].uart = &huart5;
  controls[0].expected_id = CONTROLES_JUGADOR_1;
  controls[1].uart = &huart3;
  controls[1].expected_id = CONTROLES_JUGADOR_2;

  player_1_status = Controles_StartReception(&controls[0]);
  player_2_status = Controles_StartReception(&controls[1]);

  Controles_DebugText(&controls[0], "UART ",
                      player_1_status == HAL_OK ? "ARM OK" : "ARM ERROR");
  Controles_DebugText(&controls[1], "UART ",
                      player_2_status == HAL_OK ? "ARM OK" : "ARM ERROR");
}

/* Selecciona la UART usada únicamente para diagnóstico. */
void Controles_SetDebugUart(UART_HandleTypeDef *uart)
{
  static const uint8_t ready_message[] = "celeste\r\n";

  debug_uart = uart;
  if (debug_uart != 0)
  {
    (void)HAL_UART_Transmit(debug_uart,
                            (uint8_t *)ready_message,
                            (uint16_t)(sizeof(ready_message) - 1U),
                            20U);
  }
}

/* Actualiza los dos controles desde el ciclo principal. */
void Controles_Update(uint32_t now_ms)
{
  Controles_UpdateChannel(&controls[0], now_ms);
  Controles_UpdateChannel(&controls[1], now_ms);
}

/* Recibe un byte, lo guarda y vuelve a armar la interrupción. */
void Controles_UART_RxCpltCallback(UART_HandleTypeDef *uart)
{
  ControlesCanal_t *channel = Controles_GetUartChannel(uart);

  if (channel == 0)
  {
    return;
  }

  channel->diagnostics.rx_bytes++;
  Controles_QueueRawByte(channel, channel->rx_byte);
  Controles_PushByte(channel, channel->rx_byte);
  (void)Controles_StartReception(channel);
}

/* Reinicia la recepción después de un error de UART. */
void Controles_UART_ErrorCallback(UART_HandleTypeDef *uart)
{
  ControlesCanal_t *channel = Controles_GetUartChannel(uart);

  if (channel == 0)
  {
    return;
  }

  channel->diagnostics.uart_errors++;
  channel->rx_index = 0U;
  (void)Controles_StartReception(channel);
}

/* Indica si un botón continúa mantenido. */
uint8_t Controles_Held(uint8_t player, uint8_t mask)
{
  ControlesCanal_t *channel = Controles_GetChannel(player);

  return (channel != 0 && (channel->buttons & mask) != 0U) ? 1U : 0U;
}

/* Entrega una pulsación una sola vez y luego la borra. */
uint8_t Controles_TakePressed(uint8_t player, uint8_t mask)
{
  ControlesCanal_t *channel = Controles_GetChannel(player);
  uint8_t pressed;

  if (channel == 0)
  {
    return 0U;
  }

  pressed = (uint8_t)(channel->pressed_events & mask);
  channel->pressed_events &= (uint8_t)~mask;
  return (pressed != 0U) ? 1U : 0U;
}

/* Devuelve el último estado de conexión válido. */
uint8_t Controles_IsConnected(uint8_t player)
{
  ControlesCanal_t *channel = Controles_GetChannel(player);

  return (channel != 0) ? channel->connected : 0U;
}

/* Expone contadores útiles para revisar la comunicación. */
const ControlesDiagnostico_t *Controles_GetDiagnostics(uint8_t player)
{
  ControlesCanal_t *channel = Controles_GetChannel(player);

  return (channel != 0) ? &channel->diagnostics : 0;
}
