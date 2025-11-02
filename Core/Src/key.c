#include "key.h"
#include "tim.h"

#define LONGCLICK_THRESHOLD 50
#define BOUNCE_TIME 10

KeyState key_state[KEY_COUNT] = {KEY_START, KEY_START, KEY_START, KEY_START};
GPIO_TypeDef *key_port[KEY_COUNT] = {Key1_GPIO_Port, Key2_GPIO_Port,
                                     Key3_GPIO_Port, Key4_GPIO_Port};
uint16_t key_pin[KEY_COUNT] = {Key1_Pin, Key2_Pin, Key3_Pin, Key4_Pin};
uint8_t key_clicked_flag[KEY_COUNT];
uint8_t key_long_clicked_flag[KEY_COUNT];

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    static int period_counter[KEY_COUNT];
    static int pressing_time[KEY_COUNT];

    if (htim != &htim4) return;
    // 每100ms一次

    for (int i = 0; i < KEY_COUNT; i++)
    {
        switch (key_state[i])
        {
        case KEY_START:
            if (HAL_GPIO_ReadPin(key_port[i], key_pin[i]) == GPIO_PIN_RESET)
            {
                period_counter[i] = 0;
                key_state[i] = PRESS_CHECK;
            }
            break;
        case PRESS_CHECK:
            period_counter[i]++;
            if (period_counter[i] > BOUNCE_TIME)
            {
                if (HAL_GPIO_ReadPin(key_port[i], key_pin[i]) == GPIO_PIN_RESET)
                {
                    key_state[i] = PRESSING;
                    pressing_time[i] = 0;
                }
                else
                    key_state[i] = KEY_START;
            }
            break;
        case PRESSING:
            pressing_time[i]++;
            if (HAL_GPIO_ReadPin(key_port[i], key_pin[i]) == GPIO_PIN_SET)
            {
                period_counter[i] = 0;
                key_state[i] = RELEASE_CHECK;
            }
            break;
        case RELEASE_CHECK:
            period_counter[i]++;
            if (period_counter[i] > BOUNCE_TIME)
            {
                if (HAL_GPIO_ReadPin(key_port[i], key_pin[i]) == GPIO_PIN_RESET)
                    key_state[i] = PRESSING;
                else
                    key_state[i] = RELEASING;
            }
            break;
        case RELEASING:
            if (pressing_time[i] < LONGCLICK_THRESHOLD)
                key_clicked_flag[i] = 1;
            else
                key_long_clicked_flag[i] = 1;
            key_state[i] = KEY_START;
            break;
        }
    }
}

void key_update()
{
    for (int i = 0; i < KEY_COUNT; i++)
    {
        if (key_clicked_flag[i] == 1)
        {
            key_clicked(i);
            key_clicked_flag[i] = 0;
        }
        if (key_long_clicked_flag[i] == 1)
        {
            key_long_clicked(i);
            key_long_clicked_flag[i] = 0;
        }
    }
}