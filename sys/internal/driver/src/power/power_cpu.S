/*
 * ScePower: the CPU context transfer around suspend and the secure-monitor
 * entries (banked mode registers, CPSR/SPSR, CP15 and SMC: what C cannot
 * express). Called from power.c.
 */
.syntax unified
.arch armv7-a
.arch_extension sec
.fpu neon
.thumb
.text

/* Entry after monitor restore; four CPUs rendezvous before stack setup. */
.global powerCpuResumeEntry
.type powerCpuResumeEntry, %function
.thumb_func
powerCpuResumeEntry:
	dsb	sy
	mov.w	r3, #0
	mcr	15, 0, r3, c13, c0, 1
	isb	sy
	mcr	15, 0, r0, c2, c0, 1
	isb	sy
	dsb	sy
	mcr	15, 0, r1, c13, c0, 1
	mcr	15, 0, r2, c3, c0, 1
	dsb	sy
	isb	sy
	mov.w	r0, #0
	mcr	15, 0, r0, c8, c7, 0
	mrc	15, 0, r0, c0, c0, 5
	ands.w	r0, r0, #15
	movw r1, #:lower16:s_powerCpuResumeHandshake
	movt r1, #:upper16:s_powerCpuResumeHandshake
	cmp	r0, #0
	it	eq
	streq	r0, [r1, #0]
1:
	ldrb	r2, [r1, #0]
	cmp	r0, r2
	itt	ne
	wfene
	bne.n	1b
	ldrh	r2, [r1, #0]
	adds	r2, #1
	adds.w	r2, r2, #256
	strh	r2, [r1, #0]
	dsb	sy
	sev
2:
	ldrb	r2, [r1, #1]
	cmp	r2, #4
	itt	ne
	wfene
	bne.n	2b
	mov.w	r0, r0, lsl #13
	movw r1, #:lower16:s_powerScratchPad
	movt r1, #:upper16:s_powerScratchPad
	ldr	r1, [r1, #0]
	adds	r0, r0, r1
	mov.w	r1, #8192
	mov.w	r2, #0
	mov.w	r3, #0
3:
	strd	r2, r3, [r0], #8
	subs	r1, #8
	bne.n	3b
	cpsid	aif, #31
	mov	sp, r0
	bl	powerResumeCpuContinuation
	nop
	bkpt	0x0000
.size powerCpuResumeEntry, . - powerCpuResumeEntry

.global powerSaveCpuContext
.type powerSaveCpuContext, %function
.thumb_func
powerSaveCpuContext:
	mov	ip, sp
	stmia.w	r0!, {r4, r5, r6, r7, r8, r9, sl, fp, ip, lr}
	mrs	ip, CPSR
	str.w	ip, [r0], #4
	cpsid	aif, #18
	mrs	r2, SPSR
	mov	r3, sp
	stmia.w	r0!, {r2, r3, lr}
	cps	#17
	mrs	r2, SPSR
	mov	r3, sp
	stmia.w	r0!, {r2, r3, r8, r9, sl, fp, ip, lr}
	cps	#19
	mrs	r2, SPSR
	mov	r3, sp
	stmia.w	r0!, {r2, r3, lr}
	cps	#23
	mrs	r2, SPSR
	mov	r3, sp
	stmia.w	r0!, {r2, r3, lr}
	cps	#27
	mrs	r2, SPSR
	mov	r3, sp
	stmia.w	r0!, {r2, r3, lr}
	msr	CPSR_fc, ip
	vmrs	r2, fpexc
	push	{r2, lr}
	mov.w	r3, #1073741824
	vmsr	fpexc, r3
	vmrs	r3, fpscr
	stmia	r0!, {r2, r3}
	vstmia	r0!, {d8-d15}
	mrc	15, 0, r1, c3, c0, 2
	mrc	15, 0, r2, c12, c0, 0
	mrc	15, 0, r3, c13, c0, 1
	stmia	r0!, {r1, r2, r3}
	mrc	15, 0, r1, c13, c0, 2
	mrc	15, 0, r2, c13, c0, 3
	mrc	15, 0, r3, c13, c0, 4
	stmia	r0!, {r1, r2, r3}
	bl	powerClobberVolatileRegisters
	ldmia.w	sp!, {r2, lr}
	vmsr	fpexc, r2
	mov.w	r0, #0
	bx	lr
.size powerSaveCpuContext, . - powerSaveCpuContext

.global powerRestoreCpuContext
.type powerRestoreCpuContext, %function
.thumb_func
powerRestoreCpuContext:
	ldmia.w	r0!, {r4, r5, r6, r7, r8, r9, sl, fp, ip, lr}
	mov	sp, ip
	ldr.w	ip, [r0], #4
	cpsid	aif, #18
	ldmia.w	r0!, {r2, r3, lr}
	msr	SPSR_fsxc, r2
	mov	sp, r3
	cps	#17
	ldmia.w	r0!, {r2, r3, r8, r9, sl, fp, ip, lr}
	msr	SPSR_fsxc, r2
	mov	sp, r3
	cps	#19
	ldmia.w	r0!, {r2, r3, lr}
	msr	SPSR_fsxc, r2
	mov	sp, r3
	cps	#23
	ldmia.w	r0!, {r2, r3, lr}
	msr	SPSR_fsxc, r2
	mov	sp, r3
	cps	#27
	ldmia.w	r0!, {r2, r3, lr}
	msr	SPSR_fsxc, r2
	mov	sp, r3
	msr	CPSR_fc, ip
	ldmia	r0!, {r2, r3}
	push	{r2, lr}
	mov.w	r2, #1073741824
	vmsr	fpexc, r2
	vmsr	fpscr, r3
	vldmia	r0!, {d8-d15}
	ldmia.w	r0!, {r2, r3, ip}
	mcr	15, 0, r2, c3, c0, 2
	mcr	15, 0, r3, c12, c0, 0
	mcr	15, 0, ip, c13, c0, 1
	ldmia.w	r0!, {r2, r3, ip}
	mcr	15, 0, r2, c13, c0, 2
	mcr	15, 0, r3, c13, c0, 3
	mcr	15, 0, ip, c13, c0, 4
	movs	r0, r1
	it	eq
	moveq	r0, #1
	bl	powerClobberVolatileRegisters
	ldmia.w	sp!, {r2, lr}
	vmsr	fpexc, r2
	bx	lr

powerClobberVolatileRegisters:
	movw	r1, #57005
	movt	r1, #32640
	movw	r2, #57005
	movt	r2, #32760
	vmov	d0, r1, r2
	vorr	d1, d0, d0
	vorr	q1, q0, q0
	vorr	q2, q0, q0
	vorr	q3, q0, q0
	vorr	q8, q0, q0
	vorr	q9, q0, q0
	vorr	q10, q0, q0
	vorr	q11, q0, q0
	vorr	q12, q0, q0
	vorr	q13, q0, q0
	vorr	q14, q0, q0
	vorr	q15, q0, q0
	movw	r1, #48879
	movt	r1, #57005
	mov	r2, r1
	mov	r3, r1
	mov	ip, r1
	bx	lr
.size powerRestoreCpuContext, . - powerRestoreCpuContext

.global powerSmcResumePrimary
.type powerSmcResumePrimary, %function
.thumb_func
powerSmcResumePrimary:
	movw	ip, #259
	smc	#0
	bx	lr
.size powerSmcResumePrimary, . - powerSmcResumePrimary

.global powerSmcSuspendSecondary
.type powerSmcSuspendSecondary, %function
.thumb_func
powerSmcSuspendSecondary:
	movw	ip, #271
	smc	#0
	bx	lr
.size powerSmcSuspendSecondary, . - powerSmcSuspendSecondary

.global powerSmcResumeSecondary
.type powerSmcResumeSecondary, %function
.thumb_func
powerSmcResumeSecondary:
	movw	ip, #279
	smc	#0
	bx	lr
.size powerSmcResumeSecondary, . - powerSmcResumeSecondary
