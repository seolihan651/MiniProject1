#ifndef __DS1302_H__
#define __DS1302_H__

#include "stm32f4xx_hal.h"
#include "rtc.h"

void DS1302_SetTime(RTC_TimeTypeDef* time, RTC_DateTypeDef* date);

#endif
