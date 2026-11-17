/*
 * shell.h: SceShell's exports (link names SceShell_<NID>: no Sony name is known for them) and
 * what its files share. Names ours.
 */
#ifndef _SHELL_SHELL_H
#define _SHELL_SHELL_H

#include <scetypes.h>
#include <rtc.h>

/* the shell events boot.cpp listens to (names ours, from what the listeners do) */
enum {
	SHELL_EVENT_POWER	= 0,	/* the system resumed */
	SHELL_EVENT_MODEL	= 3,
};

/* the application events (SceShell_83D16CA8's), names ours from their listeners */
enum {
	SHELL_APP_EVENT_ENDED		= 0,
	SHELL_APP_EVENT_STARTED		= 3,
	SHELL_APP_EVENT_ACTIVATED	= 5,	/* sent by the shell: ShellAppActivatedEvent */
};

/* an application event's data (the fields the boot reads) */
typedef struct ShellAppEvent {
	int reserved;
	int appId;
} ShellAppEvent;

/* what a shell event listener is called with: the event's code and its data */
typedef void (*ShellEventCallback)(int code, const void *data, void *arg);

#ifdef __cplusplus
namespace paf { class Framework; namespace graph { class SurfacePool; } }

/* the framework: made at start (0x81002C1C), then run until the shell ends (0x81002CBA) */
extern paf::Framework *g_shellFramework;
void shellFrameworkCreate(void);
int shellFrameworkRun(void);

/* the singletons the framework's creation makes (0x810E19C8, 0x810E17F6, 0x81004AAE) */
int shellExclusiveFuncMgrCreate(void);		/* SceShellExclusiveFuncMgr */
int shellAppEventServerCreate(void);		/* SceShellAppEventServerMutex */
int shellEventServerCreate(void);		/* SceShellEventServerMutex */

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
int shellStartupGetPageCount(void);		/* home screen pages (pagec) */
bool shellStartupGetIcwvp(void);		/* id storage flag 2 (icwvp) */

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

/* the telephony setup (coldboot's telephony job) */
int shellTelephonyStartSetup(int type, const char *param);

/* the application events: sends one to its listeners (0x81241230) */
int shellAppEventSend(int event, const void *data, int flags);

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

/* a request to the trigger manager service, done by a SceTriggerMgrSvcCtrlJob (0x812A4A3A) */
int shellTriggerMgrSvcCtrl(int command, int arg);

/* the singletons created once: the parental control manager (it watches
   /CONFIG/SECURITY/PARENTAL, 0x81232F64) and the accessibility zoom (0x810A7D5A) */
int shellParentalControlMgrCreate(void);
int shellZoomMgrCreate(void);

/* the services the boot job starts (ShellBootJob::Run) */
int shellCloudDataGetMcId(int type, void *mcId);	/* vshAppMgrCloudDataGetMcId (0x81417DB4) */
int shellCloudDataSetMcId(int type, const void *mcId);	/* vshAppMgrCloudDataSetMcId (0x81417DBC) */
int shellLaunchListUpdate(void);		/* ur0:game/launch from version_launch.dat (0x810A258C) */
int shellLiblocationStart(void);		/* SceLiblocationPrivateHeap, ur0:liblocation (0x81002D8C) */
void shellCalibrationUpdateStart(void);		/* thread SceCalibrationUpdate (0x81002570) */
int shellCalibrationUpdateThreadCreate(void);	/* (0x810E181C) */
void shellCoredumpCleanup(void);		/* crash dumps in ux0:data/ removed (0x81002578) */
void shellRefreshCronStart(void);		/* a cron entry that posts a refresh (0x8100267C) */
int shellLiveAreaDailyCronStart(void);		/* SceLiveAreaDailyCronJob (0x8119A072) */
int shellRemoteDownloadStart(void);		/* SceRemoteDownloadLibJob, events 13 and 14 (0x81004610) */
int shellConsoleInfoStart(void);		/* ur0:ci, SceConsoleInfoSendJobQueue (0x810E5954) */
void shellNpFriendInit(void);			/* SceNpFriendVshModelAlloc, ...ImporterAlloc (0x810F5F4A) */
int shellAppStateMarkForeground(void);		/* flags the foreground application's slot (0x81244B62) */
int shellThemeApplyPending(void);		/* the theme left to apply, /CONFIG/THEME (0x81005834) */

/* the modules loaded at start: libshellsvc, libcdlg and the system modules (0x81002E0C) */
int shellModulesLoad(void);

/* the application event server's listeners (0x810E17DE) */
int shellAppEventServerAddListener(int event, ShellEventCallback callback, void *arg);

/* the exclusive function manager's MTP mode (0x810E22CA, 0x810E2392) and the memory it lends
   (0x810E239C, 0x810E23D8, 0x810E2458, 0x810E2414) */
int shellExclusiveFuncMtpBegin(SceUID thread);
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
int shellMtpHostFunc5(int arg);

/* the screen shown while the boot waits (a busy indicator) */
void shellBootScreenCreate(void);
void shellBootScreenClose(void);
#endif

#ifdef __cplusplus
extern "C" {
#endif

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
