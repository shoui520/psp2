/*
 * location_service.cpp: the shell's location service startup (liblocationInit,
 * liblocationWaitReady), GPS flying-start app event hook, and service entry points called by
 * the IPMI dispatchers (0x81185130-0x811868F4). The file's and internal names here are ours
 * (PLACEHOLDER).
 */
#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include <liblocation.h>
#include <rtc.h>
#include <kernel/iofilemgr.h>
#include <kernel/threadmgr.h>
#include <appmgr/appmgr.h>
#include <vshbridge/vshbridge.h>
#include <plugin.h>
#include <std/string>
#include <std/string.h>
#include <std/new>
#include <common/shared_ptr.h>
#include <memory/heap_allocator.h>
#include <system/system.h>
#include <datetime/datetime.h>
#include <timer/timer.h>
#include "../../../../shell.h"
#include "../../../../plugins.h"
#include "location_service.h"

extern "C" SceUInt32 vshSblQafMgrIsAllowLiblocationChangeModel(void);

using namespace paf;

/* the private heap (0x815496B0) and location manager (0x815496B4) */
memory::HeapAllocator *g_locationHeap;
void *g_locationManager;

/* PLACEHOLDER externs for the rest of liblocation (0x81187F14-0x81199500) */
struct LocationLogger {
	char reserved0[72];
	void *fileLog;		/* +72 (0x48) */
};

struct LocationContext {
	char reserved0[32];
	LocationLogger *logger;	/* +32 (0x20) */
};

struct LocationConfirmMgr {
	char reserved0[17336];
	int lastResult;		/* +17336 (0x43B8) */
};

LocationContext *locationGetContext(void);				/* (0x8118BA84) */
void locationLog(LocationLogger *logger, int level, const char *fmt, ...); /* (0x811895CC) */
LocationConfirmMgr *locationConfirmGetInstance(void);			/* (0x81187F14) */
int locationConfirmInit(void (*cb)(int, int, int, int, int));		/* (0x81188096) */
int locationConfirmTerm(void);						/* (0x811880FE) */
int locationConfirmShow(LocationConfirmMgr *mgr, int appId, SceLocationHandle handle,
			SceUInt32 arg, SceUInt32 dataArg, const void *data); /* (0x81188902) */
SceLocationDialogResult locationConfirmGetResult(LocationConfirmMgr *mgr, int appId,
						 SceLocationHandle handle); /* (0x81188BB6) */
SceLocationDialogStatus locationConfirmGetStatus(LocationConfirmMgr *mgr, int appId,
						 SceLocationHandle handle); /* (0x81188C26) */
int locationConfirmAbort(LocationConfirmMgr *mgr, int appId,
			 SceLocationHandle handle);			/* (0x81188C98) */
void locationConfirmCloseApp(LocationConfirmMgr *mgr, int appId,
			     SceLocationHandle handle);			/* (0x81188DF6) */
void *locationLogFileInit(void *mem);					/* (0x81188FE8) */
int locationProviderInit(void);						/* (0x81189EFC) */
void *locationMgrCtor(void *mem, Timer *timer);				/* (0x8118A838) */
void *locationMgrDtor(void *mgr);					/* (0x8118A930) */
int locationMgrInit(void *mgr);						/* (0x8118A954) */
int locationPermissionInit(void);					/* (0x81199430) */
int locationPermissionTerm(void);					/* (0x81199438) */
int locationCoreInit(int (*locCb)(SceLocationHandle, int, int),
		     int (*headCb)(SceLocationHandle, int));		/* (0x8118BEE0) */
int locationCoreTerm(void);						/* (0x8118BF66) */

int locationCtxUpdateFlyingStart(LocationContext *ctx);			/* (0x8118B9FC) */
int locationCtxStopHeadingCallback(LocationContext *ctx, int appId,
				   SceLocationHandle handle);		/* (0x8118C250) */
int locationCtxStopLocationCallback(LocationContext *ctx, int appId,
				    SceLocationHandle handle);		/* (0x8118C2EE) */
int locationCtxCancelGetLocation(LocationContext *ctx, int appId,
				 SceLocationHandle handle);		/* (0x8118C3E4) */
int locationCtxClose(LocationContext *ctx, int appId,
		     SceLocationHandle handle);				/* (0x8118C508) */
int locationCtxOpen(LocationContext *ctx, int appId, SceLocationHandle *handle,
		    SceUInt32 *lmethod, SceUInt32 *hmethod, SceUInt32 *mode); /* (0x8118C7C0) */
int locationCtxReopen(LocationContext *ctx, int appId, SceLocationHandle handle,
		      SceUInt32 *lmethod, SceUInt32 *hmethod);		/* (0x8118CA5C) */
int locationCtxGetMethod(LocationContext *ctx, int appId, SceLocationHandle handle,
			 SceUInt32 *lmethod, SceUInt32 *hmethod);	/* (0x8118CDE8) */
int locationCtxGetQueryLocation(LocationContext *ctx, int appId, SceLocationHandle handle,
				SceRtcTick timestamp, SceUInt32 timeout, bool option,
				SceLocationLocationInfo *info);		/* (0x8118CF20) */
int locationCtxGetQueryLocationCache(LocationContext *ctx, int appId, SceLocationHandle handle,
				     SceLocationLocationInfo *info);	/* (0x8118D164) */
int locationCtxCancelQueryLocation(LocationContext *ctx, int appId,
				   SceLocationHandle handle);		/* (0x8118D220) */
int locationCtxGetLocation(LocationContext *ctx, int appId, SceLocationHandle handle,
			   SceUInt32 timeout, SceLocationLocationInfo *info,
			   int wifiHistory);				/* (0x8118D306) */
int locationCtxGetLocationCache(LocationContext *ctx, int appId, SceLocationHandle handle,
				SceLocationLocationInfo *info, int wifiHistory,
				int option);				/* (0x8118D60C) */
int locationCtxStartLocationCallback(LocationContext *ctx, int appId, SceLocationHandle handle,
				     SceUInt32 distance);		/* (0x8118D790) */
int locationCtxGetHeading(LocationContext *ctx, int appId, SceLocationHandle handle,
			  SceLocationHeadingInfo *info);		/* (0x8118D866) */
int locationCtxStartHeadingCallback(LocationContext *ctx, int appId,
				    SceLocationHandle handle);		/* (0x8118D936) */
int locationCtxGetPermission(LocationContext *ctx, int appId, SceLocationHandle handle,
			     SceLocationPermissionInfo *info);		/* (0x8118D9E4) */
int locationCtxSetPermissionMode(LocationContext *ctx, int mode);	/* (0x8118DE68) */
int locationCtxSetAppPermission(LocationContext *ctx, const char *titleId); /* (0x8118DE72) */
int locationCtxReloadPermission(LocationContext *ctx);			/* (0x8118DE7C) */
int locationCtxSetHandlePermission(LocationContext *ctx, SceLocationHandle handle,
				   int status);				/* (0x8118DE86) */
int locationCtxGetHandleStatus(LocationContext *ctx, int appId,
			       SceLocationHandle handle);		/* (0x8118DEE0) */
int locationCtxSetGpsEmulationFile(LocationContext *ctx, const char *path); /* (0x8118DF06) */
int locationCtxNotifyPermissionChanged(LocationContext *ctx, int appId,
				       SceLocationHandle handle);	/* (0x8118DF32) */
int locationCtxGetValue(LocationContext *ctx, SceUInt32 *value);	/* (0x8118DF3C) */
int locationCtxFlyingStartResume(LocationContext *ctx, const char *titleId); /* (0x8118DF46) */
int locationCtxFlyingStartLaunch(LocationContext *ctx, const char *titleId); /* (0x8118DFA6) */
int locationCtxIsHandlePending(LocationContext *ctx, SceLocationHandle handle,
			       bool *pending);				/* (0x8118DFF0) */
int locationCtxSetHandlePending(LocationContext *ctx, SceLocationHandle handle,
				bool pending);				/* (0x8118DFFA) */
int locationCtxResumePending(LocationContext *ctx, SceLocationHandle handle); /* (0x8118E006) */

/* debug command handlers (0x8118E12C-0x8118E4C8) */
int locationCtxDbg1(LocationContext *ctx);				/* (0x8118E12C) */
int locationCtxDbg4a(LocationContext *ctx, void *out);			/* (0x8118E144) */
int locationCtxDbg4b(LocationContext *ctx, void *out);			/* (0x8118E160) */
int locationCtxDbg2(LocationContext *ctx, int arg);			/* (0x8118E17C) */
int locationCtxDbg3(LocationContext *ctx, void *out);			/* (0x8118E1BA) */
int locationCtxDbg5(LocationContext *ctx, int arg);			/* (0x8118E1D6) */
int locationCtxDbg6(LocationContext *ctx, int arg);			/* (0x8118E1EE) */
int locationCtxDbg7(LocationContext *ctx, int arg);			/* (0x8118E206) */
int locationCtxDbg8a(LocationContext *ctx, void *out);			/* (0x8118E21E) */
int locationCtxDbg8b(LocationContext *ctx, void *out);			/* (0x8118E242) */
int locationCtxDbg9(LocationContext *ctx, void *out);			/* (0x8118E25E) */
int locationCtxDbg10(LocationContext *ctx, int arg);			/* (0x8118E27A) */
int locationCtxDbg11(LocationContext *ctx, void *out);			/* (0x8118E292) */
int locationCtxDbg12(LocationContext *ctx);				/* (0x8118E2AE) */
int locationCtxDbg14(LocationContext *ctx, void *out);			/* (0x8118E2C6) */
int locationCtxDbg13(LocationContext *ctx, int arg);			/* (0x8118E2E2) */
int locationCtxDbg15(LocationContext *ctx);				/* (0x8118E2FA) */
int locationCtxDbg17(LocationContext *ctx, const int *inArg);		/* (0x8118E39E) */
int locationCtxDbg16(LocationContext *ctx, void *out);			/* (0x8118E3B6) */
int locationCtxDbg18(LocationContext *ctx, void *out);			/* (0x8118E3CE) */
int locationCtxDbg19(LocationContext *ctx, void *out);			/* (0x8118E3E6) */
int locationCtxDbg20(LocationContext *ctx, int arg);			/* (0x8118E3FE) */
int locationCtxDbg21(LocationContext *ctx, int arg);			/* (0x8118E416) */
int locationCtxDbg22(LocationContext *ctx, int arg);			/* (0x8118E42E) */
int locationCtxDbg23(LocationContext *ctx);				/* (0x8118E446) */
int locationCtxDbg24(LocationContext *ctx);				/* (0x8118E45E) */
int locationCtxDbg25(LocationContext *ctx);				/* (0x8118E476) */
int locationCtxDbg27(LocationContext *ctx, void *out1, void *out2);	/* (0x8118E48E) */
int locationCtxDbg28(LocationContext *ctx, int arg);			/* (0x8118E4C0) */
void locationCtxDbg31(LocationContext *ctx, int arg);			/* (0x8118E4C8) */

static int locationNotifyEventCB(SceLocationHandle handle, int result, int eventType);
static int locationNotifyHeadingCB(SceLocationHandle handle, int unused);
static void locationConfirmCB(int appId, int handle, int dialogType, int result, int canceled);
static void locationAppEventCB(int event, const void *data, SceSize size, void *arg);

/* (0x81185130) */
__attribute__((noinline))
static int locationGpsFlyingStart(const LsdbAppInfoPtr *appInfo, int event)
{
	int ret = 0;
	LocationContext *ctx = locationGetContext();

	if (ctx == NULL)
		return ret;

	LocationLogger *logger = ctx->logger;

	if (logger != NULL)
		locationLog(logger, 8, "sceLocationGpsFlyingStart() called\n");

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
					locationCtxFlyingStartResume(ctx, titleId);
					ret = locationCtxUpdateFlyingStart(ctx);
				} else if (event == 0) {
					ret = locationCtxFlyingStartLaunch(ctx, titleId);
				} else if (event == -1) {
					ret = locationCtxUpdateFlyingStart(ctx);
				}
			}
		}
	}
	if (logger != NULL)
		locationLog(logger, 8, "sceLocationGpsFlyingStart() result:0x%08x\n", ret);
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
		void *mgrMem = operator new(600, g_locationHeap);
		void *mgr = (mgrMem != NULL) ? locationMgrCtor(mgrMem, timer) : NULL;

		if (mgr == NULL) {
			ret = SCE_LOCATION_ERROR_NO_SERVER_MEMORY;
		} else {
			ret = locationMgrInit(mgr);
			if (ret != 0) {
				operator delete(locationMgrDtor(mgr));
			} else {
				g_locationManager = mgr;
			}
			if (ret == 0) {
				ret = locationServerStart();
				if (ret != 0) {
					locationServerStop();
					if (g_locationManager != NULL) {
						operator delete(locationMgrDtor(g_locationManager));
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
						operator delete(locationMgrDtor(g_locationManager));
						g_locationManager = NULL;
					}
					if (g_locationHeap != NULL) {
						delete g_locationHeap;
						g_locationHeap = NULL;
					}
				}
			}
			if (ret == 0) {
				ret = locationPermissionInit();
				if (ret != 0) {
					locationServerStop();
					headingServerStop();
					if (g_locationManager != NULL) {
						operator delete(locationMgrDtor(g_locationManager));
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
					LocationContext *ctx = locationGetContext();

					if (ctx != NULL)
						ret = locationCtxReloadPermission(ctx);
				}
				if (ret != 0) {
					locationServerStop();
					headingServerStop();
					locationPermissionTerm();
					if (g_locationManager != NULL) {
						operator delete(locationMgrDtor(g_locationManager));
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
					locationPermissionTerm();
					if (g_locationManager != NULL) {
						operator delete(locationMgrDtor(g_locationManager));
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
				locationPermissionTerm();
				if (g_locationManager != NULL) {
					operator delete(locationMgrDtor(g_locationManager));
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
	LocationContext *ctx = locationGetContext();

	if (ctx != NULL) {
		SceUInt32 lm = lmethod;
		SceUInt32 hm = hmethod;
		SceUInt32 md = mode;

		ret = locationCtxOpen(ctx, appId, handle, &lm, &hm, &md);
		*status = locationCtxGetHandleStatus(ctx, appId, *handle);
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
	locationConfirmCloseApp(confirm, appId, handle);

	LocationContext *ctx = locationGetContext();

	if (ctx != NULL)
		return locationCtxClose(ctx, appId, handle);
	return ret;
}

/* (0x81185858) */
int locationSvcReopen(int appId, SceLocationHandle handle, SceUInt32 lmethod, SceUInt32 hmethod)
{
	LocationContext *ctx = locationGetContext();

	if (ctx != NULL) {
		SceUInt32 lm = lmethod;
		SceUInt32 hm = hmethod;

		return locationCtxReopen(ctx, appId, handle, &lm, &hm);
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
	LocationContext *ctx = locationGetContext();

	if (ctx != NULL) {
		ret = locationCtxGetMethod(ctx, appId, handle, &lm, &hm);
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
		locationLog(logger, 8, fmt,
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
	LocationContext *ctx = locationGetContext();

	if (ctx != NULL) {
		ret = locationCtxGetLocation(ctx, appId, handle, 15, info, 0);
		locationLogInfo(ctx->logger,
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
	LocationContext *ctx = locationGetContext();

	if (ctx != NULL) {
		ret = locationCtxGetLocation(ctx, appId, handle, timeout, info, 0);
		locationLogInfo(ctx->logger,
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
	LocationContext *ctx = locationGetContext();

	if (ctx != NULL) {
		ret = locationCtxGetLocationCache(ctx, appId, handle, info, 0, 0);
		locationLogInfo(ctx->logger,
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
	LocationContext *ctx = locationGetContext();

	if (ctx != NULL) {
		if (isQuery)
			ret = locationCtxCancelQueryLocation(ctx, appId, handle);
		else
			ret = locationCtxCancelGetLocation(ctx, appId, handle);
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
	int ret = SCE_LOCATION_ERROR_FATAL_ERROR;
	LocationContext *ctx = locationGetContext();

	if (ctx != NULL) {
		ret = locationCtxGetLocation(ctx, appId, handle, timeout, info, 1);
		locationLogInfo(ctx->logger,
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
	LocationContext *ctx = locationGetContext();

	if (ctx != NULL) {
		ret = locationCtxGetLocationCache(ctx, appId, handle, info, 1, option);
		locationLogInfo(ctx->logger,
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
	LocationContext *ctx = locationGetContext();

	if (ctx != NULL) {
		ret = locationCtxGetQueryLocation(ctx, appId, handle, timestamp, timeout, option,
						  info);
		locationLogInfo(ctx->logger,
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
	int ret = SCE_LOCATION_ERROR_FATAL_ERROR;
	LocationContext *ctx = locationGetContext();

	if (ctx != NULL) {
		ret = locationCtxGetQueryLocationCache(ctx, appId, handle, info);
		locationLogInfo(ctx->logger,
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
	LocationContext *ctx = locationGetContext();

	if (ctx != NULL)
		return locationCtxStartLocationCallback(ctx, appId, handle, distance);
	return SCE_LOCATION_ERROR_FATAL_ERROR;
}

/* (0x81186068) */
int locationSvcStopLocationCallback(int appId, SceLocationHandle handle)
{
	LocationContext *ctx = locationGetContext();

	if (ctx != NULL)
		return locationCtxStopLocationCallback(ctx, appId, handle);
	return SCE_LOCATION_ERROR_FATAL_ERROR;
}

/* (0x8118608C) */
int locationSvcGetHeading(int appId, SceLocationHandle handle, SceLocationHeadingInfo *info)
{
	LocationContext *ctx = locationGetContext();

	if (ctx != NULL)
		return locationCtxGetHeading(ctx, appId, handle, info);
	return SCE_LOCATION_ERROR_FATAL_ERROR;
}

/* (0x811860B8) */
int locationSvcStartHeadingCallback(int appId, SceLocationHandle handle)
{
	LocationContext *ctx = locationGetContext();

	if (ctx != NULL)
		return locationCtxStartHeadingCallback(ctx, appId, handle);
	return SCE_LOCATION_ERROR_FATAL_ERROR;
}

/* (0x811860EC) */
int locationSvcStopHeadingCallback(int appId, SceLocationHandle handle)
{
	LocationContext *ctx = locationGetContext();

	if (ctx != NULL)
		return locationCtxStopHeadingCallback(ctx, appId, handle);
	return SCE_LOCATION_ERROR_FATAL_ERROR;
}

/* (0x81186110) */
int locationSvcConfirm(int appId, SceLocationHandle handle, SceUInt32 arg, SceUInt32 dataArg,
		       const void *data)
{
	int ret = SCE_LOCATION_ERROR_FATAL_ERROR;
	LocationConfirmMgr *confirm = locationConfirmGetInstance();

	if (confirm != NULL) {
		ret = locationConfirmShow(confirm, appId, handle, arg, dataArg, data);

		LocationContext *ctx = locationGetContext();

		if (ctx != NULL && ctx->logger != NULL)
			locationLog(ctx->logger, 8, "[ConfirmDialogShow] result:0x%08x\n", ret);
	}
	return ret;
}

/* (0x81186168) */
int locationSvcConfirmGetResult(int appId, SceLocationHandle handle,
				SceLocationDialogResult *result)
{
	int ret = SCE_LOCATION_ERROR_FATAL_ERROR;
	LocationContext *ctx = locationGetContext();

	if (ctx != NULL) {
		ret = locationCtxGetHandleStatus(ctx, appId, handle);
		if (ret != SCE_LOCATION_ERROR_DISABLE_APPLICATION) {
			if (locationConfirmGetInstance() != NULL) {
				*result = locationConfirmGetResult(locationConfirmGetInstance(),
								   appId, handle);
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
	LocationContext *ctx = locationGetContext();

	if (ctx != NULL) {
		ret = locationCtxGetHandleStatus(ctx, appId, handle);
		if (ret != SCE_LOCATION_ERROR_DISABLE_APPLICATION) {
			LocationConfirmMgr *confirm = locationConfirmGetInstance();

			if (confirm != NULL) {
				*status = locationConfirmGetStatus(confirm, appId, handle);
				return confirm->lastResult;
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
	LocationContext *ctx = locationGetContext();

	if (ctx != NULL) {
		ret = locationCtxGetHandleStatus(ctx, appId, handle);
		if (ret != SCE_LOCATION_ERROR_DISABLE_APPLICATION) {
			LocationConfirmMgr *confirm = locationConfirmGetInstance();

			if (confirm != NULL)
				return locationConfirmAbort(confirm, appId, handle);
			return SCE_LOCATION_ERROR_FATAL_ERROR;
		}
	}
	return ret;
}

/* (0x81186266) */
int locationSvcGetPermission(int appId, SceLocationHandle handle,
			     SceLocationPermissionInfo *info)
{
	LocationContext *ctx = locationGetContext();

	if (ctx != NULL)
		return locationCtxGetPermission(ctx, appId, handle, info);
	return SCE_LOCATION_ERROR_FATAL_ERROR;
}

/* (0x81186292) */
int locationSvcDenyApplication(int appId, SceLocationHandle handle)
{
	LocationContext *ctx = locationGetContext();

	if (ctx != NULL)
		return locationCtxSetHandlePermission(ctx, handle, 2);
	return SCE_LOCATION_ERROR_FATAL_ERROR;
}

/* (0x811862B4) */
int locationSvcSetGpsEmulationFile(const char *path)
{
	if (system::Model::GetType() == 1)
		return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;

	LocationContext *ctx = locationGetContext();

	if (ctx != NULL)
		return locationCtxSetGpsEmulationFile(ctx, path);
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
	LocationContext *ctx = locationGetContext();

	if (ctx == NULL)
		return SCE_LOCATION_ERROR_FATAL_ERROR;

	if ((!vshSblAimgrIsDEX() && !vshSblAimgrIsTool() &&
	     !vshSblQafMgrIsAllowLiblocationChangeModel()) ||
	    (state.launchOrigin != 2 && !isSpecial)) {
		if (!isRestricted)
			return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
		if (ctx->logger != NULL && ctx->logger->fileLog == NULL) {
			void *mem = operator new(432);

			ctx->logger->fileLog = (mem != NULL) ? locationLogFileInit(mem) : NULL;
		}
		switch (cmd) {
		case 4:
			locationCtxDbg4a(ctx, outBuf);
			ret = locationCtxDbg4b(ctx, (char *)outBuf + 4);
			break;
		case 5:
			ret = locationCtxDbg5(ctx, *inArg);
			break;
		case 13:
			ret = locationCtxDbg13(ctx, *inArg);
			break;
		case 14:
			ret = locationCtxDbg14(ctx, outBuf);
			break;
		case 16:
			ret = locationCtxDbg16(ctx, outBuf);
			break;
		case 19:
			ret = locationCtxDbg19(ctx, outBuf);
			break;
		case 28:
			ret = locationCtxDbg28(ctx, 1000 * *inArg);
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

	if (isSpecial && ctx->logger != NULL && ctx->logger->fileLog == NULL) {
		void *mem = operator new(432);

		ctx->logger->fileLog = (mem != NULL) ? locationLogFileInit(mem) : NULL;
	}

	switch (cmd) {
	case 1:
		ret = locationCtxDbg1(ctx);
		break;
	case 2:
		ret = locationCtxDbg2(ctx, *inArg);
		break;
	case 3:
		ret = locationCtxDbg3(ctx, outBuf);
		break;
	case 4:
		locationCtxDbg4a(ctx, outBuf);
		ret = locationCtxDbg4b(ctx, (char *)outBuf + 4);
		break;
	case 5:
		ret = locationCtxDbg5(ctx, *inArg);
		break;
	case 6:
		ret = locationCtxDbg6(ctx, *inArg);
		break;
	case 7:
		ret = locationCtxDbg7(ctx, *inArg);
		break;
	case 8:
		locationCtxDbg8a(ctx, outBuf);
		locationCtxDbg8b(ctx, (char *)outBuf + 11);
		break;
	case 9:
		ret = locationCtxDbg9(ctx, outBuf);
		break;
	case 10:
		ret = locationCtxDbg10(ctx, *inArg);
		break;
	case 11:
		ret = locationCtxDbg11(ctx, outBuf);
		break;
	case 12:
		ret = locationCtxDbg12(ctx);
		break;
	case 13:
		ret = locationCtxDbg13(ctx, *inArg);
		break;
	case 14:
		ret = locationCtxDbg14(ctx, outBuf);
		break;
	case 15:
		ret = locationCtxDbg15(ctx);
		break;
	case 16:
		ret = locationCtxDbg16(ctx, outBuf);
		break;
	case 17:
		ret = locationCtxDbg17(ctx, inArg);
		break;
	case 18:
		ret = locationCtxDbg18(ctx, outBuf);
		break;
	case 19:
		ret = locationCtxDbg19(ctx, outBuf);
		break;
	case 20:
		ret = locationCtxDbg20(ctx, *inArg);
		break;
	case 21:
		ret = locationCtxDbg21(ctx, *inArg);
		break;
	case 22:
		ret = locationCtxDbg22(ctx, *inArg);
		break;
	case 23:
		ret = locationCtxDbg23(ctx);
		break;
	case 24:
		ret = locationCtxDbg24(ctx);
		break;
	case 25:
		ret = locationCtxDbg25(ctx);
		break;
	case 26:
		ret = SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
		break;
	case 27:
		ret = locationCtxDbg27(ctx, outBuf, (char *)outBuf + 4);
		break;
	case 28:
		ret = locationCtxDbg28(ctx, 1000 * *inArg);
		break;
	case 29:
		ret = locationCtxUpdateFlyingStart(ctx);
		break;
	case 31:
		locationCtxDbg31(ctx, *inArg);
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
	LocationContext *ctx = locationGetContext();

	if (ctx != NULL)
		return locationCtxNotifyPermissionChanged(ctx, appId, handle);
	return SCE_LOCATION_ERROR_FATAL_ERROR;
}

/* (0x811866C8) */
int locationSvcSetPermissionMode(int mode)
{
	LocationContext *ctx = locationGetContext();

	if (ctx != NULL)
		return locationCtxSetPermissionMode(ctx, mode);
	return SCE_LOCATION_ERROR_FATAL_ERROR;
}

/* (0x811866E8) */
int locationSvcSetAppPermission(const char *titleId, bool reload)
{
	LocationContext *ctx = locationGetContext();

	if (ctx == NULL)
		return SCE_LOCATION_ERROR_FATAL_ERROR;
	if (reload) {
		int ret = locationCtxReloadPermission(ctx);

		if (ret != 0)
			return ret;
	}
	return locationCtxSetAppPermission(ctx, titleId);
}

/* (0x81186716) */
int locationSvcGetValue(SceUInt32 *value)
{
	LocationContext *ctx = locationGetContext();

	if (ctx != NULL)
		return locationCtxGetValue(ctx, value);
	return 0;
}

/* (0x81186730) */
int liblocationWaitReady(void)
{
	int ret = 0;
	LocationContext *ctx = locationGetContext();

	if (ctx != NULL) {
		for (int i = 0; i < 50; i++) {
			ret = locationCtxDbg15(ctx);
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
	LocationContext *ctx = locationGetContext();

	if (ctx == NULL)
		return;
	if (dialogType == 2 || dialogType == 4) {
		if (result == 2)
			locationCtxSetHandlePermission(ctx, handle, 3);
		else
			locationCtxSetHandlePermission(ctx, handle, 2);
	}
	if (canceled == 0) {
		if (result != 0)
			locationCtxNotifyPermissionChanged(ctx, appId, handle);

		bool pending = false;

		locationCtxIsHandlePending(ctx, handle, &pending);
		if (pending) {
			locationCtxSetHandlePending(ctx, handle, false);
			if (result == 2) {
				locationCtxResumePending(ctx, handle);
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
