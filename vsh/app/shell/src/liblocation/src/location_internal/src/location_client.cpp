/*
 * SceLibLocationInternal's build of SceLibLocation's client list (location_client.cpp there):
 * the list it makes in the system mode and hands to SceLibLocation, which then runs the handles
 * on it. This build has no confirmPending, closes its handles without telling the service, and
 * takes its callback threads' parameters from SceLibLocation (SceLibLocation_991AF2D0).
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include "location_internal.h"
#include "../../../../../../../lowlayer/src/ipmi/ipmi_error.h"

/* the events a handle's callback thread waits for */
#define LOCATION_EVENTS_CALLBACK	(LOCATION_EVENT_CALLBACK_LOCATION | LOCATION_EVENT_LOCATION)
/* a confirmation finished, with its result to read (both location events at once) */
#define LOCATION_EVENT_CONFIRM_RESULT	(LOCATION_EVENT_CALLBACK_LOCATION | LOCATION_EVENT_LOCATION)
/* what the service sets on the client's event flag 0 for a history request */
#define LOCATION_EVENTS_WIFI_HISTORY	(LOCATION_EVENT_LOCATION | LOCATION_EVENT_LOCATION_CANCELED)
#define LOCATION_EVENTS_WIFI_HISTORY_QUERY \
					(LOCATION_EVENT_QUERY_LOCATION | LOCATION_EVENT_QUERY_CANCELED)

/* the system mode's heap */
#define LOCATION_HEAP_SIZE_SYSTEM	0x2000

/* the confirmation dialog's texts: the longest the data holds, and what separates them */
#define LOCATION_CONFIRM_TEXT_MAX	(LOCATION_CONFIRM_DATA_SIZE - sizeof(SceWChar16))
#define LOCATION_CONFIRM_SEPARATOR	L'\n'

/* which of the texts the confirmation data holds (the list's confirmDataArg) */
#define LOCATION_CONFIRM_DATA_FIRST	0x1
#define LOCATION_CONFIRM_DATA_SECOND	0x2

IpcClientList *IpcClientList::s_instance = NULL;
SceUInt8 IpcClientList::s_useHeap = 1;
SceInt32 IpcClientList::s_threadPriority = SCE_KERNEL_DEFAULT_PRIORITY;
SceSize IpcClientList::s_heapSize = LOCATION_HEAP_SIZE;

static const LocationAllocator s_defaultAllocator = { locationHeapAlloc, locationHeapFree };
LocationAllocator IpcClientList::s_allocator = s_defaultAllocator;
int IpcClientList::s_heapRefCount;
SceInt32 IpcClientList::s_cpuAffinityMask;
int IpcClientList::s_initState;

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

int IpcClientList::term()
{
	if (s_initState == LOCATION_STATE_NONE)
		return SCE_LOCATION_ERROR_UNINITIALIZED;

	destroyClients();
	destroyInstance();
	if (s_useHeap) {
		while (s_heapRefCount > 0) {
			if (s_heapRefCount == 1)
				locationHeapDestroy();
			s_heapRefCount--;
		}
	}
	s_initState = LOCATION_STATE_NONE;
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
	s_instance->lockId = ret;
	SceLibLocation_991AF2D0(&s_threadPriority, &s_cpuAffinityMask);
	error = SCE_LOCATION_ERROR_FATAL_ERROR;

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
			if (s_instance->clients[j]->headingThread < 0) {
				sceKernelDeleteThread(s_instance->clients[j]->locationThread);
				error = s_instance->clients[j]->headingThread;
				s_instance->clients[j]->locationThread = -1;
				s_instance->clients[j]->headingThread = -1;
				break;
			}
			error = 0;
			break;
		}
		if (error != 0) {
			destroyClients();
			return error;
		}
	}
	return 0;
}

/* the first sceLocationInit makes the heap, unless the system supplied an allocator */
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
		return ret;
	}
	return 0;
}

/* the system's sceLocationInit, before any of SceLibLocation's own */
int IpcClientList::init(SceUInt32 open, SceUInt32 mode)
{
	SceUInt32 sdkVersion;
	int ret;

	if (open == 0 || open > LOCATION_CLIENT_MAX)
		return SCE_LOCATION_ERROR_INVALID_VALUE;
	if (SceLibLocation_EFC9BD25() != NULL)
		return SCE_LOCATION_ERROR_INITIALIZED;

	/* a list left by an earlier system client */
	if (s_instance != NULL) {
		if (s_initState == LOCATION_STATE_SYSTEM)
			delete s_instance;
		s_instance = NULL;
		releaseHeap();
	}

	sdkVersion = SceLibKernel_9F793F84();
	switch (mode) {
	case LOCATION_MODE_SYSTEM:
		s_heapSize = LOCATION_HEAP_SIZE_SYSTEM;
		break;
	case SCE_LOCATION_MODE_V2:
		s_heapSize = sdkVersion < LOCATION_SDK_VERSION_1_800 ? LOCATION_HEAP_SIZE_LEGACY_V2
								     : LOCATION_HEAP_SIZE;
		break;
	default:
		s_heapSize = LOCATION_HEAP_SIZE;
		break;
	}
	if (sdkVersion >= LOCATION_SDK_VERSION_2_000 && mode == LOCATION_MODE_SYSTEM &&
	    !SceLibLocation_2311B24A())
		return SCE_LOCATION_ERROR_INVALID_VALUE;

	ret = acquireHeap(s_heapSize);
	if (ret != 0)
		return ret;

	if (s_instance == NULL) {
		s_instance = new IpcClientList;
		if (s_instance == NULL) {
			releaseHeap();
			return SCE_LOCATION_ERROR_NO_MEMORY;
		}
	}

	ret = createClients(open, mode);
	if (ret == 0) {
		s_initState = LOCATION_STATE_SYSTEM;
	} else {
		destroyInstance();
		releaseHeap();
	}
	return ret;
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

/* the requests run on SceLibLocation's list, wherever it came from */
void IpcClientList::setInstance(IpcClientList *list)
{
	s_instance = list;
}

/*
 * The request, with the system's option; when the service needs the user's permission first,
 * one wait for the location or its cancellation.
 */
int IpcClientList::getLocationWifiHistory(IpcClientInfo *info, bool query, SceRtcTick timestamp,
					  SceUInt32 timeout, SceLocationLocationInfo *linfo)
{
	SceUInt32 pattern;
	int ret;

	ret = locationServiceGetLocationWifiHistory(info->client, info->handle, query, true,
						    timestamp, timeout, linfo);
	if (ret == SCE_LOCATION_ERROR_INTERNAL_FILE_INVALID) {
		ret = info->client->waitEventFlag(0, query ? LOCATION_EVENTS_WIFI_HISTORY_QUERY :
							     LOCATION_EVENTS_WIFI_HISTORY,
						  SCE_KERNEL_EVF_WAITMODE_OR |
						  SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
						  &pattern, NULL);
		if (ret == 0) {
			if ((pattern & (LOCATION_EVENT_LOCATION | LOCATION_EVENT_QUERY_LOCATION)) != 0)
				ret = locationServiceGetLocationWifiHistoryResult(info->client,
										  info->handle,
										  query, true, linfo);
			else if ((pattern & (LOCATION_EVENT_LOCATION_CANCELED |
					     LOCATION_EVENT_QUERY_CANCELED)) != 0)
				ret = SCE_LOCATION_INFO_GET_LOCATION_CANCELED;
			else
				ret = SCE_LOCATION_ERROR_FATAL_ERROR;
		}
	}
	return ret;
}

int IpcClientList::setAllocator(const LocationAllocator *allocator, SceUInt8 useHeap)
{
	if ((s_initState | useHeap) == 0) {
		s_allocator.alloc = allocator->alloc;
		s_allocator.free = allocator->free;
		s_useHeap = useHeap;
		return 0;
	}
	return LOCATION_ERROR_ALLOCATOR_IN_USE;
}

LocationAllocator *IpcClientList::getAllocator()
{
	return &s_allocator;
}

SceUInt8 IpcClientList::getUseHeap()
{
	return s_useHeap;
}

/* a text's size in bytes, without its terminator; none for a missing text */
static inline SceSize confirmTextSize(const SceWChar16 *text)
{
	SceSize length;

	if (text == NULL)
		return 0;
	length = 0;
	while (*text++ != 0)
		length++;
	return length * sizeof(SceWChar16);
}

/*
 * The confirmation dialog's data for the system's request: the two texts, either of which may be
 * missing, with two separators between them, cut to what the data holds.
 */
int IpcClientList::setConfirmData(SceLocationHandle handle, const LocationAllocator *allocator,
				  SceUInt32 arg, const SceWChar16 *firstText,
				  const SceWChar16 *secondText)
{
	SceWChar16 separator = LOCATION_CONFIRM_SEPARATOR;
	SceSize firstSize;
	SceSize secondSize;
	SceSize offset;
	SceSize end;

	(void)handle;
	if (mode != LOCATION_MODE_SYSTEM)
		return SCE_LOCATION_ERROR_FATAL_ERROR;

	confirmArg = arg;
	useConfirmData = 1;
	if (confirmData == NULL) {
		confirmData = allocator->alloc(LOCATION_CONFIRM_DATA_SIZE);
		if (confirmData == NULL) {
			useConfirmData = 0;
			return SCE_LOCATION_ERROR_NO_MEMORY;
		}
	}
	sceClibMemset(confirmData, 0, LOCATION_CONFIRM_DATA_SIZE);

	firstSize = confirmTextSize(firstText);
	secondSize = confirmTextSize(secondText);

	if (firstText != NULL && secondText != NULL) {
		confirmDataArg = LOCATION_CONFIRM_DATA_FIRST | LOCATION_CONFIRM_DATA_SECOND;
		if (firstSize <= LOCATION_CONFIRM_TEXT_MAX) {
			sceClibMemcpy_safe(confirmData, firstText, firstSize);
			offset = firstSize + sizeof(separator);
			if (offset <= LOCATION_CONFIRM_TEXT_MAX) {
				sceClibMemcpy_safe((SceUInt8 *)confirmData + firstSize, &separator,
						   sizeof(separator));
				end = firstSize + 2 * sizeof(separator);
				if (end <= LOCATION_CONFIRM_TEXT_MAX) {
					sceClibMemcpy_safe((SceUInt8 *)confirmData + offset, &separator,
							   sizeof(separator));
					if (end + secondSize <= LOCATION_CONFIRM_TEXT_MAX)
						sceClibMemcpy_safe((SceUInt8 *)confirmData + end,
								   secondText, secondSize);
					else
						sceClibMemcpy_safe((SceUInt8 *)confirmData + end,
								   secondText,
								   LOCATION_CONFIRM_DATA_SIZE - end -
								   sizeof(SceWChar16));
				}
			}
		} else {
			sceClibMemcpy_safe(confirmData, firstText, LOCATION_CONFIRM_TEXT_MAX);
		}
	} else if (firstText != NULL) {
		confirmDataArg = LOCATION_CONFIRM_DATA_FIRST;
		if (firstSize <= LOCATION_CONFIRM_TEXT_MAX)
			sceClibMemcpy_safe(confirmData, firstText, firstSize);
		else
			sceClibMemcpy_safe(confirmData, firstText, LOCATION_CONFIRM_TEXT_MAX);
	} else if (secondText != NULL) {
		confirmDataArg = LOCATION_CONFIRM_DATA_SECOND;
		if (secondSize <= LOCATION_CONFIRM_TEXT_MAX)
			sceClibMemcpy_safe(confirmData, secondText, secondSize);
		else
			sceClibMemcpy_safe(confirmData, secondText, LOCATION_CONFIRM_TEXT_MAX);
	} else if (firstText == NULL && secondText == NULL) {
		confirmDataArg = 0;
	}
	return 0;
}

inline int IpcClientList::removeClient(SceLocationHandle handle)
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

/* the system's handles, closed here only: the service closes its side itself */
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
			ret = removeClient(info->handle);
	}
	return ret;
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
							     s_instance->mode == SCE_LOCATION_MODE_V1))
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
