/*
 * SceKernelThreadMgr: what C cannot express in switchThread.
 * callSwitchThread2OnStack(stackTop, mode): switchThread2(mode) on the
 * CPU's switch stack. switchThread2 never returns: a restored context
 * goes back to where it was saved.
 */
.syntax unified
.thumb
.text
.balign 4
.global callSwitchThread2OnStack
.type callSwitchThread2OnStack, %function
.thumb_func
callSwitchThread2OnStack:
	push	{r4, lr}
	mov	r4, sp
	mov	sp, r0
	mov	r0, r1
	bl	switchThread2
	bkpt	#0
.size callSwitchThread2OnStack, .-callSwitchThread2OnStack

.section .note.GNU-stack,"",%progbits
