/* Public WLAN syscall entry points; the original has no source-name records. */
#include "wlanbt_internal.h"
#include <intrmgr/syscall.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/sysmem.h>
#include <sysmem/sysroot.h>

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
		result = SceSysmemForDriver_45D22597(result, callbackId);
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
