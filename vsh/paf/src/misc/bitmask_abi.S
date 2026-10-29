/* Native PAF counter and allocated-bitmask state transition. */
	.syntax unified
	.thumb
	.text
	.global ScePafMisc_1805D207
	.type ScePafMisc_1805D207, %function
	.thumb_func
ScePafMisc_1805D207:
	.fnstart
	ldr r2, [r0, #0x14]
	movs r3, #1
	lsls.w r1, r3, r1
	cbz r2, .Lallocate
	ldr r3, [r0, #0xc]
	ands r3, r1
	beq .Lallocate
	adds r0, r2, #0
	b .Lreturn
.Lallocate:
	ldr r3, [r0, #0xc]
	adds r2, #1
	str r2, [r0, #0x14]
	orrs r1, r3
	adds r3, r0, #0
	str r1, [r3, #0xc]
	adds r0, r2, #0
.Lreturn:
	bx lr
	.fnend
	.size ScePafMisc_1805D207, . - ScePafMisc_1805D207

	.global ScePafMisc_E80F175C
	.type ScePafMisc_E80F175C, %function
	.thumb_func
ScePafMisc_E80F175C:
	.fnstart
	ldr r3, [r1, #0x48]
	cmp r3, #1
	bne .LcopyE80
	ldr.w ip, [r1, #0x1c]
	ldr r3, [r1, #0x20]
	cbz r2, .LstoreE80
.LroundE80:
	add.w r1, ip, #1
	adds r3, #1
	add.w ip, r1, r1, lsr #31
	add.w r3, r3, r3, lsr #31
	subs r2, #1
	asr.w ip, ip, #1
	asr.w r3, r3, #1
	bne .LroundE80
.LstoreE80:
	str.w ip, [r0]
	str r3, [r0, #4]
	b .LreturnE80
.LcopyE80:
	ldr r2, [r1, #0x1c]
	str r2, [r0]
	ldr r1, [r1, #0x20]
	str r1, [r0, #4]
.LreturnE80:
	bx lr
	.fnend
	.size ScePafMisc_E80F175C, . - ScePafMisc_E80F175C

	.global ScePafMisc_EA6558AD
	.type ScePafMisc_EA6558AD, %function
	.thumb_func
ScePafMisc_EA6558AD:
	.fnstart
	push {r4, lr}
	movs r3, #0
	adds r0, r3, #0
	cbz r1, .LnotFoundEA
.LsearchEA:
	ands r4, r1, #1
	beq .LshiftEA
	cmp r3, r2
	bne .LnextEA
	b .LfoundEA
.LnextEA:
	adds r3, #1
.LshiftEA:
	lsrs r1, r1, #1
	add.w r0, r0, #1
	bne .LsearchEA
.LnotFoundEA:
	movs.w r0, #-1
.LfoundEA:
	pop {r4, pc}
	.fnend
	.size ScePafMisc_EA6558AD, . - ScePafMisc_EA6558AD

	.global ScePafMisc_49034205
	.type ScePafMisc_49034205, %function
	.thumb_func
ScePafMisc_49034205:
	.fnstart
	push {r4, lr}
	ldr r3, [r0, #0x14]
	cbz r3, .Lfailure490
	movs r4, #1
	ldr r2, [r0, #0xc]
	lsls.w r1, r4, r1
	ands.w r4, r2, r1
	bne .Lrelease490
.Lfailure490:
	movs.w r0, #-1
	b .Lreturn490
.Lrelease490:
	bics.w r1, r2, r1
	str r1, [r0, #0xc]
	subs r1, r3, #1
	str r1, [r0, #0x14]
	adds r0, r1, #0
.Lreturn490:
	pop {r4, pc}
	.fnend
	.size ScePafMisc_49034205, . - ScePafMisc_49034205

	.global ScePafMisc_2B325CC6
	.type ScePafMisc_2B325CC6, %function
	.thumb_func
ScePafMisc_2B325CC6:
	.fnstart
	push {r4, lr}
	ldr r2, [r0, #0x48]
	cmp r2, #1
	ldrh.w r4, [r0, #0x52]
	bne .LnoHalf2B3
	ldr r2, [r0, #0x20]
	cbz r1, .LskipHalf2B3
.LhalfLoop2B3:
	adds r0, r2, #1
	add.w r2, r0, r0, lsr #31
	subs r1, #1
	asr.w r2, r2, #1
	bne .LhalfLoop2B3
.LskipHalf2B3:
	b .Ldivide2B3
.LnoHalf2B3:
	ldr r2, [r0, #0x20]
.Ldivide2B3:
	adds r0, r2, r4
	subs r0, #1
	adds r1, r4, #0
	bl __divsi3
	mul r0, r0, r4
	pop {r4, pc}
	.fnend
	.size ScePafMisc_2B325CC6, . - ScePafMisc_2B325CC6

	.global ScePafMisc_9CE18975
	.type ScePafMisc_9CE18975, %function
	.thumb_func
ScePafMisc_9CE18975:
	.fnstart
	push {r4, lr}
	ldr r0, [r0, #0x2c]
	bl ScePafMisc_2B325CC6
	pop {r4, pc}
	.fnend
	.size ScePafMisc_9CE18975, . - ScePafMisc_9CE18975
