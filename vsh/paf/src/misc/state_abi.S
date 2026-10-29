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
