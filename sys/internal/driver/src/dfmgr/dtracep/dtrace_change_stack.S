/*
 * dtraceChangeStack: run a function on another stack (a probe fires on its
 * CPU's probe stack). C cannot switch the stack pointer.
 */
	.syntax	unified
	.thumb
	.text

/* SceInt64 dtraceChangeStack(void *pArg, void *pNewSp, SceInt64 (*pFunc)(void *)) */
	.align	1
	.global	dtraceChangeStack
	.thumb_func
	.type	dtraceChangeStack, %function
dtraceChangeStack:
	push	{r4, lr}
	mov	r4, sp
	mov	sp, r1
	blx	r2
	mov	sp, r4
	pop	{r4, pc}
	.size	dtraceChangeStack, . - dtraceChangeStack
