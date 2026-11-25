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
#include "../../livespace_db/lsdb_keys.h"

extern void *g_liveareaInterface;			/* 0x81566F90 */
extern void *shellPluginGetInterface(const char *name, int id); /* 0x810C9D14 */
extern int liveareaRegisterInterface(paf::Plugin *plugin); /* 0x81199D10 */
extern void *liveareaCacheCreate(paf::Plugin *plugin);	/* 0x8119A5DE */
extern void liveareaCacheDestroy(void);			/* 0x8119A5F4 */
extern void *liveareaMgrCreate(paf::Plugin *plugin);	/* 0x8119A83C */
extern void liveareaMgrDestroy(void);			/* 0x8119A852 */
extern void liveareaMgrStart(void);			/* 0x8119A8C6 */
extern void liveareaMgrStop(void);			/* 0x8119A8D8 */
extern void liveareaSub34A248(void);			/* 0x8134A248 */
extern void liveareaSub34A270(void);			/* 0x8134A270 */
extern void liveareaSub34A2EA(void (*fn1)(), void (*fn2)(), void (*fn3)()); /* 0x8134A2EA */
extern void liveareaSub34A34C(int arg);			/* 0x8134A34C */
extern void liveareaSub38D570(int arg);			/* 0x8138D570 */
extern void liveareaSub38D77E(void);			/* 0x8138D77E */
extern void liveareaCb20EE1E(void);			/* 0x8120EE1E */
extern void liveareaCb20EE3C(void);			/* 0x8120EE3C */
extern void liveareaCb20EF44(void);			/* 0x8120EF44 */

/* 0x81199B68 */
void liveareaSetParam(int *outParam)
{
	outParam[0] = 50;
	outParam[1] = 0x100000;
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

	liveareaSub34A248();
	liveareaSub34A34C(33);
	liveareaSub34A2EA(liveareaCb20EE1E, liveareaCb20EE3C, liveareaCb20EF44);
	if (sceSysmoduleLoadModule(SCE_SYSMODULE_LIVEAREA) != 0)
		__builtin_trap();
	liveareaMgrCreate(plugin);
}

/* 0x81199C2A */
void liveareaOnStart(void)
{
	void *topmenu = shellPluginGetInterface("topmenu_plugin", 1);
	if (topmenu != NULL) {
		typedef void (*SetLiveAreaFn)(void *);
		SetLiveAreaFn fn = *(SetLiveAreaFn *)((char *)topmenu + 244);
		fn(g_liveareaInterface);
	}
	liveareaSub38D570(0);
	liveareaMgrStart();
}

/* 0x81199C5A */
void liveareaOnStop(void)
{
	void *topmenu = shellPluginGetInterface("topmenu_plugin", 1);
	if (topmenu != NULL) {
		typedef void (*SetLiveAreaFn)(void *);
		SetLiveAreaFn fn = *(SetLiveAreaFn *)((char *)topmenu + 244);
		fn(NULL);
	}
	liveareaMgrStop();
	liveareaSub34A270();
	liveareaSub38D77E();
}

/* 0x81199C84 */
void liveareaOnUnload(void)
{
	sceSysmoduleUnloadModule(SCE_SYSMODULE_LIVEAREA);
	liveareaCacheDestroy();
	paf::ui::filelist::Manager::Finalize();
	liveareaMgrDestroy();
}
