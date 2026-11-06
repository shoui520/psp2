/*
 * SceLibLocation's handles: the list sceLocationInit makes (IpcClientList, Sony's name, from its
 * lock), one IpcClientInfo per handle with its location client and its two callback threads,
 * and the heading client the handles share. The file's name is ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include <modulemgr/module_info.h>
#include "location_client.h"
#include "location_service_ipc_client.h"
#include "../../../../../../../lowlayer/src/ipmi/ipmi_error.h"

/* the events a handle's callback thread and sceLocationGetLocation wait for */
#define LOCATION_EVENTS_CALLBACK	(LOCATION_EVENT_CALLBACK_LOCATION | \
					 LOCATION_EVENT_LOCATION | \
					 LOCATION_EVENT_CONFIRM_RUNNING | \
					 LOCATION_EVENT_CONFIRM_DENIED | \
					 LOCATION_EVENT_CONFIRM_CLOSED)
#define LOCATION_EVENTS_GET_LOCATION	(LOCATION_EVENT_LOCATION | \
					 LOCATION_EVENT_LOCATION_CANCELED | \
					 LOCATION_EVENT_CONFIRM_ALLOWED | \
					 LOCATION_EVENT_CONFIRM_RUNNING | \
					 LOCATION_EVENT_CONFIRM_DENIED | \
					 LOCATION_EVENT_CONFIRM_CLOSED)
#define LOCATION_EVENTS_CONFIRM		(LOCATION_EVENT_CONFIRM_DENIED | \
					 LOCATION_EVENT_PERMISSION_ALLOWED)
/* a confirmation finished, with its result to read (both location events at once) */
#define LOCATION_EVENT_CONFIRM_RESULT	(LOCATION_EVENT_CALLBACK_LOCATION | LOCATION_EVENT_LOCATION)

IpcClientList *IpcClientList::s_instance = NULL;
SceUInt8 IpcClientList::s_useHeap = 1;
SceInt32 IpcClientList::s_threadPriority = SCE_KERNEL_DEFAULT_PRIORITY;
SceSize IpcClientList::s_heapSize = LOCATION_HEAP_SIZE;

static const LocationAllocator s_defaultAllocator = { locationHeapAlloc, locationHeapFree };
LocationAllocator IpcClientList::s_allocator = s_defaultAllocator;
int IpcClientList::s_heapRefCount;
SceInt32 IpcClientList::s_cpuAffinityMask;
int IpcClientList::s_initState;
bool IpcClientList::s_extensionLoaded;

/* the threads module_start and module_stop run on */
extern const SceModuleThreadParam sce_module_start_thread_parameter = {
	4, 0, 0x1000, 0, 0
};
extern const SceModuleThreadParam sce_module_stop_thread_parameter = {
	4, 0, 0x1000, 0, 0
};

__attribute__((noinline)) int IpcClientList::destroyClients()
{
	SceUInt32 i;

	for (i = 0; i < s_instance->clientCount; i++) {
		IpcClientInfo *info = s_instance->clients[i];

		if (info != NULL) {
			if (info->locationThread != -1) {
				sceKernelDeleteThread(info->locationThread);
				info->locationThread = -1;
			}
			if (info->headingThread != -1) {
				sceKernelDeleteThread(info->headingThread);
				info->headingThread = -1;
			}
			if (info->lockId == 0) {
				sceKernelDeleteLwMutex(&info->lock);
				info->lockId = -1;
			}
			s_allocator.free(info);
		}
	}
	if (s_instance->lockId == 0) {
		sceKernelDeleteLwMutex(&s_instance->lock);
		s_instance->lockId = -1;
	}
	s_instance->mode = 0;
	s_threadPriority = SCE_KERNEL_DEFAULT_PRIORITY;
	s_cpuAffinityMask = 0;
	return 0;
}

__attribute__((noinline)) int IpcClientList::createClients(SceUInt32 count, SceUInt32 mode)
{
	int error;
	SceUInt32 i;
	SceUInt32 j;
	int ret;

	s_instance->clientCount = count;
	s_instance->mode = mode;
	ret = sceKernelCreateLwMutex(&s_instance->lock, "SceIpcClientList", 0, 0, NULL);
	if (ret < 0)
		return ret;
	error = SCE_LOCATION_ERROR_FATAL_ERROR;
	s_instance->lockId = ret;

	for (i = 0; i < s_instance->clientCount; i++) {
		for (j = 0; j < LOCATION_CLIENT_MAX; j++) {
			if (s_instance->clients[j] != NULL)
				continue;

			s_instance->clients[j] =
				(IpcClientInfo *)s_allocator.alloc(sizeof(IpcClientInfo));
			if (s_instance->clients[j] == NULL) {
				error = SCE_LOCATION_ERROR_NO_MEMORY;
				break;
			}
			s_instance->clients[j]->client = NULL;
			s_instance->clients[j]->handle = 0;
			ret = sceKernelCreateLwMutex(&s_instance->clients[j]->lock, "SceIpcClientInfo",
						     0, 0, NULL);
			s_instance->clients[j]->lockId = ret;
			if (ret < 0) {
				error = ret;
				break;
			}
			ret = sceKernelCreateThread("SceLocationCallback", locationCallbackThread,
						    s_threadPriority,
						    LOCATION_CALLBACK_THREAD_STACK_SIZE, 0,
						    s_cpuAffinityMask, NULL);
			if (ret < 0) {
				s_instance->clients[j]->locationThread = -1;
				error = ret;
				break;
			}
			s_instance->clients[j]->locationThread = ret;
			s_instance->clients[j]->headingThread =
				sceKernelCreateThread("SceHeadingCallback", headingCallbackThread,
						      s_threadPriority,
						      LOCATION_CALLBACK_THREAD_STACK_SIZE, 0,
						      s_cpuAffinityMask, NULL);
			if (s_instance->clients[j]->headingThread >= 0) {
				error = 0;
				break;
			}
			sceKernelDeleteThread(s_instance->clients[j]->locationThread);
			error = s_instance->clients[j]->headingThread;
			s_instance->clients[j]->locationThread = -1;
			s_instance->clients[j]->headingThread = -1;
			break;
		}
		if (error != 0) {
			destroyClients();
			return error;
		}
	}
	return 0;
}

/* the first sceLocationInit makes the heap, unless the system's client supplied an allocator */
inline int IpcClientList::acquireHeap(SceSize size)
{
	int ret;

	if (s_useHeap) {
		ret = 0;
		if (s_heapRefCount == 0) {
			s_allocator = s_defaultAllocator;
			ret = locationHeapCreate(size);
		}
		if (ret == 0)
			s_heapRefCount++;
		if (ret != 0)
			return ret;
	}
	return 0;
}

inline void IpcClientList::releaseHeap()
{
	if (s_useHeap) {
		if (s_heapRefCount == 1)
			locationHeapDestroy();
		else if (s_heapRefCount == 0)
			return;
		s_heapRefCount--;
	}
}

inline void IpcClientList::destroyInstance()
{
	delete s_instance;
	s_instance = NULL;
}

int IpcClientList::init(SceUInt32 open, SceUInt32 mode)
{
	SceUInt32 sdkVersion;
	SceUInt32 clientMode;
	SceSize size;
	int ret;

	if (s_initState != LOCATION_STATE_NONE)
		return SCE_LOCATION_ERROR_INITIALIZED;
	if (open == 0 || open > LOCATION_CLIENT_MAX)
		return SCE_LOCATION_ERROR_INVALID_VALUE;

	sdkVersion = SceLibKernel_9F793F84();
	if ((SceUInt8)mode == SCE_LOCATION_MODE_V2)
		size = sdkVersion < LOCATION_SDK_VERSION_1_800 ? LOCATION_HEAP_SIZE_LEGACY_V2
							       : LOCATION_HEAP_SIZE;
	else
		size = LOCATION_HEAP_SIZE;
	s_heapSize = size;
	if (sdkVersion >= LOCATION_SDK_VERSION_2_000 && (SceUInt8)mode == SCE_LOCATION_MODE_V3 &&
	    !s_extensionLoaded)
		return SCE_LOCATION_ERROR_INVALID_VALUE;

	ret = acquireHeap(size);
	if (ret != 0)
		return ret;

	if (s_instance == NULL) {
		s_instance = new IpcClientList;
		if (s_instance == NULL) {
			releaseHeap();
			return SCE_LOCATION_ERROR_NO_MEMORY;
		}
	}

	if (sdkVersion < LOCATION_SDK_VERSION_1_500)
		s_initState = LOCATION_STATE_INITIALIZED;
	clientMode = mode;
	if (sdkVersion < LOCATION_SDK_VERSION_2_100)
		clientMode |= LOCATION_MODE_LEGACY_SDK;
	ret = createClients(open, clientMode);
	if (ret != 0) {
		destroyInstance();
		releaseHeap();
		return ret;
	}
	if (sdkVersion >= LOCATION_SDK_VERSION_1_500)
		s_initState = LOCATION_STATE_INITIALIZED;
	return 0;
}

__attribute__((noinline)) int IpcClientList::closeHeading(IpcClientInfo *info)
{
	if (s_instance->headingClient == NULL)
		return SCE_LOCATION_ERROR_FATAL_ERROR;
	if (s_instance->openCount == 1 &&
	    s_instance->headingClient->disconnect() == IPMI_ERROR_BUSY)
		return SCE_LOCATION_ERROR_FATAL_ERROR;

	if (info->headingThreadRunning == 1) {
		info->headingThreadRunning = 0;
		sceKernelWaitThreadEnd(info->headingThread, NULL, NULL);
	}
	info->headingCallback = NULL;
	info->headingUserdata = NULL;
	info->headingCallbackStarted = 0;
	if (s_instance->openCount == 1) {
		s_instance->headingClient->destroy();
		s_allocator.free(s_instance->headingClient);
		s_instance->headingClient = NULL;
	}
	return 0;
}

__attribute__((noinline)) int IpcClientList::closeLocation(IpcClientInfo *info)
{
	if (info->client == NULL)
		return SCE_LOCATION_ERROR_FATAL_ERROR;
	if (info->client->disconnect() == IPMI_ERROR_BUSY)
		return SCE_LOCATION_ERROR_FATAL_ERROR;

	if (info->locationThreadRunning == 1) {
		info->locationThreadRunning = 0;
		sceKernelWaitThreadEnd(info->locationThread, NULL, NULL);
	}
	info->locationCallback = NULL;
	info->locationUserdata = NULL;
	info->locationCallbackStarted = 0;
	info->client->destroy();
	s_allocator.free(info->client);
	info->client = NULL;
	if (s_instance->confirmData != NULL) {
		s_allocator.free(s_instance->confirmData);
		s_instance->confirmData = NULL;
	}
	return 0;
}

int IpcClientList::removeClient(SceLocationHandle handle)
{
	int ret = 0;
	int i;

	sceKernelLockLwMutex(&s_instance->lock, 1, NULL);
	for (i = 0; i < LOCATION_CLIENT_MAX; i++) {
		IpcClientInfo *info = s_instance->clients[i];

		if (info != NULL && info->handle == handle) {
			closeLocation(info);
			ret = closeHeading(info);
			info->handle = SCE_LOCATION_HANDLE_INVALID;
			s_instance->applicationDisabled = 0;
			s_instance->openCount--;
			break;
		}
	}
	sceKernelUnlockLwMutex(&s_instance->lock, 1);
	return ret;
}

int IpcClientList::close(IpcClientInfo *info)
{
	locationServiceClose(info->client, info->handle);
	return removeClient(info->handle);
}

int IpcClientList::closeAll()
{
	int ret;
	int i;

	if (s_initState == LOCATION_STATE_NONE)
		return SCE_LOCATION_ERROR_UNINITIALIZED;

	ret = 0;
	for (i = 0; i < LOCATION_CLIENT_MAX; i++) {
		IpcClientInfo *info = s_instance->clients[i];

		if (info != NULL && info->handle != 0 &&
		    info->handle != SCE_LOCATION_HANDLE_INVALID)
			ret = s_instance->close(info);
	}
	return ret;
}

int IpcClientList::term()
{
	int ret;

	if (s_initState == LOCATION_STATE_NONE)
		return SCE_LOCATION_ERROR_UNINITIALIZED;

	closeAll();
	ret = destroyClients();
	destroyInstance();
	releaseHeap();
	s_initState = LOCATION_STATE_NONE;
	return ret;
}

/* the first handle opened connects the heading client the handles share */
__attribute__((noinline)) int IpcClientList::connectHeading(IpcClientInfo *info)
{
	if (s_instance->openCount == 1) {
		IPMI::Client::Config config;
		void *memory;
		int result;
		int error;
		int ret;

		config.budget = IPMI::USER_BUDGET;
		sceClibStrncpy(config.serviceName, LOCATION_SERVICE_NAME_HEADING,
			       sizeof(config.serviceName));
		memory = s_allocator.alloc(IPMI::Client::getBufferSize(&config));
		if (memory == NULL)
			return SCE_LOCATION_ERROR_NO_MEMORY;

		ret = IPMI::Client::create(&s_instance->headingClient, &config, s_instance, memory);
		if (ret != 0) {
			s_allocator.free(memory);
			return ret;
		}
		ret = s_instance->headingClient->connect(NULL, 0, &result);
		if (ret != 0) {
			error = SCE_LOCATION_ERROR_FATAL_ERROR;
			switch (ret) {
			case IPMI_ERROR_CONNECTION_REFUSED:
				error = ret;
				break;
			case IPMI_ERROR_SERVER_NOT_FOUND:
				error = SCE_LOCATION_ERROR_FATAL_ERROR;
				break;
			}
			s_instance->headingClient->destroy();
			s_allocator.free(memory);
			return error;
		}
	}
	info->headingThreadRunning = 0;
	return 0;
}

int IpcClientList::connectLocation(IpcClientInfo *info)
{
	IPMI::Client::Config config;
	void *memory;
	int result;
	int error;
	int ret;

	if (info->client != NULL)
		return SCE_LOCATION_ERROR_FATAL_ERROR;

	sceClibStrncpy(config.serviceName, LOCATION_SERVICE_NAME_LOCATION,
		       sizeof(config.serviceName));
	config.budget = IPMI::USER_BUDGET;
	memory = s_allocator.alloc(IPMI::Client::getBufferSize(&config));
	if (memory == NULL)
		return SCE_LOCATION_ERROR_NO_MEMORY;

	ret = IPMI::Client::create(&info->client, &config, s_instance, memory);
	if (ret != 0) {
		s_allocator.free(memory);
		return ret;
	}
	result = 0;
	ret = info->client->connect(NULL, 0, &result);
	if (ret != 0) {
		error = SCE_LOCATION_ERROR_FATAL_ERROR;
		switch (ret) {
		case IPMI_ERROR_CONNECTION_REFUSED:
			error = ret;
			break;
		case IPMI_ERROR_SERVER_NOT_FOUND:
			error = SCE_LOCATION_ERROR_FATAL_ERROR;
			break;
		}
		info->client->destroy();
		s_allocator.free(memory);
		return error;
	}
	info->locationThreadRunning = 0;
	return 0;
}

int IpcClientList::allocClient(IpcClientInfo **ppInfo)
{
	SceInt8 i;
	int ret;

	if (s_initState == LOCATION_STATE_NONE)
		return SCE_LOCATION_ERROR_UNINITIALIZED;

	ret = sceKernelLockLwMutex(&s_instance->lock, 1, NULL);
	if (ret < 0)
		return ret;

	for (i = 0; i < s_instance->clientCount; i++) {
		if (s_instance->clients[i]->client != NULL)
			continue;

		*ppInfo = s_instance->clients[i];
		s_instance->openCount++;
		ret = s_instance->connectLocation(s_instance->clients[i]);
		if (ret != 0) {
			int unlock;

			s_instance->openCount--;
			unlock = sceKernelUnlockLwMutex(&s_instance->lock, 1);
			if (unlock < 0)
				return unlock;
			return ret;
		}
		ret = connectHeading(s_instance->clients[i]);
		if (ret != 0) {
			int unlock;

			closeLocation(s_instance->clients[i]);
			s_instance->openCount--;
			unlock = sceKernelUnlockLwMutex(&s_instance->lock, 1);
			if (unlock < 0)
				return unlock;
			return ret;
		}
		ret = sceKernelUnlockLwMutex(&s_instance->lock, 1);
		if (ret < 0)
			return ret;
		return 0;
	}
	ret = sceKernelUnlockLwMutex(&s_instance->lock, 1);
	if (ret < 0)
		return ret;
	return SCE_LOCATION_ERROR_TOO_MANY_HANDLES;
}

IpcClientList *IpcClientList::getInstance()
{
	return s_instance;
}

IpcClientInfo *IpcClientList::find(SceLocationHandle handle)
{
	int i;

	if (s_instance == NULL || handle == SCE_LOCATION_HANDLE_INVALID || handle == 0)
		return NULL;

	sceKernelLockLwMutex(&s_instance->lock, 1, NULL);
	for (i = 0; i < LOCATION_CLIENT_MAX; i++) {
		if (s_instance->clients[i] != NULL && s_instance->clients[i]->handle == handle) {
			sceKernelUnlockLwMutex(&s_instance->lock, 1);
			return s_instance->clients[i];
		}
	}
	sceKernelUnlockLwMutex(&s_instance->lock, 1);
	return NULL;
}

void IpcClientList::setInstance(IpcClientList *list)
{
	s_instance = list;
	s_initState = LOCATION_STATE_SYSTEM;
}

/* the user answers the confirmation dialog the service opened */
inline int IpcClientList::waitConfirmDialog(IPMI::Client *client)
{
	SceUInt32 pattern;
	int ret;

	confirmStatus = SCE_LOCATION_DIALOG_STATUS_RUNNING;
	confirmResult = SCE_LOCATION_DIALOG_RESULT_NONE;
	ret = client->waitEventFlag(0, LOCATION_EVENTS_CONFIRM, SCE_KERNEL_EVF_WAITMODE_OR |
				    SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, &pattern, NULL);
	if (ret == 0) {
		if (pattern == LOCATION_EVENT_PERMISSION_ALLOWED)
			confirmResult = SCE_LOCATION_DIALOG_STATUS_ENABLE;
		else if (pattern == LOCATION_EVENT_CONFIRM_DENIED)
			confirmResult = SCE_LOCATION_DIALOG_RESULT_DISABLE;
		else
			ret = SCE_LOCATION_ERROR_FATAL_ERROR;
		confirmStatus = SCE_LOCATION_DIALOG_STATUS_FINISHED;
	}
	return ret;
}

int IpcClientList::open(SceLocationHandle *handle, IpcClientInfo *info, SceUInt32 lmethod,
			SceUInt32 hmethod)
{
	int status;
	int ret;

	ret = locationServiceOpen(info->client, handle, lmethod, hmethod, mode, &status);
	info->handle = *handle;
	if (status == SCE_LOCATION_ERROR_DISABLE_APPLICATION)
		applicationDisabled = 1;
	if (ret == SCE_LOCATION_INFO_DENIED_BY_USER && (mode & LOCATION_MODE_CONFIRM_MASK) == 0)
		ret = waitConfirmDialog(info->client);
	return ret;
}

int IpcClientList::reopen(IpcClientInfo *info, SceUInt32 lmethod, SceUInt32 hmethod)
{
	SceUInt32 confirm;
	int ret;

	ret = sceKernelLockLwMutex(&info->lock, 1, NULL);
	if (ret != 0)
		return ret;
	info->locationCallback = NULL;
	info->headingCallback = NULL;
	info->headingCallbackStarted = 0;
	info->locationUserdata = NULL;
	info->locationCallbackStarted = 0;
	info->headingUserdata = NULL;
	ret = sceKernelUnlockLwMutex(&info->lock, 1);
	if (ret != 0)
		return ret;

	ret = locationServiceReopen(info->client, info->handle, lmethod, hmethod);
	confirm = mode & LOCATION_MODE_CONFIRM_MASK;
	if (ret == SCE_LOCATION_INFO_DENIED_BY_USER && confirm == SCE_LOCATION_MODE_CONFIRM_MANUAL)
		ret = waitConfirmDialog(info->client);
	return ret;
}

int IpcClientList::getMethod(IpcClientInfo *info, SceUInt32 *lmethod, SceUInt32 *hmethod)
{
	return locationServiceGetMethod(info->client, info->handle, lmethod, hmethod);
}

int IpcClientList::getLocation(IpcClientInfo *info, SceUInt32 timeout,
			       SceLocationLocationInfo *linfo)
{
	SceUInt32 confirm;
	SceUInt32 pattern;
	int ret;

	ret = locationServiceGetLocation(info->client, info->handle, timeout, linfo);
	confirm = mode & LOCATION_MODE_CONFIRM_MASK;
	if (ret == SCE_LOCATION_ERROR_INTERNAL_FILE_INVALID ||
	    (confirm == SCE_LOCATION_MODE_CONFIRM_AUTO && ret == SCE_LOCATION_INFO_DENIED_BY_USER)) {
		for (;;) {
			if (info->client->waitEventFlag(0, LOCATION_EVENTS_GET_LOCATION,
							SCE_KERNEL_EVF_WAITMODE_OR |
							SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
							&pattern, NULL) != 0)
				continue;

			if (pattern == LOCATION_EVENT_LOCATION) {
				if (confirm == SCE_LOCATION_MODE_CONFIRM_AUTO) {
					confirmStatus = SCE_LOCATION_DIALOG_STATUS_FINISHED;
					confirmResult = SCE_LOCATION_DIALOG_STATUS_ENABLE;
				}
				return locationServiceGetLocationResult(info->client, info->handle,
									linfo);
			}
			if (pattern == LOCATION_EVENT_LOCATION_CANCELED) {
				if (confirm == SCE_LOCATION_MODE_CONFIRM_AUTO &&
				    confirmStatus == SCE_LOCATION_DIALOG_STATUS_RUNNING) {
					confirmStatus = SCE_LOCATION_DIALOG_STATUS_FINISHED;
					confirmResult = SCE_LOCATION_DIALOG_STATUS_ENABLE;
				}
				return SCE_LOCATION_INFO_GET_LOCATION_CANCELED;
			}
			if (pattern == LOCATION_EVENT_CONFIRM_RUNNING) {
				confirmPending = 1;
				confirmStatus = SCE_LOCATION_DIALOG_STATUS_RUNNING;
				confirmResult = SCE_LOCATION_DIALOG_RESULT_NONE;
			} else if (pattern == LOCATION_EVENT_CONFIRM_ALLOWED) {
				confirmStatus = SCE_LOCATION_DIALOG_STATUS_FINISHED;
				confirmResult = SCE_LOCATION_DIALOG_STATUS_ENABLE;
				confirmPending = 0;
			} else if (pattern == LOCATION_EVENT_CONFIRM_DENIED) {
				ret = SCE_LOCATION_INFO_DENIED_BY_USER;
				confirmStatus = SCE_LOCATION_DIALOG_STATUS_FINISHED;
				confirmResult = SCE_LOCATION_DIALOG_RESULT_DISABLE;
				confirmPending = 0;
				return ret;
			} else if (pattern == LOCATION_EVENT_CONFIRM_CLOSED) {
				ret = SCE_LOCATION_INFO_DENIED_BY_USER;
				confirmPending = 0;
				return ret;
			} else {
				return SCE_LOCATION_ERROR_FATAL_ERROR;
			}
		}
	}
	if (ret == SCE_LOCATION_INFO_DISABLE_DEVICE &&
	    SceLibKernel_9F793F84() < LOCATION_SDK_VERSION_1_800)
		ret = SCE_LOCATION_INFO_UNDETERMINED_LOCATION;
	return ret;
}

int IpcClientList::cancelGetLocation(IpcClientInfo *info)
{
	int ret;

	ret = locationServiceCancelGetLocation(info->client, info->handle);
	if (ret == 0 && confirmPending != 0) {
		confirmStatus = SCE_LOCATION_DIALOG_STATUS_FINISHED;
		confirmResult = SCE_LOCATION_DIALOG_RESULT_NONE;
		confirmPending = 0;
	}
	return ret;
}

int IpcClientList::startLocationCallback(IpcClientInfo *info, SceUInt32 distance,
					 SceLocationLocationInfoCallback callback, void *userdata)
{
	int unlock;
	int ret;

	ret = sceKernelLockLwMutex(&info->lock, 1, NULL);
	if (ret != 0)
		return ret;
	if (info->locationCallbackStarted == 1) {
		ret = sceKernelUnlockLwMutex(&info->lock, 1);
		if (ret != 0)
			return ret;
		return SCE_LOCATION_ERROR_MULTIPLE_CALLBACK;
	}

	info->locationUserdata = userdata;
	info->locationCallbackStarted = 1;
	info->locationCallback = callback;
	ret = locationServiceStartLocationCallback(info->client, info->handle, distance);
	if (ret != 0) {
		info->locationCallback = NULL;
		info->locationUserdata = NULL;
		info->locationCallbackStarted = 0;
	} else if (info->locationThreadRunning == 0) {
		info->locationThreadRunning = 1;
		ret = sceKernelStartThread(info->locationThread, sizeof(info), &info);
		if (ret != 0)
			info->locationThreadRunning = 0;
	}
	unlock = sceKernelUnlockLwMutex(&info->lock, 1);
	if (unlock != 0)
		return unlock;
	return ret;
}

int IpcClientList::stopLocationCallback(IpcClientInfo *info)
{
	int ret;

	ret = sceKernelLockLwMutex(&info->lock, 1, NULL);
	if (ret != 0)
		return ret;
	if (info->locationCallbackStarted != 1) {
		ret = sceKernelUnlockLwMutex(&info->lock, 1);
		if (ret != 0)
			return ret;
		return SCE_LOCATION_ERROR_NOT_RUNNING_CALLBACK;
	}

	info->locationCallback = NULL;
	info->locationUserdata = NULL;
	info->locationCallbackStarted = 0;
	ret = sceKernelUnlockLwMutex(&info->lock, 1);
	if (ret != 0)
		return ret;
	if (info->locationThreadRunning == 1 && confirmStatus != SCE_LOCATION_DIALOG_STATUS_RUNNING) {
		info->locationThreadRunning = 0;
		sceKernelWaitThreadEnd(info->locationThread, NULL, NULL);
	}
	return locationServiceStopLocationCallback(info->client, info->handle);
}

int IpcClientList::getHeading(IpcClientInfo *info, SceLocationHeadingInfo *hinfo)
{
	return locationServiceGetHeading(headingClient, info->handle, hinfo);
}

int IpcClientList::startHeadingCallback(IpcClientInfo *info, SceFloat32 difference,
					SceLocationHeadingInfoCallback callback, void *userdata)
{
	int unlock;
	int ret;

	ret = sceKernelLockLwMutex(&info->lock, 1, NULL);
	if (ret != 0)
		return ret;
	if (info->headingCallbackStarted == 1) {
		ret = sceKernelUnlockLwMutex(&info->lock, 1);
		if (ret != 0)
			return ret;
		return SCE_LOCATION_ERROR_MULTIPLE_CALLBACK;
	}

	info->headingUserdata = userdata;
	info->headingCallback = callback;
	info->headingCallbackStarted = 1;
	ret = locationServiceStartHeadingCallback(headingClient, info->handle, difference);
	if (ret != 0) {
		info->headingCallback = NULL;
		info->headingUserdata = NULL;
		info->headingCallbackStarted = 0;
	} else if (info->headingThreadRunning == 0) {
		info->headingThreadRunning = 1;
		ret = sceKernelStartThread(info->headingThread, sizeof(info), &info);
		if (ret != 0)
			info->headingThreadRunning = 0;
	}
	unlock = sceKernelUnlockLwMutex(&info->lock, 1);
	if (unlock != 0)
		return unlock;
	return ret;
}

int IpcClientList::stopHeadingCallback(IpcClientInfo *info)
{
	int ret;

	ret = sceKernelLockLwMutex(&info->lock, 1, NULL);
	if (ret != 0)
		return ret;
	if (info->headingCallbackStarted == 0) {
		ret = sceKernelUnlockLwMutex(&info->lock, 1);
		if (ret != 0)
			return ret;
		return SCE_LOCATION_ERROR_NOT_RUNNING_CALLBACK;
	}

	info->headingCallbackStarted = 0;
	info->headingUserdata = NULL;
	info->headingCallback = NULL;
	ret = sceKernelUnlockLwMutex(&info->lock, 1);
	if (ret != 0)
		return ret;
	if (info->headingThreadRunning == 1) {
		info->headingThreadRunning = 0;
		sceKernelWaitThreadEnd(info->headingThread, NULL, NULL);
	}
	return locationServiceStopHeadingCallback(headingClient, info->handle);
}

int IpcClientList::setAllocator(const LocationAllocator *allocator, SceUInt8 useHeap)
{
	if (useHeap == 1 || (s_initState | useHeap) == 0) {
		s_allocator.alloc = allocator->alloc;
		s_allocator.free = allocator->free;
		s_useHeap = useHeap;
		if (useHeap == 1)
			s_initState = LOCATION_STATE_SYSTEM;
		return 0;
	}
	return LOCATION_ERROR_ALLOCATOR_IN_USE;
}

LocationAllocator *IpcClientList::getAllocator()
{
	return &s_allocator;
}

int IpcClientList::confirm(IpcClientInfo *info)
{
	char data[2048];
	int unlock;
	int ret;

	sceClibMemset(data, 0, sizeof(data));
	ret = sceKernelLockLwMutex(&info->lock, 1, NULL);
	if (ret != 0)
		return ret;
	if (confirmStatus == SCE_LOCATION_DIALOG_STATUS_RUNNING && confirmPending == 0) {
		ret = sceKernelUnlockLwMutex(&info->lock, 1);
		if (ret != 0)
			return ret;
		return SCE_LOCATION_ERROR_MULTIPLE_CONFIRM;
	}

	confirmStatus = SCE_LOCATION_DIALOG_STATUS_RUNNING;
	confirmResult = SCE_LOCATION_DIALOG_RESULT_NONE;
	if (useConfirmData == 0)
		ret = locationServiceConfirm(info->client, info->handle, confirmArg, 0, data);
	else
		ret = locationServiceConfirm(info->client, info->handle, confirmArg,
					     confirmDataArg, confirmData);
	if (ret != 0) {
		confirmStatus = SCE_LOCATION_DIALOG_STATUS_IDLE;
	} else if (info->locationThreadRunning == 0) {
		info->locationThreadRunning = 1;
		ret = sceKernelStartThread(info->locationThread, sizeof(info), &info);
		if (ret != 0)
			info->locationThreadRunning = 0;
	}
	unlock = sceKernelUnlockLwMutex(&info->lock, 1);
	if (unlock != 0)
		return unlock;
	return ret;
}

int IpcClientList::confirmGetStatus(IpcClientInfo *info, SceLocationDialogStatus *status)
{
	(void)info;
	if (applicationDisabled == 1)
		return SCE_LOCATION_ERROR_DISABLE_APPLICATION;
	*status = confirmStatus;
	return 0;
}

int IpcClientList::confirmGetResult(IpcClientInfo *info, SceLocationDialogResult *result)
{
	SceLocationDialogResult dialogResult;
	int ret = 0;

	(void)info;
	if (applicationDisabled == 1)
		return SCE_LOCATION_ERROR_DISABLE_APPLICATION;
	dialogResult = SCE_LOCATION_DIALOG_RESULT_NONE;
	if (confirmStatus == SCE_LOCATION_DIALOG_STATUS_FINISHED)
		dialogResult = confirmResult;
	else
		ret = SCE_LOCATION_ERROR_DIALOG_RESULT_NONE;
	*result = dialogResult;
	return ret;
}

int IpcClientList::confirmAbort(IpcClientInfo *info)
{
	int ret;

	ret = locationServiceConfirmAbort(info->client, info->handle);
	confirmStatus = SCE_LOCATION_DIALOG_STATUS_IDLE;
	confirmResult = SCE_LOCATION_DIALOG_RESULT_NONE;
	return ret;
}

int IpcClientList::getPermission(IpcClientInfo *info, SceLocationPermissionInfo *permission)
{
	return locationServiceGetPermission(info->client, info->handle, permission);
}

int IpcClientList::denyApplication(IpcClientInfo *info)
{
	return locationServiceDenyApplication(info->client, info->handle);
}

/* a connection of its own for a request that needs no handle */
int IpcClientList::connectTemporary(IpcClientInfo *info)
{
	IPMI::Client::Config config;
	void *memory;
	int result;
	int error;
	int ret;

	sceClibStrncpy(config.serviceName, LOCATION_SERVICE_NAME_LOCATION,
		       sizeof(config.serviceName));
	config.budget = IPMI::USER_BUDGET;
	memory = s_allocator.alloc(IPMI::Client::getBufferSize(&config));
	if (memory == NULL)
		return SCE_LOCATION_ERROR_NO_MEMORY;

	ret = IPMI::Client::create(&info->client, &config, this, memory);
	if (ret != 0) {
		s_allocator.free(memory);
		return ret;
	}
	ret = info->client->connect(NULL, 0, &result);
	if (ret != 0) {
		error = SCE_LOCATION_ERROR_FATAL_ERROR;
		switch (ret) {
		case IPMI_ERROR_CONNECTION_REFUSED:
			error = ret;
			break;
		case IPMI_ERROR_SERVER_NOT_FOUND:
			error = SCE_LOCATION_ERROR_FATAL_ERROR;
			break;
		}
		info->client->destroy();
		s_allocator.free(memory);
		return error;
	}
	return 0;
}

int IpcClientList::createTemporaryClient(IpcClientInfo **ppInfo)
{
	IpcClientInfo *info;
	int ret;

	info = (IpcClientInfo *)s_allocator.alloc(sizeof(IpcClientInfo));
	if (info == NULL)
		return SCE_LOCATION_ERROR_NO_MEMORY;
	ret = s_instance->connectTemporary(info);
	if (ret != 0) {
		s_allocator.free(info);
		info = NULL;
	}
	*ppInfo = info;
	return ret;
}

int IpcClientList::destroyTemporaryClient(IpcClientInfo **ppInfo)
{
	IpcClientInfo *info = *ppInfo;
	int ret = SCE_LOCATION_ERROR_FATAL_ERROR;

	if (info != NULL) {
		if (info->client != NULL) {
			if (info->client->disconnect() != IPMI_ERROR_BUSY) {
				info->client->destroy();
				s_allocator.free(info->client);
				ret = 0;
				info->client = NULL;
			}
			info = *ppInfo;
		}
		s_allocator.free(info);
		*ppInfo = NULL;
	}
	return ret;
}

int IpcClientList::setGpsEmulationFile(IpcClientInfo *info, const char *path)
{
	return locationServiceSetGpsEmulationFile(info->client, path);
}

int IpcClientList::getServiceValue(IpcClientInfo *info, SceUInt32 *value)
{
	return locationServiceGetValue(info->client, value);
}

int IpcClientList::setThreadParameter(SceInt32 priority, SceInt32 cpuAffinityMask)
{
	if (s_initState != LOCATION_STATE_NONE)
		return SCE_LOCATION_ERROR_INITIALIZED;
	s_threadPriority = priority;
	s_cpuAffinityMask = cpuAffinityMask;
	return 0;
}

void IpcClientList::getThreadParameter(SceInt32 *pPriority, SceInt32 *pCpuAffinityMask)
{
	*pPriority = s_threadPriority;
	*pCpuAffinityMask = s_cpuAffinityMask;
}

SceUInt32 IpcClientList::getMode()
{
	return mode;
}

void IpcClientList::setConfirmPending(int pending)
{
	confirmPending = pending;
}

bool IpcClientList::isSystemInitialized()
{
	return s_initState == LOCATION_STATE_SYSTEM;
}

bool IpcClientList::isInitialized()
{
	return s_initState != LOCATION_STATE_NONE;
}

/* a handle's location callbacks, and the confirmation dialog's answers */
SceInt32 IpcClientList::locationCallbackThread(SceSize argSize, void *argBlock)
{
	SceLocationDialogResult result = SCE_LOCATION_DIALOG_RESULT_NONE;
	IpcClientInfo *info = *(IpcClientInfo **)argBlock;
	SceUInt32 timeout;
	SceUInt32 pattern;
	SceLocationLocationInfo linfo;

	(void)argSize;
	if (info->locationThreadRunning != 0) {
		SceLocationDialogStatus status = SCE_LOCATION_DIALOG_STATUS_IDLE;

		do {
			timeout = LOCATION_CALLBACK_WAIT_USEC;
			if (info->client->waitEventFlag(0, LOCATION_EVENTS_CALLBACK,
							SCE_KERNEL_EVF_WAITMODE_OR |
							SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
							&pattern, &timeout) == 0) {
				switch (pattern) {
				case LOCATION_EVENT_CALLBACK_LOCATION: {
					int ret;

					sceClibMemset(&linfo, 0, sizeof(linfo));
					ret = locationServiceGetLocationResult(info->client, info->handle,
									       &linfo);
					if (sceKernelLockLwMutex(&info->lock, 1, NULL) == 0) {
						if (info->locationCallback != NULL) {
							if (ret == SCE_LOCATION_INFO_DISABLE_DEVICE &&
							    (SceLibKernel_9F793F84() <
							     LOCATION_SDK_VERSION_1_800 ||
							     (SceUInt8)s_instance->mode ==
							     SCE_LOCATION_MODE_V1))
								ret = SCE_LOCATION_INFO_UNDETERMINED_LOCATION;
							info->locationCallback(ret, info->handle, &linfo,
									       info->locationUserdata);
						}
						sceKernelUnlockLwMutex(&info->lock, 1);
					}
					break;
				}
				case LOCATION_EVENT_CONFIRM_RESULT:
					if (s_instance->applicationDisabled != 1)
						status = s_instance->confirmStatus;
					if (status == SCE_LOCATION_DIALOG_STATUS_RUNNING &&
					    locationServiceGetConfirmResult(info->client, info->handle,
									    &result) == 0 &&
					    sceKernelLockLwMutex(&info->lock, 1, NULL) == 0) {
						s_instance->confirmResult = result;
						s_instance->confirmStatus =
							result != SCE_LOCATION_DIALOG_RESULT_NONE ?
							SCE_LOCATION_DIALOG_STATUS_FINISHED :
							SCE_LOCATION_DIALOG_STATUS_IDLE;
						sceKernelUnlockLwMutex(&info->lock, 1);
					}
					break;
				case LOCATION_EVENT_CONFIRM_RUNNING:
					s_instance->confirmPending = 1;
					s_instance->confirmStatus = SCE_LOCATION_DIALOG_STATUS_RUNNING;
					s_instance->confirmResult = SCE_LOCATION_DIALOG_RESULT_NONE;
					break;
				case LOCATION_EVENT_CONFIRM_DENIED:
					s_instance->confirmPending = 0;
					s_instance->confirmStatus = SCE_LOCATION_DIALOG_STATUS_FINISHED;
					s_instance->confirmResult = SCE_LOCATION_DIALOG_RESULT_DISABLE;
					break;
				case LOCATION_EVENT_CONFIRM_CLOSED:
					s_instance->confirmPending = 0;
					break;
				}
			}
			if (sceKernelLockLwMutex(&info->lock, 1, NULL) == 0) {
				if (info->locationCallbackStarted == 0 &&
				    s_instance->confirmStatus != SCE_LOCATION_DIALOG_STATUS_RUNNING)
					info->locationThreadRunning = 0;
				sceKernelUnlockLwMutex(&info->lock, 1);
			}
		} while (info->locationThreadRunning != 0);
	}
	SceThreadmgrCoredumpTime_sceKernelExitThread(0);
	return 0;
}

/* a handle's heading callbacks: the heading service sets the handle's own bit */
SceInt32 IpcClientList::headingCallbackThread(SceSize argSize, void *argBlock)
{
	IpcClientInfo *info = *(IpcClientInfo **)argBlock;
	SceLocationHeadingInfo hinfo;
	SceUInt32 pattern;
	SceUInt32 timeout;
	SceUInt32 bits;

	(void)argSize;
	while (info->headingThreadRunning != 0) {
		bits = info->handle | LOCATION_EVENT_HEADING;
		timeout = LOCATION_CALLBACK_WAIT_USEC;
		s_instance->headingClient->waitEventFlag(0, bits, SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
							 &pattern, &timeout);
		if (pattern == bits) {
			/* Sony keeps the method's result in the pattern word */
			pattern = locationServiceGetHeading(s_instance->headingClient,
							    info->handle, &hinfo);
			if (sceKernelLockLwMutex(&info->lock, 1, NULL) == 0) {
				if (info->headingCallback != NULL)
					info->headingCallback(pattern, info->handle, &hinfo,
							      info->headingUserdata);
				sceKernelUnlockLwMutex(&info->lock, 1);
			}
		}
		if (sceKernelLockLwMutex(&info->lock, 1, NULL) == 0) {
			if (info->headingCallbackStarted == 0)
				info->headingThreadRunning = 0;
			sceKernelUnlockLwMutex(&info->lock, 1);
		}
	}
	SceThreadmgrCoredumpTime_sceKernelExitThread(0);
	return 0;
}
