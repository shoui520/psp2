/* User-memory copies for application queries and system events. */

#include <appmgr.h>
#include <intrmgr/syscall_copy.h>
#include <string.h>
#include "appmgr_shell_event.h"
#include "appmgr_event.h"

typedef struct AppmgrAppParamStringArguments {
	SceSize length;
	SceUInt32 reserved[3];
} AppmgrAppParamStringArguments;

_Static_assert(sizeof(AppmgrAppParamStringArguments) == 16,
	       "application parameter string argument block");

extern int appmgrAppParamGetIntSyscall(int pid, int param, int *value);
extern int appmgrAppParamGetStringSyscall(int pid, int param, char *string,
					SceSize length);

extern int appmgrGetAppStateSyscall(SceAppMgrAppState *appState, SceSize size,
				  SceUInt32 buildVersion);
extern int appmgrReceiveSystemEventSyscall(SceAppMgrSystemEvent *event);
extern int appmgrReceiveShellEventSyscall(AppmgrShellEvent *event);
extern int appmgrPeekLaunchParamForShellSyscall(int appId, char *param);
extern int appmgrOverwriteLaunchParamForShellSyscall(int appId, const char *param);
extern int appmgrReceiveNotificationRequestForShellSyscall(AppmgrNotificationRequest *request);
extern int appmgrSetNetworkDisconnectionWarningDialogStateSyscall(SceBool enable,
							const void *optParam);
extern int appmgrGetAppInfoSyscall(const char *appName, SceAppMgrAppState *appInfo);
extern int appmgrGetFgAppInfoSyscall(void *appInfo);
extern int appmgrGetStatusByNameSyscall(const char *appName, void *appStatus);
extern int appmgrGetStatusByAppIdSyscall(int appId, void *appStatus);
extern int appmgrGetRecommendedScreenOrientationSyscall(int *orientation);
extern int appmgrGetStatusByIdSyscall(SceUID id, void *appStatus);
extern int appmgrGetRunningAppIdListForShellSyscall(SceInt32 *appIds, int count);
extern int appmgrGetNameByIdSyscall(SceUID id, char *appName);
extern int appmgrGetIdByNameSyscall(SceUID *id, const char *appName);
extern int appmgrGetCurrentBgmStateSyscall(void *currentBgmId, void *currentBgmState);
extern int appmgrGetCurrentBgmState2Syscall(void *bgmState);
extern int appmgrGetAppParamSyscall(char *param);
extern int appmgrGetAppParam2Syscall(void *appParam);
extern int appmgrGetBootParamSyscall(SceUInt32 request, void *bootParam);
extern int appmgrGetAppMgrStateSyscall(void *appMgrState);
extern int appmgrGetSystemDataFilePlayReadySyscall(SceUInt32 location,
			void *information, void *userData, SceSize dataSize);
extern int appmgrSetSystemDataFilePlayReadySyscall(SceUInt32 location,
		const void *information, const void *userData, SceSize dataSize);
extern int appmgrSetSystemDataFileSyscall(SceUInt32 location,
		const void *userData, SceSize dataSize);

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
	ret = SceSysmemForDriver_BC996A7A(buffer, bootParam, arguments.size);
	if (ret < 0)
		return ret;
	ret = appmgrGetBootParamSyscall(request, buffer);
	return copyResult(ret, SceSysmemForDriver_6D88EF8A(bootParam, buffer, arguments.size));
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
	return copyResult(ret, SceSysmemForDriver_6D88EF8A(string, buffer, length));
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
	return copyResult(ret, SceSysmemForDriver_6D88EF8A(param, buffer, length));
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
		copy = SceSysmemForDriver_6D88EF8A(currentBgmId, bgmId, sizeof(*bgmId));
		if (copy < 0)
			return copy;
	}
	if (currentBgmState != NULL)
		return copyResult(ret, SceSysmemForDriver_6D88EF8A(currentBgmState, bgmState,
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

int _sceAppMgrGetAppMgrState(void *appMgrState)
{
	int ret;

	BLOCK_OUT(appMgrState, 128,
		  appmgrGetAppMgrStateSyscall(NULL),
		  appmgrGetAppMgrStateSyscall(k));
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
	return copyResult(ret, SceSysmemForDriver_6D88EF8A(param, buffer, length));
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
	return copyResult(ret, SceSysmemForDriver_6D88EF8A(appName, name, length));
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
		ret = copyResult(ret, SceSysmemForDriver_6D88EF8A(userId, id, sizeof(*id)));
	return ret;
}
