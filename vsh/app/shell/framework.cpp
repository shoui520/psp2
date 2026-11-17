/*
 * The shell's paf framework, made and run, the icon cache, and the services the boot starts
 * through a call of their own (IME, liblocation). The file's and every name here are ours (no
 * record names them).
 */
#include <stddef.h>
#include <scetypes.h>
#include <framework.h>
#include <timer/timer.h>
#include <system/system.h>
#include <graphics/graphmem/graphmem.h>
#include <graphics/surface/surface_pool.h>
#include <graphics/drawobj/draw_env.h>
#include <memory/range_allocator.h>
#include <widget/core/environment.h>
#include "shell.h"
#include "src/livespace_db/lsdb_keys.h"

using namespace paf;

/* the share of the display's refresh the framework's frame takes */
#define SHELL_FRAME_RATIO	0.8f

/* the icon cache's video memory */
#define SHELL_ICON_CACHE_SIZE	0x200000

Framework *g_shellFramework;

/* the icon cache: its memory and the surface pool made in it */
struct ShellIconCache {
	void *memory;
	graph::SurfacePool *surfacePool;
};

static ShellIconCache *s_iconCache;

void shellFrameworkCreate(void)
{
	Framework::InitParam param;

	param.allow_button_control = true;
	param.mode = Framework::Mode_Shell;
	param.option = 2;
	param.surface_pool_size = 0x40000;
	param.graph_heap_size_on_video_memory = 0x100000;
	param.text_surface_pool_size = 0x100000;
	param.decode_heap_size = 0x100000;
	param.plugin_heap_size = 0x80000;
	param.graph_heap_size_on_main_memory = 0;

	ui::Resolution resolution(1);

	param.env_param = &resolution;
	g_shellFramework = new Framework(param);
	ScePafGraphics_505CD74E(false);
	ScePafMisc_F8D63B8E(SHELL_FRAME_RATIO);
	shellExclusiveFuncMgrCreate();
	shellAppEventServerCreate();
	shellEventServerCreate();
}

int shellFrameworkRun(void)
{
	return g_shellFramework->Run();
}

void shellIconCacheInit(void)
{
	ShellIconCache *cache = new ShellIconCache;

	if (cache == NULL)
		return;
	s_iconCache = cache;
	cache->memory = GraphMem::AllocOSMemory(
		system::Model::GetType() != 1 ? GraphMem::DeviceType_VideoMemory
					      : GraphMem::DeviceType_SystemMemory,
		SHELL_ICON_CACHE_SIZE, "SceShellIconCache", NULL);

	graph::SurfacePool::Option option;
	memory::RangeAllocator::Option rangeOption;

	option.pool_addr = NULL;
	option.pool_name = NULL;
	option.heap_allocator_option = NULL;
	option.fast_allocator_option = NULL;
	rangeOption.disable_dbgprint = false;
	option.type = graph::SurfacePool::MemoryType_Normal;
	option.pool_addr = cache->memory;
	rangeOption.disable_assert_on_alloc_failure = true;
	rangeOption.memblk_type = (memory::AllocationBlockType)0;
	option.pool_size = SHELL_ICON_CACHE_SIZE;
	option.pool_name = "SceShellIconSruface";
	option.fast_allocator_option = &rangeOption;
	cache->surfacePool = new graph::SurfacePool(&option);
}

graph::SurfacePool *shellIconCacheGetSurfacePool(void)
{
	return s_iconCache->surfacePool;
}

int shellImeServiceStart(void)
{
	return shellImeServerCreate();
}

int shellLiblocationStart(void)
{
	return liblocationInit();
}

int shellLiblocationWaitReady(void)
{
	return liblocationWaitReady();
}
