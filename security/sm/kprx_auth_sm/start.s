/*
 * start.s: MeP entry point (_start), global constructor/destructor runners,
 * and stack-smashing failure handler for kprx_auth_sm.self (0x80b000..0x80b09c).
 */

	.section .text.entry, "ax", @progbits
	.global _start
	.global call_ctors
	.global call_dtors
	.global __stack_chk_fail
	.global __stack_chk_fail_local

/* 0x80b000 */
_start:
	movh	$sp, %hi(__stack_top)
	add3	$sp, $sp, %lo(__stack_top)
	movh	$gp, %hi(__gp)
	add3	$gp, $gp, %lo(__gp)
	add	$sp, -16
	sw	$1, ($sp)
	sw	$2, 0x4($sp)
	sw	$3, 0x8($sp)
	sw	$4, 0xc($sp)
	movh	$1, 0xe005
	or3	$1, $1, 0x3c
	movu	$2, __stack_chk_guard
	lw	$3, ($1)
	sw	$3, ($2)
	bsr	call_ctors
	lw	$4, 0xc($sp)
	lw	$3, 0x8($sp)
	lw	$2, 0x4($sp)
	lw	$1, ($sp)
	add	$sp, 16
	bsr	sm_main
	add	$sp, -4
	sw	$0, ($sp)
	bsr	call_dtors
	mov	$4, 1
	lw	$1, ($sp)
	add	$sp, 4
	swi	0x0
	nop
.Lsleep_loop:
	sleep
	bra	.Lsleep_loop

	.section .text.ctors, "ax", @progbits
/* 0x80b052 */
call_ctors:
	add	$sp, -28
	ldc	$1, $lp
	sw	$1, ($sp)
	movu	$5, __ctors_start
	movu	$6, __ctors_end
.Lctor_loop:
	beq	$5, $6, .Lctor_done
	lw	$0, ($5)
	jsr	$0
	add	$5, 4
	bra	.Lctor_loop
.Lctor_done:
	lw	$1, ($sp)
	add	$sp, 28
	jmp	$1

/* 0x80b072 */
call_dtors:
	add	$sp, -28
	ldc	$1, $lp
	sw	$1, ($sp)
	movu	$5, __dtors_start
	movu	$6, __dtors_end
.Ldtor_loop:
	beq	$5, $6, .Ldtor_done
	lw	$0, ($5)
	jsr	$0
	add	$5, 4
	bra	.Ldtor_loop
.Ldtor_done:
	lw	$1, ($sp)
	add	$sp, 28
	jmp	$1

/* 0x80b092 */
__stack_chk_fail:
__stack_chk_fail_local:
	ldc	$1, $lp
	bsr	smSyscall7
.Lpanic_hang:
	jmp	.Lpanic_hang

	.section .data.guard, "aw", @progbits
	.global __stack_chk_guard
	.balign 4
/* 0x810910 */
__stack_chk_guard:
	.long	0x00000000
