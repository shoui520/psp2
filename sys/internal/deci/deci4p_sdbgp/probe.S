/*
 * Fault-tolerant memory probe loads and stores: SceExcpmgr catches any data
 * abort between sdbgpProbeStart and sdbgpProbeEnd, sets r0 to the error code,
 * and advances PC by four bytes onto the trailing bx lr.
 */
	.syntax	unified
	.arch	armv7-a
	.text
	.thumb

	.macro	FUNCTION name
	.global	\name
	.thumb_func
	.type	\name, %function
\name:
	.endm

	.macro	END_FUNCTION name
	.size	\name, . - \name
	.endm

	.balign	4
	.global	sdbgpProbeStart
sdbgpProbeStart:

FUNCTION sceKernelVMRead8
	ldrb	r1, [r1]
	b.n	sceKernelVMWrite8
	bx	lr
END_FUNCTION sceKernelVMRead8

FUNCTION sceKernelVMRead16
	ldrh	r1, [r1]
	b.n	sceKernelVMWrite16
	bx	lr
END_FUNCTION sceKernelVMRead16

FUNCTION sceKernelVMRead32
	ldr	r1, [r1]
	b.n	sceKernelVMWrite32
	bx	lr
END_FUNCTION sceKernelVMRead32

FUNCTION sceKernelVMWrite8
	strb	r1, [r0]
	movs	r0, #0
	bx	lr
END_FUNCTION sceKernelVMWrite8

FUNCTION sceKernelVMWrite16
	strh	r1, [r0]
	movs	r0, #0
	bx	lr
END_FUNCTION sceKernelVMWrite16

FUNCTION sceKernelVMWrite32
	str	r1, [r0]
	movs	r0, #0
	bx	lr
END_FUNCTION sceKernelVMWrite32

	.global	sdbgpProbeEnd
sdbgpProbeEnd:

	.section .note.GNU-stack, "", %progbits
