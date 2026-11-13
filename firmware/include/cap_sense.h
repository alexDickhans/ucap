#pragma once

#include <cstdint>

/*
 * Precise capacitance via on-chip COMP1 + TIM2.
 *
 * Hardware path (schematic "RC Delay Sensing"):
 *   PA0 (current_insert) --R5 1MΩ--> H1 sense node --R6 220Ω--> PA1 (sense)
 *   R7/R8 10k/10k divide +3V3 → PA4 (v_th) ≈ 1.65 V
 *
 * MCU blocks (from pin AF list in ucap.kicad_sch):
 *   PA1 = COMP1_INP
 *   PA4 = COMP1_INM
 *   TIM2 free-running counter latched on COMP1 rising (sense crosses v_th)
 *
 * At Vth = 0.5·Vdd: t = R·C·ln(2)  ⇒  C = t / (R·ln(2))
 */

struct CapSample {
  bool valid;
  uint32_t ticks;     /* TIM2 counts from charge start to COMP trip */
  float time_s;       /* seconds */
  float capacitance_f; /* farads */
};

void cap_sense_init();
CapSample cap_sense_measure(uint32_t timeout_us = 50'000);
