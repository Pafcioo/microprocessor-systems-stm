/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "eth.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "usb_otg.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "bh1750.h"
#include "string.h"
#include "stdio.h"
#include "stdbool.h"
#include "cJSON.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define MAX_LOGGER 100U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
bool readLightRequest = false; // Flag to indicate a light intensity read request
bool printLightIntensity = false; // Flag to indicate whether to print light intensity

uint8_t rxByte; // Buffer for ONE byte of data from UART, so we can receive various length messages
char rxCommand[32]; // Buffer for whole message we receive
uint8_t rxLength = 0; // Length of the message from UART
bool rxDiscard = false; // In case if we write something after ";" character, so it wont be registered
bool commandReady = false; // Flag to indicate when we can use fucniton HandleCommand

uint16_t freq = 1; // Default frequency value

bool logger = false; // Flag for eneabling logging data
uint16_t loggerIndex = 0; // Starting index for light intensity array
uint16_t loggerFinish = 2; // Default logger array length
float loggerArray[MAX_LOGGER]; // Array for storing lux measurment

// Controlling led with PWM
float PWMvalue = 0.0f;
float PWMstep = 0.5f;
uint16_t PWMchannel = 0;
uint16_t compare_value;
//Static characteristics for led-sensor
bool do_characteristic = false;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void SendJSONArray(float* loggerArray)
{
  uint8_t opening = '[';
  uint8_t closing[] = "]\r\n";
  char part_of_message[10];
  HAL_UART_Transmit(&huart3,&opening,1,HAL_MAX_DELAY); // Opening brackets
  for(uint8_t i = 0; i < loggerFinish; i++)
  {
    char *separator = (i == 0) ? "" : ","; // Before out lux value we have ot either place nothing or comma
    snprintf(part_of_message, sizeof(part_of_message), "%s%.3f", separator, loggerArray[i]); // Putting together separator and value
    HAL_UART_Transmit(&huart3, (uint8_t*)part_of_message,(uint16_t)strlen(part_of_message),HAL_MAX_DELAY); //Sending vlaue iwth separator
  }
  HAL_UART_Transmit(&huart3,(uint8_t*)closing, sizeof(closing)-1, HAL_MAX_DELAY); // Closing brackets
}

bool AddCharacteristicPoint(cJSON *points, float pwmPercent, float lux)
{
  cJSON *point = cJSON_CreateObject();
  if (point == NULL)
  {
    return false;
  }

  if (cJSON_AddNumberToObject(point, "pwm_percent", pwmPercent) == NULL ||
      cJSON_AddNumberToObject(point, "illuminance_lux", lux) == NULL ||
      !cJSON_AddItemToArray(points, point))
  {
    cJSON_Delete(point);
    return false;
  }

  return true;
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if(htim->Instance == TIM3)
  {
    readLightRequest = true; // Set the flag to indicate a light intensity read request
  }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if(huart->Instance == USART3)
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
    HAL_UART_Receive_IT(&huart3, &rxByte, 1); // Continue receiving the next byte
  }
}

void HandleCommand(const char* command)
{
  if(strcmp(command, "print_on") == 0)
  {
    printLightIntensity = true;
  }
  else if(strcmp(command, "print_off") == 0)
  {
    printLightIntensity = false;
  }
  else if (sscanf(command, "freq=%hu", &freq) == 1) // Check if the command is a frequency setting command
  {
    uint16_t newARR = (uint16_t)((1.0f / (freq * 0.0001f)) - 1); // New autoreload value
    // Restart timer with new autorealod
    HAL_TIM_Base_Stop_IT(&htim3);
    __HAL_TIM_SET_COUNTER(&htim3, 0);
    __HAL_TIM_CLEAR_FLAG(&htim3, TIM_FLAG_UPDATE);
    __HAL_TIM_SET_AUTORELOAD(&htim3, newARR);
    HAL_TIM_Base_Start_IT(&htim3);
  }
  else if(sscanf(command, "logger=%hu", &loggerFinish) == 1)
  {
    logger = true; // Enable logger
  }
  else if(sscanf(command, "PWM%hu=%f", &PWMchannel, &PWMvalue) == 2)
  {
    if(PWMchannel == 1 && PWMvalue>=0.0f && PWMvalue<=100.0f)
    {
      __HAL_TIM_SET_COMPARE(&htim4,TIM_CHANNEL_1,PWMvalue*10); // Change the PWM value
      compare_value = __HAL_TIM_GET_COMPARE(&htim4,TIM_CHANNEL_1); // For debugging purposes
    }
  }
  else if(strcmp(command, "JSON_log") == 0)
  {
    do_characteristic = true;
  }
}


void LogLight(float lux)
{
  if(logger && (loggerIndex <= loggerFinish))
  {
    loggerArray[loggerIndex] = lux;
    loggerIndex++;
    if(loggerIndex == loggerFinish)
    {
      logger = false;
      loggerIndex = 0;
      SendJSONArray(loggerArray);
    }
  }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ETH_Init();
  MX_USART3_UART_Init();
  MX_USB_OTG_FS_PCD_Init();
  MX_I2C1_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  /* USER CODE BEGIN 2 */
  BH1750 sensor;
  uint8_t BH1750_ADDRESS = 0b0100011; // BH1750 I2C address (shifted for HAL)
  float lux = 0.0f;
  char message[30]; // Message buffer for UART transmission
  BH1750_Init(&sensor, &hi2c1, BH1750_ADDRESS);
  HAL_TIM_Base_Start_IT(&htim3); // Start the timer for periodic measurements
  HAL_UART_Receive_IT(&huart3, &rxByte, 1); // Start UART reception in interrupt mode
  HAL_TIM_PWM_Start(&htim4,TIM_CHANNEL_1);
  //Static characteristic
  cJSON *points = NULL;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    if(commandReady)
    {
      commandReady = false; // Reset the flag
      HandleCommand(rxCommand); // Process the received command
    }
    if (do_characteristic && points == NULL)
    {
      points = cJSON_CreateArray();

      if (points == NULL)
      {
        do_characteristic = false;
      }
      else
      {
        PWMvalue = 0.0f;
        __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, 0);
        readLightRequest = false;
      }
    }
    if(readLightRequest)
    {
      readLightRequest = false; // Reset the flag
      if(BH1750_ReadLightIntensity(&sensor, &lux) == HAL_OK)
      {
        snprintf(message,sizeof(message),"Light Intensity: %.2f lux\r\n",lux);
        LogLight(lux);
        if(printLightIntensity)
        {
          HAL_UART_Transmit(&huart3, (uint8_t*)message, strlen(message), HAL_MAX_DELAY);
        }
        if(do_characteristic && points != NULL)
        {
          AddCharacteristicPoint(points,PWMvalue,lux);
          if(PWMvalue+PWMstep>100.0f)
          {
            do_characteristic = false;
            char *json = cJSON_PrintUnformatted(points);
            PWMvalue = 0.0f;
            __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, 0);
            if(json != NULL)
            {
              HAL_UART_Transmit(&huart3,(uint8_t*)json,(uint16_t)strlen(json), HAL_MAX_DELAY);
              HAL_UART_Transmit(&huart3, (uint8_t *)"\r\n", 2, HAL_MAX_DELAY);
              cJSON_free(json);
            }
            cJSON_Delete(points);
            points = NULL;
          }
          else
          {
            PWMvalue+=PWMstep;
            __HAL_TIM_SET_COMPARE(&htim4,TIM_CHANNEL_1,PWMvalue*10); // Change the PWM value
          }
        }
      }
      else
      {
        // Handle error in reading light intensity
      }
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 96;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
