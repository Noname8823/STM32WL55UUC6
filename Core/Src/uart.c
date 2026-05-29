/*
 * uart.c
 *
 *  Created on: May 28, 2026
 *      Author: tranquocvu2
 */


#include "uart.h"
#include <string.h>

static UART_HandleTypeDef *uart_huart = NULL;

static uint8_t uart_rx_data;
static char uart_rx_buffer[UART_RX_BUFFER_SIZE];

static volatile uint16_t uart_rx_index = 0;
static volatile uint8_t uart_rx_done = 0;

void UART_Init(UART_HandleTypeDef *huart)
{
    uart_huart = huart;

    uart_rx_index = 0;
    uart_rx_done = 0;

    HAL_UART_Receive_IT(uart_huart, &uart_rx_data, 1);
}

HAL_StatusTypeDef UART_Send(const uint8_t *data, uint16_t size, uint32_t timeout)
{
    if (uart_huart == NULL || data == NULL || size == 0)
    {
        return HAL_ERROR;
    }

    return HAL_UART_Transmit(uart_huart, (uint8_t *)data, size, timeout);
}

HAL_StatusTypeDef UART_SendString(const char *str)
{
    if (str == NULL)
    {
        return HAL_ERROR;
    }

    return UART_Send((const uint8_t *)str, strlen(str), 1000);
}

HAL_StatusTypeDef UART_WaitTxComplete(uint32_t timeout)
{
    uint32_t tick_start;

    if (uart_huart == NULL)
    {
        return HAL_ERROR;
    }

    tick_start = HAL_GetTick();

    while (__HAL_UART_GET_FLAG(uart_huart, UART_FLAG_TC) == RESET)
    {
        if (timeout != HAL_MAX_DELAY)
        {
            if ((HAL_GetTick() - tick_start) > timeout)
            {
                return HAL_TIMEOUT;
            }
        }
    }

    return HAL_OK;
}

uint8_t UART_Available(void)
{
    return uart_rx_done;
}

char* UART_ReadLine(void)
{
    if (uart_rx_done == 0)
    {
        return NULL;
    }

    uart_rx_done = 0;
    uart_rx_index = 0;

    return uart_rx_buffer;
}

void UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (uart_huart == NULL)
    {
        return;
    }

    if (huart->Instance != uart_huart->Instance)
    {
        return;
    }

    if (uart_rx_done == 0)
    {
        if (uart_rx_data == '\r' || uart_rx_data == '\n')
        {
            if (uart_rx_index > 0)
            {
                uart_rx_buffer[uart_rx_index] = '\0';
                uart_rx_done = 1;
            }
        }
        else
        {
            if (uart_rx_index < UART_RX_BUFFER_SIZE - 1)
            {
                uart_rx_buffer[uart_rx_index] = uart_rx_data;
                uart_rx_index++;
            }
            else
            {
                uart_rx_buffer[UART_RX_BUFFER_SIZE - 1] = '\0';
                uart_rx_done = 1;
            }
        }
    }

    HAL_UART_Receive_IT(uart_huart, &uart_rx_data, 1);
}
