/*
 * libSceSha224 (the SDK's static library, sha224.o): Sony's hand-written SHA-224
 * compression core (_sceSha224Core) and round constants (K_value).
 */
	.syntax unified
	.arch armv7-a
	.cpu cortex-a9
	.fpu neon
	.thumb

	.section .text.sceSha224, "ax", %progbits
	.align 1
	.global _sceSha224Core
	.thumb_func
	.type _sceSha224Core, %function
_sceSha224Core:
	.fnstart
	.save {r4, r5, r6, r7, r8, r9, sl, fp, lr}
	.pad #36
	stmdb	sp!, {r2, r4, r5, r6, r7, r8, r9, sl, fp, lr}
	vpush	{d8-d11}
	add.w	lr, r0, r1, lsl #6
	ldmia.w	r2, {r1, r3, r4, r5, r6, r7, r8, r9}
	movw	ip, #:lower16:K_value
	movt	ip, #:upper16:K_value
	vldmia	ip!, {d16-d23}
1:
	vld1.8	{d0-d3}, [r0]!
	vrev32.8	q0, q0
	movs	r2, #5
	vrev32.8	q1, q1
	vadd.i32	q12, q8, q0
	vadd.i32	q13, q9, q1
	vld1.8	{d4-d7}, [r0]!
	vmov	sl, fp, d24
	pld	[r0, #31]
	vrev32.8	q2, q2
	pld	[r0, #63]
	vrev32.8	q3, q3
	stmdb	sp!, {r0, r1, r3, r4, r5, r6, r7, r8, r9, ip, lr}
2:
	vshl.s32	d28, d7, #15
	and.w	lr, r7, r6
	vshl.s32	d29, d7, #13
	add	r9, lr
	vshr.u32	d24, d7, #10
	bic.w	lr, r8, r6
	vsri.32	d28, d7, #17
	add	r9, lr
	eor.w	lr, r6, r6, ror #19
	eor.w	lr, lr, r6, ror #5
	vsri.32	d29, d7, #19
	add	r9, sl
	add	r8, fp
	add.w	r9, r9, lr, ror #6
	veor	d24, d24, d28
	add	r5, r9
	vext.8	q5, q0, q1, #4
	eor.w	r0, r1, r3
	vext.8	q4, q2, q3, #4
	eor.w	fp, r1, r1, ror #20
	eor.w	lr, r5, r5, ror #19
	eor.w	fp, fp, r1, ror #11
	veor	d24, d24, d29
	eor.w	lr, lr, r5, ror #5
	vshl.s32	q15, q5, #14
	add.w	r9, r9, fp, ror #2
	vshl.s32	q14, q5, #25
	and.w	fp, r6, r5
	vadd.i32	q0, q0, q4
	add.w	r8, r8, lr, ror #6
	vshr.u32	q4, q5, #3
	bic.w	lr, r7, r5
	vsri.32	q15, q5, #18
	add	fp, lr
	and.w	lr, r0, r4
	add	r8, fp
	vmov	sl, fp, d25
	add	r9, lr
	veor	q4, q4, q15
	and.w	lr, r1, r3
	vsri.32	q14, q5, #7
	add	r4, r8
	add	r9, lr
	add	r8, lr
	ands.w	r0, r0, r9
	eor.w	lr, r9, r9, ror #20
	add	r8, r0
	eor.w	lr, lr, r9, ror #11
	veor	q5, q4, q14
	and.w	r0, r5, r4
	add.w	r8, r8, lr, ror #2
	add	r7, r0
	vadd.i32	q5, q0, q5
	bic.w	lr, r6, r4
	add	r7, lr
	eor.w	lr, r4, r4, ror #19
	vadd.i32	d8, d24, d10
	eor.w	lr, lr, r4, ror #5
	add	r7, sl
	add	r6, fp
	add.w	r7, r7, lr, ror #6
	add	r3, r7
	vshl.s32	d28, d8, #15
	eor.w	r0, r8, r9
	vshl.s32	d29, d8, #13
	and.w	fp, r0, r1
	vshr.u32	d25, d8, #10
	add	r7, fp
	vsri.32	d28, d8, #17
	eor.w	fp, r8, r8, ror #20
	vsri.32	d29, d8, #19
	eor.w	lr, r3, r3, ror #19
	veor	d25, d25, d28
	eor.w	fp, fp, r8, ror #11
	eor.w	lr, lr, r3, ror #5
	add.w	r7, r7, fp, ror #2
	veor	d25, d25, d29
	and.w	fp, r4, r3
	add.w	r6, r6, lr, ror #6
	bic.w	lr, r5, r3
	vadd.i32	q0, q12, q5
	add	lr, fp
	and.w	fp, r8, r9
	add	r6, lr
	add	r7, fp
	add	r1, r6
	add	r6, fp
	vmov	sl, fp, d26
	eor.w	lr, r7, r7, ror #20
	ands	r0, r7
	eor.w	lr, lr, r7, ror #11
	add	r6, r0
	vswp	q0, q2
	add.w	r6, r6, lr, ror #2
	vshl.s32	d28, d5, #15
	and.w	lr, r3, r1
	vshl.s32	d29, d5, #13
	add	r5, lr
	vshr.u32	d24, d5, #10
	bic.w	lr, r4, r1
	vsri.32	d28, d5, #17
	add	r5, lr
	eor.w	lr, r1, r1, ror #19
	eor.w	lr, lr, r1, ror #5
	vsri.32	d29, d5, #19
	add	r5, sl
	add	r4, fp
	add.w	r5, r5, lr, ror #6
	veor	d24, d24, d28
	add	r9, r5
	vext.8	q5, q1, q0, #4
	eor.w	r0, r6, r7
	vext.8	q4, q3, q2, #4
	eor.w	fp, r6, r6, ror #20
	eor.w	lr, r9, r9, ror #19
	eor.w	fp, fp, r6, ror #11
	veor	d24, d24, d29
	eor.w	lr, lr, r9, ror #5
	vshl.s32	q15, q5, #14
	add.w	r5, r5, fp, ror #2
	vshl.s32	q14, q5, #25
	and.w	fp, r1, r9
	vadd.i32	q1, q1, q4
	add.w	r4, r4, lr, ror #6
	vshr.u32	q4, q5, #3
	bic.w	lr, r3, r9
	vsri.32	q15, q5, #18
	add	fp, lr
	and.w	lr, r0, r8
	add	r4, fp
	vmov	sl, fp, d27
	add	r5, lr
	veor	q4, q4, q15
	and.w	lr, r6, r7
	vsri.32	q14, q5, #7
	add	r8, r4
	add	r5, lr
	add	r4, lr
	ands	r0, r5
	eor.w	lr, r5, r5, ror #20
	add	r4, r0
	eor.w	lr, lr, r5, ror #11
	veor	q5, q4, q14
	and.w	r0, r9, r8
	add.w	r4, r4, lr, ror #2
	add	r3, r0
	vadd.i32	q5, q1, q5
	bic.w	lr, r1, r8
	add	r3, lr
	eor.w	lr, r8, r8, ror #19
	vadd.i32	d8, d24, d10
	eor.w	lr, lr, r8, ror #5
	add	r3, sl
	add	r1, fp
	add.w	r3, r3, lr, ror #6
	add	r7, r3
	vshl.s32	d28, d8, #15
	eor.w	r0, r4, r5
	vshl.s32	d29, d8, #13
	and.w	fp, r0, r6
	vshr.u32	d25, d8, #10
	add	r3, fp
	vsri.32	d28, d8, #17
	eor.w	fp, r4, r4, ror #20
	vsri.32	d29, d8, #19
	eor.w	lr, r7, r7, ror #19
	veor	d25, d25, d28
	eor.w	fp, fp, r4, ror #11
	eor.w	lr, lr, r7, ror #5
	add.w	r3, r3, fp, ror #2
	veor	d25, d25, d29
	and.w	fp, r8, r7
	add.w	r1, r1, lr, ror #6
	bic.w	lr, r9, r7
	vadd.i32	q1, q12, q5
	add	lr, fp
	vldmia	ip!, {d24-d27}
	and.w	fp, r4, r5
	add	r1, lr
	add	r3, fp
	add	r6, r1
	add	r1, fp
	vadd.i32	q12, q12, q0
	eor.w	lr, r3, r3, ror #20
	ands	r0, r3
	eor.w	lr, lr, r3, ror #11
	add	r1, r0
	vadd.i32	q13, q13, q3
	add.w	r1, r1, lr, ror #2
	vmov	sl, fp, d24
	subs	r2, #1
	vswp	q1, q3
	bpl.w	2b
3:
	and.w	r0, r7, r6
	bic.w	lr, r8, r6
	add	r9, r0
	eor.w	r0, r6, r6, ror #19
	add	r9, lr
	eor.w	r0, r0, r6, ror #5
	add	r9, sl
	add	r8, fp
	add.w	r9, r9, r0, ror #6
	eor.w	r0, r1, r3
	add	r5, r9
	eor.w	ip, r5, r5, ror #19
	eor.w	lr, r1, r1, ror #20
	eor.w	ip, ip, r5, ror #5
	eor.w	lr, lr, r1, ror #11
	and.w	fp, r0, r4
	add.w	r8, r8, ip, ror #6
	add	r9, fp
	and.w	ip, r6, r5
	add.w	r9, r9, lr, ror #2
	vmov	sl, fp, d25
	add	r8, ip
	and.w	lr, r1, r3
	bic.w	ip, r7, r5
	add	r9, lr
	add	r8, ip
	ands.w	r0, r0, r9
	add	r4, r8
	eor.w	ip, r9, r9, ror #20
	add	lr, r0
	eor.w	ip, ip, r9, ror #11
	add	r8, lr
	add.w	r8, r8, ip, ror #2
	vadd.i32	q12, q2, q10
	and.w	r0, r5, r4
	bic.w	lr, r6, r4
	add	r7, r0
	eor.w	r0, r4, r4, ror #19
	add	r7, lr
	eor.w	r0, r0, r4, ror #5
	add	r7, sl
	add	r6, fp
	add.w	r7, r7, r0, ror #6
	eor.w	r0, r8, r9
	add	r3, r7
	eor.w	ip, r3, r3, ror #19
	eor.w	lr, r8, r8, ror #20
	eor.w	ip, ip, r3, ror #5
	eor.w	lr, lr, r8, ror #11
	and.w	fp, r0, r1
	add.w	r6, r6, ip, ror #6
	add	r7, fp
	and.w	ip, r4, r3
	add.w	r7, r7, lr, ror #2
	vmov	sl, fp, d26
	add	r6, ip
	and.w	lr, r8, r9
	bic.w	ip, r5, r3
	add	r7, lr
	add	r6, ip
	ands	r0, r7
	add	r1, r6
	eor.w	ip, r7, r7, ror #20
	add	lr, r0
	eor.w	ip, ip, r7, ror #11
	add	r6, lr
	add.w	r6, r6, ip, ror #2
	and.w	r0, r3, r1
	bic.w	lr, r4, r1
	add	r5, r0
	eor.w	r0, r1, r1, ror #19
	add	r5, lr
	eor.w	r0, r0, r1, ror #5
	add	r5, sl
	add	r4, fp
	add.w	r5, r5, r0, ror #6
	eor.w	r0, r6, r7
	add	r9, r5
	eor.w	ip, r9, r9, ror #19
	eor.w	lr, r6, r6, ror #20
	eor.w	ip, ip, r9, ror #5
	eor.w	lr, lr, r6, ror #11
	and.w	fp, r0, r8
	add.w	r4, r4, ip, ror #6
	add	r5, fp
	and.w	ip, r1, r9
	add.w	r5, r5, lr, ror #2
	vmov	sl, fp, d27
	add	r4, ip
	and.w	lr, r6, r7
	bic.w	ip, r3, r9
	add	r5, lr
	add	r4, ip
	ands	r0, r5
	add	r8, r4
	eor.w	ip, r5, r5, ror #20
	add	lr, r0
	eor.w	ip, ip, r5, ror #11
	add	r4, lr
	add.w	r4, r4, ip, ror #2
	and.w	r0, r9, r8
	bic.w	lr, r1, r8
	add	r3, r0
	eor.w	r0, r8, r8, ror #19
	add	r3, lr
	eor.w	r0, r0, r8, ror #5
	add	r3, sl
	add	r1, fp
	add.w	r3, r3, r0, ror #6
	eor.w	r0, r4, r5
	add	r7, r3
	eor.w	ip, r7, r7, ror #19
	eor.w	lr, r4, r4, ror #20
	eor.w	ip, ip, r7, ror #5
	eor.w	lr, lr, r4, ror #11
	and.w	fp, r0, r6
	add.w	r1, r1, ip, ror #6
	add	r3, fp
	and.w	ip, r8, r7
	add.w	r3, r3, lr, ror #2
	vmov	sl, fp, d24
	add	r1, ip
	and.w	lr, r4, r5
	bic.w	ip, r9, r7
	add	r3, lr
	add	r1, ip
	ands	r0, r3
	add	r6, r1
	eor.w	ip, r3, r3, ror #20
	add	lr, r0
	eor.w	ip, ip, r3, ror #11
	add	r1, lr
	add.w	r1, r1, ip, ror #2
	vadd.i32	q13, q3, q11
	adds	r2, #1
	ble.w	3b
	ldmia.w	sp!, {r0, r2, sl, fp, ip, lr}
	add	r1, r2
	add	r3, sl
	add	r4, fp
	add	r5, ip
	add	r6, lr
	ldmia.w	sp!, {r2, sl, fp, ip, lr}
	add	r7, r2
	add	r8, sl
	add	r9, fp
	cmp	r0, lr
	bne.w	1b
	ldr	r2, [sp, #32]
	stmia.w	r2, {r1, r3, r4, r5, r6, r7, r8, r9}
	vpop	{d8-d11}
	ldmia.w	sp!, {r1, r4, r5, r6, r7, r8, r9, sl, fp, pc}
	.fnend
	.size _sceSha224Core, .-_sceSha224Core

	.section .rodata.sceSha224, "a", %progbits
	.align 2
	.type K_value, %object
	.size K_value, 256
K_value:
	.word 0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5
	.word 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5
	.word 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208
	.word 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
	.word 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3
	.word 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174
	.word 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc
	.word 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da
	.word 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7
	.word 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967
	.word 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13
	.word 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85
	.word 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3
	.word 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070
	.word 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5
	.word 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3
