#include "led.h"
#include "flash_store.h"

#include <cmd.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "gpio.h"
#include "tim.h"
#include "cmd.h"

LED led0;




static float breathe_angle = 0.0f;
static float breathe_delta_angle = 0.0f;

void Board_Alive(void) {
    static uint32_t last_tick = 0;
    if (HAL_GetTick() - last_tick < 1000) return;
    last_tick = HAL_GetTick();
    HAL_GPIO_TogglePin(LED_R_GPIO_Port, LED_R_Pin);
    uprintf("{\"evt\":\"status\",\"uptime\":%d,\"mode\":\"%d\",\"hz\":%d,\"period_ms\":%d}\r\n", 
            HAL_GetTick(), led0.mode, (int)(led0.blink_frequency), (int)(led0.breathing_period), led0.brightness); // 每秒打印一次状态
}

void LED_Init(void) {
    // 从Flash中恢复LED状态
    Flash_Read_LED_State(&led0);

    // 默认值
    if (led0.mode > BLINKING) {
        led0.mode = OFF;
        led0.brightness = 0;
        led0.blink_frequency = 0;
        led0.breathing_period = 0;
        Flash_Write_LED_State(&led0);
    }
    // 开灯
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0);

    switch (led0.mode) {
        case ON:
            LED_ON();
            break;
        case OFF:
            LED_OFF();
            break;
        case BLINKING:
            LED_StartBlink(led0.blink_frequency);
            break;
        case BREATHING:
            LED_StartBreathing(led0.breathing_period);
            break;
        default:
            LED_OFF();
            break;
    }
}

void LED_ON(void) {
    led0.mode = ON;
    led0.brightness = 255;
    LED_SetBrightness(led0.brightness);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
}
void LED_OFF(void) {
    led0.mode = OFF;
    HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_1);
    Flash_Write_LED_State(&led0);

}

void LED_SetBrightness(uint8_t brightness) {
    if (brightness > 255) brightness = 255;
    uint8_t current_brightness;
    led0.brightness = brightness;
    
    current_brightness = -led0.brightness + 255;
    uint16_t pulse = (current_brightness * (htim3.Init.Period + 1)) / 255;
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, pulse);

    if(led0.mode == ON) {
        Flash_Write_LED_State(&led0);
    }
}

void LED_StartBlink(float freq) {
    led0.mode = BLINKING;
    led0.blink_frequency = freq;
    HAL_TIM_Base_Stop_IT(&htim4);
    //计算预分频器
    uint32_t psc = 168000000UL / 2 / 2 / 1000.0 / led0.blink_frequency - 1 ;
    __HAL_TIM_SET_PRESCALER(&htim4, psc);
    __HAL_TIM_SET_AUTORELOAD(&htim4, 999);
    HAL_TIM_Base_Start_IT(&htim4);
    HAL_TIM_Base_Start_IT(&htim3);
    Flash_Write_LED_State(&led0);

}

void LED_StopBlink(void) {
    led0.mode = ON;
    HAL_TIM_Base_Stop_IT(&htim4);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
    Flash_Write_LED_State(&led0);
}

void LED_StartBreathing(float period_ms) {
    led0.mode = BREATHING;
    led0.breathing_period = period_ms;
    HAL_TIM_Base_Stop_IT(&htim4);
    __HAL_TIM_SET_PRESCALER(&htim4, 839);
    __HAL_TIM_SET_AUTORELOAD(&htim4, 999);
    //每10ms更新一次
    breathe_delta_angle = 2 * M_PI / (led0.breathing_period / 10.0f);
    breathe_angle = 0.0f; // Reset angle
    HAL_TIM_Base_Start_IT(&htim4);
    HAL_TIM_Base_Start_IT(&htim3);
    Flash_Write_LED_State(&led0);
}

void LED_StopBreathing(void) {
    led0.mode = ON;
    HAL_TIM_Base_Stop_IT(&htim4);
    LED_SetBrightness(led0.brightness);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM4 ) {
        static uint8_t blink_state = 0;
        switch(led0.mode) {
            case BLINKING:

                blink_state = !blink_state;
                if (blink_state) {
                    HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_1);
                } else {
                    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
                }
                break;
            case BREATHING:
                breathe_angle += breathe_delta_angle;
                if (breathe_angle >= 2 * M_PI) breathe_angle -= 2 * M_PI;
                uint8_t brightness = 128 + (int)(127 * sinf( breathe_angle));
                LED_SetBrightness(brightness);
                break;
        }
    }
}




void CMD_Set_Mode(int *argc, char *argv[]){
    if (*argc < 2) {
        uprintf("Usage: set_mode <mode> [<value>]\r\n");
        return;
    }
    char *mode = argv[1];
    if (strcmp(mode, "on") == 0) {
        LED_ON();
        uprintf("Mode set to on\r\n");
    } else if (strcmp(mode, "off") == 0) {
        LED_OFF();
        uprintf("Mode set to off\r\n");
    } else if (strcmp(mode, "blink") == 0) {
        if (*argc < 3) {
            uprintf("Missing hz value\r\n");
            return;
        }
        float hz = atof(argv[2]);
        LED_StartBlink(hz);
        uprintf("Mode set to blink at %d Hz\r\n", (int) (hz * 10) / 10);
    } else if (strcmp(mode, "breathe") == 0) {
        if (*argc < 3) {
            uprintf("Missing period_ms value\r\n");
            return;
        }
        float period = atof(argv[2]);
        LED_StartBreathing(period);
        uprintf("Mode set to breath at %d ms \r\n", (int) (period * 10) / 10);
    }
}

void CMD_Set_Brightness (int *argc, char *argv[]) {
    if (*argc < 2) {
        uprintf("Usage: set_brightness <value>\r\n");
        return;
    } else {
        if(led0.mode == BREATHING) {
            LED_StopBreathing();
        }
        uint8_t brightness = atoi(argv[1]);
        LED_SetBrightness(brightness);
        uprintf("Brightness is set to %d",brightness);
        return;
    }
}


