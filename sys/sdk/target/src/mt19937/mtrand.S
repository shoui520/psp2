/*
 * libSceMt19937's step (mtrand.o, section .text.sceMt19937), Sony's hand-written assembly: the
 * kernel's (sysmem/mt19937_next.S) with a NULL context refused. It advances one state word and
 * returns the tempered word it replaces. The carry of the eors with y >> 1 is y's low bit; the
 * 0xefc60000 mask is completed by movt over the index, whose low half the shift by 15 clears.
 */
/* the 3.10 SDK's revision (SceLibMarlin links it, SDK 3.100.010); 3.570.011's otherwise */
#if defined(SCE_PSP2_SDK_VERSION) && SCE_PSP2_SDK_VERSION < 0x03570000
#define MT19937_SDK_310	1
#else
#define MT19937_SDK_310	0
#endif

	.syntax unified
	.arch	armv7-a
	.thumb

	.section .text.sceMt19937, "ax", %progbits
	.balign	2
	.global	sceMt19937UInt
	.type	sceMt19937UInt, %function
	.thumb_func
sceMt19937UInt:
	.fnstart
	cbz	r0, 3f
#if MT19937_SDK_310
	ldr.w	r2, [r0], #4
#else
	ldmia	r0!, {r2}
#endif
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
#if MT19937_SDK_310
	/* the 3.10 SDK's: the mask built whole in r2 */
	and.w	r2, ip, r1, lsl #7
	eors	r1, r2
	movs	r2, #0
	movt	r2, #0xefc6
#else
	and.w	r0, ip, r1, lsl #7
	eors	r1, r0
	movt	r2, #0xefc6
#endif
	and.w	r2, r2, r1, lsl #15
	eors	r1, r2
	eor.w	r0, r1, r1, lsr #18
	bx	lr
3:
	movw	r0, #0x0100
	movt	r0, #0x8049
	bx	lr
	.fnend
	.size	sceMt19937UInt, . - sceMt19937UInt
