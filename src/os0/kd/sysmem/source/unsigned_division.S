/* ARM EABI unsigned division, including the default zero-divisor path. */
.syntax unified
.thumb
.text
.balign 4

.global __aeabi_uidiv
.type __aeabi_uidiv, %function
.thumb_func
__aeabi_uidiv:
	subs r2, r1, #1
	it eq
	bxeq lr
	bcc.w .Lunsigned_divide_zero
	cmp r0, r1
	bls.w .Lunsigned_divide_small
	tst r1, r2
	beq.w .Lunsigned_divide_power_of_two
	clz r3, r0
	clz r2, r1
	sub.w r3, r2, r3
	rsb r3, r3, #31
	adr r2, .Lunsigned_divide_steps
	add.w r3, r2, r3, lsl #4
	mov.w r2, #0
	mov pc, r3
	nop
	nop.w

/* The computed entry selects a fixed 16-byte step from leading-zero counts. */
.macro unsignedDivideStep shift
	cmp.w r0, r1, lsl #\shift
	nop
	adc.w r2, r2, r2
	it cs
	subcs.w r0, r0, r1, lsl #\shift
.endm

.Lunsigned_divide_steps:
.irp shift,31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0
	unsignedDivideStep \shift
.endr
	mov r0, r2
	bx lr
.Lunsigned_divide_small:
	ite eq
	moveq r0, #1
	movne r0, #0
	bx lr
.Lunsigned_divide_power_of_two:
	clz r2, r1
	rsb r2, r2, #31
	lsr.w r0, r0, r2
	bx lr
.Lunsigned_divide_zero:
	cbz r0, .Lunsigned_divide_zero_dispatch
	mov.w r0, #-1
.Lunsigned_divide_zero_dispatch:
	push {r0, r1, r2}
	ldr r0, .Lunsigned_divide_zero_target
	adr r1, .Lunsigned_divide_zero_target
	add r0, r1
	str r0, [sp, #8]
	pop {r0, r1, pc}
.Lunsigned_divide_zero_target:
	.word HandleDivisionByZero - .Lunsigned_divide_zero_target
.size __aeabi_uidiv, . - __aeabi_uidiv

/* Inputs r0/r1; quotient r0, remainder r1. */
.global __aeabi_uidivmod
.type __aeabi_uidivmod, %function
.thumb_func
__aeabi_uidivmod:
	cmp r1, #0
	beq .Lunsigned_divide_zero
	push.w {r0, r1, lr}
	bl __aeabi_uidiv
	pop.w {r1, r2, lr}
	mul.w r3, r2, r0
	sub.w r1, r1, r3
	bx lr
	nop
	nop.w
.size __aeabi_uidivmod, . - __aeabi_uidivmod

.section .note.GNU-stack,"",%progbits

/* Unsigned 64-bit quotient/remainder in r0:r1 and r2:r3, respectively. */
.text
.balign 4
.global __aeabi_uldivmod
.type __aeabi_uldivmod, %function
.thumb_func
__aeabi_uldivmod:
	cbnz r3, .Lunsigned64_nonzero
	cbnz r2, .Lunsigned64_nonzero
	cmp r1, #0
	it eq
	cmpeq r0, #0
	beq.n .Lunsigned64_zero_dispatch
	mov.w r1, #-1
	mov r0, r1
.Lunsigned64_zero_dispatch:
	push {r0, r1, r2}
	ldr r0, .Lunsigned64_zero_target
	adr r1, .Lunsigned64_zero_target
	add r0, r1
	str r0, [sp, #8]
	pop {r0, r1, pc}
	nop
.Lunsigned64_zero_target:
	.word HandleDivisionByZero - .Lunsigned64_zero_target
.Lunsigned64_nonzero:
	sub sp, #8
	mov ip, sp
	stmdb sp!, {ip, lr}
	bl UnsignedDivision64WithRemainder
	ldr.w lr, [sp, #4]
	add sp, #8
	pop {r2, r3}
	bx lr
	nop
.size __aeabi_uldivmod, . - __aeabi_uldivmod

/* Private helper names describe proved roles; Sony spellings are unknown. */
.type UnsignedDivision64WithRemainder, %function
.thumb_func
UnsignedDivision64WithRemainder:
	stmdb sp!, {r3, r4, r5, r6, r7, r8, r9, lr}
	mov r7, r2
	mov r8, r0
	mov r9, r1
	mov r5, r3
	ldr r6, [sp, #32]
	bl UnsignedDivision64Quotient
	mul.w r3, r0, r5
	umull r4, r5, r0, r7
	mla r7, r7, r1, r3
	subs.w r4, r8, r4
	add r5, r7
	sbc.w r5, r9, r5
	strd r4, r5, [r6]
	ldmia.w sp!, {r3, r4, r5, r6, r7, r8, r9, pc}
.size UnsignedDivision64WithRemainder, . - UnsignedDivision64WithRemainder

/* Base-2^16 quotient estimates and native correction branches. */
.type UnsignedDivision64Quotient, %function
.thumb_func
UnsignedDivision64Quotient:
	stmdb sp!, {r4, r5, r6, r7, r8, r9, sl, fp, lr}
	mov r7, r0
	sub sp, #12
	mov r6, r1
	mov r5, r2
	mov r8, r0
	mov r4, r1
	cmp r3, #0
	bne.n .Lunsigned64_quotient_0b6
	cmp r2, r1
	bls.n .Lunsigned64_quotient_0e2
	clz r3, r2
	cbz r3, .Lunsigned64_quotient_032
	rsb r4, r3, #32
	lsls r6, r3
	lsl.w r5, r2, r3
	lsl.w r8, r0, r3
	lsrs r7, r4
	orr.w r4, r7, r6
.Lunsigned64_quotient_032:
	lsrs r6, r5, #16
	mov r0, r4
	uxth.w r9, r5
	mov r1, r6
	bl __aeabi_uidiv
	mov r1, r6
	mov r7, r0
	mov r0, r4
	bl __aeabi_uidivmod
	mul.w r0, r9, r7
	mov.w r3, r8, lsr #16
	orr.w r1, r3, r1, lsl #16
	cmp r0, r1
	bls.n .Lunsigned64_quotient_06e
	adds r1, r1, r5
	add.w r2, r7, #4294967295
	bcs.n .Lunsigned64_quotient_06c
	cmp r0, r1
	itt hi
	subhi r7, #2
	addhi r1, r1, r5
	bhi.n .Lunsigned64_quotient_06e
.Lunsigned64_quotient_06c:
	mov r7, r2
.Lunsigned64_quotient_06e:
	sub.w sl, r1, r0
	mov r1, r6
	uxth.w r8, r8
	mov r0, sl
	bl __aeabi_uidiv
	mov r1, r6
	mov r4, r0
	mov r0, sl
	bl __aeabi_uidivmod
	mul.w r9, r9, r4
	orr.w r1, r8, r1, lsl #16
	cmp r9, r1
	bls.n .Lunsigned64_quotient_0a8
	adds r5, r1, r5
	add.w r3, r4, #4294967295
	bcs.w .Lunsigned64_quotient_2e2
	cmp r9, r5
	it hi
	subhi r4, #2
	bls.w .Lunsigned64_quotient_2e2
.Lunsigned64_quotient_0a8:
	orr.w r0, r4, r7, lsl #16
	movs r7, #0
.Lunsigned64_quotient_0ae:
	mov r1, r7
	add sp, #12
	ldmia.w sp!, {r4, r5, r6, r7, r8, r9, sl, fp, pc}
.Lunsigned64_quotient_0b6:
	cmp r3, r1
	itt hi
	movhi r7, #0
	movhi r0, r7
	bhi.n .Lunsigned64_quotient_0ae
	clz r8, r3
	cmp.w r8, #0
	bne.n .Lunsigned64_quotient_182
	cmp r3, r1
	it cs
	cmpcs r2, r7
	ite hi
	movhi r7, #0
	movls r7, #1
	itt ls
	movls r0, #1
	movls r7, r8
	bls.n .Lunsigned64_quotient_0ae
	mov r0, r7
	b.n .Lunsigned64_quotient_0ae
.Lunsigned64_quotient_0e2:
	cbnz r2, .Lunsigned64_quotient_0ee
	mov r1, r2
	movs r0, #1
	bl __aeabi_uidiv
	mov r5, r0
.Lunsigned64_quotient_0ee:
	clz r3, r5
	cmp r3, #0
	bne.w .Lunsigned64_quotient_250
	subs r6, r6, r5
	lsrs r4, r5, #16
	uxth.w sl, r5
	movs r7, #1
.Lunsigned64_quotient_102:
	mov r1, r4
	mov r0, r6
	bl __aeabi_uidiv
	mov r1, r4
	mov r9, r0
	mov r0, r6
	bl __aeabi_uidivmod
	mul.w r0, sl, r9
	mov.w r3, r8, lsr #16
	orr.w r1, r3, r1, lsl #16
	cmp r0, r1
	bls.n .Lunsigned64_quotient_13c
	adds r1, r1, r5
	add.w r2, r9, #4294967295
	bcs.w .Lunsigned64_quotient_2e6
	cmp r0, r1
	itt hi
	subhi.w r9, r9, #2
	addhi r1, r1, r5
	bls.w .Lunsigned64_quotient_2e6
.Lunsigned64_quotient_13c:
	sub.w fp, r1, r0
	mov r1, r4
	uxth.w r8, r8
	mov r0, fp
	bl __aeabi_uidiv
	mov r1, r4
	mov r6, r0
	mov r0, fp
	bl __aeabi_uidivmod
	mul.w sl, sl, r6
	orr.w r1, r8, r1, lsl #16
	cmp sl, r1
	bls.n .Lunsigned64_quotient_176
	adds r5, r1, r5
	add.w r3, r6, #4294967295
	bcs.w .Lunsigned64_quotient_2ea
	cmp sl, r5
	it hi
	subhi r6, #2
	bls.w .Lunsigned64_quotient_2ea
.Lunsigned64_quotient_176:
	orr.w r0, r6, r9, lsl #16
	mov r1, r7
	add sp, #12
	ldmia.w sp!, {r4, r5, r6, r7, r8, r9, sl, fp, pc}
.Lunsigned64_quotient_182:
	rsb r5, r8, #32
	lsl.w r3, r3, r8
	lsl.w r0, r2, r8
	lsl.w r1, r1, r8
	lsrs r2, r5
	lsr.w ip, r6, r5
	orr.w r9, r2, r3
	lsr.w fp, r7, r5
	str r0, [sp, #4]
	orr.w fp, fp, r1
	mov.w sl, r9, lsr #16
	mov r0, ip
	str.w ip, [sp]
	uxth.w r6, r9
	mov r1, sl
	mov.w r5, fp, lsr #16
	bl __aeabi_uidiv
	ldr.w ip, [sp]
	mov r1, sl
	mov r4, r0
	mov r0, ip
	bl __aeabi_uidivmod
	mul.w r0, r6, r4
	orr.w r1, r5, r1, lsl #16
	cmp r0, r1
	bls.n .Lunsigned64_quotient_1e6
	adds.w r1, r1, r9
	add.w r2, r4, #4294967295
	bcc.w .Lunsigned64_quotient_2fa
.Lunsigned64_quotient_1e4:
	mov r4, r2
.Lunsigned64_quotient_1e6:
	subs r2, r1, r0
	mov r1, sl
	uxth.w fp, fp
	mov r0, r2
	str r2, [sp, #0]
	bl __aeabi_uidiv
	ldr r2, [sp, #0]
	mov r1, sl
	mov r5, r0
	mov r0, r2
	bl __aeabi_uidivmod
	mul.w r6, r5, r6
	orr.w r1, fp, r1, lsl #16
	cmp r6, r1
	bls.n .Lunsigned64_quotient_21a
	adds.w r1, r1, r9
	add.w r2, r5, #4294967295
	bcc.n .Lunsigned64_quotient_2ee
.Lunsigned64_quotient_218:
	mov r5, r2
.Lunsigned64_quotient_21a:
	ldr r3, [sp, #4]
	orr.w r0, r5, r4, lsl #16
	subs r6, r1, r6
	umull r4, r5, r0, r3
	cmp r6, r5
	bcc.n .Lunsigned64_quotient_244
	lsl.w r7, r7, r8
	ite ne
	movne r1, #0
	moveq r1, #1
	cmp r7, r4
	ite cs
	movcs r7, #0
	andcc.w r7, r1, #1
	cmp r7, #0
	beq.w .Lunsigned64_quotient_0ae
.Lunsigned64_quotient_244:
	movs r7, #0
	subs r0, #1
	mov r1, r7
	add sp, #12
	ldmia.w sp!, {r4, r5, r6, r7, r8, r9, sl, fp, pc}
.Lunsigned64_quotient_250:
	lsls r5, r3
	rsb r2, r3, #32
	lsl.w lr, r6, r3
	lsl.w r8, r7, r3
	lsrs r6, r2
	lsrs r4, r5, #16
	lsrs r7, r2
	uxth.w sl, r5
	mov r1, r4
	mov r0, r6
	orr.w fp, r7, lr
	bl __aeabi_uidiv
	mov r1, r4
	mov r7, r0
	mov r0, r6
	bl __aeabi_uidivmod
	mul.w r0, sl, r7
	mov.w r3, fp, lsr #16
	orr.w r1, r3, r1, lsl #16
	cmp r0, r1
	bls.n .Lunsigned64_quotient_2a0
	adds r1, r1, r5
	add.w r3, r7, #4294967295
	bcs.n .Lunsigned64_quotient_308
	cmp r0, r1
	itt hi
	subhi r7, #2
	addhi r1, r1, r5
	bls.n .Lunsigned64_quotient_308
.Lunsigned64_quotient_2a0:
	subs r6, r1, r0
	mov r1, r4
	uxth.w fp, fp
	mov r0, r6
	bl __aeabi_uidiv
	mov r1, r4
	mov r9, r0
	mov r0, r6
	bl __aeabi_uidivmod
	mul.w r6, sl, r9
	orr.w r1, fp, r1, lsl #16
	cmp r6, r1
	bls.n .Lunsigned64_quotient_2da
	adds r1, r1, r5
	add.w r3, r9, #4294967295
	bcs.n .Lunsigned64_quotient_2d8
	cmp r6, r1
	itt hi
	subhi.w r9, r9, #2
	addhi r1, r1, r5
	bhi.n .Lunsigned64_quotient_2da
.Lunsigned64_quotient_2d8:
	mov r9, r3
.Lunsigned64_quotient_2da:
	subs r6, r1, r6
	orr.w r7, r9, r7, lsl #16
	b.n .Lunsigned64_quotient_102
.Lunsigned64_quotient_2e2:
	mov r4, r3
	b.n .Lunsigned64_quotient_0a8
.Lunsigned64_quotient_2e6:
	mov r9, r2
	b.n .Lunsigned64_quotient_13c
.Lunsigned64_quotient_2ea:
	mov r6, r3
	b.n .Lunsigned64_quotient_176
.Lunsigned64_quotient_2ee:
	cmp r6, r1
	itt hi
	subhi r5, #2
	addhi r1, r9
	bhi.n .Lunsigned64_quotient_21a
	b.n .Lunsigned64_quotient_218
.Lunsigned64_quotient_2fa:
	cmp r0, r1
	itt hi
	subhi r4, #2
	addhi r1, r9
	bhi.w .Lunsigned64_quotient_1e6
	b.n .Lunsigned64_quotient_1e4
.Lunsigned64_quotient_308:
	mov r7, r3
	b.n .Lunsigned64_quotient_2a0
.size UnsignedDivision64Quotient, . - UnsignedDivision64Quotient
