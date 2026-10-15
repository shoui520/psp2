/*
 * SceSysmem: the MMU. Builds the short-descriptor page tables (L1 sections
 * and supersections, L2 large and small pages), switches the translation
 * context (TTBR1, DACR, CONTEXTIDR) and answers address queries, either with
 * the hardware address translation (ATS1C*, PAR) or by walking the tables.
 *
 * The tables live in memory the kernel maps read-only; every store to them
 * opens the page-table domain for that one write.
 */

#include "sce/cp15.h"
#include <stddef.h>
#include <stdint.h>
#include <psp2kern/types.h>
#include "fw.h"
#include <sce/debug.h>
#include <sce/error.h>
#include "mmu.h"
#include "cpu_irq.h"
#include <sce/barrier.h>

#undef __FILE_NAME__
#define __FILE_NAME__ "mmu.c"

int ksceKernelCpuSuspendIntr(void);
int ksceKernelCpuResumeIntr(int flags);

/* PLACEHOLDER: Sony's name for 0x8002200b is unknown */

/* the DACR while a page table is written, and every domain as client */
#define MMU_DACR_PAGE_TABLE_WRITE	0x17450000

/* TTBR1 walk attributes: inner/outer write-back write-allocate, shareable */
#define MMU_TTBR_ATTR			0x4a

/* an empty L1 entry: fault, domain 15 */
#define MMU_L1_FAULT			0x1e0

/* the descriptor type in bits 1:0 */
#define MMU_L1_TYPE(_e)			((_e) & 3)
#define MMU_L1_TYPE_FAULT		0
#define MMU_L1_TYPE_PAGE_TABLE		1
#define MMU_L1_TYPE_SECTION		2
#define MMU_L1_SUPERSECTION		0x40000

#define MMU_L2_TYPE(_e)			((_e) & 3)
#define MMU_L2_TYPE_FAULT		0
#define MMU_L2_TYPE_LARGE_PAGE		1

/* the address-translation request: which accesses must be permitted */
#define MMU_ACCESS_PRIV_READ		0x01
#define MMU_ACCESS_PRIV_WRITE		0x02
#define MMU_ACCESS_USER_READ		0x10
#define MMU_ACCESS_USER_WRITE		0x20
#define MMU_ACCESS_ANY			0x33

/* VAtoPA flags: keep the current domain access instead of all-client */
#define MMU_VATOPA_KEEP_DACR		0x01

/* the memory types (memtype codes) the queries report */
#define MMU_MEMORY_TYPE_SO		0x02
#define MMU_MEMORY_TYPE_DEVICE		0x08
#define MMU_MEMORY_TYPE_NC		0x40
#define MMU_MEMORY_TYPE_SO_80		0x80
#define MMU_MEMORY_TYPE_NORMAL		0xd0

/*
 * ARMv7-A short-descriptor bits. AP[2:0]: 1 privileged RW, 3 all RW,
 * 5 privileged RO, 7 all RO.
 */
#define MMU_AP_PRIV_RW				1
#define MMU_AP_ALL_RW				3
#define MMU_AP_PRIV_RO				5
#define MMU_AP_ALL_RO				7

#define MMU_L1_B				0x00004
#define MMU_L1_C				0x00008
#define MMU_L1_XN				0x00010
#define MMU_L1_DOMAIN(domain)			(((domain) << 5) & 0x1e0)
#define MMU_L1_AP(ap)				((((ap) & 4) << 13) | (((ap) & 3) << 10))
#define MMU_L1_TEX(tex)				((tex) << 12)
#define MMU_L1_S				0x10000
#define MMU_L1_NG				0x20000

/* small and large page (B, C, AP, S and nG sit alike) */
#define MMU_L2_SMALL_XN				0x0001
#define MMU_L2_TYPE_SMALL_PAGE			0x0002
#define MMU_L2_B				0x0004
#define MMU_L2_C				0x0008
#define MMU_L2_AP(ap)				((((ap) & 4) << 7) | (((ap) & 3) << 4))
#define MMU_L2_SMALL_TEX(tex)			((tex) << 6)
#define MMU_L2_S				0x0400
#define MMU_L2_NG				0x0800
#define MMU_L2_LARGE_TEX(tex)			((tex) << 12)
#define MMU_L2_LARGE_XN				0x8000

/* set once the kernel's address spaces exist */
static int s_mmuReady;

static inline int MMUInterruptsDisabled(void)
{
	uint32_t cpsr;

	cpsr = readCpsr();
	return (cpsr & PSR_I_BIT) != 0;
}

/* cpu.c's CpuSuspendIntrLow/CpuResumeIntrLow, inlined */
static inline int MMUSuspendIntr(void)
{
	uint32_t status;

	status = readCpsr();
	status &= PSR_I_BIT;
	if (!status)
		disableIrq();
	return status;
}

static inline void MMUResumeIntr(int flags)
{
	if (!(flags & PSR_I_BIT))
		enableIrq();
}

/* ATS1CPR/ATS1CPW/ATS1CUR/ATS1CUW, then the result from PAR */
#define MMU_ATS(_op2, ats)						\
static inline uint32_t MMUTranslate##_op2(uintptr_t va)			\
{									\
	ats(va);							\
	return readPar();						\
}
MMU_ATS(0, writeAts1cpr)
MMU_ATS(1, writeAts1cpw)
MMU_ATS(2, writeAts1cur)
MMU_ATS(3, writeAts1cuw)

/* a translated address: the base from PAR, the offset from the VA */
typedef union MMUPhysicalAddress {
	uint32_t value;
	struct {
		uint32_t offset : 12;
		uint32_t base : 20;
	} page;
	struct {
		uint32_t offset : 24;
		uint32_t base : 8;
	} supersection;
} MMUPhysicalAddress;

#define PAR_FAULT			0x01
#define PAR_SUPERSECTION		0x02

/* one page-table write with the table's domain opened */
static inline void MMUWritePTE(volatile uint32_t *pte, uint32_t value)
{
	uint32_t dacr = readDacr();

	writeDacr(MMU_DACR_PAGE_TABLE_WRITE);
	*pte = value;
	writeDacr(dacr);
}

/* TLB maintenance after an unmap: user space for every ASID */
static inline void MMUInvalidateTLBEntry(uintptr_t va, uint32_t asid)
{
	uintptr_t page = va >> 12;

	if (va >= 0x40000000)
		writeTlbimvais(asid | page << 12);
	else
		writeTlbimvaais(page << 12);
}

#define MMU_DSB()	dsb()
#define MMU_ISB()	isb()

/* name unknown: marks the MMU ready for MMUIsValidMap */
__attribute__((noinline))
int MMUSetReady(void)
{
	s_mmuReady = 1;
	return 0;
}

/* name unknown */
int MMUInitPost(void)
{
	MMUSetReady();
	return 0;
}

/* the L1 section attributes of a memblock type, domain in bits 8:5 */
__attribute__((noinline))
uint32_t MemBlockTypeToL1PTE(uint32_t type, uint32_t domain)
{
	uint32_t attr = (type & MEMBLOCK_CODE_GLOBAL) ? MMU_L1_S : MMU_L1_S | MMU_L1_NG;

	switch (MEMBLOCK_CODE_MEMORY_TYPE(type)) {
	case MEMBLOCK_CODE_L1WBWA:
		attr |= MMU_L1_TEX(4) | MMU_L1_B;
		break;
	case MEMBLOCK_CODE_SO:
		break;
	case MEMBLOCK_CODE_DEVICE:
		attr |= MMU_L1_B;
		break;
	case MEMBLOCK_CODE_NORMAL_NC:
		attr &= ~MMU_L1_S;
		attr |= MMU_L1_TEX(1);
		break;
	case MEMBLOCK_CODE_NORMAL:
		attr |= MMU_L1_TEX(1) | MMU_L1_C | MMU_L1_B;
		break;
	default:
		SCE_PANIC(441);
	}
	switch (MEMBLOCK_CODE_ACCESS(type)) {
	case MEMBLOCK_ACCESS_KERNEL_RWX:
		attr |= MMU_L1_AP(MMU_AP_PRIV_RW);
		break;
	case MEMBLOCK_ACCESS_KERNEL_RX:
		attr |= MMU_L1_AP(MMU_AP_PRIV_RO);
		break;
	case MEMBLOCK_ACCESS_KERNEL_RW:
		attr |= MMU_L1_AP(MMU_AP_PRIV_RW) | MMU_L1_XN;
		break;
	case MEMBLOCK_ACCESS_KERNEL_R:
		attr |= MMU_L1_AP(MMU_AP_PRIV_RO) | MMU_L1_XN;
		break;
	case MEMBLOCK_ACCESS_USER_RX:
		attr |= MMU_L1_AP(MMU_AP_ALL_RO);
		break;
	case MEMBLOCK_ACCESS_USER_RW:
		attr |= MMU_L1_AP(MMU_AP_ALL_RW) | MMU_L1_XN;
		break;
	case MEMBLOCK_ACCESS_USER_R:
		attr |= MMU_L1_AP(MMU_AP_ALL_RO) | MMU_L1_XN;
		break;
	default:
		SCE_PANIC(491);
	}
	return MMU_L1_DOMAIN(domain) | attr;
}

/* the L2 small-page attributes of a memblock type */
__attribute__((noinline))
uint32_t MemBlockTypeToL2PTESmallPage(uint32_t type)
{
	uint32_t attr = MMU_L2_S | MMU_L2_TYPE_SMALL_PAGE, base;

	if (type & MEMBLOCK_CODE_GLOBAL) {
		base = MMU_L2_S;
	} else {
		attr = MMU_L2_S | MMU_L2_NG | MMU_L2_TYPE_SMALL_PAGE;
		base = MMU_L2_S | MMU_L2_NG;
	}
	switch (MEMBLOCK_CODE_MEMORY_TYPE(type)) {
	case MEMBLOCK_CODE_L1WBWA:
		attr = base | MMU_L2_SMALL_TEX(4) | MMU_L2_B | MMU_L2_TYPE_SMALL_PAGE;
		break;
	case MEMBLOCK_CODE_SO:
		break;
	case MEMBLOCK_CODE_DEVICE:
		attr = base | MMU_L2_B | MMU_L2_TYPE_SMALL_PAGE;
		break;
	case MEMBLOCK_CODE_NORMAL_NC:
		attr = (base & ~MMU_L2_S) | MMU_L2_SMALL_TEX(1) | MMU_L2_TYPE_SMALL_PAGE;
		break;
	case MEMBLOCK_CODE_NORMAL:
		attr = base | MMU_L2_SMALL_TEX(1) | MMU_L2_C | MMU_L2_B | MMU_L2_TYPE_SMALL_PAGE;
		break;
	default:
		SCE_PANIC(1223);
	}
	switch (MEMBLOCK_CODE_ACCESS(type)) {
	case MEMBLOCK_ACCESS_KERNEL_RWX:
		return attr | MMU_L2_AP(MMU_AP_PRIV_RW);
	case MEMBLOCK_ACCESS_KERNEL_RX:
		return attr | MMU_L2_AP(MMU_AP_PRIV_RO);
	case MEMBLOCK_ACCESS_KERNEL_RW:
		return attr | MMU_L2_AP(MMU_AP_PRIV_RW) | MMU_L2_SMALL_XN;
	case MEMBLOCK_ACCESS_KERNEL_R:
		return attr | MMU_L2_AP(MMU_AP_PRIV_RO) | MMU_L2_SMALL_XN;
	case MEMBLOCK_ACCESS_USER_RX:
		return attr | MMU_L2_AP(MMU_AP_ALL_RO);
	case MEMBLOCK_ACCESS_USER_RW:
		return attr | MMU_L2_AP(MMU_AP_ALL_RW) | MMU_L2_SMALL_XN;
	case MEMBLOCK_ACCESS_USER_R:
		return attr | MMU_L2_AP(MMU_AP_ALL_RO) | MMU_L2_SMALL_XN;
	}
	SCE_PANIC(1272);
}

/* the L2 large-page attributes of a memblock type (inlined into its caller) */
static inline __attribute__((always_inline))
uint32_t MemBlockTypeToL2PTELargePage(uint32_t type)
{
	uint32_t attr = MMU_L2_S | MMU_L2_NG | MMU_L2_TYPE_LARGE_PAGE, base;
	uint32_t code = MEMBLOCK_CODE_MEMORY_TYPE(type);

	if (type & MEMBLOCK_CODE_GLOBAL) {
		attr = MMU_L2_S | MMU_L2_TYPE_LARGE_PAGE;
		base = MMU_L2_S;
	} else {
		base = MMU_L2_S | MMU_L2_NG;
	}
	switch (code) {
	case MEMBLOCK_CODE_L1WBWA:
		attr = base | MMU_L2_LARGE_TEX(4) | MMU_L2_B | MMU_L2_TYPE_LARGE_PAGE;
		break;
	case MEMBLOCK_CODE_SO:
		break;
	case MEMBLOCK_CODE_DEVICE:
		attr = base | MMU_L2_B | MMU_L2_TYPE_LARGE_PAGE;
		break;
	case MEMBLOCK_CODE_NORMAL_NC:
		attr = (base & ~MMU_L2_S) | MMU_L2_LARGE_TEX(1) | MMU_L2_TYPE_LARGE_PAGE;
		break;
	case MEMBLOCK_CODE_NORMAL:
		attr = base | MMU_L2_LARGE_TEX(1) | MMU_L2_C | MMU_L2_B | MMU_L2_TYPE_LARGE_PAGE;
		break;
	default:
		SCE_DBG_PRINTF_FUNC(0xb, 1120, "MemBlockTypeToL2PTELargePage",
				    "Unknown type code= 0x%08x/%08x\n", type, code);
		SCE_PANIC_FUNC(1121, "MemBlockTypeToL2PTELargePage");
	}
	switch (MEMBLOCK_CODE_ACCESS(type)) {
	case MEMBLOCK_ACCESS_KERNEL_RWX:
		return attr | MMU_L2_AP(MMU_AP_PRIV_RW);
	case MEMBLOCK_ACCESS_KERNEL_RX:
		return attr | MMU_L2_AP(MMU_AP_PRIV_RO);
	case MEMBLOCK_ACCESS_KERNEL_R:
		return attr | MMU_L2_AP(MMU_AP_PRIV_RO) | MMU_L2_LARGE_XN;
	case MEMBLOCK_ACCESS_KERNEL_RW:
		return attr | MMU_L2_AP(MMU_AP_PRIV_RW) | MMU_L2_LARGE_XN;
	case MEMBLOCK_ACCESS_USER_RX:
		return attr | MMU_L2_AP(MMU_AP_ALL_RO);
	case MEMBLOCK_ACCESS_USER_RW:
		return attr | MMU_L2_AP(MMU_AP_ALL_RW) | MMU_L2_LARGE_XN;
	case MEMBLOCK_ACCESS_USER_R:
		return attr | MMU_L2_AP(MMU_AP_ALL_RO) | MMU_L2_LARGE_XN;
	}
	SCE_PANIC_FUNC(1173, "MemBlockTypeToL2PTELargePage");
}

#if FW_DEBUG
__attribute__((noinline))
void AlignCheck(uintptr_t address, SceSize size)
{
	SCE_DBG_PRINTF(0xb, 179, "align ? addr 0x%08x size 0x%08x\n", address, size);
}
#define MMU_ALIGN_CHECK(_address, _size, _mask) do {			\
	if (((_address) | (_size)) & (_mask))				\
		AlignCheck((_address), (_size));				\
} while (0)
#else
#define MMU_ALIGN_CHECK(_address, _size, _mask)	do { } while (0)
#endif

/* the memory type a PAR result reports */
__attribute__((noinline))
int PAR2MemoryType(uint32_t par)
{
	switch (par & 0x70) {
	case 0x10:
		return MMU_MEMORY_TYPE_SO;
	case 0x00:
		return MMU_MEMORY_TYPE_SO_80;
	case 0x30:
		return MMU_MEMORY_TYPE_DEVICE;
	case 0x50:
		switch (par & 0xc) {
		case 0:
			return MMU_MEMORY_TYPE_NC;
		case 4:
			return MMU_MEMORY_TYPE_NORMAL;
		}
		SCE_DBG_ASSERT_FAIL(1780);
		return SCE_KERNEL_ERROR_VA2PA_FAULT;
	}
	SCE_DBG_ASSERT_FAIL(1786);
	return SCE_KERNEL_ERROR_VA2PA_FAULT;
}

/*
 * name unknown: the physical address of va, by the hardware translation of
 * the first access in mode that is permitted. Returns 1 for a section or
 * page, 2 for a supersection.
 */
__attribute__((noinline))
int MMUVAtoPACore(uint32_t mode, uint32_t flags, uintptr_t va, uintptr_t *pa,
		  int *memoryType)
{
	uint32_t dacr = readDacr(), par;
	MMUPhysicalAddress address;
	int type, supersection;

	if (!(flags & MMU_VATOPA_KEEP_DACR))
		writeDacr(DACR_ALL_CLIENT);
	par = MMUTranslate0(va);
	if (!(mode & MMU_ACCESS_PRIV_READ & ~par))
		goto privWrite;
found:
	writeDacr(dacr);
	if (memoryType != NULL) {
		type = PAR2MemoryType(par);
		*memoryType = type;
		if (type < 0) {
			*pa = 0;
			*memoryType = 0;
			return type;
		}
	}
	/* PAR holds the page (or supersection) base; the offset comes from va */
	address.value = par;
	address.page.offset = va;
	supersection = (par >> 1) & 1;
	if (supersection)
		address.supersection.offset = va;
	*pa = address.value;
	return supersection + 1;

privWrite:
	par = MMUTranslate1(va);
	if ((mode & ~(par << 1)) & MMU_ACCESS_PRIV_WRITE)
		goto found;
	par = MMUTranslate2(va);
	if ((mode & ~(par << 4)) & MMU_ACCESS_USER_READ)
		goto found;
	par = MMUTranslate3(va);
	if ((mode & ~(par << 5)) & MMU_ACCESS_USER_WRITE)
		goto found;
	writeDacr(dacr);
	*pa = 0;
	if (memoryType != NULL)
		*memoryType = 0;
	return SCE_KERNEL_ERROR_VA2PA_FAULT;
}

/* name unknown: the current translation context */
__attribute__((noinline))
int MMUContextSave(MMUContext *context)
{
	context->ttbr1 = readTtbr1();
	context->dacr = readDacr();
	context->contextId = readContextId();
	return 0;
}

/* name unknown: back to a saved translation context */
__attribute__((noinline))
int MMUContextRestore(const MMUContext *context)
{
	int flags = MMUSuspendIntr();
	uint32_t ttbr1 = context->ttbr1 | MMU_TTBR_ATTR;
	uint32_t dacr = context->dacr & DACR_ALL_CLIENT;
	uint32_t contextId = readContextId();

	/* no ASID while TTBR1 changes */
	writeContextId(0);
	MMU_ISB();
	writeTtbr1(ttbr1);
	MMU_ISB();
	writeContextId((contextId & ~0xff) | (SceUInt8)context->contextId);
	writeDacr(dacr);
	MMUResumeIntr(flags);
	return 0;
}

/* name unknown: into another context when its ASID differs, saving the current one */
__attribute__((noinline))
int MMUContextSwitch(const MMUContext *context, MMUContext *saved, uint32_t *contextId)
{
	uint32_t current;

	if (context == NULL)
		return 0;
	current = readContextId();
	*contextId = current;
	if (!((current ^ context->contextId) & 0xff))
		return 0;
	saved->ttbr1 = readTtbr1();
	saved->dacr = readDacr();
	saved->contextId = readContextId();
	MMUContextRestore(context);
	return 0;
}

/* name unknown: the same with barriers, for a context that was just changed */
__attribute__((noinline))
int MMUContextSwitchSync(const MMUContext *context, MMUContext *saved)
{
	int flags = MMUSuspendIntr();
	uint32_t ttbr1, dacr, contextId;

	if (saved != NULL) {
		saved->ttbr1 = readTtbr1();
		saved->dacr = readDacr();
		saved->contextId = readContextId();
	}
	ttbr1 = context->ttbr1 | MMU_TTBR_ATTR;
	contextId = (readContextId() & ~0xff) | (SceUInt8)context->contextId;
	dacr = context->dacr & DACR_ALL_CLIENT;
	MMU_DSB();
	writeContextId(0);
	MMU_ISB();
	writeTtbr1(ttbr1);
	MMU_ISB();
	MMU_DSB();
	writeContextId(contextId);
	writeDacr(dacr);
	MMUResumeIntr(flags);
	return 0;
}

int sceKernelMMUL1Init(uint32_t *l1)
{
	uint32_t *end;
	int flags;

	if ((uintptr_t)l1 & 0xfff) {
		SCE_DBG_PRINTF(0xb, 591, "l1base=%p\n", l1);
		SCE_PANIC(592);
	}
	flags = ksceKernelCpuSuspendIntr();
	end = l1 + 0x1000;
	do
		MMUWritePTE(l1++, MMU_L1_FAULT);
	while (l1 != end);
	MMU_DSB();
	ksceKernelCpuResumeIntr(flags);
	return 0;
}

int sceKernelMMUSetL2PageTable(uint32_t *l1, uint32_t domain, uintptr_t va, uintptr_t l2pa)
{
	uint32_t *pte;

	SCE_DBG_ASSERT_CHECK(MMUInterruptsDisabled(), 620);
	if ((uintptr_t)l1 & 0xfff) {
		SCE_DBG_PRINTF(0xb, 623, "l1base=%p\n", l1);
		SCE_PANIC(624);
	}
	pte = &l1[va >> 20];
	if (MMU_L1_TYPE(*pte) != MMU_L1_TYPE_FAULT)
		SCE_PANIC_PRINTF(633, "l1base %p, map 0x%08x, pbase 0x%08x, *pte 0x%08x\n",
				 l1, va, l2pa, *pte);
	MMUWritePTE(pte, (l2pa >> 10) << 10 | MMU_L1_TYPE_PAGE_TABLE | MMU_L1_DOMAIN(domain));
	MMU_DSB();
	return 0;
}

int sceKernelMMUUnsetL2PageTable(uint32_t *l1, uintptr_t va)
{
	uint32_t *pte;

	SCE_DBG_ASSERT_CHECK(MMUInterruptsDisabled(), 658);
	if ((uintptr_t)l1 & 0xfff) {
		SCE_DBG_PRINTF(0xb, 661, "l1base=%p\n", l1);
		SCE_PANIC(662);
	}
	pte = &l1[va >> 20];
	if (MMU_L1_TYPE(*pte) != MMU_L1_TYPE_PAGE_TABLE)
		SCE_PANIC_PRINTF(670, "Error:PTE[0x%08x] is illegal state (!=L2)\n", *pte);
	MMUWritePTE(pte, MMU_L1_FAULT);
	MMU_DSB();
	return 0;
}

int sceKernelMMUMapSupersections(uint32_t *l1, uint32_t type, uint32_t domain, uintptr_t va,
				 SceSize size, uintptr_t pa)
{
	uint32_t *pte, attr;
	unsigned int i;

	SCE_DBG_ASSERT_CHECK(MMUInterruptsDisabled(), 702);
	if ((uintptr_t)l1 & 0xfff) {
		SCE_DBG_PRINTF(0xb, 705, "l1base=%p\n", l1);
		SCE_PANIC(706);
	}
	MMU_ALIGN_CHECK(va, size, 0xffffff);
	pte = &l1[(va >> 24) * 16];
	attr = MemBlockTypeToL1PTE(type, domain);
	if (size >> 24) {
		/* sixteen identical entries per supersection */
		for (i = 0; i != (size >> 24) * 16; i++, pa += 0x100000)
			MMUWritePTE(pte++, (pa & 0xff000000) | attr | MMU_L1_SUPERSECTION |
				    MMU_L1_TYPE_SECTION);
	}
	MMU_DSB();
	return 0;
}

int sceKernelMMUMapSections(uint32_t *l1, uint32_t type, uint32_t domain, uintptr_t va,
			    SceSize size, uintptr_t pa)
{
	uint32_t *pte, attr;
	unsigned int count = size >> 20, i;

	SCE_DBG_ASSERT_CHECK(MMUInterruptsDisabled(), 900);
	if ((uintptr_t)l1 & 0xfff) {
		SCE_DBG_PRINTF(0xb, 903, "l1base=%p\n", l1);
		SCE_PANIC(904);
	}
	MMU_ALIGN_CHECK(va, size, 0xfffff);
	attr = MemBlockTypeToL1PTE(type, domain);
	pte = &l1[va >> 20];
	if (count != 0) {
#if FW_DEBUG
		/* debug builds refuse to map over an existing entry */
		for (i = 0; i < count; i++) {
			if (MMU_L1_TYPE(pte[i]) != MMU_L1_TYPE_FAULT) {
				SCE_DBG_PRINTF(0xb, 921,
					"ERROR:l1base %p, vaddr 0x%08x, vsize 0x%08x, paddr 0x%08x\n",
					l1, va, size, pa);
				return SCE_KERNEL_ERROR_CPU_MMU_L1PTE_NOT_FAULT;
			}
		}
#endif
		for (i = 0; i != count; i++, pa += 0x100000)
			MMUWritePTE(pte++, attr | MMU_L1_TYPE_SECTION | (pa & 0xfff00000));
	}
	MMU_DSB();
	return 0;
}

int sceKernelMMUUnmapSectionsWithFlags(uint32_t *l1, uint32_t asid, uintptr_t va, SceSize size,
				       uint32_t flags)
{
	uint32_t *pte;
	unsigned int i;

	SCE_DBG_ASSERT_CHECK(MMUInterruptsDisabled(), 988);
	if ((uintptr_t)l1 & 0xfff) {
		SCE_DBG_PRINTF(0xb, 991, "l1base=%p\n", l1);
		SCE_PANIC(992);
	}
	MMU_ALIGN_CHECK(va, size, 0xfffff);
	SCE_DBG_ASSERT_CHECK(1, 998);
	if (flags & ~1) {
		SCE_DBG_ASSERT_FAIL(1001);
		return SCE_KERNEL_ERROR_INVALID_FLAGS;
	}
	pte = &l1[va >> 20];
	for (i = 0; i != size >> 20; i++, va += 0x100000) {
		MMUWritePTE(pte++, MMU_L1_FAULT);
		MMU_ISB();
		MMUInvalidateTLBEntry(va, asid);
	}
	MMU_DSB();
	return 0;
}

/* name unknown: the same for supersections */
int MMUUnmapSupersectionsWithFlags(uint32_t *l1, uint32_t asid, uintptr_t va, SceSize size,
				   uint32_t flags)
{
	MMU_ALIGN_CHECK(va, size, 0xffffff);
	return sceKernelMMUUnmapSectionsWithFlags(l1, asid, va, size, flags);
}

int sceKernelMMUL2Init(uint32_t *l2)
{
	uint32_t *end;
	int flags;

	if ((uintptr_t)l2 & 0x3ff)
		SCE_PANIC(1041);
	flags = ksceKernelCpuSuspendIntr();
	end = l2 + 0x100;
	do
		MMUWritePTE(l2++, 0);
	while (l2 != end);
	MMU_DSB();
	ksceKernelCpuResumeIntr(flags);
	return 0;
}

/* offset is va's offset in the 1 MiB the L2 table covers */
int sceKernelMMUMapLargePages(uint32_t *l2, uint32_t type, uint32_t domain, uintptr_t offset,
			      SceSize size, uintptr_t pa)
{
	uint32_t attr, *pte;
	unsigned int first, end, i;

	SCE_DBG_ASSERT_CHECK(MMUInterruptsDisabled(), 1318);
	if ((uintptr_t)l2 & 0x3ff) {
		SCE_DBG_PRINTF(0xb, 1321, "l2base=%p\n", l2);
		SCE_PANIC(1322);
	}
	if (offset >= 0x100000) {
		SCE_DBG_ASSERT_FAIL(1332);
		return SCE_KERNEL_ERROR_MMU_L2_INDEX_OVERFLOW;
	}
	first = offset >> 16;
	end = (first + (size >> 16)) * 16;
	if (end > 0x100) {
		SCE_DBG_ASSERT_FAIL(1347);
		return SCE_KERNEL_ERROR_MMU_L2_SIZE_OVERFLOW;
	}
	attr = MemBlockTypeToL2PTELargePage(type);
	/* sixteen identical entries per large page */
	for (i = first * 16, pte = &l2[first * 16]; i < end; i++, pa += 0x1000)
		MMUWritePTE(pte++, attr | (pa & 0xffff0000));
	MMU_DSB();
	return 0;
}

int sceKernelMMUUnmapLargePagesWithFlags(uint32_t *l2, uint32_t asid, uintptr_t va,
					 uintptr_t offset, SceSize size, uint32_t flags)
{
	uint32_t *pte;
	unsigned int i, end;

	SCE_DBG_ASSERT_CHECK(MMUInterruptsDisabled(), 1398);
	if ((uintptr_t)l2 & 0x3ff) {
		SCE_DBG_PRINTF(0xb, 1401, "l2base=%p\n", l2);
		SCE_PANIC(1402);
	}
	SCE_DBG_ASSERT_CHECK(1, 1405);
	if (flags & ~1) {
		SCE_DBG_ASSERT_FAIL(1410);
		return SCE_KERNEL_ERROR_INVALID_FLAGS;
	}
	if (offset >= 0x100000) {
		SCE_DBG_ASSERT_FAIL(1417);
		return SCE_KERNEL_ERROR_MMU_L2_INDEX_OVERFLOW;
	}
	offset >>= 16;
	end = (offset + (size >> 16)) * 16;
	if (end > 0x100) {
		SCE_DBG_ASSERT_FAIL(1427);
		return SCE_KERNEL_ERROR_MMU_L2_SIZE_OVERFLOW;
	}
	pte = &l2[offset * 16];
	for (i = offset * 16; i < end; i++, va += 0x1000) {
		MMUWritePTE(pte++, 0);
		MMU_ISB();
		MMUInvalidateTLBEntry(va, asid);
	}
	MMU_DSB();
	return 0;
}

int sceKernelMMUMapSmallPages(uint32_t *l2, uint32_t type, uint32_t domain, uintptr_t offset,
			      SceSize size, uintptr_t pa)
{
	uint32_t attr, *pte;
	unsigned int i, end;

	SCE_DBG_ASSERT_CHECK(MMUInterruptsDisabled(), 1579);
	if ((uintptr_t)l2 & 0x3ff) {
		SCE_DBG_PRINTF(0xb, 1582, "l2base=%p\n", l2);
		SCE_PANIC(1583);
	}
	if (offset >= 0x100000) {
		SCE_DBG_ASSERT_FAIL(1593);
		return SCE_KERNEL_ERROR_MMU_L2_INDEX_OVERFLOW;
	}
	i = offset >> 12;
	end = i + (size >> 12);
	if (end > 0x100) {
		SCE_DBG_ASSERT_FAIL(1600);
		return SCE_KERNEL_ERROR_MMU_L2_SIZE_OVERFLOW;
	}
	pte = &l2[i];
	attr = MemBlockTypeToL2PTESmallPage(type);
	for (; i < end; i++, pa += 0x1000)
		MMUWritePTE(pte++, attr | (pa & 0xfffff000));
	MMU_DSB();
	return 0;
}

int sceKernelMMUUnmapSmallPagesWithFlags(uint32_t *l2, uint32_t asid, uintptr_t va,
					 uintptr_t offset, SceSize size, uint32_t flags)
{
	uint32_t *pte;
	unsigned int i, end;

	SCE_DBG_ASSERT_CHECK(MMUInterruptsDisabled(), 1657);
	if ((uintptr_t)l2 & 0x3ff) {
		SCE_DBG_PRINTF(0xb, 1660, "l2base=%p\n", l2);
		SCE_PANIC(1661);
	}
	SCE_DBG_ASSERT_CHECK(1, 1664);
	if (flags & ~1) {
		SCE_DBG_ASSERT_FAIL(1669);
		return SCE_KERNEL_ERROR_INVALID_FLAGS;
	}
	if (offset >= 0x100000) {
		SCE_DBG_ASSERT_FAIL(1676);
		return SCE_KERNEL_ERROR_MMU_L2_INDEX_OVERFLOW;
	}
	i = offset >> 12;
	end = i + (size >> 12);
	if (end > 0x100) {
		SCE_DBG_ASSERT_FAIL(1683);
		return SCE_KERNEL_ERROR_MMU_L2_SIZE_OVERFLOW;
	}
	pte = &l2[i];
	for (; i < end; i++, va += 0x1000) {
		MMUWritePTE(pte++, 0);
		MMU_ISB();
		MMUInvalidateTLBEntry(va, asid);
	}
	MMU_DSB();
	return 0;
}

/* name unknown */
int MMUVAtoPAWithFlags(uint32_t mode, uint32_t flags, uintptr_t va, uintptr_t *pa)
{
	return MMUVAtoPACore(mode, flags, va, pa, NULL);
}

int ksceKernelMMUVAtoPAWithMode(uint32_t mode, uintptr_t va, uintptr_t *pa)
{
	return MMUVAtoPACore(mode, 0, va, pa, NULL);
}

/* name unknown */
int MMUVAtoPAWithMemoryType(uint32_t mode, uintptr_t va, uintptr_t *pa, int *memoryType)
{
	return MMUVAtoPACore(mode, 0, va, pa, memoryType);
}

/* the memory type of the first access to va that is permitted */
int ksceKernelMMUGetMemoryType(uintptr_t va)
{
	uint32_t dacr = readDacr(), par;

	writeDacr(DACR_ALL_CLIENT);
	par = MMUTranslate0(va);
	if (par & PAR_FAULT) {
		par = MMUTranslate1(va);
		if (par & PAR_FAULT) {
			par = MMUTranslate2(va);
			if (par & PAR_FAULT) {
				par = MMUTranslate3(va);
				if (par & PAR_FAULT) {
					writeDacr(dacr);
					return SCE_KERNEL_ERROR_VA2PA_FAULT;
				}
			}
		}
	}
	writeDacr(dacr);
	return PAR2MemoryType(par);
}

/* name unknown: whether every page of a range permits all the accesses in mode */
int MMUCheckAccessibleRange(uint32_t mode, uintptr_t va, SceSize size)
{
	uintptr_t end;
	uint32_t dacr, privRead, privWrite, userRead, userWrite, fault;

	if (!(mode & MMU_ACCESS_ANY))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	end = va + size;
	while (size != 0 ? va < end : va <= end) {
		dacr = readDacr();
		writeDacr(DACR_ALL_CLIENT);
		privRead = MMUTranslate0(va);
		privWrite = MMUTranslate1(va);
		userRead = MMUTranslate2(va);
		userWrite = MMUTranslate3(va);
		fault = (mode & MMU_ACCESS_PRIV_READ) ? privRead : 0;
		if (mode & MMU_ACCESS_PRIV_WRITE)
			fault |= privWrite;
		if (mode & MMU_ACCESS_USER_READ)
			fault |= userRead;
		if (mode & MMU_ACCESS_USER_WRITE)
			fault |= userWrite;
		writeDacr(dacr);
		if (fault & PAR_FAULT)
			return SCE_KERNEL_ERROR_VA2PA_FAULT;
		if (fault & PAR_SUPERSECTION)
			va = (va + 0x1000000) & ~0xffffff;
		else
			va = (va + 0x1000) & ~0xfff;
	}
	return 0;
}

int ksceKernelMMUVAtoPA(uintptr_t va, uintptr_t *pa)
{
	return MMUVAtoPACore(MMU_ACCESS_ANY, 0, va, pa, NULL);
}

/*
 * Walking the tables. These return 3 for a section, 4 for a supersection,
 * 5 for an L1 entry pointing to an L2 table and 6 for a fault entry; the L2
 * walkers 1 for a small page, 2 for a large page and 6 for a fault entry.
 */
#define MMU_WALK_SMALL_PAGE		1
#define MMU_WALK_LARGE_PAGE		2
#define MMU_WALK_SECTION		3
#define MMU_WALK_SUPERSECTION		4
#define MMU_WALK_PAGE_TABLE		5
#define MMU_WALK_FAULT			6

int sceKernelMMUL1VAtoPABySW(const uint32_t *l1, uintptr_t va, uintptr_t *pa)
{
	uint32_t entry = l1[va >> 20];

	switch (MMU_L1_TYPE(entry)) {
	case MMU_L1_TYPE_SECTION:
		if (!(entry & MMU_L1_SUPERSECTION)) {
			*pa = (va & 0xfffff) | (entry >> 20) << 20;
			return MMU_WALK_SECTION;
		}
		*pa = (va & 0xffffff) | (entry & 0xff000000);
		return MMU_WALK_SUPERSECTION;
	case 3:
		SCE_DBG_ASSERT_FAIL(2101);
		return SCE_KERNEL_ERROR_CPU_ERROR;
	case MMU_L1_TYPE_PAGE_TABLE:
		return MMU_WALK_PAGE_TABLE;
	}
	return MMU_WALK_FAULT;
}

/* the access codes the IsEqualAccessible walkers take */
#define MMU_PERM_PRIV_RO_USER_NA	0x01
#define MMU_PERM_PRIV_RW_USER_NA	0x03
#define MMU_PERM_PRIV_RO_USER_RO	0x05
#define MMU_PERM_PRIV_RO_USER_RO_X	0x10
#define MMU_PERM_PRIV_RW_USER_RW	0x30
#define MMU_PERM_PRIV_RW_USER_RO	0x50

int sceKernelMMUL1IsEqualAccessibleBySW(const uint32_t *l1, uint32_t perm, uintptr_t va,
					uintptr_t *pa)
{
	uint32_t entry = l1[va >> 20], want;

	switch (MMU_L1_TYPE(entry)) {
	case MMU_L1_TYPE_SECTION:
		break;
	case 3:
		SCE_DBG_ASSERT_FAIL(2182);
		return SCE_KERNEL_ERROR_CPU_ERROR;
	case MMU_L1_TYPE_PAGE_TABLE:
		return MMU_WALK_PAGE_TABLE;
	default:
		return MMU_WALK_FAULT;
	}
	switch (perm) {
	case 0x05:
		want = 0x8400;
		break;
	case 0x01:
		want = 0x8410;
		break;
	case 0x03:
		want = 0x410;
		break;
	case 0x30:
		want = 0xc10;
		break;
	case 0x50:
		want = 0x8c00;
		break;
	case 0x10:
		want = 0x8c10;
		break;
	default:
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	if ((entry & 0x8c10) != want)
		return SCE_KERNEL_ERROR_CPU_ERROR;
	if (entry & MMU_L1_SUPERSECTION) {
		*pa = (va & 0xffffff) | (entry & 0xff000000);
		return MMU_WALK_SUPERSECTION;
	}
	*pa = (va & 0xfffff) | (entry & 0xfff00000);
	return MMU_WALK_SECTION;
}

int sceKernelMMUL1GetMemoryType(const uint32_t *l1, uintptr_t va)
{
	uint32_t entry = l1[va >> 20];

	switch (MMU_L1_TYPE(entry)) {
	case MMU_L1_TYPE_PAGE_TABLE:
		return 0;
	case MMU_L1_TYPE_SECTION:
		switch (entry & 0x500c) {
		case 0x000c:
		case 0x100c:
			return MMU_MEMORY_TYPE_NORMAL;
		case 0x0000:
			return MMU_MEMORY_TYPE_SO;
		case 0x0004:
			return MMU_MEMORY_TYPE_DEVICE;
		case 0x4004:
			return MMU_MEMORY_TYPE_NC;
		case 0x1000:
			return MMU_MEMORY_TYPE_SO_80;
		}
		SCE_DBG_ASSERT_FAIL(2248);
		return SCE_KERNEL_ERROR_CPU_ERROR;
	}
	SCE_DBG_ASSERT_FAIL(2261);
	return SCE_KERNEL_ERROR_CPU_ERROR;
}

/* name unknown: sceKernelMMUL2VAtoPABySW */
int MMUL2VAtoPABySW(const uint32_t *l2, uintptr_t va, uintptr_t *pa)
{
	uint32_t entry = l2[(va << 12) >> 24];

	if (MMU_L2_TYPE(entry) == MMU_L2_TYPE_LARGE_PAGE) {
		*pa = (va & 0xffff) | (entry & 0xffff0000);
		return MMU_WALK_LARGE_PAGE;
	}
	if (MMU_L2_TYPE(entry) != MMU_L2_TYPE_FAULT) {
		*pa = (va & 0xfff) | (entry & 0xfffff000);
		return MMU_WALK_SMALL_PAGE;
	}
	return MMU_WALK_FAULT;
}

/* name unknown: sceKernelMMUL2IsEqualAccessibleBySW */
int MMUL2IsEqualAccessibleBySW(const uint32_t *l2, uint32_t perm, uintptr_t va, uintptr_t *pa)
{
	uint32_t entry = l2[(va << 12) >> 24], want;

	if (MMU_L2_TYPE(entry) == MMU_L2_TYPE_LARGE_PAGE) {
		switch (perm) {
		case 0x05:
			want = 0x210;
			break;
		case 0x01:
			want = 0x8210;
			break;
		case 0x03:
			want = 0x8010;
			break;
		case 0x30:
			want = 0x8030;
			break;
		case 0x50:
			want = 0x230;
			break;
		case 0x10:
			want = 0x8230;
			break;
		default:
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		}
		if ((entry & 0x8230) == want) {
			*pa = (va & 0xffff) | (entry & 0xffff0000);
			return MMU_WALK_LARGE_PAGE;
		}
		return SCE_KERNEL_ERROR_CPU_ERROR;
	}
	if (MMU_L2_TYPE(entry) == MMU_L2_TYPE_FAULT)
		return MMU_WALK_FAULT;
	switch (perm) {
	case 0x05:
		want = 0x210;
		break;
	case 0x01:
		want = 0x211;
		break;
	case 0x03:
		want = 0x11;
		break;
	case 0x30:
		want = 0x31;
		break;
	case 0x50:
		want = 0x230;
		break;
	case 0x10:
		want = 0x231;
		break;
	default:
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	if ((entry & 0x231) == want) {
		*pa = (va & 0xfff) | (entry & 0xfffff000);
		return MMU_WALK_SMALL_PAGE;
	}
	return SCE_KERNEL_ERROR_CPU_ERROR;
}

int sceKernelMMUL2GetMemoryType(const uint32_t *l2, uintptr_t va)
{
	uint32_t entry = l2[(va << 12) >> 24];

	if (MMU_L2_TYPE(entry) == MMU_L2_TYPE_LARGE_PAGE) {
		switch (entry & 0x500c) {
		case 0x000c:
		case 0x100c:
			return MMU_MEMORY_TYPE_NORMAL;
		case 0x0000:
			return MMU_MEMORY_TYPE_SO;
		case 0x0004:
			return MMU_MEMORY_TYPE_DEVICE;
		case 0x4004:
			return MMU_MEMORY_TYPE_NC;
		case 0x1000:
			return MMU_MEMORY_TYPE_SO_80;
		}
		SCE_DBG_ASSERT_FAIL(2463);
		return SCE_KERNEL_ERROR_CPU_ERROR;
	}
	if (MMU_L2_TYPE(entry) == MMU_L2_TYPE_FAULT) {
		SCE_DBG_ASSERT_FAIL(2471);
		return SCE_KERNEL_ERROR_CPU_ERROR;
	}
	switch (entry & 0x14c) {
	case 0x00c:
	case 0x04c:
		return MMU_MEMORY_TYPE_NORMAL;
	case 0x000:
		return MMU_MEMORY_TYPE_SO;
	case 0x004:
		return MMU_MEMORY_TYPE_DEVICE;
	case 0x104:
		return MMU_MEMORY_TYPE_NC;
	case 0x040:
		return MMU_MEMORY_TYPE_SO_80;
	}
	SCE_DBG_ASSERT_FAIL(2436);
	return SCE_KERNEL_ERROR_CPU_ERROR;
}

/* whether [va, va + size) maps to [pa, pa + size) in context (NULL: the current one) */
int MMUIsValidMap(const MMUContext *context, uintptr_t va, SceSize size, uintptr_t pa)
{
	MMUContext saved;
	uint32_t contextId = 0;
	uintptr_t end, got, offset;
	int ret = 0, retries;

	if (!s_mmuReady)
		return 0;
	end = va + size;
	offset = pa - va;
	MMUContextSwitch(context, &saved, &contextId);
	for (; va < end; va += 0x1000) {
		/* the translation can race a concurrent remap: retry a few times */
		retries = 10;
		do
			ret = MMUVAtoPACore(MMU_ACCESS_ANY, 0, va, &got, NULL);
		while (ret < 0 && --retries != 0);
		if (ret < 0)
			break;
		if (offset + va != (got >> 12) << 12) {
			SCE_DBG_ASSERT_FAIL(2578);
			ret = SCE_KERNEL_ERROR_VALIDATION_CHECK_FAILED;
			break;
		}
	}
	if (context != NULL && ((contextId ^ context->contextId) & 0xff))
		MMUContextRestore(&saved);
	return ret;
}
