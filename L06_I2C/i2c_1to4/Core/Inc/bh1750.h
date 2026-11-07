#ifndef BH1750_H
#define BH1750_H

#include "stm32f7xx_hal.h"

typedef struct {
    I2C_HandleTypeDef *hi2c; // Pointer to the I2C handle
    uint8_t address;         // I2C address of the BH1750 sensor
} BH1750;

HAL_StatusTypeDef BH1750_Init(BH1750 *sensor, I2C_HandleTypeDef *hi2c, uint8_t address);
HAL_StatusTypeDef BH1750_ReadLightIntensity(BH1750 *sensor, float *intensity);

#endif // BH1750_H