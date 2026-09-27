/**
 * bmp280.c
 *
 * Implementacao da biblioteca do BMP280.
 */

#include "bmp280.h"
#include <math.h>


static HAL_StatusTypeDef BMP280_ReadRegs(BMP280_HandleTypeDef *dev, uint8_t reg, uint8_t *buf, uint16_t len)
{
    return HAL_I2C_Mem_Read(dev->hi2c, BMP280_I2C_ADDR, reg, I2C_MEMADD_SIZE_8BIT, buf, len, HAL_MAX_DELAY);
}

static HAL_StatusTypeDef BMP280_WriteReg(BMP280_HandleTypeDef *dev, uint8_t reg, uint8_t value)
{
    return HAL_I2C_Mem_Write(dev->hi2c, BMP280_I2C_ADDR, reg, I2C_MEMADD_SIZE_8BIT, &value, 1, HAL_MAX_DELAY);
}

/* Le os 24 bytes de calibracao (0x88-0xA1) e monta os coeficientes dig_* */
static HAL_StatusTypeDef BMP280_ReadCalibration(BMP280_HandleTypeDef *dev)
{
    uint8_t c[24];
    HAL_StatusTypeDef st = BMP280_ReadRegs(dev, BMP280_REG_CALIB_START, c, 24);
    if (st != HAL_OK) return st;

    dev->dig_T1 = (uint16_t)(c[1]  << 8 | c[0]);
    dev->dig_T2 = (int16_t)(c[3]  << 8 | c[2]);
    dev->dig_T3 = (int16_t)(c[5]  << 8 | c[4]);
    dev->dig_P1 = (uint16_t)(c[7]  << 8 | c[6]);
    dev->dig_P2 = (int16_t)(c[9]  << 8 | c[8]);
    dev->dig_P3 = (int16_t)(c[11] << 8 | c[10]);
    dev->dig_P4 = (int16_t)(c[13] << 8 | c[12]);
    dev->dig_P5 = (int16_t)(c[15] << 8 | c[14]);
    dev->dig_P6 = (int16_t)(c[17] << 8 | c[16]);
    dev->dig_P7 = (int16_t)(c[19] << 8 | c[18]);
    dev->dig_P8 = (int16_t)(c[21] << 8 | c[20]);
    dev->dig_P9 = (int16_t)(c[23] << 8 | c[22]);

    return HAL_OK;
}

/* Formula oficial de compensacao de temperatura (datasheet 3.11.3)
 * Retorna temperatura em centesimos de grau Celsius e grava t_fine (reusado na pressao) */
static int32_t BMP280_CompensateTemperature(BMP280_HandleTypeDef *dev, int32_t adc_T)
{
    int32_t var1, var2, T;

    var1 = ((((adc_T >> 3) - ((int32_t)dev->dig_T1 << 1))) * ((int32_t)dev->dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t)dev->dig_T1)) *
              ((adc_T >> 4) - ((int32_t)dev->dig_T1))) >> 12) *
            ((int32_t)dev->dig_T3)) >> 14;

    dev->t_fine = var1 + var2;
    T = (dev->t_fine * 5 + 128) >> 8;
    return T;
}

/* Formula oficial de compensacao de pressao (datasheet 3.11.3)
 * Requer que BMP280_CompensateTemperature ja tenha sido chamada antes (usa t_fine).
 * Retorna pressao em Pa, formato Q24.8 (>>8 da o valor em Pa inteiro) */
static uint32_t BMP280_CompensatePressure(BMP280_HandleTypeDef *dev, int32_t adc_P)
{
    int64_t var1, var2, p;

    var1 = ((int64_t)dev->t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)dev->dig_P6;
    var2 = var2 + ((var1 * (int64_t)dev->dig_P5) << 17);
    var2 = var2 + (((int64_t)dev->dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)dev->dig_P3) >> 8) + ((var1 * (int64_t)dev->dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)dev->dig_P1) >> 33;

    if (var1 == 0) {
        return 0;
    }

    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)dev->dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)dev->dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)dev->dig_P7) << 4);

    return (uint32_t)p; /* Q24.8: divida por 256.0 para obter Pa */
}

/* ---------------- API publica ---------------- */

HAL_StatusTypeDef BMP280_Init(BMP280_HandleTypeDef *dev, I2C_HandleTypeDef *hi2c)
{
    dev->hi2c = hi2c;
    dev->reference_pressure_pa = 101325.0f; /* pressao ao nivel do mar */

    uint8_t id = 0;
    if (BMP280_ReadRegs(dev, BMP280_REG_ID, &id, 1) != HAL_OK) {
        return HAL_ERROR;
    }
    if (id != BMP280_CHIP_ID) {
        return HAL_ERROR;
    }

    if (BMP280_WriteReg(dev, BMP280_REG_RESET, BMP280_SOFT_RESET_CMD) != HAL_OK) {
        return HAL_ERROR;
    }
    HAL_Delay(5); /* datasheet recomenda aguardar apos reset */

    if (BMP280_ReadCalibration(dev) != HAL_OK) {
        return HAL_ERROR;
    }

    /* ctrl_meas: oversampling temp x2 (010), oversampling pressao x16 (101), modo normal (11)
     * 010 101 11 = 0x57 -> boa precisao de pressao (importante p/ altitude), custo de CPU baixo */
    if (BMP280_WriteReg(dev, BMP280_REG_CTRL_MEAS, 0x57) != HAL_OK) {
        return HAL_ERROR;
    }

    if (BMP280_WriteReg(dev, BMP280_REG_CONFIG, 0x08) != HAL_OK) {
        return HAL_ERROR;
    }

    return HAL_OK;
}

HAL_StatusTypeDef BMP280_ReadData(BMP280_HandleTypeDef *dev, float *temperature_c, float *pressure_pa)
{
    uint8_t raw[6];
    HAL_StatusTypeDef st = BMP280_ReadRegs(dev, BMP280_REG_PRESS_MSB, raw, 6);
    if (st != HAL_OK) return st;

    int32_t adc_P = ((int32_t)raw[0] << 12) | ((int32_t)raw[1] << 4) | (raw[2] >> 4);
    int32_t adc_T = ((int32_t)raw[3] << 12) | ((int32_t)raw[4] << 4) | (raw[5] >> 4);

    int32_t  T_comp = BMP280_CompensateTemperature(dev, adc_T);
    uint32_t P_comp = BMP280_CompensatePressure(dev, adc_P);

    *temperature_c = T_comp / 100.0f;
    *pressure_pa   = P_comp / 256.0f;

    return HAL_OK;
}

void BMP280_SetReferencePressure(BMP280_HandleTypeDef *dev, float pressure_pa)
{
    dev->reference_pressure_pa = pressure_pa;
}

float BMP280_CalculateAltitude(BMP280_HandleTypeDef *dev, float pressure_pa)
{
    /* Formula barometrica internacional padrao */
    return 44330.0f * (1.0f - powf(pressure_pa / dev->reference_pressure_pa, 0.1903f));
}
