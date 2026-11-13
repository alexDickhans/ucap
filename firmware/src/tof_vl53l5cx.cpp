#include "tof_vl53l5cx.h"
#include "dma_copy.h"

void TofBank::begin(TwoWire *buses[kMax]) {
  present_count_ = 0;
  for (unsigned i = 0; i < kMax; ++i) {
    online_[i] = false;
    ranging_[i] = false;
    if (!buses[i]) {
      continue;
    }
    /* SparkFun default 7-bit address is (DEFAULT_I2C_ADDR >> 1) == 0x29. */
    if (!sensors_[i].begin(0x29, *buses[i])) {
      continue;
    }
    if (!sensors_[i].isConnected()) {
      continue;
    }
    /* Prefer 4x4 @ higher rate for USB telemetry bandwidth; 8x8 optional later. */
    sensors_[i].setResolution(4 * 4);
    sensors_[i].setRangingFrequency(15);
    if (!sensors_[i].startRanging()) {
      continue;
    }
    online_[i] = true;
    ranging_[i] = true;
    ++present_count_;
  }
}

bool TofBank::present(unsigned idx) const {
  return idx < kMax && online_[idx];
}

bool TofBank::poll(unsigned idx, TofFrame &out) {
  out = {};
  if (!present(idx) || !ranging_[idx]) {
    return false;
  }
  if (!sensors_[idx].isDataReady()) {
    return false;
  }
  VL53L5CX_ResultsData data{};
  if (!sensors_[idx].getRangingData(&data)) {
    return false;
  }
  const uint8_t res = sensors_[idx].getResolution();
  out.resolution = res;
  const unsigned n = (res == 64) ? 64u : 16u;
  dma_copy(out.distance_mm, data.distance_mm, n * sizeof(int16_t));
  dma_copy(out.status, data.target_status, n * sizeof(uint8_t));
  out.valid = true;
  return true;
}
