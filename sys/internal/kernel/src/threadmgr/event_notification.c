/*
 * SceKernelThreadMgr: the user syscall wrappers of event.c's exports.
 * PLACEHOLDER: these belong with the syscall files.
 */
#include "event.h"
#include "sce/cpu.h"
#include "sce/sysmem.h"
#include <psp2kern/kernel/sysmem/data_transfers.h>
#include <psp2kern/kernel/sysmem/uid_puid.h>

int sceKernelSetEvent(SceUID eventId, uint32_t pattern, uint32_t userDataLow,
		      uint32_t userDataHigh)
{
	SceUID event = kscePUIDtoGUID(0, eventId);

	if (event < 0)
		return USER_ERROR_UNKNOWN_EVENT_ID;
	if (pattern & EVENT_PATTERN_KERNEL)
		return SCE_KERNEL_ERROR_ILLEGAL_PATTERN;
	return ksceKernelSetEvent(event, pattern, (uint64_t)userDataHigh << 32 | userDataLow);
}

int sceKernelClearEvent(SceUID eventId, uint32_t keep)
{
	SceUID event = kscePUIDtoGUID(0, eventId);

	if (event < 0)
		return USER_ERROR_UNKNOWN_EVENT_ID;
	if (keep & EVENT_PATTERN_KERNEL)
		return SCE_KERNEL_ERROR_ILLEGAL_PATTERN;
	return ksceKernelClearEvent(event, keep);
}

int sceKernelSetEventWithNotifyCallbackForUser(SceUID eventId, uint32_t pattern,
					       uint32_t userDataLow, uint32_t userDataHigh,
					       uint32_t arg)
{
	SceUID event = kscePUIDtoGUID(0, eventId);

	if (event < 0)
		return USER_ERROR_UNKNOWN_EVENT_ID;
	if (pattern & EVENT_PATTERN_KERNEL)
		return SCE_KERNEL_ERROR_ILLEGAL_PATTERN;
	return sceKernelSetEventWithNotifyCallback(event, pattern,
						   (uint64_t)userDataHigh << 32 | userDataLow, arg);
}

int _sceKernelSetEventWithNotifyCallback(SceUID eventId, uint32_t pattern, const uint32_t *args)
{
	uint32_t values[4];
	int ret;

	ret = ksceKernelCopyFromUser(values, args, sizeof(values));
	if (ret < 0)
		return ret;
	return sceKernelSetEventWithNotifyCallbackForUser(eventId, pattern, values[0], values[1],
							  values[2]);
}
