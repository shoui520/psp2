/*
 * boot.cpp: SceShell's boot sequence. The shell loads its resident plugins (applauncher,
 * download, livearea, mail_resident) when the cold-boot plugins are done, counts their loads
 * down, and keeps the stack of the applications started before the home screen (the initial
 * setup, a database recovery). Until the boot is done, launches by name are refused except the
 * network settings, which go through the settings application.
 *
 * The 3.6x shell names none of its source files: this file's name and the names in it are ours
 * (agent/namecheck).
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
#include <job/job.h>
#include <common/main_thread_call_list.h>
#include <system/system.h>
#include <rtc.h>
#include <regmgr/regmgr.h>
#include <kernel/processmgr.h>
#include <appmgr/appmgr.h>

#include "boot.h"
#include "plugins.h"
#include "shell.h"
#include "src/livespace_db/livespace_db.h"

using namespace paf;

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

static ShellAppIdStack s_bootAppStack;		/* applications started before the home screen */

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
static void shellBootOnAppEvent(int code, const void *data, void *arg);
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
	return s_bootAppStack.count != 0;
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

	virtual int Run();

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
static void shellBootOnPowerEvent(int code, const void *data, void *arg);
static void shellBootOnModelEvent(int code, const void *data, void *arg);

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
		param.arguments[string("theme_apply")] = Plugin::Argument(shellThemeGetApplyState());
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

/* livespace_db's errors after which the trigger table is rebuilt (values only: no names known) */
#define SHELL_LSDB_TRIGGER_TABLE_STALE(call)	\
	((call) == (int)0x801010F1 || (call) == (int)0x801010F4 || (call) == (int)0x801010FE || \
	 (call) == (int)0x801010FF || (call) == (int)0x801010F2)

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
		param.arguments[string("pagec")] = Plugin::Argument(shellStartupGetPageCount());
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
			shellCounterAdd(230, 1, 0);
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
static void shellBootCheckInitialSetup(bool clockUpdated)
{
	bool clockInvalid = false;

	sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DEFAULT);
	if (shellStartupGetInitialize() != 1) {
		if (!shellStartupIsPmMode() && shellStartupGetIduMode() != 1 && !shellStartupIsTestMode())
			goto initialSetup;
		sceRegMgrSetKeyInt("/CONFIG/SYSTEM", "initialize", 1);
	}

	SceRtcTick tick;

	if (sceRtcGetCurrentTick(&tick) == SCE_RTC_ERROR_NOT_INITIALIZED) {
		if (shellStartupIsPmMode() || shellStartupGetIduMode() == 1 || shellStartupIsTestMode()) {
			SceDateTime time = { 2011, 1, 1 };
			SceRtcTick local;

			sceRtcGetTick(&time, &local);
			sceRtcConvertLocalTimeToUtc(&local, &tick);
			shellRtcSetCurrentTick(&tick);
		} else {
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
			clockInvalid = true;
			goto initialSetup;
		}
	}
	s_bootState |= BOOT_STATE_BOOT_APPS_DONE;
	shellBootCheckDatabase();
	return;

initialSetup:
	SceShell_83D16CA8(3, shellBootOnAppEvent, NULL);
	SceShell_83D16CA8(0, shellBootOnAppEvent, NULL);

	common::SharedPtr<job::JobItem> setupJob(new ShellInitialSetupJob(clockInvalid));

	job::JobQueue::default_queue->Enqueue(setupJob);
}

void shellBootStartSystemEntry(void)
{
	shellBootStartSystem();
}

/* the model is known: on a model other than 1 the telephony setup starts; heard once */
static void shellBootOnModelEvent(int code, const void *data, void *arg)
{
	if (system::Model::GetType() != 1)
		shellTelephonyStartSetup(0, NULL);
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

	if (s_bootAppStack.count == 0)
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
		if (!shellThemeIsApplying())
			iface->setTheme(shellThemeGetCurrent());
	}
	if ((s_bootState & BOOT_STATE_COLDBOOT_DONE) && (s_bootState & BOOT_STATE_BOOT_APPS_DONE))
		shellBootProceed();
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
