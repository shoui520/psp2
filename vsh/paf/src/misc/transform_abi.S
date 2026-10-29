/* Native homogeneous transform constructors with original VFP ordering. */
	.syntax unified
	.thumb
	.fpu neon
	.text
	.global ScePafMisc_1534ACB0
	.type ScePafMisc_1534ACB0, %function
	.thumb_func
ScePafMisc_1534ACB0:
	.fnstart
	push {r4, lr}
	vpush {s16, s17, s18, s19}
	vsub.f32 s6, s1, s0
	vmov.f32 s7, #1.0
	vsub.f32 s8, s3, s2
	vsub.f32 s9, s4, s5
	vadd.f32 s0, s0, s1
	vadd.f32 s1, s2, s3
	vadd.f32 s2, s5, s4
	vdiv.f32 s4, s7, s6
	vdiv.f32 s5, s7, s8
	vdiv.f32 s6, s7, s9
	vmov.f32 s7, #2.0
	vmov.f32 s3, #1.0
	adds.w r1, r0, #0x10
	vnmul.f32 s0, s0, s4
	vnmul.f32 s1, s1, s5
	vmul.f32 s2, s2, s6
	vmul.f32 s4, s4, s7
	vmul.f32 s5, s5, s7
	vmul.f32 s6, s6, s7
	adds.w r2, r1, #0x10
	adds.w r3, r2, #0x10
	vmov.i32 q2, #0
	vmov.i32 q3, #0
	vmov.i32 q4, #0
	vmov.f32 s8, s4
	vmov.f32 s13, s5
	vmov.f32 s18, s6
	vstmia r0, {d4, d5}
	vstmia r1, {d6, d7}
	vstmia r2, {d8, d9}
	vstmia r3, {d0, d1}
	vpop {s16, s17, s18, s19}
	pop {r4, pc}
	.fnend
	.size ScePafMisc_1534ACB0, . - ScePafMisc_1534ACB0
