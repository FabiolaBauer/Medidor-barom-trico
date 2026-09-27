/**
 * bmp280.h
 */

#ifndef BMP280_H
#define BMP280_H

#include "stm32f3xx_hal.h"
#include <stdint.h>

#define BMP280_I2C_ADDR (0x76 << 1)

/* Registradores (datasheet secao 4.3) */
#define BMP280_REG_ID 0xD0
#define BMP280_REG_RESET 0xE0
#define BMP280_REG_STATUS 0xF3
#define BMP280_REG_CTRL_MEAS 0xF4
#define BMP280_REG_CONFIG 0xF5
#define BMP280_REG_PRESS_MSB 0xF7
#define BMP280_REG_CALIB_START 0x88

#define BMP280_CHIP_ID 0x58
#define BMP280_SOFT_RESET_CMD 0xB6

typedef struct {
    I2C_HandleTypeDef *hi2c;

    /* Coeficientes de calibracao lidos da NVM do sensor (datasheet 3.11.2) */
    uint16_t dig_T1;
    int16_t  dig_T2;
    int16_t  dig_T3;
    uint16_t dig_P1;
    int16_t  dig_P2;
    int16_t  dig_P3;
    int16_t  dig_P4;
    int16_t  dig_P5;
    int16_t  dig_P6;
    int16_t  dig_P7;
    int16_t  dig_P8;
    int16_t  dig_P9;

    int32_t  t_fine;

    float    reference_pressure_pa;
} BMP280_HandleTypeDef;


HAL_StatusTypeDef BMP280_Init(BMP280_HandleTypeDef *dev, I2C_HandleTypeDef *hi2c);

HAL_StatusTypeDef BMP280_ReadData(BMP280_HandleTypeDef *dev, float *temperature_c, float *pressure_pa);

void BMP280_SetReferencePressure(BMP280_HandleTypeDef *dev, float pressure_pa);

float BMP280_CalculateAltitude(BMP280_HandleTypeDef *dev, float pressure_pa);

#endif
