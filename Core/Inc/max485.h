/*
 * max485.h
 *
 *  Created on: May 28, 2026
 *      Author: tranquocvu2
 */

#ifndef INC_MAX485_H_
#define INC_MAX485_H_

#include "main.h"
#include <stdint.h>

void MAX485_Init(UART_HandleTypeDef *huart);

void MAX485_TransmitMode(void);
void MAX485_ReceiveMode(void);

HAL_StatusTypeDef MAX485_Send(const uint8_t *data, uint16_t size);
HAL_StatusTypeDef MAX485_SendString(const char *str);

uint8_t MAX485_Available(void);
char* MAX485_ReadLine(void);

void MAX485_UART_Callback(UART_HandleTypeDef *huart);


#endif /* INC_MAX485_H_ */
