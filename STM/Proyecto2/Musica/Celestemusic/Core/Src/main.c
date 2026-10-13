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
#include "../../main_menu.h"
#include "../../main_theme.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct
{
  const uint16_t *frequencies;
  const uint16_t *durations;
  uint16_t length;
  uint8_t command;
} MusicTrack_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define MUSIC_TIM6_CLOCK_HZ 84000000ULL
#define MUSIC_SINE_SAMPLE_COUNT 128U
#define MUSIC_TIM6_ARR_MIN 1U
#define MUSIC_TIM6_ARR_MAX 65535U
#define MUSIC_DAC_SILENCE 2048U
#define MUSIC_VOLUME_PERCENT 50U
#define MUSIC_COMMAND_NONE 0U
#define MUSIC_COMMAND_MENU ((uint8_t)'M')
#define MUSIC_COMMAND_THEME ((uint8_t)'T')

#if MUSIC_VOLUME_PERCENT > 100U
#error "MUSIC_VOLUME_PERCENT debe estar entre 0 y 100"
#endif

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
DAC_HandleTypeDef hdac;
DMA_HandleTypeDef hdma_dac1;

SPI_HandleTypeDef hspi2;

TIM_HandleTypeDef htim6;

/* USER CODE BEGIN PV */
/* Una vuelta completa de la onda que genera cada nota. */
static const uint16_t music_sine_lut[MUSIC_SINE_SAMPLE_COUNT] =
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

/* Conserva el centro del DAC y cambia solamente la amplitud. */
static uint16_t music_output_lut[MUSIC_SINE_SAMPLE_COUNT];

static const MusicTrack_t music_tracks[] =
{
  {main_menu_freqs, main_menu_durations, MAIN_MENU_LEN, MUSIC_COMMAND_MENU},
  {main_theme_freqs, main_theme_durations, MAIN_THEME_LEN, MUSIC_COMMAND_THEME}
};

static volatile uint8_t music_pending_command = MUSIC_COMMAND_NONE;
static const MusicTrack_t *music_active_track;
static uint8_t music_active_command = MUSIC_COMMAND_NONE;
static uint16_t music_note_index;
static uint32_t music_note_deadline_ms;
static uint8_t music_note_started;
static volatile uint32_t music_spi_valid_commands;
static volatile uint32_t music_spi_errors;
static volatile uint8_t music_spi_last_command;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_DAC_Init(void);
static void MX_TIM6_Init(void);
static void MX_SPI2_Init(void);
/* USER CODE BEGIN PFP */
static uint32_t Music_CalculateTimerPeriod(uint16_t frequency_hz);
static void Music_PrepareOutputLut(void);
static void Music_ReadSpi(void);
static void Music_SetFrequency(uint16_t frequency_hz);
static const MusicTrack_t *Music_FindTrack(uint8_t command);
static void Music_Select(uint8_t command, uint32_t now_ms);
static void Music_Update(uint32_t now_ms);
static void Music_Init(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* Prepara una onda con el volumen seleccionado, centrada en 2048. */
static void Music_PrepareOutputLut(void)
{
  uint16_t index;

  for (index = 0U; index < MUSIC_SINE_SAMPLE_COUNT; index++)
  {
    int32_t centered =
        (int32_t)music_sine_lut[index] - (int32_t)MUSIC_DAC_SILENCE;

    music_output_lut[index] = (uint16_t)(
        (int32_t)MUSIC_DAC_SILENCE +
        ((centered * (int32_t)MUSIC_VOLUME_PERCENT) / 100));
  }
}

/* Lee un byte de SPI2 sin detener la reproducción. */
static void Music_ReadSpi(void)
{
  uint32_t status = hspi2.Instance->SR;

  if ((status & SPI_SR_OVR) != 0U)
  {
    (void)(*(__IO uint8_t *)&hspi2.Instance->DR);
    (void)hspi2.Instance->SR; /* Leer DR y SR limpia el overrun. */
    music_spi_errors++;
    return;
  }

  if ((status & SPI_SR_RXNE) != 0U)
  {
    uint8_t command = *(__IO uint8_t *)&hspi2.Instance->DR;

    if (command == MUSIC_COMMAND_MENU ||
        command == MUSIC_COMMAND_THEME)
    {
      music_pending_command = command;
      music_spi_last_command = command;
      music_spi_valid_commands++;
    }
    else
    {
      music_spi_errors++;
    }
  }
}

/* Calcula el periodo que produce la frecuencia solicitada. */
static uint32_t Music_CalculateTimerPeriod(uint16_t frequency_hz)
{
  uint64_t divider;
  uint64_t timer_counts;

  if (frequency_hz == 0U)
  {
    return MUSIC_TIM6_ARR_MAX;
  }

  divider = (uint64_t)MUSIC_SINE_SAMPLE_COUNT * frequency_hz;
  timer_counts = (MUSIC_TIM6_CLOCK_HZ + (divider / 2ULL)) / divider;

  if (timer_counts <= ((uint64_t)MUSIC_TIM6_ARR_MIN + 1ULL))
  {
    return MUSIC_TIM6_ARR_MIN;
  }
  if ((timer_counts - 1ULL) > MUSIC_TIM6_ARR_MAX)
  {
    return MUSIC_TIM6_ARR_MAX;
  }

  return (uint32_t)(timer_counts - 1ULL);
}

/* Cambia la frecuencia sin detener el resto del programa. */
static void Music_SetFrequency(uint16_t frequency_hz)
{
  (void)HAL_TIM_Base_Stop(&htim6);

  if (frequency_hz == 0U)
  {
    (void)HAL_DAC_SetValue(&hdac,
                          DAC_CHANNEL_1,
                          DAC_ALIGN_12B_R,
                          MUSIC_DAC_SILENCE);
    if (HAL_TIM_GenerateEvent(&htim6, TIM_EVENTSOURCE_UPDATE) != HAL_OK)
    {
      Error_Handler();
    }
    return;
  }

  __HAL_TIM_SET_AUTORELOAD(&htim6,
                           Music_CalculateTimerPeriod(frequency_hz));
  __HAL_TIM_SET_COUNTER(&htim6, 0U);
  __HAL_TIM_CLEAR_FLAG(&htim6, TIM_FLAG_UPDATE);

  if (HAL_TIM_Base_Start(&htim6) != HAL_OK)
  {
    Error_Handler();
  }
}

/* Busca la canción asociada al byte recibido. */
static const MusicTrack_t *Music_FindTrack(uint8_t command)
{
  uint8_t index;

  for (index = 0U;
       index < (uint8_t)(sizeof(music_tracks) / sizeof(music_tracks[0]));
       index++)
  {
    if (music_tracks[index].command == command)
    {
      return &music_tracks[index];
    }
  }

  return 0;
}

/* Una canción nueva siempre comienza desde su primera nota. */
static void Music_Select(uint8_t command, uint32_t now_ms)
{
  const MusicTrack_t *track = Music_FindTrack(command);

  if (track == 0 || command == music_active_command)
  {
    return;
  }

  Music_SetFrequency(0U);
  music_active_track = track;
  music_active_command = command;
  music_note_index = 0U;
  music_note_deadline_ms = now_ms;
  music_note_started = 0U;

  /* El LED permite comprobar el cambio aun sin conectar la bocina. */
  HAL_GPIO_WritePin(LD2_GPIO_Port,
                    LD2_Pin,
                    command == MUSIC_COMMAND_THEME
                        ? GPIO_PIN_SET
                        : GPIO_PIN_RESET);
}

/* Avanza las notas usando tiempo, sin bloquear la recepción SPI. */
static void Music_Update(uint32_t now_ms)
{
  uint8_t pending_command = music_pending_command;

  if (pending_command != MUSIC_COMMAND_NONE)
  {
    music_pending_command = MUSIC_COMMAND_NONE;
    Music_Select(pending_command, now_ms);
  }

  if (music_active_track == 0)
  {
    Music_Select(MUSIC_COMMAND_MENU, now_ms);
  }

  if (music_note_started != 0U &&
      (int32_t)(now_ms - music_note_deadline_ms) < 0)
  {
    return;
  }

  if (music_note_started != 0U)
  {
    music_note_index++;
    if (music_note_index >= music_active_track->length)
    {
      music_note_index = 0U; /* Repite la canción completa. */
    }
  }

  Music_SetFrequency(music_active_track->frequencies[music_note_index]);
  music_note_deadline_ms =
      now_ms + music_active_track->durations[music_note_index];
  music_note_started = 1U;
}

/* Deja preparado el DAC y escucha el primer comando. */
static void Music_Init(void)
{
  Music_PrepareOutputLut();
  __HAL_SPI_ENABLE(&hspi2); /* El esclavo queda listo para recibir. */

  if (HAL_DAC_Start_DMA(&hdac,
                        DAC_CHANNEL_1,
                        (uint32_t *)music_output_lut,
                        MUSIC_SINE_SAMPLE_COUNT,
                        DAC_ALIGN_12B_R) != HAL_OK)
  {
    Error_Handler();
  }

  Music_Select(MUSIC_COMMAND_MENU, HAL_GetTick());
  Music_Update(HAL_GetTick());
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
  MX_SPI2_Init();
  /* USER CODE BEGIN 2 */
  Music_Init();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    Music_ReadSpi();
    Music_Update(HAL_GetTick());
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
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_SLAVE;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES_RXONLY;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_HARD_INPUT;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

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

  /*Configure GPIO pins : USART_TX_Pin USART_RX_Pin */
  GPIO_InitStruct.Pin = USART_TX_Pin|USART_RX_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

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
