/*
 * exclusive_func_tel_init.cpp: the telephony setup's exclusive function (0x810E396A-0x810E439E).
 * Where telephony_plugin's modem asks for it, ime_plugin and then tel_initial_check_plugin are
 * loaded into the manager's block, each by a job of the loader queue; when the check unloads
 * itself both are unloaded and the caller's callback is called, and the function stops once
 * ime_plugin is gone. The strings name the parts ("SceTelInitPluginLoader",
 * "SceTelInitSurfacePool"); the file's and the other names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <std/string.h>
#include <common/shared_ptr.h>
#include <memory/heap_allocator.h>
#include <graphics/surface/surface_pool.h>
#include <job/job.h>
#include <plugin.h>
#include "shell.h"
#include "plugins.h"
#include "exclusive_func.h"

using namespace paf;

/* the block's use: ime_plugin's surface pool and heap, then the check's at 2 MiB */
#define EXCLUSIVE_FUNC_TEL_INIT_IME_SURFACE_POOL_SIZE	0x12C000
#define EXCLUSIVE_FUNC_TEL_INIT_IME_HEAP_SIZE		0xD4000
#define EXCLUSIVE_FUNC_TEL_INIT_CHECK_OFFSET		0x200000
#define EXCLUSIVE_FUNC_TEL_INIT_CHECK_SURFACE_POOL_SIZE	0x80000
#define EXCLUSIVE_FUNC_TEL_INIT_CHECK_HEAP_SIZE		0x40000

/* the function's kind (PLACEHOLDER: the meaning of 1) */
#define EXCLUSIVE_FUNC_TEL_INIT_TYPE	1

/* the plugins' load options */
#define EXCLUSIVE_FUNC_TEL_INIT_MODULE_INTERFACE_VERSION	1
#define EXCLUSIVE_FUNC_TEL_INIT_MODULE_OPTION		1
#define EXCLUSIVE_FUNC_TEL_INIT_CHECK_DRAW_PRIORITY	40

typedef void (*ExclusiveFuncTelInitCallback)(void *arg);

/*
 * PLACEHOLDER: the telephony setup's exclusive function (32 bytes): the caller's callback and its
 * argument, and the surface pools and heaps of ime_plugin and of the check while they are loaded.
 */
class ExclusiveFuncTelInit : public ExclusiveFunc {
public:
	ExclusiveFuncTelInit(ExclusiveFuncTelInitCallback callback, void *arg)
		: ExclusiveFunc(EXCLUSIVE_FUNC_TEL_INIT_TYPE, true), m_callback(callback), m_arg(arg),
		  m_imeSurfacePool(NULL), m_imeHeap(NULL), m_checkSurfacePool(NULL),
		  m_checkHeap(NULL)
	{
	}

	virtual ~ExclusiveFuncTelInit();

	virtual int Start(const ExclusiveFuncMemBlock *mem);
	virtual int Stop();

	void Setup(const ExclusiveFuncMemBlock *mem);
	void StartInitialCheck();
	void Finish();
	void LoadInitialCheck();
	void LoadIme();

	ExclusiveFuncTelInitCallback m_callback;
	void *m_arg;
	graph::SurfacePool *m_imeSurfacePool;
	memory::HeapAllocator *m_imeHeap;
	graph::SurfacePool *m_checkSurfacePool;
	memory::HeapAllocator *m_checkHeap;
};

/* PLACEHOLDER: the job loading tel_initial_check_plugin */
class ExclusiveFuncTelInitCheckJob : public job::JobItem {
public:
	ExclusiveFuncTelInitCheckJob(ExclusiveFuncTelInit *func)
		: job::JobItem("SceTelInitPluginLoader", NULL), m_func(func)
	{
	}

	virtual ~ExclusiveFuncTelInitCheckJob()
	{
	}

	virtual int Run()
	{
		m_func->LoadInitialCheck();
		return 0;
	}

	virtual void Finish(int result)
	{
	}

	ExclusiveFuncTelInit *m_func;
};

/* PLACEHOLDER: the job loading ime_plugin */
class ExclusiveFuncTelInitImeJob : public job::JobItem {
public:
	ExclusiveFuncTelInitImeJob(ExclusiveFuncTelInit *func)
		: job::JobItem("SceTelInitPluginLoader", NULL), m_func(func)
	{
	}

	virtual ~ExclusiveFuncTelInitImeJob()
	{
	}

	virtual int Run()
	{
		m_func->LoadIme();
		return 0;
	}

	virtual void Finish(int result)
	{
	}

	ExclusiveFuncTelInit *m_func;
};

static ExclusiveFuncPtr s_telInitFunc;

/* (0x810E396A) */
int shellTelephonyStartSetup(void (*callback)(void *arg), void *arg)
{
	Plugin *telephony = Plugin::Find("telephony_plugin");

	if (telephony != NULL) {
		TelephonyPluginInterface *iface =
			(TelephonyPluginInterface *)telephony->GetInterface(1);

		if (iface != NULL) {
			TelephonyModem *modem = iface->getModem();

			if (modem != NULL && modem->vtable->needsInitialCheck(modem) == 0)
				return -1;
		}
	}

	s_telInitFunc.reset();

	ExclusiveFuncMgr *mgr = ExclusiveFuncMgr::GetInstance();

	if (mgr == NULL)
		return -1;

	s_telInitFunc = common::SharedPtr<ExclusiveFuncTelInit>(
		new ExclusiveFuncTelInit(callback, arg));
	if (s_telInitFunc.get() == NULL)
		return -1;
	return mgr->Request(&s_telInitFunc);
}

/* (0x810E3BB8, 0x810E3BD4) */
ExclusiveFuncTelInit::~ExclusiveFuncTelInit()
{
}

/* (0x810E3BF4) the block divided between the plugins' surface pools and heaps */
__attribute__((noinline)) void ExclusiveFuncTelInit::Setup(const ExclusiveFuncMemBlock *mem)
{
	char *base = (char *)mem->base;
	memory::HeapAllocator::Option option;

	option.disable_dbgprint = false;
	option.entry_max = 0;
	option.warning_size = 0;
	option.disable_assert_on_alloc_failure = true;
	option.memblk_type = memory::AllocationBlockType_User;
	option.alignment = 0;
	m_imeSurfacePool = new graph::SurfacePool(base, EXCLUSIVE_FUNC_TEL_INIT_IME_SURFACE_POOL_SIZE,
						  "SceImeSurfacePool", &option);
	m_imeHeap = new memory::HeapAllocator(base + EXCLUSIVE_FUNC_TEL_INIT_IME_SURFACE_POOL_SIZE,
					      EXCLUSIVE_FUNC_TEL_INIT_IME_HEAP_SIZE, "SceImePrivateHeap");

	char *check = (char *)mem->base + EXCLUSIVE_FUNC_TEL_INIT_CHECK_OFFSET;
	memory::HeapAllocator::Option checkOption;

	checkOption.disable_dbgprint = false;
	checkOption.entry_max = 0;
	checkOption.warning_size = 0;
	checkOption.disable_assert_on_alloc_failure = true;
	checkOption.memblk_type = memory::AllocationBlockType_User;
	checkOption.alignment = 0;
	m_checkSurfacePool = new graph::SurfacePool(check,
						    EXCLUSIVE_FUNC_TEL_INIT_CHECK_SURFACE_POOL_SIZE,
						    "SceTelInitSurfacePool", &checkOption);
	m_checkHeap = new memory::HeapAllocator(check + EXCLUSIVE_FUNC_TEL_INIT_CHECK_SURFACE_POOL_SIZE,
						EXCLUSIVE_FUNC_TEL_INIT_CHECK_HEAP_SIZE,
						"SceTelInitPrivateHeap");
}

/* (0x810E3D16) ime_plugin loaded first */
int ExclusiveFuncTelInit::Start(const ExclusiveFuncMemBlock *mem)
{
	Setup(mem);

	common::SharedPtr<job::JobItem> job(new ExclusiveFuncTelInitImeJob(this));

	job::JobQueue::default_queue->Enqueue(job);
	return 0;
}

/* (0x810E3E04) both plugins unloaded, the caller told */
int ExclusiveFuncTelInit::Stop()
{
	Plugin::UnloadAsync("tel_initial_check_plugin", NULL, 1);
	Plugin::UnloadAsync("ime_plugin", NULL, 1);
	if (m_callback != NULL)
		m_callback(m_arg);
	return 0;
}

/* (0x810E3E38) ime_plugin loaded: the check loaded next */
void ExclusiveFuncTelInit::StartInitialCheck()
{
	common::SharedPtr<job::JobItem> job(new ExclusiveFuncTelInitCheckJob(this));

	job::JobQueue::default_queue->Enqueue(job);
}

/* (0x810E3F20) ime_plugin unloaded: the memory freed, the function stopped */
void ExclusiveFuncTelInit::Finish()
{
	if (m_imeSurfacePool != NULL)
		delete m_imeSurfacePool;
	m_imeSurfacePool = NULL;
	if (m_imeHeap != NULL)
		delete m_imeHeap;
	m_imeHeap = NULL;
	if (m_checkSurfacePool != NULL)
		delete m_checkSurfacePool;
	m_checkSurfacePool = NULL;
	if (m_checkHeap != NULL)
		delete m_checkHeap;
	m_checkHeap = NULL;
	s_telInitFunc.reset();
	SetStopped();
}

/* (0x810E3FE2) the check loaded: the function started */
static void exclusiveFuncTelInitOnCheckLoaded(Plugin *plugin)
{
	ExclusiveFuncTelInit *func = (ExclusiveFuncTelInit *)plugin->m_param;

	if (func != NULL)
		func->SetStarted();
}

/* (0x810E3FF0) */
static void exclusiveFuncTelInitOnImeLoaded(Plugin *plugin)
{
	ExclusiveFuncTelInit *func = (ExclusiveFuncTelInit *)plugin->m_param;

	if (func != NULL)
		func->StartInitialCheck();
}

/* (0x810E4000) */
static void exclusiveFuncTelInitOnImeUnloaded(void *arg)
{
	if (arg != NULL)
		((ExclusiveFuncTelInit *)arg)->Finish();
}

/* (0x814120B0, folded with a deleter) the check unloaded itself: the function stops */
static void exclusiveFuncTelInitOnCheckUnloaded(void *arg)
{
	if (arg != NULL)
		((ExclusiveFunc *)arg)->Stop();
}

/* (0x810E400C) */
void ExclusiveFuncTelInit::LoadInitialCheck()
{
	Plugin::InitParam param;

	param.name = "tel_initial_check_plugin";
	param.resource_file = "vs0:vsh/shell/telephony/initial_check/tel_initial_check_plugin.rco";
	param.caller_name = "__main__";
	param.module_file = "vs0:vsh/shell/telephony/initial_check/tel_initial_check_plugin.suprx";
	param.module_interface_version = EXCLUSIVE_FUNC_TEL_INIT_MODULE_INTERFACE_VERSION;
	param.module_option = EXCLUSIVE_FUNC_TEL_INIT_MODULE_OPTION;
	param.surface_pool = m_checkSurfacePool;
	param.unload_callback = exclusiveFuncTelInitOnCheckUnloaded;
	param.param = this;
	param.draw_priority = EXCLUSIVE_FUNC_TEL_INIT_CHECK_DRAW_PRIORITY;
	param.private_heap_size = 0;
	param.private_heap = m_checkHeap;
	Plugin::LoadAsync(param, exclusiveFuncTelInitOnCheckLoaded, 1);
}

/* (0x810E4140) */
void ExclusiveFuncTelInit::LoadIme()
{
	Plugin::InitParam param;

	param.name = "ime_plugin";
	param.resource_file = "vs0:vsh/shell/ime_plugin.rco";
	param.caller_name = "__main__";
	param.module_file = "vs0:vsh/shell/ime_plugin.suprx";
	param.param = this;
	param.module_interface_version = EXCLUSIVE_FUNC_TEL_INIT_MODULE_INTERFACE_VERSION;
	param.module_option = EXCLUSIVE_FUNC_TEL_INIT_MODULE_OPTION;
	param.option = Plugin::Option_WaitWidgetActuallyDestroyed;
	param.arguments[string("small_memory")] = Plugin::Argument(1);
	param.arguments[string("userdata")] = Plugin::Argument((int32_t)this);
	param.surface_pool = m_imeSurfacePool;
	param.private_heap_size = 0;
	param.private_heap = m_imeHeap;
	param.unload_callback = exclusiveFuncTelInitOnImeUnloaded;
	Plugin::LoadAsync(param, exclusiveFuncTelInitOnImeLoaded, 1);
}
