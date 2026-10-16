/*
 * fw2 boot block: the vectors and the reset code both cores start from, and the
 * tables the ARM side and the runtime share, at fixed places in the image's
 * first 0x3000 bytes (which the cores also see at 0).
 *
 * The ARM side boots FW2 by giving the cores the image at +0x80000 as their
 * image (vnz_wrapper's second implementation).
 */
#include "../common/runtime.h"

#define CACHE_TAGS	0x310000	/* instruction cache tag RAM */
#define CACHE_LINE	32

	.section .boot.vectors, "ax"
	.global	bootVectors
bootVectors:
	jmp	bootReset		/* reset */
	jmp	vipException		/* NMI */
	jmp	vipException		/* reserved instruction */
	reti				/* zero division: ignored */
	nop
	jmp	vipException		/* break */
	jmp	vipException		/* software interrupt */

	.section .boot.text, "ax"
/*
 * Reset: clear the instruction cache tags (from local RAM when the core has any,
 * since the loop cannot clear the cache it runs from) and turn the cache on,
 * clear $hi/$lo when the core has them, set the runtime's stack, clear the local
 * RAM, and enter the runtime with lp at a halt loop. A core numbered 255 (the
 * debugger's) starts where VIP_DEBUGGER_ENTRY says.
 */
	.global	bootReset
bootReset:
	movh	$4, 0x7f
	ldc	$0, $ccfg		/* cache size, in units of 4 KB */
	mov	$1, 0
	mov	$2, CACHE_LINE * 2
	and	$0, $4
	srl	$0, 12
	beqz	$0, 3f
	add	$0, -1
	ldc	$3, $rcfg		/* local RAM size */
	movh	$tp, CACHE_TAGS >> 16
	and	$3, $4
	bnez	$3, 1f
	bsr	bootClearCacheTags
	bra	2f
1:	movu	$gp, bootClearCacheTags	/* run the loop from local RAM */
	lw	$3, 0($gp)
	lw	$4, 4($gp)
	lw	$5, 8($gp)
	movh	$gp, VIP_LOCAL_RAM >> 16
	sw	$3, 0($gp)
	sw	$4, 4($gp)
	sw	$5, 8($gp)
	jsr	$gp
2:	ldc	$12, $cfg
	or3	$12, $12, 0x2		/* instruction cache on */
	stc	$12, $cfg
3:	movh	$11, 0x300		/* $opt: multiply or divide present */
	ldc	$12, $opt
	and	$12, $11
	beqz	$12, 4f
	mov	$12, 0
	stc	$12, $hi
	stc	$12, $lo
4:	movh	$gp, VIP_RUNTIME_STACK >> 16
	add3	$gp, $gp, VIP_RUNTIME_STACK & 0xffff
	mov	$sp, $gp
	movh	$gp, 0xa1
	add3	$gp, $gp, -0x8000
	movh	$4, VIP_LOCAL_RAM >> 16
	movu	$5, VIP_LOCAL_RAM + 0x2000
	mov	$10, 0
	mov	$11, 0x1000 / 8 - 1	/* clear 0x200000 and 0x202000, 4 KB each */
	repeat	$11, 5f
	sw	$10, 0($4)
	sw	$10, 0($5)
	sw	$10, 4($4)
	sw	$10, 4($5)
	add	$4, 8
5:	add	$5, 8
	ldc	$1, $id
	srl	$1, 16
	mov	$2, 255
	bne	$1, $2, 6f
	movu	$1, VIP_DEBUGGER_ENTRY
	lw	$3, 0($1)
	bra	7f
6:	movu	$3, vipMain
7:	stc	$3, $epc
	movu	$3, bootHalt
	stc	$3, $lp
	reti

	.global	bootHalt
bootHalt:
	di
	halt
	bra	bootHalt

/* clear the tags of $0 + 1 cache line pairs from $tp; 12 bytes, so it can run from local RAM */
bootClearCacheTags:
	repeat	$0, 1f
	sw	$1, 0($tp)
	sw	$1, 0x20($tp)
1:	add3	$tp, $tp, $2
	ret

/*
 * The command blocks (VipCommandBlock), one per core: the ARM side's, zero in
 * the image.
 */
	.section .boot.commands, "aw", @nobits
	.global	g_commandBlocks
g_commandBlocks:
	.space	0x200

/*
 * What vnz_wrapper reads back after the cores start (its "reported values"):
 * a word the cores leave 0, and the build's number, version and time.
 */
	.section .boot.version, "a"
	.global	g_version
g_version:
	.word	0
	.word	0x765			/* the build */
	.word	0x01010004		/* the version */
	.word	0x53ad04f5		/* 2014-06-27 05:45:25 UTC */
