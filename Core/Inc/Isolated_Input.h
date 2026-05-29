#ifndef INC_ISOLATED_INPUT_H_
#define INC_ISOLATED_INPUT_H_

#include "main.h"
#include <stdint.h>

#define INPUT_ACTIVE_HIGH 1

void Input_Init(void);
uint8_t Input_GetState(uint8_t ch);
uint8_t Input_GetMask(void);

#endif
