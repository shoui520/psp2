/*
 * cortexm3_macro: ST's helpers for the Cortex-M3 instructions C cannot express, the two
 * the image links: the interrupts enabled (PRIMASK cleared) and disabled.
 */
	.syntax	unified
	.thumb
	.text

	.global	__RESETPRIMASK
	.type	__RESETPRIMASK, %function
	.thumb_func
__RESETPRIMASK:
	cpsie	i
	bx	lr
	.size	__RESETPRIMASK, . - __RESETPRIMASK

	.global	__SETPRIMASK
	.type	__SETPRIMASK, %function
	.thumb_func
__SETPRIMASK:
	cpsid	i
	bx	lr
	.size	__SETPRIMASK, . - __SETPRIMASK
