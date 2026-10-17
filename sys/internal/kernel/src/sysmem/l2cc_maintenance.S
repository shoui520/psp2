/*
 * SceSysmem: the PL310 L2 maintenance by physical address, Sony's assembly
 * (the same bytes in the os0, TrustZone and SKBL builds). Each operation
 * writes one line address at a time through s_cpuL2RegisterBase (l2cc.c);
 * the invalidate does not widen the size by the start's offset in its line.
 * SKBL (SYSMEM_SKBL) adds the controller's setup, enable and disable and
 * invalidate-all; it and the TrustZone build (SYSMEM_TZS) add
 * clean-and-invalidate-all, by way.
 * Register offsets: sysmem/pl310_regs.h.
 */
	.syntax unified
	.arch	armv7-a
	.thumb
	.text

#define PL310_CONTROL			0x100
#define PL310_AUX_CONTROL		0x104
#define PL310_TAG_RAM_CONTROL		0x108
#define PL310_DATA_RAM_CONTROL		0x10c
#define PL310_CACHE_SYNC		0x730
#define PL310_INVALIDATE_PA		0x770
#define PL310_CLEAN_PA			0x7b0
#define PL310_INVALIDATE_WAY		0x77c
#define PL310_CLEAN_INVALIDATE_PA	0x7f0
#define PL310_CLEAN_INVALIDATE_WAY	0x7fc
#define PL310_PREFETCH_CONTROL		0xf60
#define PL310_POWER_CONTROL		0xf80
#define PL310_AUX_EVENT_MONITOR_BUS	0x100000

	.macro	l2base reg
	movw	\reg, #:lower16:s_cpuL2RegisterBase
	movt	\reg, #:upper16:s_cpuL2RegisterBase
	ldr	\reg, [\reg]
	.endm

#if defined(SYSMEM_SKBL)
/* name unknown: prefetching (r3p0 and later), the auxiliary control, clock
 * gating and the RAM latencies */
	.balign	4
	.global	CpuL2CacheSetup
	.type	CpuL2CacheSetup, %function
	.thumb_func
CpuL2CacheSetup:
	l2base	r0
	ldr	r1, [r0]
	ands.w	r1, r1, #0x3f
	cmp	r1, #6
	movw	r2, #0x0001
	movt	r2, #0x6009
	blt	2f
	movw	r2, #0x0001
	movt	r2, #0x6009
	movw	r3, #0x0000
	movt	r3, #0x2000
	cmp	r1, #7
	blt	1f
	movw	r2, #0x0001
	movt	r2, #0x6009
	movw	r3, #0x0000
	movt	r3, #0x2100
1:	str.w	r3, [r0, #PL310_PREFETCH_CONTROL]
2:	str.w	r2, [r0, #PL310_AUX_CONTROL]
	mov.w	r1, #2
	str.w	r1, [r0, #PL310_POWER_CONTROL]
	mov.w	r1, #0
	str.w	r1, [r0, #PL310_TAG_RAM_CONTROL]
	movw	r1, #0x131
	str.w	r1, [r0, #PL310_DATA_RAM_CONTROL]
	dsb	sy
	bx	lr
	.balign	4
	.size	CpuL2CacheSetup, . - CpuL2CacheSetup

/* name unknown: the event monitor bus, for the performance monitors */
	.global	CpuL2CacheSetEventMonitorBus
	.type	CpuL2CacheSetEventMonitorBus, %function
	.thumb_func
CpuL2CacheSetEventMonitorBus:
	l2base	r1
	ldr.w	r2, [r1, #PL310_AUX_CONTROL]
	cmp	r0, #0
	ite	ne
	orrne	r2, r2, #PL310_AUX_EVENT_MONITOR_BUS
	biceq	r2, r2, #PL310_AUX_EVENT_MONITOR_BUS
	str.w	r2, [r1, #PL310_AUX_CONTROL]
	dsb	sy
	bx	lr
	.size	CpuL2CacheSetEventMonitorBus, . - CpuL2CacheSetEventMonitorBus

/* name unknown */
	.global	CpuL2CacheEnable
	.type	CpuL2CacheEnable, %function
	.thumb_func
CpuL2CacheEnable:
	l2base	r0
	mov.w	r1, #1
	str.w	r1, [r0, #PL310_CONTROL]
	dsb	sy
	bx	lr
	.size	CpuL2CacheEnable, . - CpuL2CacheEnable

/* name unknown: drains the controller before turning it off */
	.global	CpuL2CacheDisable
	.type	CpuL2CacheDisable, %function
	.thumb_func
CpuL2CacheDisable:
	l2base	r0
	mov.w	r1, #0
	str.w	r1, [r0, #PL310_CACHE_SYNC]
1:	ldr.w	r1, [r0, #PL310_CACHE_SYNC]
	tst.w	r1, #1
	bne	1b
	mov.w	r1, #0
	str.w	r1, [r0, #PL310_CONTROL]
	dsb	sy
	bx	lr
	.balign	4
	.size	CpuL2CacheDisable, . - CpuL2CacheDisable
#endif

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
