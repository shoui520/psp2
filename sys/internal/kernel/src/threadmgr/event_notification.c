/*
 * SceKernelThreadMgr: the user syscall wrappers of event.c's exports.
 * PLACEHOLDER: these belong with the syscall files.
 */
#include "event.h"
#include "sce/cpu.h"
#include "sce/sysmem.h"
#include <psp2kern/kernel/sysmem/data_transfers.h>
#include <psp2kern/kernel/sysmem/uid_puid.h>

int sceKernelSetEventWithNotifyCallbackForUser(SceUID eventId, uint32_t pattern,
					       uint32_t userDataLow, uint32_t userDataHigh,
					       uint32_t arg);

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
