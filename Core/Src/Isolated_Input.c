#include "isolated_input.h"

void Input_Init(void)
{
    /* GPIO đã init trong MX_GPIO_Init rồi */
}

uint8_t Input_GetState(uint8_t ch)
{
    GPIO_PinState s = GPIO_PIN_RESET;

    switch (ch)
    {
    case 0:
        s = HAL_GPIO_ReadPin(IN1_GPIO_Port, IN1_Pin);
        break;

    case 1:
        s = HAL_GPIO_ReadPin(IN2_GPIO_Port, IN2_Pin);
        break;

    case 2:
        s = HAL_GPIO_ReadPin(IN3_GPIO_Port, IN3_Pin);
        break;

    case 3:
        s = HAL_GPIO_ReadPin(IN4_GPIO_Port, IN4_Pin);
        break;

    default:
        return 0;
    }

#if INPUT_ACTIVE_HIGH
    return (s == GPIO_PIN_SET) ? 1 : 0;
#else
    return (s == GPIO_PIN_RESET) ? 1 : 0;
#endif
}

uint8_t Input_GetMask(void)
{
    uint8_t mask = 0;

    if (Input_GetState(0)) mask |= 0x01;
    if (Input_GetState(1)) mask |= 0x02;
    if (Input_GetState(2)) mask |= 0x04;
    if (Input_GetState(3)) mask |= 0x08;

    return mask;
}
