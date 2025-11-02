#ifndef __KEY_H
#define __KEY_H

#include "gpio.h"

#define KEY_COUNT 4

typedef enum {
    KEY_START,
    PRESS_CHECK,
    PRESSING,
    RELEASE_CHECK,
    RELEASING
} KeyState;

extern KeyState key_state[KEY_COUNT];
extern GPIO_TypeDef *key_port[KEY_COUNT];
extern uint16_t key_pin[KEY_COUNT];

void key_update();
__weak void key_clicked(int key_num);
__weak void key_long_clicked(int key_num);

#endif