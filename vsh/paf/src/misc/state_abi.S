/* Native miscellaneous state copy and class-flag transitions. */
	.syntax unified
	.thumb
	.fpu neon
	.text

	.global ScePafMisc_679B51BB
	.type ScePafMisc_679B51BB, %function
	.thumb_func
ScePafMisc_679B51BB:
	.fnstart
	adds r2, r0, #0
	ldr r0, [r1, #0x48]
	cmp r0, #0
	str r0, [r2]
	ldr r0, [r1, #0x4c]
	str r0, [r2, #4]
	beq .Lend679
	dmb sy
	adds r0, #4
.Lretry679:
	ldrex r1, [r0]
	adds r1, #1
	strex r2, r1, [r0]
	cmp r2, #0
	bne .Lretry679
	dmb sy
.Lend679:
	bx lr
	.fnend
	.size ScePafMisc_679B51BB, . - ScePafMisc_679B51BB

	.global ScePafMisc_7CF66185
	.type ScePafMisc_7CF66185, %function
	.thumb_func
ScePafMisc_7CF66185:
	.fnstart
	ldr r1, [r0, #0x10]
	ands r2, r1, #0xf000
	cmp.w r2, #0x3000
	beq .Ldone7CF
	movs r2, #2
	adds r3, r1, #0
	bfi r3, r2, #0xc, #4
	str r3, [r0, #0x10]
	ands r1, r1, #0xf00
	cmp.w r1, #0xc00
	bne .Lcopy7CF
	movs r1, #0
	str r1, [r0, #0xc]
	str r1, [r0, #0x18]
	b .Ldone7CF
.Lcopy7CF:
	vldr s0, [r0, #4]
	movs.w r1, #0x3f800000
	str r1, [r0, #0x18]
	vstr s0, [r0, #0xc]
.Ldone7CF:
	bx lr
	.fnend
	.size ScePafMisc_7CF66185, . - ScePafMisc_7CF66185

	.global ScePafMisc_1E66D069
	.type ScePafMisc_1E66D069, %function
	.thumb_func
ScePafMisc_1E66D069:
	.fnstart
	push {r4, r5, r6, lr}
	ldr r4, [r1]
	movs.w ip, #0
	cmp r4, #0
	ble .LnoUpdate1E
	ldr r3, [r1, #4]
	cmp r3, #0
	ble .LnoUpdate1E
	ldr r3, [r0, #0x2c]
	ldr r0, [r3, #0x44]
	cbz r0, .LnoUpdate1E
	cmp r0, #6
	beq .LnoUpdate1E
	cmp r0, #7
	beq .LnoUpdate1E
	cmp r0, #8
	bne .Lupdate1E
.LnoUpdate1E:
	mov r0, ip
	b .Lreturn1E
.Lupdate1E:
	str r4, [r3, #0x1c]
	ldr r4, [r1, #4]
	movs r0, #1
	str r4, [r3, #0x20]
	ldr r4, [r1]
	ldr r5, [r1, #4]
	str.w ip, [r3, #0x24]
	str.w ip, [r3, #0x28]
	str r4, [r3, #0x2c]
	str r5, [r3, #0x30]
	str r2, [r3, #0x54]
.Lreturn1E:
	pop {r4, r5, r6, pc}
	.fnend
	.size ScePafMisc_1E66D069, . - ScePafMisc_1E66D069
