/*
 * rs485.c
 *
 *  Created on: May 29, 2026
 *      Author: tranquocvu2
 */


#include "rs485.h"
#include <string.h>

static UART_HandleTypeDef *rs485_huart = NULL;

static uint8_t rx_byte;
static char rx_line[RS485_RX_BUFFER_SIZE];
static volatile uint16_t rx_index = 0;
static volatile uint8_t rx_ready = 0;

void RS485_SetTxMode(void)
{
#ifdef RS485_DIR_Pin
    HAL_GPIO_WritePin(RS485_DIR_GPIO_Port, RS485_DIR_Pin, GPIO_PIN_SET);
#endif
}

void RS485_SetRxMode(void)
{
#ifdef RS485_DIR_Pin
    HAL_GPIO_WritePin(RS485_DIR_GPIO_Port, RS485_DIR_Pin, GPIO_PIN_RESET);
#endif
}

void RS485_Init(UART_HandleTypeDef *huart)
{
    rs485_huart = huart;
    RS485_SetRxMode();
    HAL_UART_Receive_IT(rs485_huart, &rx_byte, 1);
}

void RS485_Send(uint8_t *data, uint16_t size)
{
    if (rs485_huart == NULL || data == NULL || size == 0)
    {
        return;
    }

    RS485_SetTxMode();

    if (HAL_UART_Transmit(rs485_huart, data, size, 1000) != HAL_OK)
    {
        RS485_SetRxMode();
        HAL_UART_Receive_IT(rs485_huart, &rx_byte, 1);
        return;
    }

    uint32_t start_tick = HAL_GetTick();

    while (__HAL_UART_GET_FLAG(rs485_huart, UART_FLAG_TC) == RESET)
    {
        if ((HAL_GetTick() - start_tick) > 20)
        {
            break;
        }
    }

    RS485_SetRxMode();
    HAL_UART_Receive_IT(rs485_huart, &rx_byte, 1);
}

void RS485_SendString(const char *str)
{
    RS485_Send((uint8_t *)str, strlen(str));
}

uint8_t RS485_GetLine(char *out, uint16_t max_len)
{
    if (rx_ready == 0)
    {
        return 0;
    }

    __disable_irq();

    strncpy(out, rx_line, max_len - 1);
    out[max_len - 1] = '\0';
    rx_ready = 0;

    __enable_irq();

    return 1;
}

void RS485_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (rs485_huart == NULL)
    {
        return;
    }

    if (huart->Instance != rs485_huart->Instance)
    {
        return;
    }

    if (rx_byte == '\r' || rx_byte == '\n')
    {
        if (rx_index > 0)
        {
            rx_line[rx_index] = '\0';
            rx_ready = 1;
            rx_index = 0;
        }
    }
    else
    {
        if (rx_index < RS485_RX_BUFFER_SIZE - 1)
        {
            rx_line[rx_index++] = rx_byte;
        }
        else
        {
            rx_index = 0;
        }
    }

    HAL_UART_Receive_IT(rs485_huart, &rx_byte, 1);
}
