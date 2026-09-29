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
#include "adc.h"
#include "i2c.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "bmp280.h"
#include "nokia5110.h"
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum {
    STATE_INIT = 0,
    STATE_CALIBRATE_REF,
    STATE_MEASURE,
    STATE_DISPLAY,
    STATE_ERROR
} SystemState_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
#define DEBOUNCE_MS 200
uint32_t last_button_tick = 0;
BMP280_HandleTypeDef bmp280;
SystemState_t current_state = STATE_INIT;

volatile uint8_t flag_button = 0;

#define PWM_MAX_DUTY 999

float temperatura_c = 0.0f;
float pressao_pa    = 0.0f;
float altitude_m    = 0.0f;
uint16_t adc_raw     = 0;

uint32_t led_last_toggle_tick = 0;        /* usado so nos estados que ainda piscam (CALIBRATE/ERROR) */
uint8_t  led_blink_state = 0;

uint32_t last_sample_tick = 0;            /* controla o intervalo de 1s sem travar o loop */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == B1_Pin) {
    	uint32_t now = HAL_GetTick();
    	if (now - last_button_tick > DEBOUNCE_MS) {
			last_button_tick = now;
			flag_button = 1;
		}
    }
}

static uint16_t Read_Potentiometer(void)
{
    uint16_t valor = 0;
    HAL_ADC_Start(&hadc1);
    /* espera a conversão terminar (10s) */
    if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
        valor = HAL_ADC_GetValue(&hadc1);
    }
    HAL_ADC_Stop(&hadc1);
    return valor;
}


static void Update_LED_PWM(SystemState_t state)
{
    uint32_t now = HAL_GetTick();
    uint32_t blink_period;
    uint32_t duty;

    switch (state) {
        case STATE_CALIBRATE_REF:
            blink_period = 500;
            break;
        case STATE_ERROR:
            blink_period = 100;
            break;

        case STATE_MEASURE:
        case STATE_DISPLAY:
            /* Mapeia o ADC para 0-PWM_MAX_DUTY. */
            duty = ((uint32_t)adc_raw * PWM_MAX_DUTY) / 4095;
            __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, duty);
            return;

        default:
            __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, 0);
            return;
    }

    /* Logica de piscar */
    if (now - led_last_toggle_tick >= blink_period) {
        led_last_toggle_tick = now;
        led_blink_state = !led_blink_state;
        __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, led_blink_state ? PWM_MAX_DUTY : 0);
    }
}

static void Update_Display(void)
{
    char linha[17]; /* 84px de largura / ~5px por caractere = ~14 chars por linha */

    nokia5110_clear();

    nokia5110_goto_xy(0, 0);
    snprintf(linha, sizeof(linha), "Temp: %.1fC", temperatura_c);
    nokia5110_puts(linha);

    nokia5110_goto_xy(0, 1);
    snprintf(linha, sizeof(linha), "Pres: %.0fPa", pressao_pa);
    nokia5110_puts(linha);

    nokia5110_goto_xy(0, 2);
    snprintf(linha, sizeof(linha), "Alt: %.1fm", altitude_m);
    nokia5110_puts(linha);

    nokia5110_goto_xy(0, 3);
    snprintf(linha, sizeof(linha), "Pot: %u", adc_raw);
    nokia5110_puts(linha);

    nokia5110_render();
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
  MX_USART2_UART_Init();
  MX_ADC1_Init();
  MX_I2C1_Init();
  MX_SPI1_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */

  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3);

  if (BMP280_Init(&bmp280, &hi2c1) != HAL_OK) {
      current_state = STATE_ERROR;
  }

  nokia5110_init(&hspi1, 0x38);

  current_state = STATE_CALIBRATE_REF;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    adc_raw = Read_Potentiometer();
    Update_LED_PWM(current_state);

    switch (current_state)
    {
        case STATE_CALIBRATE_REF:
            if (flag_button) {
                flag_button = 0;
                float t, p;
                if (BMP280_ReadData(&bmp280, &t, &p) == HAL_OK) {
                    BMP280_SetReferencePressure(&bmp280, p);
                    current_state = STATE_MEASURE;
                } else {
                    current_state = STATE_ERROR;
                }
            }
            break;

        case STATE_MEASURE:
            if (HAL_GetTick() - last_sample_tick >= 1000) {
                last_sample_tick = HAL_GetTick();

                if (BMP280_ReadData(&bmp280, &temperatura_c, &pressao_pa) == HAL_OK) {
                    altitude_m = BMP280_CalculateAltitude(&bmp280, pressao_pa);
                    current_state = STATE_DISPLAY;
                } else {
                    current_state = STATE_ERROR;
                }
            }

            if (flag_button) {
                flag_button = 0;
                current_state = STATE_CALIBRATE_REF;
            }
            break;

        case STATE_DISPLAY:
            Update_Display();
            current_state = STATE_MEASURE;
            break;

        case STATE_ERROR:
            if (HAL_GetTick() - last_sample_tick >= 1000) {
                last_sample_tick = HAL_GetTick();
                if (BMP280_Init(&bmp280, &hi2c1) == HAL_OK) {
                    current_state = STATE_CALIBRATE_REF;
                }
            }
            break;

        default:
            current_state = STATE_CALIBRATE_REF;
            break;
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
	RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

	/** Initializes the RCC Oscillators according to the specified parameters
	* in the RCC_OscInitTypeDef structure.
	*/
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
	RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
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

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
	{
	Error_Handler();
	}
	PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2C1|RCC_PERIPHCLK_ADC12;
	PeriphClkInit.Adc12ClockSelection = RCC_ADC12PLLCLK_DIV1;
	PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_HSI;
	if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
	{
	Error_Handler();
	}
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

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
