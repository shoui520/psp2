/* Original native NEON matrix-frame orthonormalization transform. */
	.syntax unified
	.thumb
	.fpu neon
	.text
	.global ScePafMisc_077B31E6
	.type ScePafMisc_077B31E6, %function
	.thumb_func
ScePafMisc_077B31E6:
	.fnstart
	vldmia r1, {d16, d17}
	vmov.i32 d20, #0x800000
	vmul.f32 q9, q8, q8
	vpadd.f32 d18, d18, d19
	vmov.f32 d19, #1.0
	vorr d22, d19, d19
	vpadd.f32 d18, d18, d18
	movs.w r2, #0x7f800000
	vcgt.f32 d21, d18, d20
	vbit d22, d18, d21
	vrsqrte.f32 d18, d22
	vdup.32 d24, r2
	vcle.f32 d26, d22, #0
	vmul.f32 d23, d18, d18
	vceq.i32 d25, d22, d24
	vrsqrts.f32 d23, d23, d22
	vorr d25, d26, d25
	vmul.f32 d18, d18, d23
	vmul.f32 d27, d18, d18
	vclt.f32 d23, d22, #0
	vrsqrts.f32 d26, d27, d22
	vmul.f32 d18, d18, d26
	vmul.f32 d18, d22, d18
	vbsl d25, d22, d18
	vorr d18, d23, d25
	vand d18, d18, d21
	vrecpe.f32 d21, d18
	adds.w r2, r1, #0x10
	vrecps.f32 d22, d21, d18
	vmul.f32 d21, d21, d22
	vrecps.f32 d18, d21, d18
	vmul.f32 d0, d21, d18
	vmul.f32 q8, q8, d0[0]
	vstmia r0, {d16, d17}
	vldmia r2, {d22, d23}
	vext.32 d21, d16, d17, #1
	vsli.64 d17, d16, #0x20
	vorr d18, d23, d23
	vext.32 d26, d22, d23, #1
	vext.64 q14, q10, q8, #1
	vorr d16, d17, d17
	vsli.64 d18, d22, #0x20
	vorr d31, d26, d26
	vorr d27, d22, d22
	vorr d17, d21, d21
	vmov.i32 q11, #0
	vorr d30, d18, d18
	vmov.i64 d21, #0xffffffff
	vorr d18, d23, d23
	vmul.f32 q15, q14, q15
	vmls.f32 q15, q8, q13
	vbif d31, d18, d21
	vmul.f32 q11, q15, q15
	vpadd.f32 d22, d22, d23
	vpadd.f32 d22, d22, d22
	vcgt.f32 d20, d22, d20
	vbif d22, d19, d20
	vrsqrte.f32 d19, d22
	vceq.i32 d24, d22, d24
	vcle.f32 d25, d22, #0
	vmul.f32 d23, d19, d19
	vorr d24, d25, d24
	vrsqrts.f32 d23, d23, d22
	vmul.f32 d19, d19, d23
	vmul.f32 d26, d19, d19
	vclt.f32 d23, d22, #0
	vrsqrts.f32 d25, d26, d22
	vmul.f32 d19, d19, d25
	vmul.f32 d19, d22, d19
	vbsl d24, d22, d19
	vorr d19, d23, d24
	vand d19, d19, d20
	vrecpe.f32 d20, d19
	vrecps.f32 d22, d20, d19
	vmul.f32 d20, d20, d22
	vrecps.f32 d19, d20, d19
	vmul.f32 d0, d20, d19
	vmul.f32 q11, q15, d0[0]
	vorr d25, d23, d23
	vorr d31, d22, d22
	vext.32 d27, d22, d25, #1
	vsli.64 d25, d22, #0x20
	vorr d30, d27, d27
	vorr d24, d25, d25
	vorr d25, d27, d27
	adds.w r2, r0, #0x10
	vmul.f32 q8, q15, q8
	vstr d22, [r0, #0x20]
	vstr d23, [r0, #0x28]
	vmls.f32 q8, q12, q14
	vbif d17, d18, d21
	vstmia r2, {d16, d17}
	vldr d16, [r1, #0x30]
	vldr d17, [r1, #0x38]
	vstr d16, [r0, #0x30]
	vstr d17, [r0, #0x38]
	bx lr
	.fnend
	.size ScePafMisc_077B31E6, . - ScePafMisc_077B31E6
