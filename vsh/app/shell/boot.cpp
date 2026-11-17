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
#include <plugin.h>
#include <common/shared_ptr.h>
#include <graphics/graphmem/graphmem.h>
#include <graphics/surface/surface_pool.h>
#include <graphics/effects/wave_switch.h>
#include <job/job.h>
#include <kernel/processmgr.h>
#include <appmgr/appmgr.h>

#include "boot.h"
#include "plugins.h"
#include "shell.h"

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

static unsigned char s_bootState;		/* BOOT_STATE_* */
static bool s_bootLaunchRestricted = true;	/* launches by name refused */
static int s_pluginsLoading;			/* plugin loads not finished */
static int s_databaseState = -1;		/* what the database check or recovery reported */

static ShellAppIdStack s_bootAppStack;		/* applications started before the home screen */

static void shellBootStartResidentPlugins(void);
static void shellBootCheckDatabase(void);
static void shellBootStartSystem(void);

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

void shellBootCheckDatabaseEntry(void)
{
	shellBootCheckDatabase();
}

void shellBootStartSystemEntry(void)
{
	shellBootStartSystem();
}

/* a resident plugin's load finished: the last one ends the boot's loads */
static void shellBootOnResidentPluginLoaded(Plugin *plugin)
{
	if (--s_pluginsLoading == 0)
		s_bootState |= BOOT_STATE_RESIDENT_PLUGINS_LOADED;
}

/* idu_update_plugin loaded: the resident plugins follow */
static void shellBootOnIduUpdatePluginLoaded(Plugin *plugin)
{
	s_bootState |= BOOT_STATE_RESIDENT_PLUGINS_STARTED;
	shellBootStartResidentPlugins();
}
