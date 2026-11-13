#include "spi.h"

// variables accessible from all files that include bmp280.h
extern uint32_t raw_temperature;
extern uint32_t raw_pressure;
extern double temperature_degC;
extern double pressure_Pa;

// structure holding compensation parameters of bmp280
typedef struct {
    uint16_t dig_T1;
    int16_t dig_T2;
    int16_t dig_T3;
    uint16_t dig_P1;
    int16_t dig_P2;
    int16_t dig_P3;
    int16_t dig_P4;
    int16_t dig_P5;
    int16_t dig_P6;
    int16_t dig_P7;
    int16_t dig_P8;
    int16_t dig_P9;

} bmp280_compensation_params;

// function that starts and configures sensor
uint8_t bmp280_init(SPI_HandleTypeDef *hspi);

// function that reads raw temperature and pressure data from sensor
void bmp280_read_raw_data(SPI_HandleTypeDef *hspi);

// function that reads compensation parameters from sensor. Every sensor has unique parameters saved in it's registers
void bmp280_read_compensation_params(SPI_HandleTypeDef *hspi, bmp280_compensation_params *CP);

// function that converts raw sensor data into SI units
void bmp280_compensate_data(bmp280_compensation_params *CP);
