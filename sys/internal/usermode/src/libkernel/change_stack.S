/* Stack replacement must preserve the old stack independently of C spills. */

	.syntax unified
	.arch armv7-a
	.thumb
	.text
	.equ kernelStorageOffset, 0x800
	.equ threadStackBoundsOffset, 8
	.balign 4
	.global sceKernelCallWithChangeStack
	.type sceKernelCallWithChangeStack, %function
	.thumb_func
/* (stackBottom, stackSize, callback, argument); return the callback result. */
sceKernelCallWithChangeStack:
	.fnstart
	.save {r4-r8, lr}
	push {r4-r8, lr}
	.setfp r8, sp, #0
	mov r8, sp
	/* Probe both ends before publishing the new stack bounds. */
	ldr ip, [r0]
	add r4, r0, r1
	ldr ip, [r4, #-4]
	mov r1, r0
	mov r0, r4
	mrc p15, 0, ip, c13, c0, 3
	sub ip, ip, #kernelStorageOffset
	ldrd r4, r5, [ip, #threadStackBoundsOffset]
	/* Invalidate the old bounds while SP and the published pair differ. */
	movw r6, #0xf000
	movt r6, #0xffff
	mov r7, #0
	strd r6, r7, [ip, #threadStackBoundsOffset]
	mov sp, r0
	strd r0, r1, [ip, #threadStackBoundsOffset]
	mov ip, r2
	mov r0, r3
	movw r1, #0xbeef
	movt r1, #0xdead
	mov r2, r1
	mov r3, r1
	blx ip
	mrc p15, 0, ip, c13, c0, 3
	sub ip, ip, #kernelStorageOffset
	strd r6, r7, [ip, #threadStackBoundsOffset]
	mov sp, r8
	strd r4, r5, [ip, #threadStackBoundsOffset]
	movw ip, #0xbeef
	movt ip, #0xdead
	mov r1, ip
	mov r2, ip
	mov r3, ip
	pop {r4-r8, pc}
	.fnend
	.size sceKernelCallWithChangeStack, .-sceKernelCallWithChangeStack
	.section .note.GNU-stack,"",%progbits
