/*
 * SceLibLocation's clients of the shell's location services: the list of handles
 * (location_client.cpp), the methods of the services (location_service_ipc_client.cpp) and the
 * library's own heap (location_heap.cpp). Sony's names: IpcClientList and IpcClientInfo are the
 * names of their locks ("SceIpcClientList", "SceIpcClientInfo"); the rest is ours.
 */
#ifndef LOCATION_CLIENT_SRC_LOCATION_CLIENT_H
#define LOCATION_CLIENT_SRC_LOCATION_CLIENT_H

#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <kernel/threadmgr.h>
#include <liblocation.h>
#include <liblocation/liblocation_internal.h>
#include <rtc.h>
#include "../../../../../../../lowlayer/src/ipmi/client.h"

/* the services: one location client per handle, one heading client for all of them */
#define LOCATION_SERVICE_NAME_LOCATION		"SceLocationLoc"
#define LOCATION_SERVICE_NAME_HEADING		"SceLocationHead"

/* the handles one sceLocationInit can open */
#define LOCATION_CLIENT_MAX			8

/* sceLocationInit's mode: the interface version (low byte) and the way permission is got */
#define LOCATION_MODE_VERSION_MASK		0x000000FFU
#define LOCATION_MODE_CONFIRM_MASK		0xFFFFFF00U
/* the mode the system's own client (SceLibLocation_7F74AA94) runs the services in */
#define LOCATION_MODE_SYSTEM			0x00010000U
/* set for a process built with an SDK before 2.100: the services answer it the old way */
#define LOCATION_MODE_LEGACY_SDK		0x80000000U

/* a location method only the system's own client may use */
#define LOCATION_LMETHOD_SYSTEM			0x00000100U

/* sceLocationGetLocation's timeout */
#define LOCATION_GET_LOCATION_TIMEOUT		15

/* the event bits the location service sets on a client's event flag 0 */
#define LOCATION_EVENT_CALLBACK_LOCATION	0x00000001U
#define LOCATION_EVENT_LOCATION			0x00000002U
#define LOCATION_EVENT_LOCATION_CANCELED	0x00000004U
/* sceLocationQueryLocationWifiHistory's location, and its cancellation (liblocation_extension) */
#define LOCATION_EVENT_QUERY_LOCATION		0x00000008U
#define LOCATION_EVENT_QUERY_CANCELED		0x00000010U
#define LOCATION_EVENT_CONFIRM_ALLOWED		0x00000020U
#define LOCATION_EVENT_CONFIRM_RUNNING		0x00000040U
#define LOCATION_EVENT_CONFIRM_DENIED		0x00000080U
#define LOCATION_EVENT_CONFIRM_CLOSED		0x00000100U
#define LOCATION_EVENT_PERMISSION_ALLOWED	0x00000200U
/* the bit the heading service sets, with the handle, when a heading callback is due */
#define LOCATION_EVENT_HEADING			0x10000000U

/* how long a callback thread waits for its event before looking at its state again */
#define LOCATION_CALLBACK_WAIT_USEC		10000

#define LOCATION_CALLBACK_THREAD_STACK_SIZE	0x1000

/* the library's heap: its size for a V2 process built with an SDK before 1.800, and otherwise */
#define LOCATION_HEAP_SIZE			0x1000
#define LOCATION_HEAP_SIZE_LEGACY_V2		0x2000

/* the SDK versions the library answers differently for */
#define LOCATION_SDK_VERSION_1_500		0x01500000U
#define LOCATION_SDK_VERSION_1_800		0x01800000U
#define LOCATION_SDK_VERSION_2_000		0x02000000U
#define LOCATION_SDK_VERSION_2_100		0x02100000U

/* SceLibLocation_822BBF06: the allocator cannot be replaced once the library is initialized */
#define LOCATION_ERROR_ALLOCATOR_IN_USE		(-2146430321)	/* 0x8010128F */

/* the library's sceLocationInit state */
#define LOCATION_STATE_NONE			0
#define LOCATION_STATE_INITIALIZED		1
/* initialized by the system's own client, which supplies the list (SceLibLocation_7F74AA94) */
#define LOCATION_STATE_SYSTEM			2

/* where the library's memory comes from (SceLibLocation_822BBF06 supplies another) */
struct LocationAllocator {
	void *(*alloc)(SceSize size);
	void (*free)(void *pointer);
};

/* one handle: its location client and the callbacks running on it */
struct IpcClientInfo {
	SceLocationHandle handle;				/* 0x00 */
	IPMI::Client *client;					/* 0x04 */
	SceLocationLocationInfoCallback locationCallback;	/* 0x08 */
	SceLocationHeadingInfoCallback headingCallback;		/* 0x0c */
	int locationCallbackStarted;				/* 0x10 */
	int headingCallbackStarted;				/* 0x14 */
	void *locationUserdata;					/* 0x18 */
	void *headingUserdata;					/* 0x1c */
	int locationThreadRunning;				/* 0x20 */
	int headingThreadRunning;				/* 0x24 */
	SceUID locationThread;					/* 0x28 */
	SceUID headingThread;					/* 0x2c */
	SceUID lockId;						/* 0x30 */
	SceKernelLwMutexWork lock;				/* 0x38 */
};

/* the library's handles, and the confirmation dialog they share */
class IpcClientList {
public:
	IpcClientInfo *clients[LOCATION_CLIENT_MAX];		/* 0x00 */
	SceLocationDialogStatus confirmStatus;			/* 0x20 */
	SceLocationDialogResult confirmResult;			/* 0x24 */
	/* what the system's client passes the confirmation dialog (liblocation_internal) */
	SceUInt32 confirmArg;					/* 0x28 */
	void *confirmData;					/* 0x2c */
	SceUInt32 confirmDataArg;				/* 0x30 */
	SceUID lockId;						/* 0x34 */
	SceKernelLwMutexWork lock;				/* 0x38 */
	int applicationDisabled;				/* 0x58 */
	SceUInt8 openCount;					/* 0x5c */
	SceUInt32 clientCount;					/* 0x60 */
	SceUInt32 mode;						/* 0x64 */
	int useConfirmData;					/* 0x68 */
	IPMI::Client *headingClient;				/* 0x6c */
#ifndef LOCATION_INTERNAL
	/* not in liblocation_internal's build of the list (0x70 bytes there) */
	int confirmPending;					/* 0x70 */
#endif

	IpcClientList()
	{
		sceClibMemset(clients, 0, sizeof(clients));
		confirmStatus = SCE_LOCATION_DIALOG_STATUS_IDLE;
		confirmResult = SCE_LOCATION_DIALOG_RESULT_NONE;
		confirmArg = 0;
		confirmData = NULL;
		applicationDisabled = 0;
		lockId = -1;
		openCount = 0;
		clientCount = 0;
		mode = 0;
		useConfirmData = 0;
		headingClient = NULL;
#ifndef LOCATION_INTERNAL
		confirmPending = 0;
#endif
	}

	~IpcClientList()
	{
		sceClibMemset(clients, 0, sizeof(clients));
	}

	/* the list lives in the library's allocator */
	static void *operator new(size_t size) throw()
	{
		return s_allocator.alloc(size);
	}

	static void operator delete(void *pointer)
	{
		s_allocator.free(pointer);
	}

	static IpcClientList *s_instance;
	static SceUInt8 s_useHeap;
	static SceInt32 s_threadPriority;
	static SceSize s_heapSize;
	static LocationAllocator s_allocator;
	static int s_heapRefCount;
	static SceInt32 s_cpuAffinityMask;
	static int s_initState;
	static bool s_extensionLoaded;

	static int init(SceUInt32 open, SceUInt32 mode);
	static int closeAll();
	static int term();
	static int removeClient(SceLocationHandle handle);
	static int allocClient(IpcClientInfo **ppInfo);
	static IpcClientList *getInstance();
	static IpcClientInfo *find(SceLocationHandle handle);
	static void setInstance(IpcClientList *list);
	static int setAllocator(const LocationAllocator *allocator, SceUInt8 useHeap);
	static LocationAllocator *getAllocator();
	static int createTemporaryClient(IpcClientInfo **ppInfo);
	static int destroyTemporaryClient(IpcClientInfo **ppInfo);
	static int setThreadParameter(SceInt32 priority, SceInt32 cpuAffinityMask);
	static void getThreadParameter(SceInt32 *pPriority, SceInt32 *pCpuAffinityMask);
	static bool isSystemInitialized();
	static bool isInitialized();
	/* liblocation_internal's */
	static SceUInt8 getUseHeap();

	int connectLocation(IpcClientInfo *info);
	int close(IpcClientInfo *info);
	int open(SceLocationHandle *handle, IpcClientInfo *info, SceUInt32 lmethod,
		 SceUInt32 hmethod);
	int reopen(IpcClientInfo *info, SceUInt32 lmethod, SceUInt32 hmethod);
	int getMethod(IpcClientInfo *info, SceUInt32 *lmethod, SceUInt32 *hmethod);
	int getLocation(IpcClientInfo *info, SceUInt32 timeout, SceLocationLocationInfo *linfo);
	int cancelGetLocation(IpcClientInfo *info);
	int startLocationCallback(IpcClientInfo *info, SceUInt32 distance,
				  SceLocationLocationInfoCallback callback, void *userdata);
	int stopLocationCallback(IpcClientInfo *info);
	int getHeading(IpcClientInfo *info, SceLocationHeadingInfo *hinfo);
	int startHeadingCallback(IpcClientInfo *info, SceFloat32 difference,
				 SceLocationHeadingInfoCallback callback, void *userdata);
	int stopHeadingCallback(IpcClientInfo *info);
	int confirm(IpcClientInfo *info);
	int confirmGetStatus(IpcClientInfo *info, SceLocationDialogStatus *status);
	int confirmGetResult(IpcClientInfo *info, SceLocationDialogResult *result);
	int confirmAbort(IpcClientInfo *info);
	int getPermission(IpcClientInfo *info, SceLocationPermissionInfo *permission);
	int denyApplication(IpcClientInfo *info);
	int connectTemporary(IpcClientInfo *info);
	int setGpsEmulationFile(IpcClientInfo *info, const char *path);
	int getServiceValue(IpcClientInfo *info, SceUInt32 *value);
	SceUInt32 getMode();
	void setConfirmPending(int pending);
	/* liblocation_internal's */
	int getLocationWifiHistory(IpcClientInfo *info, bool query, SceRtcTick timestamp,
				   SceUInt32 timeout, SceLocationLocationInfo *linfo);
	int setConfirmData(SceLocationHandle handle, const LocationAllocator *allocator,
			   SceUInt32 arg, const SceWChar16 *firstText, const SceWChar16 *secondText);

private:
	static int destroyClients();
	static int createClients(SceUInt32 count, SceUInt32 mode);
	static int acquireHeap(SceSize size);
	static void releaseHeap();
	static void destroyInstance();
	static int closeHeading(IpcClientInfo *info);
	static int closeLocation(IpcClientInfo *info);
	static int connectHeading(IpcClientInfo *info);
	static SceInt32 locationCallbackThread(SceSize argSize, void *argBlock);
	static SceInt32 headingCallbackThread(SceSize argSize, void *argBlock);
	int waitConfirmDialog(IPMI::Client *client);
};

/* location_heap.cpp: the heap sceLocationInit makes unless an allocator was supplied */
int locationHeapCreate(SceSize size);
void locationHeapDestroy();
void *locationHeapAlloc(SceSize size);
void locationHeapFree(void *pointer);

/* SceThreadmgrCoredumpTime 0C8A38E1: the callback threads end through it */
extern "C" int SceThreadmgrCoredumpTime_sceKernelExitThread(int status);

/* the exports the library's other modules use (location.cpp) */
extern "C" {
IpcClientList *SceLibLocation_EFC9BD25(void);
void SceLibLocation_7F74AA94(IpcClientList *list);
LocationAllocator *SceLibLocation_0C0BB66A(void);
SceInt32 SceLibLocation_822BBF06(const LocationAllocator *allocator, SceUInt32 useHeap);
void SceLibLocation_991AF2D0(SceInt32 *pPriority, SceInt32 *pCpuAffinityMask);
IpcClientInfo *SceLibLocation_4C8B6F21(SceLocationHandle handle);
SceUInt32 SceLibLocation_0341758B(void);
void SceLibLocation_5C7185D2(void);
bool SceLibLocation_2311B24A(void);
void SceLibLocation_41C1E68F(int pending);
}

#endif
