/* Capture/restore the caller's registers and stack, which C cannot express. */

	.syntax unified
	.arch armv7-a
	.fpu neon
	.thumb
	.text
	.balign 4
	.global SceLibKernel_37F4ED04
	.type SceLibKernel_37F4ED04, %function
	.thumb_func
SceLibKernel_37F4ED04:
	.fnstart
	mov ip, sp
	stmia r0!, {r4-r11, ip, lr}
	vstmia r0!, {d8-d15}
	vmrs r1, fpscr
	/* Preserve condition flags and control bits, clearing exception state. */
	bfc r1, #8, #19
	vmsr fpscr, r1
	stmia r0!, {r1}
	movs r0, #0
	b .LpoisonCallerRegisters
	.fnend
	.size SceLibKernel_37F4ED04, .-SceLibKernel_37F4ED04

	.global SceLibKernel_DC277B4D
	.type SceLibKernel_DC277B4D, %function
	.thumb_func
SceLibKernel_DC277B4D:
	.fnstart
	.cantunwind
	ldmia r0!, {r4-r11, ip, lr}
	vldmia r0!, {d8-d15}
	ldmia r0!, {r2}
	vmsr fpscr, r2
	mov sp, ip
	movs r0, r1
	it eq
	moveq r0, #1

.LpoisonCallerRegisters:
	/* Native diagnostic values: deadbeef and NaN 7ff8dead7f80dead. */
	movw r3, #0xbeef
	movt r3, #0xdead
	lsrs r1, r3, #16
	movt r1, #0x7f80
	add r2, r1, #0x780000
	vmov d0, r1, r2
	vorr d1, d0, d0
	vorr q1, q0, q0
	vorr q2, q0, q0
	vorr q3, q0, q0
	vorr q8, q0, q0
	vorr q9, q0, q0
	vorr q10, q0, q0
	vorr q11, q0, q0
	vorr q12, q0, q0
	vorr q13, q0, q0
	vorr q14, q0, q0
	vorr q15, q0, q0
	mov r2, r3
	mov r1, r3
	mov ip, r3
	bx lr
	.fnend
	.size SceLibKernel_DC277B4D, .-SceLibKernel_DC277B4D
	.section .note.GNU-stack,"",%progbits
