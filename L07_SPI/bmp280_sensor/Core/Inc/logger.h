#ifndef LOGGER_H
#define LOGGER_H

#include "main.h"

#include <stdbool.h>
#include <stdint.h>
#include "string.h"
#include "stdio.h"

#define MAX_LOGGER 4800U

extern uint8_t rxByte; // Buffer for ONE byte of data from UART, so we can receive various length messages
extern char rxCommand[32]; // Buffer for whole message we receive
extern uint8_t rxLength; // Length of the message from UART
extern bool rxDiscard; // In case if we write something after ";" character, so it wont be registered
extern bool commandReady; // Flag to indicate when we can use fucniton HandleCommand
extern uint16_t freq; // Default frequency value
extern bool logger; // Flag for eneabling logging data
extern uint16_t loggerIndex; // Starting index for light intensity array
extern uint16_t loggerFinish; // Default logger array length
extern float loggerArray[MAX_LOGGER]; // Array for storing lux measurment

void SendJSONArray(float* loggerArray, UART_HandleTypeDef *huart);
void HandleCommand(const char* command, bool *printing, TIM_HandleTypeDef *htim);
void LogData(float temp,float press,UART_HandleTypeDef *huart);
void PrintLastLogs(UART_HandleTypeDef *huart);
void ReceiveDataByByte(UART_HandleTypeDef *huart);

#endif /* LOGGER_H */