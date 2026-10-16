/*
 * SceSysmem: the PL310 L2 maintenance by physical address, Sony's assembly
 * (the same bytes in the os0, TrustZone and SKBL builds). Each operation
 * writes one line address at a time through s_cpuL2RegisterBase (l2cc.c);
 * the invalidate does not widen the size by the start's offset in its line.
 * SKBL (SYSMEM_SKBL) adds invalidate-all, it and the TrustZone build
 * (SYSMEM_TZS) clean-and-invalidate-all, by way.
 * Register offsets: sce/pl310_regs.h.
 */
	.syntax unified
	.arch	armv7-a
	.thumb
	.text

#define PL310_CACHE_SYNC		0x730
#define PL310_INVALIDATE_PA		0x770
#define PL310_CLEAN_PA			0x7b0
#define PL310_INVALIDATE_WAY		0x77c
#define PL310_CLEAN_INVALIDATE_PA	0x7f0
#define PL310_CLEAN_INVALIDATE_WAY	0x7fc

	.macro	l2base reg
	movw	\reg, #:lower16:s_cpuL2RegisterBase
	movt	\reg, #:upper16:s_cpuL2RegisterBase
	ldr	\reg, [\reg]
	.endm

	.balign	4
	.global	CpuL2CacheSyncCore
	.type	CpuL2CacheSyncCore, %function
	.thumb_func
CpuL2CacheSyncCore:
	l2base	r0
	mov.w	r1, #0
	str.w	r1, [r0, #PL310_CACHE_SYNC]
1:	ldr.w	r1, [r0, #PL310_CACHE_SYNC]
	tst.w	r1, #1
	bne	1b
	dmb	sy
	bx	lr
	.balign	4
	.size	CpuL2CacheSyncCore, . - CpuL2CacheSyncCore

	.global	CpuL2InvalidateRangePA
	.type	CpuL2InvalidateRangePA, %function
	.thumb_func
CpuL2InvalidateRangePA:
	cbz	r1, 2f
	l2base	r2
	bic	r0, r0, #0x1f
1:	str.w	r0, [r2, #PL310_INVALIDATE_PA]
	adds	r0, #0x20
	subs	r1, #0x20
	bgt	1b
	dmb	sy
2:	bx	lr
	.balign	4
	.size	CpuL2InvalidateRangePA, . - CpuL2InvalidateRangePA

#if defined(SYSMEM_SKBL)
	.global	CpuL2InvalidateAll
	.type	CpuL2InvalidateAll, %function
	.thumb_func
CpuL2InvalidateAll:
	l2base	r0
	movw	r1, #0xffff
	str.w	r1, [r0, #PL310_INVALIDATE_WAY]
	dmb	sy
1:	ldr.w	r1, [r0, #PL310_INVALIDATE_WAY]
	tst.w	r1, #1
	bne	1b
	bx	lr
	.balign	4
	.size	CpuL2InvalidateAll, . - CpuL2InvalidateAll
#endif

	.global	CpuL2CleanRangePA
	.type	CpuL2CleanRangePA, %function
	.thumb_func
CpuL2CleanRangePA:
	cbz	r1, 2f
	l2base	r2
	and	r3, r0, #0x1f
	bic	r0, r0, #0x1f
	adds	r1, r1, r3
1:	str.w	r0, [r2, #PL310_CLEAN_PA]
	adds	r0, #0x20
	subs	r1, #0x20
	bgt	1b
	dmb	sy
2:	bx	lr
	.balign	4
	.size	CpuL2CleanRangePA, . - CpuL2CleanRangePA

	.global	CpuL2CleanInvalidateRangePA
	.type	CpuL2CleanInvalidateRangePA, %function
	.thumb_func
CpuL2CleanInvalidateRangePA:
	cbz	r1, 2f
	l2base	r2
	and	r3, r0, #0x1f
	bic	r0, r0, #0x1f
	adds	r1, r1, r3
1:	str.w	r0, [r2, #PL310_CLEAN_INVALIDATE_PA]
	adds	r0, #0x20
	subs	r1, #0x20
	bgt	1b
	dmb	sy
2:	bx	lr
	.balign	4
	.size	CpuL2CleanInvalidateRangePA, . - CpuL2CleanInvalidateRangePA

#if defined(SYSMEM_SKBL) || defined(SYSMEM_TZS)
	.global	CpuL2CleanInvalidateAll
	.type	CpuL2CleanInvalidateAll, %function
	.thumb_func
CpuL2CleanInvalidateAll:
	l2base	r0
	movw	r1, #0xffff
	str.w	r1, [r0, #PL310_CLEAN_INVALIDATE_WAY]
	dmb	sy
1:	ldr.w	r1, [r0, #PL310_CLEAN_INVALIDATE_WAY]
	tst.w	r1, #1
	bne	1b
	bx	lr
	.balign	4
	.size	CpuL2CleanInvalidateAll, . - CpuL2CleanInvalidateAll
#endif
