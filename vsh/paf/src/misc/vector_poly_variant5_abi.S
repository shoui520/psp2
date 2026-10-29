/* Native NEON vector polynomial transform variant 195D4D04. */
	.syntax unified
	.thumb
	.fpu neon
	.text
	.global ScePafMisc_195D4D04
	.type ScePafMisc_195D4D04, %function
	.thumb_func
ScePafMisc_195D4D04:
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
	vbif q12, q8, q10
	vmul.f32 q8, q9, q11
	vdup.32 q11, r2
	movw r2, #0x2168
	vadd.f32 q8, q8, q12
	vmov.i32 q12, #3
	movt r2, #0x33a2
	vdup.32 q14, r2
	vcvt.s32.f32 q8, q8
	movw r3, #0x4993
	vcvt.f32.s32 q13, q8
	vand q8, q8, q12
	vorr q12, q9, q9
	movw r4, #0xa036
	movt r3, #0xbab2
	vmls.f32 q12, q13, q11
	movt r4, #0x3d2a
	vdup.32 q0, r3
	vdup.32 q1, r4
	movw r2, #0x8c6e
	movt r2, #0xb94c
	vmls.f32 q12, q13, q14
	movw r5, #0xffdf
	vdup.32 q14, r2
	movw r2, #0x8342
	movt r5, #0xbeff
	vmul.f32 q15, q12, q12
	movt r2, #0x3c08
	vdup.32 q2, r5
	movw r6, #0xaaa1
	vmov.i32 q11, #1
	movt r6, #0xbe2a
	vmla.f32 q1, q0, q15
	vdup.32 q0, r2
	vadd.i32 q13, q11, q8
	vmla.f32 q2, q1, q15
	vmla.f32 q0, q14, q15
	vdup.32 q1, r6
	vmul.f32 q14, q15, q12
	vmla.f32 q1, q0, q15
	vand q0, q13, q11
	movs.w r2, #0x7f800000
	vand q11, q8, q11
	vmla.f32 q12, q1, q14
	vmov.f32 q14, #1.000000e+00
	vbic q10, q9, q10
	vceq.i32 q11, q11, #0
	vmla.f32 q14, q2, q15
	vceq.i32 q15, q0, #0
	vmov.i32 q0, #2
	vbsl q15, q12, q14
	vand q13, q13, q0
	vand q8, q8, q0
	vdup.32 q0, r2
	vbsl q11, q12, q14
	vmov.i32 q14, #0
	vneg.f32 q1, q15
	vceq.i32 q13, q13, #0
	vceq.i32 q10, q10, q0
	vceq.i32 q8, q8, #0
	vorr d28, d29, d29
	vbsl q13, q15, q1
	vmov.i64 d29, #0xffffffff
	vorr q12, q13, q10
	vneg.f32 q13, q11
	vorr d30, d25, d25
	vbsl q8, q11, q13
	vceq.f32 q11, q9, #0
	vorr d31, d24, d24
	vbif d30, d28, d29
	vbsl q11, q9, q8
	vstmia r1, {d18, d19}
	vext.64 q8, q15, q15, #1
	vorr q10, q11, q10
	vorr d23, d30, d30
	vdup.32 d22, d25[0]
	vrev64.32 d26, d31
	vorr d17, d16, d16
	vdup.32 q9, d20[0]
	vorr d16, d21, d21
	vbif d21, d28, d29
	vorr d1, d20, d20
	vorr d27, d26, d26
	vsli.64 d23, d17, #0x20
	vorr d24, d17, d17
	vsli.64 d17, d30, #0x20
	vorr d0, d21, d21
	vsli.64 d1, d21, #0x20
	vorr d21, d16, d16
	vorr d25, d23, d23
	vorr d23, d17, d17
	vsli.64 d0, d20, #0x20
	vbif d21, d31, d29
	vmul.f32 q11, q13, q11
	vbif d25, d19, d29
	vext.32 d16, d20, d16, #1
	vdup.32 d17, d20[1]
	vmul.f32 q9, q11, q10
	vmul.f32 q11, q12, q0
	vmov.f32 d20, #-1.000000e+00
	vmov.f32 d21, #1.000000e+00
	vmul.f32 q8, q11, q8
	vmul.f32 q8, q8, q10
	vadd.f32 q8, q9, q8
	vstmia r0, {d16, d17}
	pop {r4, r5, r6, pc}
	.fnend
	.size ScePafMisc_195D4D04, . - ScePafMisc_195D4D04
