/*
 * libSceAes (the SDK's static library, aes.o): Sony's hand-written Rijndael
 * block cipher (sceAesEncrypt, sceAesDecrypt, _sceAesSubWord,
 * _sceAesInvMixColumn) and lookup tables (.rodata.sceAes).
 *
 * Each round looks up a T-table column (aesEncryptionTable or
 * aesDecryptionTable) for two state columns at a time; 192- and 256-bit blocks
 * keep the state in NEON registers and shift the rows with vtbl through the
 * ShiftRows tables. The last round reads the S-box bytes: every fourth byte of
 * the encryption table from offset 2, or aesInverseSubstitution after the
 * decryption table.
 */
	.syntax unified
	.arch	armv7-a
	.cpu	cortex-a9
	.fpu	neon
	.thumb

	.section .text.sceAes, "ax", %progbits
	.balign	4
#ifdef SCE_AES_2014
/*
 * 2014 libSceAes (SDK 0.996..3.10): Sony's earlier hand-written Thumb-2/NEON
 * Rijndael block cipher using GF(2^8) log/exp tables for InvMixColumn and
 * shared column-lookup helpers (aesMixColumnRound, aesLastRoundEncrypt,
 * aesLastRoundDecrypt) with r6 holding the active lookup table and ip the
 * final round-key word.
 */
	.global	_sceAesSubWord
	.type	_sceAesSubWord, %function
	.thumb_func
_sceAesSubWord:
	.fnstart
	movw	r1, #:lower16:aesEncryptionTable+2
	movt	r1, #:upper16:aesEncryptionTable+2
	ubfx	ip, r0, #24, #8
	ldrb.w	ip, [r1, ip, lsl #2]
	ubfx	r3, r0, #16, #8
	ldrb.w	r3, [r1, r3, lsl #2]
	ubfx	r2, r0, #8, #8
	ldrb.w	r2, [r1, r2, lsl #2]
	ubfx	r0, r0, #0, #8
	ldrb.w	r0, [r1, r0, lsl #2]
	bfi	r3, ip, #8, #8
	bfi	r2, r3, #8, #16
	bfi	r0, r2, #8, #24
	bx	lr
	.fnend
	.size	_sceAesSubWord, . - _sceAesSubWord

	.type	aesGfMulByte, %function
	.thumb_func
aesGfMulByte:
	.fnstart
	movw	r2, #:lower16:aesGfLogTable
	movt	r2, #:upper16:aesGfLogTable
	ldrb	r3, [r2, r0]
	tst.w	r0, #0
	it	ne
	movne	r0, r1
	cbz	r0, .LgfMulZero
	ldrb.w	ip, [r2, r1]
	adds.w	r3, r3, ip
	ubfx	ip, r3, #8, #1
	adds.w	r3, r3, ip
	ands.w	r3, r3, #0xff
	adds	r2, r2, r3
	ldrb.w	r0, [r2, #0x100]
	bx	lr
.LgfMulZero:
	ldrb	r0, [r2]
	bx	lr
	.fnend
	.size	aesGfMulByte, . - aesGfMulByte

	.type	aesGfMulWord, %function
	.thumb_func
aesGfMulWord:
	.fnstart
	.save	{r4, r5, r6, lr}
	push	{r4, r5, r6, lr}
	mov	r4, r0
	mov	r5, r1
	ubfx	r0, r4, #0, #8
	ubfx	r1, r5, #0, #8
	bl	aesGfMulByte
	mov	r6, r0
	ubfx	r0, r4, #8, #8
	ubfx	r1, r5, #8, #8
	bl	aesGfMulByte
	eors	r6, r0
	ubfx	r0, r4, #16, #8
	ubfx	r1, r5, #16, #8
	bl	aesGfMulByte
	eors	r6, r0
	ubfx	r0, r4, #24, #8
	ubfx	r1, r5, #24, #8
	bl	aesGfMulByte
	eors	r0, r6
	pop	{r4, r5, r6, pc}
	.fnend
	.size	aesGfMulWord, . - aesGfMulWord

	.global	_sceAesInvMixColumn
	.type	_sceAesInvMixColumn, %function
	.thumb_func
_sceAesInvMixColumn:
	.fnstart
	.save	{r4, r5, r6, lr}
	push	{r4, r5, r6, lr}
	mov	r4, r0
	movw	r5, #0x0d0b
	movt	r5, #0x0e09
	mov	r0, r4
	mov	r1, r5
	bl	aesGfMulWord
	lsl.w	r6, r0, #24
	mov	r0, r4
	ror.w	r1, r5, #8
	bl	aesGfMulWord
	bfi	r6, r0, #16, #8
	mov	r0, r4
	ror.w	r1, r5, #16
	bl	aesGfMulWord
	bfi	r6, r0, #8, #8
	mov	r0, r4
	ror.w	r1, r5, #24
	bl	aesGfMulWord
	orrs	r0, r6
	pop	{r4, r5, r6, pc}
	.fnend
	.size	_sceAesInvMixColumn, . - _sceAesInvMixColumn

	.global	sceAesEncrypt
	.type	sceAesEncrypt, %function
	.thumb_func
sceAesEncrypt:
	.fnstart
	ldr.w	ip, [r0, #8]
	cmp.w	ip, #4
	beq.n	aesEncrypt128
	cmp.w	ip, #6
	beq.w	aesEncrypt192
	cmp.w	ip, #8
	beq.w	aesEncrypt256
	mov.w	r0, #-1
	bx	lr
	.fnend
	.size	sceAesEncrypt, . - sceAesEncrypt

	.type	aesEncrypt128, %function
	.thumb_func
aesEncrypt128:
	.fnstart
	.save	{r4, r5, r6, r7, lr}
	push	{r4, r5, r6, r7, lr}
	movw	r6, #:lower16:aesEncryptionTable
	movt	r6, #:upper16:aesEncryptionTable
	mov	r7, r2
	adds.w	r4, r0, #16
	ldr	r5, [r0, #12]
	subs	r5, #1
	vld1.8	{d16-d17}, [r1]
	vldmia	r4!, {d18-d19}
	veor	q8, q8, q9
.Lenc128Loop:
	vmov.u8	r0, d16[0]
	vmov.u8	r1, d16[5]
	vmov.u8	r2, d17[2]
	vmov.u8	r3, d17[7]
	bl	aesMixColumnRound
	vmov.32	d20[0], r0
	vmov.u8	r0, d16[4]
	vmov.u8	r1, d17[1]
	vmov.u8	r2, d17[6]
	vmov.u8	r3, d16[3]
	bl	aesMixColumnRound
	vmov.32	d20[1], r0
	vmov.u8	r0, d17[0]
	vmov.u8	r1, d17[5]
	vmov.u8	r2, d16[2]
	vmov.u8	r3, d16[7]
	bl	aesMixColumnRound
	vmov.32	d21[0], r0
	vmov.u8	r0, d17[4]
	vmov.u8	r1, d16[1]
	vmov.u8	r2, d16[6]
	vmov.u8	r3, d17[3]
	bl	aesMixColumnRound
	vmov.32	d21[1], r0
	vldmia	r4!, {d18-d19}
	veor	q8, q9, q10
	subs	r5, #1
	bne.n	.Lenc128Loop
	adds	r6, #2
	vmov.u8	r0, d16[0]
	vmov.u8	r1, d16[5]
	vmov.u8	r2, d17[2]
	vmov.u8	r3, d17[7]
	ldr.w	ip, [r4]
	bl	aesLastRoundEncrypt
	vmov.32	d20[0], r0
	vmov.u8	r0, d16[4]
	vmov.u8	r1, d17[1]
	vmov.u8	r2, d17[6]
	vmov.u8	r3, d16[3]
	ldr.w	ip, [r4, #4]
	bl	aesLastRoundEncrypt
	vmov.32	d20[1], r0
	vmov.u8	r0, d17[0]
	vmov.u8	r1, d17[5]
	vmov.u8	r2, d16[2]
	vmov.u8	r3, d16[7]
	ldr.w	ip, [r4, #8]
	bl	aesLastRoundEncrypt
	vmov.32	d21[0], r0
	vmov.u8	r0, d17[4]
	vmov.u8	r1, d16[1]
	vmov.u8	r2, d16[6]
	vmov.u8	r3, d17[3]
	ldr.w	ip, [r4, #12]
	bl	aesLastRoundEncrypt
	vmov.32	d21[1], r0
	mov.w	r0, #0
	vst1.8	{d20-d21}, [r7]
	pop	{r4, r5, r6, r7, pc}
	.fnend
	.size	aesEncrypt128, . - aesEncrypt128

	.type	aesEncrypt192, %function
	.thumb_func
aesEncrypt192:
	.fnstart
	.save	{r4, r5, r6, r7, lr}
	push	{r4, r5, r6, r7, lr}
	movw	r6, #:lower16:aesEncryptionTable
	movt	r6, #:upper16:aesEncryptionTable
	mov	r7, r2
	adds.w	r4, r0, #16
	ldr	r5, [r0, #12]
	subs	r5, #1
	vld1.8	{d16-d18}, [r1]
	vldmia	r4!, {d20-d22}
	veor	q8, q8, q10
	veor	d18, d18, d22
.Lenc192Loop:
	vmov.u8	r0, d16[0]
	vmov.u8	r1, d16[5]
	vmov.u8	r2, d17[2]
	vmov.u8	r3, d17[7]
	bl	aesMixColumnRound
	vmov.32	d24[0], r0
	vmov.u8	r0, d16[4]
	vmov.u8	r1, d17[1]
	vmov.u8	r2, d17[6]
	vmov.u8	r3, d18[3]
	bl	aesMixColumnRound
	vmov.32	d24[1], r0
	vmov.u8	r0, d17[0]
	vmov.u8	r1, d17[5]
	vmov.u8	r2, d18[2]
	vmov.u8	r3, d18[7]
	bl	aesMixColumnRound
	vmov.32	d25[0], r0
	vmov.u8	r0, d17[4]
	vmov.u8	r1, d18[1]
	vmov.u8	r2, d18[6]
	vmov.u8	r3, d16[3]
	bl	aesMixColumnRound
	vmov.32	d25[1], r0
	vmov.u8	r0, d18[0]
	vmov.u8	r1, d18[5]
	vmov.u8	r2, d16[2]
	vmov.u8	r3, d16[7]
	bl	aesMixColumnRound
	vmov.32	d26[0], r0
	vmov.u8	r0, d18[4]
	vmov.u8	r1, d16[1]
	vmov.u8	r2, d16[6]
	vmov.u8	r3, d17[3]
	bl	aesMixColumnRound
	vmov.32	d26[1], r0
	vldmia	r4!, {d20-d22}
	veor	q8, q10, q12
	veor	d18, d22, d26
	subs	r5, #1
	bne.n	.Lenc192Loop
	vmov.u8	r0, d16[0]
	vmov.u8	r1, d16[5]
	vmov.u8	r2, d17[2]
	vmov.u8	r3, d17[7]
	ldr.w	ip, [r4]
	bl	aesLastRoundEncrypt
	vmov.32	d24[0], r0
	vmov.u8	r0, d16[4]
	vmov.u8	r1, d17[1]
	vmov.u8	r2, d17[6]
	vmov.u8	r3, d18[3]
	ldr.w	ip, [r4, #4]
	bl	aesLastRoundEncrypt
	vmov.32	d24[1], r0
	vmov.u8	r0, d17[0]
	vmov.u8	r1, d17[5]
	vmov.u8	r2, d18[2]
	vmov.u8	r3, d18[7]
	ldr.w	ip, [r4, #8]
	bl	aesLastRoundEncrypt
	vmov.32	d25[0], r0
	vmov.u8	r0, d17[4]
	vmov.u8	r1, d18[1]
	vmov.u8	r2, d18[6]
	vmov.u8	r3, d16[3]
	ldr.w	ip, [r4, #12]
	bl	aesLastRoundEncrypt
	vmov.32	d25[1], r0
	vmov.u8	r0, d18[0]
	vmov.u8	r1, d18[5]
	vmov.u8	r2, d16[2]
	vmov.u8	r3, d16[7]
	ldr.w	ip, [r4, #16]
	bl	aesLastRoundEncrypt
	vmov.32	d26[0], r0
	vmov.u8	r0, d18[4]
	vmov.u8	r1, d16[1]
	vmov.u8	r2, d16[6]
	vmov.u8	r3, d17[3]
	ldr.w	ip, [r4, #20]
	bl	aesLastRoundEncrypt
	vmov.32	d26[1], r0
	mov.w	r0, #0
	vst1.8	{d24-d26}, [r7]
	pop	{r4, r5, r6, r7, pc}
	.fnend
	.size	aesEncrypt192, . - aesEncrypt192

	.type	aesEncrypt256, %function
	.thumb_func
aesEncrypt256:
	.fnstart
	.save	{r4, r5, r6, r7, lr}
	push	{r4, r5, r6, r7, lr}
	movw	r6, #:lower16:aesEncryptionTable
	movt	r6, #:upper16:aesEncryptionTable
	mov	r7, r2
	adds.w	r4, r0, #16
	ldr	r5, [r0, #12]
	subs	r5, #1
	vld1.8	{d16-d19}, [r1]
	vldmia	r4!, {d20-d23}
	veor	q8, q8, q10
	veor	q9, q9, q11
.Lenc256Loop:
	vmov.u8	r0, d16[0]
	vmov.u8	r1, d16[5]
	vmov.u8	r2, d17[2]
	vmov.u8	r3, d17[7]
	bl	aesMixColumnRound
	vmov.32	d24[0], r0
	vmov.u8	r0, d16[4]
	vmov.u8	r1, d17[1]
	vmov.u8	r2, d17[6]
	vmov.u8	r3, d18[3]
	bl	aesMixColumnRound
	vmov.32	d24[1], r0
	vmov.u8	r0, d17[0]
	vmov.u8	r1, d17[5]
	vmov.u8	r2, d18[2]
	vmov.u8	r3, d18[7]
	bl	aesMixColumnRound
	vmov.32	d25[0], r0
	vmov.u8	r0, d17[4]
	vmov.u8	r1, d18[1]
	vmov.u8	r2, d18[6]
	vmov.u8	r3, d19[3]
	bl	aesMixColumnRound
	vmov.32	d25[1], r0
	vmov.u8	r0, d18[0]
	vmov.u8	r1, d18[5]
	vmov.u8	r2, d19[2]
	vmov.u8	r3, d19[7]
	bl	aesMixColumnRound
	vmov.32	d26[0], r0
	vmov.u8	r0, d18[4]
	vmov.u8	r1, d19[1]
	vmov.u8	r2, d19[6]
	vmov.u8	r3, d16[3]
	bl	aesMixColumnRound
	vmov.32	d26[1], r0
	vmov.u8	r0, d19[0]
	vmov.u8	r1, d19[5]
	vmov.u8	r2, d16[2]
	vmov.u8	r3, d16[7]
	bl	aesMixColumnRound
	vmov.32	d27[0], r0
	vmov.u8	r0, d19[4]
	vmov.u8	r1, d16[1]
	vmov.u8	r2, d16[6]
	vmov.u8	r3, d17[3]
	bl	aesMixColumnRound
	vmov.32	d27[1], r0
	vldmia	r4!, {d20-d23}
	veor	q8, q10, q12
	veor	q9, q11, q13
	subs	r5, #1
	bne.n	.Lenc256Loop
	vmov.u8	r0, d16[0]
	vmov.u8	r1, d16[5]
	vmov.u8	r2, d17[2]
	vmov.u8	r3, d17[7]
	ldr.w	ip, [r4]
	bl	aesLastRoundEncrypt
	vmov.32	d24[0], r0
	vmov.u8	r0, d16[4]
	vmov.u8	r1, d17[1]
	vmov.u8	r2, d17[6]
	vmov.u8	r3, d18[3]
	ldr.w	ip, [r4, #4]
	bl	aesLastRoundEncrypt
	vmov.32	d24[1], r0
	vmov.u8	r0, d17[0]
	vmov.u8	r1, d17[5]
	vmov.u8	r2, d18[2]
	vmov.u8	r3, d18[7]
	ldr.w	ip, [r4, #8]
	bl	aesLastRoundEncrypt
	vmov.32	d25[0], r0
	vmov.u8	r0, d17[4]
	vmov.u8	r1, d18[1]
	vmov.u8	r2, d18[6]
	vmov.u8	r3, d19[3]
	ldr.w	ip, [r4, #12]
	bl	aesLastRoundEncrypt
	vmov.32	d25[1], r0
	vmov.u8	r0, d18[0]
	vmov.u8	r1, d18[5]
	vmov.u8	r2, d19[2]
	vmov.u8	r3, d19[7]
	ldr.w	ip, [r4, #16]
	bl	aesLastRoundEncrypt
	vmov.32	d27[0], r0
	vmov.u8	r0, d18[4]
	vmov.u8	r1, d19[1]
	vmov.u8	r2, d19[6]
	vmov.u8	r3, d16[3]
	ldr.w	ip, [r4, #20]
	bl	aesLastRoundEncrypt
	vmov.32	d27[1], r0
	vmov.u8	r0, d19[0]
	vmov.u8	r1, d19[5]
	vmov.u8	r2, d16[2]
	vmov.u8	r3, d16[7]
	ldr.w	ip, [r4, #24]
	bl	aesLastRoundEncrypt
	vmov.32	d27[0], r0
	vmov.u8	r0, d19[4]
	vmov.u8	r1, d16[1]
	vmov.u8	r2, d16[6]
	vmov.u8	r3, d17[3]
	ldr.w	ip, [r4, #28]
	bl	aesLastRoundEncrypt
	vmov.32	d27[1], r0
	mov.w	r0, #0
	vst1.8	{d24-d27}, [r7]
	pop	{r4, r5, r6, r7, pc}
	.fnend
	.size	aesEncrypt256, . - aesEncrypt256

	.global	sceAesDecrypt
	.type	sceAesDecrypt, %function
	.thumb_func
sceAesDecrypt:
	.fnstart
	ldr.w	ip, [r0, #8]
	cmp.w	ip, #4
	beq.n	aesDecrypt128
	cmp.w	ip, #6
	beq.w	aesDecrypt192
	cmp.w	ip, #8
	beq.w	aesDecrypt256
	mov.w	r0, #-1
	bx	lr
	.fnend
	.size	sceAesDecrypt, . - sceAesDecrypt

	.type	aesDecrypt128, %function
	.thumb_func
aesDecrypt128:
	.fnstart
	.save	{r4, r5, r6, r7, lr}
	push	{r4, r5, r6, r7, lr}
	movw	r6, #:lower16:aesDecryptionTable
	movt	r6, #:upper16:aesDecryptionTable
	mov	r7, r2
	adds.w	r4, r0, #0x1f0
	ldr	r5, [r0, #12]
	subs	r5, #1
	vld1.8	{d16-d17}, [r1]
	vldmia	r4!, {d18-d19}
	veor	q8, q8, q9
.Ldec128Loop:
	vmov.u8	r0, d16[0]
	vmov.u8	r1, d17[5]
	vmov.u8	r2, d17[2]
	vmov.u8	r3, d16[7]
	bl	aesMixColumnRound
	vmov.32	d20[0], r0
	vmov.u8	r0, d16[4]
	vmov.u8	r1, d16[1]
	vmov.u8	r2, d17[6]
	vmov.u8	r3, d17[3]
	bl	aesMixColumnRound
	vmov.32	d20[1], r0
	vmov.u8	r0, d17[0]
	vmov.u8	r1, d16[5]
	vmov.u8	r2, d16[2]
	vmov.u8	r3, d17[7]
	bl	aesMixColumnRound
	vmov.32	d21[0], r0
	vmov.u8	r0, d17[4]
	vmov.u8	r1, d17[1]
	vmov.u8	r2, d16[6]
	vmov.u8	r3, d16[3]
	bl	aesMixColumnRound
	vmov.32	d21[1], r0
	vldmia	r4!, {d18-d19}
	veor	q8, q9, q10
	subs	r5, #1
	bne.n	.Ldec128Loop
	movw	r6, #:lower16:aesInverseSubstitution
	movt	r6, #:upper16:aesInverseSubstitution
	vmov.u8	r0, d16[0]
	vmov.u8	r1, d17[5]
	vmov.u8	r2, d17[2]
	vmov.u8	r3, d16[7]
	ldr.w	ip, [r4]
	bl	aesLastRoundDecrypt
	vmov.32	d20[0], r0
	vmov.u8	r0, d16[4]
	vmov.u8	r1, d16[1]
	vmov.u8	r2, d17[6]
	vmov.u8	r3, d17[3]
	ldr.w	ip, [r4, #4]
	bl	aesLastRoundDecrypt
	vmov.32	d20[1], r0
	vmov.u8	r0, d17[0]
	vmov.u8	r1, d16[5]
	vmov.u8	r2, d16[2]
	vmov.u8	r3, d17[7]
	ldr.w	ip, [r4, #8]
	bl	aesLastRoundDecrypt
	vmov.32	d21[0], r0
	vmov.u8	r0, d17[4]
	vmov.u8	r1, d17[1]
	vmov.u8	r2, d16[6]
	vmov.u8	r3, d16[3]
	ldr.w	ip, [r4, #12]
	bl	aesLastRoundDecrypt
	vmov.32	d21[1], r0
	mov.w	r0, #0
	vst1.8	{d20-d21}, [r7]
	pop	{r4, r5, r6, r7, pc}
	.fnend
	.size	aesDecrypt128, . - aesDecrypt128

	.type	aesDecrypt192, %function
	.thumb_func
aesDecrypt192:
	.fnstart
	.save	{r4, r5, r6, r7, lr}
	push	{r4, r5, r6, r7, lr}
	movw	r6, #:lower16:aesDecryptionTable
	movt	r6, #:upper16:aesDecryptionTable
	mov	r7, r2
	adds.w	r4, r0, #0x1f0
	ldr	r5, [r0, #12]
	subs	r5, #1
	vld1.8	{d16-d18}, [r1]
	vldmia	r4!, {d20-d22}
	veor	q8, q8, q10
	veor	d18, d18, d22
.Ldec192Loop:
	vmov.u8	r0, d16[0]
	vmov.u8	r1, d18[5]
	vmov.u8	r2, d18[2]
	vmov.u8	r3, d17[7]
	bl	aesMixColumnRound
	vmov.32	d20[0], r0
	vmov.u8	r0, d16[4]
	vmov.u8	r1, d16[1]
	vmov.u8	r2, d18[6]
	vmov.u8	r3, d18[3]
	bl	aesMixColumnRound
	vmov.32	d20[1], r0
	vmov.u8	r0, d17[0]
	vmov.u8	r1, d16[5]
	vmov.u8	r2, d16[2]
	vmov.u8	r3, d18[7]
	bl	aesMixColumnRound
	vmov.32	d21[0], r0
	vmov.u8	r0, d17[4]
	vmov.u8	r1, d17[1]
	vmov.u8	r2, d16[6]
	vmov.u8	r3, d16[3]
	bl	aesMixColumnRound
	vmov.32	d21[1], r0
	vmov.u8	r0, d18[0]
	vmov.u8	r1, d17[5]
	vmov.u8	r2, d17[2]
	vmov.u8	r3, d16[7]
	bl	aesMixColumnRound
	vmov.32	d22[0], r0
	vmov.u8	r0, d18[4]
	vmov.u8	r1, d18[1]
	vmov.u8	r2, d17[6]
	vmov.u8	r3, d17[3]
	bl	aesMixColumnRound
	vmov.32	d22[1], r0
	vldmia	r4!, {d20-d22}
	veor	q8, q10, q12
	veor	d18, d22, d26
	subs	r5, #1
	bne.n	.Ldec192Loop
	movw	r6, #:lower16:aesInverseSubstitution
	movt	r6, #:upper16:aesInverseSubstitution
	vmov.u8	r0, d16[0]
	vmov.u8	r1, d18[5]
	vmov.u8	r2, d18[2]
	vmov.u8	r3, d17[7]
	ldr.w	ip, [r4]
	bl	aesLastRoundDecrypt
	vmov.32	d20[0], r0
	vmov.u8	r0, d16[4]
	vmov.u8	r1, d16[1]
	vmov.u8	r2, d18[6]
	vmov.u8	r3, d18[3]
	ldr.w	ip, [r4, #4]
	bl	aesLastRoundDecrypt
	vmov.32	d20[1], r0
	vmov.u8	r0, d17[0]
	vmov.u8	r1, d16[5]
	vmov.u8	r2, d16[2]
	vmov.u8	r3, d18[7]
	ldr.w	ip, [r4, #8]
	bl	aesLastRoundDecrypt
	vmov.32	d21[0], r0
	vmov.u8	r0, d17[4]
	vmov.u8	r1, d17[1]
	vmov.u8	r2, d16[6]
	vmov.u8	r3, d16[3]
	ldr.w	ip, [r4, #12]
	bl	aesLastRoundDecrypt
	vmov.32	d21[1], r0
	vmov.u8	r0, d18[0]
	vmov.u8	r1, d17[5]
	vmov.u8	r2, d17[2]
	vmov.u8	r3, d16[7]
	ldr.w	ip, [r4, #16]
	bl	aesLastRoundDecrypt
	vmov.32	d22[0], r0
	vmov.u8	r0, d18[4]
	vmov.u8	r1, d18[1]
	vmov.u8	r2, d17[6]
	vmov.u8	r3, d17[3]
	ldr.w	ip, [r4, #20]
	bl	aesLastRoundDecrypt
	vmov.32	d22[1], r0
	mov.w	r0, #0
	vst1.8	{d20-d22}, [r7]
	pop	{r4, r5, r6, r7, pc}
	.fnend
	.size	aesDecrypt192, . - aesDecrypt192

	.type	aesDecrypt256, %function
	.thumb_func
aesDecrypt256:
	.fnstart
	.save	{r4, r5, r6, r7, lr}
	push	{r4, r5, r6, r7, lr}
	movw	r6, #:lower16:aesDecryptionTable
	movt	r6, #:upper16:aesDecryptionTable
	mov	r7, r2
	adds.w	r4, r0, #0x1f0
	ldr	r5, [r0, #12]
	subs	r5, #1
	vld1.8	{d16-d19}, [r1]
	vldmia	r4!, {d20-d23}
	veor	q8, q8, q10
	veor	q9, q9, q11
.Ldec256Loop:
	vmov.u8	r0, d16[0]
	vmov.u8	r1, d20[5]
	vmov.u8	r2, d20[2]
	vmov.u8	r3, d18[7]
	bl	aesMixColumnRound
	vmov.32	d20[0], r0
	vmov.u8	r0, d16[4]
	vmov.u8	r1, d16[1]
	vmov.u8	r2, d20[6]
	vmov.u8	r3, d20[3]
	bl	aesMixColumnRound
	vmov.32	d20[1], r0
	vmov.u8	r0, d17[0]
	vmov.u8	r1, d16[5]
	vmov.u8	r2, d16[2]
	vmov.u8	r3, d20[7]
	bl	aesMixColumnRound
	vmov.32	d21[0], r0
	vmov.u8	r0, d17[4]
	vmov.u8	r1, d17[1]
	vmov.u8	r2, d16[6]
	vmov.u8	r3, d16[3]
	bl	aesMixColumnRound
	vmov.32	d21[1], r0
	vmov.u8	r0, d18[0]
	vmov.u8	r1, d17[5]
	vmov.u8	r2, d17[2]
	vmov.u8	r3, d16[7]
	bl	aesMixColumnRound
	vmov.32	d22[0], r0
	vmov.u8	r0, d18[4]
	vmov.u8	r1, d19[1]
	vmov.u8	r2, d17[6]
	vmov.u8	r3, d17[3]
	bl	aesMixColumnRound
	vmov.32	d22[1], r0
	vmov.u8	r0, d19[0]
	vmov.u8	r1, d19[5]
	vmov.u8	r2, d18[2]
	vmov.u8	r3, d17[7]
	bl	aesMixColumnRound
	vmov.32	d23[0], r0
	vmov.u8	r0, d19[4]
	vmov.u8	r1, d20[1]
	vmov.u8	r2, d18[6]
	vmov.u8	r3, d18[3]
	bl	aesMixColumnRound
	vmov.32	d23[1], r0
	vldmia	r4!, {d20-d23}
	veor	q8, q10, q12
	veor	q9, q11, q13
	subs	r5, #1
	bne.n	.Ldec256Loop
	movw	r6, #:lower16:aesInverseSubstitution
	movt	r6, #:upper16:aesInverseSubstitution
	vmov.u8	r0, d16[0]
	vmov.u8	r1, d19[5]
	vmov.u8	r2, d19[2]
	vmov.u8	r3, d18[7]
	ldr.w	ip, [r4]
	bl	aesLastRoundDecrypt
	vmov.32	d20[0], r0
	vmov.u8	r0, d16[4]
	vmov.u8	r1, d16[1]
	vmov.u8	r2, d19[6]
	vmov.u8	r3, d19[3]
	ldr.w	ip, [r4, #4]
	bl	aesLastRoundDecrypt
	vmov.32	d20[1], r0
	vmov.u8	r0, d17[0]
	vmov.u8	r1, d16[5]
	vmov.u8	r2, d16[2]
	vmov.u8	r3, d19[7]
	ldr.w	ip, [r4, #8]
	bl	aesLastRoundDecrypt
	vmov.32	d21[0], r0
	vmov.u8	r0, d17[4]
	vmov.u8	r1, d17[1]
	vmov.u8	r2, d16[6]
	vmov.u8	r3, d16[3]
	ldr.w	ip, [r4, #12]
	bl	aesLastRoundDecrypt
	vmov.32	d21[1], r0
	vmov.u8	r0, d18[0]
	vmov.u8	r1, d17[5]
	vmov.u8	r2, d17[2]
	vmov.u8	r3, d16[7]
	ldr.w	ip, [r4, #16]
	bl	aesLastRoundDecrypt
	vmov.32	d22[0], r0
	vmov.u8	r0, d18[4]
	vmov.u8	r1, d18[1]
	vmov.u8	r2, d17[6]
	vmov.u8	r3, d17[3]
	ldr.w	ip, [r4, #20]
	bl	aesLastRoundDecrypt
	vmov.32	d22[1], r0
	vmov.u8	r0, d19[0]
	vmov.u8	r1, d18[5]
	vmov.u8	r2, d18[2]
	vmov.u8	r3, d17[7]
	ldr.w	ip, [r4, #24]
	bl	aesLastRoundDecrypt
	vmov.32	d23[0], r0
	vmov.u8	r0, d19[4]
	vmov.u8	r1, d19[1]
	vmov.u8	r2, d18[6]
	vmov.u8	r3, d18[3]
	ldr.w	ip, [r4, #28]
	bl	aesLastRoundDecrypt
	vmov.32	d23[1], r0
	mov.w	r0, #0
	vst1.8	{d20-d23}, [r7]
	pop	{r4, r5, r6, r7, pc}
	.fnend
	.size	aesDecrypt256, . - aesDecrypt256

	.type	aesMixColumnRound, %function
	.thumb_func
aesMixColumnRound:
	.fnstart
	ldr.w	r0, [r6, r0, lsl #2]
	ldr.w	r1, [r6, r1, lsl #2]
	ldr.w	r2, [r6, r2, lsl #2]
	ldr.w	r3, [r6, r3, lsl #2]
	eors.w	r0, r0, r1, ror #24
	eors.w	r0, r0, r2, ror #16
	eors.w	r0, r0, r3, ror #8
	bx	lr
	.fnend
	.size	aesMixColumnRound, . - aesMixColumnRound

	.type	aesLastRoundEncrypt, %function
	.thumb_func
aesLastRoundEncrypt:
	.fnstart
	ldrb.w	r0, [r6, r0, lsl #2]
	ldrb.w	r1, [r6, r1, lsl #2]
	ldrb.w	r2, [r6, r2, lsl #2]
	ldrb.w	r3, [r6, r3, lsl #2]
	bfi	r0, r1, #8, #8
	bfi	r0, r2, #16, #8
	bfi	r0, r3, #24, #8
	eors.w	r0, r0, ip
	bx	lr
	.fnend
	.size	aesLastRoundEncrypt, . - aesLastRoundEncrypt

	.type	aesLastRoundDecrypt, %function
	.thumb_func
aesLastRoundDecrypt:
	.fnstart
	ldrb	r0, [r6, r0]
	ldrb	r1, [r6, r1]
	ldrb	r2, [r6, r2]
	ldrb	r3, [r6, r3]
	bfi	r0, r1, #8, #8
	bfi	r0, r2, #16, #8
	bfi	r0, r3, #24, #8
	eors.w	r0, r0, ip
	bx	lr
	.fnend
	.size	aesLastRoundDecrypt, . - aesLastRoundDecrypt

#else
/* SubWord: the S-box applied to each byte of a key-schedule word. */
	.global	_sceAesSubWord
	.type	_sceAesSubWord, %function
	.thumb_func
_sceAesSubWord:
	.fnstart
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
	.fnend
	.size	_sceAesSubWord, . - _sceAesSubWord

/* InvMixColumn of one column: carry-less multiplies by 0e,0b,0d,09, reduced by 0x11b. */
	.global	_sceAesInvMixColumn
	.type	_sceAesInvMixColumn, %function
	.thumb_func
_sceAesInvMixColumn:
	.fnstart
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
	.fnend
	.size	_sceAesInvMixColumn, . - _sceAesInvMixColumn

/* Encrypt one block (r1 -> r2); the block width picks the 128-, 192- or 256-bit path. */
	.global	sceAesEncrypt
	.type	sceAesEncrypt, %function
	.thumb_func
sceAesEncrypt:
	.fnstart
	.save	{r4, r5, r6, r7, r8, lr}
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
	.fnend
	.size	sceAesEncrypt, . - sceAesEncrypt

/* Decrypt one block with the decryption schedule of a full context. */
	.global	sceAesDecrypt
	.type	sceAesDecrypt, %function
	.thumb_func
sceAesDecrypt:
	.fnstart
	.save	{r4, r5, r6, r7, r8, lr}
	push.w	{r4, r5, r6, r7, r8, lr}
	adds.w	ip, r0, #0x1f0
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
	.fnend
	.size	sceAesDecrypt, . - sceAesDecrypt

/* 192/256-bit round helpers: d24-d27 hold the state, d20-d23 the round key, d28-d31
 * the ShiftRows indices; r6 the table, r5/r1 the column pair, r3/r4 its result. */
	.type	AesWideAddRoundKey, %function
	.thumb_func
AesWideAddRoundKey:
	.fnstart
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
	.fnend
	.size	AesColumnPairRound, . - AesColumnPairRound

	.type	AesWideEncryptLastRound, %function
	.thumb_func
AesWideEncryptLastRound:
	.fnstart
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
	.fnend
	.size	AesEncryptLastColumnPair, . - AesEncryptLastColumnPair

	.type	AesWideDecryptLastRound, %function
	.thumb_func
AesWideDecryptLastRound:
	.fnstart
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
	.fnend
	.size	AesDecryptLastColumnPair, . - AesDecryptLastColumnPair
#endif

	.section .rodata.sceAes, "a", %progbits
	.balign	64
#ifndef SCE_AES_2014
	.type	aesEncryptShiftRows256, %object
aesEncryptShiftRows256:
	.byte	0, 5, 10, 15, 4, 9, 14, 19, 8, 13, 18, 23, 12, 17, 22, 27
	.byte	16, 21, 26, 31, 20, 25, 30, 3, 24, 29, 2, 7, 28, 1, 6, 11
	.size	aesEncryptShiftRows256, . - aesEncryptShiftRows256

	.type	aesEncryptShiftRows192, %object
aesEncryptShiftRows192:
	.byte	0, 5, 10, 15, 4, 9, 14, 19, 8, 13, 18, 23
	.byte	12, 17, 22, 3, 16, 21, 2, 7, 20, 1, 6, 11
	.size	aesEncryptShiftRows192, . - aesEncryptShiftRows192
#endif

#define AES_XTIME(b) (((b) << 1) ^ ((((b) >> 7) & 1) * 0x11b))
#define AES_FWD(b) (AES_XTIME(b) | ((b) << 8) | ((b) << 16) | ((AES_XTIME(b) ^ (b)) << 24))

	.type	aesEncryptionTable, %object
aesEncryptionTable:
	.word	AES_FWD(0x63), AES_FWD(0x7c), AES_FWD(0x77), AES_FWD(0x7b)
	.word	AES_FWD(0xf2), AES_FWD(0x6b), AES_FWD(0x6f), AES_FWD(0xc5)
	.word	AES_FWD(0x30), AES_FWD(0x01), AES_FWD(0x67), AES_FWD(0x2b)
	.word	AES_FWD(0xfe), AES_FWD(0xd7), AES_FWD(0xab), AES_FWD(0x76)
	.word	AES_FWD(0xca), AES_FWD(0x82), AES_FWD(0xc9), AES_FWD(0x7d)
	.word	AES_FWD(0xfa), AES_FWD(0x59), AES_FWD(0x47), AES_FWD(0xf0)
	.word	AES_FWD(0xad), AES_FWD(0xd4), AES_FWD(0xa2), AES_FWD(0xaf)
	.word	AES_FWD(0x9c), AES_FWD(0xa4), AES_FWD(0x72), AES_FWD(0xc0)
	.word	AES_FWD(0xb7), AES_FWD(0xfd), AES_FWD(0x93), AES_FWD(0x26)
	.word	AES_FWD(0x36), AES_FWD(0x3f), AES_FWD(0xf7), AES_FWD(0xcc)
	.word	AES_FWD(0x34), AES_FWD(0xa5), AES_FWD(0xe5), AES_FWD(0xf1)
	.word	AES_FWD(0x71), AES_FWD(0xd8), AES_FWD(0x31), AES_FWD(0x15)
	.word	AES_FWD(0x04), AES_FWD(0xc7), AES_FWD(0x23), AES_FWD(0xc3)
	.word	AES_FWD(0x18), AES_FWD(0x96), AES_FWD(0x05), AES_FWD(0x9a)
	.word	AES_FWD(0x07), AES_FWD(0x12), AES_FWD(0x80), AES_FWD(0xe2)
	.word	AES_FWD(0xeb), AES_FWD(0x27), AES_FWD(0xb2), AES_FWD(0x75)
	.word	AES_FWD(0x09), AES_FWD(0x83), AES_FWD(0x2c), AES_FWD(0x1a)
	.word	AES_FWD(0x1b), AES_FWD(0x6e), AES_FWD(0x5a), AES_FWD(0xa0)
	.word	AES_FWD(0x52), AES_FWD(0x3b), AES_FWD(0xd6), AES_FWD(0xb3)
	.word	AES_FWD(0x29), AES_FWD(0xe3), AES_FWD(0x2f), AES_FWD(0x84)
	.word	AES_FWD(0x53), AES_FWD(0xd1), AES_FWD(0x00), AES_FWD(0xed)
	.word	AES_FWD(0x20), AES_FWD(0xfc), AES_FWD(0xb1), AES_FWD(0x5b)
	.word	AES_FWD(0x6a), AES_FWD(0xcb), AES_FWD(0xbe), AES_FWD(0x39)
	.word	AES_FWD(0x4a), AES_FWD(0x4c), AES_FWD(0x58), AES_FWD(0xcf)
	.word	AES_FWD(0xd0), AES_FWD(0xef), AES_FWD(0xaa), AES_FWD(0xfb)
	.word	AES_FWD(0x43), AES_FWD(0x4d), AES_FWD(0x33), AES_FWD(0x85)
	.word	AES_FWD(0x45), AES_FWD(0xf9), AES_FWD(0x02), AES_FWD(0x7f)
	.word	AES_FWD(0x50), AES_FWD(0x3c), AES_FWD(0x9f), AES_FWD(0xa8)
	.word	AES_FWD(0x51), AES_FWD(0xa3), AES_FWD(0x40), AES_FWD(0x8f)
	.word	AES_FWD(0x92), AES_FWD(0x9d), AES_FWD(0x38), AES_FWD(0xf5)
	.word	AES_FWD(0xbc), AES_FWD(0xb6), AES_FWD(0xda), AES_FWD(0x21)
	.word	AES_FWD(0x10), AES_FWD(0xff), AES_FWD(0xf3), AES_FWD(0xd2)
	.word	AES_FWD(0xcd), AES_FWD(0x0c), AES_FWD(0x13), AES_FWD(0xec)
	.word	AES_FWD(0x5f), AES_FWD(0x97), AES_FWD(0x44), AES_FWD(0x17)
	.word	AES_FWD(0xc4), AES_FWD(0xa7), AES_FWD(0x7e), AES_FWD(0x3d)
	.word	AES_FWD(0x64), AES_FWD(0x5d), AES_FWD(0x19), AES_FWD(0x73)
	.word	AES_FWD(0x60), AES_FWD(0x81), AES_FWD(0x4f), AES_FWD(0xdc)
	.word	AES_FWD(0x22), AES_FWD(0x2a), AES_FWD(0x90), AES_FWD(0x88)
	.word	AES_FWD(0x46), AES_FWD(0xee), AES_FWD(0xb8), AES_FWD(0x14)
	.word	AES_FWD(0xde), AES_FWD(0x5e), AES_FWD(0x0b), AES_FWD(0xdb)
	.word	AES_FWD(0xe0), AES_FWD(0x32), AES_FWD(0x3a), AES_FWD(0x0a)
	.word	AES_FWD(0x49), AES_FWD(0x06), AES_FWD(0x24), AES_FWD(0x5c)
	.word	AES_FWD(0xc2), AES_FWD(0xd3), AES_FWD(0xac), AES_FWD(0x62)
	.word	AES_FWD(0x91), AES_FWD(0x95), AES_FWD(0xe4), AES_FWD(0x79)
	.word	AES_FWD(0xe7), AES_FWD(0xc8), AES_FWD(0x37), AES_FWD(0x6d)
	.word	AES_FWD(0x8d), AES_FWD(0xd5), AES_FWD(0x4e), AES_FWD(0xa9)
	.word	AES_FWD(0x6c), AES_FWD(0x56), AES_FWD(0xf4), AES_FWD(0xea)
	.word	AES_FWD(0x65), AES_FWD(0x7a), AES_FWD(0xae), AES_FWD(0x08)
	.word	AES_FWD(0xba), AES_FWD(0x78), AES_FWD(0x25), AES_FWD(0x2e)
	.word	AES_FWD(0x1c), AES_FWD(0xa6), AES_FWD(0xb4), AES_FWD(0xc6)
	.word	AES_FWD(0xe8), AES_FWD(0xdd), AES_FWD(0x74), AES_FWD(0x1f)
	.word	AES_FWD(0x4b), AES_FWD(0xbd), AES_FWD(0x8b), AES_FWD(0x8a)
	.word	AES_FWD(0x70), AES_FWD(0x3e), AES_FWD(0xb5), AES_FWD(0x66)
	.word	AES_FWD(0x48), AES_FWD(0x03), AES_FWD(0xf6), AES_FWD(0x0e)
	.word	AES_FWD(0x61), AES_FWD(0x35), AES_FWD(0x57), AES_FWD(0xb9)
	.word	AES_FWD(0x86), AES_FWD(0xc1), AES_FWD(0x1d), AES_FWD(0x9e)
	.word	AES_FWD(0xe1), AES_FWD(0xf8), AES_FWD(0x98), AES_FWD(0x11)
	.word	AES_FWD(0x69), AES_FWD(0xd9), AES_FWD(0x8e), AES_FWD(0x94)
	.word	AES_FWD(0x9b), AES_FWD(0x1e), AES_FWD(0x87), AES_FWD(0xe9)
	.word	AES_FWD(0xce), AES_FWD(0x55), AES_FWD(0x28), AES_FWD(0xdf)
	.word	AES_FWD(0x8c), AES_FWD(0xa1), AES_FWD(0x89), AES_FWD(0x0d)
	.word	AES_FWD(0xbf), AES_FWD(0xe6), AES_FWD(0x42), AES_FWD(0x68)
	.word	AES_FWD(0x41), AES_FWD(0x99), AES_FWD(0x2d), AES_FWD(0x0f)
	.word	AES_FWD(0xb0), AES_FWD(0x54), AES_FWD(0xbb), AES_FWD(0x16)
	.size	aesEncryptionTable, . - aesEncryptionTable

#ifndef SCE_AES_2014
	.type	aesDecryptShiftRows256, %object
aesDecryptShiftRows256:
	.byte	0, 29, 26, 23, 4, 1, 30, 27, 8, 5, 2, 31, 12, 9, 6, 3
	.byte	16, 13, 10, 7, 20, 17, 14, 11, 24, 21, 18, 15, 28, 25, 22, 19
	.size	aesDecryptShiftRows256, . - aesDecryptShiftRows256

	.type	aesDecryptShiftRows192, %object
aesDecryptShiftRows192:
	.byte	0, 21, 18, 15, 4, 1, 22, 19, 8, 5, 2, 23
	.byte	12, 9, 6, 3, 16, 13, 10, 7, 20, 17, 14, 11
	.size	aesDecryptShiftRows192, . - aesDecryptShiftRows192
#endif

#define AES_X2(b) AES_XTIME(b)
#define AES_X4(b) AES_X2(AES_X2(b))
#define AES_X8(b) AES_X2(AES_X4(b))
#define AES_INV(b) ((AES_X8(b) ^ AES_X4(b) ^ AES_X2(b)) | ((AES_X8(b) ^ (b)) << 8) | ((AES_X8(b) ^ AES_X4(b) ^ (b)) << 16) | ((AES_X8(b) ^ AES_X2(b) ^ (b)) << 24))

	.type	aesDecryptionTable, %object
aesDecryptionTable:
	.word	AES_INV(0x52), AES_INV(0x09), AES_INV(0x6a), AES_INV(0xd5)
	.word	AES_INV(0x30), AES_INV(0x36), AES_INV(0xa5), AES_INV(0x38)
	.word	AES_INV(0xbf), AES_INV(0x40), AES_INV(0xa3), AES_INV(0x9e)
	.word	AES_INV(0x81), AES_INV(0xf3), AES_INV(0xd7), AES_INV(0xfb)
	.word	AES_INV(0x7c), AES_INV(0xe3), AES_INV(0x39), AES_INV(0x82)
	.word	AES_INV(0x9b), AES_INV(0x2f), AES_INV(0xff), AES_INV(0x87)
	.word	AES_INV(0x34), AES_INV(0x8e), AES_INV(0x43), AES_INV(0x44)
	.word	AES_INV(0xc4), AES_INV(0xde), AES_INV(0xe9), AES_INV(0xcb)
	.word	AES_INV(0x54), AES_INV(0x7b), AES_INV(0x94), AES_INV(0x32)
	.word	AES_INV(0xa6), AES_INV(0xc2), AES_INV(0x23), AES_INV(0x3d)
	.word	AES_INV(0xee), AES_INV(0x4c), AES_INV(0x95), AES_INV(0x0b)
	.word	AES_INV(0x42), AES_INV(0xfa), AES_INV(0xc3), AES_INV(0x4e)
	.word	AES_INV(0x08), AES_INV(0x2e), AES_INV(0xa1), AES_INV(0x66)
	.word	AES_INV(0x28), AES_INV(0xd9), AES_INV(0x24), AES_INV(0xb2)
	.word	AES_INV(0x76), AES_INV(0x5b), AES_INV(0xa2), AES_INV(0x49)
	.word	AES_INV(0x6d), AES_INV(0x8b), AES_INV(0xd1), AES_INV(0x25)
	.word	AES_INV(0x72), AES_INV(0xf8), AES_INV(0xf6), AES_INV(0x64)
	.word	AES_INV(0x86), AES_INV(0x68), AES_INV(0x98), AES_INV(0x16)
	.word	AES_INV(0xd4), AES_INV(0xa4), AES_INV(0x5c), AES_INV(0xcc)
	.word	AES_INV(0x5d), AES_INV(0x65), AES_INV(0xb6), AES_INV(0x92)
	.word	AES_INV(0x6c), AES_INV(0x70), AES_INV(0x48), AES_INV(0x50)
	.word	AES_INV(0xfd), AES_INV(0xed), AES_INV(0xb9), AES_INV(0xda)
	.word	AES_INV(0x5e), AES_INV(0x15), AES_INV(0x46), AES_INV(0x57)
	.word	AES_INV(0xa7), AES_INV(0x8d), AES_INV(0x9d), AES_INV(0x84)
	.word	AES_INV(0x90), AES_INV(0xd8), AES_INV(0xab), AES_INV(0x00)
	.word	AES_INV(0x8c), AES_INV(0xbc), AES_INV(0xd3), AES_INV(0x0a)
	.word	AES_INV(0xf7), AES_INV(0xe4), AES_INV(0x58), AES_INV(0x05)
	.word	AES_INV(0xb8), AES_INV(0xb3), AES_INV(0x45), AES_INV(0x06)
	.word	AES_INV(0xd0), AES_INV(0x2c), AES_INV(0x1e), AES_INV(0x8f)
	.word	AES_INV(0xca), AES_INV(0x3f), AES_INV(0x0f), AES_INV(0x02)
	.word	AES_INV(0xc1), AES_INV(0xaf), AES_INV(0xbd), AES_INV(0x03)
	.word	AES_INV(0x01), AES_INV(0x13), AES_INV(0x8a), AES_INV(0x6b)
	.word	AES_INV(0x3a), AES_INV(0x91), AES_INV(0x11), AES_INV(0x41)
	.word	AES_INV(0x4f), AES_INV(0x67), AES_INV(0xdc), AES_INV(0xea)
	.word	AES_INV(0x97), AES_INV(0xf2), AES_INV(0xcf), AES_INV(0xce)
	.word	AES_INV(0xf0), AES_INV(0xb4), AES_INV(0xe6), AES_INV(0x73)
	.word	AES_INV(0x96), AES_INV(0xac), AES_INV(0x74), AES_INV(0x22)
	.word	AES_INV(0xe7), AES_INV(0xad), AES_INV(0x35), AES_INV(0x85)
	.word	AES_INV(0xe2), AES_INV(0xf9), AES_INV(0x37), AES_INV(0xe8)
	.word	AES_INV(0x1c), AES_INV(0x75), AES_INV(0xdf), AES_INV(0x6e)
	.word	AES_INV(0x47), AES_INV(0xf1), AES_INV(0x1a), AES_INV(0x71)
	.word	AES_INV(0x1d), AES_INV(0x29), AES_INV(0xc5), AES_INV(0x89)
	.word	AES_INV(0x6f), AES_INV(0xb7), AES_INV(0x62), AES_INV(0x0e)
	.word	AES_INV(0xaa), AES_INV(0x18), AES_INV(0xbe), AES_INV(0x1b)
	.word	AES_INV(0xfc), AES_INV(0x56), AES_INV(0x3e), AES_INV(0x4b)
	.word	AES_INV(0xc6), AES_INV(0xd2), AES_INV(0x79), AES_INV(0x20)
	.word	AES_INV(0x9a), AES_INV(0xdb), AES_INV(0xc0), AES_INV(0xfe)
	.word	AES_INV(0x78), AES_INV(0xcd), AES_INV(0x5a), AES_INV(0xf4)
	.word	AES_INV(0x1f), AES_INV(0xdd), AES_INV(0xa8), AES_INV(0x33)
	.word	AES_INV(0x88), AES_INV(0x07), AES_INV(0xc7), AES_INV(0x31)
	.word	AES_INV(0xb1), AES_INV(0x12), AES_INV(0x10), AES_INV(0x59)
	.word	AES_INV(0x27), AES_INV(0x80), AES_INV(0xec), AES_INV(0x5f)
	.word	AES_INV(0x60), AES_INV(0x51), AES_INV(0x7f), AES_INV(0xa9)
	.word	AES_INV(0x19), AES_INV(0xb5), AES_INV(0x4a), AES_INV(0x0d)
	.word	AES_INV(0x2d), AES_INV(0xe5), AES_INV(0x7a), AES_INV(0x9f)
	.word	AES_INV(0x93), AES_INV(0xc9), AES_INV(0x9c), AES_INV(0xef)
	.word	AES_INV(0xa0), AES_INV(0xe0), AES_INV(0x3b), AES_INV(0x4d)
	.word	AES_INV(0xae), AES_INV(0x2a), AES_INV(0xf5), AES_INV(0xb0)
	.word	AES_INV(0xc8), AES_INV(0xeb), AES_INV(0xbb), AES_INV(0x3c)
	.word	AES_INV(0x83), AES_INV(0x53), AES_INV(0x99), AES_INV(0x61)
	.word	AES_INV(0x17), AES_INV(0x2b), AES_INV(0x04), AES_INV(0x7e)
	.word	AES_INV(0xba), AES_INV(0x77), AES_INV(0xd6), AES_INV(0x26)
	.word	AES_INV(0xe1), AES_INV(0x69), AES_INV(0x14), AES_INV(0x63)
	.word	AES_INV(0x55), AES_INV(0x21), AES_INV(0x0c), AES_INV(0x7d)
	.size	aesDecryptionTable, . - aesDecryptionTable

	.type	aesInverseSubstitution, %object
aesInverseSubstitution:
	.byte	0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb
	.byte	0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb
	.byte	0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e
	.byte	0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25
	.byte	0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92
	.byte	0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84
	.byte	0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06
	.byte	0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b
	.byte	0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73
	.byte	0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e
	.byte	0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b
	.byte	0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4
	.byte	0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f
	.byte	0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef
	.byte	0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61
	.byte	0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d
	.size	aesInverseSubstitution, . - aesInverseSubstitution
#ifdef SCE_AES_2014

/* GF(2^8) log and exp tables (generator 0x03, polynomial 0x11b) for aesGfMulByte. */
	.type	aesGfLogTable, %object
aesGfLogTable:
	.byte	0x00, 0xff, 0x19, 0x01, 0x32, 0x02, 0x1a, 0xc6, 0x4b, 0xc7, 0x1b, 0x68, 0x33, 0xee, 0xdf, 0x03
	.byte	0x64, 0x04, 0xe0, 0x0e, 0x34, 0x8d, 0x81, 0xef, 0x4c, 0x71, 0x08, 0xc8, 0xf8, 0x69, 0x1c, 0xc1
	.byte	0x7d, 0xc2, 0x1d, 0xb5, 0xf9, 0xb9, 0x27, 0x6a, 0x4d, 0xe4, 0xa6, 0x72, 0x9a, 0xc9, 0x09, 0x78
	.byte	0x65, 0x2f, 0x8a, 0x05, 0x21, 0x0f, 0xe1, 0x24, 0x12, 0xf0, 0x82, 0x45, 0x35, 0x93, 0xda, 0x8e
	.byte	0x96, 0x8f, 0xdb, 0xbd, 0x36, 0xd0, 0xce, 0x94, 0x13, 0x5c, 0xd2, 0xf1, 0x40, 0x46, 0x83, 0x38
	.byte	0x66, 0xdd, 0xfd, 0x30, 0xbf, 0x06, 0x8b, 0x62, 0xb3, 0x25, 0xe2, 0x98, 0x22, 0x88, 0x91, 0x10
	.byte	0x7e, 0x6e, 0x48, 0xc3, 0xa3, 0xb6, 0x1e, 0x42, 0x3a, 0x6b, 0x28, 0x54, 0xfa, 0x85, 0x3d, 0xba
	.byte	0x2b, 0x79, 0x0a, 0x15, 0x9b, 0x9f, 0x5e, 0xca, 0x4e, 0xd4, 0xac, 0xe5, 0xf3, 0x73, 0xa7, 0x57
	.byte	0xaf, 0x58, 0xa8, 0x50, 0xf4, 0xea, 0xd6, 0x74, 0x4f, 0xae, 0xe9, 0xd5, 0xe7, 0xe6, 0xad, 0xe8
	.byte	0x2c, 0xd7, 0x75, 0x7a, 0xeb, 0x16, 0x0b, 0xf5, 0x59, 0xcb, 0x5f, 0xb0, 0x9c, 0xa9, 0x51, 0xa0
	.byte	0x7f, 0x0c, 0xf6, 0x6f, 0x17, 0xc4, 0x49, 0xec, 0xd8, 0x43, 0x1f, 0x2d, 0xa4, 0x76, 0x7b, 0xb7
	.byte	0xcc, 0xbb, 0x3e, 0x5a, 0xfb, 0x60, 0xb1, 0x86, 0x3b, 0x52, 0xa1, 0x6c, 0xaa, 0x55, 0x29, 0x9d
	.byte	0x97, 0xb2, 0x87, 0x90, 0x61, 0xbe, 0xdc, 0xfc, 0xbc, 0x95, 0xcf, 0xcd, 0x37, 0x3f, 0x5b, 0xd1
	.byte	0x53, 0x39, 0x84, 0x3c, 0x41, 0xa2, 0x6d, 0x47, 0x14, 0x2a, 0x9e, 0x5d, 0x56, 0xf2, 0xd3, 0xab
	.byte	0x44, 0x11, 0x92, 0xd9, 0x23, 0x20, 0x2e, 0x89, 0xb4, 0x7c, 0xb8, 0x26, 0x77, 0x99, 0xe3, 0xa5
	.byte	0x67, 0x4a, 0xed, 0xde, 0xc5, 0x31, 0xfe, 0x18, 0x0d, 0x63, 0x8c, 0x80, 0xc0, 0xf7, 0x70, 0x07
	.size	aesGfLogTable, . - aesGfLogTable

	.type	aesGfExpTable, %object
aesGfExpTable:
	.byte	0x01, 0x03, 0x05, 0x0f, 0x11, 0x33, 0x55, 0xff, 0x1a, 0x2e, 0x72, 0x96, 0xa1, 0xf8, 0x13, 0x35
	.byte	0x5f, 0xe1, 0x38, 0x48, 0xd8, 0x73, 0x95, 0xa4, 0xf7, 0x02, 0x06, 0x0a, 0x1e, 0x22, 0x66, 0xaa
	.byte	0xe5, 0x34, 0x5c, 0xe4, 0x37, 0x59, 0xeb, 0x26, 0x6a, 0xbe, 0xd9, 0x70, 0x90, 0xab, 0xe6, 0x31
	.byte	0x53, 0xf5, 0x04, 0x0c, 0x14, 0x3c, 0x44, 0xcc, 0x4f, 0xd1, 0x68, 0xb8, 0xd3, 0x6e, 0xb2, 0xcd
	.byte	0x4c, 0xd4, 0x67, 0xa9, 0xe0, 0x3b, 0x4d, 0xd7, 0x62, 0xa6, 0xf1, 0x08, 0x18, 0x28, 0x78, 0x88
	.byte	0x83, 0x9e, 0xb9, 0xd0, 0x6b, 0xbd, 0xdc, 0x7f, 0x81, 0x98, 0xb3, 0xce, 0x49, 0xdb, 0x76, 0x9a
	.byte	0xb5, 0xc4, 0x57, 0xf9, 0x10, 0x30, 0x50, 0xf0, 0x0b, 0x1d, 0x27, 0x69, 0xbb, 0xd6, 0x61, 0xa3
	.byte	0xfe, 0x19, 0x2b, 0x7d, 0x87, 0x92, 0xad, 0xec, 0x2f, 0x71, 0x93, 0xae, 0xe9, 0x20, 0x60, 0xa0
	.byte	0xfb, 0x16, 0x3a, 0x4e, 0xd2, 0x6d, 0xb7, 0xc2, 0x5d, 0xe7, 0x32, 0x56, 0xfa, 0x15, 0x3f, 0x41
	.byte	0xc3, 0x5e, 0xe2, 0x3d, 0x47, 0xc9, 0x40, 0xc0, 0x5b, 0xed, 0x2c, 0x74, 0x9c, 0xbf, 0xda, 0x75
	.byte	0x9f, 0xba, 0xd5, 0x64, 0xac, 0xef, 0x2a, 0x7e, 0x82, 0x9d, 0xbc, 0xdf, 0x7a, 0x8e, 0x89, 0x80
	.byte	0x9b, 0xb6, 0xc1, 0x58, 0xe8, 0x23, 0x65, 0xaf, 0xea, 0x25, 0x6f, 0xb1, 0xc8, 0x43, 0xc5, 0x54
	.byte	0xfc, 0x1f, 0x21, 0x63, 0xa5, 0xf4, 0x07, 0x09, 0x1b, 0x2d, 0x77, 0x99, 0xb0, 0xcb, 0x46, 0xca
	.byte	0x45, 0xcf, 0x4a, 0xde, 0x79, 0x8b, 0x86, 0x91, 0xa8, 0xe3, 0x3e, 0x42, 0xc6, 0x51, 0xf3, 0x0e
	.byte	0x12, 0x36, 0x5a, 0xee, 0x29, 0x7b, 0x8d, 0x8c, 0x8f, 0x8a, 0x85, 0x94, 0xa7, 0xf2, 0x0d, 0x17
	.byte	0x39, 0x4b, 0xdd, 0x7c, 0x84, 0x97, 0xa2, 0xfd, 0x1c, 0x24, 0x6c, 0xb4, 0xc7, 0x52, 0xf6, 0x01
	.size	aesGfExpTable, . - aesGfExpTable
#endif
