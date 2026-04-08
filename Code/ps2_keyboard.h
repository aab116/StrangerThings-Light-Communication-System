/* ps2_keyboard.h */
#ifndef PS2_KEYBOARD_H
#define PS2_KEYBOARD_H

#include "stm32f10x.h"

void PS2_Keyboard_Init(void);
void PS2_SetFlickerMode(uint8_t enable);

void PS2_SetButtonTrigger(uint8_t pressed);

#endif