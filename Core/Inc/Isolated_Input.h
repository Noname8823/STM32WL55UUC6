#ifndef INC_ISOLATED_INPUT_H_
#define INC_ISOLATED_INPUT_H_

#include "main.h"
#include <stdint.h>

/*
 * 0 = Active High
 * 1 = Active Low
 */
#define INPUT_ACTIVE_LOW 0

typedef enum
{
    INPUT_1 = 0,
    INPUT_2,
    INPUT_3,
    INPUT_4
} InputChannel_t;

void Input_Init(void);
uint8_t Input_GetState(InputChannel_t ch);
void Input_GetAll(uint8_t State[4]);

#endif /* INC_ISOLATED_INPUT_H_ */
