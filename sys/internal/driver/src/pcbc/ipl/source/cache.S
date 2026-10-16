/*
 * Interrupt control and cache maintenance (loadcore's cache.S and
 * interruptController.S, linked into the IPL).
 *
 * Allegrex caches: 2-way, 64-byte lines; the size of each comes from Config
 * (D-cache size bits 6-8, I-cache bits 9-11: 4 KiB << n). mtic needs seven
 * cycles before the interrupt state change is visible.
 *
 * The exported cache routines are entered through a prologue that parks the
 * caller's return address in t9 and returns through cacheReturn, which
 * restores it and leaves with eret (an instruction hazard barrier).
 */

	.set	noreorder
	.set	noat

#define ICACHE_INDEX_LOAD_TAG		0x00
#define ICACHE_INDEX_INVALIDATE		0x01
#define ICACHE_INDEX_LOAD_TAG2		0x02
#define ICACHE_INDEX_STORE_TAG		0x03
#define ICACHE_HIT_INVALIDATE		0x08
#define DCACHE_INDEX_LOAD_TAG		0x10
#define DCACHE_INDEX_STORE_TAG		0x11
#define DCACHE_INDEX_LOAD_TAG2		0x12
#define DCACHE_INDEX_STORE_TAG2		0x13
#define DCACHE_INDEX_WB_INVALIDATE	0x14
#define DCACHE_HIT_INVALIDATE		0x19
#define DCACHE_HIT_WRITEBACK		0x1A
#define DCACHE_HIT_WB_INVALIDATE	0x1B

#define TAGLO	$28
#define TAGHI	$29
#define CONFIG	$16

#define LEAF(name)	\
	.globl	name;	\
	.type	name, @function; \
	.ent	name;	\
name:

#define END(name)	\
	.end	name;	\
	.size	name, . - name

/* route the return through cacheReturn */
#define CACHE_ENTRY	\
	move	$t9, $ra;	\
	lui	$ra, %hi(cacheReturn);	\
	addiu	$ra, $ra, %lo(cacheReturn)

#define HAZARD7	nop; nop; nop; nop; nop; nop; nop

	.text

LEAF(sceKernelCpuSuspendIntr)
	mfic	$v0, $0
	mtic	$zero, $0
	HAZARD7
	jr	$ra
	nop
END(sceKernelCpuSuspendIntr)

LEAF(sceKernelCpuResumeIntr)
	mtic	$a0, $0
	HAZARD7
	jr	$ra
	nop
END(sceKernelCpuResumeIntr)

LEAF(sceKernelCpuResumeIntrWithSync)
	sync
	nop
	sync
	nop
	mtic	$a0, $0
	HAZARD7
	jr	$ra
	nop
END(sceKernelCpuResumeIntrWithSync)

LEAF(sceKernelIsCpuIntrSuspended)
	jr	$ra
	sltiu	$v0, $a0, 1
END(sceKernelIsCpuIntrSuspended)

LEAF(sceKernelIsCpuIntrEnable)
	mfic	$v0, $0
	jr	$ra
	nop
END(sceKernelIsCpuIntrEnable)

	.balign	64

/* write back every dirty line, way by way, keeping the lines valid */
LEAF(sceKernelDcacheWritebackAll)
	CACHE_ENTRY
	mfic	$v0, $0
	mtic	$zero, $0
	mfc0	$t0, CONFIG
	li	$t1, 2048
	ext	$t0, $t0, 6, 3
	sllv	$t1, $t1, $t0		/* half the D-cache: one way */
	move	$t0, $zero
	li	$t6, -1			/* last line written back */
1:	cache	DCACHE_INDEX_LOAD_TAG, 0($t0)
	mfc0	$t2, TAGLO
	mfc0	$t3, TAGHI
	move	$t4, $t2
	xor	$t5, $t3, $t6
	ext	$t5, $t5, 0, 19
	movz	$t2, $t3, $t5		/* way 1 first when it holds the last line */
	movz	$t3, $t4, $t5
	ext	$t4, $t2, 20, 1		/* dirty */
	beqz	$t4, 2f
	ext	$t4, $t3, 20, 1
	sll	$t5, $t2, 13
	or	$t5, $t5, $t0
	cache	DCACHE_HIT_WRITEBACK, 0($t5)
	move	$t6, $t2
2:	beqz	$t4, 3f
	sll	$t5, $t3, 13
	or	$t5, $t5, $t0
	cache	DCACHE_HIT_WRITEBACK, 0($t5)
	move	$t6, $t3
3:	mtic	$v0, $0
	addiu	$t0, $t0, 64
	mtic	$zero, $0
	bne	$t0, $t1, 1b
	nop
	mtic	$v0, $0
	move	$v0, $zero
	jr	$ra
	sync
END(sceKernelDcacheWritebackAll)

LEAF(sceKernelDcacheWritebackInvalidateAll)
	CACHE_ENTRY
	.globl	dcacheWritebackInvalidateAll
dcacheWritebackInvalidateAll:
	mfc0	$t0, CONFIG
	li	$t1, 2048
	ext	$t0, $t0, 6, 3
	sllv	$t1, $t1, $t0
	move	$t0, $zero
1:	cache	DCACHE_INDEX_WB_INVALIDATE, 0($t0)
	addiu	$t0, $t0, 64
	bne	$t0, $t1, 1b
	cache	DCACHE_INDEX_WB_INVALIDATE, -64($t0)
	move	$v0, $zero
	jr	$ra
	sync
END(sceKernelDcacheWritebackInvalidateAll)

/* drop every line without writing it back */
LEAF(sceKernelDcacheInvalidateAll)
	CACHE_ENTRY
	mfc0	$t0, CONFIG
	li	$t1, 4096
	ext	$t0, $t0, 6, 3
	sllv	$t1, $t1, $t0
	mfic	$v0, $0
	mtic	$zero, $0
	mtc0	$zero, TAGLO
	mtc0	$zero, TAGHI
	move	$t0, $zero
1:	addiu	$t0, $t0, 64
	cache	DCACHE_INDEX_STORE_TAG, -64($t0)
	bne	$t0, $t1, 1b
	cache	DCACHE_INDEX_STORE_TAG2, -64($t0)
	mtic	$v0, $0
	jr	$ra
	move	$v0, $zero
END(sceKernelDcacheInvalidateAll)

/*
 * Range operations: below 64 KiB walk the range by address, above it walk
 * every index and match the tags against the range. a0 = address, a1 = size.
 */
#define DCACHE_RANGE(name, op)					\
LEAF(name)							\
	CACHE_ENTRY;						\
	srl	$v0, $a1, 16;					\
	bnez	$v0, 3f;					\
	addu	$a2, $a0, $a1;					\
	beq	$a2, $a0, 2f;					\
	addiu	$a2, $a2, 63;					\
	ins	$a2, $a0, 0, 6;					\
1:	addiu	$a0, $a0, 64;					\
	bne	$a0, $a2, 1b;					\
	cache	op, -64($a0);					\
2:	jr	$ra;						\
	sync;							\
5:	lui	$v0, 0x8000;					\
	jr	$ra;						\
	ori	$v0, $v0, 0x104;				\
3:	ext	$v0, $k1, 19, 3;	/* user callers: no huge ranges */ \
	negu	$v0, $v0;					\
	and	$v0, $v0, $a1;					\
	bltz	$v0, 5b;					\
	li	$v0, -8192;					\
	ins	$a0, $zero, 0, 6;				\
	subu	$a1, $a1, $a0;					\
	mfic	$v1, $0;					\
	move	$t0, $v0;					\
4:	mtic	$zero, $0;					\
	addiu	$v0, $v0, 64;					\
	cache	DCACHE_INDEX_LOAD_TAG, 8128($v0);		\
	mfc0	$t2, TAGLO;					\
	mfc0	$t3, TAGHI;					\
	mtic	$v1, $0;					\
	ins	$t0, $t2, 13, 19;				\
	subu	$t4, $t0, $a0;					\
	sltu	$t4, $t4, $a1;					\
	bnezl	$t4, 6f;					\
	cache	op, 0($t0);					\
6:	ins	$t0, $t3, 13, 19;				\
	subu	$t4, $t0, $a0;					\
	sltu	$t4, $t4, $a1;					\
	bnezl	$t4, 7f;					\
	cache	op, 0($t0);					\
7:	bnez	$v0, 4b;					\
	move	$t0, $v0;					\
	jr	$ra;						\
	sync;							\
END(name)

DCACHE_RANGE(sceKernelDcacheWritebackRange, DCACHE_HIT_WRITEBACK)
DCACHE_RANGE(sceKernelDcacheWritebackInvalidateRange, DCACHE_HIT_WB_INVALIDATE)

LEAF(sceKernelDcacheInvalidateRange)
	CACHE_ENTRY
	beqz	$a1, 2f
	or	$t0, $a0, $a1
	andi	$t0, $t0, 0x3F
	bnez	$t0, 3f
	li	$t0, 0xC000
	sltu	$t0, $a1, $t0
	beqz	$t0, 4f
	addu	$a2, $a0, $a1
1:	addiu	$a0, $a0, 64
	bne	$a0, $a2, 1b
	cache	DCACHE_HIT_INVALIDATE, -64($a0)
2:	jr	$ra
	move	$v0, $zero
3:	lui	$v0, 0x8002
	jr	$ra
	ori	$v0, $v0, 0x44C		/* SCE_ERROR_KERNEL_ILLEGAL_ALIGNMENT */
4:	bltzl	$a1, 5f
	break
5:	li	$v0, 8128
6:	mfic	$v1, $0
	mtic	$zero, $0
	move	$t0, $v0
	cache	DCACHE_INDEX_LOAD_TAG, 0($v0)
	mfc0	$t2, TAGLO
	mfc0	$t3, TAGHI
	mtic	$v1, $0
	ins	$t0, $t2, 13, 19
	subu	$t4, $t0, $a0
	sltu	$t4, $t4, $a1
	bnezl	$t4, 7f
	cache	DCACHE_HIT_INVALIDATE, 0($t0)
7:	ins	$t0, $t3, 13, 19
	subu	$t4, $t0, $a0
	sltu	$t4, $t4, $a1
	bnezl	$t4, 8f
	cache	DCACHE_HIT_INVALIDATE, 0($t0)
8:	bnez	$v0, 6b
	addiu	$v0, $v0, -64
	jr	$ra
	move	$v0, $zero
END(sceKernelDcacheInvalidateRange)

/* mark the lines of a range clean (dirty bit cleared) without writing back */
LEAF(DcachePurgeRange_88601b44)
	CACHE_ENTRY
	mfc0	$t0, CONFIG
	beqz	$a1, 3f
	li	$t1, 4096
	ext	$t0, $t0, 6, 3
	sllv	$t1, $t1, $t0
	or	$t0, $a0, $a1
	andi	$t0, $t0, 0x3F
	bnez	$t0, 4f
	sltu	$t0, $a1, $t1
	addu	$a1, $a0, $a1
	beqzl	$t0, 5f
	addiu	$a1, $a1, -1
	mfic	$v0, $0
	mtic	$zero, $0
1:	srl	$t0, $a0, 13
	cache	DCACHE_INDEX_LOAD_TAG, 0($a0)
	sll	$t0, $t0, 13
	mfc0	$t1, TAGLO
	mfc0	$t2, TAGHI
	ext	$t3, $t1, 20, 1
	ext	$t4, $t2, 20, 1
	sll	$t5, $t1, 13
	sll	$t6, $t2, 13
	xor	$t5, $t5, $t0
	xor	$t6, $t6, $t0
	movz	$t3, $zero, $t5
	movz	$t4, $zero, $t6
	ext	$t1, $t3, 20, 1
	ext	$t2, $t4, 20, 1
	mtc0	$t1, TAGLO
	mtc0	$t2, TAGHI
	cache	DCACHE_INDEX_STORE_TAG, 0($a0)
	mtic	$v0, $0
	addiu	$a0, $a0, 64
	mtic	$zero, $0
	bne	$a0, $a1, 1b
	nop
	mtic	$v0, $0
3:	jr	$ra
	move	$v0, $zero
4:	lui	$v0, 0x8002
	jr	$ra
	ori	$v0, $v0, 0x44C
5:	mfic	$v0, $0
	mtic	$zero, $0
	srl	$t1, $t1, 1
	move	$t0, $zero
6:	cache	DCACHE_INDEX_LOAD_TAG, 0($t0)
	mfc0	$t2, TAGLO
	ext	$t3, $t2, 19, 2
	xori	$t3, $t3, 3
	bnez	$t3, 7f
	sll	$t3, $t2, 13
	or	$t3, $t3, $t0
	subu	$t4, $a1, $t3
	subu	$t5, $t3, $a0
	or	$t4, $t4, $t5
	bltz	$t4, 7f
	ins	$t2, $zero, 20, 1
	mtc0	$t2, TAGLO
7:	mfc0	$t2, TAGHI
	ext	$t3, $t2, 19, 2
	xori	$t3, $t3, 3
	beqz	$t3, 8f
	sll	$t3, $t2, 13
	or	$t3, $t3, $t0
	subu	$t4, $a1, $t3
	subu	$t5, $t3, $a0
	or	$t4, $t4, $t5
	bltz	$t4, 8f
	ins	$t2, $zero, 20, 1
	mtc0	$t2, TAGHI
8:	cache	DCACHE_INDEX_STORE_TAG, 0($t0)
	mtic	$v0, $0
	addiu	$t0, $t0, 64
	mtic	$zero, $0
	bne	$t0, $t1, 6b
	nop
	mtic	$v0, $0
	move	$v0, $zero
	jr	$ra
	sync
END(DcachePurgeRange_88601b44)

/* state of the line holding a0: 0 absent, 1 clean, 2 dirty, 3 ... (way 0 | way 1) */
LEAF(sceKernelDcacheProbe)
	CACHE_ENTRY
	mfic	$v0, $0
	mtic	$zero, $0
	srl	$t0, $a0, 13
	cache	DCACHE_INDEX_LOAD_TAG, 0($a0)
	sll	$t0, $t0, 13
	mfc0	$t1, TAGLO
	mfc0	$t2, TAGHI
	mtic	$v0, $0
	ext	$t3, $t1, 19, 2
	ext	$t4, $t2, 19, 2
	sll	$t1, $t1, 13
	sll	$t2, $t2, 13
	xor	$t1, $t1, $t0
	xor	$t2, $t2, $t0
	movn	$t3, $zero, $t1
	movn	$t4, $zero, $t2
	or	$v0, $t3, $t4
	ext	$t0, $v0, 1, 1
	jr	$ra
	xor	$v0, $v0, $t0
END(sceKernelDcacheProbe)

/* tags of both ways of the index of a0, invalid ones as 0 */
LEAF(sceKernelDcacheReadTag)
	CACHE_ENTRY
	mfic	$t0, $0
	mtic	$zero, $0
	cache	DCACHE_INDEX_LOAD_TAG, 0($a0)
	mfc0	$v0, TAGLO
	mfc0	$v1, TAGHI
	ext	$at, $v0, 18, 1
	movn	$v0, $zero, $at
	ext	$at, $v1, 18, 1
	movn	$v1, $zero, $at
	cache	DCACHE_INDEX_LOAD_TAG2, 0($a0)
	mfc0	$at, TAGLO
	mtic	$t0, $0
	jr	$ra
	nop
END(sceKernelDcacheReadTag)

LEAF(DcacheReadTagRaw_88601d38)
	mfic	$t0, $0
	mtic	$zero, $0
	cache	DCACHE_INDEX_LOAD_TAG, 0($a0)
	mfc0	$v0, TAGLO
	mfc0	$v1, TAGHI
	cache	DCACHE_INDEX_LOAD_TAG2, 0($a0)
	mfc0	$at, TAGLO
	mtic	$t0, $0
	jr	$ra
	nop
END(DcacheReadTagRaw_88601d38)

/* 0 nothing of the range cached, 1 cached clean, 2 a dirty line found */
LEAF(sceKernelDcacheProbeRange)
	CACHE_ENTRY
	mfc0	$t0, CONFIG
	beqz	$a1, 3f
	move	$v0, $zero
	li	$t1, 4096
	ext	$t0, $t0, 6, 3
	sllv	$t1, $t1, $t0
	addu	$a1, $a0, $a1
	addiu	$a1, $a1, -1
	ins	$a0, $zero, 0, 6
	ori	$a1, $a1, 0x3F
	mfic	$v1, $0
	mtic	$zero, $0
	srl	$t1, $t1, 1
	move	$t0, $zero
1:	cache	DCACHE_INDEX_LOAD_TAG, 0($t0)
	mfc0	$t2, TAGLO
	ext	$t3, $t2, 19, 1
	beqz	$t3, 4f
	sll	$t3, $t2, 13
	or	$t3, $t3, $t0
	subu	$t4, $a1, $t3
	subu	$t5, $t3, $a0
	or	$t4, $t4, $t5
	bltz	$t4, 2f
	ext	$t3, $t2, 20, 1
	bnez	$t3, 5f
	nop
4:	mfc0	$t2, TAGHI
	ext	$t3, $t2, 19, 1
	beqz	$t3, 6f
	sll	$t3, $t2, 13
	or	$t3, $t3, $t0
	subu	$t4, $a1, $t3
	subu	$t5, $t3, $a0
	or	$t4, $t4, $t5
	bltz	$t4, 2f
	ext	$t3, $t2, 20, 1
	bnez	$t3, 5f
	nop
6:	mtic	$v1, $0
	addiu	$t0, $t0, 64
	mtic	$zero, $0
	bne	$t0, $t1, 1b
	nop
	li	$v0, 1
2:	mtic	$v1, $0
3:	jr	$ra
	nop
5:	b	2b
	li	$v0, 2
END(sceKernelDcacheProbeRange)

LEAF(sceKernelIcacheInvalidateAll)
	CACHE_ENTRY
	.globl	icacheInvalidateAll
icacheInvalidateAll:
	mfc0	$t0, CONFIG
	li	$t1, 4096
	ext	$t0, $t0, 9, 3
	sllv	$t1, $t1, $t0
	mfic	$v0, $0
	mtic	$zero, $0
	mtc0	$zero, TAGLO
	mtc0	$zero, TAGHI
	move	$t0, $zero
1:	addiu	$t0, $t0, 64
	cache	ICACHE_INDEX_INVALIDATE, -64($t0)
	bne	$t0, $t1, 1b
	cache	ICACHE_INDEX_STORE_TAG, -64($t0)
	mtic	$v0, $0
	jr	$ra
	move	$v0, $zero
END(sceKernelIcacheInvalidateAll)

LEAF(sceKernelIcacheInvalidateRange)
	CACHE_ENTRY
	mfc0	$t0, CONFIG
	beqz	$a1, 2f
	li	$t1, 4096
	ext	$t0, $t0, 9, 3
	sllv	$t1, $t1, $t0
	sltu	$t0, $a1, $t1
	addu	$a1, $a0, $a1
	addiu	$a1, $a1, -1
	ins	$a0, $zero, 0, 6
	beqz	$t0, 3f
	ori	$a1, $a1, 0x3F
	addiu	$a1, $a1, 1
1:	addiu	$a0, $a0, 64
	bne	$a0, $a1, 1b
	cache	ICACHE_HIT_INVALIDATE, -64($a0)
2:	jr	$ra
	move	$v0, $zero
3:	mfic	$v0, $0
	mtic	$zero, $0
	srl	$t1, $t1, 1
	move	$t0, $zero
4:	cache	ICACHE_INDEX_LOAD_TAG, 0($t0)
	mfc0	$t2, TAGLO
	ext	$t3, $t2, 20, 1
	beqz	$t3, 5f
	sll	$t2, $t2, 13
	or	$t2, $t2, $t0
	subu	$t4, $a1, $t2
	subu	$t5, $t2, $a0
	or	$t4, $t4, $t5
	bltz	$t4, 5f
	nop
	cache	ICACHE_HIT_INVALIDATE, 0($t2)
5:	mfc0	$t2, TAGHI
	ext	$t3, $t2, 20, 1
	beqz	$t3, 6f
	sll	$t2, $t2, 13
	or	$t2, $t2, $t0
	subu	$t4, $a1, $t2
	subu	$t5, $t2, $a0
	or	$t4, $t4, $t5
	bltz	$t4, 6f
	nop
	cache	ICACHE_HIT_INVALIDATE, 0($t2)
6:	mtic	$v0, $0
	addiu	$t0, $t0, 64
	mtic	$zero, $0
	bne	$t0, $t1, 4b
	nop
	mtic	$v0, $0
	jr	$ra
	move	$v0, $zero
END(sceKernelIcacheInvalidateRange)

/* 1 when the line holding a0 is in the I-cache (either way) */
LEAF(sceKernelIcacheProbe)
	CACHE_ENTRY
	lui	$t0, 0xA000		/* return uncached */
	or	$ra, $t0, $ra
	mfic	$v0, $0
	mtic	$zero, $0
	nop
	cache	ICACHE_INDEX_LOAD_TAG, 0($a0)
	move	$t5, $a0
	ins	$t5, $zero, 0, 13
	mfc0	$t1, TAGLO
	mfc0	$t2, TAGHI
	mtic	$v0, $0
	ext	$t3, $t1, 19, 1
	ext	$t4, $t2, 19, 1
	sll	$t1, $t1, 13
	sll	$t2, $t2, 13
	xor	$t1, $t1, $t5
	xor	$t2, $t2, $t5
	movn	$t3, $zero, $t1
	movn	$t4, $zero, $t2
	jr	$ra
	or	$v0, $t3, $t4
END(sceKernelIcacheProbe)

LEAF(sceKernelIcacheReadTag)
	CACHE_ENTRY
	mfic	$t0, $0
	mtic	$zero, $0
	cache	ICACHE_INDEX_LOAD_TAG, 0($a0)
	mfc0	$v0, TAGLO
	mfc0	$v1, TAGHI
	ext	$at, $v0, 18, 1
	movn	$v0, $zero, $at
	ext	$at, $v1, 18, 1
	movn	$v1, $zero, $at
	cache	ICACHE_INDEX_LOAD_TAG2, 0($a0)
	mfc0	$at, TAGLO
	mtic	$t0, $0
	jr	$ra
	nop
END(sceKernelIcacheReadTag)

LEAF(IcacheReadTagRaw_88602000)
	mfic	$t0, $0
	mtic	$zero, $0
	cache	ICACHE_INDEX_LOAD_TAG, 0($a0)
	mfc0	$v0, TAGLO
	mfc0	$v1, TAGHI
	cache	ICACHE_INDEX_LOAD_TAG2, 0($a0)
	mfc0	$at, TAGLO
	mtic	$t0, $0
	jr	$ra
	nop
END(IcacheReadTagRaw_88602000)

LEAF(cacheReturn)
	move	$ra, $t9
	eret
END(cacheReturn)

