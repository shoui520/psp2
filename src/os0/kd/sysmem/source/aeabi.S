/* ARM EABI arithmetic helpers. Double words use high:low register pairs. */
.syntax unified
.thumb

.macro entry name
	.balign 4
	.global \name
	.type \name, %function
	.thumb_func
\name:
.endm

.macro end name
	.balign 4
	.size \name, . - \name
.endm

/* Input r1:r0, shift r2; return r1:r0. */
entry __aeabi_llsr
	lsrs r0, r2
	adds r3, r1, #0
	lsrs r1, r2
	mov ip, r3
	subs r2, #32
	lsrs r3, r2
	orrs r0, r3
	negs r2, r2
	mov r3, ip
	lsls r3, r2
	orrs r0, r3
	bx lr
end __aeabi_llsr

entry __aeabi_lasr
	lsrs r0, r2
	adds r3, r1, #0
	asrs r1, r2
	subs r2, #32
	bmi .Lasr_low
	mov ip, r3
	asrs r3, r2
	orrs r0, r3
	mov r3, ip
.Lasr_low:
	negs r2, r2
	lsls r3, r2
	orrs r0, r3
	bx lr
end __aeabi_lasr

entry __aeabi_llsl
	lsls r1, r2
	adds r3, r0, #0
	lsls r0, r2
	mov ip, r3
	subs r2, #32
	lsls r3, r2
	orrs r1, r3
	negs r2, r2
	mov r3, ip
	lsrs r3, r2
	orrs r1, r3
	bx lr
end __aeabi_llsl

/* Inputs r1:r0 and r3:r2; return -1/0/1 in r0. */
entry __aeabi_lcmp
	cmp r1, r3
	it lt
	movlt.w r0, #-1
	it gt
	movgt r0, #1
	it ne
	bxne lr
	subs r0, r0, r2
	it cc
	movcc.w r0, #-1
	it hi
	movhi r0, #1
	bx lr
end __aeabi_lcmp

entry __aeabi_ulcmp
	cmp r1, r3
	it cc
	movcc.w r0, #-1
	it hi
	movhi r0, #1
	it ne
	bxne lr
	cmp r0, r2
	it cc
	movcc.w r0, #-1
	it hi
	movhi r0, #1
	it eq
	moveq r0, #0
	bx lr
end __aeabi_ulcmp

/* Multiply r1:r0 by r3:r2, retaining the low double word in r1:r0. */
entry __aeabi_lmul
	push {r4, r5}
	umull r4, r5, r2, r0
	mul.w r2, r1, r2
	mla r1, r3, r0, r2
	mov r0, r4
	add r1, r5
	pop {r4, r5}
	bx lr
end __aeabi_lmul

.section .note.GNU-stack,"",%progbits
