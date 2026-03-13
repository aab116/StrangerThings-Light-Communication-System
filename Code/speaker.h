#ifndef SPEAKER_H
#define SPEAKER_H

#include "stm32f10x.h"

// Initializes PA8 as TIM1_CH1 PWM output.
// Pass the tone frequency in Hz, for example 1000 for 1 kHz.
void Speaker_Init(uint32_t toneFrequencyHz);

// Turns the tone on
void Speaker_On(void);

// Turns the tone off
void Speaker_Off(void);

#endif