/* Excpmgr descriptors: saved predecessor pointer and genuine ARM trampoline. */
/* Three macro expansions retain 21 live-register/interworking instructions. */
.syntax unified
.arm
/* Excpmgr patches link words through its kernel-text DACR write helper. */
.section .text.bsod_entries, "ax", %progbits

.macro BsodExceptionEntry name, handler
.balign 4
.global \name
.type \name, %function
\name:
	.word 0
	.word 0
	push {r0, lr}
	blx \handler
	pop {r0, lr}
	ldr r2, =\name
	ldr r2, [r2]
	add r2, r2, #8
	mov pc, r2
.size \name, . - \name
.endm

BsodExceptionEntry bsodPrefetchEntry, _sceKernelBsodPrefetchAbort
BsodExceptionEntry bsodDataEntry, _sceKernelBsodDataAbort
BsodExceptionEntry bsodUndefinedEntry, _sceKernelBsodUndfinedAbort
.ltorg
