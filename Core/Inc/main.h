/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdint.h>
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

extern volatile uint32_t ir_data;
extern volatile uint8_t bit_cnt;
extern volatile uint8_t ir_key_ready;
extern volatile uint8_t receiving;
///////////////////////////
extern uint8_t alarm_hour;
extern uint8_t alarm_min;
extern uint8_t alarm_sec;

extern uint8_t alarm_setting_mode;       // 1: 설정 중, 0: 설정 아님

extern uint8_t alarm_hour_digits[2];
extern uint8_t alarm_min_digits[2];
extern uint8_t alarm_sec_digits[2];

extern uint8_t alarm_hour_input_idx;
extern uint8_t alarm_min_input_idx;
extern uint8_t alarm_sec_input_idx;
/////////////////////////////////
extern uint8_t digit_input_stage;  // 0: 첫 자리, 1: 둘째 자리
extern uint8_t temp_digit;

extern uint8_t setup_hour_digits[2];
extern uint8_t setup_min_digits[2];
extern uint8_t setup_sec_digits[2];

extern uint8_t setup_hour_input_idx;
extern uint8_t setup_min_input_idx;
extern uint8_t setup_sec_input_idx;

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

typedef enum {
	MODE_IDLE = 0,
    MODE_SET_TIME,
    MODE_SET_ALARM
} SystemMode;

typedef enum {
	ALARM_FIELD_NONE = 0,
    ALARM_FIELD_HOUR,
    ALARM_FIELD_MINUTE,
	ALARM_FIELD_SECOND
} AlarmSelectMode;

typedef enum {
	TIME_FIELD_NONE = 0,
    TIME_FIELD_HOUR,
    TIME_FIELD_MINUTE,
    TIME_FIELD_SECOND
} TimeSelectMode;

extern SystemMode current_mode;

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define B1_Pin GPIO_PIN_13
#define B1_GPIO_Port GPIOC
#define B1_EXTI_IRQn EXTI15_10_IRQn
#define IR_Receiver_Pin GPIO_PIN_0
#define IR_Receiver_GPIO_Port GPIOA
#define IR_Receiver_EXTI_IRQn EXTI0_IRQn
#define USART2_TX_Pin GPIO_PIN_2
#define USART2_TX_GPIO_Port GPIOA
#define USART2_RX_Pin GPIO_PIN_3
#define USART2_RX_GPIO_Port GPIOA
#define LED_RED_Pin GPIO_PIN_4
#define LED_RED_GPIO_Port GPIOA
#define DS1302_CLK_Pin GPIO_PIN_5
#define DS1302_CLK_GPIO_Port GPIOA
#define DS1302_IO_Pin GPIO_PIN_6
#define DS1302_IO_GPIO_Port GPIOA
#define DS1302_RST_Pin GPIO_PIN_7
#define DS1302_RST_GPIO_Port GPIOA
#define USART1_RX_Pin GPIO_PIN_10
#define USART1_RX_GPIO_Port GPIOA
#define TCK_Pin GPIO_PIN_14
#define TCK_GPIO_Port GPIOA
#define SWO_Pin GPIO_PIN_3
#define SWO_GPIO_Port GPIOB
#define USART1_TX_Pin GPIO_PIN_6
#define USART1_TX_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */
#define LEADER_MIN 3500
#define LEADER_MAX 5500
#define BIT0_MIN   365
#define BIT0_MAX   765
#define BIT1_MIN   1390
#define BIT1_MAX   1990
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
