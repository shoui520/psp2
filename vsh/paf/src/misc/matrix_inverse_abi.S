/* Original NEON 3D affine-matrix inverse. */
	.syntax unified
	.thumb
	.fpu neon
	.text
	.global ScePafMisc_62EB3B09
	.type ScePafMisc_62EB3B09, %function
	.thumb_func
ScePafMisc_62EB3B09:
	.fnstart
	push {r4, lr}
	vpush {s16, s17, s18, s19, s20, s21, s22, s23}
	vldmia r1, {d0, d1, d2, d3, d4, d5}
	vorr q3, q0, q0
	vdup.32 d18, d0[1]
	vdup.32 d16, d2[1]
	vorr q4, q1, q1
	vdup.32 d17, d4[1]
	vorr q5, q2, q2
	vdup.32 d19, d5[0]
	vmul.f32 d20, d16, d6[0]
	vmul.f32 d22, d17, d8[0]
	vmul.f32 d24, d18, d10[0]
	vmul.f32 d16, d16, d10[0]
	vdup.32 d21, d1[0]
	vmul.f32 d25, d20, d11[0]
	vmul.f32 d27, d22, d7[0]
	vmul.f32 d26, d24, d9[0]
	vmul.f32 d18, d18, d8[0]
	vmul.f32 d28, d19, d6[1]
	vmul.f32 d17, d17, d6[0]
	vadd.f32 d25, d25, d27
	vmul.f32 d27, d16, d7[0]
	vdup.32 d23, d3[0]
	vadd.f32 d25, d25, d26
	vmul.f32 d26, d21, d10[1]
	vmul.f32 d29, d23, d10[1]
	vsub.f32 d25, d25, d27
	vmul.f32 d27, d18, d11[0]
	vsub.f32 d26, d28, d26
	vmul.f32 d28, d17, d9[0]
	vsub.f32 d17, d17, d24
	vmul.f32 d24, d23, d6[1]
	vsub.f32 d25, d25, d27
	vneg.f32 d31, d26
	vmul.f32 d26, d21, d8[1]
	vmul.f32 d27, d19, d8[1]
	vsub.f32 d25, d25, d28
	vmul.f32 d28, d23, d6[0]
	vsub.f32 d0, d24, d26
	vsub.f32 d30, d27, d29
	vmul.f32 d26, d19, d8[0]
	vmul.f32 d23, d23, d10[0]
	vmul.f32 d27, d21, d8[0]
	vmul.f32 d19, d19, d6[0]
	vmul.f32 d21, d21, d10[0]
	vmov.i32 d29, #0
	vsub.f32 d16, d22, d16
	vsub.f32 d23, d26, d23
	vsub.f32 d26, d28, d27
	vrecpe.f32 d27, d25
	vtrn.32 d30, d31
	vsub.f32 d19, d19, d21
	vorr d1, d29, d29
	vneg.f32 d28, d26
	vrecps.f32 d21, d27, d25
	vneg.f32 d22, d23
	vorr d26, d30, d30
	vsub.f32 d30, d20, d18
	vorr d23, d19, d19
	vtrn.32 d0, d1
	vorr d31, d29, d29
	vorr q9, q14, q14
	vmul.f32 d20, d27, d21
	vneg.f32 d17, d17
	vtrn.32 d22, d23
	vorr d27, d0, d0
	vtrn.32 d30, d31
	vtrn.32 d18, d19
	vrecps.f32 d21, d20, d25
	vtrn.32 d16, d17
	vorr d25, d30, d30
	vorr d23, d18, d18
	vmul.f32 d0, d20, d21
	vmov.i32 q9, #0
	vmov.i64 d20, #0xffffffff
	vmov.f32 d18, #1.0
	vorr d24, d16, d16
	vmov.i32 d16, #0
	vbif d27, d19, d20
	vbif d23, d19, d20
	vbif d25, d19, d20
	vshl.i64 d17, d18, #0x20
	vmul.f32 q10, q13, d0[0]
	vmul.f32 q9, q11, d0[0]
	vstmia r0!, {d20, d21}
	vmul.f32 q10, q12, d0[0]
	vstmia r0!, {d18, d19, d20, d21}
	vstmia r0, {d16, d17}
	vpop {s16, s17, s18, s19, s20, s21, s22, s23}
	pop {r4, pc}
	.fnend
	.size ScePafMisc_62EB3B09, . - ScePafMisc_62EB3B09
