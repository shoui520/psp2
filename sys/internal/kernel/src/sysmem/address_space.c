/*
 * SceSysmem: address spaces. An address space owns an MMU context and its
 * L1/L2 page tables, up to 32 partitions placed in it and the physical memory
 * partitions its blocks are backed from (by code bits 20..23).
 */

#include <sysmem/cp15.h>
#include <sysmem/uid_guid.h>
#include <systimer/timer_regs.h>
#include <pamgr/perfmon_regs.h>
#include "sysmem_internal.h"
#include "memblock_obj_module.h"
#include <sysmem/sysclib.h>

static inline __attribute__((always_inline)) void AddressSpaceRefer(AddressSpaceObject *as)
{
	int count = SceCpuForKernel_8F40D0FD(&as->refCount, 1, 0x7fffffff);

	SCE_DBG_ASSERT_CHECK_FUNC(count >= 0, 926, "AddressSpaceRefer");
}

static inline __attribute__((always_inline)) void AddressSpaceRelease(AddressSpaceObject *as)
{
	int count = SceCpuForDriver_2A71B03C(&as->refCount);

	SCE_DBG_ASSERT_CHECK_FUNC(count > 0, 936, "AddressSpaceRelease");
}

struct Bitheap;

/* partition.c, phymem.c, memblock.c, bitheap.c, uid.c, sysroot */
void *SceSysrootForKernel_63EBB05B(void);
SceUID sceKernelCreatePartition(const char *name, SceUInt8 domain, uintptr_t vbase, SceSize size,
				struct Partition **out, const PartitionInitParam *param);
int DeletePartition(struct Partition *partition);
int AddressSpaceContextFreeL2Tables(AddressSpaceContext *context);
int PartitionAllocMemBlockByCommand(struct Partition *partition,
				    SceKernelAllocMemBlockCommand *command);
SceUID MemBlockCreateGUID(const char *name, MemBlockObject *block, SceUInt32 attr);
const char *PhyMemPartGetName(void *part);
int PhyMemPartGetInfoCore(void *part, SceKernelPhyMemPartInfoCore *info);
int sceKernelBitheapFree(struct Bitheap *heap, int index);
int GUIDUnregisterChecked(SceUID guid);
char *SysmemAllocName(const char *name);
void SysmemFreeName(char *name);
int SceDipswForDriver_A98FC2FD(unsigned int bit);

#define DIPSW_ENABLE_TOOL_PHYMEMPART	0xd2
#define DIPSW_USER_IO_SMALL		0xd5	/* PLACEHOLDER: name */

/* the address space class and the user ASID heap (sceKernelGetUIDAddressSpaceClass) */
typedef struct AddressSpaceGlobals {
	SceClass *cls;
	struct Bitheap *asidHeap;
	SceUInt32 *asidBuffer;
} AddressSpaceGlobals;

static uintptr_t s_userIOPABase;
/* PLACEHOLDER: set by the unconverted module start */
static AddressSpaceObject *s_kernelAddressSpace;
static SceSize s_userIOPASize;
/* PLACEHOLDER: where the class pointer and the ASID heap are kept; set by the unconverted module start */
static AddressSpaceGlobals *s_addressSpaceGlobals;

/* name unknown: the physical window user I/O mappings get (smaller with dipsw 0xd5) */
void AddressSpaceInitUserIOPA(void)
{
	if (!SceDipswForDriver_A98FC2FD(DIPSW_USER_IO_SMALL)) {
		s_userIOPASize = 0x20000000;
		s_userIOPABase = 0x80000000;
		return;
	}
	s_userIOPASize = 0x8000000;
	s_userIOPABase = 0x78000000;
}

/* name unknown: a context's L2 tables, L1 table object and itself freed */
int AddressSpaceFreeContext(AddressSpaceObject *as, AddressSpaceContext *context)
{
	if (context == NULL)
		return 0;
	if (context->l2TablesBlock != NULL)
		AddressSpaceContextFreeL2Tables(context);
	if (context->l1pt != NULL) {
		as->heap->free(as->heap->heap, context->l1pt);
		context->l1pt = NULL;
	}
	as->heap->free(as->heap->heap, context);
	return 0;
}

/* a partition of the address space, as an entry of the creation tables describes it */
typedef struct AddressSpacePartitionEntry {
	const char *name;
	SceUInt32 domain;
	uintptr_t vbase;
	SceSize size;
	SceUInt32 attr;			/* 0x10: 0x10000 for the GPU game partition, else 0 */
	SceUInt32 attrMask;		/* 0x14: 0xffff00ff with it, else all ones */
	SceUInt32 index;		/* 0x18: in AddressSpaceObject.partition */
	SceUID uid;			/* 0x1c: -1 for a new GUID */
} AddressSpacePartitionEntry;

/* a partition created from a table entry (dupName: the name copied) */
int AddressSpaceCreatePartitionByCommandRaw(AddressSpaceObject *as, SceUID pid,
					    const AddressSpacePartitionEntry *entry, int dupName,
					    SceUInt32 attr)
{
	PartitionInitParam param;
	struct Partition *partition = NULL;
	const char *name;
	SceUID uid;
	int ret;

	param.context = as->context;
	param.entryAttrMask = entry->attrMask;
	param.entryAttr = entry->attr;
	param.uid = entry->uid;
	param.size = sizeof(param);
	if (dupName != 1)
		name = entry->name;
	param.attr = attr;
	param.pid = pid;
	if (dupName == 1)
		name = SysmemAllocName(entry->name);
	uid = sceKernelCreatePartition(name, (SceUInt8)entry->domain, entry->vbase, entry->size,
				       &partition, &param);
	ret = uid;
	if (uid >= 0) {
		if (partition != NULL) {
			as->partition[entry->index] = partition;
			partition->addressSpace = as;
			as->partitionUid[entry->index] = uid;
			return 0;
		}
		SCE_DBG_ASSERT_FAIL(1345);
		ret = SCE_KERNEL_ERROR_SYSMEM_ADDRESS_SPACE_ERROR;
	}
	if (dupName == 1 && name != NULL)
		SysmemFreeName((char *)name);
	return ret;
}

#define ADDRESS_SPACE_DEAD		0x07fd9f97

/* an address space taken apart once nobody refers to it: partitions, context, ASID */
void DeleteAddressSpace(AddressSpaceObject *as)
{
	struct Partition *partition;
	AddressSpaceContext *context;
	const AddressSpaceHeapOps *heap;
	SceUInt32 asid;
	SceUID uid;
	int i, j, ret;

	while ((ret = SceCpuForDriver_CDA96E81(&as->refCount, 0, -1)) != 0)
		SCE_DBG_PRINTF(3, 2057, "AddressSpaceInvalidate failed 0x%08x, 0x%08x\n", ret,
			       as->refCount);
	as->lock = 1;
	for (i = 0; i < ADDRESS_SPACE_PARTITIONS; i++) {
		partition = as->partition[i];
		/* shared entries are dropped, the partition goes once */
		for (j = i + 1; j < ADDRESS_SPACE_PARTITIONS; j++) {
			if (partition == as->partition[j]) {
				as->partition[j] = NULL;
				as->partitionUid[j] = -1;
			}
		}
		if (partition == NULL || (partition->attr & 8))
			continue;
		DeletePartition(partition);
		uid = as->partitionUid[i];
		if (uid != -1) {
			if (!(uid & 1))
				as->partitionUid[i] = uid + 1;
			GUIDUnregisterChecked(as->partitionUid[i]);
		}
		as->partition[i] = NULL;
	}
	if (as->context != NULL)
		AddressSpaceContextFreeL2Tables(as->context);
	context = as->context;
	asid = context->mmu.contextId;
	AddressSpaceFreeContext(as, context);
	heap = as->heap;
	SetMemory32Core(as, ADDRESS_SPACE_DEAD, sizeof(*as));
	heap->free(heap->heap, as);
	ret = sceKernelBitheapFree(s_addressSpaceGlobals->asidHeap, asid & 0xff);
	if (ret < 0)
		SCE_DBG_PRINTF(0xb, 2108, "FreeAsidUser failed 0x%08x\n", ret);
}

#if FW_DEBUG
/* debug builds check the index, out of line */
__attribute__((noinline))
static struct Partition *AddressSpaceGetPartitionRaw(SceUInt32 index)
{
	SCE_DBG_PRINTF(6, 1151, "Illegal pindex=0x%08x\n", index);
	return NULL;
}
#endif

static inline struct Partition *AddressSpaceGetPartition(AddressSpaceObject *as, SceUInt32 index)
{
#if FW_DEBUG
	if (index >= ADDRESS_SPACE_PARTITIONS)
		return AddressSpaceGetPartitionRaw(index);
#endif
	return as->partition[index];
}

/* an empty physical memory partition slot: fine for the defaults (1, 12) only */
__attribute__((noinline))
static int AddressSpaceGetPhyMemPartByIndexRaw(SceUInt32 index)
{
	if (!SceDipswForDriver_A98FC2FD(DIPSW_ENABLE_TOOL_PHYMEMPART) && index - 0x14 < 2) {
		SCE_DBG_PRINTF(0xb, 1421, "using pindex=%d without no TOOL environment\n", index);
		SCE_DBG_ASSERT_FAIL(1422);
	}
	switch (index) {
	case 1:
	case 12:
		return 0;
	default:
		return SCE_KERNEL_ERROR_SYSMEM_ADDRESS_SPACE_ERROR;
	}
}

/* sceKernelGetUIDAddressSpaceClass */
SceClass *SceSysmemForKernel_99D3BF76(void)
{
	return s_addressSpaceGlobals->cls;
}

/* the translation context of pid's address space */
int SceSysmemForDriver_2ECF7944(SceUID pid, MMUContext **context)
{
	AddressSpaceObject *as;
	MMUContext *mmuContext;
	void *object;
	int ret;

	ret = SysrootPIDtoAddressSpaceCB(pid, &object);
	if (ret < 0)
		return ret;
	as = object;
	AddressSpaceRefer(as);
	mmuContext = &as->context->mmu;
	AddressSpaceRelease(as);
	*context = mmuContext;
	return 0;
}

int SceSysmemForDriver_2D711589(const MMUContext *context, MMUContext *saved)
{
	return MMUContextSwitchSync(context, saved);
}

int MMUVAtoPAWithMemoryType(uint32_t mode, uintptr_t va, uintptr_t *pa, int *memoryType);

#define MMU_MODE_KERNEL_RW		0x33

/* the physical ranges behind [va, va + size) of the address space */
int AddressSpaceVARangeToPAVector(AddressSpaceObject *as, const SceKernelPARange *range,
				  SceKernelPAVector *vector, int *memoryType)
{
	SceKernelPARange *cur, *end;
	uintptr_t va, pa, base;
	SceSize size, chunk, len;
	int ret;

	if (as == NULL)
		return SCE_KERNEL_ERROR_SYSMEM_ADDRESS_SPACE_ERROR;
	if (range == NULL || vector == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (vector->size != sizeof(*vector))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE;
	if (vector->capacity == 0) {
		cur = NULL;
		end = NULL;
	} else {
		cur = vector->pRanges;
		if (cur == NULL)
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		end = cur + vector->capacity;
	}
	va = range->addr;
	size = range->size;
	vector->nDataInVector = 0;
	vector->total = 0;
	AddressSpaceRefer(as);
	len = 0;
	base = 0;
	while (size != 0) {
		ret = MMUVAtoPAWithMemoryType(MMU_MODE_KERNEL_RW, va, &pa, memoryType);
		if (ret < 0)
			goto out;
		if (len + base != pa) {
			if (vector->total != 0 && cur != end) {
				cur->addr = base;
				cur->size = len;
				cur++;
			}
			len = 0;
			vector->total++;
			base = pa;
		}
		chunk = (~va & 0xfff) + 1;
		if (size <= chunk)
			chunk = size;
		size -= chunk;
		va += chunk;
		len += chunk;
	}
	ret = 0;
	if (cur != end) {
		cur->addr = base;
		cur->size = len;
	}
	vector->nDataInVector = vector->total < vector->capacity ? vector->total : vector->capacity;
out:
	AddressSpaceRelease(as);
	return ret;
}

/* name unknown */
struct Partition *AddressSpaceGetPartitionByIndex(AddressSpaceObject *as, SceUInt32 index)
{
	struct Partition *partition;

	AddressSpaceRefer(as);
	partition = AddressSpaceGetPartition(as, index);
	AddressSpaceRelease(as);
	return partition;
}

/* name unknown: the physical memory partition of an index (NULL: the default) */
int AddressSpaceGetPhyMemPartByIndex(AddressSpaceObject *as, SceUInt32 index, void **part)
{
	int ret;

	AddressSpaceRefer(as);
	*part = as->phyMemPart[index];
	ret = *part == NULL ? AddressSpaceGetPhyMemPartByIndexRaw(index) : 0;
	AddressSpaceRelease(as);
	return ret;
}

/* MemBlockTypeInfo attr: the command's phyMemPart is kept */
#define MEMBLOCK_TYPE_GIVEN_PHYMEMPART	0x00400000

/* where the block's pages come from: the command's, or the address space's for the index */
static inline int AllocMemBlockCommandSetPhyMemPart(SceKernelAllocMemBlockCommand *command,
						    SceUInt32 index)
{
	int ret;

	if ((command->typeInfo->attr & MEMBLOCK_TYPE_GIVEN_PHYMEMPART) && command->phyMemPart != NULL) {
		SCE_DBG_PRINTF(0xb, 748, "Using pCommand->pPhyMemPart=%p[%s]\n", command->phyMemPart,
			       PhyMemPartGetName(command->phyMemPart));
		return 0;
	}
	ret = AddressSpaceGetPhyMemPartByIndex(command->addressSpace, index, &command->phyMemPart);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 756,
			       "AddressSpaceGetPhyMemPartByIndex(%p, ppindex=%d) for [%s] failed 0x%08x\n",
			       command->addressSpace, index, command->name, ret);
		SCE_DBG_PRINTF(0xb, 758, "\tsceKernelGetKernelAddressSpace()=%p\n",
			       s_kernelAddressSpace);
	}
	return ret;
}

/* a block placed in partition pindex, backed from physical partition ppindex */
int AddressSpaceAllocPartitionMemBlockByCommandRaw(AddressSpaceObject *as,
						   SceKernelAllocMemBlockCommand *command, SceUInt32 pindex,
						   SceUInt32 ppindex)
{
	struct Partition *partition;
	int ret;

	SCE_DBG_ASSERT_CHECK(as != NULL, 1484);
	SCE_DBG_ASSERT_CHECK(command != NULL, 1485);
	SCE_DBG_ASSERT_CHECK(command->addressSpace == as, 1486);
	SCE_DBG_ASSERT_CHECK(pindex < ADDRESS_SPACE_PARTITIONS, 1487);
	ret = AllocMemBlockCommandSetPhyMemPart(command, ppindex);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 1491, "AllocMemBlockCommandSetPhyMemPart failed 0x%08x\n", ret);
		return ret;
	}
	partition = AddressSpaceGetPartition(as, pindex);
	command->partition = partition;
	if (partition == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_PARTITION_INDEX;
	ret = PartitionAllocMemBlockByCommand(partition, command);
	if (ret < 0)
		return ret;
	return MemBlockCreateGUID(command->name, command->block, command->opt.attr);
}

/* name unknown: the kernel's (10) or the shell's (11) physical memory partition */
int AddressSpaceGetDefaultPhyMemPart(AddressSpaceObject *as, int kernel, void **part)
{
	return AddressSpaceGetPhyMemPartByIndex(as, kernel == 1 ? 10 : 11, part);
}

/* name unknown: the physical memory partition for a code */
int AddressSpaceGetPhyMemPart(AddressSpaceObject *as, SceUInt32 code, void **part)
{
	return AddressSpaceGetPhyMemPartByIndex(as, (code >> 20) & 0xf, part);
}

SceUInt32 MemBlockCode2PartitionIndex(SceUInt32 code);
int MemBlockGetMirrorBlock(const SceKernelAllocMemBlockKernelOpt *opt, MemBlockObject **block);
SceUID SceSysmemForDriver_45D22597(SceUID pid, SceUID puid);

/* MemBlockTypeInfo attr: how the address space allocates the type */
#define MEMBLOCK_TYPE_DIRECT		0x00200000	/* its physical partition, no checks */
#define MEMBLOCK_TYPE_MIRROR_SIZE	0x00800000	/* a mirror's size from its base */
#define MEMBLOCK_TYPE_UNCHECKED		0x01000000

/* a block of the command's address space, partition and backing from its code */
int AddressSpaceAllocMemBlockByCommand(AddressSpaceObject *unused,
				       SceKernelAllocMemBlockCommand *command)
{
	AddressSpaceObject *as = command->addressSpace;
	MemBlockObject *mirror = NULL;
	struct Partition *partition;
	SceUInt32 attr, pindex, ppindex;
	SceUID guid;
	int ret;

	AddressSpaceRefer(as);
	ppindex = (command->code >> 20) & 0xf;
	pindex = MemBlockCode2PartitionIndex(command->code);
	if (pindex >= ADDRESS_SPACE_PARTITIONS) {
		SCE_DBG_PRINTF(6, 2450, "Illegal vPindex=0x%08x, code=0x%08x\n", pindex, command->code);
		SCE_DBG_ASSERT_FAIL(2451);
	}
	attr = command->typeInfo->attr;
	if (attr & MEMBLOCK_TYPE_DIRECT) {
		command->phyMemPart = as->phyMemPart[ppindex];
		if (command->phyMemPart == NULL) {
			ret = AddressSpaceGetPhyMemPartByIndexRaw(ppindex);
			if (ret < 0)
				goto out;
		}
		partition = AddressSpaceGetPartition(as, pindex);
		command->partition = partition;
		if (partition == NULL) {
			ret = SCE_KERNEL_ERROR_ILLEGAL_PARTITION_INDEX;
		} else {
			ret = PartitionAllocMemBlockByCommand(partition, command);
			if (ret >= 0)
				ret = MemBlockCreateGUID(command->name, command->block, command->opt.attr);
		}
		goto out;
	}
	if (!(attr & MEMBLOCK_TYPE_UNCHECKED)) {
		if (attr & MEMBLOCK_TYPE_GIVEN_PHYMEMPART) {
			if (!SceDipswForDriver_A98FC2FD(DIPSW_ENABLE_TOOL_PHYMEMPART)) {
				ret = SCE_KERNEL_ERROR_ERROR;
				goto out;
			}
		} else if (attr & MEMBLOCK_TYPE_MIRROR_SIZE) {
			if (command->opt.attr & SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_MIRROR_BLOCKID) {
				ret = MemBlockGetMirrorBlock(&command->opt, &mirror);
				if (ret < 0) {
					/* given as a PUID of the caller */
					guid = SceSysmemForDriver_45D22597(0, command->opt.mirror_blockid);
					if (guid < 0 || guid == (SceUID)command->opt.mirror_blockid)
						goto out;
					command->opt.mirror_blockid = guid;
					ret = MemBlockGetMirrorBlock(&command->opt, &mirror);
					if (ret < 0)
						goto out;
				}
				command->size = mirror->size;
			}
		} else {
			SCE_DBG_ASSERT_FAIL(2542);
		}
	}
	ret = AddressSpaceAllocPartitionMemBlockByCommandRaw(as, command, pindex, ppindex);
out:
	AddressSpaceRelease(as);
	return ret;
}

/* name unknown: into the address space's translation context */
int AddressSpaceSwitchContext(AddressSpaceObject *as)
{
	int ret;

	AddressSpaceRefer(as);
	ret = MMUContextRestore(&as->context->mmu);
	AddressSpaceRelease(as);
	return ret;
}

/* name unknown: memory of another address space set, from inside it with every domain open */
int AddressSpaceMemset(AddressSpaceObject *as, void *dst, int c, SceSize size)
{
	MMUContext saved;
	int intr;

	AddressSpaceRefer(as);
	intr = SceCpuForDriver_821FC0EE();
	MMUContextSave(&saved);
	MMUContextRestore(&as->context->mmu);
	writeDacr(DACR_ALL_CLIENT);
	memset(dst, c, size);
	MMUContextRestore(&saved);
	sceKernelCpuResumeIntr(intr);
	AddressSpaceRelease(as);
	return 0;
}

int MMUCheckAccessibleRange(uint32_t mode, uintptr_t va, SceSize size);

/* name unknown: whether [va, va + size) is accessible with mode, inside the address space */
int AddressSpaceCheckAccessibleRange(AddressSpaceObject *as, uint32_t mode, uintptr_t va,
				     SceSize size)
{
	MMUContext saved;
	int ret;

	AddressSpaceRefer(as);
	MMUContextSave(&saved);
	AddressSpaceSwitchContext(as);
	ret = MMUCheckAccessibleRange(mode, va, size);
	MMUContextRestore(&saved);
	AddressSpaceRelease(as);
	return ret;
}

/* name unknown: va's physical address by the MMU, inside the address space */
int AddressSpaceVAtoPAWithMode(AddressSpaceObject *as, uint32_t mode, uintptr_t va, uintptr_t *pa)
{
	MMUContext saved;
	int ret;

	AddressSpaceRefer(as);
	MMUContextSave(&saved);
	AddressSpaceSwitchContext(as);
	ret = SceCpuForKernel_1BD23326(mode, va, pa);
	MMUContextRestore(&saved);
	AddressSpaceRelease(as);
	return ret;
}

int PartitionVAtoPABySW(struct Partition *partition, uintptr_t va, uintptr_t *pa);
int PartitionGetMemoryType(struct Partition *partition, uintptr_t va);
int L1PageTableVAtoPABySW(L1PageTable *l1pt, uintptr_t va, uintptr_t *pa);
int L1PageTableGetMemoryType(L1PageTable *l1pt, uintptr_t va);

#define PARTITION_NOT_IN		7	/* the partitions' answer for addresses outside them */
#define MMU_L1_IS_L2			5

/* name unknown: va's physical address from the page tables */
int AddressSpaceVAtoPABySW(AddressSpaceObject *as, uintptr_t va, uintptr_t *pa)
{
	int i, ret;

	AddressSpaceRefer(as);
	for (i = 0; i < ADDRESS_SPACE_PARTITIONS; i++) {
		if (as->partition[i] == NULL)
			continue;
		ret = PartitionVAtoPABySW(as->partition[i], va, pa);
		if (ret != PARTITION_NOT_IN)
			goto out;
	}
	ret = L1PageTableVAtoPABySW(as->context->l1pt, va, pa);
	if (ret == MMU_L1_IS_L2)
		ret = SCE_KERNEL_ERROR_VA2PA_FAULT;
out:
	AddressSpaceRelease(as);
	return ret;
}

/* name unknown: the memory type va is mapped with */
int AddressSpaceGetMemoryType(AddressSpaceObject *as, uintptr_t va)
{
	int i, ret;

	AddressSpaceRefer(as);
	for (i = 0; i < ADDRESS_SPACE_PARTITIONS; i++) {
		if (as->partition[i] == NULL)
			continue;
		ret = PartitionGetMemoryType(as->partition[i], va);
		if (ret != 0)
			goto out;
	}
	ret = L1PageTableGetMemoryType(as->context->l1pt, va);
out:
	AddressSpaceRelease(as);
	return ret;
}

int TinyPartitionFindMemBlock(TinyPartition *tiny, uintptr_t va, SceSize size,
			      MemBlockObject **block);

/* the block holding [va, va + size), from the partition the range lies in */
int AddressSpaceFindMemBlockCtlByAddr(AddressSpaceObject *as, uintptr_t va, SceSize size,
				      MemBlockObject **block)
{
	TinyPartition *tiny;
	int i, ret;

	AddressSpaceRefer(as);
	for (i = 0; i < ADDRESS_SPACE_PARTITIONS; i++) {
		tiny = (TinyPartition *)as->partition[i];
		if (tiny != NULL && tiny->vbase <= va && size - tiny->vbase + va <= tiny->size) {
			ret = TinyPartitionFindMemBlock(tiny, va, size, block);
			goto out;
		}
	}
	ret = SCE_KERNEL_ERROR_ADDRESS_SPACE_CANNOT_FIND_PARTITION_BY_ADDR;
out:
	AddressSpaceRelease(as);
	return ret;
}

SceUID MemBlockGetGUID(MemBlockObject *block);
SceUID SceSysmemForKernel_EC90AA11(SceObjectBase *object);

/* the GUID of the block holding [va, va + size), one made if it has none */
SceUID AddressSpaceFindMemBlockCB(AddressSpaceObject *as, uintptr_t va, SceSize size,
				  MemBlockObject **out)
{
	MemBlockObject *block;
	SceUID guid;
	int ret;

	ret = AddressSpaceFindMemBlockCtlByAddr(as, va, size, &block);
	if (ret < 0)
		return ret;
	guid = MemBlockGetGUID(block);
	if (out != NULL)
		*out = block;
	if (guid == -1)
		return SceSysmemForKernel_EC90AA11((SceObjectBase *)block);
	return guid;
}

int PartitionGetMemBlockInfoByAddr(TinyPartition *tiny, uintptr_t va, SceInt32 level,
				   MemBlockPhyInfo *info);

/* the info of the block holding va */
int AddressSpaceGetMemBlockInfoByAddr(AddressSpaceObject *as, uintptr_t va, SceInt32 level,
				      MemBlockPhyInfo *info)
{
	TinyPartition *tiny;
	int i, ret;

	AddressSpaceRefer(as);
	for (i = 0; i < ADDRESS_SPACE_PARTITIONS; i++) {
		tiny = (TinyPartition *)as->partition[i];
		if (tiny == NULL || va < tiny->vbase || va - tiny->vbase > tiny->size)
			continue;
		ret = PartitionGetMemBlockInfoByAddr(tiny, va, level, info);
		if (ret >= 0)
			goto out;
		SCE_DBG_PRINTF(0xb, 2766, "PartitionGetMemBlockInfoByAddr(0x%08x) failed 0x%08x\n", va,
			       ret);
	}
	ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
out:
	AddressSpaceRelease(as);
	return ret;
}

/* the main and the shell subbudget's physical memory partitions */
#define AS_PHYMEMPART_GAME		10
#define AS_PHYMEMPART_SHELL		11
#define AS_PHYMEMPART_CDRAM		12
#define AS_PHYMEMPART_PHYCONT		8

int AddressSpaceGetSubbudgetInfo(AddressSpaceObject *as, SceUInt32 subbudget,
				 AddressSpaceSubbudgetInfo *out)
{
	SceKernelPhyMemPartInfoCore info;
	void *part;
	int ret;

	AddressSpaceRefer(as);
	if (subbudget == 0) {
		part = as->phyMemPart[AS_PHYMEMPART_SHELL];
	} else if (subbudget == 1) {
		part = as->phyMemPart[AS_PHYMEMPART_GAME];
	} else {
		ret = SCE_KERNEL_ERROR_INVALID_SUBBUDGET_ID;
		SCE_DBG_PRINTF(0xb, 2800,
			       "pAS=%p, subbudget=%d: SCE_KERNEL_ERROR_INVALID_SUBBUDGET_ID\n", as,
			       subbudget);
		goto out;
	}
	if (part == NULL) {
		SCE_DBG_PRINTF(0xb, 2807, "pAS=%p, subbudget=%d, pPhyMemPart is NULL:%s\n", as,
			       subbudget, "SCE_KERNEL_ERROR_INVALID_SUBBUDGET_ID");
		ret = SCE_KERNEL_ERROR_INVALID_SUBBUDGET_ID;
		goto out;
	}
	ret = PhyMemPartGetInfoCore(part, &info);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 2814, "PhyMemPartGetInfoCore failed 0x%08x\n", ret);
	} else {
		out->psize = info.psize;
		out->freeSize = info.freeSize;
	}
out:
	AddressSpaceRelease(as);
	return ret;
}

int AddressSpaceGetFreeMemorySize(AddressSpaceObject *as, SceKernelFreeMemorySizeInfo *out)
{
	SceKernelPhyMemPartInfoCore info;
	void *part;

	AddressSpaceRefer(as);
	part = as->phyMemPart[AS_PHYMEMPART_SHELL];
	if (part == NULL) {
		out->sizeMain = 0;
	} else {
		PhyMemPartGetInfoCore(part, &info);
		out->sizeMain = (info.freeSize & ~0xfffff) - 0x200000;
	}
	part = as->phyMemPart[AS_PHYMEMPART_CDRAM];
	if (part == NULL) {
		out->sizeCdram = 0;
	} else {
		PhyMemPartGetInfoCore(part, &info);
		out->sizeCdram = info.freeSize & ~0xfffff;
	}
	part = as->phyMemPart[AS_PHYMEMPART_PHYCONT];
	if (part == NULL) {
		out->sizePhycont = 0;
	} else {
		PhyMemPartGetInfoCore(part, &info);
		out->sizePhycont = info.freeSize & ~0xfffff;
	}
	AddressSpaceRelease(as);
	return 0;
}

int PartitionFreeMemBlockWithBase(struct Partition *partition, MemBlockObject *block,
				  MemBlockObject **baseBlock, void **baseInfo);

/* a base block freed in the partition its info names (the address space by GUID) */
int AddressSpaceIdFreeMemBlock(SceUID guid, MemBlockBaseInfo *info, MemBlockObject **baseBlock,
			       void **baseInfo)
{
	AddressSpaceObject *as;
	struct Partition *partition;
	int i, intr, ret;

	SCE_DBG_PRINTF(0xb, 2879, "(%p)\n", info);
	ret = SceSysmemForDriver_0FC24464(guid, &as);
	if (ret < 0)
		return ret;
	AddressSpaceRefer(as);
	intr = SceCpuForDriver_D32ACE9E(&as->lock);
	for (i = 0; i < ADDRESS_SPACE_PARTITIONS; i++) {
		if (as->partitionUid[i] != info->partitionUid)
			continue;
		partition = as->partition[i];
		if (partition != NULL) {
			SCE_DBG_PRINTF(0xb, 2893, "->PartitionFreeMemBlock(%p)\n", partition);
			ret = PartitionFreeMemBlockWithBase(partition, info->block, baseBlock,
							    baseInfo);
			goto out;
		}
	}
	ret = SCE_KERNEL_ERROR_SYSMEM_ADDRESS_SPACE_ERROR;
out:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&as->lock, intr);
	AddressSpaceRelease(as);
	return ret;
}

/* sceKernelAddressSpaceGetPartitionByIndex */
struct Partition *SceSysmemForKernel_1F84FE44(AddressSpaceObject *as, SceUInt32 index)
{
	return AddressSpaceGetPartitionByIndex(as, index);
}

/* name unknown: the address space's translation context */
MMUContext *AddressSpaceGetMMUContext(AddressSpaceObject *as)
{
	MMUContext *context;

	AddressSpaceRefer(as);
	context = &as->context->mmu;
	AddressSpaceRelease(as);
	return context;
}

/* the heaps the address space's memblocks, virtual pages and L2 tables come from */
int sceKernelAddressSpaceCtrSetObjectHeap(AddressSpaceObject *as, ObjectHeap *heap)
{
	AddressSpaceContext *context;

	if (as == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (heap == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	AddressSpaceRefer(as);
	context = as->context;
	context->memBlockHeap = ObjectHeapGetHeap(heap, 0x40);
	context->virPageHeap = ObjectHeapGetHeap(heap, 0x20);
	SCE_DBG_PRINTF(0xb, 2956, "Set pAC->pL2PageTableHeap %p\n",
		       ObjectHeapGetDefaultHeap(heap));
	context->l2ptHeap = ObjectHeapGetDefaultHeap(heap);
	AddressSpaceRelease(as);
	return 0;
}

int BitheapAlloc(struct Bitheap *heap);
int sceKernelMMUL1Init(uint32_t *l1);
void L1PageTableInit(L1PageTable *l1pt, uint32_t *low, uint32_t *high, uintptr_t split,
		     SceUInt32 reserved);
int AddressSpaceContextAllocL2Tables(AddressSpaceContext *context);
int L1PageTableMapDirect(L1PageTable *l1pt, SceUInt32 type, SceUInt32 domain, uintptr_t va,
			 SceSize size, uintptr_t pa);
void AllocMemBlockCommandInit(SceKernelAllocMemBlockCommand *command, const char *name,
			      SceSize size);
int MemBlockCode2TypeInfo(SceUInt32 code, const MemBlockTypeInfo **info);
int SceSysmemForDriver_A841EDDA(SceUID guid, void **base);
int SceSysmemForDriver_B81CF0A3(SceUID guid, void **vbase);
SceUInt32 SceSysrootForKernel_41636522(void);			/* a random number */

#define ADDRESS_SPACE_MAGIC		0x4d95aeec
#define DIPSW_USER_IOPA			0x9f	/* PLACEHOLDER: name */
#define DIPSW_USER_IOPA_MAP		0xd4	/* PLACEHOLDER: name */
#define AS_PARTITION_TOOL_A		20
#define AS_PARTITION_TOOL_B		21

/* the kernel's partitions every user address space shares */
static const SceUInt32 s_sharedPartitions[] = { 1, 2, 3, 4, 5, 6, 17, 18, 19 };

/* the partitions of a user address space */
static const AddressSpacePartitionEntry s_userPartitions[] = {
	{ "SceKernelUserMain", 0, 0x81000000, 0x4f000000, 0, 0xffffffff, 12, -1 },
	{ "SceKernelUserCdram", 0, 0x60000000, 0x08000000, 0, 0xffffffff, 9, -1 },
	{ "SceKernelUserShared", 4, 0xe0000000, 0x10000000, 0, 0xffffffff, 10, -1 },
	{ "SceKernelUserIO", 6, 0x80000000, 0x01000000, 0, 0xffffffff, 11, -1 },
	{ "SceKernelUserUncache", 0, 0x70000000, 0x10000000, 0, 0xffffffff, 13, -1 },
	{ "SceKernelUserCDialog", 3, 0xd0000000, 0x10000000, 0, 0xffffffff, 14, -1 },
	{ "SceKernelUserCDialogNC", 3, 0x68000000, 0x08000000, 0, 0xffffffff, 15, -1 },
};

static const AddressSpacePartitionEntry s_userToolPartition = {
	"SceKernelUserTool", 5, 0xf0000000, 0x10000000, 0, 0xffffffff, 8, -1
};

/* the context: the new L1 table, the kernel's L1 table for the other half, the kernel's heaps */
static inline int AddressSpaceAllocAsCommonRaw(AddressSpaceObject *as, uint32_t *l1Table,
					       SceUInt32 asid)
{
	AddressSpaceObject *kernel;
	AddressSpaceContext *context;
	L1PageTable *l1pt, *kernelL1;
	uintptr_t pa;
	int ret;

	context = as->heap->alloc(as->heap->heap, sizeof(*context));
	if (context == NULL) {
		SCE_DBG_PRINTF(0xf, 1004, "AddressSpaceAllocMemoryRaw failed\n");
		return SCE_KERNEL_ERROR_SYSMEM_ADDRESS_SPACE_ERROR;
	}
	kernel = SceSysmemForKernel_FA6C098C(SCE_KERNEL_ADDRESS_SPACE_ID);
	if (kernel == NULL) {
		SCE_DBG_ASSERT_FAIL(1012);
		ret = SCE_KERNEL_ERROR_SYSMEM_ADDRESS_SPACE_ERROR;
		goto free_context;
	}
	l1pt = as->heap->alloc(as->heap->heap, sizeof(*l1pt));
	if (l1pt == NULL) {
		ret = SCE_KERNEL_ERROR_SYSMEM_ADDRESS_SPACE_ERROR;
		goto free_context;
	}
	ret = SceSysmemForDriver_8D160E65(l1Table, &pa);
	if (ret < 0)
		goto free_l1pt;
	sceKernelMMUL1Init(l1Table);
	context->partition = kernel->context->partition;
	ret = AddressSpaceContextAllocL2Tables(context);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 1037, "AsCommonAllocL2PageTableVector failed 0x%08x\n", ret);
		goto free_l1pt;
	}
	context->l1pt = l1pt;
	kernelL1 = kernel->context->l1pt;
	context->mmu.ttbr1 = SceCpuForKernel_9818A20B(pa);
	context->mmu.contextId = asid & 0xff;
	context->mmu.dacr = DACR_ALL_CLIENT;
	L1PageTableInit(l1pt, kernelL1->low, l1Table, kernelL1->split, kernelL1->reserved);
	context->memBlockHeap = kernel->context->memBlockHeap;
	context->virPageHeap = kernel->context->virPageHeap;
	context->l2ptHeap = kernel->context->l2ptHeap;
	as->context = context;
	return 0;
free_l1pt:
	as->heap->free(as->heap->heap, l1pt);
free_context:
	as->heap->free(as->heap->heap, context);
	return ret;
}

/* a new user address space object with its context and the kernel's shared partitions */
static inline __attribute__((always_inline)) int CreateAddressSpaceUserMode(const AddressSpaceCreateOpt *opt,
					     AddressSpaceObject **out)
{
	const AddressSpaceHeapOps *heap;
	struct Partition *partition;
	AddressSpaceObject *as;
	SceUInt32 asid, i;
	int ret;

	asid = BitheapAlloc(s_addressSpaceGlobals->asidHeap);
	heap = opt->heap;
	as = heap->alloc(heap->heap, sizeof(*as));
	if (as == NULL) {
		SCE_DBG_ASSERT_FAIL(1962);
		return SCE_KERNEL_ERROR_ERROR;
	}
	as->refCount = 0;
	as->lock = 0;
	as->type = 0;
	for (i = 0; i < ADDRESS_SPACE_PARTITIONS; i++) {
		as->partition[i] = NULL;
		as->partitionUid[i] = -1;
	}
	for (i = 0; i < ADDRESS_SPACE_PHYMEMPARTS; i++)
		as->phyMemPart[i] = NULL;
	as->userIOTimerBase = 0;
	as->heap = heap;
	as->magic = ADDRESS_SPACE_MAGIC;
	as->type = ADDRESS_SPACE_TYPE_USER;
	as->userIOPA = -1;
	as->userIOTimer = -1;
	ret = AddressSpaceAllocAsCommonRaw(as, opt->l1Table, asid);
	if (ret < 0) {
		as->context = NULL;
		SCE_DBG_PRINTF(0xb, 1973, "AllocAsCommon failed 0x%08x\n", ret);
		goto free;
	}
	for (i = 0; i < sizeof(s_sharedPartitions) / sizeof(s_sharedPartitions[0]); i++) {
		partition = AddressSpaceGetPartition(s_kernelAddressSpace, s_sharedPartitions[i]);
		if (partition == NULL)
			goto no_partition;
		as->partition[s_sharedPartitions[i]] = partition;
	}
	if (SceDipswForDriver_A98FC2FD(DIPSW_ENABLE_TOOL_PHYMEMPART)) {
		if (s_kernelAddressSpace->partition[AS_PARTITION_TOOL_A] == NULL) {
			i = AS_PARTITION_TOOL_A;
			goto no_share;
		}
		as->partition[AS_PARTITION_TOOL_A] = s_kernelAddressSpace->partition[AS_PARTITION_TOOL_A];
		if (s_kernelAddressSpace->partition[AS_PARTITION_TOOL_B] == NULL) {
			i = AS_PARTITION_TOOL_B;
			goto no_share;
		}
		as->partition[AS_PARTITION_TOOL_B] = s_kernelAddressSpace->partition[AS_PARTITION_TOOL_B];
	}
	*out = as;
	return 0;
no_share:
	SCE_DBG_PRINTF(0xb, 2012, "SharePartition(%d) failed 0x%08x\n", i, SCE_KERNEL_ERROR_ILLEGAL_PARTITION_INDEX);
no_partition:
	ret = SCE_KERNEL_ERROR_ILLEGAL_PARTITION_INDEX;
	AddressSpaceFreeContext(as, as->context);
free:
	heap->free(heap->heap, as);
	return ret;
}

#define USER_IOPA_VBASE			0x80000000

/* the user I/O page at 0x80000000 */
static inline int AddressSpaceAddDefaultUserIOPARaw(AddressSpaceObject *as)
{
	SceKernelAllocMemBlockCommand command;
	void *va;
	int ret;

	AllocMemBlockCommandInit(&command, "SceUserIOPA", 0x1000);
	command.code = MEMBLOCK_CODE_USER_IO_DEVICE_RW;
	command.opt.paddr = PERFMON_REG_BASE;
	command.opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR;
	ret = MemBlockCode2TypeInfo(MEMBLOCK_CODE_USER_IO_DEVICE_RW, &command.typeInfo);
	if (ret < 0)
		return ret;
	command.addressSpace = as;
	ret = AddressSpaceAllocMemBlockByCommand(as, &command);
	as->userIOPA = ret;
	if (ret < 0)
		return ret;
	ret = SceSysmemForDriver_B81CF0A3(ret, &va);
	if ((uintptr_t)va != USER_IOPA_VBASE)
		SCE_DBG_ASSERT_FAIL(1260);
	return ret;
}

/* the user partitions, the I/O page and window, the timer page at a random offset */
static inline __attribute__((always_inline)) int AddressSpaceCreatePartitionForUser(AddressSpaceObject *as, SceUID pid)
{
	SceKernelAllocMemBlockCommand command;
	MMUContext saved;
	uintptr_t pa;
	SceSize size;
	SceUInt32 i, pages;
	void *base;
	int ret;

	for (i = 0; i < sizeof(s_userPartitions) / sizeof(s_userPartitions[0]); i++) {
		if (AddressSpaceCreatePartitionByCommandRaw(as, pid, &s_userPartitions[i], 0, 0) < 0)
			SCE_DBG_ASSERT_FAIL(1875);
	}
	if (SceDipswForDriver_A98FC2FD(DIPSW_ENABLE_TOOL_PHYMEMPART) &&
	    AddressSpaceCreatePartitionByCommandRaw(as, pid, &s_userToolPartition, 0, 0) < 0)
		SCE_DBG_ASSERT_FAIL(1887);
	if (SceDipswForDriver_A98FC2FD(DIPSW_USER_IOPA)) {
		if (AddressSpaceAddDefaultUserIOPARaw(as) < 0)
			SCE_DBG_ASSERT_FAIL(1897);
		if (SceDipswForDriver_A98FC2FD(DIPSW_USER_IOPA_MAP)) {
			if (!SceDipswForDriver_A98FC2FD(DIPSW_USER_IO_SMALL)) {
				pa = 0x80000000;
				size = 0x20000000;
			} else {
				pa = 0x78000000;
				size = 0x8000000;
			}
			MMUContextSave(&saved);
			AddressSpaceSwitchContext(as);
			ret = L1PageTableMapDirect(as->context->l1pt, 0x8060, 7, 0x40000000, size, pa);
			MMUContextRestore(&saved);
			if (ret < 0)
				return ret;
		}
	}
	/* the timer page lands 8..23 pages into its 32-page window */
	AllocMemBlockCommandInit(&command, "SceUserIOTimer", 0x20000);
	command.opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR | (ALLOC_ATTR_HAS_EXTRA_LOW | ALLOC_ATTR_HAS_EXTRA_HIGH);
	command.opt.paddr = TIMER_LONG_REG_BASE(5);
	pages = (SceSysrootForKernel_41636522() & 0xf) + 8;
	command.opt.extraLow = pages * 0x1000;
	command.code = MEMBLOCK_CODE_USER_IO_DEVICE_R;
	command.opt.extraHigh = 0x1f000 - pages * 0x1000;
	ret = MemBlockCode2TypeInfo(MEMBLOCK_CODE_USER_IO_DEVICE_R, &command.typeInfo);
	if (ret < 0)
		goto fail;
	command.addressSpace = as;
	ret = AddressSpaceAllocMemBlockByCommand(as, &command);
	as->userIOTimer = ret;
	if (ret < 0 || SceSysmemForDriver_A841EDDA(ret, &base) < 0)
		goto fail;
	as->userIOTimerBase = (uintptr_t)base;
	return 0;
fail:
	SCE_DBG_ASSERT_FAIL(1932);
	return 0;
}

/* a user address space for pid, registered under name */
SceUID sceKernelCreateAddressSpace(SceUID pid, const char *name, SceUInt32 type, void *pOpt)
{
	const AddressSpaceCreateOpt *opt = pOpt;
	AddressSpaceObject *as;
	SceUID uid;
	int ret;

	if (name == NULL) {
		SCE_DBG_ASSERT_CHECK_FUNC(0, 2984, "sceKernelCreateAddressSpace");
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	if (opt == NULL) {
		SCE_DBG_ASSERT_CHECK_FUNC(0, 2988, "sceKernelCreateAddressSpace");
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	if (opt->size != sizeof(*opt)) {
		SCE_DBG_ASSERT_CHECK_FUNC(0, 2992, "sceKernelCreateAddressSpace");
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	if (type != ADDRESS_SPACE_TYPE_USER) {
		SCE_DBG_ASSERT_CHECK_FUNC(0, 3022, "sceKernelCreateAddressSpace");
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	ret = CreateAddressSpaceUserMode(opt, &as);
	if (ret < 0) {
		SCE_DBG_PRINTF_FUNC(0xb, 3001, "sceKernelCreateAddressSpace",
				    "CreateAddressSpaceUserMode failed 0x%08x\n", ret);
		SCE_DBG_ASSERT_CHECK_FUNC(0, 3002, "sceKernelCreateAddressSpace");
		return ret;
	}
	ret = AddressSpaceCreatePartitionForUser(as, pid);
	if (ret < 0) {
		SCE_DBG_PRINTF_FUNC(0xb, 3009, "sceKernelCreateAddressSpace",
				    "AddressSpaceCreatePartitionForUser failed 0x%08x\n", ret);
		ret = -1;
		goto fail;
	}
	uid = SceSysmemForKernel_6DB44A42(s_addressSpaceGlobals->cls, name, as);
	if (uid >= 0) {
		as->pid = pid;
		return uid;
	}
	ret = uid;
	SCE_DBG_PRINTF_FUNC(0xb, 3015, "sceKernelCreateAddressSpace",
			    "Register failed 0x%08x\n", ret);
fail:
	DeleteAddressSpace(as);
	return ret;
}

int TinyPartitionFreeAllMemBlock(struct Partition *partition, TinyPartitionClientInfoList *list);

/* every block of a dying process's address space freed, its clients' mappings first */
int sceKernelAddressSpaceFreeAllMemBlock(SceUID guid)
{
	TinyPartitionClientInfoList *list;
	MemBlockClientInfo *client;
	void **entries;
	SceUID *guids;
	AddressSpaceObject *as;
	struct Partition *partition;
	SceUInt32 i, j;
	int intr, ret;

	ret = SceSysmemForDriver_0FC24464(guid, &as);
	if (ret < 0)
		goto out;	/* Sony's code releases the object it did not get */
	AddressSpaceRefer(as);
	if (as->type != ADDRESS_SPACE_TYPE_USER) {
		ret = SCE_KERNEL_ERROR_ERROR;
		goto out;
	}
	if (as->userIOPA != -1)
		SceSysmemForDriver_009E1C61(as->userIOPA);
	if (as->userIOTimer != -1)
		SceSysmemForDriver_009E1C61(as->userIOTimer);
	list = SceSysmemForKernel_85571907(sizeof(*list));
	if (list == NULL)
		return SCE_KERNEL_ERROR_SYSMEM_ADDRESS_SPACE_ERROR;
	list->capacity = ADDRESS_SPACE_PARTITIONS;
	list->count = 0;
	entries = SceSysmemForKernel_85571907(ADDRESS_SPACE_PARTITIONS * sizeof(void *));
	list->entries = entries;
	if (entries == NULL) {
		SceSysmemForKernel_4233C16D(list);
		return SCE_KERNEL_ERROR_SYSMEM_ADDRESS_SPACE_ERROR;
	}
	for (i = 0; i != list->capacity; i++)
		*entries++ = NULL;
	intr = SceCpuForDriver_D32ACE9E(&as->lock);
	for (i = 0; i < ADDRESS_SPACE_PARTITIONS; i++) {
		partition = as->partition[i];
		for (j = i + 1; j < ADDRESS_SPACE_PARTITIONS; j++) {
			if (partition == as->partition[j]) {
				as->partition[j] = NULL;
				as->partitionUid[j] = -1;
			}
		}
		as->partitionUid[i] &= ~1;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&as->lock, intr);
	for (i = 0; i < ADDRESS_SPACE_PARTITIONS; i++) {
		partition = as->partition[i];
		if (partition != NULL && !(partition->attr & 8))
			TinyPartitionFreeAllMemBlock(partition, list);
	}
	entries = list->entries;
	for (i = 0; i < list->count; i++) {
		client = entries[i];
		if (client == NULL)
			continue;
		guids = client->guids;
		for (j = 0; j < client->capacity; j++) {
			if (guids[j] == -1)
				continue;
			SCE_PRINTF_WITH_INFO_FUNC(0, 3137, "sceKernelAddressSpaceFreeAllMemBlock",
						  "Force sceKernelUnmapMemBlock(GUID:0x%08x)\n",
						  guids[j]);
			SceSysmemForDriver_FFCD9B60(guids[j]);
		}
	}
	if (list->entries != NULL) {
		SceSysmemForKernel_4233C16D(list->entries);
		list->entries = NULL;
	}
	SceSysmemForKernel_4233C16D(list);
	ret = 0;
out:
	AddressSpaceRelease(as);
	return ret;
}

int SceSysmemForKernel_4A2E421D(SceUID guid)
{
	AddressSpaceObject *as;
	int ret;

	ret = SceSysmemForDriver_0FC24464(guid, &as);
	if (ret < 0)
		return ret;
	if (as->type != ADDRESS_SPACE_TYPE_USER)
		return SCE_KERNEL_ERROR_ERROR;
	DeleteAddressSpace(as);
	GUIDUnregisterChecked(guid);
	return 0;
}

/* name unknown: the address space's owning process */
int SceSysmemForKernel_5E772E19(SceUID guid, SceUID pid)
{
	AddressSpaceObject *as;
	int ret;

	ret = SceSysmemForDriver_0FC24464(guid, &as);
	if (ret < 0)
		return ret;
	AddressSpaceRefer(as);
	as->pid = pid;
	AddressSpaceRelease(as);
	return 0;
}

int SceSysmemForDriver_4CFA4100(SceUID guid, const char *name);

/* the GUID names of the user partitions, prefixed */
static const struct {
	SceUInt32 index;
	const char *prefix;
} s_partitionNames[] = {
	{ 12, "upmain: " },
	{ 9, "upcdram: " },
	{ 10, "upshared: " },
	{ 11, "upio: " },
	{ 13, "upmainnc: " },
	{ 14, "upcdlg: " },
	{ 15, "upcdlgnc: " },
	{ 8, "uptool: " },
};

/* the address space and its partitions named after the process */
int sceKernelAddressSpaceSetName(SceUID guid, const char *name)
{
	AddressSpaceObject *as;
	char buf[0x20];
	SceUID uid;
	int i, ret;

	ret = SceSysmemForDriver_0FC24464(guid, &as);
	if (ret < 0)
		return ret;
	snprintf(buf, sizeof(buf), "padsp: %s", name);
	ret = SceSysmemForDriver_4CFA4100(guid, buf);
	if (ret < 0)
		SCE_DBG_PRINTF(0xb, 3238, "sceGUIDSetName(0x%08x, [%s]) failed 0x%08x\n", guid, buf,
			       ret);
	AddressSpaceRefer(as);
	for (i = 0; i < 8; i++) {
		uid = as->partitionUid[s_partitionNames[i].index];
		if (uid == -1)
			continue;
		snprintf(buf, sizeof(buf), "%s%s", s_partitionNames[i].prefix, name);
		ret = SceSysmemForDriver_4CFA4100(uid, buf);
		if (ret < 0)
			SCE_DBG_PRINTF(0xb, 3248, "sceGUIDSetName(0x%08x, [%s]) failed 0x%08x\n", uid,
				       buf, ret);
	}
	AddressSpaceRelease(as);
	return 0;
}

int SceSysmemForKernel_7DF07D60(SceUID guid, SceUInt32 index, void *pPhyMemPart)
{
	AddressSpaceObject *as;
	void *object;
	int ret;

	ret = SceSysmemForDriver_0FC24464(guid, &object);
	if (ret < 0)
		return ret;
	as = object;
	AddressSpaceRefer(as);
	as->phyMemPart[index] = pPhyMemPart;
	AddressSpaceRelease(as);
	return 0;
}

/* name unknown: the address space's context */
int SceSysmemForKernel_A7C91D62(SceUID guid, AddressSpaceContext **context)
{
	AddressSpaceObject *as;
	int ret;

	if (context == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = SceSysmemForDriver_0FC24464(guid, &as);
	AddressSpaceRefer(as);
	if (ret < 0)
		return ret;
	*context = as->context;
	AddressSpaceRelease(as);
	return 0;
}

/* name unknown: into the address space of a GUID */
int SceSysmemForKernel_20D01AFA(SceUID guid)
{
	AddressSpaceObject *as;
	int ret;

	ret = SceSysmemForDriver_0FC24464(guid, &as);
	if (ret < 0)
		return ret;
	return AddressSpaceSwitchContext(as);
}

int L1PageTableUnmapDirect(L1PageTable *l1pt, SceUInt32 asid, SceUInt32 domain, uintptr_t va,
			   SceSize size);

/* whole sections of the address space unmapped */
int SceSysmemForKernel_73C38695(SceUID uid, int asid, int domain, void *addr, SceSize size)
{
	AddressSpaceObject *as;
	MMUContext saved;
	void *object;
	int ret;

	if (((uintptr_t)addr & 0xfffff) || (size & 0xfffff))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = SceSysmemForDriver_0FC24464(uid, &object);
	if (ret < 0)
		return ret;
	as = object;
	AddressSpaceRefer(as);
	MMUContextSave(&saved);
	AddressSpaceSwitchContext(as);
	ret = L1PageTableUnmapDirect(as->context->l1pt, asid, domain, (uintptr_t)addr, size);
	MMUContextRestore(&saved);
	AddressSpaceRelease(as);
	return ret;
}

int PartitionIsEqualAccessibleBySW(struct Partition *partition, SceUInt32 perm, uintptr_t va,
				   uintptr_t *pa);
int L1PageTableIsEqualAccessibleBySW(L1PageTable *l1pt, SceUInt32 perm, uintptr_t va,
				     uintptr_t *pa);

/* whether va is accessible with perm in the address space of a GUID */
int AddressSpaceIsEqualAccessibleBySW(SceUID guid, SceUInt32 perm, uintptr_t va, uintptr_t *pa)
{
	AddressSpaceObject *as;
	void *object;
	int i, ret;

	ret = SceSysmemForDriver_0FC24464(guid, &object);
	if (ret < 0)
		return ret;
	as = object;
	AddressSpaceRefer(as);
	for (i = 0; i < ADDRESS_SPACE_PARTITIONS; i++) {
		if (as->partition[i] == NULL)
			continue;
		ret = PartitionIsEqualAccessibleBySW(as->partition[i], perm, va, pa);
		switch (ret) {
		case 1:
		case 2:
		case 3:
		case 4:
		case 6:
			goto out;
		case PARTITION_NOT_IN:
			break;
		default:
			SCE_DBG_PRINTF(0xb, 2244, "PartitionIsEqualAccessibleBySW failed 0x%08x\n",
				       ret);
			goto out;
		}
	}
	ret = L1PageTableIsEqualAccessibleBySW(as->context->l1pt, perm, va, pa);
	if (ret == MMU_L1_IS_L2)
		ret = SCE_KERNEL_ERROR_VA2PA_FAULT;
out:
	AddressSpaceRelease(as);
	return ret;
}

int SceSysmemForKernel_1BAF8C0B(void *pAS, SceUInt32 mode, void *pVA, void **pPA)
{
	AddressSpaceObject *as;
	int ret;

	if (pPA == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = SceSysmemForDriver_0FC24464((SceUID)pAS, &as);
	if (ret < 0)
		return ret;
	return AddressSpaceVAtoPAWithMode(as, mode, (uintptr_t)pVA, (uintptr_t *)pPA);
}

/* name unknown: the physical ranges behind a range of the address space of a GUID */
int SceSysmemForKernel_A541E0CF(SceUID guid, const SceKernelPARange *range,
				SceKernelPAVector *vector)
{
	AddressSpaceObject *as;
	int ret;

	ret = SceSysmemForDriver_0FC24464(guid, &as);
	if (ret < 0)
		return ret;
	return AddressSpaceVARangeToPAVector(as, range, vector, NULL);
}

/* name unknown: where the user I/O timer page is */
int SceSysmemForKernel_FA1A73DD(SceUID guid, uintptr_t *base)
{
	AddressSpaceObject *as;
	int ret;

	ret = SceSysmemForDriver_0FC24464(guid, &as);
	if (ret < 0)
		return ret;
	AddressSpaceRefer(as);
	*base = as->userIOTimerBase;
	AddressSpaceRelease(as);
	return 0;
}

int PartitionGetInfo(struct Partition *partition, void *info);

int SceSysmemForKernel_C3EF4055(SceUID guid, AddressSpaceInfo *info)
{
	AddressSpaceObject *as;
	SceUInt32 i, n;
	int ret;

	if (info == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (info->size != sizeof(*info))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE;
	ret = SceSysmemForDriver_0FC24464(guid, &as);
	if (ret < 0)
		return ret;
	AddressSpaceRefer(as);
	info->uid = guid;
	info->partitionCount = 0;
	info->asid = (SceUInt8)as->context->mmu.contextId;
	for (i = 0; i < ADDRESS_SPACE_PARTITIONS; i++) {
		if (as->partition[i] == NULL)
			continue;
		n = info->partitionCount;
		info->partition[n].size = sizeof(info->partition[n]);
		ret = PartitionGetInfo(as->partition[i], &info->partition[n]);
		if (ret < 0)
			goto out;
		info->partitionCount++;
	}
	info->phyMemPartCount = 0;
	for (i = 0; i < ADDRESS_SPACE_PHYMEMPARTS; i++) {
		if (as->phyMemPart[i] == NULL)
			continue;
		n = info->phyMemPartCount;
		info->phyMemPart[i] = as->phyMemPart[i];
		/* the original marks a partition entry here */
		info->partition[n].size = sizeof(info->partition[n]);
		info->phyMemPartCount = n + 1;
	}
	ret = 0;
out:
	AddressSpaceRelease(as);
	return ret;
}

/* name unknown: the caller's physical memory partition of an index */
int SceSysmemForKernel_47C55DCC(SceUInt32 index, void **part)
{
	return AddressSpaceGetPhyMemPartByIndex(SceSysrootForKernel_63EBB05B(), index,
						part);
}

/* sceKernelGetKernelAddressSpace */
AddressSpaceObject *SceSysmemForKernel_2E5CC243(void)
{
	return s_kernelAddressSpace;
}

/* the physical window user I/O mappings get */
int SceSysmemForDriver_C9928F5E(uintptr_t *base, SceSize *size)
{
	*base = s_userIOPABase;
	*size = s_userIOPASize;
	return 0;
}

/* 1 when va's page lies in the user I/O window */
int SceSysmemForDriver_A7C0D1FC(const void *va)
{
	uintptr_t pa;
	int ret;

	ret = SceSysmemForDriver_8D160E65(va, &pa);
	if (ret < 0)
		return ret;
	if (pa < s_userIOPABase || pa - s_userIOPABase >= s_userIOPASize)
		return SCE_KERNEL_ERROR_NOT_PA_ADDRESS;
	return 1;
}

/* name unknown: whether the kernel address space is set up */
int AddressSpaceKernelReady(void)
{
	return s_kernelAddressSpace != NULL ? 0 : SCE_KERNEL_ERROR_ERROR;
}

int AddressSpaceObjectCreate(void *object);
int AddressSpaceObjectDestroy(void *object);
int BitheapInit(struct Bitheap *heap, SceUInt32 count, SceUInt32 *buffer);
int sceKernelBitheapUse(struct Bitheap *heap, int index);

#define ASID_COUNT			0x100
#define ASID_RESERVED_LOW		2	/* 0, 1: the kernel's */
#define ASID_RESERVED_HIGH		0xf0	/* 0xf0..0xff */

/* the class, the kernel address space and the user ASID heap */
int AsidUserStart(const UIDManager *manager)
{
	AddressSpaceObject *kernel;
	struct Bitheap *heap;
	SceClass *cls;
	int i, ret;

	AddressSpaceInitUserIOPA();
	s_addressSpaceGlobals = manager->addressSpaceGlobals;
	cls = s_addressSpaceGlobals->cls;
	cls->create_cb = AddressSpaceObjectCreate;
	cls->destroy_cb = AddressSpaceObjectDestroy;
	cls->name = "SceUIDAddressSpaceClass";
	ret = SceSysmemForDriver_0FC24464(SCE_KERNEL_ADDRESS_SPACE_ID, &kernel);
	if (ret < 0)
		return ret;
	heap = s_addressSpaceGlobals->asidHeap;
	s_kernelAddressSpace = kernel;
	if (BitheapInit(heap, ASID_COUNT, s_addressSpaceGlobals->asidBuffer) < 0) {
		SCE_DBG_ASSERT_FAIL(188);
		return ret;
	}
	sceKernelBitheapUse(heap, 0);
	sceKernelBitheapUse(heap, 1);
	for (i = ASID_RESERVED_HIGH; i != ASID_COUNT; i++)
		sceKernelBitheapUse(heap, i);
	return ret;
}

/* the game partitions of the kernel address space */
static const AddressSpacePartitionEntry s_kernelGamePartitions[] = {
	{ "SceKernelRootUncacheGpuGame", 12, 0x38000000, 0x08000000, 0x10000, 0xffff00ff, 17,
	  0x10051 },
	{ "SceKernelTmpFsGame", 12, 0x18000000, 0x08000000, 0, 0xffffffff, 18, 0x10053 },
};

#define AS_PARTITION_TMPFS_GAME		18
#define AS_PARTITION_TMPFS_GAME_ALIAS	19

/* name unknown: the kernel's game partitions; partition 19 aliases 18 */
void AddressSpaceKernelGameStart(void)
{
	AddressSpaceObject *kernel;

	AddressSpaceCreatePartitionByCommandRaw(s_kernelAddressSpace, SCE_KERNEL_PROCESS_ID,
						&s_kernelGamePartitions[0], 0, 8);
	AddressSpaceCreatePartitionByCommandRaw(s_kernelAddressSpace, SCE_KERNEL_PROCESS_ID,
						&s_kernelGamePartitions[1], 0, 8);
	AddressSpaceRefer(s_kernelAddressSpace);
	kernel = s_kernelAddressSpace;
	if (kernel->partition[AS_PARTITION_TMPFS_GAME] != NULL)
		kernel->partition[AS_PARTITION_TMPFS_GAME_ALIAS] =
			kernel->partition[AS_PARTITION_TMPFS_GAME];
	AddressSpaceRelease(kernel);
}
