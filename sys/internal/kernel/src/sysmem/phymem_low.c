/*
 * SceSysmem: the low-level physical page allocators. Each covers a physical
 * range in fixed units (256 KiB) with one bit per unit, set when the unit is
 * in use. There are two dynamic ones (main and sub, made by PhyMemLowCreate)
 * and a static one; the kernel picks them by memory type code.
 */

#include <sysmem/dipsw.h>
#include <sysmem/sysroot.h>
#include "sysmem_internal.h"

/* the memory type codes the exports take */
#define PHYMEM_LOW_TYPE_MAIN		0x10001
#define PHYMEM_LOW_TYPE_SUB		0x10002
#define PHYMEM_LOW_TYPE_STATIC		0x20001

/* PhyMemLowAlloc flags */
#define PHYMEM_LOW_ALLOC_FLAG_1		0x01	/* PLACEHOLDER: meaning unknown, always set */
#define PHYMEM_LOW_ALLOC_PARTIAL	0x02	/* a shorter run will do */

#define PHYMEM_LOW_UNIT_SHIFT		18
#define PHYMEM_LOW_BOOT_UNIT_SHIFT	20	/* the boot loader's allocators */

_Static_assert(sizeof(PhyMemLow) == 0x20, "PhyMemLow size");

/* told about every allocation and release */
typedef struct PhyMemLowHook {
	void (*onAlloc)(PhyMemLow *pml, uintptr_t pa, SceSize size, const void *lr);
	void (*onFree)(PhyMemLow *pml, uintptr_t offset, SceSize size, const void *lr);
} PhyMemLowHook;

/* a position in the bitmap */
typedef struct PhyMemLowCursor {
	SceUInt32 word;
	SceUInt32 *unit;
	SceUInt32 bit;
} PhyMemLowCursor;

#if FW_DEBUG
/* debug builds only: a tracker of physical allocations */
static const PhyMemLowHook *s_phyMemLowHook;
#define PHYMEM_LOW_HOOK(_call, ...) do {				\
	if (s_phyMemLowHook != NULL)					\
		s_phyMemLowHook->_call(__VA_ARGS__);			\
} while (0)
#else
#define PHYMEM_LOW_HOOK(_call, ...)	do { } while (0)
#endif
static PhyMemLow *s_phyMemLowMain;
static PhyMemLow *s_phyMemLowSub;
#if SYSMEM_NSKBL
static SceUInt32 *s_phyMemLowSubBitmap;
static SceUInt32 *s_phyMemLowMainBitmap;
#else
static PhyMemLow s_phyMemLowStatic;
static SceUInt32 s_phyMemLowStaticBitmap[16];
#endif

#define PhyMemLowRangeValid(pml, pa, size) \
	((pa) >= (pml)->base && !((pa) & (pml)->unitMask) && \
	 (size) <= (pml)->size && !((size) & (pml)->unitMask) && \
	 (pa) + (size) <= (pml)->size + (pml)->base)

/* name unknown: marks [pa, pa + size) used, if all of it is free */
#if SYSMEM_NSKBL
static	/* one caller, which GCC inlines it into, as in Sony's NSKBL */
#else
__attribute__((noinline))
#endif
int PhyMemLowMarkUsed(PhyMemLow *pml, uintptr_t pa, SceSize size)
{
	SceUInt32 *start, *unit, first, word, index, bit, b, count, n;
	SceUInt32 unitShift, *bitmap;
	uintptr_t base = pml->base;
	int flags, ret;

	if (base > pa || (pa & pml->unitMask) || pml->size < size ||
	    (size & pml->unitMask) || pml->size + base < pa + size)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	unitShift = pml->unitShift;
	bitmap = pml->bitmap;
	flags = sceKernelSpinlockLowLockCpuSuspendIntr(&pml->lock);
	index = (pa - base) >> unitShift;
	bit = index & 0x1f;
	start = &bitmap[index >> 5];
	count = size >> pml->unitShift;
	first = *start;

	/* every unit must be free */
	unit = start;
	word = first;
	b = bit;
	n = count;
	while (!(word & (1U << b))) {
		if (--n == 0)
			goto mark;
		if (b == 31) {
			word = *++unit;
			b = 0;
			if (word & 1)
				break;
		} else {
			b++;
		}
	}
	ret = SCE_KERNEL_ERROR_CANNOT_ALLOC_PHYMEMLOW;
	goto out;

mark:
	unit = start;
	word = first;
	for (;;) {
		word |= 1U << bit;
		*unit = word;
		if (--count == 0)
			break;
		if (bit == 31) {
			unit++;
			bit = 0;
			word = *unit;
		} else {
			bit++;
		}
	}
	ret = 0;
	PHYMEM_LOW_HOOK(onAlloc, pml, pa - base, size, __builtin_return_address(0));
out:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&pml->lock, flags);
	return ret;
}

/* name unknown: marks the units from start through (*lastWord, *lastBit) used */
__attribute__((noinline))
void PhyMemLowSetBits(const PhyMemLowCursor *start, const SceUInt32 *lastWord,
		      const SceUInt32 *lastBit)
{
	SceUInt32 word = start->word, *unit = start->unit, bit = start->bit, value = *unit;

	for (;;) {
		value |= 1 << bit;
		*unit = value;
		if (*lastWord == word && bit == *lastBit)
			break;
		if (bit == 31) {
			value = *++unit;
			word++;
			bit = 0;
		} else {
			bit++;
		}
	}
}

#if !SYSMEM_NSKBL
int SceSysmemForKernel_2C7B8D65(const PhyMemLowHook *hook)
{
#if FW_DEBUG
	s_phyMemLowHook = hook;
#endif
	return 0;
}
#endif

#if FW_DEBUG
/* prints the bitmap, one line per word */
__attribute__((noinline))
int PhyMemLowPrint(const PhyMemLow *pml)
{
	char line[33];
	const SceUInt32 *unit = pml->bitmap;
	uintptr_t pa = pml->base;
	SceUInt32 i, b;

	for (i = 0; i < pml->wordCount; i++, unit++) {
		SCE_DBG_PRINTF(0, 406, "\t%02d/%02d,0x%08x: *pUnit=0x%08x: ", i, pml->wordCount, pa,
			       *unit);
		for (b = 0; b != 32; b++)
			line[b] = (*unit & (1U << b)) ? '1' : '-';
		line[32] = '\0';
		SCE_DBG_PRINTF_WITH_INFO(0, 412, "%s\n", line);
		pa += pml->unitSize * 32;
	}
	return 0;
}
#endif

/*
 * Finds size bytes of free units starting at a multiple of alignment and
 * marks them used; with PHYMEM_LOW_ALLOC_PARTIAL the first aligned free run
 * is taken even if it is shorter. *allocated is what was taken.
 */
__attribute__((noinline))
int PhyMemLowAlloc(PhyMemLow *pml, SceSize size, SceSize alignment, SceUInt32 flags,
		   uintptr_t *pa, SceSize *allocated)
{
	PhyMemLowCursor cur, end;
	SceUInt32 baseUnit, alignUnits, partial;
	SceSize remaining, got;
	int intr, ret;

	if (size == 0 || (size & pml->unitMask) || pa == NULL || (alignment & pml->unitMask))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	*pa = 0;
	if (alignment == 0) {
		baseUnit = 0;
		alignUnits = 1;
	} else {
		baseUnit = pml->base >> pml->unitShift;
		alignUnits = alignment >> pml->unitShift;
	}
	if (flags & ~3)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	partial = flags & PHYMEM_LOW_ALLOC_PARTIAL;
	cur.word = 0;
	cur.bit = 0;
	cur.unit = pml->bitmap;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&pml->lock);

	for (;;) {
		/* the next free unit */
		if (cur.word == pml->wordCount)
			goto noSpace;
		if (cur.bit != 0) {
			if (cur.bit > 30) {
				cur.word++;
				cur.unit++;
				cur.bit = 0;
				continue;
			}
			cur.bit++;
			if (*cur.unit & (1 << cur.bit))
				continue;
		} else if (*cur.unit == 0xffffffff) {
			cur.word++;
			cur.unit++;
			continue;
		} else {
			cur.bit = __builtin_ctz(~*cur.unit);
		}

		/* aligned */
		while ((cur.bit + baseUnit + cur.word * 32) % alignUnits != 0) {
			if (cur.bit == 31) {
				if (++cur.word == pml->wordCount) {
					ret = SCE_KERNEL_ERROR_ERROR;
					goto out;
				}
				cur.unit++;
				cur.bit = 0;
			} else {
				cur.bit++;
			}
			if (*cur.unit & (1 << cur.bit))
				goto next;
		}

		/* as many free units as size needs */
		end = cur;
		remaining = size;
		got = 0;
		for (;;) {
			remaining -= pml->unitSize;
			got += pml->unitSize;
			if (remaining == 0)
				goto found;
			if (end.bit == 31) {
				if (++end.word == pml->wordCount) {
					ret = SCE_KERNEL_ERROR_ERROR;
					goto out;
				}
				end.unit++;
				end.bit = 0;
			} else {
				end.bit++;
			}
			if (*end.unit & (1 << end.bit))
				break;
		}
		if (partial)
			goto found;
		cur = end;
next:
		;
	}

found:
	PhyMemLowSetBits(&cur, &end.word, &end.bit);
	*pa = pml->base + ((cur.bit + cur.word * 32) << pml->unitShift);
	*allocated = got;
	ret = 0;
	PHYMEM_LOW_HOOK(onAlloc, pml, *pa, got, __builtin_return_address(0));
	goto out;

noSpace:
	ret = SCE_KERNEL_ERROR_CANNOT_ALLOC_PHYMEMLOW;
	SCE_DBG_PRINTF(6, 489, "Cannot allocate: pPhy %p, size0 0x%08x, alignment 0x%08x, %p\n",
		       pml, size, alignment, pa);
#if FW_DEBUG
	PhyMemLowPrint(pml);
#endif
out:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&pml->lock, intr);
	return ret;
}

#if !SYSMEM_NSKBL
/* name unknown: marks [pa, pa + size) free */
__attribute__((noinline))
int PhyMemLowFree(PhyMemLow *pml, uintptr_t pa, SceSize size)
{
	SceUInt32 *unit, word, index, bit, count;
	uintptr_t base = pml->base;
	int flags;

	if (base > pa || (pa & pml->unitMask) || pml->size < size ||
	    (size & pml->unitMask) || pml->size + base < pa + size)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	index = (pa - base) >> pml->unitShift;
	unit = pml->bitmap;
	flags = sceKernelSpinlockLowLockCpuSuspendIntr(&pml->lock);
	unit += index >> 5;
	bit = index & 0x1f;
	word = *unit;
	count = size >> pml->unitShift;
	for (;;) {
		word &= ~(1U << bit);
		*unit = word;
		if (--count == 0)
			break;
		if (bit == 31) {
			unit++;
			bit = 0;
			word = *unit;
		} else {
			bit++;
		}
	}
	PHYMEM_LOW_HOOK(onFree, pml, pa - base, size, __builtin_return_address(0));
	sceKernelSpinlockLowUnlockCpuResumeIntr(&pml->lock, flags);
	return 0;
}

/* name unknown: the free bytes */
__attribute__((noinline))
int PhyMemLowGetFreeSize(PhyMemLow *pml, SceSize *free)
{
	const SceUInt32 *unit = pml->bitmap;
	SceUInt32 i, b, units = 0;
	int flags;

	flags = sceKernelSpinlockLowLockCpuSuspendIntr(&pml->lock);
	for (i = 0; i != pml->wordCount; i++, unit++)
		for (b = 0; b != 32; b++)
			if (!(*unit & (1 << b)))
				units++;
	*free = units << pml->unitShift;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&pml->lock, flags);
	return 0;
}
#endif

/* name unknown: the main allocator */
int PhyMemLowMainAlloc(SceSize size, SceSize alignment, SceUInt32 flags, uintptr_t *pa,
		       SceSize *allocated)
{
	return PhyMemLowAlloc(s_phyMemLowMain, size, alignment, flags, pa, allocated);
}

int PhyMemLowMainMarkUsed(uintptr_t pa, SceSize size)
{
	return PhyMemLowMarkUsed(s_phyMemLowMain, pa, size);
}

#if !SYSMEM_NSKBL
int PhyMemLowMainFree(uintptr_t pa, SceSize size)
{
	return PhyMemLowFree(s_phyMemLowMain, pa, size);
}

/* name unknown: an allocator over [base, base + size), its bitmap from the kernel heap */
int PhyMemLowCreate(uintptr_t base, SceSize size, PhyMemLow **out)
{
	SceUInt32 words = size >> (PHYMEM_LOW_UNIT_SHIFT + 5), i;
	PhyMemLow *pml = SceSysmemForKernel_85571907(sizeof(*pml));
	SceUInt32 *bitmap = SceSysmemForKernel_85571907(words * sizeof(*bitmap));

	pml->base = base;
	pml->wordCount = words;
	pml->size = size;
	pml->bitmap = bitmap;
	pml->unitSize = 1 << PHYMEM_LOW_UNIT_SHIFT;
	pml->unitShift = PHYMEM_LOW_UNIT_SHIFT;
	pml->unitMask = (1 << PHYMEM_LOW_UNIT_SHIFT) - 1;
	pml->lock = 0;
	for (i = 0; i != words; i++)
		*bitmap++ = 0;
	*out = pml;
	return 0;
}

int PhyMemLowDestroy(PhyMemLow *pml)
{
	SceSysmemForKernel_4233C16D(pml);
	return 0;
}

/* name unknown: the static allocator */
int PhyMemLowStaticAlloc(SceSize size, SceSize alignment, uintptr_t *pa)
{
	SceSize allocated;

	return PhyMemLowAlloc(&s_phyMemLowStatic, size, alignment, PHYMEM_LOW_ALLOC_FLAG_1, pa,
			      &allocated);
}

int PhyMemLowStaticMarkUsed(uintptr_t pa, SceSize size)
{
	return PhyMemLowMarkUsed(&s_phyMemLowStatic, pa, size);
}

int PhyMemLowStaticFree(uintptr_t pa, SceSize size)
{
	return PhyMemLowFree(&s_phyMemLowStatic, pa, size);
}

/* physical memory of a type */
int SceSysmemForKernel_EA98CCE1(SceUInt32 type, SceSize size, SceSize alignment, uintptr_t *pa)
{
	if (type == PHYMEM_LOW_TYPE_SUB) {
		return PhyMemLowAlloc(s_phyMemLowSub, size, alignment,
				      PHYMEM_LOW_ALLOC_FLAG_1, pa, &size);
	} else if (type == PHYMEM_LOW_TYPE_STATIC) {
		SceSize allocated;

		return PhyMemLowAlloc(&s_phyMemLowStatic, size, alignment,
				      PHYMEM_LOW_ALLOC_FLAG_1, pa, &allocated);
	} else if (type == PHYMEM_LOW_TYPE_MAIN) {
		SceSize allocated;

		return PhyMemLowAlloc(s_phyMemLowMain, size, alignment,
				      PHYMEM_LOW_ALLOC_FLAG_1, pa, &allocated);
	} else {
		return SCE_KERNEL_ERROR_UNKNOWN_PHYMEMLOW_TYPE;
	}
}

int SceSysmemForKernel_844DF3DA(SceUInt32 type, uintptr_t pa, SceSize size)
{
	if (type == PHYMEM_LOW_TYPE_SUB)
		return PhyMemLowFree(s_phyMemLowSub, pa, size);
	if (type == PHYMEM_LOW_TYPE_STATIC)
		return PhyMemLowFree(&s_phyMemLowStatic, pa, size);
	if (type == PHYMEM_LOW_TYPE_MAIN)
		return PhyMemLowFree(s_phyMemLowMain, pa, size);
	return SCE_KERNEL_ERROR_UNKNOWN_PHYMEMLOW_TYPE;
}

/* the sub allocator's memory is mapped 1:1 at +1 GiB */
int SceSysmemForKernel_2CFD8C9A(SceUInt32 type, uintptr_t pa, uintptr_t *va)
{
	if (type != PHYMEM_LOW_TYPE_SUB)
		return SCE_KERNEL_ERROR_UNKNOWN_PHYMEMLOW_TYPE;
	*va = pa + 0x40000000;
	return 0;
}

int SceSysmemForKernel_05822B82(SceUInt32 type, SceSize *free)
{
	if (type == PHYMEM_LOW_TYPE_MAIN)
		return PhyMemLowGetFreeSize(s_phyMemLowMain, free);
	return SCE_KERNEL_ERROR_UNKNOWN_PHYMEMLOW_TYPE;
}

/* name unknown: the main and sub allocators, made before sysmem starts */
int PhyMemLowModuleStart(const UIDManager *manager)
{
#ifndef SYSMEM_TZS	/* the secure world has no CDRAM allocators */
	s_phyMemLowMain = manager->phyMemLowMain;
	s_phyMemLowSub = manager->phyMemLowSub;
#endif
	return 0;
}

/* name unknown */
int PhyMemLowModuleStartPhase2(const UIDManager *manager)
{
	return 0;
}

/* name unknown: the static allocator over the 128 MiB of CDRAM, all free */
int PhyMemLowStaticInit(void)
{
	SceUInt32 *bitmap = s_phyMemLowStaticBitmap;

	s_phyMemLowStatic.bitmap = s_phyMemLowStaticBitmap;
	s_phyMemLowStatic.base = 0x20000000;
	s_phyMemLowStatic.size = 0x8000000;
	s_phyMemLowStatic.unitSize = 1 << PHYMEM_LOW_UNIT_SHIFT;
	s_phyMemLowStatic.unitShift = PHYMEM_LOW_UNIT_SHIFT;
	s_phyMemLowStatic.unitMask = (1 << PHYMEM_LOW_UNIT_SHIFT) - 1;
	s_phyMemLowStatic.wordCount = 16;
	s_phyMemLowStatic.lock = 0;
	do
		*bitmap++ = 0;
	while (bitmap != &s_phyMemLowStaticBitmap[16]);
	return 0;
}
#else
/* name unknown: the main and sub allocators in 1 MiB units, made by core 0 */
int PhyMemLowStart(UIDManager *manager)
{
	PhyMemLow *pml;
	SceUInt32 *bitmap;
	SceUInt32 i;
	SceSize size, allocated;
	uintptr_t pa;

	sceKernelSysrootCorelockLock(0);
	if (sceKernelCpuId() == 0) {
		s_phyMemLowMain = sceKernelSysrootAlloc(sizeof(PhyMemLow));
		s_phyMemLowMainBitmap = sceKernelSysrootAlloc(32 * sizeof(SceUInt32));
		manager->phyMemLowMain = s_phyMemLowMain;
		if (sceKernelCheckDipsw(SCE_DIPSW_ENABLE_TOOL_PHYMEMPART))
			size = 0x40000000;
		else
			size = 0x20000000;
		bitmap = s_phyMemLowMainBitmap;
		pml = s_phyMemLowMain;
		pml->size = size;
		pml->wordCount = size >> (PHYMEM_LOW_BOOT_UNIT_SHIFT + 5);
		pml->lock = 0;
		pml->bitmap = bitmap;
		pml->base = 0x40000000;
		pml->unitSize = 1 << PHYMEM_LOW_BOOT_UNIT_SHIFT;
		pml->unitShift = PHYMEM_LOW_BOOT_UNIT_SHIFT;
		pml->unitMask = (1 << PHYMEM_LOW_BOOT_UNIT_SHIFT) - 1;
		for (i = 0; i < pml->wordCount; i++)
			bitmap[i] = 0;
		s_phyMemLowSub = sceKernelSysrootAlloc(sizeof(PhyMemLow));
		s_phyMemLowSubBitmap = sceKernelSysrootAlloc(16 * sizeof(SceUInt32));
		manager->phyMemLowSub = s_phyMemLowSub;
		bitmap = s_phyMemLowSubBitmap;
		pml = s_phyMemLowSub;
		pml->bitmap = bitmap;
		pml->base = 0x80000000;
		pml->size = 0x20000000;
		pml->unitSize = 1 << PHYMEM_LOW_BOOT_UNIT_SHIFT;
		pml->unitShift = PHYMEM_LOW_BOOT_UNIT_SHIFT;
		pml->unitMask = (1 << PHYMEM_LOW_BOOT_UNIT_SHIFT) - 1;
		pml->wordCount = 16;
		pml->lock = 0;
		for (i = 0; i < pml->wordCount; i++)
			bitmap[i] = 0;
		/* the boot loader's own first 2 MiB */
		PhyMemLowAlloc(s_phyMemLowMain, 0x200000, 0, PHYMEM_LOW_ALLOC_FLAG_1, &pa, &allocated);
	}
	sceKernelSysrootCorelockUnlock();
	return 0;
}
#endif
