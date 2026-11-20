/*
 * exclusive_func_net.cpp: the network's exclusive function (0x810E26CC-0x810E2F4A): while it runs,
 * impose_net_plugin is loaded into the manager's block, its surface pool and private heap in the
 * first 0xFA000 bytes and the rest of the block its own; the caller's callback gets the plugin's
 * result once the plugin is unloaded again. A power event aborts it. The file's and the names
 * not quoted are ours (PLACEHOLDER), after Sony's "ExclusiveFuncMtp".
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

/* the block's use: the plugin's surface pool, its private heap, and its own memory after them */
#define EXCLUSIVE_FUNC_NET_SURFACE_POOL_SIZE	0xAF000
#define EXCLUSIVE_FUNC_NET_HEAP_SIZE		0x4B000
#define EXCLUSIVE_FUNC_NET_MEM_SIZE		(EXCLUSIVE_FUNC_NET_SURFACE_POOL_SIZE + \
						 EXCLUSIVE_FUNC_NET_HEAP_SIZE)

/* impose_net_plugin's functions (GetInterface(1); PLACEHOLDER names) */
enum {
	IMPOSE_NET_PLUGIN_FUNC_START	= 0,	/* the function called with its result */
	IMPOSE_NET_PLUGIN_FUNC_ABORT	= 1,
};

/* the plugin's module and load options */
#define EXCLUSIVE_FUNC_NET_MODULE_INTERFACE_VERSION	1
#define EXCLUSIVE_FUNC_NET_MODULE_OPTION		1
#define EXCLUSIVE_FUNC_NET_OPTION			0x1C

/* the result's error when a power event aborted it (PLACEHOLDER name) */
#define EXCLUSIVE_FUNC_NET_ERROR_ABORTED	((int)0x80102408)

/* PLACEHOLDER: what impose_net_plugin gives back (12 bytes) */
struct ExclusiveFuncNetResult {
	int data[3];
};

typedef void (*ExclusiveFuncNetCallback)(ExclusiveFuncNetResult result, int error, void *arg);

/*
 * PLACEHOLDER: the network's exclusive function (48 bytes): the caller's callback, its argument
 * and the plugin's mode, the plugin's surface pool and heap while it is loaded, and its result.
 */
class ExclusiveFuncNet : public ExclusiveFunc {
public:
	ExclusiveFuncNet(ExclusiveFuncNetCallback callback, void *arg, int mode)
		: m_callback(callback), m_arg(arg), m_mode(mode), m_error(0),
		  m_surfacePool(NULL), m_heap(NULL), m_aborted(false)
	{
		sce_paf_memset(&m_result, 0, sizeof(m_result));
	}

	virtual ~ExclusiveFuncNet();

	virtual int Start(const ExclusiveFuncMemBlock *mem);
	virtual int Stop();
	virtual int Notify(int events);

	ExclusiveFuncNetCallback m_callback;
	void *m_arg;
	int m_mode;			/* the plugin's exec_mode */
	int m_error;
	graph::SurfacePool *m_surfacePool;
	memory::HeapAllocator *m_heap;
	ExclusiveFuncNetResult m_result;
	bool m_aborted;
};

static ExclusiveFuncPtr s_netFunc;

/* (0x810E26CC) */
int shellExclusiveFuncNetBegin(int mode, void *callback, void *arg)
{
	if (callback == NULL)
		return -1;

	s_netFunc.reset();

	ExclusiveFuncMgr *mgr = ExclusiveFuncMgr::GetInstance();

	if (mgr == NULL)
		return -1;

	s_netFunc = common::SharedPtr<ExclusiveFuncNet>(
		new ExclusiveFuncNet((ExclusiveFuncNetCallback)callback, arg, mode));
	if (s_netFunc.get() == NULL)
		return -1;
	mgr->Request(&s_netFunc);
	return 0;
}

/* (0x810E290A) */
int shellExclusiveFuncNetEnd(int mode)
{
	ExclusiveFuncMgr *mgr = ExclusiveFuncMgr::GetInstance();

	if (s_netFunc.get() != NULL && mgr != NULL) {
		mgr->Cancel(&s_netFunc);
		return 0;
	}
	s_netFunc.reset();
	return -1;
}

/* (0x810E2ED2) the plugin's result: the plugin unloaded */
static void exclusiveFuncNetOnResult(ExclusiveFuncNetResult result, int error, void *arg)
{
	ExclusiveFuncNet *func = (ExclusiveFuncNet *)arg;

	if (func != NULL) {
		if (func->m_aborted)
			error = EXCLUSIVE_FUNC_NET_ERROR_ABORTED;
		func->m_error = error;
		func->m_result = result;
	}
	Plugin::UnloadAsync("impose_net_plugin", NULL, 1);
}

/* (0x810E2D6A) the plugin loaded: started with the function to call with its result */
static void exclusiveFuncNetOnLoaded(Plugin *plugin)
{
	ExclusiveFuncNet *func = (ExclusiveFuncNet *)(int32_t)plugin->GetArg("userdata", NULL);
	Plugin *net = Plugin::Find("impose_net_plugin");

	if (net != NULL) {
		ShellPluginInterface *iface = (ShellPluginInterface *)net->GetInterface(1);

		if (iface != NULL)
			((void (*)(void (*)(ExclusiveFuncNetResult, int, void *), void *))
			 iface->func[IMPOSE_NET_PLUGIN_FUNC_START])(exclusiveFuncNetOnResult, func);
	}
	func->SetStarted();
}

/* (0x810E2DF6) the plugin unloaded: its memory freed, the function stopped and the caller told */
static void exclusiveFuncNetOnUnloaded(void *arg)
{
	ExclusiveFuncNet *func = (ExclusiveFuncNet *)arg;

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
	func->m_callback(func->m_result, func->m_error, func->m_arg);
	s_netFunc = ExclusiveFuncPtr();
}

/* (0x810E299E, 0x810E29BA) */
ExclusiveFuncNet::~ExclusiveFuncNet()
{
}

/* (0x810E29DA) */
int ExclusiveFuncNet::Start(const ExclusiveFuncMemBlock *mem)
{
	char *base = (char *)mem->base;

	if (base == NULL || mem->size < EXCLUSIVE_FUNC_NET_MEM_SIZE)
		return -1;

	memory::HeapAllocator::Option option;

	option.disable_dbgprint = false;
	option.entry_max = 0;
	option.warning_size = 0;
	option.disable_assert_on_alloc_failure = true;
	option.memblk_type = memory::AllocationBlockType_User;
	option.alignment = 0;

	graph::SurfacePool *pool = new graph::SurfacePool(base, EXCLUSIVE_FUNC_NET_SURFACE_POOL_SIZE,
							  "impose_net_plugin", &option);
	memory::HeapAllocator *heap = new memory::HeapAllocator(
		base + EXCLUSIVE_FUNC_NET_SURFACE_POOL_SIZE, EXCLUSIVE_FUNC_NET_HEAP_SIZE,
		"SceImposeNetPluginPrivateHeap");
	Plugin::InitParam param;

	param.name = "impose_net_plugin";
	param.resource_file = "vs0:vsh/shell/impose_net_plugin.rco";
	param.caller_name = "__main__";
	param.module_file = "vs0:vsh/shell/impose_net_plugin.suprx";
	param.module_interface_version = EXCLUSIVE_FUNC_NET_MODULE_INTERFACE_VERSION;
	param.module_option = EXCLUSIVE_FUNC_NET_MODULE_OPTION;
	param.option = EXCLUSIVE_FUNC_NET_OPTION;
	param.unload_callback = exclusiveFuncNetOnUnloaded;
	param.arguments[string("userdata")] = Plugin::Argument((int32_t)this);
	param.arguments[string("memblockaddr")] =
		Plugin::Argument((int32_t)(base + EXCLUSIVE_FUNC_NET_MEM_SIZE));
	param.arguments[string("exec_mode")] = Plugin::Argument(m_mode);
	param.param = this;
	param.surface_pool = pool;
	param.private_heap_size = 0;
	param.private_heap = heap;
	m_surfacePool = pool;
	m_heap = heap;
	Plugin::LoadAsync(param, exclusiveFuncNetOnLoaded, 1);
	return 0;
}

/* (0x810E2D14) */
int ExclusiveFuncNet::Stop()
{
	Plugin *net = Plugin::Find("impose_net_plugin");

	if (net != NULL) {
		ShellPluginInterface *iface = (ShellPluginInterface *)net->GetInterface(1);

		if (iface != NULL)
			iface->func[IMPOSE_NET_PLUGIN_FUNC_ABORT]();
	}
	return 0;
}

/* (0x810E2D38) a power event aborts it */
int ExclusiveFuncNet::Notify(int events)
{
	if ((events & EXCLUSIVE_FUNC_EVENT_POWER) != 0) {
		Plugin *net = Plugin::Find("impose_net_plugin");

		if (net != NULL) {
			ShellPluginInterface *iface = (ShellPluginInterface *)net->GetInterface(1);

			if (iface != NULL) {
				m_aborted = true;
				iface->func[IMPOSE_NET_PLUGIN_FUNC_ABORT]();
			}
		}
	}
	return 0;
}
