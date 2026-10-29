/* Original guarded wrappers for two pointer-pair record copies. */
	.syntax unified
	.thumb
	.text

	.macro PafGuardedPairCopy name, source_offset
	.global \name
	.type \name, %function
	.thumb_func
\name:
	.fnstart
	push {r4, r5, lr}
	sub sp, #0xc
	movw r5, #:lower16:__stack_chk_guard
	movt r5, #:upper16:__stack_chk_guard
	ldr r2, [r5]
	str r2, [sp, #8]
	adds r4, r0, #0
	add r0, sp, #0
	adds r1, #\source_offset
	bl ScePafMisc_447B094F
	ldrd r0, r1, [sp]
	str r0, [r4]
	str r1, [r4, #4]
	ldr r1, [sp, #8]
	ldr r0, [r5]
	cmp r0, r1
	bne .Lfail\@
	add sp, #0xc
	pop {r4, r5, pc}
.Lfail\@:
	blx __stack_chk_fail
	nop
	.fnend
	.size \name, . - \name
	.endm

	PafGuardedPairCopy ScePafMisc_D50E863E, 4
	PafGuardedPairCopy ScePafMisc_5F20E13E, 24
