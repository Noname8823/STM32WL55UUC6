/*
 * uart.h
 *
 *  Created on: May 28, 2026
 *      Author: tranquocvu2
 */

#ifndef INC_UART_H_
#define INC_UART_H_

#include "main.h"
#include "stdint.h"

#define UART_RX_BUFFER_SIZE 128

void UART_Init(UART_HandleTypeDef *huart);

HAL_StatusTypeDef UART_Send(const uint8_t *data, uint16_t size, uint32_t timeout);
HAL_StatusTypeDef UART_SendString(const char *str);
HAL_StatusTypeDef UART_WaitTxComplete(uint32_t timeout);

uint8_t UART_Available(void);
char* UART_ReadLine(void);

void UART_RxCpltCallback(UART_HandleTypeDef *huart);


#endif /* INC_UART_H_ */
