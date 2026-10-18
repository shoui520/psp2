/* The event flag syscalls that take a PUID and copy through user memory. */
#line 1 "threadmgr_for_user.c"
#include <string.h>
#include "scheduler.h"
#include "timer.h"
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <sysmem/sysmem.h>
#include "event_flag.h"
#include <sysmem/uid_class.h>

int sceKernelCancelEventFlagForUser(SceUID evfId, uint32_t pattern, int *numWaitThreads);
int sceKernelGetEventFlagInfoForUser(SceUID evfId, SceKernelEventFlagInfo *info);
int sceKernelPollEventFlagForUser(SceUID evfId, uint32_t pattern, uint32_t mode,
				  uint32_t *outPattern);

int _sceKernelPollEventFlag(SceUID evfId, uint32_t pattern, uint32_t mode, uint32_t *outPattern)
{
	uint32_t *value;
	int ret, copied;

	if (outPattern == NULL)
		return sceKernelPollEventFlagForUser(evfId, pattern, mode, NULL);
	value = __builtin_alloca(sizeof(*value));
	*value = 0;
	ret = sceKernelPollEventFlagForUser(evfId, pattern, mode, value);
	copied = sceKernelCopyToUser(outPattern, value, sizeof(*value));
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
	copied = sceKernelCopyToUser(numWaitThreads, value, sizeof(*value));
	return copied < 0 ? copied : ret;
}

/* args: the user's extra arguments, the first the size of *info */
int _sceKernelGetEventFlagInfo(SceUID evfId, SceKernelEventFlagInfo *info, const uint32_t *args)
{
	SceKernelEventFlagInfo *local;
	uint32_t extra[2];
	int ret, copied;

	ret = sceKernelCopyFromUser(extra, args, sizeof(extra));
	if (ret < 0)
		return ret;
	if (info == NULL)
		return sceKernelGetEventFlagInfoForUser(evfId, NULL);
	if (extra[0] > sizeof(*local))
		return SCE_KERNEL_ERROR_NO_MEMORY;
	local = __builtin_alloca(sizeof(*local));
	*local = (SceKernelEventFlagInfo){ 0 };
	ret = sceKernelCopyFromUser(local, info, extra[0]);
	if (ret < 0)
		return ret;
	ret = sceKernelGetEventFlagInfoForUser(evfId, local);
	copied = sceKernelCopyToUser(info, local, extra[0]);
	return copied < 0 ? copied : ret;
}
