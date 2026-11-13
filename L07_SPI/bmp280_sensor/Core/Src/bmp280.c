#include "bmp280.h"

uint32_t raw_temperature = 0;
uint32_t raw_pressure = 0;
double temperature_degC = 0;
double pressure_Pa = 0;

uint8_t bmp280_init(SPI_HandleTypeDef *hspi){
	// this function initializes BMP280 to measure temperature and pressure in 125ms intervals.
	// it assumes that PE4 pin is used as slave select pin!
	// firstly, slave select pin is set to low state.
	// secondly, a byte is transferred selecting sensor's register.
	// thirdly, a byte is transferred which value will be written to (or read from) previously selected register.
	// lastly, slave select pin is set to high state.
	// look at register map in sensor's documentation for meaning behind transmitted values.
	// IMPORTANT: when SPI is used, MSB of address determines read (1) or write (0) operation.


	// read device's ID register
	uint8_t register_addr_id = 0xD0 | 0b10000000; // changing MSB to 1 using OR operation
	uint8_t register_data_id = 0;
	HAL_GPIO_WritePin(GPIOE, GPIO_PIN_4, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi, &register_addr_id, 1, 100);
	HAL_SPI_Receive(hspi, &register_data_id, 1, 100); // received value should be 0x58 (88 in decimal)
	HAL_GPIO_WritePin(GPIOE, GPIO_PIN_4, GPIO_PIN_SET);

	// soft reset device
	uint8_t register_addr_reset = 0xE0 & 0b01111111; // changing MSB to 0 using AND operation
	uint8_t register_data_reset = 0xB6;
	HAL_GPIO_WritePin(GPIOE, GPIO_PIN_4, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi, &register_addr_reset, 1, 100);
	HAL_SPI_Transmit(hspi, &register_data_reset, 1, 100);
	HAL_GPIO_WritePin(GPIOE, GPIO_PIN_4, GPIO_PIN_SET);
	HAL_Delay(50);

	// configure standby time to 125ms
	uint8_t register_addr_conf = 0xF5 & 0b01111111;
	uint8_t register_data_conf = 0b01000000;
	HAL_GPIO_WritePin(GPIOE, GPIO_PIN_4, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi, &register_addr_conf, 1, 100);
	HAL_SPI_Transmit(hspi, &register_data_conf, 1, 100);
	HAL_GPIO_WritePin(GPIOE, GPIO_PIN_4, GPIO_PIN_SET);

	// set device to normal mode, temperature and pressure over sampling to 1
	// ---------------------------
	// TODO by student:
	// configure bmp280 by writing certain data to certain register
	uint8_t register_addr_meas = 0xF4 & 0b01111111;
	uint8_t register_data_meas = 0b00100111;
	HAL_GPIO_WritePin(GPIOE, GPIO_PIN_4, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi, &register_addr_meas, 1, 100);
	HAL_SPI_Transmit(hspi, &register_data_meas, 1, 100);
	HAL_GPIO_WritePin(GPIOE, GPIO_PIN_4, GPIO_PIN_SET);

	// ---------------------------

	return register_data_id;
}

void bmp280_read_raw_data(SPI_HandleTypeDef *hspi){
	// this function burst reads raw pressure and temperature data.
	// bytes are then merged forming raw values that later will be converted into SI units.
	// it assumes that PE4 pin is used as slave select pin!

	uint8_t burst_read_data[6];
	// ----------------------------
	// TODO by student:
	// burst read 6 bytes of pressure and temperature data
	uint8_t register_addr_press = 0xF7 | 0b10000000;
	HAL_GPIO_WritePin(GPIOE,GPIO_PIN_4, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi, &register_addr_press,1,100);
	HAL_SPI_Receive(hspi, burst_read_data, 6, 100);
	HAL_GPIO_WritePin(GPIOE, GPIO_PIN_4,GPIO_PIN_SET);
	// ----------------------------

	raw_pressure =
				((uint32_t)burst_read_data[0] << 12) |
				((uint32_t)burst_read_data[1] << 4) |
				((uint32_t)burst_read_data[2] >> 4);

	raw_temperature =
			((uint32_t)burst_read_data[3] << 12) |
			((uint32_t)burst_read_data[4] << 4) |
			((uint32_t)burst_read_data[5] >> 4);

}

void bmp280_read_compensation_params(SPI_HandleTypeDef *hspi, bmp280_compensation_params *CP){
	uint8_t BR_CP_addr = 0x88 | 0b10000000;
	uint8_t BR_CP_data[24];

	HAL_GPIO_WritePin(GPIOE, GPIO_PIN_4, GPIO_PIN_RESET);
	HAL_SPI_Transmit(hspi, &BR_CP_addr, 1, 100);
	HAL_SPI_Receive(hspi, BR_CP_data, sizeof(BR_CP_data), 100);
	HAL_GPIO_WritePin(GPIOE, GPIO_PIN_4, GPIO_PIN_SET);

	CP->dig_T1 = ((uint16_t)BR_CP_data[0] | (uint16_t)BR_CP_data[1]<<8);
	CP->dig_T2 = ((int16_t)BR_CP_data[2] | (int16_t)BR_CP_data[3]<<8);
	CP->dig_T3 = ((int16_t)BR_CP_data[4] | (int16_t)BR_CP_data[5]<<8);
	CP->dig_P1 = ((uint16_t)BR_CP_data[6] | (uint16_t)BR_CP_data[7]<<8);
	CP->dig_P2 = ((int16_t)BR_CP_data[8] | (int16_t)BR_CP_data[9]<<8);
	CP->dig_P3 = ((int16_t)BR_CP_data[10] | (int16_t)BR_CP_data[11]<<8);
	CP->dig_P4 = ((int16_t)BR_CP_data[12] | (int16_t)BR_CP_data[13]<<8);
	CP->dig_P5 = ((int16_t)BR_CP_data[14] | (int16_t)BR_CP_data[15]<<8);
	CP->dig_P6 = ((int16_t)BR_CP_data[16] | (int16_t)BR_CP_data[17]<<8);
	CP->dig_P7 = ((int16_t)BR_CP_data[18] | (int16_t)BR_CP_data[19]<<8);
	CP->dig_P8 = ((int16_t)BR_CP_data[20] | (int16_t)BR_CP_data[21]<<8);
	CP->dig_P9 = ((int16_t)BR_CP_data[22] | (int16_t)BR_CP_data[23]<<8);
}

void bmp280_compensate_data(bmp280_compensation_params *CP){
	// compensation formula in floating point, copied from documentation.
	double var1, var2, var1p, var2p, p;
	var1 = (((double)raw_temperature)/16384.0 - ((double)CP->dig_T1)/1024.0) * ((double)CP->dig_T2);
	var2 = ((((double)raw_temperature)/131072.0 - ((double)CP->dig_T1)/8192.0) *
			(((double)raw_temperature)/131072.0 - ((double) CP->dig_T1)/8192.0)) * ((double)CP->dig_T3);
	temperature_degC = (var1 + var2) / 5120.0;

	var1p = ((double)(var1+var2)/2.0) - 64000.0;
	var2p = var1p * var1p * ((double)CP->dig_P6) / 32768.0;
	var2p = var2p + var1p * ((double)CP->dig_P5) * 2.0;
	var2p = (var2p/4.0)+(((double)CP->dig_P4) * 65536.0);
	var1p = (((double)CP->dig_P3) * var1p * var1p / 524288.0 + ((double)CP->dig_P2) * var1p) / 524288.0;
	var1p = (1.0 + var1p / 32768.0)*((double)CP->dig_P1);
	if (var1 == 0.0){
		pressure_Pa = 0; // avoid exception caused by division by zero
	}
	p = 1048576.0 - (double)raw_pressure;
	p = (p - (var2p / 4096.0)) * 6250.0 / var1p;
	var1p = ((double)CP->dig_P9) * p * p / 2147483648.0;
	var2p = p * ((double)CP->dig_P8) / 32768.0;
	p = p + (var1p + var2p + ((double)CP->dig_P7)) / 16.0;
	pressure_Pa = p;

}
