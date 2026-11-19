/*
 * applauncher_plugin.cpp: the applauncher plugin's set-param, init, start, stop and exit
 * functions (0x810A8584-0x810A881C), as boot.cpp loads the plugin. The file's name is ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <plugin.h>
#include <file/localfile.h>
#include <common/shared_ptr.h>
#include <graphics/effects/wave_switch_api.h>
#include <kernel/iofilemgr.h>
#include "../../../shell.h"
#include "../../../plugins.h"
#include "../../livespace_db/lsdb_keys.h"
#include "applauncher.h"

using namespace paf;

/* the plugin's draw priority and surface pool, as boot.cpp makes it (0x810A8584) */
void applauncherPluginSetParam(Plugin::InitParam2 *param)
{
	param->draw_priority = 50;
	param->surface_pool_size = 0xc0000;
}

/*
 * The plugin's parts made: its interface, the caches, the wave colours of waveparam.bin on a
 * console with its own wave (pd0), and a theme still to apply (0x810A8590).
 */
void applauncherPluginInit(Plugin *plugin)
{
	applauncherInterfaceRegister(plugin);
	applauncherInterfaceSetResource();
	applauncherDataCacheCreate(plugin);
	applauncherIconCacheCreate(plugin);

	if (shellStartupGetIcwvp()) {
		int err;
		common::SharedPtr<File> file = LocalFile::Open("pd0:wave/waveparam.bin",
							       SCE_O_RDONLY, 0xf000, &err);

		if (file.get() != NULL) {
			ApplauncherWaveParamFile param;

			if (file->Read(&param, sizeof(param)) == sizeof(param)) {
				ScePafGraphics_45A01FA1((const graph::WavePreset *)&param);
				applauncherWaveSetColors(param.index, param.colors);
			} else {
				sceClibPrintf("[SceShell] waveparam invalid size\n");
			}
		} else {
			sceClibPrintf("[SceShell] waveparam open error \n");
		}
	}

	ShellPluginInterface *topmenu = (ShellPluginInterface *)
		applauncherPluginGetInterface("topmenu_plugin", 1);

	if (topmenu != NULL)
		topmenu->func[TOPMENU_PLUGIN_FUNC_APPLAUNCHER_INIT]();
	if ((int)plugin->GetArg("theme_apply", NULL))
		applauncherThemeReload();
}

/* the plugin started: topmenu given its interface, and the pages and their parts made
   (0x810A873A) */
void applauncherPluginStart(Plugin *plugin)
{
	ShellPluginInterface *topmenu = (ShellPluginInterface *)
		applauncherPluginGetInterface("topmenu_plugin", 1);

	if (topmenu != NULL)
		((void (*)(void *))topmenu->func[TOPMENU_PLUGIN_FUNC_SET_APPLAUNCHER])
			(g_applauncherInterface);
	applauncherIconCaptureCreate(shellIconCacheGetSurfacePool());
	applauncherPageCreate(plugin);
	applauncherFolderCreate();
	applauncherIconPoolCreate();
	applauncherSlotsCreate();
	if ((int)plugin->GetArg("theme_apply", NULL))
		applauncherThemeRefresh();
}

/* the plugin stopped: its parts taken down, in the other order (0x810A87D6) */
void applauncherPluginStop(Plugin *plugin)
{
	ShellPluginInterface *topmenu = (ShellPluginInterface *)
		applauncherPluginGetInterface("topmenu_plugin", 1);

	if (topmenu != NULL)
		((void (*)(void *))topmenu->func[TOPMENU_PLUGIN_FUNC_SET_APPLAUNCHER])(NULL);
	applauncherSlotsDestroy();
	applauncherIconPoolDestroy();
	applauncherFolderDestroy();
	applauncherPageDestroy();
	applauncherIconCaptureDestroy();
}

/* the plugin ended: the caches taken down (0x810A8808) */
void applauncherPluginExit(Plugin *plugin)
{
	applauncherIconCacheDestroy();
	applauncherDataCacheDestroy();
	applauncherExitReserved0();
	applauncherExitReserved1();
}
