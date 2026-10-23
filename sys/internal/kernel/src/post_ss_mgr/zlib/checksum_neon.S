/*
 * SCE zlib NEON check values, hand-written in assembly by Sony: the bodies of
 * adler32() and crc32() (checksum.c). crc32 folds 16 bytes at a time with
 * vmull.p8 against the constants in front of the table (zlibCrc32NeonTable), then
 * finishes byte by byte through the table (zlib's crc_table[0]).
 *
 * uLong zlibAdler32Neon(uLong adler, const Bytef *buf, uInt len);
 * uLong zlibCrc32Neon(uLong crc, const Bytef *buf, uInt len);
 */

	.syntax unified
	.cpu cortex-a9
	.fpu neon
	.thumb
	.text

.global zlibAdler32Neon
.type zlibAdler32Neon, %function
.thumb_func
	.p2align 1
zlibAdler32Neon:
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
	vmov.i32 q3, #0
	vmov d25, r0, r3
	add.w r0, ip, #0x4040404
	vmov d31, r0, ip
	vtrn.32 d25, d7
	vcge.s8 d16, d16, d31
	vmov.i16 d20, #0xf0
	vand d16, d16, d22
	vmvn.i16 d0, #0xe
	vmull.u8 q1, d16, d31
	movw ip, #0x1010
	vmovl.u8 q15, d31
	vmov.i16 q2, #8
	vpaddl.u8 d16, d16
	vbic.i32 d20, #0xe0
	vadd.i32 q2, q2, q15
	vpadal.u16 q3, q1
	vpaddl.u16 d24, d16
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
.size zlibAdler32Neon, .-zlibAdler32Neon

/* crc32 of fewer than 16 bytes: Sony's unwind region for crc32 starts here, in
 * front of its entry */
.type zlibCrc32NeonBytes, %function
.thumb_func
zlibCrc32NeonBytes:	/* name unknown */
.LcrcBytes:
	cmp r2, #0
	b.n .Lcrc7
.size zlibCrc32NeonBytes, .-zlibCrc32NeonBytes

.global zlibCrc32Neon
.type zlibCrc32Neon, %function
.thumb_func
zlibCrc32Neon:
	mvns r0, r0
	cmp r2, #0x10
	movw ip, #:lower16:(zlibCrc32NeonTable + 0x10)
	movt ip, #:upper16:(zlibCrc32NeonTable + 0x10)
	push {r0, r1, r4, lr}
	blo.n .LcrcBytes
	sub.w r3, ip, #0x10
	vldmia r3, {d27, d28}
	movs r4, #0
	vld1.8 {d0, d1}, [r1]!
	vmov d2, r0, r4
	vrev16.8 d26, d27
	vrev16.8 d29, d28
	subs r2, #0x20
	veor d0, d0, d2
	vrev32.16 q12, q13
	vrev32.16 q15, q14
	blo.n .Lcrc2
.Lcrc1:
	vmull.p8 q9, d1, d25
	vmull.p8 q11, d1, d27
	vmull.p8 q1, d0, d29
	vmull.p8 q3, d0, d31
	vmull.p8 q10, d1, d26
	vmull.p8 q2, d0, d30
	vmull.p8 q8, d1, d24
	vmull.p8 q0, d0, d28
	pld [r1, #0x40]
	veor q3, q3, q11
	subs r2, #0x10
	veor q1, q1, q9
	veor q2, q2, q10
	veor d6, d6, d7
	vuzp.16 d2, d3
	veor d2, d2, d3
	veor q0, q0, q8
	vshll.u16 q3, d6, #8
	vshll.u16 q1, d2, #8
	veor q2, q2, q3
	veor q1, q0, q1
	veor d4, d4, d5
	vadd.i64 q1, q1, q1
	vshll.u32 q2, d4, #0x11
	vld1.8 {d0, d1}, [r1]!
	veor q1, q1, q2
	veor d2, d2, d3
	veor d0, d0, d2
	bhs.n .Lcrc1
.Lcrc2:
	adds r2, #8
	blo.n .Lcrc3
	vmull.p8 q2, d1, d25
	vmull.p8 q8, d1, d27
	vmull.p8 q3, d1, d26
	vmull.p8 q1, d1, d24
	vmull.p8 q9, d0, d29
	vmull.p8 q10, d0, d31
	vmull.p8 q11, d0, d30
	vmull.p8 q15, d0, d28
	vld1.8 {d1}, [r1]!
	veor q2, q2, q9
	veor q8, q8, q10
	veor q3, q3, q11
	veor q1, q1, q15
	subs r2, #8
	b.n .Lcrc4
.Lcrc3:
	vmull.p8 q2, d0, d25
	vmull.p8 q8, d0, d27
	vmull.p8 q3, d0, d26
	vmull.p8 q1, d0, d24
.Lcrc4:
	vuzp.16 d4, d5
	add r1, r2
	veor d16, d16, d17
	lsls r2, r2, #3
	veor d4, d4, d5
	vdup.32 d18, r2
	rsbs r0, r2, #0
	vshll.u16 q8, d16, #8
	vshll.u16 q2, d4, #8
	veor q3, q3, q8
	veor q1, q1, q2
	veor d6, d6, d7
	vdup.32 q15, r0
	subs r0, #0x40
	vadd.i64 q1, q1, q1
	vshll.u32 q3, d6, #0x11
	vld1.8 {d28}, [r1]
	veor q3, q3, q1
	mov r1, sp
	veor d0, d6, d1
	vshl.u64 d1, d28, d18
	veor d0, d0, d7
	cbz r0, .Lcrc5
	vshl.u64 q14, q0, q15
	vmull.p8 q2, d28, d25
	vmull.p8 q8, d28, d27
	vorr.i32 d31, #0xc0
	vmull.p8 q1, d28, d24
	vmull.p8 q3, d28, d26
	vshl.u64 d28, d0, d31
	vuzp.16 d4, d5
	veor d16, d16, d17
	veor d4, d4, d5
	vshll.u16 q8, d16, #8
	vshll.u16 q2, d4, #8
	veor q3, q3, q8
	veor q1, q1, q2
	veor d6, d6, d7
	vadd.i64 q1, q1, q1
	vshll.u32 q3, d6, #0x11
	veor q1, q1, q14
	veor q3, q3, q1
	veor d0, d6, d7
	movs r0, #0
.Lcrc5:
	movs r2, #8
	vstr d0, [sp]
.Lcrc6:
	ldrb r3, [r1], #1
	and r4, r0, #0xff
	eors r4, r3
	ldr.w r3, [ip, r4, lsl #2]
	subs r2, #1
	eor.w r0, r3, r0, lsr #8
.Lcrc7:
	bne.n .Lcrc6
	mvns r0, r0
	add sp, #8
	pop {r4, pc}
.size zlibCrc32Neon, .-zlibCrc32Neon

.section .note.GNU-stack, "", %progbits
