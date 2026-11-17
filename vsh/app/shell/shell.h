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

/* the RTC (vshRtcSetCurrentTick) */
int shellRtcSetCurrentTick(const SceRtcTick *tick);

/* a counter of the shell's statistics, by id */
int shellCounterAdd(int id, unsigned int value, int flags);

/* the NP clock update (thread SceNpUpdateClock) */
int npUpdateClockStart(int *request, int priority, SceUID pid);
int npUpdateClockGetState(int request);		/* 2: done */
int npUpdateClockGetResult(int request, int *result);

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
