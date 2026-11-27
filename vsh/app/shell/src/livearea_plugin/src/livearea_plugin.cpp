/*
 * livearea_plugin.cpp: livearea_plugin lifecycle entry points (0x81199B68-0x81199D10,
 * sti 0x81199C9E). The file and function names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <libsysmodule.h>
#include <plugin.h>
#include <std/string>
#include <widget/w_file_list.h>
#include "livearea_internal.h"

extern int netCtlApiIncActiveRef(void);
extern int netCtlApiDecActiveRef(void);
extern "C" int netCtlApiResetExclusivePrompt(void);
extern void *applauncherPluginGetInterface(const char *name, int id);

/* 0x81199B68 */
void liveareaSetParam(paf::Plugin::InitParam2 *outParam)
{
	outParam->draw_priority = 50;
	outParam->surface_pool_size = 0x100000;
}

/* 0x81199B74 */
void liveareaOnLoad(paf::Plugin *plugin)
{
	liveareaRegisterInterface(plugin);
	liveareaCacheCreate(plugin);

	paf::string rootPath;
	paf::ui::filelist::Manager::InitParam param(rootPath);
	param.m_stack_size[3] = 0x2000;
	param.m_stack_size[2] = 0x2000;
	param.m_stack_size[1] = 0x2000;
	param.m_stack_size[0] = 0x2000;
	paf::ui::filelist::Manager::Initialize(param);

	activityUiInit();
	activityUiSetDialogPriority(33);
	activityUiSetNetCallbacks(reinterpret_cast<void (*)(void)>(netCtlApiIncActiveRef),
				  reinterpret_cast<void (*)(void)>(netCtlApiDecActiveRef),
				  reinterpret_cast<void (*)(void)>(netCtlApiResetExclusivePrompt));
	if (sceSysmoduleLoadModule(SCE_SYSMODULE_LIVEAREA) != 0)
		__builtin_trap();
	liveareaMgrCreate(plugin);
}

/* 0x81199C2A */
void liveareaOnStart(void)
{
	LiveAreaTopmenuInterface *topmenu =
		(LiveAreaTopmenuInterface *)applauncherPluginGetInterface("topmenu_plugin", 1);
	if (topmenu != NULL)
		topmenu->setLiveAreaInterface(g_liveareaInterface);
	arlGraphicsInit(0);
	liveareaMgrStart();
}

/* 0x81199C5A */
void liveareaOnStop(void)
{
	LiveAreaTopmenuInterface *topmenu =
		(LiveAreaTopmenuInterface *)applauncherPluginGetInterface("topmenu_plugin", 1);
	if (topmenu != NULL)
		topmenu->setLiveAreaInterface(NULL);
	liveareaMgrStop();
	activityUiTerm();
	arlGraphicsTerm();
}

/* 0x81199C84 */
void liveareaOnUnload(void)
{
	sceSysmoduleUnloadModule(SCE_SYSMODULE_LIVEAREA);
	liveareaCacheDestroy();
	paf::ui::filelist::Manager::Finalize();
	liveareaMgrDestroy();
}

void liveareaPluginSetParam(paf::Plugin::InitParam2 *param) { liveareaSetParam(param); }
void liveareaPluginInit(paf::Plugin *plugin) { liveareaOnLoad(plugin); }
void liveareaPluginStart(paf::Plugin *plugin) { (void)plugin; liveareaOnStart(); }
void liveareaPluginStop(paf::Plugin *plugin) { (void)plugin; liveareaOnStop(); }
void liveareaPluginExit(paf::Plugin *plugin) { (void)plugin; liveareaOnUnload(); }
