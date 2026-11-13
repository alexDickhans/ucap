#pragma once

#include <cstdint>

/*
 * Diagnostic ADC on sense nets that the PCB already wires:
 *   PA1 sense  → ADC1_IN2
 *   PA4 v_th   → ADC2_IN17
 *
 * Not used for the primary capacitance path (that is COMP1+TIM2);
 * this samples the same nodes for bring-up / threshold checks.
 * Uses ADC + DMA where the STM32duino HAL path allows.
 */

struct AdcDiagSample {
  uint16_t sense_raw;
  uint16_t vth_raw;
  float sense_v;
  float vth_v;
};

void adc_diag_init();
AdcDiagSample adc_diag_read();
