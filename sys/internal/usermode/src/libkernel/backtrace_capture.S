/* Capture the ARM wrapper frame before C can spill or replace its registers. */
	.syntax unified
	.arch armv7-a
	.arm
	.text
	.balign 4
	.global BacktraceCapture
	.type BacktraceCapture, %function
BacktraceCapture:
	.fnstart
	.save {r4, fp, lr}
	push {r4, fp, lr}
	.setfp fp, sp, #8
	add fp, sp, #8
	sub sp, sp, #64
	stmia sp, {r0-r12, sp, lr}
	ldr r4, .LcaptureEntry
	/* The last register word is the PC of this wrapper, not the caller. */
	str r4, [fp, #-12]
	mov r4, sp
	sub sp, sp, #4
	push {r4}
	ldr r4, [fp, #4]
	push {r4}
	/* Publish the SP used for BacktraceWalk, including its stack arguments. */
	str sp, [fp, #-20]
	blx BacktraceWalk
	sub sp, fp, #8
	pop {r4, fp, lr}
	bx lr
	.fnend
	.size BacktraceCapture, .-BacktraceCapture
.LcaptureEntry:
	.word BacktraceCapture
	.section .note.GNU-stack,"",%progbits
