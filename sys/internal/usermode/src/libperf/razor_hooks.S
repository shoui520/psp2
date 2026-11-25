/*
 * ARM-mode CP15 PMSELR helper and Razor CPU Target Manager breakpoint hooks.
 */
	.syntax unified
	.arm
	.text

	.balign 16
	.global _razorCpuSelectPmonCounter
	.type _razorCpuSelectPmonCounter, %function
_razorCpuSelectPmonCounter:
	mcr	p15, 0, r0, c9, c12, 5
	bx	lr
	nop
	nop
	.size _razorCpuSelectPmonCounter, . - _razorCpuSelectPmonCounter

	.balign 16
	.global _razorCpuSyncHook
	.type _razorCpuSyncHook, %function
_razorCpuSyncHook:
	nop
	bx	lr
	nop
	nop
	.size _razorCpuSyncHook, . - _razorCpuSyncHook

	.balign 16
	.global _razorCpuStopCaptureHook
	.type _razorCpuStopCaptureHook, %function
_razorCpuStopCaptureHook:
	nop
	bx	lr
	nop
	nop
	.size _razorCpuStopCaptureHook, . - _razorCpuStopCaptureHook

	.balign 16
	.global _razorCpuStartCaptureHook
	.type _razorCpuStartCaptureHook, %function
_razorCpuStartCaptureHook:
	nop
	bx	lr
	nop
	nop
	.size _razorCpuStartCaptureHook, . - _razorCpuStartCaptureHook

	.balign 16
	.global _razorCpuStopAfterFramesHook
	.type _razorCpuStopAfterFramesHook, %function
_razorCpuStopAfterFramesHook:
	nop
	bx	lr
	nop
	nop
	.size _razorCpuStopAfterFramesHook, . - _razorCpuStopAfterFramesHook
