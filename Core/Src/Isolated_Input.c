#include "Isolated_Input.h"
#include "main.h"

void Input_Init(void)
{
    // Hiện tại chưa cần làm gì
}

static GPIO_PinState Input_ReadRaw(InputChannel_t ch)
{
    switch (ch)
    {
        case INPUT_1:
            return HAL_GPIO_ReadPin(IN1_GPIO_Port, IN1_Pin);

        case INPUT_2:
            return HAL_GPIO_ReadPin(IN2_GPIO_Port, IN2_Pin);

        case INPUT_3:
            return HAL_GPIO_ReadPin(IN3_GPIO_Port, IN3_Pin);

        case INPUT_4:
            return HAL_GPIO_ReadPin(IN4_GPIO_Port, IN4_Pin);

        default:
            return GPIO_PIN_RESET;
    }
}

uint8_t Input_GetState(InputChannel_t ch)
{
    GPIO_PinState raw = Input_ReadRaw(ch);

#if INPUT_ACTIVE_LOW
    return (raw == GPIO_PIN_RESET) ? 1 : 0;
#else
    return (raw == GPIO_PIN_SET) ? 1 : 0;
#endif
}

void Input_GetAll(uint8_t State[4])
{
    State[0] = Input_GetState(INPUT_1);
    State[1] = Input_GetState(INPUT_2);
    State[2] = Input_GetState(INPUT_3);
    State[3] = Input_GetState(INPUT_4);
}
