/*
 * PU1 interrupt entry: the entries of the DMA-complete and read stream interrupts,
 * whose handlers are C, the mailbox interrupt's entry, and the vectors (jmp
 * instructions to them) that installInterruptVectors and copyVectorCode copy into
 * local RAM. Interrupts 5 and 8 enter interrupt functions (hardware.c).
 */
#include "interrupt.h"
#include "../boot/boot.h"

	.section .lram.text, "ax"
	interruptEntry dmaEntry, dmaComplete
	interruptEntry readStreamEntry, readStreamInterrupt

/*
 * The vectors: interrupts 5 to 8 (copyVectorCode; both halves of the read stream
 * go to readStreamInterrupt, which tells them apart), the mailbox interrupt's
 * entry, the DMA-complete and mailbox interrupts' vectors
 * (installInterruptVectors), then four nothing copies.
 */
	.text
	.p2align 2
	.global	vipStreamVectors, vipDmaVector, vipMailboxVector
vipStreamVectors:
	jmp	interrupt5
	jmp	readStreamEntry
	jmp	readStreamEntry
	jmp	interrupt8

/*
 * The mailbox interrupt: save what C may change, acknowledge the interrupt and
 * call acknowledgeMailbox. Each pass is recorded in core 1's crash record: $sp on
 * entry and on the way out, and the number of entries and of returns.
 */
#define MAILBOX_RECORD		(BOOT_CRASH_RECORDS + BOOT_CRASH_RECORD_SIZE)
#define RECORD_ENTRY_SP		0
#define RECORD_RETURN_SP	4
#define RECORD_ENTRIES		8
#define RECORD_RETURNS		12

mailboxEntry:
	add3	$sp, $sp, -64
	sw	$0, 0($sp)
	sw	$1, 4($sp)
	sw	$2, 8($sp)
	sw	$3, 12($sp)
	sw	$4, 16($sp)
	sw	$9, 20($sp)
	sw	$10, 24($sp)
	sw	$11, 28($sp)
	sw	$12, 32($sp)
	sw	$tp, 36($sp)
	sw	$gp, 40($sp)
	movu	$1, MAILBOX_RECORD
	sw	$sp, RECORD_ENTRY_SP($1)
	lw	$2, RECORD_ENTRIES($1)
	add3	$2, $2, 1
	sw	$2, RECORD_ENTRIES($1)
	ldc	$1, $lp
	sw	$1, 44($sp)
	ldc	$2, $sar
	sw	$2, 48($sp)
	ldc	$1, $rpb
	sw	$1, 52($sp)
	ldc	$2, $rpe
	sw	$2, 56($sp)
	ldc	$1, $rpc
	sw	$1, 60($sp)
	mov	$0, ~(1 << VIP_INTERRUPT_MAILBOX)
	stcb	$0, VIP_CB_INTERRUPT_CLEAR
	bsr	acknowledgeMailbox
	lw	$1, 44($sp)
	stc	$1, $lp
	lw	$2, 48($sp)
	stc	$2, $sar
	lw	$1, 52($sp)
	stc	$1, $rpb
	lw	$2, 56($sp)
	stc	$2, $rpe
	lw	$1, 60($sp)
	stc	$1, $rpc
	movu	$1, MAILBOX_RECORD
	sw	$sp, RECORD_RETURN_SP($1)
	lw	$2, RECORD_RETURNS($1)
	add3	$2, $2, 1
	sw	$2, RECORD_RETURNS($1)
	lw	$0, 0($sp)
	lw	$1, 4($sp)
	lw	$2, 8($sp)
	lw	$3, 12($sp)
	lw	$4, 16($sp)
	lw	$9, 20($sp)
	lw	$10, 24($sp)
	lw	$11, 28($sp)
	lw	$12, 32($sp)
	lw	$tp, 36($sp)
	lw	$gp, 40($sp)
	add3	$sp, $sp, 64
	reti

	.p2align 2
vipDmaVector:
	jmp	dmaEntry
vipMailboxVector:
	jmp	mailboxEntry
	.rept	4
	reti
	nop
	.endr
