#ifndef __LCD1602_I2C_H__
#define __LCD1602_I2C_H__

#include "stm32f4xx_hal.h"

void LCD_Init(I2C_HandleTypeDef *hi2c);
void LCD_Print(char *str);
void LCD_SetCursor(uint8_t row, uint8_t col);
void LCD_Clear(void);

#endif
