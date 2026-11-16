/*
 * Marvell 88W8787 ("Robin") start-up, exit, exception entry, CP15 and ThreadX low-level code
 * (0x000524e0 .. 0x00052b54), instruction for instruction as the original has it.
 *
 * Most of it is the ARM C library's start-up (RVCT): __rt_entry, the stack and heap set-up and
 * the library initialisation, whose functions return their results in r0-r3 (__value_in_regs)
 * or move the stack pointer, which C cannot say. The rest is hand-written ARM code: the
 * exception vectors' targets, the reset entry, the mode stacks of _tx_initialize_low_level and
 * the CP15 accesses. The few small functions of the region a C compiler made (exit,
 * __rt_lib_shutdown, __rt_lib_init_noop) are in runtime/rtlib.c; the literal pools stay where
 * the original has them, since the loads of the code around them are relative to them.
 *
 * The names are the ARM C library's where it has one; the empty functions nothing calls and
 * __rt_exit_error are named by their place.
 */
	.syntax unified
	.arch armv5te

	.section .text.boot, "ax", %progbits
	.arm

/*
 * __rt_entry (0x000524e0): after the scatter load. Sets up the stack and heap and the library,
 * calls main() (Thumb) with the arguments __rt_lib_init returns, and exit() with its result.
 */
	.global __rt_entry
__rt_entry:
	nop
	bl	__rt_stackheap_init
	push	{r0, r1}
	bl	__rt_lib_DefaultArg
	pop	{r0, r1}
	blx	__rt_lib_init
	push	{r0-r3}
	bl	__rt_lib_PostInit
	pop	{r0-r3}
	ldr	ip, .Lmain_offset
	add	ip, ip, pc
.Lmain_base:
	tst	ip, #1
	addne	lr, pc, #13		/* .Lexit_call + 1 */
	moveq	lr, pc
	bx	ip
	add	ip, pc, #1		/* .Lexit_call + 1 */
	bx	ip
	.thumb
.Lexit_call:
	bl	exit
	.arm
.Lmain_offset:
	.word	main - .Lmain_base

/*
 * __rt_exit (0x0005252c): runs the library's shutdown and stops with the status in r0.
 */
	.global __rt_exit
__rt_exit:
	push	{r0, lr}
	nop
	blx	__rt_lib_shutdown
	pop	{r0, lr}
	b	.Lsys_exit

/* __rt_exit_error (0x00052540): stops with status -1. */
	.global __rt_exit_error
__rt_exit_error:
	mvn	r0, #0
.Lsys_exit:
	b	_sys_exit

/* An empty function (0x00052548); a relative word after _sys_exit points to it. */
__rt_exit_hook:
	bx	lr

/*
 * __aeabi_memmove4 (0x0005254c): memmove of word-aligned buffers (r0=dst, r1=src, r2=len).
 */
	.global __aeabi_memmove4
__aeabi_memmove4:
	subs	r3, r0, r1
	cmpcs	r2, r3
	bls	__aeabi_memcpy4
	add	r0, r0, r2
	tst	r0, #3
	add	r1, r1, r2
1:
	subne	r2, r2, #1
	ldrbne	r3, [r1, #-1]!
	strbne	r3, [r0, #-1]!
	tst	r0, #3
	bne	1b
	subs	r2, r2, #16
	bcc	3f
	push	{r4, lr}
2:
	ldmdb	r1!, {r3, r4, ip, lr}
	subs	r2, r2, #16
	stmdb	r0!, {r3, r4, ip, lr}
	bcs	2b
	pop	{r4, lr}
3:
	movs	r3, r2, lsl #29
	ldmdbcs	r1!, {r3, ip}
	stmdbcs	r0!, {r3, ip}
	ldrmi	r3, [r1, #-4]!
	strmi	r3, [r0, #-4]!
	tst	r2, #3
	bxeq	lr
	movs	r2, r2, lsl #31
	ldrhcs	r3, [r1, #-2]!
	ldrbmi	r2, [r1, #-1]
	strhcs	r3, [r0, #-2]!
	strbmi	r2, [r0, #-1]
	bx	lr

/*
 * _sys_exit (0x000525cc): the semihosting exit, angel_SWIreason_ReportException (0x18) with
 * ADP_Stopped_ApplicationExit (0x20026).
 */
	.global _sys_exit
_sys_exit:
	ldr	r1, .Lapplication_exit
	mov	r0, #0x18
	svc	0x123456
	bx	lr
	.word	__rt_exit_hook - .
.Lapplication_exit:
	.word	0x20026

/*
 * __rt_stackheap_init (0x000525fc): the initial stack, then the heap descriptor: base and top
 * from __user_initial_stackheap, 1 and 0x480 in its first and third words.
 */
	.global __rt_stackheap_init
__rt_stackheap_init:
	mov	r4, lr
	bl	__rt_stackheap_setup
	mov	lr, r4
	push	{r0, lr}
	bl	__rt_get_heap_desc
	mov	r4, r0
	pop	{r0, lr}
	push	{r0, r1, r4, lr}
	mov	r0, #1
	mov	r1, #0x480
	str	r1, [r4, #8]
	str	r0, [r4]
	pop	{r0, r1, r4, lr}
	mov	r1, r0
	str	r0, [r4, #4]
	mov	pc, lr

/*
 * __rt_heap_extend (0x0005263c): grows the heap by r0 bytes, up to the stack, and stores the
 * old top at r1. Returns the bytes it grew by (at least 4 KiB, 8-aligned), or 0 when the
 * request reaches the stack.
 */
	.global __rt_heap_extend
__rt_heap_extend:
	push	{r4, lr}
	push	{r0, r1}
	bl	__rt_get_heap_desc
	mov	r4, r0
	pop	{r0, r1}
	ldr	r2, [r4, #4]
	mov	r3, sp
	add	lr, r2, r0
	cmp	lr, r3
	str	r2, [r1]
	bhi	1f
	adds	r3, r3, lr
	add	r1, lr, #0x1000
	rrx	r3, r3
	add	r1, r1, #7
	bic	r3, r3, #7
	bic	r1, r1, #7
	cmp	r1, r3
	movhi	r1, r3
	str	r1, [r4, #4]
	pop	{r4, lr}
	sub	r0, r1, r2
	bx	lr
1:
	mov	r2, r0
	mov	r0, #0
	nop
	pop	{r4, lr}
	bx	lr

/*
 * __rt_lib_init (0x000526ac): clears errno, runs the library's initialisers, seeds rand() and
 * sets the locale descriptor up ("C": 0, 1, 0, 0, 0). Returns main()'s arguments in r0-r3.
 */
	.thumb
	.thumb_func
	.global __rt_lib_init
__rt_lib_init:
	push	{r4-r7, lr}
	sub	sp, sp, #0x14
	nop
	nop
	movs	r5, r0
	movs	r4, r1
	blx	__rt_clear_errno
	movs	r0, #0
	str	r5, [sp, #4]
	str	r4, [sp, #8]
	nop
	nop
	str	r0, [sp, #12]
	add	r0, sp, #4
	blx	__rt_lib_init_thunk
	movs	r4, r0
	add	r0, sp, #4
	movs	r5, r1
	str	r3, [sp]
	ldmia	r0, {r0, r1}
	movs	r6, r2
	nop
	nop
	nop
	nop
	bl	robinInitHeapPool
	movs	r1, #0
	movs	r0, r1
	nop
	nop
	movs	r7, r0
	blx	__rt_get_locale_desc
	str	r0, [sp, #16]
	movs	r1, #0
	str	r7, [r0]
	movs	r0, r1
	nop
	nop
	adds	r1, r0, #1
	ldr	r0, [sp, #16]
	str	r1, [r0, #4]
	movs	r1, #0
	movs	r0, r1
	nop
	nop
	movs	r1, r0
	ldr	r0, [sp, #16]
	str	r1, [r0, #8]
	movs	r1, #0
	movs	r0, r1
	nop
	nop
	movs	r1, r0
	ldr	r0, [sp, #16]
	str	r1, [r0, #12]
	movs	r1, #0
	movs	r0, r1
	nop
	nop
	movs	r1, r0
	ldr	r0, [sp, #16]
	str	r1, [r0, #16]
	.rept	18
	nop
	.endr
	ldr	r3, [sp]
	add	sp, sp, #0x14
	movs	r0, r4
	ldr	r4, [sp, #16]
	movs	r1, r5
	movs	r2, r6
	mov	lr, r4
	pop	{r4-r7}
	add	sp, sp, #4
	bx	lr

/* __rt_lib_DefaultArg (0x00052784): argc -1 and argv -3 until the library has set them. */
	.balign	4
	.arm
	.global __rt_lib_DefaultArg
__rt_lib_DefaultArg:
	mvn	r0, #0
	mvn	r1, #2
	bx	lr

/* __rt_lib_PostInit (0x00052790): nothing to do after the library initialisation. */
	.global __rt_lib_PostInit
__rt_lib_PostInit:
	push	{r4, lr}
	nop
	pop	{r4, lr}
	bx	lr

/*
 * __rt_stackheap_setup (0x000527a0): runs __user_initial_stackheap on a stack in the library
 * space, clears the first 64 bytes of that space and moves to the initial stack it returned.
 */
__rt_stackheap_setup:
	mov	r5, lr
	bl	__rt_get_libspace
	mov	lr, r5
	mov	r5, r0
	bic	r0, r0, #7
	mov	r1, sp
	mov	r3, sl
	mov	sp, r0
	add	sp, sp, #0x60
	push	{r5, lr}
	blx	__user_initial_stackheap
	pop	{r5, lr}
	mov	r6, #0
	mov	r7, #0
	mov	r8, #0
	mov	fp, #0
	bic	r1, r1, #7
	mov	ip, r5
	stmia	ip!, {r6-r8, fp}
	stmia	ip!, {r6-r8, fp}
	stmia	ip!, {r6-r8, fp}
	stmia	ip!, {r6-r8, fp}
	mov	sp, r1
	bx	lr

/* The library space's descriptors: the heap (0x00052800) and the locale (0x00052810). */
	.global __rt_get_heap_desc
__rt_get_heap_desc:
	ldr	r0, .Lheap_desc
	bx	lr
__rt_heap_desc_hook:
	bx	lr
.Lheap_desc:
	.word	__libspace_start + 0x10

	.global __rt_get_locale_desc
__rt_get_locale_desc:
	ldr	r0, .Llocale_desc
	bx	lr
__rt_locale_desc_hook:
	bx	lr
.Llocale_desc:
	.word	__libspace_start + 0x20

/*
 * __user_initial_stackheap (0x00052826): heap base 0x5f800 in the original, 0x63800 here (the
 * initial stack's top; GCC's code is 0x3df0 bytes larger than RVCT's), heap
 * limit from robinQueryHeapLimit(); returned in r0-r3 (__value_in_regs).
 */
	.thumb
	.thumb_func
	.global __user_initial_stackheap
__user_initial_stackheap:
	push	{r0-r4, lr}
	movs	r1, #0
	movs	r2, r1
	movs	r3, r1
	movs	r4, r1
	mov	r0, sp
	stmia	r0!, {r1-r4}
	movs	r0, #0xc7
	lsls	r0, r0, #11		/* 0x00063800: the original's 0x5f800 moved up by the larger code */
	str	r0, [sp]
	bl	robinQueryHeapLimit
	str	r0, [sp, #4]
	mov	r0, sp
	ldmia	r0, {r0-r3}
	add	sp, sp, #0x10
	pop	{r4, pc}

	.balign	4
	.arm
/* The library space (0x00052848) and errno in it (0x00052854). */
	.global __rt_get_libspace
__rt_get_libspace:
	ldr	r0, .Llibspace
	bx	lr
.Llibspace:
	.word	__libspace_start

	.global __rt_get_errno_addr
__rt_get_errno_addr:
	ldr	r0, .Lerrno
	bx	lr
__rt_errno_hook:
	bx	lr
.Lerrno:
	.word	__libspace_start + 4

/* __aeabi_lmul (0x00052864): the low 64 bits of a 64-bit product of 32-bit halves. */
	.global __aeabi_lmul
__aeabi_lmul:
	umull	r3, r2, r1, r0
	mov	r0, r3
	mov	r1, r2
	bx	lr

/*
 * _tx_initialize_low_level (0x00052874): the ThreadX low-level hook (installed in DTCM at
 * 0x0400fcd8). Sets the Undefined (0xdb), IRQ (0xd2) and Supervisor (0xd3) mode stacks above
 * 0x5f330, the timer thread's stack (0x3e0 bytes) and priority, the first unused byte, and
 * the SoC OS timer 0x80003000 (period 0x1f3f, IRQ bit 4 in 0x80002808).
 */
	.thumb
	.thumb_func
	.global _tx_initialize_low_level
_tx_initialize_low_level:
	add	r0, pc, #0
	bx	r0
	.arm
	ldr	r1, .Lmode_stacks
	ldr	r2, .Lundef_stack_size
	mov	r0, #0xdb		/* Undefined mode, IRQ and FIQ disabled */
	msr	cpsr_c, r0
	add	r1, r1, r2
	bic	r1, r1, #3
	sub	r1, r1, #4
	mov	sp, r1
	mov	sl, #0
	mov	fp, #0
	ldr	r2, .Lirq_stack_size
	mov	r0, #0xd2		/* IRQ mode, IRQ and FIQ disabled */
	msr	cpsr_c, r0
	add	r1, r1, r2
	bic	r1, r1, #3
	sub	r1, r1, #4
	mov	sp, r1
	mov	r0, #0xd3		/* Supervisor mode, IRQ and FIQ disabled */
	msr	cpsr_c, r0
	ldr	r3, .Lsystem_stack_ptr
	str	r1, [r3]
	ldr	r1, .Lsystem_stack_ptr
	ldr	r0, [r1]
	add	r0, r0, #4
	ldr	r1, .Ltimer_stack_start
	ldr	r3, .Ltimer_stack_size
	ldr	r2, .Ltimer_stack_bytes
	str	r0, [r1]
	str	r2, [r3]
	add	r0, r0, r2
	ldr	r1, .Ltimer_priority
	mov	r2, #0
	str	r2, [r1]
	ldr	r2, .Lunused_memory
	str	r0, [r2]
	ldr	r2, .Los_timer
	ldr	r0, .Los_timer_period
	str	r0, [r2, #0x2c]
	mov	r0, #7
	str	r0, [r2, #0x30]
	mov	r0, #9
	str	r0, [r2]
	mov	r1, #0x0b
	str	r1, [r2, #0x10]
	mov	r3, #1
	str	r3, [r2, #0x24]
	str	r3, [r2, #0x28]
	ldr	r2, .Lirq_controller
	mov	r3, #0x10
	str	r3, [r2, #8]
	bx	lr

/* Exception vector targets (0x00052944 .. 0x000529c0). */
	.global vectorUndefined
vectorUndefined:
	movs	pc, lr

/*
 * vectorSwi (0x00052948): a ROM patch trap. Calls romPatchSlots[n].target for the slot the
 * patch unit reports (0x800038b0, low 5 bits) and adds the slot's offset to the return address.
 */
	.global vectorSwi
vectorSwi:
	push	{r0-r7, lr}
	mrs	r0, spsr
	stmfd	sp!, {r0}
	ldr	r1, .Lpatch_hit
	ldr	r2, [r1]
	and	r2, r2, #0x1f
	ldr	r3, .Lpatch_slots
	ldr	r3, [r3, r2, lsl #3]
	mov	lr, pc
	bx	r3
	ldr	r0, .Lpatch_slots
	mov	r1, #8
	mul	r2, r2, r1
	mov	r1, #4
	add	r2, r2, r1
	ldr	r1, [r0, r2]
	ldr	r2, [sp, #0x24]
	add	r2, r2, r1
	str	r2, [sp, #0x24]
	ldmfd	sp!, {r0}
	msr	spsr_fc, r0
	ldm	sp!, {r0-r7, pc}^

	.global vectorPrefetchAbort
vectorPrefetchAbort:
	b	vectorPrefetchAbort

	.global vectorDataAbort
vectorDataAbort:
	b	vectorDataAbort

	.global vectorReserved
vectorReserved:
	b	vectorReserved

	.global vectorIrq
vectorIrq:
	b	_tx_thread_context_save

	.thumb
	.thumb_func
	.global sysIrqHook
sysIrqHook:
	bl	sysIrqDispatch
	ldr	r0, .Lcontext_restore
	bx	r0

	.thumb_func
	.global vectorFiq
vectorFiq:
	mov	fp, lr
	bl	sysFatalHandler
	movs	r0, r0

/*
 * bootReset (0x000529c0): the reset vector's target. Sets the SoC's clock and bus registers,
 * the CP15 control register (0x177c), and goes to __main.
 */
	.balign	4
	.arm
	.global bootReset
bootReset:
	ldr	r0, .Lsoc_clock
	ldr	r1, .Lsoc_clock_value
	str	r1, [r0]
	ldr	r0, .Lfifo_frame
	ldr	r1, [r0]
	ldr	r0, .Lsoc_channel
	ldr	r1, [r0]
	ldr	r3, .Lsoc_channel_clear
	bic	r1, r1, r3
	mov	r3, #0
	orr	r1, r1, r3
	str	r1, [r0]
	ldr	r6, .Lirq_controller
	mov	r3, #0x800000
	str	r3, [r6, #0x108]
	ldr	r0, .Lcp15_control
	mcr	p15, 0, r0, c1, c0, 0
	ldr	pc, .Lmain_entry

/* CP15 and power primitives (0x00052a08 .. 0x00052a68). */
	.global robinCpuWaitForInterrupt
robinCpuWaitForInterrupt:
	mcr	p15, 0, r0, c7, c0, 4
	bx	lr

	.global robinSocEnterSleep
robinSocEnterSleep:
	ldr	r0, .Lirq_clear
	mvn	r1, #0
	str	r1, [r0]
	ldr	r0, .Lirq_enable
	ldr	r1, .Lirq_wake
	str	r1, [r0]
	ldr	r0, .Lsoc_sleep
	ldr	r1, [r0]
	orr	r1, r1, #8
	str	r1, [r0]
1:
	ldr	r1, [r0]
	ands	r1, r1, #8
	beq	1b
	mcr	p15, 0, r0, c7, c0, 4
	bx	lr

	.global robinReadCp15Control
robinReadCp15Control:
	mrc	p15, 0, r0, c1, c0, 0
	bx	lr

	.global robinWriteCp15Control
robinWriteCp15Control:
	mcr	p15, 0, r0, c1, c0, 0
	bx	lr

/* robinDelayLoop (0x00052a5c): r0 turns of a two-instruction loop. */
	.global robinDelayLoop
robinDelayLoop:
	subs	r0, r0, #1
	bne	robinDelayLoop
	bx	lr

/*
 * sysRegisterRomHooks (0x00052a68): fills the Mask ROM's ThreadX hook area in DTCM
 * (0x0400fc24 .. 0x04010000) with a guard word and zeroes, and installs
 * _tx_initialize_low_level, tx_application_define and sysIrqHook.
 */
	.global sysRegisterRomHooks
sysRegisterRomHooks:
	ldr	r0, .Lhook_guard
	ldr	r1, .Lhook_area
	mov	r2, #0xf7
	str	r0, [r1], #4
	sub	r2, r2, #1
	mov	r0, #0
1:
	str	r0, [r1], #4
	subs	r2, r2, #1
	bne	1b
	ldr	r0, .Lhook_low_level
	ldr	r1, .Llow_level
	str	r1, [r0]
	ldr	r0, .Lhook_application
	ldr	r1, .Lapplication_define
	str	r1, [r0]
	ldr	r0, .Lhook_irq
	ldr	r1, .Lirq_hook
	str	r1, [r0]
	bx	lr

/* The ThreadX words _tx_initialize_low_level sets, and its stack sizes. */
.Lsystem_stack_ptr:
	.word	0x0400fe5c		/* _tx_thread_system_stack_ptr */
.Lundef_stack_size:
	.word	0x10
.Lirq_stack_size:
	.word	0x400
.Ltimer_stack_bytes:
	.word	0x3e0
.Lunused_memory:
	.word	0x0400fd14		/* _tx_initialize_unused_memory */
.Ltimer_stack_start:
	.word	0x0400fd34		/* _tx_timer_stack_start */
.Ltimer_stack_size:
	.word	0x0400fd38		/* _tx_timer_stack_size */
.Ltimer_priority:
	.word	0x0400fd3c		/* _tx_timer_priority */

/*
 * __ARM_common_switch8 (0x00052ad4): the compiler's Thumb switch over a table of byte offsets
 * after the call; the last entry is the default.
 */
#ifndef ROBIN_RVCT	/* RVCT emits it with the switches that call it */
	.global __ARM_common_switch8
__ARM_common_switch8:
	ldrb	ip, [lr, #-1]
	cmp	r3, ip
	ldrbcc	r3, [lr, r3]
	ldrbcs	r3, [lr, ip]
	add	ip, lr, r3, lsl #1
	bx	ip
#endif

.Lmode_stacks:
	.word	0x0005f330		/* the mode stacks start here */
.Los_timer:
	.word	0x80003000		/* SoC OS timer */
.Los_timer_period:
	.word	0x1f3f
.Lirq_controller:
	.word	0x80002800		/* SoC interrupt controller */
.Lpatch_hit:
	.word	0x800038b0		/* the ROM patch unit's hit slot */
.Lpatch_slots:
	.word	romPatchSlots
.Lcontext_restore:
	.word	_tx_thread_context_restore
.Lsoc_clock:
	.word	0x8000200c
.Lsoc_clock_value:
	.word	0x507e3823
.Lfifo_frame:
	.word	0x90014000
.Lsoc_channel:
	.word	0x80002044
.Lsoc_channel_clear:
	.word	0xc63c
.Lcp15_control:
	.word	0x177c
.Lmain_entry:
	.word	__main
.Lirq_clear:
	.word	0x8000280c
.Lirq_enable:
	.word	0x80002808
.Lirq_wake:
	.word	0x400002
.Lsoc_sleep:
	.word	0x800020c4
.Lhook_guard:
	.word	0xf0f0f0f0
.Lhook_area:
	.word	0x0400fc24
.Lhook_low_level:
	.word	0x0400fcd8
.Llow_level:
	.word	_tx_initialize_low_level
.Lhook_application:
	.word	0x0400fcdc
.Lapplication_define:
	.word	tx_application_define
.Lhook_irq:
	.word	0x0400fce0
.Lirq_hook:
	.word	sysIrqHook
