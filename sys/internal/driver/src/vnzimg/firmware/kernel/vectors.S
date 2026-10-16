/*
 * What C cannot express: the vectors, the reset entry, the exception and
 * interrupt entries that save the interrupted registers, the thread switch.
 *
 * The vector table is kept at +0x500 in the image and copied to the image's
 * start, the vector base, by vkInterruptsInitialize.
 */
#include <vnz.h>

#define VK_BOOT_STACK		0x200		/* each core's, until its idle thread runs */
#define VK_IDLE_THREAD_SIZE	0x80
#define VK_THREAD_DISPATCH	16		/* VkThread.savedDispatchDisable */
#define VK_THREAD_SP		24		/* VkThread.savedSp */
#define VK_THREAD_STACK		64		/* VkThread.stack */
#define VK_THREAD_STACK_SIZE	68		/* VkThread.stackSize */
#define VK_CORE_DISPATCH	24		/* VkCore.dispatchDisable */
#define VK_CORE_RESCHEDULE	9		/* VkCore.reschedulePending */
#define VK_CORE_IN_KERNEL	12		/* VkCore.inKernel */
#define VK_CORE_EVENTS		16		/* VkCore.eventsHead */
#define VK_CORE_SIZE		0x40

	.section .vectors, "ax"
	.global	vkVectors
	.global	vkVectorsEnd
	.p2align 2
vkVectors:
	jmp	vkReset				/* reset */
	jmp	exceptionNmi			/* NMI */
	jmp	exceptionReservedInstruction	/* reserved instruction */
	jmp	exceptionZeroDivide		/* zero divide */
	jmp	softwareBreak			/* BRK */
	jmp	softwareInterrupt		/* SWI */
	.word	0				/* debug */
	jmp	exceptionDsp			/* DSP */
	jmp	exceptionCoprocessor		/* coprocessor */
	.word	0, 0, 0
	.rept	17				/* interrupts 0-16 */
	jmp	vkInterruptEntry
	.endr
	jmp	vkDoorbellEntry			/* interrupt 17: another core's doorbell */
	.rept	6				/* interrupts 18-23 */
	jmp	vkInterruptEntry
	.endr
vkVectorsEnd:

	.text
	.p2align 1

/*
 * Reset: clear the registers and the accumulators, take this core's boot stack
 * (it becomes the idle thread's), and start the kernel.
 */
	.global	vkReset
vkReset:
	mov	$0, 0
	mov	$1, $0
	mov	$2, $0
	mov	$3, $0
	mov	$4, $0
	mov	$5, $0
	mov	$6, $0
	mov	$7, $0
	mov	$8, $0
	mov	$9, $0
	mov	$10, $0
	mov	$11, $0
	mov	$12, $0
	mov	$tp, $0
	mov	$gp, $0
	ldc	$1, $opt
	movh	$12, 0x300			/* multiply or divide unit */
	and	$1, $12
	beqz	$1, 1f
	mov	$0, 0
	stc	$0, $hi
	stc	$0, $lo
1:	ldc	$1, $opt
	mov	$12, 0x10			/* the coprocessor */
	and	$1, $12
	beqz	$1, 2f
	xor	$0, $0
	cmovc	$acc1_0, $0
	cmovc	$acc1_1, $0
	cmovc	$acc1_2, $0
	cmovc	$acc1_3, $0
	cmovc	$acc1_4, $0
	cmovc	$acc1_5, $0
	cmovc	$acc1_6, $0
	cmovc	$acc1_7, $0
2:	movu	$5, VK_BOOT_STACK
	movh	$4, %hi(g_vkBootStacks)
	add3	$4, $4, %lo(g_vkBootStacks)
	ldc	$0, $id
	sra	$0, 16
	and3	$0, $0, 7
	mov	$9, $0
	sll	$9, 9
	add3	$4, $4, $9
	add3	$sp, $4, $5
	movh	$9, %hi(g_vkIdleThreads)
	add3	$9, $9, %lo(g_vkIdleThreads)
	sll	$0, 7
	add3	$9, $9, $0
	sw	$4, VK_THREAD_STACK($9)
	sw	$5, VK_THREAD_STACK_SIZE($9)
	movu	$12, vkKernelStart
	jsr	$12
3:	bra	3b

/*
 * Exceptions: the number goes to $tmp, then the registers to a frame below the
 * idle thread's saved stack pointer, and the core stops in vkCrash.
 */
#define EXCEPTION(name, number)		\
name:					\
	add3	$sp, $sp, -4;		\
	sw	$0, ($sp);		\
	mov	$0, number;		\
	stc	$0, $tmp;		\
	lw	$0, ($sp);		\
	add3	$sp, $sp, 4;		\
	jmp	exception

softwareBreak:
	reti
softwareInterrupt:				/* the idle loop's swi: nothing to do */
	reti

EXCEPTION(exceptionNmi, 1)
EXCEPTION(exceptionReservedInstruction, 2)
EXCEPTION(exceptionZeroDivide, 3)
EXCEPTION(exceptionDsp, 4)
EXCEPTION(exceptionCoprocessor, 5)
	.global	vkExceptionTooManyExited
EXCEPTION(vkExceptionTooManyExited, 16)

exception:
	add	$sp, -12
	sw	$0, ($sp)
	sw	$1, 4($sp)
	sw	$2, 12($sp)
	ldc	$1, $id
	sra	$1, 16
	and3	$1, $1, 7
	movh	$2, %hi(g_vkIdleThreads)
	add3	$2, $2, %lo(g_vkIdleThreads)
	sll3	$0, $1, 7
	add3	$2, $2, $0
	lw	$2, VK_THREAD_SP($2)
	movh	$0, 0
	add3	$0, $0, -68
	add3	$2, $2, $0
	sw	$2, 8($2)
	sw	$3, 12($2)
	sw	$4, 16($2)
	sw	$5, 20($2)
	sw	$6, 24($2)
	sw	$7, 28($2)
	sw	$8, 32($2)
	sw	$9, 36($2)
	sw	$10, 40($2)
	sw	$11, 44($2)
	sw	$12, 48($2)
	sw	$tp, 52($2)
	sw	$gp, 56($2)
	ldc	$3, $lp
	sw	$3, 64($2)
	lw	$9, 8($sp)
	sw	$9, 8($2)
	lw	$9, 4($sp)
	sw	$9, 4($2)
	lw	$9, ($sp)
	sw	$9, ($2)
	add	$sp, 12
	sw	$sp, 60($2)
	mov	$sp, $2
	ldc	$3, $tmp
	jmp	vkCrash				/* (core index, frame, exception) */

/* An interrupt: save the interrupted registers, and dispatch it. */
	.global	vkInterruptEntry
vkInterruptEntry:
	add3	$sp, $sp, -96
	sw	$0, ($sp)
	sw	$1, 4($sp)
	sw	$2, 8($sp)
	sw	$3, 12($sp)
	sw	$4, 16($sp)
	sw	$5, 20($sp)
	sw	$6, 24($sp)
	sw	$7, 28($sp)
	sw	$8, 32($sp)
	sw	$9, 36($sp)
	sw	$10, 40($sp)
	sw	$11, 44($sp)
	sw	$12, 48($sp)
	sw	$tp, 52($sp)
	sw	$gp, 56($sp)
	ldc	$0, $rpb
	sw	$0, 60($sp)
	ldc	$0, $rpe
	sw	$0, 64($sp)
	ldc	$0, $rpc
	sw	$0, 68($sp)
	ldc	$0, $hi
	sw	$0, 72($sp)
	ldc	$0, $lo
	sw	$0, 76($sp)
	ldc	$0, $sar
	sw	$0, 80($sp)
	ldc	$0, $epc
	sw	$0, 84($sp)
	ldc	$0, $lp
	sw	$0, 88($sp)
	ldcb	$1, VNZ_CB_INTERRUPT_STATUS
	and3	$1, $1, 0xf8
	srl	$1, 3
	bsr	vkInterruptDispatch
	bra	restore

	.global	vkDoorbellEntry
vkDoorbellEntry:
	add3	$sp, $sp, -96
	sw	$0, ($sp)
	sw	$1, 4($sp)
	sw	$2, 8($sp)
	sw	$3, 12($sp)
	sw	$4, 16($sp)
	sw	$5, 20($sp)
	sw	$6, 24($sp)
	sw	$7, 28($sp)
	sw	$8, 32($sp)
	sw	$9, 36($sp)
	sw	$10, 40($sp)
	sw	$11, 44($sp)
	sw	$12, 48($sp)
	sw	$tp, 52($sp)
	sw	$gp, 56($sp)
	ldc	$0, $rpb
	sw	$0, 60($sp)
	ldc	$0, $rpe
	sw	$0, 64($sp)
	ldc	$0, $rpc
	sw	$0, 68($sp)
	ldc	$0, $hi
	sw	$0, 72($sp)
	ldc	$0, $lo
	sw	$0, 76($sp)
	ldc	$0, $sar
	sw	$0, 80($sp)
	ldc	$0, $epc
	sw	$0, 84($sp)
	ldc	$0, $lp
	sw	$0, 88($sp)
	bsr	vkDoorbellDispatch

restore:
	lw	$0, 88($sp)
	stc	$0, $lp
	lw	$0, 84($sp)
	stc	$0, $epc
	lw	$0, 80($sp)
	stc	$0, $sar
	lw	$0, 76($sp)
	stc	$0, $lo
	lw	$0, 72($sp)
	stc	$0, $hi
	lw	$0, 68($sp)
	stc	$0, $rpc
	lw	$0, 64($sp)
	stc	$0, $rpe
	lw	$0, 60($sp)
	stc	$0, $rpb
	lw	$gp, 56($sp)
	lw	$tp, 52($sp)
	lw	$12, 48($sp)
	lw	$11, 44($sp)
	lw	$10, 40($sp)
	lw	$9, 36($sp)
	lw	$8, 32($sp)
	lw	$7, 28($sp)
	lw	$6, 24($sp)
	lw	$5, 20($sp)
	lw	$4, 16($sp)
	lw	$3, 12($sp)
	lw	$2, 8($sp)
	lw	$1, 4($sp)
	lw	$0, ($sp)
	add3	$sp, $sp, 96
	reti

/*
 * vkSwitchContext(core, current, next): save current's callee-saved registers
 * and dispatch lock on its stack, and continue next where it switched out.
 */
	.global	vkSwitchContext
vkSwitchContext:
	lw	$9, VK_CORE_DISPATCH($1)
	sw	$9, VK_THREAD_DISPATCH($2)
	ldc	$0, $lp
	add	$sp, -24
	sw	$0, 20($sp)
	sw	$tp, 16($sp)
	sw	$8, 12($sp)
	sw	$7, 8($sp)
	sw	$6, 4($sp)
	sw	$5, ($sp)
	sw	$sp, VK_THREAD_SP($2)
	lw	$sp, VK_THREAD_SP($3)
	lw	$5, ($sp)
	lw	$6, 4($sp)
	lw	$7, 8($sp)
	lw	$8, 12($sp)
	lw	$tp, 16($sp)
	lw	$0, 20($sp)
	add	$sp, 24
	lw	$9, VK_THREAD_DISPATCH($3)
	sw	$9, VK_CORE_DISPATCH($1)
	jmp	$0

/* where a new thread starts: entry(argument, argument2), then its exit */
threadStart:
	ei
	mov	$1, $6
	mov	$2, $7
	jsr	$5
	bsr	vkThreadExit

/*
 * vkInitialFrame(top, entry, argument, argument2): the frame vkSwitchContext
 * pops to start a thread at entry; returns its stack pointer.
 */
	.global	vkInitialFrame
vkInitialFrame:
	add	$1, -24
	movu	$9, threadStart
	sw	$9, 20($1)
	sw	$2, ($1)
	sw	$3, 4($1)
	sw	$4, 8($1)
	mov	$0, $1
	ret

/* vkEndThread(thread, core, idleSp): leave the ending thread's stack, then finish it. */
	.global	vkEndThread
vkEndThread:
	mov	$sp, $3
	jmp	vkThreadFinish

/* vkCallOnStack(system, resource, top): vkVThreadServe(system, resource) on another stack. */
	.global	vkCallOnStack
vkCallOnStack:
	add	$sp, -8
	ldc	$0, $lp
	sw	$0, 4($sp)
	sw	$5, ($sp)
	mov	$5, $sp
	mov	$sp, $3
	bsr	vkVThreadServe
	mov	$sp, $5
	lw	$5, ($sp)
	lw	$0, 4($sp)
	add	$sp, 8
	jmp	$0

/*
 * vkPowerSuspendCore(thread, core): save the thread to resume here, power the
 * core down, and come back when it is powered up again.
 */
	.global	vkPowerSuspendCore
vkPowerSuspendCore:
	add	$sp, -12
	ldc	$0, $lp
	sw	$0, ($sp)
	sw	$5, 4($sp)
	sw	$6, 8($sp)
	mov	$5, $1
	mov	$6, $2
	bsr	vkModulesSuspend
	mov	$1, $5
	mov	$2, $6
	lw	$9, VK_CORE_DISPATCH($2)
	sw	$9, VK_THREAD_DISPATCH($1)
	add	$sp, -24
	movu	$0, resumed
	sw	$0, 20($sp)
	sw	$tp, 16($sp)
	sw	$8, 12($sp)
	sw	$7, 8($sp)
	sw	$6, 4($sp)
	sw	$5, ($sp)
	sw	$sp, VK_THREAD_SP($1)
	bsr	vkThreadHoldForResume
	bsr	vkPowerSaveInterrupts
	movu	$0, 1 << VNZ_INTERRUPT_DOORBELL
	stcb	$0, VNZ_CB_INTERRUPT_ENABLE
	movu	$0, 0
	stcb	$0, VNZ_CB_INTERRUPT_CLEAR
	bsr	vkCacheWritebackAll
	bsr	vkPowerCoreDown
	bsr	vkPowerWaitUp			/* the core was not powered down */
	mov	$1, $5
	bsr	vkThreadReleaseFromResume
	lw	$5, ($sp)
	lw	$6, 4($sp)
	lw	$7, 8($sp)
	lw	$8, 12($sp)
	lw	$tp, 16($sp)
	add	$sp, 24
	bra	1f
resumed:					/* powered up again: from vkSwitchContext */
	ldc	$0, $psw
	mov	$1, ~VNZ_PSW_IEP
	and	$0, $1
	stc	$0, $psw
	bsr	vkPowerWaitUp
1:	lw	$6, 8($sp)
	lw	$5, 4($sp)
	lw	$0, ($sp)
	add	$sp, 12
	jmp	$0
