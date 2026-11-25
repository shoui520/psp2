/*
 * ARMv7 exclusive load/store primitives for SceSulpha lock-free structures.
 */

	.syntax unified
	.arch armv7-a
	.arm
	.text

	.global sulphaAtomicLoad32
	.type sulphaAtomicLoad32, %function
	.p2align 4
sulphaAtomicLoad32:
	ldrex	r0, [r0]
	bx	lr
	nop
	nop
	.size sulphaAtomicLoad32, .-sulphaAtomicLoad32

	.global sulphaAtomicStore32
	.type sulphaAtomicStore32, %function
	.p2align 4
sulphaAtomicStore32:
	strex	r3, r0, [r1]
	mov	r0, r3
	bx	lr
	nop
	.size sulphaAtomicStore32, .-sulphaAtomicStore32

	.global sulphaAtomicLoad64
	.type sulphaAtomicLoad64, %function
	.p2align 4
sulphaAtomicLoad64:
	mov	r3, r0
	ldrexd	r0, r1, [r3]
	bx	lr
	nop
	.size sulphaAtomicLoad64, .-sulphaAtomicLoad64

	.global sulphaAtomicStore64
	.type sulphaAtomicStore64, %function
	.p2align 4
sulphaAtomicStore64:
	strexd	r3, r0, r1, [r2]
	mov	r0, r3
	bx	lr
	.size sulphaAtomicStore64, .-sulphaAtomicStore64
