/*
 * setjmp and longjmp: the callee-saved registers, the stack pointer and the
 * return address, then d8-d15 and the FPSCR. The stack pointer and the
 * return address are kept xored with the jump buffer guard
 * (_Getjmpbufguard); longjmp restores the FPSCR's control bits only and
 * returns 1 for a value of 0.
 */
	.syntax unified
	.arch	armv7-a
	.fpu	neon
	.thumb
	.text

	.global	setjmp
	.type	setjmp, %function
	.thumb_func
setjmp:
	push	{r0, lr}
	bl	_Getjmpbufguard
	pop	{r1, r2}
	mov	lr, r2
	mov	r3, sp
	stmia.w	r1!, {r4-r11}
	eors	r2, r0
	eors	r0, r3
	stmia	r1!, {r0, r2}
	vstmia	r1!, {d8-d15}
	fmrx	r2, fpscr
	stmia	r1!, {r2}
	mov.w	r0, #0
	bx	lr
	.size	setjmp, . - setjmp

	.balign	4

	.global	longjmp
	.type	longjmp, %function
	.thumb_func
longjmp:
	push	{r0, r1}
	bl	_Getjmpbufguard
	mov	r2, r0
	pop	{r0, r1}
	ldmia.w	r0!, {r4-r12, lr}
	vldmia	r0!, {d8-d15}
	eor.w	ip, ip, r2
	eor.w	lr, lr, r2
	ldmia	r0!, {r2}
	mov	sp, ip
	fmrx	r3, fpscr
	/* the FPSCR's status bits stay, its control bits come from the buffer */
	movw	ip, #0x00ff
	movt	ip, #0xf800
	bic.w	r2, r2, ip
	and.w	r3, r3, ip
	orr.w	r2, r2, r3
	fmxr	fpscr, r2
	adds	r0, r1, #0
	it	eq
	moveq	r0, #1
	bx	lr
	.size	longjmp, . - longjmp
