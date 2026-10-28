/*
 * Floating precision can expose untouched bytes in the conversion area.
 * Keep that area at entry SP - 176; C cannot specify its stack location.
 * The extra eight bytes below it carry the C body's fifth argument.
 */
	.syntax	unified
	.thumb
	.fpu	neon
	.text

	.global	ScePafStdc_AE0F3820
	.type	ScePafStdc_AE0F3820, %function
	.thumb_func
ScePafStdc_AE0F3820:
	.fnstart
	.save	{r4-r11, lr}
	push.w	{r4-r11, lr}
	.vsave	{d8}
	vpush	{s16-s17}
	.pad	#0x8c
	sub	sp, sp, #0x8c
	movw	r4, #:lower16:__stack_chk_guard
	movt	r4, #:upper16:__stack_chk_guard
	ldr	r4, [r4]
	str	r4, [sp, #0x84]
	add	r4, sp, #8
	str	r4, [sp]
	bl	PafFormatBody
	ldr	r1, [sp, #0x84]
	movw	r2, #:lower16:__stack_chk_guard
	movt	r2, #:upper16:__stack_chk_guard
	ldr	r2, [r2]
	cmp	r2, r1
	bne	.LformatStackFail
	add	sp, sp, #0x8c
	vpop	{s16-s17}
	pop.w	{r4-r11, pc}
.LformatStackFail:
	bl	__stack_chk_fail
	.fnend
	.size	ScePafStdc_AE0F3820, . - ScePafStdc_AE0F3820
