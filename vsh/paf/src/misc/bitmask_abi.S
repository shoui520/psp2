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
