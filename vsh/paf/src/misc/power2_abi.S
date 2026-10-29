/* Native 31-bit power-of-two validation and halfword field setters. */
	.syntax unified
	.thumb
	.text

	.global ScePafMisc_C875DB31
	.type ScePafMisc_C875DB31, %function
	.thumb_func
ScePafMisc_C875DB31:
	.fnstart
	push {r4, lr}
	cmp r1, #0
	bgt .LvalidC875
	movs r0, #0
	b .LreturnC875
.LvalidC875:
	movs r3, #0
	adds r0, r3, #0
	adds r2, r0, #0
.LcountC875:
	ands r4, r1, #1
	beq .LskipC875
	adds r3, #1
.LskipC875:
	adds r2, #1
	asrs r1, r1, #1
	cmp r2, #0x1f
	blt .LcountC875
	cmp r3, #1
	bne .LreturnC875
	movs r0, #1
.LreturnC875:
	pop {r4, pc}
	.fnend
	.size ScePafMisc_C875DB31, . - ScePafMisc_C875DB31

	.global ScePafMisc_0C98E962
	.type ScePafMisc_0C98E962, %function
	.thumb_func
ScePafMisc_0C98E962:
	.fnstart
	push {r4, lr}
	mov ip, r1
	cmp.w ip, #0
	ble .Lfail0C98
	movs r3, #0
	adds r2, r3, #0
.Lcount0C98:
	ands r4, ip, #1
	beq .Lskip0C98
	adds r3, #1
.Lskip0C98:
	adds r2, #1
	asr.w ip, ip, #1
	cmp r2, #0x1f
	blt .Lcount0C98
	cmp r3, #1
	bne .Lfail0C98
	ldr r2, [r0, #0x2c]
	movs r0, #1
	strh.w r1, [r2, #0x50]
	b .Lreturn0C98
.Lfail0C98:
	movs r0, #0
.Lreturn0C98:
	pop {r4, pc}
	.fnend
	.size ScePafMisc_0C98E962, . - ScePafMisc_0C98E962

	.global ScePafMisc_BF9CCCBF
	.type ScePafMisc_BF9CCCBF, %function
	.thumb_func
ScePafMisc_BF9CCCBF:
	.fnstart
	push {r4, lr}
	mov ip, r1
	cmp.w ip, #0
	ble .LfailBF9C
	movs r3, #0
	adds r2, r3, #0
.LcountBF9C:
	ands r4, ip, #1
	beq .LskipBF9C
	adds r3, #1
.LskipBF9C:
	adds r2, #1
	asr.w ip, ip, #1
	cmp r2, #0x1f
	blt .LcountBF9C
	cmp r3, #1
	bne .LfailBF9C
	ldr r2, [r0, #0x2c]
	movs r0, #1
	strh.w r1, [r2, #0x52]
	b .LreturnBF9C
.LfailBF9C:
	movs r0, #0
.LreturnBF9C:
	pop {r4, pc}
	.fnend
	.size ScePafMisc_BF9CCCBF, . - ScePafMisc_BF9CCCBF
