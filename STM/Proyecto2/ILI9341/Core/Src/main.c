/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "fatfs.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <math.h>
#include <string.h>

#include "controles_jugadores.h"
#include "graficos.h"
#include "ili9341.h"
#include "level_1_texture.h"
#include "nieve.h"
#include "start_background.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* Formatos usados por los archivos de textura. */
typedef enum
{
  GAME_TEXTURE_PACKED_PAIRS = 0,
  GAME_TEXTURE_RGB565_VALUES
} GameTextureEncoding_t;

/* Etapa en la que puede fallar la carga de un nivel. */
typedef enum
{
  GAME_LOAD_ERROR_NONE = 0,
  GAME_LOAD_ERROR_LEVEL,
  GAME_LOAD_ERROR_MOUNT,
  GAME_LOAD_ERROR_BACKGROUND_OPEN,
  GAME_LOAD_ERROR_OVERLAY_OPEN,
  GAME_LOAD_ERROR_FLASH_ERASE,
  GAME_LOAD_ERROR_BACKGROUND_READ,
  GAME_LOAD_ERROR_OVERLAY_READ,
  GAME_LOAD_ERROR_FLASH_WRITE,
  GAME_LOAD_ERROR_FLASH_VERIFY
} GameLoadError_t;

/* Lector pequeño para no guardar el archivo completo en RAM. */
typedef struct
{
  FIL *file;
  uint8_t buffer[512U];
  UINT position;
  UINT length;
  FRESULT result;
} GameTextReader_t;

/* Datos necesarios para registrar un nivel. */
typedef struct
{
  const char *short_name;
  const char *original_name;
  uint16_t playfield_height;
  uint16_t player_start_y;
  GameTextureEncoding_t encoding;
} GameLevelFile_t;

/* Estado de movimiento, saltos y animación de un jugador. */
typedef struct
{
  uint8_t controller_id;
  const uint16_t *sprite_sheet;
  const uint16_t *jump_sprite;
  uint16_t sprite_sheet_width;
  uint8_t sprite_frame_count;
  uint8_t animation_frame;
  uint8_t animation_updates;
  uint8_t facing_left;
  uint8_t jump_count;
  uint8_t jump_buffer_frames;
  uint8_t wall_jump_lock_frames;
  uint8_t was_on_ground;
  uint8_t jump_limit_active;
  int8_t wall_jump_direction;
  uint16_t x;
  uint16_t y;
  uint16_t jump_apex_y;
  int32_t x_fixed;
  int32_t y_fixed;
  int32_t velocity_x;
  int32_t velocity_y;
} GamePlayer_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* Posición y parpadeo del aviso de inicio. */
#define START_PROMPT_BOX_X 16U
#define START_PROMPT_BOX_Y 268U
#define START_PROMPT_BOX_WIDTH 208U
#define START_PROMPT_BOX_HEIGHT 24U
#define START_PROMPT_TEXT_X 28U
#define START_PROMPT_TEXT_Y 274U
#define START_PROMPT_BLINK_MS 250U
#define START_PROMPT_TEXT "PRESIONA START"

/* Tamaño de colisión y ajuste del sprite sobre el jugador. */
#define GAME_PLAYER_WIDTH 18U
#define GAME_PLAYER_HEIGHT 14U
#define GAME_PLAYER_SPRITE_TRANSPARENT_COLOR 0x07E5U
#define GAME_PLAYER_SPRITE_OFFSET_X 1U
#define GAME_PLAYER_SPRITE_OFFSET_Y 2U
#define GAME_PLAYER_ANIMATION_STEP_UPDATES 4U
#define GAME_PLAYER_START_X 15U
#define GAME_PLAYER_START_Y 170U
#define GAME_PLAYER_2_START_X 75U
#define GAME_PLAYER_COUNT 2U

/* La física usa 256 unidades internas por cada píxel. */
#define GAME_UPDATE_MS 20U
#define GAME_FIXED_POINT_ONE 256L
/* Usa multiplicadores con sufijo f y redondea al entero más cercano. */
#define GAME_SCALE_ROUNDED(base, multiplier) \
  ((int32_t)(((float)(base) * (multiplier)) + 0.5f))
#define GAME_PLAYER_X_MAX (ILI9341_WIDTH - GAME_PLAYER_WIDTH)
#define GAME_PLAYER_Y_MAX (ILI9341_HEIGHT - GAME_PLAYER_HEIGHT)
#define GAME_PLAYER_START_X_FIXED \
  ((int32_t)GAME_PLAYER_START_X * GAME_FIXED_POINT_ONE)
#define GAME_PLAYER_START_Y_FIXED \
  ((int32_t)GAME_PLAYER_START_Y * GAME_FIXED_POINT_ONE)

/* Ajustes del movimiento horizontal. */
#define GAME_HORIZONTAL_MAX_SPEED_MULTIPLIER 2.0f
#define GAME_HORIZONTAL_MAX_SPEED \
  GAME_SCALE_ROUNDED(GAME_FIXED_POINT_ONE, \
                     GAME_HORIZONTAL_MAX_SPEED_MULTIPLIER)
#define GAME_HORIZONTAL_GROUND_ACCEL 180L
#define GAME_HORIZONTAL_AIR_ACCEL 180L
#define GAME_HORIZONTAL_DECEL 40L

/* Estados y alturas disponibles para los saltos. */
#define GAME_JUMP_COUNT_NORMAL_READY 0U
#define GAME_JUMP_COUNT_SECOND_READY 1U
#define GAME_JUMP_COUNT_EXHAUSTED 2U
#define GAME_JUMP_HEIGHT_MULTIPLIER 2.5f
#define GAME_SECOND_JUMP_HEIGHT_MULTIPLIER 3.0f
#define GAME_JUMP_HEIGHT \
  ((uint16_t)GAME_SCALE_ROUNDED(GAME_PLAYER_HEIGHT, \
                                GAME_JUMP_HEIGHT_MULTIPLIER))
#define GAME_SECOND_JUMP_HEIGHT \
  ((uint16_t)GAME_SCALE_ROUNDED(GAME_PLAYER_HEIGHT, \
                                GAME_SECOND_JUMP_HEIGHT_MULTIPLIER))
#define GAME_SECOND_JUMP_SIDE_SPEED_MULTIPLIER 3.0f
#define GAME_SECOND_JUMP_DIAGONAL_SPEED_MULTIPLIER 2.0f
#define GAME_SECOND_JUMP_SIDE_SPEED \
  GAME_SCALE_ROUNDED(GAME_FIXED_POINT_ONE, \
                     GAME_SECOND_JUMP_SIDE_SPEED_MULTIPLIER)
#define GAME_SECOND_JUMP_DIAGONAL_SPEED \
  GAME_SCALE_ROUNDED(GAME_FIXED_POINT_ONE, \
                     GAME_SECOND_JUMP_DIAGONAL_SPEED_MULTIPLIER)

/* Ajustes de caída, resbalamiento y salto de pared. */
#define GAME_GRAVITY_BASE 32L
#define GAME_APEX_GRAVITY_BASE 16L
#define GAME_GRAVITY_MULTIPLIER 2.0f
#define GAME_GRAVITY \
  GAME_SCALE_ROUNDED(GAME_GRAVITY_BASE, GAME_GRAVITY_MULTIPLIER)
#define GAME_APEX_GRAVITY \
  GAME_SCALE_ROUNDED(GAME_APEX_GRAVITY_BASE, GAME_GRAVITY_MULTIPLIER)
#define GAME_APEX_SPEED 48L
#define GAME_MAX_FALL_SPEED (1000L)
#define GAME_WALL_SLIDE_SPEED_MULTIPLIER 0.55f
#define GAME_WALL_SLIDE_MAX_FALL_SPEED \
  GAME_SCALE_ROUNDED(GAME_MAX_FALL_SPEED, \
                     GAME_WALL_SLIDE_SPEED_MULTIPLIER)
#define GAME_WALL_SLIDE_DECEL 200L
#define GAME_WALL_JUMP_HEIGHT_MULTIPLIER 2.5f
#define GAME_WALL_JUMP_HEIGHT \
  ((uint16_t)GAME_SCALE_ROUNDED(GAME_PLAYER_HEIGHT, \
                                GAME_WALL_JUMP_HEIGHT_MULTIPLIER))
#define GAME_WALL_JUMP_SIDE_SPEED_MULTIPLIER 2.5f
#define GAME_WALL_JUMP_SIDE_SPEED \
  GAME_SCALE_ROUNDED(GAME_FIXED_POINT_ONE, \
                     GAME_WALL_JUMP_SIDE_SPEED_MULTIPLIER)
#define GAME_JUMP_BUFFER_FRAMES 4U
#define GAME_WALL_JUMP_LOCK_FRAMES 5U

/* Colores y posiciones del marcador. */
#define GAME_LEVEL_1 1U
#define GAME_HUD_COLOR 0x5E7BU
#define GAME_PLAYER_1_COLOR 0xF809U
#define GAME_PLAYER_2_COLOR 0x6052U
#define GAME_HUD_TEXT_COLOR 0x0000U
#define GAME_HUD_PLAYER_1_X 60U
#define GAME_HUD_PLAYER_2_X 180U
#define GAME_HUD_CIRCLE_RADIUS 18U
#define GAME_HUD_LABEL_Y_OFFSET 4U
#define GAME_HUD_CIRCLE_Y_OFFSET 36U
#define GAME_HUD_SCORE_Y_OFFSET 65U
#define GAME_RESULT_PANEL_X 24U
#define GAME_RESULT_PANEL_Y 100U
#define GAME_RESULT_PANEL_WIDTH 192U
#define GAME_RESULT_PANEL_HEIGHT 132U
#define GAME_RESULT_CIRCLE_Y 169U
#define GAME_RESULT_CIRCLE_RADIUS 24U
#define GAME_RESULT_TIE_CIRCLE_RADIUS 20U
#define GAME_RESULT_TEXT_COLOR 0xFFFFU
#define GAME_RESULT_BACKGROUND_COLOR 0x0000U

/* Colores RGB565 equivalentes a #FA0300 y #FFEF00. */
#define GAME_LEVEL_HAZARD_COLOR 0xF800U
#define GAME_LEVEL_EXIT_COLOR 0xFF60U
#define GAME_LEVEL_HAZARD_REACH 1U
#define GAME_LEVEL_EXIT_REACH 2U
#define GAME_SD_BACKGROUND_FILE "0:/FONDOGEN.TXT"
#define GAME_SD_BACKGROUND_FILE_ORIGINAL "0:/Fondogeneral.txt"
#define GAME_SD_LEVEL_1_FILE "0:/LEVEL1~1.TXT"
#define GAME_SD_LEVEL_1_FILE_ORIGINAL "0:/level1_texture.txt"
#define GAME_SD_LEVEL_2_FILE "0:/NIVEL2.TXT"
#define GAME_SD_LEVEL_2_FILE_ORIGINAL "0:/Nivel2.txt"
#define GAME_SD_LEVEL_3_FILE "0:/NIVEL3.TXT"
#define GAME_SD_LEVEL_3_FILE_ORIGINAL "0:/Nivel3.txt"
#define GAME_PACKED_WORDS_PER_ROW (ILI9341_WIDTH / 2U)
#define GAME_LEVEL_FLASH_FIRST_SECTOR FLASH_SECTOR_6
#define GAME_LEVEL_FLASH_SECTOR_COUNT 2U
#define GAME_LEVEL_FLASH_PIXEL_COUNT \
  ((uint32_t)ILI9341_WIDTH * ILI9341_HEIGHT)
#define GAME_LEVEL_FLASH_BYTE_COUNT \
  (GAME_LEVEL_FLASH_PIXEL_COUNT * sizeof(uint16_t))
#define GAME_LEVEL_HASH_INITIAL 2166136261UL
#define GAME_LEVEL_HASH_PRIME 16777619UL

/* Cada píxel ocupa dos bits dentro del mapa de colisiones. */
#define GAME_COLLISION_BITS_PER_PIXEL 2U
#define GAME_COLLISION_PIXELS_PER_BYTE 4U
#define GAME_COLLISION_MAP_SIZE \
  ((ILI9341_WIDTH * ILI9341_HEIGHT) / GAME_COLLISION_PIXELS_PER_BYTE)
#define GAME_COLLISION_FREE 0U
#define GAME_COLLISION_SOLID 1U
#define GAME_COLLISION_HAZARD 2U
#define GAME_COLLISION_EXIT 3U

/* Eventos que puede devolver la actualización de un jugador. */
#define GAME_PLAYER_EVENT_NONE 0U
#define GAME_PLAYER_EVENT_RESPAWN (1U << 0)
#define GAME_PLAYER_EVENT_EXIT (1U << 1)

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart5;
UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;

/* USER CODE BEGIN PV */
ILI9341_t lcd;
Nieve_t nieve;

/* Tabla usada para añadir niveles sin cambiar la lógica principal. */
static uint8_t game_current_level = GAME_LEVEL_1;
static const GameLevelFile_t game_level_files[] = {
    {GAME_SD_LEVEL_1_FILE,
     GAME_SD_LEVEL_1_FILE_ORIGINAL,
     236U,
     GAME_PLAYER_START_Y,
     GAME_TEXTURE_PACKED_PAIRS},
    {GAME_SD_LEVEL_2_FILE,
     GAME_SD_LEVEL_2_FILE_ORIGINAL,
     235U,
     207U,
     GAME_TEXTURE_PACKED_PAIRS},
    {GAME_SD_LEVEL_3_FILE,
     GAME_SD_LEVEL_3_FILE_ORIGINAL,
     235U,
     192U,
     GAME_TEXTURE_RGB565_VALUES},
};
#define GAME_LEVEL_COUNT \
  ((uint8_t)(sizeof(game_level_files) / sizeof(game_level_files[0])))
static FIL game_background_file;
static FIL game_overlay_file;
static uint8_t game_background_file_open = 0U;
static uint8_t game_overlay_file_open = 0U;
static uint8_t game_sd_mounted = 0U;
static uint8_t game_level_ready = 0U;
static GameLoadError_t game_load_error = GAME_LOAD_ERROR_NONE;
static uint16_t game_load_error_row = 0U;
static GameTextReader_t game_background_reader;
static GameTextReader_t game_overlay_reader;
static uint16_t game_background_row[ILI9341_WIDTH];
static uint16_t game_overlay_row[ILI9341_WIDTH];
static uint16_t game_composed_row[ILI9341_WIDTH];
static uint8_t game_collision_map[GAME_COLLISION_MAP_SIZE];
static uint16_t game_scores[GAME_PLAYER_COUNT];

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_UART5_Init(void);
/* USER CODE BEGIN PFP */
extern uint8_t __level_cache_start__;
extern uint8_t __level_cache_end__;

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* Dibuja el aviso que parpadea en la pantalla inicial. */
static void StartScreen_DrawPrompt(void)
{
  ILI9341_FillRect(&lcd,
                   START_PROMPT_BOX_X,
                   START_PROMPT_BOX_Y,
                   START_PROMPT_BOX_WIDTH,
                   START_PROMPT_BOX_HEIGHT,
                   0x0000U);
  ILI9341_DrawText(&lcd,
                   START_PROMPT_TEXT,
                   START_PROMPT_TEXT_X,
                   START_PROMPT_TEXT_Y,
                   0xFFFFU,
                   0x0000U);
}

/* Recupera sólo el área ocupada por el aviso. */
static void StartScreen_RestorePromptBackground(void)
{
  const uint16_t *background_region =
      &start_background[START_PROMPT_BOX_Y * START_BACKGROUND_WIDTH +
                        START_PROMPT_BOX_X];

  ILI9341_DrawBitmapRegion(&lcd,
                            START_PROMPT_BOX_X,
                            START_PROMPT_BOX_Y,
                            START_PROMPT_BOX_WIDTH,
                            START_PROMPT_BOX_HEIGHT,
                            background_region,
                            START_BACKGROUND_WIDTH);
}

/* Obtiene los datos del nivel usando números desde 1. */
static const GameLevelFile_t *Game_GetLevelFile(uint8_t level)
{
  if (level < GAME_LEVEL_1 || level > GAME_LEVEL_COUNT)
  {
    return 0;
  }

  return &game_level_files[level - GAME_LEVEL_1];
}

/* Convierte un puntaje sin usar printf. */
static uint8_t Game_Uint16ToText(uint16_t value, char *text)
{
  char reverse[5];
  uint8_t length = 0U;
  uint8_t index;

  do
  {
    reverse[length++] = (char)('0' + (value % 10U)); /* Sale al revés. */
    value /= 10U;
  } while (value != 0U && length < sizeof(reverse));

  for (index = 0U; index < length; index++)
  {
    text[index] = reverse[length - index - 1U];
  }
  text[length] = '\0';
  return length;
}

/* Une el valor numérico con el texto del resultado. */
static void Game_BuildPointsText(uint16_t score, char *text)
{
  static const char suffix[] = " PUNTOS";
  uint8_t length = Game_Uint16ToText(score, text);
  uint8_t index;

  for (index = 0U; index < sizeof(suffix); index++)
  {
    text[length + index] = suffix[index];
  }
}

/* Centra texto de la fuente básica de 8 píxeles. */
static void Game_DrawCenteredText(const char *text,
                                  uint16_t y,
                                  uint16_t color,
                                  uint16_t background)
{
  uint16_t width = (uint16_t)strlen(text) * 8U; /* 8 px por carácter. */
  uint16_t x = (width < ILI9341_WIDTH)
                   ? (uint16_t)((ILI9341_WIDTH - width) / 2U)
                   : 0U;

  ILI9341_DrawText(&lcd, text, x, y, color, background);
}

/* Traduce el código de error a un mensaje corto. */
static const char *Game_LoadErrorText(GameLoadError_t error)
{
  switch (error)
  {
    case GAME_LOAD_ERROR_LEVEL:
      return "NIVEL INVALIDO";
    case GAME_LOAD_ERROR_MOUNT:
      return "MONTAR SD";
    case GAME_LOAD_ERROR_BACKGROUND_OPEN:
      return "ABRIR FONDO";
    case GAME_LOAD_ERROR_OVERLAY_OPEN:
      return "ABRIR NIVEL";
    case GAME_LOAD_ERROR_FLASH_ERASE:
      return "BORRAR FLASH";
    case GAME_LOAD_ERROR_BACKGROUND_READ:
      return "LEER FONDO";
    case GAME_LOAD_ERROR_OVERLAY_READ:
      return "LEER NIVEL";
    case GAME_LOAD_ERROR_FLASH_WRITE:
      return "ESCRIBIR FLASH";
    case GAME_LOAD_ERROR_FLASH_VERIFY:
      return "VERIFICAR FLASH";
    default:
      return "ERROR DESCONOCIDO";
  }
}

/* Muestra la etapa exacta para localizar fallos de SD o Flash. */
static void Game_DrawLoadError(void)
{
  char row_text[10] = "FILA ";
  uint8_t show_row = 0U;

  ILI9341_Clear(&lcd, 0x0000U);
  Game_DrawCenteredText("ERROR AL CARGAR",
                        126U,
                        0xF800U,
                        0x0000U);
  Game_DrawCenteredText(Game_LoadErrorText(game_load_error),
                        150U,
                        0xFFFFU,
                        0x0000U);

  if (game_load_error == GAME_LOAD_ERROR_BACKGROUND_READ ||
      game_load_error == GAME_LOAD_ERROR_OVERLAY_READ ||
      game_load_error == GAME_LOAD_ERROR_FLASH_WRITE)
  {
    show_row = 1U;
  }

  if (show_row != 0U)
  {
    Game_Uint16ToText((uint16_t)(game_load_error_row + 1U),
                      &row_text[5]);
    Game_DrawCenteredText(row_text, 174U, 0xFFFFU, 0x0000U);
  }
}

/* Dibuja los dos marcadores debajo del area jugable. */
static void Game_DrawScoreboard(uint8_t level)
{
  const GameLevelFile_t *level_file = Game_GetLevelFile(level);
  char score_text[6];
  uint8_t length;
  uint16_t label_y;
  uint16_t circle_y;
  uint16_t score_y;

  if (level_file == 0)
  {
    return;
  }

  label_y = (uint16_t)(level_file->playfield_height +
                       GAME_HUD_LABEL_Y_OFFSET);
  circle_y = (uint16_t)(level_file->playfield_height +
                        GAME_HUD_CIRCLE_Y_OFFSET);
  score_y = (uint16_t)(level_file->playfield_height +
                       GAME_HUD_SCORE_Y_OFFSET);

  ILI9341_DrawText(&lcd,
                   "J1",
                   GAME_HUD_PLAYER_1_X - 8U,
                   label_y,
                   GAME_HUD_TEXT_COLOR,
                   GAME_HUD_COLOR);
  ILI9341_DrawText(&lcd,
                   "J2",
                   GAME_HUD_PLAYER_2_X - 8U,
                   label_y,
                   GAME_HUD_TEXT_COLOR,
                   GAME_HUD_COLOR);
  ILI9341_FillCircle(&lcd,
                     GAME_HUD_PLAYER_1_X,
                     circle_y,
                     GAME_HUD_CIRCLE_RADIUS,
                     GAME_PLAYER_1_COLOR);
  ILI9341_FillCircle(&lcd,
                     GAME_HUD_PLAYER_2_X,
                     circle_y,
                     GAME_HUD_CIRCLE_RADIUS,
                     GAME_PLAYER_2_COLOR);

  length = Game_Uint16ToText(game_scores[0], score_text);
  ILI9341_DrawText(&lcd,
                   score_text,
                   (uint16_t)(GAME_HUD_PLAYER_1_X - length * 4U),
                   score_y,
                   GAME_HUD_TEXT_COLOR,
                   GAME_HUD_COLOR);
  length = Game_Uint16ToText(game_scores[1], score_text);
  ILI9341_DrawText(&lcd,
                   score_text,
                   (uint16_t)(GAME_HUD_PLAYER_2_X - length * 4U),
                   score_y,
                   GAME_HUD_TEXT_COLOR,
                   GAME_HUD_COLOR);
}

/* Esta capa se redibuja despues de la nieve para conservar el resultado. */
static void Game_DrawResultScreen(void)
{
  char points_text[13];

  ILI9341_FillRect(&lcd,
                   GAME_RESULT_PANEL_X,
                   GAME_RESULT_PANEL_Y,
                   GAME_RESULT_PANEL_WIDTH,
                   GAME_RESULT_PANEL_HEIGHT,
                   GAME_RESULT_BACKGROUND_COLOR);

  if (game_scores[0] == game_scores[1])
  {
    Game_DrawCenteredText("EMPATE",
                          GAME_RESULT_PANEL_Y + 12U,
                          GAME_RESULT_TEXT_COLOR,
                          GAME_RESULT_BACKGROUND_COLOR);
    ILI9341_FillCircle(&lcd,
                       84U,
                       GAME_RESULT_CIRCLE_Y,
                       GAME_RESULT_TIE_CIRCLE_RADIUS,
                       GAME_PLAYER_1_COLOR);
    ILI9341_FillCircle(&lcd,
                       156U,
                       GAME_RESULT_CIRCLE_Y,
                       GAME_RESULT_TIE_CIRCLE_RADIUS,
                       GAME_PLAYER_2_COLOR);
    Game_BuildPointsText(game_scores[0], points_text);
  }
  else
  {
    uint8_t winner = (game_scores[0] > game_scores[1]) ? 0U : 1U;

    Game_DrawCenteredText("GANA EL JUGADOR",
                          GAME_RESULT_PANEL_Y + 12U,
                          GAME_RESULT_TEXT_COLOR,
                          GAME_RESULT_BACKGROUND_COLOR);
    ILI9341_FillCircle(&lcd,
                       ILI9341_WIDTH / 2U,
                       GAME_RESULT_CIRCLE_Y,
                       GAME_RESULT_CIRCLE_RADIUS,
                       (winner == 0U) ? GAME_PLAYER_1_COLOR
                                      : GAME_PLAYER_2_COLOR);
    Game_BuildPointsText(game_scores[winner], points_text);
  }

  Game_DrawCenteredText(points_text,
                        GAME_RESULT_PANEL_Y + 106U,
                        GAME_RESULT_TEXT_COLOR,
                        GAME_RESULT_BACKGROUND_COLOR);
}

/* Acerca un valor al objetivo sin pasarse. */
static int32_t Game_Approach(int32_t value, int32_t target, int32_t amount)
{
  if (value < target)
  {
    value += amount;
    return (value > target) ? target : value;
  }
  if (value > target)
  {
    value -= amount;
    return (value < target) ? target : value;
  }

  return value;
}

/* Ajusta la posición del dibujo respecto al rectángulo de colisión. */
static uint16_t Game_PlayerSpriteX(uint16_t player_x)
{
  return (uint16_t)(player_x + GAME_PLAYER_SPRITE_OFFSET_X);
}

static uint16_t Game_PlayerSpriteY(uint16_t player_y)
{
  return (player_y >= GAME_PLAYER_SPRITE_OFFSET_Y)
             ? (uint16_t)(player_y - GAME_PLAYER_SPRITE_OFFSET_Y)
             : 0U;
}

/* Dibuja el sprite configurado para cada jugador. */
static void Game_DrawPlayer(const GamePlayer_t *player, uint8_t airborne)
{
  if (player == 0 || player->sprite_sheet == 0)
  {
    return;
  }

  if (airborne != 0U && player->jump_sprite != 0)
  {
    ILI9341_DrawSpriteFrameKeyed(
        &lcd,
        Game_PlayerSpriteX(player->x),
        Game_PlayerSpriteY(player->y),
        CELESTE_J1_JUMP_WIDTH,
        CELESTE_J1_JUMP_HEIGHT,
        player->jump_sprite,
        CELESTE_J1_JUMP_WIDTH,
        0U,
        GAME_PLAYER_SPRITE_TRANSPARENT_COLOR,
        player->facing_left);
    return;
  }

  ILI9341_DrawSpriteFrameKeyed(
      &lcd,
      Game_PlayerSpriteX(player->x),
      Game_PlayerSpriteY(player->y),
      CELESTE_J2_FRAME_WIDTH,
      CELESTE_J2_FRAME_HEIGHT,
      player->sprite_sheet,
      player->sprite_sheet_width,
      (airborne != 0U)
          ? 0U
          : (uint16_t)(player->animation_frame %
                       player->sprite_frame_count),
      GAME_PLAYER_SPRITE_TRANSPARENT_COLOR,
      player->facing_left);
}

/* Convierte un carácter hexadecimal a su valor numérico. */
static int8_t Game_HexDigit(char character)
{
  if (character >= '0' && character <= '9')
  {
    return (int8_t)(character - '0');
  }
  if (character >= 'a' && character <= 'f')
  {
    return (int8_t)(character - 'a' + 10);
  }
  if (character >= 'A' && character <= 'F')
  {
    return (int8_t)(character - 'A' + 10);
  }

  return -1;
}

/* Guarda bloques pequeños para leer archivos largos sin usar mucha RAM. */
static void Game_TextReaderInit(GameTextReader_t *reader, FIL *file)
{
  reader->file = file;
  reader->position = 0U;
  reader->length = 0U;
  reader->result = FR_OK;
}

static uint8_t Game_TextReaderGetChar(GameTextReader_t *reader,
                                      char *character)
{
  if (reader->position >= reader->length)
  {
    UINT bytes_read = 0U;

    reader->result = f_read(reader->file,
                            reader->buffer,
                            sizeof(reader->buffer),
                            &bytes_read);
    if (reader->result != FR_OK || bytes_read == 0U)
    {
      return 0U;
    }

    reader->position = 0U;
    reader->length = bytes_read;
  }

  *character = (char)reader->buffer[reader->position++];
  return 1U;
}

/* Lee el siguiente valor 0x... sin depender del largo de cada linea. */
static uint8_t Game_TextReaderNextHex(GameTextReader_t *reader,
                                      uint32_t *value,
                                      uint8_t max_digits)
{
  char character;
  uint8_t found_zero = 0U;
  uint8_t digit_count = 0U;
  int8_t digit;

  while (Game_TextReaderGetChar(reader, &character) != 0U)
  {
    if (found_zero != 0U && (character == 'x' || character == 'X'))
    {
      break;
    }
    found_zero = (character == '0') ? 1U : 0U;
  }

  if (found_zero == 0U || (character != 'x' && character != 'X'))
  {
    return 0U;
  }

  *value = 0U;
  while (Game_TextReaderGetChar(reader, &character) != 0U)
  {
    digit = Game_HexDigit(character);
    if (digit < 0)
    {
      break;
    }
    if (digit_count >= max_digits)
    {
      return 0U;
    }

    *value = (*value << 4U) | (uint32_t)digit;
    digit_count++;
  }

  return (digit_count != 0U) ? 1U : 0U;
}

static uint8_t Game_ReadNextRow(GameTextReader_t *reader,
                                uint16_t *pixels,
                                GameTextureEncoding_t encoding)
{
  uint16_t item_count = (encoding == GAME_TEXTURE_RGB565_VALUES)
                            ? ILI9341_WIDTH
                            : GAME_PACKED_WORDS_PER_ROW;
  uint8_t max_digits = (encoding == GAME_TEXTURE_RGB565_VALUES) ? 4U : 8U;
  uint16_t item_index;

  for (item_index = 0U; item_index < item_count; item_index++)
  {
    uint32_t value = 0U;

    if (Game_TextReaderNextHex(reader, &value, max_digits) == 0U)
    {
      return 0U;
    }

    if (encoding == GAME_TEXTURE_RGB565_VALUES)
    {
      pixels[item_index] = (uint16_t)value;
    }
    else
    {
      pixels[item_index * 2U] = (uint16_t)(value >> 16U);
      pixels[item_index * 2U + 1U] = (uint16_t)value;
    }
  }

  return 1U;
}

static uint8_t Game_OpenTextureFile(FIL *file,
                                    const char *short_name,
                                    const char *original_name)
{
  if (f_open(file, short_name, FA_READ | FA_OPEN_EXISTING) == FR_OK)
  {
    return 1U;
  }

  return (f_open(file, original_name, FA_READ | FA_OPEN_EXISTING) == FR_OK)
             ? 1U
             : 0U;
}

static void Game_CloseLevelFiles(void)
{
  if (game_background_file_open != 0U)
  {
    (void)f_close(&game_background_file);
    game_background_file_open = 0U;
  }
  if (game_overlay_file_open != 0U)
  {
    (void)f_close(&game_overlay_file);
    game_overlay_file_open = 0U;
  }
}

/* Descarta el nivel activo antes de cargar el siguiente. */
static void Game_UnloadLevel(void)
{
  Game_CloseLevelFiles();
  game_level_ready = 0U;
  memset(game_collision_map, 0, sizeof(game_collision_map));
}

/* Guarda un tipo de colisión dentro de sus dos bits. */
static void Game_SetCollision(uint16_t x, uint16_t y, uint8_t type)
{
  uint32_t pixel_index = (uint32_t)y * ILI9341_WIDTH + x; /* Índice lineal. */
  uint32_t byte_index = pixel_index / GAME_COLLISION_PIXELS_PER_BYTE;
  uint8_t shift = (uint8_t)((pixel_index % GAME_COLLISION_PIXELS_PER_BYTE) *
                            GAME_COLLISION_BITS_PER_PIXEL);
  uint8_t mask = (uint8_t)(0x03U << shift);

  game_collision_map[byte_index] =
      (uint8_t)((game_collision_map[byte_index] & (uint8_t)~mask) |
                ((type & 0x03U) << shift));
}

/* Recupera los dos bits asignados a un píxel. */
static uint8_t Game_GetCollision(uint16_t x, uint16_t y)
{
  uint32_t pixel_index = (uint32_t)y * ILI9341_WIDTH + x;
  uint32_t byte_index = pixel_index / GAME_COLLISION_PIXELS_PER_BYTE;
  uint8_t shift = (uint8_t)((pixel_index % GAME_COLLISION_PIXELS_PER_BYTE) *
                            GAME_COLLISION_BITS_PER_PIXEL);

  return (uint8_t)((game_collision_map[byte_index] >> shift) & 0x03U);
}

/* Convierte el color de la textura a un tipo de colisión. */
static uint8_t Game_CollisionTypeForColor(uint16_t color)
{
  if (color == LEVEL_1_TEXTURE_TRANSPARENT_COLOR)
  {
    return GAME_COLLISION_FREE;
  }
  if (color == GAME_LEVEL_HAZARD_COLOR)
  {
    return GAME_COLLISION_HAZARD;
  }
  if (color == GAME_LEVEL_EXIT_COLOR)
  {
    return GAME_COLLISION_EXIT;
  }

  return GAME_COLLISION_SOLID;
}

/* Combina fondo, nivel y marcador una fila a la vez. */
static void Game_ComposeRow(uint16_t y, uint16_t playfield_height)
{
  uint16_t x;

  if (y >= playfield_height)
  {
    for (x = 0U; x < ILI9341_WIDTH; x++)
    {
      game_composed_row[x] = GAME_HUD_COLOR;
      Game_SetCollision(x, y, GAME_COLLISION_SOLID);
    }
    return;
  }

  for (x = 0U; x < ILI9341_WIDTH; x++)
  {
    uint16_t overlay_color = game_overlay_row[x];

    game_composed_row[x] =
        (overlay_color == LEVEL_1_TEXTURE_TRANSPARENT_COLOR)
            ? game_background_row[x]
            : overlay_color;
    Game_SetCollision(x, y, Game_CollisionTypeForColor(overlay_color));
  }
}

/* Dirección inicial de la imagen guardada en Flash. */
static const uint16_t *Game_LevelFlashPixels(void)
{
  return (const uint16_t *)(uintptr_t)&__level_cache_start__;
}

/* Comprueba que la zona reservada alcance para 240 x 320. */
static uint8_t Game_LevelFlashHasCapacity(void)
{
  uintptr_t start = (uintptr_t)&__level_cache_start__;
  uintptr_t end = (uintptr_t)&__level_cache_end__;

  return (end >= start && (end - start) >= GAME_LEVEL_FLASH_BYTE_COUNT)
             ? 1U
             : 0U;
}

/* Borra únicamente los sectores reservados para la imagen del nivel. */
static uint8_t Game_LevelFlashBeginWrite(void)
{
  FLASH_EraseInitTypeDef erase = {0};
  uint32_t sector_error = 0xFFFFFFFFU;

  if (Game_LevelFlashHasCapacity() == 0U || HAL_FLASH_Unlock() != HAL_OK)
  {
    return 0U;
  }

  erase.TypeErase = FLASH_TYPEERASE_SECTORS;
  erase.Sector = GAME_LEVEL_FLASH_FIRST_SECTOR;
  erase.NbSectors = GAME_LEVEL_FLASH_SECTOR_COUNT;
  erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;

  if (HAL_FLASHEx_Erase(&erase, &sector_error) != HAL_OK ||
      sector_error != 0xFFFFFFFFU)
  {
    (void)HAL_FLASH_Lock();
    return 0U;
  }

  return 1U;
}

/* Bloquea la Flash y actualiza su caché de datos. */
static void Game_LevelFlashEndWrite(void)
{
  (void)HAL_FLASH_Lock();

  /* Descarta datos del nivel anterior que aún pudieran estar en caché. */
  if ((FLASH->ACR & FLASH_ACR_DCEN) != 0U)
  {
    __HAL_FLASH_DATA_CACHE_DISABLE();
    __HAL_FLASH_DATA_CACHE_RESET();
    __HAL_FLASH_DATA_CACHE_ENABLE();
  }
}

/* Guarda una fila usando el orden nativo de halfwords del Cortex-M. */
static uint8_t Game_LevelFlashWriteRow(uint16_t y,
                                       const uint16_t *pixels)
{
  uint32_t address;
  uint16_t x;

  if (pixels == 0 || y >= ILI9341_HEIGHT)
  {
    return 0U;
  }

  address = (uint32_t)(uintptr_t)&__level_cache_start__ +
            (uint32_t)y * ILI9341_WIDTH * sizeof(uint16_t);

  for (x = 0U; x < ILI9341_WIDTH; x += 2U)
  {
    uint32_t data = (uint32_t)pixels[x] |
                    ((uint32_t)pixels[x + 1U] << 16U);

    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, data) != HAL_OK)
    {
      return 0U;
    }
    address += sizeof(uint32_t);
  }

  return 1U;
}

/* Actualiza la verificación usando los dos bytes de cada píxel. */
static uint32_t Game_UpdateLevelHash(uint32_t hash,
                                     const uint16_t *pixels,
                                     uint32_t pixel_count)
{
  uint32_t index;

  for (index = 0U; index < pixel_count; index++)
  {
    hash ^= (uint8_t)pixels[index];
    hash *= GAME_LEVEL_HASH_PRIME;
    hash ^= (uint8_t)(pixels[index] >> 8U);
    hash *= GAME_LEVEL_HASH_PRIME;
  }

  return hash;
}

/* Lee la SD una vez y deja la pantalla completa en la Flash interna. */
static uint8_t Game_LoadLevel(uint8_t level)
{
  const GameLevelFile_t *level_file;
  uint32_t expected_hash = GAME_LEVEL_HASH_INITIAL;
  uint32_t stored_hash;
  uint8_t flash_write_active = 0U;
  uint16_t y;

  game_load_error = GAME_LOAD_ERROR_NONE;
  game_load_error_row = 0U;

  if (level < GAME_LEVEL_1 || level > GAME_LEVEL_COUNT)
  {
    game_load_error = GAME_LOAD_ERROR_LEVEL;
    return 0U;
  }

  Game_UnloadLevel();

  if (game_sd_mounted == 0U)
  {
    if (f_mount(&USERFatFS, USERPath, 1U) != FR_OK)
    {
      game_load_error = GAME_LOAD_ERROR_MOUNT;
      return 0U;
    }
    game_sd_mounted = 1U;
  }

  level_file = Game_GetLevelFile(level);
  if (Game_OpenTextureFile(&game_background_file,
                           GAME_SD_BACKGROUND_FILE,
                           GAME_SD_BACKGROUND_FILE_ORIGINAL) == 0U)
  {
    game_load_error = GAME_LOAD_ERROR_BACKGROUND_OPEN;
    return 0U;
  }
  game_background_file_open = 1U;

  if (Game_OpenTextureFile(&game_overlay_file,
                           level_file->short_name,
                           level_file->original_name) == 0U)
  {
    game_load_error = GAME_LOAD_ERROR_OVERLAY_OPEN;
    Game_CloseLevelFiles();
    return 0U;
  }
  game_overlay_file_open = 1U;
  Game_TextReaderInit(&game_background_reader, &game_background_file);
  Game_TextReaderInit(&game_overlay_reader, &game_overlay_file);

  if (Game_LevelFlashBeginWrite() == 0U)
  {
    game_load_error = GAME_LOAD_ERROR_FLASH_ERASE;
    goto load_failed;
  }
  flash_write_active = 1U;

  for (y = 0U; y < ILI9341_HEIGHT; y++)
  {
    game_load_error_row = y;
    if (Game_ReadNextRow(&game_background_reader,
                         game_background_row,
                         GAME_TEXTURE_PACKED_PAIRS) == 0U)
    {
      game_load_error = GAME_LOAD_ERROR_BACKGROUND_READ;
      goto load_failed;
    }

    if (y < level_file->playfield_height)
    {
      if (Game_ReadNextRow(&game_overlay_reader,
                           game_overlay_row,
                           level_file->encoding) == 0U)
      {
        game_load_error = GAME_LOAD_ERROR_OVERLAY_READ;
        goto load_failed;
      }
    }
    Game_ComposeRow(y, level_file->playfield_height);
    expected_hash = Game_UpdateLevelHash(expected_hash,
                                         game_composed_row,
                                         ILI9341_WIDTH);
    if (Game_LevelFlashWriteRow(y, game_composed_row) == 0U)
    {
      game_load_error = GAME_LOAD_ERROR_FLASH_WRITE;
      goto load_failed;
    }
  }

  Game_LevelFlashEndWrite();
  flash_write_active = 0U;
  Game_CloseLevelFiles();

  stored_hash = Game_UpdateLevelHash(GAME_LEVEL_HASH_INITIAL,
                                     Game_LevelFlashPixels(),
                                     GAME_LEVEL_FLASH_PIXEL_COUNT);
  if (stored_hash != expected_hash)
  {
    game_load_error = GAME_LOAD_ERROR_FLASH_VERIFY;
    goto load_failed;
  }

  ILI9341_DrawBitmap(&lcd,
                     0U,
                     0U,
                     ILI9341_WIDTH,
                     ILI9341_HEIGHT,
                     Game_LevelFlashPixels());
  game_level_ready = 1U;
  return 1U;

load_failed:
  if (flash_write_active != 0U)
  {
    Game_LevelFlashEndWrite();
  }
  Game_UnloadLevel();
  return 0U;
}

/* Recupera directamente de Flash el fondo que ocupaba un sprite. */
static void Game_RestorePlayerArea(uint16_t player_x, uint16_t player_y)
{
  uint16_t sprite_x = Game_PlayerSpriteX(player_x);
  uint16_t sprite_y = Game_PlayerSpriteY(player_y);

  if (game_level_ready == 0U || sprite_x >= ILI9341_WIDTH ||
      sprite_y >= ILI9341_HEIGHT)
  {
    return;
  }

  ILI9341_DrawBitmapRegion(
      &lcd,
      sprite_x,
      sprite_y,
      CELESTE_J1_FRAME_WIDTH,
      CELESTE_J1_FRAME_HEIGHT,
      &Game_LevelFlashPixels()[(uint32_t)sprite_y * ILI9341_WIDTH + sprite_x],
      ILI9341_WIDTH);
}

/* Los límites de pantalla también se tratan como sólidos. */
static uint8_t Game_IsSolidPixel(int32_t x, int32_t y)
{
  if (x < 0L || x >= (int32_t)ILI9341_WIDTH || y < 0L ||
      y >= (int32_t)ILI9341_HEIGHT)
  {
    return 1U;
  }

  return (Game_GetCollision((uint16_t)x, (uint16_t)y) !=
          GAME_COLLISION_FREE)
             ? 1U
             : 0U;
}

/* Detecta contacto con el borde del jugador, sin exigir superposición. */
static uint8_t Game_TouchesLevelColor(uint16_t player_x,
                                      uint16_t player_y,
                                      uint16_t color,
                                      uint8_t reach)
{
  uint8_t target_type = (color == GAME_LEVEL_HAZARD_COLOR)
                            ? GAME_COLLISION_HAZARD
                            : GAME_COLLISION_EXIT;
  int32_t left = (int32_t)player_x - (int32_t)reach;
  int32_t right = (int32_t)player_x + GAME_PLAYER_WIDTH - 1L + reach;
  int32_t top = (int32_t)player_y - (int32_t)reach;
  int32_t bottom = (int32_t)player_y + GAME_PLAYER_HEIGHT - 1L + reach;
  int32_t x;
  int32_t y;

  for (y = top; y <= bottom; y++)
  {
    if (y < 0L || y >= (int32_t)ILI9341_HEIGHT)
    {
      continue;
    }

    for (x = left; x <= right; x++)
    {
      if (x < 0L || x >= (int32_t)ILI9341_WIDTH)
      {
        continue;
      }

      if (x >= (int32_t)player_x &&
          x < (int32_t)player_x + GAME_PLAYER_WIDTH &&
          y >= (int32_t)player_y &&
          y < (int32_t)player_y + GAME_PLAYER_HEIGHT)
      {
        continue;
      }

      if (Game_GetCollision((uint16_t)x, (uint16_t)y) == target_type)
      {
        return 1U;
      }
    }
  }

  return 0U;
}

/* Revisa el rectángulo completo del jugador en una posición. */
static uint8_t Game_CollidesAt(int32_t x, int32_t y)
{
  uint16_t row;
  uint16_t column;

  for (row = 0U; row < GAME_PLAYER_HEIGHT; row++)
  {
    for (column = 0U; column < GAME_PLAYER_WIDTH; column++)
    {
      if (Game_IsSolidPixel(x + (int32_t)column, y + (int32_t)row) != 0U)
      {
        return 1U;
      }
    }
  }

  return 0U;
}

/* Prueba la misma caja un píxel más abajo. */
static uint8_t Game_IsOnGround(uint16_t player_x, uint16_t player_y)
{
  return Game_CollidesAt((int32_t)player_x, (int32_t)player_y + 1L);
}

/* Los peligros y la salida no cuentan como paredes para saltar. */
static uint8_t Game_IsWallSurfacePixel(int32_t x, int32_t y)
{
  if (x < 0L || x >= (int32_t)ILI9341_WIDTH || y < 0L ||
      y >= (int32_t)ILI9341_HEIGHT)
  {
    return 0U;
  }

  return (Game_GetCollision((uint16_t)x, (uint16_t)y) ==
          GAME_COLLISION_SOLID)
             ? 1U
             : 0U;
}

/* Devuelve -1 para pared izquierda y 1 para pared derecha. */
static int8_t Game_GetWallSide(uint16_t player_x, uint16_t player_y)
{
  uint16_t row;
  uint8_t touches_left = 0U;
  uint8_t touches_right = 0U;
  int32_t left_x = (int32_t)player_x - 1L;
  int32_t right_x = (int32_t)player_x + GAME_PLAYER_WIDTH;

  for (row = 1U; row < (GAME_PLAYER_HEIGHT - 1U); row++)
  {
    int32_t y = (int32_t)player_y + row;

    touches_left |= Game_IsWallSurfacePixel(left_x, y);
    touches_right |= Game_IsWallSurfacePixel(right_x, y);
  }

  if (touches_left != 0U && touches_right == 0U)
  {
    return -1;
  }
  if (touches_right != 0U && touches_left == 0U)
  {
    return 1;
  }

  return 0;
}

/* Acerca la caída al 55% de la velocidad terminal normal. */
static void Game_ApplyWallSlide(uint8_t on_ground,
                                int8_t wall_side,
                                int32_t *player_velocity_y)
{
  if (on_ground != 0U || wall_side == 0 || *player_velocity_y <= 0L ||
      *player_velocity_y <= GAME_WALL_SLIDE_MAX_FALL_SPEED)
  {
    return;
  }

  *player_velocity_y = Game_Approach(*player_velocity_y,
                                     GAME_WALL_SLIDE_MAX_FALL_SPEED,
                                     GAME_WALL_SLIDE_DECEL);
}

/* Cambia la velocidad poco a poco para evitar movimientos bruscos. */
static void Game_UpdateHorizontalVelocity(int8_t horizontal_input,
                                          uint8_t on_ground,
                                          int32_t *player_velocity_x)
{
  int32_t target = (int32_t)horizontal_input * GAME_HORIZONTAL_MAX_SPEED;

  if (*player_velocity_x > GAME_HORIZONTAL_MAX_SPEED ||
      *player_velocity_x < -GAME_HORIZONTAL_MAX_SPEED)
  {
    int32_t normal_speed = (*player_velocity_x < 0L)
                               ? -GAME_HORIZONTAL_MAX_SPEED
                               : GAME_HORIZONTAL_MAX_SPEED;

    *player_velocity_x = Game_Approach(*player_velocity_x,
                                       normal_speed,
                                       GAME_HORIZONTAL_DECEL);
    return;
  }

  *player_velocity_x = Game_Approach(
      *player_velocity_x,
      target,
      (on_ground != 0U) ? GAME_HORIZONTAL_GROUND_ACCEL
                        : GAME_HORIZONTAL_AIR_ACCEL);
}

/* Calcula el impulso necesario para alcanzar la altura configurada. */
static int32_t Game_CalculateJumpSpeed(uint16_t jump_height)
{
  float gravity = (float)GAME_GRAVITY;
  float distance = (float)jump_height * (float)GAME_FIXED_POINT_ONE;
  float speed = 0.5f *
                (gravity +
                 sqrtf((gravity * gravity) +
                       (8.0f * gravity * distance)));
  int32_t speed_rounded_up = (int32_t)speed;

  if ((float)speed_rounded_up < speed)
  {
    speed_rounded_up++;
  }

  return -speed_rounded_up;
}

/* La altura se mide desde el punto exacto donde comienza el salto. */
static void Game_StartJump(uint16_t player_y,
                           uint16_t jump_height,
                           int32_t *player_y_fixed,
                           int32_t *player_velocity_y,
                           uint16_t *jump_apex_y,
                           uint8_t *jump_limit_active)
{
  *player_y_fixed = (int32_t)player_y * GAME_FIXED_POINT_ONE;
  *player_velocity_y = Game_CalculateJumpSpeed(jump_height);
  *jump_apex_y = (player_y > jump_height) ? (player_y - jump_height) : 0U;
  *jump_limit_active = 1U;
}

/* El segundo salto conserva su altura y agrega impulso hacia donde se apunta. */
static void Game_StartSecondJump(uint16_t player_y,
                                 int8_t horizontal_input,
                                 uint8_t aiming_up,
                                 int32_t *player_y_fixed,
                                 int32_t *player_velocity_x,
                                 int32_t *player_velocity_y,
                                 uint16_t *jump_apex_y,
                                 uint8_t *jump_limit_active)
{
  int32_t side_speed = (aiming_up != 0U)
                           ? GAME_SECOND_JUMP_DIAGONAL_SPEED
                           : GAME_SECOND_JUMP_SIDE_SPEED;

  Game_StartJump(player_y,
                 GAME_SECOND_JUMP_HEIGHT,
                 player_y_fixed,
                 player_velocity_y,
                 jump_apex_y,
                 jump_limit_active);

  if (horizontal_input != 0)
  {
    *player_velocity_x = (int32_t)horizontal_input * side_speed;
  }
  else if (aiming_up != 0U)
  {
    *player_velocity_x = 0L;
  }
}

/* Salta en sentido opuesto a la pared sin recuperar el segundo salto. */
static void Game_StartWallJump(uint16_t player_y,
                               int8_t wall_side,
                               int32_t *player_y_fixed,
                               int32_t *player_velocity_x,
                               int32_t *player_velocity_y,
                               uint16_t *jump_apex_y,
                               uint8_t *jump_limit_active)
{
  Game_StartJump(player_y,
                 GAME_WALL_JUMP_HEIGHT,
                 player_y_fixed,
                 player_velocity_y,
                 jump_apex_y,
                 jump_limit_active);
  *player_velocity_x = -(int32_t)wall_side * GAME_WALL_JUMP_SIDE_SPEED;
}

/* Cerca del punto más alto se usa menos gravedad para suavizar el giro. */
static void Game_ApplyGravity(uint8_t on_ground, int32_t *player_velocity_y)
{
  int32_t gravity;

  if (on_ground != 0U)
  {
    return;
  }

  gravity = ((*player_velocity_y >= -GAME_APEX_SPEED) &&
             (*player_velocity_y <= GAME_APEX_SPEED))
                ? GAME_APEX_GRAVITY
                : GAME_GRAVITY;
  *player_velocity_y = Game_Approach(*player_velocity_y,
                                     GAME_MAX_FALL_SPEED,
                                     gravity);
}

/* Mueve un píxel a la vez para no atravesar paredes. */
static void Game_MoveHorizontal(uint16_t *player_x,
                                uint16_t player_y,
                                int32_t *player_x_fixed,
                                int32_t *player_velocity_x)
{
  int32_t desired_fixed = *player_x_fixed + *player_velocity_x;
  int32_t target_x;
  int32_t current_x = (int32_t)*player_x;

  if (desired_fixed < 0L)
  {
    desired_fixed = 0L;
    *player_velocity_x = 0L;
  }
  else if (desired_fixed > (int32_t)GAME_PLAYER_X_MAX * GAME_FIXED_POINT_ONE)
  {
    desired_fixed = (int32_t)GAME_PLAYER_X_MAX * GAME_FIXED_POINT_ONE;
    *player_velocity_x = 0L;
  }

  target_x = desired_fixed / GAME_FIXED_POINT_ONE; /* Vuelve a píxeles. */
  while (current_x != target_x)
  {
    int32_t next_x = current_x + ((target_x > current_x) ? 1L : -1L);

    if (Game_CollidesAt(next_x, (int32_t)player_y) != 0U)
    {
      *player_velocity_x = 0L;
      desired_fixed = current_x * GAME_FIXED_POINT_ONE;
      break;
    }

    current_x = next_x;
  }

  *player_x = (uint16_t)current_x;
  *player_x_fixed = desired_fixed;
}

/* Aplica altura máxima y colisiones con suelo o techo. */
static void Game_MoveVertical(uint16_t player_x,
                              uint16_t *player_y,
                              int32_t *player_y_fixed,
                              int32_t *player_velocity_y,
                              uint16_t jump_apex_y,
                              uint8_t *jump_limit_active)
{
  int32_t desired_fixed = *player_y_fixed + *player_velocity_y;
  int32_t target_y;
  int32_t current_y = (int32_t)*player_y;

  if (*jump_limit_active != 0U && *player_velocity_y < 0L &&
      desired_fixed < (int32_t)jump_apex_y * GAME_FIXED_POINT_ONE)
  {
    desired_fixed = (int32_t)jump_apex_y * GAME_FIXED_POINT_ONE;
    *player_velocity_y = 0L;
    *jump_limit_active = 0U;
  }

  if (desired_fixed < 0L)
  {
    desired_fixed = 0L;
    *player_velocity_y = 0L;
  }
  else if (desired_fixed > (int32_t)GAME_PLAYER_Y_MAX * GAME_FIXED_POINT_ONE)
  {
    desired_fixed = (int32_t)GAME_PLAYER_Y_MAX * GAME_FIXED_POINT_ONE;
    *player_velocity_y = 0L;
  }

  target_y = desired_fixed / GAME_FIXED_POINT_ONE; /* Vuelve a píxeles. */
  while (current_y != target_y)
  {
    int32_t next_y = current_y + ((target_y > current_y) ? 1L : -1L);

    if (Game_CollidesAt((int32_t)player_x, next_y) != 0U)
    {
      if (*player_velocity_y < 0L)
      {
        *jump_limit_active = 0U;
      }
      *player_velocity_y = 0L;
      desired_fixed = current_y * GAME_FIXED_POINT_ONE;
      break;
    }

    current_y = next_y;
  }

  *player_y = (uint16_t)current_y;
  *player_y_fixed = desired_fixed;

  if (*player_velocity_y >= 0L)
  {
    *jump_limit_active = 0U;
  }
}

/* Restaura posicion, saltos y animacion de un jugador. */
static void Game_ResetPlayer(GamePlayer_t *player)
{
  const GameLevelFile_t *level_file = Game_GetLevelFile(game_current_level);
  uint16_t start_x = (player->controller_id == CONTROLES_JUGADOR_2)
                         ? GAME_PLAYER_2_START_X
                         : GAME_PLAYER_START_X;
  uint16_t start_y = (level_file != 0)
                         ? level_file->player_start_y
                         : GAME_PLAYER_START_Y;

  player->x = start_x;
  player->y = start_y;
  player->x_fixed = (int32_t)start_x * GAME_FIXED_POINT_ONE;
  player->y_fixed = (int32_t)start_y * GAME_FIXED_POINT_ONE;
  player->velocity_x = 0L;
  player->velocity_y = 0L;
  player->jump_apex_y = start_y;
  player->animation_frame = 0U;
  player->animation_updates = 0U;
  player->facing_left = 0U;
  player->jump_count = GAME_JUMP_COUNT_NORMAL_READY;
  player->jump_buffer_frames = 0U;
  player->wall_jump_lock_frames = 0U;
  player->was_on_ground = 0U;
  player->jump_limit_active = 0U;
  player->wall_jump_direction = 0;
}

/* Asigna control y sprites antes de iniciar la partida. */
static void Game_ConfigurePlayer(GamePlayer_t *player,
                                 uint8_t controller_id,
                                 const uint16_t *sprite_sheet,
                                 uint16_t sprite_sheet_width,
                                 uint8_t sprite_frame_count,
                                 const uint16_t *jump_sprite)
{
  memset(player, 0, sizeof(*player));
  player->controller_id = controller_id;
  player->sprite_sheet = sprite_sheet;
  player->sprite_sheet_width = sprite_sheet_width;
  player->sprite_frame_count = sprite_frame_count;
  player->jump_sprite = jump_sprite;
  Game_ResetPlayer(player);
}

/* Actualiza un jugador usando exclusivamente el control que tiene asignado. */
static uint8_t Game_UpdatePlayer(GamePlayer_t *player)
{
  uint16_t previous_x = player->x;
  uint8_t jump_just_pressed = Controles_TakePressed(
      player->controller_id, CONTROLES_SALTO);
  uint8_t second_jump_just_pressed = Controles_TakePressed(
      player->controller_id, CONTROLES_SALTO_2);
  uint8_t held_left = Controles_Held(player->controller_id,
                                     CONTROLES_IZQUIERDA);
  uint8_t held_right = Controles_Held(player->controller_id,
                                      CONTROLES_DERECHA);
  uint8_t held_up = Controles_Held(player->controller_id,
                                   CONTROLES_ARRIBA);
  uint8_t on_ground;
  uint8_t airborne;
  uint8_t touched_hazard;
  uint8_t touched_exit;
  int8_t horizontal_input;
  int8_t wall_side;

  if (jump_just_pressed != 0U)
  {
    /* Conserva la pulsación durante unos cuadros. */
    player->jump_buffer_frames = GAME_JUMP_BUFFER_FRAMES;
  }

  if (player->wall_jump_lock_frames != 0U)
  {
    horizontal_input = player->wall_jump_direction;
    player->wall_jump_lock_frames--;
  }
  else if (held_right != held_left)
  {
    horizontal_input = (held_right != 0U) ? 1 : -1;
  }
  else
  {
    horizontal_input = 0;
  }

  on_ground = Game_IsOnGround(player->x, player->y);
  wall_side = Game_GetWallSide(player->x, player->y);
  if (on_ground != 0U && player->was_on_ground == 0U)
  {
    /* Aterrizar recupera ambos saltos. */
    player->jump_count = GAME_JUMP_COUNT_NORMAL_READY;
    player->jump_limit_active = 0U;
  }

  Game_UpdateHorizontalVelocity(horizontal_input,
                                on_ground,
                                &player->velocity_x);

  /* Cruz da prioridad al salto de pared mientras esta en el aire. */
  if (player->jump_buffer_frames != 0U && on_ground == 0U &&
      wall_side != 0)
  {
    Game_StartWallJump(player->y,
                       wall_side,
                       &player->y_fixed,
                       &player->velocity_x,
                       &player->velocity_y,
                       &player->jump_apex_y,
                       &player->jump_limit_active);
    if (player->jump_count == GAME_JUMP_COUNT_NORMAL_READY)
    {
      player->jump_count = GAME_JUMP_COUNT_SECOND_READY;
    }
    player->wall_jump_direction = (int8_t)-wall_side;
    player->wall_jump_lock_frames = GAME_WALL_JUMP_LOCK_FRAMES;
    player->jump_buffer_frames = 0U;
    on_ground = 0U;
  }
  else if (player->jump_buffer_frames != 0U &&
           player->jump_count == GAME_JUMP_COUNT_NORMAL_READY)
  {
    Game_StartJump(player->y,
                   GAME_JUMP_HEIGHT,
                   &player->y_fixed,
                   &player->velocity_y,
                   &player->jump_apex_y,
                   &player->jump_limit_active);
    player->jump_count = GAME_JUMP_COUNT_SECOND_READY;
    player->jump_buffer_frames = 0U;
    on_ground = 0U;
  }
  else if (second_jump_just_pressed != 0U &&
           player->jump_count == GAME_JUMP_COUNT_SECOND_READY)
  {
    Game_StartSecondJump(player->y,
                         horizontal_input,
                         held_up,
                         &player->y_fixed,
                         &player->velocity_x,
                         &player->velocity_y,
                         &player->jump_apex_y,
                         &player->jump_limit_active);
    player->jump_count = GAME_JUMP_COUNT_EXHAUSTED;
    player->jump_buffer_frames = 0U;
    on_ground = 0U;
  }

  if (player->jump_buffer_frames != 0U)
  {
    player->jump_buffer_frames--;
  }

  Game_ApplyGravity(on_ground, &player->velocity_y);
  Game_MoveHorizontal(&player->x,
                      player->y,
                      &player->x_fixed,
                      &player->velocity_x);
  wall_side = Game_GetWallSide(player->x, player->y);
  Game_ApplyWallSlide(on_ground, wall_side, &player->velocity_y);

  touched_hazard = Game_TouchesLevelColor(player->x,
                                          player->y,
                                          GAME_LEVEL_HAZARD_COLOR,
                                          GAME_LEVEL_HAZARD_REACH);
  touched_exit = Game_TouchesLevelColor(player->x,
                                        player->y,
                                        GAME_LEVEL_EXIT_COLOR,
                                        GAME_LEVEL_EXIT_REACH);
  /* Se vuelve a comprobar después de mover para no saltar un contacto. */
  Game_MoveVertical(player->x,
                    &player->y,
                    &player->y_fixed,
                    &player->velocity_y,
                    player->jump_apex_y,
                    &player->jump_limit_active);
  touched_hazard |= Game_TouchesLevelColor(player->x,
                                           player->y,
                                           GAME_LEVEL_HAZARD_COLOR,
                                           GAME_LEVEL_HAZARD_REACH);
  touched_exit |= Game_TouchesLevelColor(player->x,
                                         player->y,
                                         GAME_LEVEL_EXIT_COLOR,
                                         GAME_LEVEL_EXIT_REACH);

  if (touched_hazard != 0U)
  {
    Game_ResetPlayer(player);
    player->was_on_ground = Game_IsOnGround(player->x, player->y);
    return GAME_PLAYER_EVENT_RESPAWN;
  }

  airborne = (Game_IsOnGround(player->x, player->y) == 0U) ? 1U : 0U;
  if (airborne == 0U && on_ground == 0U)
  {
    player->jump_count = GAME_JUMP_COUNT_NORMAL_READY;
    player->jump_limit_active = 0U;
  }
  player->was_on_ground = (airborne == 0U) ? 1U : 0U;

  if (player->x < previous_x)
  {
    player->facing_left = 1U;
  }
  else if (player->x > previous_x)
  {
    player->facing_left = 0U;
  }

  if (airborne != 0U)
  {
    player->animation_frame = 0U;
    player->animation_updates = 0U;
  }
  else if (player->x != previous_x)
  {
    player->animation_updates++;
    if (player->animation_updates >= GAME_PLAYER_ANIMATION_STEP_UPDATES)
    {
      player->animation_updates = 0U;
      /* Avanza una imagen del ciclo de caminata. */
      player->animation_frame = (uint8_t)(
          (player->animation_frame + 1U) % player->sprite_frame_count);
    }
  }
  else
  {
    player->animation_frame = 0U;
    player->animation_updates = 0U;
  }

  return (touched_exit != 0U) ? GAME_PLAYER_EVENT_EXIT
                              : GAME_PLAYER_EVENT_NONE;
}

/* Dibuja ambos jugadores con su estado actual. */
static void Game_DrawPlayers(GamePlayer_t *players)
{
  uint8_t index;

  for (index = 0U; index < GAME_PLAYER_COUNT; index++)
  {
    uint8_t airborne = (Game_IsOnGround(players[index].x,
                                        players[index].y) == 0U)
                           ? 1U
                           : 0U;
    Game_DrawPlayer(&players[index], airborne);
  }
}

/* Carga el nivel y coloca marcador y jugadores. */
static uint8_t Game_DrawScene(GamePlayer_t *players)
{
  if (Game_LoadLevel(game_current_level) == 0U)
  {
    Game_DrawLoadError();
    return 0U;
  }

  Game_DrawScoreboard(game_current_level);
  Game_DrawPlayers(players);
  return 1U;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  ILI9341_Parallel8Config lcd_config;
  uint8_t start_screen_drawn = 0U;
  uint8_t prompt_visible = 0U;
  uint8_t game_started = 0U;
  uint8_t game_screen_drawn = 0U;
  uint8_t start_requested = 0U;
  uint8_t results_screen = 0U;
  GamePlayer_t players[GAME_PLAYER_COUNT];
  uint32_t last_prompt_blink_ms = 0U;
  uint32_t last_game_update_ms = 0U;

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI1_Init();
  MX_FATFS_Init();
  MX_USART2_UART_Init();
  MX_USART3_UART_Init();
  MX_UART5_Init();
  /* USER CODE BEGIN 2 */
  lcd_config.reset.port = LCD_RST_GPIO_Port;
  lcd_config.reset.pin = LCD_RST_Pin;
  lcd_config.read.port = LCD_RD_GPIO_Port;
  lcd_config.read.pin = LCD_RD_Pin;
  lcd_config.write.port = LCD_WR_GPIO_Port;
  lcd_config.write.pin = LCD_WR_Pin;
  lcd_config.register_select.port = LCD_RS_GPIO_Port;
  lcd_config.register_select.pin = LCD_RS_Pin;
  lcd_config.chip_select.port = LCD_CS_GPIO_Port;
  lcd_config.chip_select.pin = LCD_CS_Pin;
  lcd_config.data[0] = (ILI9341_Pin){LCD_D0_GPIO_Port, LCD_D0_Pin};
  lcd_config.data[1] = (ILI9341_Pin){LCD_D1_GPIO_Port, LCD_D1_Pin};
  lcd_config.data[2] = (ILI9341_Pin){LCD_D2_GPIO_Port, LCD_D2_Pin};
  lcd_config.data[3] = (ILI9341_Pin){LCD_D3_GPIO_Port, LCD_D3_Pin};
  lcd_config.data[4] = (ILI9341_Pin){LCD_D4_GPIO_Port, LCD_D4_Pin};
  lcd_config.data[5] = (ILI9341_Pin){LCD_D5_GPIO_Port, LCD_D5_Pin};
  lcd_config.data[6] = (ILI9341_Pin){LCD_D6_GPIO_Port, LCD_D6_Pin};
  lcd_config.data[7] = (ILI9341_Pin){LCD_D7_GPIO_Port, LCD_D7_Pin};
  lcd_config.memory_access_control = 0x48U;

  ILI9341_BeginInit(&lcd, &lcd_config, HAL_GetTick());
  Controles_SetDebugUart(&huart2);
  Controles_Init();
  Game_ConfigurePlayer(&players[0],
                       CONTROLES_JUGADOR_1,
                       CelesteJ1,
                       CELESTE_J1_SHEET_WIDTH,
                       CELESTE_J1_FRAME_COUNT,
                       CelesteJ1Jump);
  Game_ConfigurePlayer(&players[1],
                       CONTROLES_JUGADOR_2,
                       CelesteJ2,
                       CELESTE_J2_SHEET_WIDTH,
                       CELESTE_J2_FRAME_COUNT,
                       0);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	while (1) {
      uint32_t now_ms = HAL_GetTick();
      uint8_t start_j1;
      uint8_t start_j2;

      Controles_Update(now_ms);
      start_j1 = Controles_TakePressed(CONTROLES_JUGADOR_1,
                                       CONTROLES_INICIO);
      start_j2 = Controles_TakePressed(CONTROLES_JUGADOR_2,
                                       CONTROLES_INICIO);
      if (start_j1 != 0U || start_j2 != 0U)
      {
        start_requested = 1U;
      }

      if (ILI9341_UpdateInit(&lcd, HAL_GetTick()) == 0U) {
        continue;
      }

      if (start_screen_drawn == 0U) {
        ILI9341_DrawBitmap(&lcd,
                            0U,
                            0U,
                            START_BACKGROUND_WIDTH,
                            START_BACKGROUND_HEIGHT,
                            start_background);
        Nieve_Init(&nieve, HAL_GetTick(), HAL_GetTick());
        if (results_screen != 0U)
        {
          Game_DrawResultScreen();
        }
        StartScreen_DrawPrompt();
        prompt_visible = 1U;
        start_screen_drawn = 1U;
        last_prompt_blink_ms = HAL_GetTick();
        continue;
      }

      if (game_started == 0U && start_requested != 0U) {
        if (prompt_visible != 0U) {
          StartScreen_RestorePromptBackground();
          prompt_visible = 0U;
        }
        Nieve_Clear(&nieve,
                    &lcd,
                    start_background,
                    START_BACKGROUND_WIDTH,
                    START_BACKGROUND_HEIGHT);
        memset(game_scores, 0, sizeof(game_scores));
        game_current_level = GAME_LEVEL_1;
        Game_UnloadLevel();
        Game_ResetPlayer(&players[0]);
        Game_ResetPlayer(&players[1]);
        game_screen_drawn = 0U;
        results_screen = 0U;
        start_requested = 0U;
        game_started = 1U;
        continue;
      }

      if (game_started != 0U && game_screen_drawn == 0U) {
        uint8_t player_index;

        game_level_ready = Game_DrawScene(players);
        game_screen_drawn = 1U;
        for (player_index = 0U;
             player_index < GAME_PLAYER_COUNT;
             player_index++)
        {
          (void)Controles_TakePressed(players[player_index].controller_id,
                                      CONTROLES_SALTO);
          (void)Controles_TakePressed(players[player_index].controller_id,
                                      CONTROLES_SALTO_2);
          players[player_index].was_on_ground =
              (game_level_ready != 0U)
                  ? Game_IsOnGround(players[player_index].x,
                                    players[player_index].y)
                  : 0U;
        }
        last_game_update_ms = HAL_GetTick();
        continue;
      }

      if (game_started != 0U && game_level_ready != 0U &&
          (HAL_GetTick() - last_game_update_ms) >= GAME_UPDATE_MS) {
        uint16_t previous_x[GAME_PLAYER_COUNT];
        uint16_t previous_y[GAME_PLAYER_COUNT];
        uint8_t previous_on_ground[GAME_PLAYER_COUNT];
        uint8_t events[GAME_PLAYER_COUNT];
        uint8_t redraw_players = 0U;
        uint8_t level_winner = GAME_PLAYER_COUNT;
        uint8_t player_index;

        last_game_update_ms = HAL_GetTick();

        /* Primero se actualiza la fisica de ambos jugadores. */
        for (player_index = 0U;
             player_index < GAME_PLAYER_COUNT;
             player_index++)
        {
          previous_x[player_index] = players[player_index].x;
          previous_y[player_index] = players[player_index].y;
          previous_on_ground[player_index] =
              players[player_index].was_on_ground;
          events[player_index] = Game_UpdatePlayer(&players[player_index]);

          if (players[player_index].x != previous_x[player_index] ||
              players[player_index].y != previous_y[player_index] ||
              players[player_index].was_on_ground !=
                  previous_on_ground[player_index] ||
              (events[player_index] & GAME_PLAYER_EVENT_RESPAWN) != 0U)
          {
            redraw_players = 1U;
          }
          if ((events[player_index] & GAME_PLAYER_EVENT_EXIT) != 0U &&
              level_winner == GAME_PLAYER_COUNT)
          {
            level_winner = player_index;
          }
        }

        /* El primer jugador detectado recibe el punto del nivel. */
        if (level_winner < GAME_PLAYER_COUNT)
        {
          if (game_scores[level_winner] < UINT16_MAX)
          {
            game_scores[level_winner]++;
          }

          if (game_current_level < GAME_LEVEL_COUNT)
          {
            game_current_level++;
            for (player_index = 0U;
                 player_index < GAME_PLAYER_COUNT;
                 player_index++)
            {
              Game_ResetPlayer(&players[player_index]);
            }
            Game_UnloadLevel();
            game_screen_drawn = 0U;
            continue;
          }

          /* El ultimo nivel vuelve al fondo inicial y muestra el resultado. */
          Game_UnloadLevel();
          game_started = 0U;
          results_screen = 1U;
          start_screen_drawn = 0U;
          game_screen_drawn = 0U;
          start_requested = 0U;
          prompt_visible = 0U;
          continue;
        }

        if (redraw_players != 0U)
        {
          /* Se borran los dos antes de dibujar para manejar cruces. */
          for (player_index = 0U;
               player_index < GAME_PLAYER_COUNT;
               player_index++)
          {
            Game_RestorePlayerArea(previous_x[player_index],
                                   previous_y[player_index]);
          }
          Game_DrawPlayers(players);
        }
      }

      if (game_started == 0U &&
          Nieve_Update(&nieve,
                       &lcd,
                       start_background,
                       START_BACKGROUND_WIDTH,
                       START_BACKGROUND_HEIGHT,
                       HAL_GetTick()) != 0U) {
        if (results_screen != 0U)
        {
          Game_DrawResultScreen();
        }
        if (prompt_visible != 0U)
        {
          StartScreen_DrawPrompt();
        }
      }

      if (game_started == 0U &&
          (HAL_GetTick() - last_prompt_blink_ms) >= START_PROMPT_BLINK_MS) {
        last_prompt_blink_ms = HAL_GetTick();
        if (prompt_visible != 0U) {
          StartScreen_RestorePromptBackground();
          prompt_visible = 0U;
        } else {
          StartScreen_DrawPrompt();
          prompt_visible = 1U;
        }
      }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	}
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 80;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief UART5 Initialization Function
  * @param None
  * @retval None
  */
static void MX_UART5_Init(void)
{

  /* USER CODE BEGIN UART5_Init 0 */

  /* USER CODE END UART5_Init 0 */

  /* USER CODE BEGIN UART5_Init 1 */

  /* USER CODE END UART5_Init 1 */
  huart5.Instance = UART5;
  huart5.Init.BaudRate = 115200;
  huart5.Init.WordLength = UART_WORDLENGTH_8B;
  huart5.Init.StopBits = UART_STOPBITS_1;
  huart5.Init.Parity = UART_PARITY_NONE;
  huart5.Init.Mode = UART_MODE_TX_RX;
  huart5.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart5.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart5) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN UART5_Init 2 */

  /* USER CODE END UART5_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */
  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, LCD_RST_Pin|LCD_D1_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, LCD_RD_Pin|LCD_WR_Pin|LCD_RS_Pin|LCD_D7_Pin
                          |LCD_D0_Pin|LCD_D2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LCD_CS_Pin|LCD_D6_Pin|LCD_D3_Pin|LCD_D5_Pin
                          |LCD_D4_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, ESP1_CS_Pin|SD_SS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pins : LCD_RST_Pin LCD_D1_Pin */
  GPIO_InitStruct.Pin = LCD_RST_Pin|LCD_D1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : LCD_RD_Pin LCD_WR_Pin LCD_RS_Pin LCD_D7_Pin
                           LCD_D0_Pin LCD_D2_Pin */
  GPIO_InitStruct.Pin = LCD_RD_Pin|LCD_WR_Pin|LCD_RS_Pin|LCD_D7_Pin
                          |LCD_D0_Pin|LCD_D2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : btn_arriba_Pin btn_derecha_Pin btn_izquierda_Pin */
  GPIO_InitStruct.Pin = btn_arriba_Pin|btn_derecha_Pin|btn_izquierda_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : LCD_CS_Pin LCD_D6_Pin ESP1_CS_Pin LCD_D3_Pin
                           LCD_D5_Pin LCD_D4_Pin SD_SS_Pin */
  GPIO_InitStruct.Pin = LCD_CS_Pin|LCD_D6_Pin|ESP1_CS_Pin|LCD_D3_Pin
                          |LCD_D5_Pin|LCD_D4_Pin|SD_SS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : ESP1_READY_Pin */
  GPIO_InitStruct.Pin = ESP1_READY_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(ESP1_READY_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : btn_salto2_Pin btn_salto_Pin */
  GPIO_InitStruct.Pin = btn_salto2_Pin|btn_salto_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  Controles_UART_RxCpltCallback(huart);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  Controles_UART_ErrorCallback(huart);
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1) {
	}
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
