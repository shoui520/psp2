/*
 * position_manager.cpp: PositionManager, the core of SceShell's liblocation service
 * (provider module loading/unloading, Open/Reopen/Close/GetLocation/GetHeading/
 * GetPermissionStatus, GPS flying start, Hubeny distance calculation, and debug command
 * dispatch; 0x8118B9A0-0x8118E748). Sony's class name ("PositionManager::...") comes from
 * its log strings; internal helper names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <math.h>
#include <scetypes.h>
#include <liblocation.h>
#include <rtc.h>
#include <power.h>
#include <kernel/iofilemgr.h>
#include <kernel/threadmgr.h>
#include <kernel/modulemgr.h>
#include <appmgr/appmgr.h>
#include <regmgr/regmgr.h>
#include <vshbridge/vshbridge.h>
#include <std/string.h>
#include <std/vector>
#include <std/new>
#include <system/system.h>
#include "location_service.h"

using namespace paf;

extern "C" {
SceUInt64 sceRtcGetAccumulativeTime(void);
float sce_paf_sinf(float x);
float sce_paf_cosf(float x);
int SceLibLocationProvider_0CBB4331(int arg);
int SceLibLocationProvider_2A97FD02(void);
void *SceLibLocationProvider_836F5953(int (*cb)(int, int, int, int));
int SceLibLocationProvider_8A796686(void *provider);
void *SceLibLocationDolceProvide_67156291(int (*cb)(int, int, int, int));
int SceLibLocationDolceProvide_AF76021F(void *provider);
}

/* globals shared across liblocation */
extern void *g_locationManager;					/* (0x815496B4) */
void *g_locationLogger;						/* (0x815496B8) */
extern void *g_locationXtra;					/* (0x815496BC) */
void *g_locationConfig;						/* (0x815496C0) */

/* PLACEHOLDER externs for the rest of liblocation */
struct LocationConfirmMgr;
LocationConfirmMgr *locationConfirmGetInstance(void);		/* (0x81187F14) */
int locationConfirmShow(LocationConfirmMgr *mgr, int appId, SceLocationHandle handle,
			SceUInt32 arg, SceUInt32 dataArg, const void *data,
			bool allowBusy);			/* (0x81188902) */
SceLocationDialogStatus locationConfirmGetStatus(LocationConfirmMgr *mgr, int appId,
						 SceLocationHandle handle); /* (0x81188C26) */
int locationConfirmAbort(LocationConfirmMgr *mgr, int appId,
			 SceLocationHandle handle);		/* (0x81188C98) */

void *locationLoggerCtor(void *mem);				/* (0x81189064) */
int locationLoggerStopLog(void *logger);			/* (0x811890B6) */
int locationLoggerStartLog(void *logger, SceUInt32 mask);	/* (0x811890FC) */
void locationLoggerResetFileLog(void *logger);			/* (0x81189258) */
int locationLoggerInit(void *logger);				/* (0x811892B2) */
int locationLoggerTerm(void *logger);				/* (0x8118945E) */
int locationLoggerLog(void *logger, int cat, const char *fmt, ...); /* (0x811895CC) */
int locationLoggerGetField0(void *logger);			/* (0x8118961E) */
int locationLoggerGetField4(void *logger);			/* (0x81189628) */
int locationLoggerClearUr0Files(void *logger);			/* (0x81189632) */
int locationLoggerSetField8(void *logger, int val);		/* (0x811896EC) */
int locationLoggerGetField8(void *logger);			/* (0x811896F6) */
int locationLoggerSetFieldC(void *logger, int val);		/* (0x81189700) */
int locationLoggerSetFileIndex(void *logger, int idx);		/* (0x8118970C) */
const char *locationLoggerGetCallbackTag(void *logger);		/* (0x81189712) */
int locationLoggerTakeFlag1F(void *logger);			/* (0x81189726) */
void locationLoggerSetField10(void *logger, int val);		/* (0x81189736) */
int locationLoggerGetField10(void *logger);			/* (0x8118973E) */
int locationLoggerResetDebugPerm(void *logger);			/* (0x81189748) */
int locationLoggerGetField20(void *logger);			/* (0x81189762) */
int locationLoggerSetField24(void *logger, int val);		/* (0x81189776) */
int locationLoggerGetField24(void *logger);			/* (0x81189780) */
int locationLoggerSetBuf28(void *logger, const void *src);	/* (0x8118978A) */
int locationLoggerGetBuf48(void *logger, void *dst);		/* (0x811897BC) */
int locationLoggerCountUr0Files(void *logger, int *outCount);	/* (0x811897EE) */
int locationLoggerGetBuf58(void *logger, void *dst);		/* (0x81189884) */
int locationLoggerSyncDatFiles(void *logger, int fromUx0);	/* (0x81189B68) */
int locationLoggerSetField190(void *logger, int val);		/* (0x81189B98) */
int locationLoggerGetField190(void *logger);			/* (0x81189BA4) */
int locationLoggerSetField194(void *logger, int val);		/* (0x81189BB0) */
int locationLoggerGetField194(void *logger);			/* (0x81189BBC) */
int locationLoggerExportHistory(void *logger);			/* (0x81189BC8) */
int locationLoggerImportHistory(void *logger);			/* (0x81189BE0) */
int locationLoggerClearHistory(void *logger);			/* (0x81189BF8) */
int locationLoggerGetField198AndArr(void *logger, int *outVal, int *outArr); /* (0x81189CB2) */
int locationLoggerGetMask(void *logger);			/* (0x8119982C) */

int locationTaskSchedule(void *task, unsigned int delayMs);	/* (0x8118A780) */
int locationMgrRegisterTask(void *mgr, void *task);		/* (0x8118AACE) */
int locationMgrUnregisterTask(void *mgr, void *task);		/* (0x8118AB62) */
int locationMgrReleaseActive(void *mgr);			/* (0x8118ABDC) */
int locationMgrAcquireActive(void *mgr);			/* (0x8118AC32) */
int locationDebugBufLock(void *buf);				/* (0x8118B984) */
int locationDebugBufUnlock(void *buf);				/* (0x8118B992) */

void *locationBridgeCtor(void *mem, void *pm, void *heading, void *handles); /* (0x8118E748) */
void locationBridgeSetProvider(void *bridge, void *provider);	/* (0x8118E78C) */
int locationBridgeNotify(void *bridge, int flag);		/* (0x8118E7A2) */
void locationHistoryStatInc1(void *stat);			/* (0x8118E8AC) */
void locationHistoryStatInc2(void *stat);			/* (0x8118E8E2) */
void locationHistoryStatInc3(void *stat);			/* (0x8118E918) */
void locationHistoryStatInc4(void *stat);			/* (0x8118E94E) */

void *locationPermMgrCtor(void *mem, void *config);		/* (0x8118E9F4) */
int locationPermMgrInit(void *permMgr);				/* (0x8118EA76) */
int locationPermMgrFindApp(void *permMgr, const char *titleId);	/* (0x8118ECF8) */
int locationPermMgrSetApp(void *permMgr, const char *titleId, int status, int unused); /* (0x8118EDC6) */
int locationPermMgrCheck1(void *permMgr);			/* (0x8118EF34) */
int locationPermMgrCheck2(void *permMgr);			/* (0x8118EF60) */
int locationPermMgrStart(void *permMgr, void *provider);	/* (0x8118EFAA) */
int locationPermMgrStop(void *permMgr, void *provider);		/* (0x8118EFB6) */
int locationPermMgrEnable(void *permMgr, void *provider);	/* (0x8118EFC2) */
int locationPermMgrDisable(void *permMgr, void *provider);	/* (0x8118EFE4) */
int locationPermMgrResetAll(void *permMgr);			/* (0x8118F26C) */

int locationPriorityValue(int val);				/* (0x8118F370) */
void locationInitLocationInfo(SceLocationLocationInfo *info);	/* (0x8118F4BC) */
void locationInitHeadingInfo(SceLocationHeadingInfo *info);	/* (0x8118F50E) */

void *locationConfigCtor(void *mem);				/* (0x8118F750) */
void locationConfigInit(void *cfg);				/* (0x8118FA24) */
int locationConfigGetParental(void *cfg);			/* (0x8118FD0C) */
int locationConfigReloadParental(void *cfg);			/* (0x8118FD12) */
int locationConfigGetMain(void *cfg);				/* (0x8118FD8A) */
int locationConfigReloadMain(void *cfg);			/* (0x8118FDAA) */
void locationConfigLoadDebug(void *cfg);			/* (0x811908AA) */
int locationConfigIsEmulation(void *cfg);			/* (0x811909F2) */
int locationConfigGetValue(void *cfg, SceUInt32 *out);		/* (0x81190BAC) */
int locationConfigReload(void *cfg);				/* (0x81190D8A) */
void locationConfigGetMethod(void *cfg, int *outL, int *outH);	/* (0x81190EEC) */
void locationConfigSetLMethod(void *cfg, int *lmethod);		/* (0x81190EFA) */
void locationConfigSetHMethod(void *cfg, int *hmethod);		/* (0x81190F2C) */

void locationPollTaskDtor(void *task);				/* (0x81190FEC) */
void locationPollTaskSetHandles(void *task, void *handles);	/* (0x8119107E) */
void locationPollTaskNotify(void *task);			/* (0x811910A8) */
int locationWifiHistoryInit(void *wh);				/* (0x811911A0) */
void locationWifiHistoryTerm(void *wh);				/* (0x811911CC) */
int locationWifiHistorySetActive(void *wh, int active);		/* (0x811911E2) */

void *locationHandleMgrCtor(void *mem, void *pm);		/* (0x811912F0) */
int locationHandleMgrInit(void *hm);				/* (0x811913A2) */
int locationHandleMgrGetCount(void *hm);			/* (0x811913C8) */
int locationHandleMgrIsValid(void *hm, int appId, SceLocationHandle handle); /* (0x811913CE) */
int locationHandleMgrClose(void *hm, int appId, SceLocationHandle handle); /* (0x811913F2) */
int locationHandleMgrOpen(void *hm, int appId, SceLocationHandle *handle,
			  SceUInt32 *lmethod, SceUInt32 *hmethod, SceUInt32 *mode); /* (0x81191482) */
int locationHandleMgrReopen(void *hm, int appId, SceLocationHandle handle,
			    SceUInt32 *lmethod, SceUInt32 *hmethod); /* (0x8119159A) */
int locationHandleMgrGetMethod(void *hm, int appId, SceLocationHandle handle,
			       SceUInt32 *lmethod, SceUInt32 *hmethod); /* (0x81191638) */
void locationHandleMgrUpdateHeading(void *hm, SceLocationHandle handle); /* (0x811916B2) */
int locationHandleMgrGetHeadingParam(void *hm, SceLocationHandle handle, float *out); /* (0x811916D8) */
int locationHandleMgrGetActiveMethods(void *hm, int *outL, int *outH); /* (0x81191748) */
int locationHandleMgrStartLocCb(void *hm, int appId, SceLocationHandle handle, SceUInt32 dist); /* (0x8119195E) */
int locationHandleMgrHasWifiHistory(void *hm, unsigned char *out); /* (0x811919FA) */
int locationHandleMgrStartRequest(void *hm, SceLocationHandle handle, SceUInt32 timeout,
				  int reqType, SceRtcTick *ts);	/* (0x81191A70) */
int locationHandleMgrGetQueryResult(void *hm, SceLocationHandle handle,
				    SceLocationLocationInfo *info); /* (0x81191B42) */
int locationHandleMgrStartHeadCb(void *hm, int appId, SceLocationHandle handle); /* (0x81191BCE) */
int locationHandleMgrStopLocCb(void *hm, int appId, SceLocationHandle handle, SceUInt32 lmethod); /* (0x81191C60) */
int locationHandleMgrCancelRequest(void *hm, SceLocationHandle handle, int reqType); /* (0x81191CF8) */
int locationHandleMgrStopHeadCb(void *hm, int appId, SceLocationHandle handle); /* (0x81191DFA) */
int locationHandleMgrSavePending(void *hm, SceLocationHandle handle, SceUInt32 timeout,
				 int kind, SceRtcTick *ts, int status, int flags); /* (0x81191F7E) */
int locationHandleMgrSetPending(void *hm, SceLocationHandle handle, unsigned char pending); /* (0x81191FDC) */
int locationHandleMgrIsPending(void *hm, SceLocationHandle handle, unsigned char *out); /* (0x8119201E) */
int locationHandleMgrCanConfirm(void *hm, SceLocationHandle handle, unsigned char *out); /* (0x81192062) */
int locationHandleMgrGetMode(void *hm, int appId, SceLocationHandle handle, SceUInt32 *out); /* (0x8119208A) */
int locationHandleMgrOnProviderEvent(void *hm, int providerId, int eventType, int param, int status); /* (0x811920CC) */
int locationHandleMgrCheckTimeout(void *hm);			/* (0x81192678) */
int locationHandleMgrGetFlags(void *hm, int appId, SceLocationHandle handle); /* (0x811927DA) */
int locationHandleMgrGetReqFlags(void *hm, int appId, SceLocationHandle handle); /* (0x81192812) */
int locationHandleMgrCheckPermission(void *hm, SceLocationHandle handle); /* (0x8119284C) */
int locationHandleMgrSetPermission(void *hm, SceLocationHandle handle, int status); /* (0x81192874) */
int locationHandleMgrGetAppPerm(void *hm, SceLocationHandle handle, int *out); /* (0x81192AD0) */
int locationHandleMgrGetPending(void *hm, int *outAppId, SceLocationHandle handle,
				SceUInt32 *outTimeout, SceUInt32 *outKind, SceRtcTick *outTs); /* (0x81192B24) */
int locationHandleMgrGetQueryState(void *hm, SceLocationHandle handle, int *out); /* (0x81192BBA) */
int locationHandleMgrSetQueryError(void *hm, SceLocationHandle handle, int err); /* (0x81192BFE) */
int locationHandleMgrUpdateAppPerm(void *hm, int appSlot, int status); /* (0x81192CFC) */
int locationHandleMgrRefreshAllPerms(void *hm);			/* (0x81192DD2) */
int locationHandleMgrResetAllPerms(void *hm, int val);		/* (0x81192E32) */
int locationHandleMgrNotifyPermChanged(void *hm);		/* (0x81192F04) */
int locationHandleMgrPollActive(void *hm, int arg);		/* (0x81192F72) */

int locationEmuSetFile(void *emu, const char *path);		/* (0x81193078) */
void *locationEmuCtor(void *mem, void *pm);			/* (0x811930A4) */
int locationEmuSetMode(void *emu, int mode);			/* (0x8119310A) */
const char *locationEmuGetFile(void *emu);			/* (0x813C837C) */

int locationGpsInit(void);					/* (0x81193500) */
void locationGpsTerm(void);					/* (0x81193570) */
void *locationGpsGetInstance(void);				/* (0x811935AE) */
int locationGpsTriggerFlyingStart(void *gpsTimer);		/* (0x81194C04) */

void *locationHeadingMgrCtor(void *mem, void *pm);		/* (0x81194E8C) */
int locationHeadingMgrStop(void *hm);				/* (0x81194F14) */
int locationHeadingMgrIsStarted(void *hm);			/* (0x81194F34) */
void locationHeadingMgrTerm(void *hm);				/* (0x81194F38) */
int locationHeadingMgrInit(void *hm);				/* (0x81195164) */
int locationHeadingMgrStart(void *hm);				/* (0x811951BE) */
int locationHeadingMgrEnable(void *hm);				/* (0x811951E0) */
int locationHeadingMgrDisable(void *hm);			/* (0x81195202) */
int locationHeadingMgrGetHeading(void *hm, int *hmethod, SceLocationHeadingInfo *out); /* (0x811964BE) */
int locationHeadingMgrStartSensor(void *hm);			/* (0x8119685E) */
int locationHeadingMgrStopSensor(void *hm);			/* (0x8119688A) */
int locationHeadingMgrIsEnabled(void *hm);			/* (0x811968B2) */
void locationHeadingMgrResetCache(void *hm);			/* (0x811968B6) */

struct LocationProviderVtbl {
	virtual int GetLocation(SceLocationLocationInfo *info) = 0;		/* +0 */
	virtual int GetLocationCache(SceLocationLocationInfo *info) = 0;	/* +4 */
	virtual void Reserved8() = 0;
	virtual void ReservedC() = 0;
	virtual void Reserved10() = 0;
	virtual void Reserved14() = 0;
	virtual void Reserved18() = 0;
	virtual void Reserved1C() = 0;
	virtual void Reserved20() = 0;
	virtual void SetHeadingMgr(void *headingMgr) = 0;			/* +36 */
	virtual int NeedsHeading() = 0;						/* +40 */
	virtual int OnWifiHistoryStart() = 0;					/* +44 */
	virtual int OnWifiHistoryStop() = 0;					/* +48 */
	virtual int GetWifiHistoryCache(const char *titleId, void *tsOut, int option) = 0; /* +52 */
	virtual int StartQuery(SceLocationHandle handle, const char *titleId,
			       SceRtcTick *ts, int option) = 0;			/* +56 */
	virtual int CancelQuery(SceLocationHandle handle) = 0;			/* +60 */
};

struct LocationConfigBase {
	virtual void Reserved0() = 0;
	virtual void Reserved4() = 0;
	virtual void Reserved8() = 0;
	virtual void ReservedC() = 0;
	virtual int HasGps() = 0;				/* +16 */
	virtual int IsEmulationMode() = 0;			/* +20 */
	virtual int IsDeviceEnabled() = 0;			/* +24 */
	virtual void Reserved1C() = 0;
	virtual void Reserved20() = 0;
	virtual void Reserved24() = 0;
	virtual void Destroy() = 0;				/* +40 */

	char reserved4[44];
	int globalPerm;						/* +48 */
	char reserved34[12];
	int appPerm;						/* +64 */
	char reserved44[40];
	int lmethodFlags;					/* +108 */
	SceUInt32 activeLMethod;				/* +112 */
	SceUInt32 activeHMethod;				/* +116 */
	int dbgParam28;						/* +120 */
	int pad7C;
	SceUInt64 flyingStartInterval;				/* +128 */
};

struct LocationObjBase {
	virtual ~LocationObjBase();
};

class PositionManager;

class PositionConfigRefreshTask {
public:
	virtual ~PositionConfigRefreshTask()
	{
	}

	virtual int OnNotify();					/* (0x8118B9A0) */

	int m_queued;						/* +4 */
	int m_active;						/* +8 */
	PositionManager *m_pm;					/* +12 */
};

class PositionPowerRegTask {
public:
	virtual ~PositionPowerRegTask()
	{
	}

	virtual int OnNotify();					/* (0x8118B9B2) */

	int m_queued;						/* +4 */
	int m_active;						/* +8 */
	PositionManager *m_pm;					/* +12 */
	SceUID m_cbId;						/* +16 (+168 in PM) */
	unsigned char m_registered;				/* +20 (+172 in PM) */
};

class PositionManager {
public:
	typedef int (*NotifyEventFn)(SceLocationHandle handle, int result, int eventType);
	typedef int (*NotifyHeadingFn)(SceLocationHandle handle, int unused);

	PositionManager();					/* (0x8118BE24) */
	virtual int OnWifiHistoryChange(int prev, int next);	/* (0x8118E6A2) */
	virtual ~PositionManager();				/* (0x8118E4E2, 0x8118E65E) */

	int UpdateFlyingStart();				/* (0x8118B9FC) */
	int Init(NotifyEventFn locCb, NotifyHeadingFn headCb);	/* (0x8118BAC8) */
	void *GetPermMgr();					/* (0x8118BF8C) */
	void *GetBridge();					/* (0x8118BF90) */
	int LoadProvider();					/* (0x8118BF96) */
	int UpdateHeadingState();				/* (0x8118C108) */
	int UpdateEmuMode(SceUInt32 *lmethod);			/* (0x8118C1AE) */
	int StopHeadingCallback(int appId, SceLocationHandle handle); /* (0x8118C250) */
	int StopLocationCallback(int appId, SceLocationHandle handle); /* (0x8118C2EE) */
	int CancelGetLocation(int appId, SceLocationHandle handle); /* (0x8118C3E4) */
	int Close(int appId, SceLocationHandle handle);		/* (0x8118C508) */
	int ValidateLMethod(SceUInt32 *lmethod);		/* (0x8118C70C) */
	int Open(int appId, SceLocationHandle *handle, SceUInt32 *lmethod,
		 SceUInt32 *hmethod, SceUInt32 *mode);		/* (0x8118C7C0) */
	int Reopen(int appId, SceLocationHandle handle, SceUInt32 *lmethod,
		   SceUInt32 *hmethod);				/* (0x8118CA5C) */
	int GetMethod(int appId, SceLocationHandle handle, SceUInt32 *lmethod,
		      SceUInt32 *hmethod);			/* (0x8118CDE8) */
	int ComparePriority(int a, int b);			/* (0x8118CE9E) */
	int IsAppActive(int appId, bool *outActive);		/* (0x8118CEDC) */
	int GetQueryLocation(int appId, SceLocationHandle handle, int unused,
			     SceRtcTick timestamp, SceUInt32 timeout,
			     SceLocationLocationInfo *info, bool option); /* (0x8118CF20) */
	int GetQueryLocationCache(int appId, SceLocationHandle handle,
				  SceLocationLocationInfo *info); /* (0x8118D164) */
	int CancelQueryLocation(int appId, SceLocationHandle handle); /* (0x8118D220) */
	int GetLocation(int appId, SceLocationHandle handle, SceUInt32 timeout,
			SceLocationLocationInfo *info, bool wifiHistory); /* (0x8118D306) */
	int GetLocationCache(int appId, SceLocationHandle handle,
			     SceLocationLocationInfo *info, bool wifiHistory,
			     bool option);			/* (0x8118D60C) */
	int StartLocationCallback(int appId, SceLocationHandle handle, SceUInt32 distance); /* (0x8118D790) */
	int GetHeading(int appId, SceLocationHandle handle, SceLocationHeadingInfo *info); /* (0x8118D866) */
	int StartHeadingCallback(int appId, SceLocationHandle handle); /* (0x8118D936) */
	int GetPermission(int appId, SceLocationHandle handle, SceLocationPermissionInfo *info); /* (0x8118D9E4) */
	int CheckTimeout();					/* (0x8118DAE4) */
	int NotifyEvent(SceLocationHandle handle, int result, int eventType); /* (0x8118DAEE) */
	int NotifyHeading(SceLocationHandle handle, int unused); /* (0x8118DB20) */
	double CalcDistance(const SceLocationLocationInfo *a,
			    const SceLocationLocationInfo *b);	/* (0x8118DB2E) */
	int SetAppPermByTitleId(const char *titleId, int status); /* (0x8118DC8A) */
	int ResetAllPerms(int val);				/* (0x8118DD5C) */
	int GetMainPerm(int *out);				/* (0x8118DDA6) */
	int ReloadMainPerm();					/* (0x8118DDB6) */
	int GetParentalPerm(int *out);				/* (0x8118DE0C) */
	int ReloadParentalPerm();				/* (0x8118DE1C) */
	int GetGlobalPerm(int *out);				/* (0x8118DE68) */
	int GetAppPermSetting(int *out);			/* (0x8118DE72) */
	int ReloadConfig();					/* (0x8118DE7C) */
	int SetHandlePermission(SceLocationHandle handle, int status); /* (0x8118DE86) */
	int GetHandleStatus(int appId, SceLocationHandle handle); /* (0x8118DEE0) */
	int SetGpsEmulationFile(const char *path);		/* (0x8118DF06) */
	int NotifyPermissionChanged();				/* (0x8118DF32) */
	int GetValue(SceUInt32 *out);				/* (0x8118DF3C) */
	int FlyingStartResume(const char *titleId);		/* (0x8118DF46) */
	int FlyingStartLaunch(const char *titleId);		/* (0x8118DFA6) */
	int IsHandlePending(SceLocationHandle handle, unsigned char *out); /* (0x8118DFF0) */
	int SetHandlePending(SceLocationHandle handle, unsigned char pending); /* (0x8118DFFA) */
	int ResumePending(SceLocationHandle handle);		/* (0x8118E006) */
	int PollActive();					/* (0x8118E11A) */

	/* debug command handlers (0x8118E12C-0x8118E4C8) */
	int Dbg1();
	int Dbg4a(int *out);
	int Dbg4b(int *out);
	int Dbg2(int mask);
	int Dbg3(int *out);
	int Dbg5(int val);
	int Dbg6(int val);
	int Dbg7(int idx);
	int Dbg8a(char *out);
	int Dbg8b(unsigned char *out);
	int Dbg9(int *out);
	int Dbg10(int val);
	int Dbg11(int *out);
	int Dbg12();
	int Dbg14(int *out);
	int Dbg13(int val);
	int Dbg15();
	int Dbg17(const void *inArg);
	int Dbg16(void *out);
	int Dbg18(int *out);
	int Dbg19(void *out);
	int Dbg20(int fromUx0);
	int Dbg21(int val);
	int Dbg22(int val);
	int Dbg23();
	int Dbg24();
	int Dbg25();
	int Dbg27(int *out1, int *out2);
	int Dbg28(int val);
	int Dbg31(int sec);

	static int PowerCallback(int notifyId, int notifyCount, int powerInfo, void *common); /* (0x8118BA90) */
	static int ProviderEventCB(int providerId, int eventType, int param, int status); /* (0x8118E66E) */

	void *m_handleMgr;					/* +4 */
	LocationProviderVtbl *m_provider;			/* +8 */
	void *m_headingMgr;					/* +12 */
	void *m_emuMgr;						/* +16 */
	void *m_bridge;						/* +20 */
	LocationConfigBase *m_config;				/* +24 */
	void *m_permMgr;					/* +28 */
	void *m_logger;						/* +32 */
	NotifyEventFn m_notifyEventCb;				/* +36 */
	NotifyHeadingFn m_notifyHeadingCb;			/* +40 */
	unsigned char m_providerLoaded;				/* +44 */
	SceUID m_providerModId;					/* +48 */
	int m_reserved34;					/* +52 */
	/* embedded LocationPollTask (+56..+135, 80 bytes) */
	void *m_pollVtbl;					/* +56 */
	int m_pollQueued;					/* +60 */
	int m_pollActive;					/* +64 */
	int m_pollField44;					/* +68 */
	char m_pollPad[32];					/* +72..+103 */
	unsigned char m_pollFlag68;				/* +104 */
	int m_pollField6C;					/* +108 */
	void *m_pollProvider;					/* +112 */
	vector<void *> m_pollObservers;				/* +116..+131 */
	int m_pollPad84;					/* +132..+135 */
	PositionConfigRefreshTask m_configRefreshTask;		/* +136..+151 */
	PositionPowerRegTask m_powerRegTask;			/* +152..+175 */
	char m_flyingTitles[8][32];				/* +176..+431 (0x100) */
	int m_flyingCount;					/* +432 */
	int m_reserved1B4;					/* +436 */
	/* embedded LocationWifiHistory (+440..+495, 56 bytes) */
	int m_wifiHistoryState;					/* +440 */
	vector<PositionManager *> m_wifiHistoryListeners;	/* +444..+459 */
	int m_wifiField1CC;					/* +460 */
	char m_wifiPad[32];					/* +464..+495 */
};

/* the PositionManager singleton (0x81544CE4) */
static PositionManager *s_positionManager;

/* (0x8118B9A0) */
int PositionConfigRefreshTask::OnNotify()
{
	if (m_pm != NULL && m_pm->m_config != NULL)
		locationConfigReload(m_pm->m_config);
	return 1;
}

/* (0x8118B9B2) */
int PositionPowerRegTask::OnNotify()
{
	if (m_registered == 0) {
		m_cbId = sceKernelCreateCallback("SceLocationPowerCallback", 0,
						 PositionManager::PowerCallback, NULL);
		if (m_cbId >= 0) {
			if (scePowerRegisterCallback(m_cbId) == 0) {
				m_registered = 1;
				return 0;
			}
			sceKernelDeleteCallback(m_cbId);
			m_cbId = -1;
		}
	}
	return 1;
}

/* (0x8118B9FC) */
int PositionManager::UpdateFlyingStart()
{
	void *gps = locationGpsGetInstance();

	if (gps == NULL)
		return 0;
	if (!m_config->HasGps())
		return 0;
	if (m_config->IsEmulationMode())
		return 0;
	if (!m_config->IsDeviceEnabled())
		return 0;

	SceUInt64 now = sceRtcGetAccumulativeTime();
	SceUInt64 lastTime = *(SceUInt64 *)((char *)locationGpsGetInstance() + 272);

	if (now - lastTime <= m_config->flyingStartInterval)
		return 0;
	if (g_locationLogger != NULL)
		locationLoggerLog(g_locationLogger, 8, "GPS Flying Start\n");
	return locationGpsTriggerFlyingStart((char *)locationGpsGetInstance() + 384);
}

/* (0x8118BA84) */
PositionManager *locationGetContext(void)
{
	return s_positionManager;
}

/* (0x8118BA90) */
int PositionManager::PowerCallback(int notifyId, int notifyCount, int powerInfo, void *common)
{
	if ((powerInfo & 0x800000) != 0 && (powerInfo & 0x20000) != 0) {
		SceUInt32 factor = vshPowerGetWakeupFactor() & 0x7F;

		if ((factor == 0 || factor == 18) && s_positionManager != NULL &&
		    s_positionManager->m_flyingCount != 0)
			s_positionManager->UpdateFlyingStart();
	}
	return 0;
}

/* (0x8118BAC8) */
int PositionManager::Init(NotifyEventFn locCb, NotifyHeadingFn headCb)
{
	int ret;
	void *mem;

	mem = operator new(268, g_locationHeap);
	m_emuMgr = (mem != NULL) ? locationEmuCtor(mem, this) : NULL;
	if (m_emuMgr == NULL) {
		ret = SCE_LOCATION_ERROR_NO_MEMORY;
		goto fail;
	}

	mem = operator new(2360, g_locationHeap);
	m_handleMgr = (mem != NULL) ? locationHandleMgrCtor(mem, this) : NULL;
	if (m_handleMgr == NULL) {
		ret = SCE_LOCATION_ERROR_NO_MEMORY;
		goto fail;
	}
	ret = locationHandleMgrInit(m_handleMgr);
	if (ret != 0)
		goto fail;

	m_notifyEventCb = locCb;
	m_notifyHeadingCb = headCb;
	locationPollTaskSetHandles(&m_pollVtbl, m_handleMgr);
	locationMgrRegisterTask(g_locationManager, &m_pollVtbl);

	mem = operator new(1336, g_locationHeap);
	m_headingMgr = (mem != NULL) ? locationHeadingMgrCtor(mem, this) : NULL;
	if (m_headingMgr == NULL) {
		ret = SCE_LOCATION_ERROR_NO_MEMORY;
		goto fail;
	}
	ret = locationHeadingMgrInit(m_headingMgr);
	if (ret != 0)
		goto fail;

	mem = operator new(28, g_locationHeap);
	m_bridge = (mem != NULL) ? locationBridgeCtor(mem, this, m_headingMgr, m_handleMgr) : NULL;
	if (m_bridge == NULL) {
		ret = SCE_LOCATION_ERROR_NO_MEMORY;
		goto fail;
	}

	mem = operator new(136, g_locationHeap);
	m_config = (mem != NULL) ? (LocationConfigBase *)locationConfigCtor(mem) : NULL;
	if (m_config == NULL) {
		ret = SCE_LOCATION_ERROR_NO_MEMORY;
		goto fail;
	}
	locationConfigInit(m_config);
	g_locationConfig = m_config;

	mem = operator new(440, g_locationHeap);
	m_permMgr = (mem != NULL) ? locationPermMgrCtor(mem, m_config) : NULL;
	if (m_permMgr == NULL) {
		ret = SCE_LOCATION_ERROR_NO_MEMORY;
		goto fail;
	}
	ret = locationPermMgrInit(m_permMgr);
	if (ret != 0)
		goto fail;

	ret = locationWifiHistoryInit(&m_wifiHistoryState);
	if (ret != 0)
		goto fail;
	m_wifiHistoryListeners.push_back(this);

	ret = locationGpsInit();
	if (ret != 0)
		goto fail;

	m_pollObservers.push_back((char *)m_config + 84);
	m_pollObservers.push_back((char *)m_headingMgr + 4);

	locationMgrRegisterTask(g_locationManager, &m_configRefreshTask);
	locationTaskSchedule(&m_configRefreshTask, 86400000);
	ret = locationMgrRegisterTask(g_locationManager, &m_powerRegTask);
	locationTaskSchedule(&m_powerRegTask, 1000);

	mem = operator new(144, g_locationHeap);
	m_logger = (mem != NULL) ? locationLoggerCtor(mem) : NULL;
	if (m_logger != NULL) {
		g_locationLogger = m_logger;
		locationLoggerInit(m_logger);
	}
	if (ret != 0) {
		locationGpsTerm();
		goto fail;
	}
	return ret;

fail:
	locationWifiHistoryTerm(&m_wifiHistoryState);
	if (m_permMgr != NULL) {
		delete (LocationObjBase *)m_permMgr;
		m_permMgr = NULL;
	}
	if (m_config != NULL) {
		m_config->Destroy();
		m_config = NULL;
	}
	if (m_bridge != NULL) {
		delete (LocationObjBase *)m_bridge;
		m_bridge = NULL;
	}
	if (m_headingMgr != NULL) {
		locationHeadingMgrTerm(m_headingMgr);
		delete (LocationObjBase *)m_headingMgr;
		m_headingMgr = NULL;
	}
	if (m_handleMgr != NULL) {
		delete (LocationObjBase *)m_handleMgr;
		m_handleMgr = NULL;
	}
	if (m_emuMgr != NULL) {
		delete (LocationObjBase *)m_emuMgr;
		m_emuMgr = NULL;
	}
	return ret;
}

/* (0x8118BE24) */
__attribute__((noinline))
PositionManager::PositionManager()
{
	m_providerLoaded = 0;
	m_powerRegTask.m_registered = 0;
	m_pollFlag68 = 0;
	m_pollQueued = 0;
	m_pollActive = 0;
	m_pollField44 = -1;
	m_pollField6C = 0;
	m_pollProvider = NULL;
	m_configRefreshTask.m_queued = 0;
	m_configRefreshTask.m_active = 0;
	m_configRefreshTask.m_pm = this;
	m_powerRegTask.m_queued = 0;
	m_powerRegTask.m_active = 0;
	m_powerRegTask.m_pm = this;
	m_wifiHistoryState = 0;
	m_wifiField1CC = -1;
	m_provider = NULL;
	m_handleMgr = NULL;
	m_headingMgr = NULL;
	m_emuMgr = NULL;
	m_bridge = NULL;
	m_config = NULL;
	m_permMgr = NULL;
	m_logger = NULL;
	m_notifyEventCb = NULL;
	m_notifyHeadingCb = NULL;
	m_providerModId = -1;
	sce_paf_memset(m_flyingTitles, 0, sizeof(m_flyingTitles));
	m_flyingCount = 0;
}

/* (0x8118BEE0) */
int locationCoreInit(PositionManager::NotifyEventFn locCb,
		     PositionManager::NotifyHeadingFn headCb)
{
	if (s_positionManager != NULL)
		return SCE_LOCATION_ERROR_FATAL_ERROR;

	PositionManager *pm = new (g_locationHeap) PositionManager();

	s_positionManager = pm;
	if (pm == NULL)
		return SCE_LOCATION_ERROR_NO_MEMORY;

	int ret = pm->Init(locCb, headCb);

	if (ret != 0) {
		delete s_positionManager;
		s_positionManager = NULL;
	}
	return ret;
}

/* (0x8118BF66) */
int locationCoreTerm(void)
{
	if (s_positionManager != NULL) {
		delete s_positionManager;
		s_positionManager = NULL;
	}
	return 0;
}

/* (0x8118BF8C) */
void *PositionManager::GetPermMgr()
{
	return m_permMgr;
}

/* (0x8118BF90) */
void *PositionManager::GetBridge()
{
	return m_bridge;
}

/* (0x8118BF96) */
int PositionManager::LoadProvider()
{
	if (m_providerLoaded != 0)
		return SCE_LOCATION_ERROR_FATAL_ERROR;

	int status = 0;
	SceUID modId;

	if (system::Model::GetType() == 0) {
		void *args[6];

		args[0] = g_locationHeap;
		args[1] = g_locationManager;
		args[2] = m_logger;
		args[3] = m_config;
		args[4] = g_locationXtra;
		args[5] = locationGpsGetInstance();
		modId = sceKernelLoadStartModule("vs0:vsh/shell/liblocation_provider.suprx",
						 sizeof(args), args, 0, NULL, &status);
	} else {
		void *args[3];

		args[0] = g_locationHeap;
		args[1] = g_locationManager;
		args[2] = m_logger;
		modId = sceKernelLoadStartModule("vs0:vsh/shell/liblocation_dolce_provider.suprx",
						 sizeof(args), args, 0, NULL, &status);
	}
	m_providerModId = modId;
	if (modId < 0)
		return modId;

	int ret = 0;

	if (system::Model::GetType() == 0)
		ret = SceLibLocationProvider_0CBB4331(0);
	if (ret != 0) {
		sceKernelStopUnloadModule(m_providerModId, 0, NULL, 0, NULL, &status);
		return ret;
	}

	LocationProviderVtbl *prov;

	if (system::Model::GetType() != 0)
		prov = (LocationProviderVtbl *)SceLibLocationDolceProvide_67156291(ProviderEventCB);
	else
		prov = (LocationProviderVtbl *)SceLibLocationProvider_836F5953(ProviderEventCB);
	m_provider = prov;
	if (prov == NULL) {
		if (system::Model::GetType() == 0)
			SceLibLocationProvider_2A97FD02();
		sceKernelStopUnloadModule(m_providerModId, 0, NULL, 0, NULL, &status);
		return SCE_LOCATION_ERROR_NO_MEMORY;
	}
	m_providerLoaded = 1;
	locationBridgeSetProvider(m_bridge, prov);
	locationDebugBufLock(&m_pollVtbl);
	m_pollProvider = m_provider;
	locationDebugBufUnlock(&m_pollVtbl);
	if (system::Model::GetType() == 0)
		m_provider->SetHeadingMgr(m_headingMgr);
	return 0;
}

/* (0x8118C108) */
int PositionManager::UpdateHeadingState()
{
	int ret = 0;
	SceUInt32 hmethod = m_config->activeHMethod & SCE_LOCATION_HMETHOD_MASK;

	if (locationHeadingMgrIsStarted(m_headingMgr)) {
		if (locationHeadingMgrIsEnabled(m_headingMgr) && hmethod == 0)
			ret = locationHeadingMgrDisable(m_headingMgr);
		else if (!locationHeadingMgrIsEnabled(m_headingMgr) && hmethod != 0)
			ret = locationHeadingMgrEnable(m_headingMgr);
		if (hmethod == 0 && (m_provider == NULL || !m_provider->NeedsHeading())) {
			locationHeadingMgrStopSensor(m_headingMgr);
			locationBridgeNotify(m_bridge, 1);
			return locationHeadingMgrStop(m_headingMgr);
		}
	} else if (hmethod != 0 || (m_provider != NULL && m_provider->NeedsHeading())) {
		ret = locationHeadingMgrStart(m_headingMgr);
		if (ret == 0) {
			if (hmethod != 0)
				ret = locationHeadingMgrEnable(m_headingMgr);
			if (ret == 0) {
				locationBridgeNotify(m_bridge, 1);
				return locationHeadingMgrStartSensor(m_headingMgr);
			}
		}
	}
	return ret;
}

/* (0x8118C1AE) */
int PositionManager::UpdateEmuMode(SceUInt32 *lmethod)
{
	int mode;

	if (m_config->IsEmulationMode()) {
		mode = 8;
	} else if (m_config->HasGps()) {
		switch (*lmethod & SCE_LOCATION_LMETHOD_MASK) {
		case SCE_LOCATION_LMETHOD_NONE:
			mode = 0;
			break;
		case SCE_LOCATION_LMETHOD_AGPS_AND_3G_AND_WIFI:
			mode = 23;
			break;
		case SCE_LOCATION_LMETHOD_3G_AND_WIFI:
			mode = 6;
			break;
		case SCE_LOCATION_LMETHOD_WIFI:
			mode = 2;
			break;
		case SCE_LOCATION_LMETHOD_3G:
			mode = 4;
			break;
		case SCE_LOCATION_LMETHOD_GPS:
			mode = 1;
			if (g_locationLogger != NULL &&
			    locationLoggerGetField24(g_locationLogger) != 1 &&
			    locationLoggerGetField8(g_locationLogger) != 0 &&
			    (locationLoggerGetField194(g_locationLogger) != 0 ||
			     locationLoggerGetField190(g_locationLogger) == 1))
				mode = 17;
			break;
		case SCE_LOCATION_LMETHOD_GPS_EMULATION:
			mode = 8;
			break;
		default:
			mode = 0;
			break;
		}
	} else {
		mode = 2;
	}
	return locationEmuSetMode(m_emuMgr, mode);
}

/* (0x8118C250) */
int PositionManager::StopHeadingCallback(int appId, SceLocationHandle handle)
{
	if (g_locationLogger != NULL)
		locationLoggerLog(g_locationLogger, 8, "PositionManager::StopHeadingCallback() START\n");

	SceUInt32 hmethod = 0;
	int ret = locationHandleMgrGetMethod(m_handleMgr, appId, handle, NULL, &hmethod);

	if (ret == 0) {
		if ((hmethod & SCE_LOCATION_HMETHOD_MASK) != 0)
			ret = locationHandleMgrStopHeadCb(m_handleMgr, appId, handle);
		else
			ret = SCE_LOCATION_ERROR_INVALID_HEADING_METHOD;
	}
	if (g_locationLogger != NULL)
		locationLoggerLog(g_locationLogger, 8, "PositionManager::StopHeadingCallback() END(0x%x)\n", ret);
	return ret;
}

/* (0x8118C2EE) */
int PositionManager::StopLocationCallback(int appId, SceLocationHandle handle)
{
	if (g_locationLogger != NULL)
		locationLoggerLog(g_locationLogger, 8, "PositionManager::StopLocationCallback() START\n");

	SceUInt32 lmethod = 0;
	int ret = locationHandleMgrGetMethod(m_handleMgr, appId, handle, &lmethod, NULL);

	if (ret == 0) {
		ret = locationHandleMgrCheckPermission(m_handleMgr, handle);
		if (ret == 0 || ret == SCE_LOCATION_INFO_DENIED_BY_USER) {
			if ((lmethod & SCE_LOCATION_LMETHOD_MASK) != 0) {
				ret = locationHandleMgrStopLocCb(m_handleMgr, appId, handle, lmethod);
				if (ret == 0) {
					ret = locationPermMgrStop(m_permMgr, m_provider);
					if (ret == 0)
						locationBridgeNotify(m_bridge, 0);

					LocationConfirmMgr *confirm = locationConfirmGetInstance();

					if (confirm != NULL &&
					    locationConfirmGetStatus(confirm, appId, handle) ==
						    SCE_LOCATION_DIALOG_STATUS_RUNNING) {
						locationConfirmAbort(confirm, appId, handle);
						locationHandleMgrSetPending(m_handleMgr, handle, 0);
					}
				}
			} else {
				ret = SCE_LOCATION_ERROR_INVALID_LOCATION_METHOD;
			}
		}
	}
	if (g_locationLogger != NULL)
		locationLoggerLog(g_locationLogger, 8, "PositionManager::StopPositionCallback() END(0x%x)\n", ret);
	return ret;
}

/* (0x8118C3E4) */
int PositionManager::CancelGetLocation(int appId, SceLocationHandle handle)
{
	if (g_locationLogger != NULL)
		locationLoggerLog(g_locationLogger, 8, "PositionManager::CancelGetLocation() START\n");

	int ret;

	if (locationHandleMgrIsValid(m_handleMgr, appId, handle)) {
		ret = locationHandleMgrCheckPermission(m_handleMgr, handle);
		if (ret == 0 || ret == SCE_LOCATION_INFO_DENIED_BY_USER) {
			if ((locationHandleMgrGetReqFlags(m_handleMgr, appId, handle) & 2) != 0)
				locationHistoryStatInc2((char *)m_logger + 137);

			unsigned char pending = 0;

			ret = locationHandleMgrIsPending(m_handleMgr, handle, &pending);
			if (pending != 0) {
				LocationConfirmMgr *confirm = locationConfirmGetInstance();

				if (confirm != NULL)
					ret = locationConfirmAbort(confirm, appId, handle);
				locationHandleMgrSetPending(m_handleMgr, handle, 0);
			} else {
				ret = locationHandleMgrCancelRequest(m_handleMgr, handle, 3);
				if (ret == 0) {
					ret = locationPermMgrStop(m_permMgr, m_provider);
					if (ret == 0 && m_wifiHistoryState == 1) {
						unsigned char hasWh = 0;

						ret = locationHandleMgrHasWifiHistory(m_handleMgr, &hasWh);
						if ((ret | hasWh) == 0)
							ret = locationWifiHistorySetActive(&m_wifiHistoryState, 0);
					}
				}
			}
		}
	} else {
		ret = SCE_LOCATION_ERROR_INVALID_HANDLE;
	}
	if (g_locationLogger != NULL)
		locationLoggerLog(g_locationLogger, 8, "PositionManager::CancelGetLocation() END(0x%x)\n", ret);
	return ret;
}

/* (0x8118C508) */
int PositionManager::Close(int appId, SceLocationHandle handle)
{
	if (g_locationLogger != NULL)
		locationLoggerLog(g_locationLogger, 8, "PositionManager::Close() START\n");

	int flags = locationHandleMgrGetFlags(m_handleMgr, appId, handle);

	if ((flags & 4) != 0)
		CancelGetLocation(appId, handle);
	if ((flags & 8) != 0)
		StopLocationCallback(appId, handle);
	if ((flags & 0x10) != 0)
		StopHeadingCallback(appId, handle);

	locationHeadingMgrResetCache(m_headingMgr);

	int ret = locationHandleMgrClose(m_handleMgr, appId, handle);

	if (ret == 0) {
		int lmethod = 0;
		int hmethod = 0;

		ret = locationHandleMgrGetActiveMethods(m_handleMgr, &lmethod, &hmethod);
		if (ret == 0) {
			locationConfigSetLMethod(m_config, &lmethod);
			locationConfigSetHMethod(m_config, &hmethod);
			m_config->lmethodFlags = lmethod & 0x100;
		}

		int r = UpdateEmuMode(&m_config->activeLMethod);

		if (r != 0)
			ret = r;
		r = UpdateHeadingState();
		if (r != 0)
			ret = r;

		int permRet = 0;

		if (m_providerLoaded != 0) {
			if (!locationPermMgrCheck1(m_permMgr) || !locationPermMgrCheck2(m_permMgr))
				permRet = locationPermMgrEnable(m_permMgr, m_provider);
			else
				permRet = locationPermMgrDisable(m_permMgr, m_provider);
		}
		if (permRet != 0)
			ret = permRet;

		if (locationHandleMgrGetCount(m_handleMgr) > 0 && m_providerLoaded == 0 &&
		    ((m_config->activeLMethod & SCE_LOCATION_LMETHOD_MASK) != 0 ||
		     (m_config->activeHMethod & SCE_LOCATION_HMETHOD_MASK) != 0)) {
			r = LoadProvider();
			if (r != 0)
				ret = r;
		}
		if (g_locationManager != NULL)
			locationMgrReleaseActive(g_locationManager);
		else
			ret = SCE_LOCATION_ERROR_FATAL_ERROR;
	}
	if (g_locationLogger != NULL)
		locationLoggerLog(g_locationLogger, 8, "PositionManager::Close() END(0x%x)\n", ret);
	return ret;
}

/* (0x8118C70C) */
int PositionManager::ValidateLMethod(SceUInt32 *lmethod)
{
	if ((!vshSblAimgrIsDEX() && !vshSblAimgrIsTool() || system::Model::GetType() == 1) &&
	    (*lmethod & SCE_LOCATION_LMETHOD_MASK) == SCE_LOCATION_LMETHOD_GPS_EMULATION)
		return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;

	SceUInt32 m = *lmethod & SCE_LOCATION_LMETHOD_MASK;

	if (m != SCE_LOCATION_LMETHOD_NONE) {
		if (locationConfigIsEmulation(m_config))
			m = SCE_LOCATION_LMETHOD_GPS_EMULATION;
		else if (!m_config->HasGps() && m != SCE_LOCATION_LMETHOD_GPS_EMULATION &&
			 m != SCE_LOCATION_LMETHOD_WIFI)
			m = SCE_LOCATION_LMETHOD_WIFI;
	}
	*lmethod = (*lmethod & ~SCE_LOCATION_LMETHOD_MASK) | m;
	if (m == SCE_LOCATION_LMETHOD_GPS_EMULATION) {
		SceIoStat stat;

		if (sceIoGetstat(locationEmuGetFile(m_emuMgr), &stat) == 0)
			return 0;
		return SCE_LOCATION_DBGERROR_EMULATION_FILE_ERROR;
	}
	return 0;
}

/* (0x8118C7C0) */
int PositionManager::Open(int appId, SceLocationHandle *handle, SceUInt32 *lmethod,
			  SceUInt32 *hmethod, SceUInt32 *mode)
{
	if (g_locationLogger != NULL)
		locationLoggerLog(g_locationLogger, 8,
				  "PositionManager::Open() START lmethod:0x%08x initMode:0x%08x\n",
				  *lmethod & SCE_LOCATION_LMETHOD_MASK, *mode & 0xFFFFFF00u);

	int ret = locationConfigReload(m_config);

	if (ret == 0) {
		if (vshSblAimgrIsDEX() || vshSblAimgrIsTool())
			locationConfigLoadDebug(m_config);

		SceUInt32 lm = *lmethod;

		ret = ValidateLMethod(&lm);
		if (ret == 0) {
			ret = locationHandleMgrOpen(m_handleMgr, appId, handle, &lm, hmethod, mode);
			if (ret == 0) {
				int activeL = 0;
				int activeH = 0;

				ret = locationHandleMgrGetActiveMethods(m_handleMgr, &activeL,
									&activeH);
				if (ret != 0) {
					Close(appId, *handle);
					goto done;
				}
				locationConfigSetLMethod(m_config, &activeL);
				locationConfigSetHMethod(m_config, &activeH);
				m_config->lmethodFlags = activeL & 0x100;
				if (g_locationManager != NULL) {
					locationMgrAcquireActive(g_locationManager);

					bool loadedNow = false;
					SceUInt32 lmBits = m_config->activeLMethod & SCE_LOCATION_LMETHOD_MASK;
					SceUInt32 hmBits = m_config->activeHMethod & SCE_LOCATION_HMETHOD_MASK;

					if (locationHandleMgrGetCount(m_handleMgr) > 0 &&
					    m_providerLoaded == 0 && (lmBits | hmBits) != 0) {
						ret = LoadProvider();
						loadedNow = (ret >= 0);
					}
					if (loadedNow)
						locationTaskSchedule(&m_pollVtbl, 16);
					locationPollTaskNotify(&m_pollVtbl);
					if (ret == 0) {
						ret = UpdateHeadingState();
						if (ret == 0) {
							if (m_providerLoaded != 0) {
								if (!locationPermMgrCheck1(m_permMgr) ||
								    !locationPermMgrCheck2(m_permMgr))
									ret = locationPermMgrEnable(
										m_permMgr, m_provider);
								else
									ret = locationPermMgrDisable(
										m_permMgr, m_provider);
							}
							if (ret == 0)
								ret = UpdateEmuMode(&m_config->activeLMethod);
						}
					}
					if (ret != 0)
						Close(appId, *handle);
				} else {
					ret = SCE_LOCATION_ERROR_FATAL_ERROR;
					Close(appId, *handle);
				}

				if (ret == 0 && (*mode & 0xFFFFFF00u) == 0 &&
				    (*lmethod & SCE_LOCATION_LMETHOD_MASK) != 0 &&
				    locationHandleMgrCheckPermission(m_handleMgr, *handle) ==
					    SCE_LOCATION_INFO_DENIED_BY_USER) {
					unsigned char canConfirm = 0;

					if (locationHandleMgrCanConfirm(m_handleMgr, *handle,
									&canConfirm) == 0 &&
					    canConfirm == 1) {
						LocationConfirmMgr *confirm =
							locationConfirmGetInstance();

						if (confirm != NULL) {
							if (g_locationLogger != NULL)
								locationLoggerLog(
									g_locationLogger, 8,
									"PositionManager::Open() ConfirmStart\n");
							locationConfirmShow(confirm, appId, *handle,
									    0, 0, NULL, false);
							locationHandleMgrSetPending(m_handleMgr,
										    *handle, 1);
							locationHandleMgrSavePending(
								m_handleMgr, *handle, 0, 128, NULL,
								0, 1);
							ret = SCE_LOCATION_INFO_DENIED_BY_USER;
						}
					}
				}
			}
		}
	}
done:
	if (g_locationLogger != NULL)
		locationLoggerLog(g_locationLogger, 8, "PositionManager::Open() END(0x%x)\n", ret);
	return ret;
}

/* (0x8118CA5C) */
int PositionManager::Reopen(int appId, SceLocationHandle handle, SceUInt32 *lmethod,
			    SceUInt32 *hmethod)
{
	if (g_locationLogger != NULL)
		locationLoggerLog(g_locationLogger, 8, "PositionManager::Reopen() START\n");

	int ret;

	if (locationHandleMgrIsValid(m_handleMgr, appId, handle)) {
		int flags = locationHandleMgrGetFlags(m_handleMgr, appId, handle);

		if ((flags & 4) != 0)
			CancelGetLocation(appId, handle);
		if ((flags & 0x40) != 0)
			CancelGetLocation(appId, handle);
		if ((flags & 8) != 0)
			StopLocationCallback(appId, handle);
		if ((flags & 0x10) != 0)
			StopHeadingCallback(appId, handle);

		if (vshSblAimgrIsDEX() || vshSblAimgrIsTool())
			locationConfigLoadDebug(m_config);

		SceUInt32 lm = *lmethod;

		ret = ValidateLMethod(&lm);
		if (ret == 0) {
			if ((lm & SCE_LOCATION_LMETHOD_MASK) != 0 &&
			    locationHandleMgrCheckPermission(m_handleMgr, handle) ==
				    SCE_LOCATION_ERROR_DISABLE_APPLICATION)
				ret = SCE_LOCATION_ERROR_DISABLE_APPLICATION;
			if (ret != SCE_LOCATION_ERROR_DISABLE_APPLICATION) {
				ret = locationHandleMgrReopen(m_handleMgr, appId, handle, &lm,
							      hmethod);
				if (ret == 0) {
					int activeL = 0;
					int activeH = 0;

					ret = locationHandleMgrGetActiveMethods(m_handleMgr,
										&activeL, &activeH);
					if (ret == 0) {
						locationConfigSetLMethod(m_config, &activeL);
						locationConfigSetHMethod(m_config, &activeH);
						m_config->lmethodFlags = activeL & 0x100;
					}

					bool loadedNow = false;
					SceUInt32 lmBits = m_config->activeLMethod & SCE_LOCATION_LMETHOD_MASK;
					SceUInt32 hmBits = m_config->activeHMethod & SCE_LOCATION_HMETHOD_MASK;

					if (locationHandleMgrGetCount(m_handleMgr) > 0 &&
					    m_providerLoaded == 0 && (lmBits | hmBits) != 0) {
						int lr = LoadProvider();

						loadedNow = (lr >= 0);
						if (lr != 0)
							ret = lr;
					}
					if (m_providerLoaded != 0) {
						int pr;

						if (!locationPermMgrCheck1(m_permMgr) ||
						    !locationPermMgrCheck2(m_permMgr))
							pr = locationPermMgrEnable(m_permMgr,
										   m_provider);
						else
							pr = locationPermMgrDisable(m_permMgr,
										    m_provider);
						if (pr != 0)
							ret = pr;
					}
					if (loadedNow)
						locationTaskSchedule(&m_pollVtbl, 16);
					locationPollTaskNotify(&m_pollVtbl);

					int hr = UpdateHeadingState();

					if (hr != 0)
						ret = hr;

					int er = UpdateEmuMode(&m_config->activeLMethod);

					if (er != 0)
						ret = er;

					SceUInt32 mode = 0;

					locationHandleMgrGetMode(m_handleMgr, appId, handle, &mode);
					if (ret == 0 && (mode & 0xFFFFFF00u) == 0 &&
					    (*lmethod & SCE_LOCATION_LMETHOD_MASK) != 0 &&
					    locationHandleMgrCheckPermission(m_handleMgr, handle) ==
						    SCE_LOCATION_INFO_DENIED_BY_USER) {
						unsigned char canConfirm = 0;

						if (locationHandleMgrCanConfirm(m_handleMgr, handle,
										&canConfirm) == 0 &&
						    canConfirm == 1) {
							LocationConfirmMgr *confirm =
								locationConfirmGetInstance();

							if (confirm != NULL) {
								if (g_locationLogger != NULL)
									locationLoggerLog(
										g_locationLogger, 8,
										"PositionManager::Reopen() ConfirmStart\n");
								locationConfirmShow(confirm, appId,
										    handle, 0, 0,
										    NULL, false);
								locationHandleMgrSetPending(
									m_handleMgr, handle, 1);
								locationHandleMgrSavePending(
									m_handleMgr, handle, 0, 128,
									NULL, 0, 1);
								ret = SCE_LOCATION_INFO_DENIED_BY_USER;
							}
						}
					}
				}
			}
		}
	} else {
		ret = SCE_LOCATION_ERROR_INVALID_HANDLE;
	}
	if (g_locationLogger != NULL)
		locationLoggerLog(g_locationLogger, 8, "PositionManager::Reopen() END(0x%x)\n", ret);
	return ret;
}

/* (0x8118CDE8) */
int PositionManager::GetMethod(int appId, SceLocationHandle handle, SceUInt32 *lmethod,
			       SceUInt32 *hmethod)
{
	if (lmethod == NULL && hmethod == NULL)
		return SCE_LOCATION_ERROR_INVALID_ADDRESS;
	if (!locationHandleMgrIsValid(m_handleMgr, appId, handle))
		return SCE_LOCATION_ERROR_INVALID_HANDLE;

	int lm = 0;
	SceUInt32 hm = 0;

	locationConfigGetMethod(m_config, &lm, NULL);
	locationHandleMgrGetMethod(m_handleMgr, appId, handle, NULL, &hm);
	if (lmethod != NULL) {
		int r = locationHandleMgrCheckPermission(m_handleMgr, handle);

		if (r != SCE_LOCATION_INFO_DENIED_BY_USER && r != 0)
			return r;
		*lmethod = (SceUInt32)lm;
	}
	if (hmethod != NULL)
		*hmethod = hm;
	return 0;
}

/* (0x8118CE9E) */
int PositionManager::ComparePriority(int a, int b)
{
	if (locationPriorityValue(a) != locationPriorityValue(b))
		return (locationPriorityValue(a) < locationPriorityValue(b)) ? 1 : -1;
	return 0;
}

/* (0x8118CEDC) */
int PositionManager::IsAppActive(int appId, bool *outActive)
{
	SceAppMgrAppStatus status;
	int ret = sceAppMgrGetStatusByAppId(appId, &status);

	if (ret == 0)
		*outActive = (status.activated != 0 && status.processRunRequested != 0);
	return ret;
}

/* (0x8118CF20) */
int PositionManager::GetQueryLocation(int appId, SceLocationHandle handle, int unused,
				      SceRtcTick timestamp, SceUInt32 timeout,
				      SceLocationLocationInfo *info, bool option)
{
	if (g_locationLogger != NULL)
		locationLoggerLog(g_locationLogger, 8, "PositionManager::GetQueryLocation() START\n");

	SceUInt32 lmethod = 0;
	int ret;

	if (locationHandleMgrGetMethod(m_handleMgr, appId, handle, &lmethod, NULL) != 0) {
		ret = SCE_LOCATION_ERROR_INVALID_HANDLE;
	} else {
		ret = locationHandleMgrCheckPermission(m_handleMgr, handle);
		if (ret != 0) {
			if (ret == SCE_LOCATION_INFO_DENIED_BY_USER) {
				if ((lmethod & SCE_LOCATION_LMETHOD_MASK) != 0) {
					SceUInt32 mode = 0;

					locationHandleMgrGetMode(m_handleMgr, appId, handle, &mode);
					if ((mode & 0xFFFFFF00u) == SCE_LOCATION_MODE_CONFIRM_AUTO) {
						bool active = false;
						unsigned char canConfirm = 0;
						int stRet = IsAppActive(appId, &active);

						if (locationHandleMgrCanConfirm(m_handleMgr, handle,
										&canConfirm) != 0 ||
						    canConfirm != 1 || stRet != 0 || !active) {
							m_notifyEventCb(handle,
									SCE_LOCATION_INFO_DENIED_BY_USER,
									7);
						} else if (!option) {
							LocationConfirmMgr *confirm =
								locationConfirmGetInstance();

							if (confirm != NULL) {
								if (g_locationLogger != NULL)
									locationLoggerLog(
										g_locationLogger, 8,
										"PositionManager::GetQueryLocation() ConfirmStart\n");
								locationConfirmShow(confirm, appId,
										    handle, 0, 0,
										    NULL, false);
								locationHandleMgrSetPending(
									m_handleMgr, handle, 1);
								locationHandleMgrSavePending(
									m_handleMgr, handle, timeout,
									8, &timestamp, 0, 1);
								m_notifyEventCb(
									handle,
									SCE_LOCATION_INFO_DENIED_BY_USER,
									6);
							}
						}
					}
				} else {
					ret = SCE_LOCATION_ERROR_INVALID_LOCATION_METHOD;
				}
			}
		} else if ((lmethod & SCE_LOCATION_LMETHOD_MASK) != 0) {
			if (m_provider != NULL) {
				SceAppMgrAppStatus status;

				ret = sceAppMgrGetStatusByAppId(appId, &status);
				if (ret == 0) {
					ret = locationHandleMgrStartRequest(m_handleMgr, handle,
									    timeout, 4, &timestamp);
					if (ret == 0) {
						ret = m_provider->StartQuery(handle, status.appName,
									     &timestamp,
									     option ? 1 : 0);
						if (ret != 0) {
							int qstate = 0;

							locationHandleMgrGetQueryState(m_handleMgr,
										       handle,
										       &qstate);
							if (option || qstate != 8)
								locationHandleMgrCancelRequest(
									m_handleMgr, handle, 4);
							else
								locationHandleMgrSetQueryError(
									m_handleMgr, handle, ret);
						} else {
							ret = SCE_LOCATION_ERROR_INTERNAL_FILE_INVALID;
						}
					}
				}
			}
		} else {
			ret = SCE_LOCATION_ERROR_INVALID_LOCATION_METHOD;
		}
	}
	locationInitLocationInfo(info);
	info->timestamp = timestamp;
	if (g_locationLogger != NULL)
		locationLoggerLog(g_locationLogger, 8, "PositionManager::GetQueryLocation() END(0x%x)\n", ret);
	return ret;
}

/* (0x8118D164) */
int PositionManager::GetQueryLocationCache(int appId, SceLocationHandle handle,
					   SceLocationLocationInfo *info)
{
	if (g_locationLogger != NULL)
		locationLoggerLog(g_locationLogger, 8, "PositionManager::GetQueryLocationCache() START\n");

	int ret = locationHandleMgrCheckPermission(m_handleMgr, handle);

	if (ret == 0) {
		if (m_provider != NULL) {
			if ((locationHandleMgrGetFlags(m_handleMgr, appId, handle) & 0x40) != 0) {
				ret = locationHandleMgrGetQueryResult(m_handleMgr, handle, info);

				int cr = locationHandleMgrCancelRequest(m_handleMgr, handle, 4);

				if (cr != 0)
					ret = cr;
				if (ret != SCE_LOCATION_ERROR_FATAL_ERROR &&
				    ret != SCE_LOCATION_INFO_NETWORK_NOT_ACCESSED) {
					locationHistoryStatInc3((char *)m_logger + 137);
					if (ret == 0)
						locationHistoryStatInc4((char *)m_logger + 137);
				}
			}
		} else {
			ret = SCE_LOCATION_ERROR_FATAL_ERROR;
		}
	}
	if (g_locationLogger != NULL)
		locationLoggerLog(g_locationLogger, 8, "PositionManager::GetQueryLocationCache() END(0x%x)\n", ret);
	return ret;
}

/* (0x8118D220) */
int PositionManager::CancelQueryLocation(int appId, SceLocationHandle handle)
{
	if (g_locationLogger != NULL)
		locationLoggerLog(g_locationLogger, 8, "PositionManager::CancelQueryLocation() START\n");

	int ret;

	if (locationHandleMgrIsValid(m_handleMgr, appId, handle)) {
		ret = locationHandleMgrCheckPermission(m_handleMgr, handle);
		if (ret == 0 || ret == SCE_LOCATION_INFO_DENIED_BY_USER) {
			unsigned char pending = 0;

			locationHandleMgrIsPending(m_handleMgr, handle, &pending);
			if (pending != 0) {
				LocationConfirmMgr *confirm = locationConfirmGetInstance();

				if (confirm != NULL)
					ret = locationConfirmAbort(confirm, appId, handle);
				locationHandleMgrSetPending(m_handleMgr, handle, 0);
			} else {
				ret = locationHandleMgrCancelRequest(m_handleMgr, handle, 4);
				if (ret == 0)
					ret = m_provider->CancelQuery(handle);
			}
		}
	} else {
		ret = SCE_LOCATION_ERROR_INVALID_HANDLE;
	}
	if (g_locationLogger != NULL)
		locationLoggerLog(g_locationLogger, 8, "PositionManager::CancelQueryLocation() END(0x%x)\n", ret);
	return ret;
}

/* (0x8118D306) */
int PositionManager::GetLocation(int appId, SceLocationHandle handle, SceUInt32 timeout,
				 SceLocationLocationInfo *info, bool wifiHistory)
{
	if (g_locationLogger != NULL)
		locationLoggerLog(g_locationLogger, 8, "PositionManager::GetPosition() START\n");

	SceUInt32 lmethod = 0;
	int ret;

	if (locationHandleMgrGetMethod(m_handleMgr, appId, handle, &lmethod, NULL) != 0) {
		ret = SCE_LOCATION_ERROR_INVALID_HANDLE;
		goto fail_init;
	}

	ret = locationHandleMgrCheckPermission(m_handleMgr, handle);
	if (ret != 0) {
		if (ret == SCE_LOCATION_INFO_DENIED_BY_USER) {
			if ((lmethod & SCE_LOCATION_LMETHOD_MASK) == 0) {
				ret = SCE_LOCATION_ERROR_INVALID_LOCATION_METHOD;
				goto fail_init;
			}

			SceUInt32 mode = 0;

			locationHandleMgrGetMode(m_handleMgr, appId, handle, &mode);
			if ((mode & 0xFFFFFF00u) == SCE_LOCATION_MODE_CONFIRM_AUTO) {
				bool active = false;
				unsigned char canConfirm = 0;
				int stRet = IsAppActive(appId, &active);

				if (locationHandleMgrCanConfirm(m_handleMgr, handle, &canConfirm) != 0 ||
				    canConfirm != 1 || stRet != 0 || !active) {
					m_notifyEventCb(handle, SCE_LOCATION_INFO_DENIED_BY_USER, 7);
				} else {
					LocationConfirmMgr *confirm = locationConfirmGetInstance();

					if (confirm != NULL) {
						if (g_locationLogger != NULL)
							locationLoggerLog(
								g_locationLogger, 8,
								"PositionManager::GetLocation() ConfirmStart\n");
						locationConfirmShow(confirm, appId, handle, 0, 0,
								    NULL, false);
						locationHandleMgrSetPending(m_handleMgr, handle, 1);
						locationHandleMgrSavePending(
							m_handleMgr, handle, timeout,
							wifiHistory ? 4 : 1, NULL, 0, 1);
						m_notifyEventCb(handle,
								SCE_LOCATION_INFO_DENIED_BY_USER, 6);
					}
				}
			}
		}
		goto check_init;
	}

	if ((lmethod & SCE_LOCATION_LMETHOD_MASK) == 0) {
		ret = SCE_LOCATION_ERROR_INVALID_LOCATION_METHOD;
		goto fail_init;
	}

	{
		SceUInt32 mode = 0;

		locationHandleMgrGetMode(m_handleMgr, appId, handle, &mode);
		if (((mode & 0xFF) != 0 || (mode & 0xF0000) == 0x10000) &&
		    !m_config->IsDeviceEnabled()) {
			ret = SCE_LOCATION_INFO_DISABLE_DEVICE;
			goto fail_init;
		}
	}

	if (m_provider == NULL) {
		ret = SCE_LOCATION_ERROR_FATAL_ERROR;
		goto fail_init;
	}

	if (wifiHistory && m_wifiHistoryState == 0)
		ret = locationWifiHistorySetActive(&m_wifiHistoryState, 1);

	if (ret == 0) {
		ret = m_provider->GetLocation(info);
		if (wifiHistory)
			locationHistoryStatInc1((char *)m_logger + 137);
		if (ret == SCE_LOCATION_ERROR_INTERNAL_FILE_INVALID) {
			int reqType = wifiHistory ? 2 : 1;
			int sr = locationHandleMgrStartRequest(m_handleMgr, handle, timeout,
							       reqType, NULL);

			if (sr == 0) {
				locationBridgeNotify(m_bridge, 0);
				sr = locationPermMgrStart(m_permMgr, m_provider);
				if (sr == 0)
					goto check_init;
				locationHandleMgrCancelRequest(m_handleMgr, handle, reqType);
			}
			ret = sr;
		} else if (wifiHistory) {
			unsigned char hasWh = 0;
			int wr = locationHandleMgrHasWifiHistory(m_handleMgr, &hasWh);

			if ((wr | hasWh) == 0)
				wr = locationWifiHistorySetActive(&m_wifiHistoryState, 0);
			if (wr != 0)
				ret = wr;
		}
	}

check_init:
	if (ret == 0)
		goto done;
fail_init:
	locationInitLocationInfo(info);
done:
	if (g_locationLogger != NULL)
		locationLoggerLog(g_locationLogger, 8, "PositionManager::GetPosition() END(0x%x)\n", ret);
	return ret;
}

/* (0x8118D60C) */
int PositionManager::GetLocationCache(int appId, SceLocationHandle handle,
				      SceLocationLocationInfo *info, bool wifiHistory,
				      bool option)
{
	if (g_locationLogger != NULL)
		locationLoggerLog(g_locationLogger, 8, "PositionManager::GetLocationCache() START\n");

	int ret = locationHandleMgrCheckPermission(m_handleMgr, handle);

	if (ret == 0) {
		if (m_provider != NULL) {
			if ((locationHandleMgrGetFlags(m_handleMgr, appId, handle) & 0x20) != 0 &&
			    locationHandleMgrCancelRequest(m_handleMgr, handle, 3) == 0 &&
			    locationPermMgrStop(m_permMgr, m_provider) == 0)
				locationBridgeNotify(m_bridge, 0);

			SceUInt32 mode = 0;

			locationHandleMgrGetMode(m_handleMgr, appId, handle, &mode);
			if (((mode & 0xFF) != 0 || (mode & 0xF0000) == 0x10000) &&
			    !m_config->IsDeviceEnabled())
				ret = SCE_LOCATION_INFO_DISABLE_DEVICE;
			else
				ret = m_provider->GetLocationCache(info);

			if (wifiHistory) {
				if (ret == SCE_LOCATION_INFO_UNDETERMINED_LOCATION) {
					SceAppMgrAppStatus status;
					int stRet = sceAppMgrGetStatusByAppId(appId, &status);

					if (stRet != 0) {
						ret = stRet;
					} else {
						int hr = m_provider->GetWifiHistoryCache(
							status.appName, &info->timestamp,
							option ? 1 : 0);

						if (hr != 0)
							ret = hr;
					}
					locationHistoryStatInc2((char *)m_logger + 137);
				}

				unsigned char hasWh = 0;
				int wr = locationHandleMgrHasWifiHistory(m_handleMgr, &hasWh);

				if ((wr | hasWh) == 0)
					wr = locationWifiHistorySetActive(&m_wifiHistoryState, 0);
				if (wr < 0)
					ret = wr;
			}
		} else {
			ret = SCE_LOCATION_ERROR_FATAL_ERROR;
		}
	}
	if (ret != 0 && ret != SCE_LOCATION_INFO_UNDETERMINED_LOCATION)
		locationInitLocationInfo(info);
	if (g_locationLogger != NULL)
		locationLoggerLog(g_locationLogger, 8, "PositionManager::GetLocationCache() END(0x%x)\n", ret);
	return ret;
}

/* (0x8118D790) */
int PositionManager::StartLocationCallback(int appId, SceLocationHandle handle, SceUInt32 distance)
{
	if (g_locationLogger != NULL)
		locationLoggerLog(g_locationLogger, 8, "PositionManager::StartLocationCallback() START\n");

	SceUInt32 lmethod = 0;
	int ret = locationHandleMgrGetMethod(m_handleMgr, appId, handle, &lmethod, NULL);

	if (ret == 0) {
		ret = locationHandleMgrCheckPermission(m_handleMgr, handle);
		if (ret == 0 || ret == SCE_LOCATION_INFO_DENIED_BY_USER) {
			if ((lmethod & SCE_LOCATION_LMETHOD_MASK) != 0) {
				ret = locationHandleMgrStartLocCb(m_handleMgr, appId, handle,
								  distance);
				if (ret == 0) {
					locationBridgeNotify(m_bridge, 0);
					ret = locationPermMgrStart(m_permMgr, m_provider);
					if (m_logger != NULL)
						locationLoggerResetFileLog(m_logger);
				}
			} else {
				ret = SCE_LOCATION_ERROR_INVALID_LOCATION_METHOD;
			}
		}
	}
	if (g_locationLogger != NULL)
		locationLoggerLog(g_locationLogger, 8, "PositionManager::StartLocationCallback() END(0x%x)\n", ret);
	return ret;
}

/* (0x8118D866) */
int PositionManager::GetHeading(int appId, SceLocationHandle handle, SceLocationHeadingInfo *info)
{
	if (g_locationLogger != NULL)
		locationLoggerLog(g_locationLogger, 8, "PositionManager::GetHeading() START\n");

	SceUInt32 hmethod = 0;
	int ret = locationHandleMgrGetMethod(m_handleMgr, appId, handle, NULL, &hmethod);

	if (ret == 0) {
		if ((hmethod & SCE_LOCATION_HMETHOD_MASK) != 0) {
			float param = 0.0f;

			ret = locationHandleMgrGetHeadingParam(m_handleMgr, handle, &param);
			if (ret == 0) {
				ret = locationHeadingMgrGetHeading(m_headingMgr, (int *)&hmethod,
								   info);
				if (ret == 0)
					locationHandleMgrUpdateHeading(m_handleMgr, handle);
			}
		} else {
			ret = SCE_LOCATION_ERROR_INVALID_HEADING_METHOD;
		}
	}
	if (ret != 0)
		locationInitHeadingInfo(info);
	if (g_locationLogger != NULL)
		locationLoggerLog(g_locationLogger, 8, "PositionManager::GetHeading() END(0x%x)\n", ret);
	return ret;
}

/* (0x8118D936) */
int PositionManager::StartHeadingCallback(int appId, SceLocationHandle handle)
{
	if (g_locationLogger != NULL)
		locationLoggerLog(g_locationLogger, 8, "PositionManager::StartHeadingCallback() START\n");

	SceUInt32 hmethod = 0;
	int ret = locationHandleMgrGetMethod(m_handleMgr, appId, handle, NULL, &hmethod);

	if (ret == 0) {
		if ((hmethod & SCE_LOCATION_HMETHOD_MASK) != 0)
			ret = locationHandleMgrStartHeadCb(m_handleMgr, appId, handle);
		else
			ret = SCE_LOCATION_ERROR_INVALID_HEADING_METHOD;
	}
	if (g_locationLogger != NULL)
		locationLoggerLog(g_locationLogger, 8, "PositionManager::StartHeadingCallback() END(0x%x)\n", ret);
	return ret;
}

/* (0x8118D9E4) */
int PositionManager::GetPermission(int appId, SceLocationHandle handle,
				   SceLocationPermissionInfo *info)
{
	if (g_locationLogger != NULL)
		locationLoggerLog(g_locationLogger, 8, "PositionManager::GetPermissionStatus() START\n");

	int ret;

	if (!locationHandleMgrIsValid(m_handleMgr, appId, handle)) {
		ret = SCE_LOCATION_ERROR_INVALID_HANDLE;
		sce_paf_memset(info, 0, sizeof(*info));
	} else {
		int permRet = locationHandleMgrCheckPermission(m_handleMgr, handle);

		if (permRet != SCE_LOCATION_INFO_DENIED_BY_USER && permRet != 0) {
			ret = permRet;
		} else {
			info->parentalstatus = locationConfigGetParental(m_config);
			info->mainstatus = locationConfigGetMain(m_config);

			int ar = locationHandleMgrGetAppPerm(m_handleMgr, handle,
							     &info->applicationstatus);

			ret = (ar == 0) ? permRet : ar;

			unsigned char canConfirm = 0;
			int cr = locationHandleMgrCanConfirm(m_handleMgr, handle, &canConfirm);

			info->updated = ((cr | canConfirm) != 0) ? 1 : 0;
		}
		if (ret != SCE_LOCATION_INFO_DENIED_BY_USER && ret != 0)
			sce_paf_memset(info, 0, sizeof(*info));
	}
	if (g_locationLogger != NULL)
		locationLoggerLog(g_locationLogger, 8, "PositionManager::GetPermissionStatus() END(0x%x)\n", ret);
	return ret;
}

/* (0x8118DAE4) */
int PositionManager::CheckTimeout()
{
	return locationHandleMgrCheckTimeout(m_handleMgr);
}

/* (0x8118DAEE) */
int PositionManager::NotifyEvent(SceLocationHandle handle, int result, int eventType)
{
	if (result == SCE_LOCATION_INFO_TIMEOUT)
		m_provider->CancelQuery(handle);
	return m_notifyEventCb(handle, result, eventType);
}

/* (0x8118DB20) */
int PositionManager::NotifyHeading(SceLocationHandle handle, int unused)
{
	return m_notifyHeadingCb(handle, unused);
}

/* (0x8118DB2E) */
double PositionManager::CalcDistance(const SceLocationLocationInfo *a,
				     const SceLocationLocationInfo *b)
{
	double dlon = a->longitude - b->longitude;

	if (fabsf((float)dlon) > 180.0f) {
		if (a->longitude > b->longitude)
			dlon = a->longitude - (b->longitude + 360.0);
		else
			dlon = (a->longitude + 360.0) - b->longitude;
	}

	const double pi = 3.14159265358979323846;
	double avgLat = (a->latitude + b->latitude) * 0.5 * pi / 180.0;
	double dlat = (a->latitude - b->latitude) * pi / 180.0;

	dlon = dlon * pi / 180.0;

	double sinLat = sce_paf_sinf((float)avgLat);
	double w = sqrtf((float)(1.0 - 0.006694380022900788 * sinLat * sinLat));
	double m = 6335439.32708387 / (w * w * w);
	double n = 6378137.0 / w;
	double dx = n * dlon * sce_paf_cosf((float)avgLat);
	double dy = m * dlat;

	return sqrtf((float)(dx * dx + dy * dy));
}

/* (0x8118DC8A) */
int PositionManager::SetAppPermByTitleId(const char *titleId, int status)
{
	int ret = locationPermMgrSetApp(m_permMgr, titleId, status, -1);

	if (ret == 0) {
		int slot = locationPermMgrFindApp(m_permMgr, titleId);

		ret = locationHandleMgrUpdateAppPerm(m_handleMgr, slot, status);
		if (ret == 0) {
			if (sce_paf_strncmp(titleId, "NPXS10004", 32) == 0) {
				char alt[32];

				sce_paf_strncpy(alt, "NPXS10095", 32);
				ret = locationPermMgrSetApp(m_permMgr, alt, status, -1);
				if (ret != 0)
					return ret;
				slot = locationPermMgrFindApp(m_permMgr, alt);
				ret = locationHandleMgrUpdateAppPerm(m_handleMgr, slot, status);
			}
			if (ret == 0 && m_providerLoaded != 0) {
				if (!locationPermMgrCheck1(m_permMgr) ||
				    !locationPermMgrCheck2(m_permMgr))
					return locationPermMgrEnable(m_permMgr, m_provider);
				return locationPermMgrDisable(m_permMgr, m_provider);
			}
		}
	}
	return ret;
}

/* (0x8118DD5C) */
int PositionManager::ResetAllPerms(int val)
{
	int ret = locationPermMgrResetAll(m_permMgr);

	if (ret == 0) {
		ret = locationHandleMgrResetAllPerms(m_handleMgr, val);
		if (ret == 0 && m_providerLoaded != 0) {
			if (!locationPermMgrCheck1(m_permMgr) ||
			    !locationPermMgrCheck2(m_permMgr))
				return locationPermMgrEnable(m_permMgr, m_provider);
			return locationPermMgrDisable(m_permMgr, m_provider);
		}
	}
	return ret;
}

/* (0x8118DDA6) */
int PositionManager::GetMainPerm(int *out)
{
	*out = locationConfigGetMain(m_config);
	return 0;
}

/* (0x8118DDB6) */
int PositionManager::ReloadMainPerm()
{
	if (m_config->globalPerm != 1)
		return 0;

	int ret = locationConfigReloadMain(m_config);

	if (ret == 0) {
		if (m_providerLoaded != 0) {
			if (!locationPermMgrCheck1(m_permMgr) ||
			    !locationPermMgrCheck2(m_permMgr))
				ret = locationPermMgrEnable(m_permMgr, m_provider);
			else
				ret = locationPermMgrDisable(m_permMgr, m_provider);
		}
		int r = locationHandleMgrRefreshAllPerms(m_handleMgr);

		if (r != 0)
			return r;
	}
	return ret;
}

/* (0x8118DE0C) */
int PositionManager::GetParentalPerm(int *out)
{
	*out = locationConfigGetParental(m_config);
	return 0;
}

/* (0x8118DE1C) */
int PositionManager::ReloadParentalPerm()
{
	int ret = locationConfigReloadParental(m_config);

	if (ret == 0) {
		if (m_providerLoaded != 0) {
			if (!locationPermMgrCheck1(m_permMgr) ||
			    !locationPermMgrCheck2(m_permMgr))
				ret = locationPermMgrEnable(m_permMgr, m_provider);
			else
				ret = locationPermMgrDisable(m_permMgr, m_provider);
		}
		int r = locationHandleMgrRefreshAllPerms(m_handleMgr);

		if (r != 0)
			return r;
	}
	return ret;
}

/* (0x8118DE68) */
int PositionManager::GetGlobalPerm(int *out)
{
	*out = m_config->globalPerm;
	return 0;
}

/* (0x8118DE72) */
int PositionManager::GetAppPermSetting(int *out)
{
	*out = m_config->appPerm;
	return 0;
}

/* (0x8118DE7C) */
int PositionManager::ReloadConfig()
{
	return locationConfigReload(m_config);
}

/* (0x8118DE86) */
int PositionManager::SetHandlePermission(SceLocationHandle handle, int status)
{
	int ret = locationHandleMgrCheckPermission(m_handleMgr, handle);

	if (ret == SCE_LOCATION_INFO_DENIED_BY_USER || ret == 0) {
		ret = locationHandleMgrSetPermission(m_handleMgr, handle, status);
		if (ret == 0 && m_providerLoaded != 0) {
			if (!locationPermMgrCheck1(m_permMgr) ||
			    !locationPermMgrCheck2(m_permMgr))
				return locationPermMgrEnable(m_permMgr, m_provider);
			return locationPermMgrDisable(m_permMgr, m_provider);
		}
	}
	return ret;
}

/* (0x8118DEE0) */
int PositionManager::GetHandleStatus(int appId, SceLocationHandle handle)
{
	if (locationHandleMgrIsValid(m_handleMgr, appId, handle))
		return locationHandleMgrCheckPermission(m_handleMgr, handle);
	return SCE_LOCATION_ERROR_INVALID_HANDLE;
}

/* (0x8118DF06) */
int PositionManager::SetGpsEmulationFile(const char *path)
{
	if (vshSblAimgrIsDEX() || vshSblAimgrIsTool())
		return locationEmuSetFile(m_emuMgr, path);
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118DF32) */
int PositionManager::NotifyPermissionChanged()
{
	return locationHandleMgrNotifyPermChanged(m_handleMgr);
}

/* (0x8118DF3C) */
int PositionManager::GetValue(SceUInt32 *out)
{
	return locationConfigGetValue(m_config, out);
}

/* (0x8118DF46) */
int PositionManager::FlyingStartResume(const char *titleId)
{
	for (int i = 0; i < 8; i++) {
		if (sce_paf_strncmp(titleId, m_flyingTitles[i], 32) == 0)
			return 0;
	}
	for (int i = 0; i < 8; i++) {
		if (m_flyingTitles[i][0] == '\0') {
			sce_paf_memcpy(m_flyingTitles[i], titleId, 32);
			m_flyingCount++;
			return 0;
		}
	}
	return 0;
}

/* (0x8118DFA6) */
int PositionManager::FlyingStartLaunch(const char *titleId)
{
	for (int i = 0; i < 8; i++) {
		if (sce_paf_strncmp(titleId, m_flyingTitles[i], 32) == 0) {
			sce_paf_memset(m_flyingTitles[i], 0, 32);
			m_flyingCount--;
			return 0;
		}
	}
	return 0;
}

/* (0x8118DFF0) */
int PositionManager::IsHandlePending(SceLocationHandle handle, unsigned char *out)
{
	return locationHandleMgrIsPending(m_handleMgr, handle, out);
}

/* (0x8118DFFA) */
int PositionManager::SetHandlePending(SceLocationHandle handle, unsigned char pending)
{
	return locationHandleMgrSetPending(m_handleMgr, handle, pending);
}

/* (0x8118E006) */
int PositionManager::ResumePending(SceLocationHandle handle)
{
	int appId = 0;
	SceUInt32 timeout = 0;
	SceUInt32 kind = 0;
	SceRtcTick ts;
	int eventType = 0;
	int ret = locationHandleMgrGetPending(m_handleMgr, &appId, handle, &timeout, &kind, &ts);

	if (ret == 0) {
		SceLocationLocationInfo info;

		switch (kind) {
		case 1:
			ret = GetLocation(appId, handle, timeout, &info, false);
			if (ret == SCE_LOCATION_ERROR_INTERNAL_FILE_INVALID)
				eventType = 5;
			break;
		case 4:
			ret = GetLocation(appId, handle, timeout, &info, true);
			if (ret == SCE_LOCATION_ERROR_INTERNAL_FILE_INVALID)
				eventType = 5;
			break;
		case 8:
			ret = GetQueryLocation(appId, handle, 0, ts, timeout, &info, false);
			eventType = (ret == SCE_LOCATION_ERROR_INTERNAL_FILE_INVALID) ? 5 : 3;
			break;
		case 64:
			ret = 0;
			eventType = 1;
			break;
		case 128:
			ret = 0;
			eventType = 9;
			break;
		default:
			break;
		}
		m_notifyEventCb(handle, ret, eventType);
	}
	return ret;
}

/* (0x8118E11A) */
int PositionManager::PollActive()
{
	if (m_handleMgr != NULL)
		return locationHandleMgrPollActive(m_handleMgr, 1);
	return 1;
}

/* (0x8118E12C) */
int PositionManager::Dbg1()
{
	if (m_logger != NULL)
		return locationLoggerClearUr0Files(m_logger);
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E144) */
int PositionManager::Dbg4a(int *out)
{
	if (m_logger != NULL) {
		*out = locationLoggerGetField0(m_logger);
		return 0;
	}
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E160) */
int PositionManager::Dbg4b(int *out)
{
	if (m_logger != NULL) {
		*out = locationLoggerGetField4(m_logger);
		return 0;
	}
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E17C) */
int PositionManager::Dbg2(int mask)
{
	if (m_logger != NULL) {
		sceRegMgrSetKeyInt("/CONFIG/LOCATION", "enable_log", mask);
		if (mask != 0)
			return locationLoggerStartLog(m_logger, (SceUInt32)mask);
		return locationLoggerStopLog(m_logger);
	}
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E1BA) */
int PositionManager::Dbg3(int *out)
{
	if (m_logger != NULL) {
		*out = locationLoggerGetMask(m_logger);
		return 0;
	}
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E1D6) */
int PositionManager::Dbg5(int val)
{
	if (m_logger != NULL) {
		locationLoggerSetField8(m_logger, val);
		return 0;
	}
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E1EE) */
int PositionManager::Dbg6(int val)
{
	if (m_logger != NULL)
		return locationLoggerSetFieldC(m_logger, val);
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E206) */
int PositionManager::Dbg7(int idx)
{
	if (m_logger != NULL)
		return locationLoggerSetFileIndex(m_logger, idx);
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E21E) */
int PositionManager::Dbg8a(char *out)
{
	if (m_logger != NULL) {
		sce_paf_strncpy(out, locationLoggerGetCallbackTag(m_logger), 11);
		return 0;
	}
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E242) */
int PositionManager::Dbg8b(unsigned char *out)
{
	if (m_logger != NULL) {
		*out = (unsigned char)locationLoggerTakeFlag1F(m_logger);
		return 0;
	}
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E25E) */
int PositionManager::Dbg9(int *out)
{
	if (m_logger != NULL) {
		*out = (*((unsigned char *)m_logger + 16) != 0) ? 1 : 0;
		return 0;
	}
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E27A) */
int PositionManager::Dbg10(int val)
{
	if (m_logger != NULL) {
		locationLoggerSetField10(m_logger, val);
		return 0;
	}
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E292) */
int PositionManager::Dbg11(int *out)
{
	if (m_logger != NULL) {
		*out = locationLoggerGetField10(m_logger);
		return 0;
	}
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E2AE) */
int PositionManager::Dbg12()
{
	if (m_logger != NULL)
		return locationLoggerResetDebugPerm(m_logger);
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E2C6) */
int PositionManager::Dbg14(int *out)
{
	if (m_logger == NULL)
		return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
	*out = locationLoggerGetField20(m_logger);
	return 0;
}

/* (0x8118E2E2) */
int PositionManager::Dbg13(int val)
{
	if (m_logger != NULL)
		return locationLoggerSetField24(m_logger, val);
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E2FA) */
int PositionManager::Dbg15()
{
	if (locationHandleMgrGetCount(m_handleMgr) != 0)
		return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
	if (m_provider == NULL)
		return SCE_LOCATION_ERROR_FATAL_ERROR;

	locationPermMgrDisable(m_permMgr, m_provider);
	locationDebugBufLock(&m_pollVtbl);
	m_pollProvider = NULL;
	locationDebugBufUnlock(&m_pollVtbl);
	locationBridgeSetProvider(m_bridge, NULL);

	if (system::Model::GetType() == 0) {
		SceLibLocationProvider_8A796686(m_provider);
		SceLibLocationProvider_2A97FD02();
	} else {
		SceLibLocationDolceProvide_AF76021F(m_provider);
	}

	SceUID modId = m_providerModId;
	int status = 0;

	m_providerLoaded = 0;
	m_provider = NULL;
	int ret = sceKernelStopUnloadModule(modId, 0, NULL, 0, NULL, &status);

	locationEmuSetMode(m_emuMgr, 0);
	return ret;
}

/* (0x8118E39E) */
int PositionManager::Dbg17(const void *inArg)
{
	if (m_logger != NULL)
		return locationLoggerSetBuf28(m_logger, inArg);
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E3B6) */
int PositionManager::Dbg16(void *out)
{
	if (m_logger != NULL)
		return locationLoggerGetBuf48(m_logger, out);
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E3CE) */
int PositionManager::Dbg18(int *out)
{
	if (m_logger != NULL)
		return locationLoggerCountUr0Files(m_logger, out);
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E3E6) */
int PositionManager::Dbg19(void *out)
{
	if (m_logger != NULL)
		return locationLoggerGetBuf58(m_logger, out);
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E3FE) */
int PositionManager::Dbg20(int fromUx0)
{
	if (m_logger != NULL)
		return locationLoggerSyncDatFiles(m_logger, fromUx0);
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E416) */
int PositionManager::Dbg21(int val)
{
	if (m_logger != NULL)
		return locationLoggerSetField190(m_logger, val);
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E42E) */
int PositionManager::Dbg22(int val)
{
	if (m_logger != NULL)
		return locationLoggerSetField194(m_logger, val);
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E446) */
int PositionManager::Dbg23()
{
	if (m_logger != NULL)
		return locationLoggerExportHistory(m_logger);
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E45E) */
int PositionManager::Dbg24()
{
	if (m_logger != NULL)
		return locationLoggerImportHistory(m_logger);
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E476) */
int PositionManager::Dbg25()
{
	if (m_logger != NULL)
		return locationLoggerClearHistory(m_logger);
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E48E) */
int PositionManager::Dbg27(int *out1, int *out2)
{
	if (m_logger == NULL)
		return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;

	int ret = locationLoggerGetField198AndArr(m_logger, out1, out2);
	void *fileLog = *(void **)((char *)m_logger + 72);

	if (fileLog != NULL) {
		*(int *)((char *)fileLog + 408) = 0;
		sce_paf_memset((char *)fileLog + 412, 0, 20);
	}
	return ret;
}

/* (0x8118E4C0) */
int PositionManager::Dbg28(int val)
{
	m_config->dbgParam28 = val;
	return 0;
}

/* (0x8118E4C8) */
int PositionManager::Dbg31(int sec)
{
	m_config->flyingStartInterval = (unsigned int)(1000000 * sec);
	return 0;
}

/* (0x8118E4E2, 0x8118E65E) */
PositionManager::~PositionManager()
{
	locationMgrUnregisterTask(g_locationManager, &m_configRefreshTask);
	locationMgrUnregisterTask(g_locationManager, &m_powerRegTask);
	if (m_powerRegTask.m_cbId >= 0) {
		scePowerUnregisterCallback(m_powerRegTask.m_cbId);
		sceKernelDeleteCallback(m_powerRegTask.m_cbId);
		m_powerRegTask.m_cbId = -1;
	}
	if (m_providerLoaded != 0 && m_provider != NULL) {
		locationPermMgrDisable(m_permMgr, m_provider);
		locationDebugBufLock(&m_pollVtbl);
		m_pollProvider = NULL;
		locationDebugBufUnlock(&m_pollVtbl);
		locationBridgeSetProvider(m_bridge, NULL);
		if (system::Model::GetType() == 0) {
			SceLibLocationProvider_8A796686(m_provider);
			SceLibLocationProvider_2A97FD02();
		} else {
			SceLibLocationDolceProvide_AF76021F(m_provider);
		}

		int status = 0;

		m_providerLoaded = 0;
		m_provider = NULL;
		sceKernelStopUnloadModule(m_providerModId, 0, NULL, 0, NULL, &status);
		locationEmuSetMode(m_emuMgr, 0);
	}
	locationGpsTerm();
	if (m_logger != NULL) {
		locationLoggerTerm(m_logger);
		operator delete(m_logger);
	}
	locationMgrUnregisterTask(g_locationManager, &m_pollVtbl);
	if (m_permMgr != NULL)
		delete (LocationObjBase *)m_permMgr;
	if (m_config != NULL)
		m_config->Destroy();
	if (m_bridge != NULL)
		delete (LocationObjBase *)m_bridge;
	if (m_headingMgr != NULL)
		delete (LocationObjBase *)m_headingMgr;
	if (m_handleMgr != NULL)
		delete (LocationObjBase *)m_handleMgr;
	if (m_emuMgr != NULL)
		delete (LocationObjBase *)m_emuMgr;
	locationPollTaskDtor(&m_pollVtbl);
}

/* (0x8118E66E) */
int PositionManager::ProviderEventCB(int providerId, int eventType, int param, int status)
{
	if (s_positionManager != NULL)
		return locationHandleMgrOnProviderEvent(s_positionManager->m_handleMgr, providerId, eventType, param, status);
	return SCE_LOCATION_ERROR_FATAL_ERROR;
}

/* (0x8118E6A2) */
int PositionManager::OnWifiHistoryChange(int prev, int next)
{
	if (m_provider != NULL) {
		if (prev == 1 && next == 0)
			return m_provider->OnWifiHistoryStop();
		if (prev == 0 && next == 1)
			return m_provider->OnWifiHistoryStart();
	}
	return 0;
}
