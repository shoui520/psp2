/* Native PAF nonlocal-jump ABI: general, NEON and FPSCR state. */
	.syntax unified
	.thumb
	.fpu neon
	.text

	.global sce_paf_setjmp
	.type sce_paf_setjmp, %function
	.thumb_func
sce_paf_setjmp:
	.fnstart
	mov ip, sp
	stmia.w r0!, {r4-r12, lr}
	vstmia r0!, {d8-d15}
	vmrs r1, fpscr
	movw ip, #0x00ff
	movt ip, #0xf800
	bic.w r1, r1, ip
	vmsr fpscr, r1
	stmia r0!, {r1}
	mov.w r0, #0
	b .LscrubCallerState
	.fnend
	.size sce_paf_setjmp, . - sce_paf_setjmp

	.global sce_paf_longjmp
	.type sce_paf_longjmp, %function
	.thumb_func
sce_paf_longjmp:
	.fnstart
	ldmia.w r0!, {r4-r12, lr}
	vldmia r0!, {d8-d15}
	ldmia r0!, {r2}
	vmsr fpscr, r2
	mov sp, ip
	adds r0, r1, #0
	it eq
	moveq r0, #1
.LscrubCallerState:
	movw r1, #0xdead
	movt r1, #0x7f80
	movw r2, #0xdead
	movt r2, #0x7ff8
	vmov d0, r1, r2
	vmov d1, d0
	vmov q1, q0
	vmov q2, q0
	vmov q3, q0
	vmov q8, q0
	vmov q9, q0
	vmov q10, q0
	vmov q11, q0
	vmov q12, q0
	vmov q13, q0
	vmov q14, q0
	vmov q15, q0
	movw r1, #0xbeef
	movt r1, #0xdead
	mov r2, r1
	mov r3, r1
	mov ip, r1
	bx lr
	.fnend
	.size sce_paf_longjmp, . - sce_paf_longjmp

/*
 * sce_paf_memfill32(dst, pattern, bytes): bytes of the 32-bit pattern from dst on, the pattern
 * turned a byte on with every byte stored before dst is word aligned, then words to a 32-byte
 * boundary and NEON blocks of 32 bytes; returns dst (Sony's helper after longjmp; name ours).
 */
	.global sce_paf_memfill32
	.type sce_paf_memfill32, %function
	.thumb_func
sce_paf_memfill32:
	.fnstart
	cbz r2, .Lreturn
	mov ip, r0
.Lhead:
	tst.w r0, #3
	beq .Lwords
	subs r2, #1
	strb.w r1, [r0], #1
	mov.w r1, r1, ror #8
	bne .Lhead
	mov r0, ip
.Lreturn:
	bx lr
.Lwords:
	tst.w r0, #0x1f
	beq .Lblocks
	subs r2, #4
	bcc .Ltail4
	str.w r1, [r0], #4
	bne .Lwords
	mov r0, ip
	bx lr
.Lblocks:
	vdup.32 q0, r1
	vmov q1, q0
.Lblock:
	subs r2, #0x20
	blt .Ltail32
	vstmia r0!, {d0-d3}
	bne .Lblock
	mov r0, ip
	bx lr
.Ltail32:
	adds r2, #0x1c
.Ltail4:
	adds r2, #4
.Ltail:
	subs r2, #1
	strb.w r1, [r0], #1
	mov.w r1, r1, ror #8
	bne .Ltail
	mov r0, ip
	bx lr
	.fnend
	.size sce_paf_memfill32, . - sce_paf_memfill32
