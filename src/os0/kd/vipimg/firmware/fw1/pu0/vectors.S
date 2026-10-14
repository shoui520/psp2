/*
 * PU0 interrupt entry: a stub per interrupt, and the vectors (jmp instructions to
 * the stubs) that installInterruptVectors and copyVectorCode copy into local RAM.
 */
#include "interrupt.h"

	.section .lram.text, "ax"
	interruptStub dmaEntry, VIP_INTERRUPT_DMA, INTERRUPT_ACKNOWLEDGE, INTERRUPT_SLOT_DMA
	interruptStub mailboxEntry, VIP_INTERRUPT_MAILBOX, INTERRUPT_ACKNOWLEDGE, INTERRUPT_SLOT_MAILBOX
	interruptStub decoderEntry, VIP_INTERRUPT_DECODER, 0, INTERRUPT_SLOT_DECODER
	interruptStub readStream0Entry, VIP_INTERRUPT_READ_STREAM0, 0, INTERRUPT_SLOT_READ_STREAM
	interruptStub readStream1Entry, VIP_INTERRUPT_READ_STREAM1, 0, INTERRUPT_SLOT_READ_STREAM
	interruptStub interrupt8Entry, VIP_INTERRUPT_8, 0, INTERRUPT_SLOT_8
	interruptStub writeStream0Entry, VIP_INTERRUPT_WRITE_STREAM0, 0, INTERRUPT_SLOT_WRITE_STREAM
	interruptStub writeStream1Entry, VIP_INTERRUPT_WRITE_STREAM1, 0, INTERRUPT_SLOT_WRITE_STREAM
	interruptStub interrupt11Entry, VIP_INTERRUPT_11, 0, INTERRUPT_SLOT_11

/*
 * The vectors: interrupts 5 to 11 (copyVectorCode), then the DMA-complete and
 * mailbox interrupts (installInterruptVectors).
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
