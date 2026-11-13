#pragma once

#include "board_pins.h"
#include <Wire.h>

/*
 * Four hardware I2C peripherals, one per Qwiic socket (J3–J6).
 * STM32duino TwoWire(SDA, SCL) binds pins; AF selected by the core.
 */

struct UcapI2cBuses {
  TwoWire bus0; /* J3 I2C0 → I2C1: PB9/PA15 */
  TwoWire bus1; /* J4 I2C1 → I2C2: PF0/PC4 */
  TwoWire bus2; /* J5 I2C2 → I2C3: PC11/PA8 */
  TwoWire bus3; /* J6 I2C3 → I2C4: PB7/PC6 */

  UcapI2cBuses()
      : bus0(PIN_I2C0_SDA, PIN_I2C0_SCL),
        bus1(PIN_I2C1_SDA, PIN_I2C1_SCL),
        bus2(PIN_I2C2_SDA, PIN_I2C2_SCL),
        bus3(PIN_I2C3_SDA, PIN_I2C3_SCL) {}

  void beginAll(uint32_t hz = 400000);
  TwoWire &operator[](unsigned index);
};

TwoWire &ucap_bus(unsigned index);
void ucap_i2c_init();
UcapI2cBuses &ucap_i2c();
