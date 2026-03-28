/* ps2_keyboard.c */
#include "ps2_keyboard.h"

// ==========================================================
// PS/2 connections
// Clock -> PA9
// Data  -> PA10
//
// Letter output mapping:
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
//
// ACTIVE-LOW OUTPUT BEHAVIOR:
// - Idle / not pressed = HIGH = 3.3 V
// - Pressed            = LOW  = 0 V
// ==========================================================

// PS/2 receiver state variables
static volatile uint8_t ps2BitCount = 0;
static volatile uint8_t ps2DataByte = 0;
static volatile uint8_t ps2BreakCode = 0;
static volatile uint8_t ps2ExtendedCode = 0;

// 26-bit software copy of the current letter states
// bit = 1 means key is currently pressed
static volatile uint32_t ledStateMask = 0U;

// Flicker mode state
static volatile uint8_t flickerModeEnabled = 0U;
static volatile uint8_t flickerPhaseOn = 0U;

// ----------------------------------------------------------
// Configure a GPIO pin as push-pull output at 2 MHz
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

// ----------------------------------------------------------
// Drive a pin LOW
// ----------------------------------------------------------
static void GPIO_Pin_Low(GPIO_TypeDef *port, uint8_t pin)
{
    port->BRR = (1U << pin);
}

// ----------------------------------------------------------
// Drive a pin HIGH
// ----------------------------------------------------------
static void GPIO_Pin_High(GPIO_TypeDef *port, uint8_t pin)
{
    port->BSRR = (1U << pin);
}

// ----------------------------------------------------------
// Initialize all 26 letter output pins
//
// Since outputs are ACTIVE-LOW:
// - OFF / idle state = HIGH
// So all pins are initialized HIGH.
// ----------------------------------------------------------
static void LED_GPIO_Init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;

    // Port A
    GPIO_Config_Output_PP_2MHz(GPIOA, 0);    // A
    GPIO_Config_Output_PP_2MHz(GPIOA, 4);    // B
    GPIO_Config_Output_PP_2MHz(GPIOA, 5);    // C
    GPIO_Config_Output_PP_2MHz(GPIOA, 6);    // D
    GPIO_Config_Output_PP_2MHz(GPIOA, 7);    // E
    GPIO_Config_Output_PP_2MHz(GPIOA, 11);   // F
    GPIO_Config_Output_PP_2MHz(GPIOA, 12);   // G

    // Port B
    GPIO_Config_Output_PP_2MHz(GPIOB, 0);    // H
    GPIO_Config_Output_PP_2MHz(GPIOB, 1);    // I
    GPIO_Config_Output_PP_2MHz(GPIOB, 5);    // J
    GPIO_Config_Output_PP_2MHz(GPIOB, 6);    // K
    GPIO_Config_Output_PP_2MHz(GPIOB, 7);    // L
    GPIO_Config_Output_PP_2MHz(GPIOB, 8);    // M
    GPIO_Config_Output_PP_2MHz(GPIOB, 9);    // N
    GPIO_Config_Output_PP_2MHz(GPIOB, 10);   // O
    GPIO_Config_Output_PP_2MHz(GPIOB, 11);   // P
    GPIO_Config_Output_PP_2MHz(GPIOB, 12);   // Q
    GPIO_Config_Output_PP_2MHz(GPIOB, 13);   // R
    GPIO_Config_Output_PP_2MHz(GPIOB, 14);   // S
    GPIO_Config_Output_PP_2MHz(GPIOB, 15);   // T

    // Port C
    GPIO_Config_Output_PP_2MHz(GPIOC, 0);    // U
    GPIO_Config_Output_PP_2MHz(GPIOC, 1);    // V
    GPIO_Config_Output_PP_2MHz(GPIOC, 2);    // W
    GPIO_Config_Output_PP_2MHz(GPIOC, 3);    // X
    GPIO_Config_Output_PP_2MHz(GPIOC, 4);    // Y
    GPIO_Config_Output_PP_2MHz(GPIOC, 5);    // Z

    // ACTIVE-LOW idle state = HIGH
    GPIOA->BSRR = (1U << 0) | (1U << 4) | (1U << 5) | (1U << 6) | (1U << 7) | (1U << 11) | (1U << 12);
    GPIOB->BSRR = (1U << 0) | (1U << 1) | (1U << 5) | (1U << 6) | (1U << 7) | (1U << 8) |
                  (1U << 9) | (1U << 10) | (1U << 11) | (1U << 12) | (1U << 13) | (1U << 14) | (1U << 15);
    GPIOC->BSRR = (1U << 0) | (1U << 1) | (1U << 2) | (1U << 3) | (1U << 4) | (1U << 5);
}

// ----------------------------------------------------------
// Convert PS/2 make/break scan code to letter index
//
// Returns:
// 0  -> A
// 1  -> B
// ...
// 25 -> Z
// -1 -> unsupported key
// ----------------------------------------------------------
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

// ----------------------------------------------------------
// Turn all letter outputs OFF
//
// ACTIVE-LOW:
// OFF = HIGH
// ----------------------------------------------------------
static void LED_All_Off(void)
{
    GPIOA->BSRR = (1U << 0) | (1U << 4) | (1U << 5) | (1U << 6) | (1U << 7) | (1U << 11) | (1U << 12);
    GPIOB->BSRR = (1U << 0) | (1U << 1) | (1U << 5) | (1U << 6) | (1U << 7) | (1U << 8) |
                  (1U << 9) | (1U << 10) | (1U << 11) | (1U << 12) | (1U << 13) | (1U << 14) | (1U << 15);
    GPIOC->BSRR = (1U << 0) | (1U << 1) | (1U << 2) | (1U << 3) | (1U << 4) | (1U << 5);
}

// ----------------------------------------------------------
// Turn all letter outputs ON
//
// ACTIVE-LOW:
// ON = LOW
// ----------------------------------------------------------
static void LED_All_On(void)
{
    GPIOA->BRR = (1U << 0) | (1U << 4) | (1U << 5) | (1U << 6) | (1U << 7) | (1U << 11) | (1U << 12);
    GPIOB->BRR = (1U << 0) | (1U << 1) | (1U << 5) | (1U << 6) | (1U << 7) | (1U << 8) |
                 (1U << 9) | (1U << 10) | (1U << 11) | (1U << 12) | (1U << 13) | (1U << 14) | (1U << 15);
    GPIOC->BRR = (1U << 0) | (1U << 1) | (1U << 2) | (1U << 3) | (1U << 4) | (1U << 5);
}

// ----------------------------------------------------------
// Apply the current letter-state mask to the outputs
//
// ACTIVE-LOW behavior:
// bit = 1 -> that letter pin is driven LOW
// bit = 0 -> that letter pin stays HIGH
// ----------------------------------------------------------
static void LED_ApplyMask(uint32_t mask)
{
    // Start with everything OFF / idle
    LED_All_Off();

    // Pull only the active letter outputs LOW
    if (mask & (1U << 0))  GPIO_Pin_Low(GPIOA, 0);    // A
    if (mask & (1U << 1))  GPIO_Pin_Low(GPIOA, 4);    // B
    if (mask & (1U << 2))  GPIO_Pin_Low(GPIOA, 5);    // C
    if (mask & (1U << 3))  GPIO_Pin_Low(GPIOA, 6);    // D
    if (mask & (1U << 4))  GPIO_Pin_Low(GPIOA, 7);    // E
    if (mask & (1U << 5))  GPIO_Pin_Low(GPIOA, 11);   // F
    if (mask & (1U << 6))  GPIO_Pin_Low(GPIOA, 12);   // G

    if (mask & (1U << 7))  GPIO_Pin_Low(GPIOB, 0);    // H
    if (mask & (1U << 8))  GPIO_Pin_Low(GPIOB, 1);    // I
    if (mask & (1U << 9))  GPIO_Pin_Low(GPIOB, 5);    // J
    if (mask & (1U << 10)) GPIO_Pin_Low(GPIOB, 6);    // K
    if (mask & (1U << 11)) GPIO_Pin_Low(GPIOB, 7);    // L
    if (mask & (1U << 12)) GPIO_Pin_Low(GPIOB, 8);    // M
    if (mask & (1U << 13)) GPIO_Pin_Low(GPIOB, 9);    // N
    if (mask & (1U << 14)) GPIO_Pin_Low(GPIOB, 10);   // O
    if (mask & (1U << 15)) GPIO_Pin_Low(GPIOB, 11);   // P
    if (mask & (1U << 16)) GPIO_Pin_Low(GPIOB, 12);   // Q
    if (mask & (1U << 17)) GPIO_Pin_Low(GPIOB, 13);   // R
    if (mask & (1U << 18)) GPIO_Pin_Low(GPIOB, 14);   // S
    if (mask & (1U << 19)) GPIO_Pin_Low(GPIOB, 15);   // T

    if (mask & (1U << 20)) GPIO_Pin_Low(GPIOC, 0);    // U
    if (mask & (1U << 21)) GPIO_Pin_Low(GPIOC, 1);    // V
    if (mask & (1U << 22)) GPIO_Pin_Low(GPIOC, 2);    // W
    if (mask & (1U << 23)) GPIO_Pin_Low(GPIOC, 3);    // X
    if (mask & (1U << 24)) GPIO_Pin_Low(GPIOC, 4);    // Y
    if (mask & (1U << 25)) GPIO_Pin_Low(GPIOC, 5);    // Z
}

// ----------------------------------------------------------
// Configure PA9 and PA10 for PS/2 keyboard input
//
// PA9  = clock input with EXTI on falling edge
// PA10 = data input
// ----------------------------------------------------------
static void PS2_GPIO_EXTI_Init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;

    // PA9 as floating input
    GPIOA->CRH &= ~(GPIO_CRH_MODE9 | GPIO_CRH_CNF9);
    GPIOA->CRH |= GPIO_CRH_CNF9_0;

    // PA10 as floating input
    GPIOA->CRH &= ~(GPIO_CRH_MODE10 | GPIO_CRH_CNF10);
    GPIOA->CRH |= GPIO_CRH_CNF10_0;

    // Map EXTI9 to PA9
    AFIO->EXTICR[2] &= ~AFIO_EXTICR3_EXTI9;

    // Interrupt on falling edge of PS/2 clock
    EXTI->IMR |= EXTI_IMR_MR9;
    EXTI->FTSR |= EXTI_FTSR_TR9;
    EXTI->RTSR &= ~EXTI_RTSR_TR9;
    EXTI->PR = EXTI_PR_PR9;

    NVIC_ClearPendingIRQ(EXTI9_5_IRQn);
    NVIC_EnableIRQ(EXTI9_5_IRQn);
}

// ----------------------------------------------------------
// Process one fully received PS/2 byte
//
// Handles:
// - 0xE0 extended code prefix
// - 0xF0 break code prefix
// - normal make/break for A-Z
// ----------------------------------------------------------
static void PS2_ProcessByte(uint8_t byte)
{
    int8_t ledIndex;

    // Extended scan code prefix
    if (byte == 0xE0)
    {
        ps2ExtendedCode = 1U;
        return;
    }

    // Break code prefix (key release)
    if (byte == 0xF0)
    {
        ps2BreakCode = 1U;
        return;
    }

    // Ignore extended keys for now
    if (!ps2ExtendedCode)
    {
        ledIndex = ScanCode_ToIndex(byte);

        if (ledIndex >= 0)
        {
            if (ps2BreakCode)
            {
                // Key released -> clear bit
                ledStateMask &= ~(1UL << (uint32_t)ledIndex);
            }
            else
            {
                // Key pressed -> set bit
                ledStateMask |= (1UL << (uint32_t)ledIndex);
            }

            // Only apply directly when flicker mode is not active
            if (!flickerModeEnabled)
            {
                LED_ApplyMask(ledStateMask);
            }
        }
    }

    // Clear prefixes after handling this byte
    ps2BreakCode = 0U;
    ps2ExtendedCode = 0U;
}

// ----------------------------------------------------------
// Public initialization function
// ----------------------------------------------------------
void PS2_Keyboard_Init(void)
{
    LED_GPIO_Init();
    PS2_GPIO_EXTI_Init();

    // SysTick used for flicker timing
    SystemCoreClockUpdate();
    SysTick_Config(SystemCoreClock / 10U);   // 100 ms period
}

// ----------------------------------------------------------
// Enable or disable flicker mode
//
// enable = 1 -> all outputs flicker
// enable = 0 -> restore outputs according to pressed keys
// ----------------------------------------------------------
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

// ----------------------------------------------------------
// SysTick interrupt handler for flicker mode
// ----------------------------------------------------------
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

// ----------------------------------------------------------
// EXTI handler for PS/2 clock on PA9
//
// Reads data bit from PA10 on every falling clock edge.
// Frame format:
// start(0), 8 data bits LSB first, parity, stop(1)
// ----------------------------------------------------------
void EXTI9_5_IRQHandler(void)
{
    uint8_t bitValue;

    if (EXTI->PR & EXTI_PR_PR9)
    {
        // Clear interrupt pending flag
        EXTI->PR = EXTI_PR_PR9;

        // Sample data line
        bitValue = (GPIOA->IDR & GPIO_IDR_IDR10) ? 1U : 0U;

        // Waiting for start bit
        if (ps2BitCount == 0U)
        {
            if (bitValue == 0U)
            {
                ps2BitCount = 1U;
                ps2DataByte = 0U;
            }
        }
        // Receive 8 data bits, LSB first
        else if ((ps2BitCount >= 1U) && (ps2BitCount <= 8U))
        {
            if (bitValue)
            {
                ps2DataByte |= (uint8_t)(1U << (ps2BitCount - 1U));
            }

            ps2BitCount++;
        }
        // Parity bit, ignored
        else if (ps2BitCount == 9U)
        {
            ps2BitCount++;
        }
        // Stop bit
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