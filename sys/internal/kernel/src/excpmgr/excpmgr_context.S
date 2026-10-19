/*
 * Return into a saved context from System mode, reload the debug registers
 * of the resumed code, and report a return from a nested exception.
 */
	.syntax	unified
	.arch	armv7-a
	.fpu	neon
	.section .text.excpmgr_context, "ax", %progbits
	.arm
#include "excpmgr_asm.h"

/*
 * SceExcpmgrForKernel_3E55B5C3 (context): return into a context from System
 * mode. The pending fault status is cleared, the nesting level dropped, VFP
 * state restored when the context has any, and RFE resumes the context from
 * the Abort-mode stack.
 */
	FUNCTION SceExcpmgrForKernel_3E55B5C3
	mov	r1, #0
	mcr	p15, 0, r1, c5, c0, 0		@ DFSR
	mcr	p15, 0, r1, c5, c0, 1		@ IFSR
	mcr	p15, 0, r1, c5, c1, 0		@ ADFSR
	mcr	p15, 0, r1, c5, c1, 1		@ AIFSR
	mcr	p15, 0, r1, c6, c0, 0		@ DFAR
	mcr	p15, 0, r1, c6, c0, 2		@ IFAR
	ldr	r1, [r0, #CTX_TPIDRURO]
	mcr	p15, 0, r1, c13, c0, 3
	ldr	r1, [r0, #CTX_TTBR1]
	ldr	r2, [r0, #CTX_CONTEXT_ID]
	mov	r3, #0
	mcr	p15, 0, r3, c13, c0, 1
	isb
	mcr	p15, 0, r1, c2, c0, 1
	isb
	mcr	p15, 0, r2, c13, c0, 1
	ldr	r1, [r0, #CTX_DACR]
	mcr	p15, 0, r1, c3, c0, 0
	ldr	r1, [r0, #CTX_PAR]
	mcr	p15, 0, r1, c7, c4, 0
	NEST_COUNT_ADDRESS
	ldr	r3, [r2]
	subs	r3, r3, #1
	bne	excpPanicNested
	str	r3, [r2]
	ldr	r1, [r0, #CTX_EXCP_CODE]
	cmp	r1, #0
	beq	1f				@ no VFP state
	ldr	r2, [r0, #CTX_FPEXC]
	ldr	r3, [r0, #CTX_FPSCR]
	vmsr	fpexc, r2
	vmsr	fpscr, r3
	adds	r2, r0, #CTX_VFP
	vldmia	r2!, {d0-d15}
	vldmia	r2!, {d16-d31}
1:	ldr	r1, [r0, #CTX_CPACR]
	mcr	p15, 0, r1, c1, c0, 2
	bl	excpRestoreDebugRegisters
	cps	#0x17				@ Abort mode: its stack takes the RFE frame
	REARM_TRACE
	RETURN_FROM_CONTEXT
	END_FUNCTION SceExcpmgrForKernel_3E55B5C3

/*
 * Reload the debug registers of the resumed code: the hardware breakpoints
 * (DBGBVR/DBGBCR) of its process, or the kernel's when it has no thread, then
 * the watchpoints (DBGWVR/DBGWCR), then the thread's own breakpoint 5, each
 * set under the lock that starts it. As in Sony's code the watchpoint values
 * are read from context + 8, not from the watchpoint set.
 */
	FUNCTION excpRestoreDebugRegisters
	ldr	r4, [r0, #CTX_THREAD]
	cmp	r4, #0
	movweq	r5, #:lower16:g_excpData + DATA_KERNEL_BREAKPOINTS
	movteq	r5, #:upper16:g_excpData + DATA_KERNEL_BREAKPOINTS
	ldreq	r6, [r5]
	ldrne	r5, [r4, #THREAD_PROCESS]
	ldrne	r6, [r5, #PROCESS_HW_BREAKPOINTS]
	cmp	r6, #0
	beq	2f
	push	{r0-r3, ip, lr}
	mov	r0, r6
	bl	sceKernelSpinlockLowLock
	pop	{r0-r3, ip, lr}
	mov	r9, r6
	add	r6, r6, #HW_REGISTERS
	.irp	index, 0, 1, 2, 3, 4
	ldm	r6!, {r7, r8}
	mcr	p14, 0, r7, c0, c\index, 4		@ DBGBVR
	mcr	p14, 0, r8, c0, c\index, 5		@ DBGBCR
	.endr
	push	{r0-r3, ip, lr}
	mov	r0, r9
	bl	sceKernelSpinlockLowUnlock
	pop	{r0-r3, ip, lr}
	cmp	r4, #0
	movweq	r5, #:lower16:g_excpData + DATA_KERNEL_WATCHPOINTS
	movteq	r5, #:upper16:g_excpData + DATA_KERNEL_WATCHPOINTS
	ldreq	r6, [r5]
	ldrne	r5, [r4, #THREAD_PROCESS]
	ldrne	r6, [r5, #PROCESS_HW_BREAKPOINTS]
	cmp	r6, #0
	beq	2f
	push	{r0-r3, ip, lr}
	mov	r0, r6
	bl	sceKernelSpinlockLowLock
	pop	{r0-r3, ip, lr}
	mov	r9, r6
	add	r6, r0, #HW_REGISTERS
	.irp	index, 0, 1, 2, 3
	ldm	r6!, {r7, r8}
	mcr	p14, 0, r7, c0, c\index, 6		@ DBGWVR
	mcr	p14, 0, r8, c0, c\index, 7		@ DBGWCR
	.endr
	push	{r0-r3, ip, lr}
	mov	r0, r9
	bl	sceKernelSpinlockLowUnlock
	pop	{r0-r3, ip, lr}
	cmp	r4, #0
	beq	2f
	ldr	r6, [r4, #THREAD_THBP]
	mcr	p14, 0, r6, c0, c5, 4			@ DBGBVR5
	ldr	r6, [r4, #THREAD_THBP + 4]
	mcr	p14, 0, r6, c0, c5, 5			@ DBGBCR5
2:	bx	lr
	END_FUNCTION excpRestoreDebugRegisters

/* Report an exception that returns while another one is still pending. */
	FUNCTION excpPanicNested
	push	{r0-r3, ip, lr}
	blx	sceKernelPanicHandlerReturnFromNestedExceptionCfunc
	pop	{r0-r3, ip, lr}
	b	excpPanicNested
	END_FUNCTION excpPanicNested
