/*
 * Floating precision can expose untouched words in the conversion area.
 * Keep that area at entry SP - 216; C cannot specify its stack location.
 * The extra eight bytes below it carry the C body's fifth argument.
 */
	.syntax	unified
	.thumb
	.fpu	neon
	.text

	.global	ScePafStdc_AFB23563
	.type	ScePafStdc_AFB23563, %function
	.thumb_func
ScePafStdc_AFB23563:
	.fnstart
	.save	{r4-r11, lr}
	push.w	{r4-r11, lr}
	.vsave	{d8}
	vpush	{s16-s17}
	.pad	#0xb4
	sub	sp, sp, #0xb4
	movw	r4, #:lower16:__stack_chk_guard
	movt	r4, #:upper16:__stack_chk_guard
	ldr	r4, [r4]
	str	r4, [sp, #0xac]
	add	r4, sp, #8
	str	r4, [sp]
	bl	PafWideFormatBody
	ldr	r1, [sp, #0xac]
	movw	r2, #:lower16:__stack_chk_guard
	movt	r2, #:upper16:__stack_chk_guard
	ldr	r2, [r2]
	cmp	r2, r1
	bne	.LwideFormatStackFail
	add	sp, sp, #0xb4
	vpop	{s16-s17}
	pop.w	{r4-r11, pc}
.LwideFormatStackFail:
	bl	__stack_chk_fail
	.fnend
	.size	ScePafStdc_AFB23563, . - ScePafStdc_AFB23563
