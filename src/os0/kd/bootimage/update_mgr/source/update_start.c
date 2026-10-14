#include <psp2kern/kernel/threadmgr/thread.h>
#include <psp2kern/kernel/proc_event.h>
#include "update_internal.h"

int updateInitializeAsync = 1;
int updateMotionPollCount = 40;
int updateTouchPollCount = 50;
int pupSmId = -1;
uint32_t updateRequestIdCounter = 1;
SceUID updateHeapId = -1;
int slskSmId = -1;

int UpdateMgrProcessCreate(SceUID pid, SceProcEventInvokeParam2 *param, int flags)
{
	return 0;
}

int UpdateMgrProcessStart(SceUID pid, int eventType,
			 SceProcEventInvokeParam1 *param, int flags)
{
	return 0;
}

int invalidate_prev_system_info(SceSize arglen, void *argp)
{
	unsigned int version = 0;
	int result;

	result = SpkgInfoUtilGetVersion(9, 32, &version, 1, -1);
	if (result == 0 && version != (unsigned int)-1) {
		version = (unsigned int)-1;
		result = SpkgInfoUtilSetVersion(9, 32, version, 1, -1);
	}
	if (updateInitializeAsync)
		result = sceKernelExitDeleteThread(result);
	return result;
}

int UpdateMgrProcessExit(SceUID pid, SceProcEventInvokeParam1 *param, int flags)
{
	return UpdateUtilReleaseAllBuffers(pid);
}

int UpdateMgrProcessKill(SceUID pid, SceProcEventInvokeParam1 *param, int flags)
{
	return UpdateUtilReleaseAllBuffers(pid);
}
