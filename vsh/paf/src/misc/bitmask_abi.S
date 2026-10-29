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
