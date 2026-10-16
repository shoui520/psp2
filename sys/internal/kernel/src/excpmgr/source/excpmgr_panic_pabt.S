/* Chain link words precede this handler's ARM C entry. */
	.syntax unified
	.arm
	.section .text.excpmgr_panic_pabt, "ax", %progbits
	.global panicPabtHandler
	.type panicPabtHandler, %object
panicPabtHandler:
	.word 0, 0
	.size panicPabtHandler, . - panicPabtHandler
