/*
 * paf::Framework: the one instance that owns the widget environment, the
 * heaps, the plugins and the main loop.
 */
#ifndef PAF_FRAMEWORK_H
#define PAF_FRAMEWORK_H

#include <stdint.h>
#include <scetypes.h>
#include <std/vector>
#include <plugin.h>
#include <thread/thread.h>
#include <memory/heap_allocator.h>
#include <widget/core/environment.h>

namespace paf {

namespace graph {
class SurfacePool;
}

namespace sound {
class Output;
}

namespace ui {
class EnvironmentParam;
class Screen;
}

class Framework {
public:
	typedef void (*LoadCRFinishCallback)();
	typedef void (*UnloadCRFinishCallback)();
	typedef void (*ShutdownCallback)(void *data);

	/* how the framework shares the display */
	enum Mode {
		Mode_Normal = 0,
		Mode_Shell = 1,
		Mode_Application = 2,
		Mode_ApplicationB = 3,
		Mode_ApplicationC = 4,
		Mode_CommonDialog = 5
	};

	/* m_option bits */
	enum Option {
		Option_OneFrameMode = 1,
		Option_DisableInternalCallbackChecks = 2,
		Option_4 = 4,
		Option_UseDisplayAreaSettings = 8
	};

	class InitParam;

	static void SampleInit(InitParam *init_param);

	int32_t GetMode();
	bool IsShutdownDone();
	void RequestShutdown();
	~Framework();
	int32_t Run();
	void Update(void *param);
	graph::SurfacePool *GetSurfacePool() const;
	void SetLocale(int32_t locale);

	static Framework *framework;

private:
	static void JobInit();

	int32_t UnloadPreaction(Plugin *plugin);

	uint32_t m_run_loop_count;
	vector<Plugin *> m_plugin_list;
	Plugin *m_active_plugin;
	int32_t m_loading_plugin_count;
	Plugin *m_common_resource_plugin;
	LoadCRFinishCallback m_load_finish_callback;
	UnloadCRFinishCallback m_unload_finish_callback;
	ShutdownCallback m_shutdown_callback;
	void *m_shutdown_callback_data;
	bool m_job_ready;
	memory::HeapAllocator *m_decode_heap;
	bool m_decode_heap_created;
	memory::HeapAllocator *m_plugin_heap;
	bool m_plugin_heap_created;
	void *m_surface_pool_addr;
	graph::SurfacePool *m_surface_pool;
	ui::Environment *m_env;
	ui::Screen *m_scr;
	int32_t m_mode;
	uint32_t m_option;
	int32_t m_locale;
	thread::Mutex *m_update_mutex;
	thread::Cond *m_update_cond;
	thread::RWLock *m_list_lock;
	sound::Output *m_sound_output;
	bool m_shutdown_requested;
	SceUID m_power_cb_id;
	ui::EnvironmentParam *m_env_param;
};

}

/* Exports with no name found for their NIDs. */
extern "C" {
void ScePafToplevel_1EEFAECD(paf::Framework *framework, bool **done);
}

#endif
