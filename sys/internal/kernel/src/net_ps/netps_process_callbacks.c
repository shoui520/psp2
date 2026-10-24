/* Shared Sony network process callbacks; unchanged original adapters. */

#include "net_ps_internal.h"

extern int gNetPsSuspendResult;
extern SceNetPsConfigSubscriber *gNetPsConfigSubscriberHead;
extern SceNetPsConfigSubscriber *gNetPsConfigSubscriberTail;
int sceNetPsManageProcessSockets(SceUID, unsigned int);
int sceNetPsDestroyProcessDumps(SceUID);
int sceNetPsUnregisterProcessNetworkState(SceUID);
int sceNetPsReleaseProcessProtocol(SceUID);
int sceNetPsInitializeProcessProtocol(SceUID);

/* Name unknown; clear the process's suspended socket state. */
int sceNetPsResumeProcessSockets(SceUID processId)
{
	sceNetPsManageProcessSockets(processId, 4);
	return 0;
}

/* Name unknown; forward the process resume event through its complete hooks. */
int sceNetPsProcessResumeHandler(SceUID processId, int event, const void *param,
				 SceNetPsProcessHooks *hooks)
{
	if (event == 0x20000 && hooks->resume)
		return hooks->resume(processId);
	return 0;
}

/* Name unknown; suspend sockets while retaining the busy-process result. */
int sceNetPsSuspendProcessSockets(SceUID processId)
{
	unsigned int result = sceNetPsManageProcessSockets(processId, 0x2000002) + 77u;

	return result ? 0 : -77;
}

/* Name unknown; save the stop result for the process manager's second phase. */
int sceNetPsProcessSuspendHandler(SceUID processId, int event, const void *param,
				  SceNetPsProcessHooks *hooks)
{
	if (event == 0x201) {
		if (hooks->suspend)
			gNetPsSuspendResult = hooks->suspend(processId);
		return 0;
	}
	if (event == 0x202)
		return gNetPsSuspendResult;
	return 0;
}

/* Name unknown; release all network resources owned by an exiting process. */
int sceNetPsCleanupProcess(SceUID processId)
{
	SceNetPsConfigSubscriber *subscriber;

	sceNetPsManageProcessSockets(processId, 0);
	sceNetPsDestroyProcessEpolls(processId);
	sceNetPsDestroyProcessDescriptors(processId);
	sceNetPsDestroyProcessDumps(processId);
	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	sceNetPsUnregisterProcessNetworkState(processId);
	sceNetPsReleaseProcessProtocol(processId);
	subscriber = gNetPsConfigSubscriberHead;
	while (subscriber && subscriber->processId != processId)
		subscriber = subscriber->next;
	if (subscriber) {
		if (subscriber->next)
			subscriber->next->prev = subscriber->prev;
		else
			gNetPsConfigSubscriberTail = subscriber->prev;
		if (subscriber->prev)
			subscriber->prev->next = subscriber->next;
		else
			gNetPsConfigSubscriberHead = subscriber->next;
		sceNetPsFree(subscriber);
	}
	sceNetPsDeleteProcess(processId);
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return 0;
}

/* Name unknown; the process-kill hook shares the normal process cleanup. */
int sceNetPsOnProcessKill(SceUID processId)
{
	return sceNetPsCleanupProcess(processId);
}

/* Name unknown; dispatch the process manager's kill callback. */
int sceNetPsProcessKillHandler(SceUID processId, const void *param,
			       SceNetPsProcessHooks *hooks)
{
	if (hooks->kill)
		return hooks->kill(processId);
	return 0;
}

/* Name unknown; the normal process exit uses the common cleanup path. */
int sceNetPsOnProcessExit(SceUID processId)
{
	return sceNetPsCleanupProcess(processId);
}

/* Name unknown; dispatch a process's normal exit callback. */
int sceNetPsProcessExitHandler(SceUID processId, const void *param,
			       SceNetPsProcessHooks *hooks)
{
	if (hooks->exit)
		return hooks->exit(processId);
	return 0;
}

/* Name unknown; initialize both layers of process-owned network state. */
int sceNetPsOnProcessCreate(SceUID processId)
{
	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	sceNetPsCreateProcess(processId);
	sceNetPsInitializeProcessProtocol(processId);
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return 0;
}

/* Name unknown; dispatch creation through the process manager's hook owner. */
int sceNetPsProcessCreateHandler(SceUID processId, const void *param,
				 SceNetPsProcessHooks *hooks)
{
	if (hooks->create)
		return hooks->create(processId);
	return 0;
}

/* Name unknown; serialize cleanup of an exiting thread's network bookkeeping. */
int sceNetPsOnThreadExit(SceUID threadId)
{
	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	sceNetPsRetireThread(threadId);
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return 0;
}

