#pragma once

#include "board_pins.h"
#include <SparkFun_VL53L5CX_Library.h>
#include <Wire.h>
#include <cstdint>

/*
 * SparkFun Qwiic Mini ToF Imager — VL53L5CX
 * https://www.sparkfun.com/sparkfun-qwiic-mini-tof-imager-vl53l5cx.html
 *
 * Up to three units, each on its own I2C bus (I2C1/I2C2/I2C3 sockets =
 * firmware buses 1/2/3). Default 7-bit address 0x29.
 */

struct TofFrame {
  bool valid;
  uint8_t resolution; /* 16 (4x4) or 64 (8x8) */
  int16_t distance_mm[64];
  uint8_t status[64];
};

class TofBank {
public:
  static constexpr unsigned kMax = UCAP_TOF_MAX;

  void begin(TwoWire *buses[kMax]);
  unsigned presentCount() const { return present_count_; }
  bool present(unsigned idx) const;
  bool poll(unsigned idx, TofFrame &out);

private:
  SparkFun_VL53L5CX sensors_[kMax];
  bool online_[kMax] = {};
  bool ranging_[kMax] = {};
  unsigned present_count_ = 0;
};
