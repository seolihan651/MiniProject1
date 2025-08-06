/* ntptimer.h */
#ifndef __NTPTIMER_H__
#define __NTPTIMER_H__

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Exported types ------------------------------------------------------------*/
typedef struct {
    uint8_t buffer[256];
    volatile uint16_t head;
    volatile uint16_t tail;
} NTP_RingBuffer_t;

typedef enum {
    NTP_WIFI_INIT,
    NTP_WIFI_IDLE,
    NTP_WIFI_CONNECTING,
    NTP_WIFI_CONNECTED,
    NTP_CIPMUX_SETTING,
    NTP_UDP_CONNECTING,
    NTP_UDP_CONNECTED,
    NTP_REQUESTING,
    NTP_COMPLETE
} NTP_WiFiState_t;

typedef enum {
    NTP_THREAD_IDLE,
    NTP_THREAD_RUNNING,
    NTP_THREAD_SUCCESS,
    NTP_THREAD_ERROR
} NTP_ThreadState_t;

typedef struct {
    uint8_t years;
    uint8_t months;
    uint8_t days;
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
    uint32_t unix_timestamp;
    uint8_t valid;
} NTP_TimeData_t;

/* Exported constants --------------------------------------------------------*/
#define NTP_REQUEST_INTERVAL_MS     10000   // 10초
#define NTP_COMMAND_TIMEOUT_MS      5000    // 5초
#define NTP_MAX_RETRY               3

/* Exported macro ------------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
void NTP_Timer_Init(UART_HandleTypeDef *huart_esp, UART_HandleTypeDef *huart_debug);
void NTP_Timer_Task(void const *argument);
void NTP_UART_RxCallback(UART_HandleTypeDef *huart);
void NTP_GetCurrentTime(NTP_TimeData_t *time_data);
void NTP_PrintTime(void);
uint8_t NTP_IsTimeValid(void);
void NTP_RequestTimeSync(void);
NTP_ThreadState_t NTP_GetSyncThreadState(void);
void NTP_StartSyncThread(void);
void NTP_StopSyncThread(void);

// 테스트 및 디버그 함수들
void NTP_TestSyncOnce(void);
void NTP_PrintThreadStatus(void);

// 내부 함수들 (외부에서 직접 호출용)
void NTP_ESP_ProcessResponse(void);
void NTP_WiFi_StateMachine(void);
void NTP_SyncThread_Task(void *argument);

/* Exported variables --------------------------------------------------------*/
extern osMutexId NTP_MutexHandle;
extern osThreadId NTP_SyncThreadHandle;

#ifdef __cplusplus
}
#endif

#endif /* __NTPTIMER_H__ */
