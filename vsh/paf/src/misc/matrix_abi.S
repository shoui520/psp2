/* Original 4x4 single-precision NEON transpose. */
	.syntax unified
	.thumb
	.fpu neon
	.text
	.global ScePafMisc_34EF5545
	.type ScePafMisc_34EF5545, %function
	.thumb_func
ScePafMisc_34EF5545:
	.fnstart
	vldmia r1, {d16-d23}
	vorr q0, q8, q8
	vorr q1, q9, q9
	adds.w r1, r0, #0x10
	vorr q8, q10, q10
	vorr q9, q11, q11
	adds.w r2, r1, #0x10
	vtrn.32 q0, q1
	adds.w r3, r2, #0x10
	vtrn.32 q8, q9
	vorr d20, d0, d0
	vorr d21, d16, d16
	vorr d22, d2, d2
	vorr d23, d18, d18
	vorr d16, d1, d1
	vorr d18, d3, d3
	vstmia r0, {d20, d21}
	vstmia r1, {d22, d23}
	vstmia r2, {d16, d17}
	vstmia r3, {d18, d19}
	bx lr
	.fnend
	.size ScePafMisc_34EF5545, . - ScePafMisc_34EF5545
