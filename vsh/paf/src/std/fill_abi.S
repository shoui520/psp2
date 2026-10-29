/* Native PAF word-pattern fill: 32-bit value repeated for count words. */
	.syntax unified
	.thumb
	.fpu neon
	.text

	.global ScePafStdc_8A58EE24
	.type ScePafStdc_8A58EE24, %function
	.thumb_func
ScePafStdc_8A58EE24:
	.fnstart
	push {r4, lr}
	adds r4, r0, #0
	lsls r2, r2, #2
	bl PafWordFillInternal
	adds r0, r4, #0
	pop {r4, pc}
	.fnend
	.size ScePafStdc_8A58EE24, . - ScePafStdc_8A58EE24

	.type PafWordFillInternal, %function
	.thumb_func
PafWordFillInternal:
	.fnstart
	cbz r2, .Lreturn
	mov ip, r0
.Lalign4:
	tst.w r0, #3
	beq .Lalign32
	subs r2, #1
	strb.w r1, [r0], #1
	ror.w r1, r1, #8
	bne .Lalign4
	mov r0, ip
.Lreturn:
	bx lr
.Lalign32:
	tst.w r0, #0x1f
	beq .Lneon
	subs r2, #4
	bcc .Ltail
	str.w r1, [r0], #4
	bne .Lalign32
	mov r0, ip
	bx lr
.Lneon:
	vdup.32 q0, r1
	vmov q1, q0
.Lneonloop:
	subs r2, #0x20
	blt .Lneonrest
	vstmia r0!, {d0-d3}
	bne .Lneonloop
	mov r0, ip
	bx lr
.Lneonrest:
	adds r2, #0x1c
.Ltail:
	adds r2, #4
.Ltailloop:
	subs r2, #1
	strb.w r1, [r0], #1
	ror.w r1, r1, #8
	bne .Ltailloop
	mov r0, ip
	bx lr
	.fnend
	.size PafWordFillInternal, . - PafWordFillInternal
