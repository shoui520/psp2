/* Native affine viewport scaling with original fused VFP arithmetic. */
	.syntax unified
	.thumb
	.fpu neon
	.text

	.global ScePafMisc_8424A840
	.type ScePafMisc_8424A840, %function
	.thumb_func
ScePafMisc_8424A840:
	.fnstart
	movw r2, #:lower16:sce_paf_misc_float_112c
	vmov.f32 s2, #0.5
	movt r2, #:upper16:sce_paf_misc_float_112c
	vldr s3, [r2]
	movw r2, #:lower16:sce_paf_misc_float_1130
	movt r2, #:upper16:sce_paf_misc_float_1130
	vldr s4, [r2]
	movw r2, #:lower16:sce_paf_misc_float_1144
	vmls.f32 s0, s3, s2
	movt r2, #:upper16:sce_paf_misc_float_1144
	vldr s3, [r2]
	movw r2, #:lower16:sce_paf_misc_float_1148
	vnmls.f32 s1, s4, s2
	movt r2, #:upper16:sce_paf_misc_float_1148
	vldr s2, [r2]
	vmul.f32 s0, s0, s3
	vmul.f32 s1, s1, s2
	vstr s0, [r0]
	vstr s1, [r1]
	bx lr
	.fnend
	.size ScePafMisc_8424A840, . - ScePafMisc_8424A840

	.global ScePafMisc_9C8CBB11
	.type ScePafMisc_9C8CBB11, %function
	.thumb_func
ScePafMisc_9C8CBB11:
	.fnstart
	movw r2, #:lower16:sce_paf_misc_float_112c
	vmov.f32 s2, #0.5
	movt r2, #:upper16:sce_paf_misc_float_112c
	vldr s3, [r2]
	movw r2, #:lower16:sce_paf_misc_float_1150
	movt r2, #:upper16:sce_paf_misc_float_1150
	vldr s4, [r2]
	movw r2, #:lower16:sce_paf_misc_float_114c
	vmul.f32 s3, s3, s2
	movt r2, #:upper16:sce_paf_misc_float_114c
	vldr s5, [r2]
	movw r2, #:lower16:sce_paf_misc_float_1130
	vmul.f32 s1, s4, s1
	movt r2, #:upper16:sce_paf_misc_float_1130
	vldr s4, [r2]
	vmla.f32 s3, s5, s0
	vnmls.f32 s1, s4, s2
	vstr s3, [r0]
	vstr s1, [r1]
	bx lr
	.fnend
	.size ScePafMisc_9C8CBB11, . - ScePafMisc_9C8CBB11
