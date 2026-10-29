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

/* Four native adapters operating on the wrapped HTTP handle at +0x0c. */
	.macro PafHttpHandleCall name, import
	.global \name
	.type \name, %function
	.thumb_func
\name:
	.fnstart
	push {r4, lr}
	ldr r0, [r0, #0xc]
	blx \import
	pop {r4, pc}
	.fnend
	.size \name, . - \name
	.endm

	PafHttpHandleCall ScePafMisc_12DC2F23, SceHttp_F580D304
	PafHttpHandleCall ScePafMisc_7EA133E0, SceHttp_EA61662F
	PafHttpHandleCall ScePafMisc_807E22C4, SceHttp_27071691
	PafHttpHandleCall ScePafMisc_CECA5E52, SceHttp_9CA58B99
