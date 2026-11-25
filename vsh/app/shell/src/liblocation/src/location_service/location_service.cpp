/*
 * location_service.cpp: the shell's location service startup (liblocationInit,
 * liblocationWaitReady), GPS flying-start app event hook, and service entry points called by
 * the IPMI dispatchers (0x81185130-0x811868F4). The file's and internal names here are ours
 * (PLACEHOLDER).
 */
#include "location_internal.h"
#include <string.h>
#include <kernel/iofilemgr.h>
#include <appmgr/appmgr.h>
#include <vshbridge/vshbridge.h>
#include <plugin.h>
#include <std/string>
#include <std/string.h>
#include <std/new>
#include <common/shared_ptr.h>
#include <system/system.h>
#include <datetime/datetime.h>
#include "../../../../shell.h"
#include "../../../../plugins.h"

extern "C" SceUInt32 vshSblQafMgrIsAllowLiblocationChangeModel(void);

using namespace paf;

/* the private heap (0x815496B0) and location manager (0x815496B4) */
memory::HeapAllocator *g_locationHeap;
LocationTimerMgr *g_locationManager;

static int locationNotifyEventCB(SceLocationHandle handle, int result, int eventType);
static int locationNotifyHeadingCB(SceLocationHandle handle, int unused);
static void locationConfirmCB(int appId, int handle, int dialogType, int result, int canceled);
static void locationAppEventCB(int event, const void *data, SceSize size, void *arg);

/* (0x81185130) */
__attribute__((noinline))
static int locationGpsFlyingStart(const LsdbAppInfoPtr *appInfo, int event)
{
	int ret = 0;
	PositionManager *ctx = locationGetContext();

	if (ctx == NULL)
		return ret;

	LocationLogger *logger = ctx->m_logger;

	if (logger != NULL)
		logger->Log(8, "sceLocationGpsFlyingStart() called\n");

	LsdbAppInfo *info = appInfo->get();

	if (info->find(0xA9492ECAu) != info->end()) {
		LsdbAppInfo::iterator it = info->find(LSDB_KEY_TITLE_ID_SFO);

		if (it != info->end()) {
			const char *src = "";

			if (it->second.type == LSDB_VALUE_TEXT && it->second.text != NULL)
				src = it->second.text->c_str();

			char titleId[32];

			sce_paf_strncpy(titleId, src, 31);
			if (sce_paf_strncmp(titleId, "NPXS10918", 32) != 0 &&
			    sce_paf_strncmp(titleId, "NPXS10859", 32) != 0 &&
			    sce_paf_strncmp(titleId, "NPXS10861", 32) != 0) {
				if (event == 3) {
					ctx->FlyingStartResume(titleId);
					ret = ctx->UpdateFlyingStart();
				} else if (event == 0) {
					ret = ctx->FlyingStartLaunch(titleId);
				} else if (event == -1) {
					ret = ctx->UpdateFlyingStart();
				}
			}
		}
	}
	if (logger != NULL)
		logger->Log(8, "sceLocationGpsFlyingStart() result:0x%08x\n", ret);
	return ret;
}

/* (0x811852CA) */
static void locationAppEventCB(int event, const void *data, SceSize size, void *arg)
{
	LsdbAppInfoPtr appInfo(new LsdbAppInfo());
	Plugin *topmenu = Plugin::Find("topmenu_plugin");

	if (topmenu != NULL) {
		ShellPluginInterface *iface = (ShellPluginInterface *)topmenu->GetInterface(1);

		if (iface != NULL) {
			typedef int (*GetAppInfoFn)(LsdbAppInfoPtr *out, SceUID appId);
			GetAppInfoFn getAppInfo =
				(GetAppInfoFn)iface->func[TOPMENU_PLUGIN_FUNC_GET_APP_INFO];

			if (getAppInfo(&appInfo, (SceUID)(intptr_t)data) == 1)
				locationGpsFlyingStart(&appInfo, event);
		}
	}
}

/* (0x811853E0) */
int liblocationInit(void)
{
	memory::HeapAllocator::Option opt;

	memset(&opt, 0, sizeof(opt));
	opt.disable_dbgprint = true;
	opt.memblk_type = memory::AllocationBlockType_User;
	opt.alignment = 0x8000;

	memory::HeapAllocator *heap;

	if (system::Model::GetType() != 0)
		heap = new memory::HeapAllocator(NULL, 0x12800, "SceLiblocationPrivateHeap", &opt);
	else
		heap = new memory::HeapAllocator(NULL, 0x80000, "SceLiblocationPrivateHeap", &opt);
	if (heap == NULL)
		return SCE_LOCATION_ERROR_NO_MEMORY;

	g_locationHeap = heap;
	sceIoMkdir("ur0:liblocation", 6);

	Timer *timer = new (g_locationHeap) Timer(1000.0f);
	int ret;

	if (timer == NULL) {
		ret = SCE_LOCATION_ERROR_NO_SERVER_MEMORY;
	} else {
		LocationTimerMgr *mgr = new (g_locationHeap) LocationTimerMgr(timer);

		if (mgr == NULL) {
			ret = SCE_LOCATION_ERROR_NO_SERVER_MEMORY;
		} else {
			ret = mgr->Init();
			if (ret != 0) {
				delete mgr;
			} else {
				g_locationManager = mgr;
			}
			if (ret == 0) {
				ret = locationServerStart();
				if (ret != 0) {
					locationServerStop();
					if (g_locationManager != NULL) {
						delete g_locationManager;
						g_locationManager = NULL;
					}
					if (g_locationHeap != NULL) {
						delete g_locationHeap;
						g_locationHeap = NULL;
					}
				}
			}
			if (ret == 0) {
				ret = headingServerStart();
				if (ret != 0) {
					locationServerStop();
					headingServerStop();
					if (g_locationManager != NULL) {
						delete g_locationManager;
						g_locationManager = NULL;
					}
					if (g_locationHeap != NULL) {
						delete g_locationHeap;
						g_locationHeap = NULL;
					}
				}
			}
			if (ret == 0) {
				ret = locationPermSvcStart();
				if (ret != 0) {
					locationServerStop();
					headingServerStop();
					if (g_locationManager != NULL) {
						delete g_locationManager;
						g_locationManager = NULL;
					}
					if (g_locationHeap != NULL) {
						delete g_locationHeap;
						g_locationHeap = NULL;
					}
				}
			}
			if (ret == 0) {
				ret = locationCoreInit(locationNotifyEventCB,
						       locationNotifyHeadingCB);
				if (ret == 0) {
					PositionManager *ctx = locationGetContext();

					if (ctx != NULL)
						ret = ctx->ReloadConfig();
				}
				if (ret != 0) {
					locationServerStop();
					headingServerStop();
					locationPermSvcStop();
					if (g_locationManager != NULL) {
						delete g_locationManager;
						g_locationManager = NULL;
					}
					if (g_locationHeap != NULL) {
						delete g_locationHeap;
						g_locationHeap = NULL;
					}
				}
			}
			if (ret == 0) {
				ret = locationConfirmInit(locationConfirmCB);
				if (ret != 0) {
					locationCoreTerm();
					locationServerStop();
					headingServerStop();
					locationPermSvcStop();
					if (g_locationManager != NULL) {
						delete g_locationManager;
						g_locationManager = NULL;
					}
					if (g_locationHeap != NULL) {
						delete g_locationHeap;
						g_locationHeap = NULL;
					}
				}
			}
		}
	}

	if (system::Model::GetType() == 0) {
		if (ret == 0) {
			ret = locationProviderInit();
			if (ret != 0) {
				locationConfirmTerm();
				locationCoreTerm();
				locationServerStop();
				headingServerStop();
				locationPermSvcStop();
				if (g_locationManager != NULL) {
					delete g_locationManager;
					g_locationManager = NULL;
				}
				if (g_locationHeap != NULL) {
					delete g_locationHeap;
					g_locationHeap = NULL;
				}
			}
		}
		shellAppEventServerAddListener(3, locationAppEventCB, NULL);
		shellAppEventServerAddListener(0, locationAppEventCB, NULL);
	}
	return ret;
}

/* (0x8118579E) */
int locationSvcOpen(int appId, SceLocationHandle *handle, SceUInt32 lmethod, SceUInt32 hmethod,
		    int *status, SceUInt32 mode)
{
	int ret = SCE_LOCATION_ERROR_FATAL_ERROR;
	PositionManager *ctx = locationGetContext();

	if (ctx != NULL) {
		SceUInt32 lm = lmethod;
		SceUInt32 hm = hmethod;
		SceUInt32 md = mode;

		ret = ctx->Open(appId, handle, &lm, &hm, &md);
		*status = ctx->GetHandleStatus(appId, *handle);
	}
	return ret;
}

/* (0x8118581E) */
int locationSvcClose(int appId, SceLocationHandle handle)
{
	int ret = SCE_LOCATION_ERROR_FATAL_ERROR;
	LocationConfirmMgr *confirm = locationConfirmGetInstance();

	if (confirm == NULL)
		return ret;
	confirm->CloseApp(appId, handle);

	PositionManager *ctx = locationGetContext();

	if (ctx != NULL)
		return ctx->Close(appId, handle);
	return ret;
}

/* (0x81185858) */
int locationSvcReopen(int appId, SceLocationHandle handle, SceUInt32 lmethod, SceUInt32 hmethod)
{
	PositionManager *ctx = locationGetContext();

	if (ctx != NULL) {
		SceUInt32 lm = lmethod;
		SceUInt32 hm = hmethod;

		return ctx->Reopen(appId, handle, &lm, &hm);
	}
	return SCE_LOCATION_ERROR_FATAL_ERROR;
}

/* (0x811858B0) */
int locationSvcGetMethod(int appId, SceLocationHandle handle, SceUInt32 *lmethod,
			 SceUInt32 *hmethod)
{
	SceUInt32 lm = 0;
	SceUInt32 hm = 0;
	int ret = SCE_LOCATION_ERROR_FATAL_ERROR;
	PositionManager *ctx = locationGetContext();

	if (ctx != NULL) {
		ret = ctx->GetMethod(appId, handle, &lm, &hm);
		if (ret == 0) {
			*lmethod = lm;
			*hmethod = hm;
		}
	}
	return ret;
}

static inline void locationLogInfo(LocationLogger *logger, const char *fmt,
				   const SceLocationLocationInfo *info)
{
	if (logger != NULL) {
		SceDateTime time;

		sceRtcSetTick(&time, &info->timestamp);
		logger->Log(8, fmt,
			    info->latitude, info->longitude, info->altitude,
			    (double)info->accuracy, (double)info->speed, (double)info->direction,
			    info->timestamp.tick,
			    time.year, time.month, time.day,
			    time.hour, time.minute, time.second, time.microsecond / 1000);
	}
}

/* (0x81185916) */
int locationSvcGetLocation(int appId, SceLocationHandle handle, SceLocationLocationInfo *info)
{
	int ret = SCE_LOCATION_ERROR_FATAL_ERROR;
	PositionManager *ctx = locationGetContext();

	if (ctx != NULL) {
		ret = ctx->GetLocation(appId, handle, 15, info, false);
		locationLogInfo(ctx->m_logger,
				"[GetLocation] latitude[%f] longitude[%f] altitude[%f] accuracy[%f] "
				"speed[%f] direction[%f] timestamp[0x%016llx]:"
				"[%d/%02d/%02d %02d:%02d:%02d.%03d]\n",
				info);
	}
	return ret;
}

/* (0x81185A04) */
int locationSvcGetLocationWithTimeout(int appId, SceLocationHandle handle, SceUInt32 timeout,
				      SceLocationLocationInfo *info)
{
	int ret = SCE_LOCATION_ERROR_FATAL_ERROR;
	PositionManager *ctx = locationGetContext();

	if (ctx != NULL) {
		ret = ctx->GetLocation(appId, handle, timeout, info, false);
		locationLogInfo(ctx->m_logger,
				"[GetLocationWithTimeout] latitude[%f] longitude[%f] altitude[%f] "
				"accuracy[%f] speed[%f] direction[%f] timestamp[0x%016llx]:"
				"[%d/%02d/%02d %02d:%02d:%02d.%03d]\n",
				info);
	}
	return ret;
}

/* (0x81185AF8) */
int locationSvcGetLocationCache(int appId, SceLocationHandle handle,
				SceLocationLocationInfo *info)
{
	int ret = SCE_LOCATION_ERROR_FATAL_ERROR;
	PositionManager *ctx = locationGetContext();

	if (ctx != NULL) {
		ret = ctx->GetLocationCache(appId, handle, info, false, false);
		locationLogInfo(ctx->m_logger,
				"[GetLocationCache] latitude[%f] longitude[%f] altitude[%f] "
				"accuracy[%f] speed[%f] direction[%f] timestamp[0x%016llx]:"
				"[%d/%02d/%02d %02d:%02d:%02d.%03d]\n",
				info);
	}
	return ret;
}

/* (0x81185BE8) */
int locationSvcCancelGetLocation(int appId, SceLocationHandle handle, bool isQuery)
{
	int ret = SCE_LOCATION_ERROR_FATAL_ERROR;
	PositionManager *ctx = locationGetContext();

	if (ctx != NULL) {
		if (isQuery)
			ret = ctx->CancelQueryLocation(appId, handle);
		else
			ret = ctx->CancelGetLocation(appId, handle);
		if (ret != 0) {
			if (ret == SCE_LOCATION_INFO_GET_LOCATION_CANCELED)
				return 0;
		} else {
			LocationServer *server = locationServerGetInstance();

			if (server != NULL)
				server->SetEventFlag(handle, isQuery ? 16 : 4);
			else
				return SCE_LOCATION_ERROR_FATAL_ERROR;
		}
	}
	return ret;
}

/* (0x81185C4E) */
int locationSvcGetLocationWifiHistory(int appId, SceLocationHandle handle, SceUInt32 timeout,
				      bool option, SceLocationLocationInfo *info)
{
	(void)option;
	int ret = SCE_LOCATION_ERROR_FATAL_ERROR;
	PositionManager *ctx = locationGetContext();

	if (ctx != NULL) {
		ret = ctx->GetLocation(appId, handle, timeout, info, true);
		locationLogInfo(ctx->m_logger,
				"[GetLocationWifiHistory] latitude[%f] longitude[%f] altitude[%f] "
				"accuracy[%f] speed[%f] direction[%f] timestamp[0x%016llx]:"
				"[%d/%02d/%02d %02d:%02d:%02d.%03d]\n",
				info);
	}
	return ret;
}

/* (0x81185D44) */
int locationSvcGetLocationCacheWifiHistory(int appId, SceLocationHandle handle, bool option,
					   SceLocationLocationInfo *info)
{
	int ret = SCE_LOCATION_ERROR_FATAL_ERROR;
	PositionManager *ctx = locationGetContext();

	if (ctx != NULL) {
		ret = ctx->GetLocationCache(appId, handle, info, true, option);
		locationLogInfo(ctx->m_logger,
				"[GetLocationCacheWifiHistory] latitude[%f] longitude[%f] "
				"altitude[%f] accuracy[%f] speed[%f] direction[%f] "
				"timestamp[0x%016llx]:[%d/%02d/%02d %02d:%02d:%02d.%03d]\n",
				info);
	}
	return ret;
}

/* (0x81185E38) */
int locationSvcGetQueryLocation(int appId, SceLocationHandle handle, SceRtcTick timestamp,
				SceUInt32 timeout, bool option, SceLocationLocationInfo *info)
{
	int ret = SCE_LOCATION_ERROR_FATAL_ERROR;
	PositionManager *ctx = locationGetContext();

	if (ctx != NULL) {
		ret = ctx->GetQueryLocation(appId, handle, 0, timestamp, timeout, info, option);
		locationLogInfo(ctx->m_logger,
				"[GetQueryLocation] latitude[%f] longitude[%f] altitude[%f] "
				"accuracy[%f] speed[%f] direction[%f] timestamp[0x%016llx]:"
				"[%d/%02d/%02d %02d:%02d:%02d.%03d]\n",
				info);
	}
	return ret;
}

/* (0x81185F54) */
int locationSvcGetQueryLocationCache(int appId, SceLocationHandle handle, bool option,
				     SceLocationLocationInfo *info)
{
	(void)option;
	int ret = SCE_LOCATION_ERROR_FATAL_ERROR;
	PositionManager *ctx = locationGetContext();

	if (ctx != NULL) {
		ret = ctx->GetQueryLocationCache(appId, handle, info);
		locationLogInfo(ctx->m_logger,
				"[GetQueryLocationCache] latitude[%f] longitude[%f] altitude[%f] "
				"accuracy[%f] speed[%f] direction[%f] timestamp[0x%016llx]:"
				"[%d/%02d/%02d %02d:%02d:%02d.%03d]\n",
				info);
	}
	return ret;
}

/* (0x8118603C) */
int locationSvcStartLocationCallback(int appId, SceLocationHandle handle, SceUInt32 distance)
{
	PositionManager *ctx = locationGetContext();

	if (ctx != NULL)
		return ctx->StartLocationCallback(appId, handle, distance);
	return SCE_LOCATION_ERROR_FATAL_ERROR;
}

/* (0x81186068) */
int locationSvcStopLocationCallback(int appId, SceLocationHandle handle)
{
	PositionManager *ctx = locationGetContext();

	if (ctx != NULL)
		return ctx->StopLocationCallback(appId, handle);
	return SCE_LOCATION_ERROR_FATAL_ERROR;
}

/* (0x8118608C) */
int locationSvcGetHeading(int appId, SceLocationHandle handle, SceLocationHeadingInfo *info)
{
	PositionManager *ctx = locationGetContext();

	if (ctx != NULL)
		return ctx->GetHeading(appId, handle, info);
	return SCE_LOCATION_ERROR_FATAL_ERROR;
}

/* (0x811860B8) */
int locationSvcStartHeadingCallback(int appId, SceLocationHandle handle)
{
	PositionManager *ctx = locationGetContext();

	if (ctx != NULL)
		return ctx->StartHeadingCallback(appId, handle);
	return SCE_LOCATION_ERROR_FATAL_ERROR;
}

/* (0x811860EC) */
int locationSvcStopHeadingCallback(int appId, SceLocationHandle handle)
{
	PositionManager *ctx = locationGetContext();

	if (ctx != NULL)
		return ctx->StopHeadingCallback(appId, handle);
	return SCE_LOCATION_ERROR_FATAL_ERROR;
}

/* (0x81186110) */
int locationSvcConfirm(int appId, SceLocationHandle handle, SceUInt32 arg, SceUInt32 dataArg,
		       const void *data)
{
	int ret = SCE_LOCATION_ERROR_FATAL_ERROR;
	LocationConfirmMgr *confirm = locationConfirmGetInstance();

	if (confirm != NULL) {
		ret = confirm->Show(appId, handle, arg, dataArg, data, false);

		PositionManager *ctx = locationGetContext();

		if (ctx != NULL && ctx->m_logger != NULL)
			ctx->m_logger->Log(8, "[ConfirmDialogShow] result:0x%08x\n", ret);
	}
	return ret;
}

/* (0x81186168) */
int locationSvcConfirmGetResult(int appId, SceLocationHandle handle,
				SceLocationDialogResult *result)
{
	int ret = SCE_LOCATION_ERROR_FATAL_ERROR;
	PositionManager *ctx = locationGetContext();

	if (ctx != NULL) {
		ret = ctx->GetHandleStatus(appId, handle);
		if (ret != SCE_LOCATION_ERROR_DISABLE_APPLICATION) {
			LocationConfirmMgr *confirm = locationConfirmGetInstance();

			if (confirm != NULL) {
				*result = confirm->GetResult(appId, handle);
				return 0;
			}
			return SCE_LOCATION_ERROR_FATAL_ERROR;
		}
	}
	return ret;
}

/* (0x811861C0) */
int locationSvcConfirmGetStatus(int appId, SceLocationHandle handle,
				SceLocationDialogStatus *status)
{
	int ret = SCE_LOCATION_ERROR_FATAL_ERROR;
	PositionManager *ctx = locationGetContext();

	if (ctx != NULL) {
		ret = ctx->GetHandleStatus(appId, handle);
		if (ret != SCE_LOCATION_ERROR_DISABLE_APPLICATION) {
			LocationConfirmMgr *confirm = locationConfirmGetInstance();

			if (confirm != NULL) {
				*status = confirm->GetStatus(appId, handle);
				return confirm->m_lastResult;
			}
			return SCE_LOCATION_ERROR_FATAL_ERROR;
		}
	}
	return ret;
}

/* (0x8118621A) */
int locationSvcConfirmAbort(int appId, SceLocationHandle handle)
{
	int ret = SCE_LOCATION_ERROR_FATAL_ERROR;
	PositionManager *ctx = locationGetContext();

	if (ctx != NULL) {
		ret = ctx->GetHandleStatus(appId, handle);
		if (ret != SCE_LOCATION_ERROR_DISABLE_APPLICATION) {
			LocationConfirmMgr *confirm = locationConfirmGetInstance();

			if (confirm != NULL)
				return confirm->Abort(appId, handle);
			return SCE_LOCATION_ERROR_FATAL_ERROR;
		}
	}
	return ret;
}

/* (0x81186266) */
int locationSvcGetPermission(int appId, SceLocationHandle handle,
			     SceLocationPermissionInfo *info)
{
	PositionManager *ctx = locationGetContext();

	if (ctx != NULL)
		return ctx->GetPermission(appId, handle, info);
	return SCE_LOCATION_ERROR_FATAL_ERROR;
}

/* (0x81186292) */
int locationSvcDenyApplication(int appId, SceLocationHandle handle)
{
	(void)appId;
	PositionManager *ctx = locationGetContext();

	if (ctx != NULL)
		return ctx->SetHandlePermission(handle, 2);
	return SCE_LOCATION_ERROR_FATAL_ERROR;
}

/* (0x811862B4) */
int locationSvcSetGpsEmulationFile(const char *path)
{
	if (system::Model::GetType() == 1)
		return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;

	PositionManager *ctx = locationGetContext();

	if (ctx != NULL)
		return ctx->SetGpsEmulationFile(path);
	return SCE_LOCATION_ERROR_FATAL_ERROR;
}

/* (0x811862E6) */
int locationSvcDebugCmd(int appId, int cmd, const int *inArg, void *outBuf)
{
	SceAppMgrAppStatus state;
	int ret = sceAppMgrGetStatusByAppId(appId, &state);

	if (ret != 0)
		return ret;

	bool isSpecial = sce_paf_strncmp(state.appName, "NPXS10918", 32) == 0 ||
			 sce_paf_strncmp(state.appName, "NPXS10859", 32) == 0;
	bool isRestricted = sce_paf_strncmp(state.appName, "NPXS10861", 32) == 0;
	PositionManager *ctx = locationGetContext();

	if (ctx == NULL)
		return SCE_LOCATION_ERROR_FATAL_ERROR;

	if ((!vshSblAimgrIsDEX() && !vshSblAimgrIsTool() &&
	     !vshSblQafMgrIsAllowLiblocationChangeModel()) ||
	    (state.launchOrigin != 2 && !isSpecial)) {
		if (!isRestricted)
			return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
		if (ctx->m_logger != NULL && ctx->m_logger->m_debugInfo == NULL)
			ctx->m_logger->m_debugInfo = new LocationDebugInfo();
		switch (cmd) {
		case 4: {
			LocationDebugCounts *counts = (LocationDebugCounts *)outBuf;

			ctx->DbgGetGpsSatelliteCount(&counts->gpsSatelliteCount);
			ret = ctx->DbgGetWifiApCount(&counts->wifiApCount);
			break;
		}
		case 5:
			ret = ctx->DbgSetXtraAssistEnabled(*inArg);
			break;
		case 13:
			ret = ctx->DbgSetXtraLocalFileOnly(*inArg);
			break;
		case 14:
			ret = ctx->DbgGetXtraState((int *)outBuf);
			break;
		case 16:
			ret = ctx->DbgGetDopInfo((float *)outBuf);
			break;
		case 19:
			ret = ctx->DbgGetGpsSnapshot(outBuf);
			break;
		case 28:
			ret = ctx->DbgSetGpsTimeoutMs(1000 * *inArg);
			break;
		case 1: case 2: case 3: case 6: case 7: case 8: case 9: case 10:
		case 11: case 12: case 15: case 17: case 18: case 20: case 21:
		case 22: case 23: case 24: case 26: case 27: case 29: case 31:
			ret = SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
			break;
		default:
			ret = SCE_LOCATION_ERROR_FATAL_ERROR;
			break;
		}
		return ret;
	}

	if (isSpecial && ctx->m_logger != NULL && ctx->m_logger->m_debugInfo == NULL)
		ctx->m_logger->m_debugInfo = new LocationDebugInfo();

	switch (cmd) {
	case 1:
		ret = ctx->DbgClearUr0Files();
		break;
	case 2:
		ret = ctx->DbgSetLogMask(*inArg);
		break;
	case 3:
		ret = ctx->DbgGetLogMask((int *)outBuf);
		break;
	case 4: {
		LocationDebugCounts *counts = (LocationDebugCounts *)outBuf;

		ctx->DbgGetGpsSatelliteCount(&counts->gpsSatelliteCount);
		ret = ctx->DbgGetWifiApCount(&counts->wifiApCount);
		break;
	}
	case 5:
		ret = ctx->DbgSetXtraAssistEnabled(*inArg);
		break;
	case 6:
		ret = ctx->DbgSetFakeWifiAs3g(*inArg);
		break;
	case 7:
		ret = ctx->DbgSetLogFileIndex(*inArg);
		break;
	case 8: {
		LocationDebugCallbackStatus *cbStatus = (LocationDebugCallbackStatus *)outBuf;

		ctx->DbgGetCallbackTag(cbStatus->callbackTag);
		ctx->DbgTakeSensorActiveMask(&cbStatus->sensorActiveMask);
		break;
	}
	case 9:
		ret = ctx->DbgGetLogIoError((int *)outBuf);
		break;
	case 10:
		ret = ctx->DbgSetIntervalOverrideSec(*inArg);
		break;
	case 11:
		ret = ctx->DbgGetIntervalOverrideSec((int *)outBuf);
		break;
	case 12:
		ret = ctx->DbgResetDebugPermission();
		break;
	case 13:
		ret = ctx->DbgSetXtraLocalFileOnly(*inArg);
		break;
	case 14:
		ret = ctx->DbgGetXtraState((int *)outBuf);
		break;
	case 15:
		ret = ctx->DbgUnloadProvider();
		break;
	case 16:
		ret = ctx->DbgGetDopInfo((float *)outBuf);
		break;
	case 17:
		ret = ctx->DbgSetDopBounds((const float *)inArg);
		break;
	case 18:
		ret = ctx->DbgCountUr0Files((int *)outBuf);
		break;
	case 19:
		ret = ctx->DbgGetGpsSnapshot(outBuf);
		break;
	case 20:
		ret = ctx->DbgSyncDatFiles(*inArg);
		break;
	case 21:
		ret = ctx->DbgSetForceXtraDownload(*inArg);
		break;
	case 22:
		ret = ctx->DbgSetXtraSourceMode(*inArg);
		break;
	case 23:
		ret = ctx->DbgExportHistory();
		break;
	case 24:
		ret = ctx->DbgImportHistory();
		break;
	case 25:
		ret = ctx->DbgClearHistory();
		break;
	case 26:
		ret = SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
		break;
	case 27: {
		LocationDebugCellStatus *cellStatus = (LocationDebugCellStatus *)outBuf;

		ret = ctx->DbgGetCellInfo(&cellStatus->radioIf, cellStatus->cellInfo);
		break;
	}
	case 28:
		ret = ctx->DbgSetGpsTimeoutMs(1000 * *inArg);
		break;
	case 29:
		ret = ctx->UpdateFlyingStart();
		break;
	case 31:
		ctx->DbgSetFlyingStartIntervalSec(*inArg);
		/* fallthrough */
	default:
		ret = SCE_LOCATION_ERROR_FATAL_ERROR;
		break;
	}
	return ret;
}

/* (0x811866A4) */
int locationSvcNotifyPermissionChanged(int appId, SceLocationHandle handle)
{
	PositionManager *ctx = locationGetContext();

	if (ctx != NULL)
		return ctx->ConfirmHandle(appId, handle);
	return SCE_LOCATION_ERROR_FATAL_ERROR;
}

/* (0x811866C8) */
int locationSvcGetGlobalPerm(int *out)
{
	PositionManager *ctx = locationGetContext();

	if (ctx != NULL)
		return ctx->GetGlobalPerm(out);
	return SCE_LOCATION_ERROR_FATAL_ERROR;
}

/* (0x811866E8) */
int locationSvcGetAppPermSetting(int *out, bool reload)
{
	PositionManager *ctx = locationGetContext();

	if (ctx == NULL)
		return SCE_LOCATION_ERROR_FATAL_ERROR;
	if (reload) {
		int ret = ctx->ReloadConfig();

		if (ret != 0)
			return ret;
	}
	return ctx->GetAppPermSetting(out);
}

/* (0x81186716) */
int locationSvcGetValue(SceUInt32 *value)
{
	PositionManager *ctx = locationGetContext();

	if (ctx != NULL)
		return ctx->GetValue(value);
	return 0;
}

/* (0x81186730) */
int liblocationWaitReady(void)
{
	int ret = 0;
	PositionManager *ctx = locationGetContext();

	if (ctx != NULL) {
		for (int i = 0; i < 50; i++) {
			ret = ctx->DbgUnloadProvider();
			if (ret != SCE_LOCATION_ERROR_DISABLE_DEBUG_API)
				return ret;
			sceKernelDelayThread(20000);
		}
		return SCE_LOCATION_ERROR_FATAL_ERROR;
	}
	return ret;
}

/* (0x8118676C) */
static int locationNotifyEventCB(SceLocationHandle handle, int result, int eventType)
{
	LocationServer *server = locationServerGetInstance();

	if (server != NULL) {
		server->NotifyEvent(handle, result, eventType);
		return 0;
	}
	return SCE_LOCATION_ERROR_FATAL_ERROR;
}

/* (0x81186798) */
static int locationNotifyHeadingCB(SceLocationHandle handle, int unused)
{
	HeadingServer *server = headingServerGetInstance();

	if (server != NULL) {
		server->NotifyHeading(handle, unused);
		return 0;
	}
	return SCE_LOCATION_ERROR_FATAL_ERROR;
}

/* (0x811867BC) */
static void locationConfirmCB(int appId, int handle, int dialogType, int result, int canceled)
{
	PositionManager *ctx = locationGetContext();

	if (ctx == NULL)
		return;
	if (dialogType == 2 || dialogType == 4) {
		if (result == 2)
			ctx->SetHandlePermission(handle, 3);
		else
			ctx->SetHandlePermission(handle, 2);
	}
	if (canceled == 0) {
		if (result != 0)
			ctx->ConfirmHandle(appId, handle);

		unsigned char pending = 0;

		ctx->IsHandlePending(handle, &pending);
		if (pending != 0) {
			ctx->SetHandlePending(handle, 0);
			if (result == 2) {
				ctx->ResumePending(handle);
			} else {
				LocationServer *server = locationServerGetInstance();

				if (server != NULL)
					server->NotifyEvent(handle,
							    SCE_LOCATION_INFO_DENIED_BY_USER, 8);
			}
		} else {
			LocationServer *server = locationServerGetInstance();

			if (server != NULL)
				server->NotifyDenied(handle);
		}
	}
}
