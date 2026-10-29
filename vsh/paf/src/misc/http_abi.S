/* Native object-owned SceHttp request-control adapters. */
	.syntax unified
	.thumb
	.text

	.macro PafHttpRequestCall name, import
	.global \name
	.type \name, %function
	.thumb_func
\name:
	.fnstart
	push {r4, lr}
	ldr r0, [r0]
	ldr r0, [r0, #0x4c]
	blx \import
	pop {r4, pc}
	.fnend
	.size \name, . - \name
	.endm

	PafHttpRequestCall ScePafMisc_23F9BC0A, SceHttp_43A7CC70
	PafHttpRequestCall ScePafMisc_AA992DF5, SceHttp_FD76E1FB
