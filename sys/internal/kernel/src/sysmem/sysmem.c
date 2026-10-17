/*
 * SceSysmem: sysmem.c, the module's own exports: VA to PA queries, the
 * subbudget and VM domain syscalls and the module start.
 */
#include <sysmem/cp15.h>
#include "sysmem_internal.h"
#include "memblock_obj_module.h"
#include "sysmem_boot.h"

#define SYSMEM_ERROR_NOT_THREAD		SCE_KERNEL_ERROR_ILLEGAL_CONTEXT

/* the MMU access the VA to PA queries ask for */
#define SYSMEM_VA2PA_MODE		0x33
/* above this the software page table walk is cheaper than the MMU's */
#define SYSMEM_VA2PA_BY_HW_MAX		0xffff

/* what AddressSpaceVAtoPABySW found va mapped with */
#define PAGE_KIND_SMALL			1	/* 4 KiB */
#define PAGE_KIND_LARGE			2	/* 64 KiB */
#define PAGE_KIND_SECTION		3	/* 1 MiB */
#define PAGE_KIND_SUPERSECTION		4	/* 16 MiB */
#define PAGE_KIND_NOT_MAPPED		5
#define PAGE_KIND_FAULT			6
#define PAGE_KIND_NOT_IN		7

#define DIPSW_SUBBUDGET_INFO		0x9f	/* PLACEHOLDER: name */

void *ksceKernelSysrootGetCurrentAddressSpaceCB(void);
int ksceKernelSysrootCheckRemapCodeForUser(int unk);
int ksceKernelCheckDipsw(unsigned int bit);
int ksceKernelCopyToUser(void *dst, const void *src, SceSize size);
SceUID SceSysrootForKernel_7334F1E8(SceUID processId);
void *SysrootGetProcessLocalStorageAddr(SceUInt32 key);
int AddressSpaceVAtoPABySW(AddressSpaceObject *as, uintptr_t va, uintptr_t *pa);
int AddressSpaceVAtoPAWithMode(AddressSpaceObject *as, uint32_t mode, uintptr_t va, uintptr_t *pa);
int AddressSpaceCheckAccessibleRange(AddressSpaceObject *as, uint32_t mode, uintptr_t va,
				     SceSize size);
int AddressSpaceIsEqualAccessibleBySW(SceUID guid, SceUInt32 perm, uintptr_t va, uintptr_t *pa);
int AddressSpaceGetMemoryType(AddressSpaceObject *as, uintptr_t va);
int AddressSpaceGetSubbudgetInfo(AddressSpaceObject *as, SceUInt32 subbudget,
				 AddressSpaceSubbudgetInfo *out);
int AddressSpaceGetFreeMemorySize(AddressSpaceObject *as, AddressSpaceFreeMemorySize *out);
int AddressSpaceIdFreeMemBlock(SceUID guid, MemBlockBaseInfo *info, MemBlockObject **baseBlock,
			       void **baseInfo);
int AddressSpaceKernelReady(void);

/* the bytes from va to the end of its 4 KiB page */
static inline SceSize SmallPageRest(uintptr_t va)
{
	return (~va & 0xfff) + 1;
}

/* name unknown: the one physical range a VA range lies in, by the MMU */
int VARangeToPARangeByHW(const SceKernelVARange *range, SceKernelPARange *out)
{
	uintptr_t va = range->addr, pa, next = 0;
	SceSize size = range->size, chunk;
	int ret;

	while (size != 0) {
		ret = ksceKernelMMUVAtoPAWithMode(SYSMEM_VA2PA_MODE, (void *)va, &pa);
		if (ret < 0)
			return ret;
		if (next != pa) {
			if (next != 0)
				return SCE_KERNEL_ERROR_VARANGE_IS_NOT_PHYSICAL_CONTINUOUS;
			out->addr = pa;
			out->size = size;
		}
		chunk = SmallPageRest(va);
		if (chunk >= size)
			chunk = size;
		size -= chunk;
		va += chunk;
		next = pa + chunk;
	}
	return 0;
}

/* name unknown: the physical ranges a VA range lies in, by the MMU */
int VARangeToPAVectorByHW(const SceKernelVARange *range, MemBlockPAVector *vector)
{
	SceKernelPARange *cur, *end;
	uintptr_t va, pa, start;
	SceSize size, chunk, length;
	SceUInt32 count;
	int ret;

	if (vector->capacity == 0) {
		cur = NULL;
		end = NULL;
	} else {
		cur = vector->ranges;
		if (cur == NULL)
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		end = cur + vector->capacity;
	}
	size = range->size;
	length = 0;
	va = range->addr;
	vector->count = 0;
	vector->total = 0;
	if (size == 0) {
		length = 0;
		count = 0;
		start = 0;
	} else {
		start = 0;
		do {
			ret = ksceKernelMMUVAtoPAWithMode(SYSMEM_VA2PA_MODE, (void *)va, &pa);
			if (ret < 0)
				return ret;
			if (start + length != pa) {
				SceUInt32 total = vector->total;

				if (total != 0) {
					if (cur != end) {
						cur->addr = start;
						cur->size = length;
						cur++;
					}
				}
				length = 0;
				vector->total = total + 1;
				start = pa;
			}
			chunk = SmallPageRest(va);
			if (chunk >= size)
				chunk = size;
			size -= chunk;
			va += chunk;
			length += chunk;
		} while (size != 0);
		count = vector->total;
		if (vector->capacity <= count)
			count = vector->capacity;
	}
	if (cur != end) {
		cur->addr = start;
		cur->size = length;
	}
	vector->count = count;
	return 0;
}

/* name unknown: the one physical range a VA range lies in, from the page tables */
int VARangeToPARangeBySW(const SceKernelVARange *range, SceKernelPARange *out)
{
	uintptr_t va = range->addr, pa, next = 0;
	SceSize size, chunk, mask;
	int ret;

	for (size = range->size; size != 0; size -= chunk) {
		ret = AddressSpaceVAtoPABySW(ksceKernelSysrootGetCurrentAddressSpaceCB(), va, &pa);
		if (ret < 0)
			return ret;
		switch (ret) {
		case PAGE_KIND_SMALL:
			mask = 0xfff;
			break;
		case PAGE_KIND_LARGE:
			mask = 0xffff;
			break;
		case PAGE_KIND_SECTION:
			mask = 0xfffff;
			break;
		case PAGE_KIND_SUPERSECTION:
			mask = 0xffffff;
			break;
		case PAGE_KIND_NOT_MAPPED:
			return SCE_KERNEL_ERROR_VA2PA_FAULT;
		default:
			return ret;
		}
		if (next != pa) {
			if (next != 0)
				return SCE_KERNEL_ERROR_VARANGE_IS_NOT_PHYSICAL_CONTINUOUS;
			out->addr = pa;
			out->size = size;
		}
		chunk = (mask & ~va) + 1;
		if (chunk >= size)
			chunk = size;
		va += chunk;
		next = pa + chunk;
	}
	return 0;
}

int VARangeToPAVectorBySWCore(const SceKernelVARange *range, MemBlockPAVector *vector)
{
	SceKernelPARange *cur, *end;
	uintptr_t va, pa, start;
	SceSize size, chunk, length, mask;
	SceUInt32 count;
	int ret;

	if (vector->capacity == 0) {
		cur = NULL;
		end = NULL;
	} else {
		cur = vector->ranges;
		if (cur == NULL)
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		end = cur + vector->capacity;
	}
	size = range->size;
	length = 0;
	va = range->addr;
	vector->count = 0;
	vector->total = 0;
	start = 0;
	while (size != 0) {
		ret = AddressSpaceVAtoPABySW(ksceKernelSysrootGetCurrentAddressSpaceCB(), va,
					     &pa);
		if (ret < 0)
			return ret;
		switch (ret) {
		case PAGE_KIND_SMALL:
			mask = 0xfff;
			break;
		case PAGE_KIND_LARGE:
			mask = 0xffff;
			break;
		case PAGE_KIND_SECTION:
			mask = 0xfffff;
			break;
		case PAGE_KIND_SUPERSECTION:
			mask = 0xffffff;
			break;
#if FW_SINCE(0x04000000)
		case PAGE_KIND_FAULT:
			return PAGE_KIND_FAULT;
		case PAGE_KIND_NOT_MAPPED:
		default:
			SCE_DBG_ASSERT_FAIL(1104);
			return ret;
#else
		default:
			return ret;
#endif
		}
		if (start + length != pa) {
			SceUInt32 total = vector->total;

			if (total != 0) {
				if (cur != end) {
					cur->addr = start;
					cur->size = length;
					cur++;
				}
			}
			length = 0;
			vector->total = total + 1;
			start = pa;
		}
		chunk = (mask & ~va) + 1;
		if (chunk >= size)
			chunk = size;
		size -= chunk;
		va += chunk;
		length += chunk;
	}
	count = vector->total;
	if (vector->capacity <= count)
		count = vector->capacity;
	if (cur != end) {
		cur->addr = start;
		cur->size = length;
	}
	vector->count = count;
	return 0;
}
int SysrootGetProcessLocalStorageAddrForPid(SceUID pid, SceUInt32 key, void **addr, int create);
int SceSysrootForKernel_AE2132FD(void *ptr);

/* a base block freed from the address space of the process its info names */
int PIDFreeMemBlock(MemBlockBaseInfo *info, MemBlockObject **baseBlock, void **baseInfo)
{
	SceUID guid = SceSysrootForKernel_7334F1E8(info->pid);

	if (guid < 0)
		return guid;
	return AddressSpaceIdFreeMemBlock(guid, info, baseBlock, baseInfo);
}

int ksceKernelVAtoPA(const void *va, uintptr_t *pa)
{
	if (pa == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	return ksceKernelMMUVAtoPAWithMode(SYSMEM_VA2PA_MODE, va, pa);
}

int ksceKernelIsAccessibleRange(SceUInt32 mode, const void *addr, SceSize size)
{
	if ((uintptr_t)addr + size < (uintptr_t)addr)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	return MMUCheckAccessibleRange(mode, (uintptr_t)addr, size);
}

int ksceKernelIsAccessibleRangeProc(SceUID pid, SceUInt32 mode, const void *addr, SceSize size)
{
	AddressSpaceObject *as;
	int ret;

	if (pid == 0) {
		if ((uintptr_t)addr + size < (uintptr_t)addr)
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		return MMUCheckAccessibleRange(mode, (uintptr_t)addr, size);
	}
	if ((uintptr_t)addr + size < (uintptr_t)addr)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = SysrootPIDtoAddressSpaceCB(pid, (void **)&as);
	if (ret < 0)
		return ret;
	return AddressSpaceCheckAccessibleRange(as, mode, (uintptr_t)addr, size);
}

int ksceKernelProcModeVAtoPA(SceUID pid, SceUInt32 mode, const void *va, uintptr_t *pa)
{
	AddressSpaceObject *as;
	int ret;

	if (pa == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = SysrootPIDtoAddressSpaceCB(pid, (void **)&as);
	if (ret < 0)
		return ret;
	return AddressSpaceVAtoPAWithMode(as, mode, (uintptr_t)va, pa);
}

int ksceKernelVAtoPABySW(const void *va, uintptr_t *pa)
{
	int ret;

	if (pa == NULL) {
		SCE_DBG_ASSERT_CHECK_FUNC(pa != NULL, 916, "sceKernelVAtoPABySW");
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	ret = AddressSpaceVAtoPABySW(ksceKernelSysrootGetCurrentAddressSpaceCB(), (uintptr_t)va,
				     pa);
	switch (ret) {
	case PAGE_KIND_SMALL:
	case PAGE_KIND_LARGE:
	case PAGE_KIND_SUPERSECTION:
	case PAGE_KIND_NOT_MAPPED:
		return 0;
	case PAGE_KIND_FAULT:
	case PAGE_KIND_NOT_IN:
		return SCE_KERNEL_ERROR_VA2PA_FAULT;
	default:
		return ret;
	}
}

int ksceKernelVARangeToPARangeBySW(const SceKernelVARange *range, SceKernelPARange *out)
{
	if (range == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (out == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	return VARangeToPARangeBySW(range, out);
}

int ksceKernelVARangeToPAVectorBySW(const SceKernelVARange *range, SceKernelPAVector *vector)
{
	if (range == NULL) {
		SCE_DBG_ASSERT_CHECK_FUNC(range != NULL, 1158, "sceKernelVARangeToPAVectorBySW");
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	if (vector == NULL) {
		SCE_DBG_ASSERT_CHECK_FUNC(vector != NULL, 1162, "sceKernelVARangeToPAVectorBySW");
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	if (vector->size != sizeof(*vector))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE;
	return VARangeToPAVectorBySWCore(range, (MemBlockPAVector *)vector);
}

int ksceKernelIsEqualAccessibleRangeProcBySW(SceUID pid, SceUInt32 perm, const void *addr,
					     SceSize size)
{
	uintptr_t va = (uintptr_t)addr, pa, mask;
	SceSize chunk;
	SceUID guid;
	int ret;

	guid = SceSysrootForKernel_7334F1E8(pid);
	if (guid < 0)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (size == 0)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	do {
		ret = AddressSpaceIsEqualAccessibleBySW(guid, perm, va, &pa);
		if (ret < 0)
			return ret;
		switch (ret) {
		case PAGE_KIND_SMALL:
			mask = 0xfff;
			break;
		case PAGE_KIND_LARGE:
			mask = 0xffff;
			break;
		case PAGE_KIND_SECTION:
			mask = 0xfffff;
			break;
		case PAGE_KIND_SUPERSECTION:
			mask = 0xffffff;
			break;
		case PAGE_KIND_NOT_MAPPED:
			return SCE_KERNEL_ERROR_VA2PA_FAULT;
		default:
			return ret;
		}
		chunk = (mask & ~va) + 1;
		if (chunk >= size)
			chunk = size;
		size -= chunk;
		va += chunk;
	} while (size != 0);
	return 0;
}

int ksceKernelVARangeToPARangeByHW(const SceKernelVARange *range, SceKernelPARange *out)
{
	if (range == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (out == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	return VARangeToPARangeByHW(range, out);
}

int ksceKernelVARangeToPARange(const SceKernelVARange *range, SceKernelPARange *out)
{
	if (range == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (out == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (range->size <= SYSMEM_VA2PA_BY_HW_MAX)
		return VARangeToPARangeByHW(range, out);
	return VARangeToPARangeBySW(range, out);
}

int ksceKernelVARangeToPAVectorByHW(const SceKernelVARange *range, SceKernelPAVector *vector)
{
	if (range == NULL) {
		SCE_DBG_ASSERT_CHECK_FUNC(range != NULL, 1437, "sceKernelVARangeToPAVectorByHW");
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	if (vector == NULL) {
		SCE_DBG_ASSERT_CHECK_FUNC(vector != NULL, 1441, "sceKernelVARangeToPAVectorByHW");
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	if (vector->size != sizeof(*vector))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE;
	return VARangeToPAVectorByHW(range, (MemBlockPAVector *)vector);
}

int ksceKernelVARangeToPAVector(const SceKernelVARange *range, SceKernelPAVector *vector)
{
	if (range == NULL) {
		SCE_DBG_ASSERT_CHECK_FUNC(range != NULL, 1459, "sceKernelVARangeToPAVector");
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	if (vector == NULL) {
		SCE_DBG_ASSERT_CHECK_FUNC(vector != NULL, 1463, "sceKernelVARangeToPAVector");
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	if (vector->size != sizeof(*vector))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE;
	if (range->size > SYSMEM_VA2PA_BY_HW_MAX)
		return VARangeToPAVectorBySWCore(range, (MemBlockPAVector *)vector);
	return VARangeToPAVectorByHW(range, (MemBlockPAVector *)vector);
}

/* what memory va's physical address is in */
int ksceKernelGetPhysicalMemoryType(const void *va)
{
	uintptr_t pa;
	int ret;

	ret = ksceKernelMMUVAtoPAWithMode(SYSMEM_VA2PA_MODE, va, &pa);
	if (ret < 0)
		return ret;
	if (pa - 0x20000000 < 0x10000000)
		return 5;	/* CDRAM */
	if (pa - 0x40000000 < 0x40000000)
		return 1;	/* LPDDR2 */
	if (pa - 0x80000000 < 0x40000000)
		return 3;
	return 7;
}

/* name unknown: the memory type va is mapped with in the current address space */
int SceSysmemForKernel_0B5A3E93(const void *va)
{
	return AddressSpaceGetMemoryType(ksceKernelSysrootGetCurrentAddressSpaceCB(),
					 (uintptr_t)va);
}

/* name unknown: whether the kernel address space exists */
int SceSysmemForKernel_16A54BFE(void)
{
	return AddressSpaceKernelReady();
}

int sceKernelGetSubbudgetInfo(SceUInt32 subbudget, AddressSpaceSubbudgetInfo *userInfo)
{
	AddressSpaceSubbudgetInfo info;
	int ret;

	if (!ksceKernelCheckDipsw(DIPSW_SUBBUDGET_INFO))
		return SCE_KERNEL_ERROR_ONLY_DEVELOPMENT_MODE;
	ret = ksceKernelCopyFromUser(&info, userInfo, sizeof(info));
	if (ret < 0 || info.size != sizeof(info))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = AddressSpaceGetSubbudgetInfo(ksceKernelSysrootGetCurrentAddressSpaceCB(), subbudget,
					   &info);
	if (ret < 0)
		return ret;
	return ksceKernelCopyToUser(userInfo, &info, sizeof(info));
}

int sceKernelGetFreeMemorySize(AddressSpaceFreeMemorySize *userInfo)
{
	AddressSpaceFreeMemorySize info;
	int ret;

	ret = ksceKernelCopyFromUser(&info, userInfo, sizeof(info));
	if (ret < 0 || info.size != sizeof(info))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = AddressSpaceGetFreeMemorySize(ksceKernelSysrootGetCurrentAddressSpaceCB(), &info);
	if (ret < 0)
		return ret;
	return ksceKernelCopyToUser(userInfo, &info, sizeof(info));
}

/* sceKernelCheckOpenVMDomain: 2 while the process holds domain 2 as manager */
SceUInt32 SceSysmemForKernel_4E6D8BC3(void)
{
	ThreadCB *thread = getCurrentThreadCB();

	if (thread == NULL)
		return SYSMEM_ERROR_NOT_THREAD;
	return (thread->pRegisterContext->userDacr & 0x30) == 0x30 ? 2 : 0;
}

/* the DACR bits of domain 2 (the VM domain): 0x30 manager, 0x10 client */
#define VM_DOMAIN_MASK			0x30
#define VM_DOMAIN_CLIENT		0x10

/* the current process takes domain 2 as manager (its VM blocks become accessible) */
int sceKernelOpenVMDomain(void)
{
	SysmemProcessStorage *storage;
	ThreadCB *thread;
	SceUInt32 dacr;
	int intr;

	if (!ksceKernelSysrootCheckRemapCodeForUser(0))
		return SCE_KERNEL_ERROR_PROCESS_CANNOT_REMAP_MEMBLOCK;
	storage = SysrootGetProcessLocalStorageAddr(g_uidManager->processStorageKey);
	if (storage == NULL)
		return SCE_ERROR_ERRNO_ENOSYS;
	if (ksceKernelAtomicCompareAndSet32((SceInt32 *)&storage->managerThread, 0, 1) != 0) {
		SCE_DBG_PRINTF_FUNC(0xb, 1631, "sceKernelOpenVMDomainForUser",
				    "nManagerThread is not 0\n");
		SCE_DBG_ASSERT_CHECK_FUNC(0, 1632, "sceKernelOpenVMDomainForUser");
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	intr = ksceKernelCpuSuspendIntr();
	thread = getCurrentThreadCB();
	if (thread == NULL)
		goto fail;
	dacr = thread->pRegisterContext->userDacr;
	thread = getCurrentThreadCB();
	if (thread == NULL)
		goto fail;
	thread->pRegisterContext->userDacr = dacr | VM_DOMAIN_MASK;
	ksceKernelCpuResumeIntr(intr);
	return 0;
fail:
	ksceKernelCpuResumeIntr(intr);
	return SYSMEM_ERROR_NOT_THREAD;
}

/* domain 2 back to client */
int sceKernelCloseVMDomain(void)
{
	SysmemProcessStorage *storage;
	ThreadCB *thread;
	SceUInt32 dacr;
	int intr;

	if (!ksceKernelSysrootCheckRemapCodeForUser(0))
		return SCE_KERNEL_ERROR_PROCESS_CANNOT_REMAP_MEMBLOCK;
	storage = SysrootGetProcessLocalStorageAddr(g_uidManager->processStorageKey);
	if (storage == NULL)
		return SCE_ERROR_ERRNO_ENOSYS;
	if (ksceKernelAtomicCompareAndSet32((SceInt32 *)&storage->managerThread, 1, 0) != 1) {
		SCE_DBG_PRINTF_FUNC(0xb, 1671, "sceKernelCloseVMDomainForUser",
				    "nManagerThread is not 1\n");
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	intr = ksceKernelCpuSuspendIntr();
	thread = getCurrentThreadCB();
	if (thread == NULL)
		goto fail;
	dacr = thread->pRegisterContext->userDacr;
	thread = getCurrentThreadCB();
	if (thread == NULL)
		goto fail;
	thread->pRegisterContext->userDacr = (dacr & ~VM_DOMAIN_MASK) | VM_DOMAIN_CLIENT;
	ksceKernelCpuResumeIntr(intr);
	return 0;
fail:
	ksceKernelCpuResumeIntr(intr);
	return SYSMEM_ERROR_NOT_THREAD;
}

/* name unknown: frees what pid's sysmem storage holds */
void SceSysmemForKernel_CAB15F74(SceUID pid)
{
	SysmemProcessStorage *storage = NULL;

	if (SysrootGetProcessLocalStorageAddrForPid(pid, g_uidManager->processStorageKey,
						    (void **)&storage, 0) < 0 || storage == NULL)
		return;
	if (storage->vmBlocks != NULL) {
		SceSysrootForKernel_AE2132FD(storage->vmBlocks);
		storage->vmBlocks = NULL;
	}
}

/* PLACEHOLDER: name; a second copy of the canary, written once and never read */
unsigned int s_stackChkGuardCopy = 0x0b8064e5;
/* the canary: the boot value until the KBL's replaces it at module start */
unsigned int __stack_chk_guard = 0x0b8064e5;

UIDManager *g_uidManager;

/* the domain access sysmem reads the KBL parameters with */
#define SYSMEM_BOOT_DACR		0x17450000

#define DIPSW_DEBUG_PUTCHAR		0xc5	/* PLACEHOLDER: name */
#define DIPSW_DEBUG_PUTCHAR_ALT		0xc4	/* PLACEHOLDER: name */

int InitializeDipsw(const struct SysmemBootContext *context);
int InitializeDebugPrintLevel(const struct SysmemBootContext *context);
int InitializeDebugNop(void);
int InitializeAssertLevel(const struct SysmemBootContext *context);
int SysrootModuleStart(const struct SysmemBootContext *context);
int MMUInitPost(void);
SceUInt32 ksceKernelGetDipswInfo(unsigned int index);
int ksceMt19937GlobalInit(SceUInt32 seed);
void *ksceKernelSysrootGetModulePrivate(unsigned int index);
uintptr_t ksceKernelRegisterDebugPutcharHandler(uintptr_t handler, uintptr_t argument);
int BitheapModuleStart(void);
int PhyMemLowModuleStart(const UIDManager *manager);
int PhyMemModuleStart(const UIDManager *manager);
int UIDClassInit(UIDManager *manager);
int VirPageDestroy(void);
int UIDDLinkClassInit(UIDManager *manager);
int NameHeapInit(UIDManager *manager);
int UIDHeapClassInitManager(UIDManager *manager);
int FixedHeapSetUID(UIDManager *manager);
void ObjectHeapSetKernelHeap(const struct UIDManager *manager);
int UIDEntryHeapClassInit(UIDManager *manager);
int L1PageTableModuleStart(void);
int L2PageTableModuleStart(void);
int VirPageCreate(void);
int MemBlockModuleStart(void);
void MemBlockObjModuleStart(const UIDManager *manager);
int PartitionModuleStart(const UIDManager *manager);
int SysmemHeapModuleStart(const UIDManager *manager);
int AsidUserStart(const UIDManager *manager);
void sceKernelL2CacheRemap(void);
int SuspendInit(void);
int LedModuleStart(const struct SysmemBootContext *context);
int SerialInit(const struct SysmemBootContext *context);
int SerialDebugPutchar(void *args, int c);
int PhyMemLowModuleStartPhase2(const UIDManager *manager);
int PhyMemModuleStartPhase2(const UIDManager *manager);
int ObjectHeapInitShared(void);
int AimgrInit(void);
int sceKernelSysEventInit(void);
int sceKernelProcessEventInit(void);

int SysmemModuleStart(SceSize args, void *argp)
{
	const struct SysmemBootContext *context = argp;
	const SceKblParam *kbl;
	SceUInt32 dacr;

	if (context == NULL || (kbl = context->kblParam) == NULL)
		SCE_PANIC(1737);
	dacr = readDacr();
	writeDacr(SYSMEM_BOOT_DACR);
	s_stackChkGuardCopy = kbl->__stack_chk_guard;
	__stack_chk_guard = s_stackChkGuardCopy;
	writeDacr(dacr);
	InitializeDipsw(context);
	InitializeDebugPrintLevel(context);
	if (ksceKernelCheckDipsw(DIPSW_DEBUG_PUTCHAR) ||
	    ksceKernelCheckDipsw(DIPSW_DEBUG_PUTCHAR_ALT))
		ksceKernelRegisterDebugPutcharHandler(context->debugPutchar, 0);
	if (SysrootModuleStart(context) < 0)
		SCE_PANIC(1795);
	if (InitializeDebugNop() < 0)
		SCE_PANIC(1802);
	if (MMUInitPost() < 0)
		SCE_PANIC(1809);
	if (InitializeAssertLevel(context) < 0)
		SCE_PANIC(1815);
	ksceMt19937GlobalInit(ksceKernelGetDipswInfo(3));
	g_uidManager = ksceKernelSysrootGetModulePrivate(3);
	BitheapModuleStart();
	PhyMemLowModuleStart(g_uidManager);
	PhyMemModuleStart(g_uidManager);
	if (UIDClassInit(g_uidManager) < 0)
		SCE_PANIC(1835);
	VirPageDestroy();
	if (UIDDLinkClassInit(g_uidManager) < 0)
		SCE_PANIC(1843);
	NameHeapInit(g_uidManager);
	UIDHeapClassInitManager(g_uidManager);
	FixedHeapSetUID(g_uidManager);
	ObjectHeapSetKernelHeap(g_uidManager);
	UIDEntryHeapClassInit(g_uidManager);
	L1PageTableModuleStart();
	L2PageTableModuleStart();
	VirPageCreate();
	MemBlockModuleStart();
	MemBlockObjModuleStart(g_uidManager);
	PartitionModuleStart(g_uidManager);
	SysmemHeapModuleStart(g_uidManager);
	AsidUserStart(g_uidManager);
	sceKernelL2CacheRemap();
	SuspendInit();
	if (LedModuleStart(context) < 0)
		SCE_PANIC(1903);
	if (SerialInit(context) < 0)
		SCE_PANIC(1912);
	if (ksceKernelCheckDipsw(DIPSW_DEBUG_PUTCHAR) ||
	    ksceKernelCheckDipsw(DIPSW_DEBUG_PUTCHAR_ALT))
		ksceKernelRegisterDebugPutcharHandler((uintptr_t)SerialDebugPutchar, 0);
	PhyMemLowModuleStartPhase2(g_uidManager);
	PhyMemModuleStartPhase2(g_uidManager);
	ObjectHeapInitShared();
	AimgrInit();
	sceKernelSysEventInit();
	sceKernelProcessEventInit();
	return 0;
}

int SysrootCreateProcessLocalStorage(const char *name, SceSize size);
int PhyMemPartInitCdram(void);
int MemBlockModuleStartAfterProcessmgr(void);
int CreateTinyPartitionRaw(void);
void AddressSpaceKernelGameStart(void);
int VSlotModuleStart(void);

int sceKernelSysmemModuleStartAfterProcessmgr(void)
{
	int key;

	key = SysrootCreateProcessLocalStorage("SceKernelSysmem", sizeof(SysmemProcessStorage));
	if (key < 0)
		SCE_PANIC(1950);
	g_uidManager->processStorageKey = key;
	PhyMemPartInitCdram();
	MemBlockModuleStartAfterProcessmgr();
	CreateTinyPartitionRaw();
	AddressSpaceKernelGameStart();
	VSlotModuleStart();
	return 0;
}

int sceKernelSysmemCleaner(void)
{
	SCE_DBG_PRINTF(0xb, 1969, "()\n");
	return 0;
}

/*
 * SceSysmem: copies between kernel and user memory. The user side is touched
 * only by the vmaccess.S primitives, so a bad user pointer returns an error
 * instead of faulting the kernel. The kernel side is touched first (its first,
 * middle and last byte) so that its pages fault in before the domain access
 * changes.
 *
 * The *Proc variants do the same in another process's address space, with
 * interrupts off while its translation context is in.
 */

/* the domain access while user memory is touched */
#define USER_COPY_DACR_TEXT		0x15450fc3	/* user text writable */
#define USER_COPY_DACR_TEXT_DBGP	0x1545cfc3	/* the same, for the debugger */

/* vmaccess.S */
int VmaccessCopyFromUser(void *dst, const void *userSrc, SceSize size);
int VmaccessCopyToUser(void *userDst, const void *src, SceSize size);
int VmaccessFillUser32(SceUInt32 *userDst, SceUInt32 value, SceSize count);
int VmaccessCopyUserToUser(void *userDst, const void *userSrc, SceSize size);
SceSize VmaccessStrnlenUser(const char *userStr, SceSize max);
SceSSize VmaccessStrncpyFromUser(char *dst, const char *userSrc, SceSize n);
SceSSize VmaccessStrncpyToUser(char *userDst, const char *src, SceSize n);
SceSSize VmaccessCountFillUser32(const void *userSrc, SceUInt32 value, SceSize size);
SceSSize VmaccessCountFillUser64(const void *userSrc, SceUInt32 low, SceUInt32 high, SceSize size);

int AddressSpaceSwitchContext(AddressSpaceObject *as);

/* fault in the kernel buffer's pages */
static inline void UserCopyTouchKernel(const void *buffer, SceSize size)
{
	const volatile SceUInt8 *p = buffer;

	(void)p[0];
	(void)p[size - 1];
	(void)p[size >> 1];
}

/* fault in a user page, with an unprivileged read */
static inline void UserCopyTouchUser(const void *user)
{
	SceUInt32 value;

	__asm__ volatile("ldrbt %0, [%1]" : "=r"(value) : "r"(user) : "memory");
}

static inline void UserCopyTouchUser32(const void *user)
{
	SceUInt32 value;

	__asm__ volatile("ldrt %0, [%1]" : "=r"(value) : "r"(user) : "memory");
}

int ksceKernelCopyFromUser(void *dst, const void *src, SceSize size)
{
	SceUInt32 dacr;
	int ret;

	if (size != 0)
		UserCopyTouchKernel(dst, size);
	dacr = readDacr();
	writeDacr(DACR_ALL_CLIENT);
	ret = VmaccessCopyFromUser(dst, src, size);
	writeDacr(dacr);
	return ret;
}

/*
 * In pid's address space: interrupts off, its translation context in, the
 * call, back to ours.
 */
#define USER_COPY_IN_PROCESS(_pid, _ret, _call) do {			\
	MMUContext saved;						\
	void *as;							\
	int flags = ksceKernelCpuSuspendIntr();				\
									\
	MMUContextSave(&saved);						\
	_ret = SysrootPIDtoAddressSpaceCB((_pid), &as);			\
	if (_ret >= 0) {						\
		_ret = AddressSpaceSwitchContext(as);			\
		if (_ret >= 0)						\
			_ret = (_call);					\
	}								\
	MMUContextRestore(&saved);					\
	ksceKernelCpuResumeIntr(flags);					\
} while (0)

int ksceKernelCopyFromUserProc(SceUID pid, void *dst, const void *src, SceSize size)
{
	int ret;

	SCE_DBG_ASSERT_FUNC(pid != SCE_KERNEL_PROCESS_ID, 224, "sceKernelCopyFromUserProc");
	USER_COPY_IN_PROCESS(pid, ret, ksceKernelCopyFromUser(dst, src, size));
	return ret;
}

int ksceKernelCopyToUser(void *dst, const void *src, SceSize size)
{
	SceUInt32 dacr;
	int ret;

	if (size != 0)
		UserCopyTouchKernel(src, size);
	dacr = readDacr();
	writeDacr(DACR_ALL_CLIENT);
	ret = VmaccessCopyToUser(dst, src, size);
	writeDacr(dacr);
	return ret;
}

int ksceKernelCopyToUserProc(SceUID pid, void *dst, const void *src, SceSize size)
{
	int ret;

	SCE_DBG_ASSERT_FUNC(pid != SCE_KERNEL_PROCESS_ID, 280, "sceKernelCopyToUserProc");
	USER_COPY_IN_PROCESS(pid, ret, ksceKernelCopyToUser(dst, src, size));
	return ret;
}

/* into user memory whatever its domain */
int ksceKernelCopyToUserDomain(void *dst, const void *src, SceSize size)
{
	SceUInt32 dacr;
	int ret;

	if (size != 0) {
		UserCopyTouchKernel(src, size);
		UserCopyTouchUser(dst);
	}
	dacr = readDacr();
	writeDacr(USER_COPY_DACR_TEXT);
	ret = VmaccessCopyToUser(dst, src, size);
	writeDacr(dacr);
	return ret;
}

/* into user text: the written lines are cleaned for the instruction fetch */
static inline __attribute__((always_inline))
int UserCopyToText(void *dst, const void *src, SceSize size, SceUInt32 textDacr)
{
	SceUInt32 dacr = readDacr();
	int ret;

	writeDacr(textDacr);
	ret = VmaccessCopyToUser(dst, src, size);
	ksceKernelDcacheCleanRange((void *)((uintptr_t)dst & ~0x1f),
				   (((uintptr_t)dst + size + 0x1f) & ~0x1f) -
				   ((uintptr_t)dst & ~0x1f));
	writeDacr(dacr);
	return ret;
}

int ksceKernelCopyToUserTextDomain(void *dst, const void *src, SceSize size)
{
	if (size != 0) {
		UserCopyTouchKernel(src, size);
		UserCopyTouchUser(dst);
	}
	return UserCopyToText(dst, src, size, USER_COPY_DACR_TEXT);
}

int ksceKernelCopyToUserProcTextDomain(SceUID pid, void *dst, const void *src, SceSize size)
{
	int ret;

	USER_COPY_IN_PROCESS(pid, ret, ksceKernelCopyToUserTextDomain(dst, src, size));
	return ret;
}

int sceKernelCopyToUserProcTextDomainForDBGP(SceUID pid, void *dst, const void *src,
					     SceSize size)
{
	MMUContext saved;
	void *as;
	int flags, ret;

	SCE_DBG_ASSERT(pid != SCE_KERNEL_PROCESS_ID, 439);
	if (size != 0)
		UserCopyTouchKernel(src, size);
	flags = ksceKernelCpuSuspendIntr();
	MMUContextSave(&saved);
	ret = SysrootPIDtoAddressSpaceCB(pid, &as);
	if (ret >= 0) {
		ret = AddressSpaceSwitchContext(as);
		if (ret >= 0) {
			if (size != 0)
				UserCopyTouchUser(dst);
			ret = UserCopyToText(dst, src, size, USER_COPY_DACR_TEXT_DBGP);
		}
	}
	MMUContextRestore(&saved);
	ksceKernelCpuResumeIntr(flags);
	return ret;
}

/* name unknown: fills user words (text domain) with a value */
int SceSysmemForKernel_FCB5745A(SceUInt32 *dst, SceUInt32 value, SceSize count)
{
	SceUInt32 dacr;
	int ret;

	if (count != 0)
		UserCopyTouchUser32(dst);
	dacr = readDacr();
	writeDacr(USER_COPY_DACR_TEXT);
	ret = VmaccessFillUser32(dst, value, count);
	writeDacr(dacr);
	return ret;
}

int ksceKernelCopyFromToUser(void *dst, const void *src, SceSize size)
{
	SceUInt32 dacr = readDacr();
	int ret;

	writeDacr(DACR_ALL_CLIENT);
	ret = VmaccessCopyUserToUser(dst, src, size);
	writeDacr(dacr);
	return ret;
}

int ksceKernelCopyFromToUserProc(SceUID pid, void *dst, const void *src, SceSize size)
{
	int ret;

	SCE_DBG_ASSERT_FUNC(pid != SCE_KERNEL_PROCESS_ID, 551, "sceKernelCopyFromToUserProc");
	USER_COPY_IN_PROCESS(pid, ret, VmaccessCopyUserToUser(dst, src, size));
	return ret;
}

/* the bytes from src on that hold value, word by word */
SceSSize ksceKernelCountFillValueFromUser(const void *src, SceUInt32 value, SceSize size)
{
	SceUInt32 dacr = readDacr();
	SceSSize ret;

	writeDacr(DACR_ALL_CLIENT);
	ret = VmaccessCountFillUser32(src, value, size);
	writeDacr(dacr);
	return ret;
}

SceSSize ksceKernelCountFillValueFromUserProc(SceUID pid, const void *src, SceUInt32 value,
					     SceSize size)
{
	int ret;

	SCE_DBG_ASSERT_FUNC(pid != SCE_KERNEL_PROCESS_ID, 595, "sceKernelCountFillValueFromUserProc");
	USER_COPY_IN_PROCESS(pid, ret, VmaccessCountFillUser32(src, value, size));
	return ret;
}

SceSSize ksceKernelCountFillValue64FromUser(const void *src, SceUInt64 value, SceSize size)
{
	SceUInt32 dacr = readDacr();
	SceSSize ret;

	writeDacr(DACR_ALL_CLIENT);
	ret = VmaccessCountFillUser64(src, (SceUInt32)value, (SceUInt32)(value >> 32), size);
	writeDacr(dacr);
	return ret;
}

SceSSize ksceKernelCountFillValue64FromUserProc(SceUID pid, const void *src, SceUInt64 value,
					       SceSize size)
{
	int ret;

	SCE_DBG_ASSERT_FUNC(pid != SCE_KERNEL_PROCESS_ID, 639, "sceKernelCountFillValue64FromUserProc");
	USER_COPY_IN_PROCESS(pid, ret, VmaccessCountFillUser64(src, (SceUInt32)value,
							       (SceUInt32)(value >> 32), size));
	return ret;
}

SceSize ksceKernelStrnlenFromUser(const char *str, SceSize max)
{
	SceUInt32 dacr = readDacr();
	SceSize ret;

	writeDacr(DACR_ALL_CLIENT);
	ret = VmaccessStrnlenUser(str, max);
	writeDacr(dacr);
	return ret;
}

SceSSize ksceKernelStrnlenFromUserProc(SceUID pid, const char *str, SceSize max)
{
	int ret;

	SCE_DBG_ASSERT_FUNC(pid != SCE_KERNEL_PROCESS_ID, 679, "sceKernelStrnlenFromUserProc");
	USER_COPY_IN_PROCESS(pid, ret, VmaccessStrnlenUser(str, max));
	return ret;
}

SceSSize ksceKernelStrncpyFromUser(char *dst, const char *src, SceSize n)
{
	SceUInt32 dacr;
	SceSSize ret;

	if (n != 0)
		UserCopyTouchKernel(dst, n);
	dacr = readDacr();
	writeDacr(DACR_ALL_CLIENT);
	ret = VmaccessStrncpyFromUser(dst, src, n);
	writeDacr(dacr);
	return ret;
}

SceSSize ksceKernelStrncpyFromUserProc(SceUID pid, char *dst, const char *src, SceSize n)
{
	int ret;

	SCE_DBG_ASSERT_FUNC(pid != SCE_KERNEL_PROCESS_ID, 734, "sceKernelStrncpyFromUserProc");
	USER_COPY_IN_PROCESS(pid, ret, VmaccessStrncpyFromUser(dst, src, n));
	return ret;
}

SceSSize ksceKernelStrncpyToUser(char *dst, const char *src, SceSize n)
{
	SceUInt32 dacr;
	SceSSize ret;

	(void)*(const volatile char *)src;
	dacr = readDacr();
	writeDacr(DACR_ALL_CLIENT);
	ret = VmaccessStrncpyToUser(dst, src, n);
	writeDacr(dacr);
	return ret;
}

SceSSize ksceKernelStrncpyToUserProc(SceUID pid, char *dst, const char *src, SceSize n)
{
	int ret;

	SCE_DBG_ASSERT_FUNC(pid != SCE_KERNEL_PROCESS_ID, 786, "sceKernelStrncpyToUserProc");
	(void)*(const volatile char *)src;
	USER_COPY_IN_PROCESS(pid, ret, VmaccessStrncpyToUser(dst, src, n));
	return ret;
}
