/* button.c */
#include "button.h"
#include "speaker.h"
#include "ps2_keyboard.h"

static volatile int buttonPressed = 0;

void Button_Init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;

    // PA1 as input with pull-up
    GPIOA->CRL &= ~(GPIO_CRL_MODE1 | GPIO_CRL_CNF1);
    GPIOA->CRL |= GPIO_CRL_CNF1_1;
    GPIOA->BSRR = GPIO_BSRR_BS1;

    // EXTI1 mapped to PA1
    AFIO->EXTICR[0] &= ~AFIO_EXTICR1_EXTI1;

    EXTI->IMR |= EXTI_IMR_MR1;
    EXTI->FTSR |= EXTI_FTSR_TR1;   // falling edge = press
    EXTI->RTSR |= EXTI_RTSR_TR1;   // rising edge = release
    EXTI->PR = EXTI_PR_PR1;

    buttonPressed = ((GPIOA->IDR & GPIO_IDR_IDR1) == 0U) ? 1 : 0;

    NVIC_ClearPendingIRQ(EXTI1_IRQn);
    NVIC_EnableIRQ(EXTI1_IRQn);
}

int Button_IsPressed(void)
{
    return buttonPressed;
}

void EXTI1_IRQHandler(void)
{
    if (EXTI->PR & EXTI_PR_PR1)
    {
        EXTI->PR = EXTI_PR_PR1;

        buttonPressed = ((GPIOA->IDR & GPIO_IDR_IDR1) == 0U) ? 1 : 0;

        /* Let the keyboard/sound module decide whether the overall effect
           should be on, based on button OR RUN trigger */
        PS2_SetButtonTrigger((uint8_t)buttonPressed);
    }
}