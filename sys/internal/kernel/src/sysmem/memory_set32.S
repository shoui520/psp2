/*
 * SetMemory32Core: the word fill SceSysmem uses to clear and poison its
 * objects. Sony's hand-scheduled fill engine (SetMemoryCore, fill_core.S)
 * with the 32-bit pattern spread over q0 in place of a byte. Sony's name is
 * unknown.
 */
	.syntax unified
	.arch	armv7-a
	.fpu	neon
	.thumb
	.text

	.balign	16
	.global	SetMemory32Core
	.type	SetMemory32Core, %function
	.thumb_func
SetMemory32Core:
	vdup.32	q0, r1
	subs	r2, #16
	push	{r0, lr}
	vmov	ip, r1, d0
	bcc	.Lsmall
	add	r2, r0
	vst1.8	{d0-d1}, [r0]!
	beq	.Ldone
	bic	r0, r0, #15
	subs	r2, r2, r0
	bls	.Llast
	vmov	lr, r3, d0
.Lblocks:
	subs	r2, #16
	stmia	r0!, {r1, r3, ip, lr}
	bhi	.Lblocks
.Llast:
	adds	r0, r0, r2
	vst1.8	{d0-d1}, [r0]
.Ldone:
	pop	{r0, pc}
.Lsmall:
	lsls	r3, r2, #29
	bcc	.Lwords
	vst1.8	{d0}, [r0]!
	adds	r3, r0, r2
	vst1.8	{d1}, [r3]
	pop	{r0, pc}
.Lwords:
	it	mi
	strmi	r1, [r0], #4
	lsls	r3, r2, #31
	it	cs
	strhcs	ip, [r0], #2
	it	mi
	strbmi	r1, [r0]
	pop	{r0, pc}
	.balign	16
	.size	SetMemory32Core, . - SetMemory32Core
