	.syntax unified
	.text

	.thumb
	.global hudHostToolMarker
	.type hudHostToolMarker, %function
	.thumb_func
	.align 2
hudHostToolMarker:
	bkpt #0xb0
	bkpt #0xba
	bkpt #0xfe
	bkpt #0x77
	.word g_hostToolInfo
	bx lr
	nop
	.size hudHostToolMarker, . - hudHostToolMarker

	.arm
	.global hudHostToolHook
	.type hudHostToolHook, %function
	.align 4
hudHostToolHook:
	nop
	bx lr
	nop
	nop
	.size hudHostToolHook, . - hudHostToolHook
