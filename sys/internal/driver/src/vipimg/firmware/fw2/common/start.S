/*
 * fw2 runtime: what cannot be C. The code the runtime copies into local RAM (the
 * interrupt vectors, the sleep and halt sequences), the interrupt and exception
 * entries, and the call of a program with its context's $gp.
 */
#include "runtime.h"

	.text
/*
 * int vipCallProgram(entry, output, gp): run a program's entry with $gp at its
 * context's globals, and return its result.
 */
	.global	vipCallProgram
vipCallProgram:
	add	$sp, -8
	ldc	$0, $lp
	sw	$0, 0($sp)
	sw	$gp, 4($sp)
	mov	$gp, $3
	mov	$3, $1
	mov	$1, $2
	jsr	$3
	lw	$gp, 4($sp)
	lw	$1, 0($sp)
	add	$sp, 8
	jmp	$1

/* An exception: record where it happened in the core's record, and go on. */
	.global	vipException
vipException:
	add	$sp, -12
	sw	$tp, 8($sp)
	sw	$1, 4($sp)
	sw	$0, 0($sp)
	ldc	$1, $id
	movu	$0, g_exceptionRecords
	srl	$1, 16
	sll	$1, 4
	add3	$tp, $0, $1
	ldc	$1, $lp
	sw	$gp, 0($tp)
	sw	$1, 4($tp)
	ldc	$0, $epc
	ldc	$1, $npc
	sw	$0, 8($tp)
	sw	$1, 12($tp)
	lw	$0, 0($sp)
	lw	$1, 4($sp)
	lw	$tp, 8($sp)
	add	$sp, 12
	reti

	interruptEntry vipMailboxEntry, vipReceiveCommand

/* The timer interrupt (vipWaitTimer's wake-up): acknowledge it. */
	.global	vipTimerEntry
vipTimerEntry:
	stc	$1, $tmp
	mov	$1, ~(1 << VIP_INTERRUPT_TIMER)
	stcb	$1, VIP_CB_INTERRUPT_CLEAR
	ldc	$1, $tmp
	nop
	reti

/*
 * The interrupt vectors, copied to VIP_INTERRUPT_VECTORS: the DMA-complete and
 * mailbox interrupts, nothing for interrupts 2 and 3, the timer.
 */
	.section .rodata.vectors, "a"
	.p2align 2
	.global	vipInterruptVectors, vipInterruptVectorsSize
vipInterruptVectors:
	jmp	vipDmaEntry
	jmp	vipMailboxEntry
	nop
	nop
	nop
	nop
	jmp	vipTimerEntry
	.set	vipInterruptVectorsSize, . - vipInterruptVectors

/*
 * Copied to VIP_LOCAL_RAM: set *status to sleeping, sleep, set it to awake.
 * vipSleepCode(status, sleeping, awake).
 */
	.p2align 2
	.global	vipSleepCode, vipSleepCodeSize, vipLocalSleep
	.set	vipLocalSleep, VIP_LOCAL_RAM
vipSleepCode:
	sw	$2, 0($1)
	sleep
	sw	$3, 0($1)
	ret
	.set	vipSleepCodeSize, . - vipSleepCode

/*
 * Copied to VIP_HALT_CODE, from where the core stops for good once DRAM may go:
 * vipHaltCode(answer, value, status) clears the status, gives the ARM side its
 * answer and sleeps.
 */
	.p2align 2
	.global	vipHaltCode, vipHaltCodeEnd
vipHaltCode:
	di
	mov	$0, 0
	sw	$0, 0($3)
	sw	$2, 0($1)
1:	sleep
	bra	1b
vipHaltCodeEnd:			/* the branch may relax: its size is the link's */

/* the DMA-complete interrupt, from local RAM: acknowledge it and run fw1's handler */
	.section .lram.text, "ax"
	.global	vipDmaEntry
vipDmaEntry:
	add3	$sp, $sp, -56
	sw	$0, 0($sp)
	sw	$1, 4($sp)
	mov	$1, ~(1 << VIP_INTERRUPT_DMA)
	stcb	$1, VIP_CB_INTERRUPT_CLEAR
	sw	$2, 8($sp)
	sw	$3, 12($sp)
	sw	$4, 16($sp)
	sw	$9, 20($sp)
	sw	$10, 24($sp)
	sw	$11, 28($sp)
	sw	$12, 32($sp)
	sw	$tp, 36($sp)
	ldc	$0, $lp
	ldc	$1, $rpb
	ldc	$2, $rpe
	ldc	$3, $rpc
	sw	$0, 40($sp)
	sw	$1, 44($sp)
	sw	$2, 48($sp)
	sw	$3, 52($sp)
	mov	$1, 0
	bsr	dmaComplete
	lw	$0, 40($sp)
	lw	$1, 44($sp)
	lw	$2, 48($sp)
	lw	$3, 52($sp)
	stc	$0, $lp
	stc	$1, $rpb
	stc	$2, $rpe
	stc	$3, $rpc
	lw	$4, 16($sp)
	lw	$9, 20($sp)
	lw	$10, 24($sp)
	lw	$11, 28($sp)
	lw	$12, 32($sp)
	lw	$tp, 36($sp)
	lw	$0, 0($sp)
	lw	$1, 4($sp)
	lw	$2, 8($sp)
	lw	$3, 12($sp)
	add3	$sp, $sp, 56
	reti
