/*
 * max485.c
 *
 *  Created on: May 28, 2026
 *      Author: tranquocvu2
 */


#include "max485.h"
#include "uart.h"
#include <string.h>

#define MAX485_TX_TIMEOUT 1000

static UART_HandleTypeDef *max485_huart = NULL;

void MAX485_Init(UART_HandleTypeDef *huart)
{
    max485_huart = huart;

    MAX485_ReceiveMode();

    UART_Init(max485_huart);
}

void MAX485_TransmitMode(void)
{
    /*
     * DE  = 1: cho phép truyền
     * /RE = 1: tắt nhận
     */
    HAL_GPIO_WritePin(RS485_DE_GPIO_Port, RS485_DE_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(RS485_RE_GPIO_Port, RS485_RE_Pin, GPIO_PIN_SET);
}

void MAX485_ReceiveMode(void)
{
    /*
     * DE  = 0: tắt truyền
     * /RE = 0: cho phép nhận
     */
    HAL_GPIO_WritePin(RS485_DE_GPIO_Port, RS485_DE_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(RS485_RE_GPIO_Port, RS485_RE_Pin, GPIO_PIN_RESET);
}

HAL_StatusTypeDef MAX485_Send(const uint8_t *data, uint16_t size)
{
    HAL_StatusTypeDef ret;

    if (max485_huart == NULL || data == NULL || size == 0)
    {
        return HAL_ERROR;
    }

    MAX485_TransmitMode();

    ret = UART_Send(data, size, MAX485_TX_TIMEOUT);

    UART_WaitTxComplete(MAX485_TX_TIMEOUT);

    MAX485_ReceiveMode();

    return ret;
}

HAL_StatusTypeDef MAX485_SendString(const char *str)
{
    if (str == NULL)
    {
        return HAL_ERROR;
    }

    return MAX485_Send((const uint8_t *)str, strlen(str));
}

uint8_t MAX485_Available(void)
{
    return UART_Available();
}

char* MAX485_ReadLine(void)
{
    return UART_ReadLine();
}

void MAX485_UART_Callback(UART_HandleTypeDef *huart)
{
    UART_RxCpltCallback(huart);
}
