/* NEON-agnostic ARM exclusive reservation loop; not expressible by this C toolchain. */
	.syntax unified
	.thumb
	.text

	.global PafAtomicIncrementStrong
	.type PafAtomicIncrementStrong, %function
	.thumb_func
PafAtomicIncrementStrong:
	.fnstart
	dmb sy
.Lretry:
	ldrex r1, [r0]
	adds r1, #1
	strex r2, r1, [r0]
	cmp r2, #0
	bne .Lretry
	dmb sy
	bx lr
	.fnend
	.size PafAtomicIncrementStrong, . - PafAtomicIncrementStrong
