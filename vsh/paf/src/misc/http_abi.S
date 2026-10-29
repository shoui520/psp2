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

	.global ScePafMisc_71F59160
	.type ScePafMisc_71F59160, %function
	.thumb_func
ScePafMisc_71F59160:
	.fnstart
	push {r4, lr}
	uxtb r1, r1
	ldr r0, [r0, #0xc]
	cmp r1, #0
	mov.w r1, #0
	beq .LsetRedirect71
	movs r1, #1
.LsetRedirect71:
	blx sceHttpSetAutoRedirect
	pop {r4, pc}
	.fnend
	.size ScePafMisc_71F59160, . - ScePafMisc_71F59160

	.global ScePafMisc_D3E4B405
	.type ScePafMisc_D3E4B405, %function
	.thumb_func
ScePafMisc_D3E4B405:
	.fnstart
	push {r4, lr}
	ldr r0, [r0, #0xc]
	movs r3, #0
	blx sceHttpAddRequestHeader
	cmp r0, #0
	bge .LsuccessD3
	b .LreturnD3
.LsuccessD3:
	movs r0, #0
.LreturnD3:
	pop {r4, pc}
	.fnend
	.size ScePafMisc_D3E4B405, . - ScePafMisc_D3E4B405
