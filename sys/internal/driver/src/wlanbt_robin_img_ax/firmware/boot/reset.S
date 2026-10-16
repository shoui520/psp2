/*
 * Marvell 88W8787 ("Robin") ARMv5TE reset, CP15, exception/interrupt entry,
 * ThreadX low-level initialization, and runtime helpers (0x00052480 .. 0x00052b54
 * and 0x00053c64 .. 0x00053ce8).
 */
	.syntax unified
	.arch armv5te

	.section .text.boot, "ax", %progbits
	.arm

/*
 * strcmp (0x0005248c): standard byte-by-byte string compare.
 */
	.global robinStrcmp
robinStrcmp:
1:
	ldrb	r2, [r0], #1
	ldrb	r3, [r1], #1
	cmp	r2, #1
	cmpcs	r2, r3
	bne	2f
	ldrb	r2, [r0], #1
	ldrb	r3, [r1], #1
	cmp	r2, #1
	cmpcs	r2, r3
	beq	1b
2:
	sub	r0, r2, r3
	bx	lr

/*
 * __aeabi_uwrite4 (0x000524c0): unaligned 32-bit little-endian store of r0 to [r1].
 */
	.global __aeabi_uwrite4
__aeabi_uwrite4:
	mov	r2, r0, lsr #8
	strb	r0, [r1]
	strb	r2, [r1, #1]
	mov	r2, r0, lsr #16
	strb	r2, [r1, #2]
	mov	r2, r0, lsr #24
	strb	r2, [r1, #3]
	bx	lr

/*
 * __rt_entry (0x000524e0): post-scatterload runtime entry. Sets up the C
 * library state and stack/heap, then enters main() in Thumb mode.
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
	ldr	r12, =main + 1
	bx	r12

/*
 * __aeabi_memmove (0x0005254c): overlapping-safe memory copy (r0=dst, r1=src, r2=len).
 */
	.global __aeabi_memmove
__aeabi_memmove:
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
	ldmdb	r1!, {r3, r4, r12, lr}
	subs	r2, r2, #16
	stmdb	r0!, {r3, r4, r12, lr}
	bcs	2b
	pop	{r4, lr}
3:
	movs	r3, r2, lsl #29
	ldmdbcs	r1!, {r3, r12}
	stmdbcs	r0!, {r3, r12}
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
 * __rt_stackheap_init (0x000525fc): initializes the initial SVC stack and heap
 * descriptors in g_rtLibSpace (0x0005ae54 .. 0x0005ae78).
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
	bx	lr

	.thumb
	.thumb_func
	.global __rt_lib_init
__rt_lib_init:
	push	{r4-r7, lr}
	sub	sp, sp, #0x14
	movs	r5, r0
	movs	r4, r1
	blx	__rt_clear_errno
	movs	r0, #0
	str	r5, [sp, #4]
	str	r4, [sp, #8]
	str	r0, [sp, #12]
	add	r0, sp, #4
	blx	__rt_lib_init_thunk
	movs	r4, r0
	add	r0, sp, #4
	movs	r5, r1
	str	r3, [sp]
	ldm	r0, {r0, r1}
	movs	r6, r2
	bl	robinInitHeapPool
	movs	r7, #0
	blx	__rt_get_locale_desc
	str	r0, [sp, #16]
	str	r7, [r0]
	movs	r1, #1
	str	r1, [r0, #4]
	str	r7, [r0, #8]
	str	r7, [r0, #12]
	str	r7, [r0, #16]
	ldr	r3, [sp]
	add	sp, sp, #0x14
	movs	r0, r4
	movs	r1, r5
	movs	r2, r6
	pop	{r4-r7, pc}

	.balign 4
	.arm
__rt_lib_DefaultArg:
	mvn	r0, #0
	mvn	r1, #2
	bx	lr

__rt_lib_PostInit:
	bx	lr

__rt_stackheap_setup:
	mov	r5, lr
	bl	__rt_get_libspace
	mov	lr, r5
	mov	r5, r0
	bic	r0, r0, #7
	mov	r1, sp
	mov	r3, r10
	mov	sp, r0
	add	sp, sp, #0x60
	push	{r5, lr}
	blx	__user_initial_stackheap
	pop	{r5, lr}
	mov	r6, #0
	mov	r7, #0
	mov	r8, #0
	mov	r11, #0
	bic	r1, r1, #7
	mov	r12, r5
	stm	r12!, {r6-r8, r11}
	stm	r12!, {r6-r8, r11}
	stm	r12!, {r6-r8, r11}
	stm	r12!, {r6-r8, r11}
	mov	sp, r1
	bx	lr

	.global __rt_get_heap_desc
__rt_get_heap_desc:
	ldr	r0, =g_rtHeapDesc
	bx	lr

	.global __rt_get_locale_desc
__rt_get_locale_desc:
	ldr	r0, =g_rtLocaleDesc
	bx	lr

	.thumb
	.thumb_func
	.global __rt_lib_init_noop
__rt_lib_init_noop:
	push	{r4}
	pop	{r4}
	bx	lr

	.thumb_func
	.global __user_initial_stackheap
__user_initial_stackheap:
	push	{r0-r4, lr}
	movs	r1, #0
	movs	r2, r1
	movs	r3, r1
	movs	r4, r1
	mov	r0, sp
	stm	r0!, {r1-r4}
	movs	r0, #0xbe
	lsls	r0, r0, #11		/* 0x0005f000 */
	adds	r0, #1
	lsls	r0, r0, #11		/* 0x0005f800: initial stack base */
	str	r0, [sp]
	bl	robinQueryHeapLimit
	str	r0, [sp, #4]
	mov	r0, sp
	ldm	r0, {r0-r3}
	add	sp, sp, #0x10
	pop	{r4, pc}

	.balign 4
	.arm
	.global __rt_get_libspace
__rt_get_libspace:
	ldr	r0, =g_rtLibSpace
	bx	lr

	.global __rt_get_errno_addr
__rt_get_errno_addr:
	ldr	r0, =g_rtErrno
	bx	lr

	.global __rt_clear_errno
__rt_clear_errno:
	push	{r4, lr}
	bl	__rt_get_errno_addr
	mov	r1, #0
	str	r1, [r0]
	pop	{r4, lr}
	bx	lr

/*
 * __aeabi_lmul (0x00052864): 64-bit integer multiplication helper.
 */
	.global __aeabi_lmul
__aeabi_lmul:
	umull	r3, r2, r1, r0
	mov	r0, r3
	mov	r1, r2
	bx	lr

/*
 * _tx_initialize_low_level (0x00052874): ThreadX low-level initialization hook
 * registered in DTCM (0x0400fcd8). Sets Undefined (0xdb), IRQ (0xd2) and SVC
 * (0xd3) mode stacks, ThreadX timer thread stack parameters, and hardware timer
 * 0x80003000 (period 0x1f3f = 8000 cycles, IRQ bit 4 in 0x80002808).
 */
	.thumb
	.thumb_func
	.global _tx_initialize_low_level
_tx_initialize_low_level:
	bx	pc
	nop
	.arm
_tx_initialize_low_level_arm:
	ldr	r1, =0x0005f330
	mov	r2, #0x10
	mov	r0, #0xdb		/* Undefined mode, IRQ+FIQ disabled */
	msr	cpsr_c, r0
	add	r1, r1, r2
	bic	r1, r1, #3
	sub	r1, r1, #4
	mov	sp, r1
	mov	r10, #0
	mov	r11, #0
	mov	r2, #0x400
	mov	r0, #0xd2		/* IRQ mode, IRQ+FIQ disabled */
	msr	cpsr_c, r0
	add	r1, r1, r2
	bic	r1, r1, #3
	sub	r1, r1, #4
	mov	sp, r1
	mov	r0, #0xd3		/* Supervisor mode, IRQ+FIQ disabled */
	msr	cpsr_c, r0
	ldr	r3, =0x0400fe5c		/* _tx_thread_system_stack_ptr */
	str	r1, [r3]
	ldr	r0, [r3]
	add	r0, r0, #4
	ldr	r1, =0x0400fd34		/* _tx_timer_stack_start */
	ldr	r3, =0x0400fd38		/* _tx_timer_stack_size */
	mov	r2, #0x3e0
	str	r0, [r1]
	str	r2, [r3]
	add	r0, r0, r2
	ldr	r1, =0x0400fd3c		/* _tx_timer_priority */
	mov	r2, #0
	str	r2, [r1]
	ldr	r2, =0x0400fd14		/* _tx_initialize_unused_memory */
	str	r0, [r2]
	ldr	r2, =0x80003000		/* SoC OS Timer base */
	ldr	r0, =0x1f3f
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
	ldr	r2, =0x80002800		/* SoC IRQ controller base */
	mov	r3, #0x10
	str	r3, [r2, #8]
	bx	lr

/*
 * Exception vector handlers (0x00052944 .. 0x000529c0).
 */
	.global vectorUndefined
vectorUndefined:
	movs	pc, lr

	.global vectorSwi
vectorSwi:
	push	{r0-r7, lr}
	mrs	r0, spsr
	push	{r0}
	ldr	r1, =0x800038b0
	ldr	r2, [r1]
	and	r2, r2, #0x1f
	ldr	r3, =romPatchSlots
	ldr	r3, [r3, r2, lsl #3]
	mov	lr, pc
	bx	r3
	ldr	r0, =romPatchSlots
	mov	r1, #8
	mul	r2, r1, r2		/* original encodes Rd == Rm */
	add	r2, r2, #4
	ldr	r1, [r0, r2]
	ldr	r2, [sp, #0x24]
	add	r2, r2, r1
	str	r2, [sp, #0x24]
	pop	{r0}
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
	ldr	pc, =_tx_thread_context_save

	.thumb
	.thumb_func
	.global sysIrqHook
sysIrqHook:
	bl	sysIrqDispatch
	ldr	r0, =_tx_thread_context_restore
	bx	r0

	.thumb_func
	.global vectorFiq
vectorFiq:
	mov	r11, lr
	bl	sysFatalHandler
	movs	r0, r0

/*
 * bootReset (0x000529c0): hardware entry from reset vector 0x00000000.
 */
	.balign 4
	.arm
	.global bootReset
bootReset:
	ldr	r0, =0x8000200c
	ldr	r1, =0x507e3823
	str	r1, [r0]
	ldr	r0, =0x90014000
	ldr	r1, [r0]
	ldr	r0, =0x80002044
	ldr	r1, [r0]
	ldr	r3, =0xc63c
	bic	r1, r1, r3
	str	r1, [r0]
	ldr	r6, =0x80002800
	mov	r3, #0x800000
	str	r3, [r6, #0x108]
	ldr	r0, =0x0005177c
	mcr	p15, 0, r0, c1, c0, 0
	ldr	pc, =__main

/*
 * Low-level CP15 and power-management primitives (0x00052a08 .. 0x00052ab4).
 */
	.global robinCpuWaitForInterrupt
robinCpuWaitForInterrupt:
	mcr	p15, 0, r0, c7, c0, 4
	bx	lr

	.global robinSocEnterSleep
robinSocEnterSleep:
	ldr	r0, =0x8000280c
	mvn	r1, #0
	str	r1, [r0]
	ldr	r0, =0x80002808
	ldr	r1, =0x400002
	str	r1, [r0]
	ldr	r0, =0x800020c4
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

	.global robinDelayLoop
robinDelayLoop:
	subs	r0, r0, #1
	bne	robinDelayLoop
	bx	lr

/*
 * sysRegisterRomHooks (0x00052a68): initializes the Mask ROM ThreadX hook
 * region in DTCM (0x0400fc24 .. 0x04010000) and registers _tx_initialize_low_level,
 * tx_application_define, and sysIrqHook.
 */
	.global sysRegisterRomHooks
sysRegisterRomHooks:
	ldr	r0, =0xf0f0f0f0
	ldr	r1, =0x0400fc24
	mov	r2, #0xf7
	str	r0, [r1], #4
	sub	r2, r2, #1
	mov	r0, #0
1:
	str	r0, [r1], #4
	subs	r2, r2, #1
	bne	1b
	ldr	r0, =0x0400fcd8
	ldr	r1, =_tx_initialize_low_level + 1
	str	r1, [r0]
	ldr	r0, =0x0400fcdc
	ldr	r1, =tx_application_define + 1
	str	r1, [r0]
	ldr	r0, =0x0400fce0
	ldr	r1, =sysIrqHook + 1
	str	r1, [r0]
	bx	lr

/*
 * __ARM_common_switch8 (0x00052ad4): Thumb-1 8-bit relative switch table helper.
 */
	.global __ARM_common_switch8
__ARM_common_switch8:
	ldrb	r12, [lr, #-1]
	cmp	r3, r12
	ldrbcc	r3, [lr, r3]
	ldrbcs	r3, [lr, r12]
	add	r12, lr, r3, lsl #1
	bx	r12

/*
 * robinClz16 (0x00053c64): count leading zeros of r0 (16-bit signed result).
 */
	.global robinClz16
robinClz16:
	clz	r0, r0
	mov	r0, r0, lsl #16
	mov	r0, r0, asr #16
	bx	lr
