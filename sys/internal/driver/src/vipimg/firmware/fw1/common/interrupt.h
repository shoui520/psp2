/*
 * fw1 runtime: interrupt entry. An interrupt enters at its vector in local RAM,
 * VIP_INTERRUPT_VECTORS + 4 * its number, which holds a jmp to its handler; each
 * core keeps the jmp instructions in its program (vectors.S) and copies them in.
 * Both cores take the DMA-complete interrupt into dmaComplete (lram.c), through
 * interruptEntry; their other interrupts are their own (pu0, pu1).
 */
#pragma once

#include "runtime.h"

#if !defined(__ASSEMBLER__) && !defined(VIP_FW2)

void dmaComplete(void);
void dmaCallbackReturned(void);

#endif
