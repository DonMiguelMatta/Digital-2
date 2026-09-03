/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Laboratorio 5 - Timers y UART
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/*
 * CLOCK CONFIGURATION
 *
 * SYSCLK = 96 MHz
 * HCLK   = 48 MHz
 * PCLK1  = 24 MHz
 *
 * APB1 Prescaler = 2
 * Timer Clock APB1 = 2 * PCLK1 = 48 MHz
 *
 * TIM2 Prescaler = 47
 *
 * TIM2 Counter Clock:
 *
 * 48 MHz / (47 + 1) = 1 MHz
 *
 * Por lo tanto:
 * 1 tick TIM2 = 1 us
 */
#define TIM2_COUNTER_HZ 1000000UL

/* UART cada 100 ms */
#define UART_PERIOD_MS 100U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

/* Input Capture TIM2 */
volatile uint32_t captura1 = 0;
volatile uint32_t captura2 = 0;
volatile uint32_t diferencia = 0;

volatile float frecuencia = 0.0f;

volatile uint8_t primeraCaptura = 0;
volatile uint8_t nuevaMedicion = 0;

/* Variable de diagnostico */
volatile uint32_t contadorCapturas = 0;

/* UART */
char mensaje[100];

uint32_t tiempoUART = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/

void SystemClock_Config(void);

static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM4_Init(void);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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

  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */

  MX_GPIO_Init();

  MX_TIM2_Init();

  MX_USART2_UART_Init();

  MX_TIM3_Init();

  MX_TIM4_Init();

  /* USER CODE BEGIN 2 */


  /* ==========================================================
   * TIM2
   * Input Capture para medir frecuencia por PA0
   * ========================================================== */

  if (HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }


  /* ==========================================================
   * TIM3
   *
   * PSC = 47999
   * ARR = 249
   *
   * Interrupcion cada 250 ms
   *
   * LED1:
   * 250 ms ON
   * 250 ms OFF
   *
   * Periodo = 500 ms
   * ========================================================== */

  if (HAL_TIM_Base_Start_IT(&htim3) != HAL_OK)
  {
    Error_Handler();
  }


  /* ==========================================================
   * TIM4
   *
   * PSC = 47999
   * ARR = 999
   *
   * Interrupcion cada 1 segundo
   *
   * LED2:
   * 1 s ON
   * 1 s OFF
   *
   * Periodo = 2 segundos
   * ========================================================== */

  if (HAL_TIM_Base_Start_IT(&htim4) != HAL_OK)
  {
    Error_Handler();
  }


  /* Estado inicial de los LEDs */

  HAL_GPIO_WritePin(
      Led1_GPIO_Port,
      Led1_Pin,
      GPIO_PIN_RESET
  );

  HAL_GPIO_WritePin(
      Led2_GPIO_Port,
      Led2_Pin,
      GPIO_PIN_RESET
  );


  /* Mensaje inicial UART */

  const char inicio[] =
      "\r\n"
      "Laboratorio 5 - STM32F446RE\r\n"
      "TIM2: Medidor de frecuencia\r\n"
      "TIM3: LED1 - Periodo 500 ms\r\n"
      "TIM4: LED2 - Periodo 2 s\r\n"
      "\r\n";


  HAL_UART_Transmit(
      &huart2,
      (uint8_t *)inicio,
      (uint16_t)strlen(inicio),
      HAL_MAX_DELAY
  );


  tiempoUART = HAL_GetTick();


  /* USER CODE END 2 */


  /* Infinite loop */

  /* USER CODE BEGIN WHILE */

  while (1)
  {

    /* ==========================================================
     * UART
     *
     * Enviar una lectura cada 100 ms.
     *
     * ========================================================== */

    if ((HAL_GetTick() - tiempoUART) >= UART_PERIOD_MS)
    {

      tiempoUART = HAL_GetTick();


      if (nuevaMedicion)
      {

        float frecuenciaLocal = frecuencia;

        nuevaMedicion = 0;


        int longitud = snprintf(
            mensaje,
            sizeof(mensaje),
            "Frecuencia: %.2f Hz\r\n",
            frecuenciaLocal
        );


        if (longitud > 0)
        {

          HAL_UART_Transmit(
              &huart2,
              (uint8_t *)mensaje,
              (uint16_t)longitud,
              100
          );

        }

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


  /* Configure the main internal regulator output voltage */

  __HAL_RCC_PWR_CLK_ENABLE();


  __HAL_PWR_VOLTAGESCALING_CONFIG(
      PWR_REGULATOR_VOLTAGE_SCALE3
  );


  /* Initializes the RCC Oscillators */

  RCC_OscInitStruct.OscillatorType =
      RCC_OSCILLATORTYPE_HSI;


  RCC_OscInitStruct.HSIState =
      RCC_HSI_ON;


  RCC_OscInitStruct.HSICalibrationValue =
      RCC_HSICALIBRATION_DEFAULT;


  RCC_OscInitStruct.PLL.PLLState =
      RCC_PLL_ON;


  RCC_OscInitStruct.PLL.PLLSource =
      RCC_PLLSOURCE_HSI;


  RCC_OscInitStruct.PLL.PLLM = 8;

  RCC_OscInitStruct.PLL.PLLN = 96;


  RCC_OscInitStruct.PLL.PLLP =
      RCC_PLLP_DIV2;


  RCC_OscInitStruct.PLL.PLLQ = 2;

  RCC_OscInitStruct.PLL.PLLR = 2;


  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }


  /* Initializes CPU, AHB and APB buses clocks */

  RCC_ClkInitStruct.ClockType =
      RCC_CLOCKTYPE_HCLK |
      RCC_CLOCKTYPE_SYSCLK |
      RCC_CLOCKTYPE_PCLK1 |
      RCC_CLOCKTYPE_PCLK2;


  RCC_ClkInitStruct.SYSCLKSource =
      RCC_SYSCLKSOURCE_PLLCLK;


  RCC_ClkInitStruct.AHBCLKDivider =
      RCC_SYSCLK_DIV2;


  RCC_ClkInitStruct.APB1CLKDivider =
      RCC_HCLK_DIV2;


  RCC_ClkInitStruct.APB2CLKDivider =
      RCC_HCLK_DIV1;


  if (HAL_RCC_ClockConfig(
          &RCC_ClkInitStruct,
          FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }

}


/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */


  TIM_ClockConfigTypeDef sClockSourceConfig = {0};

  TIM_MasterConfigTypeDef sMasterConfig = {0};

  TIM_IC_InitTypeDef sConfigIC = {0};


  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */


  htim2.Instance = TIM2;


  /*
   * TIM2 Clock = 48 MHz
   *
   * PSC = 47
   *
   * Counter:
   *
   * 48 MHz / 48 = 1 MHz
   */
  htim2.Init.Prescaler = 47;


  htim2.Init.CounterMode =
      TIM_COUNTERMODE_UP;


  /*
   * TIM2 es de 32 bits
   */
  htim2.Init.Period =
      4294967295;


  htim2.Init.ClockDivision =
      TIM_CLOCKDIVISION_DIV1;


  htim2.Init.AutoReloadPreload =
      TIM_AUTORELOAD_PRELOAD_ENABLE;


  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }


  sClockSourceConfig.ClockSource =
      TIM_CLOCKSOURCE_INTERNAL;


  if (HAL_TIM_ConfigClockSource(
          &htim2,
          &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }


  if (HAL_TIM_IC_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }


  sMasterConfig.MasterOutputTrigger =
      TIM_TRGO_RESET;


  sMasterConfig.MasterSlaveMode =
      TIM_MASTERSLAVEMODE_DISABLE;


  if (HAL_TIMEx_MasterConfigSynchronization(
          &htim2,
          &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }


  /* Input Capture CH1 */

  sConfigIC.ICPolarity =
      TIM_INPUTCHANNELPOLARITY_RISING;


  sConfigIC.ICSelection =
      TIM_ICSELECTION_DIRECTTI;


  sConfigIC.ICPrescaler =
      TIM_ICPSC_DIV1;


  sConfigIC.ICFilter = 0;


  if (HAL_TIM_IC_ConfigChannel(
          &htim2,
          &sConfigIC,
          TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }


  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

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

  TIM_IC_InitTypeDef sConfigIC = {0};


  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */


  htim3.Instance = TIM3;


  /*
   * TIM3 Clock = 48 MHz
   *
   *
   */
  htim3.Init.Prescaler = 47999;


  htim3.Init.CounterMode =
      TIM_COUNTERMODE_UP;


  /*
   * 1000 Hz / (249 + 1)
   *
   * = 4 Hz
   *
   * Interrupcion cada 250 ms
   */
  htim3.Init.Period = 249;


  htim3.Init.ClockDivision =
      TIM_CLOCKDIVISION_DIV1;


  htim3.Init.AutoReloadPreload =
      TIM_AUTORELOAD_PRELOAD_DISABLE;


  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }


  sClockSourceConfig.ClockSource =
      TIM_CLOCKSOURCE_INTERNAL;


  if (HAL_TIM_ConfigClockSource(
          &htim3,
          &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }



  if (HAL_TIM_IC_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }


  sMasterConfig.MasterOutputTrigger =
      TIM_TRGO_RESET;


  sMasterConfig.MasterSlaveMode =
      TIM_MASTERSLAVEMODE_DISABLE;


  if (HAL_TIMEx_MasterConfigSynchronization(
          &htim3,
          &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }



  sConfigIC.ICPolarity =
      TIM_INPUTCHANNELPOLARITY_RISING;


  sConfigIC.ICSelection =
      TIM_ICSELECTION_DIRECTTI;


  sConfigIC.ICPrescaler =
      TIM_ICPSC_DIV1;


  sConfigIC.ICFilter = 0;


  if (HAL_TIM_IC_ConfigChannel(
          &htim3,
          &sConfigIC,
          TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }


  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}


/**
  * @brief TIM4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM4_Init(void)
{

  /* USER CODE BEGIN TIM4_Init 0 */

  /* USER CODE END TIM4_Init 0 */


  TIM_ClockConfigTypeDef sClockSourceConfig = {0};

  TIM_MasterConfigTypeDef sMasterConfig = {0};


  /* USER CODE BEGIN TIM4_Init 1 */

  /* USER CODE END TIM4_Init 1 */


  htim4.Instance = TIM4;


  /*
   * TIM4 Clock = 48 MHz
   *
   * 48 MHz / (47999 + 1)
   *
   * = 1000 Hz
   */
  htim4.Init.Prescaler = 47999;


  htim4.Init.CounterMode =
      TIM_COUNTERMODE_UP;


  /*
   * 1000 Hz / (999 + 1)
   *
   * = 1 Hz
   *
   * Interrupcion cada 1 segundo
   */
  htim4.Init.Period = 999;


  htim4.Init.ClockDivision =
      TIM_CLOCKDIVISION_DIV1;


  htim4.Init.AutoReloadPreload =
      TIM_AUTORELOAD_PRELOAD_DISABLE;


  if (HAL_TIM_Base_Init(&htim4) != HAL_OK)
  {
    Error_Handler();
  }


  sClockSourceConfig.ClockSource =
      TIM_CLOCKSOURCE_INTERNAL;


  if (HAL_TIM_ConfigClockSource(
          &htim4,
          &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }


  sMasterConfig.MasterOutputTrigger =
      TIM_TRGO_RESET;


  sMasterConfig.MasterSlaveMode =
      TIM_MASTERSLAVEMODE_DISABLE;


  if (HAL_TIMEx_MasterConfigSynchronization(
          &htim4,
          &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }


  /* USER CODE BEGIN TIM4_Init 2 */

  /* USER CODE END TIM4_Init 2 */

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


  huart2.Instance =
      USART2;


  huart2.Init.BaudRate =
      115200;


  huart2.Init.WordLength =
      UART_WORDLENGTH_8B;


  huart2.Init.StopBits =
      UART_STOPBITS_1;


  huart2.Init.Parity =
      UART_PARITY_NONE;


  huart2.Init.Mode =
      UART_MODE_TX_RX;


  huart2.Init.HwFlowCtl =
      UART_HWCONTROL_NONE;


  huart2.Init.OverSampling =
      UART_OVERSAMPLING_16;


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

  __HAL_RCC_GPIOC_CLK_ENABLE();

  __HAL_RCC_GPIOH_CLK_ENABLE();

  __HAL_RCC_GPIOA_CLK_ENABLE();

  __HAL_RCC_GPIOB_CLK_ENABLE();


  /* Output levels */

  HAL_GPIO_WritePin(
      LD2_GPIO_Port,
      LD2_Pin,
      GPIO_PIN_RESET
  );


  HAL_GPIO_WritePin(
      Led1_GPIO_Port,
      Led1_Pin,
      GPIO_PIN_RESET
  );


  HAL_GPIO_WritePin(
      Led2_GPIO_Port,
      Led2_Pin,
      GPIO_PIN_RESET
  );


  /* B1 */

  GPIO_InitStruct.Pin =
      B1_Pin;


  GPIO_InitStruct.Mode =
      GPIO_MODE_IT_FALLING;


  GPIO_InitStruct.Pull =
      GPIO_NOPULL;


  HAL_GPIO_Init(
      B1_GPIO_Port,
      &GPIO_InitStruct
  );


  /* LD2 */

  GPIO_InitStruct.Pin =
      LD2_Pin;


  GPIO_InitStruct.Mode =
      GPIO_MODE_OUTPUT_PP;


  GPIO_InitStruct.Pull =
      GPIO_NOPULL;


  GPIO_InitStruct.Speed =
      GPIO_SPEED_FREQ_LOW;


  HAL_GPIO_Init(
      LD2_GPIO_Port,
      &GPIO_InitStruct
  );


  /* LED1 */

  GPIO_InitStruct.Pin =
      Led1_Pin;


  GPIO_InitStruct.Mode =
      GPIO_MODE_OUTPUT_PP;


  GPIO_InitStruct.Pull =
      GPIO_NOPULL;


  GPIO_InitStruct.Speed =
      GPIO_SPEED_FREQ_LOW;


  HAL_GPIO_Init(
      Led1_GPIO_Port,
      &GPIO_InitStruct
  );


  /* LED2 */

  GPIO_InitStruct.Pin =
      Led2_Pin;


  GPIO_InitStruct.Mode =
      GPIO_MODE_OUTPUT_PP;


  GPIO_InitStruct.Pull =
      GPIO_NOPULL;


  GPIO_InitStruct.Speed =
      GPIO_SPEED_FREQ_LOW;


  HAL_GPIO_Init(
      Led2_GPIO_Port,
      &GPIO_InitStruct
  );


  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */

}


/* USER CODE BEGIN 4 */


/* ============================================================
 * TIM2 INPUT CAPTURE CALLBACK
 * ============================================================ */

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{

  /*
   * Verificar TIM2
   */
  if (htim->Instance == TIM2)
  {

    /*
     * Verificar Channel 1
     */
    if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1)
    {

      /*
       * Contador para debug.
       *
       * Debe aumentar con cada flanco recibido
       * en PA0.
       */
      contadorCapturas++;


      /* ======================================================
       * PRIMERA CAPTURA
       * ====================================================== */

      if (primeraCaptura == 0)
      {

        captura1 =
            HAL_TIM_ReadCapturedValue(
                htim,
                TIM_CHANNEL_1
            );


        primeraCaptura = 1;

      }


      /* ======================================================
       * SEGUNDA CAPTURA Y SIGUIENTES
       * ====================================================== */

      else
      {

        captura2 =
            HAL_TIM_ReadCapturedValue(
                htim,
                TIM_CHANNEL_1
            );


        /*
         * Sin overflow
         */
        if (captura2 >= captura1)
        {

          diferencia =
              captura2 - captura1;

        }


        /*
         * Con overflow del contador de 32 bits
         */
        else
        {

          diferencia =
              (0xFFFFFFFFU - captura1)
              + captura2
              + 1U;

        }


        /*
         * Calcular frecuencia
         *
         */

        if (diferencia != 0U)
        {

          frecuencia =
              (float)TIM2_COUNTER_HZ /
              (float)diferencia;


          nuevaMedicion = 1;

        }


        /*
         * Preparar siguiente medicion
         */
        captura1 = captura2;

      }

    }

  }

}


/* ============================================================
 * TIM3 / TIM4 BASE TIMER CALLBACK
 * ============================================================ */

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{

  /* ==========================================================
   * TIM3
   *
   * Interrupcion cada 250 ms
   *
   * LED1:
   *
   * HIGH 250 ms
   * LOW  250 ms
   *
   * Periodo = 500 ms
   * Frecuencia = 2 Hz
   * ========================================================== */

  if (htim->Instance == TIM3)
  {

    HAL_GPIO_TogglePin(
        Led1_GPIO_Port,
        Led1_Pin
    );

  }


  /* ==========================================================
   * TIM4
   *
   * Interrupcion cada 1 segundo
   *
   * LED2:
   *
   * HIGH 1 s
   * LOW  1 s
   *
   * Periodo = 2 segundos
   * Frecuencia = 0.5 Hz
   * ========================================================== */

  if (htim->Instance == TIM4)
  {

    HAL_GPIO_TogglePin(
        Led2_GPIO_Port,
        Led2_Pin
    );

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

  __disable_irq();


  while (1)
  {

  }


  /* USER CODE END Error_Handler_Debug */

}


#ifdef USE_FULL_ASSERT

/**
  * @brief Reports the name of the source file and line number.
  */
void assert_failed(uint8_t *file, uint32_t line)
{

  /* USER CODE BEGIN 6 */

  /* USER CODE END 6 */

}

#endif
