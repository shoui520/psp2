/*
 * SceKernelThreadMgr: the user syscall wrappers of event.c's exports.
 * PLACEHOLDER: these belong with the syscall files.
 */
#include "event.h"
#include <sysmem/cpu.h>
#include <sysmem/sysmem.h>

int sceKernelSetEventWithNotifyCallbackForUser(SceUID eventId, uint32_t pattern,
					       uint32_t userDataLow, uint32_t userDataHigh,
					       uint32_t arg);

int _sceKernelSetEventWithNotifyCallback(SceUID eventId, uint32_t pattern, const uint32_t *args)
{
	uint32_t values[4];
	int ret;

	ret = SceSysmemForDriver_BC996A7A(values, args, sizeof(values));
	if (ret < 0)
		return ret;
	return sceKernelSetEventWithNotifyCallbackForUser(eventId, pattern, values[0], values[1],
							  values[2]);
}
