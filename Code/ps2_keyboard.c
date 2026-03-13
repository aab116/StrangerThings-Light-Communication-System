#include "ps2_keyboard.h"

// PS/2 connections:
// Clock -> PA9
// Data  -> PA10
// A output -> PA0

static volatile uint8_t ps2BitCount = 0;
static volatile uint8_t ps2DataByte = 0;
static volatile uint8_t ps2BreakCode = 0;
static volatile uint8_t ps2ExtendedCode = 0;

static void PA0_Init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;

    // PA0 = general purpose push-pull output, 2 MHz
    GPIOA->CRL &= ~(GPIO_CRL_MODE0 | GPIO_CRL_CNF0);
    GPIOA->CRL |= GPIO_CRL_MODE0_1;

    // Start LOW
    GPIOA->BRR = GPIO_BRR_BR0;
}

static void PS2_GPIO_EXTI_Init(void)
{
    // Enable GPIOA and AFIO clocks
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;

    // PA9 = input floating (PS/2 clock)
    GPIOA->CRH &= ~(GPIO_CRH_MODE9 | GPIO_CRH_CNF9);
    GPIOA->CRH |= GPIO_CRH_CNF9_0;

    // PA10 = input floating (PS/2 data)
    GPIOA->CRH &= ~(GPIO_CRH_MODE10 | GPIO_CRH_CNF10);
    GPIOA->CRH |= GPIO_CRH_CNF10_0;

    // EXTI9 mapped to PA9
    AFIO->EXTICR[2] &= ~AFIO_EXTICR3_EXTI9;

    // Unmask EXTI9
    EXTI->IMR |= EXTI_IMR_MR9;

    // Trigger on falling edge only
    EXTI->FTSR |= EXTI_FTSR_TR9;
    EXTI->RTSR &= ~EXTI_RTSR_TR9;

    // Clear pending flag
    EXTI->PR = EXTI_PR_PR9;

    NVIC_ClearPendingIRQ(EXTI9_5_IRQn);
    NVIC_EnableIRQ(EXTI9_5_IRQn);
}

static void PS2_ProcessByte(uint8_t byte)
{
    // Assumes standard PS/2 Set 2:
    // A make  = 0x1C
    // A break = F0 1C

    if (byte == 0xE0)
    {
        ps2ExtendedCode = 1;
        return;
    }

    if (byte == 0xF0)
    {
        ps2BreakCode = 1;
        return;
    }

    // Only handle the A key for now
    if (!ps2ExtendedCode && byte == 0x1C)
    {
        if (ps2BreakCode)
        {
            // A released -> PA0 LOW
            GPIOA->BRR = GPIO_BRR_BR0;
        }
        else
        {
            // A pressed -> PA0 HIGH
            GPIOA->BSRR = GPIO_BSRR_BS0;
        }
    }

    ps2BreakCode = 0;
    ps2ExtendedCode = 0;
}

void PS2_Keyboard_Init(void)
{
    PA0_Init();
    PS2_GPIO_EXTI_Init();
}

void EXTI9_5_IRQHandler(void)
{
    uint8_t bitValue;

    if (EXTI->PR & EXTI_PR_PR9)
    {
        // Clear interrupt flag
        EXTI->PR = EXTI_PR_PR9;

        // Sample data on each falling edge of clock
        bitValue = (GPIOA->IDR & GPIO_IDR_IDR10) ? 1U : 0U;

        if (ps2BitCount == 0U)
        {
            // Start bit must be 0
            if (bitValue == 0U)
            {
                ps2BitCount = 1U;
                ps2DataByte = 0U;
            }
        }
        else if (ps2BitCount >= 1U && ps2BitCount <= 8U)
        {
            // 8 data bits, LSB first
            if (bitValue)
            {
                ps2DataByte |= (uint8_t)(1U << (ps2BitCount - 1U));
            }

            ps2BitCount++;
        }
        else if (ps2BitCount == 9U)
        {
            // Parity bit, ignore for now
            ps2BitCount++;
        }
        else if (ps2BitCount == 10U)
        {
            // Stop bit should be 1
            if (bitValue == 1U)
            {
                PS2_ProcessByte(ps2DataByte);
            }

            // Reset for next frame
            ps2BitCount = 0U;
        }
        else
        {
            ps2BitCount = 0U;
        }
    }
}