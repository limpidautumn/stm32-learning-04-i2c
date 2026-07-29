#include "app_main.hpp"
#include "gpio.h"
#include "i2c.h"
#include "main.h"
#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_gpio.h"
#include "usart.h"

void cpp_setup() { ; }

void cpp_loop() {
  HAL_GPIO_TogglePin(LED_R_GPIO_Port, LED_R_Pin);
  HAL_Delay(100);
}
