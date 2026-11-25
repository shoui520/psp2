/*
 * SceRazorCapture: debugger / host tool marker and breakpoint hook stubs.
 */

	.syntax unified
	.arm
	.text

	.global razorCaptureHostMarker
	.type razorCaptureHostMarker, %function
razorCaptureHostMarker:
	bkpt	#0xde
	bkpt	#0xad
	bkpt	#0xbe
	bkpt	#0xef
	.word	g_razorCaptureHostInfo
	bx	lr
	.size razorCaptureHostMarker, . - razorCaptureHostMarker

	.balign 16
	.global razorCaptureHostHookStart
	.type razorCaptureHostHookStart, %function
razorCaptureHostHookStart:
	nop
	bx	lr
	nop
	nop
	.size razorCaptureHostHookStart, . - razorCaptureHostHookStart

	.balign 16
	.global razorCaptureHostHookComplete
	.type razorCaptureHostHookComplete, %function
razorCaptureHostHookComplete:
	nop
	bx	lr
	nop
	nop
	.size razorCaptureHostHookComplete, . - razorCaptureHostHookComplete
