#include "cap_sense.h"
#include "board_pins.h"

#include <Arduino.h>
#include <cmath>

/*
 * COMP1 + TIM2 capacitance measurement (register-level for G4).
 *
 * PA1 = COMP1_INP, PA4 = COMP1_INM (INMSEL = 0b0110 → PA4 per RM0440).
 *
 * TIM2 runs unprescaled at the full timer clock (170 MHz → ~5.9 ns/tick).
 * It is 32-bit, so even the 50 ms timeout uses < 0.2% of its range and no
 * prescaling is ever needed. COMP1_OUT is routed to TIM2 TI1 (TISEL) and
 * latched into CCR1 by hardware input capture, so the count does not
 * depend on interrupt latency. The COMP1 EXTI (line 21) interrupt only
 * stops charging and collects CCR1.
 */

namespace {

volatile uint32_t g_capture_ticks = 0;
volatile bool g_capture_done = false;
volatile bool g_capture_valid = false;
volatile bool g_capture_armed = false;

/* TIM2 tick rate (no prescaler), measured from the clock tree at init. */
float g_tim_hz = 170'000'000.0f;

/* TIM2 kernel clock: PCLK1, doubled when the APB1 prescaler divides. */
uint32_t tim2_clock_hz() {
  const uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
  const bool apb1_divided = (RCC->CFGR & RCC_CFGR_PPRE1_2) != 0;
  return apb1_divided ? pclk1 * 2u : pclk1;
}

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
      GPIOA->BSRR = (1u << 16); /* PA0 reset — stop charging */
      TIM2->CCER &= ~TIM_CCER_CC1E; /* no further captures this cycle */
      const uint32_t sr = TIM2->SR;
      /* CCR1 holds the hardware-latched trip time. Overcapture means the
       * comparator chattered and the first edge was overwritten. */
      g_capture_valid = (sr & TIM_SR_CC1IF) && !(sr & TIM_SR_CC1OF);
      g_capture_ticks = TIM2->CCR1;
      g_capture_done = true;
      g_capture_armed = false;
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

  /* TIM2 free-running 32-bit at the full timer clock (PSC = 0). */
  g_tim_hz = static_cast<float>(tim2_clock_hz());
  TIM2->CR1 = 0;
  TIM2->PSC = 0;
  TIM2->ARR = 0xFFFFFFFFu;
  /* CH1 input capture from COMP1_OUT: TISEL.TI1SEL = 0001 (RM0440),
   * CC1S = 01 (IC1 on TI1), no filter/prescaler, rising edge. */
  TIM2->TISEL = (TIM2->TISEL & ~TIM_TISEL_TI1SEL) | (1u << TIM_TISEL_TI1SEL_Pos);
  TIM2->CCMR1 = (TIM2->CCMR1 & ~(TIM_CCMR1_CC1S | TIM_CCMR1_IC1F | TIM_CCMR1_IC1PSC)) |
                TIM_CCMR1_CC1S_0;
  TIM2->CCER &= ~(TIM_CCER_CC1E | TIM_CCER_CC1P | TIM_CCER_CC1NP);
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
  g_capture_valid = false;
  g_capture_ticks = 0;
  TIM2->CCER &= ~TIM_CCER_CC1E;
  TIM2->SR = ~(TIM_SR_CC1IF | TIM_SR_CC1OF); /* rc_w0 */
  TIM2->CCER |= TIM_CCER_CC1E;
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
      TIM2->CCER &= ~TIM_CCER_CC1E;
      return out;
    }
  }

  if (!g_capture_valid) {
    return out;
  }
  out.ticks = g_capture_ticks;
  out.time_s = static_cast<float>(out.ticks) / g_tim_hz;
  out.capacitance_f = out.time_s / (CAP_R_OHMS * 0.693147f);
  out.valid = true;

  GPIOA->BSRR = (1u << 16);
  return out;
}
