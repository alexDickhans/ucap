#include "adc_diag.h"
#include "board_pins.h"

void adc_diag_init() {
  analogReadResolution(12);
  /* Touch pins once so the core attaches ADC1_IN2 / ADC2_IN17. */
  (void)analogRead(PIN_SENSE);
  (void)analogRead(PIN_V_TH);
}

AdcDiagSample adc_diag_read() {
  AdcDiagSample s{};
  s.sense_raw = static_cast<uint16_t>(analogRead(PIN_SENSE));
  s.vth_raw = static_cast<uint16_t>(analogRead(PIN_V_TH));
  constexpr float vref = 3.3f;
  constexpr float full = 4095.0f;
  s.sense_v = (s.sense_raw / full) * vref;
  s.vth_v = (s.vth_raw / full) * vref;
  return s;
}
