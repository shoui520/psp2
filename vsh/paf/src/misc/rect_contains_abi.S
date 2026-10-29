/* Native floating-point point-in-rectangle tests, preserving VFP flags. */
	.syntax unified
	.thumb
	.fpu neon
	.text

	.global ScePafMisc_FFB654B3
	.type ScePafMisc_FFB654B3, %function
	.thumb_func
ScePafMisc_FFB654B3:
	.fnstart
	vmov.f32 s2, #0.5
	vldr s3, [r0, #8]
	vldr s4, [r0]
	movs r1, #0
	vmul.f32 s3, s3, s2
	vsub.f32 s5, s4, s3
	vcmp.f32 s5, s0
	vmrs apsr_nzcv, fpscr
	bls .LrightFFB
	b .LreturnFFB
.LrightFFB:
	vadd.f32 s3, s4, s3
	vcmp.f32 s0, s3
	vmrs apsr_nzcv, fpscr
	bls .LtopFFB
	b .LreturnFFB
.LtopFFB:
	vldr s3, [r0, #0xc]
	vldr s0, [r0, #4]
	vmul.f32 s2, s3, s2
	vadd.f32 s3, s0, s2
	vcmp.f32 s3, s1
	vmrs apsr_nzcv, fpscr
	bge .LbottomFFB
	b .LreturnFFB
.LbottomFFB:
	vsub.f32 s0, s0, s2
	vcmp.f32 s1, s0
	vmrs apsr_nzcv, fpscr
	bge .LfoundFFB
	b .LreturnFFB
.LfoundFFB:
	movs r1, #1
.LreturnFFB:
	adds r0, r1, #0
	bx lr
	.fnend
	.size ScePafMisc_FFB654B3, . - ScePafMisc_FFB654B3

	.global ScePafMisc_BC570EAF
	.type ScePafMisc_BC570EAF, %function
	.thumb_func
ScePafMisc_BC570EAF:
	.fnstart
	vldr d0, [r1]
	vldr s2, [r0, #8]
	vldr s3, [r0]
	vmov.f32 s4, s0
	vmov.f32 s0, #0.5
	movs r1, #0
	vmul.f32 s2, s2, s0
	vsub.f32 s5, s3, s2
	vcmp.f32 s5, s4
	vmrs apsr_nzcv, fpscr
	bls .LrightBC5
	b .LreturnBC5
.LrightBC5:
	vadd.f32 s2, s3, s2
	vcmp.f32 s4, s2
	vmrs apsr_nzcv, fpscr
	bls .LtopBC5
	b .LreturnBC5
.LtopBC5:
	vldr s3, [r0, #0xc]
	vldr s2, [r0, #4]
	vmul.f32 s0, s3, s0
	vadd.f32 s3, s2, s0
	vcmp.f32 s3, s1
	vmrs apsr_nzcv, fpscr
	bge .LbottomBC5
	b .LreturnBC5
.LbottomBC5:
	vsub.f32 s0, s2, s0
	vcmp.f32 s1, s0
	vmrs apsr_nzcv, fpscr
	bge .LfoundBC5
	b .LreturnBC5
.LfoundBC5:
	movs r1, #1
.LreturnBC5:
	adds r0, r1, #0
	bx lr
	.fnend
	.size ScePafMisc_BC570EAF, . - ScePafMisc_BC570EAF
