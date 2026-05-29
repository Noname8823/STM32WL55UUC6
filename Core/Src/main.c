/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2022 STMicroelectronics.
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
#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "app_subghz_phy.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include <string.h>
#include "board_define.h"
#include "protocol.h"
#include "radio_pin_manager.h"
#include "app_logic.h"
#include "rs485.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
typedef StaticTask_t osStaticThreadDef_t;
typedef StaticTimer_t osStaticTimerDef_t;
/* USER CODE BEGIN PTD */

typedef enum {

	RADIO_TX_START = 0, RADIO_RX_START, RADIO_SLEEP

} radio_state_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define UART_BUFFER_SIZE 255
#define RADIO_TX_TIMEOUT_COUNT 1000
#define RADIO_RX_TIMEOUT_COUNT 1000

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
LPTIM_HandleTypeDef hlptim1;

RTC_HandleTypeDef hrtc;

SUBGHZ_HandleTypeDef hsubghz;

UART_HandleTypeDef huart2;

/* Definitions for radioTask */
osThreadId_t radioTaskHandle;
uint32_t radioTaskBuffer[ 128 ];
osStaticThreadDef_t radioTaskControlBlock;
const osThreadAttr_t radioTask_attributes = {
  .name = "radioTask",
  .stack_mem = &radioTaskBuffer[0],
  .stack_size = sizeof(radioTaskBuffer),
  .cb_mem = &radioTaskControlBlock,
  .cb_size = sizeof(radioTaskControlBlock),
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for mainTask */
osThreadId_t mainTaskHandle;
uint32_t mainTaskBuffer[ 128 ];
osStaticThreadDef_t mainTaskControlBlock;
const osThreadAttr_t mainTask_attributes = {
  .name = "mainTask",
  .stack_mem = &mainTaskBuffer[0],
  .stack_size = sizeof(mainTaskBuffer),
  .cb_mem = &mainTaskControlBlock,
  .cb_size = sizeof(mainTaskControlBlock),
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for mainTimer */
osTimerId_t mainTimerHandle;
osStaticTimerDef_t mainTimerControlBlock;
const osTimerAttr_t mainTimer_attributes = {
  .name = "mainTimer",
  .cb_mem = &mainTimerControlBlock,
  .cb_size = sizeof(mainTimerControlBlock),
};
/* Definitions for radioBinarySem */
osSemaphoreId_t radioBinarySemHandle;
const osSemaphoreAttr_t radioBinarySem_attributes = {
  .name = "radioBinarySem"
};
/* Definitions for mainBinarySem */
osSemaphoreId_t mainBinarySemHandle;
const osSemaphoreAttr_t mainBinarySem_attributes = {
  .name = "mainBinarySem"
};
/* USER CODE BEGIN PV */

const uint16_t COMPANY_ID = 0x0000;
const uint16_t DEVICE_ID = 0x0000;

static uint8_t mac_address[MAC_ADDRESS_SIZE];

static uint16_t uart_tx_buffer_length = 0x00;
static uint8_t uart_tx_buffer[UART_BUFFER_SIZE] = { 0x00 };

static uint8_t radio_packet_protocol_size = 0;
static radio_packet_protocol_t radio_packet_protocol;

static volatile radio_state_t radio_state = RADIO_SLEEP;
static uint16_t radio_timer_count = 0;

static uint8_t radio_tx_buffer_size = 0;
static uint8_t radio_tx_buffer[128] = { 0x00 };

static int16_t radio_rx_rssi = 0;
static int8_t radio_rx_snr = 0;
static uint8_t radio_rx_buffer_size = 0;
static uint8_t radio_rx_buffer[128] = { 0x00 };

static uint8_t uart_2_data = 0x00;
static uint8_t uart_rx_buffer_size = 0;
static uint8_t uart_rx_buffer[128] = { 0x00 };

#ifdef EXAMPLE_TX

static volatile uint8_t cycle_timeout_count = 0;

#endif

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_LPTIM1_Init(void);
static void MX_RTC_Init(void);
static void MX_USART2_UART_Init(void);
void StartRadioTask(void *argument);
void StartMainTask(void *argument);
void mainTimerCallback(void *argument);

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
  MX_LPTIM1_Init();
  MX_RTC_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();

  /* USER CODE BEGIN RTOS_MUTEX */
	/* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* Create the semaphores(s) */
  /* creation of radioBinarySem */
  radioBinarySemHandle = osSemaphoreNew(1, 0, &radioBinarySem_attributes);

  /* creation of mainBinarySem */
  mainBinarySemHandle = osSemaphoreNew(1, 1, &mainBinarySem_attributes);

  /* USER CODE BEGIN RTOS_SEMAPHORES */

	/* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* Create the timer(s) */
  /* creation of mainTimer */
  mainTimerHandle = osTimerNew(mainTimerCallback, osTimerPeriodic, NULL, &mainTimer_attributes);

  /* USER CODE BEGIN RTOS_TIMERS */
	/* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
	/* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of radioTask */
  radioTaskHandle = osThreadNew(StartRadioTask, NULL, &radioTask_attributes);

  /* creation of mainTask */
  mainTaskHandle = osThreadNew(StartMainTask, NULL, &mainTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
	/* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
	/* add events, ... */
  /* USER CODE END RTOS_EVENTS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	while (1) {
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
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSIDiv = RCC_LSI_DIV1;
  RCC_OscInitStruct.HSEDiv = RCC_HSE_DIV1;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the SYSCLKSource, HCLK, PCLK1 and PCLK2 clocks dividers
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK3|RCC_CLOCKTYPE_HCLK
                              |RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1
                              |RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSE;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.AHBCLK3Divider = RCC_SYSCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief LPTIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_LPTIM1_Init(void)
{

  /* USER CODE BEGIN LPTIM1_Init 0 */

  /* USER CODE END LPTIM1_Init 0 */

  /* USER CODE BEGIN LPTIM1_Init 1 */

  /* USER CODE END LPTIM1_Init 1 */
  hlptim1.Instance = LPTIM1;
  hlptim1.Init.Clock.Source = LPTIM_CLOCKSOURCE_APBCLOCK_LPOSC;
  hlptim1.Init.Clock.Prescaler = LPTIM_PRESCALER_DIV1;
  hlptim1.Init.Trigger.Source = LPTIM_TRIGSOURCE_SOFTWARE;
  hlptim1.Init.OutputPolarity = LPTIM_OUTPUTPOLARITY_HIGH;
  hlptim1.Init.UpdateMode = LPTIM_UPDATE_IMMEDIATE;
  hlptim1.Init.CounterSource = LPTIM_COUNTERSOURCE_INTERNAL;
  hlptim1.Init.Input1Source = LPTIM_INPUT1SOURCE_GPIO;
  hlptim1.Init.Input2Source = LPTIM_INPUT2SOURCE_GPIO;
  if (HAL_LPTIM_Init(&hlptim1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN LPTIM1_Init 2 */

  /* USER CODE END LPTIM1_Init 2 */

}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv = 127;
  hrtc.Init.SynchPrediv = 255;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutRemap = RTC_OUTPUT_REMAP_NONE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  hrtc.Init.OutPutPullUp = RTC_OUTPUT_PULLUP_NONE;
  hrtc.Init.BinMode = RTC_BINARY_NONE;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */

}

/**
  * @brief SUBGHZ Initialization Function
  * @param None
  * @retval None
  */
void MX_SUBGHZ_Init(void)
{

  /* USER CODE BEGIN SUBGHZ_Init 0 */

  /* USER CODE END SUBGHZ_Init 0 */

  /* USER CODE BEGIN SUBGHZ_Init 1 */

  /* USER CODE END SUBGHZ_Init 1 */
  hsubghz.Init.BaudratePrescaler = SUBGHZSPI_BAUDRATEPRESCALER_8;
  if (HAL_SUBGHZ_Init(&hsubghz) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SUBGHZ_Init 2 */

  /* USER CODE END SUBGHZ_Init 2 */

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
  huart2.Init.BaudRate = 9600;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart2, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart2, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart2) != HAL_OK)
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
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, LED1_Pin|LED2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : IN1_Pin IN2_Pin IN3_Pin IN4_Pin */
  GPIO_InitStruct.Pin = IN1_Pin|IN2_Pin|IN3_Pin|IN4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : SW1_Pin SW2_Pin */
  GPIO_InitStruct.Pin = SW1_Pin|SW2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : LED1_Pin LED2_Pin */
  GPIO_InitStruct.Pin = LED1_Pin|LED2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : RF_MODE_Pin */
  GPIO_InitStruct.Pin = RF_MODE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(RF_MODE_GPIO_Port, &GPIO_InitStruct);
  HAL_GPIO_WritePin(RF_MODE_GPIO_Port, RF_MODE_Pin, GPIO_PIN_RESET);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {

	RS485_UART_RxCpltCallback(huart);

}

static void uart_usb_in_out_data_send(uint8_t *data, uint16_t size) {

	uint8_t send_data = 0x00;

	for (uint16_t i = 0; i < size; i++) {

		send_data = data[i];
		HAL_UART_Transmit(&huart2, &send_data, 1, 10);

	}

}

static void set_uart_usb_in_out_write_string(char *data) {

	uart_tx_buffer_length = 0;
	memset(uart_tx_buffer, 0x00, sizeof(uart_tx_buffer));

	uart_tx_buffer_length = sprintf((char*) uart_tx_buffer, "%s\r\n", data);

	uart_usb_in_out_data_send(uart_tx_buffer, uart_tx_buffer_length);

}

static void set_uart_usb_in_out_find_i2c_address(uint8_t address) {

	uart_tx_buffer_length = 0;
	memset(uart_tx_buffer, 0x00, sizeof(uart_tx_buffer));

	uart_tx_buffer_length = sprintf((char*) uart_tx_buffer,
			"I2C Address : %02X\r\n", address);

	uart_usb_in_out_data_send(uart_tx_buffer, uart_tx_buffer_length);

}

static void set_uart_usb_in_out_mac_address(uint8_t *mac_address) {

	uart_tx_buffer_length = 0;
	memset(uart_tx_buffer, 0x00, sizeof(uart_tx_buffer));

	uart_tx_buffer_length = sprintf((char*) uart_tx_buffer, "MAC Address : ");

	for (uint8_t i = 0; i < 8; i++) {

		uart_tx_buffer_length += sprintf(
				(char*) uart_tx_buffer + uart_tx_buffer_length, "%02X",
				mac_address[i]);

	}

	uart_tx_buffer_length += sprintf(
			(char*) uart_tx_buffer + uart_tx_buffer_length, "\r\n");

	uart_usb_in_out_data_send(uart_tx_buffer, uart_tx_buffer_length);

}

static void set_uart_usb_in_out_voltage(int16_t voltage) {

	uart_tx_buffer_length = 0;
	memset(uart_tx_buffer, 0x00, sizeof(uart_tx_buffer));

	uart_tx_buffer_length = sprintf((char*) uart_tx_buffer,
			"Voltage : %d V\r\n", voltage);

	uart_usb_in_out_data_send(uart_tx_buffer, uart_tx_buffer_length);

}

static void set_uart_usb_in_out_payload(
		radio_packet_protocol_t receive_radio_packet, int16_t rssi, int8_t snr) {

	uart_tx_buffer_length = 0;
	memset(uart_tx_buffer, 0x00, sizeof(uart_tx_buffer));

	uart_tx_buffer_length = sprintf((char*) uart_tx_buffer,
			"\r\nPacket Receive\r\n");

	uint16_t company_id = receive_radio_packet.Packet.company_id[0] << 8;
	company_id |= receive_radio_packet.Packet.company_id[1];

	uart_tx_buffer_length += sprintf(
			(char*) uart_tx_buffer + uart_tx_buffer_length,
			"Company ID : %d\r\n", company_id);

	uint16_t device_id = receive_radio_packet.Packet.device_id[0] << 8;
	device_id |= receive_radio_packet.Packet.device_id[1];

	uart_tx_buffer_length += sprintf(
			(char*) uart_tx_buffer + uart_tx_buffer_length,
			"Device ID : %d\r\n", device_id);

	uart_tx_buffer_length += sprintf(
			(char*) uart_tx_buffer + uart_tx_buffer_length, "MAC Address : ");

	for (uint8_t i = 0; i < 8; i++) {

		uart_tx_buffer_length += sprintf(
				(char*) uart_tx_buffer + uart_tx_buffer_length, "%02X",
				receive_radio_packet.Packet.mac_address[i]);

	}

	uart_tx_buffer_length += sprintf(
			(char*) uart_tx_buffer + uart_tx_buffer_length, "\r\n");

	uart_tx_buffer_length += sprintf(
			(char*) uart_tx_buffer + uart_tx_buffer_length,
			"Control Number : %d\r\n",
			receive_radio_packet.Packet.control_number);

	uart_tx_buffer_length += sprintf(
			(char*) uart_tx_buffer + uart_tx_buffer_length, "RSSI : %d\r\n",
			rssi);

	uart_tx_buffer_length += sprintf(
			(char*) uart_tx_buffer + uart_tx_buffer_length, "SNR : %d\r\n",
			snr);

	uart_tx_buffer_length += sprintf(
			(char*) uart_tx_buffer + uart_tx_buffer_length, "Payload : %s\r\n",
			receive_radio_packet.Packet.paylaod);

	uart_usb_in_out_data_send(uart_tx_buffer, uart_tx_buffer_length);

}
//
//static uint8_t get_i2c_scan_address() {
//
//	uint8_t count = 0;
//	HAL_StatusTypeDef result;
//
//	for (uint8_t i = 1; i < 128; i++) {
//
//		result = HAL_I2C_IsDeviceReady(&hi2c2, (uint16_t) (i << 1), 2, 2);
//
//		if (result == HAL_OK) {
//
//			set_uart_usb_in_out_find_i2c_address(i);
//
//			count++;
//
//		}
//
//	}
//
//	return count;
//
//}
//
//static void get_voltage_level() {
//
//	HAL_ADC_Start(&hadc);
//
//	HAL_ADC_PollForConversion(&hadc, 10);
//	float battery_adc = HAL_ADC_GetValue(&hadc);
//	battery_adc = battery_adc * 3.3 / 4095;
//	battery_adc *= 2;
//
//	HAL_ADC_Stop(&hadc);
//
//	set_uart_usb_in_out_voltage((int16_t) 4);
//
//}

/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartRadioTask */
/**
 * @brief  Function implementing the radioTask thread.
 * @param  argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartRadioTask */
void StartRadioTask(void *argument)
{
  /* USER CODE BEGIN 5 */

  for (;;)
  {
      osDelay(1000);
  }

  /* USER CODE END 5 */
}
/* USER CODE BEGIN Header_StartMainTask */
/**
 * @brief Function implementing the mainTask thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartMainTask */
void StartMainTask(void *argument)
{
  /* USER CODE BEGIN StartMainTask */

  App_Init();

  for (;;)
  {
      App_Process();
  }

  /* USER CODE END StartMainTask */
}
/* mainTimerCallback function */
void mainTimerCallback(void *argument)
{
  /* USER CODE BEGIN mainTimerCallback */

#ifdef EXAMPLE_TX

	cycle_timeout_count++;

	if (cycle_timeout_count > 100) {

		cycle_timeout_count = 0;
		osSemaphoreRelease(mainBinarySemHandle);

	}

#endif

	if (HAL_GPIO_ReadPin(GPIO_BUTTON_0_Port, GPIO_BUTTON_0_Pin) == 0x01) {

		HAL_GPIO_TogglePin(GPIO_LED_0_Port, GPIO_LED_0_Pin);

	}

	if (HAL_GPIO_ReadPin(GPIO_BUTTON_1_Port, GPIO_BUTTON_1_Pin) == 0x01) {

		HAL_GPIO_TogglePin(GPIO_LED_1_Port, GPIO_LED_1_Pin);

	}

  /* USER CODE END mainTimerCallback */
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM17 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM17)
  {
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
