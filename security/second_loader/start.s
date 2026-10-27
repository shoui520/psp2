/*
 * second_loader entry, exception vectors, Bigmac DMA memory fill helper,
 * C runtime startup, and exit trampolines (0x800000..0x8003df).
 *
 * Symbol names here are ours (descriptive); Sony's names are unknown.
 */

	.section .vectors, "ax"
	.global	vectors
vectors:
	jmp	0x40180
	nop
	reti
	.rept	42
	reti
	nop
	.endr

	/*
	 * Copied to 0x5ff00 on boot failure (g_bootExitStatus != 0): triggers Bigmac
	 * channel 0 to wipe second_loader's SRAM (0x800100..0x81ff00), waits for
	 * completion, clears registers, and halts the Cmep.
	 */
	.section .trampoline0, "ax"
	.global	trampoline_fatal_sleep
trampoline_fatal_sleep:
	mov	$0, 1
	sw	$0, ($1)
1:
	lw	$0, ($2)
	and3	$0, $0, 1
	bnez	$0, 1b
	xor	$0, $0
	xor	$1, $1
	xor	$2, $2
2:
	sleep
	bra	2b

	/*
	 * Copied to 0x5ff00 on successful boot (g_bootExitStatus == 0): triggers
	 * Bigmac channel 0 to wipe second_loader's SRAM (0x800100..0x81ff00),
	 * releases PERVASIVE_RESET->arm (0xe3101000) and, on cold boot ($5 == 1),
	 * PERVASIVE_RESET->armDebugger (0xe3101004), writes 0x700 to
	 * CMEP_MAILBOX->cry2Arm[0] (0xe0000000), clears registers, and sleeps.
	 */
	.section .trampoline1, "ax"
	.global	trampoline_arm_boot
trampoline_arm_boot:
	mov	$0, 1
	sw	$0, ($1)
1:
	lw	$0, ($2)
	and3	$0, $0, 1
	bnez	$0, 1b
	xor	$1, $1
	mov	$0, 0
	sw	$0, ($gp)
2:
	lw	$2, ($gp)
	bnez	$2, 2b
	bnei	$5, 1, 4f
	sw	$0, ($sp)
3:
	lw	$2, ($sp)
	bnez	$2, 3b
4:
	xor	$0, $0
	xor	$2, $2
	xor	$5, $5
	xor	$gp, $gp
	xor	$sp, $sp
	sw	$4, ($3)
	xor	$3, $3
	xor	$4, $4
5:
	sleep
	bra	5b

	/*
	 * Reset entry at 0x40180 (aliased to 0x800180): overwrites the reset vector
	 * at 0x40000 with `sleep` (0x0000b000), initializes the Cmep instruction
	 * cache tag RAM at 0x310000 if present in $ccfg, enables the I-cache in
	 * $cfg, calls earlyInitHook, and jumps to _start.
	 */
	.section .reset, "ax"
	.global	reset_entry
reset_entry:
	di
	mov	$1, 0
	or3	$1, $1, 0xb000
	sw	$1, (0x40000)
	ldc	$3, $ccfg
	srl	$3, 16
	and3	$3, $3, 0x7f
	beqz	$3, 2f
	ldc	$2, $ccfg
	srl	$2, 24
	and3	$2, $2, 7
	blti	$2, 2, 2f
	bgei	$2, 5, 2f
	add	$2, 3
	mov	$0, 10
	sub	$0, $2
	sll	$3, $0
	mov	$4, 1
	sll	$4, $2
	ldc	$2, $cfg
	or3	$2, $2, 2
	stc	$2, $cfg
	nop
	mov	$0, 0
	add	$3, -1
	movh	$1, 0x31
	add3	$1, $1, 0
	repeat	$3, 1f
	nop
	sw	$0, ($1)
1:
	add3	$1, $1, $4
	or3	$2, $2, 0x400
	stc	$2, $cfg
	nop
	nop
2:
	bsr	earlyInitHook
	jmp	_start

	.section .entry, "ax"
	.global	maskBit0
maskBit0:
	and3	$0, $1, 1
	ret

	/*
	 * Configures Bigmac channel ($4 & 1) to fill ($2 & 0xffffff) bytes at $1
	 * with byte value $3 (mode 0xc = memset), without starting the channel.
	 */
	.global	bigmacSetupMemset
bigmacSetupMemset:
	movu	$0, 0xffffff
	and3	$4, $4, 1
	and	$2, $0
	beqz	$4, 1f
	movh	$0, 0xe005
	or3	$0, $0, 0xb4
	sw	$3, ($0)
	movh	$3, 0xe005
	or3	$3, $3, 0x80
	mov	$0, 0
	sw	$0, ($3)
	movh	$3, 0xe005
	or3	$3, $3, 0x84
	sw	$1, ($3)
	movh	$3, 0xe005
	or3	$3, $3, 0x88
	sw	$2, ($3)
	movh	$3, 0xe005
	or3	$3, $3, 0x8c
	mov	$2, 12
	sw	$2, ($3)
	movh	$3, 0xe005
	or3	$3, $3, 0x94
	sw	$0, ($3)
	ret
1:
	movh	$0, 0xe005
	or3	$0, $0, 0x34
	sw	$3, ($0)
	movh	$3, 0xe005
	sw	$4, ($3)
	or3	$3, $3, 0x4
	sw	$1, ($3)
	movh	$3, 0xe005
	or3	$3, $3, 0x8
	sw	$2, ($3)
	movh	$3, 0xe005
	or3	$3, $3, 0xc
	mov	$2, 12
	sw	$2, ($3)
	movh	$3, 0xe005
	or3	$3, $3, 0x14
	sw	$4, ($3)
	ret
	nop
	nop

	.global	_start
_start:
	movu	$3, 0x81e000
	mov	$sp, $3
	movu	$3, __gp
	mov	$gp, $3
	mov	$0, 0
	stc	$0, $hi
	stc	$0, $lo
	movh	$1, 0xe005
	or3	$1, $1, 0x3c
	movu	$2, __stack_chk_guard
	lw	$3, ($1)
	sw	$3, ($2)
	ldc	$0, $cfg
	mov	$1, -17
	and	$0, $1
	or3	$0, $0, 8
	stc	$0, $cfg
	movu	$1, __bss_start
	movu	$2, __bss_end
	sub	$2, $1
	mov	$3, 0
	mov	$4, 0
	bsr	bigmacSetupMemset
	mov	$0, 1
	movh	$1, 0xe005
	or3	$1, $1, 0x1c
	movh	$2, 0xe005
	or3	$2, $2, 0x24
	sw	$0, ($1)
1:
	lw	$0, ($2)
	and3	$0, $0, 1
	bnez	$0, 1b
	bsr	__do_global_ctors
	bsr	secondLoaderMain
	sw	$0, (g_bootExitStatus)
	bsr	__do_global_dtors

	.global	__stack_chk_fail
__stack_chk_fail:
	movu	$1, 0x800100
	movu	$2, 0x81ff00
	sub	$2, $1
	mov	$3, 0
	mov	$4, 0
	bsr	bigmacSetupMemset
	lw	$0, (g_bootExitStatus)
	bnez	$0, 2f
	movh	$3, 0xe000
	or3	$3, $3, 0
	mov	$4, 0x700
	lw	$5, (g_isColdBoot)
	movu	$6, trampoline_arm_boot
	movh	$gp, 0xe310
	add3	$gp, $gp, 0x1000
	movh	$sp, 0xe310
	add3	$sp, $sp, 0x1004
	jmp	3f
2:
	xor	$3, $3
	xor	$4, $4
	xor	$5, $5
	movu	$6, trampoline_fatal_sleep
	xor	$gp, $gp
	xor	$sp, $sp
3:
	movu	$7, 0x5ff00
	mov	$2, 15
	repeat	$2, 4f
	lw	$1, ($6)
	add	$6, 4
4:
	sw	$1, ($7)
	add	$7, 4
	xor	$0, $0
	xor	$6, $6
	xor	$7, $7
	xor	$8, $8
	xor	$9, $9
	xor	$10, $10
	xor	$11, $11
	xor	$12, $12
	xor	$tp, $tp
	stc	$0, $sar
	stc	$0, $hi
	stc	$0, $lo
	stc	$0, $tmp
	stc	$0, $epc
	stc	$0, $npc
	stc	$0, $rpb
	stc	$0, $rpe
	stc	$0, $rpc
	movh	$1, 0xe005
	or3	$1, $1, 0x1c
	movh	$2, 0xe005
	or3	$2, $2, 0x24
	jmp	0x5ff00
	nop
	nop
	nop
	nop
	nop

	.global	__do_global_ctors
__do_global_ctors:
	add	$sp, -28
	ldc	$1, $lp
	sw	$1, ($sp)
	movu	$5, __ctors_start
	movu	$6, __ctors_end
1:
	beq	$5, $6, 2f
	lw	$0, ($5)
	jsr	$0
	add	$5, 4
	bra	1b
2:
	lw	$1, ($sp)
	add	$sp, 28
	jmp	$1

	.global	__do_global_dtors
__do_global_dtors:
	add	$sp, -28
	ldc	$1, $lp
	sw	$1, ($sp)
	movu	$5, __dtors_start
	movu	$6, __dtors_end
1:
	beq	$5, $6, 2f
	lw	$0, ($5)
	jsr	$0
	add	$5, 4
	bra	1b
2:
	lw	$1, ($sp)
	add	$sp, 28
	jmp	$1
