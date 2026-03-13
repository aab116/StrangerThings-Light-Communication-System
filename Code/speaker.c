#include "speaker.h"

// Stores the CCR1 value that gives approximately 50% duty cycle
static uint32_t speakerCompareValue = 0;

void Speaker_Init(uint32_t toneFrequencyHz)
{
    uint32_t prescaler;
    uint32_t autoReload;

    // Update system clock variable
    SystemCoreClockUpdate();

    // Enable clocks:
    // GPIOA for PA8
    // AFIO for alternate function output
    // TIM1 for PWM generation
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;
    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;

    // -----------------------------------------
    // Configure PA8 as Alternate Function Push-Pull
    //
    // PA8 is controlled by GPIOA->CRH
    // MODE8 = 10 -> output, max speed 2 MHz
    // CNF8  = 10 -> alternate function push-pull
    // -----------------------------------------
    GPIOA->CRH &= ~(GPIO_CRH_MODE8 | GPIO_CRH_CNF8);
    GPIOA->CRH |= GPIO_CRH_MODE8_1 | GPIO_CRH_CNF8_1;

    // -----------------------------------------
    // Set up TIM1 base frequency
    //
    // Make timer count at 1 MHz:
    // PSC = (SystemCoreClock / 1,000,000) - 1
    //
    // Then:
    // ARR = (1,000,000 / toneFrequencyHz) - 1
    // -----------------------------------------
    prescaler = (SystemCoreClock / 1000000U) - 1U;
    autoReload = (1000000U / toneFrequencyHz) - 1U;

    TIM1->PSC = prescaler;
    TIM1->ARR = autoReload;

    // 50% duty cycle compare value
    speakerCompareValue = (autoReload + 1U) / 2U;

    // Start with output OFF
    TIM1->CCR1 = 0U;

    // -----------------------------------------
    // Configure TIM1 Channel 1 as PWM Mode 1
    //
    // CC1S = 00 -> output
    // OC1M = 110 -> PWM mode 1
    // OC1PE = 1 -> preload enable
    // -----------------------------------------
    TIM1->CCMR1 &= ~(TIM_CCMR1_CC1S | TIM_CCMR1_OC1M);
    TIM1->CCMR1 |= TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2;
    TIM1->CCMR1 |= TIM_CCMR1_OC1PE;

    // Enable TIM1 Channel 1 output
    TIM1->CCER |= TIM_CCER_CC1E;

    // Enable auto-reload preload
    TIM1->CR1 |= TIM_CR1_ARPE;

    // TIM1 is an advanced-control timer, so MOE must be set
    TIM1->BDTR |= TIM_BDTR_MOE;

    // Load registers immediately
    TIM1->EGR |= TIM_EGR_UG;

    // Start timer
    TIM1->CR1 |= TIM_CR1_CEN;
}

void Speaker_On(void)
{
    // Apply 50% duty cycle so PA8 outputs the tone
    TIM1->CCR1 = speakerCompareValue;
}

void Speaker_Off(void)
{
    // 0% duty cycle = silent output
    TIM1->CCR1 = 0U;
}