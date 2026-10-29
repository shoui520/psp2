/* Original native NEON polynomial transform, export 315CCB24. */
	.syntax unified
	.thumb
	.fpu neon
	.text
	.global ScePafMisc_315CCB24
	.type ScePafMisc_315CCB24, %function
	.thumb_func
ScePafMisc_315CCB24:
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
	vmls.f32 q15, q13, q14
	movw r5, #0xffdf
	vdup.32 q13, r2
	movw r2, #0x8342
	movt r5, #0xbeff
	vmul.f32 q14, q15, q15
	movt r2, #0x3c08
	vdup.32 q2, r5
	vmov.i32 q12, #3
	movw r6, #0xaaa1
	vmov.i32 q11, #1
	movt r6, #0xbe2a
	vmla.f32 q1, q0, q14
	vdup.32 q0, r2
	vand q8, q8, q12
	vmla.f32 q2, q1, q14
	vmla.f32 q0, q13, q14
	vdup.32 q1, r6
	vmul.f32 q13, q14, q15
	vadd.i32 q12, q11, q8
	movs.w r2, #0x7f800000
	vmla.f32 q1, q0, q14
	vmov.f32 q0, #1.000000e+00
	vbic q10, q9, q10
	vmla.f32 q15, q1, q13
	vand q13, q8, q11
	vmla.f32 q0, q2, q14
	vmov.i32 q14, #2
	vand q11, q12, q11
	vceq.i32 q13, q13, #0
	vand q8, q8, q14
	vand q12, q12, q14
	vceq.i32 q11, q11, #0
	vdup.32 q14, r2
	vbsl q13, q15, q0
	vceq.i32 q8, q8, #0
	vbsl q11, q15, q0
	vceq.i32 q10, q10, q14
	vceq.i32 q12, q12, #0
	vceq.f32 q15, q9, #0
	vneg.f32 q14, q13
	vneg.f32 q0, q11
	vbsl q8, q13, q14
	vbsl q12, q11, q0
	vbsl q15, q9, q8
	vorr q8, q12, q10
	vstmia r1, {d18, d19}
	vorr q10, q15, q10
	vmov.i32 q9, #0
	vmov.i64 d22, #0xffffffff
	vdup.32 d28, d17[0]
	vorr d25, d21, d21
	vext.32 d29, d16, d17, #1
	vbif d17, d20, d22
	vdup.32 d21, d20[0]
	vrev64.32 d26, d16
	vorr d31, d25, d25
	vorr d27, d16, d16
	vorr d24, d20, d20
	vmov.f32 d18, #1.000000e+00
	vbif d31, d19, d22
	vext.32 d22, d20, d25, #1
	vorr d23, d22, d22
	vsli.64 d25, d16, #0x20
	vmul.f32 q13, q13, q14
	vsli.64 d31, d20, #0x20
	vmul.f32 q8, q8, q11
	vorr d30, d31, d31
	vorr d31, d21, d21
	vmov.f32 d19, #-1.000000e+00
	vmul.f32 q10, q13, q12
	vmul.f32 q8, q8, q15
	vmul.f32 q8, q8, q9
	vadd.f32 q8, q10, q8
	vstmia r0, {d16, d17}
	pop {r4, r5, r6, pc}
	.fnend
	.size ScePafMisc_315CCB24, . - ScePafMisc_315CCB24
