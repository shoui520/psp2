/* Native variant of the original polynomial NEON vector transform. */
	.syntax unified
	.thumb
	.fpu neon
	.text
	.global ScePafMisc_22CB84B0
	.type ScePafMisc_22CB84B0, %function
	.thumb_func
ScePafMisc_22CB84B0:
	.fnstart
	push {r4, r5, r6, lr}
	vmov.i32 q8, #0x3f000000
	vldmia r1, {d18, d19}
	movw r2, #0xf983
	vmov.i32 q10, #0x80000000
	movt r2, #0x3f22
	vdup.32 q11, r2
	vmul.f32 q9, q9, q8
	movw r2, #0xfda
	vorr q12, q9, q9
	movt r2, #0x3fc9
	vorr q15, q9, q9
	vbif q12, q8, q10
	vmul.f32 q8, q9, q11
	vdup.32 q11, r2
	movw r2, #0x2168
	vadd.f32 q8, q8, q12
	movt r2, #0x33a2
	vdup.32 q14, r2
	vcvt.s32.f32 q8, q8
	movw r3, #0x4993
	movw r4, #0xa036
	vcvt.f32.s32 q13, q8
	movt r3, #0xbab2
	vdup.32 q0, r3
	movt r4, #0x3d2a
	vmls.f32 q15, q13, q11
	movw r2, #0x8c6e
	vdup.32 q1, r4
	movt r2, #0xb94c
	vmov.i32 q12, #3
	vmls.f32 q15, q13, q14
	movw r5, #0xffdf
	vdup.32 q13, r2
	movw r2, #0x8342
	movt r5, #0xbeff
	vmul.f32 q14, q15, q15
	movt r2, #0x3c08
	vdup.32 q2, r5
	movw r6, #0xaaa1
	vand q8, q8, q12
	vmov.i32 q11, #1
	movt r6, #0xbe2a
	vmla.f32 q1, q0, q14
	vdup.32 q0, r2
	vadd.i32 q12, q11, q8
	vmla.f32 q2, q1, q14
	vmla.f32 q0, q13, q14
	vdup.32 q1, r6
	vmul.f32 q13, q14, q15
	movs.w r2, #0x7f800000
	vmla.f32 q1, q0, q14
	vmov.f32 q0, #1.0
	vbic q10, q9, q10
	vmla.f32 q15, q1, q13
	vand q13, q12, q11
	vand q11, q8, q11
	vmla.f32 q0, q2, q14
	vmov.i32 q14, #2
	vceq.i32 q13, q13, #0
	vceq.i32 q11, q11, #0
	vand q12, q12, q14
	vand q8, q8, q14
	vdup.32 q14, r2
	vbsl q13, q15, q0
	vbsl q11, q15, q0
	vceq.i32 q12, q12, #0
	vceq.i32 q8, q8, #0
	vceq.i32 q10, q10, q14
	vceq.f32 q15, q9, #0
	vneg.f32 q14, q13
	vneg.f32 q0, q11
	vstmia r1, {d18, d19}
	vbsl q12, q13, q14
	vbsl q8, q11, q0
	vmov.i32 q11, #0
	vorr q12, q12, q10
	vbsl q15, q9, q8
	vmov.i64 d17, #0xffffffff
	vorr q9, q15, q10
	vorr d20, d25, d25
	vext.32 d16, d24, d25, #1
	vbif d20, d23, d17
	vorr d23, d24, d24
	vdup.32 d17, d24[1]
	vdup.32 d21, d18[1]
	vsli.64 d25, d19, #0x20
	vrev64.32 d28, d18
	vorr d22, d20, d20
	vsli.64 d23, d20, #0x20
	vdup.32 d20, d19[0]
	vmul.f32 q10, q12, q10
	vsli.64 d19, d24, #0x20
	vdup.32 d29, d18[0]
	vsli.64 d22, d24, #0x20
	vmul.f32 q8, q11, q8
	vldr d22, [pc, #0x1c]
	vldr d23, [pc, #0x20]
	vmul.f32 q8, q8, q9
	vmul.f32 q9, q10, q14
	vmul.f32 q9, q9, q11
	vadd.f32 q8, q8, q9
	vstmia r0, {d16, d17}
	pop {r4, r5, r6, pc}
	.fnend
	.size ScePafMisc_22CB84B0, . - ScePafMisc_22CB84B0

	/* Native coefficient literals loaded through PC-relative VLDR. */
	.word 0xbf800000, 0x3f800000, 0xbf800000, 0x3f800000
