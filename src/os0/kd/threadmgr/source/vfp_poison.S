/* Boot-time VFP/NEON register poison precedes the stack-switch boundaries. */
.syntax unified
.arch armv7-a
.fpu neon
.thumb
.text
.balign 4
vfpPoison:
	.word	0x7f80dead, 0x7ff8dead
.global poisonVfpRegisters
.type poisonVfpRegisters, %function
.thumb_func
poisonVfpRegisters:
	vldr	d0, vfpPoison
	vmov	d1, d0
	vmov	q2, q0
	vmov	q3, q0
	vmov	q4, q0
	vmov	q5, q0
	vmov	q6, q0
	vmov	q7, q0
	vmov	q8, q0
	vmov	q9, q0
	vmov	q10, q0
	vmov	q11, q0
	vmov	q12, q0
	vmov	q13, q0
	vmov	q14, q0
	vmov	q15, q0
	bx	lr
.size poisonVfpRegisters, .-poisonVfpRegisters
.section .note.GNU-stack,"",%progbits

/* These boundaries share Sony's text fragment; separate objects add padding. */
#include "kernel_stack.S"
#include "context_runtime.S"
#include "return_entries.S"
