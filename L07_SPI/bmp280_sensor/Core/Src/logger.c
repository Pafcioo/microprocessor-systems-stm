#include "logger.h"

// UART variables
uint8_t rxByte;
char rxCommand[32];
uint8_t rxLength = 0;
bool rxDiscard = false;
bool commandReady = false;
uint16_t freq = 1;

// Simples logger variables
bool logger = false;
uint16_t loggerIndex = 0;
uint16_t loggerFinish = 2;

// Last logs variables
bool printLastLogs = false;
uint16_t lastIndex = 2;
uint16_t numberOfLogsToPrint = 0;
static uint16_t logsAvailable = 0;

// Live logs variables
bool log_live = false;
uint16_t log_live_samples = 0;
uint16_t log_live_index = 0;

// Arrays for storing data for logger
float loggerArrayTemp[MAX_LOGGER];
float loggerArrayPress[MAX_LOGGER];

// Characters for creating a JSON array
uint8_t opening = '[';
uint8_t closing = ']';
uint8_t comma = ',';
uint8_t closingWholeJSON[] = "]\r\n";

// Function that sends obly a part of values array, like temp or pressure
// Needed for sending part of last logs
void SendJSONArrayCount(const float *values, uint16_t count, UART_HandleTypeDef *huart)
{
  char part_of_message[32];
  HAL_UART_Transmit(huart,&opening,1,HAL_MAX_DELAY); // Opening brackets
  for(uint16_t i = 0; i < count; i++)
  {
    char *separator = (i == 0) ? "" : ","; // Before out lux value we have ot either place nothing or comma
    float value = (values == NULL) ? 0.0f : values[i];
    snprintf(part_of_message, sizeof(part_of_message), "%s%.3f", separator, value); // Putting together separator and value
    HAL_UART_Transmit(huart, (uint8_t *)part_of_message,(uint16_t)strlen(part_of_message),HAL_MAX_DELAY); //Sending vlaue iwth separator
  }
  HAL_UART_Transmit(huart, &closing, 1, HAL_MAX_DELAY); // Closing brackets
}

// This function uses the SendJSONArrayCOunt function, but sends the whole array
// Needed for simple logger
void SendJSONArray(float *loggerArray, UART_HandleTypeDef *huart)
{
  SendJSONArrayCount(loggerArray, loggerFinish, huart);
}

// Function that receive byte by byte via UART
void ReceiveDataByByte(UART_HandleTypeDef * huart)
{
    char receivedChar = (char)rxByte; // Convert the received byte to a character
    if(receivedChar == ';') // Check for command termination
    {
      rxCommand[rxLength] = '\0'; // Add \0 so it will be compatible with fucntions that expect C string
      commandReady = true; // Set the flag to indicate a complete command is ready
      rxLength = 0; // Reset the length for the next command
      rxDiscard = false; // Reset discard flag
    }
    else if(receivedChar == '\r' || receivedChar == '\n') // Ignore carriage return and newline characters
    {

    }
    else if(rxLength < sizeof(rxCommand) - 1) // Check if there's space in the buffer
    {
      if(!rxDiscard)
      {
        rxCommand[rxLength] = receivedChar; // Store the character
        rxLength++; // Increment index of array iwth our command
      }
    }
    else
    {
      rxDiscard = true; // If buffer is full, discard next bytes
    }
    HAL_UART_Receive_IT(huart, &rxByte, 1); // Continue receiving the next byte
}

void HandleCommand(const char* command, bool *printing, TIM_HandleTypeDef *htim)
{
    uint16_t requestedLogCount;
    uint16_t requestedPrintCount;

    if(strcmp(command, "print_on") == 0)
    {
        *printing = true;
    }
    else if(strcmp(command, "print_off") == 0)
    {
        *printing = false;
    }
    else if (sscanf(command, "freq=%hu", &freq) == 1) // Check if the command is a frequency setting command
    {
        uint16_t newARR = (uint16_t)((1.0f / (freq * 0.0001f)) - 1); // New autoreload value
        // Restart timer with new autorealod
        HAL_TIM_Base_Stop_IT(htim);
        __HAL_TIM_SET_COUNTER(htim, 0);
        __HAL_TIM_CLEAR_FLAG(htim, TIM_FLAG_UPDATE);
        __HAL_TIM_SET_AUTORELOAD(htim, newARR);
        HAL_TIM_Base_Start_IT(htim);
    }
    else if(sscanf(command, "logger=%hu", &requestedLogCount) == 1 &&
            requestedLogCount > 0 && requestedLogCount <= MAX_LOGGER)
    {
        loggerFinish = requestedLogCount;
        loggerIndex = 0;
        logsAvailable = 0;
        logger = true; // Enable logger
    }
    else if(sscanf(command, "logger_last=%hu", &requestedPrintCount) == 1 &&
            requestedPrintCount > 0 && requestedPrintCount <= MAX_LOGGER)
    {
        numberOfLogsToPrint = requestedPrintCount;
        printLastLogs = true;
    }
    else if(sscanf(rxCommand, "log_live=%hu", &log_live_samples) == 1)
    {
      log_live = true;
    }
    else if(strcmp(command, "log_live:stop") == 0)
    {
      log_live = false;
      log_live_index = 0;
      log_live_samples = 0;
    }
}

// Function for logging data, it stores temp and press in arrays that were declared at the start
void LogData(float temp,float press,UART_HandleTypeDef *huart)
{
  if(logger && loggerIndex < loggerFinish && loggerFinish <= MAX_LOGGER)
  {
    loggerArrayTemp[loggerIndex] = temp;
    loggerArrayPress[loggerIndex] = press;
    loggerIndex++;
    if(loggerIndex == loggerFinish) // When we stored the numerb of samples we wanted we disable logger reset indexes and send it via UART
    {
      logger = false;
      loggerIndex = 0;
      logsAvailable = loggerFinish;
      HAL_UART_Transmit(huart,&opening,1,HAL_MAX_DELAY); // Opening brackets
      SendJSONArray(loggerArrayTemp, huart);
      HAL_UART_Transmit(huart,&comma,1,HAL_MAX_DELAY); // comma
      SendJSONArray(loggerArrayPress, huart);
      HAL_UART_Transmit(huart,closingWholeJSON,3,HAL_MAX_DELAY); // Closing brackets and return and newline chars
    }
  }
}

// Function that sends only a part of recent logs
void PrintLastLogs(UART_HandleTypeDef *huart)
{
    if(!printLastLogs)
    {
        return;
    }

    printLastLogs = false;
    if(logsAvailable == 0 || numberOfLogsToPrint == 0) //Condition where either we dont hav eany logged data or we want to pirnt 0 samples
    {
        if(numberOfLogsToPrint == 0)
        {
            return;
        }
        // Sending zeros
        HAL_UART_Transmit(huart, &opening, 1, HAL_MAX_DELAY);
        SendJSONArrayCount(NULL, numberOfLogsToPrint, huart);
        HAL_UART_Transmit(huart, &comma, 1, HAL_MAX_DELAY);
        SendJSONArrayCount(NULL, numberOfLogsToPrint, huart);
        HAL_UART_Transmit(huart, closingWholeJSON, sizeof(closingWholeJSON) - 1,
                        HAL_MAX_DELAY);
        return;
    }

    uint16_t count;
    // If statement to prevent from sending zeros if we want to send more recent logs that we even have logged
    if(numberOfLogsToPrint < logsAvailable)
    {
        count = numberOfLogsToPrint;
    }
    else
    {
        count = logsAvailable;
    }
    uint16_t start = logsAvailable - count;

    HAL_UART_Transmit(huart, &opening, 1, HAL_MAX_DELAY);
    SendJSONArrayCount(&loggerArrayTemp[start], count, huart);
    HAL_UART_Transmit(huart, &comma, 1, HAL_MAX_DELAY);
    SendJSONArrayCount(&loggerArrayPress[start], count, huart);
    HAL_UART_Transmit(huart, closingWholeJSON, sizeof(closingWholeJSON) - 1,
                      HAL_MAX_DELAY);
}

//Function for live logging
void LogSamples(float temp,float press,UART_HandleTypeDef *huart)
{
  if(log_live)
  {
    if(log_live_index>=log_live_samples)
    {
      log_live = false;
      log_live_index = 0;
    }
    else
    {
      char part_of_message[32];
      char *separator = ",";
      snprintf(part_of_message, sizeof(part_of_message), "[%.3f%s%.3f]\r\n", temp, separator, press); // A single array that later is pared in python script
      HAL_UART_Transmit(huart, (uint8_t *)part_of_message,(uint16_t)strlen(part_of_message),HAL_MAX_DELAY);
      log_live_index++;
    }
    
  }
}
