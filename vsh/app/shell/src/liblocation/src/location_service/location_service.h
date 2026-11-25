/*
 * location_service.h: the shell's location and heading IPMI service ("SceLocationLoc" and
 * "SceLocationHead"), statically linked into SceShell (0x81185130-0x81187F14+). The file's and
 * internal names here are ours (PLACEHOLDER).
 */
#ifndef LOCATION_SERVICE_H
#define LOCATION_SERVICE_H

#include <stddef.h>
#include <scetypes.h>
#include <liblocation.h>
#include <rtc.h>
#include <kernel/threadmgr.h>
#include <memory/heap_allocator.h>
#include <ipmi/server.h>
#include <ipmi/session.h>
#include "../../../livespace_db/livespace_db_internal.h"
#include "../../../livespace_db/lsdb_keys.h"

/* the location service's private heap (0x815496B0; PLACEHOLDER name) */
extern paf::memory::HeapAllocator *g_locationHeap;

/* the location service's IPC method IDs (matching location_service_ipc_client.h) */
#define LOC_SVC_METHOD_OPEN				0x12340000
#define LOC_SVC_METHOD_CLOSE				0x12340001
#define LOC_SVC_METHOD_REOPEN				0x12340002
#define LOC_SVC_METHOD_GET_METHOD			0x12340003
#define LOC_SVC_METHOD_GET_LOCATION			0x12340004
#define LOC_SVC_METHOD_GET_LOCATION_CACHE		0x12340005
#define LOC_SVC_METHOD_CANCEL_GET_LOCATION		0x12340006
#define LOC_SVC_METHOD_START_LOCATION_CALLBACK		0x12340007
#define LOC_SVC_METHOD_STOP_LOCATION_CALLBACK		0x12340008
#define LOC_SVC_METHOD_GET_HEADING			0x12340009
#define LOC_SVC_METHOD_START_HEADING_CALLBACK		0x1234000A
#define LOC_SVC_METHOD_STOP_HEADING_CALLBACK		0x1234000B
#define LOC_SVC_METHOD_CONFIRM				0x1234000C
#define LOC_SVC_METHOD_CONFIRM_GET_STATUS		0x1234000D
#define LOC_SVC_METHOD_CONFIRM_GET_RESULT		0x1234000E
#define LOC_SVC_METHOD_CONFIRM_ABORT			0x1234000F
#define LOC_SVC_METHOD_GET_PERMISSION			0x12340010
#define LOC_SVC_METHOD_DENY_APPLICATION			0x12340011
#define LOC_SVC_METHOD_SET_GPS_EMULATION_FILE		0x12340012
#define LOC_SVC_METHOD_DEBUG_CMD			0x12340013
#define LOC_SVC_METHOD_GET_LOCATION_WITH_TIMEOUT	0x12340014
#define LOC_SVC_METHOD_GET_LOCATION_WIFI_HISTORY	0x12340015
#define LOC_SVC_METHOD_GET_LOCATION_WIFI_HISTORY_RESULT	0x12340016
#define LOC_SVC_METHOD_GET_VALUE			0x12340017
#define LOC_SVC_METHOD_CANCEL_QUERY_WIFI_HISTORY	0x12340018

/* the maximum number of simultaneous location (9) and heading (8) sessions */
#define LOCATION_SERVER_MAX_SESSIONS	9
#define HEADING_SERVER_MAX_SESSIONS	8

struct LocationSessionSlot {
	IPMI::Session *session;
	SceLocationHandle handle;
	int requestId;
	int methodId;
};

class LocationServer : public IPMI::Server::EventHandler {
public:
	LocationServer();
	virtual ~LocationServer();

	virtual void onSyncMethodDispatch(IPMI::Session *pSession, SceUInt32 methodId,
					  const IPMI::DataInfo *pInList, SceUInt32 inCount,
					  IPMI::BufferInfo *pOutList, SceUInt32 outCount);
	virtual void onAsyncMethodDispatch(IPMI::Session *pSession, SceUInt32 requestId,
					   SceUInt32 methodId, const IPMI::DataInfo *pInList,
					   SceUInt32 inCount);
	virtual int onNewClient(IPMI::Server *pServer, const IPMI::Session::Config *pConfig,
				const void *pParam, SceSize paramSize);
	virtual void onSessionClosedByClient(IPMI::Session *pSession);
	virtual void onSessionKilled(IPMI::Session *pSession);

	int Start();
	int Stop();
	void NotifyEvent(SceLocationHandle handle, int result, int eventType);
	void SetEventFlag(SceLocationHandle handle, SceUInt32 pattern);
	int BindHandle(IPMI::Session *session, SceLocationHandle handle);
	void NotifyDenied(SceLocationHandle handle);

	static int ThreadEntry(SceSize args, void *argp);

	IPMI::Server *m_server;
	LocationSessionSlot m_slots[LOCATION_SERVER_MAX_SESSIONS];
	int m_sessionCount;
	int m_reserved9C;
	SceUID m_threadId;
	int m_reservedA4;
	SceKernelLwMutexWork m_mutex;
	int m_mutexResult;
	int m_reservedCC;
	int m_stopped;
	void *m_workBuffer;
	SceSize m_workBufferSize;
};

struct HeadingSessionSlot {
	int appId;
	SceUInt32 handleCount;
	IPMI::Session *session;
	SceLocationHandle handles[8];
	int requestId;
	int methodId;
};

class HeadingServer : public IPMI::Server::EventHandler {
public:
	HeadingServer();
	virtual ~HeadingServer();

	virtual void onSyncMethodDispatch(IPMI::Session *pSession, SceUInt32 methodId,
					  const IPMI::DataInfo *pInList, SceUInt32 inCount,
					  IPMI::BufferInfo *pOutList, SceUInt32 outCount);
	virtual void onAsyncMethodDispatch(IPMI::Session *pSession, SceUInt32 requestId,
					   SceUInt32 methodId, const IPMI::DataInfo *pInList,
					   SceUInt32 inCount);
	virtual int onNewClient(IPMI::Server *pServer, const IPMI::Session::Config *pConfig,
				const void *pParam, SceSize paramSize);
	virtual void onSessionClosedByClient(IPMI::Session *pSession);
	virtual void onSessionKilled(IPMI::Session *pSession);

	int Start();
	int Stop();
	void NotifyHeading(SceLocationHandle handle, int unused);
	void BindHandle(int appId, signed char slotIdx, SceLocationHandle handle);
	void UnbindHandle(int appId, SceLocationHandle handle);

	static int ThreadEntry(SceSize args, void *argp);

	IPMI::Server *m_server;
	HeadingSessionSlot m_slots[HEADING_SERVER_MAX_SESSIONS];
	int m_sessionCount;
	SceUID m_threadId;
	SceKernelLwMutexWork m_mutex;
	int m_mutexResult;
	int m_stopped;
	void *m_workBuffer;
	SceSize m_workBufferSize;
};

/* location_server.cpp */
int locationServerStart(void);
int locationServerStop(void);
LocationServer *locationServerGetInstance(void);
int headingServerStart(void);
int headingServerStop(void);
HeadingServer *headingServerGetInstance(void);

/* location_dispatch.cpp */
int locationServerDispatch(IPMI::Session *session, SceUInt32 methodId,
			   const IPMI::DataInfo *inList, SceUInt32 inCount,
			   IPMI::BufferInfo *outList, SceUInt32 outCount);
int headingServerDispatch(IPMI::Session *session, SceUInt32 methodId,
			  const IPMI::DataInfo *inList, SceUInt32 inCount,
			  IPMI::BufferInfo *outList, SceUInt32 outCount);

/* location_service.cpp (0x8118579E-0x81186730) */
int locationSvcOpen(int appId, SceLocationHandle *handle, SceUInt32 lmethod, SceUInt32 hmethod,
		    int *status, SceUInt32 mode);
int locationSvcClose(int appId, SceLocationHandle handle);
int locationSvcReopen(int appId, SceLocationHandle handle, SceUInt32 lmethod, SceUInt32 hmethod);
int locationSvcGetMethod(int appId, SceLocationHandle handle, SceUInt32 *lmethod,
			 SceUInt32 *hmethod);
int locationSvcGetLocation(int appId, SceLocationHandle handle, SceLocationLocationInfo *info);
int locationSvcGetLocationWithTimeout(int appId, SceLocationHandle handle, SceUInt32 timeout,
				      SceLocationLocationInfo *info);
int locationSvcGetLocationCache(int appId, SceLocationHandle handle,
				SceLocationLocationInfo *info);
int locationSvcCancelGetLocation(int appId, SceLocationHandle handle, bool isQuery);
int locationSvcGetLocationWifiHistory(int appId, SceLocationHandle handle, SceUInt32 timeout,
				      bool option, SceLocationLocationInfo *info);
int locationSvcGetLocationCacheWifiHistory(int appId, SceLocationHandle handle, bool option,
					   SceLocationLocationInfo *info);
int locationSvcGetQueryLocation(int appId, SceLocationHandle handle, SceRtcTick timestamp,
				SceUInt32 timeout, bool option, SceLocationLocationInfo *info);
int locationSvcGetQueryLocationCache(int appId, SceLocationHandle handle, bool option,
				     SceLocationLocationInfo *info);
int locationSvcStartLocationCallback(int appId, SceLocationHandle handle, SceUInt32 distance);
int locationSvcStopLocationCallback(int appId, SceLocationHandle handle);
int locationSvcGetHeading(int appId, SceLocationHandle handle, SceLocationHeadingInfo *info);
int locationSvcStartHeadingCallback(int appId, SceLocationHandle handle);
int locationSvcStopHeadingCallback(int appId, SceLocationHandle handle);
int locationSvcConfirm(int appId, SceLocationHandle handle, SceUInt32 arg, SceUInt32 dataArg,
		       const void *data);
int locationSvcConfirmGetResult(int appId, SceLocationHandle handle,
				SceLocationDialogResult *result);
int locationSvcConfirmGetStatus(int appId, SceLocationHandle handle,
				SceLocationDialogStatus *status);
int locationSvcConfirmAbort(int appId, SceLocationHandle handle);
int locationSvcGetPermission(int appId, SceLocationHandle handle,
			     SceLocationPermissionInfo *info);
int locationSvcDenyApplication(int appId, SceLocationHandle handle);
int locationSvcSetGpsEmulationFile(const char *path);
int locationSvcDebugCmd(int appId, int cmd, const int *inArg, void *outBuf);
int locationSvcNotifyPermissionChanged(int appId, SceLocationHandle handle);
int locationSvcSetPermissionMode(int mode);
int locationSvcSetAppPermission(const char *titleId, bool reload);
int locationSvcGetValue(SceUInt32 *value);

#endif
