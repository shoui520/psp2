/*
 * SceKernelThreadMgr: calling a function on another stack (what C cannot
 * express). callWithStack(arg, stackTop, intrState, function): the call
 * runs on stackTop with the interrupt mask *intrState held, which comes
 * back updated; IRQ and FIQ are masked again on return.
 */
.syntax unified
.arch armv7-a
.thumb
.text
.align 1

.global callWithStack
.type callWithStack, %function
.thumb_func
callWithStack:
	push	{r4, r5, r6, lr}
	mov	r4, sp
	mov	sp, r1
	mov	r5, r2
	mrs	r1, cpsr
	bic	r1, r1, #0xc0
	ldr	r2, [r2]
	and	r2, r2, #0xc0
	orr	r1, r1, r2
	msr	cpsr_c, r1
	blx	r3
	mrs	r3, cpsr
	and	r2, r3, #0xc0
	str	r2, [r5]
	orr	r3, r3, #0xc0
	msr	cpsr_c, r3
	mov	sp, r4
	pop	{r4, r5, r6, pc}
.size callWithStack, .-callWithStack

.section .note.GNU-stack,"",%progbits
