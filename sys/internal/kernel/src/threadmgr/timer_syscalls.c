/*
 * SceKernelThreadMgr: the user syscall wrappers of timer.c's exports.
 */
#include <string.h>
#include "timer.h"
#include "scheduler.h"
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <sysmem/sysmem.h>
#include <sysmem/sysroot.h>
#include <sysmem/uid_class.h>
#include <threadmgr/threadmgr.h>

#undef __FILE_NAME__
#define __FILE_NAME__ "threadmgr_for_user.c"

/* Copy dimensions supplied alongside the user timer-info buffer. */
struct ThreadmgrTimerInfoCopyArgs {
	SceSize size;
	uint32_t reserved;
};

_Static_assert(sizeof(struct ThreadmgrTimerInfoCopyArgs) == 8,
	       "timer info copy arguments size");

int _sceKernelCancelTimer(SceUID uid, uint32_t *count)
{
	uint32_t completed = 0;
	int result, copied;

	if (!count)
		return cancelTimerForUser(uid, NULL);
	result = cancelTimerForUser(uid, &completed);
	copied = sceKernelCopyToUser(count, &completed, 4);
	return copied < 0 ? copied : result;
}

int _sceKernelCreateTimer(const char *name, uint32_t attributes, const uint32_t *options)
{
	uint32_t copied;
	int result;

	if (options) {
		result = sceKernelCopyFromUser(&copied, options, 4);
		if (result < 0)
			return result;
		options = &copied;
	}
	return createTimerForUser(name, attributes, options);
}

int _sceKernelSetTimerEvent(SceUID uid, uint32_t type, const uint64_t *interval, uint32_t repeat)
{
	uint64_t value;
	int result;

	if (!interval)
		return setTimerEventForUser(uid, type, NULL, repeat);
	result = sceKernelCopyFromUser(&value, interval, 8);
	return result < 0 ? result : setTimerEventForUser(uid, type, &value, repeat);
}

int _sceKernelGetTimerEventRemainingTime(SceUID uid, uint64_t *output)
{
	uint64_t value = 0;
	int result, copied;

	if (!output)
		return getTimerEventRemainingTimeForUser(uid, NULL);
	result = getTimerEventRemainingTimeForUser(uid, &value);
	copied = sceKernelCopyToUser(output, &value, 8);
	return copied < 0 ? copied : result;
}

int _sceKernelGetTimerInfo(SceUID uid, uint32_t *output, const uint32_t *options)
{
	struct ThreadmgrTimerInfoCopyArgs args;
	SceKernelTimerInfo info;
	int result, copied;

	result = sceKernelCopyFromUser(&args, options, sizeof(args));
	if (result < 0)
		return result;
	if (!output)
		return getTimerInfoForUser(uid, NULL);
	if (args.size > sizeof(info))
		return 0x8002710bu;
	memset(&info, 0, sizeof(info));
	result = sceKernelCopyFromUser(&info, output, args.size);
	if (result < 0)
		return result;
	result = getTimerInfoForUser(uid, &info);
	copied = sceKernelCopyToUser(output, &info, args.size);
	return copied < 0 ? copied : result;
}

int _sceKernelGetTimerBase(SceUID uid, uint64_t *userValue)
{
	uint64_t value = 0;
	int result, copied;

	if (!userValue)
		return getTimerBaseForUser(uid, NULL);
	result = getTimerBaseForUser(uid, &value);
	copied = sceKernelCopyToUser(userValue, &value, sizeof(value));
	return copied < 0 ? copied : result;
}

int _sceKernelGetTimerTime(SceUID uid, uint64_t *userValue)
{
	uint64_t value = 0;
	int result, copied;

	if (!userValue)
		return getTimerTimeForUser(uid, NULL);
	result = getTimerTimeForUser(uid, &value);
	copied = sceKernelCopyToUser(userValue, &value, sizeof(value));
	return copied < 0 ? copied : result;
}

int _sceKernelSetTimerTime(SceUID uid, uint64_t *userValue)
{
	return setTimerTimeForUser(uid, userValue);
}
