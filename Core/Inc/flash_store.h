#ifndef BYR_FLASH_STORE_H
#define BYR_FLASH_STORE_H

#include "led.h"

void Flash_Write_LED_State(LED* led_state);
void Flash_Read_LED_State(LED* led_state);

#endif //BYR_FLASH_STORE_H
