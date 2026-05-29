/*
 * rs485.h
 *
 *  Created on: May 29, 2026
 *      Author: tranquocvu2
 */

#ifndef INC_RS485_H_
#define INC_RS485_H_

#include "main.h"
#include <stdint.h>

#define RS485_RX_BUFFER_SIZE 128

void RS485_Init(UART_HandleTypeDef *huart);
void RS485_SetTxMode(void);
void RS485_SetRxMode(void);
void RS485_Send(uint8_t *data, uint16_t size);
void RS485_SendString(const char *str);
uint8_t RS485_GetLine(char *out, uint16_t max_len);
void RS485_UART_RxCpltCallback(UART_HandleTypeDef *huart);


#endif /* INC_RS485_H_ */
