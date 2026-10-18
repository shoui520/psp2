/* processor.c */
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

unsigned int * _sceCodecEngineGetProcessor(SceUID pid);

extern int setVeneziaClockSelector(unsigned int selector);
CodecEngineProcessorState g_codecEngineProcessorState;

int initializeCodecEngineProcessor(void)
{
	return sceKernelInitializeFastMutex(&g_codecEngineProcessorState.mutex,
		"SceCodecEngineProcessor", 0x8000, 0);
}

int _sceCodecEngineProcessorProcessSuspend(SceUID pid)
{
	unsigned int *processor;
	int ret;

	ret = lockFastMutex(&g_codecEngineProcessorState.mutex);
	if (ret >= 0) {
		processor = _sceCodecEngineGetProcessor(pid);
#if FW_DEBUG
		SceDebugForDriver_35A35322(processor != NULL, SCE_DBGINFO_FUNC(70, "_sceCodecEngineProcessorProcessSuspend"), __builtin_return_address(0));
#endif
		if (*processor) {
#if FW_DEBUG
			SceDebugForDriver_35A35322(*processor <= g_codecEngineProcessorState.processorCount,
				SCE_DBGINFO_FUNC(76, "_sceCodecEngineProcessorProcessSuspend"), __builtin_return_address(0));
#endif
			if (g_codecEngineProcessorState.processorCount == *processor) {
				ret = setVeneziaClockSelector(6);
				if (ret < 0)
					goto unlock;
			}
			g_codecEngineProcessorState.processorCount -= *processor;
		}
unlock:
		sceKernelUnlockFastMutex(&g_codecEngineProcessorState.mutex);
	}
	return ret;
}
int _sceCodecEngineProcessorProcessResume(SceUID pid)
{
	unsigned int *processor;
	int ret;

	ret = lockFastMutex(&g_codecEngineProcessorState.mutex);
	if (ret >= 0) {
		processor = _sceCodecEngineGetProcessor(pid);
#if FW_DEBUG
		SceDebugForDriver_35A35322(processor != NULL, SCE_DBGINFO_FUNC(112, "_sceCodecEngineProcessorProcessResume"), __builtin_return_address(0));
#endif
		if (*processor) {
			if (g_codecEngineProcessorState.processorCount == 0) {
				ret = setVeneziaClockSelector(7);
				if (ret < 0)
					goto unlock;
			}
			g_codecEngineProcessorState.processorCount += *processor;
		}
unlock:
		sceKernelUnlockFastMutex(&g_codecEngineProcessorState.mutex);
	}
	return ret;
}

extern int SceCodecEngineWrapperForDriver_CB8703D8(void);
extern int _unlockProcessSuspendCore(void);

int sceCodecEngineChangeNumWorkerCoresDefaultForUser(void)
{
	SceUInt32 tls = sceSyscallEnter();
	unsigned int *processor;
	unsigned int count;
	int ret;

	ret = SceCodecEngineWrapperForDriver_CB8703D8();
	if (ret >= 0) {
		ret = lockFastMutex(&g_codecEngineProcessorState.mutex);
		if (ret < 0)
			goto unlockProcess;
		processor = _sceCodecEngineGetProcessor(0);
#if FW_DEBUG
		SceDebugForDriver_35A35322(processor != NULL, SCE_DBGINFO_FUNC(161, "sceCodecEngineChangeNumWorkerCoresDefaultForUser"), __builtin_return_address(0));
#endif
		if (*processor) {
#if FW_DEBUG
			SceDebugForDriver_35A35322(*processor <= g_codecEngineProcessorState.processorCount,
				SCE_DBGINFO_FUNC(167, "sceCodecEngineChangeNumWorkerCoresDefaultForUser"), __builtin_return_address(0));
#endif
			if (g_codecEngineProcessorState.processorCount == 1) {
				ret = setVeneziaClockSelector(6);
				if (ret < 0)
					goto unlock;
			}
			count = *processor;
			g_codecEngineProcessorState.processorCount--;
			*processor = count - 1;
		}
unlock:
		sceKernelUnlockFastMutex(&g_codecEngineProcessorState.mutex);
unlockProcess:
		_unlockProcessSuspendCore();
	}
	sceSyscallLeave(tls);
	return ret;
}

int sceCodecEngineChangeNumWorkerCoresMaxForUser(void)
{
	SceUInt32 tls = sceSyscallEnter();
	unsigned int *processor;
	unsigned int count;
	int ret;

	ret = SceCodecEngineWrapperForDriver_CB8703D8();
	if (ret >= 0) {
		ret = lockFastMutex(&g_codecEngineProcessorState.mutex);
		if (ret < 0)
			goto unlockProcess;
		processor = _sceCodecEngineGetProcessor(0);
#if FW_DEBUG
		SceDebugForDriver_35A35322(processor != NULL, SCE_DBGINFO_FUNC(218, "sceCodecEngineChangeNumWorkerCoresMaxForUser"), __builtin_return_address(0));
#endif
#if FW_DEBUG
		SceDebugForDriver_35A35322(*processor <= g_codecEngineProcessorState.processorCount,
			SCE_DBGINFO_FUNC(220, "sceCodecEngineChangeNumWorkerCoresMaxForUser"), __builtin_return_address(0));
#endif
		if (g_codecEngineProcessorState.processorCount == 0) {
			ret = setVeneziaClockSelector(7);
			if (ret < 0)
				goto unlock;
		}
		count = *processor;
		g_codecEngineProcessorState.processorCount++;
		*processor = count + 1;
unlock:
		sceKernelUnlockFastMutex(&g_codecEngineProcessorState.mutex);
unlockProcess:
		_unlockProcessSuspendCore();
	}
	sceSyscallLeave(tls);
	return ret;
}
