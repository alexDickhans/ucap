#include "cap_sense.h"
#include "board_pins.h"

#include <Arduino.h>
#include <cmath>

/*
 * COMP1 + TIM2 capacitance measurement (register-level for G4).
 *
 * PA1 = COMP1_INP, PA4 = COMP1_INM (INMSEL = 0b0110 → PA4 per RM0440).
 * COMP1 EXTI (line 21) rising edge latches TIM2 CNT.
 */

namespace {

volatile uint32_t g_capture_ticks = 0;
volatile bool g_capture_done = false;
volatile bool g_capture_armed = false;

/* TIM2 tick rate after PSC (170 MHz / 17 ≈ 10 MHz). */
constexpr float kTimHz = 10'000'000.0f;
constexpr uint32_t kTimPsc = 16; /* ARR clock = Ftim / (PSC+1) */

void gpio_set_mode(GPIO_TypeDef *port, unsigned pin_no, uint32_t mode, uint32_t speed = 0) {
  /* mode: 0=input 1=output 2=AF 3=analog */
  port->MODER = (port->MODER & ~(3u << (pin_no * 2))) | (mode << (pin_no * 2));
  if (mode == 1) {
    port->OTYPER &= ~(1u << pin_no);
    port->OSPEEDR = (port->OSPEEDR & ~(3u << (pin_no * 2))) | (speed << (pin_no * 2));
  }
  port->PUPDR &= ~(3u << (pin_no * 2));
}

} // namespace

extern "C" void COMP1_2_3_IRQHandler(void) {
  /* COMP1 interrupt pending: EXTI PR1 bit 21 (EXTI_PR1 on G4 uses EXTI). */
  if (EXTI->PR1 & (1u << 21)) {
    EXTI->PR1 = (1u << 21); /* W1C */
    if (g_capture_armed) {
      g_capture_ticks = TIM2->CNT;
      g_capture_done = true;
      g_capture_armed = false;
      GPIOA->BSRR = (1u << 16); /* PA0 reset — stop charging */
    }
  }
}

void cap_sense_init() {
  RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
  RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;
  RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

  /* PA0 output PP (current_insert). */
  gpio_set_mode(GPIOA, 0, 1, 3);
  GPIOA->BSRR = (1u << 16); /* low — discharge */

  /* PA1, PA4 analog for COMP inputs. */
  gpio_set_mode(GPIOA, 1, 3);
  gpio_set_mode(GPIOA, 4, 3);

  /* TIM2 free-running 32-bit @ ~10 MHz. */
  TIM2->CR1 = 0;
  TIM2->PSC = kTimPsc;
  TIM2->ARR = 0xFFFFFFFFu;
  TIM2->EGR = TIM_EGR_UG;
  TIM2->CNT = 0;
  TIM2->CR1 = TIM_CR1_CEN;

  /*
   * COMP1 CSR:
   *  EN
   *  INMSEL = 0110 (PA4) — RM0440
   *  INPSEL = 0 (PA1)
   *  HYST = 01 (low)
   *  POLARITY = 0 (not inverted: out high when INP > INM)
   */
  /* CSR: EN | INMSEL=0110 (PA4) | HYST=01 (low). INPSEL=0 → PA1. */
  COMP1->CSR = 0;
  COMP1->CSR = (1u << 0) | (6u << 4) | (1u << 16);

  /* EXTI line 21 ← COMP1, rising edge interrupt. */
  EXTI->IMR1 |= (1u << 21);
  EXTI->RTSR1 |= (1u << 21);
  EXTI->FTSR1 &= ~(1u << 21);

  NVIC_SetPriority(COMP1_2_3_IRQn, 0);
  NVIC_EnableIRQ(COMP1_2_3_IRQn);
}

CapSample cap_sense_measure(uint32_t timeout_us) {
  CapSample out{};
  out.valid = false;

  /* Discharge through R5. */
  GPIOA->BSRR = (1u << 16);
  delayMicroseconds(500);

  g_capture_done = false;
  g_capture_ticks = 0;
  g_capture_armed = true;

  __disable_irq();
  TIM2->CNT = 0;
  GPIOA->BSRR = (1u << 0); /* PA0 high — start charging */
  __enable_irq();

  const uint32_t t0 = micros();
  while (!g_capture_done) {
    if ((micros() - t0) > timeout_us) {
      g_capture_armed = false;
      GPIOA->BSRR = (1u << 16);
      return out;
    }
  }

  out.ticks = g_capture_ticks;
  out.time_s = static_cast<float>(out.ticks) / kTimHz;
  out.capacitance_f = out.time_s / (CAP_R_OHMS * 0.693147f);
  out.valid = true;

  GPIOA->BSRR = (1u << 16);
  return out;
}
