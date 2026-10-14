/*
 * What C cannot express: a return to a point saved, as setjmp and longjmp.
 * The engines save one before parsing a bitstream and jump back to it on an
 * error found deep inside. The buffer holds $lp, $5-$8, $tp, $gp and $sp.
 */

	.text
	.global	videoSetJump
	.global	videoLongJump
	.p2align 1

/* int32_t videoSetJump(uint32_t *buffer): 0 now, the value given later */
videoSetJump:
	ldc	$4, $lp
	sw	$4, ($1)
	sw	$5, 4($1)
	sw	$6, 8($1)
	sw	$7, 12($1)
	sw	$8, 16($1)
	sw	$tp, 20($1)
	sw	$gp, 24($1)
	sw	$sp, 28($1)
	mov	$0, 0
	ret

/* void videoLongJump(uint32_t *buffer, int32_t value): value, or 1 for 0 */
videoLongJump:
	mov	$0, 1
	beqz	$2, 1f
	mov	$0, $2
1:	lw	$4, ($1)
	lw	$5, 4($1)
	lw	$6, 8($1)
	lw	$7, 12($1)
	lw	$8, 16($1)
	stc	$4, $lp
	lw	$tp, 20($1)
	lw	$gp, 24($1)
	lw	$sp, 28($1)
	ret
