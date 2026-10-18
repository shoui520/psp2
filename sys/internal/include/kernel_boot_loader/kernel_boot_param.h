/*
 * The kernel boot parameters: what the boot loaders (SKBL, then NSKBL) fill
 * in for the kernel and hand to SceSysmem as its argument. 0x310 bytes,
 * the same layout in 3.6x and 4.00. The layout and most names are the
 * henkaku wiki's (NSKBL page); the words after the reset vector are named
 * from SceSysmem's readers.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>

struct SceKblParam;
struct SceCorelockContext;

#define SCE_KERNEL_BOOT_PARAM_MAGIC	0x7f407c30
#define SCE_KERNEL_BOOT_MEMORY_MAX	4
#define SCE_KERNEL_BOOT_CPU_MAX		4

/* a physical memory range the kernel may use */
typedef struct SceKernelBootMemory {
	SceUIntPtr pbase;
	SceSize psize;
} SceKernelBootMemory;

/* a mapping the boot loader made for the kernel; vsize = psize + extraHigh */
typedef struct SceKernelBootMapping {
	const char *name;
	SceUIntPtr pbase;
	SceUIntPtr vbase;
	SceSize psize;
	SceSize vsize;
	SceSize extraHigh;
} SceKernelBootMapping;

/* each CPU's translation settings and boot stack */
typedef struct SceKernelBootCpu {
	uint32_t reserved[2];
	uint32_t ttbcr;
	uint32_t dacr;
	uint32_t contextId;
	SceSize stackSize;
	SceUID stackBlockId;
	void *stackBottom;
} SceKernelBootCpu;

typedef struct SceKernelBootParam {
	SceSize size;
	SceBool secure;				/* SKBL: 1, NSKBL: 0 */
	SceUInt32 numMemory;
	SceKernelBootMemory memory[SCE_KERNEL_BOOT_MEMORY_MAX];
	struct SceKblParam *kblParam;		/* 0x2c */
	uint32_t reserved0[5];
	uint32_t socRevision;			/* 0x44: ScePervasiveMisc revision */
	uint32_t socCacheStatus;		/* ScePervasiveMisc cacheStatus */
	uint32_t kermitRevision;		/* socRevision & 0x1ffff */
	uint32_t hardwareDependent[2];		/* 0x50: by the KBL Param's hardware info */
	SceKernelBootMapping ttbr0;		/* 0x58 */
	SceUIntPtr ttbr0MaxAddress;		/* the last address TTBR0 translates */
	SceSize ttbr0Size;
	SceKernelBootMapping ttbr1;		/* 0x78 */
	SceSize ttbr1Size;
	SceUIntPtr l2PageTable000MapBase;	/* 0x94: the first address SceKernelL2PageTable000 maps */
	SceKernelBootMapping reset;		/* 0x98 */
	SceKernelBootMapping exceptionEntry;	/* 0xb0 */
	SceKernelBootMapping l2PageTable000;	/* 0xc8 */
	SceKernelBootMapping l2Vector;		/* 0xe0 */
	SceKernelBootMapping sysroot;		/* 0xf8 */
	SceKernelBootMapping fixedHeap32B;	/* 0x110 */
	SceKernelBootMapping fixedHeap48B;
	SceKernelBootMapping fixedHeap64B;
	SceKernelBootMapping fixedHeapUIDEntry;
	SceKernelBootMapping fixedHeapForL2Object;	/* 0x170 */
	SceKernelBootMapping reserved1;
	SceKernelBootMapping phyPageTable;	/* 0x1a0 */
	SceKernelBootMapping phyPageTableHigh;
	SceKernelBootMapping bootKernelImage;	/* 0x1d0 */
	SceKernelBootMapping hardwareRegisters;	/* 0x1e8: physical = virtual */
	struct SceCorelockContext *corelock;	/* 0x200 */
	SceKernelBootCpu cpu[SCE_KERNEL_BOOT_CPU_MAX];	/* 0x204 */
	void *pSysroot;				/* 0x284 */
	uint32_t reserved2;
	void *pL2PageTable000;
	SceUIntPtr resetVector;			/* 0x290: VBAR, exceptionEntry.vbase + 0x100 */
	uint32_t reserved3[26];			/* SceSysmem's boot objects */
	uintptr_t debugPutchar;			/* 0x2fc: the boot loader's putchar */
	uintptr_t debugPrintf;			/* 0x300: the boot loader's printf */
	uintptr_t debugPutcharHandler;		/* the putchar handler registered with it */
	uint32_t debugPrintLevel;		/* 0x308 */
	uint32_t magic;				/* SCE_KERNEL_BOOT_PARAM_MAGIC */
} SceKernelBootParam;

_Static_assert(offsetof(SceKernelBootParam, kblParam) == 0x2c, "boot param KBL Param");
_Static_assert(offsetof(SceKernelBootParam, socRevision) == 0x44, "boot param SoC revision");
_Static_assert(offsetof(SceKernelBootParam, ttbr0) == 0x58, "boot param TTBR0");
_Static_assert(offsetof(SceKernelBootParam, ttbr1Size) == 0x90, "boot param TTBR1 size");
_Static_assert(offsetof(SceKernelBootParam, reset) == 0x98, "boot param reset");
_Static_assert(offsetof(SceKernelBootParam, sysroot) == 0xf8, "boot param sysroot");
_Static_assert(offsetof(SceKernelBootParam, phyPageTable) == 0x1a0, "boot param phy page table");
_Static_assert(offsetof(SceKernelBootParam, hardwareRegisters) == 0x1e8, "boot param registers");
_Static_assert(offsetof(SceKernelBootParam, corelock) == 0x200, "boot param corelock");
_Static_assert(offsetof(SceKernelBootParam, cpu[1]) == 0x224, "boot param CPU");
_Static_assert(offsetof(SceKernelBootParam, pSysroot) == 0x284, "boot param sysroot pointer");
_Static_assert(offsetof(SceKernelBootParam, resetVector) == 0x290, "boot param reset vector");
_Static_assert(offsetof(SceKernelBootParam, debugPutchar) == 0x2fc, "boot param putchar");
_Static_assert(offsetof(SceKernelBootParam, debugPrintLevel) == 0x308, "boot param print level");
_Static_assert(sizeof(SceKernelBootParam) == 0x310, "boot param size");
