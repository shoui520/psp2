/* SceNgs kernel platform services. Internal Sony names are unavailable. */

#include <scetypes.h>
#include <sysmem/sysmem.h>
#include <processmgr/processmgr.h>
#include <threadmgr/fast_mutex_object.h>
#include <threadmgr/threadmgr.h>
#include <threadmgr/fast_mutex.h>
#include <stddef.h>
#include <string.h>
#include <vnz_wrapper/vnz_wrapper.h>

#include "ngs_platform.h"
#include "ngs_system.h"
#include "ngs_module_memory.h"

#define SCE_NGS_ERROR ((int)0x804a0001)
#define SCE_NGS_ERROR_OUT_OF_ASSETS ((int)0x804a0006)

NgsPlatformState ngsPlatformState;

int ngsPlatformInitialize(void)
{
	int key;

	key = sceKernelRegisterKPLS("SceLibNgs", 808);
	if (key < 0)
		return key;
	ngsPlatformState.processStorageKey = key;
	ngsPlatformState.globalMutex = sceKernelCreateMutex(
		"SceLibNgsGlobalMutex", 0x2000, 0, NULL);
	if (ngsPlatformState.globalMutex == 0)
		return SCE_NGS_ERROR;
	ngsPlatformState.sharedMemory = SceCodecEngineWrapperForDriver_2DD83BB0(1, 64);
	if (ngsPlatformState.sharedMemory == NULL)
		return SCE_NGS_ERROR_OUT_OF_ASSETS;
	memset(ngsPlatformState.sharedMemory, 0, 64);
	ngsPlatformState.sharedPhysicalAddress =
		SceCodecEngineWrapperForDriver_C7C150AE(ngsPlatformState.sharedMemory);
	return 0;
}

int ngsPlatformShutdown(void)
{
	sceKernelDeleteMutex(ngsPlatformState.globalMutex);
	ngsPlatformState.globalMutex = 0;
	if (ngsPlatformState.sharedMemory != NULL)
		SceCodecEngineWrapperForDriver_3C5E9CC3(1,
			ngsPlatformState.sharedMemory, 64);
	ngsPlatformState.sharedPhysicalAddress = 0;
	ngsPlatformState.sharedMemory = NULL;
	return 0;
}

int ngsProcessInitialize(SceUID pid)
{
	NgsProcessStorage *storage = NULL;
	int result;

	result = SceProcessmgrForDriver_AF80F39C(pid,
		ngsPlatformState.processStorageKey, (void **)&storage, 1);
	if (result >= 0) {
		memset(storage, 0, sizeof(*storage));
		storage->system = NULL;
		storage->processLock = 0;
		storage->systemId = -1;
		storage->runState = 1;
		storage->copyUserData = 1;
	}
	return result;
}

/* PLACEHOLDER: process release needs the complete NgsSynSystem/rack owners. */

int ngsProcessSuspend(SceUID pid)
{
	NgsProcessStorage *storage = NULL;
	int result;
	int retries;
	int state;

	result = SceProcessmgrForDriver_AF80F39C(pid,
		ngsPlatformState.processStorageKey, (void **)&storage, 0);
	if (result >= 0) {
		if (storage->runState != 0) {
			retries = 0;
			ngsAtomicSet(&storage->suspendRequested, 1, 0);
			for (;;) {
				do {
					state = ngsAtomicSet(&storage->runState, 0, 1);
				} while (state < 0);
				if (state <= 1) {
					ngsAtomicSet(&storage->suspendRequested, 0, 1);
					break;
				}
				if (state == 2) {
					if (retries >= 1000) {
						ngsAtomicSet(&storage->suspendRequested, 0, 1);
						break;
					}
					++retries;
					sceKernelDelayThread(1000);
				}
			}
		} else {
			result = 0;
		}
	}
	return result;
}

int ngsProcessResume(SceUID pid)
{
	NgsProcessStorage *storage = NULL;
	int result;

	result = SceProcessmgrForDriver_AF80F39C(pid,
		ngsPlatformState.processStorageKey, (void **)&storage, 0);
	if (result < 0)
		return result;
	if (storage->runState == 1)
		return 0;
	if (ngsAtomicSet(&storage->runState, 1, 0) != 0)
		return SCE_NGS_ERROR;
	return result;
}

int ngsGetSystemInstanceCount(void)
{
	return 1;
}

int ngsGetSystemExtraMemorySize(SceSize *size)
{
	*size = 0;
	return 0;
}

int ngsGetProcessStorage(NgsProcessStorage **storage)
{
	SceUID pid = sceKernelGetProcessId();
	int index;
	int result;

	if (ngsPlatformState.processes[0].pid == pid)
		index = 0;
	else if (ngsPlatformState.processes[1].pid == pid)
		index = 1;
	else if (ngsPlatformState.processes[2].pid == pid)
		index = 2;
	else if (ngsPlatformState.processes[3].pid == pid)
		index = 3;
	else if (ngsPlatformState.processes[4].pid == pid)
		index = 4;
	else if (ngsPlatformState.processes[5].pid == pid)
		index = 5;
	else if (ngsPlatformState.processes[6].pid == pid)
		index = 6;
	else if (ngsPlatformState.processes[7].pid == pid)
		index = 7;
	else {
		if (storage == NULL)
			return SCE_NGS_ERROR;
		*storage = NULL;
		result = SceProcessmgrForDriver_AF80F39C(pid,
			ngsPlatformState.processStorageKey, (void **)storage, 0);
		return (result >> 31) & SCE_NGS_ERROR;
	}
	*storage = ngsPlatformState.processes[index].storage;
	return 0;
}

int ngsCreateSystem(NgsSynSystem *system)
{
	NgsProcessStorage *storage = NULL;
	SceUID pid;
	int index = 0;
	unsigned int processCount;
	int result;
	NgsPlatformContext *context;
	struct {
		unsigned int sharedPhysicalAddress;
		unsigned int rackVoiceCounts;
		unsigned int granularity;
		unsigned int sampleRate;
	} command;

	if (system == NULL)
		return SCE_NGS_ERROR;
	pid = sceKernelGetProcessId();
	if (ngsPlatformState.processes[0].pid == pid)
		index = 0;
	else if (ngsPlatformState.processes[1].pid == pid)
		index = 1;
	else if (ngsPlatformState.processes[2].pid == pid)
		index = 2;
	else if (ngsPlatformState.processes[3].pid == pid)
		index = 3;
	else if (ngsPlatformState.processes[4].pid == pid)
		index = 4;
	else if (ngsPlatformState.processes[5].pid == pid)
		index = 5;
	else if (ngsPlatformState.processes[6].pid == pid)
		index = 6;
	else if (ngsPlatformState.processes[7].pid == pid)
		index = 7;
	else {
		storage = NULL;
		if (SceProcessmgrForDriver_AF80F39C(pid,
			ngsPlatformState.processStorageKey, (void **)&storage, 0) < 0)
			return SCE_NGS_ERROR;
		goto foundStorage;
	}
	storage = ngsPlatformState.processes[index].storage;
foundStorage:
	if (storage->systemId != -1)
		return SCE_NGS_ERROR_OUT_OF_ASSETS;
	processCount = ngsPlatformState.processCount++;
	if (ngsPlatformState.processCount > 8) {
		ngsPlatformState.processCount = processCount;
		return SCE_NGS_ERROR_OUT_OF_ASSETS;
	}
	if (ngsPlatformState.processes[0].storage == NULL &&
	    ngsPlatformState.processes[0].pid == 0)
		index = 0;
	else if (ngsPlatformState.processes[1].storage == NULL &&
		 ngsPlatformState.processes[1].pid == 0)
		index = 1;
	else if (ngsPlatformState.processes[2].storage == NULL &&
		 ngsPlatformState.processes[2].pid == 0)
		index = 2;
	else if (ngsPlatformState.processes[3].storage == NULL &&
		 ngsPlatformState.processes[3].pid == 0)
		index = 3;
	else if (ngsPlatformState.processes[4].storage == NULL &&
		 ngsPlatformState.processes[4].pid == 0)
		index = 4;
	else if (ngsPlatformState.processes[5].storage == NULL &&
		 ngsPlatformState.processes[5].pid == 0)
		index = 5;
	else if (ngsPlatformState.processes[6].storage == NULL &&
		 ngsPlatformState.processes[6].pid == 0)
		index = 6;
	else if (ngsPlatformState.processes[7].storage == NULL &&
		 ngsPlatformState.processes[7].pid == 0)
		index = 7;
	else
		return SCE_NGS_ERROR;
	ngsPlatformState.processes[index].storage = storage;
	pid = sceKernelGetProcessId();
	context = system->context;
	context->codecMutex = ngsPlatformState.globalMutex;
	ngsPlatformState.processes[index].pid = pid;
	command.rackVoiceCounts = system->MaxRacks | system->MaxVoices << 16;
	command.granularity = system->Granularity;
	command.sharedPhysicalAddress = ngsPlatformState.sharedPhysicalAddress;
	command.sampleRate = system->SampleRate;
	ngsLockCodec(context);
	result = SceCodecEngineWrapperForDriver_391BD4FD(320, (const unsigned int *)&command, NULL, NULL);
	ngsUnlockCodec(system->context);
	if (result < 0) {
		if (system->compiledSdkVersion < 0x1800000 &&
		    result == (int)0x804a0011)
			result = SCE_NGS_ERROR_OUT_OF_ASSETS;
	} else {
		system->systemId = result;
		result = sceKernelInitializeFastMutex(
			(SceKernelFastMutex *)&system->context->systemMutex,
			"SceNgsProcMutex", 0x8000, NULL);
		if (result >= 0) {
			context = system->context;
			context->systemLock = 0;
			context->paramsLock = 0;
			context->voiceLock = 0;
			context->updateLock = 0;
			context->transferLock = 0;
			context->systemHeap =
				SceCodecEngineWrapperForDriver_2DD83BB0(1, 0x40000);
			if (system->context->systemHeap != NULL) {
				memset(system->context->systemHeap, 0, 0x40000);
				storage->system = system;
				storage->systemHeap = system->context->systemHeap;
				storage->systemId = system->systemId;
				return 0;
			}
			command.sharedPhysicalAddress =
				ngsPlatformState.sharedPhysicalAddress;
			command.rackVoiceCounts = system->systemId;
			ngsLockCodec(system->context);
			SceCodecEngineWrapperForDriver_391BD4FD(322, (const unsigned int *)&command, NULL, NULL);
			ngsUnlockCodec(system->context);
			sceKernelFinalizeFastMutex(
				(SceKernelFastMutex *)&system->context->systemMutex);
		}
	}
	if (ngsPlatformState.processCount == 0)
		return result;
	--ngsPlatformState.processCount;
	if (ngsPlatformState.processes[0].storage == storage)
		index = 0;
	else if (ngsPlatformState.processes[1].storage == storage)
		index = 1;
	else if (ngsPlatformState.processes[2].storage == storage)
		index = 2;
	else if (ngsPlatformState.processes[3].storage == storage)
		index = 3;
	else if (ngsPlatformState.processes[4].storage == storage)
		index = 4;
	else if (ngsPlatformState.processes[5].storage == storage)
		index = 5;
	else if (ngsPlatformState.processes[6].storage == storage)
		index = 6;
	else if (ngsPlatformState.processes[7].storage == storage)
		index = 7;
	else
		return result;
	ngsPlatformState.processes[index].pid = 0;
	ngsPlatformState.processes[index].storage = NULL;
	return result;
}

int ngsReleaseSystem(NgsSynSystem *system)
{
	NgsProcessStorage *storage = NULL;
	SceUID pid;
	int index = 0;
	int result;
	struct {
		unsigned int sharedPhysicalAddress;
		int systemId;
	} command;

	if (system == NULL)
		return SCE_NGS_ERROR;
	pid = sceKernelGetProcessId();
	if (ngsPlatformState.processes[0].pid == pid)
		index = 0;
	else if (ngsPlatformState.processes[1].pid == pid)
		index = 1;
	else if (ngsPlatformState.processes[2].pid == pid)
		index = 2;
	else if (ngsPlatformState.processes[3].pid == pid)
		index = 3;
	else if (ngsPlatformState.processes[4].pid == pid)
		index = 4;
	else if (ngsPlatformState.processes[5].pid == pid)
		index = 5;
	else if (ngsPlatformState.processes[6].pid == pid)
		index = 6;
	else if (ngsPlatformState.processes[7].pid == pid)
		index = 7;
	else {
		storage = NULL;
		if (SceProcessmgrForDriver_AF80F39C(pid,
			ngsPlatformState.processStorageKey, (void **)&storage, 0) < 0)
			return SCE_NGS_ERROR;
		goto foundStorage;
	}
	storage = ngsPlatformState.processes[index].storage;
foundStorage:
	command.sharedPhysicalAddress = ngsPlatformState.sharedPhysicalAddress;
	command.systemId = system->systemId;
	ngsLockCodec(system->context);
	result = SceCodecEngineWrapperForDriver_391BD4FD(322, (const unsigned int *)&command, NULL, NULL);
	ngsUnlockCodec(system->context);
	if (result != 0)
		return result;
	sceKernelFinalizeFastMutex(
		(SceKernelFastMutex *)&system->context->systemMutex);
	SceCodecEngineWrapperForDriver_3C5E9CC3(1,
		system->context->systemHeap, 0x40000);
	storage->systemHeap = NULL;
	if (ngsPlatformState.processCount != 0) {
		--ngsPlatformState.processCount;
		if (ngsPlatformState.processes[0].storage == storage)
			index = 0;
		else if (ngsPlatformState.processes[1].storage == storage)
			index = 1;
		else if (ngsPlatformState.processes[2].storage == storage)
			index = 2;
		else if (ngsPlatformState.processes[3].storage == storage)
			index = 3;
		else if (ngsPlatformState.processes[4].storage == storage)
			index = 4;
		else if (ngsPlatformState.processes[5].storage == storage)
			index = 5;
		else if (ngsPlatformState.processes[6].storage == storage)
			index = 6;
		else if (ngsPlatformState.processes[7].storage == storage)
			index = 7;
		else
			goto released;
		ngsPlatformState.processes[index].pid = 0;
		ngsPlatformState.processes[index].storage = NULL;
	}
released:
	storage->systemId = -1;
	storage->system = NULL;
	return 0;
}

int ngsGetAlignedMemorySize(SceSize size, int unused, int unused2,
			int unused3, SceSize *alignedSize)
{
	*alignedSize = ngsAlignMemorySize(size);
	return 0;
}

int ngsAllocateProcessMemory(unsigned int handle, SceSize coherentSize,
	SceSize callbackSize, SceSize dspHeapSize, SceSize systemSize,
	void *unusedUserMemory, SceSize unusedUserSize, void **callbackMemory,
	void **coherentMemory, void **dspHeapMemory, void **systemMemory,
	SceUID *memBlock)
{
	NgsProcessStorage *storage = NULL;
	NgsSynSystem *system = ngsGetSystemFromHandle(handle);
	SceUID pid = sceKernelGetProcessId();
	void *coherentAllocation = NULL;
	void *dspAllocation = NULL;
	void *memBlockBase = NULL;
	SceKernelAllocMemBlockKernelOpt opt;
	unsigned int index = 0;
	unsigned int used;
	unsigned int allocationSize;

	if (ngsPlatformState.processes[0].pid == pid)
		index = 0;
	else if (ngsPlatformState.processes[1].pid == pid)
		index = 1;
	else if (ngsPlatformState.processes[2].pid == pid)
		index = 2;
	else if (ngsPlatformState.processes[3].pid == pid)
		index = 3;
	else if (ngsPlatformState.processes[4].pid == pid)
		index = 4;
	else if (ngsPlatformState.processes[5].pid == pid)
		index = 5;
	else if (ngsPlatformState.processes[6].pid == pid)
		index = 6;
	else if (ngsPlatformState.processes[7].pid == pid)
		index = 7;
	else {
		storage = NULL;
		if (SceProcessmgrForDriver_AF80F39C(pid,
			ngsPlatformState.processStorageKey, (void **)&storage, 0) < 0)
			return SCE_NGS_ERROR;
		goto foundStorage;
	}
	storage = ngsPlatformState.processes[index].storage;
foundStorage:
	*memBlock = 0;
	if (coherentSize != 0) {
		coherentAllocation = SceCodecEngineWrapperForDriver_2DD83BB0(
			1, coherentSize);
		if (coherentAllocation == NULL)
			return (int)0x804a0011;
		memset(coherentAllocation, 0, coherentSize);
		SceCpuForDriver_103872A5(coherentAllocation, coherentSize);
		for (index = 0; index < 65; ++index) {
			if (storage->allocations[index].address == NULL) {
				storage->allocations[index].address = coherentAllocation;
				storage->allocations[index].size = coherentSize;
				break;
			}
		}
	}
	if (dspHeapSize != 0) {
		if (system == NULL)
			return SCE_NGS_ERROR;
		used = system->dspHeapUsed;
		if (used > 0x40000 || dspHeapSize > 0x40000 - used)
			goto freeCoherent;
		system->dspHeapUsed = used + dspHeapSize;
		dspAllocation = (unsigned char *)system->context->systemHeap + used;
	}
	allocationSize = callbackSize | systemSize;
	if (allocationSize != 0) {
		memBlockBase = NULL;
		memset(&opt, 0, sizeof(opt));
		opt.size = sizeof(opt);
		opt.attr = 0x80;
		opt.pid = sceKernelGetProcessId();
		*memBlock = sceKernelAllocMemBlockForKernel("SceNgsBlock", 0x10c0d006,
			(callbackSize + 4095 + systemSize) >> 12 << 12, &opt);
		if (sceKernelGetMemBlockBase(*memBlock, &memBlockBase) != 0) {
			*systemMemory = NULL;
			*coherentMemory = NULL;
			*dspHeapMemory = NULL;
			system->dspHeapUsed -= dspHeapSize;
			goto freeCoherent;
		}
		for (index = 0; index < 65; ++index) {
			if (storage->memBlocks[index] == 0) {
				storage->memBlocks[index] = *memBlock;
				break;
			}
		}
	}
	*coherentMemory = coherentAllocation;
	*callbackMemory = (unsigned char *)memBlockBase + systemSize;
	*dspHeapMemory = dspAllocation;
	*systemMemory = memBlockBase;
	return 0;

freeCoherent:
	if (coherentSize != 0) {
		SceCodecEngineWrapperForDriver_3C5E9CC3(1,
			coherentAllocation, coherentSize);
		for (index = 0; index < 65; ++index) {
			if (storage->allocations[index].address == coherentAllocation) {
				storage->allocations[index].address = NULL;
				storage->allocations[index].size = 0;
				break;
			}
		}
	}
	return (int)0x804a0011;
}

void ngsRelocateRackMemory(struct NgsSynRack *rack, void *releasedAddress,
			  SceSize releasedSize);

void ngsFreeProcessMemory(unsigned int handle, void *coherentMemory,
	SceSize coherentSize, void *dspHeapMemory, SceSize dspHeapSize,
	void *systemMemory, SceSize systemSize, SceUID memBlock)
{
	NgsProcessStorage *storage = NULL;
	NgsSynSystem *system = ngsGetSystemFromHandle(handle);
	SceUID pid = sceKernelGetProcessId();
	unsigned int index;
	unsigned int used;
	unsigned char *heapEnd;
	unsigned char *releasedEnd;

	if (ngsPlatformState.processes[0].pid == pid)
		index = 0;
	else if (ngsPlatformState.processes[1].pid == pid)
		index = 1;
	else if (ngsPlatformState.processes[2].pid == pid)
		index = 2;
	else if (ngsPlatformState.processes[3].pid == pid)
		index = 3;
	else if (ngsPlatformState.processes[4].pid == pid)
		index = 4;
	else if (ngsPlatformState.processes[5].pid == pid)
		index = 5;
	else if (ngsPlatformState.processes[6].pid == pid)
		index = 6;
	else if (ngsPlatformState.processes[7].pid == pid)
		index = 7;
	else {
		storage = NULL;
		if (SceProcessmgrForDriver_AF80F39C(pid,
			ngsPlatformState.processStorageKey, (void **)&storage, 0) < 0)
			return;
		goto foundStorage;
	}
	storage = ngsPlatformState.processes[index].storage;
foundStorage:
	if (coherentMemory != NULL && coherentSize != 0) {
		SceCodecEngineWrapperForDriver_3C5E9CC3(1,
			coherentMemory, coherentSize);
		for (index = 0; index < 65; ++index) {
			if (storage->allocations[index].address == coherentMemory) {
				storage->allocations[index].address = NULL;
				storage->allocations[index].size = 0;
				break;
			}
		}
	}
	if (system != NULL && dspHeapMemory != NULL && dspHeapSize != 0) {
		for (index = 0; index < system->MaxRacks; ++index) {
			if (system->racks[index] != NULL)
				ngsRelocateRackMemory(system->racks[index],
					dspHeapMemory, dspHeapSize);
		}
		used = system->dspHeapUsed;
		heapEnd = (unsigned char *)system->context->systemHeap + used;
		if (dspHeapSize != used) {
			releasedEnd = (unsigned char *)dspHeapMemory + dspHeapSize;
			if ((uintptr_t)releasedEnd < (uintptr_t)heapEnd) {
				memmove(dspHeapMemory, releasedEnd,
					heapEnd - releasedEnd);
				used = system->dspHeapUsed;
			}
		}
		system->dspHeapUsed = used - dspHeapSize;
	}
	if (systemMemory != NULL && systemSize != 0) {
		SceSysmemForDriver_009E1C61(memBlock);
		for (index = 0; index < 65; ++index) {
			if (storage->memBlocks[index] == memBlock) {
				storage->memBlocks[index] = 0;
				break;
			}
		}
	}
}

int ngsCheckContextUnlocked(NgsPlatformContext *context)
{
	ThreadmgrFastMutexInfo info;
	int result;

	if (context->updateLock != 0 || context->voiceLock != 0 ||
	    context->paramsLock != 0 || context->systemLock != 0) {
		result = (int)0x804a0008;
	} else {
		info.size = sizeof(info);
		result = sceKernelGetFastMutexInfo(
			(SceKernelFastMutex *)&context->systemMutex, &info);
		if (result >= 0)
			result = info.lockCount > 0 ? (int)0x804a0008 : 0;
	}
	return result;
}

int ngsReleaseDebugBuffer(SceNgsBufferInfo *buffer, SceUID memBlock)
{
	int result;

	if (buffer->data == NULL || buffer->size == 0)
		return SCE_NGS_ERROR;
	result = SceSysmemForDriver_009E1C61(memBlock);
	buffer->data = NULL;
	buffer->size = 0;
	return result;
}

int ngsAllocateDebugBuffer(SceNgsBufferInfo *buffer, SceUID *memBlock, int size)
{
	SceKernelAllocMemBlockKernelOpt opt;
	int result;

	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	*memBlock = sceKernelAllocMemBlockForKernel("SceNgsDbgBlk", 0x10f0d006,
		(unsigned int)(size + 4095) >> 12 << 12, &opt);
	result = sceKernelGetMemBlockBase(*memBlock, &buffer->data);
	if (result != 0) {
		buffer->data = NULL;
		size = 0;
		result = SCE_NGS_ERROR_OUT_OF_ASSETS;
	}
	buffer->size = size;
	return result;
}
