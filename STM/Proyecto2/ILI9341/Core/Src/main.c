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

#include "graficos.h"
#include "ili9341.h"
#include "level_1_texture.h"
#include "nieve.h"
#include "start_background.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct
{
  GPIO_TypeDef *port;
  uint16_t pin;
  uint8_t raw_pressed;
  uint8_t stable_pressed;
  uint8_t pressed_event;
  uint32_t last_raw_change_ms;
} GameButton_t;

typedef struct
{
  const char *short_name;
  const char *original_name;
  uint16_t height;
} GameLevelFile_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define UART_RX_QUEUE_SIZE 16U
#define START_PROMPT_BOX_X 16U
#define START_PROMPT_BOX_Y 268U
#define START_PROMPT_BOX_WIDTH 208U
#define START_PROMPT_BOX_HEIGHT 24U
#define START_PROMPT_TEXT_X 28U
#define START_PROMPT_TEXT_Y 274U
#define START_PROMPT_BLINK_MS 250U
#define START_PROMPT_TEXT "PRESIONA A PARA INICIAR"
#define BUTTON_DEBOUNCE_MS 25U
#define GAME_PLAYER_WIDTH 18U
#define GAME_PLAYER_HEIGHT 14U
#define GAME_PLAYER_SPRITE_TRANSPARENT_COLOR 0x07E5U
#define GAME_PLAYER_SPRITE_OFFSET_X 1U
#define GAME_PLAYER_SPRITE_OFFSET_Y 2U
#define GAME_PLAYER_ANIMATION_STEP_UPDATES 4U
#define GAME_PLAYER_START_X 15U
#define GAME_PLAYER_START_Y 170U
#define GAME_LEVEL_2_PLAYER_START_X 15U
#define GAME_LEVEL_2_PLAYER_START_Y 207U
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
#define GAME_HORIZONTAL_MAX_SPEED_MULTIPLIER 2.0f
#define GAME_HORIZONTAL_MAX_SPEED \
  GAME_SCALE_ROUNDED(GAME_FIXED_POINT_ONE, \
                     GAME_HORIZONTAL_MAX_SPEED_MULTIPLIER)
#define GAME_HORIZONTAL_GROUND_ACCEL 180L
#define GAME_HORIZONTAL_AIR_ACCEL 180L
#define GAME_HORIZONTAL_DECEL 40L
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
#define GAME_LEVEL_1 1U
#define GAME_LEVEL_2 2U
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
#define GAME_SD_LINE_BUFFER_SIZE 2048U
#define GAME_PACKED_WORDS_PER_ROW (ILI9341_WIDTH / 2U)
#define GAME_LEVEL_FLASH_FIRST_SECTOR FLASH_SECTOR_6
#define GAME_LEVEL_FLASH_SECTOR_COUNT 2U
#define GAME_LEVEL_FLASH_PIXEL_COUNT \
  ((uint32_t)ILI9341_WIDTH * ILI9341_HEIGHT)
#define GAME_LEVEL_FLASH_BYTE_COUNT \
  (GAME_LEVEL_FLASH_PIXEL_COUNT * sizeof(uint16_t))
#define GAME_LEVEL_HASH_INITIAL 2166136261UL
#define GAME_LEVEL_HASH_PRIME 16777619UL
#define GAME_COLLISION_BITS_PER_PIXEL 2U
#define GAME_COLLISION_PIXELS_PER_BYTE 4U
#define GAME_COLLISION_MAP_SIZE \
  ((ILI9341_WIDTH * ILI9341_HEIGHT) / GAME_COLLISION_PIXELS_PER_BYTE)
#define GAME_COLLISION_FREE 0U
#define GAME_COLLISION_SOLID 1U
#define GAME_COLLISION_HAZARD 2U
#define GAME_COLLISION_EXIT 3U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
ILI9341_t lcd;
Nieve_t nieve;
static uint8_t game_current_level = GAME_LEVEL_1;
static const GameLevelFile_t game_level_files[] = {
    {GAME_SD_LEVEL_1_FILE,
     GAME_SD_LEVEL_1_FILE_ORIGINAL,
     LEVEL_1_TEXTURE_HEIGHT},
    {GAME_SD_LEVEL_2_FILE, GAME_SD_LEVEL_2_FILE_ORIGINAL, 235U},
};
static FIL game_background_file;
static FIL game_overlay_file;
static uint8_t game_background_file_open = 0U;
static uint8_t game_overlay_file_open = 0U;
static uint8_t game_sd_mounted = 0U;
static uint8_t game_level_ready = 0U;
static char game_sd_line[GAME_SD_LINE_BUFFER_SIZE];
static uint16_t game_background_row[ILI9341_WIDTH];
static uint16_t game_overlay_row[ILI9341_WIDTH];
static uint16_t game_composed_row[ILI9341_WIDTH];
static uint8_t game_collision_map[GAME_COLLISION_MAP_SIZE];
static uint16_t game_player_background[
    CELESTE_J1_FRAME_WIDTH * CELESTE_J1_FRAME_HEIGHT];
static uint16_t game_player_background_x = 0U;
static uint16_t game_player_background_y = 0U;
static uint8_t game_player_background_valid = 0U;
volatile uint8_t uart_rx_byte;
volatile uint8_t uart_rx_queue[UART_RX_QUEUE_SIZE];
volatile uint8_t uart_rx_write = 0U;
volatile uint8_t uart_rx_read = 0U;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */
extern uint8_t __level_cache_start__;
extern uint8_t __level_cache_end__;

static uint8_t Game_CapturePlayerBackground(uint16_t player_x,
                                            uint16_t player_y);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void UART_QueuePush(uint8_t byte)
{
  uint8_t next = (uint8_t)(uart_rx_write + 1U);

  if (next >= UART_RX_QUEUE_SIZE)
  {
    next = 0U;
  }

  if (next != uart_rx_read)
  {
    uart_rx_queue[uart_rx_write] = byte;
    uart_rx_write = next;
  }
}

static uint8_t UART_QueuePop(uint8_t *byte)
{
  if (uart_rx_read == uart_rx_write)
  {
    return 0U;
  }

  *byte = uart_rx_queue[uart_rx_read];
  uart_rx_read++;
  if (uart_rx_read >= UART_RX_QUEUE_SIZE)
  {
    uart_rx_read = 0U;
  }

  return 1U;
}

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

static uint8_t Game_ReadRawButton(GPIO_TypeDef *port, uint16_t pin)
{
  return (HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_RESET) ? 1U : 0U;
}

static void Game_ButtonInit(GameButton_t *button,
                            GPIO_TypeDef *port,
                            uint16_t pin,
                            uint32_t now_ms)
{
  button->port = port;
  button->pin = pin;
  button->raw_pressed = Game_ReadRawButton(port, pin);
  button->stable_pressed = button->raw_pressed;
  button->pressed_event = 0U;
  button->last_raw_change_ms = now_ms;
}

static void Game_ButtonUpdate(GameButton_t *button, uint32_t now_ms)
{
  uint8_t raw_pressed = Game_ReadRawButton(button->port, button->pin);

  if (raw_pressed != button->raw_pressed)
  {
    button->raw_pressed = raw_pressed;
    button->last_raw_change_ms = now_ms;
  }

  if (button->stable_pressed != button->raw_pressed &&
      (now_ms - button->last_raw_change_ms) >= BUTTON_DEBOUNCE_MS)
  {
    button->stable_pressed = button->raw_pressed;
    if (button->stable_pressed != 0U)
    {
      button->pressed_event = 1U;
    }
  }
}

static uint8_t Game_ButtonPressed(const GameButton_t *button)
{
  return button->stable_pressed;
}

/* Conserva la pulsación hasta que la física esté lista para procesarla. */
static uint8_t Game_ButtonTakePress(GameButton_t *button)
{
  uint8_t pressed_event = button->pressed_event;

  button->pressed_event = 0U;
  return pressed_event;
}

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

/* Dibuja un cuadro de la hoja y refleja el personaje al mirar a la izquierda. */
static void Game_DrawPlayer(uint16_t x,
                            uint16_t y,
                            uint8_t animation_frame,
                            uint8_t facing_left,
                            uint8_t airborne)
{
  if (Game_CapturePlayerBackground(x, y) == 0U)
  {
    return;
  }

  if (airborne != 0U)
  {
    ILI9341_DrawSpriteFrameKeyed(
        &lcd,
        Game_PlayerSpriteX(x),
        Game_PlayerSpriteY(y),
        CELESTE_J1_JUMP_WIDTH,
        CELESTE_J1_JUMP_HEIGHT,
        CelesteJ1Jump,
        CELESTE_J1_JUMP_WIDTH,
        0U,
        GAME_PLAYER_SPRITE_TRANSPARENT_COLOR,
        facing_left);
    return;
  }

  ILI9341_DrawSpriteFrameKeyed(
      &lcd,
      Game_PlayerSpriteX(x),
      Game_PlayerSpriteY(y),
      CELESTE_J1_FRAME_WIDTH,
      CELESTE_J1_FRAME_HEIGHT,
      CelesteJ1,
      CELESTE_J1_SHEET_WIDTH,
      (uint16_t)(animation_frame % CELESTE_J1_FRAME_COUNT),
      GAME_PLAYER_SPRITE_TRANSPARENT_COLOR,
      facing_left);
}

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

/* Convierte una fila de 120 palabras en 240 píxeles RGB565. */
static uint8_t Game_ParsePackedRow(char *line, uint16_t *pixels)
{
  char *cursor = line;
  uint16_t word_index = 0U;

  while (*cursor != '\0' && word_index < GAME_PACKED_WORDS_PER_ROW)
  {
    uint32_t value = 0U;
    uint8_t digit_count = 0U;
    int8_t digit;

    while (*cursor != '\0' &&
           !(cursor[0] == '0' &&
             (cursor[1] == 'x' || cursor[1] == 'X')))
    {
      cursor++;
    }
    if (*cursor == '\0')
    {
      break;
    }

    cursor += 2;
    while ((digit = Game_HexDigit(*cursor)) >= 0)
    {
      value = (value << 4U) | (uint32_t)digit;
      digit_count++;
      cursor++;
    }
    if (digit_count == 0U)
    {
      return 0U;
    }

    pixels[word_index * 2U] = (uint16_t)(value >> 16U);
    pixels[word_index * 2U + 1U] = (uint16_t)value;
    word_index++;
  }

  return (word_index == GAME_PACKED_WORDS_PER_ROW) ? 1U : 0U;
}

static uint8_t Game_ReadNextPackedRow(FIL *file, uint16_t *pixels)
{
  if (f_gets(game_sd_line, sizeof(game_sd_line), file) == 0)
  {
    return 0U;
  }

  return Game_ParsePackedRow(game_sd_line, pixels);
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
  game_player_background_valid = 0U;
  memset(game_collision_map, 0, sizeof(game_collision_map));
}

static void Game_SetCollision(uint16_t x, uint16_t y, uint8_t type)
{
  uint32_t pixel_index = (uint32_t)y * ILI9341_WIDTH + x;
  uint32_t byte_index = pixel_index / GAME_COLLISION_PIXELS_PER_BYTE;
  uint8_t shift = (uint8_t)((pixel_index % GAME_COLLISION_PIXELS_PER_BYTE) *
                            GAME_COLLISION_BITS_PER_PIXEL);
  uint8_t mask = (uint8_t)(0x03U << shift);

  game_collision_map[byte_index] =
      (uint8_t)((game_collision_map[byte_index] & (uint8_t)~mask) |
                ((type & 0x03U) << shift));
}

static uint8_t Game_GetCollision(uint16_t x, uint16_t y)
{
  uint32_t pixel_index = (uint32_t)y * ILI9341_WIDTH + x;
  uint32_t byte_index = pixel_index / GAME_COLLISION_PIXELS_PER_BYTE;
  uint8_t shift = (uint8_t)((pixel_index % GAME_COLLISION_PIXELS_PER_BYTE) *
                            GAME_COLLISION_BITS_PER_PIXEL);

  return (uint8_t)((game_collision_map[byte_index] >> shift) & 0x03U);
}

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

static void Game_ComposeRow(uint16_t y)
{
  uint16_t x;

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

static const uint16_t *Game_LevelFlashPixels(void)
{
  return (const uint16_t *)(uintptr_t)&__level_cache_start__;
}

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

  if (level < GAME_LEVEL_1 || level > GAME_LEVEL_2)
  {
    return 0U;
  }

  Game_UnloadLevel();

  if (game_sd_mounted == 0U)
  {
    if (f_mount(&USERFatFS, USERPath, 1U) != FR_OK)
    {
      return 0U;
    }
    game_sd_mounted = 1U;
  }

  level_file = &game_level_files[level - GAME_LEVEL_1];
  if (Game_OpenTextureFile(&game_background_file,
                           GAME_SD_BACKGROUND_FILE,
                           GAME_SD_BACKGROUND_FILE_ORIGINAL) == 0U)
  {
    return 0U;
  }
  game_background_file_open = 1U;

  if (Game_OpenTextureFile(&game_overlay_file,
                           level_file->short_name,
                           level_file->original_name) == 0U)
  {
    Game_CloseLevelFiles();
    return 0U;
  }
  game_overlay_file_open = 1U;

  if (Game_LevelFlashBeginWrite() == 0U)
  {
    goto load_failed;
  }
  flash_write_active = 1U;

  for (y = 0U; y < ILI9341_HEIGHT; y++)
  {
    if (Game_ReadNextPackedRow(&game_background_file,
                               game_background_row) == 0U)
    {
      goto load_failed;
    }

    if (y < level_file->height)
    {
      if (Game_ReadNextPackedRow(&game_overlay_file,
                                 game_overlay_row) == 0U)
      {
        goto load_failed;
      }
    }
    else
    {
      uint16_t x;

      for (x = 0U; x < ILI9341_WIDTH; x++)
      {
        game_overlay_row[x] = LEVEL_1_TEXTURE_TRANSPARENT_COLOR;
      }
    }

    Game_ComposeRow(y);
    expected_hash = Game_UpdateLevelHash(expected_hash,
                                         game_composed_row,
                                         ILI9341_WIDTH);
    if (Game_LevelFlashWriteRow(y, game_composed_row) == 0U)
    {
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

static uint8_t Game_CapturePlayerBackground(uint16_t player_x,
                                            uint16_t player_y)
{
  uint16_t sprite_x = Game_PlayerSpriteX(player_x);
  uint16_t sprite_y = Game_PlayerSpriteY(player_y);
  uint16_t row;

  if (game_level_ready == 0U ||
      (uint32_t)sprite_x + CELESTE_J1_FRAME_WIDTH > ILI9341_WIDTH ||
      (uint32_t)sprite_y + CELESTE_J1_FRAME_HEIGHT > ILI9341_HEIGHT)
  {
    return 0U;
  }

  for (row = 0U; row < CELESTE_J1_FRAME_HEIGHT; row++)
  {
    memcpy(&game_player_background[(uint32_t)row * CELESTE_J1_FRAME_WIDTH],
           &Game_LevelFlashPixels()[
               (uint32_t)(sprite_y + row) * ILI9341_WIDTH + sprite_x],
           CELESTE_J1_FRAME_WIDTH * sizeof(uint16_t));
  }

  game_player_background_x = sprite_x;
  game_player_background_y = sprite_y;
  game_player_background_valid = 1U;
  return 1U;
}

static void Game_RestorePlayerBackground(void)
{
  if (game_player_background_valid == 0U)
  {
    return;
  }

  ILI9341_DrawBitmap(&lcd,
                     game_player_background_x,
                     game_player_background_y,
                     CELESTE_J1_FRAME_WIDTH,
                     CELESTE_J1_FRAME_HEIGHT,
                     game_player_background);
  game_player_background_valid = 0U;
}

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

  target_x = desired_fixed / GAME_FIXED_POINT_ONE;
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

  target_y = desired_fixed / GAME_FIXED_POINT_ONE;
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

/* Restaura posición y física usando el inicio configurado. */
static void Game_ResetPlayerPosition(uint16_t *player_x,
                                     uint16_t *player_y,
                                     int32_t *player_x_fixed,
                                     int32_t *player_y_fixed,
                                     int32_t *player_velocity_x,
                                     int32_t *player_velocity_y,
                                     uint16_t *jump_apex_y,
                                     uint8_t *jump_limit_active)
{
  uint16_t start_x = (game_current_level == GAME_LEVEL_2)
                         ? GAME_LEVEL_2_PLAYER_START_X
                         : GAME_PLAYER_START_X;
  uint16_t start_y = (game_current_level == GAME_LEVEL_2)
                         ? GAME_LEVEL_2_PLAYER_START_Y
                         : GAME_PLAYER_START_Y;

  *player_x = start_x;
  *player_y = start_y;
  *player_x_fixed = (int32_t)start_x * GAME_FIXED_POINT_ONE;
  *player_y_fixed = (int32_t)start_y * GAME_FIXED_POINT_ONE;
  *player_velocity_x = 0L;
  *player_velocity_y = 0L;
  *jump_apex_y = start_y;
  *jump_limit_active = 0U;
}

static void Game_ErasePlayer(uint16_t x, uint16_t y)
{
  (void)x;
  (void)y;
  Game_RestorePlayerBackground();
}

static uint8_t Game_DrawScene(uint16_t player_x,
                              uint16_t player_y,
                              uint8_t animation_frame,
                              uint8_t facing_left)
{
  if (Game_LoadLevel(game_current_level) == 0U)
  {
    ILI9341_Clear(&lcd, 0x0000U);
    ILI9341_DrawText(&lcd, "ERROR AL LEER SD", 40U, 150U, 0xF800U, 0x0000U);
    return 0U;
  }

  Game_DrawPlayer(player_x,
                  player_y,
                  animation_frame,
                  facing_left,
                  (Game_IsOnGround(player_x, player_y) == 0U) ? 1U : 0U);
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
  uint8_t uart_command;
  GameButton_t button_left;
  GameButton_t button_right;
  GameButton_t button_up;
  GameButton_t button_jump;
  GameButton_t button_second_jump;
  uint8_t player_animation_frame = 0U;
  uint8_t player_animation_updates = 0U;
  uint8_t player_facing_left = 0U;
  uint8_t jump_count = GAME_JUMP_COUNT_NORMAL_READY;
  uint8_t jump_buffer_frames = 0U;
  uint8_t wall_jump_lock_frames = 0U;
  uint8_t was_on_ground = 0U;
  uint8_t jump_limit_active = 0U;
  int8_t wall_jump_direction = 0;
  uint16_t player_x = GAME_PLAYER_START_X;
  uint16_t player_y = GAME_PLAYER_START_Y;
  uint16_t jump_apex_y = GAME_PLAYER_START_Y;
  int32_t player_x_fixed = GAME_PLAYER_START_X_FIXED;
  int32_t player_y_fixed = GAME_PLAYER_START_Y_FIXED;
  int32_t player_velocity_x = 0L;
  int32_t player_velocity_y = 0L;
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
  MX_USART2_UART_Init();
  MX_FATFS_Init();
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

  if (HAL_UART_Receive_IT(&huart2, &uart_rx_byte, 1U) != HAL_OK)
  {
    Error_Handler();
  }

  Game_ButtonInit(&button_left,
                  btn_izquierda_GPIO_Port,
                  btn_izquierda_Pin,
                  HAL_GetTick());
  Game_ButtonInit(&button_right,
                  btn_derecha_GPIO_Port,
                  btn_derecha_Pin,
                  HAL_GetTick());
  Game_ButtonInit(&button_up,
                  btn_arriba_GPIO_Port,
                  btn_arriba_Pin,
                  HAL_GetTick());
  Game_ButtonInit(&button_jump,
                  btn_salto_GPIO_Port,
                  btn_salto_Pin,
                  HAL_GetTick());
  Game_ButtonInit(&button_second_jump,
                  btn_salto2_GPIO_Port,
                  btn_salto2_Pin,
                  HAL_GetTick());

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	while (1) {
      Game_ButtonUpdate(&button_left, HAL_GetTick());
      Game_ButtonUpdate(&button_right, HAL_GetTick());
      Game_ButtonUpdate(&button_up, HAL_GetTick());
      Game_ButtonUpdate(&button_jump, HAL_GetTick());
      Game_ButtonUpdate(&button_second_jump, HAL_GetTick());

      while (UART_QueuePop(&uart_command) != 0U) {
        if (uart_command == (uint8_t)'A' || uart_command == (uint8_t)'a') {
          start_requested = 1U;
        }
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
        game_current_level = GAME_LEVEL_1;
        game_level_ready = 0U;
        game_started = 1U;
        continue;
      }

      if (game_started != 0U && game_screen_drawn == 0U) {
        game_level_ready = Game_DrawScene(player_x,
                                          player_y,
                                          player_animation_frame,
                                          player_facing_left);
        game_screen_drawn = 1U;
        (void)Game_ButtonTakePress(&button_jump);
        (void)Game_ButtonTakePress(&button_second_jump);
        was_on_ground = (game_level_ready != 0U)
                            ? Game_IsOnGround(player_x, player_y)
                            : 0U;
        last_game_update_ms = HAL_GetTick();
        continue;
      }

      if (game_started != 0U && game_level_ready != 0U &&
          (HAL_GetTick() - last_game_update_ms) >= GAME_UPDATE_MS) {
        uint16_t previous_player_x = player_x;
        uint16_t previous_player_y = player_y;
        uint8_t player_moved;
        uint8_t jump_just_pressed = Game_ButtonTakePress(&button_jump);
        uint8_t second_jump_just_pressed =
            Game_ButtonTakePress(&button_second_jump);
        uint8_t on_ground;
        uint8_t player_airborne;
        uint8_t touched_hazard;
        uint8_t touched_exit;
        int8_t horizontal_input;
        int8_t wall_side;

        last_game_update_ms = HAL_GetTick();

        if (jump_just_pressed != 0U)
        {
          jump_buffer_frames = GAME_JUMP_BUFFER_FRAMES;
        }

        /* Mantiene el impulso lejos de la pared durante 100 ms. */
        if (wall_jump_lock_frames != 0U)
        {
          horizontal_input = wall_jump_direction;
          wall_jump_lock_frames--;
        }
        else if (Game_ButtonPressed(&button_right) != 0U) {
          horizontal_input = 1;
        } else if (Game_ButtonPressed(&button_left) != 0U) {
          horizontal_input = -1;
        } else {
          horizontal_input = 0;
        }

        on_ground = Game_IsOnGround(player_x, player_y);
        wall_side = Game_GetWallSide(player_x, player_y);
        if (on_ground != 0U && was_on_ground == 0U)
        {
          jump_count = GAME_JUMP_COUNT_NORMAL_READY;
          jump_limit_active = 0U;
        }

        Game_UpdateHorizontalVelocity(horizontal_input,
                                      on_ground,
                                      &player_velocity_x);

        /* PA12 da prioridad al salto de pared mientras está en el aire. */
        if (jump_buffer_frames != 0U && on_ground == 0U && wall_side != 0)
        {
          Game_StartWallJump(player_y,
                             wall_side,
                             &player_y_fixed,
                             &player_velocity_x,
                             &player_velocity_y,
                             &jump_apex_y,
                             &jump_limit_active);
          if (jump_count == GAME_JUMP_COUNT_NORMAL_READY)
          {
            jump_count = GAME_JUMP_COUNT_SECOND_READY;
          }
          wall_jump_direction = (int8_t)-wall_side;
          wall_jump_lock_frames = GAME_WALL_JUMP_LOCK_FRAMES;
          jump_buffer_frames = 0U;
          on_ground = 0U;
        }
        /* PA12 habilita el segundo salto sin consultar el suelo. */
        else if (jump_buffer_frames != 0U &&
            jump_count == GAME_JUMP_COUNT_NORMAL_READY)
        {
          Game_StartJump(player_y,
                         GAME_JUMP_HEIGHT,
                         &player_y_fixed,
                         &player_velocity_y,
                         &jump_apex_y,
                         &jump_limit_active);
          jump_count = GAME_JUMP_COUNT_SECOND_READY;
          jump_buffer_frames = 0U;
          on_ground = 0U;
        }
        /* PA11 sólo funciona después de haber usado PA12. */
        else if (second_jump_just_pressed != 0U &&
                 jump_count == GAME_JUMP_COUNT_SECOND_READY)
        {
          Game_StartSecondJump(player_y,
                               horizontal_input,
                               Game_ButtonPressed(&button_up),
                               &player_y_fixed,
                               &player_velocity_x,
                               &player_velocity_y,
                               &jump_apex_y,
                               &jump_limit_active);
          jump_count = GAME_JUMP_COUNT_EXHAUSTED;
          jump_buffer_frames = 0U;
          on_ground = 0U;
        }

        if (jump_buffer_frames != 0U)
        {
          jump_buffer_frames--;
        }

        Game_ApplyGravity(on_ground, &player_velocity_y);

        Game_MoveHorizontal(&player_x,
                            player_y,
                            &player_x_fixed,
                            &player_velocity_x);
        wall_side = Game_GetWallSide(player_x, player_y);
        Game_ApplyWallSlide(on_ground, wall_side, &player_velocity_y);
        touched_hazard = Game_TouchesLevelColor(player_x,
                                                player_y,
                                                GAME_LEVEL_HAZARD_COLOR,
                                                GAME_LEVEL_HAZARD_REACH);
        touched_exit = Game_TouchesLevelColor(player_x,
                                              player_y,
                                              GAME_LEVEL_EXIT_COLOR,
                                              GAME_LEVEL_EXIT_REACH);
        Game_MoveVertical(player_x,
                          &player_y,
                          &player_y_fixed,
                          &player_velocity_y,
                          jump_apex_y,
                          &jump_limit_active);
        touched_hazard |= Game_TouchesLevelColor(player_x,
                                                 player_y,
                                                 GAME_LEVEL_HAZARD_COLOR,
                                                 GAME_LEVEL_HAZARD_REACH);
        touched_exit |= Game_TouchesLevelColor(player_x,
                                               player_y,
                                               GAME_LEVEL_EXIT_COLOR,
                                               GAME_LEVEL_EXIT_REACH);

        if (touched_hazard != 0U)
        {
          Game_ErasePlayer(previous_player_x, previous_player_y);
          Game_ResetPlayerPosition(&player_x,
                                   &player_y,
                                   &player_x_fixed,
                                   &player_y_fixed,
                                   &player_velocity_x,
                                   &player_velocity_y,
                                   &jump_apex_y,
                                   &jump_limit_active);
          jump_count = GAME_JUMP_COUNT_NORMAL_READY;
          jump_buffer_frames = 0U;
          wall_jump_lock_frames = 0U;
          wall_jump_direction = 0;
          was_on_ground = Game_IsOnGround(player_x, player_y);
          player_animation_frame = 0U;
          player_animation_updates = 0U;
          player_facing_left = 0U;
          Game_DrawPlayer(player_x,
                          player_y,
                          player_animation_frame,
                          player_facing_left,
                          (was_on_ground == 0U) ? 1U : 0U);
          continue;
        }

        if (touched_exit != 0U && game_current_level == GAME_LEVEL_1)
        {
          game_current_level = GAME_LEVEL_2;
          Game_ResetPlayerPosition(&player_x,
                                   &player_y,
                                   &player_x_fixed,
                                   &player_y_fixed,
                                   &player_velocity_x,
                                   &player_velocity_y,
                                   &jump_apex_y,
                                   &jump_limit_active);
          jump_count = GAME_JUMP_COUNT_NORMAL_READY;
          jump_buffer_frames = 0U;
          wall_jump_lock_frames = 0U;
          wall_jump_direction = 0;
          was_on_ground = 0U;
          player_animation_frame = 0U;
          player_animation_updates = 0U;
          player_facing_left = 0U;
          Game_UnloadLevel();
          game_screen_drawn = 0U;
          continue;
        }

        player_airborne = (Game_IsOnGround(player_x, player_y) == 0U)
                              ? 1U
                              : 0U;

        /* El movimiento vertical puede aterrizar dentro de esta actualización. */
        if (player_airborne == 0U && on_ground == 0U)
        {
          jump_count = GAME_JUMP_COUNT_NORMAL_READY;
          jump_limit_active = 0U;
        }

        was_on_ground = (player_airborne == 0U) ? 1U : 0U;
        player_moved = (player_x != previous_player_x ||
                        player_y != previous_player_y) ? 1U : 0U;

        if (player_x < previous_player_x)
        {
          player_facing_left = 1U;
        }
        else if (player_x > previous_player_x)
        {
          player_facing_left = 0U;
        }

        if (player_moved != 0U) {
          if (player_airborne != 0U)
          {
            player_animation_frame = 0U;
            player_animation_updates = 0U;
          }
          else if (player_x != previous_player_x)
          {
            player_animation_updates++;
            if (player_animation_updates >= GAME_PLAYER_ANIMATION_STEP_UPDATES)
            {
              player_animation_updates = 0U;
              player_animation_frame = (uint8_t)(
                  (player_animation_frame + 1U) % CELESTE_J1_FRAME_COUNT);
            }
          }
          else
          {
            player_animation_frame = 0U;
            player_animation_updates = 0U;
          }
          Game_ErasePlayer(previous_player_x, previous_player_y);
          Game_DrawPlayer(player_x,
                          player_y,
                          player_animation_frame,
                          player_facing_left,
                          player_airborne);
        }
      }

      if (game_started == 0U &&
          Nieve_Update(&nieve,
                       &lcd,
                       start_background,
                       START_BACKGROUND_WIDTH,
                       START_BACKGROUND_HEIGHT,
                       HAL_GetTick()) != 0U &&
          prompt_visible != 0U) {
        StartScreen_DrawPrompt();
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

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, LCD_RST_Pin|LCD_D1_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, LCD_RD_Pin|LCD_WR_Pin|LCD_RS_Pin|LCD_D7_Pin
                          |LCD_D0_Pin|LCD_D2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LCD_CS_Pin|LCD_D6_Pin|LCD_D3_Pin|LCD_D5_Pin
                          |LCD_D4_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(SD_SS_GPIO_Port, SD_SS_Pin, GPIO_PIN_SET);

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

  /*Configure GPIO pins : LCD_CS_Pin LCD_D6_Pin LCD_D3_Pin LCD_D5_Pin
                           LCD_D4_Pin SD_SS_Pin */
  GPIO_InitStruct.Pin = LCD_CS_Pin|LCD_D6_Pin|LCD_D3_Pin|LCD_D5_Pin
                          |LCD_D4_Pin|SD_SS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : btn_salto2_Pin btn_salto_Pin */
  GPIO_InitStruct.Pin = btn_salto2_Pin|btn_salto_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* Los cinco botones ya son configurados por CubeMX. */
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART2)
  {
    UART_QueuePush(uart_rx_byte);
    (void)HAL_UART_Receive_IT(&huart2, &uart_rx_byte, 1U);
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART2)
  {
    (void)HAL_UART_Receive_IT(&huart2, &uart_rx_byte, 1U);
  }
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
