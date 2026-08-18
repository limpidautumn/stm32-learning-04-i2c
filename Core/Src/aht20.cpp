#include "aht20.hpp"
#include "i2c.h"
#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_def.h"
#include "stm32f1xx_hal_i2c.h"
#include <cstdint>

namespace aht20 {

const uint8_t address = 0x70;

void setup() { HAL_Delay(5); }

uint8_t calc_crc(const uint8_t recv_buf[], const uint8_t size) {
  uint8_t crc = 0xff;
  for (uint8_t cur = 0; cur < size; ++cur) {
    crc ^= recv_buf[cur];
    for (uint8_t i = 8; i > 0; --i) {
      if (crc & 0x80)
        crc = (crc << 1) ^ 0x31;
      else
        crc = (crc << 1);
    }
  }
  return crc;
}

uint8_t read(double &temperature, double &relative_humidity) {
  uint8_t xmit_buf[] = {0xAC, 0x33, 0x00};
  HAL_I2C_Master_Transmit(&hi2c1, address, xmit_buf, sizeof(xmit_buf),
                          HAL_MAX_DELAY);
  HAL_Delay(80);

  uint8_t recv_buf[7] = {};
  HAL_I2C_Master_Receive(&hi2c1, address, recv_buf, sizeof(recv_buf),
                         HAL_MAX_DELAY);

  if (calc_crc(recv_buf, 6) != recv_buf[6])
    return 0;
  if (recv_buf[0] & 0x80)
    return 0;

  const uint32_t S_RH = (recv_buf[3] >> 4) + (uint32_t(recv_buf[2]) << 4) +
                        (uint32_t(recv_buf[1]) << 12);
  relative_humidity = 1.0 * S_RH / (1 << 20);

  const uint32_t S_T = recv_buf[5] + (uint32_t(recv_buf[4]) << 8) +
                       (uint32_t(recv_buf[3] & 0x0F) << 16);
  temperature = 200.0 * S_T / (1 << 20) - 50;

  return 1;
}

}; // namespace aht20
