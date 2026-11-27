/*
 * sceZlibAdler32 (libdeflt.h), the SDK's libSceDeflt vadler.o: hand-written
 * NEON Adler-32 checksum.
 *
 * unsigned int sceZlibAdler32(unsigned int uiAdler, const unsigned char *pSrc,
 *                             unsigned int uiSize);
 */

	.syntax unified
	.cpu cortex-a9
	.fpu neon
	.thumb

	.section .text.sceZlibAdler32, "ax", %progbits
	.p2align 1
	.global sceZlibAdler32
	.type sceZlibAdler32, %function
	.thumb_func
sceZlibAdler32:
	lsrs r3, r0, #0x10
	cmp r2, #0x17
	uxth r0, r0
	bls.n .Ladler5
	mov ip, r1
	bic r1, r1, #7
	vldmia r1!, {d22}
	subs.w ip, r1, ip
	vdup.8 d16, ip
	mla r3, r0, ip, r3
	subs.w r2, r2, ip
	movw ip, #0x304
	movt ip, #0x102
	vmov d25, r0, r3
	vmov.i32 q3, #0
	add.w r0, ip, #0x4040404
	vmov d31, r0, ip
	vtrn.32 d25, d7
	vcge.s8 d16, d16, d31
	vand d16, d16, d22
	vmull.u8 q0, d16, d31
	vpadal.u16 q3, q0
	vpaddl.u8 d16, d16
	vpaddl.u16 d24, d16
	vmvn.i16 d0, #0xe
	vmovl.u8 q15, d31
	vmov.i16 q2, #8
	vmov.i16 d20, #0xf0
	vbic.i32 d20, #0xe0
	vadd.i32 q2, q2, q15
	movw ip, #0x1010
.Ladler1:
	subs.w r0, r2, #0xb100
	vmov.i32 q14, #0
	bhs.n .Ladler2
	and r0, r0, #0xf
.Ladler2:
	subs r3, r2, r0
	cmp r3, ip
	bls.n .Ladler3
	mov r3, ip
.Ladler3:
	vmov.i32 q8, #0
	subs r2, r2, r3
	vmov.i32 q11, #0
.Ladler4:
	vldmia r1!, {d18, d19}
	subs r3, #0x10
	pld [r1, #0xc0]
	vaddw.u8 q11, q11, d18
	vpaddl.u8 q13, q9
	vadd.i32 q14, q14, q12
	vaddw.u8 q8, q8, d19
	vpadal.u16 q12, q13
	bne.n .Ladler4
	vmlal.u16 q3, d22, d4
	vmlal.u16 q3, d23, d5
	cmp r2, r0
	vmlal.u16 q3, d16, d30
	vmlal.u16 q3, d17, d31
	bne.n .Ladler2
	vmlal.u16 q3, d28, d20
	vshrn.i32 d1, q12, #0x10
	vmlal.u16 q3, d29, d20
	vmlsl.u16 q12, d0, d1
	cmp r2, #0xf
	vshrn.i32 d1, q3, #0x10
	vmlsl.u16 q3, d0, d1
	bhi.n .Ladler1
	vpadd.i32 d17, d25, d24
	vpadd.i32 d16, d7, d6
	vpadd.i32 d17, d17, d16
	vmov r0, r3, d17
.Ladler5:
	cbz r2, .Ladler7
.Ladler6:
	ldrb ip, [r1], #1
	subs r2, #1
	add r0, ip
	add r3, r0
	bne.n .Ladler6
.Ladler7:
	lsrs r2, r0, #0x10
	lsrs r1, r3, #0x10
	rsb r2, r2, r2, lsl #4
	rsb r1, r1, r1, lsl #4
	uxtah r2, r2, r0
	uxtah r1, r1, r3
	movw r3, #0xfff1
	subs r2, r2, r3
	it lo
	addlo r2, r2, r3
	subs r1, r1, r3
	it lo
	addlo r1, r1, r3
	add.w r0, r2, r1, lsl #16
	bx lr
	.size sceZlibAdler32, .-sceZlibAdler32

	.section .note.GNU-stack, "", %progbits
