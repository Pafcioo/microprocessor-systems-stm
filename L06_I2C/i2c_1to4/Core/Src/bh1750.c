#include "bh1750.h"
#include "stdint.h"

HAL_StatusTypeDef BH1750_Init(BH1750 *sensor, I2C_HandleTypeDef *hi2c, uint8_t address)
{
    sensor->hi2c = hi2c;
    sensor->address = (uint16_t)(address << 1); // Shift address for HAL (7-bit address)
    uint8_t powerOn = 0b00000001; // Power on command
    uint8_t H_resolution = 0b00010000; // Continuous H-Resolution Mode
    HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(sensor->hi2c, sensor->address, &powerOn, 1, HAL_MAX_DELAY);
    if (status != HAL_OK) {
        return status;
    }

    status = HAL_I2C_Master_Transmit(sensor->hi2c, sensor->address, &H_resolution, 1, HAL_MAX_DELAY);
    if (status != HAL_OK) {
        return status;
    }
    HAL_Delay(180); // Wait for measurement to be taken (max. 180ms)
    return HAL_OK;
}

HAL_StatusTypeDef BH1750_ReadLightIntensity(BH1750 *sensor, float *lux)
{
    uint8_t data[2];
    HAL_StatusTypeDef status = HAL_I2C_Master_Receive(sensor->hi2c, sensor->address, data, 2, HAL_MAX_DELAY);
    if (status != HAL_OK) {
        return status;
    }
    // Convert the received data to light intensity in lux
    uint16_t rawValue = (data[0] << 8) | data[1];
    *lux = (float)rawValue / 1.2; // Convert to lux

    return HAL_OK;
}
