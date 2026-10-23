/* Save-data slot user-memory copies; original source filename unavailable. */

#include <appmgr/appmgr_savedata.h>
#include <threadmgr/threadmgr.h>
#include <intrmgr/syscall_copy.h>
#include <string.h>
#include "app_state_internal.h"
#include "mount.h"

typedef struct AppmgrSaveDataSlotGetParamArguments {
	SceAppMgrSaveDataSlot *slot;
	const SyscallSizeArgs *userArguments;
} AppmgrSaveDataSlotGetParamArguments;

_Static_assert(sizeof(AppmgrSaveDataSlotGetParamArguments) == 8,
	       "save-data slot stack arguments");

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

int _sceAppMgrSaveDataSlotFileGetParam(unsigned int slotId, void *param)
{
	int ret;

	BLOCK_OUT(param, 1024,
		  appmgrSaveDataSlotFileGetParamSyscall(slotId, NULL),
		  appmgrSaveDataSlotFileGetParamSyscall(slotId, k));
}

int _sceAppMgrSaveDataAddMount(const AppmgrAddMountParam *userParam)
{
	void *param = (void *)userParam;
	int ret;

	BLOCK_IN(param, userParam, sizeof(AppmgrAddMountParam));
	return appmgrSaveDataAddMountSyscall(param);
}
