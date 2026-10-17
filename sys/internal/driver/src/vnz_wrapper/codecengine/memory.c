/* memory.c */
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <sysmem/sysmem.h>
#include <psp2kern/types.h>
#include <sysmem/cpu.h>
#include <psp2kern/kernel/threadmgr/callback.h>
#include <psp2kern/kernel/threadmgr/semaphores.h>
#include <sysmem/debug.h>
#include <psp2kern/kernel/threadmgr/event_flags.h>
#include <psp2kern/kernel/threadmgr/thread.h>
#include <lowio/pervasive_regs.h>
#include <lowio/mailbox_regs.h>
#include <intrmgr/intrmgr.h>
#include <processmgr/processmgr.h>
#include <intrmgr/syscall.h>
#include <psp2kern/kernel/dmac.h>
#include <psp2kern/kernel/threadmgr/misc.h>
#include "../venezia_internal.h"
#include <vnz_wrapper/venezia_debug.h>
#include <string.h>
#include <sysmem/sysroot.h>

struct ThreadmgrFastMutex * _sceCodecEngineGetMutex(SceUID pid);
CodecEngineUnmapMemBlockEntry * _sceCodecEngineGetUnmapMemBlockEntry(SceUID pid, SceUID guid);


extern int _closeUnmapMemBlockAll(SceUID pid);

extern int SceCodecEngineWrapperForDriver_CB8703D8(void);
extern int _unlockProcessSuspendCore(void);
SceClass g_codecEngineMemoryClass;

int _closeUnmapMemBlockCore(SceUID pid, CodecEngineUnmapMemBlockEntry *entry)
{
	CodecEngineMemory *memory = NULL;
	int ret, error;

	ret = SceSysmemForDriver_00ED6C14(entry->guid, &g_codecEngineMemoryClass, (SceObjectBase **)&memory);
	if (ret < 0) {
		if (!memory)
			goto closeGuid;
	} else {
#if FW_DEBUG
		SceDebugForDriver_35A35322(memory != NULL, SCE_DBGINFO_FUNC(470, "_closeUnmapMemBlockCore"), __builtin_return_address(0));
#endif
		if (memory->kernelBase && memory->size)
			memset(memory->kernelBase, 0, memory->size);
		memory->size = 0;
		ret = 0;
		if (memory->userMemBlock >= 0) {
			error = SceSysmemForDriver_04059C4B(memory->userMemBlock, 1);
			ret = error & (error >> 31);
			memory->userBase = NULL;
			memory->userMemBlock = -1;
		}
		if (entry->puid >= 0) {
			error = SceSysmemForDriver_84A4AF5E(pid, entry->puid);
			entry->puid = -1;
			if (error < 0)
				ret = error;
		}
		if (memory->kernelMemBlock >= 0) {
			error = SceSysmemForDriver_009E1C61(memory->kernelMemBlock);
			ret &= ~(error >> 31);
			if (error < 0)
				ret = error;
			memory->kernelMemBlock = -1;
			memory->kernelBase = NULL;
			memory->physicalBase = 0;
		}
	}
	error = sceGUIDReleaseObject(entry->guid);
	ret &= ~(error >> 31);
	if (error < 0)
		ret = error;
closeGuid:
	if (entry->guid >= 0) {
		error = sceGUIDClose(entry->guid);
		ret &= ~(error >> 31);
		if (error < 0)
			ret = error;
		entry->guid = -1;
	}
	return ret;
}

int initializeCodecEngineMemoryClass(void)
{
	return SceSysmemForDriver_61317102(&g_codecEngineMemoryClass,
		"SceUIDCodecEngineMemoryClass", SceSysmemForDriver_85336A1C(),
		sizeof(CodecEngineMemory), NULL, NULL);
}

/* name unknown: require exactly one physical range and a successful query. */
int isVeneziaPhysicalRangeContiguous(const void *base, SceSize size)
{
	SceKernelVARange range;
	SceKernelPAVector vector;
	int ret;

	if (!base)
		return 0;
	range.addr = (uintptr_t)base;
	range.size = size;
	vector.size = sizeof(vector);
	vector.capacity = 0;
	vector.pRanges = NULL;
	ret = sceKernelVARangeToPAVector(&range, &vector);
	return ret >= 0 && vector.total == 1 && ret == 0;
}

extern int callVeneziaRpcCommand500(unsigned int argument0, unsigned int argument1);
extern int initVeneziaHeapManager(VeneziaHeapManager *manager, void *base, SceSize size);

int sceCodecEngineOpenUnmapMemBlockForUser(void *base, SceSize size)
{
	SceUInt32 tls = sceSyscallEnter();
	CodecEngineMemory *memory = NULL;
	CodecEngineUnmapMemBlockEntry *entry;
	struct ThreadmgrFastMutex *mutex;
	SceKernelVARange range;
	union {
		SceKernelPAVector vector;
		uintptr_t physicalBase;
	} query;
	SceKernelAllocMemBlockKernelOpt opt;
	MemBlockPhyInfo info;
	SceUID guid, puid, userMemBlock, kernelMemBlock;
	SceKernelMemBlockType type;
	int ret, error;

	ret = SceCodecEngineWrapperForDriver_CB8703D8();
	if (ret < 0)
		goto leave;
	mutex = _sceCodecEngineGetMutex(0);
	if (!mutex) {
		ret = (int)0x80600000;
		goto unlockProcess;
	}
	ret = ksceKernelLockFastMutex(mutex);
	if (ret < 0)
		goto unlockProcess;
	guid = SceSysmemForDriver_56A13E90(&g_codecEngineMemoryClass, "SceCodecEngineMemory", (SceObjectBase **)&memory);
	ret = guid;
	if (guid < 0)
		goto unlock;
#if FW_DEBUG
	SceDebugForDriver_35A35322(memory != NULL, SCE_DBGINFO_FUNC(139, "sceCodecEngineOpenUnmapMemBlockForUser"), __builtin_return_address(0));
#endif
	memory->userMemBlock = -1;
	memory->kernelMemBlock = -1;
	entry = _sceCodecEngineGetUnmapMemBlockEntry(0, -1);
	if (!entry) {
		ret = (int)0x80600007;
		goto close;
	}
	if (!base) {
		ret = (int)0x80600000;
		goto close;
	}
	if ((uintptr_t)base & 255) {
		ret = (int)0x80600002;
		goto close;
	}
	if (size & 255) {
		ret = (int)0x80600001;
		goto close;
	}
	range.addr = (uintptr_t)base;
	range.size = size;
	query.vector.capacity = 0;
	query.vector.pRanges = NULL;
	query.vector.size = sizeof(query.vector);
	ret = sceKernelVARangeToPAVector(&range, &query.vector);
	if (ret < 0)
		goto close;
	if (query.vector.total != 1) {
		ret = (int)0x80600003;
		goto close;
	}
	ret = SceSysmemForDriver_9C78064C(48, base, size);
	if (ret < 0)
		goto close;
	userMemBlock = SceSysmemForDriver_8A1742F6(base, size);
	ret = userMemBlock;
	if (userMemBlock < 0)
		goto close;
	info.ex.size = sizeof(info);
	ret = SceSysmemForDriver_24A99FFF(userMemBlock, &info);
	if (ret < 0)
		goto close;
	if (SceSysmemForDriver_20C811FA(info.ex.details.type) != 0x8000) {
		ret = (int)0x80600008;
		goto close;
	}
	if (size > info.ex.details.size) {
		ret = (int)0x80600001;
		goto close;
	}
	error = SceSysmemForDriver_8D160E65(base, &query.physicalBase);
#if FW_DEBUG
	SceDebugForDriver_35A35322(error >= 0, SCE_DBGINFO_FUNC(208, "sceCodecEngineOpenUnmapMemBlockForUser"), __builtin_return_address(0));
#endif
	type = 0x60208006;
	if (query.physicalBase >= 0x20000000 && query.physicalBase + size <= 0x30000000)
		type = 0x40408006;
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.mirror_blockid = userMemBlock;
	opt.attr = 0x1000040;
	kernelMemBlock = SceSysmemForDriver_C94850C9(info.ex.details.name, type, 0, &opt);
	ret = kernelMemBlock;
	if (kernelMemBlock < 0)
		goto close;
	puid = SceSysmemForDriver_BF209859(0, guid);
	ret = puid;
	if (puid >= 0) {
		info.ex.size = sizeof(info);
		ret = SceSysmemForDriver_24A99FFF(kernelMemBlock, &info);
		if (ret >= 0) {
			ret = SceSysmemForDriver_FFCD9B60(userMemBlock);
			if (ret >= 0) {
				ret = callVeneziaRpcCommand500(query.physicalBase, size);
				if (ret >= 0) {
					ret = initVeneziaHeapManager(&memory->heapManager, (void *)info.ex.details.vbase, size);
					if (ret >= 0) {
						memory->userBase = base;
						memory->kernelBase = (void *)info.ex.details.vbase;
						error = SceSysmemForDriver_8D160E65(memory->kernelBase, &memory->physicalBase);
#if FW_DEBUG
						SceDebugForDriver_35A35322(error >= 0, SCE_DBGINFO_FUNC(268, "sceCodecEngineOpenUnmapMemBlockForUser"), __builtin_return_address(0));
						SceDebugForDriver_35A35322(memory->physicalBase != 0, SCE_DBGINFO_FUNC(269, "sceCodecEngineOpenUnmapMemBlockForUser"), __builtin_return_address(0));
#endif
						memory->size = size;
						memory->userMemBlock = userMemBlock;
						memory->kernelMemBlock = kernelMemBlock;
						entry->guid = guid;
						entry->puid = puid;
						ret = puid;
						goto unlock;
					}
				}
				SceSysmemForDriver_04059C4B(userMemBlock, 1);
			}
		}
		if (puid != -1)
			SceSysmemForDriver_84A4AF5E(0, puid);
	}
	SceSysmemForDriver_009E1C61(kernelMemBlock);
close:
	sceGUIDClose(guid);
unlock:
	ksceKernelUnlockFastMutex(mutex);
unlockProcess:
	_unlockProcessSuspendCore();
leave:
	sceSyscallLeave(tls);
	return ret;
}

extern int isVeneziaHeapManagerRegistered(VeneziaHeapManager *manager);

int _closeUnmapMemBlock(SceUID puid)
{
	SceUInt32 tls = sceSyscallEnter();
	struct ThreadmgrFastMutex *mutex;
	CodecEngineUnmapMemBlockEntry *entry;
	CodecEngineMemory *memory;
	SceUID guid;
	int ret, registered;

	ret = SceCodecEngineWrapperForDriver_CB8703D8();
	if (ret < 0)
		goto leave;
	mutex = _sceCodecEngineGetMutex(0);
	if (!mutex) {
		ret = (int)0x80600000;
		goto unlockProcess;
	}
	ret = ksceKernelLockFastMutex(mutex);
	if (ret < 0)
		goto unlockProcess;
	memory = NULL;
	guid = SceSysmemForDriver_45D22597(0, puid);
	if (guid <= 0) {
		ret = guid | 0x40000000;
		if (guid < 0)
			goto unlock;
	} else {
		entry = _sceCodecEngineGetUnmapMemBlockEntry(0, guid);
		if (!entry) {
			ret = (int)0x80600009;
			goto unlock;
		}
#if FW_DEBUG
		SceDebugForDriver_35A35322(puid == entry->puid, SCE_DBGINFO_FUNC(1382, "_isUnmapMemoryRegistered"), __builtin_return_address(0));
#endif
		registered = SceSysmemForDriver_00ED6C14(entry->guid,
			&g_codecEngineMemoryClass, (SceObjectBase **)&memory);
		if (registered >= 0) {
#if FW_DEBUG
			SceDebugForDriver_35A35322(memory != NULL, SCE_DBGINFO_FUNC(1390, "_isUnmapMemoryRegistered"), __builtin_return_address(0));
#endif
			registered = isVeneziaHeapManagerRegistered(&memory->heapManager);
		}
		if (memory) {
			ret = sceGUIDReleaseObject(entry->guid);
			if (ret < 0)
				goto unlock;
		}
		if (registered < 0) {
			ret = registered;
			goto unlock;
		}
	}
	guid = SceSysmemForDriver_45D22597(0, puid);
	if (guid <= 0) {
		ret = guid | 0x40000000;
		goto unlock;
	}
	entry = _sceCodecEngineGetUnmapMemBlockEntry(0, guid);
	if (!entry) {
		ret = (int)0x80600009;
		goto unlock;
	}
#if FW_DEBUG
	SceDebugForDriver_35A35322(puid == entry->puid, SCE_DBGINFO_FUNC(382, "_closeUnmapMemBlock"), __builtin_return_address(0));
#endif
	ret = _closeUnmapMemBlockCore(0, entry);
unlock:
	ksceKernelUnlockFastMutex(mutex);
unlockProcess:
	_unlockProcessSuspendCore();
leave:
	sceSyscallLeave(tls);
	return ret;
}

extern void *_sceVeneziaHeapManagerAllocMemory(VeneziaHeapManager *manager,
	SceSize size, SceSize alignment);

void *sceCodecEngineAllocMemoryFromUnmapMemBlockForUser(SceUID puid,
	SceSize size, SceSize alignment)
{
	SceUInt32 tls = sceSyscallEnter();
	struct ThreadmgrFastMutex *mutex;
	CodecEngineMemory *memory;
	CodecEngineUnmapMemBlockEntry *entry;
	uintptr_t result = size;
	SceUID guid;
	int ret;

	sceKernelApiDeclareTypeUInt32();
	if (!size)
		goto finish;
	if (alignment <= 255)
		alignment = 256;
	else if (alignment > 0x100000 || (alignment & (alignment - 1))) {
		result = 0;
		goto finish;
	}
	ret = SceCodecEngineWrapperForDriver_CB8703D8();
	if (ret < 0) {
		result = ret;
		goto leave;
	}
	mutex = _sceCodecEngineGetMutex(0);
	if (!mutex || ksceKernelLockFastMutex(mutex) < 0) {
		result = 0;
		goto unlockProcess;
	}
	memory = NULL;
	guid = SceSysmemForDriver_45D22597(0, puid);
	if (guid <= 0) {
		result = 0;
		goto unlock;
	}
	entry = _sceCodecEngineGetUnmapMemBlockEntry(0, guid);
	if (!entry) {
		result = 0;
		goto unlock;
	}
#if FW_DEBUG
	SceDebugForDriver_35A35322(puid == entry->puid, SCE_DBGINFO_FUNC(595, "sceCodecEngineAllocMemoryFromUnmapMemBlockForUser"), __builtin_return_address(0));
#endif
	ret = SceSysmemForDriver_00ED6C14(guid, &g_codecEngineMemoryClass,
		(SceObjectBase **)&memory);
	if (ret < 0) {
		result = 0;
	} else {
#if FW_DEBUG
		SceDebugForDriver_35A35322(memory != NULL, SCE_DBGINFO_FUNC(604, "sceCodecEngineAllocMemoryFromUnmapMemBlockForUser"), __builtin_return_address(0));
#endif
		result = (uintptr_t)_sceVeneziaHeapManagerAllocMemory(
			&memory->heapManager, (size + 255) & ~255U, alignment);
		if (result) {
#if FW_DEBUG
			SceDebugForDriver_35A35322(result >= (uintptr_t)memory->kernelBase &&
				result < (uintptr_t)memory->kernelBase + memory->size,
				SCE_DBGINFO_FUNC(616, "sceCodecEngineAllocMemoryFromUnmapMemBlockForUser"), __builtin_return_address(0));
#endif
			result += (uintptr_t)memory->userBase - (uintptr_t)memory->kernelBase;
		}
	}
	if (memory)
		result &= ~(sceGUIDReleaseObject(entry->guid) >> 31);
unlock:
	ksceKernelUnlockFastMutex(mutex);
unlockProcess:
	_unlockProcessSuspendCore();
leave:
	sceSyscallLeave(tls);
finish:
	return (void *)result;
}

extern int _procMemory(VeneziaHeapManager *manager, void *memory);

int sceCodecEngineFreeMemoryFromUnmapMemBlockForUser(SceUID puid, void *address)
{
	SceUInt32 tls = sceSyscallEnter();
	struct ThreadmgrFastMutex *mutex;
	CodecEngineUnmapMemBlockEntry *entry;
	CodecEngineMemory *memory;
	SceUID guid;
	int ret, error;

	ret = SceCodecEngineWrapperForDriver_CB8703D8();
	if (ret < 0)
		goto leave;
	mutex = _sceCodecEngineGetMutex(0);
	if (!mutex) {
		ret = (int)0x80600000;
		goto unlockProcess;
	}
	ret = ksceKernelLockFastMutex(mutex);
	if (ret < 0)
		goto unlockProcess;
	memory = NULL;
	guid = SceSysmemForDriver_45D22597(0, puid);
	if (guid <= 0) {
		ret = guid | 0x40000000;
		goto unlock;
	}
	entry = _sceCodecEngineGetUnmapMemBlockEntry(0, guid);
	if (!entry) {
		ret = (int)0x80600009;
		goto unlock;
	}
#if FW_DEBUG
	SceDebugForDriver_35A35322(puid == entry->puid, SCE_DBGINFO_FUNC(695, "sceCodecEngineFreeMemoryFromUnmapMemBlockForUser"), __builtin_return_address(0));
#endif
	ret = SceSysmemForDriver_00ED6C14(guid, &g_codecEngineMemoryClass,
		(SceObjectBase **)&memory);
	if (ret >= 0) {
#if FW_DEBUG
		SceDebugForDriver_35A35322(memory != NULL, SCE_DBGINFO_FUNC(703, "sceCodecEngineFreeMemoryFromUnmapMemBlockForUser"), __builtin_return_address(0));
#endif
		if ((uintptr_t)address < (uintptr_t)memory->userBase ||
			(uintptr_t)address >= (uintptr_t)memory->userBase + memory->size) {
			ret = (int)0x80600000;
		} else {
			ret = _procMemory(&memory->heapManager,
				(void *)((uintptr_t)memory->kernelBase - (uintptr_t)memory->userBase + (uintptr_t)address));
		}
	}
	if (memory) {
		error = sceGUIDReleaseObject(entry->guid);
		ret &= ~(error >> 31);
		if (error < 0)
			ret = error;
	}
unlock:
	ksceKernelUnlockFastMutex(mutex);
unlockProcess:
	_unlockProcessSuspendCore();
leave:
	sceSyscallLeave(tls);
	return ret;
}

extern int registerVeneziaHeapManagerMemory(VeneziaHeapManager *manager,
	void *memory, SceSize size);

int sceCodecEngineRegisterUnmapMemory(SceUID pid, void *address, SceSize size)
{
	struct ThreadmgrFastMutex *mutex;
	CodecEngineUnmapMemBlockEntry *entries;
	CodecEngineMemory *memory;
	unsigned int index;
	int ret, error, registered;

	if (!address)
		return (int)0x80600000;
	mutex = _sceCodecEngineGetMutex(pid);
	if (!mutex)
		return (int)0x80600000;
	ret = ksceKernelLockFastMutex(mutex);
	if (ret < 0)
		return ret;
	entries = _sceCodecEngineGetUnmapMemBlockEntry(pid, 0);
#if FW_DEBUG
	SceDebugForDriver_35A35322(entries != NULL, SCE_DBGINFO_FUNC(773, "sceCodecEngineRegisterUnmapMemory"), __builtin_return_address(0));
#endif
	for (index = 0; index < 4; index++) {
		if (entries[index].guid == -1)
			continue;
		memory = NULL;
		ret = SceSysmemForDriver_00ED6C14(entries[index].guid,
			&g_codecEngineMemoryClass, (SceObjectBase **)&memory);
		registered = 0;
		if (ret < 0) {
			if (!memory)
				goto checkResult;
		} else {
#if FW_DEBUG
			SceDebugForDriver_35A35322(memory != NULL, SCE_DBGINFO_FUNC(789, "sceCodecEngineRegisterUnmapMemory"), __builtin_return_address(0));
			SceDebugForDriver_35A35322(memory->kernelMemBlock != -1, SCE_DBGINFO_FUNC(790, "sceCodecEngineRegisterUnmapMemory"), __builtin_return_address(0));
#endif
			if ((uintptr_t)address >= (uintptr_t)memory->userBase &&
				(uintptr_t)address < (uintptr_t)memory->userBase + memory->size) {
				ret = registerVeneziaHeapManagerMemory(&memory->heapManager,
					(void *)((uintptr_t)memory->kernelBase - (uintptr_t)memory->userBase + (uintptr_t)address), size);
				registered = ret >= 0;
				if (!memory)
					goto checkResult;
			}
		}
		error = sceGUIDReleaseObject(entries[index].guid);
		if (error < 0) {
			ret = error;
			goto unlock;
		}
checkResult:
		if (ret < 0 || registered)
			goto unlock;
	}
	ret = (int)0x80600000;
unlock:
	ksceKernelUnlockFastMutex(mutex);
	return ret;
}

extern int unregisterVeneziaHeapManagerMemory(VeneziaHeapManager *manager,
	void *memory, SceSize size);

int sceCodecEngineUnregisterUnmapMemory(SceUID pid, void *address, SceSize size)
{
	struct ThreadmgrFastMutex *mutex;
	CodecEngineUnmapMemBlockEntry *entries;
	CodecEngineMemory *memory;
	unsigned int index;
	int ret, error, registered;

	if (!address)
		return (int)0x80600000;
	mutex = _sceCodecEngineGetMutex(pid);
	if (!mutex)
		return (int)0x80600000;
	ret = ksceKernelLockFastMutex(mutex);
	if (ret < 0)
		return ret;
	entries = _sceCodecEngineGetUnmapMemBlockEntry(pid, 0);
#if FW_DEBUG
	SceDebugForDriver_35A35322(entries != NULL, SCE_DBGINFO_FUNC(865, "sceCodecEngineUnregisterUnmapMemory"), __builtin_return_address(0));
#endif
	for (index = 0; index < 4; index++) {
		if (entries[index].guid == -1)
			continue;
		memory = NULL;
		ret = SceSysmemForDriver_00ED6C14(entries[index].guid,
			&g_codecEngineMemoryClass, (SceObjectBase **)&memory);
		registered = 0;
		if (ret < 0) {
			if (!memory)
				goto checkResult;
		} else {
#if FW_DEBUG
			SceDebugForDriver_35A35322(memory != NULL, SCE_DBGINFO_FUNC(881, "sceCodecEngineUnregisterUnmapMemory"), __builtin_return_address(0));
			SceDebugForDriver_35A35322(memory->kernelMemBlock != -1, SCE_DBGINFO_FUNC(882, "sceCodecEngineUnregisterUnmapMemory"), __builtin_return_address(0));
#endif
			if ((uintptr_t)address >= (uintptr_t)memory->userBase &&
				(uintptr_t)address < (uintptr_t)memory->userBase + memory->size) {
				ret = unregisterVeneziaHeapManagerMemory(&memory->heapManager,
					(void *)((uintptr_t)memory->kernelBase - (uintptr_t)memory->userBase + (uintptr_t)address), size);
				registered = ret >= 0;
				if (!memory)
					goto checkResult;
			}
		}
		error = sceGUIDReleaseObject(entries[index].guid);
		if (error < 0) {
			ret = error;
			goto unlock;
		}
checkResult:
		if (ret < 0 || registered)
			goto unlock;
	}
	ret = (int)0x80600000;
unlock:
	ksceKernelUnlockFastMutex(mutex);
	return ret;
}

int sceCodecEngineIsUnmapMemory(SceUID pid, const void *address, SceSize size)
{
	struct ThreadmgrFastMutex *mutex;
	CodecEngineUnmapMemBlockEntry *entries;
	CodecEngineMemory *memory;
	uintptr_t end = (uintptr_t)address + size;
	unsigned int index;
	int ret, error, contains;

	mutex = _sceCodecEngineGetMutex(pid);
	if (!mutex)
		return (int)0x80600000;
	ret = ksceKernelLockFastMutex(mutex);
	if (ret < 0)
		return ret;
	entries = _sceCodecEngineGetUnmapMemBlockEntry(pid, 0);
#if FW_DEBUG
	SceDebugForDriver_35A35322(entries != NULL, SCE_DBGINFO_FUNC(952, "sceCodecEngineIsUnmapMemory"), __builtin_return_address(0));
#endif
	for (index = 0; index < 4; index++) {
		if (entries[index].guid == -1)
			continue;
		memory = NULL;
		contains = 0;
		ret = SceSysmemForDriver_00ED6C14(entries[index].guid,
			&g_codecEngineMemoryClass, (SceObjectBase **)&memory);
		if (ret >= 0) {
#if FW_DEBUG
			SceDebugForDriver_35A35322(memory != NULL, SCE_DBGINFO_FUNC(968, "sceCodecEngineIsUnmapMemory"), __builtin_return_address(0));
			SceDebugForDriver_35A35322(memory->kernelMemBlock != -1, SCE_DBGINFO_FUNC(969, "sceCodecEngineIsUnmapMemory"), __builtin_return_address(0));
#endif
			contains = (uintptr_t)memory->userBase <= (uintptr_t)address &&
				(uintptr_t)address < (uintptr_t)memory->userBase + memory->size &&
				(uintptr_t)memory->userBase < end &&
				(uintptr_t)memory->userBase + memory->size >= end;
		}
		if (memory) {
			error = sceGUIDReleaseObject(entries[index].guid);
			if (error < 0) {
				ret = error;
				goto unlock;
			}
		}
		if (ret < 0)
			goto unlock;
		if (contains) {
			ret = 1;
			goto unlock;
		}
	}
unlock:
	ksceKernelUnlockFastMutex(mutex);
	return ret;
}

int sceCodecEngineUnmapUserVAtoPA(SceUID pid, const void *address, uintptr_t *physicalAddress)
{
	struct ThreadmgrFastMutex *mutex;
	CodecEngineUnmapMemBlockEntry *entries;
	CodecEngineMemory *memory;
	unsigned int index;
	int ret, error, contains;

	if (!address)
		return (int)0x80600000;
	mutex = _sceCodecEngineGetMutex(pid);
	if (!mutex)
		return (int)0x80600000;
	ret = ksceKernelLockFastMutex(mutex);
	if (ret < 0)
		return ret;
	entries = _sceCodecEngineGetUnmapMemBlockEntry(pid, 0);
#if FW_DEBUG
	SceDebugForDriver_35A35322(entries != NULL, SCE_DBGINFO_FUNC(1033, "sceCodecEngineUnmapUserVAtoPA"), __builtin_return_address(0));
#endif
	for (index = 0; index < 4; index++) {
		if (entries[index].guid == -1)
			continue;
		memory = NULL;
		ret = SceSysmemForDriver_00ED6C14(entries[index].guid,
			&g_codecEngineMemoryClass, (SceObjectBase **)&memory);
		contains = 0;
		if (ret < 0) {
			if (!memory)
				goto unlock;
		} else {
#if FW_DEBUG
			SceDebugForDriver_35A35322(memory != NULL, SCE_DBGINFO_FUNC(1049, "sceCodecEngineUnmapUserVAtoPA"), __builtin_return_address(0));
			SceDebugForDriver_35A35322(memory->kernelMemBlock != -1, SCE_DBGINFO_FUNC(1050, "sceCodecEngineUnmapUserVAtoPA"), __builtin_return_address(0));
#endif
			if ((uintptr_t)address >= (uintptr_t)memory->userBase &&
				(uintptr_t)address < (uintptr_t)memory->userBase + memory->size) {
				contains = 1;
				if (physicalAddress)
					*physicalAddress = memory->physicalBase - (uintptr_t)memory->userBase + (uintptr_t)address;
			}
		}
		error = sceGUIDReleaseObject(entries[index].guid);
		if (error < 0) {
			ret = error;
			goto unlock;
		}
		if (ret < 0 || contains)
			goto unlock;
	}
	ret = (int)0x80600000;
unlock:
	ksceKernelUnlockFastMutex(mutex);
	return ret;
}

int sceCodecEngineUnmapPAtoUserVA(SceUID pid, uintptr_t address, void **userAddress)
{
	struct ThreadmgrFastMutex *mutex;
	CodecEngineUnmapMemBlockEntry *entries;
	CodecEngineMemory *memory;
	unsigned int index;
	int ret, error, contains;

	if (!address)
		return (int)0x80600000;
	mutex = _sceCodecEngineGetMutex(pid);
	if (!mutex)
		return (int)0x80600000;
	ret = ksceKernelLockFastMutex(mutex);
	if (ret < 0)
		return ret;
	entries = _sceCodecEngineGetUnmapMemBlockEntry(pid, 0);
#if FW_DEBUG
	SceDebugForDriver_35A35322(entries != NULL, SCE_DBGINFO_FUNC(1121, "sceCodecEngineUnmapPAtoUserVA"), __builtin_return_address(0));
#endif
	for (index = 0; index < 4; index++) {
		if (entries[index].guid == -1)
			continue;
		memory = NULL;
		ret = SceSysmemForDriver_00ED6C14(entries[index].guid,
			&g_codecEngineMemoryClass, (SceObjectBase **)&memory);
		contains = 0;
		if (ret < 0) {
			if (!memory)
				goto unlock;
		} else {
#if FW_DEBUG
			SceDebugForDriver_35A35322(memory != NULL, SCE_DBGINFO_FUNC(1137, "sceCodecEngineUnmapPAtoUserVA"), __builtin_return_address(0));
			SceDebugForDriver_35A35322(memory->kernelMemBlock != -1, SCE_DBGINFO_FUNC(1138, "sceCodecEngineUnmapPAtoUserVA"), __builtin_return_address(0));
#endif
			if ((uintptr_t)address >= memory->physicalBase &&
				(uintptr_t)address < memory->physicalBase + memory->size) {
				contains = 1;
				if (userAddress)
					*userAddress = (void *)((uintptr_t)memory->userBase - memory->physicalBase + address);
			}
		}
		error = sceGUIDReleaseObject(entries[index].guid);
		if (error < 0) {
			ret = error;
			goto unlock;
		}
		if (ret < 0 || contains)
			goto unlock;
	}
	ret = (int)0x80600000;
unlock:
	ksceKernelUnlockFastMutex(mutex);
	return ret;
}

int sceCodecEngineUnmapUserVAtoKernelVA(SceUID pid, const void *address, void **kernelAddress)
{
	struct ThreadmgrFastMutex *mutex;
	CodecEngineUnmapMemBlockEntry *entries;
	CodecEngineMemory *memory;
	unsigned int index;
	int ret, error, contains;

	if (!address)
		return (int)0x80600000;
	mutex = _sceCodecEngineGetMutex(pid);
	if (!mutex)
		return (int)0x80600000;
	ret = ksceKernelLockFastMutex(mutex);
	if (ret < 0)
		return ret;
	entries = _sceCodecEngineGetUnmapMemBlockEntry(pid, 0);
#if FW_DEBUG
	SceDebugForDriver_35A35322(entries != NULL, SCE_DBGINFO_FUNC(1209, "sceCodecEngineUnmapUserVAtoKernelVA"), __builtin_return_address(0));
#endif
	for (index = 0; index < 4; index++) {
		if (entries[index].guid == -1)
			continue;
		memory = NULL;
		ret = SceSysmemForDriver_00ED6C14(entries[index].guid,
			&g_codecEngineMemoryClass, (SceObjectBase **)&memory);
		contains = 0;
		if (ret < 0) {
			if (!memory)
				goto unlock;
		} else {
#if FW_DEBUG
			SceDebugForDriver_35A35322(memory != NULL, SCE_DBGINFO_FUNC(1225, "sceCodecEngineUnmapUserVAtoKernelVA"), __builtin_return_address(0));
			SceDebugForDriver_35A35322(memory->kernelMemBlock != -1, SCE_DBGINFO_FUNC(1226, "sceCodecEngineUnmapUserVAtoKernelVA"), __builtin_return_address(0));
#endif
			if ((uintptr_t)address >= (uintptr_t)memory->userBase &&
				(uintptr_t)address < (uintptr_t)memory->userBase + memory->size) {
				contains = 1;
				if (kernelAddress)
					*kernelAddress = (void *)((uintptr_t)memory->kernelBase - (uintptr_t)memory->userBase + (uintptr_t)address);
			}
		}
		error = sceGUIDReleaseObject(entries[index].guid);
		if (error < 0) {
			ret = error;
			goto unlock;
		}
		if (ret < 0 || contains)
			goto unlock;
	}
	ret = (int)0x80600000;
unlock:
	ksceKernelUnlockFastMutex(mutex);
	return ret;
}

int sceCodecEngineUnmapKernelVAtoUserVA(SceUID pid, const void *address, void **userAddress)
{
	struct ThreadmgrFastMutex *mutex;
	CodecEngineUnmapMemBlockEntry *entries;
	CodecEngineMemory *memory;
	unsigned int index;
	int ret, error, contains;

	if (!address)
		return (int)0x80600000;
	mutex = _sceCodecEngineGetMutex(pid);
	if (!mutex)
		return (int)0x80600000;
	ret = ksceKernelLockFastMutex(mutex);
	if (ret < 0)
		return ret;
	entries = _sceCodecEngineGetUnmapMemBlockEntry(pid, 0);
#if FW_DEBUG
	SceDebugForDriver_35A35322(entries != NULL, SCE_DBGINFO_FUNC(1297, "sceCodecEngineUnmapKernelVAtoUserVA"), __builtin_return_address(0));
#endif
	for (index = 0; index < 4; index++) {
		if (entries[index].guid == -1)
			continue;
		memory = NULL;
		ret = SceSysmemForDriver_00ED6C14(entries[index].guid,
			&g_codecEngineMemoryClass, (SceObjectBase **)&memory);
		contains = 0;
		if (ret < 0) {
			if (!memory)
				goto unlock;
		} else {
#if FW_DEBUG
			SceDebugForDriver_35A35322(memory != NULL, SCE_DBGINFO_FUNC(1313, "sceCodecEngineUnmapKernelVAtoUserVA"), __builtin_return_address(0));
			SceDebugForDriver_35A35322(memory->kernelMemBlock != -1, SCE_DBGINFO_FUNC(1314, "sceCodecEngineUnmapKernelVAtoUserVA"), __builtin_return_address(0));
#endif
			if ((uintptr_t)address >= (uintptr_t)memory->kernelBase &&
				(uintptr_t)address < (uintptr_t)memory->kernelBase + memory->size) {
				contains = 1;
				if (userAddress)
					*userAddress = (void *)((uintptr_t)memory->userBase - (uintptr_t)memory->kernelBase + (uintptr_t)address);
			}
		}
		error = sceGUIDReleaseObject(entries[index].guid);
		if (error < 0) {
			ret = error;
			goto unlock;
		}
		if (ret < 0 || contains)
			goto unlock;
	}
	ret = (int)0x80600000;
unlock:
	ksceKernelUnlockFastMutex(mutex);
	return ret;
}

int _closeUnmapMemBlockAll(SceUID pid)
{
	struct ThreadmgrFastMutex *mutex;
	CodecEngineUnmapMemBlockEntry *entries, *entry;
	CodecEngineMemory *memory;
	SceUID puid, guid;
	unsigned int index;
	int ret, registered, error;

	mutex = _sceCodecEngineGetMutex(pid);
	if (!mutex)
		return (int)0x80600000;
	ret = ksceKernelLockFastMutex(mutex);
	if (ret < 0)
		return ret;
	entries = _sceCodecEngineGetUnmapMemBlockEntry(pid, 0);
#if FW_DEBUG
	SceDebugForDriver_35A35322(entries != NULL, SCE_DBGINFO_FUNC(423, "_closeUnmapMemBlockAll"), __builtin_return_address(0));
#endif
	for (index = 0; index < 4; index++) {
		if (entries[index].guid == -1)
			continue;
		puid = entries[index].puid;
		memory = NULL;
		guid = SceSysmemForDriver_45D22597(pid, puid);
		if (guid <= 0) {
			if (guid < 0)
				ret = guid | 0x40000000;
		} else {
			entry = _sceCodecEngineGetUnmapMemBlockEntry(pid, guid);
			if (!entry) {
				ret = (int)0x80600009;
			} else {
#if FW_DEBUG
				SceDebugForDriver_35A35322(puid == entry->puid, SCE_DBGINFO_FUNC(1382, "_isUnmapMemoryRegistered"), __builtin_return_address(0));
#endif
				registered = SceSysmemForDriver_00ED6C14(entry->guid,
					&g_codecEngineMemoryClass, (SceObjectBase **)&memory);
				if (registered >= 0) {
#if FW_DEBUG
					SceDebugForDriver_35A35322(memory != NULL, SCE_DBGINFO_FUNC(1390, "_isUnmapMemoryRegistered"), __builtin_return_address(0));
#endif
					registered = isVeneziaHeapManagerRegistered(&memory->heapManager);
				}
				if (memory && (error = sceGUIDReleaseObject(entry->guid)) < 0) {
					ret = error;
				} else {
					ret &= ~(registered >> 31);
					if (registered < 0)
						ret = registered;
				}
			}
		}
		error = _closeUnmapMemBlockCore(pid, &entries[index]);
		ret &= ~(error >> 31);
		if (error < 0)
			ret = error;
	}
	ret &= ret >> 31;
	ksceKernelUnlockFastMutex(mutex);
	return ret;
}
