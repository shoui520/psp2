/* Preserve the observable caller-clobbered r2 from the wait import.
 * The packed result returns status in r0 and captured scratch in r1.
 */
.syntax unified
.cpu cortex-a9
.thumb
.text
.align 2
.global I2cWaitWithScratch
.type I2cWaitWithScratch, %function
.thumb_func
I2cWaitWithScratch:
	push {r4, lr}
	sub sp, #8
	mov r3, r1
	movw r1, #0xfb0f
	movs r2, #5
	movs r4, #0
	str r4, [sp]
	bl ksceKernelWaitEventFlag
	mov r1, r2
	add sp, #8
	pop {r4, pc}
.size I2cWaitWithScratch, .-I2cWaitWithScratch
.section .note.GNU-stack,"",%progbits
