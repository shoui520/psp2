/*
 * exclusive_func_auth_reset.cpp: auth_reset_plugin exclusive function loader (passcode lockout).
 * 3.65-CEX: 0x813C45A4 - 0x813C4C67
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

#define EXCLUSIVE_FUNC_AUTH_RESET_MEM_SIZE		0x180000u
#define EXCLUSIVE_FUNC_AUTH_RESET_SURFACE_POOL_SIZE	0x100000u
#define EXCLUSIVE_FUNC_AUTH_RESET_HEAP_SIZE		0x80000u
#define EXCLUSIVE_FUNC_AUTH_RESET_MODULE_IFACE_VER	1
#define EXCLUSIVE_FUNC_AUTH_RESET_DRAW_PRIORITY		27

typedef void (*AuthResetLoadedCallback)(Plugin *plugin, void *userData);
typedef void (*AuthResetUnloadedCallback)(void *userData);

class ExclusiveFuncAuthReset : public ExclusiveFunc {
public:
	ExclusiveFuncAuthReset(AuthResetLoadedCallback loadedCb,
			       AuthResetUnloadedCallback unloadedCb, void *userData)
		: ExclusiveFunc(0, false),
		  m_loadedCb(loadedCb),
		  m_unloadedCb(unloadedCb),
		  m_userData(userData),
		  m_heap(NULL),
		  m_surfacePool(NULL),
		  m_memBlockAddr(NULL)
	{
	}

	/* 0x813C4862 / 0x813C487E */
	virtual ~ExclusiveFuncAuthReset() {}

	virtual int Start(const ExclusiveFuncMemBlock *mem);	/* 0x813C4988 */
	virtual int Stop();					/* 0x813C4B4C */

	int Setup(const ExclusiveFuncMemBlock *mem);		/* 0x813C48A0 */
	void Finish();						/* 0x813C4B80 */

	AuthResetLoadedCallback m_loadedCb;
	AuthResetUnloadedCallback m_unloadedCb;
	void *m_userData;
	memory::HeapAllocator *m_heap;
	graph::SurfacePool *m_surfacePool;
	char *m_memBlockAddr;
};

static ExclusiveFuncPtr s_authResetFunc; /* 0x815CC3BC */

static void exclusiveFuncAuthResetOnLoaded(Plugin *plugin);
static void exclusiveFuncAuthResetOnUnloaded(void *userData);

/* 0x813C45A4 */
int shellExclusiveFuncAuthResetStart(AuthResetLoadedCallback loadedCb,
				     AuthResetUnloadedCallback unloadedCb, void *userData)
{
	s_authResetFunc.reset();

	ExclusiveFuncMgr *mgr = ExclusiveFuncMgr::GetInstance();
	if (mgr == NULL)
		return -1;

	s_authResetFunc = common::SharedPtr<ExclusiveFuncAuthReset>(
		new ExclusiveFuncAuthReset(loadedCb, unloadedCb, userData));
	if (s_authResetFunc.get() == NULL)
		return -1;

	mgr->Request(&s_authResetFunc);
	return 0;
}

/* 0x813C47CE */
int shellExclusiveFuncPasscodeLockoutStop(void)
{
	ExclusiveFuncMgr *mgr = ExclusiveFuncMgr::GetInstance();
	if (s_authResetFunc.get() != NULL && mgr != NULL) {
		mgr->Cancel(&s_authResetFunc);
		return 0;
	}
	s_authResetFunc.reset();
	return -1;
}

/* 0x813C48A0 */
__attribute__((noinline)) int ExclusiveFuncAuthReset::Setup(const ExclusiveFuncMemBlock *mem)
{
	char *base = static_cast<char *>(mem->base);
	if (base == NULL || mem->size < EXCLUSIVE_FUNC_AUTH_RESET_MEM_SIZE)
		return -1;

	m_memBlockAddr = base + EXCLUSIVE_FUNC_AUTH_RESET_MEM_SIZE;

	memory::HeapAllocator::Option poolOption;
	poolOption.entry_max = 0;
	poolOption.warning_size = 0;
	poolOption.disable_dbgprint = false;
	poolOption.disable_assert_on_alloc_failure = true;
	poolOption.memblk_type = memory::AllocationBlockType_User;
	poolOption.alignment = 0;

	m_surfacePool = new graph::SurfacePool(base, EXCLUSIVE_FUNC_AUTH_RESET_SURFACE_POOL_SIZE,
					       "auth_reset_plugin", &poolOption);
	if (m_surfacePool == NULL)
		return -1;

	m_heap = new memory::HeapAllocator(base + EXCLUSIVE_FUNC_AUTH_RESET_SURFACE_POOL_SIZE,
					   EXCLUSIVE_FUNC_AUTH_RESET_HEAP_SIZE,
					   "SceAuthResetPluginPrivateHeap");
	if (m_heap != NULL)
		return 0;

	m_memBlockAddr = NULL;
	if (m_surfacePool != NULL) {
		delete m_surfacePool;
		m_surfacePool = NULL;
	}
	return -1;
}

/* 0x813C4988 */
int ExclusiveFuncAuthReset::Start(const ExclusiveFuncMemBlock *mem)
{
	int ret = Setup(mem);
	if (ret != 0)
		return ret;

	Plugin::InitParam param;
	param.name = "auth_reset_plugin";
	param.resource_file = "vs0:vsh/shell/auth_reset_plugin.rco";
	param.caller_name = "__main__";
	param.module_file = "vs0:vsh/shell/auth_reset_plugin.suprx";
	param.module_interface_version = EXCLUSIVE_FUNC_AUTH_RESET_MODULE_IFACE_VER;
	param.module_option = 1;
	param.private_heap = m_heap;
	param.surface_pool = m_surfacePool;
	param.draw_priority = EXCLUSIVE_FUNC_AUTH_RESET_DRAW_PRIORITY;
	param.arguments[string("memblockaddr")] =
		Plugin::Argument(static_cast<int32_t>(reinterpret_cast<intptr_t>(m_memBlockAddr)));
	param.unload_callback = exclusiveFuncAuthResetOnUnloaded;
	param.param = this;

	Plugin::LoadAsync(param, exclusiveFuncAuthResetOnLoaded, 0);
	return 0;
}

/* 0x813C4B4C */
int ExclusiveFuncAuthReset::Stop()
{
	Plugin::UnloadAsync("auth_reset_plugin", NULL, 0);
	return 0;
}

/* 0x813C4B62 */
static void exclusiveFuncAuthResetOnLoaded(Plugin *plugin)
{
	ExclusiveFuncAuthReset *func = static_cast<ExclusiveFuncAuthReset *>(plugin->m_param);
	if (func == NULL)
		return;

	func->SetStarted();
	if (func->m_loadedCb != NULL)
		func->m_loadedCb(plugin, func->m_userData);
}

/* 0x813C4B80 */
void ExclusiveFuncAuthReset::Finish()
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
	if (s_authResetFunc.get() != NULL)
		s_authResetFunc.reset();
	if (m_unloadedCb != NULL)
		m_unloadedCb(m_userData);
}

/* 0x813C4C36 */
static void exclusiveFuncAuthResetOnUnloaded(void *userData)
{
	if (userData != NULL)
		static_cast<ExclusiveFuncAuthReset *>(userData)->Finish();
}
