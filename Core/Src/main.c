/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "cmsis_os.h"
#include "i2c.h"
#include "rtc.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stm32f4xx_hal.h"
#include "lcd.h"
#include "ir_decode.h"
#include "ntptimer.h"
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
// NEC IR 리모컨 타이밍 정의
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
volatile uint32_t ir_data = 0;
volatile uint8_t bit_cnt = 0;
volatile uint8_t ir_key_ready = 0;
volatile uint8_t receiving = 0;

//알람 시간 관련 전역 변수
uint8_t alarm_hour = 0;
uint8_t alarm_min = 0;
uint8_t alarm_sec = 0;

uint8_t alarm_setting_mode = 0;       // 1: 설정 중, 0: 설정 아님
uint8_t alarm_hour_input_idx = 0;
uint8_t alarm_min_input_idx = 0;
uint8_t alarm_sec_input_idx = 0;
uint8_t alarm_hour_digits[2] = {0};
uint8_t alarm_min_digits[2] = {0};
uint8_t alarm_sec_digits[2] = {0};
///////////////////////////////////////////
uint8_t setup_hour_digits[2] = {0, 0};
uint8_t setup_min_digits[2] = {0, 0};
uint8_t setup_sec_digits[2] = {0, 0};
uint8_t setup_hour_input_idx = 0;
uint8_t setup_min_input_idx = 0;
uint8_t setup_sec_input_idx = 0;

uint8_t digit_input_stage = 0;  // 0: 첫 자리, 1: 둘째 자리
uint8_t temp_digit = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void MX_FREERTOS_Init(void);
/* USER CODE BEGIN PFP */
// NTP 태스크 관련 함수들
void NTP_Task_Create(void);
void NTP_Task_Delete(void);
void NTP_Sync_Task(void *argument);

// NTP 태스크 핸들
osThreadId_t NTP_SyncTaskHandle = NULL;

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
  MX_USART2_UART_Init();
  MX_I2C1_Init();
  MX_RTC_Init();
  MX_TIM2_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  HAL_TIM_Base_Start(&htim2); // TIM2 타이머 시작
  reset_ir_state();

  // NTP 타이머 초기화 (ESP01과 디버그용 UART2 사용)
  NTP_Timer_Init(&huart1, &huart2);  // ESP01용 UART 활성화

  // 처음 한 번만 RTC 설정
  HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, 0x0000);  // 강제 초기화 (개발 중 1회)
  //if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR1) != 0x32F2) {
      Parse_CompileTime_And_Set_RTC();  // 최초 1회만 설정
  //}


  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();  /* Call init function for freertos objects (in cmsis_os2.c) */
  MX_FREERTOS_Init();

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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 4;
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

/* USER CODE BEGIN 4 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == GPIO_PIN_0) {
        if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET)
        {
            __HAL_TIM_SET_COUNTER(&htim2, 0); // Rising : 시간 초기
        }
        else
        {
            uint16_t delta = __HAL_TIM_GET_COUNTER(&htim2); // Falling : 시간 측정
            check_ir(delta);
        }
    }
}

/**
 * @brief  NTP 태스크 생성
 * @retval None
 */
void NTP_Task_Create(void) {
    if (NTP_SyncTaskHandle == NULL) {
        // 태스크 속성 정의
        const osThreadAttr_t ntp_task_attributes = {
            .name = "NTPSyncTask",
            .stack_size = 512 * 4,
            .priority = (osPriority_t) osPriorityNormal,
        };
        
        // 태스크 생성
        NTP_SyncTaskHandle = osThreadNew(NTP_Sync_Task, NULL, &ntp_task_attributes);
        
        if (NTP_SyncTaskHandle != NULL) {
            // 디버그 메시지 (UART2 사용)
            char msg[] = "NTP Sync Task Created\r\n";
            HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 100);
        }
    }
}

/**
 * @brief  NTP 태스크 삭제
 * @retval None
 */
void NTP_Task_Delete(void) {
    if (NTP_SyncTaskHandle != NULL) {
        osThreadTerminate(NTP_SyncTaskHandle);
        NTP_SyncTaskHandle = NULL;
        
        // 디버그 메시지
        char msg[] = "NTP Sync Task Deleted\r\n";
        HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 100);
    }
}

/**
 * @brief  NTP 동기화 태스크 (일회성)
 * @param  argument: 사용하지 않음
 * @retval None
 */
void NTP_Sync_Task(void *argument) {
    // NTP 동기화 요청
    NTP_RequestTimeSync();
    
    // 동기화 완료까지 대기 (최대 30초)
    uint32_t timeout = 30000; // 30초
    uint32_t start_time = osKernelGetTickCount();
    
    while ((osKernelGetTickCount() - start_time) < timeout) {
        // ESP 응답 처리
        NTP_ESP_ProcessResponse();
        
        // WiFi 상태 머신 실행
        NTP_WiFi_StateMachine();
        
        // 동기화 완료 확인
        if (NTP_IsTimeValid()) {
            char msg[] = "NTP Sync Completed Successfully\r\n";
            HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 100);
            break;
        }
        
        osDelay(100);
    }
    
    // 타임아웃 또는 완료 후 자동 종료
    if (!NTP_IsTimeValid()) {
        char msg[] = "NTP Sync Timeout\r\n";
        HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 100);
    }
    
    // 태스크 핸들 리셋 후 종료
    NTP_SyncTaskHandle = NULL;
    osThreadTerminate(osThreadGetId());
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    // NTP 타이머 UART 콜백 호출
    NTP_UART_RxCallback(huart);
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
