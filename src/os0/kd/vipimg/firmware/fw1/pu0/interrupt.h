/*
 * PU0 interrupts: the slots of their handlers.
 */
#pragma once

#include "../common/interrupt.h"

#define INTERRUPT_SLOT_DECODER		1
#define INTERRUPT_SLOT_READ_STREAM	2
#define INTERRUPT_SLOT_8		3
#define INTERRUPT_SLOT_WRITE_STREAM	4
#define INTERRUPT_SLOT_11		5
#define INTERRUPT_SLOT_MAILBOX		12

#ifndef __ASSEMBLER__

void acknowledgeMailbox(uint32_t argument);
void decoderInterrupt(uint32_t argument);

#endif
