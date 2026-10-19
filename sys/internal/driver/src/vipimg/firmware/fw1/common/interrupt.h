/*
 * fw1 runtime: interrupt entry. An interrupt's vector enters a stub with a code:
 * the interrupt number, whether to acknowledge it on entry, and the slot of its
 * handler in the core's g_interruptHandlers. interruptDispatch (dispatch.S) saves
 * the interrupted context and calls the handler with the code's bits from 12 up.
 */
#pragma once

#include "runtime.h"

#define INTERRUPT_ACKNOWLEDGE		0x80
#define INTERRUPT_SLOT_SHIFT		8
#define INTERRUPT_ARGUMENT_SHIFT	12
#define INTERRUPT_SLOTS			16

#define INTERRUPT_SLOT_DMA		0
#define INTERRUPT_SLOT_MAILBOX		12

#ifdef __ASSEMBLER__

	.macro	interruptStub name, interrupt, acknowledge, slot
\name:
	add	$sp, -4
	sw	$1, 0($sp)
	movu	$1, (\slot << INTERRUPT_SLOT_SHIFT) | \acknowledge | \interrupt
	bra	interruptDispatch
	.endm

#else

extern void (*g_interruptHandlers[INTERRUPT_SLOTS])(uint32_t argument);

void dmaComplete(uint32_t argument);

#endif
