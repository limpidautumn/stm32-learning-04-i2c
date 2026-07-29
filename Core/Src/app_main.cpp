#include "app_main.hpp"
#include "aht20.hpp"

#include "usart.h"
#include <cstdio>
#include <cstring>

uint8_t xmit_buf[256];

void cpp_setup() {
  aht20::setup();
  snprintf((char *)xmit_buf, sizeof(xmit_buf), "Ready.");
  HAL_UART_Transmit_IT(&huart2, xmit_buf, strlen((char *)xmit_buf));
}

void cpp_loop() {
  double tp, rh;
  uint8_t aht20_status;
  aht20_status = aht20::read(tp, rh);

  snprintf((char *)xmit_buf, sizeof(xmit_buf),
           "status: %u, 100tp: %u, 100rh: %u", aht20_status, unsigned(100 * tp),
           unsigned(100 * rh));
  HAL_UART_Transmit_IT(&huart2, xmit_buf, strlen((char *)xmit_buf));

  HAL_GPIO_TogglePin(LED_R_GPIO_Port, LED_R_Pin);
  HAL_Delay(1000);
}
