/* ps2_keyboard.c */
#include "ps2_keyboard.h"

// ==========================================================
// PS/2 connections
// Clock -> PA9
// Data  -> PA10
//
// LED outputs based on the table:
// A  -> PA0
// B  -> PA4
// C  -> PA5
// D  -> PA6
// E  -> PA7
// F  -> PA11
// G  -> PA12
// H  -> PB0
// I  -> PB1
// J  -> PB5
// K  -> PB6
// L  -> PB7
// M  -> PB8
// N  -> PB9
// O  -> PB10
// P  -> PB11
// Q  -> PB12
// R  -> PB13
// S  -> PB14
// T  -> PB15
// U  -> PC0
// V  -> PC1
// W  -> PC2
// X  -> PC3
// Y  -> PC4
// Z  -> PC5
// ==========================================================

// PS/2 receiver state variables
static volatile uint8_t ps2BitCount = 0;
static volatile uint8_t ps2DataByte = 0;
static volatile uint8_t ps2BreakCode = 0;
static volatile uint8_t ps2ExtendedCode = 0;

// 26-bit software copy of the letter LED states
static volatile uint32_t ledStateMask = 0U;

// Flicker mode state
static volatile uint8_t flickerModeEnabled = 0U;
static volatile uint8_t flickerPhaseOn = 0U;

// ----------------------------------------------------------
// Helper function: configure a GPIO pin as push-pull output
// at 2 MHz
// ----------------------------------------------------------
static void GPIO_Config_Output_PP_2MHz(GPIO_TypeDef *port, uint8_t pin)
{
    volatile uint32_t *configReg;
    uint32_t shift;

    if (pin < 8U)
    {
        configReg = &port->CRL;
        shift = pin * 4U;
    }
    else
    {
        configReg = &port->CRH;
        shift = (pin - 8U) * 4U;
    }

    *configReg &= ~(0xFU << shift);
    *configReg |= (0x2U << shift);
}

static void GPIO_Pin_Low(GPIO_TypeDef *port, uint8_t pin)
{
    port->BRR = (1U << pin);
}

static void GPIO_Pin_High(GPIO_TypeDef *port, uint8_t pin)
{
    port->BSRR = (1U << pin);
}

static void LED_GPIO_Init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;

    GPIO_Config_Output_PP_2MHz(GPIOA, 0);
    GPIO_Config_Output_PP_2MHz(GPIOA, 4);
    GPIO_Config_Output_PP_2MHz(GPIOA, 5);
    GPIO_Config_Output_PP_2MHz(GPIOA, 6);
    GPIO_Config_Output_PP_2MHz(GPIOA, 7);
    GPIO_Config_Output_PP_2MHz(GPIOA, 11);
    GPIO_Config_Output_PP_2MHz(GPIOA, 12);

    GPIO_Config_Output_PP_2MHz(GPIOB, 0);
    GPIO_Config_Output_PP_2MHz(GPIOB, 1);
    GPIO_Config_Output_PP_2MHz(GPIOB, 5);
    GPIO_Config_Output_PP_2MHz(GPIOB, 6);
    GPIO_Config_Output_PP_2MHz(GPIOB, 7);
    GPIO_Config_Output_PP_2MHz(GPIOB, 8);
    GPIO_Config_Output_PP_2MHz(GPIOB, 9);
    GPIO_Config_Output_PP_2MHz(GPIOB, 10);
    GPIO_Config_Output_PP_2MHz(GPIOB, 11);
    GPIO_Config_Output_PP_2MHz(GPIOB, 12);
    GPIO_Config_Output_PP_2MHz(GPIOB, 13);
    GPIO_Config_Output_PP_2MHz(GPIOB, 14);
    GPIO_Config_Output_PP_2MHz(GPIOB, 15);

    GPIO_Config_Output_PP_2MHz(GPIOC, 0);
    GPIO_Config_Output_PP_2MHz(GPIOC, 1);
    GPIO_Config_Output_PP_2MHz(GPIOC, 2);
    GPIO_Config_Output_PP_2MHz(GPIOC, 3);
    GPIO_Config_Output_PP_2MHz(GPIOC, 4);
    GPIO_Config_Output_PP_2MHz(GPIOC, 5);

    GPIOA->BRR = (1U << 0) | (1U << 4) | (1U << 5) | (1U << 6) | (1U << 7) | (1U << 11) | (1U << 12);
    GPIOB->BRR = (1U << 0) | (1U << 1) | (1U << 5) | (1U << 6) | (1U << 7) | (1U << 8) |
                 (1U << 9) | (1U << 10) | (1U << 11) | (1U << 12) | (1U << 13) | (1U << 14) | (1U << 15);
    GPIOC->BRR = (1U << 0) | (1U << 1) | (1U << 2) | (1U << 3) | (1U << 4) | (1U << 5);
}

static int8_t ScanCode_ToIndex(uint8_t scanCode)
{
    switch (scanCode)
    {
        case 0x1C: return 0;   // A
        case 0x32: return 1;   // B
        case 0x21: return 2;   // C
        case 0x23: return 3;   // D
        case 0x24: return 4;   // E
        case 0x2B: return 5;   // F
        case 0x34: return 6;   // G
        case 0x33: return 7;   // H
        case 0x43: return 8;   // I
        case 0x3B: return 9;   // J
        case 0x42: return 10;  // K
        case 0x4B: return 11;  // L
        case 0x3A: return 12;  // M
        case 0x31: return 13;  // N
        case 0x44: return 14;  // O
        case 0x4D: return 15;  // P
        case 0x15: return 16;  // Q
        case 0x2D: return 17;  // R
        case 0x1B: return 18;  // S
        case 0x2C: return 19;  // T
        case 0x3C: return 20;  // U
        case 0x2A: return 21;  // V
        case 0x1D: return 22;  // W
        case 0x22: return 23;  // X
        case 0x35: return 24;  // Y
        case 0x1A: return 25;  // Z
        default:   return -1;
    }
}

static void LED_All_Off(void)
{
    GPIOA->BRR = (1U << 0) | (1U << 4) | (1U << 5) | (1U << 6) | (1U << 7) | (1U << 11) | (1U << 12);
    GPIOB->BRR = (1U << 0) | (1U << 1) | (1U << 5) | (1U << 6) | (1U << 7) | (1U << 8) |
                 (1U << 9) | (1U << 10) | (1U << 11) | (1U << 12) | (1U << 13) | (1U << 14) | (1U << 15);
    GPIOC->BRR = (1U << 0) | (1U << 1) | (1U << 2) | (1U << 3) | (1U << 4) | (1U << 5);
}

static void LED_All_On(void)
{
    GPIOA->BSRR = (1U << 0) | (1U << 4) | (1U << 5) | (1U << 6) | (1U << 7) | (1U << 11) | (1U << 12);
    GPIOB->BSRR = (1U << 0) | (1U << 1) | (1U << 5) | (1U << 6) | (1U << 7) | (1U << 8) |
                  (1U << 9) | (1U << 10) | (1U << 11) | (1U << 12) | (1U << 13) | (1U << 14) | (1U << 15);
    GPIOC->BSRR = (1U << 0) | (1U << 1) | (1U << 2) | (1U << 3) | (1U << 4) | (1U << 5);
}

static void LED_ApplyMask(uint32_t mask)
{
    LED_All_Off();

    if (mask & (1U << 0))  GPIO_Pin_High(GPIOA, 0);
    if (mask & (1U << 1))  GPIO_Pin_High(GPIOA, 4);
    if (mask & (1U << 2))  GPIO_Pin_High(GPIOA, 5);
    if (mask & (1U << 3))  GPIO_Pin_High(GPIOA, 6);
    if (mask & (1U << 4))  GPIO_Pin_High(GPIOA, 7);
    if (mask & (1U << 5))  GPIO_Pin_High(GPIOA, 11);
    if (mask & (1U << 6))  GPIO_Pin_High(GPIOA, 12);

    if (mask & (1U << 7))  GPIO_Pin_High(GPIOB, 0);
    if (mask & (1U << 8))  GPIO_Pin_High(GPIOB, 1);
    if (mask & (1U << 9))  GPIO_Pin_High(GPIOB, 5);
    if (mask & (1U << 10)) GPIO_Pin_High(GPIOB, 6);
    if (mask & (1U << 11)) GPIO_Pin_High(GPIOB, 7);
    if (mask & (1U << 12)) GPIO_Pin_High(GPIOB, 8);
    if (mask & (1U << 13)) GPIO_Pin_High(GPIOB, 9);
    if (mask & (1U << 14)) GPIO_Pin_High(GPIOB, 10);
    if (mask & (1U << 15)) GPIO_Pin_High(GPIOB, 11);
    if (mask & (1U << 16)) GPIO_Pin_High(GPIOB, 12);
    if (mask & (1U << 17)) GPIO_Pin_High(GPIOB, 13);
    if (mask & (1U << 18)) GPIO_Pin_High(GPIOB, 14);
    if (mask & (1U << 19)) GPIO_Pin_High(GPIOB, 15);

    if (mask & (1U << 20)) GPIO_Pin_High(GPIOC, 0);
    if (mask & (1U << 21)) GPIO_Pin_High(GPIOC, 1);
    if (mask & (1U << 22)) GPIO_Pin_High(GPIOC, 2);
    if (mask & (1U << 23)) GPIO_Pin_High(GPIOC, 3);
    if (mask & (1U << 24)) GPIO_Pin_High(GPIOC, 4);
    if (mask & (1U << 25)) GPIO_Pin_High(GPIOC, 5);
}

static void PS2_GPIO_EXTI_Init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;

    GPIOA->CRH &= ~(GPIO_CRH_MODE9 | GPIO_CRH_CNF9);
    GPIOA->CRH |= GPIO_CRH_CNF9_0;

    GPIOA->CRH &= ~(GPIO_CRH_MODE10 | GPIO_CRH_CNF10);
    GPIOA->CRH |= GPIO_CRH_CNF10_0;

    AFIO->EXTICR[2] &= ~AFIO_EXTICR3_EXTI9;

    EXTI->IMR |= EXTI_IMR_MR9;
    EXTI->FTSR |= EXTI_FTSR_TR9;
    EXTI->RTSR &= ~EXTI_RTSR_TR9;
    EXTI->PR = EXTI_PR_PR9;

    NVIC_ClearPendingIRQ(EXTI9_5_IRQn);
    NVIC_EnableIRQ(EXTI9_5_IRQn);
}

static void PS2_ProcessByte(uint8_t byte)
{
    int8_t ledIndex;

    if (byte == 0xE0)
    {
        ps2ExtendedCode = 1U;
        return;
    }

    if (byte == 0xF0)
    {
        ps2BreakCode = 1U;
        return;
    }

    if (!ps2ExtendedCode)
    {
        ledIndex = ScanCode_ToIndex(byte);

        if (ledIndex >= 0)
        {
            if (ps2BreakCode)
            {
                ledStateMask &= ~(1UL << (uint32_t)ledIndex);
            }
            else
            {
                ledStateMask |= (1UL << (uint32_t)ledIndex);
            }

            if (!flickerModeEnabled)
            {
                LED_ApplyMask(ledStateMask);
            }
        }
    }

    ps2BreakCode = 0U;
    ps2ExtendedCode = 0U;
}

void PS2_Keyboard_Init(void)
{
    LED_GPIO_Init();
    PS2_GPIO_EXTI_Init();

    SystemCoreClockUpdate();
    SysTick_Config(SystemCoreClock / 10U);   // 100 ms flicker period
}

void PS2_SetFlickerMode(uint8_t enable)
{
    if (enable)
    {
        flickerModeEnabled = 1U;
        flickerPhaseOn = 1U;
        LED_All_On();
    }
    else
    {
        flickerModeEnabled = 0U;
        flickerPhaseOn = 0U;
        LED_ApplyMask(ledStateMask);
    }
}

void SysTick_Handler(void)
{
    if (flickerModeEnabled)
    {
        flickerPhaseOn ^= 1U;

        if (flickerPhaseOn)
        {
            LED_All_On();
        }
        else
        {
            LED_All_Off();
        }
    }
}

void EXTI9_5_IRQHandler(void)
{
    uint8_t bitValue;

    if (EXTI->PR & EXTI_PR_PR9)
    {
        EXTI->PR = EXTI_PR_PR9;

        bitValue = (GPIOA->IDR & GPIO_IDR_IDR10) ? 1U : 0U;

        if (ps2BitCount == 0U)
        {
            if (bitValue == 0U)
            {
                ps2BitCount = 1U;
                ps2DataByte = 0U;
            }
        }
        else if ((ps2BitCount >= 1U) && (ps2BitCount <= 8U))
        {
            if (bitValue)
            {
                ps2DataByte |= (uint8_t)(1U << (ps2BitCount - 1U));
            }

            ps2BitCount++;
        }
        else if (ps2BitCount == 9U)
        {
            ps2BitCount++;
        }
        else if (ps2BitCount == 10U)
        {
            if (bitValue == 1U)
            {
                PS2_ProcessByte(ps2DataByte);
            }

            ps2BitCount = 0U;
        }
        else
        {
            ps2BitCount = 0U;
        }
    }
}