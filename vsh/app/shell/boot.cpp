/*
 * boot.cpp: SceShell's boot sequence. The shell loads its resident plugins (applauncher,
 * download, livearea, mail_resident) when the cold-boot plugins are done, counts their loads
 * down, and keeps the stack of the applications started before the home screen (the initial
 * setup, a database recovery). Until the boot is done, launches by name are refused except the
 * network settings, which go through the settings application.
 *
 * The 3.6x shell names none of its source files: this file's name and the names in it are ours.
 */
#include <scetypes.h>
#include <sceerror.h>
#include <std/string.h>
#include <std/stdlib.h>
#include <plugin.h>
#include <common/shared_ptr.h>
#include <graphics/graphmem/graphmem.h>
#include <graphics/surface/surface_pool.h>
#include <graphics/effects/wave_switch.h>
#include <graphics/effects/gate_blur.h>
#include <graphics/effects/screen_copy.h>
#include <job/job.h>
#include <common/main_thread_call_list.h>
#include <system/system.h>
#include <rtc.h>
#include <regmgr/regmgr.h>
#include <kernel/processmgr.h>
#include <appmgr/appmgr.h>
#include <av_config/av_config.h>
#include <std/vector>
#include <inputdevice/touchscreen/touchscreen.h>
#include <vshbridge/vshbridge.h>

#include "boot.h"
#include "plugins.h"
#include "shell.h"
#include "theme.h"
#include "src/livespace_db/livespace_db.h"
#include "src/livespace_db/lsdb_keys.h"
#include <graphics/drawobj/effect_path.h>
#include "../game/game_plugin/white_list.h"

using namespace paf;

extern int shellConsoleInfoInit(void);
extern int consoleInfoFileWriteInt(int index, SceUInt32 value, int mode);
extern "C" int vshRtcSetCurrentTick(const void *tick);
extern "C" int _vshAppMgrCloudDataGetMcId(int type, void *mcId);
extern "C" int _vshAppMgrCloudDataSetMcId(int type, const void *mcId);

/* 0x81417DB4 */
int shellCloudDataGetMcId(int type, void *mcId)
{
	return _vshAppMgrCloudDataGetMcId(type, mcId);
}

/* 0x81417DBC */
int shellCloudDataSetMcId(int type, const void *mcId)
{
	return _vshAppMgrCloudDataSetMcId(type, mcId);
}

/* what s_bootState has seen happen */
enum {
	BOOT_STATE_COLDBOOT_DONE		= 0x01,
	BOOT_STATE_SYSTEM_PLUGINS_LOADED	= 0x02,
	BOOT_STATE_BOOT_APPS_DONE		= 0x04,	/* the application stack emptied */
	BOOT_STATE_DATABASE_READY		= 0x08,	/* checked, or recovered */
	BOOT_STATE_RESIDENT_PLUGINS_STARTED	= 0x10,
	BOOT_STATE_RESIDENT_PLUGINS_LOADED	= 0x20,
};

static signed char s_bootState;		/* BOOT_STATE_* */
static bool s_bootLaunchRestricted = true;	/* launches by name refused */
static int s_pluginsLoading;			/* plugin loads not finished */
static int s_databaseState = -1;		/* what the database check or recovery reported */
static unsigned int s_clockPollCount;		/* frames the network clock update has taken */

static vector<int> s_bootAppStack;		/* ids of the applications started before the home screen */

static void shellBootStartResidentPlugins(void);
static void shellBootStartResidentOrIdu(void);
static void shellBootCheckDatabase(void);
static void shellBootStartSystem(void);
static void shellBootProceed(void);
static void shellBootOnIduUpdatePluginUnloaded(void *userdata);
static void shellBootOnDbrecoveryPluginUnloaded(void *userdata);
static void shellBootOnDbrecoveryPluginLoaded(Plugin *plugin);
static void shellBootCheckInitialSetup(bool clockUpdated);
static void shellBootOnClockUpdatePoll(void *userdata);
static void shellBootOnColdbootPluginUnloaded(void *userdata);
static void shellBootOnAppEvent(int code, const void *data, SceSize size, void *arg);
static void shellBootOnSystemPluginLoaded(Plugin *plugin);

bool shellBootIsLaunchRestricted(void)
{
	return s_bootLaunchRestricted;
}

bool shellBootArePluginsLoaded(void)
{
	return (s_bootState & BOOT_STATE_RESIDENT_PLUGINS_LOADED) != 0;
}

bool shellBootHasAppStack(void)
{
	return s_bootAppStack.size() != 0;
}

int shellBootGetDatabaseState(void)
{
	return s_databaseState;
}

/* SceShell's launch by name: during the boot, only the network settings, as the settings */
extern "C" int SceShell_99893E59(const char *name, const char *param,
				 const AppmgrLaunchAppByNameOption *option)
{
	int ret = SCE_ERROR_ERRNO_EINVAL;

	if (!s_bootLaunchRestricted) {
		ret = sceAppMgrLaunchAppByName2(name, param, option);
	} else if (sce_paf_strcmp(name, "NPXS10016") == 0) {
		const char *path = "vs0:app/NPXS10015/eboot.bin";

		if (path != NULL) {
			AppmgrLaunchAppOption launch;

			sce_paf_memset(&launch, 0, sizeof(launch));
			launch.size = sizeof(launch);
			launch.flags = option->launchMode;
			sce_paf_strncpy(launch.proxyAppName, "NPXS10062", sizeof(launch.proxyAppName));
			ret = sceAppMgrLaunchAppByPath4(path, name, 0x2000000, param, 0, &launch);
		}
	}
	return ret;
}


/* the shell's first job: brings up the shell's services once the plugins run */
class ShellBootJob : public job::JobItem {
public:
	ShellBootJob() : job::JobItem(NULL, NULL)
	{
	}

	/* the services the shell starts once its boot is done */
	virtual int Run()
	{
		char mcId[8];

		if (shellCloudDataGetMcId(0, mcId) != 0)
			shellCloudDataSetMcId(0, NULL);
		if (system::Model::GetType() == 1)
			gameWhiteListUpdate();
		shellLiblocationStart();
		if (system::Model::GetType() != 1)
			shellCalibrationUpdateStart();
		shellRefreshCronStart();
		shellLiveAreaDailyCronStart();
		shellRemoteDownloadStart();
		shellConsoleInfoInit();
		shellNpFriendInit();
		shellAppStateMarkForeground();
		shellThemeApplyPending();
		return 0;
	}

	virtual void Finish(int result)
	{
	}
};

/* the plugins' callbacks that do nothing */
static void shellBootPluginSetParamNoop(Plugin::InitParam2 *param)
{
}

static void shellBootPluginNoop(Plugin *plugin)
{
}

static void shellBootOnResidentPluginLoaded(Plugin *plugin);
static void shellBootOnPowerEvent(int code, const void *data, SceSize size, void *arg);
static void shellBootOnModelEvent(int code, const void *data, SceSize size, void *arg);

/* the boot is done: the shell's own job, its event listeners and its resident plugins */
static void shellBootStartResidentPlugins(void)
{
	sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DEFAULT);
	ScePafGraphics_3EB90427();

	Plugin *topmenu = Plugin::Find("topmenu_plugin");

	if (topmenu != NULL) {
		ShellPluginInterface *iface = (ShellPluginInterface *)topmenu->GetInterface(1);

		if (iface != NULL)
			iface->func[TOPMENU_PLUGIN_FUNC_BOOT_DONE]();
	}
	s_bootLaunchRestricted = false;

	common::SharedPtr<job::JobItem> bootJob(new ShellBootJob());

	job::JobQueue::default_queue->Enqueue(bootJob);
	SceShell_7FE4E732(SHELL_EVENT_POWER, shellBootOnPowerEvent, NULL);
	SceShell_7FE4E732(SHELL_EVENT_MODEL, shellBootOnModelEvent, NULL);

	{
		Plugin::InitParam param;

		param.name = "applauncher_plugin";
		param.resource_file = "vs0:vsh/shell/applauncher_plugin.rco";
		param.caller_name = "__main__";
		param.set_param_func = applauncherPluginSetParam;
		param.init_func = applauncherPluginInit;
		param.start_func = applauncherPluginStart;
		param.stop_func = applauncherPluginStop;
		param.exit_func = applauncherPluginExit;

		char poolName[] = "SceShellApplauncherSurfacePool";
		void *pool = GraphMem::AllocOSMemory(GraphMem::DeviceType_VideoMemory, 0xc0000, poolName, NULL);

		param.surface_pool = new graph::SurfacePool(pool, 0xc0000, poolName);
		param.arguments[string("theme_apply")] = Plugin::Argument(shellThemeIsPending());
		param.graphics_flag = 7;
		Plugin::LoadAsync(param, shellBootOnResidentPluginLoaded, 0);
		s_pluginsLoading++;
	}
	{
		Plugin::InitParam param;

		param.name = "download_plugin";
		param.resource_file = "vs0:vsh/shell/download_plugin.rco";
		param.caller_name = "__main__";
		param.set_param_func = shellBootPluginSetParamNoop;
		param.init_func = downloadPluginInit;
		param.start_func = downloadPluginStart;
		param.stop_func = shellBootPluginNoop;
		param.exit_func = downloadPluginExit;
		Plugin::LoadAsync(param, shellBootOnResidentPluginLoaded, 1);
		s_pluginsLoading++;
	}
	{
		Plugin::InitParam param;

		param.name = "livearea_plugin";
		param.resource_file = "vs0:vsh/shell/livearea_plugin.rco";
		param.caller_name = "__main__";
		param.set_param_func = liveareaPluginSetParam;
		param.init_func = liveareaPluginInit;
		param.start_func = liveareaPluginStart;
		param.stop_func = liveareaPluginStop;
		param.exit_func = liveareaPluginExit;

		char poolName[] = "SceShellLiveareaSurfacePool";
		void *pool = GraphMem::AllocOSMemory(GraphMem::DeviceType_VideoMemory, 0x100000, poolName, NULL);

		param.surface_pool = new graph::SurfacePool(pool, 0x100000, poolName);
		Plugin::LoadAsync(param, shellBootOnResidentPluginLoaded, 1);
		s_pluginsLoading++;
	}
	{
		Plugin::InitParam param;

		param.name = "mail_resident_plugin";
		param.caller_name = "__main__";
		param.set_param_func = shellBootPluginSetParamNoop;
		param.init_func = mailResidentPluginInit;
		param.start_func = mailResidentPluginStart;
		param.stop_func = mailResidentPluginStop;
		param.exit_func = shellBootPluginNoop;
		param.private_heap_size = 0x2000;
		Plugin::LoadAsync(param, shellBootOnResidentPluginLoaded, 1);
		s_pluginsLoading++;
	}
}

/* in idu (demo unit) mode, idu_update_plugin first; the resident plugins once it unloads */
static void shellBootStartResidentOrIdu(void)
{
	sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DEFAULT);
	if (shellStartupGetIduMode() == 1) {
		Plugin::InitParam param;

		param.name = "idu_update_plugin";
		param.resource_file = "vs0:vsh/shell/idu_update_plugin.rco";
		param.caller_name = "__main__";
		param.draw_priority = 24;
		param.module_file = "vs0:vsh/shell/idu_update_plugin.suprx";
		param.module_interface_version = 1;
		param.module_option = 1;
		param.unload_callback = shellBootOnIduUpdatePluginUnloaded;
		Plugin::LoadAsync(param, NULL, 0);
	} else {
		s_bootState |= BOOT_STATE_RESIDENT_PLUGINS_STARTED;
		shellBootStartResidentPlugins();
	}
}

/* the database: recovered by dbrecovery_plugin when the shell found it damaged, else checked */
static void shellBootCheckDatabase(void)
{
	sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DEFAULT);
	shellIconCacheInit();

	int recovery = shellDatabaseNeedsRecovery();

	sce_paf_printf_dummy();
	if (recovery != 0) {
		if (SHELL_LSDB_TRIGGER_TABLE_STALE(SceLsdb_B0E42B67()))
			shellTriggerTableReset();

		Plugin::InitParam param;

		param.name = "dbrecovery_plugin";
		param.resource_file = "vs0:vsh/common/dbrecovery_plugin.rco";
		param.caller_name = "__main__";
		param.draw_priority = 24;
		param.module_file = "vs0:vsh/common/dbrecovery_plugin.suprx";
		param.module_interface_version = 1;
		param.module_option = 1;
		param.arguments[string("state")] = Plugin::Argument(1);
		param.arguments[string("pagec")] = Plugin::Argument(shellStartupGetPageColor());
		param.arguments[string("icwvp")] = Plugin::Argument(shellStartupGetIcwvp());
		param.arguments[string("spool")] = Plugin::Argument((int32_t)shellIconCacheGetSurfacePool());
		param.unload_callback = shellBootOnDbrecoveryPluginUnloaded;
		Plugin::LoadAsync(param, shellBootOnDbrecoveryPluginLoaded, 0);
	} else {
		s_databaseState = shellDatabaseGetState();
		s_bootState |= BOOT_STATE_DATABASE_READY;
		if ((s_bootState & BOOT_STATE_COLDBOOT_DONE) && (s_bootState & BOOT_STATE_BOOT_APPS_DONE)) {
			shellBootScreenCreate();
			shellBootStartResidentOrIdu();
		}
	}
}

void shellBootCheckDatabaseEntry(void)
{
	shellBootCheckDatabase();
}

/* the cold-boot screens (the boot logo, the initial setup's first screens) */
void shellBootLoadColdbootPlugin(void)
{
	Plugin::InitParam param;

	param.name = "coldboot_plugin";
	param.resource_file = "vs0:vsh/shell/coldboot_plugin.rco";
	param.caller_name = "__main__";
	param.set_param_func = coldbootPluginSetParam;
	param.init_func = coldbootPluginInit;
	param.start_func = coldbootPluginStart;
	param.stop_func = coldbootPluginStop;
	param.exit_func = coldbootPluginExit;
	param.unload_callback = shellBootOnColdbootPluginUnloaded;
	Plugin::LoadSync(param, NULL, 0);
}

/* the job that starts the initial setup application */
class ShellInitialSetupJob : public job::JobItem {
public:
	ShellInitialSetupJob(bool clockInvalid) : job::JobItem(NULL, NULL)
	{
		m_clockInvalid = clockInvalid;
	}

	virtual int Run()
	{
		sceAppMgrLaunchAppByPath4("vs0:vsh/initialsetup/initialsetup.self", "NPXS10062", 0, NULL, 0, NULL);
		if (m_clockInvalid)
			consoleInfoFileWriteInt(230, 1, 0);
		return 0;
	}

	virtual void Finish(int result)
	{
	}

	bool m_clockInvalid;			/* the clock was not set when the setup started */
};

/*
 * the system plugins are loaded: a system not set up yet starts the initial setup; else the clock
 * is checked (set to 2011-01-01 in the special modes, from the network when the date is set
 * automatically) before the database
 */
static void shellBootStartInitialSetup(bool clockInvalid)
{
	SceShell_83D16CA8(SHELL_APP_EVENT_STARTED, shellBootOnAppEvent, NULL);
	SceShell_83D16CA8(SHELL_APP_EVENT_ENDED, shellBootOnAppEvent, NULL);

	common::SharedPtr<job::JobItem> setupJob(new ShellInitialSetupJob(clockInvalid));

	job::JobQueue::default_queue->Enqueue(setupJob);
}

static void shellBootCheckInitialSetup(bool clockUpdated)
{
	sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DEFAULT);
	if (shellStartupGetInitialize() != 1) {
		if (!shellStartupIsPmMode() && shellStartupGetIduMode() != 1 && !shellStartupIsTestMode()) {
			shellBootStartInitialSetup(false);
			return;
		}
		sceRegMgrSetKeyInt("/CONFIG/SYSTEM", "initialize", 1);
	}

	SceRtcTick tick;

	if (sceRtcGetCurrentTick(&tick) == SCE_RTC_ERROR_NOT_INITIALIZED) {
		if (!shellStartupIsPmMode() && shellStartupGetIduMode() != 1 && !shellStartupIsTestMode()) {
			int automatic;

			if (system::Model::GetType() == 1 && !clockUpdated &&
			    sceRegMgrGetKeyInt("/CONFIG/DATE", "set_automatically", &automatic) == 0 &&
			    automatic == 1) {
				int request;

				if (npUpdateClockStart(&request, 0x1000010A, sceKernelGetProcessId()) == 0) {
					common::MainThreadCallList::Register(shellBootOnClockUpdatePoll,
									     (void *)request);
					return;
				}
			}
			shellBootStartInitialSetup(true);
			return;
		}

		SceDateTime time;
		SceRtcTick local;

		time.year = 2011;
		time.month = 1;
		time.day = 1;
		time.hour = 0;
		time.minute = 0;
		time.second = 0;
		time.microsecond = 0;
		sceRtcGetTick(&time, &local);
		sceRtcConvertLocalTimeToUtc(&local, &tick);
		vshRtcSetCurrentTick(&tick);
	}
	s_bootState |= BOOT_STATE_BOOT_APPS_DONE;
	shellBootCheckDatabase();
}

/* the system plugins the home screen needs, each counted until it is loaded */
static void shellBootStartSystem(void)
{
	sceKernelPowerLock(SCE_KERNEL_POWER_TICK_DEFAULT);
	sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DEFAULT);
	graph::GateBlur::Initialize();
	graph::ScreenCopy::Initialize();

	{
		Plugin::InitParam param;

		param.name = "power_manage_plugin";
		param.resource_file = "vs0:vsh/shell/power_manage_plugin.rco";
		param.caller_name = "__main__";
		param.set_param_func = powerManagePluginSetParam;
		param.init_func = powerManagePluginInit;
		param.start_func = powerManagePluginStart;
		param.stop_func = powerManagePluginStop;
		param.exit_func = powerManagePluginExit;

		GraphMem::DeviceType device = (GraphMem::DeviceType)(system::Model::GetType() != 1);
		char poolName[] = "SceShellPowerManageSurfacePool";
		void *pool = GraphMem::AllocOSMemory(device, 0xa0000, poolName, NULL);

		param.surface_pool = new graph::SurfacePool(pool, 0xa0000, poolName);
		Plugin::LoadAsync(param, shellBootOnSystemPluginLoaded, 0);
		s_pluginsLoading++;
	}
	{
		Plugin::InitParam param;

		param.name = "telephony_plugin";
		param.resource_file = "vs0:vsh/shell/telephony_plugin.rco";
		param.caller_name = "__main__";
		param.set_param_func = shellBootPluginSetParamNoop;
		param.init_func = telephonyPluginInit;
		param.start_func = telephonyPluginStart;
		param.stop_func = telephonyPluginStop;
		param.exit_func = telephonyPluginExit;
		param.private_heap_size = system::Model::GetType() == 0 ? 0x80000 : 0x8000;
		Plugin::LoadAsync(param, shellBootOnSystemPluginLoaded, 0);
		s_pluginsLoading++;
	}
	{
		Plugin::InitParam param;

		param.name = "musiccore_plugin";
		param.resource_file = "vs0:vsh/shell/musiccore_plugin.rco";
		param.caller_name = "__main__";
		param.set_param_func = shellBootPluginSetParamNoop;
		param.init_func = musiccorePluginInit;
		param.start_func = musiccorePluginStart;
		param.stop_func = musiccorePluginStop;
		param.exit_func = musiccorePluginExit;
		Plugin::LoadAsync(param, shellBootOnSystemPluginLoaded, 0);
		s_pluginsLoading++;
	}
	{
		Plugin::InitParam param;

		param.name = "impose_plugin";
		param.resource_file = "vs0:vsh/shell/impose_plugin.rco";
		param.caller_name = "__main__";
		param.set_param_func = imposePluginSetParam;
		param.init_func = imposePluginInit;
		param.start_func = imposePluginStart;
		param.stop_func = imposePluginStop;
		param.exit_func = imposePluginExit;

		char poolName[] = "SceShellImposeSurfacePool";
		void *pool = GraphMem::AllocOSMemory(GraphMem::DeviceType_VideoMemory, 0x40000, poolName, NULL);

		param.surface_pool = new graph::SurfacePool(pool, 0x40000, poolName);
		param.graphics_flag = 7;
		Plugin::LoadAsync(param, shellBootOnSystemPluginLoaded, 0);
		s_pluginsLoading++;
	}
	{
		Plugin::InitParam param;

		param.name = "indicator_plugin";
		param.resource_file = "vs0:vsh/shell/indicator_plugin.rco";
		param.caller_name = "__main__";
		param.set_param_func = indicatorPluginSetParam;
		param.init_func = indicatorPluginInit;
		param.start_func = indicatorPluginStart;
		param.stop_func = indicatorPluginStop;
		param.exit_func = indicatorPluginExit;

		GraphMem::DeviceType device = (GraphMem::DeviceType)(system::Model::GetType() != 1);
		char poolName[] = "SceShellIndicatorSurfacePool";
		void *pool = GraphMem::AllocOSMemory(device, 0x4b000, poolName, NULL);

		param.surface_pool = new graph::SurfacePool(pool, 0x4b000, poolName);
		param.graphics_flag = 7;
		Plugin::LoadAsync(param, shellBootOnSystemPluginLoaded, 0);
		s_pluginsLoading++;
	}
	{
		Plugin::InitParam param;

		param.name = "game_plugin";
		param.caller_name = "__main__";
		param.set_param_func = shellBootPluginSetParamNoop;
		param.init_func = gamePluginInit;
		param.start_func = gamePluginStart;
		param.stop_func = gamePluginStop;
		param.exit_func = gamePluginExit;
		Plugin::LoadAsync(param, shellBootOnSystemPluginLoaded, 0);
		s_pluginsLoading++;
	}
	{
		Plugin::InitParam param;

		param.name = "topmenu_plugin";
		param.resource_file = "vs0:vsh/shell/topmenu_plugin.rco";
		param.caller_name = "__main__";
		param.set_param_func = topmenuPluginSetParam;
		param.init_func = topmenuPluginInit;
		param.start_func = topmenuPluginStart;
		param.stop_func = topmenuPluginStop;
		param.exit_func = topmenuPluginExit;

		GraphMem::DeviceType device = GraphMem::DeviceType_VideoMemory;

		if (system::Model::GetType() == 1)
			device = GraphMem::DeviceType_SystemMemory;

		char poolName[] = "SceShellTopmenuSurfacePool";
		void *pool = GraphMem::AllocOSMemory(device, 0x40000, poolName, NULL);

		param.surface_pool = new graph::SurfacePool(pool, 0x40000, poolName);
		param.graphics_flag = 7;
		Plugin::LoadAsync(param, shellBootOnSystemPluginLoaded, 0);
		s_pluginsLoading++;
	}
	if (s_pluginsLoading == 0) {
		sceKernelPowerUnlock(SCE_KERNEL_POWER_TICK_DEFAULT);
		s_bootState |= BOOT_STATE_SYSTEM_PLUGINS_LOADED;
		shellBootCheckInitialSetup(false);
	}
}

void shellBootStartSystemEntry(void)
{
	shellBootStartSystem();
}

/* the first power event: the system event goes out, the shell's media information is written,
   the trigger manager service and the parental control and zoom managers start; heard once */
static void shellBootOnPowerEvent(int code, const void *data, SceSize size, void *arg)
{
	ShellMediaInfoRequest request;

	vshKernelSendSysEvent(0);
	request.type = 1;
	request.attr[0] = 0;
	request.attr[1] = 0;
	request.attr[2] = 0;
	request.attr[3] = 0;
	shellMediaInfoWrite(&request, false);
	shellTriggerMgrSvcCtrl(0, 0);
	shellParentalControlMgrCreate();
	shellZoomMgrCreate();

	Plugin *powerManage = Plugin::Find("power_manage_plugin");

	if (powerManage != NULL) {
		ShellPluginInterface *iface = (ShellPluginInterface *)powerManage->GetInterface(1);

		if (iface != NULL) {
			iface->func[POWER_MANAGE_PLUGIN_FUNC_POWER_EVENT]();
			iface->func[POWER_MANAGE_PLUGIN_FUNC_BOOT_DONE]();
		}
	}
	SceShell_D4C7D2B7(SHELL_EVENT_POWER, shellBootOnPowerEvent, NULL);
}

/* the model is known: on a model other than 1 the telephony setup starts; heard once */
static void shellBootOnModelEvent(int code, const void *data, SceSize size, void *arg)
{
	if (system::Model::GetType() != 1)
		shellTelephonyStartSetup(NULL, NULL);
	SceShell_D4C7D2B7(SHELL_EVENT_MODEL, shellBootOnModelEvent, NULL);
}

/* a resident plugin's load finished: the last one ends the boot's loads */
static void shellBootOnResidentPluginLoaded(Plugin *plugin)
{
	if (--s_pluginsLoading == 0)
		s_bootState |= BOOT_STATE_RESIDENT_PLUGINS_LOADED;
}

/* idu_update_plugin unloaded: the resident plugins follow */
static void shellBootOnIduUpdatePluginUnloaded(void *userdata)
{
	s_bootState |= BOOT_STATE_RESIDENT_PLUGINS_STARTED;
	shellBootStartResidentPlugins();
}

/* the cold boot and the boot applications are done: the plugin in front closes its screen */
static __attribute__((noinline)) void shellBootProceed(void)
{
	sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DEFAULT);

	Plugin *coldboot = Plugin::Find("coldboot_plugin");
	ColdbootPluginInterface *coldbootIface;

	if (coldboot != NULL &&
	    (coldbootIface = (ColdbootPluginInterface *)coldboot->GetInterface(1)) != NULL) {
		shellBootScreenClose();
		coldbootIface->finish();
		return;
	}

	Plugin *dbrecovery = Plugin::Find("dbrecovery_plugin");
	DbrecoveryPluginInterface *dbrecoveryIface;

	if (dbrecovery != NULL &&
	    (dbrecoveryIface = (DbrecoveryPluginInterface *)dbrecovery->GetInterface(1)) != NULL) {
		shellBootScreenClose();
		dbrecoveryIface->finish();

		Plugin *powerManage = Plugin::Find("power_manage_plugin");

		if (powerManage != NULL) {
			ShellPluginInterface *iface = (ShellPluginInterface *)powerManage->GetInterface(1);

			if (iface != NULL)
				iface->func[POWER_MANAGE_PLUGIN_FUNC_BOOT_DONE]();
		}
		return;
	}

	if (s_bootAppStack.size() == 0)
		shellBootScreenCreate();
	if (s_bootState & BOOT_STATE_DATABASE_READY)
		shellBootStartResidentOrIdu();
}

/* dbrecovery_plugin unloaded: the database is ready */
static void shellBootOnDbrecoveryPluginUnloaded(void *userdata)
{
	s_bootState |= BOOT_STATE_DATABASE_READY;
	if ((s_bootState & BOOT_STATE_COLDBOOT_DONE) && (s_bootState & BOOT_STATE_BOOT_APPS_DONE)) {
		shellBootScreenCreate();
		shellBootStartResidentOrIdu();
	}
}

/* dbrecovery_plugin's recovery finished: what it reports is the database state */
static void shellBootOnDbrecoveryFinished(Plugin *plugin)
{
	DbrecoveryPluginInterface *iface = (DbrecoveryPluginInterface *)plugin->GetInterface(1);

	if (iface != NULL)
		s_databaseState = iface->getState();
}

/* dbrecovery_plugin loaded: told when it finishes and which theme to show */
static void shellBootOnDbrecoveryPluginLoaded(Plugin *plugin)
{
	DbrecoveryPluginInterface *iface = (DbrecoveryPluginInterface *)plugin->GetInterface(1);

	if (iface != NULL) {
		iface->setFinishCallback(shellBootOnDbrecoveryFinished);
		if (!shellThemeIconsEnabled())
			iface->setTheme(shellThemeGetInfo());
	}
	if ((s_bootState & BOOT_STATE_COLDBOOT_DONE) && (s_bootState & BOOT_STATE_BOOT_APPS_DONE))
		shellBootProceed();
}

/*
 * an application event while the boot's applications run (the initial setup, a recovery): their
 * ids are stacked as they start and unstacked as they end, the one on top is brought to the
 * front, and the last one's end lets the boot go on
 */
static void shellBootOnAppEvent(int code, const void *data, SceSize size, void *arg)
{
	const ShellAppEvent *event = (const ShellAppEvent *)data;
	int appId;

	if (code > 0) {
		SceAppMgrAppStatus status;

		if (code != SHELL_APP_EVENT_STARTED || sceAppMgrGetStatusByAppId(event->appId, &status) != 0 ||
		    (status.processAttr & 1))
			return;
		if (s_bootAppStack.size() == 0) {
			shellBootScreenClose();
			if (system::Model::GetType() != 1) {
				sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);
				sceAVConfigSetAutoSuspend2(600);
			}
			inputdevice::touchscreen::ActivateDefaultRegion(0, false);
			inputdevice::touchscreen::ActivateDefaultRegion(1, false);
		}
		s_bootAppStack.push_back(event->appId);
		appId = event->appId;
	} else {
		if (code < 0)
			return;
		if (s_bootAppStack.size() == 0 || s_bootAppStack.back() != event->appId)
			return;
		s_bootAppStack.pop_back();
		if (s_bootAppStack.size() == 0) {
			if (system::Model::GetType() != 1)
				sceAVConfigClearAutoSuspend2();
			inputdevice::touchscreen::ActivateDefaultRegion(0, true);
			inputdevice::touchscreen::ActivateDefaultRegion(1, true);
			SceShell_E3D3A162(SHELL_APP_EVENT_STARTED, shellBootOnAppEvent, NULL);
			SceShell_E3D3A162(SHELL_APP_EVENT_ENDED, shellBootOnAppEvent, NULL);
			shellAppSetForeground(1, "NPXS19999", 0);
			s_bootState |= BOOT_STATE_BOOT_APPS_DONE;
			shellBootProceed();
			return;
		}
		appId = s_bootAppStack.back();
	}
	if (appId != -1) {
		SceAppMgrAppStatus status;

		sceAppMgrActivateApp(appId);
		if (sceAppMgrGetStatusByAppId(appId, &status) == 0) {
			ShellAppActivatedEvent activated;

			activated.processId = status.processId;
			activated.appId = status.appId;
			activated.appName = status.appName;
			shellAppEventSend(SHELL_APP_EVENT_ACTIVATED, &activated, 0);
			shellAppSetForeground(1, status.appName, 0);
		}
	}
}

/* the network clock update, polled each frame: the setup check again when it is done */
static void shellBootOnClockUpdatePoll(void *userdata)
{
	int request = (int)userdata;

	if (npUpdateClockGetState(request) == 2) {
		int result;

		npUpdateClockGetResult(request, &result);
		common::MainThreadCallList::Unregister(shellBootOnClockUpdatePoll, userdata);
		shellBootCheckInitialSetup(true);
	} else if (++s_clockPollCount / 60 > 15) {
		SceNpManager_EB16514C(request);
	}
}

/* a system plugin loaded: musiccore starts the background music; the last one ends the loads */
static void shellBootOnSystemPluginLoaded(Plugin *plugin)
{
	if (sce_paf_strcmp(plugin->GetName(), "musiccore_plugin") == 0 && shellMusicStart(2) == 0) {
		int bgm;

		if ((sceRegMgrGetKeyInt("/CONFIG/SHELL", "bgm", &bgm) | bgm) == 0)
			shellMusicSetBgm(1);
	}
	if (--s_pluginsLoading == 0) {
		sceKernelPowerUnlock(SCE_KERNEL_POWER_TICK_DEFAULT);
		s_bootState |= BOOT_STATE_SYSTEM_PLUGINS_LOADED;
		shellBootCheckInitialSetup(false);
	}
}

/* coldboot_plugin unloaded: the cold boot is done */
static void shellBootOnColdbootPluginUnloaded(void *userdata)
{
	s_bootState |= BOOT_STATE_COLDBOOT_DONE;
	shellBootProceed();
}
