/* Native 39B88D48 polynomial transform, reconstructed Thumb/NEON. */
	.syntax unified
	.thumb
	.fpu neon
	.text
	.global ScePafMisc_39B88D48
	.type ScePafMisc_39B88D48, %function
	.thumb_func
ScePafMisc_39B88D48:
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
	vstmia r1, {d18, d19}
	vbsl q15, q9, q8
	vmov.i32 q9, #0
	vorr q8, q12, q10
	vorr q10, q15, q10
	vorr d18, d19, d19
	vmov.i64 d22, #0xffffffff
	vorr d1, d17, d17
	vorr d25, d21, d21
	vorr d27, d20, d20
	vorr d0, d16, d16
	vdup.32 d28, d17[0]
	vdup.32 d29, d16[1]
	vrev64.32 d30, d16
	vorr d19, d25, d25
	vbif d1, d27, d22
	vdup.32 d31, d16[0]
	vsli.64 d25, d17, #0x20
	vorr d24, d27, d27
	vdup.32 d26, d21[0]
	vbif d19, d18, d22
	vrev64.32 d22, d27
	vorr d23, d22, d22
	vmul.f32 q10, q14, q15
	vmul.f32 q8, q0, q11
	vsli.64 d27, d19, #0x20
	vldr d18, [pc, #0x1c]
	vldr d19, [pc, #0x20]
	vmul.f32 q10, q10, q12
	vmul.f32 q8, q8, q13
	vmul.f32 q8, q8, q9
	vadd.f32 q8, q10, q8
	vstmia r0, {d16, d17}
	pop {r4, r5, r6, pc}
	.fnend
	.size ScePafMisc_39B88D48, . - ScePafMisc_39B88D48

	/* Original PC-relative coefficients. */
	.word 0x3f800000
	.word 0xbf800000
	.word 0xbf800000
	.word 0x3f800000
