#include "adc_diag.h"
#include "board_pins.h"
#include "bus_i2c.h"
#include "cap_sense.h"
#include "dma_copy.h"
#include "lis3mdl.h"
#include "stat_led.h"
#include "tof_vl53l5cx.h"

#include <Arduino.h>

/*
 * uCap v1 firmware
 *
 * On-chip blocks exercised (only where the PCB ties pins in):
 *   COMP1 + TIM2  — capacitance (PA1/PA4/PA0)
 *   ADC1/ADC2     — diagnostic samples of sense / v_th
 *   I2C1..I2C4    — four Qwiic sockets
 *   USB FS CDC    — telemetry (PA11/PA12)
 *   GPIO          — STAT LED (PA5)
 *
 * External modules:
 *   LIS3MDL on I2C0 (J3)
 *   VL53L5CX × up to 3 on I2C1..I2C3 (J4..J6)
 */

namespace {
Lis3mdl *g_mag = nullptr;
TofBank g_tof;
uint32_t g_last_print_ms = 0;
} // namespace

void setup() {
  Serial.begin(115200);
  const uint32_t t0 = millis();
  while (!Serial && (millis() - t0) < 2000) {
    /* USB CDC enumerate */
  }

  Serial.println(F("uCap v1"));
  Serial.println(F("pins: PA0 charge, PA1 COMP1_INP, PA4 COMP1_INM, PA5 LED"));

  stat_led_init();
  stat_led_set(true);

  cap_sense_init();
  adc_diag_init();
  dma_copy_init();
  ucap_i2c_init();

  static Lis3mdl mag(ucap_bus(BUS_MAG));
  g_mag = &mag;
  if (g_mag->begin()) {
    Serial.println(F("LIS3MDL ok on I2C0 (J3)"));
  } else {
    Serial.println(F("LIS3MDL missing on I2C0"));
  }

  TwoWire *tof_buses[TofBank::kMax] = {
      &ucap_bus(BUS_TOF0),
      &ucap_bus(BUS_TOF1),
      &ucap_bus(BUS_TOF2),
  };
  g_tof.begin(tof_buses);
  Serial.print(F("VL53L5CX present: "));
  Serial.println(g_tof.presentCount());

  stat_led_set(false);
}

void loop() {
  CapSample cap = cap_sense_measure();
  AdcDiagSample adc = adc_diag_read();
  MagSample mag{};
  if (g_mag) {
    mag = g_mag->read();
  }

  TofFrame frames[TofBank::kMax];
  for (unsigned i = 0; i < TofBank::kMax; ++i) {
    (void)g_tof.poll(i, frames[i]);
  }

  const uint32_t now = millis();
  if (now - g_last_print_ms >= 200) {
    g_last_print_ms = now;
    stat_led_toggle();

    Serial.print(F("C_fF="));
    if (cap.valid) {
      Serial.print(cap.capacitance_f * 1e15f, 1);
      Serial.print(F(" t_us="));
      Serial.print(cap.time_s * 1e6f, 2);
    } else {
      Serial.print(F("timeout"));
    }

    Serial.print(F(" sense_V="));
    Serial.print(adc.sense_v, 3);
    Serial.print(F(" vth_V="));
    Serial.print(adc.vth_v, 3);

    if (mag.valid) {
      Serial.print(F(" mag_uT="));
      Serial.print(mag.x_uT, 1);
      Serial.print(',');
      Serial.print(mag.y_uT, 1);
      Serial.print(',');
      Serial.print(mag.z_uT, 1);
    }

    for (unsigned i = 0; i < TofBank::kMax; ++i) {
      Serial.print(F(" tof"));
      Serial.print(i);
      Serial.print('=');
      if (!g_tof.present(i)) {
        Serial.print(F("-"));
      } else if (!frames[i].valid) {
        Serial.print(F("wait"));
      } else {
        /* Center zone of 4x4 (index 5 or 6) as a quick range. */
        Serial.print(frames[i].distance_mm[5]);
        Serial.print(F("mm"));
      }
    }
    Serial.println();
  }
}
