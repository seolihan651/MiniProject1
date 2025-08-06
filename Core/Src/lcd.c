#include "lcd.h"
#include "string.h"
#include "stdio.h"
#include "cmsis_os.h"
#include "main.h"

#define LCD_ADDR (0x27 << 1)

extern I2C_HandleTypeDef hi2c1;

static void LCD_Send_Cmd(uint8_t cmd);
static void LCD_Send_Data(uint8_t data);
static void LCD_Send(uint8_t data, uint8_t mode);

static void LCD_Delay(uint32_t ms) {
    osDelay(ms);
}

void LCD_Init(I2C_HandleTypeDef *hi2c) {
    LCD_Delay(50);
    LCD_Send_Cmd(0x33);
    LCD_Send_Cmd(0x32);
    LCD_Send_Cmd(0x28);
    LCD_Send_Cmd(0x0C);
    LCD_Send_Cmd(0x06);
    LCD_Send_Cmd(0x01);
    LCD_Delay(5);
}

void LCD_SetCursor(uint8_t row, uint8_t col) {
    uint8_t addr = (row == 0) ? 0x80 + col : 0xC0 + col;
    LCD_Send_Cmd(addr);
}

void LCD_Print(char *str) {
    while (*str) {
        LCD_Send_Data(*str++);
    }
}

void LCD_Clear(void) {
    LCD_Send_Cmd(0x01);
    LCD_Delay(2);
}

static void LCD_Send_Cmd(uint8_t cmd) {
    LCD_Send(cmd & 0xF0, 0);
    LCD_Send((cmd << 4) & 0xF0, 0);
}

static void LCD_Send_Data(uint8_t data) {
    LCD_Send(data & 0xF0, 1);
    LCD_Send((data << 4) & 0xF0, 1);
}

static void LCD_Send(uint8_t data, uint8_t mode) {
    uint8_t data_u = data | (mode ? 0x01 : 0x00) | 0x08; // backlight
    uint8_t en_high = data_u | 0x04;
    uint8_t en_low  = data_u & ~0x04;

    uint8_t data_arr[2] = {en_high, en_low};
    HAL_I2C_Master_Transmit(&hi2c1, LCD_ADDR, data_arr, 2, 100);
    LCD_Delay(1);
}
