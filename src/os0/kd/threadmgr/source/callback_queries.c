/* Callback user syscalls: PUID to GUID, then the kernel export. */
#include "private.h"
#include "sce/sysmem.h"

extern SceUID kscePUIDtoGUID(SceUID pid, SceUID puid);
int ksceKernelGetCallbackCount(SceUID uid);
int ksceKernelCancelCallback(SceUID uid);

int sceKernelGetCallbackCount(SceUID uid)
{
	SceUID guid = kscePUIDtoGUID(0, uid);

	if (guid < 0)
		return SCE_KERNEL_ERROR_UNKNOWN_CALLBACK_ID | 0x40000000;
	return ksceKernelGetCallbackCount(guid);
}

int sceKernelCancelCallback(SceUID uid)
{
	SceUID guid = kscePUIDtoGUID(0, uid);

	if (guid < 0)
		return SCE_KERNEL_ERROR_UNKNOWN_CALLBACK_ID | 0x40000000;
	return ksceKernelCancelCallback(guid);
}
