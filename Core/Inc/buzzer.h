/*
 * buzzer.h
 *
 *  Created on: Aug 1, 2025
 *      Author: KCCISTC
 */

#ifndef APPLICATION_USER_CORE_BUZZER_H_
#define APPLICATION_USER_CORE_BUZZER_H_

#define BUZZER_PORT GPIOA
#define BUZZER_PIN  GPIO_PIN_4
void Buzzer_On(void);
void Buzzer_Off(void);

#endif /* APPLICATION_USER_CORE_BUZZER_H_ */
