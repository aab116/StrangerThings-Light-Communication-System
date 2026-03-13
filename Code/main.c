#include "stm32f10x.h"
#include "button.h"
#include "speaker.h"
#include "ps2_keyboard.h"

int main(void)
{
    Button_Init();         // PA1 pushbutton
    Speaker_Init(1000);    // PA8 speaker, 1 kHz tone
    Speaker_Off();

    PS2_Keyboard_Init();   // PA9 = clock, PA10 = data, PA0 = A-output LED

    while (1)
    {
        if (Button_IsPressed())
        {
            Speaker_On();
        }
        else
        {
            Speaker_Off();
        }
    }
}