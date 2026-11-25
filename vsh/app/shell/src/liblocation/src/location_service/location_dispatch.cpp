/*
 * location_dispatch.cpp: IPMI synchronous method dispatchers for "SceLocationLoc" and
 * "SceLocationHead" (0x811878D0-0x81187F14). The file's and internal names here are ours
 * (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <liblocation.h>
#include <ipmi/server.h>
#include <ipmi/session.h>
#include <ipmi/ipmi_error.h>
#include "location_service.h"

struct LocationWifiHistoryReq {
	SceLocationHandle handle;
	bool query;
	bool option;
	SceRtcTick timestamp;
	SceUInt32 timeout;
};

struct LocationWifiHistoryResultReq {
	SceLocationHandle handle;
	bool query;
	bool option;
};

/* (0x811878D0) */
int locationServerDispatch(IPMI::Session *session, SceUInt32 methodId,
			   const IPMI::DataInfo *inList, SceUInt32 inCount,
			   IPMI::BufferInfo *outList, SceUInt32 outCount)
{
	switch (methodId) {
	case LOC_SVC_METHOD_OPEN: {
		const SceUInt32 *in = (const SceUInt32 *)inList[0].pData;
		SceLocationHandle *outHandle = (SceLocationHandle *)outList[0].pBuffer;
		int *outStatus = (int *)outList[1].pBuffer;
		int appId = session->getAppId();
		int ret = locationSvcOpen(appId, outHandle, in[0], in[1], outStatus, in[2]);

		if (ret == 0 || ret == SCE_LOCATION_INFO_DENIED_BY_USER) {
			signed char slotIdx = (signed char)locationServerGetInstance()->BindHandle(
				session, *outHandle);

			headingServerGetInstance()->BindHandle(appId, slotIdx, *outHandle);
		}
		outList[0].outSize = sizeof(SceLocationHandle);
		outList[1].outSize = sizeof(int);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}
	case LOC_SVC_METHOD_CLOSE: {
		SceLocationHandle handle = *(const SceLocationHandle *)inList[0].pData;
		int appId = session->getAppId();
		int ret = locationSvcClose(appId, handle);

		headingServerGetInstance()->UnbindHandle(appId, handle);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}
	case LOC_SVC_METHOD_REOPEN: {
		const SceUInt32 *in = (const SceUInt32 *)inList[0].pData;
		int ret = locationSvcReopen(session->getAppId(), in[0], in[1], in[2]);

		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}
	case LOC_SVC_METHOD_GET_METHOD: {
		SceLocationHandle handle = *(const SceLocationHandle *)inList[0].pData;
		SceUInt32 *outLmethod = (SceUInt32 *)outList[0].pBuffer;
		SceUInt32 *outHmethod = (SceUInt32 *)outList[1].pBuffer;
		int ret = locationSvcGetMethod(session->getAppId(), handle, outLmethod, outHmethod);

		outList[0].outSize = sizeof(SceUInt32);
		outList[1].outSize = sizeof(SceUInt32);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}
	case LOC_SVC_METHOD_GET_LOCATION: {
		SceLocationHandle handle = *(const SceLocationHandle *)inList[0].pData;
		SceLocationLocationInfo *outInfo = (SceLocationLocationInfo *)outList[0].pBuffer;
		int ret = locationSvcGetLocation(session->getAppId(), handle, outInfo);

		outList[0].outSize = sizeof(SceLocationLocationInfo);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}
	case LOC_SVC_METHOD_GET_LOCATION_CACHE: {
		SceLocationHandle handle = *(const SceLocationHandle *)inList[0].pData;
		SceLocationLocationInfo *outInfo = (SceLocationLocationInfo *)outList[0].pBuffer;
		int ret = locationSvcGetLocationCache(session->getAppId(), handle, outInfo);

		outList[0].outSize = sizeof(SceLocationLocationInfo);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}
	case LOC_SVC_METHOD_CANCEL_GET_LOCATION: {
		SceLocationHandle handle = *(const SceLocationHandle *)inList[0].pData;
		int ret = locationSvcCancelGetLocation(session->getAppId(), handle, false);

		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}
	case LOC_SVC_METHOD_START_LOCATION_CALLBACK: {
		const SceUInt32 *in = (const SceUInt32 *)inList[0].pData;
		int ret = locationSvcStartLocationCallback(session->getAppId(), in[0], in[1]);

		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}
	case LOC_SVC_METHOD_STOP_LOCATION_CALLBACK: {
		SceLocationHandle handle = *(const SceLocationHandle *)inList[0].pData;
		int ret = locationSvcStopLocationCallback(session->getAppId(), handle);

		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}
	case LOC_SVC_METHOD_GET_HEADING: {
		SceLocationHandle handle = *(const SceLocationHandle *)inList[0].pData;
		SceLocationHeadingInfo *outInfo = (SceLocationHeadingInfo *)outList[0].pBuffer;
		int ret = locationSvcGetHeading(session->getAppId(), handle, outInfo);

		outList[0].outSize = sizeof(SceLocationHeadingInfo);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}
	case LOC_SVC_METHOD_START_HEADING_CALLBACK: {
		SceLocationHandle handle = *(const SceLocationHandle *)inList[0].pData;
		int ret = locationSvcStartHeadingCallback(session->getAppId(), handle);

		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}
	case LOC_SVC_METHOD_STOP_HEADING_CALLBACK: {
		SceLocationHandle handle = *(const SceLocationHandle *)inList[0].pData;
		int ret = locationSvcStopHeadingCallback(session->getAppId(), handle);

		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}
	case LOC_SVC_METHOD_CONFIRM: {
		const SceUInt32 *in = (const SceUInt32 *)inList[0].pData;
		const void *data = inList[1].pData;
		int ret = locationSvcConfirm(session->getAppId(), in[0], in[1], in[2], data);

		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}
	case LOC_SVC_METHOD_CONFIRM_GET_STATUS: {
		SceLocationHandle handle = *(const SceLocationHandle *)inList[0].pData;
		SceLocationDialogStatus *outStatus = (SceLocationDialogStatus *)outList[0].pBuffer;
		int ret = locationSvcConfirmGetStatus(session->getAppId(), handle, outStatus);

		outList[0].outSize = sizeof(SceLocationDialogStatus);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}
	case LOC_SVC_METHOD_CONFIRM_GET_RESULT: {
		SceLocationHandle handle = *(const SceLocationHandle *)inList[0].pData;
		SceLocationDialogResult *outResult = (SceLocationDialogResult *)outList[0].pBuffer;
		int ret = locationSvcConfirmGetResult(session->getAppId(), handle, outResult);

		outList[0].outSize = sizeof(SceLocationDialogResult);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}
	case LOC_SVC_METHOD_CONFIRM_ABORT: {
		SceLocationHandle handle = *(const SceLocationHandle *)inList[0].pData;
		int ret = locationSvcConfirmAbort(session->getAppId(), handle);

		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}
	case LOC_SVC_METHOD_GET_PERMISSION: {
		SceLocationHandle handle = *(const SceLocationHandle *)inList[0].pData;
		SceLocationPermissionInfo *outInfo = (SceLocationPermissionInfo *)outList[0].pBuffer;
		int ret = locationSvcGetPermission(session->getAppId(), handle, outInfo);

		outList[0].outSize = sizeof(SceLocationPermissionInfo);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}
	case LOC_SVC_METHOD_DENY_APPLICATION: {
		SceLocationHandle handle = *(const SceLocationHandle *)inList[0].pData;
		int appId = session->getAppId();
		int ret = locationSvcDenyApplication(appId, handle);

		if (ret == 0)
			locationSvcNotifyPermissionChanged(appId, handle);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}
	case LOC_SVC_METHOD_SET_GPS_EMULATION_FILE: {
		int ret = locationSvcSetGpsEmulationFile((const char *)inList[0].pData);

		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}
	case LOC_SVC_METHOD_DEBUG_CMD: {
		int cmd = *(const int *)inList[0].pData;
		const int *inArg = (const int *)inList[1].pData;
		void *outBuf = outList[0].pBuffer;
		int ret = locationSvcDebugCmd(session->getAppId(), cmd, inArg, outBuf);

		outList[0].outSize = 320;
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}
	case LOC_SVC_METHOD_GET_LOCATION_WITH_TIMEOUT: {
		const SceUInt32 *in = (const SceUInt32 *)inList[0].pData;
		SceLocationLocationInfo *outInfo = (SceLocationLocationInfo *)outList[0].pBuffer;
		int ret = locationSvcGetLocationWithTimeout(session->getAppId(), in[0], in[1],
							    outInfo);

		outList[0].outSize = sizeof(SceLocationLocationInfo);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}
	case LOC_SVC_METHOD_GET_LOCATION_WIFI_HISTORY: {
		const LocationWifiHistoryReq *req = (const LocationWifiHistoryReq *)inList[0].pData;
		SceLocationLocationInfo *outInfo = (SceLocationLocationInfo *)outList[0].pBuffer;
		int appId = session->getAppId();
		int ret;

		if (req->query)
			ret = locationSvcGetQueryLocation(appId, req->handle, req->timestamp,
							  req->timeout, req->option, outInfo);
		else
			ret = locationSvcGetLocationWifiHistory(appId, req->handle, req->timeout,
								req->option, outInfo);
		outList[0].outSize = sizeof(SceLocationLocationInfo);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}
	case LOC_SVC_METHOD_GET_LOCATION_WIFI_HISTORY_RESULT: {
		const LocationWifiHistoryResultReq *req =
			(const LocationWifiHistoryResultReq *)inList[0].pData;
		SceLocationLocationInfo *outInfo = (SceLocationLocationInfo *)outList[0].pBuffer;
		int appId = session->getAppId();
		int ret;

		if (req->query)
			ret = locationSvcGetQueryLocationCache(appId, req->handle, req->option,
							       outInfo);
		else
			ret = locationSvcGetLocationCacheWifiHistory(appId, req->handle,
								     req->option, outInfo);
		outList[0].outSize = sizeof(SceLocationLocationInfo);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}
	case LOC_SVC_METHOD_GET_VALUE: {
		int ret = locationSvcGetValue((SceUInt32 *)outList[0].pBuffer);

		outList[0].outSize = sizeof(SceUInt32);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}
	case LOC_SVC_METHOD_CANCEL_QUERY_WIFI_HISTORY: {
		SceLocationHandle handle = *(const SceLocationHandle *)inList[0].pData;
		int ret = locationSvcCancelGetLocation(session->getAppId(), handle, true);

		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}
	default:
		return IPMI_ERROR_UNKNOWN_METHOD;
	}
}

/* (0x81187DDE) */
int headingServerDispatch(IPMI::Session *session, SceUInt32 methodId,
			  const IPMI::DataInfo *inList, SceUInt32 inCount,
			  IPMI::BufferInfo *outList, SceUInt32 outCount)
{
	switch (methodId) {
	case LOC_SVC_METHOD_GET_HEADING: {
		SceLocationHandle handle = *(const SceLocationHandle *)inList[0].pData;
		SceLocationHeadingInfo *outInfo = (SceLocationHeadingInfo *)outList[0].pBuffer;
		int ret = locationSvcGetHeading(session->getAppId(), handle, outInfo);

		outList[0].outSize = sizeof(SceLocationHeadingInfo);
		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}
	case LOC_SVC_METHOD_START_HEADING_CALLBACK: {
		SceLocationHandle handle = *(const SceLocationHandle *)inList[0].pData;
		int ret = locationSvcStartHeadingCallback(session->getAppId(), handle);

		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}
	case LOC_SVC_METHOD_STOP_HEADING_CALLBACK: {
		SceLocationHandle handle = *(const SceLocationHandle *)inList[0].pData;
		int ret = locationSvcStopHeadingCallback(session->getAppId(), handle);

		return session->respondToSyncMethodRequest(ret, outList, outCount);
	}
	default:
		return IPMI_ERROR_UNKNOWN_METHOD;
	}
}
