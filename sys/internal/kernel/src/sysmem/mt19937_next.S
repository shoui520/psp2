/*
 * The MT19937 step (SceKernelUtilsForDriver 92AEDFBC), Sony's hand-written
 * assembly (the file name is ours): advances one state word and returns the
 * tempered word it replaces (mt19937.c). The carry of the eors with
 * y >> 1 is y's low bit; the 0xefc60000 mask is completed by movt over the
 * index, whose low half the shift by 15 clears anyway.
 */
	.syntax unified
	.arch	armv7-a
	.thumb
	.text

	.balign	16
	.global	sceMt19937UInt
	.type	sceMt19937UInt, %function
	.thumb_func
sceMt19937UInt:
	ldmia	r0!, {r2}
	subw	r1, r2, #623
	adds	r3, r2, #1
	cbnz	r1, 1f
	movs	r3, #0
1:
	ldr.w	r1, [r0, r2, lsl #2]
	str.w	r3, [r0, #-4]
	ldr.w	r3, [r0, r3, lsl #2]
	mov	ip, r1
	bfi	ip, r3, #0, #31
	subs.w	r3, r2, #227
	bge	2f
	addw	r3, r2, #397
2:
	ldr.w	r3, [r0, r3, lsl #2]
	eors.w	r3, r3, ip, lsr #1
	eor.w	r1, r1, r1, lsr #11
	movw	ip, #0xb0df
	movt	ip, #0x9908
	it	cs
	eorcs.w	r3, r3, ip
	str.w	r3, [r0, r2, lsl #2]
	movw	ip, #0x5680
	movt	ip, #0x9d2c
	and.w	r0, ip, r1, lsl #7
	eors	r1, r0
	movt	r2, #0xefc6
	and.w	r2, r2, r1, lsl #15
	eors	r1, r2
	eor.w	r0, r1, r1, lsr #18
	bx	lr
	.size	sceMt19937UInt, . - sceMt19937UInt
