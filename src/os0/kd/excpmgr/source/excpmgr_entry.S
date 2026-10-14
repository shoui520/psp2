/* Context return requires banked registers and RFE. */
	.syntax unified
	.arch armv7-a
	.fpu neon
	.section .text.excpmgr_return, "ax", %progbits
	.arm
#define CTX_SIZE 0x400
#define CTX_SP 0x34
#define CTX_PC 0x3c
#define CTX_CPSR 0x44
#define VECTOR_TRACE_AREA 0x24
#define TRACE_CONTROL 0x100

/*
 * Return into a context. excpReturnToAbortMode: from SceExcpmgrForKernel_3E55B5C3
 * (System mode), excpResumeFrame: an initial handler's own frame.
 * r0 = context; registers are reloaded from it and RFE restores PC and CPSR.
 */
	.global	excpReturnToAbortMode
	.type	excpReturnToAbortMode, %function
excpReturnToAbortMode:
	cps	#0x17
	/* Rearm hardware trace after selecting the banked Abort return stack. */
	mrc	p15, 0, r1, c12, c0, 0
	ldr	r1, [r1, #VECTOR_TRACE_AREA]
	cmp	r1, #0
	mov	r2, #1
	strne	r2, [r1, #TRACE_CONTROL]
	b	excpResumeContext

	.global excpResumeFrame
excpResumeFrame:
	mov	r0, sp
	add	sp, sp, #CTX_SIZE

	.global	excpResumeContext
	.type	excpResumeContext, %function
excpResumeContext:
	mov	ip, r0
	ldm	ip!, {r0-r11}
	add	ip, ip, #4
	ldm	ip, {sp, lr}^
	sub	ip, ip, #CTX_SP
	ldr	lr, [ip, #CTX_CPSR]
	push	{lr}
	ldr	lr, [ip, #CTX_PC]
	push	{lr}
	ldr	ip, [ip, #48]		@ r12 last
	clrex
	rfeia	sp!
