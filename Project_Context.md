# Project Context

## Hardware Configuration
- **MCU**: STM32F407ZGT6 (ARM Cortex-M4)
- **Audio**: ES8388 Codec (I2C1 Control, I2S2 Data)
- **Display**: LCD (FSMC Interface)
- **Touch**: Resistive/Capacitive (I2C/SPI via GPIO simulation)
- **Storage**: MicroSD Card (SDIO Interface)
- **LED**: GPIOF Pin 9

## Software Stack
- **IDE**: STM32CubeIDE
- **Framework**: STM32 HAL Library
- **GUI**: LVGL v9.4.0
- **File System**: FatFS
- **RTOS**: None (Currently Bare Metal)

## Directory Structure
- `Core/Inc`, `Core/Src`: System initialization (HAL, Peripherals).
- `Core/Gui`: LVGL library and porting layer.
- `Core/Touch`: Touch screen drivers.
- `Core/App`: Application logic (New!).
    - `GUI`: UI application code (`gui_app.c`).
    - `Player`: Music player logic (`music_player.c`).

## Current Status
- **Music Player**: Can play WAV files from SD card using DMA.
- **GUI**: LVGL initialized, "Hello LVGL" button displayed.
- **Touch**: Touch driver working, integrated with LVGL.
- **Refactoring**: Code moved from `main.c` to `Core/App` modules.

## Known Issues
- Mouse cursor was visible (fixed by disabling mouse in `lv_port_indev.c`).
- `main.c` was cluttered (fixed by refactoring).
