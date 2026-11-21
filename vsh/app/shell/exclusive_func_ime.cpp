/*
 * exclusive_func_ime.cpp: ime_plugin as an exclusive function (0x8113FD2C-0x81140118): while it
 * runs, the plugin is loaded into the manager's block, its surface pool and private heap there,
 * with the IME settings and the server as its listener. The plugin's, the heap's and the pool's
 * names are Sony's; the file's and the other names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <memory/heap_allocator.h>
#include <graphics/surface/surface_pool.h>
#include <plugin.h>
#include "exclusive_func.h"
#include "ime_service.h"

using namespace paf;

/* the block's use: the plugin's surface pool, then its private heap */
#define EXCLUSIVE_FUNC_IME_MEM_SIZE		0x400000
#define EXCLUSIVE_FUNC_IME_SURFACE_POOL_SIZE	1228800
#define EXCLUSIVE_FUNC_IME_HEAP_SIZE		0x2D4000

/* the plugin's load options */
#define EXCLUSIVE_FUNC_IME_INTERFACE_VERSION	1
#define EXCLUSIVE_FUNC_IME_MODULE_OPTION	1

static void exclusiveFuncImeOnUnloaded(void *arg);
static void exclusiveFuncImeOnLoaded(Plugin *plugin);

/* (0x8113FD2C) */
ExclusiveFuncIme::ExclusiveFuncIme(ImePluginLoadListener *listener, ImeConfig *config)
	: ExclusiveFunc(2, true), m_listener(listener), m_surfacePool(NULL), m_heap(NULL),
	  m_config(config)
{
}

/* (0x8113FD58, 0x8113FD74) */
ExclusiveFuncIme::~ExclusiveFuncIme()
{
}

/* (0x8113FD94) */
int ExclusiveFuncIme::Start(const ExclusiveFuncMemBlock *mem)
{
	char *base = (char *)mem->base;

	if (base == NULL || mem->size < EXCLUSIVE_FUNC_IME_MEM_SIZE)
		return -1;

	memory::HeapAllocator::Option option;

	option.disable_dbgprint = false;
	option.entry_max = 0;
	option.warning_size = 0;
	option.disable_assert_on_alloc_failure = true;
	option.memblk_type = memory::AllocationBlockType_User;
	option.alignment = 0;

	graph::SurfacePool *pool = new graph::SurfacePool(base, EXCLUSIVE_FUNC_IME_SURFACE_POOL_SIZE,
							  "SceImePlugin", &option);
	Plugin::InitParam param;

	param.name = "ime_plugin";
	param.resource_file = "vs0:vsh/shell/ime_plugin.rco";
	param.caller_name = "__main__";
	param.module_file = "vs0:vsh/shell/ime_plugin.suprx";
	param.module_interface_version = EXCLUSIVE_FUNC_IME_INTERFACE_VERSION;
	param.private_heap_size = 0;
	param.private_heap = m_heap =
		new memory::HeapAllocator(base + EXCLUSIVE_FUNC_IME_SURFACE_POOL_SIZE,
					  EXCLUSIVE_FUNC_IME_HEAP_SIZE, "SceImePluginPrivateHeap");
	param.surface_pool = pool;
	param.module_option = EXCLUSIVE_FUNC_IME_MODULE_OPTION;
	param.option = Plugin::Option_WaitWidgetActuallyDestroyed;
	param.arguments[string("exfunc_this")] = Plugin::Argument((int32_t)this);
	param.arguments[string("config")] = Plugin::Argument((int32_t)m_config);
	param.unload_callback = exclusiveFuncImeOnUnloaded;
	param.param = this;
	m_surfacePool = pool;
	Plugin::LoadAsync(param, exclusiveFuncImeOnLoaded, 1);
	return 0;
}

/* (0x81140048) the plugin unloaded: its memory freed, the function stopped and the server told */
__attribute__((noinline)) static void exclusiveFuncImeOnUnloaded(void *arg)
{
	ExclusiveFuncIme *func = (ExclusiveFuncIme *)arg;

	if (func->m_surfacePool != NULL) {
		delete func->m_surfacePool;
		func->m_surfacePool = NULL;
	}
	if (func->m_heap != NULL) {
		delete func->m_heap;
		func->m_heap = NULL;
	}
	func->SetStopped();
	if (func->m_listener != NULL)
		func->m_listener->OnPluginUnloaded();
}

/* (0x8114007A) */
int ExclusiveFuncIme::Stop()
{
	if (Plugin::Find("ime_plugin") != NULL)
		Plugin::UnloadAsync("ime_plugin", NULL, 1);
	else
		exclusiveFuncImeOnUnloaded(this);
	return 0;
}

/* (0x811400AC) the plugin loaded: the function started and the server told */
static void exclusiveFuncImeOnLoaded(Plugin *plugin)
{
	ExclusiveFuncIme *func = (ExclusiveFuncIme *)(int32_t)plugin->GetArg("exfunc_this", NULL);

	func->SetStarted();
	if (func->m_listener != NULL)
		func->m_listener->OnPluginLoaded(plugin);
}
