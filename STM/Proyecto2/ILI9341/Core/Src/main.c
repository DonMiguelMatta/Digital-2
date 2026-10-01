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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ili9341.h"
#include "level_1_texture.h"
#include "level_background.h"
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
#define GAME_PLAYER_START_X 15U
#define GAME_PLAYER_START_Y 230U
#define GAME_UPDATE_MS 20U
#define GAME_FIXED_POINT_ONE 256L
#define GAME_PLAYER_X_MAX (ILI9341_WIDTH - GAME_PLAYER_WIDTH)
#define GAME_PLAYER_Y_MAX (ILI9341_HEIGHT - GAME_PLAYER_HEIGHT)
#define GAME_PLAYER_START_X_FIXED \
  ((int32_t)GAME_PLAYER_START_X * GAME_FIXED_POINT_ONE)
#define GAME_PLAYER_START_Y_FIXED \
  ((int32_t)GAME_PLAYER_START_Y * GAME_FIXED_POINT_ONE)
#define GAME_HORIZONTAL_MAX_SPEED (1L * GAME_FIXED_POINT_ONE)
#define GAME_HORIZONTAL_GROUND_ACCEL 96L
#define GAME_HORIZONTAL_AIR_ACCEL 64L
#define GAME_HORIZONTAL_DECEL 40L
#define GAME_JUMP_COUNT_NORMAL_READY 0U
#define GAME_JUMP_COUNT_SECOND_READY 1U
#define GAME_JUMP_COUNT_EXHAUSTED 2U
#define GAME_JUMP_HEIGHT (2U * GAME_PLAYER_HEIGHT)
#define GAME_SECOND_JUMP_HEIGHT (3U * GAME_PLAYER_HEIGHT)
#define GAME_JUMP_SPEED (-888L)
#define GAME_SECOND_JUMP_SPEED (-1080L)
#define GAME_SECOND_JUMP_SIDE_SPEED (3L * GAME_FIXED_POINT_ONE)
#define GAME_SECOND_JUMP_DIAGONAL_SPEED (2L * GAME_FIXED_POINT_ONE)
#define GAME_GRAVITY 51L
#define GAME_APEX_GRAVITY 26L
#define GAME_APEX_SPEED 48L
#define GAME_MAX_FALL_SPEED (384L)

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

static uint16_t Game_PlayerColor(uint8_t animation_frame)
{
  switch (animation_frame & 0x03U)
  {
    case 0U:
      return 0xF800U;
    case 1U:
      return 0xFD20U;
    case 2U:
      return 0xF81FU;
    default:
      return 0xFFE0U;
  }
}

static void Game_DrawPlayer(uint16_t x, uint16_t y, uint8_t animation_frame)
{
  ILI9341_FillRect(&lcd,
                   x,
                   y,
                   GAME_PLAYER_WIDTH,
                   GAME_PLAYER_HEIGHT,
                   Game_PlayerColor(animation_frame));
}

static uint16_t Game_LevelTexturePixel(uint16_t x, uint16_t y)
{
  uint32_t pixel_index = (uint32_t)y * LEVEL_1_TEXTURE_WIDTH + x;
  uint32_t packed_color = level_1_texture[pixel_index / 2U];

  return ((pixel_index & 1U) == 0U) ? (uint16_t)(packed_color >> 16U)
                                     : (uint16_t)packed_color;
}

static uint8_t Game_IsSolidPixel(int32_t x, int32_t y)
{
  if (x < 0L || x >= (int32_t)LEVEL_1_TEXTURE_WIDTH || y < 0L ||
      y >= (int32_t)LEVEL_1_TEXTURE_HEIGHT)
  {
    return 1U;
  }

  return (Game_LevelTexturePixel((uint16_t)x, (uint16_t)y) !=
          LEVEL_1_TEXTURE_TRANSPARENT_COLOR)
             ? 1U
             : 0U;
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

/* La altura se mide desde el punto exacto donde comienza el salto. */
static void Game_StartJump(uint16_t player_y,
                           uint16_t jump_height,
                           int32_t jump_speed,
                           int32_t *player_y_fixed,
                           int32_t *player_velocity_y,
                           uint16_t *jump_apex_y,
                           uint8_t *jump_limit_active)
{
  *player_y_fixed = (int32_t)player_y * GAME_FIXED_POINT_ONE;
  *player_velocity_y = jump_speed;
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
                 GAME_SECOND_JUMP_SPEED,
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

static void Game_ErasePlayer(uint16_t x, uint16_t y)
{
  ILI9341_DrawPackedBitmapRGB565KeyedRegion(&lcd,
                                             x,
                                             y,
                                             GAME_PLAYER_WIDTH,
                                             GAME_PLAYER_HEIGHT,
                                             level_background,
                                             level_1_texture,
                                             LEVEL_1_TEXTURE_WIDTH,
                                             x,
                                             y,
                                             LEVEL_1_TEXTURE_TRANSPARENT_COLOR);
}

static void Game_DrawScene(uint16_t player_x,
                           uint16_t player_y,
                           uint8_t animation_frame)
{
  ILI9341_DrawPackedBitmapRGB565Keyed(&lcd,
                                       0U,
                                       0U,
                                       LEVEL_1_TEXTURE_WIDTH,
                                       LEVEL_1_TEXTURE_HEIGHT,
                                       level_background,
                                       level_1_texture,
                                       LEVEL_1_TEXTURE_TRANSPARENT_COLOR);
  Game_DrawPlayer(player_x, player_y, animation_frame);
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
  uint8_t jump_count = GAME_JUMP_COUNT_NORMAL_READY;
  uint8_t was_on_ground = 0U;
  uint8_t jump_limit_active = 0U;
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
        game_started = 1U;
        continue;
      }

      if (game_started != 0U && game_screen_drawn == 0U) {
        Game_DrawScene(player_x, player_y, player_animation_frame);
        game_screen_drawn = 1U;
        (void)Game_ButtonTakePress(&button_jump);
        (void)Game_ButtonTakePress(&button_second_jump);
        was_on_ground = Game_IsOnGround(player_x, player_y);
        last_game_update_ms = HAL_GetTick();
        continue;
      }

      if (game_started != 0U &&
          (HAL_GetTick() - last_game_update_ms) >= GAME_UPDATE_MS) {
        uint16_t previous_player_x = player_x;
        uint16_t previous_player_y = player_y;
        uint8_t player_moved;
        uint8_t jump_just_pressed = Game_ButtonTakePress(&button_jump);
        uint8_t second_jump_just_pressed =
            Game_ButtonTakePress(&button_second_jump);
        uint8_t on_ground;
        int8_t horizontal_input;

        last_game_update_ms = HAL_GetTick();

        if (Game_ButtonPressed(&button_right) != 0U) {
          horizontal_input = 1;
        } else if (Game_ButtonPressed(&button_left) != 0U) {
          horizontal_input = -1;
        } else {
          horizontal_input = 0;
        }

        on_ground = Game_IsOnGround(player_x, player_y);
        if (on_ground != 0U && was_on_ground == 0U)
        {
          jump_count = GAME_JUMP_COUNT_NORMAL_READY;
          jump_limit_active = 0U;
        }

        Game_UpdateHorizontalVelocity(horizontal_input,
                                      on_ground,
                                      &player_velocity_x);

        /* PA12 habilita el segundo salto sin consultar el suelo. */
        if (jump_just_pressed != 0U &&
            jump_count == GAME_JUMP_COUNT_NORMAL_READY)
        {
          Game_StartJump(player_y,
                         GAME_JUMP_HEIGHT,
                         GAME_JUMP_SPEED,
                         &player_y_fixed,
                         &player_velocity_y,
                         &jump_apex_y,
                         &jump_limit_active);
          jump_count = GAME_JUMP_COUNT_SECOND_READY;
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
          on_ground = 0U;
        }

        Game_ApplyGravity(on_ground, &player_velocity_y);

        Game_MoveHorizontal(&player_x,
                            player_y,
                            &player_x_fixed,
                            &player_velocity_x);
        Game_MoveVertical(player_x,
                          &player_y,
                          &player_y_fixed,
                          &player_velocity_y,
                          jump_apex_y,
                          &jump_limit_active);
        was_on_ground = on_ground;
        player_moved = (player_x != previous_player_x ||
                        player_y != previous_player_y) ? 1U : 0U;

        if (player_moved != 0U) {
          player_animation_frame++;
          Game_ErasePlayer(previous_player_x, previous_player_y);
          Game_DrawPlayer(player_x, player_y, player_animation_frame);
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
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
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
                          |LCD_D4_Pin|SD_SS_Pin, GPIO_PIN_RESET);

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
