/* codecengine.c */
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <sysmem/sysmem.h>
#include <scetypes.h>
#include <sysmem/cpu.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/debug.h>
#include <lowio/pervasive_regs.h>
#include <lowio/mailbox_regs.h>
#include <intrmgr/intrmgr.h>
#include <processmgr/processmgr.h>
#include <intrmgr/syscall.h>
#include <dmacmgr/dmacmgr.h>
#include "../venezia_internal.h"
#include <vnz_wrapper/venezia_debug.h>
#include <string.h>
#include <psp2kern/kernel/processmgr.h>

int _sceCodecEngineProcessorProcessResume(SceUID pid);
int _sceCodecEngineProcessorProcessSuspend(SceUID pid);

extern int initializeCodecEngineProcessor(void);
extern int initializeCodecEngineMemoryClass(void);
int g_codecEngineProcessStorageKey;

/* name unknown: allocate process storage and initialize the processor/memory owners. */
int initCodecEngine(void)
{
	int ret;

	g_codecEngineProcessStorageKey = -1;
	ret = SceProcessmgrForDriver_3801D7D6("SceCodecEngine", sizeof(CodecEngineProcessStorage));
	if (ret < 0)
		return ret;
	g_codecEngineProcessStorageKey = ret;
	ret = initializeCodecEngineProcessor();
	if (ret < 0)
		return ret;
	ret = initializeCodecEngineMemoryClass();
	return ret;
}

int _sceCodecEngineProcessCreateEntry(SceUID pid, SceUInt32 type)
{
	CodecEngineProcessStorage *storage = NULL;
	int ret;

	ret = SceProcessmgrForDriver_AF80F39C(pid, g_codecEngineProcessStorageKey,
		(void **)&storage, 1);
	if (ret >= 0) {
#if FW_DEBUG
		SceDebugForDriver_35A35322(storage != NULL, SCE_DBGINFO_FUNC(100, "_sceCodecEngineProcessCreateEntry"), __builtin_return_address(0));
#endif
		memset(storage, 0, sizeof(*storage));
		storage->unmapMemBlocks[0].puid = -1;
		storage->unmapMemBlocks[0].guid = -1;
		storage->unmapMemBlocks[1].puid = -1;
		storage->unmapMemBlocks[1].guid = -1;
		storage->unmapMemBlocks[2].puid = -1;
		storage->unmapMemBlocks[2].guid = -1;
		storage->unmapMemBlocks[3].puid = -1;
		storage->unmapMemBlocks[3].guid = -1;
		ret = sceKernelInitializeFastMutex(&storage->mutex, "SceCodecEngine", 0x8000, 0);
	}
	return ret;
}

extern int _closeUnmapMemBlockAll(SceUID pid);
int _sceCodecEngineProcessExitEntry(SceUID pid, SceUInt32 type)
{
	CodecEngineProcessStorage *storage = NULL;
	int storageResult, ret, finalResult;

	storageResult = SceProcessmgrForDriver_AF80F39C(pid,
		g_codecEngineProcessStorageKey, (void **)&storage, 0);
#if FW_DEBUG
	SceDebugForDriver_35A35322(storage != NULL, SCE_DBGINFO_FUNC(139, "_sceCodecEngineProcessExitEntry"), __builtin_return_address(0));
#endif
	ret = _closeUnmapMemBlockAll(pid);
	if (ret >= 0)
		ret = storageResult & (storageResult >> 31);
	finalResult = sceKernelFinalizeFastMutex(&storage->mutex);
	if (finalResult >= 0)
		finalResult = ret;
	else
		finalResult &= finalResult >> 31;
	return finalResult;
}

typedef int (*CodecEngineProcExitHandler)(SceUID pid, ProcEventParam *param, int flags);
CodecEngineProcExitHandler g_codecEngineProcExitHandlers[4];
CodecEngineProcExitHandler g_codecEngineProcKillHandlers[4];

CodecEngineProcExitHandler * getProcExitHandlers(void)
{
	return g_codecEngineProcExitHandlers;
}

CodecEngineProcExitHandler * getProcKillHandlers(void)
{
	return g_codecEngineProcKillHandlers;
}

int sceCodecEngineRegisterProcExitHandler(CodecEngineProcExitHandler handler)
{
	unsigned int index = 0;

#if FW_DEBUG
	SceDebugForDriver_35A35322(handler != NULL, SCE_DBGINFO_FUNC(172, "sceCodecEngineRegisterProcExitHandler"), __builtin_return_address(0));
#endif
	if (g_codecEngineProcExitHandlers[0]) {
		if (g_codecEngineProcExitHandlers[1]) {
			if (g_codecEngineProcExitHandlers[2]) {
				if (g_codecEngineProcExitHandlers[3])
					return -2141192185;
				index = 3;
			} else {
				index = 2;
			}
		} else {
			index = 1;
		}
	}
	g_codecEngineProcExitHandlers[index] = handler;
	return 0;
}

int sceCodecEngineRegisterProcKillHandler(CodecEngineProcExitHandler handler)
{
	unsigned int index = 0;

#if FW_DEBUG
	SceDebugForDriver_35A35322(handler != NULL, SCE_DBGINFO_FUNC(196, "sceCodecEngineRegisterProcKillHandler"), __builtin_return_address(0));
#endif
	if (g_codecEngineProcKillHandlers[0]) {
		if (g_codecEngineProcKillHandlers[1]) {
			if (g_codecEngineProcKillHandlers[2]) {
				if (g_codecEngineProcKillHandlers[3])
					return -2141192185;
				index = 3;
			} else {
				index = 2;
			}
		} else {
			index = 1;
		}
	}
	g_codecEngineProcKillHandlers[index] = handler;
	return 0;
}

int _sceCodecEngineProcessSuspendPhase0(SceUID pid)
{
	CodecEngineProcessStorage *storage = NULL;
	int ret;

	ret = SceProcessmgrForDriver_AF80F39C(pid, g_codecEngineProcessStorageKey,
		(void **)&storage, 0);
	if (ret >= 0) {
#if FW_DEBUG
		SceDebugForDriver_35A35322(storage != NULL, SCE_DBGINFO_FUNC(228, "_sceCodecEngineProcessSuspendPhase0"), __builtin_return_address(0));
#endif
		ret = _sceCodecEngineProcessorProcessSuspend(pid);
	}
	return ret;
}

int _sceCodecEngineProcessResume(SceUID pid)
{
	CodecEngineProcessStorage *storage = NULL;
	int ret;

	ret = SceProcessmgrForDriver_AF80F39C(pid, g_codecEngineProcessStorageKey,
		(void **)&storage, 0);
	if (ret >= 0) {
#if FW_DEBUG
		SceDebugForDriver_35A35322(storage != NULL, SCE_DBGINFO_FUNC(253, "_sceCodecEngineProcessResume"), __builtin_return_address(0));
#endif
		ret = _sceCodecEngineProcessorProcessResume(pid);
	}
	return ret;
}

struct ThreadmgrFastMutex * _sceCodecEngineGetMutex(SceUID pid)
{
	CodecEngineProcessStorage *storage = NULL;

	if (SceProcessmgrForDriver_AF80F39C(pid, g_codecEngineProcessStorageKey,
		(void **)&storage, 0) < 0)
		return NULL;
#if FW_DEBUG
	SceDebugForDriver_35A35322(storage != NULL, SCE_DBGINFO_FUNC(279, "_sceCodecEngineGetMutex"), __builtin_return_address(0));
#endif
	return &storage->mutex;
}

CodecEngineUnmapMemBlockEntry * _sceCodecEngineGetUnmapMemBlockEntry(SceUID pid, SceUID guid)
{
	CodecEngineProcessStorage *storage = NULL;
	unsigned int index = 0;

	if (SceProcessmgrForDriver_AF80F39C(pid, g_codecEngineProcessStorageKey,
		(void **)&storage, 0) < 0)
		return NULL;
#if FW_DEBUG
	SceDebugForDriver_35A35322(storage != NULL, SCE_DBGINFO_FUNC(301, "_sceCodecEngineGetUnmapMemBlockEntry"), __builtin_return_address(0));
#endif
	if (!guid)
		return storage->unmapMemBlocks;
	if (storage->unmapMemBlocks[0].guid != guid) {
		if (storage->unmapMemBlocks[1].guid == guid)
			index = 1;
		else if (storage->unmapMemBlocks[2].guid == guid)
			index = 2;
		else if (storage->unmapMemBlocks[3].guid == guid)
			index = 3;
		else
			return NULL;
	}
	return &storage->unmapMemBlocks[index];
}

unsigned int * _sceCodecEngineGetProcessor(SceUID pid)
{
	CodecEngineProcessStorage *storage = NULL;

	if (SceProcessmgrForDriver_AF80F39C(pid, g_codecEngineProcessStorageKey,
		(void **)&storage, 0) < 0)
		return NULL;
#if FW_DEBUG
	SceDebugForDriver_35A35322(storage != NULL, SCE_DBGINFO_FUNC(336, "_sceCodecEngineGetProcessor"), __builtin_return_address(0));
#endif
	if (storage)
		return &storage->processorCount;
	return NULL;
}
