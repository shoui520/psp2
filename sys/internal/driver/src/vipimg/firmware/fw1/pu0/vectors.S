/*
 * PU0 interrupt entry: a stub per interrupt, the DMA-complete interrupt's entry,
 * and the vectors (jmp instructions to them) that installInterruptVectors and
 * copyVectorCode copy into local RAM.
 */
#include "interrupt.h"

	.section .lram.text, "ax"
	interruptStub mailboxEntry, VIP_INTERRUPT_MAILBOX, INTERRUPT_ACKNOWLEDGE, INTERRUPT_SLOT_MAILBOX
	interruptStub decoderEntry, VIP_INTERRUPT_DECODER, 0, INTERRUPT_SLOT_DECODER
	interruptStub readStream0Entry, VIP_INTERRUPT_READ_STREAM0, 0, INTERRUPT_SLOT_READ_STREAM
	interruptStub readStream1Entry, VIP_INTERRUPT_READ_STREAM1, 0, INTERRUPT_SLOT_READ_STREAM
	interruptStub interrupt8Entry, VIP_INTERRUPT_8, 0, INTERRUPT_SLOT_8
	interruptStub writeStream0Entry, VIP_INTERRUPT_WRITE_STREAM0, 0, INTERRUPT_SLOT_WRITE_STREAM
	interruptStub writeStream1Entry, VIP_INTERRUPT_WRITE_STREAM1, 0, INTERRUPT_SLOT_WRITE_STREAM
	interruptStub interrupt11Entry, VIP_INTERRUPT_11, 0, INTERRUPT_SLOT_11
	interruptEntry dmaEntry, dmaComplete

/*
 * The vectors: interrupts 5 to 11 (copyVectorCode), then the DMA-complete and
 * mailbox interrupts (installInterruptVectors), then three nothing copies.
 */
	.section .rodata.vectors, "a"
	.p2align 2
	.global	vipDecoderVectors, vipDmaVector, vipMailboxVector
vipDecoderVectors:
	jmp	decoderEntry
	jmp	readStream0Entry
	jmp	readStream1Entry
	jmp	interrupt8Entry
	jmp	writeStream0Entry
	jmp	writeStream1Entry
	jmp	interrupt11Entry
vipDmaVector:
	jmp	dmaEntry
vipMailboxVector:
	jmp	mailboxEntry
	.rept	3
	reti
	nop
	.endr
