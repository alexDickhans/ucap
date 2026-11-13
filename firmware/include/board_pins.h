#pragma once

/*
 * uCap v1 pin map — sourced from schematic labels + README connector table.
 * Do not invent alternate nets; every #define below is on the PCB.
 *
 * MCU: STM32G474CEUx (U1)
 */

#include <Arduino.h>

/* ---- Capacitance sense (RC Delay Sensing sheet) ----
 * PA0  current_insert  — GPIO charge drive through R5 (1 MΩ) to H1
 * PA1  sense           — COMP1_INP (also ADC1_IN2 / ADC2_IN2)
 * PA4  v_th            — COMP1_INM from R7/R8 divider (also ADC2_IN17)
 */
static constexpr uint32_t PIN_CURRENT_INSERT = PA0;
static constexpr uint32_t PIN_SENSE          = PA1;
static constexpr uint32_t PIN_V_TH           = PA4;

/* Charge resistor on the board (R5). */
static constexpr float CAP_R_OHMS = 1'000'000.0f;

/* ---- Status LED (D1 STAT) ----
 * PA5 high → LED on (anode to PA5, R9 470 Ω to cathode/GND).
 */
static constexpr uint32_t PIN_STAT_LED = PA5;

/* ---- USB FS (J1) ---- */
static constexpr uint32_t PIN_USB_DM = PA11;
static constexpr uint32_t PIN_USB_DP = PA12;

/* ---- SWD (J2) ---- */
static constexpr uint32_t PIN_SWDIO = PA13;
static constexpr uint32_t PIN_SWCLK = PA14;

/* ---- Four independent Qwiic I2C buses (J3–J6) ----
 * Each maps to a distinct STM32 I2C peripheral (AF from KiCad symbol).
 *
 * Bus  Silk  Conn  SDA   SCL    Peripheral
 * 0    I2C 0 J3    PB9   PA15   I2C1
 * 1    I2C 1 J4    PF0   PC4    I2C2
 * 2    I2C 2 J5    PC11  PA8    I2C3
 * 3    I2C 3 J6    PB7   PC6    I2C4
 */
static constexpr uint32_t PIN_I2C0_SDA = PB9;
static constexpr uint32_t PIN_I2C0_SCL = PA15;
static constexpr uint32_t PIN_I2C1_SDA = PF0;
static constexpr uint32_t PIN_I2C1_SCL = PC4;
static constexpr uint32_t PIN_I2C2_SDA = PC11;
static constexpr uint32_t PIN_I2C2_SCL = PA8;
static constexpr uint32_t PIN_I2C3_SDA = PB7;
static constexpr uint32_t PIN_I2C3_SCL = PC6;

static constexpr unsigned UCAP_I2C_BUS_COUNT = 4;
static constexpr unsigned UCAP_TOF_MAX       = 3;

/*
 * Default sensor socket assignment (PCB does not silk-label sensors;
 * sockets are identical Qwiic. This mapping is firmware policy):
 *   I2C0 (J3) — Adafruit #4022 LIS3MDL magnetometer
 *   I2C1 (J4) — VL53L5CX ToF #0
 *   I2C2 (J5) — VL53L5CX ToF #1
 *   I2C3 (J6) — VL53L5CX ToF #2
 */
static constexpr unsigned BUS_MAG   = 0;
static constexpr unsigned BUS_TOF0  = 1;
static constexpr unsigned BUS_TOF1  = 2;
static constexpr unsigned BUS_TOF2  = 3;
