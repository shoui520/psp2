/* Original native rectangle transform with directional vector flags. */
	.syntax unified
	.thumb
	.fpu neon
	.text

	.global ScePafMisc_362D3890
	.type ScePafMisc_362D3890, %function
	.thumb_func
ScePafMisc_362D3890:
	.fnstart
	push {r4, lr}
	vldr d18, [r3]
	movs r4, #0
	vmov.f32 d17, #1.0
	vldr s4, [r1]
	vorr d0, d18, d18
	vldr s3, [r1, #4]
	vldr s2, [r1, #8]
	vldr s1, [r1, #12]
	vorr d16, d17, d17
	vmov.f32 s5, s0
	vmov s0, r4
	vcmp.f32 s5, s0
	vmrs apsr_nzcv, fpscr
	bmi .Lneg362x
	b .Ldone362x
.Lneg362x:
	vmov.f32 d17, #-1.0
.Ldone362x:
	vmov.i64 d19, #0xffffffff
	vorr d3, d17, d17
	vbif d3, d18, d19
	vcmp.f32 s7, s0
	vmrs apsr_nzcv, fpscr
	bmi .Lneg362y
	b .Lpos362y
.Lneg362y:
	vmov.f32 d19, #-1.0
	vmov.i64 d18, #0xffffffff
	vorr d16, d17, d17
	vbif d16, d19, d18
	vstr d16, [r3]
	b .Lnext362
.Lpos362y:
	vorr d19, d16, d16
	vmov.i64 d18, #0xffffffff
	vorr d16, d17, d17
	vbif d16, d19, d18
	vstr d16, [r3]
.Lnext362:
	vldr d17, [r2]
	ldr r1, [sp, #8]
	vldr d18, [r1]
	vmov.f32 s5, s3
	vmov.f32 s3, s1
	vmul.f32 d1, d1, d18
	vsub.f32 d17, d2, d17
	vorr d2, d1, d1
	vmul.f32 d17, d17, d18
	vmov.f32 s2, s4
	vmov.f32 s1, s5
	vmul.f32 d2, d17, d16
	vstr s4, [r0]
	vstr s5, [r0, #4]
	vmov.f32 s4, s0
	vcmp.f32 s2, s4
	vmrs apsr_nzcv, fpscr
	bge .Lclamp362x
	b .Lclamped362x
.Lclamp362x:
	vmov.f32 s4, s2
.Lclamped362x:
	vcmp.f32 s1, s0
	vstr s4, [r0, #8]
	vmrs apsr_nzcv, fpscr
	bge .Lclamp362y
	b .Lclamped362y
.Lclamp362y:
	vmov.f32 s0, s1
.Lclamped362y:
	vstr s0, [r0, #12]
	pop {r4, pc}
	.fnend
	.size ScePafMisc_362D3890, . - ScePafMisc_362D3890
