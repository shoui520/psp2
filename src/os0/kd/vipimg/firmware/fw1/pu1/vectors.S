/*
 * PU1 interrupt entry: a stub per interrupt, and the vectors (jmp instructions to
 * the stubs) that installInterruptVectors and copyVectorCode copy into local RAM.
 */
#include "interrupt.h"

	.section .lram.text, "ax"
	interruptStub dmaEntry, VIP_INTERRUPT_DMA, INTERRUPT_ACKNOWLEDGE, INTERRUPT_SLOT_DMA
	interruptStub mailboxEntry, VIP_INTERRUPT_MAILBOX, INTERRUPT_ACKNOWLEDGE, INTERRUPT_SLOT_MAILBOX
	interruptStub readStream0Entry, VIP_INTERRUPT_READ_STREAM0, 0, INTERRUPT_SLOT_READ_STREAM
	interruptStub readStream1Entry, VIP_INTERRUPT_READ_STREAM1, 0, INTERRUPT_SLOT_READ_STREAM

/* interrupts 5 and 8: acknowledged, nothing else */
	.macro	ignoreStub name, interrupt
\name:
	add	$sp, -4
	sw	$12, 0($sp)
	mov	$12, ~(1 << \interrupt)
	stcb	$12, VIP_CB_INTERRUPT_CLEAR
	lw	$12, 0($sp)
	add	$sp, 4
	reti
	.endm

	ignoreStub interrupt5Entry, VIP_INTERRUPT_DECODER
	ignoreStub interrupt8Entry, VIP_INTERRUPT_8

/*
 * The vectors: interrupts 5 to 8 (copyVectorCode), then the DMA-complete and
 * mailbox interrupts (installInterruptVectors).
 */
	.section .rodata.vectors, "a"
	.p2align 2
	.global	vipStreamVectors, vipDmaVector, vipMailboxVector
vipStreamVectors:
	jmp	interrupt5Entry
	jmp	readStream0Entry
	jmp	readStream1Entry
	jmp	interrupt8Entry
vipDmaVector:
	jmp	dmaEntry
vipMailboxVector:
	jmp	mailboxEntry
