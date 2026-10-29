/* Native two-vector rectangle bounds conversion ABI. */
	.syntax unified
	.thumb
	.fpu neon
	.text

	.global ScePafMisc_3D34B4D1
	.type ScePafMisc_3D34B4D1, %function
	.thumb_func
ScePafMisc_3D34B4D1:
	.fnstart
	vmov.f32 s2, #0.5
	vldr s0, [r1, #8]
	vldr s1, [r1, #12]
	vldr s3, [r1]
	vldr s4, [r1, #4]
	vmul.f32 s1, s1, s2
	vmul.f32 s0, s0, s2
	vadd.f32 s2, s4, s1
	vsub.f32 s8, s3, s0
	vsub.f32 s4, s4, s1
	vadd.f32 s5, s3, s0
	vext.32 d0, d1, d1, #1
	vorr d1, d2, d2
	vext.32 d0, d0, d4, #1
	vstmia r0, {d0, d1}
	bx lr
	.fnend
	.size ScePafMisc_3D34B4D1, . - ScePafMisc_3D34B4D1

	.global ScePafMisc_7A4AAFF2
	.type ScePafMisc_7A4AAFF2, %function
	.thumb_func
ScePafMisc_7A4AAFF2:
	.fnstart
	vmov.f32 s1, #0.5
	vldr s0, [r1, #8]
	vldr s2, [r1, #12]
	vldr s3, [r1]
	vldr s4, [r1, #4]
	vmul.f32 s0, s0, s1
	vmul.f32 s1, s2, s1
	vsub.f32 s2, s3, s0
	vadd.f32 s8, s4, s1
	vsub.f32 s1, s4, s1
	vadd.f32 s0, s3, s0
	vext.32 d2, d1, d1, #1
	vorr d3, d0, d0
	vext.32 d2, d2, d4, #1
	vstmia r0, {d2, d3}
	bx lr
	.fnend
	.size ScePafMisc_7A4AAFF2, . - ScePafMisc_7A4AAFF2
