/*
 * exclusive_func_location.cpp: location_plugin exclusive function loader.
 * 3.65-CEX: 0x813C4C68 - 0x813C541B
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <common/shared_ptr.h>
#include <memory/heap_allocator.h>
#include <graphics/surface/surface_pool.h>
#include <plugin.h>

#include "shell.h"
#include "exclusive_func.h"

using namespace paf;

#define EXCLUSIVE_FUNC_LOCATION_MEM_SIZE		0x14B000u
#define EXCLUSIVE_FUNC_LOCATION_SURFACE_POOL_SIZE	0x100000u
#define EXCLUSIVE_FUNC_LOCATION_HEAP_SIZE		0x4B000u
#define EXCLUSIVE_FUNC_LOCATION_MODULE_IFACE_VER	1

typedef void (*LocationPluginDoneFn)(int result, void *userData);
typedef void (*LocationPluginLoadedFn)(int pluginHandle, LocationPluginDoneFn doneCb,
				       void *userData);

struct LocationDialogPluginInterface {
	int (*openDialog)(void);
	int (*dismissDialog)(void);
};

class ExclusiveFuncLocation : public ExclusiveFunc {
public:
	ExclusiveFuncLocation(LocationPluginLoadedFn loadedCb, LocationPluginDoneFn doneCb,
			      void *userData)
		: ExclusiveFunc(1, true),
		  m_doneCb(doneCb),
		  m_loadedCb(loadedCb),
		  m_userData(userData),
		  m_dialogResult(0),
		  m_surfacePool(NULL),
		  m_heap(NULL)
	{
	}

	/* 0x813C4F2C / 0x813C4F48 */
	virtual ~ExclusiveFuncLocation() {}

	virtual int Start(const ExclusiveFuncMemBlock *mem);	/* 0x813C4F68 */
	virtual int Stop();					/* 0x813C522E */

	LocationPluginDoneFn m_doneCb;
	LocationPluginLoadedFn m_loadedCb;
	void *m_userData;
	int m_dialogResult;
	graph::SurfacePool *m_surfacePool;
	memory::HeapAllocator *m_heap;
};

static ExclusiveFuncPtr s_locationFunc; /* 0x815CC3C8 */

static void exclusiveFuncLocationOnLoaded(Plugin *plugin);
static void exclusiveFuncLocationOnUnloaded(void *userData);
static void exclusiveFuncLocationOnDialogDone(int result, void *userData);

/* 0x813C4C68 */
int locationPluginLoad(LocationPluginLoadedFn loadedCb, LocationPluginDoneFn doneCb,
		       void *userData)
{
	if (doneCb == NULL)
		return -1;

	s_locationFunc.reset();

	ExclusiveFuncMgr *mgr = ExclusiveFuncMgr::GetInstance();
	if (mgr == NULL)
		return -1;

	s_locationFunc = common::SharedPtr<ExclusiveFuncLocation>(
		new ExclusiveFuncLocation(loadedCb, doneCb, userData));
	if (s_locationFunc.get() == NULL)
		return -1;

	mgr->Request(&s_locationFunc);
	return 0;
}

/* 0x813C4E96 */
int locationPluginUnload(void)
{
	ExclusiveFuncMgr *mgr = ExclusiveFuncMgr::GetInstance();
	if (s_locationFunc.get() != NULL && mgr != NULL) {
		mgr->Cancel(&s_locationFunc);
		return 0;
	}
	s_locationFunc.reset();
	return -1;
}

/* 0x813C4F68 */
int ExclusiveFuncLocation::Start(const ExclusiveFuncMemBlock *mem)
{
	char *base = static_cast<char *>(mem->base);
	if (base == NULL || mem->size < EXCLUSIVE_FUNC_LOCATION_MEM_SIZE)
		return -1;

	memory::HeapAllocator::Option poolOption;
	poolOption.entry_max = 0;
	poolOption.warning_size = 0;
	poolOption.disable_dbgprint = false;
	poolOption.disable_assert_on_alloc_failure = true;
	poolOption.memblk_type = memory::AllocationBlockType_User;
	poolOption.alignment = 0;

	char *remainingBase = base + EXCLUSIVE_FUNC_LOCATION_MEM_SIZE;
	graph::SurfacePool *surfacePool = new graph::SurfacePool(
		base, EXCLUSIVE_FUNC_LOCATION_SURFACE_POOL_SIZE, "location_plugin", &poolOption);
	memory::HeapAllocator *heap = new memory::HeapAllocator(
		base + EXCLUSIVE_FUNC_LOCATION_SURFACE_POOL_SIZE,
		EXCLUSIVE_FUNC_LOCATION_HEAP_SIZE, "SceLocationPluginPrivateHeap");

	Plugin::InitParam param;
	param.name = "location_plugin";
	param.resource_file = "vs0:vsh/shell/location_plugin.rco";
	param.caller_name = "__main__";
	param.module_file = "vs0:vsh/shell/location_dialog_plugin.suprx";
	param.module_interface_version = EXCLUSIVE_FUNC_LOCATION_MODULE_IFACE_VER;
	param.module_option = 1;
	param.arguments[string("userdata")] =
		Plugin::Argument(static_cast<int32_t>(reinterpret_cast<intptr_t>(this)));
	param.arguments[string("memblockaddr")] =
		Plugin::Argument(static_cast<int32_t>(reinterpret_cast<intptr_t>(remainingBase)));
	param.unload_callback = exclusiveFuncLocationOnUnloaded;
	param.param = this;
	param.surface_pool = surfacePool;
	param.private_heap_size = 0;
	param.private_heap = heap;

	m_surfacePool = surfacePool;
	m_heap = heap;

	Plugin::LoadAsync(param, exclusiveFuncLocationOnLoaded, 0);
	return 0;
}

/* 0x813C522E */
int ExclusiveFuncLocation::Stop()
{
	Plugin *plugin = Plugin::Find("location_plugin");
	if (plugin != NULL) {
		LocationDialogPluginInterface *iface =
			static_cast<LocationDialogPluginInterface *>(plugin->GetInterface(1));
		if (iface != NULL)
			iface->dismissDialog();
	}
	return 0;
}

/* 0x813C5252 */
static void exclusiveFuncLocationOnLoaded(Plugin *plugin)
{
	int32_t rawUserData = static_cast<int32_t>(plugin->GetArg("userdata", NULL));
	ExclusiveFuncLocation *func =
		reinterpret_cast<ExclusiveFuncLocation *>(static_cast<intptr_t>(rawUserData));
	if (func != NULL)
		func->m_loadedCb(static_cast<int>(reinterpret_cast<intptr_t>(plugin)),
				 exclusiveFuncLocationOnDialogDone, func->m_userData);
	func->SetStarted();
}

/* 0x813C52C4 */
static void exclusiveFuncLocationOnUnloaded(void *userData)
{
	ExclusiveFuncLocation *func = static_cast<ExclusiveFuncLocation *>(userData);
	if (func == NULL)
		return;

	if (func->m_surfacePool != NULL) {
		delete func->m_surfacePool;
		func->m_surfacePool = NULL;
	}
	if (func->m_heap != NULL) {
		delete func->m_heap;
		func->m_heap = NULL;
	}
	func->SetStopped();
	func->m_doneCb(func->m_dialogResult, func->m_userData);
	if (s_locationFunc.get() != NULL)
		s_locationFunc.reset();
}

/* 0x813C5374 */
static void exclusiveFuncLocationOnDialogDone(int result, void *userData)
{
	Plugin *plugin = Plugin::Find("location_plugin");
	int32_t rawUserData = static_cast<int32_t>(plugin->GetArg("userdata", NULL));
	ExclusiveFuncLocation *func =
		reinterpret_cast<ExclusiveFuncLocation *>(static_cast<intptr_t>(rawUserData));
	if (func != NULL) {
		func->m_userData = userData;
		func->m_dialogResult = result;
	}
	Plugin::UnloadAsync("location_plugin", NULL, 0);
}
