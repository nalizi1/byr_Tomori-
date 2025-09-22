#include "flash_store.h"
#include "stm32f4xx_hal.h"
#include <string.h>

#define FLASH_STORAGE_ADDRESS 0x080E0000

void Flash_Write_LED_State(LED* led_state) {
    HAL_FLASH_Unlock();

    // Erase the sector
    FLASH_EraseInitTypeDef EraseInitStruct;
    EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    EraseInitStruct.Sector = FLASH_SECTOR_11;
    EraseInitStruct.NbSectors = 1;
    uint32_t SectorError = 0;
    if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK) {
        // Error occurred while sector erase.
        HAL_FLASH_Lock();
        return;
    }

    // Program the flash
    uint32_t* data = (uint32_t*)led_state;
    uint32_t address = FLASH_STORAGE_ADDRESS;
    for (size_t i = 0; i < sizeof(LED) / sizeof(uint32_t); i++) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, data[i]) != HAL_OK) {
            // Error occurred while programming
            break;
        }
        address += 4;
    }
     if (sizeof(LED) % sizeof(uint32_t) != 0) {
        uint32_t remaining_bytes_word = 0;
        uint8_t* remaining_bytes_ptr = (uint8_t*)data + (sizeof(LED) / sizeof(uint32_t)) * sizeof(uint32_t);
        memcpy(&remaining_bytes_word, remaining_bytes_ptr, sizeof(LED) % sizeof(uint32_t));
         if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, remaining_bytes_word) != HAL_OK) {
            // Error occurred while programming
        }
    }


    HAL_FLASH_Lock();
}

void Flash_Read_LED_State(LED* led_state) {
    uint32_t address = FLASH_STORAGE_ADDRESS;
    memcpy(led_state, (void*)address, sizeof(LED));
}
