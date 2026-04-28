# Stranger Things Light Communication System

An embedded systems project inspired by the light wall from *Stranger Things*.  
The system uses a PS/2 keyboard, 26 letter LEDs, a pushbutton, and a speaker to create an interactive light-and-sound communication display.

## Project Overview

This project was built using an STM32F103RB microcontroller. Each alphabet key on a PS/2 keyboard is mapped to a physical LED output representing the letters A-Z. When a key is pressed, the matching letter LED activates. When the key is released, the LED turns off.

The project also includes a special scare/effect mode. A pushbutton can trigger a flickering light pattern and an eerie speaker sound. The system also detects the typed sequence `RUN`, which starts a sound effect immediately and activates the light flicker after a delay.

## Main Features

- PS/2 keyboard input using external interrupts
- A-Z letter detection using PS/2 Set 2 scan codes
- 26 LED outputs mapped to alphabet letters
- Active-low LED output behavior
- Pushbutton-triggered flicker and speaker effect
- `RUN` keyboard sequence trigger
- `ESC` key cancels the RUN-triggered effect
- PWM speaker output using TIM1 Channel 1
- SysTick-based timing for flicker and sound pattern updates
- Low-power main loop using `__WFI()` while waiting for interrupts

## Hardware Used

- STM32F103RB microcontroller board
- PS/2 keyboard
- 26 LEDs for alphabet display
- Current-limiting resistors for LEDs
- Pushbutton
- Speaker or small audio amplifier module
- Breadboard / wiring setup
- External power source as required by the hardware setup

## Software and Tools

- Keil µVision
- ARM Compiler / ARMCLANG
- STM32F1xx device support pack
- CMSIS / STM32F10x device headers
- C programming language
- STM32 register-level programming

## Repository Structure

```text
StrangerThings-Light-Communication-System/
│
├── Code/
│   ├── main.c
│   ├── button.c
│   ├── button.h
│   ├── ps2_keyboard.c
│   ├── ps2_keyboard.h
│   ├── speaker.c
│   ├── speaker.h
│   ├── ENEL351Project.uvprojx
│   ├── ENEL351Project.uvoptx
│   ├── RTE/
│   ├── Objects/
│   ├── Listings/
│   └── DebugConfig/
│
├── Datasheets/
│
├── Schematic.pdf
│
└── README.md
