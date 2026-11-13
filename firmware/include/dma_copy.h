#pragma once

#include <cstddef>
#include <cstdint>

/*
 * Exercises on-chip DMA1 (mem2mem) for bulk copies used when packing
 * multi-zone ToF frames into the USB CDC path. Falls back to memcpy if
 * DMA is busy or unavailable under the Arduino core.
 */
void dma_copy_init();
void dma_copy(void *dst, const void *src, size_t nbytes);
