/* Chain link words precede this handler's ARM C entry. */
	.syntax unified
	.arm
	.section .text.excpmgr_panic_undef, "ax", %progbits
	.global panicUndefHandler
	.type panicUndefHandler, %object
panicUndefHandler:
	.word 0, 0
	.size panicUndefHandler, . - panicUndefHandler
