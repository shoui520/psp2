/* Original signed-FP rectangle containment predicate. */
	.syntax unified
	.thumb
	.fpu neon
	.text
	.global ScePafMisc_9DF81658
	.type ScePafMisc_9DF81658, %function
	.thumb_func
ScePafMisc_9DF81658:
	.fnstart
	vmov.f32 s0, #0.5
	vldr s3, [r0, #8]
	vldr s1, [r1, #8]
	vldr s4, [r0]
	vldr s2, [r1]
	vmul.f32 s3, s3, s0
	vmul.f32 s1, s1, s0
	vsub.f32 s5, s4, s3
	vsub.f32 s6, s2, s1
	vcmp.f32 s5, s6
	vmrs apsr_nzcv, fpscr
	bmi .LfalseFirst
	b .LcheckSecond
.LfalseFirst:
	movs r0, #0
	b .Lreturn
.LcheckSecond:
	vadd.f32 s3, s4, s3
	vadd.f32 s1, s2, s1
	vcmp.f32 s3, s1
	vmrs apsr_nzcv, fpscr
	bgt .LfalseSecond
	b .LcheckThird
.LfalseSecond:
	movs r0, #0
	b .Lreturn
.LcheckThird:
	vldr s2, [r0, #0xc]
	vldr s4, [r1, #0xc]
	vldr s3, [r0, #4]
	vldr s1, [r1, #4]
	vmul.f32 s2, s2, s0
	vmul.f32 s0, s4, s0
	vadd.f32 s4, s3, s2
	vadd.f32 s5, s1, s0
	vcmp.f32 s4, s5
	vmrs apsr_nzcv, fpscr
	bgt .LfalseThird
	b .LcheckFourth
.LfalseThird:
	movs r0, #0
	b .Lreturn
.LcheckFourth:
	vsub.f32 s2, s3, s2
	vsub.f32 s0, s1, s0
	vcmp.f32 s2, s0
	vmrs apsr_nzcv, fpscr
	bmi .LfalseFourth
	b .Ltrue
.LfalseFourth:
	movs r0, #0
	b .Lreturn
.Ltrue:
	movs r0, #1
.Lreturn:
	bx lr
	.fnend
	.size ScePafMisc_9DF81658, . - ScePafMisc_9DF81658
