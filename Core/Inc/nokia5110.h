/**
 * nokia5110.h
 *
 * Driver proprio para o display Nokia 5110 (controlador PCD8544), via SPI.
 * Escrito no mesmo estilo do exemplo do ADXL345 visto em aula (Aula 5 -
 * Comunicacao SPI): defines para os comandos/registradores, funcoes
 * static que fazem o toggle manual do CS e chamam a HAL diretamente.
 *
 * O PCD8544 e' somente-escrita (nao ha leitura via SPI), entao usamos
 * HAL_SPI_Transmit em vez de HAL_SPI_TransmitReceive.
 */

#ifndef NOKIA5110_H
#define NOKIA5110_H

#include "main.h"   /* pega os defines de pino gerados pelo CubeMX (NOKIA_*_Pin/_GPIO_Port) */
#include <stdint.h>

/* ---- Comandos do PCD8544 (datasheet, secao "Command Set") ---- */
#define NOKIA_CMD_FUNCTION_SET_BASIC      0x20  /* PD=0, V=0, H=0 */
#define NOKIA_CMD_FUNCTION_SET_EXTENDED   0x21  /* PD=0, V=0, H=1 */
#define NOKIA_CMD_DISPLAY_NORMAL          0x0C
#define NOKIA_CMD_TEMP_CONTROL            0x04
#define NOKIA_CMD_BIAS_SYSTEM             0x14  /* 1:48 */
#define NOKIA_CMD_SET_VOP_BASE            0x80  /* soma o valor de contraste (0-127) */
#define NOKIA_CMD_SET_X_ADDR              0x80  /* modo basico: endereco de coluna (0-83) */
#define NOKIA_CMD_SET_Y_ADDR              0x40  /* modo basico: endereco de linha (0-5)   */

#define NOKIA_WIDTH   84
#define NOKIA_HEIGHT  48
#define NOKIA_ROWS    (NOKIA_HEIGHT / 8)  /* 6 linhas de 8 pixels */

/* ---- API publica ---- */
HAL_StatusTypeDef nokia5110_init(SPI_HandleTypeDef *hspi, uint8_t contraste);
HAL_StatusTypeDef nokia5110_clear(void);
void              nokia5110_goto_xy(uint8_t x, uint8_t linha);
HAL_StatusTypeDef nokia5110_puts(const char *str);
HAL_StatusTypeDef nokia5110_render(void);

#endif /* NOKIA5110_H */
