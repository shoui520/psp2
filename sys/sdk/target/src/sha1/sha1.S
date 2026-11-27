/*
 * libSceSha1's compression (the SDK's sha1.o) in the 3.10 SDK's revision (SceLibMarlin links
 * it, SDK 3.100.010): Sony's hand-written Sha1Transform(block, state). The sixteen-word
 * schedule is the caller's block, rewritten in place from round 16 on; a..e live in r7..r11,
 * the state pointer and its five words wait on the stack. Returns the block. 3.570.011's
 * revision is SceKernelUtils' (sysmem's sha1_transform.S).
 */
	.syntax unified
	.arch	armv7-a
	.thumb
	.text

	.balign	2
	.global	Sha1Transform
	.type	Sha1Transform, %function
	.thumb_func
Sha1Transform:
	stmdb	sp!, {r4, r5, r6, r7, r8, r9, sl, fp, lr}
#ifdef SCE_SHA1_2014
	ldmia.w	r1, {r7, r8, r9, sl, fp}
	stmdb	sp!, {r1, r7, r8, r9, sl, fp}
#else
	subs.w	sp, sp, #24
	ldmia.w	r1, {r7, r8, r9, sl, fp}
	stmia.w	sp, {r1, r7, r8, r9, sl, fp}
#endif
	mov.w	r1, #0
.Lround:
	/* w = block[t & 15], from round 16 on rol1(w ^ w[t+2] ^ w[t+8] ^ w[t+13]), stored */
	ands.w	ip, r1, #15
	cmp	r1, #16
	ldr.w	r2, [r0, ip, lsl #2]
#ifdef SCE_SHA1_2014
	blt.n	.Lsum
#else
	blt.w	.Lsum
#endif
	adds	r4, r1, #2
	adds.w	r5, r1, #8
	adds.w	r6, r1, #13
	ands.w	r4, r4, #15
	ands.w	r5, r5, #15
	ands.w	r6, r6, #15
	ldr.w	r4, [r0, r4, lsl #2]
	ldr.w	r5, [r0, r5, lsl #2]
	ldr.w	r6, [r0, r6, lsl #2]
	eors	r2, r4
	eors	r5, r6
	eors	r2, r5
	mov.w	r2, r2, ror #31
	str.w	r2, [r0, ip, lsl #2]
.Lsum:
	/* w + e + rol5(a), then the round's function and constant */
	mov.w	r4, r7, ror #27
	adds.w	r2, r2, fp
	adds	r2, r2, r4
	cmp	r1, #20
#ifdef SCE_SHA1_2014
	blt.n	.Lchoose
	cmp	r1, #40
	blt.n	.Lparity
	cmp	r1, #60
	blt.n	.Lmajority
#else
	blt.w	.Lchoose
	cmp	r1, #40
	blt.w	.Lparity
	cmp	r1, #60
	blt.w	.Lmajority
#endif
	eors.w	r4, r8, r9
	eors.w	r4, r4, sl
	movw	r6, #0xc1d6
	movt	r6, #0xca62
#ifdef SCE_SHA1_2014
	b.n	.Ladvance
#else
	b.w	.Ladvance
#endif
.Lchoose:
	ands.w	r4, r8, r9
	mvn.w	r5, r8
	ands.w	r5, r5, sl
	orrs	r4, r5
	movw	r6, #0x7999
	movt	r6, #0x5a82
#ifdef SCE_SHA1_2014
	b.n	.Ladvance
#else
	b.w	.Ladvance
#endif
.Lparity:
	eors.w	r4, r8, r9
	eors.w	r4, r4, sl
	movw	r6, #0xeba1
	movt	r6, #0x6ed9
#ifdef SCE_SHA1_2014
	b.n	.Ladvance
#else
	b.w	.Ladvance
#endif
.Lmajority:
	ands.w	r4, r8, r9
	ands.w	r5, r8, sl
	orrs	r4, r5
	ands.w	r5, r9, sl
	movw	r6, #0xbcdc
	movt	r6, #0x8f1b
	orrs	r4, r5
.Ladvance:
	/* e = d, d = c, c = rol30(b), b = a, a = the sum */
	adds	r4, r4, r6
	mov	fp, sl
	mov	sl, r9
	mov.w	r9, r8, ror #2
	mov	r8, r7
	adds	r7, r2, r4
	adds	r1, #1
	cmp	r1, #80
#ifdef SCE_SHA1_2014
	bne.n	.Lround
#else
	bne.w	.Lround
#endif
	/* the state plus a..e */
	ldmia.w	sp!, {r1, r2, r4, r5, r6, ip}
	adds	r7, r7, r2
	adds.w	r8, r8, r4
	adds.w	r9, r9, r5
	adds.w	sl, sl, r6
	adds.w	fp, fp, ip
	stmia.w	r1, {r7, r8, r9, sl, fp}
	ldmia.w	sp!, {r4, r5, r6, r7, r8, r9, sl, fp, pc}
	.size	Sha1Transform, . - Sha1Transform
