//
// Created by 10616 on 2025/9/21.
//

#include <stdio.h>
#include "led.h"
#include "gpio.h"
void LED_Board_Alive(void) {
    HAL_GPIO_TogglePin(LED_R_GPIO_Port, LED_R_Pin);
    HAL_Delay(500);
}


void LED_Init(void) {

}