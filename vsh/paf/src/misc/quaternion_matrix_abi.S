/* Native quaternion-to-4x4 matrix conversion with its zero-norm path. */
	.syntax unified
	.thumb
	.fpu neon
	.text
	.global ScePafMisc_2921E1B2
	.type ScePafMisc_2921E1B2, %function
	.thumb_func
ScePafMisc_2921E1B2:
	.fnstart
	push {r4, r5, r6, lr}
	vpush {s16, s17, s18, s19}
	vldmia r1, {d18, d19}
	vmul.f32 q8, q9, q9
	vpadd.f32 d16, d16, d17
	vpadd.f32 d0, d16, d16
	vmov.f32 d16, #1.0
	vmov.i32 d17, #0
	vcmp.f32 s0, #0
	vshl.i64 d16, d16, #0x20
	vmrs apsr_nzcv, fpscr
	vext.64 q8, q8, q8, #1
	beq .Lidentity292
	b .Lconvert292
.Lidentity292:
	vmov.f32 d18, #1.0
	vmov.f32 d20, #1.0
	vmov.f32 d21, #1.0
	vmov.i32 d19, #0
	vmov.i32 d25, #0
	adds.w r1, r0, #0x10
	adds.w r2, r1, #0x10
	vshr.u64 d18, d18, #0x20
	vshl.i64 d24, d20, #0x20
	vmov.i32 d20, #0
	vshr.u64 d21, d21, #0x20
	vstmia r0, {d18, d19}
	vstmia r1, {d24, d25}
	adds.w r0, r2, #0x10
	vstmia r2, {d20, d21}
	vstmia r0, {d16, d17}
	b .Lfinish292
.Lconvert292:
	vmov.f32 s1, #2.0
	movs r1, #0
	vdiv.f32 s0, s1, s0
	movs r2, #0
	vmov s19, r2
	movs r3, #0
	adds.w r4, r0, #0x10
	adds.w r5, r4, #0x10
	vdup.32 d2, d18[1]
	vdup.32 d1, d18[0]
	vdup.32 d3, d19[0]
	vdup.32 d4, d19[1]
	vmul.f32 s3, s4, s0
	vmul.f32 s1, s2, s0
	vmul.f32 s0, s6, s0
	vmov.f32 s9, #1.0
	vmul.f32 s7, s4, s3
	vmul.f32 s5, s2, s1
	vmul.f32 s2, s2, s3
	vmul.f32 s10, s8, s1
	vmul.f32 s3, s8, s3
	vmul.f32 s8, s8, s0
	vmul.f32 s4, s4, s0
	vmul.f32 s1, s6, s1
	vmul.f32 s11, s6, s0
	vsub.f32 s12, s9, s7
	vsub.f32 s5, s9, s5
	vmov s9, r1
	vadd.f32 s13, s8, s2
	vadd.f32 s14, s10, s4
	vsub.f32 s15, s1, s3
	vsub.f32 s2, s2, s8
	vmls.f32 s12, s6, s0
	vadd.f32 s0, s3, s1
	vsub.f32 s1, s4, s10
	vsub.f32 s4, s5, s7
	vsub.f32 s16, s5, s11
	vmov s5, r3
	vext.32 d3, d4, d4, #1
	vext.32 d4, d1, d1, #1
	vext.32 d5, d9, d7, #1
	vorr d18, d6, d6
	vorr d1, d2, d2
	vext.32 d19, d7, d3, #1
	vext.32 d4, d4, d8, #1
	vext.32 d5, d5, d5, #1
	vstmia r0, {d18, d19}
	adds.w r0, r5, #0x10
	vstmia r4, {d4, d5}
	vstmia r5, {d0, d1}
	vstmia r0, {d16, d17}
.Lfinish292:
	vpop {s16, s17, s18, s19}
	pop {r4, r5, r6, pc}
	.fnend
	.size ScePafMisc_2921E1B2, . - ScePafMisc_2921E1B2
