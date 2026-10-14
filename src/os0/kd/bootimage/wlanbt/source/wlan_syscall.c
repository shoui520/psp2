/* Public WLAN syscall entry points; the original has no source-name records. */
#include "wlanbt_internal.h"
#include <sce/syscall.h>

SceUID kscePUIDtoGUID(SceUID pid, SceUID puid);
SceUID sceKernelGetProcessId(void);
void sceKernelApiDeclareTypeUInt32(void);

int sceWlanSetConfiguration(int request)
{
	SceUInt32 state = sceSyscallEnter();
	int result = wlanSetConfiguration(request);

	sceSyscallLeave(state);
	return result;
}

unsigned int sceWlanGetConfiguration(void)
{
	SceUInt32 state = sceSyscallEnter();
	unsigned int result = wlanGetConfiguration();

	sceSyscallLeave(state);
	return result;
}

int sceWlanRegisterCallback(SceUID callbackId)
{
	int result = sceKernelGetProcessId();
	SceUInt32 state;

	if (result >= 0) {
		result = kscePUIDtoGUID(result, callbackId);
		if (result >= 0) {
			state = sceSyscallEnter();
			result = wlanRegisterCallback(result);
			sceSyscallLeave(state);
		}
	}
	return result;
}

int sceWlanUnregisterCallback(void)
{
	SceUInt32 state = sceSyscallEnter();
	int result = wlanUnregisterCallback();

	sceSyscallLeave(state);
	return result;
}

int sceWlanGetWakeOnTargetProcess(void)
{
	SceUInt32 state = sceSyscallEnter();
	int result = wlanGetWakeOnTargetProcess();

	if (result == -1)
		sceKernelApiDeclareTypeUInt32();
	sceSyscallLeave(state);
	return result;
}
