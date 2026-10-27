/*
 * Sony's hand-written inflater (RFC 1951) for SceKernelUtils. The bit buffer
 * and the output position live in registers for the whole stream; the
 * Huffman tables, built by deflate_huffman.S, sit in a frame on the stack.
 */
	.syntax unified
	.arch	armv7-a
	.thumb
	.text

	.balign	4
/*
 * Inflate a raw deflate stream (src) into dst_size bytes at dst; *next, when
 * given, receives the end of the input. Returns the output size or an error.
 */
	.global	sceDeflateDecompress
	.type	sceDeflateDecompress, %function
	.thumb_func
sceDeflateDecompress:
	push.w	{r0, r3, r4, r5, r6, r7, r8, r9, sl, fp, lr}
	subw	sp, sp, #0x4ec
	lsls	r6, r2, #31
	str	r6, [sp, #560]
	bic.w	r8, r2, #1
	lsrs	r6, r6, #28
	ldr.w	r9, [r8], #2
	mov	r4, r0
	subs	r6, #17
	adds	r5, r0, r1
.Linflate1:
	ror.w	r7, r9, r6
	adds	r6, #3
	itt	gt
	ldrgt.w	r9, [r8], #2
	subgt	r6, #16
	ubfx	r3, r7, #18, #2
	subs	r2, r3, #2
	bne.w	.Linflate36
	str	r7, [sp, #560]
	add	r0, sp, #416
.Linflate2:
	strd	r2, r2, [r0, #-8]!
	cmp	sp, r0
	bne.n	.Linflate2
	ror.w	r7, r9, r6
	adds	r6, #14
	itt	gt
	ldrgt.w	r9, [r8], #2
	subgt	r6, #16
	ubfx	r1, r7, #27, #4
	adds	r1, #4
	movw	ip, #:lower16:deflateCodeLengthOrder
	movt	ip, #:upper16:deflateCodeLengthOrder
	mov.w	fp, #0x80000000
.Linflate3:
	ror.w	r2, r9, r6
	adds	r6, #3
	itt	gt
	ldrgt.w	r9, [r8], #2
	subgt	r6, #16
	ldrb.w	r3, [ip], #1
	ubfx	r2, r2, #17, #3
	subs	r1, #1
	ldr.w	lr, [sp, r2, lsl #2]
	lsr.w	r3, fp, r3
	add	r3, lr
	str.w	r3, [sp, r2, lsl #2]
	bne.n	.Linflate3
	bl	DeflateBuildPrecode
	ubfx	r3, r7, #17, #5
	ubfx	r7, r7, #22, #5
	cmp	r3, #29
	it	ls
	cmpls	r7, #29
	it	ls
	cmpls	r0, #128
	bne.n	.Linflate12
	addw	ip, r3, #257
	add	r2, sp, #124
	add	ip, r2
	mov	r3, fp
	add.w	sl, sp, #688
.Linflate4:
	ror.w	r1, r9, r6
	ubfx	r1, r1, #17, #7
	ldrsb.w	r1, [sl, r1]
	and.w	r0, r1, #7
	adds	r6, r6, r0
	itt	gt
	ldrgt.w	r9, [r8], #2
	subgt	r6, #16
	asrs	r1, r1, #3
	bmi.n	.Linflate13
	strb.w	r1, [r2], #1
	mov	r3, r1
	beq.n	.Linflate5
	adds	r1, #15
	ldr.w	r0, [sp, r1, lsl #2]
	adds	r0, #1
	str.w	r0, [sp, r1, lsl #2]
.Linflate5:
	subs.w	r0, r2, ip
	bmi.n	.Linflate4
	lsr.w	r1, fp, r0
	cbz	r0, .Linflate6
	subs	r7, r7, r0
	cbz	r3, .Linflate6
	lsls	r2, r1, #1
	negs	r2, r2
	str.w	r2, [sp, r3, lsl #2]
	adds	r3, #15
	ldr.w	r2, [sp, r3, lsl #2]
	subs	r2, r2, r0
	str.w	r2, [sp, r3, lsl #2]
.Linflate6:
	adds	r7, #1
	ble.n	.Linflate10
.Linflate7:
	ror.w	r2, r9, r6
	ubfx	r2, r2, #17, #7
	ldrsb.w	r2, [sl, r2]
	and.w	r0, r2, #7
	adds	r6, r6, r0
	itt	gt
	ldrgt.w	r9, [r8], #2
	subgt	r6, #16
	asrs	r2, r2, #3
	bmi.n	.Linflate17
	ldr.w	r0, [sp, r2, lsl #2]
	orrs	r0, r1
	mov	r3, r2
	movs	r2, #1
.Linflate8:
	str.w	r0, [sp, r3, lsl #2]
.Linflate9:
	subs	r7, r7, r2
	ble.n	.Linflate10
	lsrs	r1, r2
	b.n	.Linflate7
.Linflate10:
	bne.n	.Linflate12
	cmp	r6, #0
	itt	gt
	ldrgt.w	r9, [r8], #2
	subgt	r6, #16
	mov	r0, sp
	bl	DeflateBuild
	cbnz	r0, .Linflate12
.Linflate11:
	ldr.w	sl, [sp, #0x4ec]
	add.w	fp, sp, #492
	add.w	ip, sp, #624
	b.n	.Linflate23
.Linflate12:
	b.n	.Linflate35
.Linflate13:
	adds	r1, #16
	cbnz	r1, .Linflate15
	ror.w	r1, r9, r6
	adds	r6, #2
	ubfx	r1, r1, #17, #2
	adds	r1, #3
	cbz	r3, .Linflate16
	adds.w	r0, r3, #15
	bmi.n	.Linflate12
	ldr.w	lr, [sp, r0, lsl #2]
	add	lr, r1
	str.w	lr, [sp, r0, lsl #2]
.Linflate14:
	subs	r1, #1
	strb.w	r3, [r2], #1
	bne.n	.Linflate14
	b.n	.Linflate5
.Linflate15:
	add.w	r2, r2, r1, lsl #1
	subs	r2, #3
	add	r6, r1
	ror.w	r3, r9, r6
	rsb	r0, r1, #32
	lsls	r1, r3, #15
	lsrs	r1, r0
	movs	r3, #0
.Linflate16:
	add	r2, r1
	b.n	.Linflate5
.Linflate17:
	adds	r2, #16
	cbnz	r2, .Linflate18
	ror.w	r2, r9, r6
	adds	r6, #2
	ubfx	r2, r2, #17, #2
	ldr.w	r0, [sp, r3, lsl #2]
	adds	r2, #3
	lsr.w	ip, r1, r2
	rsb	ip, ip, r1
	orr.w	r0, r0, ip, lsl #1
	b.n	.Linflate8
.Linflate18:
	add	r6, r2
	ror.w	r3, r9, r6
	lsls	r3, r3, #15
	rsb	r0, r2, #32
	lsrs	r3, r0
	add.w	r2, r3, r2, lsl #1
	subs	r2, #3
	movs	r3, #0
	b.n	.Linflate9
.Linflate19:
	rbit	r1, r1
	lsls	r1, r1, #17
.Linflate20:
	ldr.w	r2, [fp, r3, lsl #2]
	subs	r3, #1
	cmp	r1, r2
	bcs.n	.Linflate20
	lsrs	r1, r3
	sxtah	r1, r1, r2
	rsb	r2, r3, #32
	ldrh.w	r3, [sp, r1, lsl #1]
	cmp.w	r3, #0x800
	bge.n	.Linflate24
.Linflate21:
	cmp	r4, r5
	beq.n	.Linflate33
	adds	r6, r6, r2
	itt	gt
	ldrgt.w	r9, [r8], #2
	subgt	r6, #16
.Linflate22:
	strb.w	r3, [r4], #1
.Linflate23:
	ror.w	r1, r9, r6
	ubfx	r3, r1, #17, #8
	add	r3, r3
	ldrh.w	r3, [sp, r3]
	asrs	r2, r3, #12
	beq.n	.Linflate19
	bcc.n	.Linflate21
.Linflate24:
	adds	r6, r6, r2
	itt	gt
	ldrgt.w	r9, [r8], #2
	subgt	r6, #16
	lsls	r2, r3, #21
	bmi.n	.Linflate32
	lsrs	r2, r2, #21
.Linflate25:
	ror.w	r7, r9, r6
	ubfx	r3, r7, #17, #5
	ldrh.w	r1, [ip, r3, lsl #1]
	and.w	r3, r1, #255
	lsrs	r1, r1, #12
	bcc.n	.Linflate27
	rbit	r3, r7
	lsls	r3, r3, #17
.Linflate26:
	ldr.w	r7, [fp, r1, lsl #2]
	adds	r1, #1
	cmp	r3, r7
	bcs.n	.Linflate26
	rsb	r0, r1, #32
	lsrs	r3, r0
	sxtah	r3, r3, r7
	ldrb.w	r3, [sp, r3]
.Linflate27:
	adds	r6, r6, r1
	itt	gt
	ldrgt.w	r9, [r8], #2
	subgt	r6, #16
	asrs	r1, r3, #2
	beq.n	.Linflate35
	adds	r6, #30
	subs	r6, r6, r1
	ror.w	r7, r9, r6
	bfi	r7, r3, #17, #2
	itt	gt
	ldrgt.w	r9, [r8], #2
	subgt	r6, #16
	lsls	r3, r7, #13
	lsrs	r3, r1
	adds	r1, r2, r4
	sbc.w	r0, r4, r3
	subs	r1, r5, r1
	bcc.n	.Linflate33
	cmp	r0, sl
	bcc.n	.Linflate35
	cmp	r1, #2
	bls.n	.Linflate29
	cmp	r3, #2
	bls.n	.Linflate30
.Linflate28:
	subs	r2, #4
	ldr.w	r3, [r0], #4
	str.w	r3, [r4], #4
	bgt.n	.Linflate28
	subs	r4, r5, r1
	b.n	.Linflate23
.Linflate29:
	ldrb.w	r3, [r0], #1
	subs	r2, #1
	beq.n	.Linflate22
	strb.w	r3, [r4], #1
	b.n	.Linflate29
.Linflate30:
	beq.n	.Linflate29
	subs	r3, #1
	ldr	r3, [r0, #0]
	it	ne
	bfine	r3, r3, #8, #8
	bfi	r3, r3, #16, #16
.Linflate31:
	subs	r2, #4
	str.w	r3, [r4], #4
	bgt.n	.Linflate31
	subs	r4, r5, r1
	b.n	.Linflate23
.Linflate32:
	uxtb	r3, r3
	cbz	r3, .Linflate38
	adds	r6, r6, r3
	ror.w	r1, r9, r6
	itt	gt
	ldrgt.w	r9, [r8], #2
	subgt	r6, #16
	bfi	r2, r1, #12, #17
	rsb	r3, r3, #29
	lsrs	r2, r3
	adds	r2, #3
	b.n	.Linflate25
.Linflate33:
	movs	r0, #252
.Linflate34:
	adds	r0, #5
	movt	r0, #0x8056
	b.n	.Linflate40
.Linflate35:
	movs	r0, #251
	b.n	.Linflate34
.Linflate36:
	bcs.n	.Linflate35
	ldr	r1, [sp, #560]
	str	r7, [sp, #560]
	cbz	r3, .Linflate37
	mov	r0, sp
	bl	DeflateSetupBlock
	b.n	.Linflate11
.Linflate37:
	adds	r6, #8
	add.w	r1, r8, r6, asr #3
	ldr.w	r3, [r1], #4
	eor.w	r6, r3, r3, ror #16
	adds	r2, r6, #1
	bne.n	.Linflate35
	uxth	r2, r3
	mov	r0, r4
	add	r4, r2
	cmp	r4, r5
	bhi.n	.Linflate33
	add.w	r8, r1, r2
	bl	memcpy
	ldr.w	r9, [r8], #2
	subs	r6, #16
	b.n	.Linflate39
.Linflate38:
	lsls	r2, r2, #3
	bcs.n	.Linflate35
.Linflate39:
	ldr	r3, [sp, #560]
	lsls	r3, r3, #14
	bpl.w	.Linflate1
	addw	r0, sp, #0x4ec
	ldmia	r0, {r0, r3}
	subs	r0, r4, r0
	cbz	r3, .Linflate40
	adds	r6, #8
	add.w	r6, r8, r6, asr #3
	str	r6, [r3, #0]
.Linflate40:
	addw	sp, sp, #0x4f4
	pop.w	{r4, r5, r6, r7, r8, r9, sl, fp, pc}
	.size	sceDeflateDecompress, . - sceDeflateDecompress

/* sceDeflateDecompressPartial without a valid cbInfo: SCE_DEFLATE_ERROR_INVALID_CBINFO */
	.type	DeflatePartialInvalidCbInfo, %function
	.thumb_func
DeflatePartialInvalidCbInfo:
	movs	r0, #255
	b.n	.Lpartial51
	.size	DeflatePartialInvalidCbInfo, . - DeflatePartialInvalidCbInfo

/*
 * The same with the input supplied piecewise: DeflateRefill calls the
 * callback of cbInfo for more input when the current piece is used up.
 */
	.global	sceDeflateDecompressPartial
	.type	sceDeflateDecompressPartial, %function
	.thumb_func
sceDeflateDecompressPartial:
	push.w	{r0, r3, r4, r5, r6, r7, r8, r9, sl, fp, lr}
	subw	sp, sp, #0x4ec
	adds	r5, r0, r1
	mov	r4, r0
	ldr.w	r7, [sp, #0x518]
	cmp	r7, #0
	beq.n	DeflatePartialInvalidCbInfo
	ldrd	r6, fp, [r7]
	cmp	r6, #16
	bne.n	DeflatePartialInvalidCbInfo
	subs.w	fp, fp, #2
	eor.w	r9, fp, r2
	subs	r6, #17
	str	r6, [sp, #560]
	ands.w	r9, r9, #1
	beq.n	.Lpartial1
	ldrb.w	r9, [r2], #1
	mov.w	r9, r9, lsl #24
	subs	r6, #8
.Lpartial1:
	subs.w	r8, r2, #2
	cmp	r8, fp
	bne.n	.Lpartial2
	bl	DeflateRefill
	b.n	.Lpartial3
.Lpartial2:
	ldr.w	r9, [r8], #2
.Lpartial3:
	ror.w	r7, r9, r6
	adds	r6, #3
	ble.n	.Lpartial5
	subs	r6, #16
	cmp	r8, fp
	bne.n	.Lpartial4
	bl	DeflateRefill
	b.n	.Lpartial5
.Lpartial4:
	ldr.w	r9, [r8], #2
.Lpartial5:
	ubfx	r3, r7, #18, #2
	subs	r2, r3, #2
	bne.w	.Lpartial62
	str	r7, [sp, #560]
	add	r3, sp, #416
.Lpartial6:
	strd	r2, r2, [r3, #-8]!
	cmp	sp, r3
	bne.n	.Lpartial6
	ror.w	r7, r9, r6
	adds	r6, #14
	ble.n	.Lpartial8
	subs	r6, #16
	cmp	r8, fp
	bne.n	.Lpartial7
	bl	DeflateRefill
	b.n	.Lpartial8
.Lpartial7:
	ldr.w	r9, [r8], #2
.Lpartial8:
	ubfx	r1, r7, #27, #4
	adds	r1, #4
	movw	r3, #:lower16:deflateCodeLengthOrder
	movt	r3, #:upper16:deflateCodeLengthOrder
	mov.w	ip, #0x80000000
.Lpartial9:
	ror.w	r2, r9, r6
	adds	r6, #3
	ble.n	.Lpartial11
	subs	r6, #16
	cmp	r8, fp
	bne.n	.Lpartial10
	bl	DeflateRefill
	b.n	.Lpartial11
.Lpartial10:
	ldr.w	r9, [r8], #2
.Lpartial11:
	ldrb.w	r0, [r3], #1
	ubfx	r2, r2, #17, #3
	subs	r1, #1
	ldr.w	lr, [sp, r2, lsl #2]
	lsr.w	r0, ip, r0
	add	r0, lr
	str.w	r0, [sp, r2, lsl #2]
	bne.n	.Lpartial9
	mov	r0, sp
	bl	DeflateBuildPrecode
	ubfx	r3, r7, #17, #5
	ubfx	r7, r7, #22, #5
	cmp	r3, #29
	it	ls
	cmpls	r7, #29
	it	ls
	cmpls	r0, #128
	bne.n	.Lpartial26
	addw	ip, r3, #257
	add	r2, sp, #124
	add	ip, r2
	lsls	r3, r0, #24
	add.w	sl, sp, #688
.Lpartial12:
	ror.w	r1, r9, r6
	ubfx	r1, r1, #17, #7
	ldrsb.w	r1, [sl, r1]
	and.w	r0, r1, #7
	adds	r6, r6, r0
	ble.n	.Lpartial14
	subs	r6, #16
	cmp	r8, fp
	bne.n	.Lpartial13
	bl	DeflateRefill
	b.n	.Lpartial14
.Lpartial13:
	ldr.w	r9, [r8], #2
.Lpartial14:
	asrs	r1, r1, #3
	bmi.n	.Lpartial27
	strb.w	r1, [r2], #1
	mov	r3, r1
	beq.n	.Lpartial15
	adds	r1, #15
	ldr.w	r0, [sp, r1, lsl #2]
	adds	r0, #1
	str.w	r0, [sp, r1, lsl #2]
.Lpartial15:
	subs.w	r0, r2, ip
	bmi.n	.Lpartial12
	mov.w	r1, #0x80000000
	lsrs	r1, r0
	cbz	r0, .Lpartial16
	subs	r7, r7, r0
	cbz	r3, .Lpartial16
	lsls	r2, r1, #1
	negs	r2, r2
	str.w	r2, [sp, r3, lsl #2]
	adds	r3, #15
	ldr.w	r2, [sp, r3, lsl #2]
	subs	r2, r2, r0
	str.w	r2, [sp, r3, lsl #2]
.Lpartial16:
	adds	r7, #1
	ble.n	.Lpartial22
.Lpartial17:
	ror.w	r2, r9, r6
	ubfx	r2, r2, #17, #7
	ldrsb.w	r2, [sl, r2]
	and.w	r0, r2, #7
	adds	r6, r6, r0
	ble.n	.Lpartial19
	subs	r6, #16
	cmp	r8, fp
	bne.n	.Lpartial18
	bl	DeflateRefill
	b.n	.Lpartial19
.Lpartial18:
	ldr.w	r9, [r8], #2
.Lpartial19:
	asrs	r2, r2, #3
	bmi.n	.Lpartial31
	ldr.w	r0, [sp, r2, lsl #2]
	orrs	r0, r1
	mov	r3, r2
	movs	r2, #1
.Lpartial20:
	str.w	r0, [sp, r3, lsl #2]
.Lpartial21:
	subs	r7, r7, r2
	ble.n	.Lpartial22
	lsrs	r1, r2
	b.n	.Lpartial17
.Lpartial22:
	bne.n	.Lpartial26
	cmp	r6, #0
	ble.n	.Lpartial24
	subs	r6, #16
	cmp	r8, fp
	bne.n	.Lpartial23
	bl	DeflateRefill
	b.n	.Lpartial24
.Lpartial23:
	ldr.w	r9, [r8], #2
.Lpartial24:
	mov	r0, sp
	bl	DeflateBuild
	cbnz	r0, .Lpartial26
.Lpartial25:
	ldr.w	sl, [sp, #0x4ec]
	add.w	ip, sp, #624
	b.n	.Lpartial38
.Lpartial26:
	b.n	.Lpartial58
.Lpartial27:
	adds	r1, #16
	cbnz	r1, .Lpartial29
	ror.w	r1, r9, r6
	adds	r6, #2
	ubfx	r1, r1, #17, #2
	adds	r1, #3
	cbz	r3, .Lpartial30
	adds.w	r0, r3, #15
	bmi.n	.Lpartial26
	ldr.w	lr, [sp, r0, lsl #2]
	add	lr, r1
	str.w	lr, [sp, r0, lsl #2]
.Lpartial28:
	subs	r1, #1
	strb.w	r3, [r2], #1
	bne.n	.Lpartial28
	b.n	.Lpartial15
.Lpartial29:
	add.w	r2, r2, r1, lsl #1
	subs	r2, #3
	add	r6, r1
	ror.w	r3, r9, r6
	rsb	r0, r1, #32
	lsls	r1, r3, #15
	lsrs	r1, r0
	movs	r3, #0
.Lpartial30:
	add	r2, r1
	b.n	.Lpartial15
.Lpartial31:
	adds	r2, #16
	cbnz	r2, .Lpartial32
	ror.w	r2, r9, r6
	adds	r6, #2
	ubfx	r2, r2, #17, #2
	ldr.w	r0, [sp, r3, lsl #2]
	adds	r2, #3
	lsr.w	ip, r1, r2
	rsb	ip, ip, r1
	orr.w	r0, r0, ip, lsl #1
	b.n	.Lpartial20
.Lpartial32:
	add	r6, r2
	ror.w	r3, r9, r6
	lsls	r3, r3, #15
	rsb	r0, r2, #32
	lsrs	r3, r0
	add.w	r2, r3, r2, lsl #1
	subs	r2, #3
	movs	r3, #0
	b.n	.Lpartial21
.Lpartial33:
	rbit	r1, r1
	add	r0, sp, #492
	lsls	r1, r1, #17
.Lpartial34:
	ldr.w	r2, [r0, r3, lsl #2]
	subs	r3, #1
	cmp	r1, r2
	bcs.n	.Lpartial34
	lsrs	r1, r3
	sxtah	r1, r1, r2
	rsb	r2, r3, #32
	ldrh.w	r3, [sp, r1, lsl #1]
	cmp.w	r3, #0x800
	bge.n	.Lpartial39
.Lpartial35:
	cmp	r4, r5
	beq.n	.Lpartial50
	adds	r6, r6, r2
	ble.n	.Lpartial37
	subs	r6, #16
	cmp	r8, fp
	bne.n	.Lpartial36
	bl	DeflateRefill
	b.n	.Lpartial37
.Lpartial36:
	ldr.w	r9, [r8], #2
.Lpartial37:
	strb.w	r3, [r4], #1
.Lpartial38:
	ror.w	r1, r9, r6
	ubfx	r3, r1, #17, #8
	add	r3, r3
	ldrh.w	r3, [sp, r3]
	asrs	r2, r3, #12
	beq.n	.Lpartial33
	bcc.n	.Lpartial35
.Lpartial39:
	adds	r6, r6, r2
	ble.n	.Lpartial41
	subs	r6, #16
	cmp	r8, fp
	bne.n	.Lpartial40
	bl	DeflateRefill
	b.n	.Lpartial41
.Lpartial40:
	ldr.w	r9, [r8], #2
.Lpartial41:
	lsls	r2, r3, #21
	bmi.n	.Lpartial55
	lsrs	r2, r2, #21
.Lpartial42:
	ror.w	r7, r9, r6
	ubfx	r3, r7, #17, #5
	ldrh.w	r1, [ip, r3, lsl #1]
	and.w	r3, r1, #255
	lsrs	r1, r1, #12
	bcc.n	.Lpartial44
	add	r0, sp, #492
	rbit	r3, r7
	lsls	r3, r3, #17
.Lpartial43:
	ldr.w	r7, [r0, r1, lsl #2]
	adds	r1, #1
	cmp	r3, r7
	bcs.n	.Lpartial43
	rsb	r0, r1, #32
	lsrs	r3, r0
	sxtah	r3, r3, r7
	ldrb.w	r3, [sp, r3]
.Lpartial44:
	adds	r6, r6, r1
	ble.n	.Lpartial46
	subs	r6, #16
	cmp	r8, fp
	bne.n	.Lpartial45
	bl	DeflateRefill
	b.n	.Lpartial46
.Lpartial45:
	ldr.w	r9, [r8], #2
.Lpartial46:
	asrs	r1, r3, #2
	beq.n	.Lpartial58
	adds	r6, #30
	subs	r6, r6, r1
	ror.w	r7, r9, r6
	bfi	r7, r3, #17, #2
	ble.n	.Lpartial48
	subs	r6, #16
	cmp	r8, fp
	bne.n	.Lpartial47
	bl	DeflateRefill
	b.n	.Lpartial48
.Lpartial47:
	ldr.w	r9, [r8], #2
.Lpartial48:
	lsls	r3, r7, #13
	lsrs	r3, r1
	adds	r1, r2, r4
	sbc.w	r0, r4, r3
	subs	r1, r5, r1
	bcc.n	.Lpartial50
	cmp	r0, sl
	bcc.n	.Lpartial58
	cmp	r1, #2
	bls.n	.Lpartial52
	cmp	r3, #2
	bls.n	.Lpartial53
.Lpartial49:
	subs	r2, #4
	ldr.w	r3, [r0], #4
	str.w	r3, [r4], #4
	bgt.n	.Lpartial49
	subs	r4, r5, r1
	b.n	.Lpartial38
.Lpartial50:
	movs	r0, #252
.Lpartial51:
	adds	r0, #5
	movt	r0, #0x8056
	b.n	.Lpartial61
.Lpartial52:
	ldrb.w	r3, [r0], #1
	subs	r2, #1
	beq.n	.Lpartial37
	strb.w	r3, [r4], #1
	b.n	.Lpartial52
.Lpartial53:
	beq.n	.Lpartial52
	subs	r3, #1
	ldr	r3, [r0, #0]
	it	ne
	bfine	r3, r3, #8, #8
	bfi	r3, r3, #16, #16
.Lpartial54:
	subs	r2, #4
	str.w	r3, [r4], #4
	bgt.n	.Lpartial54
	subs	r4, r5, r1
	b.n	.Lpartial38
.Lpartial55:
	uxtb	r3, r3
	cbz	r3, .Lpartial59
	adds	r6, r6, r3
	ror.w	r1, r9, r6
	ble.n	.Lpartial57
	subs	r6, #16
	cmp	r8, fp
	bne.n	.Lpartial56
	bl	DeflateRefill
	b.n	.Lpartial57
.Lpartial56:
	ldr.w	r9, [r8], #2
.Lpartial57:
	bfi	r2, r1, #12, #17
	rsb	r3, r3, #29
	lsrs	r2, r3
	adds	r2, #3
	b.n	.Lpartial42
.Lpartial58:
	movs	r0, #251
	b.n	.Lpartial51
.Lpartial59:
	lsls	r2, r2, #3
	bcs.n	.Lpartial58
.Lpartial60:
	ldr	r3, [sp, #560]
	lsls	r3, r3, #14
	bpl.w	.Lpartial3
	addw	r0, sp, #0x4ec
	ldmia	r0, {r0, r3}
	subs	r0, r4, r0
	cbz	r3, .Lpartial61
	adds	r6, #8
	add.w	r6, r8, r6, asr #3
	str	r6, [r3, #0]
.Lpartial61:
	addw	sp, sp, #0x4f4
	pop.w	{r4, r5, r6, r7, r8, r9, sl, fp, pc}
.Lpartial62:
	bcs.n	.Lpartial58
	ldr	r1, [sp, #560]
	str	r7, [sp, #560]
	cbz	r3, .Lpartial63
	mov	r0, sp
	bl	DeflateSetupBlock
	b.n	.Lpartial25
.Lpartial63:
	adds	r6, #8
	bics.w	r6, r6, #7
	ble.n	.Lpartial65
	subs	r6, #16
	cmp	r8, fp
	bne.n	.Lpartial64
	bl	DeflateRefill
	b.n	.Lpartial65
.Lpartial64:
	ldr.w	r9, [r8], #2
.Lpartial65:
	ror.w	r3, r9, r6
	cmp	r8, fp
	bne.n	.Lpartial66
	bl	DeflateRefill
	b.n	.Lpartial67
.Lpartial66:
	ldr.w	r9, [r8], #2
.Lpartial67:
	ror.w	r1, r9, r6
	cmp	r8, fp
	bne.n	.Lpartial68
	bl	DeflateRefill
	b.n	.Lpartial69
.Lpartial68:
	ldr.w	r9, [r8], #2
.Lpartial69:
	eors	r1, r3
	asrs	r1, r1, #16
	adds	r1, #1
	bne.n	.Lpartial58
	lsrs	r3, r3, #16
	beq.n	.Lpartial76
	adds	r2, r4, r3
	cmp	r2, r5
	bhi.n	.Lpartial50
	add.w	r8, r8, r6, asr #3
	mov	r6, r2
	ldr.w	r7, [sp, #0x518]
	add.w	fp, fp, #2
.Lpartial70:
	sub.w	r2, fp, r8
	mov	r0, r4
	mov	r1, r8
	cmp	r2, r3
	it	cs
	movcs	r2, r3
	add	r8, r2
	add	r4, r2
	bl	memcpy
	cmp	r8, fp
	bne.n	.Lpartial71
	ldr	r3, [r7, #12]
	mov	r0, r7
	ldr.w	r1, [sp, #0x4ec]
	subs	r1, r4, r1
	blx	r3
	ldr.w	fp, [r7, #4]
	mov	r8, r0
	cbz	r0, .Lpartial73
	subs.w	r0, fp, r0
	beq.n	.Lpartial73
	lsls	r3, r0, #31
	bmi.n	.Lpartial73
.Lpartial71:
	subs	r3, r6, r4
	bne.n	.Lpartial70
	sub.w	fp, fp, #2
	eor.w	r6, fp, r8
	lsls	r6, r6, #31
	beq.n	.Lpartial75
	ldrb.w	r9, [r8], #-1
	mov.w	r9, r9, lsl #8
	mvn.w	r6, #8
	cmp	r8, fp
	bne.n	.Lpartial74
	bl	DeflateRefill
	b.n	.Lpartial60
.Lpartial72:
	add	sp, #24
.Lpartial73:
	movs	r0, #254
	b.n	.Lpartial51
.Lpartial74:
	ldrh.w	r3, [r8, #2]!
	bfi	r9, r3, #16, #16
	b.n	.Lpartial60
.Lpartial75:
	mov.w	r6, #0xffffffff
	ldrh.w	r9, [r8]
	mov.w	r9, r9, lsl #16
	b.n	.Lpartial60
.Lpartial76:
	subs	r6, #1
	b.n	.Lpartial60
	.size	sceDeflateDecompressPartial, . - sceDeflateDecompressPartial

/* DeflateRefill: the next input piece, from the callback in fp. */
	.type	DeflateRefill, %function
	.thumb_func
DeflateRefill:
	push.w	{r0, r1, r2, r3, ip, lr}
	ldr.w	r0, [sp, #0x530]
	ldr.w	fp, [r0, #12]
	ldr.w	r1, [sp, #0x504]
	subs	r1, r4, r1
	blx	fp
	movs.w	r8, r0
	beq.n	.Lpartial72
	ldr.w	r2, [sp, #0x530]
	ldr	r2, [r2, #4]
	subs	r1, r0, r2
	beq.n	.Lpartial72
	lsls	r1, r1, #31
	bne.n	.Lpartial72
	sub.w	fp, r2, #2
	ldrh	r0, [r0, #0]
	bfi	r9, r0, #0, #16
	mov.w	r9, r9, ror #16
	pop.w	{r0, r1, r2, r3, ip, pc}
	.size	DeflateRefill, . - DeflateRefill
