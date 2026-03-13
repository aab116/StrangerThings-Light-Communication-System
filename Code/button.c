#include "button.h"

void Button_Init(void)
{
    // Enable GPIOA clock
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;

    // PA1 as input with pull-up/pull-down
    GPIOA->CRL &= ~(GPIO_CRL_MODE1 | GPIO_CRL_CNF1);
    GPIOA->CRL |= GPIO_CRL_CNF1_1;

    // Select pull-up on PA1
    GPIOA->BSRR = GPIO_BSRR_BS1;
}

int Button_IsPressed(void)
{
    // Active-low button:
    // not pressed -> 1
    // pressed     -> 0
    return ((GPIOA->IDR & GPIO_IDR_IDR1) == 0);
}