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

/* Pair of guarded two-coordinate conditional insertion routines. */
	.macro PafGuardedCoordinateCopy name, source_offset, dst0, dst1
	.global \name
	.type \name, %function
	.thumb_func
\name:
	.fnstart
	push {r4, r5, r6, r7, lr}
	sub sp, #0xc
	movw r7, #:lower16:__stack_chk_guard
	movt r7, #:upper16:__stack_chk_guard
	ldr r2, [r7]
	str r2, [sp, #8]
	adds r4, r1, #0
	ldr r1, [r4]
	adds r5, r0, #0
	cmp r1, #0
	mov.w r6, #0
	blt .Lzero\@
	ldr r0, [r4, #4]
	cmp r0, #0
	blt .Lzero\@
	add r0, sp, #0
	.if \source_offset == 24
	adds.w r1, r5, #0x18
	.else
	adds r1, r5, #4
	.endif
	bl ScePafMisc_447B094F
	ldr r0, [sp]
	ldr r3, [r4]
	cmp r3, r0
	ldr r2, [sp, #4]
	bge .Lzero\@
	ldr r0, [r4, #4]
	cmp r0, r2
	bge .Lzero\@
	str r3, [r5, #\dst0]
	movs r0, #1
	ldr r1, [r4, #4]
	str r1, [r5, #\dst1]
	b .Lverify\@
.Lzero\@:
	adds r0, r6, #0
.Lverify\@:
	ldr r2, [sp, #8]
	ldr r1, [r7]
	cmp r1, r2
	bne .Lfail\@
	add sp, #0xc
	pop {r4, r5, r6, r7, pc}
.Lfail\@:
	blx __stack_chk_fail
	nop
	.fnend
	.size \name, . - \name
	.endm

	PafGuardedCoordinateCopy ScePafMisc_AFAF03B3, 24, 56, 60
	PafGuardedCoordinateCopy ScePafMisc_FBE1B9F7, 4, 48, 52
