/*
 * The syscalls libult uses to start and stop the kernel's support.
 */
#include <psp2kern/kernel/sysmem.h>
#include <psp2kern/kernel/processmgr.h>
#include <intrmgr/syscall.h>
#include "ulobjmgr_internal.h"
#include <threadmgr/threadmgr.h>

static inline __attribute__((always_inline))
int sceUlobjMgrStartSupportingUserlevelObjectImpl(SceUID pid, SceUInt32 numEntries,
						  SceUInt32 sdkVersion, void **pUserBase)
{
	UlobjProcCB *procCB;
	SceKernelIntrStatus lockState;
	int ret;

	SCE_DBG_ASSERT(pUserBase != NULL, 40);
	ret = allocateAndLockUlobjInfoArray(pid, &procCB, &lockState);
	if (ret != 0)
		return ret;
	ret = allocateInfoArray(procCB, numEntries, sdkVersion, pUserBase);
	if (ret != 0) {
		freeAndUnlockUlobjInfoArrayImpl(procCB, lockState);
		return ret;
	}
	initializeInfoArray(procCB, numEntries, sdkVersion);
	unlockUlobjInfoArrayImpl(procCB, lockState);
	return 0;
}

int _sceUlobjMgrStartSupportingUserlevelObject(SceUInt32 numEntries, SceUInt32 sdkVersion,
					       void **pUserBase)
{
	UlobjProcCB *procCB;
	SceKernelIntrStatus lockState;
	void *userBase;
	SceUInt32 state;
	SceUID pid;
	int ret;

	state = sceSyscallEnter();
	pid = sceKernelGetProcessId();
	if (sdkVersion > ULOBJMGR_SDK_VERSION_MAX) {
		sceSyscallLeave(state);
		return SCE_KERNEL_ERROR_UNSUP;
	}
	if (numEntries > ULOBJMGR_MAX_OBJECTS) {
		sceSyscallLeave(state);
		return SCE_KERNEL_ERROR_NO_MEMORY;
	}
	ret = sceUlobjMgrStartSupportingUserlevelObjectImpl(pid, numEntries, sdkVersion,
							    &userBase);
	if (ret != 0)
		goto error;
	ret = ksceKernelCopyToUser(pUserBase, &userBase, sizeof(userBase));
	if (ret != 0) {
		procCB = getAndLockUlobjInfoArray(pid, &lockState);
		if (procCB != NULL) {
			freeInfoArray(procCB);
			freeAndUnlockUlobjInfoArrayImpl(procCB, lockState);
		}
		SCE_DBG_ASSERT(procCB != NULL, 128);
		goto error;
	}
	sceSyscallLeave(state);
	return 0;

error:
	sceSyscallLeave(state);
	return ret & ~0x40000000;
}

int _sceUlobjMgrStopSupportingUserlevelObject(void)
{
	UlobjProcCB *procCB;
	SceKernelIntrStatus lockState;
	SceUInt32 state;
	int ret;

	state = sceSyscallEnter();
	procCB = getAndLockUlobjInfoArray(sceKernelGetProcessId(), &lockState);
	if (procCB == NULL) {
		ret = SCE_KERNEL_ERROR_INVALID_PID;
	} else {
		freeInfoArray(procCB);
		freeAndUnlockUlobjInfoArrayImpl(procCB, lockState);
		ret = 0;
	}
	sceSyscallLeave(state);
	return ret;
}

int _sceUlobjMgrRegisterLibultProtocolRevision(SceUInt32 revision)
{
	SceUInt32 state;
	int ret;

	state = sceSyscallEnter();
	if (revision >= ULOBJMGR_PROTOCOL_MIN && (revision & 0xffff) != 0)
		ret = 0;
	else
		ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	sceSyscallLeave(state);
	return ret;
}
