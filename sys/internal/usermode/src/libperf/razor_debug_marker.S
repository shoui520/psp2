/*
 * Razor CPU debugger discovery marker (DE AD BE EF + pointer to debug info).
 */
	.syntax unified
	.thumb
	.text
	.balign 8
	.global _razorCpuDebugMarker
	.thumb_func
	.type _razorCpuDebugMarker, %function
_razorCpuDebugMarker:
	bkpt	#0xde
	bkpt	#0xad
	bkpt	#0xbe
	bkpt	#0xef
	.word	g_razorCpuDebugInfo
	.size _razorCpuDebugMarker, . - _razorCpuDebugMarker
