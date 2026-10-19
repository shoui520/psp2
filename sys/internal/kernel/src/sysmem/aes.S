/*
 * Sony's hand-written Rijndael for SceKernelUtils. Each round looks up a
 * T-table column (aesEncryptionTable or aesDecryptionTable) for two state
 * columns at a time; 192- and 256-bit blocks keep the state in NEON
 * registers and shift the rows with vtbl through the ShiftRows tables. The
 * last round reads the S-box bytes: every fourth byte of the encryption
 * table from offset 2, or aesInverseSubstitution after the decryption table.
 * The round helpers fall through into one another and return with lr; the
 * entries park callee-saved registers in d0/d1.
 */
	.syntax unified
	.arch	armv7-a
	.fpu	neon
	.thumb
	.text

	.balign	2
/* SubWord: the S-box applied to each byte of a key-schedule word. */
	.global	SceKernelUtilsForDriver_60ED6EA9
	.type	SceKernelUtilsForDriver_60ED6EA9, %function
	.thumb_func
SceKernelUtilsForDriver_60ED6EA9:
	movw	r1, #:lower16:aesEncryptionTable+2
	movt	r1, #:upper16:aesEncryptionTable+2
	lsrs	r2, r0, #24
	ldrb.w	ip, [r1, r2, lsl #2]
	ubfx	r3, r0, #16, #8
	ldrb.w	r3, [r1, r3, lsl #2]
	ubfx	r2, r0, #8, #8
	ldrb.w	r2, [r1, r2, lsl #2]
	uxtb	r0, r0
	ldrb.w	r0, [r1, r0, lsl #2]
	bfi	r3, ip, #8, #8
	bfi	r2, r3, #8, #16
	bfi	r0, r2, #8, #24
	bx	lr
	.size	SceKernelUtilsForDriver_60ED6EA9, . - SceKernelUtilsForDriver_60ED6EA9

/* InvMixColumn of one column: carry-less multiplies by 0e,0b,0d,09, reduced by 0x11b. */
	.global	SceKernelUtilsForDriver_C76A7685
	.type	SceKernelUtilsForDriver_C76A7685, %function
	.thumb_func
SceKernelUtilsForDriver_C76A7685:
	movw	r1, #0x0b0e
	movt	r1, #0x090d
	mov.w	r2, r1, ror #24
	vmov	d0, r1, r2
	vdup.32	d4, r0
	vrev32.16	d2, d0
	vmull.p8	q0, d4, d0
	vmull.p8	q1, d4, d2
	vuzp.32	q0, q1
	veor	q0, q0, q1
	vuzp.16	d0, d1
	veor	d0, d0, d1
	vmov.i8	d4, #27
	vuzp.8	d1, d0
	vmul.p8	d0, d0, d4
	veor	d0, d0, d1
	vmov	r0, s1
	bx	lr
	.size	SceKernelUtilsForDriver_C76A7685, . - SceKernelUtilsForDriver_C76A7685

/* Encrypt one block with a compact context: the same as with a full one. */
	.global	SceKernelUtilsForDriver_302947B6
	.type	SceKernelUtilsForDriver_302947B6, %function
	.thumb_func
SceKernelUtilsForDriver_302947B6:
	b.n	SceKernelUtilsForDriver_C2A61770
	.size	SceKernelUtilsForDriver_302947B6, . - SceKernelUtilsForDriver_302947B6

/* Encrypt one block (r1 -> r2); the block width picks the 128-, 192- or 256-bit path. */
	.global	SceKernelUtilsForDriver_C2A61770
	.type	SceKernelUtilsForDriver_C2A61770, %function
	.thumb_func
SceKernelUtilsForDriver_C2A61770:
	push.w	{r4, r5, r6, r7, r8, lr}
	adds.w	ip, r0, #16
	ldrd	r3, r0, [r0, #8]
	subs	r0, #1
	cmp	r3, #4
	bne.w	.Lencrypt192
	vmov	d1, r2, fp
	vmov	d0, r9, sl
	ldr	r7, [r1, #12]
	ldr	r3, [r1, #8]
	ldr	r2, [r1, #4]
	ldr	r1, [r1, #0]
	ldmia.w	ip!, {r8, r9, sl, fp}
	movw	lr, #:lower16:aesEncryptionTable
	movt	lr, #:upper16:aesEncryptionTable
.Lencrypt128Round:
	eor.w	r7, r7, fp
	eor.w	r5, r2, r9
	eor.w	r4, r1, r8
	eor.w	r6, r3, sl
	ubfx	r8, r5, #8, #8
	uxtb	r1, r4
	ubfx	r9, r6, #16, #8
	ldr.w	r8, [lr, r8, lsl #2]
	lsrs	r2, r7, #24
	ldr.w	r1, [lr, r1, lsl #2]
	ldr.w	r9, [lr, r9, lsl #2]
	ldr.w	sl, [lr, r2, lsl #2]
	uxtb	r2, r5
	eor.w	r1, r1, r8, ror #24
	ubfx	r8, r6, #8, #8
	eor.w	r1, r1, r9, ror #16
	ldr.w	r8, [lr, r8, lsl #2]
	ubfx	r9, r7, #16, #8
	eor.w	r1, r1, sl, ror #8
	lsrs	r3, r4, #24
	ldr.w	r2, [lr, r2, lsl #2]
	ldr.w	r9, [lr, r9, lsl #2]
	ldr.w	sl, [lr, r3, lsl #2]
	uxtb	r3, r6
	eor.w	r2, r2, r8, ror #24
	ubfx	r8, r7, #8, #8
	eor.w	r2, r2, r9, ror #16
	ubfx	r9, r4, #16, #8
	eor.w	r2, r2, sl, ror #8
	mov.w	sl, r5, lsr #24
	ldr.w	r8, [lr, r8, lsl #2]
	ldr.w	r3, [lr, r3, lsl #2]
	ldr.w	r9, [lr, r9, lsl #2]
	ldr.w	sl, [lr, sl, lsl #2]
	uxtb	r7, r7
	eor.w	r3, r3, r8, ror #24
	ubfx	r4, r4, #8, #8
	eor.w	r3, r3, r9, ror #16
	ldr.w	r7, [lr, r7, lsl #2]
	ubfx	r5, r5, #16, #8
	lsrs	r6, r6, #24
	eor.w	r3, r3, sl, ror #8
	ldr.w	r4, [lr, r4, lsl #2]
	ldr.w	r5, [lr, r5, lsl #2]
	ldr.w	r6, [lr, r6, lsl #2]
	subs	r0, #1
	ldmia.w	ip!, {r8, r9, sl, fp}
	eor.w	r7, r7, r4, ror #24
	eor.w	r7, r7, r5, ror #16
	eor.w	r7, r7, r6, ror #8
	bne.n	.Lencrypt128Round
	eor.w	r4, r1, r8
	eor.w	r5, r2, r9
	eor.w	r6, r3, sl
	eor.w	r7, r7, fp
	add.w	lr, lr, #2
	uxtb	r1, r4
	ubfx	r8, r5, #8, #8
	ubfx	r9, r6, #16, #8
	lsrs	r2, r7, #24
	ldrb.w	r1, [lr, r1, lsl #2]
	ldrb.w	r8, [lr, r8, lsl #2]
	ldrb.w	r9, [lr, r9, lsl #2]
	ldrb.w	sl, [lr, r2, lsl #2]
	uxtb	r2, r5
	eor.w	r1, r1, r8, ror #24
	ubfx	r8, r6, #8, #8
	eor.w	r1, r1, r9, ror #16
	ubfx	r9, r7, #16, #8
	eor.w	r1, r1, sl, ror #8
	lsrs	r3, r4, #24
	ldrb.w	r2, [lr, r2, lsl #2]
	ldrb.w	r8, [lr, r8, lsl #2]
	ldrb.w	r9, [lr, r9, lsl #2]
	ldrb.w	sl, [lr, r3, lsl #2]
	uxtb	r3, r6
	eor.w	r2, r2, r8, ror #24
	ubfx	r8, r7, #8, #8
	eor.w	r2, r2, r9, ror #16
	ubfx	r9, r4, #16, #8
	eor.w	r2, r2, sl, ror #8
	mov.w	sl, r5, lsr #24
	ldrb.w	r3, [lr, r3, lsl #2]
	ldrb.w	r8, [lr, r8, lsl #2]
	ldrb.w	r9, [lr, r9, lsl #2]
	ldrb.w	sl, [lr, sl, lsl #2]
	uxtb	r7, r7
	eor.w	r3, r3, r8, ror #24
	ubfx	r4, r4, #8, #8
	eor.w	r3, r3, r9, ror #16
	ubfx	r5, r5, #16, #8
	eor.w	r3, r3, sl, ror #8
	lsrs	r6, r6, #24
	ldrb.w	r7, [lr, r7, lsl #2]
	ldrb.w	r4, [lr, r4, lsl #2]
	ldrb.w	r5, [lr, r5, lsl #2]
	ldrb.w	r6, [lr, r6, lsl #2]
	eor.w	r7, r7, r4, ror #24
	eor.w	r7, r7, r5, ror #16
	eor.w	r4, r7, r6, ror #8
	ldmia.w	ip!, {r5, r6, r7, r8}
	vmov	ip, fp, d1
	vmov	r9, sl, d0
	eors	r1, r5
	eors	r2, r6
	eors	r3, r7
	eors.w	r4, r4, r8
	stmia.w	ip!, {r1, r2, r3, r4}
	pop.w	{r4, r5, r6, r7, r8, pc}
.Lencrypt192:
	cmp	r3, #6
	bne.n	.Lencrypt256
	vld1.8	{d24-d26}, [r1]
	vldmia	ip!, {d20-d22}
	movw	r6, #:lower16:aesEncryptShiftRows192
	movt	r6, #:upper16:aesEncryptShiftRows192
	vld1.8	{d28-d30}, [r6 :64]!
.Lencrypt192Round:
	bl	AesWideAddRoundKey
	vmov	r5, r1, d25
	vmov	d24, r3, r4
	bl	AesColumnPairRound
	vmov	r5, r1, d26
	vmov	d25, r3, r4
	bl	AesColumnPairRound
	vmov	d26, r3, r4
	subs	r0, #1
	bne.n	.Lencrypt192Round
	veor	q8, q10, q12
	veor	d18, d22, d26
	bl	AesWideEncryptLastRound
	vmov	r5, r1, d25
	bl	AesEncryptLastColumnPair
	vmov	r5, r1, d26
	bl	AesEncryptLastColumnPair
	vdup.32	q12, r0
	vdup.32	q13, r0
	pop.w	{r4, r5, r6, r7, r8, pc}
.Lencrypt256:
	cmp	r3, #8
	bne.n	.LencryptBadBlock
	vld1.8	{d24-d27}, [r1]
	vldmia	ip!, {d20-d23}
	movw	r6, #:lower16:aesEncryptShiftRows256
	movt	r6, #:upper16:aesEncryptShiftRows256
	vld1.8	{d28-d31}, [r6 :64]
	add.w	r6, r6, #56
.Lencrypt256Round:
	veor	q8, q10, q12
	veor	q9, q11, q13
	vldmia	ip!, {d20-d23}
	vtbl.8	d27, {d16-d19}, d31
	bl	AesWideShiftRows
	vmov	r5, r1, d25
	vmov	d24, r3, r4
	bl	AesColumnPairRound
	vmov	r5, r1, d26
	vmov	d25, r3, r4
	bl	AesColumnPairRound
	vmov	r5, r1, d27
	vmov	d26, r3, r4
	bl	AesColumnPairRound
	vmov	d27, r3, r4
	subs	r0, #1
	bne.n	.Lencrypt256Round
	veor	q8, q10, q12
	veor	q9, q11, q13
	vtbl.8	d27, {d16-d19}, d31
	bl	AesWideEncryptLastRound
	vmov	r5, r1, d25
	bl	AesEncryptLastColumnPair
	vmov	r5, r1, d26
	bl	AesEncryptLastColumnPair
	vmov	r5, r1, d27
	bl	AesEncryptLastColumnPair
	vdup.32	q12, r0
	vdup.32	q13, r0
	pop.w	{r4, r5, r6, r7, r8, pc}
.LencryptBadBlock:
	mov.w	r0, #-1
	pop.w	{r4, r5, r6, r7, r8, pc}
	.size	SceKernelUtilsForDriver_C2A61770, . - SceKernelUtilsForDriver_C2A61770

/* Decrypt with a compact context, whose only schedule follows the header. */
	.global	SceKernelUtilsForDriver_E39CD272
	.type	SceKernelUtilsForDriver_E39CD272, %function
	.thumb_func
SceKernelUtilsForDriver_E39CD272:
	adds.w	ip, r0, #16
	b.n	.Ldecrypt
	.size	SceKernelUtilsForDriver_E39CD272, . - SceKernelUtilsForDriver_E39CD272

/* Decrypt one block with the decryption schedule of a full context. */
	.global	SceKernelUtilsForDriver_D8678061
	.type	SceKernelUtilsForDriver_D8678061, %function
	.thumb_func
SceKernelUtilsForDriver_D8678061:
	adds.w	ip, r0, #0x1f0
.Ldecrypt:
	push.w	{r4, r5, r6, r7, r8, lr}
	ldrd	r3, r0, [r0, #8]
	subs	r0, #1
	cmp	r3, #4
	bne.w	.Ldecrypt192
	vmov	d1, r2, fp
	vmov	d0, r9, sl
	ldr	r7, [r1, #12]
	ldr	r3, [r1, #8]
	ldr	r2, [r1, #4]
	ldr	r1, [r1, #0]
	ldmia.w	ip!, {r8, r9, sl, fp}
	movw	lr, #:lower16:aesDecryptionTable
	movt	lr, #:upper16:aesDecryptionTable
.Ldecrypt128Round:
	eor.w	r5, r2, r9
	eor.w	r7, r7, fp
	eor.w	r4, r1, r8
	eor.w	r6, r3, sl
	ubfx	r8, r7, #8, #8
	uxtb	r1, r4
	ubfx	r9, r6, #16, #8
	ldr.w	r8, [lr, r8, lsl #2]
	lsrs	r2, r5, #24
	ldr.w	r1, [lr, r1, lsl #2]
	ldr.w	r9, [lr, r9, lsl #2]
	ldr.w	sl, [lr, r2, lsl #2]
	uxtb	r2, r5
	eor.w	r1, r1, r8, ror #24
	ubfx	r8, r4, #8, #8
	eor.w	r1, r1, r9, ror #16
	ldr.w	r8, [lr, r8, lsl #2]
	ubfx	r9, r7, #16, #8
	eor.w	r1, r1, sl, ror #8
	lsrs	r3, r6, #24
	ldr.w	r2, [lr, r2, lsl #2]
	ldr.w	r9, [lr, r9, lsl #2]
	ldr.w	sl, [lr, r3, lsl #2]
	uxtb	r3, r6
	eor.w	r2, r2, r8, ror #24
	ubfx	r8, r5, #8, #8
	eor.w	r2, r2, r9, ror #16
	ubfx	r9, r4, #16, #8
	eor.w	r2, r2, sl, ror #8
	mov.w	sl, r7, lsr #24
	ldr.w	r8, [lr, r8, lsl #2]
	ldr.w	r3, [lr, r3, lsl #2]
	ldr.w	r9, [lr, r9, lsl #2]
	ldr.w	sl, [lr, sl, lsl #2]
	uxtb	r7, r7
	eor.w	r3, r3, r8, ror #24
	ubfx	r6, r6, #8, #8
	eor.w	r3, r3, r9, ror #16
	ldr.w	r7, [lr, r7, lsl #2]
	ubfx	r5, r5, #16, #8
	lsrs	r4, r4, #24
	eor.w	r3, r3, sl, ror #8
	ldr.w	r6, [lr, r6, lsl #2]
	ldr.w	r5, [lr, r5, lsl #2]
	ldr.w	r4, [lr, r4, lsl #2]
	subs	r0, #1
	ldmia.w	ip!, {r8, r9, sl, fp}
	eor.w	r7, r7, r6, ror #24
	eor.w	r7, r7, r5, ror #16
	eor.w	r7, r7, r4, ror #8
	bne.n	.Ldecrypt128Round
	eor.w	r4, r1, r8
	eor.w	r5, r2, r9
	eor.w	r6, r3, sl
	eor.w	r7, r7, fp
	add.w	lr, lr, #1024
	uxtb	r1, r4
	ubfx	r8, r7, #8, #8
	ubfx	r9, r6, #16, #8
	lsrs	r2, r5, #24
	ldrb.w	r1, [lr, r1]
	ldrb.w	r8, [lr, r8]
	ldrb.w	r9, [lr, r9]
	ldrb.w	sl, [lr, r2]
	uxtb	r2, r5
	eor.w	r1, r1, r8, ror #24
	ubfx	r8, r4, #8, #8
	eor.w	r1, r1, r9, ror #16
	ubfx	r9, r7, #16, #8
	eor.w	r1, r1, sl, ror #8
	lsrs	r3, r6, #24
	ldrb.w	r2, [lr, r2]
	ldrb.w	r8, [lr, r8]
	ldrb.w	r9, [lr, r9]
	ldrb.w	sl, [lr, r3]
	uxtb	r3, r6
	eor.w	r2, r2, r8, ror #24
	ubfx	r8, r5, #8, #8
	eor.w	r2, r2, r9, ror #16
	ubfx	r9, r4, #16, #8
	eor.w	r2, r2, sl, ror #8
	mov.w	sl, r7, lsr #24
	ldrb.w	r3, [lr, r3]
	ldrb.w	r8, [lr, r8]
	ldrb.w	r9, [lr, r9]
	ldrb.w	sl, [lr, sl]
	uxtb	r7, r7
	eor.w	r3, r3, r8, ror #24
	ubfx	r6, r6, #8, #8
	eor.w	r3, r3, r9, ror #16
	ubfx	r5, r5, #16, #8
	eor.w	r3, r3, sl, ror #8
	lsrs	r4, r4, #24
	ldrb.w	r7, [lr, r7]
	ldrb.w	r6, [lr, r6]
	ldrb.w	r5, [lr, r5]
	ldrb.w	r4, [lr, r4]
	eor.w	r7, r7, r6, ror #24
	eor.w	r7, r7, r5, ror #16
	eor.w	r4, r7, r4, ror #8
	ldmia.w	ip!, {r5, r6, r7, r8}
	vmov	ip, fp, d1
	vmov	r9, sl, d0
	eors	r1, r5
	eors	r2, r6
	eors	r3, r7
	eors.w	r4, r4, r8
	stmia.w	ip!, {r1, r2, r3, r4}
	pop.w	{r4, r5, r6, r7, r8, pc}
.Ldecrypt192:
	cmp	r3, #6
	bne.n	.Ldecrypt256
	vld1.8	{d24-d26}, [r1]
	vldmia	ip!, {d20-d22}
	movw	r6, #:lower16:aesDecryptShiftRows192
	movt	r6, #:upper16:aesDecryptShiftRows192
	vld1.8	{d28-d30}, [r6 :64]!
.Ldecrypt192Round:
	bl	AesWideAddRoundKey
	vmov	r5, r1, d25
	vmov	d24, r3, r4
	bl	AesColumnPairRound
	vmov	r5, r1, d26
	vmov	d25, r3, r4
	bl	AesColumnPairRound
	vmov	d26, r3, r4
	subs	r0, #1
	bne.n	.Ldecrypt192Round
	veor	q8, q10, q12
	veor	d18, d22, d26
	bl	AesWideDecryptLastRound
	vmov	r5, r1, d25
	bl	AesDecryptLastColumnPair
	vmov	r5, r1, d26
	bl	AesDecryptLastColumnPair
	vdup.32	q12, r0
	vdup.32	q13, r0
	pop.w	{r4, r5, r6, r7, r8, pc}
.Ldecrypt256:
	cmp	r3, #8
	bne.n	.LdecryptBadBlock
	vld1.8	{d24-d27}, [r1]
	vldmia	ip!, {d20-d23}
	movw	r6, #:lower16:aesDecryptShiftRows256
	movt	r6, #:upper16:aesDecryptShiftRows256
	vld1.8	{d28-d31}, [r6 :64]
	add.w	r6, r6, #56
.Ldecrypt256Round:
	veor	q8, q10, q12
	veor	q9, q11, q13
	vldmia	ip!, {d20-d23}
	vtbl.8	d27, {d16-d19}, d31
	bl	AesWideShiftRows
	vmov	r5, r1, d25
	vmov	d24, r3, r4
	bl	AesColumnPairRound
	vmov	r5, r1, d26
	vmov	d25, r3, r4
	bl	AesColumnPairRound
	vmov	r5, r1, d27
	vmov	d26, r3, r4
	bl	AesColumnPairRound
	vmov	d27, r3, r4
	subs	r0, #1
	bne.n	.Ldecrypt256Round
	veor	q8, q10, q12
	veor	q9, q11, q13
	vtbl.8	d27, {d16-d19}, d31
	bl	AesWideDecryptLastRound
	vmov	r5, r1, d25
	bl	AesDecryptLastColumnPair
	vmov	r5, r1, d26
	bl	AesDecryptLastColumnPair
	vmov	r5, r1, d27
	bl	AesDecryptLastColumnPair
	vdup.32	q12, r0
	vdup.32	q13, r0
	pop.w	{r4, r5, r6, r7, r8, pc}
.LdecryptBadBlock:
	mov.w	r0, #-1
	pop.w	{r4, r5, r6, r7, r8, pc}
	.size	SceKernelUtilsForDriver_D8678061, . - SceKernelUtilsForDriver_D8678061

/* 192/256-bit round helpers: d24-d27 hold the state, d20-d23 the round key, d28-d31
 * the ShiftRows indices; r6 the table, r5/r1 the column pair, r3/r4 its result. */
	.type	AesWideAddRoundKey, %function
	.thumb_func
AesWideAddRoundKey:
	veor	q8, q10, q12
	veor	d18, d22, d26
	vldmia	ip!, {d20-d22}
	.size	AesWideAddRoundKey, . - AesWideAddRoundKey

	.type	AesWideShiftRows, %function
	.thumb_func
AesWideShiftRows:
	vtbl.8	d24, {d16-d19}, d28
	vtbl.8	d25, {d16-d19}, d29
	vtbl.8	d26, {d16-d19}, d30
	vmov	r5, r1, d24
	.size	AesWideShiftRows, . - AesWideShiftRows

	.type	AesColumnPairRound, %function
	.thumb_func
AesColumnPairRound:
	ubfx	r7, r5, #16, #8
	lsrs	r3, r5, #24
	ubfx	r4, r5, #8, #8
	uxtb	r5, r5
	ldr.w	r7, [r6, r7, lsl #2]
	ldr.w	r3, [r6, r3, lsl #2]
	ldr.w	r4, [r6, r4, lsl #2]
	eor.w	r3, r3, r7, ror #8
	ldr.w	r5, [r6, r5, lsl #2]
	ubfx	r7, r1, #16, #8
	eor.w	r3, r3, r4, ror #16
	ubfx	r4, r1, #8, #8
	eor.w	r3, r5, r3, ror #8
	ldr.w	r7, [r6, r7, lsl #2]
	lsrs	r5, r1, #24
	uxtb	r1, r1
	ldr.w	r5, [r6, r5, lsl #2]
	ldr.w	r4, [r6, r4, lsl #2]
	ldr.w	r1, [r6, r1, lsl #2]
	eor.w	r5, r5, r7, ror #8
	eor.w	r5, r5, r4, ror #16
	eor.w	r4, r1, r5, ror #8
	bx	lr
	.size	AesColumnPairRound, . - AesColumnPairRound

	.type	AesWideEncryptLastRound, %function
	.thumb_func
AesWideEncryptLastRound:
	vtbl.8	d24, {d16-d19}, d28
	vtbl.8	d25, {d16-d19}, d29
	vtbl.8	d26, {d16-d19}, d30
	vmov	r5, r1, d24
	vdup.32	q9, r0
	vdup.32	q11, r0
	adds	r6, #2
	vdup.32	q8, r0
	vdup.32	q10, r0
	.size	AesWideEncryptLastRound, . - AesWideEncryptLastRound

	.type	AesEncryptLastColumnPair, %function
	.thumb_func
AesEncryptLastColumnPair:
	ubfx	r7, r5, #16, #8
	lsrs	r3, r5, #24
	ubfx	r4, r5, #8, #8
	uxtb	r5, r5
	ldrb.w	r3, [r6, r3, lsl #2]
	ldrb.w	r7, [r6, r7, lsl #2]
	ldrb.w	r4, [r6, r4, lsl #2]
	ldrb.w	r5, [r6, r5, lsl #2]
	eor.w	r7, r7, r3, lsl #8
	lsrs	r3, r1, #24
	eor.w	r4, r4, r7, lsl #8
	ubfx	r8, r1, #16, #8
	eor.w	r5, r5, r4, lsl #8
	ubfx	r4, r1, #8, #8
	uxtb	r1, r1
	ldrb.w	r3, [r6, r3, lsl #2]
	ldrb.w	r8, [r6, r8, lsl #2]
	ldrb.w	r4, [r6, r4, lsl #2]
	ldrb.w	r1, [r6, r1, lsl #2]
	eor.w	r8, r8, r3, lsl #8
	ldrd	r7, r3, [ip], #8
	eor.w	r4, r4, r8, lsl #8
	eors	r5, r7
	eors	r1, r3
	eor.w	r7, r1, r4, lsl #8
	stmia	r2!, {r5, r7}
	bx	lr
	.size	AesEncryptLastColumnPair, . - AesEncryptLastColumnPair

	.type	AesWideDecryptLastRound, %function
	.thumb_func
AesWideDecryptLastRound:
	vtbl.8	d24, {d16-d19}, d28
	vtbl.8	d25, {d16-d19}, d29
	vtbl.8	d26, {d16-d19}, d30
	vmov	r5, r1, d24
	vdup.32	q9, r0
	vdup.32	q11, r0
	addw	r6, r6, #1024
	vdup.32	q8, r0
	vdup.32	q10, r0
	.size	AesWideDecryptLastRound, . - AesWideDecryptLastRound

	.type	AesDecryptLastColumnPair, %function
	.thumb_func
AesDecryptLastColumnPair:
	ubfx	r7, r5, #16, #8
	lsrs	r3, r5, #24
	ubfx	r4, r5, #8, #8
	uxtb	r5, r5
	ldrb	r3, [r6, r3]
	ldrb	r7, [r6, r7]
	ldrb	r4, [r6, r4]
	ldrb	r5, [r6, r5]
	eor.w	r7, r7, r3, lsl #8
	lsrs	r3, r1, #24
	eor.w	r4, r4, r7, lsl #8
	ubfx	r7, r1, #16, #8
	eor.w	r5, r5, r4, lsl #8
	ubfx	r4, r1, #8, #8
	uxtb	r1, r1
	ldrb	r3, [r6, r3]
	ldrb	r7, [r6, r7]
	ldrb	r4, [r6, r4]
	ldrb	r1, [r6, r1]
	eor.w	r7, r7, r3, lsl #8
	ldrd	r8, r3, [ip], #8
	eor.w	r4, r4, r7, lsl #8
	eors.w	r5, r5, r8
	eors	r1, r3
	eor.w	r7, r1, r4, lsl #8
	stmia	r2!, {r5, r7}
	bx	lr
	.size	AesDecryptLastColumnPair, . - AesDecryptLastColumnPair
