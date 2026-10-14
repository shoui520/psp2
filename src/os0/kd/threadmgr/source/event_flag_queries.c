/* The event flag syscalls that take a PUID and copy through user memory. */
#line 1 "threadmgr_for_user.c"
#include <string.h>
#include "scheduler.h"
#include "timer.h"
#include "sce/cpu.h"
#include "sce/debug.h"
#include "sce/sysmem.h"
#include "event_flag.h"

extern int scePUIDGetAttr(SceUID processId, SceUID uid, uint32_t *attributes);
extern void ksceKernelPrintfAssertLevel(SceUInt32 level, SceBool condition,
	const SceKernelDebugInfo *info, const void *lr, const char *format, ...);

int sceKernelPollEventFlagForUser(SceUID evfId, uint32_t pattern, uint32_t mode,
				  uint32_t *outPattern)
{
	SceUID guid = kscePUIDtoGUID(0, evfId);

	return guid < 0 ? (int)USER_ERROR_UNKNOWN_EVENT_FLAG_ID :
			  ksceKernelPollEventFlag(guid, pattern, mode, outPattern);
}

int sceKernelCancelEventFlagForUser(SceUID evfId, uint32_t pattern, int *numWaitThreads)
{
	SceUID guid = kscePUIDtoGUID(0, evfId);

	return guid < 0 ? (int)USER_ERROR_UNKNOWN_EVENT_FLAG_ID :
			  sceKernelCancelEventFlag(guid, pattern, numWaitThreads);
}

int sceKernelGetEventFlagInfoForUser(SceUID evfId, ThreadmgrEventFlagInfo *info)
{
	uint32_t attributes;
	int ret = kscePUIDtoGUID(0, evfId);

	if (ret < 0)
		return USER_ERROR_UNKNOWN_EVENT_FLAG_ID;
	ret = ksceKernelGetEventFlagInfo(ret, info);
	if (ret >= 0) {
		info->evfId = evfId;
		ret = scePUIDGetAttr(0, evfId, &attributes);
		if (ret >= 0 && (attributes & PUID_ATTR_OPENED))
			info->attr |= PUID_ATTR_OPENED;
	}
	return ret;
}

int sceKernelClearEventFlag(SceUID evfId, uint32_t pattern)
{
	SceUID guid = kscePUIDtoGUID(0, evfId);

	return guid < 0 ? (int)USER_ERROR_UNKNOWN_EVENT_FLAG_ID :
			  ksceKernelClearEventFlag(guid, pattern);
}

int _sceKernelPollEventFlag(SceUID evfId, uint32_t pattern, uint32_t mode, uint32_t *outPattern)
{
	uint32_t *value;
	int ret, copied;

	if (outPattern == NULL)
		return sceKernelPollEventFlagForUser(evfId, pattern, mode, NULL);
	value = __builtin_alloca(sizeof(*value));
	*value = 0;
	ret = sceKernelPollEventFlagForUser(evfId, pattern, mode, value);
	copied = ksceKernelCopyToUser(outPattern, value, sizeof(*value));
	return copied < 0 ? copied : ret;
}

int _sceKernelCancelEventFlag(SceUID evfId, uint32_t pattern, int *numWaitThreads)
{
	int *value, ret, copied;

	if (numWaitThreads == NULL)
		return sceKernelCancelEventFlagForUser(evfId, pattern, NULL);
	value = __builtin_alloca(sizeof(*value));
	*value = 0;
	ret = sceKernelCancelEventFlagForUser(evfId, pattern, value);
	copied = ksceKernelCopyToUser(numWaitThreads, value, sizeof(*value));
	return copied < 0 ? copied : ret;
}

/* args: the user's extra arguments, the first the size of *info */
int _sceKernelGetEventFlagInfo(SceUID evfId, ThreadmgrEventFlagInfo *info, const uint32_t *args)
{
	ThreadmgrEventFlagInfo *local;
	uint32_t extra[2];
	int ret, copied;

	ret = ksceKernelCopyFromUser(extra, args, sizeof(extra));
	if (ret < 0)
		return ret;
	if (info == NULL)
		return sceKernelGetEventFlagInfoForUser(evfId, NULL);
	if (extra[0] > sizeof(*local))
		return SCE_KERNEL_ERROR_NO_MEMORY;
	local = __builtin_alloca(sizeof(*local));
	*local = (ThreadmgrEventFlagInfo){ 0 };
	ret = ksceKernelCopyFromUser(local, info, extra[0]);
	if (ret < 0)
		return ret;
	ret = sceKernelGetEventFlagInfoForUser(evfId, local);
	copied = ksceKernelCopyToUser(info, local, extra[0]);
	return copied < 0 ? copied : ret;
}
