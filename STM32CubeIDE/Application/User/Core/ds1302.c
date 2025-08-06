#include "ds1302.h"
#include "main.h"

// DS1302 핀 정의
#define DS1302_CE_HIGH()     HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_SET)
#define DS1302_CE_LOW()      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET)
#define DS1302_CLK_HIGH()    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET)
#define DS1302_CLK_LOW()     HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET)
#define DS1302_IO_HIGH()     HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET)
#define DS1302_IO_LOW()      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET)
#define DS1302_IO_READ()     HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5)
#define DS1302_IO_INPUT_MODE()  ds1302_set_pin_input()
#define DS1302_IO_OUTPUT_MODE() ds1302_set_pin_output()

// BCD 변환 함수
static uint8_t dec_to_bcd(uint8_t val) {
    return ((val / 10) << 4) | (val % 10);
}

// GPIO 모드 설정 함수
void ds1302_set_pin_output(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

void ds1302_set_pin_input(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

// 바이트 쓰기
static void ds1302_write_byte(uint8_t byte) {
    DS1302_IO_OUTPUT_MODE();
    for (int i = 0; i < 8; ++i) {
        (byte & 0x01) ? DS1302_IO_HIGH() : DS1302_IO_LOW();
        DS1302_CLK_HIGH();
        byte >>= 1;
        DS1302_CLK_LOW();
    }
}

// DS1302에 시간 쓰기
void DS1302_SetTime(RTC_TimeTypeDef* time, RTC_DateTypeDef* date) {
    DS1302_CE_LOW();
    DS1302_CLK_LOW();
    DS1302_CE_HIGH();

    // Write protect 해제
    ds1302_write_byte(0x8E); // Control register address
    ds1302_write_byte(0x00); // Disable write protect

    // 시간 설정
    ds1302_write_byte(0x80); // Seconds (write)
    ds1302_write_byte(dec_to_bcd(time->Seconds));

    ds1302_write_byte(0x82); // Minutes
    ds1302_write_byte(dec_to_bcd(time->Minutes));

    ds1302_write_byte(0x84); // Hours
    ds1302_write_byte(dec_to_bcd(time->Hours));

    ds1302_write_byte(0x86); // Date
    ds1302_write_byte(dec_to_bcd(date->Date));

    ds1302_write_byte(0x88); // Month
    ds1302_write_byte(dec_to_bcd(date->Month));

    ds1302_write_byte(0x8A); // Day of week
    ds1302_write_byte(dec_to_bcd(date->WeekDay));

    ds1302_write_byte(0x8C); // Year
    ds1302_write_byte(dec_to_bcd(date->Year));

    // Write protect 다시 설정
    ds1302_write_byte(0x8E); // Control register
    ds1302_write_byte(0x80); // Enable write protect

    DS1302_CE_LOW();
}
