/* Save-data slot user-memory copies; original source filename unavailable. */

#include <appmgr/appmgr_savedata.h>
#include <threadmgr/threadmgr.h>
#include <intrmgr/syscall_copy.h>
#include <string.h>
#include "app_state_internal.h"
#include "mount.h"
#include "appmgr_savedata_slot.h"

typedef struct AppmgrSaveDataSlotGetParamArguments {
	SceAppMgrSaveDataSlot *slot;
	const SyscallSizeArgs *userArguments;
} AppmgrSaveDataSlotGetParamArguments;

_Static_assert(sizeof(AppmgrSaveDataSlotGetParamArguments) == 8,
	       "save-data slot stack arguments");

typedef struct AppmgrSaveDataDataRemoveArguments {
	SceAppMgrSaveDataDataDelete *operation;
	const SyscallSizeArgs *userArguments;
} AppmgrSaveDataDataRemoveArguments;

_Static_assert(sizeof(AppmgrSaveDataDataRemoveArguments) == 8,
	       "save-data remove stack arguments");

int appmgrSaveDataSlotGetParamForUser(SceAppMgrSaveDataSlot *userSlot,
				    const SyscallSizeArgs *userArguments)
{
	SyscallSizeArgs arguments;
	SceAppMgrSaveDataSlot *slot;
	SceSize size;
	int ret;

	ARGS_IN(arguments, userArguments);
	if (userSlot == NULL)
		return appmgrSaveDataSlotGetParamSyscall(NULL);
	size = arguments.size;
	if (size > sizeof(*slot))
		return SCE_KERNEL_ERROR_NO_MEMORY;
	slot = __builtin_alloca(sizeof(*slot));
	__builtin_memset(slot, 0, sizeof(*slot));
	ret = sceKernelCopyFromUser(slot, userSlot, size);
	if (ret < 0)
		return ret;
	ret = appmgrSaveDataSlotGetParamSyscall(slot);
	return copyResult(ret, sceKernelCopyToUser(userSlot, slot, arguments.size));
}

int appmgrSaveDataSlotGetParamOnStack(void *arg)
{
	AppmgrSaveDataSlotGetParamArguments *arguments = arg;

	return appmgrSaveDataSlotGetParamForUser(arguments->slot,
					       arguments->userArguments);
}

int appmgrSaveDataDataRemoveForUser(SceAppMgrSaveDataDataDelete *userOperation,
				    const SyscallSizeArgs *userArguments)
{
	SyscallSizeArgs arguments;
	SceAppMgrSaveDataDataDelete *operation = userOperation;
	int ret;

	ARGS_IN(arguments, userArguments);
	BLOCK_IN_SIZED(operation, userOperation, arguments.size, sizeof(*operation));
	return appmgrSaveDataDataRemoveSyscall(operation);
}

int appmgrSaveDataDataRemoveOnStack(void *arg)
{
	AppmgrSaveDataDataRemoveArguments *arguments = arg;

	return appmgrSaveDataDataRemoveForUser(arguments->operation,
					       arguments->userArguments);
}

int appmgrSaveDataDataRemove2ForUser(SceAppMgrSaveDataDataDelete *userOperation,
				    const SyscallSizeArgs *userArguments)
{
	SyscallSizeArgs arguments;
	SceAppMgrSaveDataDataDelete *operation = userOperation;
	int ret;

	ARGS_IN(arguments, userArguments);
	BLOCK_IN_SIZED(operation, userOperation, arguments.size, sizeof(*operation));
	return appmgrSaveDataDataRemove2Syscall(operation);
}

int appmgrSaveDataDataRemove2OnStack(void *arg)
{
	AppmgrSaveDataDataRemoveArguments *arguments = arg;

	return appmgrSaveDataDataRemove2ForUser(arguments->operation,
					       arguments->userArguments);
}

int _sceAppMgrSaveDataSlotInit(void *userSlot,
			     const SyscallSizeArgs *userArguments)
{
	SyscallSizeArgs arguments;
	void *slot = NULL;
	int ret;

	ARGS_IN(arguments, userArguments);
	BLOCK_IN_SIZED(slot, userSlot, arguments.size, 60);
	return appmgrSaveDataSlotInitSyscall(slot);
}

int _sceAppMgrSaveDataSlotCreate(SceAppMgrSaveDataSlot *userSlot,
			       const SyscallSizeArgs *userArguments)
{
	SyscallSizeArgs arguments;
	SceAppMgrSaveDataSlot *slot = NULL;
	int ret;

	ARGS_IN(arguments, userArguments);
	BLOCK_IN_SIZED(slot, userSlot, arguments.size, sizeof(*slot));
	return appmgrSaveDataSlotCreateSyscall(slot);
}

int _sceAppMgrSaveDataSlotDelete(SceAppMgrSaveDataSlotDelete *userSlot,
			       const SyscallSizeArgs *userArguments)
{
	SyscallSizeArgs arguments;
	SceAppMgrSaveDataSlotDelete *slot = NULL;
	int ret;

	ARGS_IN(arguments, userArguments);
	BLOCK_IN_SIZED(slot, userSlot, arguments.size, sizeof(*slot));
	return appmgrSaveDataSlotDeleteSyscall(slot);
}

int _sceAppMgrSaveDataSlotSetParam(SceAppMgrSaveDataSlot *userSlot,
				 const SyscallSizeArgs *userArguments)
{
	SyscallSizeArgs arguments;
	SceAppMgrSaveDataSlot *slot = NULL;
	int ret;

	ARGS_IN(arguments, userArguments);
	BLOCK_IN_SIZED(slot, userSlot, arguments.size, sizeof(*slot));
	return appmgrSaveDataSlotSetParamSyscall(slot);
}

int _sceAppMgrSaveDataSlotGetParam(SceAppMgrSaveDataSlot *slot,
				 const SyscallSizeArgs *userArguments)
{
	AppmgrSaveDataSlotGetParamArguments arguments;

	arguments.userArguments = userArguments;
	arguments.slot = slot;
	return SceThreadmgrForDriver_E54FD746(8192, appmgrSaveDataSlotGetParamOnStack,
				      &arguments);
}

int _sceAppMgrSaveDataSlotSetStatus(void *userStatus,
				  const SyscallSizeArgs *userArguments)
{
	SyscallSizeArgs arguments;
	void *status = NULL;
	int ret;

	ARGS_IN(arguments, userArguments);
	BLOCK_IN_SIZED(status, userStatus, arguments.size, 28);
	return appmgrSaveDataSlotSetStatusSyscall(status);
}

int _sceAppMgrSaveDataSlotGetStatus(void *userStatus,
				  const SyscallSizeArgs *userArguments)
{
	SyscallSizeArgs arguments;
	void *status;
	SceSize size;
	int ret;

	ARGS_IN(arguments, userArguments);
	if (userStatus == NULL)
		return appmgrSaveDataSlotGetStatusSyscall(NULL);
	size = arguments.size;
	if (size > 28)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	status = __builtin_alloca(28);
	__builtin_memset(status, 0, 28);
	ret = sceKernelCopyFromUser(status, userStatus, size);
	if (ret < 0)
		return ret;
	ret = appmgrSaveDataSlotGetStatusSyscall(status);
	return copyResult(ret, sceKernelCopyToUser(userStatus, status, arguments.size));
}

int _sceAppMgrSaveDataDataSave(SceAppMgrSaveDataData *userOperation,
			       const SyscallSizeArgs *userArguments)
{
	SyscallSizeArgs arguments;
	SceAppMgrSaveDataData *operation;
	SceSize size;
	int ret;

	ARGS_IN(arguments, userArguments);
	if (userOperation == NULL)
		return appmgrSaveDataDataSaveSyscall(NULL);
	size = arguments.size;
	if (size > sizeof(*operation))
		return SCE_KERNEL_ERROR_NO_MEMORY;
	operation = __builtin_alloca(sizeof(*operation));
	__builtin_memset(operation, 0, sizeof(*operation));
	ret = sceKernelCopyFromUser(operation, userOperation, size);
	if (ret < 0)
		return ret;
	ret = appmgrSaveDataDataSaveSyscall(operation);
	return copyResult(ret, sceKernelCopyToUser(userOperation, operation, arguments.size));
}

int _sceAppMgrSaveDataDataSave2(SceAppMgrSaveDataData *userOperation,
			       const SyscallSizeArgs *userArguments)
{
	SyscallSizeArgs arguments;
	SceAppMgrSaveDataData *operation;
	SceSize size;
	int ret;

	ARGS_IN(arguments, userArguments);
	if (userOperation == NULL)
		return appmgrSaveDataDataSave2Syscall(NULL);
	size = arguments.size;
	if (size > sizeof(*operation))
		return SCE_KERNEL_ERROR_NO_MEMORY;
	operation = __builtin_alloca(sizeof(*operation));
	__builtin_memset(operation, 0, sizeof(*operation));
	ret = sceKernelCopyFromUser(operation, userOperation, size);
	if (ret < 0)
		return ret;
	ret = appmgrSaveDataDataSave2Syscall(operation);
	return copyResult(ret, sceKernelCopyToUser(userOperation, operation, arguments.size));
}

int _sceAppMgrSaveDataDataRemove(SceAppMgrSaveDataDataDelete *operation,
				 const SyscallSizeArgs *userArguments)
{
	AppmgrSaveDataDataRemoveArguments arguments;

	arguments.userArguments = userArguments;
	arguments.operation = operation;
	return SceThreadmgrForDriver_E54FD746(8192, appmgrSaveDataDataRemoveOnStack,
				      &arguments);
}

int _sceAppMgrSaveDataDataRemove2(SceAppMgrSaveDataDataDelete *operation,
				 const SyscallSizeArgs *userArguments)
{
	AppmgrSaveDataDataRemoveArguments arguments;

	arguments.userArguments = userArguments;
	arguments.operation = operation;
	return SceThreadmgrForDriver_E54FD746(8192, appmgrSaveDataDataRemove2OnStack,
				      &arguments);
}

int _sceAppMgrSaveDataGetQuota(SceAppMgrSaveDataQuota *userQuota,
			       const SyscallSizeArgs *userArguments)
{
	SyscallSizeArgs arguments;
	SceAppMgrSaveDataQuota *quota;
	SceSize size;
	int ret;

	ARGS_IN(arguments, userArguments);
	if (userQuota == NULL)
		return appmgrSaveDataGetQuotaSyscall(NULL);
	size = arguments.size;
	if (size > sizeof(*quota))
		return SCE_KERNEL_ERROR_NO_MEMORY;
	quota = __builtin_alloca(sizeof(*quota));
	__builtin_memset(quota, 0, sizeof(*quota));
	ret = sceKernelCopyFromUser(quota, userQuota, size);
	if (ret < 0)
		return ret;
	ret = appmgrSaveDataGetQuotaSyscall(quota);
	return copyResult(ret, sceKernelCopyToUser(userQuota, quota, arguments.size));
}

int _sceAppMgrSaveDataMount(const SceAppMgrSaveDataMountParam *userParam, const SyscallSizeArgs *userArguments)
{
	SyscallSizeArgs arguments;
	SceAppMgrSaveDataMountParam *param = (SceAppMgrSaveDataMountParam *)userParam;
	int ret;

	ARGS_IN(arguments, userArguments);
	BLOCK_IN_SIZED(param, userParam, arguments.size, sizeof(*param));
	return appmgrSaveDataMountSyscall(param);
}

int _sceAppMgrSaveDataUmount(const SceAppMgrSaveDataUmountParam *userParam, const SyscallSizeArgs *userArguments)
{
	SyscallSizeArgs arguments;
	SceAppMgrSaveDataUmountParam *param = (SceAppMgrSaveDataUmountParam *)userParam;
	int ret;

	ARGS_IN(arguments, userArguments);
	BLOCK_IN_SIZED(param, userParam, arguments.size, sizeof(*param));
	return appmgrSaveDataUmountSyscall(param);
}

int _sceAppMgrSaveDataSlotFileOpen(const char *userMountPoint, AppmgrSaveDataSlotFileInfo *info)
{
	void *mountPoint = (void *)userMountPoint;
	int ret;

	BLOCK_IN(mountPoint, userMountPoint, 16);
	BLOCK_OUT(info, sizeof(AppmgrSaveDataSlotFileInfo),
		  appmgrSaveDataSlotFileOpenSyscall(mountPoint, NULL),
		  appmgrSaveDataSlotFileOpenSyscall(mountPoint, (AppmgrSaveDataSlotFileInfo *)k));
}

int _sceAppMgrSaveDataSlotFileGetParam(unsigned int slotId, void *param)
{
	int ret;

	BLOCK_OUT(param, 1024,
		  appmgrSaveDataSlotFileGetParamSyscall(slotId, NULL),
		  appmgrSaveDataSlotFileGetParamSyscall(slotId, k));
}

int _sceAppMgrSaveDataSlotFileClose(const char *userMountPoint)
{
	void *mountPoint = (void *)userMountPoint;
	int ret;

	BLOCK_IN(mountPoint, userMountPoint, 16);
	return appmgrSaveDataSlotFileCloseSyscall(mountPoint);
}

int _sceAppMgrSaveDataAddMount(const AppmgrAddMountParam *userParam)
{
	void *param = (void *)userParam;
	int ret;

	BLOCK_IN(param, userParam, sizeof(AppmgrAddMountParam));
	return appmgrSaveDataAddMountSyscall(param);
}

int _sceAppMgrPhotoMount(const SceAppMgrPhotoMountParam *userParam, const SyscallSizeArgs *userArguments)
{
	SyscallSizeArgs arguments;
	SceAppMgrPhotoMountParam *param = (SceAppMgrPhotoMountParam *)userParam;
	int ret;

	ARGS_IN(arguments, userArguments);
	BLOCK_IN_SIZED(param, userParam, arguments.size, sizeof(*param));
	return appmgrPhotoMountSyscall(param);
}

int _sceAppMgrPhotoUmount(const SceAppMgrPhotoUmountParam *userParam, const SyscallSizeArgs *userArguments)
{
	SyscallSizeArgs arguments;
	SceAppMgrPhotoUmountParam *param = (SceAppMgrPhotoUmountParam *)userParam;
	int ret;

	ARGS_IN(arguments, userArguments);
	BLOCK_IN_SIZED(param, userParam, arguments.size, sizeof(*param));
	return appmgrPhotoUmountSyscall(param);
}
