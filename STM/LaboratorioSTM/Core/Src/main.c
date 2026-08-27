/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Laboratorio 4 - Carrera 2.0
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define PULSOS_POR_DECENA 10
#define META_DECENAS 4
#define META_PULSOS (PULSOS_POR_DECENA * META_DECENAS)

#define CUENTA_INICIO 5
#define DEBOUNCE_MS 50

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

volatile uint8_t contadorJ1 = 0;
volatile uint8_t contadorJ2 = 0;

volatile uint8_t carreraHabilitada = 0;
volatile uint8_t carreraFinalizada = 0;
volatile uint8_t solicitudInicio = 0;
volatile uint8_t inicioEnProceso = 0;

volatile uint8_t botonListoJ1 = 1;
volatile uint8_t botonListoJ2 = 1;
volatile uint8_t botonListoStart = 1;

/* Debounce independiente */
volatile uint32_t ultimoTiempoJ1 = 0;
volatile uint32_t ultimoTiempoJ2 = 0;
volatile uint32_t ultimoTiempoStart = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
/* USER CODE BEGIN PFP */

void ApagarLedsJ1(void);
void ApagarLedsJ2(void);
void MostrarDecenasJ1(uint8_t decenas);
void MostrarDecenasJ2(uint8_t decenas);

void MostrarDisplay(uint8_t numero);
void ApagarDisplay(void);

void ReiniciarCarrera(void);
void EjecutarInicioCarrera(void);
void ActualizarLiberacionBotones(void);

void ProcesarPulsacionJ1(void);
void ProcesarPulsacionJ2(void);

void GanadorJ1(void);
void GanadorJ2(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */


/*
 * LEDS JUGADOR 1
 */

void ApagarLedsJ1(void)
{
    HAL_GPIO_WritePin(LED1J1_GPIO_Port, LED1J1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED2J1_GPIO_Port, LED2J1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED3J1_GPIO_Port, LED3J1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED4J1_GPIO_Port, LED4J1_Pin, GPIO_PIN_RESET);
}


void MostrarDecenasJ1(uint8_t decenas)
{
    if (decenas > META_DECENAS)
    {
        decenas = META_DECENAS;
    }

    HAL_GPIO_WritePin(
        LED1J1_GPIO_Port,
        LED1J1_Pin,
        (decenas >= 1) ? GPIO_PIN_SET : GPIO_PIN_RESET
    );

    HAL_GPIO_WritePin(
        LED2J1_GPIO_Port,
        LED2J1_Pin,
        (decenas >= 2) ? GPIO_PIN_SET : GPIO_PIN_RESET
    );

    HAL_GPIO_WritePin(
        LED3J1_GPIO_Port,
        LED3J1_Pin,
        (decenas >= 3) ? GPIO_PIN_SET : GPIO_PIN_RESET
    );

    HAL_GPIO_WritePin(
        LED4J1_GPIO_Port,
        LED4J1_Pin,
        (decenas >= 4) ? GPIO_PIN_SET : GPIO_PIN_RESET
    );
}


/*
 * LEDS JUGADOR 2
 */

void ApagarLedsJ2(void)
{
    HAL_GPIO_WritePin(LED1J2_GPIO_Port, LED1J2_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED2J2_GPIO_Port, LED2J2_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED3J2_GPIO_Port, LED3J2_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED4J2_GPIO_Port, LED4J2_Pin, GPIO_PIN_RESET);
}


void MostrarDecenasJ2(uint8_t decenas)
{
    if (decenas > META_DECENAS)
    {
        decenas = META_DECENAS;
    }

    HAL_GPIO_WritePin(
        LED1J2_GPIO_Port,
        LED1J2_Pin,
        (decenas >= 1) ? GPIO_PIN_SET : GPIO_PIN_RESET
    );

    HAL_GPIO_WritePin(
        LED2J2_GPIO_Port,
        LED2J2_Pin,
        (decenas >= 2) ? GPIO_PIN_SET : GPIO_PIN_RESET
    );

    HAL_GPIO_WritePin(
        LED3J2_GPIO_Port,
        LED3J2_Pin,
        (decenas >= 3) ? GPIO_PIN_SET : GPIO_PIN_RESET
    );

    HAL_GPIO_WritePin(
        LED4J2_GPIO_Port,
        LED4J2_Pin,
        (decenas >= 4) ? GPIO_PIN_SET : GPIO_PIN_RESET
    );
}


/*
 * DISPLAY 7 SEGMENTOS - CATODO COMUN
 */

void MostrarDisplay(uint8_t numero)
{
    uint8_t patron = 0;

    /*
     * bit 0 = A
     * bit 1 = B
     * bit 2 = C
     * bit 3 = D
     * bit 4 = E
     * bit 5 = F
     * bit 6 = G
     */

    switch(numero)
    {
        case 0:
            patron = 0x3F;
            break;

        case 1:
            patron = 0x06;
            break;

        case 2:
            patron = 0x5B;
            break;

        case 3:
            patron = 0x4F;
            break;

        case 4:
            patron = 0x66;
            break;

        case 5:
            patron = 0x6D;
            break;

        case 6:
            patron = 0x7D;
            break;

        case 7:
            patron = 0x07;
            break;

        case 8:
            patron = 0x7F;
            break;

        case 9:
            patron = 0x6F;
            break;

        default:
            patron = 0x00;
            break;
    }


    /* Segmento A - PB9 */
    HAL_GPIO_WritePin(
        GPIOB,
        GPIO_PIN_9,
        (patron & 0x01) ? GPIO_PIN_SET : GPIO_PIN_RESET
    );

    /* Segmento B - PB8 */
    HAL_GPIO_WritePin(
        GPIOB,
        GPIO_PIN_8,
        (patron & 0x02) ? GPIO_PIN_SET : GPIO_PIN_RESET
    );

    /* Segmento C - PA0 */
    HAL_GPIO_WritePin(
        GPIOA,
        GPIO_PIN_0,
        (patron & 0x04) ? GPIO_PIN_SET : GPIO_PIN_RESET
    );

    /* Segmento D - PA1 */
    HAL_GPIO_WritePin(
        GPIOA,
        GPIO_PIN_1,
        (patron & 0x08) ? GPIO_PIN_SET : GPIO_PIN_RESET
    );

    /* Segmento E - PA4 */
    HAL_GPIO_WritePin(
        GPIOA,
        GPIO_PIN_4,
        (patron & 0x10) ? GPIO_PIN_SET : GPIO_PIN_RESET
    );

    /* Segmento F - PB0 */
    HAL_GPIO_WritePin(
        GPIOB,
        GPIO_PIN_0,
        (patron & 0x20) ? GPIO_PIN_SET : GPIO_PIN_RESET
    );

    /* Segmento G - PC1 */
    HAL_GPIO_WritePin(
        GPIOC,
        GPIO_PIN_1,
        (patron & 0x40) ? GPIO_PIN_SET : GPIO_PIN_RESET
    );
}


/* ============================================================
 * APAGAR DISPLAY
 * ============================================================ */

void ApagarDisplay(void)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);

    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_RESET);
}


/* ============================================================
 * ESTADO DE LA CARRERA
 * ============================================================ */

void ReiniciarCarrera(void)
{
    contadorJ1 = 0;
    contadorJ2 = 0;

    carreraFinalizada = 0;
    carreraHabilitada = 0;

    ApagarLedsJ1();
    ApagarLedsJ2();
    ApagarDisplay();
}


void EjecutarInicioCarrera(void)
{
    uint8_t numero = 0;

    solicitudInicio = 0;
    inicioEnProceso = 1;
    ReiniciarCarrera();

    for (numero = CUENTA_INICIO; numero > 0; numero--)
    {
        MostrarDisplay(numero);
        HAL_Delay(1000);
    }

    MostrarDisplay(0);
    HAL_Delay(1000);
    ApagarDisplay();

    /*
     * Si alguien deja presionado un boton durante la cuenta regresiva,
     * no debe ganar un conteo automatico al iniciar.
     */
    botonListoJ1 =
        (HAL_GPIO_ReadPin(BTN_J1_GPIO_Port, BTN_J1_Pin) == GPIO_PIN_SET);

    botonListoJ2 =
        (HAL_GPIO_ReadPin(BTN_J2_GPIO_Port, BTN_J2_Pin) == GPIO_PIN_SET);

    botonListoStart =
        (HAL_GPIO_ReadPin(BTN_START_GPIO_Port, BTN_START_Pin) == GPIO_PIN_SET);

    solicitudInicio = 0;
    carreraHabilitada = 1;
    inicioEnProceso = 0;
}


void ActualizarLiberacionBotones(void)
{
    if (HAL_GPIO_ReadPin(BTN_J1_GPIO_Port, BTN_J1_Pin) == GPIO_PIN_SET)
    {
        botonListoJ1 = 1;
    }

    if (HAL_GPIO_ReadPin(BTN_J2_GPIO_Port, BTN_J2_Pin) == GPIO_PIN_SET)
    {
        botonListoJ2 = 1;
    }

    if (HAL_GPIO_ReadPin(BTN_START_GPIO_Port, BTN_START_Pin) == GPIO_PIN_SET)
    {
        botonListoStart = 1;
    }
}


void ProcesarPulsacionJ1(void)
{
    contadorJ1++;

    MostrarDecenasJ1(contadorJ1 / PULSOS_POR_DECENA);

    if (contadorJ1 >= META_PULSOS)
    {
        contadorJ1 = META_PULSOS;
        GanadorJ1();
    }
}


void ProcesarPulsacionJ2(void)
{
    contadorJ2++;

    MostrarDecenasJ2(contadorJ2 / PULSOS_POR_DECENA);

    if (contadorJ2 >= META_PULSOS)
    {
        contadorJ2 = META_PULSOS;
        GanadorJ2();
    }
}


/* ============================================================
 * GANADOR JUGADOR 1
 * ============================================================ */

void GanadorJ1(void)
{
    carreraFinalizada = 1;
    carreraHabilitada = 0;


    /* Encender TODOS los LEDs del Jugador 1 */

    HAL_GPIO_WritePin(
        LED1J1_GPIO_Port,
        LED1J1_Pin,
        GPIO_PIN_SET
    );

    HAL_GPIO_WritePin(
        LED2J1_GPIO_Port,
        LED2J1_Pin,
        GPIO_PIN_SET
    );

    HAL_GPIO_WritePin(
        LED3J1_GPIO_Port,
        LED3J1_Pin,
        GPIO_PIN_SET
    );

    HAL_GPIO_WritePin(
        LED4J1_GPIO_Port,
        LED4J1_Pin,
        GPIO_PIN_SET
    );


    /* Apagar TODOS los LEDs del Jugador 2 */

    HAL_GPIO_WritePin(
        LED1J2_GPIO_Port,
        LED1J2_Pin,
        GPIO_PIN_RESET
    );

    HAL_GPIO_WritePin(
        LED2J2_GPIO_Port,
        LED2J2_Pin,
        GPIO_PIN_RESET
    );

    HAL_GPIO_WritePin(
        LED3J2_GPIO_Port,
        LED3J2_Pin,
        GPIO_PIN_RESET
    );

    HAL_GPIO_WritePin(
        LED4J2_GPIO_Port,
        LED4J2_Pin,
        GPIO_PIN_RESET
    );


    /* Mostrar 1 en el display */

    MostrarDisplay(1);
}


/* ============================================================
 * GANADOR JUGADOR 2
 * ============================================================ */

void GanadorJ2(void)
{
    carreraFinalizada = 1;
    carreraHabilitada = 0;


    /* Apagar TODOS los LEDs del Jugador 1 */

    HAL_GPIO_WritePin(
        LED1J1_GPIO_Port,
        LED1J1_Pin,
        GPIO_PIN_RESET
    );

    HAL_GPIO_WritePin(
        LED2J1_GPIO_Port,
        LED2J1_Pin,
        GPIO_PIN_RESET
    );

    HAL_GPIO_WritePin(
        LED3J1_GPIO_Port,
        LED3J1_Pin,
        GPIO_PIN_RESET
    );

    HAL_GPIO_WritePin(
        LED4J1_GPIO_Port,
        LED4J1_Pin,
        GPIO_PIN_RESET
    );


    /* Encender TODOS los LEDs del Jugador 2 */

    HAL_GPIO_WritePin(
        LED1J2_GPIO_Port,
        LED1J2_Pin,
        GPIO_PIN_SET
    );

    HAL_GPIO_WritePin(
        LED2J2_GPIO_Port,
        LED2J2_Pin,
        GPIO_PIN_SET
    );

    HAL_GPIO_WritePin(
        LED3J2_GPIO_Port,
        LED3J2_Pin,
        GPIO_PIN_SET
    );

    HAL_GPIO_WritePin(
        LED4J2_GPIO_Port,
        LED4J2_Pin,
        GPIO_PIN_SET
    );


    /* Mostrar 2 en el display */

    MostrarDisplay(2);
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
  /* USER CODE BEGIN 2 */


  /* Inicializar carrera en espera del boton START */

  ReiniciarCarrera();


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    ActualizarLiberacionBotones();

    if (solicitudInicio && !carreraHabilitada)
    {
        EjecutarInicioCarrera();
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
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1|LED1J2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_4|LD2_Pin
                          |LED4J2_Pin|LED3J2_Pin|LED3J1_Pin|LED4J1_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|LED2J1_Pin|LED1J1_Pin|LED2J2_Pin
                          |GPIO_PIN_8|GPIO_PIN_9, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : PC1 LED1J2_Pin */
  GPIO_InitStruct.Pin = GPIO_PIN_1|LED1J2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PA0 PA1 PA4 LD2_Pin
                           LED4J2_Pin LED3J2_Pin LED3J1_Pin LED4J1_Pin */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_4|LD2_Pin
                          |LED4J2_Pin|LED3J2_Pin|LED3J1_Pin|LED4J1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : USART_TX_Pin USART_RX_Pin */
  GPIO_InitStruct.Pin = USART_TX_Pin|USART_RX_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB0 LED2J1_Pin LED1J1_Pin LED2J2_Pin
                           PB8 PB9 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|LED2J1_Pin|LED1J1_Pin|LED2J2_Pin
                          |GPIO_PIN_8|GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : BTN_START_Pin */
  GPIO_InitStruct.Pin = BTN_START_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(BTN_START_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : BTN_J1_Pin BTN_J2_Pin */
  GPIO_InitStruct.Pin = BTN_J1_Pin|BTN_J2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI3_IRQn);

  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* Interrupciones de botones */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    uint32_t ahora = HAL_GetTick();


    /* Botón START */
    if (GPIO_Pin == BTN_START_Pin)
    {
        if ((ahora - ultimoTiempoStart) < DEBOUNCE_MS)
        {
            return;
        }

        ultimoTiempoStart = ahora;

        if (botonListoStart &&
            HAL_GPIO_ReadPin(BTN_START_GPIO_Port, BTN_START_Pin) == GPIO_PIN_RESET)
        {
            botonListoStart = 0;

            if (!carreraHabilitada && !inicioEnProceso)
            {
                solicitudInicio = 1;
            }
        }

        return;
    }


    /* Botón J1 */
    if (GPIO_Pin == BTN_J1_Pin)
    {
        if ((ahora - ultimoTiempoJ1) < DEBOUNCE_MS)
        {
            return;
        }

        ultimoTiempoJ1 = ahora;

        if (carreraHabilitada &&
            !carreraFinalizada &&
            botonListoJ1 &&
            HAL_GPIO_ReadPin(BTN_J1_GPIO_Port, BTN_J1_Pin) == GPIO_PIN_RESET)
        {
            botonListoJ1 = 0;
            ProcesarPulsacionJ1();
        }

        return;
    }


    /* Botón J2 */
    if (GPIO_Pin == BTN_J2_Pin)
    {
        if ((ahora - ultimoTiempoJ2) < DEBOUNCE_MS)
        {
            return;
        }

        ultimoTiempoJ2 = ahora;

        if (carreraHabilitada &&
            !carreraFinalizada &&
            botonListoJ2 &&
            HAL_GPIO_ReadPin(BTN_J2_GPIO_Port, BTN_J2_Pin) == GPIO_PIN_RESET)
        {
            botonListoJ2 = 0;
            ProcesarPulsacionJ2();
        }

        return;
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
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */

  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
