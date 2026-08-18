# stm32-learning-04-i2c

STM32 练习代码 04 - I2C 主机读写

[English](README.md) | [中文](README.zh-CN.md)

## 概述

本固件运行于 STM32F103C8T6 上，通过 I2C1 读取 AHT20 温湿度传感器，且全程不阻塞 CPU：I2C 传输由 DMA 驱动，整个测量过程用一个**状态机**表达。每秒通过 USART2（115200 波特率）上报一次最新读数，例如 `100tp: 2345, 100rh: 5512`（温度与相对湿度，放大了 100 倍）。

传感器挂在 I2C1 上，7 位地址 `0x70`（SCL = PB6、SDA = PB7、100 kHz）；UART 调试输出在 PA2（TX）/ PA3（RX）。MCU 使用内部 8 MHz HSI 时钟、无 PLL。本项目基于 [Keysking 的 STM32 教程](https://space.bilibili.com/6100925/lists/1025423)，在 [他的开发板](https://docs.keysking.com/docs/stm32/resourcePack/) 上运行。

## 特性 / 实现细节

- **非阻塞状态机**（核心思想）：将 AHT20 的读取拆分为五个状态 —— `Init`、`Idle`、`SendingRequest`、`AwaitingMeasurement`、`ReadingData`（`enum class StatusEnum`）—— 由 `aht20::advanceStatus()` 每循环推进一步。每个状态要么启动一次异步传输，要么等待完成标志或超时，因此 `main()` 从不阻塞。
- **I2C1 使用 DMA**：触发命令用 `HAL_I2C_Master_Transmit_DMA` 发送（TX = DMA1 通道 6），7 字节结果用 `HAL_I2C_Master_Receive_DMA` 读取（RX = DMA1 通道 7）。`HAL_I2C_MasterTxCpltCallback` / `RxCpltCallback` 置位易失标志 `xmit_done` / `recv_done`，由状态机轮询。
- **基于 tick 的迁移与超时**：`SensorStatus` 辅助类保存进入状态的时间戳，`et()` 通过 `HAL_GetTick()` 计算已过时间。每个状态都有超时（初始化 5 ms、测量 80 ms、TX/RX 100 ms），总线卡死时通过回到 `Init` 恢复。
- **I2C1 主机 100 kHz、7 位寻址**，PB6/PB7 开漏复用，关闭时钟拉伸。
- **软件 CRC-8 校验**：对前 6 字节做 CRC-8（多项式 `0x31`、初值 `0xFF`），结果必须等于第 7 字节，且忙标志位（状态字节 bit 7）必须为 0，否则丢弃该采样。
- **原始值解码**：将 20 位 `S_RH` 与 `S_T` 字段换算为相对湿度（百分比）和温度（°C）。
- **非阻塞 UART 发送**：结果用 `snprintf` 格式化到 256 字节缓冲区，每隔 1000 ms 通过 `HAL_UART_Transmit_IT` 发送（由 `HAL_GetTick()` 驱动）。
- **基于 CubeMX C 代码之上的 C++ 应用层**：`main.c` 只调用 `cpp_setup()` / `cpp_loop()`（在 `app_main.hpp` 中声明）；应用逻辑放在 `app_main.cpp`，驱动放在 `aht20.cpp`（`aht20` 命名空间）。

## 代码结构

应用逻辑位于 `Core/Src`（CubeMX 生成）外加一个很小的 C++ 层。调用流程如下：

```text
main()
├── HAL_Init() / SystemClock_Config()   # 8 MHz HSI，无 PLL
├── MX_GPIO_Init()                      # RGB 灯输出（本版本未使用）
├── MX_DMA_Init()                       # DMA1 时钟 + 通道 6/7 中断
├── MX_I2C1_Init()                      # I2C1 @100 kHz + DMA 通道，PB6/PB7
├── MX_USART2_UART_Init()               # USART2 @115200，PA2/PA3
└── while (1)
    ├── cpp_setup()                     # aht20::setup()；发送 "Ready."
    └── cpp_loop()                      # aht20::loop()；每 1000 ms 打印
        └── aht20::loop() → advanceStatus()   # 推进一次状态机
            ├── trig_meas()             # 通过 I2C TX DMA 发送 {0xAC,0x33,0x00}
            ├── fetch_data()            # 通过 I2C RX DMA 读取 7 字节
            └── parse_data()            # CRC 校验 + 解码 S_RH / S_T
```

## 学习目标 / 说明

- 如何用**轮询式、DMA 驱动的状态机**替代阻塞的 `HAL_I2C_Master_Transmit/Receive` + `HAL_Delay` 循环，使主循环永不停顿。
- 将一次传感器事务建模为明确的状态与迁移（请求 → 等待 → 读取 → 解码），并为每个状态设置超时以便错误恢复。
- 将 DMA 接到 I2C1，并通过 HAL 回调处理完成事件。
- 阅读真实传感器数据手册：命令时序、状态/忙标志以及 20 位数据提取；用软件实现 CRC-8。
- 将 CubeMX 的 C 代码与 C++ 应用层（C++17、`extern "C"` 包装）结合。
- 未使用 RTOS —— "并发"完全来自状态机加 DMA/中断完成标志。

## 其他实现

- [`aht20-i2c-state-machine`](https://github.com/limpidautumn/stm32-learning-04-i2c/tree/aht20-i2c-state-machine) —— 上文所述的非阻塞 DMA 状态机。
- [`aht20-i2c-test`](https://github.com/limpidautumn/stm32-learning-04-i2c/tree/aht20-i2c-test) —— 早期的阻塞/轮询版本，使用 `HAL_I2C_Master_Transmit/Receive` + `HAL_Delay`，保留以供对比。
