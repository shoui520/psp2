/*
 * PU1 interrupts: the slots of their handlers. Interrupts 5 and 8 are only
 * acknowledged (vectors.S).
 */
#pragma once

#include "../common/interrupt.h"

#define INTERRUPT_SLOT_READ_STREAM	1

#ifndef __ASSEMBLER__

void acknowledgeMailbox(uint32_t argument);

#endif
