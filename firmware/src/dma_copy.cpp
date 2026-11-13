#include "dma_copy.h"

#include <Arduino.h>
#include <cstring>

void dma_copy_init() {
  RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
}

void dma_copy(void *dst, const void *src, size_t nbytes) {
  if (!dst || !src || nbytes == 0) {
    return;
  }
  /* Prefer DMA1 channel 1 mem2mem for word-aligned bulk copies. */
  if (((uintptr_t)dst & 3u) == 0 && ((uintptr_t)src & 3u) == 0 && (nbytes & 3u) == 0) {
    DMA_Channel_TypeDef *ch = DMA1_Channel1;
    ch->CCR = 0;
    while (ch->CCR & DMA_CCR_EN) {
      ch->CCR = 0;
    }
    ch->CPAR = reinterpret_cast<uint32_t>(src);
    ch->CMAR = reinterpret_cast<uint32_t>(dst);
    ch->CNDTR = nbytes / 4u;
    /* MEM2MEM | MINC | PINC | PSIZE=32 | MSIZE=32 | DIR=from peri(src) */
    ch->CCR = DMA_CCR_MEM2MEM | DMA_CCR_MINC | DMA_CCR_PINC | DMA_CCR_PSIZE_1 |
              DMA_CCR_MSIZE_1 | DMA_CCR_DIR | DMA_CCR_EN;
    const uint32_t t0 = millis();
    while (ch->CNDTR != 0) {
      if ((millis() - t0) > 5) {
        ch->CCR = 0;
        std::memcpy(dst, src, nbytes);
        return;
      }
    }
    ch->CCR = 0;
    return;
  }
  std::memcpy(dst, src, nbytes);
}
