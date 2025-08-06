/* ntptimer.c */
#include "ntptimer.h"

#include "cmsis_os2.h"

/* Private includes ----------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/
#define WIFI_SSID "KCCI_STC_S"
#define WIFI_PASSWORD "kcci098#"
#define NTP_IP "27.96.158.81"
#define NTP_PORT "123"
#define NTP_DEBUG_MODE 1

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
static UART_HandleTypeDef *huart_esp_module;
static UART_HandleTypeDef *huart_debug_module;

// UART 수신 변수들
static uint8_t uart_esp_rx_data;
static uint8_t uart_debug_rx_data;

// 링 버퍼들
static NTP_RingBuffer_t esp_to_system_buffer;
static NTP_RingBuffer_t system_to_esp_buffer;

// WiFi 및 NTP 상태 관리 변수들
static volatile NTP_WiFiState_t ntp_wifi_state = NTP_WIFI_INIT;
static uint32_t ntp_command_timer = 0;
static uint8_t ntp_retry_count = 0;
static uint8_t ntp_state_executed = 0;
static volatile uint32_t ntp_last_request_time = 0;

// ESP 응답 버퍼
static char ntp_esp_response_buffer[512];
static volatile uint16_t ntp_esp_response_index = 0;

// NTP 데이터 처리 변수들
static uint8_t ntp_response_data[48];
static volatile uint8_t ntp_data_received = 0;

// 현재 시간 데이터
static NTP_TimeData_t current_time_data = {0};

// NTP 요청 관련 플래그
static volatile uint8_t ntp_sync_requested = 0;

// FreeRTOS 핸들들
osMutexId NTP_MutexHandle;

/* Private function prototypes -----------------------------------------------*/
static void NTP_RingBuffer_Init(NTP_RingBuffer_t *rb);
static int NTP_RingBuffer_Put(NTP_RingBuffer_t *rb, uint8_t data);
static int NTP_RingBuffer_Get(NTP_RingBuffer_t *rb, uint8_t *data);
static void NTP_ESP_SendString(const char *str);
static void NTP_Debug_Print(const char *msg);
static int NTP_ESP_CheckResponse(const char *expected_response);
static void NTP_ESP_ClearResponseBuffer(void);

/* Private user code ---------------------------------------------------------*/

/**
 * @brief  NTP 타이머 초기화
 * @param  huart_esp: ESP 모듈과 연결된 UART 핸들
 * @param  huart_debug: 디버그 출력용 UART 핸들
 * @retval None
 */
void NTP_Timer_Init(UART_HandleTypeDef *huart_esp,
                    UART_HandleTypeDef *huart_debug) {
    huart_esp_module = huart_esp;
    huart_debug_module = huart_debug;

    // 링 버퍼 초기화
    NTP_RingBuffer_Init(&esp_to_system_buffer);
    NTP_RingBuffer_Init(&system_to_esp_buffer);

    // 뮤텍스 생성
    const osMutexAttr_t ntp_mutex_attr = {.name = "NTP_Mutex"};
    NTP_MutexHandle = osMutexNew(&ntp_mutex_attr);

    // UART 수신 인터럽트 시작
    HAL_UART_Receive_IT(huart_esp_module, &uart_esp_rx_data, 1);
    HAL_UART_Receive_IT(huart_debug_module, &uart_debug_rx_data, 1);

    // 초기화 메시지
    NTP_Debug_Print("NTP Timer initialized\r\n");
}

/**
 * @brief  NTP 타이머 태스크
 * @param  argument: 사용하지 않음
 * @retval None
 */
void NTP_Timer_Task(void const *argument) {
    for (;;) {
        // ESP 응답 처리
        NTP_ESP_ProcessResponse();

        // WiFi 상태 머신 실행
        NTP_WiFi_StateMachine();

        // 100ms 대기
        osDelay(100);
    }
}

/**
 * @brief  UART 수신 콜백 함수
 * @param  huart: UART 핸들
 * @retval None
 */
void NTP_UART_RxCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == huart_esp_module->Instance) {
        // ESP 모듈로부터 수신
        NTP_RingBuffer_Put(&esp_to_system_buffer, uart_esp_rx_data);
        HAL_UART_Receive_IT(huart_esp_module, &uart_esp_rx_data, 1);
    } else if (huart->Instance == huart_debug_module->Instance) {
        // 디버그 포트로부터 수신
        NTP_RingBuffer_Put(&system_to_esp_buffer, uart_debug_rx_data);
        HAL_UART_Receive_IT(huart_debug_module, &uart_debug_rx_data, 1);
    }
}

/**
 * @brief  현재 시간 데이터 가져오기
 * @param  time_data: 시간 데이터를 저장할 구조체 포인터
 * @retval None
 */
void NTP_GetCurrentTime(NTP_TimeData_t *time_data) {
    if (osMutexAcquire(NTP_MutexHandle, osWaitForever) == osOK) {
        *time_data = current_time_data;
        osMutexRelease(NTP_MutexHandle);
    }
}

/**
 * @brief  현재 시간 출력
 * @retval None
 */
void NTP_PrintTime(void) {
    NTP_TimeData_t time_data;
    NTP_GetCurrentTime(&time_data);

    if (time_data.valid) {
        char time_msg[100];
        sprintf(time_msg,
                "Current Time (KST): 20%02d-%02d-%02d %02d:%02d:%02d\r\n",
                time_data.years, time_data.months, time_data.days,
                time_data.hours, time_data.minutes, time_data.seconds);
        HAL_UART_Transmit(huart_debug_module, (uint8_t *)time_msg,
                          strlen(time_msg), 100);
    } else {
        NTP_Debug_Print("Time not synchronized yet\r\n");
    }
}

/**
 * @brief  시간 유효성 확인
 * @retval 1: 유효, 0: 무효
 */
uint8_t NTP_IsTimeValid(void) {
    return current_time_data.valid;
}

/* Private functions ---------------------------------------------------------*/

/**
 * @brief  링 버퍼 초기화
 */
static void NTP_RingBuffer_Init(NTP_RingBuffer_t *rb) {
    rb->head = 0;
    rb->tail = 0;
}

/**
 * @brief  링 버퍼에 데이터 추가
 */
static int NTP_RingBuffer_Put(NTP_RingBuffer_t *rb, uint8_t data) {
    uint16_t next_head = (rb->head + 1) % 256;
    if (next_head == rb->tail) {
        return -1;  // 버퍼 가득참
    }
    rb->buffer[rb->head] = data;
    rb->head = next_head;
    return 0;
}

/**
 * @brief  링 버퍼에서 데이터 가져오기
 */
static int NTP_RingBuffer_Get(NTP_RingBuffer_t *rb, uint8_t *data) {
    if (rb->head == rb->tail) {
        return -1;  // 버퍼 비어있음
    }
    *data = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1) % 256;
    return 0;
}

/**
 * @brief  ESP 모듈에 문자열 전송
 */
static void NTP_ESP_SendString(const char *str) {
#if NTP_DEBUG_MODE
    char tx_buf[128];
    sprintf(tx_buf, ">> %s\r\n", str);
    HAL_UART_Transmit(huart_debug_module, (uint8_t *)tx_buf, strlen(tx_buf),
                      100);
#endif

    char cmd_buf[128];
    sprintf(cmd_buf, "%s\r\n", str);
    HAL_UART_Transmit(huart_esp_module, (uint8_t *)cmd_buf, strlen(cmd_buf),
                      100);
    osDelay(2000);
}

/**
 * @brief  디버그 메시지 출력
 */
static void NTP_Debug_Print(const char *msg) {
#if NTP_DEBUG_MODE
    const char *prefix = "--ntp-debug :: ";
    HAL_UART_Transmit(huart_debug_module, (uint8_t *)prefix, strlen(prefix),
                      100);
    HAL_UART_Transmit(huart_debug_module, (uint8_t *)msg, strlen(msg), 100);
#endif
}

/**
 * @brief  ESP 응답 확인
 */
static int NTP_ESP_CheckResponse(const char *expected_response) {
    if (strstr(ntp_esp_response_buffer, expected_response) != NULL) {
        return 1;
    }
    return 0;
}

/**
 * @brief  ESP 응답 버퍼 클리어
 */
static void NTP_ESP_ClearResponseBuffer(void) {
    memset(ntp_esp_response_buffer, 0, sizeof(ntp_esp_response_buffer));
    ntp_esp_response_index = 0;
}

/**
 * @brief  ESP 응답 처리 (외부 호출 가능)
 */
void NTP_ESP_ProcessResponse(void) {
    uint8_t data;
    static uint8_t ipd_detected = 0;
    static uint8_t ipd_data_count = 0;

    while (NTP_RingBuffer_Get(&esp_to_system_buffer, &data) == 0) {
        // NTP 데이터 수신 중인지 확인
        if (ipd_detected && ipd_data_count < 48) {
            ntp_response_data[ipd_data_count++] = data;
            if (ipd_data_count >= 48) {
                ntp_data_received = 1;
                ipd_detected = 0;
                ipd_data_count = 0;
            }
            continue;
        }

        // 응답 버퍼에 저장
        if (ntp_esp_response_index < sizeof(ntp_esp_response_buffer) - 1) {
            ntp_esp_response_buffer[ntp_esp_response_index++] = data;
            ntp_esp_response_buffer[ntp_esp_response_index] = '\0';
        }

        // "+IPD,4,48:" 패턴 감지
        if (strstr(ntp_esp_response_buffer, "+IPD,4,48:") != NULL &&
            !ipd_detected) {
            ipd_detected = 1;
            ipd_data_count = 0;
            ntp_data_received = 0;
        }

        // 버퍼가 가득 차면 절반 삭제
        if (ntp_esp_response_index >= sizeof(ntp_esp_response_buffer) - 1) {
            uint16_t half_size = sizeof(ntp_esp_response_buffer) / 2;
            memmove(ntp_esp_response_buffer,
                    ntp_esp_response_buffer + half_size, half_size);
            ntp_esp_response_index = half_size;
            ntp_esp_response_buffer[ntp_esp_response_index] = '\0';
        }
    }
}

/**
 * @brief  WiFi 상태 머신 (외부 호출 가능)
 */
void NTP_WiFi_StateMachine(void) {
    uint32_t current_time = osKernelSysTick();
    static NTP_WiFiState_t last_state = NTP_WIFI_INIT;
    static uint8_t init_done = 0;

    // 초기화
    if (!init_done) {
        NTP_Debug_Print("ESP-01 initializing...\r\n");
        NTP_ESP_ClearResponseBuffer();
        NTP_ESP_SendString("AT");
        ntp_wifi_state = NTP_WIFI_INIT;
        ntp_command_timer = current_time;
        ntp_retry_count = 0;
        ntp_state_executed = 0;
        init_done = 1;
        return;
    }

    // 상태 변경 시 플래그 리셋
    if (ntp_wifi_state != last_state) {
        ntp_state_executed = 0;
        last_state = ntp_wifi_state;
    }

    switch (ntp_wifi_state) {
        case NTP_WIFI_INIT:
            if (NTP_ESP_CheckResponse("OK")) {
                NTP_Debug_Print("ESP AT command OK\r\n");
                NTP_ESP_ClearResponseBuffer();
                NTP_ESP_SendString("AT+CWMODE=1");
                ntp_wifi_state = NTP_WIFI_IDLE;
                ntp_command_timer = current_time;
                ntp_retry_count = 0;
            } else if (current_time - ntp_command_timer >
                       NTP_COMMAND_TIMEOUT_MS) {
                if (ntp_retry_count < NTP_MAX_RETRY) {
                    ntp_retry_count++;
                    NTP_Debug_Print("ESP AT command retry...\r\n");
                    NTP_ESP_ClearResponseBuffer();
                    NTP_ESP_SendString("AT");
                    ntp_command_timer = current_time;
                } else {
                    NTP_Debug_Print("ESP initialization failed\r\n");
                    ntp_retry_count = 0;
                    osDelay(5000);
                    NTP_ESP_ClearResponseBuffer();
                    ntp_command_timer = current_time;
                }
            }
            break;

        case NTP_WIFI_IDLE:
            if (NTP_ESP_CheckResponse("OK")) {
                NTP_Debug_Print("Station mode set successfully\r\n");
                NTP_ESP_ClearResponseBuffer();
                char cmd[128];
                sprintf(cmd, "AT+CWJAP=\"%s\",\"%s\"", WIFI_SSID,
                        WIFI_PASSWORD);
                NTP_ESP_SendString(cmd);
                ntp_wifi_state = NTP_WIFI_CONNECTING;
                ntp_command_timer = current_time;
                NTP_Debug_Print("WiFi connecting...\r\n");
            } else if (current_time - ntp_command_timer >
                       NTP_COMMAND_TIMEOUT_MS) {
                if (ntp_retry_count < NTP_MAX_RETRY) {
                    ntp_retry_count++;
                    NTP_Debug_Print("Station mode retry...\r\n");
                    NTP_ESP_ClearResponseBuffer();
                    NTP_ESP_SendString("AT+CWMODE=1");
                    ntp_command_timer = current_time;
                } else {
                    NTP_Debug_Print("Station mode failed\r\n");
                    ntp_wifi_state = NTP_WIFI_INIT;
                    ntp_retry_count = 0;
                    osDelay(5000);
                }
            }
            break;

        case NTP_WIFI_CONNECTING:
            if (NTP_ESP_CheckResponse("WIFI GOT IP")) {
                NTP_Debug_Print("WiFi connected successfully!\r\n");
                ntp_wifi_state = NTP_WIFI_CONNECTED;
                ntp_retry_count = 0;
                NTP_ESP_ClearResponseBuffer();
            } else if (current_time - ntp_command_timer >
                       NTP_COMMAND_TIMEOUT_MS) {
                if (ntp_retry_count < NTP_MAX_RETRY) {
                    ntp_retry_count++;
                    NTP_Debug_Print("WiFi connection retry...\r\n");
                    NTP_ESP_ClearResponseBuffer();
                    char cmd[128];
                    sprintf(cmd, "AT+CWJAP=\"%s\",\"%s\"", WIFI_SSID,
                            WIFI_PASSWORD);
                    osDelay(5000);
                    NTP_ESP_SendString(cmd);
                    ntp_command_timer = current_time;
                } else {
                    NTP_Debug_Print("WiFi connection failed\r\n");
                    ntp_retry_count = 0;
                    osDelay(5000);
                }
            }
            break;

        case NTP_WIFI_CONNECTED:
            if (!ntp_state_executed) {
                NTP_ESP_SendString("AT+CIPMUX=1");
                osDelay(2000);
                ntp_wifi_state = NTP_CIPMUX_SETTING;
                ntp_command_timer = current_time;
                NTP_Debug_Print("Setting CIPMUX mode...\r\n");
                ntp_state_executed = 1;
            }
            break;

        case NTP_CIPMUX_SETTING:
            if (NTP_ESP_CheckResponse("OK")) {
                NTP_Debug_Print("CIPMUX set successfully\r\n");
                NTP_ESP_ClearResponseBuffer();
                NTP_ESP_SendString("AT+CIPSTART=4,\"UDP\",\"" NTP_IP
                                   "\"," NTP_PORT);
                ntp_wifi_state = NTP_UDP_CONNECTING;
                ntp_command_timer = current_time;
                NTP_Debug_Print("NTP server connecting...\r\n");
                ntp_retry_count = 0;
            } else if (current_time - ntp_command_timer >
                       NTP_COMMAND_TIMEOUT_MS) {
                if (ntp_retry_count < NTP_MAX_RETRY) {
                    ntp_retry_count++;
                    NTP_Debug_Print("CIPMUX setting retry...\r\n");
                    NTP_ESP_ClearResponseBuffer();
                    NTP_ESP_SendString("AT+CIPMUX=1");
                    ntp_command_timer = current_time;
                } else {
                    NTP_Debug_Print("CIPMUX setting failed\r\n");
                    ntp_wifi_state = NTP_WIFI_CONNECTED;
                    ntp_retry_count = 0;
                }
            }
            break;

        case NTP_UDP_CONNECTING:
            if (NTP_ESP_CheckResponse("CONNECT") ||
                NTP_ESP_CheckResponse("ALREADY CONNECTED")) {
                NTP_Debug_Print("UDP connection established\r\n");
                osDelay(500);
                NTP_ESP_ClearResponseBuffer();
                ntp_wifi_state = NTP_UDP_CONNECTED;
                ntp_retry_count = 0;
                NTP_Debug_Print("Ready for NTP communication\r\n");
            } else if (current_time - ntp_command_timer >
                       NTP_COMMAND_TIMEOUT_MS) {
                if (ntp_retry_count < NTP_MAX_RETRY) {
                    ntp_retry_count++;
                    NTP_Debug_Print("UDP connection retry...\r\n");
                    NTP_ESP_ClearResponseBuffer();
                    NTP_ESP_SendString("AT+CIPSTART=4,\"UDP\",\"" NTP_IP
                                       "\"," NTP_PORT);
                    ntp_command_timer = current_time;
                } else {
                    NTP_Debug_Print("UDP connection failed\r\n");
                    ntp_wifi_state = NTP_WIFI_CONNECTED;
                    ntp_retry_count = 0;
                }
            }
            break;

        case NTP_UDP_CONNECTED:
            if (!ntp_state_executed) {
                if (ntp_sync_requested) {
                    NTP_ESP_SendString("AT+CIPSEND=4,48");
                    ntp_wifi_state = NTP_REQUESTING;
                    ntp_command_timer = current_time;
                    ntp_last_request_time = current_time;
                    ntp_sync_requested = 0;  // 요청 플래그 리셋
                    NTP_Debug_Print("NTP send command issued\r\n");
                    ntp_state_executed = 1;
                }
            }
            break;

        case NTP_REQUESTING:
            if (current_time - ntp_command_timer > NTP_COMMAND_TIMEOUT_MS) {
                NTP_Debug_Print("NTP send command timeout\r\n");
                ntp_wifi_state = NTP_COMPLETE;
                ntp_state_executed = 0;
            } else if (NTP_ESP_CheckResponse(">")) {
                NTP_Debug_Print("Ready to send NTP data\r\n");
                NTP_ESP_ClearResponseBuffer();
                uint8_t ntp_packet[48] = {0};
                ntp_packet[0] = 0x1B;
                HAL_UART_Transmit(huart_esp_module, ntp_packet, 48, 1000);
                NTP_Debug_Print("NTP request sent\r\n");
                ntp_command_timer = current_time;
            } else if (NTP_ESP_CheckResponse("+IPD,4,48:") &&
                       ntp_data_received) {
                NTP_Debug_Print("NTP response received!\r\n");

                // NTP 타임스탬프 추출
                uint32_t ntp_timestamp =
                    ((uint32_t)ntp_response_data[40] << 24) |
                    ((uint32_t)ntp_response_data[41] << 16) |
                    ((uint32_t)ntp_response_data[42] << 8) |
                    ((uint32_t)ntp_response_data[43]);

                if (ntp_timestamp > 2208988800UL) {
                    uint32_t unix_timestamp = ntp_timestamp - 2208988800UL;
                    uint32_t kst_timestamp = unix_timestamp + (9 * 3600);

                    // 일수 계산 (1970년 1월 1일부터)
                    uint32_t days_since_epoch = kst_timestamp / 86400;
                    uint32_t seconds_in_day = kst_timestamp % 86400;

                    // 연도 계산
                    uint32_t year = 1970;
                    uint32_t days_in_year;
                    while (
                        days_since_epoch >=
                        (days_in_year = ((year % 4 == 0 && year % 100 != 0) ||
                                         (year % 400 == 0))
                                            ? 366
                                            : 365)) {
                        days_since_epoch -= days_in_year;
                        year++;
                    }

                    // 월과 일 계산
                    uint8_t days_in_month[] = {31, 28, 31, 30, 31, 30,
                                               31, 31, 30, 31, 30, 31};
                    if ((year % 4 == 0 && year % 100 != 0) ||
                        (year % 400 == 0)) {
                        days_in_month[1] = 29;  // 윤년
                    }

                    uint32_t month = 1;
                    while (days_since_epoch >= days_in_month[month - 1]) {
                        days_since_epoch -= days_in_month[month - 1];
                        month++;
                    }
                    uint32_t day = days_since_epoch + 1;

                    // 뮤텍스로 보호하여 시간 데이터 업데이트
                    if (osMutexAcquire(NTP_MutexHandle, osWaitForever) ==
                        osOK) {
                        current_time_data.years =
                            year - 2000;  // 2000년 기준 (예: 2025 -> 25)
                        current_time_data.months = month;
                        current_time_data.days = day;
                        current_time_data.hours = (seconds_in_day / 3600) % 24;
                        current_time_data.minutes = (seconds_in_day / 60) % 60;
                        current_time_data.seconds = seconds_in_day % 60;
                        current_time_data.unix_timestamp = unix_timestamp;
                        current_time_data.valid = 1;
                        osMutexRelease(NTP_MutexHandle);
                    }

                    char time_msg[100];
                    sprintf(
                        time_msg,
                        "NTP Time (KST): 20%02d-%02d-%02d %02d:%02d:%02d\r\n",
                        current_time_data.years, current_time_data.months,
                        current_time_data.days, current_time_data.hours,
                        current_time_data.minutes, current_time_data.seconds);
                    HAL_UART_Transmit(huart_debug_module, (uint8_t *)time_msg,
                                      strlen(time_msg), 100);
                } else {
                    NTP_Debug_Print("Invalid NTP timestamp received\r\n");
                }

                ntp_data_received = 0;
                ntp_wifi_state = NTP_COMPLETE;
            }
            break;

        case NTP_COMPLETE:
            if (!ntp_state_executed) {
                NTP_Debug_Print("NTP time synchronized\r\n");
                ntp_state_executed = 1;
                ntp_command_timer = current_time;
            }
            if (current_time - ntp_command_timer > 3000) {
                NTP_ESP_ClearResponseBuffer();
                ntp_wifi_state = NTP_UDP_CONNECTED;
                ntp_state_executed = 0;
                NTP_Debug_Print("Ready for next NTP request\r\n");
            }
            break;

        default:
            ntp_wifi_state = NTP_WIFI_INIT;
            break;
    }
}

/**
 * @brief  NTP 시간 동기화 요청
 * @retval None
 */
void NTP_RequestTimeSync(void) {
    if (osMutexAcquire(NTP_MutexHandle, osWaitForever) == osOK) {
        ntp_sync_requested = 1;
        osMutexRelease(NTP_MutexHandle);
        NTP_Debug_Print("NTP time sync requested\r\n");
    }
}
