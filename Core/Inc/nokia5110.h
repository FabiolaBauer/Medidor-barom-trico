/**
 * nokia5110.h
 *
 * Driver proprio para o display Nokia 5110 (controlador PCD8544), via SPI.
 */

#ifndef NOKIA5110_H
#define NOKIA5110_H

#include "main.h"
#include <stdint.h>

/* ---- Comandos do PCD8544 (datasheet, secao "Command Set") ---- */
#define NOKIA_CMD_FUNCTION_SET_BASIC      0x20
#define NOKIA_CMD_FUNCTION_SET_EXTENDED   0x21
#define NOKIA_CMD_DISPLAY_NORMAL          0x0C
#define NOKIA_CMD_TEMP_CONTROL            0x04
#define NOKIA_CMD_BIAS_SYSTEM             0x14
#define NOKIA_CMD_SET_VOP_BASE            0x80
#define NOKIA_CMD_SET_X_ADDR              0x80
#define NOKIA_CMD_SET_Y_ADDR              0x40

#define NOKIA_WIDTH   84
#define NOKIA_HEIGHT  48
#define NOKIA_ROWS (NOKIA_HEIGHT / 8)

/* ---- API publica ---- */
HAL_StatusTypeDef nokia5110_init(SPI_HandleTypeDef *hspi, uint8_t contraste);
HAL_StatusTypeDef nokia5110_clear(void);
void              nokia5110_goto_xy(uint8_t x, uint8_t linha);
HAL_StatusTypeDef nokia5110_puts(const char *str);
HAL_StatusTypeDef nokia5110_render(void);

#endif /* NOKIA5110_H */
