/* ARM EABI signed division with native computed steps and zero saturation. */
.syntax unified
.thumb
.text
.balign 4
.global __aeabi_idiv
.type __aeabi_idiv, %function
.thumb_func
__aeabi_idiv:
	cmp r1, #0
	beq.w .Lsigned_divide_zero
.Lsigned_divide_nonzero:
	eor.w ip, r0, r1
	it mi
	rsbmi r1, r1, #0
	subs r2, r1, #1
	beq.w .Lsigned_divide_one
	movs r3, r0
	it mi
	rsbmi r3, r0, #0
	cmp r3, r1
	bls.w .Lsigned_divide_small
	tst r1, r2
	beq.w .Lsigned_divide_power_of_two
	clz r2, r3
	clz r0, r1
	sub.w r2, r0, r2
	rsb.w r2, r2, #31
	adr r0, .Lsigned_divide_steps
	add.w r2, r0, r2, lsl #4
	mov.w r0, #0
	mov pc, r2
	nop
	nop.w

/* Same fixed 16-byte restoring steps as unsigned division, signed magnitudes. */
.macro signedDivideStep shift
	cmp.w r3, r1, lsl #\shift
	nop
	adc.w r0, r0, r0
	it cs
	subcs.w r3, r3, r1, lsl #\shift
.endm
.Lsigned_divide_steps:
.irp shift,31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0
	signedDivideStep \shift
.endr
	cmp.w ip, #0
	it mi
	rsbmi r0, r0, #0
	bx lr
.Lsigned_divide_one:
	teq.w ip, r0
	it mi
	rsbmi r0, r0, #0
	bx lr
.Lsigned_divide_small:
	it cc
	movcc r0, #0
	itt eq
	asreq.w r0, ip, #31
	orreq r0, r0, #1
	bx lr
.Lsigned_divide_power_of_two:
	clz r2, r1
	rsb.w r2, r2, #31
	cmp.w ip, #0
	lsr.w r0, r3, r2
	it mi
	rsbmi r0, r0, #0
	bx lr
.Lsigned_divide_zero:
	cmp r0, #0
	it gt
	mvngt r0, #0x80000000
	it lt
	movlt.w r0, #0x80000000
	push {r0, r1, r2}
	ldr r0, .Lsigned_divide_zero_target
	adr r1, .Lsigned_divide_zero_target
	add r0, r1
	str r0, [sp, #8]
	pop {r0, r1, pc}
.Lsigned_divide_zero_target:
	.word HandleSignedDivisionByZero - .Lsigned_divide_zero_target
.size __aeabi_idiv, . - __aeabi_idiv

.global __aeabi_idivmod
.type __aeabi_idivmod, %function
.thumb_func
__aeabi_idivmod:
	cmp r1, #0
	beq .Lsigned_divide_zero
	push.w {r0, r1, lr}
	bl .Lsigned_divide_nonzero
	pop.w {r1, r2, lr}
	mul r3, r2, r0
	sub.w r1, r1, r3
	bx lr
.size __aeabi_idivmod, . - __aeabi_idivmod
	nop
	nop.w
.type HandleSignedDivisionByZero, %function
.thumb_func
HandleSignedDivisionByZero:
	bx lr
.size HandleSignedDivisionByZero, . - HandleSignedDivisionByZero
.section .note.GNU-stack,"",%progbits
