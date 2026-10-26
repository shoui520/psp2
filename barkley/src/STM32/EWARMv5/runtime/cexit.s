/*
 * __exit: the debugger told the application exited (semihosting SYS_EXIT, reason
 * ADP_Stopped_ApplicationExit), again and again.
 */
	.syntax	unified
	.thumb
	.text

	.global	__exit
	.type	__exit, %function
	.thumb_func
__exit:
	ldr	r1, =0x20026
	movs	r0, #0x18
	bkpt	0xab
	b	__exit
	.pool
	.size	__exit, . - __exit
