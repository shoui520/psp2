#ifndef SCE_NETPS_PROCESS_H
#define SCE_NETPS_PROCESS_H

#include <scetypes.h>

/* A configuration query records each process until the next link change. */
typedef struct SceNetPsConfigSubscriber {
	struct SceNetPsConfigSubscriber *next;
	struct SceNetPsConfigSubscriber *prev;
	SceUID processId;
} SceNetPsConfigSubscriber;

typedef struct SceNetPsProcessHooks {
	int (*create)(SceUID processId);
	int (*exit)(SceUID processId);
	int (*kill)(SceUID processId);
	int (*suspend)(SceUID processId);
	int (*resume)(SceUID processId);
	SceUID softThread;
} SceNetPsProcessHooks;

_Static_assert(sizeof(SceNetPsConfigSubscriber) == 12,
	       "SceNetPsConfigSubscriber size");
_Static_assert(sizeof(SceNetPsProcessHooks) == 24, "SceNetPsProcessHooks size");

/* Network-owned bookkeeping, separate from the kernel process object. */
typedef struct SceNetPsResourceCounts {
	int counts[3];
	int peakCounts[3];
	unsigned int flags;
} SceNetPsResourceCounts;

typedef struct SceNetPsProcess {
	struct SceNetPsProcess *next;
	struct SceNetPsProcess *prev;
	SceUID processId;
	int policy;
	int error;
	SceNetPsResourceCounts resources;
	void *protocolContext;
} SceNetPsProcess;

_Static_assert(sizeof(SceNetPsProcess) == 52, "SceNetPsProcess size");

extern SceNetPsProcess *gNetPsProcessHead;
extern SceNetPsProcess *gNetPsProcessTail;

int sceNetPsGetProcessPolicy(SceUID processId, int *policy);
int sceNetPsSetProcessPolicy(SceUID processId, int policy);
int sceNetPsGetProcessError(SceUID processId, int *error);
int sceNetPsSetProcessError(SceUID processId, int error);
SceNetPsResourceCounts *sceNetPsGetProcessResourceCounts(void);
void *sceNetPsGetNextProtocolContext(void *context);
void *sceNetPsGetProtocolContext(SceUID processId);
int sceNetPsSetProtocolContext(SceUID processId, void *context);
int sceNetPsCreateProcess(SceUID processId);
int sceNetPsDeleteProcess(SceUID processId);

int sceNetPsResumeProcessSockets(SceUID processId);
int sceNetPsProcessResumeHandler(SceUID processId, int event, const void *param,
				 SceNetPsProcessHooks *hooks);
int sceNetPsSuspendProcessSockets(SceUID processId);
int sceNetPsProcessSuspendHandler(SceUID processId, int event, const void *param,
				  SceNetPsProcessHooks *hooks);
int sceNetPsCleanupProcess(SceUID processId);
int sceNetPsOnProcessKill(SceUID processId);
int sceNetPsProcessKillHandler(SceUID processId, const void *param,
			       SceNetPsProcessHooks *hooks);
int sceNetPsOnProcessExit(SceUID processId);
int sceNetPsProcessExitHandler(SceUID processId, const void *param,
			       SceNetPsProcessHooks *hooks);
int sceNetPsOnProcessCreate(SceUID processId);
int sceNetPsProcessCreateHandler(SceUID processId, const void *param,
				 SceNetPsProcessHooks *hooks);
int sceNetPsOnThreadExit(SceUID threadId);

#endif
