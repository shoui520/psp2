/* Secure monitor services used by ScePervasive. */
.syntax unified
.thumb
.arch armv7-a
.arch_extension sec
.text

.macro SERVICE name, number
	.global \name
	.type \name, %function
	.thumb_func
\name:
	movw ip, #\number
	smc #0
	bx lr
	.size \name, .-\name
.endm

SERVICE Smc117, 0x117
SERVICE Smc110, 0x110
SERVICE Smc111, 0x111
SERVICE Smc113, 0x113
SERVICE Smc114, 0x114
SERVICE Smc11B, 0x11b
SERVICE Smc11C, 0x11c
SERVICE Smc11D, 0x11d
SERVICE Smc11E, 0x11e
SERVICE Smc11F, 0x11f
SERVICE Smc121, 0x121
SERVICE SceCdramForDriver_2D728EBF, 0x118
SERVICE SceCdramForDriver_0657FC1B, 0x119
/* Preserve the original worker's incoming/clobbered r2/r3 SMC arguments. */
.global CdramInitWorker
.type CdramInitWorker, %function
.thumb_func
CdramInitWorker:
	push {r4, r5, r6, lr}
	movs r1, #0x10
	movs r0, #0
	movs r4, #1
	blx ksceKernelChangeThreadPriority
	movs r1, #6
	bics.w r5, r0, r0, asr #32
	it cs
	movcs r5, #0xbf
	movs r0, #0
	blx ksceKernelChangeThreadCpuAffinityMask
	bics.w r6, r0, r0, asr #32
	it cs
	movcs r6, #0xf
	movs r0, #0
	bl Smc117
	movw r3, #:lower16:s_cdramReady
	mov r1, r6
	movt r3, #:upper16:s_cdramReady
	movs r0, #0
	str r4, [r3]
	blx ksceKernelChangeThreadCpuAffinityMask
	mov r1, r5
	movs r0, #0
	blx ksceKernelChangeThreadPriority
	mov r0, r4
	pop {r4, r5, r6, pc}
.size CdramInitWorker, .-CdramInitWorker
.section .note.GNU-stack,"",%progbits
