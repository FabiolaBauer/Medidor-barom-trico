/**
 * nokia5110.c
 */

#include "nokia5110.h"
#include <string.h>

static SPI_HandleTypeDef *hspi_nokia;
static uint8_t framebuffer[NOKIA_WIDTH * NOKIA_ROWS];
static uint8_t cursor_x = 0;
static uint8_t cursor_linha = 0;

typedef struct { char c; uint8_t col[5]; } nokia_glyph_t;

static const nokia_glyph_t fonte[] = {
    {' ', {0x00,0x00,0x00,0x00,0x00}},
    {'-', {0x08,0x08,0x08,0x08,0x08}},
    {'.', {0x00,0x60,0x60,0x00,0x00}},
    {':', {0x00,0x36,0x36,0x00,0x00}},
    {'0', {0x3E,0x51,0x49,0x45,0x3E}},
    {'1', {0x00,0x42,0x7F,0x40,0x00}},
    {'2', {0x42,0x61,0x51,0x49,0x46}},
    {'3', {0x21,0x41,0x45,0x4B,0x31}},
    {'4', {0x18,0x14,0x12,0x7F,0x10}},
    {'5', {0x27,0x45,0x45,0x45,0x39}},
    {'6', {0x3C,0x4A,0x49,0x49,0x30}},
    {'7', {0x01,0x71,0x09,0x05,0x03}},
    {'8', {0x36,0x49,0x49,0x49,0x36}},
    {'9', {0x06,0x49,0x49,0x29,0x1E}},
    {'A', {0x7E,0x11,0x11,0x11,0x7E}},
    {'B', {0x7F,0x49,0x49,0x49,0x36}},
    {'C', {0x3E,0x41,0x41,0x41,0x22}},
    {'E', {0x7F,0x49,0x49,0x49,0x41}},
    {'M', {0x7F,0x02,0x0C,0x02,0x7F}},
    {'O', {0x3E,0x41,0x41,0x41,0x3E}},
    {'P', {0x7F,0x09,0x09,0x09,0x06}},
    {'R', {0x7F,0x09,0x19,0x29,0x46}},
    {'T', {0x01,0x01,0x7F,0x01,0x01}},
    {'a', {0x20,0x54,0x54,0x54,0x78}},
    {'b', {0x7F,0x48,0x44,0x44,0x38}},
    {'e', {0x38,0x54,0x54,0x54,0x18}},
    {'l', {0x00,0x41,0x7F,0x40,0x00}},
    {'m', {0x7C,0x04,0x18,0x04,0x78}},
    {'n', {0x7C,0x08,0x04,0x04,0x78}},
    {'o', {0x38,0x44,0x44,0x44,0x38}},
    {'p', {0x7C,0x14,0x14,0x14,0x08}},
    {'r', {0x7C,0x08,0x04,0x04,0x08}},
    {'s', {0x48,0x54,0x54,0x54,0x20}},
    {'t', {0x04,0x3F,0x44,0x40,0x20}},
    {'u', {0x3C,0x40,0x40,0x20,0x7C}},
};
#define FONTE_QTD (sizeof(fonte) / sizeof(fonte[0]))

static const uint8_t *nokia_busca_glifo(char c)
{
    for (uint32_t i = 0; i < FONTE_QTD; i++) {
        if (fonte[i].c == c) return fonte[i].col;
    }
    return fonte[0].col;
}

static HAL_StatusTypeDef nokia_write_cmd(uint8_t cmd)
{
    HAL_StatusTypeDef status;

    HAL_GPIO_WritePin(NOKIA_DC_GPIO_Port, NOKIA_DC_Pin, GPIO_PIN_RESET);   // DC Low = comando

    HAL_GPIO_WritePin(NOKIA_CS_GPIO_Port, NOKIA_CS_Pin, GPIO_PIN_RESET);   // CS Low
    status = HAL_SPI_Transmit(hspi_nokia, &cmd, 1, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(NOKIA_CS_GPIO_Port, NOKIA_CS_Pin, GPIO_PIN_SET);     // CS High

    return status;
}

static HAL_StatusTypeDef nokia_write_data(uint8_t dado)
{
    HAL_StatusTypeDef status;

    HAL_GPIO_WritePin(NOKIA_DC_GPIO_Port, NOKIA_DC_Pin, GPIO_PIN_SET);     // DC High = dado

    HAL_GPIO_WritePin(NOKIA_CS_GPIO_Port, NOKIA_CS_Pin, GPIO_PIN_RESET);   // CS Low
    status = HAL_SPI_Transmit(hspi_nokia, &dado, 1, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(NOKIA_CS_GPIO_Port, NOKIA_CS_Pin, GPIO_PIN_SET);     // CS High

    return status;
}

#define NOKIA_ROTATE_180 1

#if NOKIA_ROTATE_180
static uint8_t Reverse_Bits(uint8_t b)
{
    b = (uint8_t)((b & 0xF0) >> 4 | (b & 0x0F) << 4);
    b = (uint8_t)((b & 0xCC) >> 2 | (b & 0x33) << 2);
    b = (uint8_t)((b & 0xAA) >> 1 | (b & 0x55) << 1);
    return b;
}
#endif

HAL_StatusTypeDef nokia5110_init(SPI_HandleTypeDef *hspi, uint8_t contraste)
{
    hspi_nokia = hspi;
    HAL_StatusTypeDef status;

    HAL_GPIO_WritePin(NOKIA_RST_GPIO_Port, NOKIA_RST_Pin, GPIO_PIN_RESET); // pulso de reset
    HAL_Delay(10);
    HAL_GPIO_WritePin(NOKIA_RST_GPIO_Port, NOKIA_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(10);

    status  = nokia_write_cmd(NOKIA_CMD_FUNCTION_SET_EXTENDED);
    status |= nokia_write_cmd(NOKIA_CMD_SET_VOP_BASE | (contraste & 0x7F)); // ajusta o Vop (contraste)
    status |= nokia_write_cmd(NOKIA_CMD_TEMP_CONTROL);
    status |= nokia_write_cmd(NOKIA_CMD_BIAS_SYSTEM);
    status |= nokia_write_cmd(NOKIA_CMD_FUNCTION_SET_BASIC);
    status |= nokia_write_cmd(NOKIA_CMD_DISPLAY_NORMAL);

    if (status != HAL_OK) return status;

    nokia5110_clear();
    return nokia5110_render();
}

HAL_StatusTypeDef nokia5110_clear(void)
{
    memset(framebuffer, 0x00, sizeof(framebuffer));
    cursor_x = 0;
    cursor_linha = 0;
    return HAL_OK;
}

void nokia5110_goto_xy(uint8_t x, uint8_t linha)
{
    cursor_x = x;
    cursor_linha = (linha < NOKIA_ROWS) ? linha : (NOKIA_ROWS - 1);
}

HAL_StatusTypeDef nokia5110_puts(const char *str)
{
    while (*str) {
        if (cursor_x + 5 >= NOKIA_WIDTH) break;

        const uint8_t *glifo = nokia_busca_glifo(*str);
        uint16_t base = (uint16_t)cursor_linha * NOKIA_WIDTH + cursor_x;

        for (uint8_t col = 0; col < 5; col++) {
            framebuffer[base + col] = glifo[col];
        }
        cursor_x += 6;
        str++;
    }
    return HAL_OK;
}

HAL_StatusTypeDef nokia5110_render(void)
{
    HAL_StatusTypeDef status;

    status  = nokia_write_cmd(NOKIA_CMD_SET_X_ADDR | 0);
    status |= nokia_write_cmd(NOKIA_CMD_SET_Y_ADDR | 0);

#if NOKIA_ROTATE_180
    for (uint8_t banda = 0; banda < NOKIA_ROWS; banda++) {
        uint8_t banda_origem = (NOKIA_ROWS - 1) - banda;
        for (uint8_t x = 0; x < NOKIA_WIDTH; x++) {
            uint8_t x_origem = (NOKIA_WIDTH - 1) - x;
            uint8_t byte = framebuffer[(uint16_t)banda_origem * NOKIA_WIDTH + x_origem];
            status |= nokia_write_data(Reverse_Bits(byte));
        }
    }
#else
    for (uint16_t i = 0; i < sizeof(framebuffer); i++) {
        status |= nokia_write_data(framebuffer[i]);
    }
#endif

    return status;
}
