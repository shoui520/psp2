/*
 * shell.h: SceShell's exports (link names SceShell_<NID>: no Sony name is known for them) and
 * what its files share. Names ours.
 */
#ifndef _SHELL_SHELL_H
#define _SHELL_SHELL_H

#include <scetypes.h>
#include <rtc.h>
#include <net.h>
#include <libnetctl.h>

/* the shell events boot.cpp listens to (names ours, from what the listeners do) */
enum {
	SHELL_EVENT_POWER	= 0,	/* the system resumed */
	SHELL_EVENT_MODEL	= 3,
};

/* what the singletons' Create functions give back (values only) */
#define SHELL_ERROR_ALREADY_CREATED	-2136014816	/* 0x80AF0020 */
#define SHELL_ERROR_NO_MEMORY		-2136014814	/* 0x80AF0022 */

/* the application events (SceShell_83D16CA8's), names ours from their listeners */
enum {
	SHELL_APP_EVENT_ENDED		= 0,
	SHELL_APP_EVENT_STARTED		= 3,
	SHELL_APP_EVENT_ACTIVATED	= 5,	/* sent by the shell: ShellAppActivatedEvent */
	SHELL_APP_EVENT_DEACTIVATED	= 6,	/* PLACEHOLDER name: the zoom's listener, after 5 */
};

/* the application whose status says which process is in front (appmgr's, name ours) */
#define SHELL_APP_ID_FRONT	-6

/* an application event's data (the fields the boot reads) */
typedef struct ShellAppEvent {
	SceUID processId;
	int appId;
	const char *titleId;
} ShellAppEvent;

/* what a shell event listener is called with: the event's code, its data and their size, and
   the argument the listener was added with */
typedef void (*ShellEventCallback)(int code, const void *data, SceSize size, void *arg);

#ifdef __cplusplus
#include <std/string>
#include <std/memory>
namespace paf { class Framework; namespace graph { class SurfacePool; class Surface; }
	namespace memory { class HeapAllocator; } }

/* the framework: made at start (0x81002C1C), then run until the shell ends (0x81002CBA) */
extern paf::Framework *g_shellFramework;
void shellFrameworkCreate(void);
int shellFrameworkRun(void);

/* the singletons the framework's creation makes (0x810E19C8, 0x810E17F6, 0x81004AAE) */
int shellExclusiveFuncMgrCreate(void);		/* SceShellExclusiveFuncMgr */
int shellAppEventServerCreate(void);		/* SceShellAppEventServerMutex */
int shellEventServerCreate(void);		/* SceShellEventServerMutex */

/* send an event to the shell event server's listeners of its code (0x81004AA6) */
int shellEventServerSend(int code, const void *data, SceSize size);

/* the IME service: SceImeService's server made and started (0x81002D84, 0x8113E642) */
int shellImeServiceStart(void);
int shellImeServerCreate(void);

/* the location library (statically linked): its heap, ur0:liblocation and its backends
   (0x811853E0), and the wait until it answers (0x81186730) */
int liblocationInit(void);
int liblocationWaitReady(void);
int shellLiblocationWaitReady(void);		/* (0x81002D94) */

/* startup: the settings read from the registry and the id storage (leaf 278) at boot */
int shellStartupGetInitialize(void);		/* /CONFIG/SYSTEM initialize */
bool shellStartupNeedsInitialSetup(void);	/* initialize bit 0 clear (0x81004D4C) */
bool shellStartupIsPmMode(void);		/* sceSblPmMgrGetCurrentMode gave 1 */
bool shellStartupIsTestMode(void);		/* false in this build (0x81409e2c, folded) */
int shellStartupGetIduMode(void);		/* 1: idu (demo unit) mode */
int shellStartupGetPageColor(void);		/* the pages' colour, their wave (pagec) (0x81004D7C) */
bool shellStartupGetIcwvp(void);		/* id storage flag 2 (icwvp) */
bool shellStartupHasPresetTheme(void);	/* a pd0 theme, id storage flag 4 (0x81004D94) */
bool shellStartupLeaveIduMode(void);		/* shell event 8 (0x81004E94) */

/* the icon cache: video memory and a surface pool for the home screen's icons */
void shellIconCacheInit(void);
paf::graph::SurfacePool *shellIconCacheGetSurfacePool(void);

/* the content database */
/* livespace_db's errors after which the trigger table is rebuilt (values only: no names known) */
#define SHELL_LSDB_TRIGGER_TABLE_STALE(call)	\
	((call) == (int)0x801010F1 || (call) == (int)0x801010F4 || (call) == (int)0x801010FE || \
	 (call) == (int)0x801010FF || (call) == (int)0x801010F2)
void shellLsdbStart(void);			/* livespace_db loaded, /CONFIG/NP callbacks (0x81002740) */
void shellActivityDbOpen(void);			/* act.db (0x81002826) */
int shellLsdbRegCallbackCreate(void);		/* SceLsdbRegCb (0x8100291A) */
void shellDbCheckAll(void);			/* the databases checked (0x81002954) */
void shellDatabaseCheckRecovery(void);		/* (0x81002A26) */
int shellDatabaseNeedsRecovery(void);		/* (0x81002A3A) */
int shellDatabaseGetState(void);
void shellTriggerTableReset(void);		/* removes ur0:shell/trigger_table.bin */
int shellWhatsNewInit(bool reset);		/* ur0:shell/whats_new, push_info (0x8122ACC0) */
int shellLsdbCallback(int *flag, int *value);	/* given to livespace_db (0x81326F12) */

/* musiccore_plugin's requests (through its IPC) */
int shellMusicStart(int mode);
int shellMusicSetBgm(bool on);

/* the telephony setup: ime_plugin and tel_initial_check_plugin run as an exclusive function, the
   callback called with its argument once the check is over (exclusive_func_tel_init.cpp) */
int shellTelephonyStartSetup(void (*callback)(void *arg), void *arg);

/* the application events: sends one to its listeners (0x81241230) */
int shellAppEventSend(int event, const void *data, int flags);

/* the shell's start (start.cpp): its time, the services started by its start job, and the
   error the start ended with (0x81004AE4) */
int main(int argc, char *argv[]);			/* the process main (0x81004AE4) */
SceUInt64 shellStartGetTime(void);			/* (0x81004C4C) */
/* SceThreadmgr's system time, low word (the user library's export; vitasdk:
   sceKernelGetSystemTimeLow) */
extern "C" SceUInt32 SceThreadmgr_47F6DE49(void);
int shellStartAppDataMount(void);			/* ux0 application data, id 105 (0x81004C1E) */
int shellStartAppDataUmount(void);			/* (0x81004C3C) */
int shellStartupInit(void);				/* the startup settings read (0x81004EC6) */
int shellSvcServerCreate(void);				/* SceShellSvc's IPMI server (0x811B7538) */
int shellSvcServerStart(void);				/* its thread (0x811B7550) */
void shellClosedCaptionMigrate(void);			/* (0x810A7760) */
int shellTriggerMgrStart(void);				/* (0x81006C60) */
int shellNpStart(void);					/* SceNpSystem, /CONFIG/NP env (0x81003F90) */
int shellNpServicesStart(void);				/* (0x8100463C) */
int shellNpNotifyStart(void);				/* (0x81004608) */
int shellShutterSoundInit(void);			/* (0x81004D38) */
int shellShutterSoundStart(void);			/* (0x812443E8) */
int shellRtcUtilSync(bool fromCallback);		/* /CONFIG/DATE, summer time (0x81365888) */
int shellCronStart(void);				/* the cron and its thread (0x810FB5D4) */
int shellCronStop(void);					/* (0x810FB612) */
int shellAppEventInit(void);				/* (0x81241238) */
int shellNetworkMgrCreate(bool enable);			/* (0x811C9070) */
void shellAccessibilityApply(void);			/* invert colours, key remap (0x810A76AA) */
int shellNoticeServiceStart(void);			/* (0x81005204) */
int shellNoticeServiceInit(void);			/* made and started (0x8139AB6C) */
void shellIncomingDialogServerCreate(void);		/* (0x8115B1CA) */

/*
 * An incoming dialog as the application asks for it through sceIncomingDialog (604 bytes;
 * PLACEHOLDER names, the type's after vitasdk's)
 */
struct SceIncomingDialogParam {
	SceInt32 sdkVersion;
	char titleId[16];		/* the application the dialog launches */
	char audioPath[128];		/* the sound played while it is up, in the application */
	SceUInt32 timeout;		/* in seconds */
	bool launchFlag;		/* passed on with the launch */
	SceUInt8 reserved[65];
	wchar_t acceptText[32];
	wchar_t rejectText[32];		/* empty for a dialog with one button */
	wchar_t dialogText[64];
	wchar_t noticeText[65];		/* the new event written when the dialog goes */
};

/*
 * A request of the incoming dialog server (0x8115B1CA keeps eight): the application's
 * parameters, then the server's own fields (PLACEHOLDER names).
 */
struct ShellIncomingDialogRequest : SceIncomingDialogParam {
	bool canceled;
	SceUID processId;		/* the application's; -1 for a free slot */
	SceUInt64 startTime;		/* sceKernelGetProcessTimeWide when it was shown */
	char *param;			/* the launch's param= argument, or NULL */
	SceInt32 reserved2;		/* nothing reads or writes it */
	SceUInt32 sequence;		/* the oldest pending request is taken first */
	SceInt32 state;			/* 1 pending, 2 taken, else its result */
	bool taken;
	bool freeOnRelease;		/* the slot is freed when its request is released */
};

/* the oldest pending request taken, or NULL (0x8115B228); a taken request given back with its
   result (0x8115B294) */
ShellIncomingDialogRequest *shellIncomingDialogServerTakeRequest(void);
void shellIncomingDialogServerReleaseRequest(ShellIncomingDialogRequest *request, int result);
int shellIsComTestMode(int *error);			/* id storage leaf 2 says test (0x81132AA2) */
int shellThemeLoad(void);				/* the current theme (0x810053D0) */
int shellConsoleInfoRecord(int type, SceInt64 tick, int arg);	/* ur0:ci, tick -1: now (0x81376B74) */
int shellStateWatcherCreate(void);			/* (0x8124D6BA) */
int shellAccessibilityMgrCreate(void);			/* (0x810A7616) */
int shellHandlerTableCreate(void);			/* (0x81248BB4) */
int shellUtilServerMgrCreate(void);			/* (0x8124BB02) */
int shellMainThreadPollerCreate(void);			/* (0x8121CFE2) */
/* the error dialog's types: the error is a blue screen, or another error */
#define SHELL_ERROR_DIALOG_BLUE_SCREEN	4
#define SHELL_ERROR_DIALOG_START	5
/* the registry manager's errors (0x800Dxxxx), the ones the error dialog is shown for (value
   only: no name known) */
#define SHELL_ERROR_FACILITY_REGMGR	0x00D
void shellErrorDialogShow(int type, int error, const paf::wstring *message, bool loadModule);	/* (0x810FB45E) */

/* a notice request: an ux0 theme whose activation has expired sends (0, -1, NULL)
   (0x8124B4DA) */
int shellSystemNoticeRequest(int type, int arg, const char *text);

/* the home screen told to update: 3 after a theme change (0x811CCE2C) */
int shellHomeUpdateRequest(SceUInt8 kind);

/* a callback added to the shell's table of eight, called with a result (0x8132773E) */
typedef void (*ShellTableCallback)(int result, void *arg);
int shellCallbackTableAdd(ShellTableCallback callback, void *arg);

/* the shell's livespace_db client (0x811C4BF8 makes it) */
struct AppInfoSClient;
struct ShellLsdbClient {
	AppInfoSClient *client;
};
extern ShellLsdbClient *g_shellLsdbClient;

/* the application in front, as the shell's screens follow it (0x810E17EE) */
int shellAppSetForeground(int type, const char *name, int flags);

/* the RTC (vshRtcSetCurrentTick) */
int shellRtcSetCurrentTick(const SceRtcTick *tick);

/* a counter of the shell's statistics, by id */
int shellCounterAdd(int id, unsigned int value, int flags);

/* the NP clock update (thread SceNpUpdateClock) */
int npUpdateClockStart(int *request, int priority, SceUID pid);
int npUpdateClockGetState(int request);		/* 2: done */
int npUpdateClockGetResult(int request, int *result);

/* a media information write, done by a SceMediaInfoWriterJob (0x810F3098); type 1-3 write
   the shell's own (NPXS19999) */
struct ShellMediaInfoRequest {
	int type;
	char titleId[12];
	int attr[4];
};
void shellMediaInfoWrite(const ShellMediaInfoRequest *request, bool cancelOnFailure);

/* the trigger manager: its heap, job queue and database, made at start (0x812A4876) */
int shellTriggerMgrInit(void);
/* a request to the trigger manager service, done by a SceTriggerMgrSvcCtrlJob (0x812A4A3A) */
int shellTriggerMgrSvcCtrl(int command, int arg);

/* the singletons created once: the parental control manager (it watches
   /CONFIG/SECURITY/PARENTAL, 0x81232F64) and the accessibility zoom (0x810A7D5A) */
int shellParentalControlMgrCreate(void);
int shellZoomMgrCreate(void);
void shellZoomDisable(void);			/* held off, and ended (0x810A832A) */
void shellZoomEnable(void);			/* let back (0x810A8344) */
void shellZoomReset(void);			/* ended (0x810A835C) */

/* the services the boot job starts (ShellBootJob::Run) */
int shellCloudDataGetMcId(int type, void *mcId);	/* vshAppMgrCloudDataGetMcId (0x81417DB4) */
int shellCloudDataSetMcId(int type, const void *mcId);	/* vshAppMgrCloudDataSetMcId (0x81417DBC) */
int shellLiblocationStart(void);		/* SceLiblocationPrivateHeap, ur0:liblocation (0x81002D8C) */
void shellCalibrationUpdateStart(void);		/* thread SceCalibrationUpdate (0x81002570) */
void shellCalibrationUpdateThreadCreate(void);	/* (0x810E181C) */
void shellCoredumpCleanup(void);		/* crash dumps in ux0:data/ removed (0x81002578) */
void shellRefreshCronStart(void);		/* a cron entry that posts a refresh (0x8100267C) */
int shellLiveAreaDailyCronStart(void);		/* SceLiveAreaDailyCronJob (0x8119A072) */
int shellRemoteDownloadStart(void);		/* SceRemoteDownloadLibJob, events 13 and 14 (0x81004610) */
int shellConsoleInfoStart(void);		/* ur0:ci, SceConsoleInfoSendJobQueue (0x810E5954) */
void shellNpFriendInit(void);			/* SceNpFriendVshModelAlloc, ...ImporterAlloc (0x810F5F4A) */

/* the NP friend libraries' memory block (SceNpFriendVshMemBlock, an mspace; np_friend.cpp) */
int shellNpFriendMemInit(SceSize size);		/* (0x810F5E88) */
void *shellNpFriendMalloc(SceSize size);	/* (0x810F5F18) */
void shellNpFriendFree(void *ptr);		/* (0x810F5F34) */

/* PLACEHOLDER names: the NP friend libraries' starts: the friend storage on its heap and its end
   (0x810F60FC, 0x810F6104), the importer on its heap and its end (0x813C541C, 0x813C5426), and the
   allocator the friend list uses (0x812BCEDC) */
struct NpFriendAllocator {
	void *(*malloc)(size_t size);
	void (*free)(void *ptr);
};

int npFriendStorageInit(paf::memory::HeapAllocator *heap);
int npFriendStorageTerm(void);
int npFriendImporterInit(paf::memory::HeapAllocator *heap);
int npFriendImporterTerm(void);
int npFriendListSetAllocator(const NpFriendAllocator *allocator);

/* PLACEHOLDER name: an SQLite error on a database told to the database recovery, which may ask
   the user to rebuild it (0x81371828) */
int shellDbRecoveryNotify(int error, const char *path, unsigned int flags);
/* PLACEHOLDER names: a screenshot taken, the callback given its result (0x81244BFE), and the
   callback the screenshot requests are given to (0x81244C42); 0x80102FA2 while the shell's
   screenshot service is not up */
int shellScreenshotCapture(void (*callback)(int *result, void *data), void *data);
int shellScreenshotSetRequestCallback(void (*callback)(void *data), void *data);
int shellAppStateMarkForeground(void);		/* flags the foreground application's slot (0x81244B62) */
/* PLACEHOLDER names: the screenshot service's counterpart of 0x81244B62 on the foreground
   application's slot (0x81244BB0), and the screen captured into a surface with the flags
   (0x81244C20; 0x80102FA2 while the service is not up) */
int Shell_81244BB0(void);
int shellScreenshotCaptureSurface(paf::intrusive_ptr<paf::graph::Surface> *surface, int flags);
int shellThemeApplyPending(void);		/* the theme left to apply, /CONFIG/THEME (0x81005834) */

/* the modules loaded at start: libshellsvc, libcdlg and the system modules (0x81002E0C) */
int shellModulesLoad(void);

/* the application event server's listeners added (0x810E17DE) and removed (0x810E17E6), and an
   event sent to them (0x810E17EE) */
int shellAppEventServerAddListener(int event, ShellEventCallback callback, void *arg);
int shellAppEventServerRemoveListener(int event, ShellEventCallback callback, void *arg);
int shellAppEventServerSend(int event, const void *data, SceSize size);

/* the exclusive function manager's MTP mode (0x810E22CA, 0x810E2392) and the memory it lends
   (0x810E239C, 0x810E23D8, 0x810E2458, 0x810E2414) */
int shellExclusiveFuncMtpBegin(void (*callback)(int event));
int shellExclusiveFuncMtpEnd(void);
void *shellExclusiveFuncMtpMalloc(SceSize size);
void *shellExclusiveFuncMtpMemalign(SceSize alignment, SceSize size);
void shellExclusiveFuncMtpFree(void *ptr);
void shellExclusiveFuncMtpPrintStats(void);

/* MTP: the host started at boot (0x81002F04), its registry callback (0x810030E2), its server
   (0x812B1B90) and what the server is told (0x812B1BCC, 0x812B1BE2, 0x812B1BF8, 0x812B1C0E) */
struct ShellMtpServerParam;
struct ShellMtpAppEvent {
	int value[3];
};
void shellMtpStart(void);
void shellMtpRegCallbackCreate(void);
int shellMtpServerStart(ShellMtpServerParam *param);
int shellMtpServerNotifyApp6(int a, int b, int c);
int shellMtpServerNotifyAppActivated(int a, int b, int c);
int shellMtpServerNotifyAppEnded(int a, int b, const char *c);
int shellMtpServerNotifyShellEvent(const void *data);
/* PLACEHOLDER: the function the server calls to have the console info sent (0x812B1B64), and an
   HTTP request the PC's host sends for the console (0x812B1B7A; an empty one ends the requests) */
struct ShellMtpHttpRequest {
	const char *host;
	const char *scheme;
	const char *uri;
	SceSize uriLength;
};
int shellMtpServerSetConsoleInfoCallback(int (*callback)(void), int arg);
int shellMtpServerHttpRequest(const ShellMtpHttpRequest *request, int timeout);
/* MTP over Wi-Fi: made at start (0x811C99E4), ended (0x812B1C7A) */
int shellMtpWiFiCreate(void);
int shellMtpWiFiTerminate(void);
/* the host's functions given to libmtp (0x810FCC30, 0x810FCC3A, 0x810FCC42, 0x810FCC4A,
   0x810FCC52, 0x810FCC5A) */
int shellMtpHostFunc0(int arg);
int shellMtpHostFunc1(int arg);
int shellMtpHostFunc2(int arg);
int shellMtpHostFunc3(int arg);
int shellMtpHostFunc4(int arg);
paf::string shellMtpHostFunc5(int taskId);	/* a task's folder, returned in libmtp's string */

/* the MTP Marlin service (SceMtpMlnSvc, 0x812BC4F0) started at boot (0x81003250) */
int shellMtpMarlinServiceStart(void);
void shellMtpMarlinStart(void);

/* the network: libraries started at boot (0x81003AFE) */
void shellNetStart(void);

/* what the WISPr login reports its events to */
typedef void (*ShellNetWisprCallback)(int event, int arg0, int arg1, int arg2, int arg3, int arg4);

/* what the shell's NetCtl server asks the shell (net.cpp's functions, in this order) */
struct ShellNetCtlHostFuncs {
	void (*getProcessStatus)(SceUID processId, int *allowed, int *isFront);
	int (*isProcessConnecting)(SceUID frontProcessId, SceUID processId);
	int (*hasNetworkThread)(SceUID processId);
	int (*exclusiveBegin)(int id, void *callback, void *arg);
	int (*exclusiveEnd)(int id);
	int (*isAnyProcessAllowed)(const SceUID *processIds, SceUInt32 count);
	int (*wisprStart)(ShellNetWisprCallback callback, int id, const char *ssid,
			  const void *bssid, int arg);
	void (*wisprStop)(void);
	int (*getMobileNetworkName)(char *name, SceSize size);
	bool (*isMobileNetworkAvailable)(void);
	void (*notifyComPrevented)(int wlan, int mobile);
	int (*getMobileNetworkStatus)(int *status);
};

/* the shell's NetCtl server (SceNetCtlInt, 0x8120E820), its IPC server (SceNetCtlIpcInt,
   0x8120CEB4) and what the host functions ask it (0x8120EEF0, 0x8120F070, 0x8120EE5A,
   0x8120F178) */
int shellNetCtlInit(SceSize inetStackSize, int inetPriority, SceSize adhocStackSize,
		    int adhocPriority, const ShellNetCtlHostFuncs *host);
int shellNetCtlIpcInit(void);
int shellNetCtlGetProcessState(SceUID processId);
int shellNetCtlIsProcessLinked(SceUID processId, SceUID frontProcessId);
int shellNetCtlGetActiveCount(void);
int shellNetCtlProcessEnded(SceUID processId);

/* the shell's NetCtl functions libnetctl calls in place of its requests (0x8120EA6C...) */
int shellNetCtlInetGetInfo(int code, SceNetCtlInfo *info);
/* PLACEHOLDER: a function called on the network's events (0x8120E87C) */
int shellNetCtlInetRegisterCallback(void (*callback)(int code, int event, int state), void *arg,
				    int *id);
int shellNetCtlInetGetState(int *state);
int shellNetCtlHandle_E8E8DF18(int *out);
int shellNetCtlHandle_CDEBA500(int code, void *info);
int shellNetCtlAdhocGetState(int *state);
int shellNetCtlHandle_0CC36330(int *out);
int shellNetCtlAdhocGetInAddr(SceNetInAddr *inaddr);
int shellNetCtlGetNatInfo(SceNetCtlNatInfo *natinfo);
int shellNetCtlHandle_F30365D6(void *info);
int shellNetCtlHandle_6A56FC3A(const char **ssids, SceUInt32 count, void *param);
int shellNetCtlHandle_47980B9B(void);
int shellNetCtlHandle_92EB6B1A(int a0, void *out);
int shellNetCtlHandle_A840C74C(void *buf, int count);
int shellNetCtlHandle_1A254169(void *a0, void *a1, int a2, int *a3);
int shellNetCtlHandle_27932A1B(void *a0, int a1, int *a2);
int shellNetCtlHandle_484EE0B2(void *buf, int count);
int shellNetCtlHandle_A59146E1(void *a0, void *a1, int a2, int *a3);
int shellNetCtlHandle_147B054C(int *out);
int shellNetCtlHandle_C15740E9(int *out);
int shellNetCtlHandle_CCF30B92(int a0);
/* also the table's 0x54 entry (one function for both) */
int shellNetCtlGetIfStat(int device, SceNetCtlIfStat *ifstat);
/* PLACEHOLDER name: shellNetCtlGetIfStat's body, the service started or not (0x8120EFAA) */
int shellNetGetIfStat(int device, SceNetCtlIfStat *ifstat);
int shellNetCtlHandle_777FE4C2(int a0, int a1);
int shellNetCtlHandle_8B2FEFAA(int a0, int a1);

/* the exclusive function the NetCtl server runs under (0x810E26CC, 0x810E290A) */
int shellExclusiveFuncNetBegin(int id, void *callback, void *arg);
int shellExclusiveFuncNetEnd(int id);

/* STUN (SceNetStun, 0x812181CC), UPnP (SceNetUpnp, 0x8121A038, 0x8121A06C) and the WISPr
   login (SceNetWispr, 0x8121ADB0, 0x8121AE02, 0x8121AE20, 0x8121ADE2) */
struct ShellNetUpnpParam {
	int enable;
};
struct ShellNetWisprParam {
	SceUInt8 bssid[8];
	char ssid[33];
	SceUInt8 reserved[3];
};
int shellNetStunInit(SceSize memorySize, int (*rand)(void));
int shellNetUpnpInit(SceSize stackSize, int priority, const char *env);
int shellNetUpnpSetParam(const ShellNetUpnpParam *param);
int shellNetWisprInit(ShellNetWisprCallback callback, SceSize stackSize, int priority,
		      int arg);
int shellNetWisprStart(int id, const ShellNetWisprParam *param);
int shellNetWisprStop(void);
void shellNetWisprTerm(void);

/* the screen shown while the boot waits (a busy indicator) */
void shellBootScreenCreate(void);
void shellBootScreenClose(void);
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* the NetCtl server's state for the shell's own process (0x8120EDE2, 0x8120EE00) */
int SceShell_5928D835(void);
int SceShell_871A8E92(void);

/* add a listener of an application event */
int SceShell_83D16CA8(int event, ShellEventCallback callback, void *arg);

/* SceNpManager's (np_manager_ipc_client.cpp): stops a request */
int SceNpManager_EB16514C(int value);

/* add and remove a listener of a shell event */
int SceShell_7FE4E732(int event, ShellEventCallback callback, void *arg);
int SceShell_E3D3A162(int event, ShellEventCallback callback, void *arg);
int SceShell_D4C7D2B7(int event, ShellEventCallback callback, void *arg);

#ifdef __cplusplus
}
#endif

#endif
