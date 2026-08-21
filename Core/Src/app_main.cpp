#include "app_main.hpp"
#include "aht20.hpp"

#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_uart.h"
#include "usart.h"
#include <cstdio>
#include <cstring>

uint8_t xmit_buf[256];
uint32_t lst = 0; //

void cpp_setup() {
  aht20::setup();
  snprintf((char *)xmit_buf, sizeof(xmit_buf), "Ready.");
  HAL_UART_Transmit_IT(&huart2, xmit_buf, strlen((char *)xmit_buf));

  lst = HAL_GetTick();
}

void cpp_loop() {
  aht20::loop();

  if (HAL_GetTick() - lst < 1000)
    return;

  lst = HAL_GetTick();
  snprintf((char *)xmit_buf, sizeof(xmit_buf), "100tp: %u, 100rh: %u",
           unsigned(100 * aht20::tp), unsigned(100 * aht20::rh));
  HAL_UART_Transmit_IT(&huart2, xmit_buf, strlen((char *)xmit_buf));
}
