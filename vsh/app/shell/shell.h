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
namespace paf { namespace graph { class SurfacePool; } }

/* startup: the settings read from the registry and the id storage (leaf 278) at boot */
int shellStartupGetInitialize(void);		/* /CONFIG/SYSTEM initialize */
bool shellStartupIsPmMode(void);		/* sceSblPmMgrGetCurrentMode gave 1 */
bool shellStartupIsTestMode(void);		/* false in this build (0x81409e2c, folded) */
int shellStartupGetIduMode(void);		/* 1: idu (demo unit) mode */
int shellStartupGetPageCount(void);		/* home screen pages (pagec) */
bool shellStartupGetIcwvp(void);		/* id storage flag 2 (icwvp) */

/* the icon cache: video memory and a surface pool for the home screen's icons */
void shellIconCacheInit(void);
paf::graph::SurfacePool *shellIconCacheGetSurfacePool(void);

/* the content database */
int shellDatabaseNeedsRecovery(void);
int shellDatabaseGetState(void);
void shellTriggerTableReset(void);		/* removes ur0:shell/trigger_table.bin */

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
