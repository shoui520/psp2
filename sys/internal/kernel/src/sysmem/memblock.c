/*
 * SceSysmem: memory blocks. The kernel and user allocation entry points pick
 * the address space and partition for a block's type and hand the command to
 * partition.c; the rest looks blocks up by GUID or address and reports or
 * changes them.
 */

#include <sysmem/cp15.h>
#include <sysmem/uid_guid.h>
#include <sysmem/dipsw.h>
#include <sysmem/sysroot.h>
#pragma push_macro("__FILE_NAME__")
#undef __FILE_NAME__
#define __FILE_NAME__ "memblock_obj_module.h"
#include "memblock_obj_module.h"
#pragma pop_macro("__FILE_NAME__")
#include "memblock/status_module.h"
#include <sysmem/sysclib.h>
#include <dmacmgr/dmacmgr.h>

#undef __FILE_NAME__
#define __FILE_NAME__ "memblock.c"

/* address_space.c, sysroot, uid.c, user_copy.c, phymem.c, memblock_db.c, memblock_obj.c */

/* MemBlockTypeInfo attr: how a type is allocated */
#define MEMBLOCK_TYPE_USER_INFO		0x00000001	/* user code may see its info */
#define MEMBLOCK_TYPE_KERNEL		0x00001000	/* the kernel address space */
#define MEMBLOCK_TYPE_KERNEL_ONLY	0x00002000	/* ... called by the kernel only */
#define MEMBLOCK_TYPE_KERNEL_PID	0x00004000	/* ... on the kernel's behalf */
#define MEMBLOCK_TYPE_TOOL_PARTITION	0x00008000	/* the tool partition */
#define MEMBLOCK_TYPE_PADDR		0x00010000	/* at given physical pages */
#define MEMBLOCK_TYPE_TOOL_PHYMEMPART	0x00020000	/* needs the tool dipsw */
#define MEMBLOCK_TYPE_PADDR_KERNEL	0x00040000	/* at given pages, kernel space */
#define MEMBLOCK_TYPE_SECTION		0x00080000	/* whole MiBs, 1 MiB aligned */
#define MEMBLOCK_TYPE_LOW		0x00100000	/* the caller's address space */

/* the block for the process the command names (the kernel for kernel codes) */
int AllocMemBlockLowForKernelByCommand(SceKernelAllocMemBlockCommand *command)
{
	SceUID pid = (command->code & 0xf0000000) ? SCE_KERNEL_PROCESS_ID : command->opt.pid;
	int ret;

	ret = SysrootPIDtoAddressSpaceCB(pid, (void **)&command->addressSpace);
	if (ret < 0)
		return ret;
	SCE_DBG_ASSERT_CHECK(command->addressSpace != NULL, 382);
	return AddressSpaceAllocMemBlockByCommand(command->addressSpace, command);
}

/* the block in the address space of the process opt.pid names */
int AllocMemBlockLowByCommand(SceKernelAllocMemBlockCommand *command)
{
	int ret;

	ret = SysrootPIDtoAddressSpaceCB(command->opt.pid, (void **)&command->addressSpace);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 397, "sceKernelSysrootPIDtoAddressSpaceCB(0x%08x) failed 0x%08x\n",
			       command->opt.pid, ret);
		return ret;
	}
	SCE_DBG_ASSERT_CHECK(command->addressSpace != NULL, 400);
	return AddressSpaceAllocMemBlockByCommand(command->addressSpace, command);
}

/* a kernel allocation, routed by its type's attributes */
int AllocMemBlockForKernelByCommand(SceKernelAllocMemBlockCommand *command)
{
	SceUInt32 attr = command->typeInfo->attr;
	uintptr_t paddr;

	if (attr & MEMBLOCK_TYPE_KERNEL)
		return AllocMemBlockLowForKernelByCommand(command);
	if (attr & MEMBLOCK_TYPE_KERNEL_ONLY) {
		if (SceSysrootForKernel_7385CADE() != SCE_KERNEL_PROCESS_ID)
			return SCE_KERNEL_ERROR_MEMBLOCK_TYPE_FOR_KERNEL_PROCESS;
		return AllocMemBlockLowForKernelByCommand(command);
	}
	if (attr & MEMBLOCK_TYPE_KERNEL_PID) {
		command->opt.pid = SCE_KERNEL_PROCESS_ID;
		return AllocMemBlockLowForKernelByCommand(command);
	}
	if (attr & MEMBLOCK_TYPE_TOOL_PARTITION)
		return sceKernelAllocPartitionMemBlock(SCE_KERNEL_PARTITION_ROOT_ID, command->name,
							command->type, command->size, &command->opt);
	if (attr & MEMBLOCK_TYPE_PADDR) {
		if (!(command->opt.attr & SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR))
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		paddr = command->opt.paddr;
		if (command->size < 0x100000) {
			if (paddr & 0xfff) {
				SCE_DBG_PRINTF(0xb, 507,
					       "SCE_KERNEL_ERROR_INVALID_ARGUMENT:vsize=0x%08x,pbase=0x%08x\n",
					       command->size, paddr);
				return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
			}
		} else if (paddr & 0xfffff) {
			SCE_DBG_PRINTF(0xb, 501,
				       "SCE_KERNEL_ERROR_INVALID_ARGUMENT:vsize=0x%08x,pbase=0x%08x\n",
				       command->size, paddr);
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		}
		command->opt.attr |= ALLOC_ATTR_NO_CLEAR;
		return AllocMemBlockLowByCommand(command);
	}
	if (attr & MEMBLOCK_TYPE_TOOL_PHYMEMPART) {
		if (sceKernelCheckDipsw(SCE_DIPSW_ENABLE_TOOL_PHYMEMPART))
			return AllocMemBlockLowByCommand(command);
		SCE_DBG_PRINTF(0xb, 524, "Only SCE_DIPSW_ENABLE_TOOL_PHYMEMPART\n");
		SCE_DBG_ASSERT_FAIL(525);
		return SCE_KERNEL_ERROR_ILLEGAL_MEMBLOCK_TYPE;
	}
	if (attr & MEMBLOCK_TYPE_PADDR_KERNEL) {
		if (!(command->opt.attr & SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR)) {
			SCE_DBG_PRINTF(0xb, 536, "There is no HAS_PBASE.\n");
			SCE_DBG_PRINTF(0xb, 537, "SCE_KERNEL_ERROR_INVALID_ARGUMENT:attr=0x%08x\n",
				       command->opt.attr);
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		}
		return AllocMemBlockLowForKernelByCommand(command);
	}
	if (attr & MEMBLOCK_TYPE_SECTION) {
		if ((command->size & 0xfffff) || command->opt.vbase != 0 || command->opt.paddr != 0 ||
		    command->opt.alignment != 0)
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		command->opt.alignment = 0x100000;
		if (command->opt.extraLow != 0 || command->opt.extraHigh != 0 ||
		    command->opt.paddr_list != NULL)
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		command->opt.attr |= SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_PHYCONT;
		return AllocMemBlockLowByCommand(command);
	}
	if (attr & MEMBLOCK_TYPE_LOW)
		return AllocMemBlockLowByCommand(command);
	return -1;
}

#if !SYSMEM_NSKBL
/* name unknown: a list block mapped (flags bit 0 only) */
int MapMemBlockByGUID(SceUID guid, SceUInt32 flags)
{
	MemBlockObject *block;
	int intr, ret;

	ret = sceGUIDReferObjectWithClass(guid, SceSysmemForKernel_86681B64(),
					   (SceObjectBase **)&block);
	if (ret < 0)
		return ret;
	if (flags & ~1) {
		ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	} else {
		intr = SceCpuForDriver_4C38CE4D(&block->lock);
		ret = PartitionMapMemBlock(block->partition, block, NULL, flags);
		sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&block->lock, intr);
		sceGUIDReleaseObject(guid);
	}
	return ret;
}

/* whole units of a partial block mapped or unmapped (op) */
int PartialMapUnmapMemBlock(SceUID guid, SceUInt32 op, uintptr_t va, SceSize size)
{
	MemBlockObject *block;
	SceSize unit;
	int ret;

	ret = sceGUIDReferObjectWithClass(guid, SceSysmemForKernel_86681B64(),
					   (SceObjectBase **)&block);
	if (ret < 0) {
		SCE_DBG_PRINTF(3, 1059, "GUIDReferMemBlock(0x%08x) failed 0x%08x\n", guid, ret);
		return ret;
	}
	if (!(block->attr & MEMBLOCK_ATTR_PARTIAL)) {
		ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		SCE_DBG_PRINTF(0xb, 1064, "Cannot allow parial op 0x%08x\n", block->attr);
	} else {
		unit = MemBlockGetPartialInfoRaw(block)->unitSize;
		if ((va | size) & (unit - 1)) {
			SCE_DBG_PRINTF(0xb, 1074,
				       "INVALID_ARGUMENT: vbase %p, vsize 0x%08x, unitSize 0x%08x\n",
				       va, size, unit);
			ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		} else if (va < block->vbase || block->size < size - block->vbase + va) {
			ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		} else {
			ret = MemBlockPartialOpRange(block->partition, block, op, va, size);
		}
	}
	sceGUIDReleaseObject(guid);
	return ret;
}

/* name unknown: a block's details; levels below the caller's see only fixed heap pages */
int MemBlockGetInfoDetails(MemBlockObject *block, SceInt32 level, MemBlockInfoDetails *info)
{
	MemBlockObject *src;
	SceUInt32 code, attr, opt;
	uintptr_t pa, vbase;
	SceSize size;
	int ret;

	if (block->code == 0) {
		info->name = "FREE";
		goto check_name;
	}
	if (block->attr & MEMBLOCK_ATTR_WHOLE) {
		if (block->smbPage == NULL) {
			info->type = 0;
			info->mapSize = 0;
		} else {
			if (MemBlockCode2Type(block->code, &info->type) < 0)
				info->type = SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_RW;
			info->mapSize = block->smbSize;
		}
		info->guid = 0;
		goto named;
	}
	MemBlockCode2Type(block->code, &info->type);
	info->guid = block->guid;
	if (info->guid > 0) {
		if (SceSysmemForKernel_4E923101(info->guid) > level) {
			vbase = block->vbase;
			size = block->size;
			info->guid = 0;
			info->name = "SceSysrootFixedHeap";
			info->vbase = vbase;
			info->size = size;
			info->mapSize = size;
			info->headFree = 0;
			info->tailFree = 0;
			info->partitionUid = -1;
			info->phyMemPart = SceSysmemForKernel_0164D817();
			return SCE_KERNEL_ERROR_UID_NOT_VISIBLE;
		}
		if (info->guid > 0) {
			info->name = SceSysmemForDriver_E655852F(info->guid);
			goto check_name;
		}
	}
	if (info->guid != 0 && info->guid != -1) {
		info->name = "SceNULL";
		goto check_name;
	}
named:
	if (block->flags & MEMBLOCK_FLAG_NAMED)
		info->name = MemBlockGetNameRaw(block);
	else
		info->name = "SceNULL";
check_name:
	ret = sceKernelVAtoPA(info->name, &pa);
	code = block->code;
	if (ret < 0)
		info->name = "SceInvalid";
	attr = block->attr;
	vbase = block->vbase;
	size = block->size;
	info->vbase = vbase;
	info->allocationAttributes = (attr >> 16) & 0xf;
	info->size = size;
	src = block;
	if (code != 0 && !(attr & MEMBLOCK_ATTR_WHOLE)) {
		opt = block->flags & MEMBLOCK_OPT_MASK;
		if (opt == MEMBLOCK_OPT_BASE_BLOCK && (attr & MEMBLOCK_ATTR_KEEP_PHY))
			src = MemBlockOptGetBaseBlockRaw(block);
		else if (opt == MEMBLOCK_OPT_BASE_INFO && (attr & MEMBLOCK_ATTR_KEEP_PHY))
			src = ((MemBlockBaseInfo *)MemBlockOptGetBaseInfoRaw(block))->block;
		info->mapSize = MemBlockGetAllocMapSizeRaw(src);
		info->headFree = MemBlockGetHeadFreeSize(block);
		info->tailFree = MemBlockGetTailFreeSize(block);
		if (src->partition != NULL)
			info->partitionUid = ((struct Partition *)src->partition)->uid;
		else
			info->partitionUid = -1;
	} else {
		info->headFree = 0;
		info->tailFree = 0;
		info->partitionUid = -1;
		if (block->smbPage != NULL)
			info->mapSize = block->smbSize;
		else
			info->mapSize = 0;
	}
	info->phyMemPart = MemBlockGetPhyMemPartRaw(src);
	return 0;
}

/* name unknown: pid joins the block's clients */
int MemBlockClientInfoAddGUID(MemBlockClientInfo *info, SceUID pid)
{
	SceUID *slot;
	SceUInt32 i, capacity;
	int intr;

	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&info->lock);
	capacity = info->capacity;
	slot = info->guids;
	if (capacity != 0) {
		if (*slot != -1) {
			i = 0;
			do {
				i++;
				slot++;
				if (i == capacity)
					goto out;
			} while (*slot != -1);
		}
		*slot = pid;
		info->count++;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, intr);
		return 0;
	}
out:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, intr);
	return 0;
}

/* name unknown: pid leaves the block's clients */
int MemBlockClientInfoRemoveGUID(MemBlockClientInfo *info, SceUID pid)
{
	SceUID *slot;
	SceUInt32 i, capacity;
	int intr;

	if (pid == -1)
		return SCE_KERNEL_ERROR_SYSMEM_MEMBLOCK_ERROR;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&info->lock);
	capacity = info->capacity;
	slot = info->guids;
	if (capacity != 0) {
		if (*slot != pid) {
			i = 0;
			do {
				i++;
				slot++;
				if (i == capacity)
					goto out;
			} while (*slot != pid);
		}
		*slot = -1;
		info->count--;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, intr);
		return 0;
	}
out:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&info->lock, intr);
	return 0;
}

/* name unknown: whether user code may see a block of the code */
int MemBlockCodeCheckUserInfo(SceUInt32 code)
{
	const MemBlockCodeInfo *entry = MemBlockCode2CodeInfo(code);

	if (entry == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	return (entry->typeInfo->attr & MEMBLOCK_TYPE_USER_INFO) ? 0 :
	       SCE_KERNEL_ERROR_INVALID_ARGUMENT;
}

/* name unknown: the object of a GUID, or of a PUID of pid (0: the caller) */
int UIDReferObjectWithLevelForPID(SceUID pid, SceUID uid, SceUInt32 level, SceObjectBase **object)
{
	if (pid == 0)
		pid = SceSysrootForKernel_7385CADE();
	if (pid == SCE_KERNEL_PROCESS_ID) {
		if (uid & 0x40000000)
			return -1;
	} else if (uid & 0x40000000) {
		return PUIDReferObjectWithLevel(pid, uid, level, object);
	}
	return sceGUIDReferObjectWithLevel(uid, level, object);
}

/* name unknown */
int UIDReleaseObjectForPID(SceUID pid, SceUID uid)
{
	if (pid == 0)
		pid = SceSysrootForKernel_7385CADE();
	if (pid == SCE_KERNEL_PROCESS_ID) {
		if (uid & 0x40000000)
			return -1;
	} else if (uid & 0x40000000) {
		return UIDEntryReleaseObject(pid, uid);
	}
	return sceGUIDReleaseObject(uid);
}
#endif

#define MEMBLOCK_INFO_BY_RANGE		2	/* [va, va + size) must lie in the block */

#if !SYSMEM_NSKBL
/* the user info of the caller's block holding va */
int GetMemBlockInfoByRangeForUser(SceUInt32 mode, uintptr_t va, SceSize size,
				  SceKernelMemBlockInfo *userInfo)
{
	SceKernelMemBlockInfo info;
	MemBlockObject *block;
	uintptr_t vbase;
	SceUInt32 type, code;
	int ret;

	ret = sceKernelCopyFromUser(&info, userInfo, sizeof(info));
	if (ret < 0)
		return ret;
	if (info.size != sizeof(info))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = AddressSpaceFindMemBlockCB(sceKernelSysrootGetCurrentAddressSpaceCB(), va, 0, &block);
	if (ret < 0)
		return ret;
	if (mode == MEMBLOCK_INFO_BY_RANGE &&
	    (va < block->vbase || block->size < va + (size - block->vbase)))
		return SCE_KERNEL_ERROR_MEMBLOCK_RANGE_ERROR;
	ret = MemBlockCodeCheckUserInfo(block->code);
	if (ret < 0)
		return ret;
	vbase = block->vbase;
	info.mappedBase = (void *)(vbase + MemBlockGetHeadFreeSize(block));
	info.mappedSize = MemBlockGetMapSize(block);
	SCE_DBG_ASSERT_CHECK(info.mappedSize != 0, 5080);
	code = block->code;
	MemBlockCode2Type(code, &type);
	info.memoryType = (SceUInt8)(code >> 8) & 0xd0;
	info.access = (SceUInt8)code >> 4 & 6;
	info.type = type;
	return sceKernelCopyToUser(userInfo, &info, sizeof(info));
}
#endif

int MemBlockOptSetClientInfoRaw(MemBlockObject *block, const SceKernelAllocMemBlockKernelOpt *opt);

#define AS_LOCK_RANGE_MAX		16	/* blocks one call may lock */

#if !SYSMEM_NSKBL
/* the access a lock mode needs the blocks to have */
int ASLockUnlockModeCheck(SceUInt32 mode, SceUInt32 *access)
{
	SCE_DBG_ASSERT_CHECK(access != NULL, 1735);
	switch (mode) {
	case 0:
		return 0;
	case 1:
		*access = 4;
		return 0;
	case 2:
	case 3:
		*access = 2;
		return 0;
	case 0x10:
		*access = 0x40;
		return 0;
	case 0x20:
	case 0x30:
		*access = 0x20;
		return 0;
	default:
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
}

/* the blocks of [va, va + size) unlocked once all of them are checked */
int ASUnlockRangeWithMode(struct AddressSpaceObject *as, SceUInt32 mode, uintptr_t va,
			  SceSize size)
{
	MemBlockObject *blocks[AS_LOCK_RANGE_MAX], *block;
	SceUInt32 access = 0;
	uintptr_t cur, end;
	int count = 0, ret;

	ret = ASLockUnlockModeCheck(mode, &access);
	if (ret < 0)
		return ret;
	end = va + size;
	if (va >= end)
		return 0;
	cur = va;
	do {
		ret = AddressSpaceFindMemBlockCtlByAddr(as, cur, 0, &block);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 1872,
				       "0x%08x+0x%08x: AddressSpaceFindMemBlockCtlByAddr(0x%08x) failed 0x%08x\n",
				       va, size, cur, ret);
			SCE_DBG_ASSERT_FAIL(1876);
			return ret;
		}
		if (cur < block->vbase || cur - block->vbase >= block->size) {
			SCE_DBG_PRINTF(0xb, 1885, "%p:0x%08x+0x%08x\n", block, block->vbase,
				       block->size);
			SCE_DBG_ASSERT_FAIL(1886);
		}
		if (mode != 0 && !(block->code & access)) {
			SCE_DBG_PRINTF(0xb, 1897, "ubase=0x%08x, %p:0x%08x & 0x%08x\n", cur, block,
				       block->code, access);
			if (block->code == 0)
				SCE_DBG_ASSERT_FAIL(1899);
			goto fail;
		}
		if (count == AS_LOCK_RANGE_MAX)
			goto fail;
		blocks[count++] = block;
		cur = block->vbase + block->size;
	} while (cur < end);
	do {
		block = blocks[--count];
		MemBlockDecLockRef(block);
	} while (count != 0);
	return 0;
fail:
	return ret != 0 ? ret : SCE_KERNEL_ERROR_ADDRESS_SPACE_UNLOCK_ERROR;
}

/* name unknown: [va, va + size) of pid (0: the caller) unlocked */
int UnlockRangeForPID(SceUID pid, uintptr_t va, SceSize size)
{
	void *as;
	int ret;

	if (pid == 0) {
		as = sceKernelSysrootGetCurrentAddressSpaceCB();
	} else {
		ret = SysrootPIDtoAddressSpaceCB(pid, &as);
		if (ret < 0)
			return ret;
	}
	return ASUnlockRangeWithMode(as, 0, va, size);
}

/* the blocks of [va, va + size) locked, all or none */
int ASLockRangeWithMode(struct AddressSpaceObject *as, SceUInt32 mode, uintptr_t va, SceSize size)
{
	MemBlockObject *blocks[AS_LOCK_RANGE_MAX], *block;
	SceUInt32 access = 0;
	uintptr_t cur;
	int count = 0, ret;

	ret = ASLockUnlockModeCheck(mode, &access);
	if (ret < 0)
		return ret;
	for (cur = va; cur < va + size; cur = block->vbase + block->size) {
		ret = AddressSpaceFindMemBlockCtlByAddr(as, cur, 0, &block);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 1785,
				       "0x%08x+0x%08x: AddressSpaceFindMemBlockCtlByAddr(0x%08x) failed 0x%08x\n",
				       va, size, cur, ret);
			goto unlock;
		}
		if (cur < block->vbase || cur - block->vbase >= block->size) {
			SCE_DBG_PRINTF(0xb, 1794, "%p:0x%08x+0x%08x\n", block, block->vbase,
				       block->size);
			SCE_DBG_ASSERT_FAIL(1795);
		}
		if (mode != 0 && !(block->code & access)) {
			SCE_DBG_PRINTF(0xb, 1806, "ubase=0x%08x, %p:0x%08x & 0x%08x\n", cur, block,
				       block->code, access);
			if (block->code == 0)
				SCE_DBG_ASSERT_FAIL(1808);
			if (ret == 0)
				ret = SCE_KERNEL_ERROR_ADDRESS_SPACE_LOCK_ERROR;
			goto unlock;
		}
		MemBlockIncLockRef(block);
		if (count == AS_LOCK_RANGE_MAX) {
			ret = SCE_KERNEL_ERROR_ERROR;
			goto unlock;
		}
		blocks[count++] = block;
	}
	return 0;
unlock:
	while (count != 0) {
		block = blocks[--count];
		MemBlockDecLockRef(block);
	}
	return ret;
}

/* name unknown: [va, va + size) of pid (0: the caller) locked */
int LockRangeForPID(SceUID pid, uintptr_t va, SceSize size)
{
	void *as;
	int ret;

	if (pid == 0) {
		as = sceKernelSysrootGetCurrentAddressSpaceCB();
	} else {
		ret = SysrootPIDtoAddressSpaceCB(pid, &as);
		if (ret < 0)
			return ret;
	}
	return ASLockRangeWithMode(as, 0, va, size);
}

/*
 * name unknown: the physical ranges behind a virtual range, from the MMU
 * (pid 0: through mode) or the address space of pid.
 */
int VARangeToPAVectorForPID(SceUID pid, uint32_t mode, const SceKernelPARange *range,
			    SceKernelPAVector *vector, int *memoryType)
{
	SceKernelPARange *cur, *end;
	uintptr_t va, pa, base;
	SceSize size, chunk, len;
	SceUInt32 count, total;
	void *as;
	int ret;

	if (pid != 0) {
		ret = SysrootPIDtoAddressSpaceCB(pid, &as);
		if (ret < 0)
			return ret;
		return AddressSpaceVARangeToPAVector(as, range, vector, memoryType);
	}
	cur = vector->pRanges;
	if (cur == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	end = cur + vector->capacity;
	size = range->size;
	va = range->addr;
	vector->nDataInVector = 0;
	vector->total = 0;
	if (size == 0) {
		len = 0;
		count = 0;
		base = 0;
	} else {
		len = 0;
		base = 0;
		do {
			ret = MMUVAtoPAWithMemoryType(mode, va, &pa, memoryType);
			if (ret < 0)
				return ret;
			if (base + len != pa) {
				/* a new physical run: the last one is complete */
				total = vector->total;
				if (total != 0) {
					if (cur != end) {
						cur->addr = base;
						cur->size = len;
						cur++;
					}
				}
				len = 0;
				vector->total = total + 1;
				base = pa;
			}
			chunk = (~va & 0xfff) + 1;
			if (size <= chunk)
				chunk = size;
			size -= chunk;
			va += chunk;
			len += chunk;
		} while (size != 0);
		count = vector->total;
		if (vector->capacity <= vector->total)
			count = vector->capacity;
	}
	if (cur != end) {
		cur->addr = base;
		cur->size = len;
	}
	vector->nDataInVector = count;
	return 0;
}
#endif

#pragma push_macro("__FILE_NAME__")
#undef __FILE_NAME__
#define __FILE_NAME__ "memblock_obj_module.h"

#if !SYSMEM_NSKBL
/* name from the record: pid leaves the block's clients */
__attribute__((noinline)) void MemBlockOptGetClientInfo(MemBlockObject *block, SceUID pid)
{
	if ((block->flags & MEMBLOCK_OPT_MASK) != MEMBLOCK_OPT_CLIENT_INFO)
		SCE_DBG_PRINTF(0xb, 1052, "Warning:status=0x%08x\n", block->flags);
	if (block->opt != 0)
		MemBlockClientInfoRemoveGUID((MemBlockClientInfo *)block->opt, pid);
}
#endif

#pragma pop_macro("__FILE_NAME__")

/* name unknown: the block a mirror shares, and its base info */
int MemBlockOptGetBase(MemBlockObject *block, MemBlockObject **base, void **info)
{
	SceUInt32 opt;

	*base = NULL;
	if (block == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (block->code == 0 || (block->attr & MEMBLOCK_ATTR_WHOLE) ||
	    (block->attr & MEMBLOCK_ATTR_CLIENT))
		return 0;
	opt = block->flags & MEMBLOCK_OPT_MASK;
	if (opt == MEMBLOCK_OPT_CLIENT_INFO)
		return 0;
	if (opt == MEMBLOCK_OPT_BASE_INFO) {
		*info = MemBlockOptGetBaseInfoRaw(block);
		*base = ((MemBlockBaseInfo *)*info)->block;
		return 0;
	}
	if (opt == MEMBLOCK_OPT_BASE_BLOCK) {
		MemBlockObject *mirrored = MemBlockOptGetBaseBlockRaw(block);

		if (mirrored != NULL)
			*base = mirrored;
	}
	return 0;
}

#if !SYSMEM_NSKBL
/* an allocation given as a whole command */
int sceKernelAllocMemBlockByCommand(SceKernelAllocMemBlockCommand *command)
{
	SceUInt32 type;
	int ret;

	if (command == NULL) {
		SCE_DBG_PRINTF(0xb, 654, "pCommand is NULL\n");
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	if (command->cmdSize != sizeof(*command)) {
		SCE_DBG_PRINTF(0xb, 658, "0x%08x != 0x%08x\n", command->cmdSize, sizeof(*command));
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE;
	}
	if (command->code == 0) {
		type = command->type;
	} else {
		ret = MemBlockCode2TypeInfo(command->code, &command->typeInfo);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 666, "MemBlockCode2TypeInfo failed 0x%08x\n", ret);
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		}
		type = command->typeInfo->type;
		command->type = type;
	}
	if (type == 0 || MemBlockType2TypeInfo(type, &command->typeInfo) < 0)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	command->code = command->typeInfo->code;
	if (command->name == NULL || (command->size & 0xfff) || (command->padLow & 0xfff) ||
	    (command->padHigh & 0xfff) || command->size <= command->padLow + command->padHigh)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	command->opt.size = sizeof(command->opt);
	if (!(command->opt.attr & SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PID))
		command->opt.pid = SCE_KERNEL_PROCESS_ID;
	ret = AllocMemBlockForKernelByCommand(command);
	if (ret < 0)
		SCE_DBG_PRINTF(0xb, 707, "AllocMemBlockForKernelByCommand failed 0x%08x\n", ret);
	return ret;
}
#endif

/* the kernel tmp types as user code may ask for them */
static inline void KernelTmp2UserKtmpType(SceUInt32 type)
{
	if (!sceKernelSysrootIsUserModeThread())
		return;
	switch (type) {
	case SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TMP_NC_R:
	case SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TMP_NC_RW:
	case SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TMP_R:
	case SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TMP_RW:
		break;
	default:
		SCE_DBG_ASSERT_FAIL(355);
		break;
	}
}

#define MEMBLOCK_CODE_W			0x20

/* a kernel block, with where it landed in info */
SceUID sceKernelAllocMemBlockWithInfoForKernel(const char *name, SceUInt32 type, SceSize size,
				       const SceKernelAllocMemBlockKernelOpt *opt,
				       MemBlockAllocResult *info)
{
	SceKernelAllocMemBlockCommand command;
	SceUInt32 attr;
	int ret;

	AllocMemBlockCommandInitWithType(&command, name, type, size);
#if !defined(SYSMEM_TZS) && !defined(SYSMEM_SKBL)	/* no user mode threads in the secure world */
	switch (type) {
	case SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TMP_NC_R:
	case SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TMP_NC_RW:
	case SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TMP_R:
	case SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TMP_RW:
		KernelTmp2UserKtmpType(type);
		break;
	}
#endif
	ret = MemBlockType2TypeInfo(type, &command.typeInfo);
	if (ret < 0) {
		SCE_DBG_PRINTF_FUNC(0xb, 747, "sceKernelAllocMemBlockWithInfoForKernel",
			"MemBlockType2TypeInfo failed 0x%08x\n", ret);
		return ret;
	}
	command.code = command.typeInfo->code;
	attr = command.opt.attr;
	if (opt != NULL) {
		SceSize optSize = opt->size;

		attr = opt->attr;
		command.opt.attr = attr;
		if (optSize != sizeof(*opt) && optSize != 0x30)
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		if (attr & SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PID)
			command.opt.pid = opt->pid;
		if (attr & SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_MIRROR_BLOCKID)
			command.opt.mirror_blockid = opt->mirror_blockid;
		if (attr & ALLOC_ATTR_HAS_VBASE)
			command.opt.vbase = opt->vbase;
		if (attr & SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR)
			command.opt.paddr = opt->paddr;
		if (attr & SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ALIGNMENT)
			command.opt.alignment = opt->alignment;
		if (attr & ALLOC_ATTR_HAS_EXTRA_LOW)
			command.opt.extraLow = opt->extraLow;
		if (attr & ALLOC_ATTR_HAS_EXTRA_HIGH)
			command.opt.extraHigh = opt->extraHigh;
		if (attr & ALLOC_ATTR_HAS_CLIENT_INFO) {
			command.opt.openFlags = opt->openFlags;
			command.opt.capabilities[0] = opt->capabilities[0];
			command.opt.capabilities[1] = opt->capabilities[1];
			command.opt.capabilities[2] = opt->capabilities[2];
			command.opt.capabilities[3] = opt->capabilities[3];
			command.opt.capabilities[4] = opt->capabilities[4];
			command.opt.capabilities[5] = opt->capabilities[5];
			command.opt.capabilities[6] = opt->capabilities[6];
			command.opt.capabilities[7] = opt->capabilities[7];
		}
		if (attr & SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR_LIST)
			command.opt.paddr_list = opt->paddr_list;
		if (attr & ALLOC_ATTR_HAS_UNIT_SIZE)
			command.opt.unitSize = opt->unitSize;
		if (optSize >= sizeof(*opt) && (attr & ALLOC_ATTR_HAS_DOMAIN))
			command.opt.domain = opt->domain;
	}
	if (attr & ALLOC_ATTR_KEEP_CLEAR) {
		attr &= ~ALLOC_ATTR_KEEP_CLEAR;
		command.opt.attr = attr;
	} else if (!(command.code & MEMBLOCK_CODE_W)) {
		if (sceKernelCheckDipsw(DIPSW_NO_CLEAR))
			command.opt.attr |= ALLOC_ATTR_NO_CLEAR;
		attr = command.opt.attr;
	}
	if (!(attr & SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PID)) {
		command.opt.attr = attr | SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PID;
#if defined(SYSMEM_TZS) || defined(SYSMEM_SKBL)
		command.opt.pid = SCE_KERNEL_PROCESS_ID;
#else
		command.opt.pid = SceSysrootForKernel_7385CADE();
#endif
	}
	if (info != NULL && info->size == sizeof(*info))
		command.result = info;
	ret = AllocMemBlockForKernelByCommand(&command);
	if (ret < 0)
		SCE_DBG_PRINTF_FUNC(0xb, 844, "sceKernelAllocMemBlockWithInfoForKernel",
			"AllocMemBlockForKernelByCommand failed 0x%08x\n", ret);
	return ret;
}

SceUID sceKernelAllocMemBlockForKernel(const char *name, SceKernelMemBlockType type, SceSize size,
			       SceKernelAllocMemBlockKernelOpt *opt)
{
	return sceKernelAllocMemBlockWithInfoForKernel(name, type, size, opt, NULL);
}

#if !SYSMEM_NSKBL
/* the access a type grants, as SCE_KERNEL_MEMORY_REF_PERM bits */
SceUInt32 SceSysmemForDriver_6A0792A3(SceUInt32 type)
{
	SceUInt32 code, perm;

	MemBlockType2Code(type, &code);
	perm = (code >> 2) & 1;
	if (code & 2)
		perm |= 2;
	if (code & 1)
		perm |= 4;
	if (code & 0x40)
		perm |= 0x10;
	if (code & 0x20)
		perm |= 0x20;
	if (code & 0x10)
		perm |= 0x40;
	return perm;
}

/* name unknown: a type's memory kind, folded to the main ones */
SceUInt32 SceSysmemForDriver_CB0F3A33(SceUInt32 type)
{
	SceUInt32 code;

	MemBlockType2Code(type, &code);
	code &= 0xf00000;
	switch (code) {
	case 0x500000:
	case 0x600000:
	case 0x800000:
	case 0x900000:
	case 0xa00000:
	case 0xb00000:
	case 0xf00000:
		return 0x200000;
	case 0x400000:
	case 0xc00000:
		return 0x400000;
	default:
		return code;
	}
}

int sceKernelMemBlockType2Memtype(SceUInt32 type)
{
	SceUInt32 code;
	int ret;

	ret = MemBlockType2Code(type, &code);
	if (ret >= 0)
		ret = MEMBLOCK_CODE_MEMORY_TYPE(code);
	return ret;
}

/* name unknown: the low byte of a type's code */
SceUInt32 SceSysmemForKernel_48D2E408(SceUInt32 type)
{
	SceUInt32 code;

	MemBlockType2Code(type, &code);
	return (SceUInt8)code;
}

int SceSysmemForDriver_04059C4B(SceUID guid, int flags)
{
	return MapMemBlockByGUID(guid, flags);
}

int sceKernelMapMemBlock(SceUID guid)
{
	return MapMemBlockByGUID(guid, 0);
}

int sceKernelPartialAllocMemBlock(SceUID guid, uintptr_t va, SceSize size, SceUInt32 flags)
{
	if (flags & ~SCE_KERNEL_MEMBLOCK_PARTIAL_CHECK_FREE)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	return PartialMapUnmapMemBlock(guid, flags | PARTIAL_OP_ALLOC, va, size);
}

int sceKernelPartialFreeMemBlock(SceUID guid, uintptr_t va, SceSize size, SceUInt32 flags)
{
	if ((flags & PARTIAL_CHECK_MASK) > SCE_KERNEL_MEMBLOCK_PARTIAL_CHECK_ALLOCATED)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	return PartialMapUnmapMemBlock(guid, flags | PARTIAL_OP_FREE, va, size);
}

int sceKernelPartialMapMemBlock(SceUID guid, uintptr_t va, SceSize size, SceUInt32 flags)
{
	switch (flags & PARTIAL_CHECK_MASK) {
	case 0:
	case SCE_KERNEL_MEMBLOCK_PARTIAL_CHECK_ALLOCATED:
	case SCE_KERNEL_MEMBLOCK_PARTIAL_CHECK_ALLOCNOMAP:
		return PartialMapUnmapMemBlock(guid, flags | PARTIAL_OP_ALLOC_MAP, va, size);
	default:
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
}

int sceKernelPartialUnmapMemBlock(SceUID guid, uintptr_t va, SceSize size, SceUInt32 flags)
{
	if ((flags & PARTIAL_CHECK_MASK) != 0 &&
	    (flags & PARTIAL_CHECK_MASK) != SCE_KERNEL_MEMBLOCK_PARTIAL_CHECK_MAPPED)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	return PartialMapUnmapMemBlock(guid, flags | PARTIAL_OP_UNMAP, va, size);
}

int sceKernelPartialRemapMemBlock(SceUID guid, uintptr_t va, SceSize size, SceUInt32 op)
{
	switch (op) {
	case PARTIAL_OP_RW_TO_RO:
	case PARTIAL_OP_RW_TO_RO | SCE_KERNEL_MEMBLOCK_PARTIAL_CHECK_MAPPED_RW:
	case PARTIAL_OP_RO_TO_RW:
	case PARTIAL_OP_RO_TO_RW | SCE_KERNEL_MEMBLOCK_PARTIAL_CHECK_MAPPED_RO:
		return PartialMapUnmapMemBlock(guid, op, va, size);
	default:
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
}
#endif

/* the codes a block of one code may be remapped to; 0 ends a code's list */
typedef struct MemBlockRemapRule {
	SceUInt32 code;
	SceUInt32 newCode;
} MemBlockRemapRule;

static const MemBlockRemapRule s_remapKernel[] = {
	{ MEMBLOCK_CODE_KERNEL_ROOT_RW, MEMBLOCK_CODE_KERNEL_ROOT_RX },
	{ MEMBLOCK_CODE_KERNEL_ROOT_RW, MEMBLOCK_CODE_KERNEL_ROOT_R },
	{ MEMBLOCK_CODE_KERNEL_ROOT_RW, 0 },
	{ 0, 0 },
};

/* with SCE_DIPSW_ENABLE_TOOL_PHYMEMPART; the original has no end entry here */
static const MemBlockRemapRule s_remapTool[] = {
	{ MEMBLOCK_CODE_KERNEL_TOOL_RW, MEMBLOCK_CODE_KERNEL_TOOL_RX },
	{ MEMBLOCK_CODE_USER_SHARED_TOOL_RW, 0 },
};

static const MemBlockRemapRule s_remapCdram[] = {
	{ MEMBLOCK_CODE_USER_MAIN_RW, MEMBLOCK_CODE_USER_MAIN_RW },
	{ MEMBLOCK_CODE_USER_MAIN_RW, 0 },
	{ 0, 0 },
};

static inline int MemBlockRemapAllowed(const MemBlockRemapRule *rule, SceUInt32 code,
				       SceUInt32 newCode)
{
	for (; rule->code != 0; rule++) {
		if (rule->code != code)
			continue;
		if (rule->newCode == newCode)
			return 1;
		if (rule->newCode == 0)
			break;
	}
	return 0;
}

/* a block given another type, among the allowed changes */
int sceKernelRemapMemBlock(SceUID guid, SceKernelMemBlockType type)
{
	MemBlockObject *block;
	SceUInt32 code, currentCode, newCode;
	int intr, ret;

	ret = sceGUIDReferObjectWithClass(guid, SceSysmemForKernel_86681B64(),
					   (SceObjectBase **)&block);
	if (ret < 0)
		return ret;
	ret = MemBlockType2Code(type, &code);
	if (ret >= 0) {
		newCode = code;
		currentCode = block->code;
		if (MemBlockRemapAllowed(s_remapKernel, currentCode, newCode) ||
		    (sceKernelCheckDipsw(SCE_DIPSW_ENABLE_TOOL_PHYMEMPART) &&
		     MemBlockRemapAllowed(s_remapTool, currentCode, newCode)) ||
		    MemBlockRemapAllowed(s_remapCdram, currentCode, newCode)) {
			intr = SceCpuForDriver_4C38CE4D(&block->lock);
			ret = PartitionRemapMemBlock(block->partition, block, code, 0);
			sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&block->lock, intr);
		} else {
			ret = SCE_KERNEL_ERROR_ILLEGAL_MEMBLOCK_REMAP_TYPE;
		}
	}
	sceGUIDReleaseObject(guid);
	return ret;
}

int sceKernelUnmapMemBlock(SceUID guid)
{
	MemBlockObject *block;
	int intr, ret;

	ret = sceGUIDReferObjectWithClass(guid, SceSysmemForKernel_86681B64(),
					   (SceObjectBase **)&block);
	if (ret < 0)
		return ret;
	intr = SceCpuForDriver_4C38CE4D(&block->lock);
	ret = PartitionUnmapMemBlock(block->partition, block);
	sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&block->lock, intr);
	sceGUIDReleaseObject(guid);
	return ret;
}

/*
 * A block freed by its GUID once nobody else refers to it: its base info
 * dropped (or handed back), the clients mapping it unmapped first.
 */
#if defined(SYSMEM_TZS) || SYSMEM_NSKBL	/* the secure world's and the boot loader's callers
					   want neither the type nor the attributes */
int FreeMemBlockCore(SceUID guid, int checkLock, MemBlockObject **freed, MemBlockObject **base)
#else
int FreeMemBlockCore(SceUID guid, int checkLock, MemBlockObject **freed, MemBlockObject **base,
		     SceUInt32 *type, SceUInt32 *attr)
#endif
{
	MemBlockClientInfo *client;
	void *info = NULL;
	MemBlockObject *block;
	SceUInt32 i;
	int ret;

	do {
		ret = GUIDReferObjectWithClassExclusive(guid, SceSysmemForKernel_86681B64(),
							(SceObjectBase **)&block);
		if (ret == SCE_KERNEL_ERROR_UID_REFER_NOT_ZERO)
			SCE_DBG_PRINTF(0xb, 1386,
				       "sceGUIDReferObjectWithClassIfNoRefer(0x%08x) retuens UID_REFER_NOT_ZERO\n",
				       guid);
	} while (ret == SCE_KERNEL_ERROR_UID_REFER_NOT_ZERO);
	if (ret < 0)
		return ret;
	if (checkLock && MemBlockCheckLockStatusRaw(block)) {
		GUIDReleaseObject(guid);
		return SCE_KERNEL_ERROR_MEMBLOCK_LOCKED;
	}
#if !defined(SYSMEM_TZS) && !SYSMEM_NSKBL
	if (attr != NULL)
		*attr = block->attr;
	if (type != NULL)
		MemBlockCode2Type(block->code, type);
#endif
	if (base != NULL) {
		MemBlockOptGetBase(block, base, &info);
		if (info != NULL)
			MemBlockOptClearBaseInfoRaw(block);
	}
	if ((block->flags & MEMBLOCK_OPT_MASK) == MEMBLOCK_OPT_CLIENT_INFO &&
	    (client = (MemBlockClientInfo *)block->opt) != NULL) {
		if (client->guids != NULL && client->count != 0) {
			for (i = 0; i < client->count; i++) {
				SCE_PRINTF_WITH_INFO(0, 1432,
						     "GUID=0x%08x:Force sceKernelUnmapMemBlock(GUID:0x%08x)\n",
						     guid, client->guids[i]);
				sceKernelUnmapMemBlock(client->guids[i]);
			}
		}
		MemBlockOptClearClientInfoRaw(block);
		if (client->guids != NULL) {
			SceSysmemForKernel_4233C16D(client->guids);
			client->guids = NULL;
		}
		SceSysmemForKernel_4233C16D(client);
		MemBlockClearMapRef(block);
	}
	ret = PartitionFreeMemBlock(block->partition, block);
	if (ret < 0) {
		GUIDReleaseObject(guid);
		return ret;
	}
	GUIDReleaseObject(guid);
	ret = GUIDUnregisterChecked(guid);
	if (info != NULL)
		SceSysmemForKernel_4233C16D(info);
	if (freed != NULL)
		*freed = block;
	return ret;
}

#if defined(SYSMEM_TZS) || SYSMEM_NSKBL
/* the secure world's and the boot loader's FreeMemBlockCore take no type or attribute pointers */
#define FreeMemBlockCore(guid, checkLock, freed, base, type, attr) \
	FreeMemBlockCore(guid, checkLock, freed, base)
#endif

int sceKernelFreeMemBlock(SceUID guid)
{
	return FreeMemBlockCore(guid, 0, NULL, NULL, NULL, NULL);
}

/* the user allocation options: SceKernelAllocMemBlockOpt as the kernel reads it, up to 0x40 bytes */
typedef struct MemBlockUserOpt {
	SceSize size;			/* 0x14, 0x38, 0x3c or 0x40 */
	SceUInt32 attr;
	SceSize alignment;		/* 0x08 */
	SceUID uidBaseBlock;		/* 0x0c */
	const char *strBaseBlockName;	/* 0x10 */
	SceUInt32 client[9];		/* 0x14: to opt fields 0x34..0x54 */
	SceUInt32 unitSize;		/* 0x38: to opt.unitSize */
	SceUInt32 extAttr;		/* 0x3c: bit 0 DMAC clear, bit 1 never */
} MemBlockUserOpt;

/* the attr bits each option size accepts (the rest must be clear) */
#define USER_OPT_ATTR_INVALID_14	0xfedfbffb
#define USER_OPT_ATTR_INVALID_38	0xfedfbfdb
#define USER_OPT_ATTR_INVALID_40	0xfedf9fdb
#define USER_OPT_ATTR_BASE_NAME		0x00000200
#define USER_OPT_EXT_CLEAR		0x1
#define USER_OPT_EXT_NO_CLEAR		0x2

#define MEMBLOCK_ALLOC_USER_CLIENT	0x4000	/* AllocMemBlockForUser flags */

#if !SYSMEM_NSKBL
/* the titles that may still pass the attr bits a newer SDK rejects */
static inline int checkTitle(char *titleId)
{
	if (SceSysrootForKernel_EC3124A3(0, titleId, 0x20) < 0)
		return 0;
	SCE_DBG_PRINTF(0xb, 3377, "TitleId [%s]\n", titleId);
	return strncmp(titleId, "PCSC80009", 0x20) == 0 ||
	       strncmp(titleId, "PCSF00124", 0x20) == 0 ||
	       strncmp(titleId, "PCSA00044", 0x20) == 0 ||
	       strncmp(titleId, "PCSD00025", 0x20) == 0;
}

/* the codes a block may be mirrored from by name */
static inline int MemBlockCodeIsMirrorable(SceUInt32 code)
{
	switch (code) {
	case MEMBLOCK_CODE_USER_CDRAM_R:
	case MEMBLOCK_CODE_USER_CDRAM_RW:
	case MEMBLOCK_CODE_USER_MAIN_R:
	case MEMBLOCK_CODE_USER_MAIN_RW:
	case MEMBLOCK_CODE_USER_MAIN_NC_RW:
	case MEMBLOCK_CODE_USER_CDIALOG_R:
	case MEMBLOCK_CODE_USER_CDIALOG_RW:
	case MEMBLOCK_CODE_USER_CDIALOG_NC_R:
	case MEMBLOCK_CODE_USER_CDIALOG_NC_RW:
		return 1;
	default:
		return 0;
	}
}

/*
 * A user block: the user options checked and turned into kernel ones, the
 * block allocated, registered with the block it mirrors, cleared through the
 * DMAC when asked, and a PUID of the caller returned.
 */
SceUID AllocMemBlockForUser(const char *userName, SceUInt32 type, SceSize size, SceUInt32 flags,
			    const MemBlockUserOpt *userOpt)
{
	SceKernelAllocMemBlockKernelOpt kopt;
	MemBlockUserOpt uopt;
	MemBlockObject *base = NULL;
	char name[0x20], title[0x20];
	SceUInt32 attr, invalid, tpidruro;
	SceUID baseGuid, guid, puid;
	int hasBase, referred, ret;
	void *vbase;

	if (userName == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	ret = sceKernelStrncpyFromUser(name, userName, sizeof(name));
	if (ret < 0)
		return ret;
	name[sizeof(name) - 1] = '\0';
	memset(&kopt, 0, sizeof(kopt));
	kopt.size = sizeof(kopt);
	if (userOpt == NULL) {
		baseGuid = -1;
		referred = 0;
		goto check_size;
	}
	ret = sceKernelCopyFromUser(&uopt, userOpt, 0x14);
	if (ret < 0)
		return ret;
	switch (uopt.size) {
	case 0x14:
		invalid = USER_OPT_ATTR_INVALID_14;
		break;
	case 0x38:
		ret = sceKernelCopyFromUser(&uopt, userOpt, 0x38);
		if (ret < 0)
			return ret;
		uopt.extAttr = 0;
		invalid = USER_OPT_ATTR_INVALID_38;
		break;
	case 0x3c:
		ret = sceKernelCopyFromUser(&uopt, userOpt, sizeof(uopt));
		if (ret < 0)
			return ret;
		uopt.extAttr = 0;
		invalid = USER_OPT_ATTR_INVALID_40;
		break;
	case 0x40:
		ret = sceKernelCopyFromUser(&uopt, userOpt, sizeof(uopt));
		if (ret < 0)
			return ret;
		if (uopt.extAttr >> 2) {
			SCE_DBG_PRINTF(0xb, 3493, "Invalid extAttr: opt.extAttr=0x%08x\n",
				       uopt.extAttr);
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		}
		invalid = USER_OPT_ATTR_INVALID_40;
		break;
	default:
		SCE_DBG_PRINTF(6, 3504, "size mismatch 0x%08x!=0x%08x|0x%08x|0x%08x|0x%08x(expect)\n",
			       uopt.size, 0x14, 0x38, 0x3c, 0x40);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE;
	}
	attr = uopt.attr;
	if (invalid & attr) {
		if (!(attr & 0x240) || !checkTitle(title))
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		SCE_DBG_PRINTF(0xb, 3516, "Temporary ACCEPT: opt.attr=0x%08x\n", uopt.attr);
		attr = uopt.attr;
	}
	kopt.attr = attr;
	if (attr & SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ALIGNMENT) {
		if (uopt.alignment & (uopt.alignment - 1))
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		if (uopt.alignment < 0x1000) {
			SCE_DBG_PRINTF(0, 3537, "Warning:ignore smaller alignment 0x%08x\n",
				       uopt.alignment);
			kopt.attr &= ~SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ALIGNMENT;
			attr = kopt.attr;
		} else {
			kopt.alignment = uopt.alignment;
		}
	}
	if (attr & SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_MIRROR_BLOCKID) {
		baseGuid = PUID2GUIDWithRefer(0, uopt.uidBaseBlock, SceSysmemForKernel_86681B64());
		if (baseGuid < 0)
			return baseGuid | 0x40000000;
		kopt.mirror_blockid = baseGuid;
		kopt.attr |= SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_MIRROR_BLOCKID;
		attr = kopt.attr;
		hasBase = 1;
	} else {
		hasBase = 0;
		baseGuid = -1;
	}
	referred = attr & USER_OPT_ATTR_BASE_NAME;
	if (referred) {
		ret = sceKernelStrncpyFromUser(title, uopt.strBaseBlockName, sizeof(title));
		if (ret < 0)
			return ret;
		title[sizeof(title) - 1] = '\0';
		baseGuid = GUIDFindByName(title);
		if (baseGuid < 0)
			return baseGuid;
		ret = sceGUIDReferObjectWithClass(baseGuid, SceSysmemForKernel_86681B64(),
						   (SceObjectBase **)&base);
		if (ret < 0)
			return ret;
		if (!MemBlockCodeIsMirrorable(base->code)) {
			sceGUIDReleaseObject(baseGuid);
			return SCE_KERNEL_ERROR_ILLEGAL_MEMBLOCK_TYPE;
		}
		referred = 1;
		kopt.mirror_blockid = baseGuid;
		kopt.attr = (kopt.attr & ~USER_OPT_ATTR_BASE_NAME) |
			    SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_MIRROR_BLOCKID;
		hasBase = 1;
	}
	if (uopt.size >= 0x38) {
		if (kopt.attr & ALLOC_ATTR_HAS_CLIENT_INFO) {
			kopt.openFlags = uopt.client[0];
			kopt.capabilities[0] = uopt.client[1];
			kopt.capabilities[1] = uopt.client[2];
			kopt.capabilities[2] = uopt.client[3];
			kopt.capabilities[3] = uopt.client[4];
			kopt.capabilities[4] = uopt.client[5];
			kopt.capabilities[5] = uopt.client[6];
			kopt.capabilities[6] = uopt.client[7];
			kopt.capabilities[7] = uopt.client[8];
		}
		if (uopt.size >= 0x40 && (kopt.attr & ALLOC_ATTR_HAS_UNIT_SIZE))
			kopt.unitSize = uopt.unitSize;
	}
	if (!hasBase)
		goto check_size;
	/* a mirror takes its size from the base block */
	if (uopt.extAttr & (USER_OPT_EXT_CLEAR | USER_OPT_EXT_NO_CLEAR))
		goto ext_attr;
	goto type_clear;
check_size:
	if (size < 0x1000 || (size & 0xfff))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	hasBase = 0;
	if (!(uopt.extAttr & (USER_OPT_EXT_CLEAR | USER_OPT_EXT_NO_CLEAR)))
		goto type_clear;
ext_attr:
	if ((kopt.attr & SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_MIRROR_BLOCKID) ||
	    !(uopt.extAttr & USER_OPT_EXT_CLEAR))
		kopt.attr |= ALLOC_ATTR_KEEP_CLEAR;
	else
		kopt.attr |= ALLOC_ATTR_NO_CLEAR;
	if (uopt.extAttr & USER_OPT_EXT_NO_CLEAR)
		kopt.attr = (kopt.attr & ~ALLOC_ATTR_NO_CLEAR) | ALLOC_ATTR_KEEP_CLEAR;
	goto alloc;
type_clear:
	/* big blocks of these types are cleared by the DMAC for debuggable processes */
	if ((type == SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_R ||
	     type == SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW ||
	     type == SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_L1WBWA_RW) &&
	    !(kopt.attr & SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_MIRROR_BLOCKID) && size >= 0x40000 &&
	    CheckProcessDebugCapability(0))
		kopt.attr |= ALLOC_ATTR_NO_CLEAR;
	else
		kopt.attr |= ALLOC_ATTR_KEEP_CLEAR;
alloc:
	if (flags & MEMBLOCK_ALLOC_USER_CLIENT)
		kopt.attr |= ALLOC_ATTR_USER_CLIENT;
	guid = sceKernelAllocMemBlockWithInfoForKernel(name, type, size, &kopt, NULL);
	if (guid < 0) {
		SCE_DBG_PRINTF(0xb, 3663, "sceKernelAllocMemBlockForKernel failed 0x%08x\n", guid);
		if (hasBase == 1 && referred == 1)
			sceGUIDReleaseObject(baseGuid);
		return guid;
	}
	if (hasBase == 1) {
		MemBlockObject *current = base;

		if (current == NULL) {
			if (sceGUIDReferObjectWithClass(baseGuid, SceSysmemForKernel_86681B64(),
							 (SceObjectBase **)&base) < 0)
				goto clear;
			referred = hasBase;
			current = base;
		}
		attr = current->flags & MEMBLOCK_OPT_MASK;
		if (attr == 0) {
			if (MemBlockOptSetClientInfoRaw(current, NULL) < 0)
				goto release;
			attr = current->flags & MEMBLOCK_OPT_MASK;
		}
		if (attr == MEMBLOCK_OPT_CLIENT_INFO)
			MemBlockClientInfoAddGUID((MemBlockClientInfo *)current->opt, guid);
release:
		if (referred == 1)
			sceGUIDReleaseObject(baseGuid);
	}
clear:
	if (kopt.attr & ALLOC_ATTR_NO_CLEAR) {
		tpidruro = readTpidruro();
		writeTpidruro(tpidruro << 16);
		sceKernelGetMemBlockVBase(guid, &vbase);
		sceKernelDmacMemset(vbase, 0, size);
		writeTpidruro(tpidruro);
	}
	puid = scePUIDOpenByGUID(0, guid);
	if (puid < 0) {
		sceKernelFreeMemBlock(guid);
		return puid;
	}
	return puid;
}

SceUID sceKernelFindProcMemBlockByAddr(SceUID pid, const void *addr, SceSize size)
{
	void *as;
	int ret;

	ret = SysrootPIDtoAddressSpaceCB(pid, &as);
	if (ret < 0)
		return ret;
	return AddressSpaceFindMemBlockCB(as, (uintptr_t)addr, size, NULL);
}

SceUID sceKernelFindMemBlockByAddr(void *addr, SceSize size)
{
	return AddressSpaceFindMemBlockCB(sceKernelSysrootGetCurrentAddressSpaceCB(),
					  (uintptr_t)addr, size, NULL);
}
#endif

/* told about lock reference changes (debug builds; SceSysmemForKernel_F57A97A0) */
typedef struct MemBlockDebugCallbacks {
	void (*incRef)(int unused, MemBlockObject *block, SceUInt16 count,
		       void *returnAddress);
	void (*decRef)(int unused, MemBlockObject *block, SceUInt16 count,
		       void *returnAddress);
} MemBlockDebugCallbacks;

#if FW_DEBUG
/* not static: the debug dump reads it */
static MemBlockDebugCallbacks *s_memBlockDebugCallbacks;
#endif

#if !SYSMEM_NSKBL
int sceKernelIncRefCountMemBlock(SceUID guid)
{
	MemBlockObject *block;
	int ret;

	ret = sceGUIDReferObjectWithClass(guid, SceSysmemForKernel_86681B64(),
					   (SceObjectBase **)&block);
	if (ret < 0)
		return ret;
	MemBlockIncLockRef(block);
#if FW_DEBUG
	{
		MemBlockObject *current = block;
		SceUInt16 count = MemBlockGetLockRef(current);

		if (s_memBlockDebugCallbacks != NULL)
			s_memBlockDebugCallbacks->incRef(0, current, count,
						__builtin_return_address(0));
	}
#endif
	sceGUIDReleaseObject(guid);
	return 0;
}

int sceKernelDecRefCountMemBlock(SceUID guid)
{
	MemBlockObject *block;
	int ret;

	ret = sceGUIDReferObjectWithClass(guid, SceSysmemForKernel_86681B64(),
					   (SceObjectBase **)&block);
	if (ret < 0)
		return ret;
	ret = MemBlockDecLockRef(block);
#if FW_DEBUG
	{
		MemBlockObject *current = block;
		SceUInt16 count = MemBlockGetLockRef(current);

		if (s_memBlockDebugCallbacks != NULL)
			s_memBlockDebugCallbacks->decRef(0, current, count,
						__builtin_return_address(0));
	}
#endif
	sceGUIDReleaseObject(guid);
	return ret;
}

int sceKernelGetMemBlockPARange(SceUID guid, SceKernelPARange *range)
{
	MemBlockObject *block;
	int intr, ret;

	if (range == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = sceGUIDReferObjectWithClass(guid, SceSysmemForKernel_86681B64(),
					   (SceObjectBase **)&block);
	if (ret < 0)
		return ret;
	intr = SceCpuForDriver_EC53D007(&block->lock);
	ret = PartitionGetMemBlockPARange(block->partition, block, range);
	sceKernelRWSpinlockLowReadUnlockCpuResumeIntr(&block->lock, intr);
	sceGUIDReleaseObject(guid);
	return ret;
}

int sceKernelGetMemBlockPAVector(SceUID guid, SceKernelPAVector *vector)
{
	MemBlockObject *block;
	int intr, ret;

	if (vector == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (vector->size != sizeof(*vector))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE;
	if (vector->capacity != 0 && vector->pRanges == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = sceGUIDReferObjectWithClass(guid, SceSysmemForKernel_86681B64(),
					   (SceObjectBase **)&block);
	if (ret < 0)
		return ret;
	intr = SceCpuForDriver_EC53D007(&block->lock);
	ret = PartitionGetMemBlockPAVector(block->partition, block, vector);
	sceKernelRWSpinlockLowReadUnlockCpuResumeIntr(&block->lock, intr);
	sceGUIDReleaseObject(guid);
	return ret;
}

/* name unknown: the process of the address space the block is in */
SceUID sceKernelGetMemBlockProcess(SceUID guid)
{
	MemBlockObject *block;
	SceUID pid;
	int ret;

	ret = sceGUIDReferObjectWithClass(guid, SceSysmemForKernel_86681B64(),
					   (SceObjectBase **)&block);
	if (ret < 0)
		return ret;
	pid = GetMemBlockPartition(block)->addressSpace->pid;
	sceGUIDReleaseObject(guid);
	return pid;
}

int sceKernelGetMemBlockMemtypeByAddr(const void *addr)
{
	MemBlockObject *block;
	int ret;

	ret = AddressSpaceFindMemBlockCtlByAddr(sceKernelSysrootGetCurrentAddressSpaceCB(),
						(uintptr_t)addr, 0, &block);
	if (ret < 0)
		return ret;
	return MEMBLOCK_CODE_MEMORY_TYPE(MemBlockGetCode(block));
}

/* kernel addresses below 0x60000000 need no lock */
int sceKernelLockRange(void *addr, SceSize size)
{
	uintptr_t va = (uintptr_t)addr;

	if (va >= 0x40000000 && va - 0x40000000 + size <= 0x20000000)
		return 1;
	return ASLockRangeWithMode(sceKernelSysrootGetCurrentAddressSpaceCB(), 0, va, size);
}

int sceKernelUnlockRange(void *addr, SceSize size)
{
	return ASUnlockRangeWithMode(sceKernelSysrootGetCurrentAddressSpaceCB(), 0, (uintptr_t)addr,
				     size);
}
#endif

/* what a user map block records (opt state 3) */
typedef struct UserMapInfo {
	SceUID pid;
	uintptr_t va;
	SceSize size;
	SceUInt32 noLock;		/* the range needed no lock (kernel memory) */
} UserMapInfo;

#pragma push_macro("__FILE_NAME__")
#undef __FILE_NAME__
#define __FILE_NAME__ "memblock_obj_module.h"

#if !SYSMEM_NSKBL
static inline void MemBlockOptSetUserMapInfoRaw(MemBlockObject *block, UserMapInfo *info)
{
	SCE_DBG_ASSERT_CHECK((block->flags & MEMBLOCK_OPT_MASK) == 0, 1005);
	block->opt = (SceUInt32)info;
	block->flags = (block->flags & ~MEMBLOCK_OPT_MASK) | MEMBLOCK_OPT_USER_MAP;
}
#endif

#pragma pop_macro("__FILE_NAME__")

#define USER_MAP_FLAGS_VALID		0x11
#define USER_MAP_SHARED			0x11	/* PLACEHOLDER: name */
#define USER_MAP_VECTOR			4	/* ranges kept on the stack */

#if !SYSMEM_NSKBL
/* the memory types the pages are already mapped with */
static inline int UserMapIsDeviceMemory(int memoryType)
{
	switch (memoryType) {
	case 2:
	case 8:
	case 0x40:
	case 0x80:
		return 1;
	default:
		return 0;
	}
}

/* the block type for a map of pages with this memory type */
static inline SceUInt32 UserMapType(SceUInt32 type, SceUInt32 flags, int memoryType)
{
	if (type == SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TMP_R) {
		if (flags == USER_MAP_SHARED)
			return UserMapIsDeviceMemory(memoryType) ?
			       SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TMP_FS_GAME_NC_R :
			       SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TMP_FS_GAME_R;
		return UserMapIsDeviceMemory(memoryType) ? SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TMP_NC_R : type;
	}
	if (type == SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TMP_RW) {
		if (flags == USER_MAP_SHARED)
			return UserMapIsDeviceMemory(memoryType) ?
			       SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TMP_FS_GAME_NC_RW :
			       SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TMP_FS_GAME_RW;
		return UserMapIsDeviceMemory(memoryType) ? SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TMP_NC_RW : type;
	}
	return type;
}

/*
 * The pages of [va, va + size) of pid (0: the caller) mapped again into a
 * new block of the kernel. *base, *mapSize and *offset say where va landed.
 */
SceUID UserMap(SceUID pid, const char *name, SceUInt32 perm, SceUInt32 flags, uintptr_t va,
	       SceSize size, void **base, SceSize *mapSize, SceSize *offset)
{
	SceKernelAllocMemBlockKernelOpt opt;
	SceKernelPARange range, ranges[USER_MAP_VECTOR], *first, *last;
	SceKernelPAVector vector;
	MemBlockObject *block;
	UserMapInfo *info;
	SceUInt32 type, mode, head, extra, total, startPage, endPage;
	SceSize len;
	int locked, memoryType, allocated, ret;
	SceUID guid;
	void *vbase;

	if ((flags & ~USER_MAP_FLAGS_VALID) || va + 0x80000000 < 0x1000000 || base == NULL ||
	    mapSize == NULL || offset == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (pid == 0) {
		locked = sceKernelLockRange((void *)va, size);
		if (locked < 0)
			return SCE_KERNEL_ERROR_ERROR;
	} else {
		locked = LockRangeForPID(pid, va, size);
		if (locked < 0) {
			SCE_DBG_PRINTF_LEVEL(2, 0xb, 2267,
					     "sceKernelLockRangeProc(pid 0x%08x) failed 0x%08x\n", pid,
					     locked);
			return SCE_KERNEL_ERROR_ERROR;
		}
	}
	if (perm == 1) {
		type = SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TMP_R;
		mode = 0x10;
	} else if (perm == 0 || perm > 3) {
		ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		goto unlock;
	} else {
		type = SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TMP_RW;
		mode = 0x20;
	}
	startPage = va >> 12;
	endPage = (size + 0xfff + va) >> 12;
	range.addr = startPage << 12;
	len = (endPage << 12) - range.addr;
	range.size = len;
	vector.size = sizeof(vector);
	vector.capacity = USER_MAP_VECTOR;
	vector.nDataInVector = 0;
	vector.total = 0;
	vector.pRanges = ranges;
	ret = VARangeToPAVectorForPID(pid, mode, &range, &vector, &memoryType);
	if (ret < 0)
		goto unlock;
	allocated = 0;
	if (vector.total > USER_MAP_VECTOR) {
		vector.pRanges = SceSysmemForKernel_85571907(vector.total * sizeof(SceKernelPARange));
		if (vector.pRanges == NULL) {
			ret = SCE_KERNEL_ERROR_HEAPLIB_NOMEM;
			goto unlock;
		}
		vector.size = sizeof(vector);
		vector.capacity = vector.total;
		vector.nDataInVector = 0;
		vector.total = 0;
		ret = VARangeToPAVectorForPID(pid, mode, &range, &vector, &memoryType);
		if (ret < 0) {
			info = NULL;
			goto free_vector;
		}
		allocated = 1;
	}
	total = vector.total;
	type = UserMapType(type, flags, memoryType);
	/* the ends grown to whole 64 KiB / 1 MiB pages where the run is that large */
	head = va & 0xfff;
	first = &vector.pRanges[0];
	if (first->size > 0x80000) {
		extra = first->addr & 0xfffff;
		first->size += extra;
		head += extra;
		first->addr &= ~0xfffff;
		len += extra;
	} else if (first->size > 0x8000) {
		extra = first->addr & 0xffff;
		head += extra;
		len += extra;
		first->addr &= ~0xffff;
		first->size += extra;
	}
	last = &first[total - 1];
	if (last->size < 0x100000) {
		if (last->size > 0x80000) {
			if ((last->addr & 0xfffff) == 0) {
				extra = 0x100000 - ((last->addr + last->size) & 0xfffff);
				len += extra;
				last->size += extra;
			}
		} else if (last->size > 0x8000 && (last->addr & 0xffff) == 0) {
			extra = 0x10000 - ((last->addr + last->size) & 0xffff);
			len += extra;
			last->size += extra;
		}
	}
	info = NULL;
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.paddr_list = &vector;
	opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR_LIST | ALLOC_ATTR_NO_CLEAR;
	guid = sceKernelAllocMemBlockWithInfoForKernel(name, type, len, &opt, NULL);
	ret = guid;
	if (guid >= 0) {
		block = NULL;
		ret = sceGUIDReferObjectWithClass(guid, SceSysmemForKernel_86681B64(),
						   (SceObjectBase **)&block);
		if (ret >= 0) {
			info = SceSysmemForKernel_85571907(sizeof(*info));
			if (info != NULL) {
				info->pid = pid != 0 ? pid : SceSysrootForKernel_7385CADE();
				info->va = va;
				info->size = size;
				info->noLock = locked == 1;
				MemBlockOptSetUserMapInfoRaw(block, info);
				sceGUIDReleaseObject(guid);
				ret = sceKernelGetMemBlockVBase(guid, &vbase);
				if (ret < 0) {
					sceKernelFreeMemBlock(guid);
				} else {
					*base = vbase;
					*mapSize = len;
					*offset = head;
					ret = guid;
				}
				goto done;
			}
			/* the original releases the error code, not the GUID */
			sceGUIDReleaseObject(SCE_KERNEL_ERROR_HEAPLIB_NOMEM);
			ret = SCE_KERNEL_ERROR_HEAPLIB_NOMEM;
		}
		info = NULL;
	}
done:
	if (allocated) {
free_vector:
		if (vector.pRanges != NULL)
			SceSysmemForKernel_4233C16D(vector.pRanges);
	}
	if (ret >= 0)
		return ret;
	if (info != NULL)
		SceSysmemForKernel_4233C16D(info);
unlock:
	if (pid == 0)
		sceKernelUnlockRange((void *)va, size);
	return ret;
}

int sceKernelLockRangeProc(SceUID pid, void *addr, SceSize size)
{
	return LockRangeForPID(pid, (uintptr_t)addr, size);
}

int sceKernelUnlockRangeProc(SceUID pid, void *addr, SceSize size)
{
	return UnlockRangeForPID(pid, (uintptr_t)addr, size);
}

int sceKernelLockRangeWithMode(SceKernelMemoryRefPerm mode, void *addr, SceSize size)
{
	int ret;

	ret = ASLockRangeWithMode(sceKernelSysrootGetCurrentAddressSpaceCB(), mode,
				  (uintptr_t)addr, size);
	if (ret < 0)
		SCE_DBG_PRINTF_FUNC(0xb, 2037, "sceKernelLockRangeWithMode",
				    "LockRangeWithMode(0x%08x, 0x%08x) failed 0x%08x\n", addr,
				    size, ret);
	return ret;
}

int sceKernelUnlockRangeWithMode(SceKernelMemoryRefPerm mode, void *addr, SceSize size)
{
	return ASUnlockRangeWithMode(sceKernelSysrootGetCurrentAddressSpaceCB(), mode,
				     (uintptr_t)addr, size);
}

SceUID sceKernelUserMap(const char *name, int permission, const void *user_buf, SceSize size,
			 void **kernel_page, SceSize *kernel_size, SceUInt32 *kernel_offset)
{
	return UserMap(0, name, permission, 0, (uintptr_t)user_buf, size, kernel_page, kernel_size,
		       kernel_offset);
}

SceUID SceSysmemForDriver_7D4F8B5F(const char *name, int permission, int flags,
				  const void *user_buf, SceSize size, void **kernel_page,
				  SceSize *kernel_size, unsigned int *kernel_offset)
{
	return UserMap(0, name, permission, flags, (uintptr_t)user_buf, size, kernel_page,
		       kernel_size, kernel_offset);
}

SceUID SceSysmemForDriver_0091D74D(SceUID pid, const char *name, int permission, const void *user_buf,
			     SceSize size, void **kernel_page, SceSize *kernel_size,
			     SceUInt32 *kernel_offset)
{
	return UserMap(pid, name, permission, 0, (uintptr_t)user_buf, size, kernel_page,
		       kernel_size, kernel_offset);
}

/* a user map freed and the user range unlocked again; the range is the caller's note only */
int sceKernelUserUnmap(SceUID guid, const void *userBuffer, SceSize userSize)
{
	MemBlockObject *block;
	UserMapInfo *info;
	uintptr_t va;
	SceSize size;
	SceUInt32 noLock;
	int intr, ret;

	ret = sceGUIDReferObjectWithClass(guid, SceSysmemForKernel_86681B64(),
					   (SceObjectBase **)&block);
	if (ret < 0)
		return ret;
	intr = SceCpuForDriver_EC53D007(&block->lock);
	if ((block->flags & MEMBLOCK_OPT_MASK) == MEMBLOCK_OPT_USER_MAP &&
	    (info = MemBlockOptGetUserMapInfoRaw(block)) != NULL) {
		va = info->va;
		size = info->size;
		noLock = info->noLock;
		sceKernelRWSpinlockLowReadUnlockCpuResumeIntr(&block->lock, intr);
		sceGUIDReleaseObject(guid);
		ret = FreeMemBlockCore(guid, 0, NULL, NULL, NULL, NULL);
		if (ret < 0)
			goto fail;
		if (noLock != 1) {
			if (info->pid == SceSysrootForKernel_7385CADE()) {
				ret = sceKernelUnlockRange((void *)va, size);
				if (ret < 0) {
					SCE_DBG_PRINTF_FUNC(0xb, 2637, "sceKernelUserUnmap",
						       "sceKernelUnlockRange(0x%08x, 0x%08x) failed 0x%08x\n",
						       va, size, ret);
					ret = SCE_KERNEL_ERROR_ERROR;
				}
			} else {
				ret = UnlockRangeForPID(info->pid, va, size);
				if (ret < 0) {
					SCE_DBG_PRINTF_FUNC(0xb, 2630, "sceKernelUserUnmap",
						       "sceKernelUnlockRangeProc(0x%08x, 0x%08x, 0x%08x) failed 0x%08x\n",
						       info->pid, va, size, ret);
					ret = SCE_KERNEL_ERROR_ERROR;
				}
			}
		}
		SceSysmemForKernel_4233C16D(info);
		return ret;
	}
	sceKernelRWSpinlockLowReadUnlockCpuResumeIntr(&block->lock, intr);
	sceGUIDReleaseObject(guid);
	ret = FreeMemBlockCore(guid, 0, NULL, NULL, NULL, NULL);
	if (ret >= 0)
		return ret;
fail:
	SCE_DBG_PRINTF_FUNC(0xb, 2620, "sceKernelUserUnmap",
			    "sceKernelFreeMemBlock(0x%08x) failed 0x%08x\n", guid, ret);
	return ret;
}

/* name unknown: the physical range of a contiguous block */
int SceSysmemForDriver_64DBE472(SceUID guid, uintptr_t range[2])
{
	MemBlockObject *block;
	int ret;

	ret = sceGUIDReferObjectWithClass(guid, SceSysmemForKernel_86681B64(),
					   (SceObjectBase **)&block);
	if (ret < 0)
		return ret;
	ret = MemBlockGetPhyRange(block, range);
	sceGUIDReleaseObject(guid);
	return ret;
}

/* the physical ranges of a block, adjacent pieces merged */
int sceKernelMemBlockToPAVector(SceUID guid, SceKernelPAVector *vector)
{
	SceKernelPARange *cur, *end;
	MemBlockObject *block;
	SceUInt32 count;
	int ret;

	if (vector == NULL) {
		SCE_DBG_ASSERT_FAIL(2681);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	if (vector->size != sizeof(*vector)) {
		SCE_DBG_ASSERT_FAIL(2686);
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE;
	}
	if (vector->capacity == 0) {
		cur = NULL;
		end = NULL;
	} else {
		cur = vector->pRanges;
		if (cur == NULL)
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		end = cur + vector->capacity;
	}
	vector->nDataInVector = 0;
	vector->total = 0;
	ret = sceGUIDReferObjectWithClass(guid, SceSysmemForKernel_86681B64(),
					   (SceObjectBase **)&block);
	if (ret < 0)
		return ret;
	MemBlockGetPhyRangeList(block, vector, cur, end);
	count = vector->total;
	count = count < vector->capacity ? count : vector->capacity;
	vector->nDataInVector = count;
	sceGUIDReleaseObject(guid);
	return 0;
}

/* a block's info as TinyPartitionGetMemBlockInfoList lists it */
int MemBlockGetInfoRaw(MemBlockObject *block, SceInt32 level, MemBlockInfoEx *info)
{
	MemBlockGetInfoDetails(block, level, &info->details);
	return 0;
}

/* a block's info with its physical ranges */
int MemBlockGetInfo(MemBlockObject *block, SceInt32 level, MemBlockPhyInfo *info)
{
	SceUInt32 page, code;
	SceSize size;

	MemBlockGetInfoDetails(block, level, &info->ex.details);
	code = block->code;
	info->count = 0;
	info->rangeCount = 0;
	if (code == 0 || !(block->attr & MEMBLOCK_ATTR_WHOLE)) {
		if ((block->flags & MEMBLOCK_KIND_MASK) == MEMBLOCK_KIND_VPL) {
			MemBlockVirPageListGetInfo(block, info);
			return 0;
		}
	} else if (block->smbPage != NULL) {
		page = *(const SceUInt32 *)block->smbPage;
		if ((page & 0x30000000) == 0x30000000) {
			info->pbase[0] = page << 12;
			size = block->smbSize;
			info->count = 1;
			info->rangeCount = 1;
			info->size[0] = size;
		}
	}
	return 0;
}

/* name unknown: the info of a block by GUID or PUID (of pid) */
int GetMemBlockInfoForPID(SceUID pid, SceUID uid, SceInt32 level, MemBlockPhyInfo *info)
{
	MemBlockObject *block, *current;
	SceClass *cls;
	int ret;

	if (pid == -1)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (info == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (info->ex.size != sizeof(*info))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE;
	ret = UIDReferObjectWithLevelForPID(pid, uid, level, (SceObjectBase **)&block);
	if (ret < 0)
		return ret;
	current = block;
	cls = SceSysmemForKernel_86681B64();
	if (current->cls != cls)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = MemBlockGetInfo(current, level, info);
	UIDReleaseObjectForPID(pid, uid);
	return ret;
}

/* whether all of a block's pages lie in [pa, pa + size) */
int MemBlockWithinPhyRange(MemBlockObject *block, uintptr_t pa, SceSize size)
{
	SceUInt32 page;
	uintptr_t pbase;

	if (block->code == 0 || !(block->attr & MEMBLOCK_ATTR_WHOLE)) {
		if ((block->flags & MEMBLOCK_KIND_MASK) == MEMBLOCK_KIND_VPL)
			return MemBlockVirPageListCheckWithinPhyRange(block, pa, size);
		return 1;
	}
	if (block->smbPage == NULL)
		return 0;
	page = *(const SceUInt32 *)block->smbPage;
	if ((page & 0x30000000) != 0x30000000) {
		SCE_DBG_ASSERT_FAIL(2948);
		return 0;
	}
	pbase = page << 12;
	if (pa > pbase)
		return 0;
	return PhyPageGetSizeRaw(block->smbPage) + pbase <= size + pa;
}

/* sceKernelAddressSpaceGetMemBlockInfoByAddr: the info of pid's block holding va */
int SceSysmemForKernel_C3F9607E(SceUID pid, uintptr_t va, SceInt32 level, MemBlockPhyInfo *info)
{
	void *as;
	int ret;

	if (info == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (info->ex.size != sizeof(*info))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE;
	ret = SysrootPIDtoAddressSpaceCB(pid, &as);
	if (ret < 0)
		return ret;
	return AddressSpaceGetMemBlockInfoByAddr(as, va, level, info);
}

/* the PUIDs' owner when uid is one */
static inline SceUID MemBlockUIDOwner(SceUID uid)
{
	return (uid & 0x40000000) ? SceSysrootForKernel_340575CB() : 0;
}

/* sceKernelGetMemBlockInfoSimple: the details without the ranges */
int sceKernelGetMemBlockInfoSimple(SceUID uid, MemBlockInfoEx *info)
{
	MemBlockObject *block;
	SceUID pid = MemBlockUIDOwner(uid);
	int ret;

	if (info == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (info->size != sizeof(*info))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE;
	ret = UIDReferObjectWithLevelForPID(pid, uid, 7, (SceObjectBase **)&block);
	if (ret < 0)
		return ret;
	MemBlockGetInfoDetails(block, 7, &info->details);
	UIDReleaseObjectForPID(pid, uid);
	return 0;
}

int SceSysmemForDriver_24A99FFF(SceUID uid, MemBlockPhyInfo *info)
{
	return GetMemBlockInfoForPID(MemBlockUIDOwner(uid), uid, 7, info);
}

int sceKernelGetMemBlockInfo(SceUID uid, SceInt32 level, MemBlockPhyInfo *info)
{
	return GetMemBlockInfoForPID(MemBlockUIDOwner(uid), uid, level, info);
}

/* name unknown: the info of a partition's blocks based in [lo, hi) */
int SceSysmemForKernel_AD19285A(TinyPartition *tiny, uintptr_t lo, uintptr_t hi, SceUInt32 level,
				MemBlockInfoEx *info, SceSize infoSize, SceUInt32 *count)
{
	if (tiny == NULL)
		goto invalid;
	if (count == NULL)
		goto invalid;
	return TinyPartitionGetMemBlockInfoListByVA(tiny, lo, hi, level, info, infoSize, count);
invalid:
	return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
}

/* sceKernelGetMemBlockInfoAll: ... also backed in [pa, pa + psize) */
int GetMemBlockInfoAll(TinyPartition *tiny, uintptr_t lo, uintptr_t hi, uintptr_t pa,
		       SceSize psize, SceUInt32 level, MemBlockInfoEx *info, SceSize infoSize,
		       SceUInt32 *count)
{
	SCE_DBG_PRINTF(0xb, 3258, "(pPart %p, level %d, pInfoBuffer %p)\n", tiny, level, info);
	if (tiny == NULL)
		goto invalid;
	if (count == NULL)
		goto invalid;
	return TinyPartitionGetMemBlockInfoListByPA(tiny, lo, hi, pa, psize, level, info, infoSize,
						    count);
invalid:
	return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
}
#endif

#define MEMBLOCK_ERROR_OVERFLOW_FATAL	0xc0024b0b

#define VM_SIZE_MAX			0x1000000

#if !SYSMEM_NSKBL
/* the program authority IDs denied SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_GAME_RW */
static inline int MemBlockAuthDenied(SceUInt32 authLow, SceUInt32 authHigh)
{
	return (authHigh == 0x28008000 && (authLow & ~0xf) == 0) ||
	       (authHigh == 0x28000000 && authLow == 0x15);
}

SceUID sceKernelAllocMemBlockForUser(const char *name, SceKernelMemBlockType type, SceSize size,
			      const SceKernelAllocMemBlockOpt *opt)
{
	SceSelfAuthInfo auth;
	SceUInt32 attr, restype;
	SceUID ret;

	sceKernelSysrootGetCurrentThreadAttr(&attr);
	if (type == SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_CDIALOG_RW ||
	    type == SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_CDIALOG_NC_RW)
		restype = 2;
	else
		restype = (attr >> 14) & 1;
	if (SysrootIncProcessResource(restype) < 0) {
		SCE_PRINTF_WITH_INFO_FUNC(0xb, 3836, "sceKernelAllocMemBlockForUser",
					  "MemBlock overflow: restype=0x%08x\n", restype);
		SCE_PRINTF_WITH_INFO_FUNC(0xb, 3837, "sceKernelAllocMemBlockForUser",
					  "name [%s], type=0x%08x, vsize=0x%08x\n", name, type, size);
		return MEMBLOCK_ERROR_OVERFLOW_FATAL;
	}
	switch (type) {
	case SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_GAME_RW:
		ret = SceSysrootForKernel_4F0A4066(0, &auth);
		if (ret >= 0 && MemBlockAuthDenied((SceUInt32)auth.programAuthorityId,
						(SceUInt32)(auth.programAuthorityId >> 32))) {
			ret = SCE_KERNEL_ERROR_MEMBLOCK_TYPE_FOR_UPDATER_OR_SAFEMODE;
			break;
		}
		/* fall through */
	case SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_L1WBWA_RW:
	case SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_R:
	case SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW:
	case SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_DEVICE_RW:
	case SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_NC_RW:
	case SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_R:
	case SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_RW:
	case SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_RW:
	case SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_CDIALOG_NC_RW:
	case SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_CDIALOG_RW:
	case SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_TOOL_NC_RW:
	case SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_TOOL_RW:
	case SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW:
	case SCE_KERNEL_MEMBLOCK_TYPE_CDIALOG_NC_R:
	case 0x0e208050:
	case SCE_KERNEL_MEMBLOCK_TYPE_USER_CDIALOG_NC_RW:
	case SCE_KERNEL_MEMBLOCK_TYPE_USER_CDIALOG_R:
	case SCE_KERNEL_MEMBLOCK_TYPE_USER_CDIALOG_RW:
		if (size & 0xfff) {
			ret = SCE_KERNEL_ERROR_ILLEGAL_MEMBLOCK_SIZE;
			break;
		}
		ret = AllocMemBlockForUser(name, type, size, attr, (const MemBlockUserOpt *)opt);
		if (ret >= 0)
			return ret;
		SCE_DBG_PRINTF_FUNC(0xb, 3854, "sceKernelAllocMemBlockForUser",
				    "AllocMemBlockForUser failed 0x%08x\n", ret);
		break;
	default:
		ret = SCE_KERNEL_ERROR_ILLEGAL_BLOCK_TYPE;
		break;
	}
	SysrootDecProcessResource(restype);
	return ret;
}
#endif

#define DOMAIN_VM			2

#if !SYSMEM_NSKBL
/* a block of the VM domain, listed in the process's VM blocks */
SceUID sceKernelAllocMemBlockForVMForUser(const char *userName, SceSize size)
{
	SceKernelAllocMemBlockKernelOpt opt;
	SysmemProcessStorage *storage;
	SysmemVMBlock *entry, *end;
	SceUInt32 attr, restype;
	MemBlockObject *block = NULL;
	SceUID guid, puid;
	char name[0x20];
	int intr, rintr, ret;

	if (!SceSysrootForKernel_F8769E86(0))
		return SCE_ERROR_ERRNO_ENOSYS;
	ret = sceKernelCheckOpenVMDomain();
	if (ret >= 1) {
		SCE_DBG_PRINTF_FUNC(0xb, 3893, "sceKernelAllocMemBlockForVMForUser",
				    "sceKernelCheckOpenVMDomain returns 0x%08x\n", ret);
		SCE_DBG_ASSERT_CHECK_FUNC(0, 3894, "sceKernelAllocMemBlockForVMForUser");
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	if (userName == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	ret = sceKernelStrncpyFromUser(name, userName, sizeof(name));
	if (ret < 0)
		return ret;
	name[sizeof(name) - 1] = '\0';
	if (size & 0xfffff)
		return SCE_KERNEL_ERROR_ILLEGAL_MEMBLOCK_SIZE;
	sceKernelSysrootGetCurrentThreadAttr(&attr);
	restype = (attr >> 14) & 1;
	if (SysrootIncProcessResource(restype) < 0) {
		SCE_PRINTF_WITH_INFO_FUNC(0xb, 3923, "sceKernelAllocMemBlockForVMForUser",
					  "MemBlock overflow, restype=0x%08x\n", restype);
		return SCE_KERNEL_ERROR_MEMBLOCK_OVERFLOW;
	}
	storage = SysrootGetProcessLocalStorageAddr(g_uidManager->processStorageKey);
	if (size > VM_SIZE_MAX) {
		ret = SCE_KERNEL_ERROR_MEMBLOCK_OVERFLOW;
		goto fail;
	}
	if ((SceSize)sceKernelAtomicAddAndGet32((SceInt32 *)&storage->vmSize, size) > VM_SIZE_MAX) {
		sceKernelAtomicSubAndGet32((SceInt32 *)&storage->vmSize, size);
		ret = SCE_KERNEL_ERROR_MEMBLOCK_OVERFLOW;
		goto fail;
	}
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = ALLOC_ATTR_KEEP_CLEAR | ALLOC_ATTR_HAS_DOMAIN;
	opt.domain = DOMAIN_VM;
	guid = sceKernelAllocMemBlockWithInfoForKernel(name, SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_RX, size,
					       &opt, NULL);
	ret = guid;
	if (guid < 0)
		goto fail;
	puid = scePUIDOpenByGUID(0, guid);
	ret = puid;
	if (puid >= 0) {
		intr = sceKernelSpinlockLowLockCpuSuspendIntr(&storage->lock);
		if (storage->vmBlocks == NULL) {
			storage->vmBlocks = SceSysrootForKernel_2F75C1DC(
				SYSMEM_VM_BLOCKS * sizeof(SysmemVMBlock), NULL);
			if (storage->vmBlocks == NULL)
				SceDebugForDriver_391B5B74(SCE_DBGINFO_FUNC(3976, "sceKernelAllocMemBlockForVMForUser"),
						__builtin_return_address(0));
			for (entry = storage->vmBlocks; entry != storage->vmBlocks + SYSMEM_VM_BLOCKS;
			     entry++)
				entry->puid = -1;
		}
		ret = sceGUIDReferObjectWithClass(guid, SceSysmemForKernel_86681B64(),
						   (SceObjectBase **)&block);
		if (ret >= 0) {
			rintr = SceCpuForDriver_EC53D007(&block->lock);
			entry = storage->vmBlocks;
			end = entry + SYSMEM_VM_BLOCKS;
			do {
				MemBlockObject *current;
				uintptr_t base;

				if (entry->puid == -1) {
					current = block;
					entry->puid = puid;
					base = current->vbase;
					base += MemBlockGetHeadFreeSize(current);
					entry->base = base;
					entry->size = MemBlockGetAllocMapSizeRaw(block);
					break;
				}
				entry++;
			} while (entry != end);
			sceKernelRWSpinlockLowReadUnlockCpuResumeIntr(&block->lock, rintr);
			sceGUIDReleaseObject(guid);
			sceKernelSpinlockLowUnlockCpuResumeIntr(&storage->lock, intr);
			return puid;
		}
		/* the original keeps the storage lock here */
	}
	sceKernelAtomicSubAndGet32((SceInt32 *)&storage->vmSize, size);
	sceKernelFreeMemBlock(guid);
fail:
	SysrootDecProcessResource(restype);
	return ret;
}

/* a VM block left unmapped and locked, for the process to map itself */
SceUID sceKernelAllocUnmapMemBlockForUser(const char *userName, SceSize size)
{
	SceKernelAllocMemBlockKernelOpt opt;
	SceUInt32 attr, restype;
	MemBlockObject *block = NULL;
	char name[0x20];
	SceUID guid;
	int ret;

	if (!SceSysrootForKernel_F8769E86(0))
		return SCE_ERROR_ERRNO_ENOSYS;
	ret = sceKernelCheckOpenVMDomain();
	if (ret >= 1) {
		SCE_DBG_PRINTF_FUNC(0xb, 4049, "sceKernelAllocUnmapMemBlockForUser",
				    "sceKernelCheckOpenVMDomain returns 0x%08x\n", ret);
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	if (userName == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	ret = sceKernelStrncpyFromUser(name, userName, sizeof(name));
	if (ret < 0)
		return ret;
	name[sizeof(name) - 1] = '\0';
	if (size & 0xfffff)
		return SCE_KERNEL_ERROR_ILLEGAL_MEMBLOCK_SIZE;
	sceKernelSysrootGetCurrentThreadAttr(&attr);
	restype = (attr >> 14) & 1;
	if (SysrootIncProcessResource(restype) < 0) {
		SCE_PRINTF_WITH_INFO_FUNC(0xb, 4078, "sceKernelAllocUnmapMemBlockForUser",
					  "MemBlock overflow:restype=0x%08x\n", restype);
		return SCE_KERNEL_ERROR_MEMBLOCK_OVERFLOW;
	}
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = ALLOC_ATTR_NO_CLEAR | ALLOC_ATTR_NO_MAP;
	guid = sceKernelAllocMemBlockWithInfoForKernel(name, SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_RX, size,
					       &opt, NULL);
	ret = guid;
	if (guid >= 0) {
		ret = sceGUIDGetObject(guid, &block);
		if (ret >= 0) {
			MemBlockSetLockStatusRaw(block, MEMBLOCK_STATUS_UNMAP);
			MemBlockSetLockStatusRaw(block, MEMBLOCK_STATUS_LOCKED);
			ret = scePUIDOpenByGUID(0, guid);
			if (ret >= 0)
				return ret;
		}
		sceKernelFreeMemBlock(guid);
	}
	SysrootDecProcessResource(restype);
	return ret;
}

/* a VM block's range written back to memory and the I-cache dropped */
int sceKernelSyncVMDomainForUser(SceUID puid, uintptr_t va, SceSize size)
{
	SysmemProcessStorage *storage;
	SysmemVMBlock *entry;
	MemBlockObject *block = NULL;
	int ret;

	if (!SceSysrootForKernel_F8769E86(0))
		return SCE_ERROR_ERRNO_ENOSYS;
	storage = SysrootGetProcessLocalStorageAddr(g_uidManager->processStorageKey);
	if (storage != NULL && (entry = storage->vmBlocks) != NULL) {
		SysmemVMBlock *end = entry + SYSMEM_VM_BLOCKS;

		while (entry->puid != puid) {
			entry++;
			if (entry == end)
				goto overflow;
		}
		if (va < entry->base || entry->size < size - entry->base + va)
			return SCE_KERNEL_ERROR_MEMBLOCK_RANGE_ERROR;
		ret = 0;
		goto sync;
	}
overflow:
	SCE_DBG_PRINTF_FUNC(0xb, 4201, "sceKernelSyncVMDomainForUser",
			    "Warning: VM Block overflow\n");
	ret = scePUIDReferObjectWithClass(0, puid, SceSysmemForKernel_86681B64(),
					  (SceObjectBase **)&block);
	if (ret < 0)
		return ret | 0x40000000;
	if (va < block->vbase || block->size < size - block->vbase + va) {
		UIDEntryReleaseObject(0, puid);
		return SCE_KERNEL_ERROR_MEMBLOCK_RANGE_ERROR;
	}
	UIDEntryReleaseObject(0, puid);
sync:
	SceCpuForKernel_4F442396((void *)va, size);
	SceCpuForKernel_2E637B1D((void *)va, size);
	return ret;
}

/* name unknown: a block's base info dropped, the mirrored block told */
int MemBlockClearBaseInfo(MemBlockObject *block)
{
	MemBlockObject *base = NULL;
	void *info = NULL;
	int ret;

	ret = MemBlockOptGetBase(block, &base, &info);
	if (ret < 0)
		return ret;
	if (base != NULL) {
		SceUInt32 flags = base->flags;
		SceUID guid = block->guid;

		if ((flags & MEMBLOCK_OPT_MASK) == MEMBLOCK_OPT_CLIENT_INFO)
			MemBlockOptGetClientInfo(base, guid);
	}
	if (info == NULL)
		return 0;
	MemBlockOptClearBaseInfoRaw(block);
	SceSysmemForKernel_4233C16D(info);
	return 0;
}

/* name unknown: a block freed (not while locked), its mirrored block told */
int FreeMemBlockByGUID(SceUID guid)
{
	MemBlockObject *block = NULL, *base = NULL;
	int ret;

	ret = FreeMemBlockCore(guid, 1, &block, &base, NULL, NULL);
	if (ret >= 0 && base != NULL &&
	    (base->flags & MEMBLOCK_OPT_MASK) == MEMBLOCK_OPT_CLIENT_INFO)
		MemBlockOptGetClientInfo(base, guid);
	return ret;
}
#endif

#define MEMBLOCK_ERROR_LOCKED_FATAL	0xc0024b10

#if !SYSMEM_NSKBL
/* the resource counter a block was charged to */
static inline SceUInt32 MemBlockRestype(SceUInt32 type, SceUInt32 attr)
{
	if (type == SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_CDIALOG_RW ||
	    type == SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_CDIALOG_NC_RW)
		return 2;
	return (attr >> 15) & 1;
}

/* a block of the caller freed by its PUID, the PUID closed */
int sceKernelFreeMemBlockForUser(SceUID puid)
{
	SceUInt32 attr = 0, type = 0;
	MemBlockObject *block = NULL, *base = NULL;
	SceUID guid;
	int ret = 0;

	if (puid == -1)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	guid = SceSysmemForDriver_184172B1(0, puid, SceSysmemForKernel_86681B64());
	if (guid < 0) {
		SCE_DBG_PRINTF_FUNC(0xb, 4340, "sceKernelFreeMemBlockForUser",
			"MemBlockPUIDtoGUID(puid:0x%08x) failed 0x%08x\n", puid, guid);
		return guid | 0x40000000;
	}
	if (sceGUIDGetObjectWithClass(guid, SceSysmemForKernel_86681B64(),
				       (SceObjectBase **)&block) < 0) {
		SCE_DBG_PRINTF_FUNC(0xb, 4348, "sceKernelFreeMemBlockForUser",
			"GUIDGetMemBlock(0x%08x) failed\n", guid);
	} else if (MemBlockCheckLockStatusRaw(block)) {
		ret = MEMBLOCK_ERROR_LOCKED_FATAL;
	} else {
		ret = FreeMemBlockCore(guid, 1, &block, &base, &type, &attr);
		if (ret < 0) {
			SCE_DBG_PRINTF_FUNC(0xb, 4365, "sceKernelFreeMemBlockForUser",
				"FreeMemBlockCore(guid 0x%08x) failed 0x%08x\n", guid, ret);
			ret |= 0x40000000;
		} else if (base != NULL &&
			   (base->flags & MEMBLOCK_OPT_MASK) == MEMBLOCK_OPT_CLIENT_INFO) {
			MemBlockOptGetClientInfo(base, guid);
		}
	}
	PUIDCloseWithFlags(0, puid, 1);
	SysrootDecProcessResource(MemBlockRestype(type, attr));
	return ret;
}

/* a VM block freed and taken off the process's list */
int sceKernelFreeMemBlockForVMForUser(SceUID puid)
{
	SysmemProcessStorage *storage;
	SysmemVMBlock *entry, *end;
	int ret;

	ret = sceKernelCheckOpenVMDomain();
	if (ret > 0) {
		SCE_DBG_PRINTF_FUNC(0xb, 4149, "sceKernelFreeMemBlockForVMForUser",
			"sceKernelCheckOpenVMDomain returns 0x%08x\n", ret);
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	storage = SysrootGetProcessLocalStorageAddr(g_uidManager->processStorageKey);
	if (storage == NULL || (entry = storage->vmBlocks) == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	end = entry + SYSMEM_VM_BLOCKS;
	while (entry->puid != puid) {
		entry++;
		if (entry == end)
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	sceKernelAtomicSubAndGet32((SceInt32 *)&storage->vmSize, entry->size);
	entry->puid = -1;
	return sceKernelFreeMemBlockForUser(puid);
}
#endif

#define MEMBLOCK_OPEN_RO		0x10
#define MEMBLOCK_OPEN_RW		0x30

#if !SYSMEM_NSKBL
/*
 * A mirror of a named shared block for the caller: the block's clients
 * must hold all the capabilities its client info lists.
 */
SceUID sceKernelOpenMemBlockForUser(const char *userName, SceUInt32 flags)
{
	SceKernelAllocMemBlockKernelOpt opt;
	MemBlockClientInfo *client;
	MemBlockBaseInfo *info;
	char name[0x20], clientName[0x20];
	SceUInt32 attr;
	MemBlockObject *base;
	SceUInt32 type, restype, code, mask, i;
	SceUID guid, mirror, puid;
	SceSize size;
	int intr, bit, ret;

	sceKernelSysrootGetCurrentThreadAttr(&attr);
	/* type is not set yet here in the original */
	restype = (attr >> 14) & 1;
	if (type == SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_CDIALOG_RW ||
	    type == SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_CDIALOG_NC_RW)
		restype = 2;
	if (userName == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	switch (flags) {
	case MEMBLOCK_OPEN_RO:
	case MEMBLOCK_OPEN_RW:
		break;
	default:
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	ret = sceKernelStrncpyFromUser(name, userName, sizeof(name));
	if (ret < 0)
		return ret;
	name[sizeof(name) - 1] = '\0';
	guid = GUIDFindByName(name);
	if (guid < 0)
		return guid;
	ret = sceGUIDGetObjectWithClass(guid, SceSysmemForKernel_86681B64(),
					 (SceObjectBase **)&base);
	if (ret < 0)
		return ret;
	info = SceSysmemForKernel_85571907(sizeof(*info));
	if (info == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	intr = SceCpuForDriver_EC53D007(&base->lock);
	{
		MemBlockObject *current = base;
		struct Partition *partition = current->partition;
		SceUID pid, partitionUid;

		code = current->code;
		size = current->size;
		pid = partition->pid;
		partitionUid = partition->uid;
		info->block = current;
		info->pid = pid;
		info->partitionUid = partitionUid;
		info->guid = guid;
		sceKernelRWSpinlockLowReadUnlockCpuResumeIntr(&current->lock, intr);
	}
	if ((base->flags & MEMBLOCK_OPT_MASK) == MEMBLOCK_OPT_CLIENT_INFO) {
		client = (MemBlockClientInfo *)base->opt;
		if (client == NULL) {
			ret = SCE_KERNEL_ERROR_ILLEGAL_MEMBLOCK_PERMISSION;
			goto fail;
		}
	} else {
		client = NULL;	/* the original reads the capabilities through it anyway */
	}
	for (i = 0; i < 8; i++) {
		for (mask = client->capabilities[i]; mask != 0; mask &= ~(1u << bit)) {
			bit = __builtin_ctz(mask);
			if (CheckProcessCapabilityGranted(SceSysrootForKernel_7385CADE(),
							  bit + i * 32) < 0) {
				ret = SCE_ERROR_ERRNO_ENOSYS;
				goto fail;
			}
		}
	}
	if (flags & ~client->openFlags) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_MEMBLOCK_PERMISSION;
		goto fail;
	}
	switch (code) {
	case MEMBLOCK_CODE_USER_CDRAM_RW:
	case MEMBLOCK_CODE_USER_MAIN_RW:
	case MEMBLOCK_CODE_USER_MAIN_NC_RW:
	case MEMBLOCK_CODE_USER_CDIALOG_RW:
	case MEMBLOCK_CODE_USER_CDIALOG_NC_RW:
		if (flags == MEMBLOCK_OPEN_RO)
			code &= ~MEMBLOCK_CODE_W;
		break;
	case MEMBLOCK_CODE_USER_CDRAM_R:
	case MEMBLOCK_CODE_USER_MAIN_R:
	case MEMBLOCK_CODE_USER_CDIALOG_R:
	case MEMBLOCK_CODE_USER_CDIALOG_NC_R:
		if (flags != MEMBLOCK_OPEN_RO) {
			ret = SCE_KERNEL_ERROR_ILLEGAL_MEMBLOCK_PERMISSION;
			goto fail;
		}
		break;
	default:
		ret = SCE_KERNEL_ERROR_ILLEGAL_MEMBLOCK_TYPE;
		goto fail;
	}
	snprintf(clientName, sizeof(clientName), "SceClient:%s", name);
	MemBlockCode2Type(code, &type);
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = ALLOC_ATTR_SHARE_PHYPAGE | SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_MIRROR_BLOCKID;
	if (attr & THREAD_ATTR_UNLIMITED)
		opt.attr = ALLOC_ATTR_SHARE_PHYPAGE | ALLOC_ATTR_USER_CLIENT |
			   SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_MIRROR_BLOCKID;
	opt.mirror_blockid = guid;
	if (SysrootIncProcessResource(restype) < 0) {
		SCE_PRINTF_WITH_INFO_FUNC(0xb, 4580, "sceKernelOpenMemBlockForUser",
			"MemBlock overflow, restype=0x%08x\n", restype);
		ret = SCE_KERNEL_ERROR_MEMBLOCK_OVERFLOW;
		goto fail;
	}
	mirror = sceKernelAllocMemBlockWithInfoForKernel(clientName, type, size, &opt, NULL);
	ret = mirror;
	if (mirror < 0)
		goto uncharge;
	ret = MemBlockSetBaseInfo(mirror, info);
	if (ret < 0) {
		SCE_DBG_PRINTF_FUNC(0xb, 4597, "sceKernelOpenMemBlockForUser",
			"MemBlockSetBaseInfo failed 0x%08x\n", ret);
		goto uncharge;
	}
	ret = MemBlockClientInfoAddGUID(client, mirror);
	if (ret < 0) {
		sceKernelFreeMemBlock(mirror);
		goto uncharge;
	}
	puid = scePUIDOpenByGUID(0, mirror);
	if (puid >= 0)
		return puid;
	MemBlockClientInfoRemoveGUID(client, mirror);
	sceKernelFreeMemBlock(mirror);
	ret = puid;
uncharge:
	SysrootDecProcessResource(restype);
fail:
	SceSysmemForKernel_4233C16D(info);
	return ret;
}

int sceKernelCloseMemBlock(SceUID puid)
{
	return sceKernelFreeMemBlockForUser(puid);
}

/* the base of a block user code may see */
int sceKernelGetMemBlockBaseForUser(SceUID puid, void **userBase)
{
	uintptr_t base;
	MemBlockObject *block;
	SceUID guid;
	int intr, ret, copied;

	if (userBase == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	guid = PUID2GUIDWithRefer(0, puid, SceSysmemForKernel_86681B64());
	if (guid < 0)
		return guid | 0x40000000;
	ret = sceGUIDGetObject(guid, &block);
	if (ret < 0) {
		SCE_DBG_PRINTF_FUNC(0xb, 4937, "sceKernelGetMemBlockBaseForUser",
			"sceGUIDGetObject failed 0x%08x\n", ret);
		goto none;
	}
	intr = SceCpuForDriver_EC53D007(&block->lock);
	ret = MemBlockCodeCheckUserInfo(block->code);
	if (ret < 0) {
		SCE_DBG_PRINTF_FUNC(0xb, 4947, "sceKernelGetMemBlockBaseForUser",
			"MemBlockCheckUserBlockRaw failed 0x%08x\n", ret);
		sceKernelRWSpinlockLowReadUnlockCpuResumeIntr(&block->lock, intr);
		goto none;
	}
	base = block->vbase;
	sceKernelRWSpinlockLowReadUnlockCpuResumeIntr(&block->lock, intr);
	ret = sceKernelCopyToUser(userBase, &base, sizeof(base));
	sceGUIDReleaseObject(guid);
	return ret;
none:
	sceGUIDReleaseObject(guid);
	base = 0;
	copied = sceKernelCopyToUser(userBase, &base, sizeof(base));
	return copied < 0 ? copied : ret;
}

/* the caller's PUID of the block holding [addr, addr + size) */
SceUID sceKernelFindMemBlockByAddrForUser(void *addr, SceSize size)
{
	MemBlockObject *block;
	SceUID guid;
	int ret;

	ret = sceKernelIsAccessibleRange(0x10, addr, size);
	if (ret < 0)
		return ret;
	guid = AddressSpaceFindMemBlockCB(sceKernelSysrootGetCurrentAddressSpaceCB(),
					  (uintptr_t)addr, size, &block);
	if (guid <= 0)
		return guid;
	ret = MemBlockCodeCheckUserInfo(MemBlockGetCode(block));
	if (ret < 0)
		return ret;
	return PUIDFindByGUID(0, guid);
}

int sceKernelGetMemBlockInfoByAddrForUser(void *addr, SceKernelMemBlockInfo *info)
{
	int ret;

	ret = sceKernelIsAccessibleRange(0x10, addr, 0);
	if (ret < 0)
		return ret;
	return GetMemBlockInfoByRangeForUser(1, (uintptr_t)addr, 0, info);
}

int sceKernelGetMemBlockInfoByRange(void *addr, SceSize size, SceKernelMemBlockInfo *info)
{
	int ret;

	ret = sceKernelIsAccessibleRange(0x10, addr, size);
	if (ret < 0)
		return ret;
	return GetMemBlockInfoByRangeForUser(MEMBLOCK_INFO_BY_RANGE, (uintptr_t)addr, size, info);
}

/* name unknown: memblock.c's part of sysmem's start after the process manager */
int MemBlockModuleStartAfterProcessmgr(void)
{
	return 0;
}

/* name unknown: memblock.c's part of sysmem's start */
int MemBlockModuleStart(void)
{
	return 0;
}

/* sceKernelSetMemBlockDebugCallback */
int SceSysmemForKernel_F57A97A0(MemBlockDebugCallbacks *callbacks)
{
#if FW_DEBUG
	s_memBlockDebugCallbacks = callbacks;
#endif
	return 0;
}
#endif

#pragma push_macro("__FILE_NAME__")
#undef __FILE_NAME__
#define __FILE_NAME__ "memblock_obj_module.h"

#define MEMBLOCK_CLIENT_PIDS		0x20

#if !SYSMEM_NSKBL
/* the block gets client info, copied from the allocation options */
int MemBlockOptSetClientInfoRaw(MemBlockObject *block, const SceKernelAllocMemBlockKernelOpt *opt)
{
	MemBlockClientInfo *info;
	SceUID *slot;
	SceUInt32 i, capacity, flags;

	if (block->flags & MEMBLOCK_OPT_MASK)
		return SCE_KERNEL_ERROR_SYSMEM_MEMBLOCK_ERROR;
	info = SceSysmemForKernel_85571907(sizeof(*info));
	if (info == NULL)
		return SCE_KERNEL_ERROR_SYSMEM_MEMBLOCK_ERROR;
	info->lock = 0;
	info->capacity = MEMBLOCK_CLIENT_PIDS;
	slot = SceSysmemForKernel_85571907(MEMBLOCK_CLIENT_PIDS * sizeof(SceUID));
	info->guids = slot;
	if (slot == NULL) {
		info->capacity = 0;
	} else {
		capacity = info->capacity;
		for (i = 0; i != capacity; i++)
			*slot++ = -1;
		info->count = 0;
	}
	SCE_DBG_ASSERT_CHECK_FUNC((block->flags & MEMBLOCK_OPT_MASK) == 0, 1063,
				  "MemBlockOptSetClientInfoRaw");
	flags = block->flags;
	block->opt = (SceUInt32)info;
	block->flags = (flags & ~MEMBLOCK_OPT_MASK) | MEMBLOCK_OPT_CLIENT_INFO;
	if (opt != NULL) {
		info->openFlags = opt->openFlags;
		for (i = 0; i < 8; i++)
			info->capabilities[i] = opt->capabilities[i];
	}
	return 0;
}
#endif

#pragma pop_macro("__FILE_NAME__")

#define MEMBLOCK_GUID_ATTR		0x30000
#define MEMBLOCK_GUID_ATTR_CLIENT	0x3c000	/* PLACEHOLDER: what the 0xc000 grants */

/* name unknown: the block's GUID */
SceUID MemBlockCreateGUID(const char *name, MemBlockObject *block, SceUInt32 attr)
{
	SceUID guid;

	guid = GUIDRegister(SceSysmemForKernel_86681B64(), name, (SceObjectBase *)block,
			    (attr & 0x4000) ? MEMBLOCK_GUID_ATTR_CLIENT : MEMBLOCK_GUID_ATTR);
	if (guid >= 0)
		block->guid = guid;
	return guid;
}

#if SYSMEM_NSKBL
/* the boot loaders' memblock start: nothing for any core */
int BlockStart(void)
{
	sceKernelSysrootCorelockLock(0);
	if (sceKernelCpuId() == 0) {
	}
	sceKernelSysrootCorelockUnlock();
	return 0;
}
#endif
