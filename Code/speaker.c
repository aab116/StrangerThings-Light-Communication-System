#include "speaker.h"

// Stores the CCR1 value used for the current duty cycle
static uint32_t speakerCompareValue = 0U;

// Tracks whether sound output is currently enabled
static uint8_t speakerEnabled = 0U;

// ----------------------------------------------------------
// Change the PWM tone frequency while TIM1 is running
// ----------------------------------------------------------
void Speaker_SetTone(uint32_t toneFrequencyHz)
{
    uint32_t prescaler;
    uint32_t autoReload;

    // Prevent impossible / silly values
    if (toneFrequencyHz < 100U)
    {
        toneFrequencyHz = 100U;
    }

    SystemCoreClockUpdate();

    // Timer clock = 1 MHz
    prescaler = (SystemCoreClock / 1000000U) - 1U;
    autoReload = (1000000U / toneFrequencyHz) - 1U;

    TIM1->PSC = prescaler;
    TIM1->ARR = autoReload;

    // Use about 35% duty cycle instead of 50%
    // This makes the sound a bit harsher / rougher
    speakerCompareValue = ((autoReload + 1U) * 20U) / 100U;

    // If speaker is currently ON, keep output active
    // If OFF, keep silent
    if (speakerEnabled)
    {
        TIM1->CCR1 = speakerCompareValue;
    }
    else
    {
        TIM1->CCR1 = 0U;
    }

    // Force registers to reload now
    TIM1->EGR |= TIM_EGR_UG;
}

void Speaker_Init(uint32_t toneFrequencyHz)
{
    // Enable clocks:
    // GPIOA for PA8
    // AFIO for alternate function output
    // TIM1 for PWM generation
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;
    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;

    // PA8 = alternate function push-pull, 2 MHz
    GPIOA->CRH &= ~(GPIO_CRH_MODE8 | GPIO_CRH_CNF8);
    GPIOA->CRH |= GPIO_CRH_MODE8_1 | GPIO_CRH_CNF8_1;

    // TIM1 CH1 = PWM mode 1, preload enabled
    TIM1->CCMR1 &= ~(TIM_CCMR1_CC1S | TIM_CCMR1_OC1M);
    TIM1->CCMR1 |= TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2;
    TIM1->CCMR1 |= TIM_CCMR1_OC1PE;

    // Enable channel output
    TIM1->CCER |= TIM_CCER_CC1E;

    // Enable ARR preload
    TIM1->CR1 |= TIM_CR1_ARPE;

    // Advanced timer main output enable
    TIM1->BDTR |= TIM_BDTR_MOE;

    // Start in OFF state
    speakerEnabled = 0U;

    // Set initial tone
    Speaker_SetTone(toneFrequencyHz);

    // Start timer
    TIM1->CR1 |= TIM_CR1_CEN;
}

void Speaker_On(void)
{
    speakerEnabled = 1U;
    TIM1->CCR1 = speakerCompareValue;
}

void Speaker_Off(void)
{
    speakerEnabled = 0U;
    TIM1->CCR1 = 0U;
}