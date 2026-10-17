/*
 * SceKernelThreadMgr: the user syscall wrappers of timer.c's exports.
 */
#include <string.h>
#include "timer.h"
#include "scheduler.h"
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <sysmem/sysmem.h>
#include <psp2kern/kernel/sysmem/data_transfers.h>

int cancelTimerForUser(SceUID uid, uint32_t *count);
int createTimerForUser(const char *name, uint32_t attributes, const uint32_t *options);
int getTimerBaseForUser(SceUID uid, uint64_t *value);
int getTimerEventRemainingTimeForUser(SceUID uid, uint64_t *output);
int getTimerInfoForUser(SceUID uid, ThreadmgrTimerInfo *output);
int getTimerTimeForUser(SceUID uid, uint64_t *value);
int setTimerEventForUser(SceUID uid, uint32_t type, const volatile uint64_t *interval, uint32_t repeat);
int setTimerTimeForUser(SceUID uid, uint64_t *userValue);

#undef __FILE_NAME__
#define __FILE_NAME__ "threadmgr_for_user.c"

#define TIMER_PUID_ATTR_OPENED		0x80000

/* Copy dimensions supplied alongside the user timer-info buffer. */
struct ThreadmgrTimerInfoCopyArgs {
	SceSize size;
	uint32_t reserved;
};

_Static_assert(sizeof(struct ThreadmgrTimerInfoCopyArgs) == 8,
	       "timer info copy arguments size");

extern void sceKernelApiDeclareTypeUInt64(void);
extern int scePUIDGetAttr(SceUID processId, SceUID uid, uint32_t *attributes);
extern int SceSysrootForDriver_CAE820B2(SceUID processId, SceUInt32 *version);
extern int copyinUIDNameWithAttr(char *name, const char *userName, uint32_t attributes);
extern int ksceGUIDClose(SceUID uid);
extern SceUID kscePUIDOpenByGUID(SceUID pid, SceUID guid);
extern void ksceKernelPrintfAssertLevel(SceUInt32 level, SceBool condition,
	const SceKernelDebugInfo *info, const void *lr, const char *format, ...);
SceUID sceKernelCreateTimer(const char *name, SceUInt32 attr, const void *option);
int ksceKernelStartTimer(SceUID uid);
int ksceKernelStopTimer(SceUID uid);
int sceKernelGetTimerBase(SceUID uid, uint64_t *value);
int sceKernelGetTimerTime(SceUID uid, uint64_t *value);
uint64_t ksceKernelGetTimerBaseWide(SceUID uid);
uint64_t ksceKernelGetTimerTimeWide(SceUID uid);
int sceKernelSetTimerTime(SceUID uid, uint64_t *value);
uint64_t ksceKernelSetTimerTimeWide(SceUID uid, uint64_t value);
int sceKernelSetTimerEvent(SceUID uid, uint32_t type, const volatile uint64_t *interval, int repeat);
int sceKernelCancelTimer(SceUID uid, uint32_t *count);
int sceKernelGetTimerEventRemainingTime(SceUID uid, uint64_t *remaining);

int _sceKernelCancelTimer(SceUID uid, uint32_t *count)
{
	uint32_t completed = 0;
	int result, copied;

	if (!count)
		return cancelTimerForUser(uid, NULL);
	result = cancelTimerForUser(uid, &completed);
	copied = ksceKernelCopyToUser(count, &completed, 4);
	return copied < 0 ? copied : result;
}

int _sceKernelCreateTimer(const char *name, uint32_t attributes, const uint32_t *options)
{
	uint32_t copied;
	int result;

	if (options) {
		result = ksceKernelCopyFromUser(&copied, options, 4);
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
	result = ksceKernelCopyFromUser(&value, interval, 8);
	return result < 0 ? result : setTimerEventForUser(uid, type, &value, repeat);
}

int _sceKernelGetTimerEventRemainingTime(SceUID uid, uint64_t *output)
{
	uint64_t value = 0;
	int result, copied;

	if (!output)
		return getTimerEventRemainingTimeForUser(uid, NULL);
	result = getTimerEventRemainingTimeForUser(uid, &value);
	copied = ksceKernelCopyToUser(output, &value, 8);
	return copied < 0 ? copied : result;
}

int _sceKernelGetTimerInfo(SceUID uid, uint32_t *output, const uint32_t *options)
{
	struct ThreadmgrTimerInfoCopyArgs args;
	ThreadmgrTimerInfo info;
	int result, copied;

	result = ksceKernelCopyFromUser(&args, options, sizeof(args));
	if (result < 0)
		return result;
	if (!output)
		return getTimerInfoForUser(uid, NULL);
	if (args.size > sizeof(info))
		return 0x8002710bu;
	memset(&info, 0, sizeof(info));
	result = ksceKernelCopyFromUser(&info, output, args.size);
	if (result < 0)
		return result;
	result = getTimerInfoForUser(uid, &info);
	copied = ksceKernelCopyToUser(output, &info, args.size);
	return copied < 0 ? copied : result;
}

int _sceKernelGetTimerBase(SceUID uid, uint64_t *userValue)
{
	uint64_t value = 0;
	int result, copied;

	if (!userValue)
		return getTimerBaseForUser(uid, NULL);
	result = getTimerBaseForUser(uid, &value);
	copied = ksceKernelCopyToUser(userValue, &value, sizeof(value));
	return copied < 0 ? copied : result;
}

int _sceKernelGetTimerTime(SceUID uid, uint64_t *userValue)
{
	uint64_t value = 0;
	int result, copied;

	if (!userValue)
		return getTimerTimeForUser(uid, NULL);
	result = getTimerTimeForUser(uid, &value);
	copied = ksceKernelCopyToUser(userValue, &value, sizeof(value));
	return copied < 0 ? copied : result;
}

int _sceKernelSetTimerTime(SceUID uid, uint64_t *userValue)
{
	return setTimerTimeForUser(uid, userValue);
}
