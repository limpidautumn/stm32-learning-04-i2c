#include "aht20.hpp"
#include "i2c.h"
#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_def.h"
#include "stm32f1xx_hal_i2c.h"
#include <cstdint>
#include <cstring>

namespace aht20 {

constexpr uint8_t addr = 0x70;
double tp, rh;

uint8_t xmit_buf[256];
const uint8_t meas_cmd[] = {0xAC, 0x33, 0x00};
const uint8_t meas_cmd_size = sizeof(meas_cmd);
volatile uint8_t xmit_done = 1;

uint8_t recv_buf[256];
const uint8_t meas_data_size = 7;
volatile uint8_t recv_done = 1;

uint8_t calc_crc(const uint8_t data[], const uint8_t size) {
  uint8_t crc = 0xff;
  for (uint8_t cur = 0; cur < size; ++cur) {
    crc ^= data[cur];
    for (uint8_t i = 8; i > 0; --i) {
      if (crc & 0x80)
        crc = (crc << 1) ^ 0x31;
      else
        crc = (crc << 1);
    }
  }
  return crc;
}

HAL_StatusTypeDef trig_meas() {
  xmit_done = 0;
  std::memcpy(xmit_buf, meas_cmd, sizeof(meas_cmd));
  return HAL_I2C_Master_Transmit_DMA(&hi2c1, addr, xmit_buf, meas_cmd_size);
}

HAL_StatusTypeDef fetch_data() {
  recv_done = 0;
  return HAL_I2C_Master_Receive_DMA(&hi2c1, addr, recv_buf, meas_data_size);
}

uint8_t parse_data(double &temperature, double &relative_humidity) {
  if (calc_crc(recv_buf, 6) != recv_buf[6])
    return 0;
  if (recv_buf[0] & 0x80)
    return 0;

  const uint32_t S_RH = (recv_buf[3] >> 4) + (uint32_t(recv_buf[2]) << 4) +
                        (uint32_t(recv_buf[1]) << 12);
  relative_humidity = 100.0 * S_RH / (1 << 20);

  const uint32_t S_T = recv_buf[5] + (uint32_t(recv_buf[4]) << 8) +
                       (uint32_t(recv_buf[3] & 0x0F) << 16);
  temperature = 200.0 * S_T / (1 << 20) - 50;

  return 1;
}

// State machine for reading TP & RH from the sensor
enum class StatusEnum : uint8_t {
  Init,                // 0. Sensor initialization
  Idle,                // 1. Reading complete; idle
  SendingRequest,      // 2. Sending measurement request
  AwaitingMeasurement, // 3. Request sent; waiting for measurement
  ReadingData,         // 4. Measurement complete; reading measurement data
};

class SensorStatus {
private:
  uint32_t tick = 0;

public:
  StatusEnum status = StatusEnum::Init;

  // Get the elapsed time
  uint32_t et() { return HAL_GetTick() - tick; }

  // Set the status and update the timestamp
  void set(StatusEnum new_status) {
    status = new_status;
    tick = HAL_GetTick();
  }
};

constexpr uint32_t init_time = 5;
constexpr uint32_t meas_time = 80;
constexpr uint32_t xmit_timeout = 100;
constexpr uint32_t recv_timeout = 100;

void advanceStatus(SensorStatus &cur) {
  switch (cur.status) {

  case StatusEnum::Init:
    // Conditions
    if (cur.et() < init_time)
      break;

    // Actions

    // Update status
    cur.set(StatusEnum::Idle);
    break;

  case StatusEnum::Idle:
    // Conditions

    // Actions
    trig_meas();

    // Update status
    cur.set(StatusEnum::SendingRequest);
    break;

  case StatusEnum::SendingRequest:
    // Conditions
    if (cur.et() > xmit_timeout) {
      cur.set(StatusEnum::Init);
      break;
    }
    if (!xmit_done)
      break;

    // Actions

    // Update status
    cur.set(StatusEnum::AwaitingMeasurement);
    break;

  case StatusEnum::AwaitingMeasurement:
    // Conditions
    if (cur.et() < meas_time)
      break;

    // Actions
    fetch_data();

    // Update status
    cur.set(StatusEnum::ReadingData);
    break;

  case StatusEnum::ReadingData:
    // Conditions
    if (cur.et() > recv_timeout) {
      cur.set(StatusEnum::Init);
      break;
    }
    if (!recv_done)
      break;

    // Actions
    parse_data(tp, rh);

    // Update status
    cur.set(StatusEnum::Idle);
    break;

  default:
    HAL_NVIC_SystemReset();
    break;
  }
}

SensorStatus status;

void setup() { status.set(StatusEnum::Init); }

void loop() { advanceStatus(status); }

} // namespace aht20

extern "C" void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c) {
  if (hi2c == &hi2c1) {
    aht20::xmit_done = 1;
  }
}

extern "C" void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c) {
  if (hi2c == &hi2c1) {
    aht20::recv_done = 1;
  }
}
