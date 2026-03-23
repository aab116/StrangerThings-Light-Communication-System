/* main.c */
#include "stm32f10x.h"
#include "button.h"
#include "speaker.h"
#include "ps2_keyboard.h"

int main(void)
{
    Speaker_Init(1000);      // PA8 speaker, 1 kHz tone
    Speaker_Off();

    PS2_Keyboard_Init();     // PS/2 keyboard + LED control + flicker timer
    Button_Init();           // PA1 pushbutton interrupt

    while (1)
    {
        __WFI();             // Sleep until an interrupt occurs
    }
}