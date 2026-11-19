/*
 * setjmp and longjmp of libSceFt2's C library: r4-r12 and lr, d8-d15 and the FPSCR's control bits go
 * into the jump buffer; both leave the caller-saved registers and the NEON registers scrubbed with
 * fixed patterns.
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
	mov	ip, sp
	stmia.w	r0!, {r4-r12, lr}
	vstmia	r0!, {d8-d15}
	vmrs	r1, fpscr
	movw	ip, #0x00ff
	movt	ip, #0xf800
	bic.w	r1, r1, ip
	vmsr	fpscr, r1
	stmia	r0!, {r1}
	mov.w	r0, #0
	b	scrub
	.size	setjmp, . - setjmp

	.global	longjmp
	.type	longjmp, %function
	.thumb_func
longjmp:
	ldmia.w	r0!, {r4-r12, lr}
	vldmia	r0!, {d8-d15}
	ldmia	r0!, {r2}
	vmsr	fpscr, r2
	mov	sp, ip
	movs	r0, r1
	it	eq
	moveq	r0, #1
scrub:
	movw	r1, #0xdead
	movt	r1, #0x7f80
	movw	r2, #0xdead
	movt	r2, #0x7ff8
	vmov	d0, r1, r2
	vmov	d1, d0
	vmov	q1, q0
	vmov	q2, q0
	vmov	q3, q0
	vmov	q8, q0
	vmov	q9, q0
	vmov	q10, q0
	vmov	q11, q0
	vmov	q12, q0
	vmov	q13, q0
	vmov	q14, q0
	vmov	q15, q0
	movw	r1, #0xbeef
	movt	r1, #0xdead
	mov	r2, r1
	mov	r3, r1
	mov	ip, r1
	bx	lr
	.size	longjmp, . - longjmp
