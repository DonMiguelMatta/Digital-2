/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Lectura de joystick con ADC1, DMA y USART2
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define ADC1_LIMITE_INFERIOR 2048
#define ADC1_LIMITE_SUPERIOR 2099

#define ADC2_LIMITE_INFERIOR 1960
#define ADC2_LIMITE_SUPERIOR 2100

#define INTERVALO_JOYSTICK 200U
#define TIEMPO_AGRUPACION_MANDO2 10U
#define TAMANO_COLA 16U
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

/* USART1 recibe un byte a la vez desde el ATmega */
uint8_t datoUART1;
uint8_t esperandoNumero = 0;

/* Cola de comandos recibidos por interrupcion */
volatile uint8_t colaComandos[TAMANO_COLA];
volatile uint8_t posicionEscritura = 0;
volatile uint8_t posicionLectura = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */
static void guardarComando(uint8_t comando);
static uint8_t leerComando(uint8_t *comando);
static void mostrarMando1(const char *vertical,
                          const char *horizontal,
                          uint16_t adcVertical,
                          uint16_t adcHorizontal);
static void mostrarMando2(uint8_t comandos);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* Guarda un comando si todavia hay espacio */
static void guardarComando(uint8_t comando)
{
  uint8_t siguiente = posicionEscritura + 1U;

  if (siguiente >= TAMANO_COLA)
  {
    siguiente = 0;
  }

  if (siguiente != posicionLectura)
  {
    colaComandos[posicionEscritura] = comando;
    posicionEscritura = siguiente;
  }
}

/* Devuelve un comando pendiente sin consultar el periferico UART */
static uint8_t leerComando(uint8_t *comando)
{
  if (posicionLectura == posicionEscritura)
  {
    return 0;
  }

  *comando = colaComandos[posicionLectura];
  posicionLectura++;

  if (posicionLectura >= TAMANO_COLA)
  {
    posicionLectura = 0;
  }

  return 1;
}

/* Muestra una o dos mediciones del joystick en la misma linea */
static void mostrarMando1(const char *vertical,
                          const char *horizontal,
                          uint16_t adcVertical,
                          uint16_t adcHorizontal)
{
  char mensaje[100];
  int longitud;

  if (vertical == NULL && horizontal == NULL)
  {
    return;
  }

  longitud = snprintf(mensaje, sizeof(mensaje), "Mando 1: ");

  if (vertical != NULL)
  {
    longitud += snprintf(&mensaje[longitud],
                         sizeof(mensaje) - (uint32_t)longitud,
                         "%s (ADC1=%u)",
                         vertical,
                         (unsigned int)adcVertical);
  }

  if (horizontal != NULL)
  {
    if (vertical != NULL)
    {
      longitud += snprintf(&mensaje[longitud],
                           sizeof(mensaje) - (uint32_t)longitud,
                           " | ");
    }

    longitud += snprintf(&mensaje[longitud],
                         sizeof(mensaje) - (uint32_t)longitud,
                         "%s (ADC2=%u)",
                         horizontal,
                         (unsigned int)adcHorizontal);
  }

  longitud += snprintf(&mensaje[longitud],
                       sizeof(mensaje) - (uint32_t)longitud,
                       "\r\n");

  HAL_UART_Transmit(&huart2,
                    (uint8_t *)mensaje,
                    (uint16_t)longitud,
                    1000);
}

/* Muestra juntos los botones presionados al mismo tiempo */
static void mostrarMando2(uint8_t comandos)
{
  const uint8_t orden[] = {4, 3, 5, 6, 2, 1};
  const char *acciones[] = {
      "Arriba", "Abajo", "Derecha", "Izquierda", "A", "B"
  };

  char mensaje[100];
  uint8_t i;
  uint8_t primeraAccion = 1;
  int longitud;

  longitud = snprintf(mensaje, sizeof(mensaje), "Mando 2: ");

  for (i = 0; i < 6; i++)
  {
    if (comandos & (1U << (orden[i] - 1U)))
    {
      if (!primeraAccion)
      {
        longitud += snprintf(&mensaje[longitud],
                             sizeof(mensaje) - (uint32_t)longitud,
                             " | ");
      }

      longitud += snprintf(&mensaje[longitud],
                           sizeof(mensaje) - (uint32_t)longitud,
                           "%s",
                           acciones[i]);

      primeraAccion = 0;
    }
  }

  longitud += snprintf(&mensaje[longitud],
                       sizeof(mensaje) - (uint32_t)longitud,
                       "\r\n");

  HAL_UART_Transmit(&huart2,
                    (uint8_t *)mensaje,
                    (uint16_t)longitud,
                    1000);
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  uint16_t adcVertical;
  uint16_t adcHorizontal;

  const char *vertical;
  const char *horizontal;

  uint8_t comando;
  uint8_t comandosMando2 = 0;
  uint8_t mando2Pendiente = 0;
  uint32_t tiempoJoystick = 0;
  uint32_t ultimoComandoMando2 = 0;
  HAL_StatusTypeDef estadoADC;
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

  /* Activa la recepcion del control por interrupciones */
  HAL_NVIC_SetPriority(USART1_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(USART1_IRQn);

  if (HAL_UART_Receive_IT(&huart1, &datoUART1, 1) != HAL_OK)
  {
    const char error[] = "ERROR: no se pudo iniciar USART1\r\n";

    HAL_UART_Transmit(&huart2,
                      (uint8_t *)error,
                      sizeof(error) - 1,
                      1000);

    Error_Handler();
  }

  const char uartOK[] = "Control ATmega listo por USART1\r\n";

  HAL_UART_Transmit(&huart2,
                    (uint8_t *)uartOK,
                    sizeof(uartOK) - 1,
                    1000);

  tiempoJoystick = HAL_GetTick();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    /* Atiende primero todos los botones pendientes */
    while (leerComando(&comando))
    {
      comandosMando2 |= (1U << (comando - 1U));
      ultimoComandoMando2 = HAL_GetTick();
      mando2Pendiente = 1;
    }

    /* Espera el final del grupo para mostrar una sola linea */
    if (mando2Pendiente &&
        (HAL_GetTick() - ultimoComandoMando2) >= TIEMPO_AGRUPACION_MANDO2)
    {
      mostrarMando2(comandosMando2);
      comandosMando2 = 0;
      mando2Pendiente = 0;
    }

    /* Revisa el joystick cada 200 ms sin detener el programa */
    if ((HAL_GetTick() - tiempoJoystick) >= INTERVALO_JOYSTICK)
    {
      tiempoJoystick = HAL_GetTick();
      adcVertical = adcValores[0];
      adcHorizontal = adcValores[1];
      vertical = NULL;
      horizontal = NULL;

      if (adcVertical > ADC1_LIMITE_SUPERIOR)
      {
        vertical = "Arriba";
      }
      else if (adcVertical < ADC1_LIMITE_INFERIOR)
      {
        vertical = "Abajo";
      }

      /* Una diagonal muestra las dos acciones */
      if (adcHorizontal > ADC2_LIMITE_SUPERIOR)
      {
        horizontal = "Izquierda";
      }
      else if (adcHorizontal < ADC2_LIMITE_INFERIOR)
      {
        horizontal = "Derecha";
      }

      mostrarMando1(vertical,
                    horizontal,
                    adcVertical,
                    adcHorizontal);
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
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = ENABLE;
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 2;
  hadc1.Init.DMAContinuousRequests = ENABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SEQ_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = 2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 9600;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

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
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA2_Stream0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);

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

  /*Configure GPIO pin : JOY_SW_Pin */
  GPIO_InitStruct.Pin = JOY_SW_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(JOY_SW_GPIO_Port, &GPIO_InitStruct);

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
/* Vector de interrupcion de USART1 */
void USART1_IRQHandler(void)
{
  HAL_UART_IRQHandler(&huart1);
}

/* Recibe b1-b6 desde el ATmega */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    if (datoUART1 == 'b')
    {
      esperandoNumero = 1;
    }
    else if (esperandoNumero && datoUART1 >= '1' && datoUART1 <= '6')
    {
      guardarComando((uint8_t)(datoUART1 - '0'));
      esperandoNumero = 0;
    }
    else if (datoUART1 != '\r' && datoUART1 != '\n')
    {
      esperandoNumero = 0;
    }

    HAL_UART_Receive_IT(&huart1, &datoUART1, 1);
  }
}

/* Reinicia la recepcion si la linea UART presenta un error */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    esperandoNumero = 0;
    HAL_UART_Receive_IT(&huart1, &datoUART1, 1);
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
