/*
 * ARMv7 exclusive load/store primitives used by SceNgsUser.
 */
	.syntax unified
	.arm

	.section .text.ngsAtomicLoad32, "ax", %progbits
	.align 4
	.global ngsAtomicLoad32
	.type ngsAtomicLoad32, %function
ngsAtomicLoad32:
	ldrex	r0, [r0]
	bx	lr
	.size ngsAtomicLoad32, . - ngsAtomicLoad32

	.section .text.ngsAtomicStore32, "ax", %progbits
	.align 4
	.global ngsAtomicStore32
	.type ngsAtomicStore32, %function
ngsAtomicStore32:
	strex	r3, r0, [r1]
	mov	r0, r3
	bx	lr
	.size ngsAtomicStore32, . - ngsAtomicStore32
