/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Lectura de joystick con ADC1, DMA y USART2
  ******************************************************************************
  */
/* USER CODE END Header */

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
#define ADC1_LIMITE_INFERIOR 1975
#define ADC1_LIMITE_SUPERIOR 2030

#define ADC2_LIMITE_INFERIOR 2050
#define ADC2_LIMITE_SUPERIOR 2100
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
volatile uint16_t adcValores[2] = {0, 0};

char mensaje[100];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);

/* USER CODE BEGIN PFP */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* USER CODE END 0 */

int main(void)
{
  /* USER CODE BEGIN 1 */
  uint16_t adcVertical;
  uint16_t adcHorizontal;

  const char *vertical;
  const char *horizontal;

  int longitud;
  HAL_StatusTypeDef estadoADC;
  /* USER CODE END 1 */

  HAL_Init();

  SystemClock_Config();

  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();

  /* USER CODE BEGIN 2 */

  /* Comprueba primero que USART2 funciona */
  const char inicio[] =
      "\r\nSistema iniciado\r\n"
      "USART2 funcionando a 115200 baudios\r\n";

  HAL_UART_Transmit(&huart2,
                    (uint8_t *)inicio,
                    sizeof(inicio) - 1,
                    1000);

  HAL_Delay(200);

  /* Inicia la lectura de los dos ADC mediante DMA */
  estadoADC = HAL_ADC_Start_DMA(&hadc1,
                               (uint32_t *)adcValores,
                               2);

  if (estadoADC != HAL_OK)
  {
    const char error[] = "ERROR: no se pudo iniciar ADC DMA\r\n";

    HAL_UART_Transmit(&huart2,
                      (uint8_t *)error,
                      sizeof(error) - 1,
                      1000);

    Error_Handler();
  }

  /*
   * El DMA continúa actualizando el arreglo sin necesidad de generar
   * miles de interrupciones por segundo.
   */
  if (hadc1.DMA_Handle != NULL)
  {
    __HAL_DMA_DISABLE_IT(hadc1.DMA_Handle,
                         DMA_IT_HT | DMA_IT_TC);
  }

  const char adcOK[] = "ADC DMA iniciado correctamente\r\n";

  HAL_UART_Transmit(&huart2,
                    (uint8_t *)adcOK,
                    sizeof(adcOK) - 1,
                    1000);

  /* USER CODE END 2 */

  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    adcVertical = adcValores[0];
    adcHorizontal = adcValores[1];

    uint8_t adc1Activo = 0;
    uint8_t adc2Activo = 0;

    /* ADC1: zona muerta 1975-2030 */
    if (adcVertical > ADC1_LIMITE_SUPERIOR)
    {
      vertical = "Arriba";
      adc1Activo = 1;
    }
    else if (adcVertical < ADC1_LIMITE_INFERIOR)
    {
      vertical = "Abajo";
      adc1Activo = 1;
    }

    /* ADC2: zona muerta 2050-2100 */
    if (adcHorizontal > ADC2_LIMITE_SUPERIOR)
    {
      horizontal = "Izquierda";
      adc2Activo = 1;
    }
    else if (adcHorizontal < ADC2_LIMITE_INFERIOR)
    {
      horizontal = "Derecha";
      adc2Activo = 1;
    }

    /* Ambos ADC fuera de la zona muerta */
    if (adc1Activo && adc2Activo)
    {
      longitud = snprintf(mensaje,
                          sizeof(mensaje),
                          "ADC1: %u - %s | ADC2: %u - %s\r\n",
                          (unsigned int)adcVertical,
                          vertical,
                          (unsigned int)adcHorizontal,
                          horizontal);

      HAL_UART_Transmit(&huart2,
                        (uint8_t *)mensaje,
                        longitud,
                        1000);
    }
    /* Solo ADC1 fuera de la zona muerta */
    else if (adc1Activo)
    {
      longitud = snprintf(mensaje,
                          sizeof(mensaje),
                          "ADC1: %u - %s\r\n",
                          (unsigned int)adcVertical,
                          vertical);

      HAL_UART_Transmit(&huart2,
                        (uint8_t *)mensaje,
                        longitud,
                        1000);
    }
    /* Solo ADC2 fuera de la zona muerta */
    else if (adc2Activo)
    {
      longitud = snprintf(mensaje,
                          sizeof(mensaje),
                          "ADC2: %u - %s\r\n",
                          (unsigned int)adcHorizontal,
                          horizontal);

      HAL_UART_Transmit(&huart2,
                        (uint8_t *)mensaje,
                        longitud,
                        1000);
    }

    /* Si ambos están en zona muerta, no transmite */
    HAL_Delay(200);

    /* USER CODE END 3 */
 }
}
/**
  * @brief Configuración del reloj.
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();

  __HAL_PWR_VOLTAGESCALING_CONFIG(
      PWR_REGULATOR_VOLTAGE_SCALE3);

  RCC_OscInitStruct.OscillatorType =
      RCC_OSCILLATORTYPE_HSI;

  RCC_OscInitStruct.HSIState = RCC_HSI_ON;

  RCC_OscInitStruct.HSICalibrationValue =
      RCC_HSICALIBRATION_DEFAULT;

  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;

  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType =
      RCC_CLOCKTYPE_HCLK |
      RCC_CLOCKTYPE_SYSCLK |
      RCC_CLOCKTYPE_PCLK1 |
      RCC_CLOCKTYPE_PCLK2;

  RCC_ClkInitStruct.SYSCLKSource =
      RCC_SYSCLKSOURCE_HSI;

  RCC_ClkInitStruct.AHBCLKDivider =
      RCC_SYSCLK_DIV1;

  RCC_ClkInitStruct.APB1CLKDivider =
      RCC_HCLK_DIV1;

  RCC_ClkInitStruct.APB2CLKDivider =
      RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct,
                          FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief Configuración ADC1.
  */
static void MX_ADC1_Init(void)
{
  ADC_ChannelConfTypeDef sConfig = {0};

  hadc1.Instance = ADC1;

  hadc1.Init.ClockPrescaler =
      ADC_CLOCK_SYNC_PCLK_DIV8;

  hadc1.Init.Resolution =
      ADC_RESOLUTION_12B;

  hadc1.Init.ScanConvMode =
      ENABLE;

  hadc1.Init.ContinuousConvMode =
      ENABLE;

  hadc1.Init.DiscontinuousConvMode =
      DISABLE;

  hadc1.Init.ExternalTrigConvEdge =
      ADC_EXTERNALTRIGCONVEDGE_NONE;

  hadc1.Init.ExternalTrigConv =
      ADC_SOFTWARE_START;

  hadc1.Init.DataAlign =
      ADC_DATAALIGN_RIGHT;

  hadc1.Init.NbrOfConversion =
      2;

  hadc1.Init.DMAContinuousRequests =
      ENABLE;

  hadc1.Init.EOCSelection =
      ADC_EOC_SEQ_CONV;

  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /* PA0: ADC1 canal 0, Rank 1 */
  sConfig.Channel =
      ADC_CHANNEL_0;

  sConfig.Rank =
      1;

  sConfig.SamplingTime =
      ADC_SAMPLETIME_84CYCLES;

  if (HAL_ADC_ConfigChannel(&hadc1,
                            &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /* PA1: ADC1 canal 1, Rank 2 */
  sConfig.Channel =
      ADC_CHANNEL_1;

  sConfig.Rank =
      2;

  sConfig.SamplingTime =
      ADC_SAMPLETIME_84CYCLES;

  if (HAL_ADC_ConfigChannel(&hadc1,
                            &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART1 hacia el ATmega328P.
  */
static void MX_USART1_UART_Init(void)
{
  huart1.Instance = USART1;

  huart1.Init.BaudRate =
      9600;

  huart1.Init.WordLength =
      UART_WORDLENGTH_8B;

  huart1.Init.StopBits =
      UART_STOPBITS_1;

  huart1.Init.Parity =
      UART_PARITY_NONE;

  huart1.Init.Mode =
      UART_MODE_TX_RX;

  huart1.Init.HwFlowCtl =
      UART_HWCONTROL_NONE;

  huart1.Init.OverSampling =
      UART_OVERSAMPLING_16;

  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART2 hacia la computadora.
  */
static void MX_USART2_UART_Init(void)
{
  huart2.Instance = USART2;

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
}

/**
  * @brief Configuración DMA.
  */
static void MX_DMA_Init(void)
{
  __HAL_RCC_DMA2_CLK_ENABLE();

  HAL_NVIC_SetPriority(DMA2_Stream0_IRQn,
                       0,
                       0);

  HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
}

/**
  * @brief Configuración GPIO.
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  HAL_GPIO_WritePin(LD2_GPIO_Port,
                    LD2_Pin,
                    GPIO_PIN_RESET);

  /* Botón azul de la Nucleo */
  GPIO_InitStruct.Pin =
      B1_Pin;

  GPIO_InitStruct.Mode =
      GPIO_MODE_IT_FALLING;

  GPIO_InitStruct.Pull =
      GPIO_NOPULL;

  HAL_GPIO_Init(B1_GPIO_Port,
                &GPIO_InitStruct);

  /* Botón del joystick PC0 */
  GPIO_InitStruct.Pin =
      GPIO_PIN_0;

  GPIO_InitStruct.Mode =
      GPIO_MODE_INPUT;

  GPIO_InitStruct.Pull =
      GPIO_PULLUP;

  HAL_GPIO_Init(GPIOC,
                &GPIO_InitStruct);

  /* LED integrado */
  GPIO_InitStruct.Pin =
      LD2_Pin;

  GPIO_InitStruct.Mode =
      GPIO_MODE_OUTPUT_PP;

  GPIO_InitStruct.Pull =
      GPIO_NOPULL;

  GPIO_InitStruct.Speed =
      GPIO_SPEED_FREQ_LOW;

  HAL_GPIO_Init(LD2_GPIO_Port,
                &GPIO_InitStruct);
}

/**
  * @brief Manejo de errores.
  */
void Error_Handler(void)
{
  __disable_irq();

  while (1)
  {
  }
}

#ifdef USE_FULL_ASSERT

/**
  * @brief Reporte de error de parámetros.
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* Evita advertencias */
  (void)file;
  (void)line;
}

#endif
