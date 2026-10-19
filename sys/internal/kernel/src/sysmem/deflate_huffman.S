/*
 * Sony's hand-written Huffman table construction for the inflater in
 * deflate.S, with the symbol tables both read. The state holds, per code
 * length, bit masks of the symbols that have it; the tables built index
 * the bit-reversed input directly.
 */
#include "deflate.h"

	.syntax unified
	.arch	armv7-a
	.thumb
	.text

	.balign	4
/*
 * DeflateBuildPrecode(state): the 128-entry lookup of the code-length code
 * from the per-length bit masks of its symbols.
 */
	.global	DeflateBuildPrecode
	.type	DeflateBuildPrecode, %function
	.thumb_func
DeflateBuildPrecode:
	push	{r4, r5, r6, r7, lr}
	movs	r3, #6
	add.w	r7, r0, #688
	adds	r4, r0, #4
	movs	r0, #0
.Lprecode1:
	ldmia	r4!, {r2}
	cbz	r2, .Lprecode4
	mov.w	ip, #128
	mov.w	lr, #1
	lsr.w	ip, ip, r3
	lsl.w	lr, lr, r3
	mvns	r5, r3
.Lprecode2:
	clz	r6, r2
	rbit	r1, r0
	adds	r6, #1
	lsrs	r1, r1, #25
	lsls	r2, r6
	add.w	r5, r5, r6, lsl #3
.Lprecode3:
	strb	r5, [r7, r1]
	add	r1, ip
	cmp	r1, #127
	bls.n	.Lprecode3
	add	r0, lr
	cmp	r2, #0
	bne.n	.Lprecode2
	str.w	r2, [r4, #-4]
.Lprecode4:
	subs	r3, #1
	bcs.n	.Lprecode1
	pop	{r4, r5, r6, r7, pc}
	.size	DeflateBuildPrecode, . - DeflateBuildPrecode

/*
 * DeflateBuild(state): the literal/length and distance decode tables from
 * the code lengths; nonzero when the lengths do not make a complete code.
 */
	.global	DeflateBuild
	.type	DeflateBuild, %function
	.thumb_func
DeflateBuild:
	movs	r2, #0
	push.w	{r4, r5, r6, r7, r8, lr}
	mov	r3, r0
	mov	r5, r0
	movs	r1, #1
	mov	r0, r2
.Lbuild1:
	rsb	r7, r1, #15
	ldr.w	r4, [r5, #4]!
	lsr.w	r6, r0, r7
	subs	r6, r2, r6
	cbz	r4, .Lbuild3
	mov.w	r8, #1
	lsl.w	r8, r8, r7
.Lbuild2:
	clz	lr, r4
	mov.w	ip, #0x80000000
	lsr.w	r7, ip, lr
	movw	ip, #:lower16:deflateDistanceCodes
	movt	ip, #:upper16:deflateDistanceCodes
	ldrb.w	lr, [lr, ip]
	adds	r2, #1
	subs	r4, r4, r7
	add.w	ip, r3, r2
	add	r0, r8
	strb.w	lr, [ip, #591]
	bne.n	.Lbuild2
.Lbuild3:
	cmp	r1, #5
	bls.n	.Lbuild4
	add.w	r4, r6, #592
	add.w	r7, r3, r1, lsl #2
	movt	r4, #0xffff
	add.w	r4, r4, r0, lsl #17
	str.w	r4, [r7, #488]
	b.n	.Lbuild6
.Lbuild4:
	cbnz	r6, .Lbuild5
	str.w	r1, [r3, #632]
.Lbuild5:
	lsrs	r6, r0, #10
	adds	r4, r3, r1
	strb.w	r6, [r4, #551]
.Lbuild6:
	adds	r1, #1
	cmp	r1, #16
	bne.n	.Lbuild1
	cmp	r2, #1
	bhi.n	.Lbuild7
	movs	r1, #32
	movs	r4, #0
	strb.w	r1, [r3, #552]
	adds	r1, r3, r2
	str.w	r2, [r3, #632]
	strb.w	r4, [r1, #592]
	cbz	r2, .Lbuild8
	lsls	r0, r0, #1
.Lbuild7:
	subs.w	r0, r0, #0x8000
	beq.n	.Lbuild8
	pop.w	{r4, r5, r6, r7, r8, pc}
.Lbuild8:
	movs	r2, #0
	mov	r5, r3
	movs	r1, #1
	mov	r0, r2
	mov	r4, r2
.Lbuild9:
	ldr	r6, [r5, #64]
	cbz	r6, .Lbuild10
	add.w	r7, r4, #344
	add	r4, r6
	add.w	r7, r3, r7, lsl #1
	cmp	r2, #0
	it	eq
	moveq	r2, r1
	str	r7, [r5, #64]
	rsb	r7, r1, #15
	lsls	r6, r7
	add	r0, r6
.Lbuild10:
	cmp	r1, #8
	bls.n	.Lbuild11
	rsb	lr, r1, #15
	add.w	r6, r4, #344
	asr.w	lr, r0, lr
	sub.w	r7, r3, r1, lsl #2
	sub.w	r6, r6, lr
	movt	r6, #0xffff
	add.w	r6, r6, r0, lsl #17
	str.w	r6, [r7, #624]
	b.n	.Lbuild12
.Lbuild11:
	asrs	r7, r0, #7
	adds	r6, r3, r1
	subs	r7, #1
	strb.w	r7, [r6, #623]
.Lbuild12:
	adds	r1, #1
	adds	r5, #4
	cmp	r1, #16
	bne.n	.Lbuild9
	subs.w	r0, r0, #0x8000
	bne.w	.Lbuild33
	add.w	r6, r3, #124
	add.w	r4, r3, #60
.Lbuild13:
	ldrb	r5, [r6, r0]
	cbz	r5, .Lbuild14
	sxth	r5, r5
	ldr.w	r1, [r4, r5, lsl #2]
	adds	r7, r1, #2
	str.w	r7, [r4, r5, lsl #2]
	strh	r0, [r1, #0]
.Lbuild14:
	adds	r0, #1
	cmp.w	r0, #256
	bne.n	.Lbuild13
	add.w	r0, r3, #380
	movs	r1, #0
.Lbuild15:
	ldrb.w	r6, [r0], #1
	cbz	r6, .Lbuild16
	sxth	r6, r6
	ldr.w	r5, [r4, r6, lsl #2]
	adds	r7, r5, #2
	str.w	r7, [r4, r6, lsl #2]
	ldr	r6, .LbuildLengthCodes
	ldrh	r6, [r1, r6]
	strh	r6, [r5, #0]
.Lbuild16:
	adds	r1, #2
	cmp	r1, #60
	bne.n	.Lbuild15
	add.w	r6, r3, #688
	movs	r1, #0
.Lbuild17:
	ldrh.w	r4, [r6], #2
	mov.w	r0, #256
	lsrs	r0, r2
	add	r0, r1
	lsls	r5, r4, #21
	bpl.n	.Lbuild18
	ands.w	r5, r4, #7
	beq.n	.Lbuild18
	adds	r7, r5, r2
	cmp	r7, #8
	bhi.n	.Lbuild18
	ubfx	r4, r4, #8, #3
	lsls	r4, r5
	add.w	r4, r4, r7, lsl #12
	movs	r7, #1
	lsl.w	r5, r7, r5
	addw	r4, r4, #0x803
	subs	r5, #1
	b.n	.Lbuild19
.Lbuild18:
	add.w	r4, r4, r2, lsl #12
	movs	r5, #0
.Lbuild19:
	lsls	r7, r1, #24
	adds	r1, #1
	cmp	r1, r0
	rbit	r7, r7
	lsr.w	lr, r7, r2
	and.w	lr, lr, r5
	add	lr, r4
	strh.w	lr, [r3, r7, lsl #1]
	bne.n	.Lbuild19
.Lbuild20:
	adds	r1, r3, r2
	ldrb.w	r1, [r1, #623]
	cmp	r0, r1
	bls.n	.Lbuild21
	adds	r2, #1
	cmp	r2, #8
	bls.n	.Lbuild20
	b.n	.Lbuild22
.Lbuild21:
	mov	r1, r0
	b.n	.Lbuild17
.Lbuild22:
	lsls	r2, r0, #24
	beq.n	.Lbuild24
	movs	r1, #24
	b.n	.Lbuild25
.Lbuild23:
	rbit	r0, r2
	adds.w	r2, r2, #0x1000000
	strh.w	r1, [r3, r0, lsl #1]
	bne.n	.Lbuild25
.Lbuild24:
	add.w	r4, r3, #592
	ldr.w	r2, [r3, #632]
	movs	r1, #0
	b.n	.Lbuild28
.Lbuild25:
	add.w	r0, r3, r1, lsl #2
	ldr.w	r0, [r0, #492]
	cmp	r2, r0
	bcc.n	.Lbuild23
	subs	r1, #1
	b.n	.Lbuild25
.Lbuild26:
	rbit	r5, r1
	adds	r1, #1
	lsrs	r5, r5, #27
	cmp	r1, r0
	add.w	r5, r5, #312
	strh.w	r6, [r3, r5, lsl #1]
	bne.n	.Lbuild26
.Lbuild27:
	adds	r1, r3, r2
	ldrb.w	r1, [r1, #551]
	cmp	r0, r1
	bcs.n	.Lbuild29
	mov	r1, r0
.Lbuild28:
	ldrb.w	r6, [r4], #1
	movs	r0, #32
	lsrs	r0, r2
	add	r0, r1
	add.w	r6, r6, r2, lsl #12
	b.n	.Lbuild26
.Lbuild29:
	adds	r2, #1
	cmp	r2, #5
	ble.n	.Lbuild27
	lsls	r1, r0, #27
	b.n	.Lbuild31
.Lbuild30:
	lsls	r4, r2, #12
	rbit	r0, r1
	adds.w	r1, r1, #0x8000000
	add.w	r0, r0, #312
	sub.w	r4, r4, #0x800
	strh.w	r4, [r3, r0, lsl #1]
.Lbuild31:
	bne.n	.Lbuild32
	movs	r0, #0
	pop.w	{r4, r5, r6, r7, r8, pc}
.Lbuild32:
	add.w	r0, r2, #122
	ldr.w	r0, [r3, r0, lsl #2]
	cmp	r1, r0
	bcc.n	.Lbuild30
	adds	r2, #1
	b.n	.Lbuild32
.Lbuild33:
	pop.w	{r4, r5, r6, r7, r8, pc}
.LbuildLengthCodes:
	.word	deflateLengthCodes
	.size	DeflateBuild, . - DeflateBuild

/*
 * DeflateSetupBlock(state, header): the fixed-code decode tables of RFC 1951
 * 3.2.6, unless bits 18-19 of the header word are 1.
 */
	.global	DeflateSetupBlock
	.type	DeflateSetupBlock, %function
	.thumb_func
DeflateSetupBlock:
	ubfx	r1, r1, #18, #2
	push	{r4, r5, lr}
	cmp	r1, #1
	beq.n	.Lsetup8
	mov	r2, r0
	movs	r3, #0
.Lsetup1:
	movs	r1, #24
	rbit	r4, r3
	strh	r1, [r2, #14]
	lsrs	r4, r4, #27
	strh	r1, [r2, #6]
	adds	r2, #16
	ldr	r1, .LsetupLengthCodes
	add.w	r4, r4, #312
	adds	r5, r1, r3
	adds	r3, #1
	ldrb.w	r5, [r5, #64]
	cmp	r3, #32
	add.w	r5, r5, #0x5000
	strh.w	r5, [r0, r4, lsl #1]
	bne.n	.Lsetup1
	movs	r3, #0
.Lsetup2:
	ldrh.w	r2, [r1, r3, lsl #1]
	rbit	r5, r3
	lsrs	r5, r5, #25
	uxtb	r4, r2
	cmp	r4, #1
	bne.n	.Lsetup3
	lsrs	r2, r2, #7
	add.w	r4, r2, #0x8700
	add.w	r2, r4, #243
	adds	r4, #244
	b.n	.Lsetup4
.Lsetup3:
	add.w	r2, r2, #0x7000
	mov	r4, r2
.Lsetup4:
	adds	r3, #1
	strh.w	r2, [r0, r5, lsl #1]
	cmp	r3, #24
	add.w	r5, r5, #128
	strh.w	r4, [r0, r5, lsl #1]
	bne.n	.Lsetup2
	movs	r3, #48
.Lsetup5:
	rbit	r2, r3
	add.w	r4, r3, #0x7f80
	adds	r3, #1
	lsrs	r2, r2, #24
	cmp	r3, #192
	add.w	r4, r4, #80
	strh.w	r4, [r0, r2, lsl #1]
	bne.n	.Lsetup5
.Lsetup6:
	add.w	r4, r1, r3, lsl #1
	rbit	r2, r3
	adds	r3, #1
	lsrs	r2, r2, #24
	sub.w	r4, r4, #336
	ldrh	r4, [r4, #0]
	cmp	r3, #200
	sub.w	r4, r4, #0x8000
	strh.w	r4, [r0, r2, lsl #1]
	bne.n	.Lsetup6
	movs	r3, #144
.Lsetup7:
	add.w	r2, r0, r3, lsl #1
	strh.w	r3, [r2, #416]
	adds	r3, #1
	cmp.w	r3, #256
	bne.n	.Lsetup7
	mvn.w	r3, #47
	str.w	r3, [r0, #588]
.Lsetup8:
	pop	{r4, r5, pc}
	.balign	4
.LsetupLengthCodes:
	.word	deflateLengthCodes
	.size	DeflateSetupBlock, . - DeflateSetupBlock

	.section .rodata
	.balign	4
/* the literal/length symbols 256..287 (RFC 1951 3.2.5) */
	.type	deflateLengthCodes, %object
deflateLengthCodes:
	.short	DEFLATE_END_OF_BLOCK
	.short	DEFLATE_LENGTH(3), DEFLATE_LENGTH(4), DEFLATE_LENGTH(5), DEFLATE_LENGTH(6)
	.short	DEFLATE_LENGTH(7), DEFLATE_LENGTH(8), DEFLATE_LENGTH(9), DEFLATE_LENGTH(10)
	.short	DEFLATE_LENGTH_EXTRA(1, 0), DEFLATE_LENGTH_EXTRA(1, 1), DEFLATE_LENGTH_EXTRA(1, 2), DEFLATE_LENGTH_EXTRA(1, 3)
	.short	DEFLATE_LENGTH_EXTRA(2, 0), DEFLATE_LENGTH_EXTRA(2, 1), DEFLATE_LENGTH_EXTRA(2, 2), DEFLATE_LENGTH_EXTRA(2, 3)
	.short	DEFLATE_LENGTH_EXTRA(3, 0), DEFLATE_LENGTH_EXTRA(3, 1), DEFLATE_LENGTH_EXTRA(3, 2), DEFLATE_LENGTH_EXTRA(3, 3)
	.short	DEFLATE_LENGTH_EXTRA(4, 0), DEFLATE_LENGTH_EXTRA(4, 1), DEFLATE_LENGTH_EXTRA(4, 2), DEFLATE_LENGTH_EXTRA(4, 3)
	.short	DEFLATE_LENGTH_EXTRA(5, 0), DEFLATE_LENGTH_EXTRA(5, 1), DEFLATE_LENGTH_EXTRA(5, 2), DEFLATE_LENGTH_EXTRA(5, 3)
	.short	DEFLATE_LENGTH(258)
	.short	DEFLATE_INVALID, DEFLATE_INVALID
	.size	deflateLengthCodes, . - deflateLengthCodes
/* the distance symbols 0..31 (RFC 1951 3.2.5) */
	.type	deflateDistanceCodes, %object
deflateDistanceCodes:
	.byte	DEFLATE_DISTANCE(0, 0), DEFLATE_DISTANCE(0, 1), DEFLATE_DISTANCE(0, 2), DEFLATE_DISTANCE(0, 3)
	.byte	DEFLATE_DISTANCE(1, 2), DEFLATE_DISTANCE(1, 3), DEFLATE_DISTANCE(2, 2), DEFLATE_DISTANCE(2, 3)
	.byte	DEFLATE_DISTANCE(3, 2), DEFLATE_DISTANCE(3, 3), DEFLATE_DISTANCE(4, 2), DEFLATE_DISTANCE(4, 3)
	.byte	DEFLATE_DISTANCE(5, 2), DEFLATE_DISTANCE(5, 3), DEFLATE_DISTANCE(6, 2), DEFLATE_DISTANCE(6, 3)
	.byte	DEFLATE_DISTANCE(7, 2), DEFLATE_DISTANCE(7, 3), DEFLATE_DISTANCE(8, 2), DEFLATE_DISTANCE(8, 3)
	.byte	DEFLATE_DISTANCE(9, 2), DEFLATE_DISTANCE(9, 3), DEFLATE_DISTANCE(10, 2), DEFLATE_DISTANCE(10, 3)
	.byte	DEFLATE_DISTANCE(11, 2), DEFLATE_DISTANCE(11, 3), DEFLATE_DISTANCE(12, 2), DEFLATE_DISTANCE(12, 3)
	.byte	DEFLATE_DISTANCE(13, 2), DEFLATE_DISTANCE(13, 3), 0, 0
	.size	deflateDistanceCodes, . - deflateDistanceCodes
/*
 * the order of the code-length code lengths (RFC 1951 3.2.7); the repeat
 * symbols 17 and 18 are kept at the slots 19 and 23
 */
	.global	deflateCodeLengthOrder
	.type	deflateCodeLengthOrder, %object
deflateCodeLengthOrder:
	.byte	16, 19, 23, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
	.size	deflateCodeLengthOrder, . - deflateCodeLengthOrder
