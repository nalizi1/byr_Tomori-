//
// Created by 10616 on 2025/9/21.
//

#ifndef LED_H
#define LED_H
#include "stm32f4xx.h"
#include "tim.h"

typedef enum {
    ON,
    OFF,
    BREATHING,
    BLINKING
}LED_MODE;


typedef struct {
    LED_MODE mode;
    int brightness;
    float blink_frequency;
    int breathing_period;
} LED;

void Board_Alive(void);
void LED_Init(void);
void LED_ON(void);
void LED_OFF(void);
void LED_SetBrightness(uint8_t brightness); // 0-255
void LED_StartBlink(float freq);
void LED_StopBlink(void);
void LED_StartBreathing(float period_ms);
void LED_StopBreathing(void);

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim);

#endif //LED_H
