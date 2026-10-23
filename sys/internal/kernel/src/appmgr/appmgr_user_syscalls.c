/* User-memory copies for application queries and system events. */

#include <appmgr.h>
#include <intrmgr/syscall_copy.h>
#include <string.h>
#include <sysmem/sysclib.h>
#include "appmgr_shell_event.h"
#include "appmgr_event.h"
#include "app_state_internal.h"
#include <display.h>
#include <processmgr/process_budget.h>

typedef struct AppmgrAppParamStringArguments {
	SceSize length;
	SceUInt32 reserved[3];
} AppmgrAppParamStringArguments;

_Static_assert(sizeof(AppmgrAppParamStringArguments) == 16,
	       "application parameter string argument block");

/* the argument block of the launch calls by name: read, not used */
typedef struct AppmgrLaunchArguments {
	SceUInt32 reserved[4];
} AppmgrLaunchArguments;

/* the argument block of _sceAppMgrLaunchAppByPath4 */
typedef struct AppmgrLaunchAppByPathArguments {
	const char *param;
	SceUInt32 processAttr;
	const AppmgrLaunchAppOption *option;
	SceUInt32 reserved[5];
} AppmgrLaunchAppByPathArguments;

/* the argument block of _sceAppMgrLaunchAppByName2ndStage */
typedef struct AppmgrLaunchAppByName2ndStageArguments {
	SceUInt32 processAttr;
	const AppmgrLaunchAppOption *option;
	SceUInt32 reserved[2];
} AppmgrLaunchAppByName2ndStageArguments;

_Static_assert(sizeof(AppmgrLaunchArguments) == 16, "launch argument block");
_Static_assert(sizeof(AppmgrLaunchAppByPathArguments) == 32,
	       "launch by path argument block");
_Static_assert(sizeof(AppmgrLaunchAppByName2ndStageArguments) == 16,
	       "launch second stage argument block");

int __sceAppMgrGetAppState(SceAppMgrAppState *appState, SceSize size,
			 SceUInt32 buildVersion)
{
	int ret;

	BLOCK_OUT(appState, sizeof(*appState),
		  appmgrGetAppStateSyscall(NULL, size, buildVersion),
		  appmgrGetAppStateSyscall((SceAppMgrAppState *)k, size, buildVersion));
}

int _sceAppMgrReceiveSystemEvent(SceAppMgrSystemEvent *event)
{
	int ret;

	BLOCK_OUT(event, sizeof(*event),
		  appmgrReceiveSystemEventSyscall(NULL),
		  appmgrReceiveSystemEventSyscall((SceAppMgrSystemEvent *)k));
}

int _sceAppMgrSetNetworkDisconnectionWarningDialogState(SceBool enable,
						      const void *userOptParam)
{
	void *optParam = NULL;
	int ret;

	BLOCK_IN(optParam, userOptParam, 128);
	return appmgrSetNetworkDisconnectionWarningDialogStateSyscall(enable, optParam);
}

int _sceAppMgrLoadExec(const char *userPath, const char *const *argv, const void *option,
		       const SyscallUnusedArgs *userArguments)
{
	SyscallUnusedArgs arguments;
	char *path = (char *)userPath;
	int ret;

	ARGS_IN(arguments, userArguments);
	STRING_IN(path, userPath, 292);
	return appmgrLoadExecSyscall(path, argv, option);
}

int _sceAppMgrGetAppInfo(const char *appName, SceAppMgrAppState *appInfo,
		       const SyscallUnusedArgs *userArguments)
{
	SyscallUnusedArgs arguments;
	char *name = (char *)appName;
	int ret;

	ARGS_IN(arguments, userArguments);
	STRING_IN(name, appName, 32);
	BLOCK_OUT(appInfo, 128,
		  appmgrGetAppInfoSyscall(name, NULL),
		  appmgrGetAppInfoSyscall(name, (SceAppMgrAppState *)k));
}

int _sceAppMgrGetFgAppInfo(void *appInfo)
{
	int ret;

	BLOCK_OUT(appInfo, 128, appmgrGetFgAppInfoSyscall(NULL),
		  appmgrGetFgAppInfoSyscall(k));
}

int _sceAppMgrGetBootParam(SceUInt32 request, void *bootParam,
			 const SyscallSizeArgs *userArguments)
{
	SyscallSizeArgs arguments;
	void *buffer;
	int ret;

	ARGS_IN(arguments, userArguments);
	if (bootParam == NULL)
		return appmgrGetBootParamSyscall(request, NULL);
	if (arguments.size > 44)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	buffer = __builtin_alloca(44);
	__builtin_memset(buffer, 0, 44);
	ret = sceKernelCopyFromUser(buffer, bootParam, arguments.size);
	if (ret < 0)
		return ret;
	ret = appmgrGetBootParamSyscall(request, buffer);
	return copyResult(ret, sceKernelCopyToUser(bootParam, buffer, arguments.size));
}

int _sceAppMgrAppMount(const struct AppmgrAppMountParam *userParam,
		       const SyscallSizeArgs *userArguments)
{
	SyscallSizeArgs arguments;
	void *param = (void *)userParam;
	int ret;

	ARGS_IN(arguments, userArguments);
	BLOCK_IN_SIZED(param, userParam, arguments.size, 140);
	return appmgrAppMountSyscall(param);
}

int _sceAppMgrAppUmount(const struct AppmgrAppUmountParam *userParam,
			const SyscallSizeArgs *userArguments)
{
	SyscallSizeArgs arguments;
	void *param = (void *)userParam;
	int ret;

	ARGS_IN(arguments, userArguments);
	BLOCK_IN_SIZED(param, userParam, arguments.size, 12);
	return appmgrAppUmountSyscall(param);
}

int _sceAppMgrAppParamGetInt(int pid, int param, int *value)
{
	int ret;

	BLOCK_OUT(value, sizeof(*value),
		  appmgrAppParamGetIntSyscall(pid, param, NULL),
		  appmgrAppParamGetIntSyscall(pid, param, (int *)k));
}

int _sceAppMgrAppParamGetString(int pid, int param, char *string,
			      const AppmgrAppParamStringArguments *userArguments)
{
	AppmgrAppParamStringArguments arguments;
	char *buffer;
	SceSize capacity, length;
	int ret;

	ARGS_IN(arguments, userArguments);
	capacity = arguments.length;
	if (string == NULL)
		return appmgrAppParamGetStringSyscall(pid, param, NULL, capacity);
	buffer = __builtin_alloca(1024);
	__builtin_memset(buffer, 0, 1024);
	ret = appmgrAppParamGetStringSyscall(pid, param, buffer, capacity);
	length = strnlen(buffer, 1024) + 1;
	if (length >= 1024)
		length = 1024;
	return copyResult(ret, sceKernelCopyToUser(string, buffer, length));
}

int _sceAppMgrAppParamSetString(int param, const char *userString,
				const SyscallUnusedArgs *userArguments)
{
	SyscallUnusedArgs arguments;
	char *string = (char *)userString;
	int ret;

	ARGS_IN(arguments, userArguments);
	STRING_IN(string, userString, 1024);
	return appmgrAppParamSetStringSyscall(param, string);
}

int _sceAppMgrGetSystemDataFilePlayReady(SceUInt32 location, void *information,
				      void *userData, SceSize dataSize)
{
	int ret;

	BLOCK_OUT(information, 8,
		  appmgrGetSystemDataFilePlayReadySyscall(location, NULL,
						       userData, dataSize),
		  appmgrGetSystemDataFilePlayReadySyscall(location, k,
						       userData, dataSize));
}

int _sceAppMgrSetSystemDataFilePlayReady(SceUInt32 location,
		const void *information, const void *userData, SceSize dataSize)
{
	void *buffer = (void *)information;
	int ret;

	BLOCK_IN(buffer, information, 8);
	return appmgrSetSystemDataFilePlayReadySyscall(location, buffer,
						    userData, dataSize);
}

int _sceAppMgrSetSystemDataFile(SceUInt32 location, const void *userData,
			     SceSize dataSize)
{
	return appmgrSetSystemDataFileSyscall(location, userData, dataSize);
}

int _sceAppMgrLaunchAppByName(SceUInt32 launchMode, const char *userName, const char *userParam,
			       const AppmgrLaunchArguments *userArguments)
{
	AppmgrLaunchArguments arguments;
	char *name = (char *)userName;
	char *param = (char *)userParam;
	int ret;

	ARGS_IN(arguments, userArguments);
	STRING_IN(name, userName, 32);
	STRING_IN(param, userParam, 1024);
	return appmgrLaunchAppByNameSyscall(launchMode, (const SceChar8 *)name, param);
}

int _sceAppMgrLaunchAppByNameForShell(SceUInt32 launchMode, const char *userName, const char *userParam,
				       const AppmgrLaunchArguments *userArguments)
{
	AppmgrLaunchArguments arguments;
	char *name = (char *)userName;
	char *param = (char *)userParam;
	int ret;

	ARGS_IN(arguments, userArguments);
	STRING_IN(name, userName, 32);
	STRING_IN(param, userParam, 1024);
	return appmgrLaunchAppByNameForShellSyscall(launchMode, (const SceChar8 *)name, param);
}

int _sceAppMgrLaunchAppByName2(const char *userName, const char *userParam,
			        const AppmgrLaunchAppByNameOption *userOption,
			        const AppmgrLaunchArguments *userArguments)
{
	AppmgrLaunchArguments arguments;
	char *name = (char *)userName;
	char *param = (char *)userParam;
	void *option = (void *)userOption;
	int ret;

	ARGS_IN(arguments, userArguments);
	STRING_IN(name, userName, 32);
	STRING_IN(param, userParam, 1024);
	BLOCK_IN(option, userOption, 64);
	return appmgrLaunchAppByName2Syscall((const SceChar8 *)name, param, option);
}

int _sceAppMgrLaunchAppByName2ForShell(const char *userName, const char *userParam,
				        const AppmgrLaunchAppByNameOption *userOption,
				        const AppmgrLaunchArguments *userArguments)
{
	AppmgrLaunchArguments arguments;
	char *name = (char *)userName;
	char *param = (char *)userParam;
	void *option = (void *)userOption;
	int ret;

	ARGS_IN(arguments, userArguments);
	STRING_IN(name, userName, 32);
	STRING_IN(param, userParam, 1024);
	BLOCK_IN(option, userOption, 64);
	return appmgrLaunchAppByName2ForShellSyscall((const SceChar8 *)name, param, option);
}

int _sceAppMgrDestroyAppByName(const char *userName, const SyscallUnusedArgs *userArguments)
{
	SyscallUnusedArgs arguments;
	char *name = (char *)userName;
	int ret;

	ARGS_IN(arguments, userArguments);
	STRING_IN(name, userName, 32);
	return appmgrDestroyAppByNameSyscall(name);
}

int _sceAppMgrGetAppParam(char *param, const SyscallUnusedArgs *userArguments)
{
	SyscallUnusedArgs arguments;
	char *buffer;
	SceSize length;
	int ret;

	ARGS_IN(arguments, userArguments);
	if (param == NULL)
		return appmgrGetAppParamSyscall(NULL);
	buffer = __builtin_alloca(1024);
	__builtin_memset(buffer, 0, 1024);
	ret = appmgrGetAppParamSyscall(buffer);
	length = strnlen(buffer, 1024) + 1;
	if (length >= 1024)
		length = 1024;
	return copyResult(ret, sceKernelCopyToUser(param, buffer, length));
}

int _sceAppMgrGetAppParam2(void *appParam)
{
	int ret;

	BLOCK_OUT(appParam, 1088,
		  appmgrGetAppParam2Syscall(NULL),
		  appmgrGetAppParam2Syscall(k));
}

int _sceAppMgrGetStatusByName(const char *appName, void *appStatus,
			    const SyscallUnusedArgs *userArguments)
{
	SyscallUnusedArgs arguments;
	char *name = (char *)appName;
	int ret;

	ARGS_IN(arguments, userArguments);
	STRING_IN(name, appName, 32);
	BLOCK_OUT(appStatus, 128,
		  appmgrGetStatusByNameSyscall(name, NULL),
		  appmgrGetStatusByNameSyscall(name, k));
}

int _sceAppMgrGetStatusByAppId(int appId, void *appStatus)
{
	int ret;

	BLOCK_OUT(appStatus, 128,
		  appmgrGetStatusByAppIdSyscall(appId, NULL),
		  appmgrGetStatusByAppIdSyscall(appId, k));
}

int _sceAppMgrSendParam(const char *userName, const char *userParam, const void *userOption,
			const AppmgrLaunchArguments *userArguments)
{
	AppmgrLaunchArguments arguments;
	char *name = (char *)userName;
	char *param = (char *)userParam;
	void *option = (void *)userOption;
	int ret;

	ARGS_IN(arguments, userArguments);
	STRING_IN(name, userName, 32);
	STRING_IN(param, userParam, 1024);
	BLOCK_IN(option, userOption, 128);
	return appmgrSendParamSyscall(name, param, option);
}

int _sceAppMgrLaunchAppByUri(SceUInt32 launchMode, const char *userUri,
			     const SyscallUnusedArgs *userArguments)
{
	SyscallUnusedArgs arguments;
	char *uri = (char *)userUri;
	int ret;

	ARGS_IN(arguments, userArguments);
	STRING_IN(uri, userUri, 2083);
	return appmgrLaunchAppByUriSyscall(launchMode, uri);
}

int _sceAppMgrLaunchAppByUri2(const char *userUri, const AppmgrLaunchAppByNameOption *userOption,
			      const SyscallUnusedArgs *userArguments)
{
	SyscallUnusedArgs arguments;
	char *uri = (char *)userUri;
	void *option = (void *)userOption;
	int ret;

	ARGS_IN(arguments, userArguments);
	STRING_IN(uri, userUri, 2083);
	BLOCK_IN(option, userOption, 64);
	return appmgrLaunchAppByUri2Syscall(uri, option);
}

int _sceAppMgrDeclareShellProcess2(const char *userName, const SyscallUnusedArgs *userArguments)
{
	SyscallUnusedArgs arguments;
	char *name = (char *)userName;
	int ret;

	ARGS_IN(arguments, userArguments);
	STRING_IN(name, userName, 32);
	return appmgrDeclareShellProcess2Syscall(name);
}

int _sceAppMgrGetCurrentBgmState(void *currentBgmId, void *currentBgmState)
{
	int *bgmId = currentBgmId;
	int *bgmState;
	int ret, copy;

	if (currentBgmId != NULL) {
		bgmId = __builtin_alloca(sizeof(*bgmId));
		*bgmId = 0;
	}
	bgmState = currentBgmState;
	if (currentBgmState != NULL) {
		bgmState = __builtin_alloca(sizeof(*bgmState));
		*bgmState = 0;
	}
	ret = appmgrGetCurrentBgmStateSyscall(bgmId, bgmState);
	if (currentBgmId != NULL) {
		copy = sceKernelCopyToUser(currentBgmId, bgmId, sizeof(*bgmId));
		if (copy < 0)
			return copy;
	}
	if (currentBgmState != NULL)
		return copyResult(ret, sceKernelCopyToUser(currentBgmState, bgmState,
							 sizeof(*bgmState)));
	return ret;
}

int _sceAppMgrGetCurrentBgmState2(void *bgmState)
{
	int ret;

	BLOCK_OUT(bgmState, 32,
		  appmgrGetCurrentBgmState2Syscall(NULL),
		  appmgrGetCurrentBgmState2Syscall(k));
}

int _sceAppMgrCaptureFrameBufIFTUByAppId(int appId, const SceDisplayFrameBuf *userFrameBuf)
{
	void *frameBuf = (void *)userFrameBuf;
	int ret;

	BLOCK_IN(frameBuf, userFrameBuf, sizeof(SceDisplayFrameBuf));
	return appmgrCaptureFrameBufIFTUByAppIdSyscall(appId, frameBuf);
}

int _sceAppMgrCaptureFrameBufDMACByAppId(int appId, SceDisplayFrameBuf *userFrameBuf)
{
	SceDisplayFrameBuf frameBuf = { 0 };
	int ret;

	if (userFrameBuf == NULL)
		return appmgrCaptureFrameBufDMACByAppIdSyscall(appId, NULL);
	ret = sceKernelCopyFromUser(&frameBuf, userFrameBuf, sizeof(frameBuf));
	if (ret < 0)
		return ret;
	ret = appmgrCaptureFrameBufDMACByAppIdSyscall(appId, &frameBuf);
	return copyResult(ret, sceKernelCopyToUser(userFrameBuf, &frameBuf, sizeof(frameBuf)));
}

int _sceAppMgrGetAppMgrState(void *appMgrState)
{
	int ret;

	BLOCK_OUT(appMgrState, 128,
		  appmgrGetAppMgrStateSyscall(NULL),
		  appmgrGetAppMgrStateSyscall(k));
}

int _sceAppMgrLaunchAppByPath4(const char *userPath, const char *userName, SceUInt32 budgetSize,
			       const AppmgrLaunchAppByPathArguments *userArguments)
{
	AppmgrLaunchAppByPathArguments arguments;
	char *path = (char *)userPath;
	char *name = (char *)userName;
	const char *userParam;
	const AppmgrLaunchAppOption *userOption;
	char *param;
	void *option;
	SceUInt32 processAttr;
	int ret;

	ARGS_IN(arguments, userArguments);
	userParam = arguments.param;
	processAttr = arguments.processAttr;
	userOption = arguments.option;
	param = (char *)userParam;
	option = (void *)userOption;
	STRING_IN(path, userPath, 292);
	STRING_IN(name, userName, 32);
	STRING_IN(param, userParam, 1024);
	BLOCK_IN(option, userOption, 128);
	return appmgrLaunchApp2Syscall(path, name, budgetSize, param, processAttr, option);
}

int _sceAppMgrLaunchAppByName2ndStage(int appId, const char *userPath, SceUInt32 budgetSize,
		const AppmgrLaunchAppByName2ndStageArguments *userArguments)
{
	AppmgrLaunchAppByName2ndStageArguments arguments;
	char *path = (char *)userPath;
	const AppmgrLaunchAppOption *userOption;
	void *option;
	SceUInt32 processAttr;
	int ret;

	ARGS_IN(arguments, userArguments);
	processAttr = arguments.processAttr;
	userOption = arguments.option;
	option = (void *)userOption;
	STRING_IN(path, userPath, 292);
	BLOCK_IN(option, userOption, 128);
	return appmgrLaunchAppByName2ndStageSyscall(appId, path, budgetSize, processAttr, option);
}

int _sceAppMgrLaunchVideoStreamingApp(const void *extraParam, SceSize extraParamSize,
				      const AppmgrLaunchAppByNameOption *userOption)
{
	AppmgrLaunchAppByNameOption buffer;
	const AppmgrLaunchAppByNameOption *option;
	int ret;

	if (userOption != NULL) {
		ret = sceKernelCopyFromUser(&buffer, userOption, sizeof(buffer));
		option = &buffer;
		if (ret < 0)
			return ret;
	} else {
		option = NULL;
	}
	return appmgrLaunchVideoStreamingAppSyscall(extraParam, extraParamSize, option);
}

int _sceAppMgrReceiveShellEvent(AppmgrShellEvent *event)
{
	int ret;

	BLOCK_OUT(event, sizeof(*event),
		  appmgrReceiveShellEventSyscall(NULL),
		  appmgrReceiveShellEventSyscall((AppmgrShellEvent *)k));
}

int _sceAppMgrPeekLaunchParamForShell(int appId, char *param,
				    const SyscallUnusedArgs *userArguments)
{
	SyscallUnusedArgs arguments;
	char *buffer;
	SceSize length;
	int ret;

	ARGS_IN(arguments, userArguments);
	if (param == NULL)
		return appmgrPeekLaunchParamForShellSyscall(appId, NULL);
	buffer = __builtin_alloca(1024);
	memset(buffer, 0, 1024);
	ret = appmgrPeekLaunchParamForShellSyscall(appId, buffer);
	length = strnlen(buffer, 1024) + 1;
	if (length >= 1024)
		length = 1024;
	return copyResult(ret, sceKernelCopyToUser(param, buffer, length));
}

int _sceAppMgrOverwriteLaunchParamForShell(int appId, const char *param,
					 const SyscallUnusedArgs *userArguments)
{
	SyscallUnusedArgs arguments;
	char *buffer = (char *)param;
	int ret;

	ARGS_IN(arguments, userArguments);
	STRING_IN(buffer, param, 1024);
	return appmgrOverwriteLaunchParamForShellSyscall(appId, buffer);
}

int _sceAppMgrGetCoredumpStateForShell(AppmgrShellCoredumpState *state)
{
	int ret;

	BLOCK_OUT(state, sizeof(*state),
		  appmgrGetCoredumpStateForShellSyscall(NULL),
		  appmgrGetCoredumpStateForShellSyscall((AppmgrShellCoredumpState *)k));
}

int _sceAppMgrGetPidListForShell(SceUID *pidList, unsigned int count,
				const SyscallSizeArgs *userArguments)
{
	SyscallSizeArgs arguments;
	int ret;

	ARGS_IN(arguments, userArguments);
	BLOCK_OUT_SIZED(pidList, arguments.size, 80,
		       appmgrGetPidListForShellSyscall(NULL, count),
		       appmgrGetPidListForShellSyscall((SceUID *)k, count));
}

int _sceAppMgrReceiveNotificationRequestForShell(AppmgrNotificationRequest *request)
{
	int ret;

	BLOCK_OUT(request, sizeof(*request),
		  appmgrReceiveNotificationRequestForShellSyscall(NULL),
		  appmgrReceiveNotificationRequestForShellSyscall((AppmgrNotificationRequest *)k));
}

int _sceAppMgrGetRecommendedScreenOrientation(int *orientation)
{
	int ret;

	BLOCK_OUT(orientation, sizeof(*orientation),
		  appmgrGetRecommendedScreenOrientationSyscall(NULL),
		  appmgrGetRecommendedScreenOrientationSyscall((int *)k));
}

int _sceAppMgrGetRunningAppIdListForShell(SceInt32 *appIds, int count,
					const SyscallSizeArgs *userArguments)
{
	SyscallSizeArgs arguments;
	int ret;

	ARGS_IN(arguments, userArguments);
	BLOCK_OUT_SIZED(appIds, arguments.size, 80,
		       appmgrGetRunningAppIdListForShellSyscall(NULL, count),
		       appmgrGetRunningAppIdListForShellSyscall((SceInt32 *)k, count));
}

int _sceAppMgrGetBudgetInfo(SceKernelBudgetInfo *userInfo)
{
	SceKernelBudgetInfo info;
	int ret;

	if (userInfo == NULL)
		return appmgrGetBudgetInfoSyscall(NULL);
	memset(&info, 0, sizeof(info));
	ret = sceKernelCopyFromUser(&info, userInfo, sizeof(info));
	if (ret < 0)
		return ret;
	ret = appmgrGetBudgetInfoSyscall(&info);
	return copyResult(ret, sceKernelCopyToUser(userInfo, &info, sizeof(info)));
}

int _sceAppMgrGetStatusById(SceUID id, void *appStatus)
{
	int ret;

	BLOCK_OUT(appStatus, 128,
		  appmgrGetStatusByIdSyscall(id, NULL),
		  appmgrGetStatusByIdSyscall(id, k));
}

int _sceAppMgrGetNameById(SceUID id, char *appName,
			const SyscallUnusedArgs *userArguments)
{
	SyscallUnusedArgs arguments;
	char *name;
	SceSize length;
	int ret;

	ARGS_IN(arguments, userArguments);
	if (appName == NULL)
		return appmgrGetNameByIdSyscall(id, NULL);
	name = __builtin_alloca(32);
	__builtin_memset(name, 0, 32);
	ret = appmgrGetNameByIdSyscall(id, name);
	length = strnlen(name, 32) + 1;
	if (length >= 32)
		length = 32;
	return copyResult(ret, sceKernelCopyToUser(appName, name, length));
}

int _sceAppMgrGetIdByName(SceUID *userId, const char *appName,
			const SyscallUnusedArgs *userArguments)
{
	SyscallUnusedArgs arguments;
	SceUID *id = userId;
	char *name = (char *)appName;
	int ret;

	ARGS_IN(arguments, userArguments);
	if (userId != NULL) {
		id = __builtin_alloca(sizeof(*id));
		*id = 0;
	}
	STRING_IN(name, appName, 32);
	ret = appmgrGetIdByNameSyscall(id, name);
	if (userId != NULL)
		ret = copyResult(ret, sceKernelCopyToUser(userId, id, sizeof(*id)));
	return ret;
}
