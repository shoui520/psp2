/*
 * The game plugin's main object and the functions of its interface: what it keeps while it
 * runs (its work pool, its alarm, the promoter of the game card), the calls that tell
 * topmenu_plugin and power_manage_plugin it is at work, and the interface's entries that pass on
 * to the plugin's other files. The file's and every name here are ours (no record names them).
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/processmgr.h>
#include <std/string.h>
#include <std/string>
#include <std/vector>
#include <common/shared_ptr.h>
#include <memory/pool_allocator.h>
#include <thread/thread.h>
#include <timer/timer.h>
#include <timer/timerlistener.h>
#include <timer/timerlistenerlist.h>
#include <file/localfile.h>
#include <system/system.h>
#include <post_ss_mgr/sblpmmgr.h>
#include <plugin.h>
#include <shell_svc/shell_util.h>

#include "../../shell/boot.h"
#include "../../shell/plugins.h"
#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"

using namespace paf;

/* the work pool's size and block: one block of 64 KiB */
#define GAME_POOL_SIZE		0x10000
#define GAME_POOL_BLOCK_SIZE	0x10000

/* how often the power tick listener keeps the system awake while the plugin works, in ms */
#define GAME_POWER_TICK_INTERVAL	10000.0f

/* the path a promote of the new application's LiveArea is asked for */
#define GAME_APPMETA_NEW	"ux0:appmeta/new"

/* the shell lock the plugin's installs hold (value only; PLACEHOLDER: what it locks is not read) */
#define GAME_SHELL_LOCK_TYPE	0x200

/* the game card's own param.sfo */
#define GAME_GC_PARAM_SFO	"gro0:gc/param.sfo"

/* the part of an application whose delete is followed by a promote from its original path
   (value only; PLACEHOLDER name: which part kind 3 names is not read) */
#define GAME_APP_PART_REPROMOTE	3

/* what an application's size counts: every part of it (value only) */
#define GAME_SIZES_ALL		15

/* LAREA_TYPE's value for an application without its own LiveArea (value only) */
#define GAME_LAREA_TYPE_NONE	3

/* where the PSP emulator keeps the PSP games installed on the memory card */
#define GAME_PSPEMU_GAME_DIR	"ux0:pspemu/PSP/GAME"

/* the applications' licenses */
#define GAME_LICENSE_APP_DIR	"ux0:license/app/"

/* the PocketStation's license, a RIF in the PSP games' license folder */
#define GAME_PSP_LICENSE_DIR	"ux0:pspemu/PSP/LICENSE"
#define GAME_POCKETSTATION_RIF	"/JA0003-PCSC80018_00-POCKETSTATION001.rif"

/* s_dolcePsmApps's length, and the title ID that would end it early */
#define GAME_DOLCE_PSM_APPS	72
#define GAME_DOLCE_PSM_APPS_END	"00000XXXX"

/*
 * What keeps the system from sleeping while the plugin works: the power save timers cancelled
 * each time its timer finishes (0x8140ED68).
 */
class GamePowerTickListener : public TimerListener {
public:
	GamePowerTickListener(Timer *timer, void *data) : TimerListener(timer, data)
	{
	}

	virtual ~GamePowerTickListener()
	{
	}

	virtual void OnFinish(void *data1, void *data2)
	{
		sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_OLED_OFF |
				   SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);
	}
};

static ShellPluginInterface *s_topmenu = NULL;
static GamePowerTickListener *s_powerTick = NULL;
static void (*s_stopCallback)(void *arg) = NULL;
static void *s_stopArg = NULL;
static GameAlarm *s_alarm = NULL;
static GameLicenseUpdater *s_licenseUpdater = NULL;
static thread::Cond *s_cond = NULL;

/* the PlayStation Mobile applications a PS TV runs while its launch check is on */
static const char *s_dolcePsmApps[] = {
	"NPPA02101", "NPPA02105", "NPPA02106", "NPPA02571", "NPPA02575", "NPPA02576",
	"NPNA00122", "NPNA00137", "NPOA00012", "NPOA00102", "NPOA00103", "NPOA00104",
	"NPOA00107", "NPOA00109", "NPOA00110", "NPOA00114", "NPOA00121", "NPOA00126",
	"NPOA00132", "NPPA00106", "NPPA00121", "NPPA00127", "NPPA00138", "NPNA00106",
	"NPNA00110", "NPNA00116", "NPNA00117", "NPNA00118", "NPNA00120", "NPNA00123",
	"NPNA00126", "NPNA00128", "NPOA00017", "NPOA00018", "NPOA00022", "NPOA00030",
	"NPOA00031", "NPOA00032", "NPOA00119", "NPOA00123", "NPOA00124", "NPOA00127",
	"NPOA00129", "NPOA00130", "NPOA00133", "NPPA00104", "NPPA00107", "NPPA00109",
	"NPPA00120", "NPPA00124", "NPPA00129", "NPPA00134", "NPPA00139", "NPPA00141",
	"NPPA00143", "NPPA00144", "NPPA00145", "NPPA00157", "NPQA00104", "NPPA00011",
	"NPPA00003", "NPNA00125", "NPOA00131", "NPPA00158", "NPNA00142", "NPNA00143",
	"NPNA00139", "NPPA00163", "NPOA00021", "NPOA00029", "NPNA00112", "NPNA00109",
};

static GamePlugin *s_gamePlugin = NULL;

/* the categories of a game's application, of EG, of ME and of a PlayStation Mobile application */
static const char *s_gameCategories[] = { "gd", "gp", "gdc", "gdd", "gpc", "gpd" };
static const char *s_egCategories[] = { "EG" };
static const char *s_meCategories[] = { "ME" };
static const char *s_psmCategories[] = { "mb", "mba" };

static bool s_started;
static thread::Mutex s_mutex("SceGamePlugin", 0);
static thread::Mutex s_condMutex("SceGamePluginCond", 0);
static thread::Mutex s_setupUr0Mutex("SceGamePluginSetupUr0", 0);
static int s_topmenuBusyCount;
static common::SharedPtr<memory::PoolAllocator> s_pool;
static bool s_topmenuHeld;
static bool s_powerManageHeld;
static string s_pspemuWorkMountPoint;
static bool s_repromoting;

/* PLACEHOLDER: defined in another file of the shell, what it does is not read yet */
int gameLockedJobRun(void *arg, int arg1);	/* 0x8103DCF4 */
int gameContentImportPrepare(const char *titleId);	/* 0x810562E4 */
int gameContentImportRun(const char *titleId);		/* 0x810569FC: the installing dialog */
int gameContentImportCheck(const char *titleId);	/* 0x8105718C */
int gamePspAddcontInfoCheck(const char *path, vector<string> *titleIds, bool *found);	/* 0x8109F2E4 */
int gameAddcontInfoCheck(const char *path, vector<string> *dirs, bool *found);		/* 0x8109B7A6 */
int gameDolceAppCheck(const string &titleId, bool *allowed);	/* 0x810A4A24 */
int gameLaunchEmuListCheck(const string &titleId, bool *listed);	/* 0x810A4AA2: list_launch_emu.dat */
int gameUr0AppmetaSetup(const string *titleId, const char *path);	/* 0x81062CC6: ur0:temp/game */
int gameUmassCheck(int arg, bool *found);	/* 0x8106371E: ux0:umass */
int gameFttLicenseRead(common::SharedPtr<File> file, GameFttLicense *license);	/* 0x810A4EC4 */
bool gameLaunchCheckIsOn(void);		/* 0x810A4B20: /CONFIG/SYSTEM/launch_check, on without it */

/* topmenu_plugin's interface, found once */
static inline ShellPluginInterface *gameTopmenuInterface(void)
{
	if (s_topmenu == NULL) {
		Plugin *topmenu = Plugin::Find("topmenu_plugin");

		if (topmenu != NULL)
			s_topmenu = (ShellPluginInterface *)topmenu->GetInterface(1);
	}

	return s_topmenu;
}

static void gameOnShellEvent(int result, int event, int param, void *arg);

/* the plugin's work begun: GAME_ERROR_STOPPING while a stop is requested */
static inline int gameEnter(void)
{
	s_mutex.Lock();

	if (s_stopCallback != NULL) {
		s_mutex.Unlock();
		return GAME_ERROR_STOPPING;
	}

	return 0;
}

/* the plugin's work ended: a requested stop is told it may go on */
static inline void gameLeave(void)
{
	s_mutex.Unlock();

	if (s_stopCallback != NULL)
		s_stopCallback(s_stopArg);
}

/* the shell locked for an install, once the shell says it is (gameOnShellEvent) */
static inline void gameShellLock(void)
{
	s_condMutex.Lock();
	sceShellUtilRegisterEventHandler(gameOnShellEvent, NULL);
	sceShellUtilLock(GAME_SHELL_LOCK_TYPE);
	s_cond->Wait(0);
	s_condMutex.Unlock();
}

static inline void gameShellUnlock(void)
{
	sceShellUtilUnlock(GAME_SHELL_LOCK_TYPE);
	sceShellUtilRegisterEventHandler(NULL, NULL);
}

/* topmenu kept busy and the system awake while the plugin works */
static inline void gameWorkBegin(void)
{
	gameTopmenuBusyBegin();
	sceKernelPowerLock(SCE_KERNEL_POWER_TICK_DEFAULT);
	if (TimerListenerList::s_default_list != NULL)
		TimerListenerList::s_default_list->Register(
			TimerListenerList::ListenerParam(s_powerTick, true, NULL, NULL));
}

static inline void gameWorkEnd(void)
{
	if (TimerListenerList::s_default_list != NULL)
		TimerListenerList::s_default_list->Unregister(s_powerTick);
	sceKernelPowerUnlock(SCE_KERNEL_POWER_TICK_DEFAULT);
	gameTopmenuBusyEnd();
}

/* (0x8102AFFC) */
GamePlugin::GamePlugin(Plugin *plugin) : m_plugin(plugin)
{
	s_powerTick = new GamePowerTickListener(new Timer(GAME_POWER_TICK_INTERVAL), this);
	s_cond = new thread::Cond(s_condMutex, "SceGameCond", 0);

	gameGcPromoteCreate(this);
	gameGcPromoteSetNoticeHeld(shellBootIsLaunchRestricted());

	memory::PoolAllocator::Option option;

	option.disable_dbgprint = false;
	option.disable_assert_on_alloc_failure = true;
	option.memblk_type = memory::AllocationBlockType_User;
	s_pool = common::SharedPtr<memory::PoolAllocator>(
		new memory::PoolAllocator(NULL, GAME_POOL_SIZE, GAME_POOL_BLOCK_SIZE, "SceGamePool",
					  &option));

	s_alarm = new GameAlarm;
	s_licenseUpdater = new GameLicenseUpdater;
	s_started = true;
}

/* The plugin's main object made (0x8102B296). */
void gamePluginCreate(Plugin *plugin)
{
	s_gamePlugin = new GamePlugin(plugin);
}

/* The plugin's main object deleted (0x8102B2BA). */
void gamePluginDestroy(void)
{
	if (s_gamePlugin != NULL)
		delete s_gamePlugin;
	s_gamePlugin = NULL;
}

bool gameIsStopRequested(void)
{
	return s_stopCallback != NULL;
}

__attribute__((noinline)) void gameShellHold(bool topmenu, bool powerManage)
{
	if (topmenu) {
		thread::RMutex::main_thread_mutex.Lock();

		ShellPluginInterface *iface = gameTopmenuInterface();

		if (iface != NULL) {
			iface->func[TOPMENU_PLUGIN_FUNC_GAME_HOLD]();
			s_topmenuHeld = true;
		}

		thread::RMutex::main_thread_mutex.Unlock();
	}

	if (powerManage) {
		Plugin *plugin = Plugin::Find("power_manage_plugin");

		if (plugin != NULL) {
			ShellPluginInterface *iface = (ShellPluginInterface *)plugin->GetInterface(1);

			if (iface != NULL) {
				iface->func[POWER_MANAGE_PLUGIN_FUNC_GAME_HOLD]();
				s_powerManageHeld = true;
			}
		}
	}
}

__attribute__((noinline)) void gameShellRelease(void)
{
	if (s_topmenuHeld) {
		thread::RMutex::main_thread_mutex.Lock();

		ShellPluginInterface *iface = gameTopmenuInterface();

		if (iface != NULL) {
			iface->func[TOPMENU_PLUGIN_FUNC_GAME_RELEASE]();
			s_topmenuHeld = false;
		}

		thread::RMutex::main_thread_mutex.Unlock();
	}

	if (s_powerManageHeld) {
		Plugin *plugin = Plugin::Find("power_manage_plugin");

		if (plugin != NULL) {
			ShellPluginInterface *iface = (ShellPluginInterface *)plugin->GetInterface(1);

			if (iface != NULL) {
				iface->func[POWER_MANAGE_PLUGIN_FUNC_GAME_RELEASE]();
				s_powerManageHeld = false;
			}
		}
	}
}

void gameTopmenuBusyBegin(void)
{
	ShellPluginInterface *iface = gameTopmenuInterface();

	if (iface != NULL) {
		if (s_topmenuBusyCount == 0)
			iface->func[TOPMENU_PLUGIN_FUNC_BUSY_BEGIN]();
		s_topmenuBusyCount++;
	}
}

void gameTopmenuBusyEnd(void)
{
	ShellPluginInterface *iface = gameTopmenuInterface();

	if (iface != NULL && --s_topmenuBusyCount == 0)
		iface->func[TOPMENU_PLUGIN_FUNC_BUSY_END]();
}

bool gameCategoryIsGame(const char *category)
{
	for (SceUInt32 i = 0; i < 6; i++)
		if (sce_paf_strncmp(category, s_gameCategories[i], 4) == 0)
			return true;

	return false;
}

bool gameCategoryIsEg(const char *category)
{
	for (int i = 0; i < 1; i++)
		if (sce_paf_strncmp(category, s_egCategories[i], 4) == 0)
			return true;

	return false;
}

bool gameCategoryIsMe(const char *category)
{
	for (int i = 0; i < 1; i++)
		if (sce_paf_strncmp(category, s_meCategories[i], 4) == 0)
			return true;

	return false;
}

bool gameCategoryIsPsm(const char *category)
{
	for (SceUInt32 i = 0; i < 2; i++)
		if (sce_paf_strncmp(category, s_psmCategories[i], 4) == 0)
			return true;

	return false;
}

/* An application's category kind, once started (0x8102B5BA). */
int gamePluginGetCategoryKind(const LsdbAppInfoPtr *info, int *kind)
{
	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	return gameAppInfoGetCategoryKind(info, kind);
}

/* An application's storage, once started (0x8102B5D8). */
int gamePluginGetStorage(const LsdbAppInfoPtr *info, int *storage)
{
	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	return gameAppGetStorage(info, storage);
}

int gamePluginGetPool(const char *user, common::SharedPtr<memory::PoolAllocator> *pool)
{
	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	if (s_pool.get() == NULL || s_pool.m_ref_counter->m_counter != 1 ||
	    s_pool->GetFreeSize() != GAME_POOL_SIZE)
		return GAME_ERROR_POOL_BUSY;

	*pool = s_pool;

	return 0;
}

void gamePluginReleasePool(common::SharedPtr<memory::PoolAllocator> *pool)
{
	if (s_started)
		pool->reset();
}

int gameAppDirMount(const char *appPath, char *mountPoint, SceSize size)
{
	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	if (appPath == NULL || mountPoint == NULL || size <= 16)
		return GAME_ERROR_INVALID_ARGUMENT;

	string point;
	int ret = gameGameDataMount(appPath, NULL, NULL, &point);

	if (ret < 0)
		return ret;

	if (point.length() >= size)
		return GAME_ERROR_BUFFER_TOO_SMALL;

	sce_paf_strncpy(mountPoint, point.c_str(), point.length());
	mountPoint[point.length()] = '\0';

	return 0;
}

int gameAppDirUnmount(const char *mountPoint)
{
	string point(mountPoint);

	return gameAppUnmount(&point);
}

int gamePluginPspemuWorkMount(void)
{
	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	return gamePspemuWorkMount(&s_pspemuWorkMountPoint);
}

int gamePluginPspemuWorkUnmount(void)
{
	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	return gamePspemuWorkUnmount(&s_pspemuWorkMountPoint);
}

job::JobQueue *gamePluginGetJobQueue(void)
{
	if (!s_started)
		return NULL;

	return gameGcPromoteGetJobQueue();
}

int gamePluginGetAddcontTitles(vector<string> *titles)
{
	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	return gameGcPromoteGetPendingTitles(titles);
}

void gamePluginSetDialogHold(bool hold)
{
	if (s_started)
		gameGcPromoteSetDialogHold(hold);
}

int gamePluginPromote(const char *path, bool report, GameAppRegistration *reg)
{
	int ret;

	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	ret = gameEnter();
	if (ret < 0)
		return ret;

	gameWorkBegin();
	gameShellHold(true, false);

	if (sce_paf_strncmp(path, GAME_APPMETA_NEW, sizeof(GAME_APPMETA_NEW)) == 0) {
		ret = 1;
	} else {
		GameAppPromoter *promoter = new GameAppPromoter(path, false, reg, gameIsStopRequested,
								0, 0, 0, 0);

		if (promoter == NULL) {
			ret = GAME_ERROR_NO_INFO_MEMORY;
		} else {
			ret = promoter->Run();
			if (ret >= 0) {
				LsdbAppInfo::iterator it = reg->info->find(LSDB_KEY_LAREA_TYPE);

				if (it != reg->info->end() &&
				    (int)gameLsdbValueInteger(it->second) == GAME_LAREA_TYPE_NONE)
					reg->liveAreaState = 0;

				if (report && promoter->m_promoted) {
					if (!promoter->m_installed)
						ret = 2;
					else
						ret = promoter->m_replaced ? 2 : 1;
				} else {
					ret = 0;
				}
			}
		}

		if (promoter != NULL)
			delete promoter;
	}

	gameShellRelease();
	gameWorkEnd();
	gameLeave();

	return ret;
}

/*
 * An application promoted from its directory at path with the shell locked, its registration
 * thrown away (0x8102BBAC).
 */
int gamePluginPromoteLocked(const char *path)
{
	int ret;

	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	if (path == NULL)
		return GAME_ERROR_INVALID_ARGUMENT;

	ret = gameEnter();
	if (ret < 0)
		return ret;

	gameShellLock();
	gameWorkBegin();
	gameShellHold(true, false);

	GameAppRegistration reg;

	if (reg.info.get() != NULL && reg.entry.get() != NULL) {
		GameAppPromoter *promoter = new GameAppPromoter(path, true, &reg, gameIsStopRequested,
								0, 0, 0, 0);

		if (promoter == NULL) {
			ret = GAME_ERROR_NO_INFO_MEMORY;
		} else {
			ret = promoter->m_result;
			if (ret >= 0) {
				ret = promoter->Run();
				if (ret >= 0)
					ret = 0;
			}
		}

		if (promoter != NULL)
			delete promoter;
	} else {
		ret = GAME_ERROR_NO_INFO_MEMORY;
	}

	gameShellRelease();
	gameWorkEnd();
	gameShellUnlock();
	gameLeave();

	return ret;
}

/*
 * An install with the shell locked: the title ID an error names left in param (0x8102BFB2).
 */
int gamePluginInstall(GameInstallParam *param)
{
	int ret;

	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	if (param == NULL)
		return GAME_ERROR_INVALID_ARGUMENT;

	ret = gameEnter();
	if (ret >= 0) {
		gameShellLock();
		gameWorkBegin();
		gameShellHold(true, false);

		GameInstallJob *job = new GameInstallJob(param, true, gameIsStopRequested, 0);

		if (job == NULL) {
			ret = GAME_ERROR_NO_INFO_MEMORY;
		} else {
			ret = job->m_result;
			if (ret >= 0) {
				ret = job->Run();
				if (ret >= 0)
					ret = 0;
			}
		}

		param->errorTitleId[0] = '\0';
		if (ret < 0) {
			/* Sony's reads the job also when it was not made */
			sce_paf_strncpy(param->errorTitleId, job->m_errorTitleId.c_str(),
					job->m_errorTitleId.length());
			param->errorTitleId[sizeof(param->errorTitleId) - 1] = '\0';
		}

		if (job != NULL)
			delete job;

		gameShellRelease();
		gameWorkEnd();
		gameShellUnlock();
		gameLeave();
	}

	return ret;
}

/*
 * An install of the second kind with the shell locked: the title ID an error names left in
 * param (0x8102C228).
 */
int gamePluginInstallEx(GameInstallExParam *param)
{
	int ret;

	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	if (param == NULL)
		return GAME_ERROR_INVALID_ARGUMENT;

	ret = gameEnter();
	if (ret < 0)
		return ret;

	gameShellLock();
	gameWorkBegin();
	gameShellHold(true, false);

	GameAppRegistration reg;
	GameInstallExJob *job;

	if (reg.entry.get() != NULL && reg.info.get() != NULL) {
		job = new GameInstallExJob(param, true, 0, 0);

		if (job == NULL) {
			ret = GAME_ERROR_NO_INFO_MEMORY;
		} else {
			ret = job->m_result;
			if (ret >= 0) {
				ret = job->Run();
				if (ret >= 0)
					ret = 0;
			}
		}
	} else {
		ret = GAME_ERROR_NO_INFO_MEMORY;
		job = NULL;
	}

	param->errorTitleId[0] = '\0';
	if (ret < 0) {
		sce_paf_strncpy(param->errorTitleId, job->m_errorTitleId.c_str(),
				job->m_errorTitleId.length());
		param->errorTitleId[sizeof(param->errorTitleId) - 1] = '\0';
	}

	if (job != NULL)
		delete job;

	gameShellRelease();
	gameWorkEnd();
	gameShellUnlock();
	gameLeave();

	return ret;
}

/*
 * A stop of the plugin requested: callback is called with arg once the work under way ends, at
 * once when none is (0x8102C644). False before start and when a stop is requested already.
 */
bool gamePluginRequestStop(void (*callback)(void *arg), void *arg)
{
	if (!s_started)
		return false;

	if (s_stopCallback != NULL)
		return false;

	int ret = s_mutex.TryLock();

	s_stopCallback = callback;
	s_stopArg = arg;

	if (ret == 0) {
		s_mutex.Unlock();
		if (s_stopCallback != NULL)
			s_stopCallback(s_stopArg);
	}

	return true;
}

int gameGcLoadParamSfo(LsdbAppInfoPtr *info)
{
	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	GameSfoReader *reader = new GameSfoReader(info);

	if (reader == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	int ret = reader->Load(string(GAME_GC_PARAM_SFO));

	delete reader;

	if (ret < 0)
		return ret;

	return 0;
}

/*
 * An application promoted from param with the shell locked, stopped when cancel says so: the
 * title ID an error names left in param (0x8102C776).
 */
int gamePluginPromoteParam(GamePromoteParam *param, bool (*cancel)(void *arg), void *arg)
{
	int ret;

	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	ret = gameEnter();
	if (ret < 0)
		return ret;

	gameShellLock();
	gameWorkBegin();
	gameShellHold(true, false);

	GameAppRegistration reg;
	GameAppPromoter *promoter;

	if (reg.info.get() != NULL && reg.entry.get() != NULL) {
		promoter = new GameAppPromoter(param, true, &reg, cancel, arg);

		if (promoter == NULL) {
			ret = GAME_ERROR_NO_INFO_MEMORY;
		} else {
			ret = promoter->m_result;
			if (ret >= 0) {
				ret = promoter->Run();
				if (ret >= 0)
					ret = 0;
			}
		}
	} else {
		ret = GAME_ERROR_NO_INFO_MEMORY;
		promoter = NULL;
	}

	param->errorTitleId[0] = '\0';
	if (ret < 0) {
		sce_paf_strncpy(param->errorTitleId, promoter->m_errorTitleId.c_str(),
				promoter->m_errorTitleId.length());
		param->errorTitleId[sizeof(param->errorTitleId) - 1] = '\0';
	}

	if (promoter != NULL)
		delete promoter;

	gameShellRelease();
	gameWorkEnd();
	gameShellUnlock();
	gameLeave();

	return ret;
}

/* An application's param.sfo at path loaded into info (0x8102CBA2). */
int gamePluginSfoLoad(const char *path, LsdbAppInfoPtr *info)
{
	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	return gameSfoLoad(string(path), gameSfoGetAppParams(), gameSfoGetAppParamCount(), info);
}

/*
 * A SELF's eboot parameters at path: the record's header word left in header and the
 * parameters from its attribute on (28 bytes) in params, each when given (0x8102CC2C).
 */
int gamePluginReadEbootParam(const char *path, SceUInt32 *header, void *params)
{
	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	GameEbootParam *param = new GameEbootParam;

	if (param == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	int ret = param->Read(string(path));

	if (ret < 0) {
		delete param;
		return ret;
	}

	if (header != NULL)
		*header = param->data.header;
	if (params != NULL)
		sce_paf_memcpy(params, &param->data.attribute, 0x1C);

	delete param;

	return 0;
}

/* An application deleted with the shell locked (0x8102CD0A). */
int gamePluginAppDelete(const char *titleId)
{
	int ret;

	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	ret = gameEnter();
	if (ret >= 0) {
		gameShellLock();
		gameWorkBegin();
		gameShellHold(true, false);

		ret = gameAppDelete(titleId);
		if (ret >= 0)
			ret = 0;

		gameShellRelease();
		gameWorkEnd();
		gameShellUnlock();
		gameLeave();
	}

	return ret;
}

/*
 * A part of an application deleted with the shell locked, the whole of it for kind 0; kind
 * GAME_APP_PART_REPROMOTE promotes the application again from its original path (0x8102CF30).
 */
int gamePluginAppDeletePart(const char *titleId, int kind)
{
	int ret;

	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	if (kind == 0)
		return gamePluginAppDelete(titleId);

	ret = gameEnter();
	if (ret < 0)
		return ret;

	gameShellLock();
	gameWorkBegin();
	gameShellHold(true, false);

	ret = gameAppDeletePart(titleId, kind);
	if (ret >= 0)
		ret = 0;

	gameShellRelease();
	gameWorkEnd();
	gameShellUnlock();
	gameLeave();

	if (kind != GAME_APP_PART_REPROMOTE)
		return ret;

	LsdbAppInfoPtr info(new LsdbAppInfo);

	if (info.get() == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	int err = gameAppGetInfo(titleId, &info);

	if (err < 0)
		return err;

	LsdbAppInfo::iterator it = info->find(LSDB_KEY_ORG_PATH);

	if (it != info->end()) {
		s_repromoting = true;
		err = gamePluginPromoteLocked(LsdbValueText(it->second));
		s_repromoting = false;
		if (err < 0)
			return err;
	}

	return ret;
}

/* An application's whole size, cancel polled while it is counted (0x8102D372). */
int gamePluginTitleGetSize(const char *titleId, GameCancelFunc cancel, void *arg, SceUInt64 *size)
{
	int ret;

	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	if (size == NULL)
		return GAME_ERROR_INVALID_ARGUMENT;

	*size = 0;

	ret = gameEnter();
	if (ret >= 0) {
		GameAppSizes sizes;

		sce_paf_memset(&sizes, 0, sizeof(sizes));
		ret = gameTitleGetSizes(titleId, GAME_SIZES_ALL, cancel, arg, &sizes);
		if (ret >= 0) {
			ret = 0;
			*size = sizes.total;
		}

		gameLeave();
	}

	return ret;
}

/*
 * PLACEHOLDER name: what 0x8103DCF4 does with arg, with the shell locked and the system kept
 * awake (0x8102D456).
 */
int gamePluginShellLockedJob(void *arg)
{
	int ret;

	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	ret = gameEnter();
	if (ret >= 0) {
		gameShellLock();
		gameWorkBegin();

		ret = gameLockedJobRun(arg, 0);
		if (ret >= 0)
			ret = 0;

		gameWorkEnd();
		gameShellUnlock();
		gameLeave();
	}

	return ret;
}

/* A license's status (0x8102D654). */
int gamePluginLicenseGetStatus(GameLicenseStatus *status)
{
	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	int ret = gameLicenseGetStatus(status);

	if (ret < 0)
		return ret;

	return 0;
}

/*
 * An application's license checked: its status into status when given, and for an EG or ME
 * application whether its EBOOT.PBP opens (0x8102D67C).
 */
int gamePluginLicenseCheck(const LsdbAppInfoPtr *info, GameLicenseStatus *status)
{
	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_ORG_PATH);

	if (it == (*info)->end())
		return GAME_ERROR_NO_VALUE;

	string path(LsdbValueText(it->second));

	it = (*info)->find(LSDB_KEY_CATEGORY);
	if (it == (*info)->end())
		return GAME_ERROR_NO_VALUE;

	string category(LsdbValueText(it->second));
	int kind;
	int ret = gameAppInfoGetCategoryKind(info, &kind);

	if (ret < 0)
		return ret;

	if (path.c_str() == NULL || path.c_str()[0] == '\0')
		return GAME_ERROR_INVALID_ARGUMENT;

	GameLicenseStatus license;

	sce_paf_memset(&license, 0, sizeof(license));
	sce_paf_strncpy(license.path, path.c_str(), sizeof(license.path) - 1);
	license.path[sizeof(license.path) - 1] = '\0';

	ret = gameLicenseGetStatus(&license);
	if (ret >= 0) {
		if (status != NULL)
			sce_paf_memcpy(status, &license, sizeof(license));
		ret = license.result;
	}
	if (ret < 0)
		return ret;

	if (gameCategoryIsEgMe(category) && gameEbootCheck(path.c_str(), NULL) < 0)
		return GAME_ERROR_EBOOT;

	return 0;
}

/* The alarm of the application at path set (0x8102D90A). */
int gamePluginAlarmSet(const char *path, const char *arg)
{
	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	if (s_alarm == NULL)
		return GAME_ERROR_NOT_READY;

	int ret = s_alarm->SetForPath(path, arg);

	if (ret < 0)
		return ret;

	return 0;
}

/* An application's alarm updated (0x8102D94A). */
int gamePluginAlarmUpdate(const LsdbAppInfoPtr *info)
{
	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	if (s_alarm == NULL)
		return GAME_ERROR_NOT_READY;

	int ret = s_alarm->Update(info);

	if (ret < 0)
		return ret;

	return 0;
}

/* An application's alarm removed (0x8102D988). */
int gamePluginAlarmRemove(const LsdbAppInfoPtr *info)
{
	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	if (s_alarm == NULL)
		return GAME_ERROR_NOT_READY;

	int ret = s_alarm->Remove(info);

	if (ret < 0)
		return ret;

	return 0;
}

/* An application's alarm removed, its result not wanted (0x8102D9C6). */
void gamePluginAlarmRemoveQuiet(const LsdbAppInfoPtr *info)
{
	if (s_started && s_alarm != NULL)
		s_alarm->Remove(info);
}

/* An application's license updated (0x8102D9E8). */
int gamePluginLicenseUpdate(const char *titleId)
{
	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	if (s_licenseUpdater == NULL)
		return GAME_ERROR_NOT_READY;

	int ret = s_licenseUpdater->Update(titleId);

	if (ret < 0)
		return ret;

	return 0;
}

/* The content import's steps for titleId, once started (0x8102DA28, 0x8102DA46, 0x8102DA64). */
int gamePluginContentImportPrepare(const char *titleId)
{
	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	return gameContentImportPrepare(titleId);
}

int gamePluginContentImportRun(const char *titleId)
{
	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	return gameContentImportRun(titleId);
}

int gamePluginContentImportCheck(const char *titleId)
{
	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	return gameContentImportCheck(titleId);
}

/* An imported content deleted, once started (0x8102DA82). */
int gamePluginContentImportDelete(const char *titleId)
{
	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	return gameContentImportDelete(titleId);
}

/* Whether an application is a game, EG, ME or PSM application by its category (0x8102DAA0). */
__attribute__((noinline)) bool gamePluginAppIsGame(const LsdbAppInfoPtr *info)
{
	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_CATEGORY);

	if (it == (*info)->end())
		return false;

	const char *category = LsdbValueText(it->second);

	/* gameCategoryIsGame, IsEg, IsMe and IsPsm, inlined */
	for (SceUInt32 i = 0; i < 6; i++)
		if (sce_paf_strncmp(category, s_gameCategories[i], 4) == 0)
			return true;
	for (int i = 0; i < 1; i++)
		if (sce_paf_strncmp(category, s_egCategories[i], 4) == 0)
			return true;
	for (int i = 0; i < 1; i++)
		if (sce_paf_strncmp(category, s_meCategories[i], 4) == 0)
			return true;
	for (SceUInt32 i = 0; i < 2; i++)
		if (sce_paf_strncmp(category, s_psmCategories[i], 4) == 0)
			return true;

	return false;
}

/*
 * Whether an application's additional contents have their information, found set by the
 * check: for a PSP game its own title ID, else the folders INSTALL_DIR_ADDCONT and
 * INSTALL_DIR_ADDCONT_ADD_1..7 name, looked up in addcont/info of its storage (0x8102DB94).
 */
int gamePluginAddcontInfoCheck(const char *titleId, bool *found)
{
	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	*found = false;

	LsdbAppInfoPtr info(new LsdbAppInfo);
	int ret = gameAppGetInfo(titleId, &info);

	if (ret < 0)
		return ret;

	if (!gamePluginAppIsGame(&info))
		return 0;

	LsdbAppInfo::iterator it = info->find(LSDB_KEY_ORG_PATH);

	if (it == info->end())
		return GAME_ERROR_NO_VALUE;

	string orgPath(LsdbValueText(it->second));
	bool pspemu = sce_paf_strncmp(GAME_PSPEMU_GAME_DIR, orgPath.c_str(),
				      sizeof(GAME_PSPEMU_GAME_DIR) - 1) == 0;
	vector<string> dirs;

	if (pspemu) {
		dirs.push_back(string(titleId));
	} else {
		unsigned int keys[] = {
			LSDB_KEY_INSTALL_DIR_ADDCONT,
			LSDB_KEY_INSTALL_DIR_ADDCONT_ADD_1, LSDB_KEY_INSTALL_DIR_ADDCONT_ADD_2,
			LSDB_KEY_INSTALL_DIR_ADDCONT_ADD_3, LSDB_KEY_INSTALL_DIR_ADDCONT_ADD_4,
			LSDB_KEY_INSTALL_DIR_ADDCONT_ADD_5, LSDB_KEY_INSTALL_DIR_ADDCONT_ADD_6,
			LSDB_KEY_INSTALL_DIR_ADDCONT_ADD_7,
		};

		for (int i = 0; i < 8; i++) {
			string dir(LsdbValueText((*info)[keys[i]]));

			if (dir.length() != 0)
				dirs.push_back(dir);
		}
	}

	if (dirs.size() == 0) {
		*found = false;
		return 0;
	}

	int storage = 0;

	ret = gameAppGetStorage(&info, &storage);
	if (ret < 0)
		return ret;

	string device;

	switch (storage) {
	case 1:
		device = "grw0:";
		break;
	case 2:
	case 3:
		device = "ux0:";
		break;
	default:
		return GAME_ERROR_BAD_STORAGE;
	}

	string path = device + string("addcont/info");

	if (pspemu)
		ret = gamePspAddcontInfoCheck(path.c_str(), &dirs, found);
	else
		ret = gameAddcontInfoCheck(path.c_str(), &dirs, found);

	if (ret < 0)
		return ret;

	return 0;
}

/* PLACEHOLDER name: what the license updater does with arg (0x8102E442) */
int gamePluginLicenseUpdaterRun(void *arg)
{
	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	if (s_licenseUpdater == NULL)
		return GAME_ERROR_NOT_READY;

	int ret = s_licenseUpdater->Run(arg);

	if (ret < 0)
		return ret;

	return 0;
}

/*
 * Whether an application may run on this model, which is given in model: a game by the bits
 * of its ATTRIBUTE_MINOR for the model; on a PS TV without them, outside PM mode, by 0x810A4A24
 * (0x8102E480).
 */
int gamePluginAppIsAllowed(const string &titleId, bool *allowed, int *model)
{
	*allowed = false;

	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	*model = system::Model::GetType();

	LsdbAppInfoPtr info(new LsdbAppInfo);
	int ret = gameAppGetInfo(titleId.c_str(), &info);

	if (ret < 0)
		return ret;

	if (!gamePluginAppIsGame(&info)) {
		*allowed = true;
		return 0;
	}

	bool valid = false;
	bool bit3Clear = false;
	bool bit2Clear = false;

	ret = gameAppInfoGetAttributeMinor(&info, &valid, &bit2Clear, &bit3Clear);
	if (ret < 0)
		return ret;

	bool dolce = *model == system::Model::Type_Dolce;

	if (valid) {
		if (dolce)
			*allowed = bit3Clear;
		else
			*allowed = bit2Clear;
	} else if (!dolce) {
		*allowed = true;
	} else {
		SceUInt8 mode = 0;

		if (sceSblPmMgrGetCurrentMode(&mode) == 0 && mode == 1) {
			*allowed = true;
		} else {
			ret = gameDolceAppCheck(titleId, allowed);
			if (ret < 0)
				return ret;
		}
	}

	return 0;
}

/*
 * Whether a PSP or PS1 game may run on this model, which is given in model: on a PS TV only
 * when ur0:game/launch/list_launch_emu.dat lists it (0x8102E78C).
 */
int gamePluginEmuAppIsAllowed(const string &titleId, bool *allowed, int *model)
{
	*allowed = false;

	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	*model = system::Model::GetType();
	if (*model != system::Model::Type_Dolce) {
		*allowed = true;
		return 0;
	}

	int ret = gameLaunchEmuListCheck(titleId, allowed);

	if (ret < 0)
		return ret;

	return 0;
}

/* The PocketStation's license checked (0x8102E7CE). */
int gamePluginPocketStationLicenseCheck(void)
{
	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	string path = string(GAME_PSP_LICENSE_DIR) + string(GAME_POCKETSTATION_RIF);

	return gamePspRifCheck(path);
}

/*
 * Whether a PlayStation Mobile application may run on this model, which is given in model: by
 * the bits of its ATTRIBUTE_MINOR for the model; on a PS TV without them, outside PM mode and
 * with the launch check on, only the applications s_dolcePsmApps lists (0x8102E928).
 */
int gamePluginPsmAppIsAllowed(const string &titleId, bool *allowed, int *model)
{
	*allowed = false;

	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	*model = system::Model::GetType();

	LsdbAppInfoPtr info(new LsdbAppInfo);
	int ret = gameAppGetInfo(titleId.c_str(), &info);

	if (ret < 0)
		return ret;

	if (!gamePluginAppIsGame(&info)) {
		*allowed = true;
		return 0;
	}

	bool valid = false;
	bool bit3Clear = false;
	bool bit2Clear = false;

	ret = gameAppInfoGetAttributeMinor(&info, &valid, &bit2Clear, &bit3Clear);
	if (ret < 0)
		return ret;

	bool dolce = *model == system::Model::Type_Dolce;

	if (valid) {
		if (dolce)
			*allowed = bit3Clear;
		else
			*allowed = bit2Clear;
	} else if (dolce) {
		SceUInt8 mode = 0;
		bool pmMode = sceSblPmMgrGetCurrentMode(&mode) == 0 && mode == 1;
		bool launchCheck = gameLaunchCheckIsOn();

		if (!pmMode && launchCheck) {
			string end(GAME_DOLCE_PSM_APPS_END);

			for (SceUInt32 i = 0; i < GAME_DOLCE_PSM_APPS; i++) {
				string app(s_dolcePsmApps[i]);

				if (app == end)
					break;
				if (app == titleId) {
					*allowed = true;
					break;
				}
			}
		} else {
			*allowed = true;
		}
	} else {
		*allowed = true;
	}

	return 0;
}

/* An application set up in ur0, one at a time (0x8102ECC4). */
int gamePluginSetupUr0(const LsdbAppInfoPtr *info, int arg)
{
	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	s_setupUr0Mutex.Lock();

	int ret;
	GameUr0Setup *setup = new GameUr0Setup(info, arg);

	if (setup != NULL)
		ret = setup->Run();
	else
		ret = GAME_ERROR_NOT_READY;

	if (setup != NULL)
		delete setup;

	s_setupUr0Mutex.Unlock();

	return ret;
}

/* An application's appmeta set up in ur0, under the same mutex (0x8102ED34). */
int gamePluginSetupUr0Appmeta(const string *titleId, const char *path)
{
	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	s_setupUr0Mutex.Lock();

	int ret = gameUr0AppmetaSetup(titleId, path);

	s_setupUr0Mutex.Unlock();

	return ret;
}

/* PLACEHOLDER name: what ux0:umass's check gives, once started (0x8102ED72). */
int gamePluginUmassCheck(int arg, bool *found)
{
	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	return gameUmassCheck(arg, found);
}

/* What an application's folders take, the folders dirs names (0x8102ED90). */
int gamePluginAppGetDirsSize(const char *titleId, int dirs, GameCancelFunc cancel, void *arg,
			     SceUInt64 *size)
{
	int ret;

	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	ret = gameEnter();
	if (ret >= 0) {
		ret = gameAppGetDirsSize(titleId, dirs, cancel, arg, size);
		gameLeave();
	}

	return ret;
}

/* What the data of a title the shell keeps as its own takes (0x8102EE36). */
int gamePluginPresetGetDataSize(const char *titleId, GameCancelFunc cancel, void *arg,
				GamePresetData *data)
{
	int ret;

	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	ret = gameEnter();
	if (ret >= 0) {
		ret = gamePresetGetDataSize(titleId, cancel, arg, data);
		gameLeave();
	}

	return ret;
}

/* Whether titleId is one the shell keeps as its own (0x8102EED0). */
int gamePluginPresetCheckTitle(const char *titleId, GamePresetTitle *preset)
{
	int ret;

	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	ret = gameEnter();
	if (ret >= 0) {
		ret = gamePresetCheckTitle(titleId, preset);
		gameLeave();
	}

	return ret;
}

/* PLACEHOLDER name: a GameParamJob run with the shell locked and held (0x8102EF54). */
int gamePluginParamJobRun(const GameJobParam *param)
{
	int ret;

	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	if (param == NULL)
		return GAME_ERROR_INVALID_ARGUMENT;

	ret = gameEnter();
	if (ret >= 0) {
		gameShellLock();
		gameWorkBegin();
		gameShellHold(true, false);

		GameParamJob *job = new GameParamJob(param, NULL, NULL);

		if (job != NULL) {
			ret = job->m_result;
			if (ret >= 0)
				ret = job->Run();
		} else {
			ret = GAME_ERROR_NO_INFO_MEMORY;
		}

		if (job != NULL)
			delete job;

		gameShellRelease();
		gameWorkEnd();
		gameShellUnlock();
		gameLeave();
	}

	return ret;
}

/*
 * An application's free-trial license, ux0:license/app/<title ID>/<its .ftt name>, read into
 * info when it is the application's content (0x8102F198).
 */
int gamePluginFttRead(const char *titleId, GameFttInfo *info)
{
	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	if (titleId == NULL || info == NULL)
		return GAME_ERROR_INVALID_ARGUMENT;

	int ret = 0;
	LsdbAppInfoPtr app(new LsdbAppInfo);
	string contentId;
	string fttContentId;
	string fttName;
	string path;
	common::SharedPtr<File> file;

	ret = gameAppGetInfo(titleId, &app);
	if (ret >= 0) {
		LsdbAppInfo::iterator it = app->find(LSDB_KEY_CONTENT_ID);

		if (it == app->end()) {
			ret = GAME_ERROR_NO_VALUE;
		} else {
			contentId = LsdbValueText(it->second);
			ret = gameNpDrmGetFttName(&fttName);
			if (ret >= 0) {
				path = string(GAME_LICENSE_APP_DIR) + string(titleId);
				path += "/";
				path.append(fttName);

				if (LocalFile::Exists(path.c_str())) {
					file = LocalFile::Open(path.c_str(), SCE_O_RDONLY, 0, &ret);
					if (ret >= 0) {
						GameFttLicense license;

						sce_paf_memset(&license, 0, sizeof(license));
						ret = gameFttLicenseRead(file, &license);
						if (ret >= 0) {
							fttContentId = license.contentId;
							if (contentId != fttContentId) {
								ret = GAME_ERROR_FTT_MISMATCH;
							} else {
								sce_paf_memcpy(info->contentId, license.contentId,
									       sizeof(info->contentId));
								info->time1 = license.time1;
								info->time2 = license.time2;
								info->otherField = license.otherField;
								info->flag = (license.flags & 1) != 0;
							}
						}
					}
					if (file.get() != NULL && file->IsOpened())
						file->Close();
				} else {
					ret = GAME_ERROR_NO_FTT;
				}
			}
		}
	}

	return ret;
}

/*
 * An application's license updated from the package the license updater finds, installed with
 * the shell locked and held; the package's folder is removed after (0x8102F79C).
 */
int gamePluginLicenseUpdateInstall(const string &titleId)
{
	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	if (s_licenseUpdater == NULL)
		return GAME_ERROR_NOT_READY;

	if (titleId.length() == 0)
		return GAME_ERROR_INVALID_ARGUMENT;

	LsdbAppInfoPtr info(new LsdbAppInfo);
	string contentId;
	string dir;
	string path;
	int ret = gameEnter();

	if (ret < 0)
		return ret;

	gameShellLock();
	gameWorkBegin();
	gameShellHold(true, false);

	ret = gameAppGetInfo(titleId.c_str(), &info);
	if (ret >= 0) {
		LsdbAppInfo::iterator it = info->find(LSDB_KEY_CONTENT_ID);

		if (it == info->end()) {
			ret = GAME_ERROR_NO_VALUE;
		} else {
			contentId = LsdbValueText(it->second);
			ret = s_licenseUpdater->Prepare(titleId, &contentId, &dir, &path);
			if (ret >= 0) {
				GameInstallParam param;

				sce_paf_memset(&param, 0, sizeof(param));
				sce_paf_strncpy(param.path, path.c_str(), path.length());
				sce_paf_strncpy(param.titleId, titleId.c_str(), titleId.length());
				param.flag2 = false;
				param.kind = 2;
				param.flag1 = true;

				GameInstallJob *job = new GameInstallJob(&param, true, NULL, 0);

				if (job != NULL) {
					ret = job->m_result;
					if (ret >= 0)
						ret = job->Run();
				} else {
					ret = GAME_ERROR_NO_INFO_MEMORY;
				}

				if (job != NULL)
					delete job;
			}
		}
	}

	gameRemoveDirectory(dir.c_str(), 0, 0, 0, true);

	gameShellRelease();
	gameWorkEnd();
	gameShellUnlock();
	gameLeave();

	return ret;
}

/* An installed application's sizes on ux0, for what flags asks (0x8102FCC8). */
int gamePluginAppGetInstalledSizes(const char *titleId, unsigned int flags, GameCancelFunc cancel,
				   void *arg, GameAppSizes *sizes)
{
	int ret;

	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	if (sizes == NULL)
		return GAME_ERROR_INVALID_ARGUMENT;

	ret = gameEnter();
	if (ret >= 0) {
		ret = gameAppGetInstalledSizes(string(titleId), flags, cancel, arg, sizes);
		gameLeave();
	}

	return ret;
}

/* An application's sizes, for what flags asks (0x8102FDCA). */
int gamePluginTitleGetSizes(const char *titleId, unsigned int flags, GameCancelFunc cancel,
			    void *arg, GameAppSizes *sizes)
{
	int ret;

	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	if (sizes == NULL)
		return GAME_ERROR_INVALID_ARGUMENT;

	ret = gameEnter();
	if (ret >= 0) {
		ret = gameTitleGetSizes(titleId, flags, cancel, arg, sizes);
		gameLeave();
	}

	return ret;
}

/*
 * The ATTRIBUTE_MINOR flags of the application in dir, from its param.sfo checked against the
 * folder's digest (0x8102FE80).
 */
int gamePluginDirGetAttributeMinor(const string &dir, bool *valid, bool *bit2Clear,
				   bool *bit3Clear)
{
	*valid = false;
	*bit2Clear = false;
	*bit3Clear = false;

	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	SceNpDrmInstallInfo stat;

	sce_paf_memset(&stat, 0, sizeof(stat));

	int ret = gameDirGetStat(dir, &stat);

	if (ret < 0)
		return ret;

	LsdbAppInfoPtr info(new LsdbAppInfo);

	if (info.get() == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	ret = gameSfoLoadVerified(dir + string(GAME_SFO_PATH), gameSfoGetAttributeParams(),
				  gameSfoGetAttributeParamCount(), &info, stat.param_digest,
				  stat.param_digest_size);
	if (ret < 0)
		return ret;

	return gameAppInfoGetAttributeMinor(&info, valid, bit2Clear, bit3Clear);
}

/*
 * An operation on an application's data, kinds 1 to 3, with the shell locked and held
 * (0x81030136). PLACEHOLDER: what the operation and its words are is not read yet.
 */
int gamePluginAppDataOperate(const char *titleId, int kind, int arg2, int arg3, int arg4)
{
	int ret;

	if (!s_started)
		return GAME_ERROR_NOT_STARTED;

	ret = gameEnter();
	if (ret >= 0) {
		gameShellLock();
		gameWorkBegin();
		gameShellHold(true, false);

		switch (kind) {
		case 1:
		case 2:
		case 3:
			ret = gameAppDataOperate(titleId, kind, arg2, arg3, arg4);
			break;
		default:
			ret = GAME_ERROR_INVALID_ARGUMENT;
			break;
		}

		gameShellRelease();
		gameWorkEnd();
		gameShellUnlock();
		gameLeave();
	}

	return ret;
}

/* (0x8103037C) */
GamePlugin::~GamePlugin()
{
	if (s_licenseUpdater != NULL)
		delete s_licenseUpdater;
	if (s_alarm != NULL)
		delete s_alarm;
	s_pool.reset();
	gameGcPromoteDestroy();
	if (s_cond != NULL)
		delete s_cond;
	if (s_powerTick != NULL)
		delete s_powerTick;
	m_plugin = NULL;
}

/* The shell's answer to the plugin's lock: the install waiting for it goes on (0x81030474). */
static void gameOnShellEvent(int result, int event, int param, void *arg)
{
	if (event == 1 && param == GAME_SHELL_LOCK_TYPE) {
		s_condMutex.Lock();
		s_cond->Notify();
		s_condMutex.Unlock();
	}
}
