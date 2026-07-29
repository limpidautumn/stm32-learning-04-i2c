#pragma once

#include "i2c.h"

namespace aht20 {

void setup();
uint8_t read(double &, double &);

}; // namespace aht20
