/*
 * SceFriendSelect (vs0:sys/external/friend_select.suprx):
 * Standalone PAF plugin entrypoints registering the SceFriendSelect
 * ModuleInterface and plugin lifecycle hooks.
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <plugin.h>
#include <module/module.h>
#include "friend_select_internal.h"

extern "C" {
#include "../../../../../vsh/paf/src/std/stdc++/atexit.h"

void *__dso_handle;
extern void (*__start__Zpreinit_array[])(void);
extern void (*__stop__Zpreinit_array[])(void);
extern void (*__start__Zinit_array[])(void);
extern void (*__stop__Zinit_array[])(void);
}

using namespace paf;

static void FriendSelectAbortApi(void)
{
	FriendSelectAppAbort();
}

static FriendSelectInterface s_friendSelectInterface = {
	FriendSelectAbortApi
};

static Plugin::ModuleInterface s_friendSelectModuleInterface = {
	FriendSelectSetParam,
	FriendSelectInit,
	FriendSelectStart,
	FriendSelectStop,
	FriendSelectExit
};

__attribute__((noinline))
static void FriendSelectModuleInit(void)
{
	__dso_handle = &__dso_handle;
	for (void (**ctor)(void) = __start__Zpreinit_array; ctor < __stop__Zpreinit_array; ++ctor) {
		if (*ctor != NULL)
			(*ctor)();
	}
	for (void (**ctor)(void) = __start__Zinit_array; ctor < __stop__Zinit_array; ++ctor) {
		if (*ctor != NULL)
			(*ctor)();
	}
}

__attribute__((noinline))
static void FriendSelectModuleTerm(void)
{
	__cxa_finalize(&__dso_handle);
}

extern "C" int module_start(SceSize arglen, const void *argp)
{
	(void)arglen;
	FriendSelectModuleInit();
	Module *module = *(Module *const *)argp;
	module->SetInterface(1, &s_friendSelectModuleInterface);
	return 0;
}

extern "C" int module_stop(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	FriendSelectModuleTerm();
	return 0;
}

void FriendSelectSetParam(Plugin::InitParam2 *param)
{
	FriendLibRegisterWidgetFactory();
	param->draw_priority = 100;
}

void FriendSelectInit(Plugin *plugin)
{
	plugin->SetInterface(1, &s_friendSelectInterface);
	FriendLibInit(plugin->m_private_heap, plugin->m_surface_pool);
	FriendSelectAppCreate(plugin);
}

void FriendSelectStart(Plugin *plugin)
{
	(void)plugin;
	FriendLibStart();
	FriendSelectAppStart();
}

void FriendSelectStop(Plugin *plugin)
{
	(void)plugin;
	FriendSelectAppAbort();
	FriendLibStop();
}

void FriendSelectExit(Plugin *plugin)
{
	(void)plugin;
	FriendSelectAppDestroy();
	FriendLibTerm();
	FriendLibUnregisterWidgetFactory();
}
