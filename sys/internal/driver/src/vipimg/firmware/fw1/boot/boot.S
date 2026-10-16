/*
 * fw1 boot block: the reset and exception vectors both VIP cores start from.
 *
 * The block is the first 0x3000 bytes of the image. A core resets at 0, where the
 * image is also visible, configures its caches and enters its own program through
 * the entry table at 0x200, by the core number in $id.
 */
#include "boot.h"

	.section .boot.vectors, "ax"
	.global	bootVectors
bootVectors:
	jmp	bootReset		/* reset */
	jmp	bootException		/* exception */
	jmp	bootUnusedVector	/* the other vectors */
	jmp	bootUnusedVector
	jmp	bootUnusedVector
	jmp	bootUnusedVector
	jmp	bootUnusedVector
	jmp	bootUnusedVector
	jmp	bootUnusedVector

	.section .boot.text, "ax"
/*
 * Reset: invalidate the instruction cache tags (from local RAM when the core has
 * any, since the cache cannot be cleared while it fetches the loop), turn the
 * cache on, and enter the program of this core.
 */
	.global	bootReset
bootReset:
	movu	$0, 0x10
	stcb	$0, VIP_CB_CACHE_CONTROL
	movh	$4, 0x7f
	ldc	$0, $ccfg		/* cache size, in units of 4 KB */
	mov	$1, 0
	mov	$2, BOOT_CACHE_LINE * 2
	and	$0, $4
	srl	$0, 12
	beqz	$0, 2f
	add	$0, -1
	ldc	$3, $rcfg		/* local RAM size */
	movh	$tp, BOOT_CACHE_TAGS >> 16
	and	$3, $4
	bnez	$3, 1f
	bsr	bootClearCacheTags
	bra	2f
1:	movu	$sp, bootClearCacheTags	/* run the loop from local RAM */
	lw	$3, 0($sp)
	lw	$4, 4($sp)
	lw	$5, 8($sp)
	movh	$sp, VIP_LOCAL_RAM >> 16
	sw	$3, 0($sp)
	sw	$4, 4($sp)
	sw	$5, 8($sp)
	jsr	$sp
2:	beqz	$0, 3f
	ldc	$1, $cfg
	or3	$1, $1, 0x2		/* instruction cache on */
	stc	$1, $cfg
3:	ldc	$1, $id
	movu	$0, bootEntries
	srl	$1, 16
	and3	$1, $1, 0x3
	sll	$1, 2
	add3	$2, $0, $1
	lw	$3, 0($2)
	jmp	$3

/* clear the tags of $0 + 1 cache line pairs from $tp; 12 bytes, so it can run from local RAM */
bootClearCacheTags:
	repeat	$0, 1f
	sw	$1, 0($tp)
	sw	$1, 0x20($tp)
1:	add3	$tp, $tp, $2
	ret

bootReturnFromInterrupt:
	reti

/* an unexpected vector: resume at bootHalt */
	.global	bootUnusedVector
bootUnusedVector:
	movu	$0, bootHalt
	stc	$0, $npc
	nop
	stc	$0, $epc
	nop
	nop
	nop
	reti

/*
 * An exception: record the core, the status and the cause in this core's crash
 * record, count it, tell the ARM side through the mailbox, and stop.
 */
	.global	bootException
bootException:
	movu	$0, bootCrashRecords
	ldc	$1, $id
	srl	$1, 12			/* core number * 16 */
	add3	$0, $0, $1
	ldc	$1, $id
	sw	$1, 0($0)
	ldc	$1, $psw
	sw	$1, 4($0)
	ldc	$1, $exc
	sw	$1, 8($0)
	lw	$1, 12($0)
	add3	$1, $1, 1
	sw	$1, 12($0)
	movh	$0, 0xe310
	or3	$0, $0, 0x6054		/* mailbox debugReceive[1] */
	movh	$1, 0xffff
	or3	$1, $1, 0xffff
	sw	$1, 0($0)
	di
1:	halt
	halt
	halt
	halt
	halt
	jmp	1b

	.global	bootHalt
bootHalt:
	halt
	halt
	halt
	halt
	halt
	jmp	bootHalt

/*
 * The program of each core (by the core number in $id), and the regions the
 * programs leave to be cleared: their address and size.
 */
	.section .boot.entries, "a"
	.global	bootEntries
bootEntries:
	.word	pu0Start
	.word	pu1Start
	.word	0
	.word	0
	.word	-1, -1, -1, -1, -1
	.word	0, 0, 0
	.global	bootClearRegions
bootClearRegions:
	.word	__pu0_lram_image_end, __pu0_lram_image_gap
	.word	__pu0_bss_start, __pu0_bss_size
	.word	__pu1_lram_image_end, __pu1_lram_image_gap
	.word	__pu1_bss_start, __pu1_bss_size

/* one record per core: $id, $psw, $exc and the number of exceptions */
	.section .boot.crash, "aw"
	.global	bootCrashRecords
bootCrashRecords:
	.space	16 * 4
