#include "stat_led.h"
#include "board_pins.h"

void stat_led_init() {
  pinMode(PIN_STAT_LED, OUTPUT);
  digitalWrite(PIN_STAT_LED, LOW);
}

void stat_led_set(bool on) { digitalWrite(PIN_STAT_LED, on ? HIGH : LOW); }

void stat_led_toggle() { digitalWrite(PIN_STAT_LED, !digitalRead(PIN_STAT_LED)); }
