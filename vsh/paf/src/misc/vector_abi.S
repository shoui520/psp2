/* Original four-float NEON reciprocal-norm transform. */
	.syntax unified
	.thumb
	.fpu neon
	.text
	.global ScePafMisc_28D57B83
	.type ScePafMisc_28D57B83, %function
	.thumb_func
ScePafMisc_28D57B83:
	.fnstart
	vldmia r1, {d16, d17}
	vmul.f32 q9, q8, q8
	vpadd.f32 d18, d18, d19
	vpadd.f32 d18, d18, d18
	vdup.32 q9, d18[0]
	vrecpe.f32 q10, q9
	vrecps.f32 q11, q10, q9
	vmul.f32 q10, q10, q11
	vmov.i64 d22, #0xffffffff
	vrecps.f32 q9, q10, q9
	vmul.f32 q9, q10, q9
	vmul.f32 q8, q8, q9
	vneg.f32 q9, q8
	vbif d19, d17, d22
	vstmia r0, {d18, d19}
	bx lr
	.fnend
	.size ScePafMisc_28D57B83, . - ScePafMisc_28D57B83
