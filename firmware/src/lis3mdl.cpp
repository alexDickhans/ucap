#include "lis3mdl.h"

/* Register map (ST LIS3MDL datasheet). */
namespace {
constexpr uint8_t REG_WHO_AM_I = 0x0F;
constexpr uint8_t WHO_AM_I_VAL = 0x3D;
constexpr uint8_t REG_CTRL1    = 0x20;
constexpr uint8_t REG_CTRL2    = 0x21;
constexpr uint8_t REG_CTRL3    = 0x22;
constexpr uint8_t REG_CTRL4    = 0x23;
constexpr uint8_t REG_CTRL5    = 0x24;
constexpr uint8_t REG_OUT_X_L  = 0x28;
} // namespace

Lis3mdl::Lis3mdl(TwoWire &wire, uint8_t addr) : wire_(wire), addr_(addr) {}

bool Lis3mdl::writeReg(uint8_t reg, uint8_t val) {
  wire_.beginTransmission(addr_);
  wire_.write(reg);
  wire_.write(val);
  return wire_.endTransmission() == 0;
}

bool Lis3mdl::readRegs(uint8_t reg, uint8_t *buf, uint8_t len) {
  wire_.beginTransmission(addr_);
  wire_.write(reg | 0x80); /* auto-increment */
  if (wire_.endTransmission(false) != 0) {
    return false;
  }
  const uint8_t n = wire_.requestFrom(addr_, len);
  if (n != len) {
    return false;
  }
  for (uint8_t i = 0; i < len; ++i) {
    buf[i] = static_cast<uint8_t>(wire_.read());
  }
  return true;
}

bool Lis3mdl::whoAmI(uint8_t *out) {
  uint8_t id = 0;
  if (!readRegs(REG_WHO_AM_I, &id, 1)) {
    return false;
  }
  if (out) {
    *out = id;
  }
  return id == WHO_AM_I_VAL;
}

bool Lis3mdl::begin() {
  if (!whoAmI()) {
    return false;
  }
  /* CTRL2: FS = ±16 gauss (0b11 << 5). Sensitivity 1711 LSB/gauss. */
  if (!writeReg(REG_CTRL2, 0x60)) {
    return false;
  }
  scale_uT_per_lsb_ = 1000.0f / 1711.0f;

  /* CTRL1: temp enable, XY ultra-high-performance, 80 Hz ODR (0x02<<2? OM=11 UHP, DO=111 80Hz) */
  /* TEMP_EN | OM[1:0]=11 | DO[2:0]=111 | FAST_ODR=0 | ST=0 → 0xFE */
  if (!writeReg(REG_CTRL1, 0xFE)) {
    return false;
  }
  /* CTRL4: Z UHP OMZ=11 */
  if (!writeReg(REG_CTRL4, 0x0C)) {
    return false;
  }
  /* CTRL5: BDU continuous update block until MSB+LSB read */
  if (!writeReg(REG_CTRL5, 0x40)) {
    return false;
  }
  /* CTRL3: continuous-conversion MD=00 */
  if (!writeReg(REG_CTRL3, 0x00)) {
    return false;
  }
  return true;
}

MagSample Lis3mdl::read() {
  MagSample s{};
  uint8_t raw[6];
  if (!readRegs(REG_OUT_X_L, raw, 6)) {
    return s;
  }
  const auto s16 = [](uint8_t lo, uint8_t hi) -> int16_t {
    return static_cast<int16_t>((static_cast<uint16_t>(hi) << 8) | lo);
  };
  const int16_t x = s16(raw[0], raw[1]);
  const int16_t y = s16(raw[2], raw[3]);
  const int16_t z = s16(raw[4], raw[5]);
  s.x_uT = x * scale_uT_per_lsb_;
  s.y_uT = y * scale_uT_per_lsb_;
  s.z_uT = z * scale_uT_per_lsb_;
  s.valid = true;
  return s;
}
