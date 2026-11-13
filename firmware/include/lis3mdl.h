#pragma once

#include <Wire.h>
#include <cstdint>

/*
 * ST LIS3MDL (Adafruit #4022 STEMMA QT / Qwiic magnetometer).
 * Default 7-bit address 0x1C (SA1 low) or 0x1E (SA1 high).
 */

struct MagSample {
  bool valid;
  float x_uT;
  float y_uT;
  float z_uT;
};

class Lis3mdl {
public:
  explicit Lis3mdl(TwoWire &wire, uint8_t addr = 0x1C);

  bool begin();
  bool whoAmI(uint8_t *out = nullptr);
  MagSample read();

private:
  TwoWire &wire_;
  uint8_t addr_;
  float scale_uT_per_lsb_ = 0.684f; /* ±16 gauss default after cfg */

  bool writeReg(uint8_t reg, uint8_t val);
  bool readRegs(uint8_t reg, uint8_t *buf, uint8_t len);
};
