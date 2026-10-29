/* Native NEON matrix operations retained in original register order. */
	.syntax unified
	.thumb
	.fpu neon
	.text
	.global ScePafMisc_EE135486
	.type ScePafMisc_EE135486, %function
	.thumb_func
ScePafMisc_EE135486:
	.fnstart
	vldmia r1, {d16, d17}
	vldmia r2, {d0, d1}
	vmul.f32 q8, q8, q0
	vorr d18, d1, d1
	vpadd.f32 d16, d16, d16
	vadd.f32 d16, d16, d17
	vdup.32 d16, d16[0]
	vneg.f32 d2, d16
	vorr d20, d0, d0
	vldmia r3, {d16, d17}
	vorr d19, d2, d2
	vtrn.32 d18, d19
	vorr d21, d18, d18
	vmul.f32 q11, q8, d0[0]
	vmul.f32 q9, q8, q10
	vmov.i32 d20, #0
	vmul.f32 q12, q8, d0[1]
	vmul.f32 q13, q8, d1[0]
	vpadd.f32 d18, d18, d19
	vpadd.f32 d18, d18, d18
	vorr d29, d20, d20
	vmul.f32 q8, q8, d2[0]
	vorr d21, d18, d18
	vsli.64 d29, d18, #0x20
	adds.w r1, r0, #0x10
	vsli.64 d21, d20, #0x20
	adds.w r2, r1, #0x10
	adds.w r3, r2, #0x10
	vext.64 q9, q10, q10, #1
	vext.64 q15, q14, q10, #1
	vsub.f32 q9, q9, q11
	vorr q11, q10, q10
	vorr d28, d20, d20
	vsub.f32 q10, q15, q12
	vsub.f32 q11, q11, q13
	vsub.f32 q8, q14, q8
	vstmia r0, {d18, d19}
	vstmia r1, {d20, d21}
	vstmia r2, {d22, d23}
	vstmia r3, {d16, d17}
	bx lr
	.fnend
	.size ScePafMisc_EE135486, . - ScePafMisc_EE135486

	.global ScePafMisc_293092F5
	.type ScePafMisc_293092F5, %function
	.thumb_func
ScePafMisc_293092F5:
	.fnstart
	vldmia r1!, {d16, d17}
	vmov.i32 q11, #0
	vldmia r1!, {d18, d19, d20, d21}
	vorr d26, d16, d16
	vorr d31, d23, d23
	vorr d27, d16, d16
	vorr d28, d20, d20
	vorr d29, d20, d20
	vsli.64 d26, d18, #0x20
	vsri.64 d18, d16, #0x20
	vorr d16, d21, d21
	vsri.64 d31, d20, #0x20
	vsli.64 d28, d22, #0x20
	vsri.64 d22, d20, #0x20
	vmov.i64 d24, #0xffffffff
	vorr d20, d17, d17
	vorr d0, d18, d18
	vorr d30, d22, d22
	vbif d29, d23, d24
	vbif d21, d23, d24
	vsli.64 d20, d19, #0x20
	vbif d27, d19, d24
	vmov.f32 d22, #1.0
	vorr d18, d26, d26
	vorr d19, d29, d29
	vorr d17, d21, d21
	vorr d16, d20, d20
	vorr d20, d0, d0
	vorr d21, d31, d31
	adds.w r2, r0, #0x10
	vmov.i32 d24, #0
	vshl.i64 d25, d22, #0x20
	adds.w r3, r2, #0x10
	vstmia r0, {d18, d19}
	vstmia r2, {d20, d21}
	adds.w r2, r3, #0x10
	vstmia r3, {d16, d17}
	vstmia r2, {d24, d25}
	vldmia r1, {d22, d23}
	vmov.f32 q13, #1.0
	vneg.f32 q0, q11
	vmul.f32 q9, q9, d0[0]
	vmul.f32 q10, q10, d0[1]
	vmla.f32 q9, q8, d1[0]
	vmla.f32 q10, q12, q13
	vadd.f32 q8, q9, q10
	vstr d16, [r0, #0x30]
	vstr d17, [r0, #0x38]
	bx lr
	.fnend
	.size ScePafMisc_293092F5, . - ScePafMisc_293092F5
