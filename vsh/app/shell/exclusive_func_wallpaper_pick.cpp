/*
 * exclusive_func_wallpaper_pick.cpp: the wallpaper picker's exclusive function
 * (0x810E439E-0x810E4B28): while it runs, theme_plugin, which the shell carries, is loaded into
 * the manager's block on the wallpaper given, and the wallpaper picked is given to the caller once
 * the plugin is unloaded again. The file's and the names not quoted are ours (PLACEHOLDER), after
 * Sony's "ExclusiveFuncPhotoImport".
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <std/string.h>
#include <common/shared_ptr.h>
#include <memory/heap_allocator.h>
#include <graphics/surface/surface_pool.h>
#include <plugin.h>
#include "shell.h"
#include "plugins.h"
#include "exclusive_func.h"

using namespace paf;

/* the block's use: theme_plugin's surface pool and private heap; its memblockaddr argument */
#define EXCLUSIVE_FUNC_WALLPAPER_PICK_MEM_SIZE		0x380000
#define EXCLUSIVE_FUNC_WALLPAPER_PICK_SURFACE_POOL_SIZE	0x300000
#define EXCLUSIVE_FUNC_WALLPAPER_PICK_HEAP_SIZE		0x80000
#define EXCLUSIVE_FUNC_WALLPAPER_PICK_MEM_BLOCK_OFFSET	0x680000	/* PLACEHOLDER: past the block */

/* the plugin's load options */
#define EXCLUSIVE_FUNC_WALLPAPER_PICK_MODULE_INTERFACE_VERSION	1
#define EXCLUSIVE_FUNC_WALLPAPER_PICK_GRAPHICS_FLAG		7

/* the result before the plugin gives one: none picked */
#define EXCLUSIVE_FUNC_WALLPAPER_PICK_NONE	(-1)

/*
 * PLACEHOLDER: the wallpaper picker's exclusive function (40 bytes): the wallpaper to open on, the
 * caller's callbacks and their argument, the plugin's heap, surface pool and memory block address
 * while it is loaded, and the wallpaper picked.
 */
class ExclusiveFuncWallpaperPick : public ExclusiveFunc {
public:
	ExclusiveFuncWallpaperPick(int index, ExclusiveFuncWallpaperPickLoadedCallback loaded,
				   ExclusiveFuncWallpaperPickCallback callback, void *arg)
		: ExclusiveFunc(0, false), m_index(index), m_loaded(loaded), m_callback(callback),
		  m_arg(arg), m_heap(NULL), m_surfacePool(NULL), m_memBlockAddr(NULL),
		  m_result(EXCLUSIVE_FUNC_WALLPAPER_PICK_NONE)
	{
	}

	virtual ~ExclusiveFuncWallpaperPick();

	virtual int Start(const ExclusiveFuncMemBlock *mem);
	virtual int Stop();

	int Setup(const ExclusiveFuncMemBlock *mem);
	void Finish();
	void SetResult(int result);

	int m_index;
	ExclusiveFuncWallpaperPickLoadedCallback m_loaded;
	ExclusiveFuncWallpaperPickCallback m_callback;
	void *m_arg;
	memory::HeapAllocator *m_heap;
	graph::SurfacePool *m_surfacePool;
	char *m_memBlockAddr;
	int m_result;
};

static ExclusiveFuncPtr s_wallpaperPickFunc;

/* (0x810E439E) */
int StartExclusiveFuncWallpaperPick(int index, ExclusiveFuncWallpaperPickLoadedCallback loaded,
				    ExclusiveFuncWallpaperPickCallback callback, void *arg)
{
	s_wallpaperPickFunc.reset();

	ExclusiveFuncMgr *mgr = ExclusiveFuncMgr::GetInstance();

	if (mgr == NULL)
		return -1;

	s_wallpaperPickFunc = common::SharedPtr<ExclusiveFuncWallpaperPick>(
		new ExclusiveFuncWallpaperPick(index, loaded, callback, arg));
	if (s_wallpaperPickFunc.get() == NULL)
		return -1;
	mgr->Request(&s_wallpaperPickFunc);
	return 0;
}

/* (0x810E45E0) */
int AbortExclusiveFuncWallpaperPick(void)
{
	ExclusiveFuncMgr *mgr = ExclusiveFuncMgr::GetInstance();

	if (s_wallpaperPickFunc.get() != NULL && mgr != NULL) {
		mgr->Cancel(&s_wallpaperPickFunc);
		return 0;
	}
	s_wallpaperPickFunc.reset();
	return -1;
}

/* (0x810E4676, 0x810E4692) */
ExclusiveFuncWallpaperPick::~ExclusiveFuncWallpaperPick()
{
}

/* (0x810E46B4) */
__attribute__((noinline)) int ExclusiveFuncWallpaperPick::Setup(const ExclusiveFuncMemBlock *mem)
{
	char *base = (char *)mem->base;

	if (base == NULL || mem->size < EXCLUSIVE_FUNC_WALLPAPER_PICK_MEM_SIZE)
		return -1;

	memory::HeapAllocator::Option option;

	m_memBlockAddr = base + EXCLUSIVE_FUNC_WALLPAPER_PICK_MEM_BLOCK_OFFSET;
	option.disable_dbgprint = false;
	option.entry_max = 0;
	option.warning_size = 0;
	option.disable_assert_on_alloc_failure = true;
	option.memblk_type = memory::AllocationBlockType_User;
	option.alignment = 0;
	m_surfacePool = new graph::SurfacePool(base, EXCLUSIVE_FUNC_WALLPAPER_PICK_SURFACE_POOL_SIZE,
					       "theme_plugin", &option);
	if (m_surfacePool == NULL)
		return -1;
	m_heap = new memory::HeapAllocator(base + EXCLUSIVE_FUNC_WALLPAPER_PICK_SURFACE_POOL_SIZE,
					   EXCLUSIVE_FUNC_WALLPAPER_PICK_HEAP_SIZE,
					   "SceThemePluginPrivateHeap");
	if (m_heap != NULL)
		return 0;
	m_memBlockAddr = NULL;
	if (m_surfacePool != NULL) {
		delete m_surfacePool;
		m_surfacePool = NULL;
	}
	return -1;
}

static void exclusiveFuncWallpaperPickOnLoaded(Plugin *plugin);
static void exclusiveFuncWallpaperPickOnUnloaded(void *arg);
static void exclusiveFuncWallpaperPickOnResult(int32_t result, void *arg);

/* (0x810E479C) */
int ExclusiveFuncWallpaperPick::Start(const ExclusiveFuncMemBlock *mem)
{
	int ret = Setup(mem);

	if (ret != 0)
		return ret;

	Plugin::InitParam param;

	param.name = "theme_plugin";
	param.resource_file = "vs0:vsh/shell/theme_plugin.rco";
	param.caller_name = "__main__";
	param.module_interface_version = EXCLUSIVE_FUNC_WALLPAPER_PICK_MODULE_INTERFACE_VERSION;
	param.private_heap = m_heap;
	param.surface_pool = m_surfacePool;
	param.arguments[string("memblockaddr")] = Plugin::Argument((int32_t)m_memBlockAddr);
	param.arguments[string("select_idx")] = Plugin::Argument(m_index);
	param.notify_callback = exclusiveFuncWallpaperPickOnResult;
	param.unload_callback = exclusiveFuncWallpaperPickOnUnloaded;
	param.set_param_func = themePluginSetParam;
	param.init_func = themePluginInit;
	param.start_func = themePluginStart;
	param.param = this;
	param.stop_func = themePluginStop;
	param.exit_func = themePluginExit;
	param.graphics_flag = EXCLUSIVE_FUNC_WALLPAPER_PICK_GRAPHICS_FLAG;
	Plugin::LoadAsync(param, exclusiveFuncWallpaperPickOnLoaded, 0);
	return 0;
}

/* (0x810E49FC) */
int ExclusiveFuncWallpaperPick::Stop()
{
	Plugin::UnloadAsync("theme_plugin", NULL, 0);
	return 0;
}

/* (0x810E4A12) the plugin loaded: the function started, the caller told */
static void exclusiveFuncWallpaperPickOnLoaded(Plugin *plugin)
{
	ExclusiveFuncWallpaperPick *func = (ExclusiveFuncWallpaperPick *)plugin->m_param;

	if (func == NULL)
		return;

	func->SetStarted();
	if (func->m_loaded != NULL)
		func->m_loaded(plugin, func->m_arg);
}

/* (0x810E4A30) the plugin unloaded: its memory freed, the function stopped, the caller told */
void ExclusiveFuncWallpaperPick::Finish()
{
	m_memBlockAddr = NULL;
	if (m_heap != NULL) {
		delete m_heap;
		m_heap = NULL;
	}
	if (m_surfacePool != NULL) {
		delete m_surfacePool;
		m_surfacePool = NULL;
	}
	SetStopped();
	if (s_wallpaperPickFunc.get() != NULL)
		s_wallpaperPickFunc.reset();
	if (m_callback != NULL)
		m_callback(m_result, m_arg);
}

/* (0x810E4AEA) */
static void exclusiveFuncWallpaperPickOnUnloaded(void *arg)
{
	if (arg != NULL)
		((ExclusiveFuncWallpaperPick *)arg)->Finish();
}

/* (0x810E4AF4) */
void ExclusiveFuncWallpaperPick::SetResult(int result)
{
	m_result = result;
}

/* (0x810E4AF8) the plugin's result: the wallpaper picked */
static void exclusiveFuncWallpaperPickOnResult(int32_t result, void *arg)
{
	if (arg != NULL)
		((ExclusiveFuncWallpaperPick *)arg)->SetResult(result);
}
