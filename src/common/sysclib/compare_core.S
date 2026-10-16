/*
 * CompareMemoryCore: Sony's hand-scheduled memcmp (memcmp in release
 * builds). Word by word when both pointers are word aligned the same way
 * and 4 bytes or more are compared, else byte by byte. A differing word is
 * byte-reversed, so the first differing byte decides; when fewer than 4
 * bytes were left, the bytes past the end are shifted out first.
 */
	.syntax unified
	.arch	armv7-a
	.thumb
	.text

	.global	CompareMemoryCore
	.type	CompareMemoryCore, %function
	.thumb_func
CompareMemoryCore:
#if !FW_DEBUG && !defined(SYSCLIB_NO_STANDARD_ALIASES)
	.global	memcmp
	.type	memcmp, %function
	.thumb_set memcmp, CompareMemoryCore
#endif
	subs	r3, r0, r1
	beq	.Lequal
	cmp	r2, #4
	it	cs
	lslcs	r0, r3, #30
	cbnz	r0, .Lbytes
	lsls	r0, r1, #30
	bne	.Lbytes
.Lwords:
	ldr	r0, [r1, r3]
	ldr	ip, [r1], #4
	cmp	r0, ip
	bne	.Lword_differs
	subs	r2, #4
	bhi	.Lwords
.Lequal:
	movs	r0, #0
	bx	lr
.Lword_differs:
	rsbs	r3, r2, #4
	rev	r1, ip
	rev	r2, r0
	bhi	.Lpartial_word
	movs	r0, #1
	subs	r2, r2, r1
	it	ls
	negls	r0, r0
	bx	lr
.Lpartial_word:
	lsls	r3, r3, #3
	lsrs	r1, r3
	lsrs	r2, r3
	subs	r0, r2, r1
	bx	lr
.Lbytes:
	subs	r2, #1
	bcc	.Lequal
.Lbyte:
	ldrb	r0, [r1, r3]
	ldrb	ip, [r1], #1
	subs	r2, #1
	it	cs
	cmpcs	r0, ip
	beq	.Lbyte
	sub	r0, r0, ip
	bx	lr
	.size	CompareMemoryCore, . - CompareMemoryCore
