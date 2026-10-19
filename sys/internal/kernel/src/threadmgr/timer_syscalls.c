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
	uint32_t *completed;
	int result, copied;

	if (!count)
		return cancelTimerForUser(uid, NULL);
	completed = __builtin_alloca(sizeof(*completed));
	*completed = 0;
	result = cancelTimerForUser(uid, completed);
	copied = sceKernelCopyToUser(count, completed, sizeof(*completed));
	return copied < 0 ? copied : result;
}

int _sceKernelCreateTimer(const char *name, uint32_t attributes,
			  const SceKernelTimerOptParam *options)
{
	SceKernelTimerOptParam *kOptions;
	int result;

	kOptions = (SceKernelTimerOptParam *)options;
	if (options) {
		kOptions = __builtin_alloca(sizeof(*kOptions));
		result = sceKernelCopyFromUser(kOptions, options, sizeof(*kOptions));
		if (result < 0)
			return result;
	}
	return createTimerForUser(name, attributes, kOptions);
}

int _sceKernelSetTimerEvent(SceUID uid, uint32_t type, const uint64_t *interval, uint32_t repeat)
{
	uint64_t *kInterval;
	int result;

	kInterval = (uint64_t *)interval;
	if (interval) {
		kInterval = __builtin_alloca(sizeof(*kInterval));
		result = sceKernelCopyFromUser(kInterval, interval, sizeof(*kInterval));
		if (result < 0)
			return result;
	}
	return setTimerEventForUser(uid, type, kInterval, repeat);
}

int _sceKernelGetTimerEventRemainingTime(SceUID uid, uint64_t *output)
{
	uint32_t *value;
	int result, copied;

	if (!output)
		return getTimerEventRemainingTimeForUser(uid, NULL);
	value = __builtin_alloca(2 * sizeof(*value));
	value[0] = 0;
	value[1] = 0;
	result = getTimerEventRemainingTimeForUser(uid, (uint64_t *)value);
	copied = sceKernelCopyToUser(output, value, 2 * sizeof(*value));
	return copied < 0 ? copied : result;
}

int _sceKernelGetTimerInfo(SceUID uid, uint32_t *userInfo, const uint32_t *userArgs)
{
	struct ThreadmgrTimerInfoCopyArgs args;
	SceKernelTimerInfo *info;
	SceSize size;
	int result, copied;

	result = sceKernelCopyFromUser(&args, userArgs, sizeof(args));
	if (result < 0)
		return result;
	if (!userInfo)
		return getTimerInfoForUser(uid, NULL);
	size = args.size;
	if (size > sizeof(*info))
		return 0x8002710bu;
	info = __builtin_alloca(sizeof(*info));
	memset(info, 0, sizeof(*info));
	result = sceKernelCopyFromUser(info, userInfo, size);
	if (result < 0)
		return result;
	result = getTimerInfoForUser(uid, info);
	copied = sceKernelCopyToUser(userInfo, info, args.size);
	return copied < 0 ? copied : result;
}

int _sceKernelGetTimerBase(SceUID uid, uint64_t *userValue)
{
	uint32_t *value;
	int result, copied;

	if (!userValue)
		return getTimerBaseForUser(uid, NULL);
	value = __builtin_alloca(2 * sizeof(*value));
	value[0] = 0;
	value[1] = 0;
	result = getTimerBaseForUser(uid, (uint64_t *)value);
	copied = sceKernelCopyToUser(userValue, value, 2 * sizeof(*value));
	return copied < 0 ? copied : result;
}

int _sceKernelGetTimerTime(SceUID uid, uint64_t *userValue)
{
	uint32_t *value;
	int result, copied;

	if (!userValue)
		return getTimerTimeForUser(uid, NULL);
	value = __builtin_alloca(2 * sizeof(*value));
	value[0] = 0;
	value[1] = 0;
	result = getTimerTimeForUser(uid, (uint64_t *)value);
	copied = sceKernelCopyToUser(userValue, value, 2 * sizeof(*value));
	return copied < 0 ? copied : result;
}

int _sceKernelSetTimerTime(SceUID uid, uint64_t *userValue)
{
	return setTimerTimeForUser(uid, userValue);
}
