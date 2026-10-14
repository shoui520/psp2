#include <psp2kern/kernel/suspend.h>
#include "update_internal.h"

int UpdateMgrProcessStop(SceUID pid, int eventType,
			SceProcEventInvokeParam1 *param, int flags)
{
	UpdateUtilNodeStatus status;
	int idle;
	int node;
	int result = 0;

	if (eventType == 0x202) {
		idle = 1;
		for (node = 1; node != 4; node++) {
			if (UpdateUtilGetNodeStatus(node, &status, 0) == 0 && status.progress != 5)
				idle = 0;
		}
		result = (int)0x800f0229;
		if (idle)
			result = 0;
	}
	return result;
}

int UpdateMgrSystemEvent(int resume, int eventId, void *args, void *opt)
{
	UpdateUtilNodeStatus status;
	int idle = 1;
	int node;

	if (resume)
		return 0;
	for (node = 1; node != 4; node++) {
		if (UpdateUtilGetNodeStatus(node, &status, 0) == 0 && status.progress != 5)
			idle = 0;
	}
	return idle ? 0 : (int)0x800f0229;
}

int UpdateMgrRegisterEventHandlers(void)
{
	SceProcEventHandler handler;
	int result;

	handler.size = sizeof(handler);
	handler.create = UpdateMgrProcessCreate;
	handler.exit = UpdateMgrProcessExit;
	handler.kill = UpdateMgrProcessKill;
	handler.stop = UpdateMgrProcessStop;
	handler.start = UpdateMgrProcessStart;
	handler.switch_process = 0;
	result = ksceKernelRegisterProcEventHandler("SceSblUpdateMgr", &handler, 0);
	if (result >= 0) {
		result = ksceKernelRegisterSysEventHandler("SceSblUpdateMgr",
							UpdateMgrSystemEvent, 0);
		if (result >= 0)
			result = 0;
	}
	return result;
}
