/*
 * start.s: MeP hardware exception vector table (0x800000..0x8000af),
 * unaliased icache tag clear helpers (0x8000b0..0x8000df), SRAM wipe & sleep
 * routine (0x8000e0..0x8000ff), _start entry point (0x800100..0x8001a7),
 * kernel re-entry helper (0x8001a8..0x8001b9), icache flush helpers
 * (0x8001ba..0x800215), __stack_chk_fail_local (0x800216..0x80021f), and
 * global constructor/destructor runners (0x800220..0x80025f).
 */

	.section .vectors, "ax", @progbits
	.global vector_table
vector_table:
	/* 0x800000: Reset / NMI / Exception vectors */
	jmp	_start				/* 0x800000: Reset */
	reti					/* 0x800004: NMI */
	nop
	jmp	excReservedInsnHandler		/* 0x800008: RI (Reserved Instruction) */
	jmp	excZeroDivHandler		/* 0x80000c: ZDIV (Zero Division) */
	reti					/* 0x800010: BRK */
	nop
	jmp	swiExceptionEntry		/* 0x800014: SWI (Software Interrupt) */
1:	jmp	1b				/* 0x800018: DEBUG */
	reti					/* 0x80001c: DSP */
	nop
	reti					/* 0x800020: COP */
	nop
	reti					/* 0x800024: Reserved */
	nop
	reti					/* 0x800028: Reserved */
	nop
	reti					/* 0x80002c: Reserved */
	nop

	/* 0x800030: Hardware interrupt vectors (INT0..INT31) */
	reti					/* 0x800030: INT0 */
	nop
	jmp	irqExceptionEntry		/* 0x800034: INT1 */
	jmp	irqExceptionEntry		/* 0x800038: INT2 */
	jmp	irqExceptionEntry		/* 0x80003c: INT3 */
	jmp	irqExceptionEntry		/* 0x800040: INT4 */
	jmp	irqExceptionEntry		/* 0x800044: INT5 */
	jmp	irqExceptionEntry		/* 0x800048: INT6 */
	reti					/* 0x80004c: INT7 */
	nop
	jmp	cmdIrqExceptionEntry		/* 0x800050: INT8 (ARM->MeP command) */
	jmp	irqExceptionEntry		/* 0x800054: INT9 */
	jmp	irqExceptionEntry		/* 0x800058: INT10 */
	jmp	irqExceptionEntry		/* 0x80005c: INT11 */
	.rept 20
	reti					/* 0x800060..0x8000af: INT12..INT31 */
	nop
	.endr

	.section .init_text, "ax", @progbits

	/*
	 * skClearIcacheTags (0x8000b0, invoked at unaliased 0x400b0, name ours):
	 * Enables icache tag RAM access in $cfg and zeroes all icache tag lines.
	 */
	.global skClearIcacheTags
skClearIcacheTags:
	ldc	$0, $cfg
	or3	$0, $0, 0x2
	stc	$0, $cfg
	or3	$0, $0, 0x400
	stc	$0, $cfg
	mov	$0, 0
	add	$3, -1
	repeat	$3, 1f
	nop
	sw	$0, ($1)
1:	add3	$1, $1, $4
	ret

	/*
	 * skClearIcacheCfgBits (0x8000ce, invoked at unaliased 0x400ce, name ours):
	 * Clears bits 1 and 10 (0x402) in $cfg.
	 */
	.global skClearIcacheCfgBits
skClearIcacheCfgBits:
	ldc	$0, $cfg
	mov	$1, -1027
	and	$0, $1
	stc	$0, $cfg
	mov	$0, 0
	ret
	nop
	nop

	/*
	 * skWipeSramAndSleep (0x8000e0, name ours):
	 * Wipes SRAM words [$0 .. $0 + ($1 + 1)*4), clears registers, and halts.
	 */
	.global skWipeSramAndSleep
skWipeSramAndSleep:
	repeat	$1, 1f
	nop
	sw	$3, ($0)
1:	add	$0, 4
	xor	$0, $0
	xor	$1, $1
	xor	$2, $2
	stc	$3, $rpb
	stc	$3, $rpe
	stc	$3, $rpc
.Lsleep_forever:
	sleep
.Lsleep_Forever_jmp:
	jmp	.Lsleep_forever
.Lreset_panic_jmp:
	jmp	excResetVectorHandler

	/*
	 * _start (0x800100):
	 * Secure Kernel entry point (and re-entry point on SM exit/unload).
	 */
	.global _start
_start:
	di
	mov	$1, 0
	movu	$2, __bss_start
	movu	$3, __bss_end
	sub	$3, $2
	srl	$3, 3
1:	beqz	$3, 2f
	sw	$1, ($2)
	sw	$1, 4($2)
	add	$2, 8
	add	$3, -1
	bra	1b
2:	movu	$3, 0x808ff0
	mov	$sp, $3
	movu	$3, __gp
	mov	$gp, $3
	mov	$0, 0
	stc	$0, $hi
	stc	$0, $lo
	/* Seed __stack_chk_guard from hardware TRNG (0xe005003c) */
	movh	$1, 0xe005
	or3	$1, $1, 0x3c
	movu	$2, __stack_chk_guard
	lw	$3, ($1)
	sw	$3, ($2)
	/* Enable instruction cache ($cfg = ($cfg & ~0x10) | 0x08) */
	ldc	$0, $cfg
	mov	$1, -17
	and	$0, $1
	or3	$0, $0, 0x8
	stc	$0, $cfg
	/* Trap unexpected jumps to 0x800000 into excResetVectorHandler */
	lw	$1, (.Lreset_panic_jmp)
	sw	$1, (vector_table)
	bsr	skFlushIcache
	bsr	__do_global_ctors
	bsr	sceSblSkMain
	bsr	__do_global_dtors
	/* Shutdown: clear all general and control registers, wipe SRAM, and sleep */
	xor	$3, $3
	xor	$4, $4
	xor	$5, $5
	xor	$6, $6
	xor	$7, $7
	xor	$8, $8
	xor	$9, $9
	xor	$10, $10
	xor	$11, $11
	xor	$12, $12
	xor	$tp, $tp
	xor	$gp, $gp
	xor	$sp, $sp
	stc	$3, $sar
	stc	$3, $hi
	stc	$3, $lo
	stc	$3, $tmp
	stc	$3, $epc
	stc	$3, $npc
	lw	$1, (.Lsleep_Forever_jmp)
	sw	$1, (vector_table)
	movu	$0, .Lsleep_Forever_jmp
	stc	$0, $lp
	movu	$0, _start
	movu	$1, 0x808ff0
	sub	$1, $0
	srl	$1, 2
	add	$1, -1
	jmp	skWipeSramAndSleep

	/*
	 * skReenterKernel (0x8001a8, name ours):
	 * Returns from exception/interrupt context directly to _start (0x800100).
	 */
	.global skReenterKernel
skReenterKernel:
	ldc	$0, $psw
	mov	$1, -3
	and	$0, $1
	stc	$0, $psw
	movu	$0, _start
	stc	$0, $epc
	nop
	reti

	/*
	 * skFlushIcache (0x8001ba, name ours):
	 * Reads $ccfg to determine icache geometry and clears all icache tags
	 * at 0x00310000 via unaliased 0x400b0.
	 */
	.global skFlushIcache
skFlushIcache:
	add	$sp, -4
	ldc	$1, $lp
	sw	$1, ($sp)
	ldc	$3, $ccfg
	srl	$3, 16
	and3	$3, $3, 0x7f
	beqz	$3, 1f
	ldc	$2, $ccfg
	srl	$2, 24
	and3	$2, $2, 0x7
	blti	$2, 2, 1f
	bgei	$2, 5, 1f
	add	$2, 3
	mov	$0, 10
	sub	$0, $2
	sll	$3, $0
	mov	$4, 1
	sll	$4, $2
	movh	$1, 0x31
	add3	$1, $1, 0
	movh	$0, 0x4
	add3	$0, $0, 0xb0
	jsr	$0
1:	lw	$1, ($sp)
	add	$sp, 4
	jmp	$1

	/*
	 * skInvalidateAndFlushIcache (0x8001fe, name ours):
	 * Clears icache $cfg bits via unaliased 0x400ce and flushes icache tags.
	 */
	.global skInvalidateAndFlushIcache
skInvalidateAndFlushIcache:
	add	$sp, -4
	ldc	$1, $lp
	sw	$1, ($sp)
	movh	$1, 0x4
	add3	$1, $1, 0xce
	jsr	$1
	bsr	skFlushIcache
	lw	$1, ($sp)
	add	$sp, 4
	jmp	$1

	/*
	 * __stack_chk_fail_local (0x800216):
	 * Passes caller's $lp in $1 to __stack_chk_fail (0x8004c6).
	 */
	.global __stack_chk_fail_local
__stack_chk_fail_local:
	ldc	$1, $lp
	bsr	__stack_chk_fail
	ret
	nop

	/*
	 * __do_global_ctors (0x800220)
	 */
	.section .ctors_text, "ax", @progbits
	.global __do_global_ctors
__do_global_ctors:
	add	$sp, -28
	ldc	$1, $lp
	sw	$1, ($sp)
	movu	$5, __ctors_start
	movu	$6, __ctors_end
1:	beq	$5, $6, 2f
	lw	$0, ($5)
	jsr	$0
	add	$5, 4
	bra	1b
2:	lw	$1, ($sp)
	add	$sp, 28
	jmp	$1

	/*
	 * __do_global_dtors (0x800240)
	 */
	.global __do_global_dtors
__do_global_dtors:
	add	$sp, -28
	ldc	$1, $lp
	sw	$1, ($sp)
	movu	$5, __dtors_start
	movu	$6, __dtors_end
1:	beq	$5, $6, 2f
	lw	$0, ($5)
	jsr	$0
	add	$5, 4
	bra	1b
2:	lw	$1, ($sp)
	add	$sp, 28
	jmp	$1
