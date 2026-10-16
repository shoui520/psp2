/*
 * SceKernelThreadMgr: the user syscall wrappers of timer.c's exports.
 */
#include <string.h>
#include "timer.h"
#include "scheduler.h"
#include "sce/cpu.h"
#include "sce/debug.h"
#include "sce/sysmem.h"
#include <psp2kern/kernel/sysmem/data_transfers.h>

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

/* name unknown */
int cancelTimerForUser(SceUID uid, uint32_t *count)
{
	SceUID guid = kscePUIDtoGUID(0, uid);

	return guid < 0 ? (int)0xc0028241u : sceKernelCancelTimer(guid, count);
}

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

int createTimerForUser(const char *name, uint32_t attributes, const uint32_t *options)
{
	char copied[0x20];
	int result = copyinUIDNameWithAttr(copied, name, attributes);

	if (result >= 0) {
		result = sceKernelCreateTimer(copied, attributes, options);
		if (result >= 0)
			result = kscePUIDOpenByGUID(currentThreadContext()->processId, result);
	}
	return result;
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

/* name unknown */
int setTimerEventForUser(SceUID uid, uint32_t type, const volatile uint64_t *interval, uint32_t repeat)
{
	SceUID guid = kscePUIDtoGUID(0, uid);

	return guid < 0 ? (int)0xc0028241u : sceKernelSetTimerEvent(guid, type, interval, repeat);
}

/* name unknown */
int getTimerEventRemainingTimeForUser(SceUID uid, uint64_t *output)
{
	SceUID guid = kscePUIDtoGUID(0, uid);

	return guid < 0 ? (int)0xc0028241u : sceKernelGetTimerEventRemainingTime(guid, output);
}

/* name unknown */
int getTimerInfoForUser(SceUID uid, ThreadmgrTimerInfo *output)
{
	SceUID guid = kscePUIDtoGUID(0, uid);
	uint32_t attributes;
	int result;

	if (guid < 0)
		return 0xc0028241u;
	result = SceThreadmgrForDriver_19F3E92A(guid, output);
	if (result >= 0) {
		output->timerId = uid;
		result = scePUIDGetAttr(0, uid, &attributes);
		if (result >= 0 && (attributes & 0x80000))
			output->attr |= TIMER_PUID_ATTR_OPENED;
	}
	return result;
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

int sceKernelOpenTimer(const char *userName)
{
	struct ThreadCB *thread = currentThreadContext();
	char name[32];
	UIDCreateOpt options = {
		.flags = UID_CREATE_OPT_ATTR | UID_CREATE_OPT_CLASS,
		.attr = TIMER_PUID_ATTR_OPENED,
	};
	int result;

	result = copyinUIDNameWithAttr(name, userName, 0x80);
	if (result < 0)
		return result;
	options.cls = s_threadmgrRuntime.manager->timerClass;
	result = SceSysmemForKernel_201E970B(0, name, &options);
	if (result >= 0) {
		uint32_t low = thread->processId;
		uint32_t high = thread->userThreadId | ((int32_t)low >> 31);
		SceUID guid = kscePUIDtoGUID(0, result);
		int wake = sceKernelSetEventWithNotifyCallback(guid, 0x100, (uint64_t)high << 32 | low, 0x100);
		SCE_DBG_ASSERT_CHECK_FUNC(wake >= 0, 3328, "sceKernelOpenTimerForUser");
	}
	return result;
}

int sceKernelCloseTimer(SceUID uid)
{
	SceKernelIntrStatus intrState = ksceKernelCpuSuspendIntr();
	ThreadmgrUidTimer *object;
	struct ThreadCB *thread;
	SceUID guid;
	uint32_t attributes;
	SceUInt32 version;
	uint32_t wake = 0;
	int result = 0xc0028241u;

	guid = kscePUIDtoGUID(0, uid);
	if (guid < 0 || ksceGUIDReferObjectWithClass(guid,
		s_threadmgrRuntime.manager->timerClass, (SceObjectBase **)&object) < 0)
		goto resume;
	if (scePUIDGetAttr(0, uid, &attributes) < 0)
		goto release;
	if (!(attributes & 0x80000)) {
		int status = SceSysrootForDriver_CAE820B2(0, &version);
		if (status < 0 || version >= 0x03100000) {
			SCE_DBG_PRINTF_FUNC(0, 3375, "sceKernelCloseTimerForUser",
				"*** error: sceKernelCloseTimerForUser(0x%08X), delete/close mismatch\n", uid);
			goto release;
		}
	}
	if (kscePUIDClose(0, uid) < 0)
		goto release;
	thread = currentThreadContext();
	uint32_t high = thread->userThreadId;
	thread = currentThreadContext();
	uint32_t low = thread->processId;
	high |= (int32_t)low >> 31;
	ksceKernelSpinlockLowLock(&object->base.event.lock);
	wake = setEventWithNotifyCallback(guid, 0x200, (uint64_t)high << 32 | low,
					  0x200, &object->base.event);
	ksceKernelSpinlockLowUnlock(&object->base.event.lock);
	result = 0;
release:
	if (ksceGUIDReleaseObject(guid) < 0)
		SCE_PANIC_FUNC(3401, "sceKernelCloseTimerForUser");
	if (wake & 1)
		switchThread(0);
resume:
	ksceKernelCpuResumeIntr(intrState);
	return result;
}

/* name unknown */
int getTimerBaseForUser(SceUID uid, uint64_t *value)
{
	int guid = kscePUIDtoGUID(0, uid);

	if (guid < 0)
		return 0xc0028241u;
	return sceKernelGetTimerBase(guid, value);
}

/* name unknown */
int getTimerTimeForUser(SceUID uid, uint64_t *value)
{
	int guid = kscePUIDtoGUID(0, uid);

	if (guid < 0)
		return 0xc0028241u;
	return sceKernelGetTimerTime(guid, value);
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

uint64_t sceKernelGetTimerBaseWide(SceUID uid)
{
	int guid;

	sceKernelApiDeclareTypeUInt64();
	guid = kscePUIDtoGUID(0, uid);
	if (guid >= 0)
		return ksceKernelGetTimerBaseWide(guid);
	SCE_DBG_PRINTF_FUNC(0, 3481, "sceKernelGetTimerBaseWideForUser",
		"*** error: sceKernelGetTimerBaseWideForUser(0x%08X), scePUIDtoGUID()=0x%08X\n", uid, guid);
	return UINT64_MAX;
}

uint64_t sceKernelGetTimerTimeWide(SceUID uid)
{
	int guid;

	sceKernelApiDeclareTypeUInt64();
	guid = kscePUIDtoGUID(0, uid);
	if (guid >= 0)
		return ksceKernelGetTimerTimeWide(guid);
	SCE_DBG_PRINTF_FUNC(0, 3522, "sceKernelGetTimerTimeWideForUser",
		"*** error: sceKernelGetTimerTimeWideForUser(0x%08X), scePUIDtoGUID()=0x%08X\n", uid, guid);
	return UINT64_MAX;
}

int sceKernelStartTimer(SceUID uid)
{
	int guid = kscePUIDtoGUID(0, uid);

	return guid < 0 ? 0xc0028241u : ksceKernelStartTimer(guid);
}

int sceKernelStopTimer(SceUID uid)
{
	int guid = kscePUIDtoGUID(0, uid);

	return guid < 0 ? 0xc0028241u : ksceKernelStopTimer(guid);
}

/* name unknown */
int setTimerTimeForUser(SceUID uid, uint64_t *userValue)
{
	uint64_t value;
	SceUInt32 version;
	int result, guid;

	result = ksceKernelCopyFromUser(&value, userValue, sizeof(value));
	if (result < 0)
		return result;
	guid = kscePUIDtoGUID(0, uid);
	if (guid < 0)
		return 0xc0028241u;
	result = sceKernelSetTimerTime(guid, &value);
	if (result >= 0 && SceSysrootForDriver_CAE820B2(0, &version) >= 0 && version >= 0x02000000)
		return ksceKernelCopyToUser(userValue, &value, sizeof(value));
	return result;
}

int _sceKernelSetTimerTime(SceUID uid, uint64_t *userValue)
{
	return setTimerTimeForUser(uid, userValue);
}

uint64_t sceKernelSetTimerTimeWide(SceUID uid, uint64_t value)
{
	int guid;

	sceKernelApiDeclareTypeUInt64();
	guid = kscePUIDtoGUID(0, uid);
	if (guid >= 0)
		return ksceKernelSetTimerTimeWide(guid, value);
	SCE_DBG_PRINTF_FUNC(0, 3587, "sceKernelSetTimerTimeWideForUser",
		"*** error: sceKernelSetTimerTimeWideForUser(0x%08X), scePUIDtoGUID()=0x%08X\n", uid, guid);
	return UINT64_MAX;
}
