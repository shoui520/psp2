/*
 * libSceDeflt's inflater (RFC 1951), which Sony wrote in assembly: the SDK's
 * libSceDeflt.a builds sceDeflateDecompress from deflate.S with GNU AS, and the
 * labels and frame offsets here are the ones its object keeps. The bit buffer
 * and the output position live in registers for the whole stream; the Huffman
 * tables, built by deflate_common.c, sit in a frame on the stack.
 *
 * SceSysmem builds the same code into its .text (DEFLATE_SYSMEM), with its own
 * table builders and without unwind information.
 */
/* the memcpy the library links: libfios2's copy calls sceClibMemcpy, libpaf's sceClibMemcpy_safe */
#ifndef DEFLATE_MEMCPY
#define DEFLATE_MEMCPY sceClibMemcpy
#endif

	.syntax unified
	.arch	armv7-a
	.thumb

#ifdef SCE_DEFLT_2014
/*
 * 2014 SDK 3.10 libSceDeflt.a revision of sceDeflateDecompress (0x4e4-byte frame,
 * inline fast-table expansion after _sceDeflateMakeHuffmanTable, 8-level distance
 * threshold chain, and byte-wise stored block copy).
 */
	.set	WK2014_LITLEN, 0x7c
	.set	WK2014_DISTSYM, 0x200
	.set	WK2014_DISTLIM, 0x23c
	.set	WK2014_DISTBASE, 0x274
	.set	WK2014_LITLIM, 0x278
	.set	WK2014_LENSYM, 0x294
	.set	WK2014_LITINFO, 0x2d8
	.set	WK2014_FASTCNT, 0x4ce
	.set	WK2014_ENDFLAG, 0x4e0
	.set	WK2014_FRAMESIZE, 0x4e4
	.set	WK2014_PDST0, 0x4e4
	.set	WK2014_PNEXT, 0x4e8

	.section .text.sceDeflateDecompress, "ax", %progbits
	.global	sceDeflateDecompress
	.type	sceDeflateDecompress, %function
	.thumb_func
sceDeflateDecompress:
	.fnstart
	push.w	{r0, r3, r4, r5, r6, r7, r8, r9, sl, fp, lr}
	.save	{r0, r3, r4, r5, r6, r7, r8, r9, sl, fp, lr}
	subw	sp, sp, #WK2014_FRAMESIZE
	.pad	#WK2014_FRAMESIZE
	and.w	r6, r2, #1
	lsls	r6, r6, #3
	bic.w	r8, r2, #1
	ldr.w	r9, [r8], #2
	mov	r4, r0
	subs	r6, #17
	adds	r5, r0, r1
.blockLoop:
	ror.w	r3, r9, r6
	adds	r6, #3
	itt	gt
	ldrgt.w	r9, [r8], #2
	subgt	r6, #16
	str.w	r3, [sp, #WK2014_ENDFLAG]
	ubfx	r3, r3, #18, #2
	mov	r0, sp
	subs	r2, r3, #2
	bne.w	.noDynamic
	add	r3, sp, #416
.Linflate2:
	strd	r2, r2, [r0], #8
	cmp	r0, r3
	bne.n	.Linflate2
	ror.w	r7, r9, r6
	adds	r6, #14
	itt	gt
	ldrgt.w	r9, [r8], #2
	subgt	r6, #16
	ubfx	r1, r7, #27, #4
	adds	r1, #4
	movw	r3, #:lower16:_sceDeflateLengthOrder
	movt	r3, #:upper16:_sceDeflateLengthOrder
	adds	r1, r1, r3
	mov.w	fp, #0x80000000
.Linflate3:
	ror.w	r2, r9, r6
	adds	r6, #3
	itt	gt
	ldrgt.w	r9, [r8], #2
	subgt	r6, #16
	ldrb.w	r0, [r3], #1
	ubfx	r2, r2, #17, #3
	cmp	r3, r1
	ldr.w	lr, [sp, r2, lsl #2]
	lsr.w	r0, fp, r0
	orr.w	r0, lr, r0
	str.w	r0, [sp, r2, lsl #2]
	blt.n	.Linflate3
	mov	r0, sp
	bl	_sceDeflateMakeHuffmanTableLength
	ubfx	r3, r7, #17, #5
	ubfx	r7, r7, #22, #5
	cmp	r0, #0
	it	ls
	cmpls	r3, #29
	it	ls
	cmpls	r7, #29
	bhi.n	.errorInvalid2
	addw	ip, r3, #257
	add	r2, sp, #WK2014_LITLEN
	add	ip, r2
	mov	r3, fp
	add.w	sl, sp, #WK2014_LENSYM
.litSymLenReadLoop:
	ror.w	r1, r9, r6
	ubfx	r1, r1, #17, #7
	ldrsb.w	r1, [sl, r1]
	and.w	r0, r1, #7
	adds	r6, r6, r0
	itt	gt
	ldrgt.w	r9, [r8], #2
	subgt	r6, #16
	asrs	r1, r1, #3
	bmi.n	.litSymLenSub
	strb.w	r1, [r2], #1
	mov	r3, r1
	beq.n	.litSymLenReadNext
	adds	r1, #15
	ldr.w	r0, [sp, r1, lsl #2]
	adds	r0, #1
	str.w	r0, [sp, r1, lsl #2]
.litSymLenReadNext:
	subs.w	r0, r2, ip
	bmi.n	.litSymLenReadLoop
	lsrs.w	r1, fp, r0
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
	ble.n	.MakeHuffTable
.DistSymLenReadLoop:
	ror.w	r2, r9, r6
	ubfx	r2, r2, #17, #7
	ldrsb.w	r2, [sl, r2]
	and.w	r0, r2, #7
	adds	r6, r6, r0
	itt	gt
	ldrgt.w	r9, [r8], #2
	subgt	r6, #16
	asrs	r2, r2, #3
	bmi.n	.distSymLenSub
	ldr.w	r0, [sp, r2, lsl #2]
	orrs	r0, r1
	mov	r3, r2
	movs	r2, #1
.DistSymLenReadNext0:
	str.w	r0, [sp, r3, lsl #2]
.DistSymLenReadNext:
	subs	r7, r7, r2
	ble.n	.MakeHuffTable
	lsrs	r1, r2
	b.n	.DistSymLenReadLoop
.errorInvalid2:
	b.n	.errorInvalid
.litSymLenSub:
	adds	r1, #16
	cbnz	r1, .Linflate15
	ror.w	r1, r9, r6
	adds	r6, #2
	ubfx	r1, r1, #17, #2
	adds	r1, #3
	cbz	r3, .Linflate16
	adds.w	r0, r3, #15
	bmi.n	.errorInvalid2
	ldr.w	lr, [sp, r0, lsl #2]
	add	lr, r1
	str.w	lr, [sp, r0, lsl #2]
.Linflate14:
	subs	r1, #1
	strb.w	r3, [r2], #1
	bne.n	.Linflate14
	b.n	.litSymLenReadNext
.Linflate15:
	adds.w	r2, r2, r1, lsl #1
	subs	r2, #3
	adds	r6, r6, r1
	ror.w	r3, r9, r6
	lsls	r3, r3, #15
	rsb.w	r1, r1, #32
	lsrs.w	r1, r3, r1
	movs	r3, #0
.Linflate16:
	adds	r2, r2, r1
	b.n	.litSymLenReadNext
.distSymLenSub:
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
	b.n	.DistSymLenReadNext0
.Linflate18:
	adds	r6, r6, r2
	ror.w	r3, r9, r6
	lsls	r3, r3, #15
	rsb.w	r0, r2, #32
	lsrs	r3, r0
	adds.w	r2, r3, r2, lsl #1
	subs	r2, #3
	movs	r3, #0
	b.n	.DistSymLenReadNext
.MakeHuffTable:
	bne.n	.errorInvalid2
	cmp	r6, #0
	itt	gt
	ldrgt.w	r9, [r8], #2
	subgt	r6, #16
	mov	r0, sp
	bl	_sceDeflateMakeHuffmanTable
	cmp	r0, #0
	blt.n	.errorInvalid2
.makeFastTable:
	addw	lr, sp, #WK2014_FASTCNT
	movs	r3, #0
	add	r2, sp, #WK2014_LENSYM
.fastSymLoop:
	ldrh.w	ip, [r2], #2
	mov.w	r1, #256
	asrs	r1, r0
	orr.w	ip, ip, r0, lsl #12
.fastFillLoop:
	rbit	r7, r3
	adds.w	r3, r3, #0x01000000
	subs	r1, #1
	strh.w	ip, [sp, r7, lsl #1]
	bne.n	.fastFillLoop
	lsrs	r1, r3, #17
	beq.n	.makeFastDone
	add.w	ip, lr, r0, lsl #1
.fastLenAdvance:
	ldrh.w	r7, [ip], #2
	cmp	r1, r7
	blo.n	.fastSymLoop
	adds	r0, #1
	cmp	r0, #8
	bls.n	.fastLenAdvance
	add	r1, sp, #WK2014_LITLIM
	rsb.w	r0, r0, #34
.fastLongLoad:
	ldr.w	ip, [r1], #4
	subs	r0, #1
.fastLongLoop:
	cmp	r3, ip
	bhs.n	.fastLongLoad
	rbit	r2, r3
	strh.w	r0, [sp, r2, lsl #1]
	adds.w	r3, r3, #0x01000000
	blo.n	.fastLongLoop
.makeFastDone:
	ldr.w	sl, [sp, #WK2014_PDST0]
	add.w	fp, sp, #WK2014_LITINFO
	add.w	ip, sp, #WK2014_DISTSYM
	b.n	.mainLoop
.multiBit:
	rbit	r0, r1
	sub.w	r1, fp, r3, lsl #2
	lsls	r0, r0, #17
.Linflate20:
	ldr.w	r2, [r1], #4
	subs	r3, #1
	cmp	r0, r2
	bhs.n	.Linflate20
	sxth	r1, r2
	rsb.w	r2, r3, #32
	lsrs	r0, r3
	subs	r0, r0, r1
	ldrh.w	r3, [sp, r0, lsl #1]
	cmp.w	r3, #0x800
	bge.n	.dictCopy
.noCopySymbol:
	cmp	r4, r5
	beq.w	.errorOverrun
	adds	r6, r6, r2
	itt	gt
	ldrgt.w	r9, [r8], #2
	subgt	r6, #16
	strb.w	r3, [r4], #1
.mainLoop:
	ror.w	r1, r9, r6
	ubfx	r3, r1, #17, #8
	ldrh.w	r3, [sp, r3, lsl #1]
	asrs	r2, r3, #12
	beq.n	.multiBit
	blo.n	.noCopySymbol
.dictCopy:
	adds	r6, r6, r2
	itt	gt
	ldrgt.w	r9, [r8], #2
	subgt	r6, #16
	ubfx	r2, r3, #3, #8
	ands.w	r3, r3, #7
	beq.n	.distDec
	cmp	r2, r3
	ble.w	.endCheck
	adds	r6, r6, r3
	ror.w	r1, r9, r6
	itt	gt
	ldrgt.w	r9, [r8], #2
	subgt	r6, #16
	lsls	r1, r1, #15
	rsb.w	r3, r3, #32
	lsrs	r1, r3
	adds	r2, r2, r1
.distDec:
	ldr	r1, [sp, #WK2014_DISTBASE]
	ror.w	r3, r9, r6
	rbit	r3, r3
	lsls	r3, r3, #17
	ldrd	lr, r7, [sp, #WK2014_DISTLIM + 48]
	cmp	r3, r1
	blo.n	.distLookup
	cmp	r3, lr
	blo.n	.distResolve
	ldrd	lr, r7, [sp, #WK2014_DISTLIM + 40]
	adds	r1, #2
	cmp	r3, lr
	blo.n	.distResolve
	ldrd	lr, r7, [sp, #WK2014_DISTLIM + 32]
	adds	r1, #2
	cmp	r3, lr
	blo.n	.distResolve
	ldrd	lr, r7, [sp, #WK2014_DISTLIM + 24]
	adds	r1, #2
	cmp	r3, lr
	blo.n	.distResolve
	ldrd	lr, r7, [sp, #WK2014_DISTLIM + 16]
	adds	r1, #2
	cmp	r3, lr
	blo.n	.distResolve
	ldrd	lr, r7, [sp, #WK2014_DISTLIM + 8]
	adds	r1, #2
	cmp	r3, lr
	blo.n	.distResolve
	ldrd	lr, r7, [sp, #WK2014_DISTLIM]
	adds	r1, #2
.distResolve:
	cmp	r3, r7
	adc.w	r1, r1, #1
	it	hs
	movhs	r7, lr
	sub.w	r3, r3, r7, lsl #17
.distLookup:
	and.w	r1, r1, #15
	adds	r6, r6, r1
	rsb.w	r1, r1, #32
	lsr.w	r3, r3, r1
	ldrsh.w	r3, [ip, r3, lsl #1]
	itt	gt
	ldrgt.w	r9, [r8], #2
	subgt	r6, #16
	cmp	r3, #3
	ble.n	.distSmall
	clz	r1, r3
	adds	r6, #30
	subs	r6, r6, r1
	ror.w	r7, r9, r6
	itt	gt
	ldrgt.w	r9, [r8], #2
	subgt	r6, #16
	lsls	r7, r7, #15
	lsrs	r7, r1
	adds.w	r3, r3, r7, lsr #2
.dictCopyStart:
	adds	r2, #3
	adds	r1, r2, r4
	subs	r1, r5, r1
	blo.n	.errorOverrun
	subs	r0, r4, r3
	cmp	r0, sl
	bls.n	.errorInvalid
	subs	r0, #1
	cmp	r3, #2
	bls.n	.byteCopy
	cmp	r1, #2
	bls.n	.byteCopy
.Linflate28:
	subs	r2, #4
	ldr.w	r3, [r0], #4
	str.w	r3, [r4], #4
	bgt.n	.Linflate28
	subs	r4, r5, r1
	b.n	.mainLoop
.byteCopy:
	subs	r2, #1
	ldrb.w	r3, [r0], #1
	strb.w	r3, [r4], #1
	bne.n	.byteCopy
	b.n	.mainLoop
.distSmall:
	bls.n	.dictCopyStart
.errorInvalid:
	movw	r0, #0x100
	b.n	.error
.errorOverrun:
	movw	r0, #0x101
.error:
	movt	r0, #0x8056
	b.n	.return
.noDynamic:
	bhs.n	.errorInvalid
	cbz	r3, .noComp
	bl	_sceDeflateMakeHuffmanFixcode
	b.n	.makeFastTable
.noComp:
	adds	r6, #8
	bics.w	r6, r6, #7
	itt	gt
	ldrgt.w	r9, [r8], #2
	subgt	r6, #16
	ror.w	r3, r9, r6
	ldr.w	r9, [r8], #2
	ror.w	r1, r9, r6
	ldr.w	r9, [r8], #2
	eors	r1, r3
	asrs	r1, r1, #16
	adds	r1, #1
	bne.n	.errorInvalid
	lsrs	r3, r3, #16
	cbz	r3, .noCompDone
	adds	r2, r4, r3
	cmp	r2, r5
	bhi.n	.errorOverrun
.noCompCopy:
	ror.w	r1, r9, r6
	adds	r6, #8
	itt	gt
	ldrgt.w	r9, [r8], #2
	subgt	r6, #16
	lsrs	r1, r1, #16
	strb.w	r1, [r4], #1
	cmp	r2, r4
	bne.n	.noCompCopy
.noCompDone:
	subs	r6, #1
	b.n	.nextBlock
.endCheck:
	bne.n	.errorInvalid
.nextBlock:
	ldr.w	r3, [sp, #WK2014_ENDFLAG]
	lsls	r3, r3, #14
	bpl.w	.blockLoop
	ldr.w	r3, [sp, #WK2014_PNEXT]
	ldr.w	r0, [sp, #WK2014_PDST0]
	subs	r0, r4, r0
	cbz	r3, .return
	adds	r6, #8
	add.w	r6, r8, r6, asr #3
	str	r6, [r3, #0]
.return:
	addw	sp, sp, #WK2014_FRAMESIZE + 8
	pop.w	{r4, r5, r6, r7, r8, r9, sl, fp, pc}
	.fnend
	.size	sceDeflateDecompress, . - sceDeflateDecompress

#else
/* the frame: the work area, the saved r0 (dst) and r3 (next) above it */
	.set	WK_COUNT, 0x40		/* code counts per length */
	.set	WK_LITLEN, 0x7c		/* literal/length and distance code lengths */
	.set	WK_ENDFLAG, 0x230	/* the block header; bit 17 marks the last block */
	.set	WK_LITINFO, 0x234
	.set	WK_FTDIST, 0x270	/* the distance table */
	.set	WK_LENSYM, 0x2b0	/* the code length code's fast table */
	.set	WK_FRAMESIZE, 0x4ec
	.set	WK_PDST0, 0x4ec

#if DEFLATE_SYSMEM
	.text
	.balign	4
#else
	.section .text.sceDeflateDecompress, "ax", %progbits
#endif
/*
 * Inflate a raw deflate stream (src) into dst_size bytes at dst; *next, when
 * given, receives the end of the input. Returns the output size or an error.
 */
	.global	sceDeflateDecompress
	.type	sceDeflateDecompress, %function
	.thumb_func
sceDeflateDecompress:
#if !DEFLATE_SYSMEM
	.fnstart
#endif
	push.w	{r0, r3, r4, r5, r6, r7, r8, r9, sl, fp, lr}
#if !DEFLATE_SYSMEM
	.save	{r0, r3, r4, r5, r6, r7, r8, r9, sl, fp, lr}
#endif
	subw	sp, sp, #WK_FRAMESIZE
#if !DEFLATE_SYSMEM
	.pad	#WK_FRAMESIZE
#endif
	lsls	r6, r2, #31
	str	r6, [sp, #WK_ENDFLAG]
	bic.w	r8, r2, #1
	lsrs	r6, r6, #28
	ldr.w	r9, [r8], #2
	mov	r4, r0
	subs	r6, #17
	adds	r5, r0, r1
.blockLoop:
	ror.w	r7, r9, r6
	adds	r6, #3
	itt	gt
	ldrgt.w	r9, [r8], #2
	subgt	r6, #16
	ubfx	r3, r7, #18, #2
	subs	r2, r3, #2
	bne.w	.noDynamic
	str	r7, [sp, #WK_ENDFLAG]
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
	movw	ip, #:lower16:_sceDeflateLengthOrder
	movt	ip, #:upper16:_sceDeflateLengthOrder
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
	bl	_sceDeflateMakeHuffmanTableLength
	ubfx	r3, r7, #17, #5
	ubfx	r7, r7, #22, #5
	cmp	r3, #29
	it	ls
	cmpls	r7, #29
	it	ls
	cmpls	r0, #128
	bne.n	.errorInvalid2
	addw	ip, r3, #257
	add	r2, sp, #WK_LITLEN
	add	ip, r2
	mov	r3, fp
	add.w	sl, sp, #WK_LENSYM
.litSymLenReadLoop:
	ror.w	r1, r9, r6
	ubfx	r1, r1, #17, #7
	ldrsb.w	r1, [sl, r1]
	and.w	r0, r1, #7
	adds	r6, r6, r0
	itt	gt
	ldrgt.w	r9, [r8], #2
	subgt	r6, #16
	asrs	r1, r1, #3
	bmi.n	.litSymLenSub
	strb.w	r1, [r2], #1
	mov	r3, r1
	beq.n	.litSymLenReadNext
	adds	r1, #15
	ldr.w	r0, [sp, r1, lsl #2]
	adds	r0, #1
	str.w	r0, [sp, r1, lsl #2]
.litSymLenReadNext:
	subs.w	r0, r2, ip
	bmi.n	.litSymLenReadLoop
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
	ble.n	.MakeHuffTable
.DistSymLenReadLoop:
	ror.w	r2, r9, r6
	ubfx	r2, r2, #17, #7
	ldrsb.w	r2, [sl, r2]
	and.w	r0, r2, #7
	adds	r6, r6, r0
	itt	gt
	ldrgt.w	r9, [r8], #2
	subgt	r6, #16
	asrs	r2, r2, #3
	bmi.n	.distSymLenSub
	ldr.w	r0, [sp, r2, lsl #2]
	orrs	r0, r1
	mov	r3, r2
	movs	r2, #1
.DistSymLenReadNext0:
	str.w	r0, [sp, r3, lsl #2]
.DistSymLenReadNext:
	subs	r7, r7, r2
	ble.n	.MakeHuffTable
	lsrs	r1, r2
	b.n	.DistSymLenReadLoop
.MakeHuffTable:
	bne.n	.errorInvalid2
	cmp	r6, #0
	itt	gt
	ldrgt.w	r9, [r8], #2
	subgt	r6, #16
	mov	r0, sp
	bl	_sceDeflateMakeHuffmanTable
	cbnz	r0, .errorInvalid2
.makeFastDone:
	ldr.w	sl, [sp, #WK_PDST0]
	add.w	fp, sp, #492
	add.w	ip, sp, #WK_FTDIST
	b.n	.mainLoop
.errorInvalid2:
	b.n	.errorInvalid
.litSymLenSub:
	adds	r1, #16
	cbnz	r1, .Linflate15
	ror.w	r1, r9, r6
	adds	r6, #2
	ubfx	r1, r1, #17, #2
	adds	r1, #3
	cbz	r3, .Linflate16
	adds.w	r0, r3, #15
	bmi.n	.errorInvalid2
	ldr.w	lr, [sp, r0, lsl #2]
	add	lr, r1
	str.w	lr, [sp, r0, lsl #2]
.Linflate14:
	subs	r1, #1
	strb.w	r3, [r2], #1
	bne.n	.Linflate14
	b.n	.litSymLenReadNext
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
	b.n	.litSymLenReadNext
.distSymLenSub:
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
	b.n	.DistSymLenReadNext0
.Linflate18:
	add	r6, r2
	ror.w	r3, r9, r6
	lsls	r3, r3, #15
	rsb	r0, r2, #32
	lsrs	r3, r0
	add.w	r2, r3, r2, lsl #1
	subs	r2, #3
	movs	r3, #0
	b.n	.DistSymLenReadNext
.multiBit:
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
	bge.n	.dictCopy
.noCopySymbol:
	cmp	r4, r5
	beq.n	.errorOverrun
	adds	r6, r6, r2
	itt	gt
	ldrgt.w	r9, [r8], #2
	subgt	r6, #16
.mainLoop2:
	strb.w	r3, [r4], #1
.mainLoop:
	ror.w	r1, r9, r6
	ubfx	r3, r1, #17, #8
	add	r3, r3
	ldrh.w	r3, [sp, r3]
	asrs	r2, r3, #12
	beq.n	.multiBit
	bcc.n	.noCopySymbol
.dictCopy:
	adds	r6, r6, r2
	itt	gt
	ldrgt.w	r9, [r8], #2
	subgt	r6, #16
	lsls	r2, r3, #21
	bmi.n	.extraLen
	lsrs	r2, r2, #21
.distDec:
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
	beq.n	.errorInvalid
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
	bcc.n	.errorOverrun
	cmp	r0, sl
	bcc.n	.errorInvalid
	cmp	r1, #2
	bls.n	.byteCopy
	cmp	r3, #2
	bls.n	.byteCopy2
.Linflate28:
	subs	r2, #4
	ldr.w	r3, [r0], #4
	str.w	r3, [r4], #4
	bgt.n	.Linflate28
	subs	r4, r5, r1
	b.n	.mainLoop
.byteCopy:
	ldrb.w	r3, [r0], #1
	subs	r2, #1
	beq.n	.mainLoop2
	strb.w	r3, [r4], #1
	b.n	.byteCopy
.byteCopy2:
	beq.n	.byteCopy
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
	b.n	.mainLoop
.extraLen:
	uxtb	r3, r3
	cbz	r3, .endCheck
	adds	r6, r6, r3
	ror.w	r1, r9, r6
	itt	gt
	ldrgt.w	r9, [r8], #2
	subgt	r6, #16
	bfi	r2, r1, #12, #17
	rsb	r3, r3, #29
	lsrs	r2, r3
	adds	r2, #3
	b.n	.distDec
.errorOverrun:
	movs	r0, #252
.error:
	adds	r0, #5
	movt	r0, #0x8056
	b.n	.return
.errorInvalid:
	movs	r0, #251
	b.n	.error
.noDynamic:
	bcs.n	.errorInvalid
	ldr	r1, [sp, #WK_ENDFLAG]
	str	r7, [sp, #WK_ENDFLAG]
	cbz	r3, .noComp
	mov	r0, sp
	bl	_sceDeflateMakeHuffmanFixcode
	b.n	.makeFastDone
.noComp:
	adds	r6, #8
	add.w	r1, r8, r6, asr #3
	ldr.w	r3, [r1], #4
	eor.w	r6, r3, r3, ror #16
	adds	r2, r6, #1
	bne.n	.errorInvalid
	uxth	r2, r3
	mov	r0, r4
	add	r4, r2
	cmp	r4, r5
	bhi.n	.errorOverrun
	add.w	r8, r1, r2
	bl	DEFLATE_MEMCPY
	ldr.w	r9, [r8], #2
	subs	r6, #16
	b.n	.nextBlock
.endCheck:
	lsls	r2, r2, #3
	bcs.n	.errorInvalid
.nextBlock:
	ldr	r3, [sp, #WK_ENDFLAG]
	lsls	r3, r3, #14
	bpl.w	.blockLoop
	addw	r0, sp, #WK_PDST0
	ldmia	r0, {r0, r3}
	subs	r0, r4, r0
	cbz	r3, .return
	adds	r6, #8
	add.w	r6, r8, r6, asr #3
	str	r6, [r3, #0]
.return:
	addw	sp, sp, #WK_FRAMESIZE + 8
	pop.w	{r4, r5, r6, r7, r8, r9, sl, fp, pc}
#if !DEFLATE_SYSMEM
	.fnend
#endif
	.size	sceDeflateDecompress, . - sceDeflateDecompress
#endif
