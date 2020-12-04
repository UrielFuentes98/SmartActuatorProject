/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
# define POINTS_NUM 7
# define DIST_POINTS 7
# define HALF_OFFSET 3

# define COUNTS_PER_CONTROL 200
# define PULSE_MAX 1000
# define PERIOD_COUNT 1000

# define MAX_SP 55
# define MIN_SP 2

#define ADC_VOL_NORMAL 2420
#define ADC_DIF_VOL_LIMIT 800

#define ADC_CURR_NORMAL 2420
#define ADC_DIF_CURR_LIMIT 360

#define HOME_POS 30
#define COUNTS_HOME 25
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
ADC_HandleTypeDef hadc2;

CAN_HandleTypeDef hcan;

TIM_HandleTypeDef htim3;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart3;

/* Definitions for CANReadPos */
osThreadId_t CANReadPosHandle;
const osThreadAttr_t CANReadPos_attributes = {
  .name = "CANReadPos",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 128 * 4
};
/* Definitions for CANReadSP */
osThreadId_t CANReadSPHandle;
const osThreadAttr_t CANReadSP_attributes = {
  .name = "CANReadSP",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 128 * 4
};
/* Definitions for FailureAnalyzer */
osThreadId_t FailureAnalyzerHandle;
const osThreadAttr_t FailureAnalyzer_attributes = {
  .name = "FailureAnalyzer",
  .priority = (osPriority_t) osPriorityAboveNormal,
  .stack_size = 128 * 4
};
/* Definitions for PositionControl */
osThreadId_t PositionControlHandle;
const osThreadAttr_t PositionControl_attributes = {
  .name = "PositionControl",
  .priority = (osPriority_t) osPriorityAboveNormal,
  .stack_size = 128 * 4
};
/* USER CODE BEGIN PV */

enum SysStates{VOL_ABOVE, VOL_BELOW, CURR_BELOW, CURR_ABOVE, NORMAL}SysState;

uint32_t adcRaw = 0;
uint32_t adcRawVol = 0;
uint32_t adcRawCurr = 0;
uint16_t actualPos_mm = 0;
uint32_t setPoint = HOME_POS;
int32_t error = 0;
uint32_t pulseCount = 0;
const uint32_t adcPoints[7] = {5, 130, 969, 2070, 3190, 3845, 3965};
const uint32_t linearFactors[7] = {15, 120, 137, 140, 93, 15, 10};
CAN_TxHeaderTypeDef txHeader;
uint8_t txMessage[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint32_t txMailbox;
CAN_RxHeaderTypeDef rxHeader;
uint8_t rxMessage[8];
CAN_FilterTypeDef filterConfig;
uint8_t InBuff[1];
uint8_t sendBuff[2];
uint8_t readSP = 0;
uint32_t countsHome = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_CAN_Init(void);
static void MX_TIM3_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_ADC2_Init(void);
static void MX_USART3_UART_Init(void);
void CAN_Read_Pos(void *argument);
void CAN_Read_SP(void *argument);
void Check_Failures(void *argument);
void Pos_Control(void *argument);

/* USER CODE BEGIN PFP */
uint32_t readRawADC (ADC_HandleTypeDef hadc);
uint32_t getPos_mm (uint32_t adcVal);
static void setPWM(TIM_HandleTypeDef, uint32_t, uint16_t, uint16_t);
void sendSerialData(void);
void MotForward (void);
void MotBackward (void);
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
  MX_ADC1_Init();
  MX_CAN_Init();
  MX_TIM3_Init();
  MX_USART1_UART_Init();
  MX_ADC2_Init();
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */

  HAL_GPIO_WritePin(LEDInt_GPIO_Port, LEDInt_Pin, GPIO_PIN_SET);


  txHeader.DLC = 8;
  txHeader.StdId = 0x18FFF848;
  txHeader.IDE = CAN_ID_STD;
  txHeader.RTR = CAN_RTR_DATA;
  filterConfig.FilterIdHigh = 0;
  filterConfig.FilterIdLow = 0;
  filterConfig.FilterMaskIdHigh = 0;
  filterConfig.FilterMaskIdLow = 0;
  filterConfig.FilterBank = 15;
  filterConfig.FilterFIFOAssignment = CAN_FILTER_FIFO0;
  filterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
  filterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
  filterConfig.FilterActivation = ENABLE;
  HAL_CAN_Start(&hcan);
  HAL_CAN_ConfigFilter(&hcan, &filterConfig);

  SysState = NORMAL;
  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of CANReadPos */
  CANReadPosHandle = osThreadNew(CAN_Read_Pos, NULL, &CANReadPos_attributes);

  /* creation of CANReadSP */
  CANReadSPHandle = osThreadNew(CAN_Read_SP, NULL, &CANReadSP_attributes);

  /* creation of FailureAnalyzer */
  FailureAnalyzerHandle = osThreadNew(Check_Failures, NULL, &FailureAnalyzer_attributes);

  /* creation of PositionControl */
  PositionControlHandle = osThreadNew(Pos_Control, NULL, &PositionControl_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */
  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {



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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
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
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
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
  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 2;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_2;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_5;
  sConfig.Rank = ADC_REGULAR_RANK_2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief ADC2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC2_Init(void)
{

  /* USER CODE BEGIN ADC2_Init 0 */

  /* USER CODE END ADC2_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC2_Init 1 */

  /* USER CODE END ADC2_Init 1 */
  /** Common config
  */
  hadc2.Instance = ADC2;
  hadc2.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc2.Init.ContinuousConvMode = DISABLE;
  hadc2.Init.DiscontinuousConvMode = DISABLE;
  hadc2.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc2.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc2.Init.NbrOfConversion = 1;
  if (HAL_ADC_Init(&hadc2) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_3;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC2_Init 2 */

  /* USER CODE END ADC2_Init 2 */

}

/**
  * @brief CAN Initialization Function
  * @param None
  * @retval None
  */
static void MX_CAN_Init(void)
{

  /* USER CODE BEGIN CAN_Init 0 */

  /* USER CODE END CAN_Init 0 */

  /* USER CODE BEGIN CAN_Init 1 */

  /* USER CODE END CAN_Init 1 */
  hcan.Instance = CAN1;
  hcan.Init.Prescaler = 24;
  hcan.Init.Mode = CAN_MODE_NORMAL;
  hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan.Init.TimeSeg1 = CAN_BS1_1TQ;
  hcan.Init.TimeSeg2 = CAN_BS2_1TQ;
  hcan.Init.TimeTriggeredMode = DISABLE;
  hcan.Init.AutoBusOff = DISABLE;
  hcan.Init.AutoWakeUp = DISABLE;
  hcan.Init.AutoRetransmission = DISABLE;
  hcan.Init.ReceiveFifoLocked = DISABLE;
  hcan.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN_Init 2 */

  /* USER CODE END CAN_Init 2 */

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
  htim3.Init.Prescaler = 100;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 1600;
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
  sConfigOC.Pulse = 0;
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
  huart3.Init.BaudRate = 9600;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
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

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LEDInt_GPIO_Port, LEDInt_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, In1_Pin|In2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : LEDInt_Pin */
  GPIO_InitStruct.Pin = LEDInt_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LEDInt_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : In1_Pin In2_Pin */
  GPIO_InitStruct.Pin = In1_Pin|In2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : LED2_Pin */
  GPIO_InitStruct.Pin = LED2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED2_GPIO_Port, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */


void setPWM(TIM_HandleTypeDef timer, uint32_t channel, uint16_t period,
uint16_t pulse)
{
 HAL_TIM_PWM_Stop(&timer, channel); // stop generation of pwm
 TIM_OC_InitTypeDef sConfigOC;
 timer.Init.Period = period; // set the period duration
 HAL_TIM_PWM_Init(&timer); // reinititialise with new period value
 sConfigOC.OCMode = TIM_OCMODE_PWM1;
 sConfigOC.Pulse = pulse; // set the pulse duration
 sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
 sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
 HAL_TIM_PWM_ConfigChannel(&timer, &sConfigOC, channel);
 HAL_TIM_PWM_Start(&timer, channel); // start pwm generation
}

uint32_t readRawADC (ADC_HandleTypeDef hadc) {

	uint32_t reading;

	HAL_ADC_Start(&hadc);
	HAL_ADC_PollForConversion(&hadc, HAL_MAX_DELAY);
	reading = HAL_ADC_GetValue(&hadc);
	HAL_ADC_Stop(&hadc);

	return reading;

}

void sendSerialData(void) {

	sendBuff[0] = actualPos_mm;
	sendBuff[1] = SysState;

	HAL_UART_Transmit (&huart3, sendBuff, 2, 200);

}

uint32_t getPos_mm (uint32_t adcVal) {

	uint8_t sector = 0;
	uint16_t pos_mm = 0;

	while (adcVal >= adcPoints[sector] && sector < POINTS_NUM){
		sector ++;
	}

	pos_mm += DIST_POINTS * sector;
	if (pos_mm >= 3){
		pos_mm += HALF_OFFSET;
	}

	if (sector > 0){
		pos_mm += (adcVal - adcPoints[sector - 1]) / linearFactors[sector - 1];
	} else{
		pos_mm += adcVal * 1.4;
	}

	return pos_mm;

}


void MotForward (void){
	HAL_GPIO_WritePin(GPIOA, In1_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, In2_Pin, GPIO_PIN_SET);
}

void MotBackward (void){
	HAL_GPIO_WritePin(GPIOA, In1_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOA, In2_Pin, GPIO_PIN_RESET);
}

/* USER CODE END 4 */

/* USER CODE BEGIN Header_CAN_Read_Pos */
/**
  * @brief  Function implementing the CANReadPos thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_CAN_Read_Pos */
void CAN_Read_Pos(void *argument)
{
  /* USER CODE BEGIN 5 */
  /* Infinite loop */
	//uint8_t MSG[25] = {'\0'};
	//uint8_t X = 0;
  for(;;)
  {

	/*adcRaw = readRawADC(hadc2);
	actualPos_mm = getPos_mm(adcRaw);
	txHeader.StdId = 0x18F03248;
	txMessage[0]= actualPos_mm & 0xff;
	txMessage[1]=(actualPos_mm >> 8);
	HAL_CAN_AddTxMessage(&hcan, &txHeader, txMessage, &txMailbox);*/

    osDelay(1000);
  }
  /* USER CODE END 5 */
}

/* USER CODE BEGIN Header_CAN_Read_SP */
/**
* @brief Function implementing the CANReadSP thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_CAN_Read_SP */
void CAN_Read_SP(void *argument)
{
  /* USER CODE BEGIN CAN_Read_SP */

  /* Infinite loop */
	for(;;)
	{
		//HAL_UART_Receive( &huart3, buff, 1, 200);
		//txHeader.StdId = 0x18FFF848;
		//HAL_CAN_GetRxMessage(&hcan, CAN_RX_FIFO0, &rxHeader, rxMessage);
		//setPoint = buff[0];

		if(HAL_UART_Receive (&huart3, InBuff, 1, 200) == HAL_OK) {

			//Limit SetPoint range.
			if (InBuff[0] > MAX_SP) {
				setPoint = MAX_SP;
			} else if (InBuff[0] < MIN_SP) {
				setPoint = MIN_SP;
			} else {
				setPoint = InBuff[0];
			}

			osDelay(1000);

		} else {
			osDelay(200);
		}
	}
  /* USER CODE END CAN_Read_SP */
}

/* USER CODE BEGIN Header_Check_Failures */
/**
* @brief Function implementing the FailureAnalyzer thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_Check_Failures */
void Check_Failures(void *argument)
{
  /* USER CODE BEGIN Check_Failures */
  /* Infinite loop */
  for(;;)
  {

	  HAL_ADC_Start(&hadc1);
	  HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
	  adcRawVol = HAL_ADC_GetValue(&hadc1);
	  osDelay(5);
	  HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
	  adcRawCurr = HAL_ADC_GetValue(&hadc1);
	  HAL_ADC_Stop(&hadc1);

	  //Check for voltage read outside of valid limits.
	  if (adcRawVol < ADC_VOL_NORMAL - ADC_DIF_VOL_LIMIT) {
		  SysState = VOL_BELOW;
	  } else if (adcRawVol > ADC_VOL_NORMAL + ADC_DIF_VOL_LIMIT) {
		  SysState = VOL_ABOVE;
	  } else if (adcRawCurr < ADC_CURR_NORMAL - ADC_DIF_CURR_LIMIT){
		  SysState = CURR_BELOW;
	  } else if(adcRawCurr > ADC_CURR_NORMAL + ADC_DIF_CURR_LIMIT) {
		  SysState = CURR_ABOVE;
	  } else {
		  SysState = NORMAL;
	  }

	  osDelay(1000);
  }
  /* USER CODE END Check_Failures */
}

/* USER CODE BEGIN Header_Pos_Control */
/**
* @brief Function implementing the PositionControl thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_Pos_Control */
void Pos_Control(void *argument)
{
  /* USER CODE BEGIN Pos_Control */
  /* Infinite loop */
  for(;;)
  {
	adcRaw = readRawADC(hadc2);
	actualPos_mm = getPos_mm(adcRaw);

	error = setPoint - actualPos_mm;

	if (error >= 0) {
		MotForward();
	}else {
		MotBackward();
	}

	if (error < 0){
		error = error * -1;
	}

	if (error >= 5){
		pulseCount = PULSE_MAX;
	}else{
		pulseCount = COUNTS_PER_CONTROL * error;
	}

	if (SysState == NORMAL) {
		setPWM(htim3, TIM_CHANNEL_1, PERIOD_COUNT, pulseCount);
	} else {
		setPWM(htim3, TIM_CHANNEL_1, PERIOD_COUNT, 0);
	}

	if (error <= 1) {
		if (setPoint != HOME_POS) {
			countsHome ++;
			if (countsHome >= COUNTS_HOME) {
				setPoint = HOME_POS;
				countsHome = 0;
			}
		}
	}

	sendSerialData();

    osDelay(200);
  }
  /* USER CODE END Pos_Control */
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM1 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM1) {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */

  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
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
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
