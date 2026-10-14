/*
 * SmcCall(r0, r1, r2, r3, command): secure monitor call, command in r12.
 * name unknown
 */

	.syntax	unified
	.arch	armv7-a
	.arch_extension sec
	.arm
	.text

	.global	SmcCall
	.type	SmcCall, %function
SmcCall:
	ldr	ip, [sp]
	smc	#0
	bx	lr
	.size	SmcCall, . - SmcCall
