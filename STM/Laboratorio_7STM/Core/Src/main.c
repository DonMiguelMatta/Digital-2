/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include <string.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define TIM6_INPUT_CLOCK_HZ  84000000ULL
#define TIM3_COUNTER_CLOCK_HZ 1000000ULL
#define SINE_LUT_SIZE        128U
#define NOTE_GAP_MS          60U
#define PWM_NOTE_GAP_MS      30U
#define TIM6_ARR_MIN         1U
#define TIM6_ARR_MAX         65535U
#define TIM3_ARR_MIN         1U
#define TIM3_ARR_MAX         65535U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#define ARRAY_LENGTH(array)  (sizeof(array) / sizeof((array)[0]))

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
DAC_HandleTypeDef hdac;
DMA_HandleTypeDef hdma_dac1;

TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim6;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
/* Onda senoidal de 12 bits y 128 muestras. */
static const uint16_t sine_lut[128] =
{
  2048, 2148, 2248, 2348, 2447, 2545, 2642, 2737,
  2831, 2923, 3013, 3100, 3185, 3267, 3346, 3423,
  3495, 3565, 3630, 3692, 3750, 3804, 3853, 3898,
  3939, 3975, 4007, 4034, 4056, 4073, 4085, 4093,
  4095, 4093, 4085, 4073, 4056, 4034, 4007, 3975,
  3939, 3898, 3853, 3804, 3750, 3692, 3630, 3565,
  3495, 3423, 3346, 3267, 3185, 3100, 3013, 2923,
  2831, 2737, 2642, 2545, 2447, 2348, 2248, 2148,
  2048, 1947, 1847, 1747, 1648, 1550, 1453, 1358,
  1264, 1172, 1082,  995,  910,  828,  749,  672,
   600,  530,  465,  403,  345,  291,  242,  197,
   156,  120,   88,   61,   39,   22,   10,    2,
     0,    2,   10,   22,   39,   61,   88,  120,
   156,  197,  242,  291,  345,  403,  465,  530,
   600,  672,  749,  828,  910,  995, 1082, 1172,
  1264, 1358, 1453, 1550, 1648, 1747, 1847, 1947
};

/* Zelda's Lullaby simplificada, frecuencias en Hz. */
static const uint16_t zelda_notes_hz[] =
{
  494, 587, 440, 392, 440, 494, 587, 440,
  494, 587, 880, 784, 587, 523, 494, 440
};

/* Duraciones de las notas en milisegundos. */
static const uint16_t zelda_durations_ms[] =
{
  450, 450, 750, 450, 450, 750, 450, 750,
  450, 450, 750, 450, 450, 450, 450, 900
};

/* Tema de Mario simplificado, frecuencias en Hz. */
static const uint16_t mario_notes_hz[] =
{
  659, 659, 659, 523, 659, 784, 392, 523,
  392, 330, 440, 494, 466, 440, 392, 659,
  784, 880, 698, 784, 659, 523, 587, 494
};

/* Duraciones de Mario en milisegundos. */
static const uint16_t mario_durations_ms[] =
{
  180, 180, 360, 180, 180, 360, 360, 360,
  360, 360, 300, 300, 180, 300, 240, 240,
  240, 300, 180, 180, 180, 180, 180, 300
};

/* Tema de Tetris simplificado para PWM, frecuencias en Hz. */
static const uint16_t tetris_notes_hz[] =
{
  659, 494, 523, 587, 523, 494, 440, 440,
  523, 659, 587, 523, 494, 494, 523, 587,
  659, 523, 440, 440, 587, 698, 880, 784,
  698, 659, 523, 659, 587, 523, 494, 494,
  523, 587, 659, 523, 440, 440
};

static const uint16_t tetris_durations_ms[] =
{
  300, 150, 150, 300, 150, 150, 300, 150,
  150, 300, 150, 150, 300, 150, 150, 300,
  300, 300, 300, 450, 300, 150, 300, 150,
  150, 300, 150, 300, 150, 150, 300, 150,
  150, 300, 300, 300, 300, 450
};

/* Melodia breve de estilo himno para PWM. */
static const uint16_t charly_notes_hz[] =
{
  523, 659, 784, 784, 880, 784, 659, 523,
  698, 880, 784, 659, 587, 659, 523, 523
};

static const uint16_t charly_durations_ms[] =
{
  300, 300, 450, 300, 300, 300, 300, 500,
  300, 300, 450, 300, 300, 300, 300, 700
};

_Static_assert(ARRAY_LENGTH(zelda_notes_hz) ==
               ARRAY_LENGTH(zelda_durations_ms),
               "Los arreglos de Zelda deben tener el mismo tamano");
_Static_assert(ARRAY_LENGTH(mario_notes_hz) ==
               ARRAY_LENGTH(mario_durations_ms),
               "Los arreglos de Mario deben tener el mismo tamano");
_Static_assert(ARRAY_LENGTH(tetris_notes_hz) ==
               ARRAY_LENGTH(tetris_durations_ms),
               "Los arreglos de Tetris deben tener el mismo tamano");
_Static_assert(ARRAY_LENGTH(charly_notes_hz) ==
               ARRAY_LENGTH(charly_durations_ms),
               "Los arreglos de Charly deben tener el mismo tamano");

static uint8_t uart_command;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_DAC_Init(void);
static void MX_TIM6_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM3_Init(void);
/* USER CODE BEGIN PFP */
static void uart_send(const char *text);
static void show_menu(void);
static uint32_t calculate_tim6_arr(uint16_t frequency_hz);
static void start_note(uint16_t frequency_hz);
static void stop_note(void);
static void play_zelda(void);
static void play_mario(void);
static uint32_t calculate_tim3_arr(uint16_t frequency_hz);
static void start_pwm_note(uint16_t frequency_hz);
static void stop_pwm_note(void);
static void play_tetris_pwm(void);
static void play_charly_pwm(void);

/* USER CODE END PFP */

/* Private user code ---------------------s------------------------------------*/
/* USER CODE BEGIN 0 */
/* Envia una cadena completa por USART2. */
static void uart_send(const char *text)
{
  if (HAL_UART_Transmit(&huart2, (uint8_t *)text,
                        (uint16_t)strlen(text), HAL_MAX_DELAY) != HAL_OK)
  {
    Error_Handler();
  }
}

/* Muestra las opciones disponibles. */
static void show_menu(void)
{
  uart_send("\r\n1. Zelda - Ocarina of Time\r\n"
            "2. Mario Bros Theme\r\n"
            "3. Tetris [PWM]\r\n"
            "4. We Are Charly Kirk [PWM]\r\n"
            "M. Mostrar nuevamente el menu\r\n"
            "\r\n"
            "Ingrese 1, 2, 3 o 4 y presione Enter: ");
}

/* Calcula ARR con redondeo y limita el resultado a 16 bits. */
static uint32_t calculate_tim6_arr(uint16_t frequency_hz)
{
  uint64_t denominator;
  uint64_t timer_counts;
  uint64_t arr;

  if (frequency_hz == 0U)
  {
    return TIM6_ARR_MAX;
  }

  denominator = (uint64_t)SINE_LUT_SIZE * (uint64_t)frequency_hz;
  timer_counts = (TIM6_INPUT_CLOCK_HZ + (denominator / 2ULL)) /
                 denominator;

  if (timer_counts <= ((uint64_t)TIM6_ARR_MIN + 1ULL))
  {
    return TIM6_ARR_MIN;
  }

  arr = timer_counts - 1ULL;
  if (arr > TIM6_ARR_MAX)
  {
    arr = TIM6_ARR_MAX;
  }

  return (uint32_t)arr;
}

static void start_note(uint16_t frequency_hz)
{
  uint32_t arr = calculate_tim6_arr(frequency_hz);

  if (HAL_TIM_Base_Stop(&htim6) != HAL_OK)
  {
    Error_Handler();
  }

  __HAL_TIM_SET_AUTORELOAD(&htim6, arr);
  __HAL_TIM_SET_COUNTER(&htim6, 0U);
  __HAL_TIM_CLEAR_FLAG(&htim6, TIM_FLAG_UPDATE);


  if (HAL_TIM_Base_Start(&htim6) != HAL_OK)
  {
    Error_Handler();
  }
}


static void stop_note(void)
{
  if (HAL_TIM_Base_Stop(&htim6) != HAL_OK)
  {
    Error_Handler();
  }
}


static void play_zelda(void)
{
  size_t note_index;

  uart_send("\r\nReproduciendo Zelda - Ocarina of Time...\r\n");

  for (note_index = 0U; note_index < ARRAY_LENGTH(zelda_notes_hz);
       note_index++)
  {
    start_note(zelda_notes_hz[note_index]);
    HAL_Delay(zelda_durations_ms[note_index]);
    stop_note();

    if ((note_index + 1U) < ARRAY_LENGTH(zelda_notes_hz))
    {
      HAL_Delay(NOTE_GAP_MS);
    }
  }

  uart_send("\r\nReproduccion finalizada.\r\n");
}


static void play_mario(void)
{
  size_t note_index;

  uart_send("\r\nReproduciendo Mario Bros Theme...\r\n");

  for (note_index = 0U; note_index < ARRAY_LENGTH(mario_notes_hz);
       note_index++)
  {
    start_note(mario_notes_hz[note_index]);
    HAL_Delay(mario_durations_ms[note_index]);
    stop_note();

    if ((note_index + 1U) < ARRAY_LENGTH(mario_notes_hz))
    {
      HAL_Delay(NOTE_GAP_MS);
    }
  }

  uart_send("\r\nReproduccion finalizada.\r\n");
}

/* Calcula ARR para TIM3 con un contador de 1 MHz. */
static uint32_t calculate_tim3_arr(uint16_t frequency_hz)
{
  uint64_t timer_counts;
  uint64_t arr;

  if (frequency_hz == 0U)
  {
    return TIM3_ARR_MAX;
  }

  timer_counts = (TIM3_COUNTER_CLOCK_HZ +
                  ((uint64_t)frequency_hz / 2ULL)) /
                 (uint64_t)frequency_hz;

  if (timer_counts <= ((uint64_t)TIM3_ARR_MIN + 1ULL))
  {
    return TIM3_ARR_MIN;
  }

  arr = timer_counts - 1ULL;
  if (arr > TIM3_ARR_MAX)
  {
    arr = TIM3_ARR_MAX;
  }

  return (uint32_t)arr;
}

/* Configura TIM3 CH1 con ciclo de trabajo del 50 por ciento. */
static void start_pwm_note(uint16_t frequency_hz)
{
  uint32_t arr = calculate_tim3_arr(frequency_hz);
  uint32_t pulse = (arr + 1U) / 2U;

  if (HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }

  __HAL_TIM_SET_AUTORELOAD(&htim3, arr);
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, pulse);
  __HAL_TIM_SET_COUNTER(&htim3, 0U);
  if (HAL_TIM_GenerateEvent(&htim3, TIM_EVENTSOURCE_UPDATE) != HAL_OK)
  {
    Error_Handler();
  }
  __HAL_TIM_CLEAR_FLAG(&htim3, TIM_FLAG_UPDATE);

  if (HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/* Detiene la salida PWM de PA6. */
static void stop_pwm_note(void)
{
  if (HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/* Reproduce el tema de Tetris por PWM. */
static void play_tetris_pwm(void)
{
  size_t note_index;

  uart_send("\r\nReproduciendo Tetris por PWM...\r\n");

  for (note_index = 0U; note_index < ARRAY_LENGTH(tetris_notes_hz);
       note_index++)
  {
    start_pwm_note(tetris_notes_hz[note_index]);
    HAL_Delay(tetris_durations_ms[note_index]);
    stop_pwm_note();

    if ((note_index + 1U) < ARRAY_LENGTH(tetris_notes_hz))
    {
      HAL_Delay(PWM_NOTE_GAP_MS);
    }
  }

  uart_send("\r\nReproduccion finalizada.\r\n");
}

/* Reproduce We Are Charly Kirk por PWM. */
static void play_charly_pwm(void)
{
  size_t note_index;

  uart_send("\r\nReproduciendo We Are Charly Kirk por PWM...\r\n");

  for (note_index = 0U; note_index < ARRAY_LENGTH(charly_notes_hz);
       note_index++)
  {
    start_pwm_note(charly_notes_hz[note_index]);
    HAL_Delay(charly_durations_ms[note_index]);
    stop_pwm_note();

    if ((note_index + 1U) < ARRAY_LENGTH(charly_notes_hz))
    {
      HAL_Delay(PWM_NOTE_GAP_MS);
    }
  }

  uart_send("\r\nReproduccion finalizada.\r\n");
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

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
  MX_DMA_Init();
  MX_DAC_Init();
  MX_TIM6_Init();
  MX_USART2_UART_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */

  if (HAL_DAC_Start_DMA(&hdac,
                        DAC_CHANNEL_1,
                        (uint32_t *)sine_lut,
                        128,
                        DAC_ALIGN_12B_R) != HAL_OK)
  {
    Error_Handler();
  }

  show_menu();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    if (HAL_UART_Receive(&huart2, &uart_command, 1U,
                         HAL_MAX_DELAY) != HAL_OK)
    {
      Error_Handler();
    }

    if ((uart_command == '\r') || (uart_command == '\n'))
    {
      continue;
    }

    switch (uart_command)
    {
      case '1':
        play_zelda();
        show_menu();
        break;

      case '2':
        play_mario();
        show_menu();
        break;

      case '3':
        play_tetris_pwm();
        show_menu();
        break;

      case '4':
        play_charly_pwm();
        show_menu();
        break;

      case 'M':
      case 'm':
        show_menu();
        break;

      default:
        uart_send("\r\nComando inv\xC3\xA1lido.\r\n");
        show_menu();
        break;
    }
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
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
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
  * @brief DAC Initialization Function
  * @param None
  * @retval None
  */
static void MX_DAC_Init(void)
{

  /* USER CODE BEGIN DAC_Init 0 */

  /* USER CODE END DAC_Init 0 */

  DAC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN DAC_Init 1 */

  /* USER CODE END DAC_Init 1 */

  /** DAC Initialization
  */
  hdac.Instance = DAC;
  if (HAL_DAC_Init(&hdac) != HAL_OK)
  {
    Error_Handler();
  }

  /** DAC channel OUT1 config
  */
  sConfig.DAC_Trigger = DAC_TRIGGER_T6_TRGO;
  sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
  if (HAL_DAC_ConfigChannel(&hdac, &sConfig, DAC_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN DAC_Init 2 */

  /* USER CODE END DAC_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 83;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 65535;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 500;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */
  HAL_TIM_MspPostInit(&htim3);

}

/**
  * @brief TIM6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM6_Init(void)
{

  /* USER CODE BEGIN TIM6_Init 0 */

  /* USER CODE END TIM6_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM6_Init 1 */

  /* USER CODE END TIM6_Init 1 */
  htim6.Instance = TIM6;
  htim6.Init.Prescaler = 0;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.Period = 1000-1;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM6_Init 2 */

  /* USER CODE END TIM6_Init 2 */

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
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);

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
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

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
  while (1)
  {
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
