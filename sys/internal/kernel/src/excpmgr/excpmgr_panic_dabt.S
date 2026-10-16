/* Chain link words precede this handler's ARM C entry. */
	.syntax unified
	.arm
	.section .text.excpmgr_panic_dabt, "ax", %progbits
	.global panicDabtHandler
	.type panicDabtHandler, %object
panicDabtHandler:
	.word 0, 0
	.size panicDabtHandler, . - panicDabtHandler
