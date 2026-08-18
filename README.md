# stm32-learning-04-i2c

STM32 Practice Code 04 - I2C Master Read & Write

[English](README.md) | [中文](README.zh-CN.md)

## Overview

This firmware runs on an STM32F103C8T6 and reads an AHT20 temperature and humidity sensor over I2C1 without ever blocking the CPU: I2C transfers are DMA-driven and the whole measurement is expressed as a **state machine**. Once per second it reports the latest readings over USART2 at 115200 baud, e.g. `100tp: 2345, 100rh: 5512` (temperature and relative humidity, scaled by 100).

The sensor sits on I2C1 at 7-bit address `0x70` (SCL = PB6, SDA = PB7, 100 kHz); UART debug output is on PA2 (TX) / PA3 (RX). The MCU runs from the internal 8 MHz HSI with no PLL. The project follows the [Keysking's STM32 tutorial](https://space.bilibili.com/6100925/lists/1025423), and runs on [his development board](https://docs.keysking.com/docs/stm32/resourcePack/).

## Features / Implementation Details

- **Non-blocking state machine** (the core idea): the AHT20 read is split into five states — `Init`, `Idle`, `SendingRequest`, `AwaitingMeasurement`, `ReadingData` (`enum class StatusEnum`) — advanced one step per loop by `aht20::advanceStatus()`. Each state either kicks off an asynchronous transfer or waits for a completion flag or timeout, so `main()` never blocks.
- **I2C1 over DMA**: the trigger command is sent with `HAL_I2C_Master_Transmit_DMA` (TX = DMA1 channel 6) and the 7-byte result is read with `HAL_I2C_Master_Receive_DMA` (RX = DMA1 channel 7). The `HAL_I2C_MasterTxCpltCallback` / `RxCpltCallback` set volatile `xmit_done` / `recv_done` flags that the state machine polls.
- **Tick-based transitions & timeouts**: a `SensorStatus` helper stores the entry timestamp; `et()` measures elapsed time via `HAL_GetTick()`. Each state has a timeout (init 5 ms, measurement 80 ms, TX/RX 100 ms), so a stuck bus recovers by resetting to `Init`.
- **I2C1 master at 100 kHz, 7-bit addressing**, open-drain alternate function on PB6/PB7, clock stretching disabled.
- **Software CRC-8 check**: a CRC-8 (polynomial `0x31`, init `0xFF`) over the first 6 bytes must equal byte 7, and the busy bit (bit 7 of the status byte) must be clear, otherwise the sample is rejected.
- **Raw-value decoding**: the 20-bit `S_RH` and `S_T` fields are converted to relative humidity (percent) and temperature (°C).
- **Non-blocking UART TX**: results are formatted with `snprintf` into a 256-byte buffer and sent via `HAL_UART_Transmit_IT` every 1000 ms (driven by `HAL_GetTick()`).
- **C++ app layer over CubeMX C code**: `main.c` only calls `cpp_setup()` / `cpp_loop()` (declared in `app_main.hpp`); application logic lives in `app_main.cpp` and the driver in `aht20.cpp` (an `aht20` namespace).

## Code Layout

Application logic lives in `Core/Src` (CubeMX-generated) plus a small C++ layer. The call flow is:

```text
main()
├── HAL_Init() / SystemClock_Config()   # 8 MHz HSI, no PLL
├── MX_GPIO_Init()                      # RGB LED outputs (unused in this version)
├── MX_DMA_Init()                       # DMA1 clock + channel 6/7 IRQs
├── MX_I2C1_Init()                      # I2C1 @100 kHz + DMA channels, PB6/PB7
├── MX_USART2_UART_Init()               # USART2 @115200, PA2/PA3
└── while (1)
    ├── cpp_setup()                     # aht20::setup(); send "Ready."
    └── cpp_loop()                      # aht20::loop(); print every 1000 ms
        └── aht20::loop() → advanceStatus()   # step the state machine once
            ├── trig_meas()             # send {0xAC,0x33,0x00} via I2C TX DMA
            ├── fetch_data()            # read 7 bytes via I2C RX DMA
            └── parse_data()            # CRC check + decode S_RH / S_T
```

## Learning Objectives / Notes

- How to replace a blocking `HAL_I2C_Master_Transmit/Receive` + `HAL_Delay` loop with a **polled, DMA-driven state machine** that never stalls the main loop.
- Modeling a sensor transaction as explicit states and transitions (request → wait → read → decode), with per-state timeouts for error recovery.
- Wiring DMA to I2C1 and handling completion through HAL callbacks.
- Reading a real sensor datasheet: command sequence, status/busy flag, and 20-bit data extraction; implementing a software CRC-8.
- Bridging CubeMX C code with a C++ application layer (C++17, `extern "C"` wrappers).
- No RTOS is used — the "concurrency" comes purely from the state machine plus DMA/interrupt completion flags.

## Other Implementations

- [`aht20-i2c-state-machine`](https://github.com/limpidautumn/stm32-learning-04-i2c/tree/aht20-i2c-state-machine) — the non-blocking DMA state machine documented above.
- [`aht20-i2c-test`](https://github.com/limpidautumn/stm32-learning-04-i2c/tree/aht20-i2c-test) — an earlier blocking/polling version using `HAL_I2C_Master_Transmit/Receive` + `HAL_Delay`, kept for comparison.
