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
#include <src/paf_types.h>

namespace paf {
class Framework;
class IDParam;
namespace ui {
class Widget;
}
}

/* Exports with no name found for their NIDs. */
extern "C" {
/* runs the frames until the framework may end; nested, it goes on from the step it was called in */
int32_t ScePafToplevel_1EEFAECD(paf::Framework *framework, bool **done);
/* plays a sound through the framework's Output for the widgets (the Environment's callback) */
int32_t ScePafToplevel_85EF6EC0(const char *buffer1, uint32_t size1, const char *buffer2,
				uint32_t size2, int32_t samplerate, float vol, bool ignore_init,
				void *data);
/* makes a widget from the common resource (the Environment's callback) */
paf::ui::Widget *ScePafToplevel_E37EC9D8(paf::ui::Widget *parent, const char *type,
					 const paf::IDParam &id, const paf::IDParam &style,
					 void *data);
/* reads the system parameters SystemParam keeps again; true when they changed */
bool ScePafToplevel_072EE507(void);
/* loads the effects whose bits are set, from dir or vs0:data/effects */
int32_t ScePafToplevel_397DD062(paf::Framework *framework, const char *dir, uint32_t effects);
/* the common resource's unload callback: frees its surface pool */
void ScePafToplevel_C9C02705(void *userdata);
/* a sound of the common resource; -1 without one */
int32_t ScePafToplevel_95CF05AC(paf::Framework *framework, const paf::IDParam &id,
				const char **buffer1, uint32_t *size1, const char **buffer2,
				uint32_t *size2, int32_t *samplerate, uint32_t *type);
/* a sound parameter of the common resource's; -1 without one */
int32_t ScePafToplevel_C04E8309(paf::Framework *framework, const paf::IDParam &id,
				uint32_t *value1, uint32_t *value2);
/* the Locale for a system language */
int32_t ScePafToplevel_1AEDE4E0(int32_t language);
}

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

	/* what the framework is made with; the defaults read the system's settings */
	class InitParam {
	public:
		InitParam();

		uint32_t display_list_size;
		uint32_t parameter_buffer_size;
		uint32_t parameter_buffer_location;
		uint32_t surface_pool_size;
		uint32_t graph_heap_size_on_main_memory;
		uint32_t graph_heap_size_on_video_memory;
		uint32_t text_surface_pool_size;
		void *surf_dev_mem_addr;
		uint32_t surf_dev_mem_size;
		void *mem_pvf_japanese_addr;
		uint32_t mem_pvf_japanese_size;
		void *mem_pvf_latin_addr;
		uint32_t mem_pvf_latin_size;
		void *mem_pvf_hangul_addr;
		uint32_t mem_pvf_hangul_size;
		void *mem_pvf_chinese_addr;
		uint32_t mem_pvf_chinese_size;
		char *path_pvf_japanese;
		char *path_pvf_latin;
		char *path_pvf_hangul;
		char *path_pvf_chinese;
		uint32_t pvf_option;
		uint32_t pvf_heap_size;
		uint32_t decode_heap_size;
		uint32_t plugin_heap_size;
		/* the ImageMode */
		int32_t image_mode;
		uint32_t screen_width;
		uint32_t screen_height;
		int32_t mode;
		uint32_t option;
		/* the Locale */
		int32_t locale;
		uint32_t enter_button_assign;
		bool allow_button_control;
		bool show_focus;
		bool ext_control;
		bool reserved;
		ui::EnvironmentParam *env_param;
		int32_t screen_orientation_mode;
		/* the text style SystemParam keeps, TEXT_STYLE_UNSET for its own */
		int32_t text_style;
		/* the listeners wutil hands out, in its order */
		void *edit_listener;
		void *web_edit_listener;
		void *message_dialog_listener;
		void *listener_A55215E5;
		void *webview_listener;
		void *listener_E638AA1F;
		/* the sound::Output::Mode */
		uint32_t sound_mode;
		/* handed to the Environment */
		bool env_option;
	};

	enum {
		TEXT_STYLE_UNSET = 0x80
	};

	/* the common resource, when it is not vs0:data/external/common/common_resource.rco */
	class CommonResourceInitParam {
	public:
		CommonResourceInitParam();

		string resource_file;
		uint8_t *resource_data;
		uint32_t resource_data_size;
		uint32_t private_heap_size;
		memory::HeapAllocator *private_heap;
		graph::SurfacePool *surface_pool;
		int32_t draw_priority;
	};

	Framework(const InitParam &init_param);

	static void SampleInit(InitParam *init_param);

	int32_t GetMode();
	bool IsShutdownDone();
	void RequestShutdown();
	~Framework();
	int32_t Run();
	void Update(void *param);
	graph::SurfacePool *GetSurfacePool() const;
	void SetLocale(Locale locale);
	void LoadCommonResourceSync(LoadCRFinishCallback callback);
	void LoadCommonResourceAsync(LoadCRFinishCallback callback);
	void LoadCommonResourceSync(CommonResourceInitParam &param, LoadCRFinishCallback callback);
	void LoadCommonResourceAsync(CommonResourceInitParam &param, LoadCRFinishCallback callback);
	void UnLoadCommonResourceAsync(UnloadCRFinishCallback callback);
	/* L"" without the common resource */
	const wchar_t *GetCommonString(const IDParam &id) const;
	intrusive_ptr<graph::Surface> GetCommonTexture(const IDParam &id);
	int32_t GetCommonResourceStyles(const char *name, vector<string> *styles);
	int32_t GetCommonResourceTextures(vector<string> *textures);
	int32_t GetCommonResourceLocale() const;
	int32_t SetCommonResourceLocale(Locale locale);
	/* the common sounds, numbered from 1 */
	int32_t InitCommonSE();
	int32_t PlayCommonSE(int32_t no, float vol, bool ignore_init);
	int32_t StopCommonSE(int32_t no);
	/* Mode_ApplicationC only: the frame buffer shared or off screen */
	int32_t SetMode(uint32_t mode);
	int32_t GetMode_ApplicationC();

	/* the common resource's load finish: its sounds, and the common hooks */
	static void CRLoadFinishCallback(Plugin *plugin);

	static Framework *framework;

private:
	friend int32_t (::ScePafToplevel_85EF6EC0)(const char *buffer1, uint32_t size1,
						    const char *buffer2, uint32_t size2,
						    int32_t samplerate, float vol, bool ignore_init,
						    void *data);
	friend int32_t (::ScePafToplevel_1EEFAECD)(Framework *framework, bool **done);
	friend void (::ScePafToplevel_C9C02705)(void *userdata);
	friend int32_t (::ScePafToplevel_95CF05AC)(Framework *framework, const IDParam &id,
						    const char **buffer1, uint32_t *size1,
						    const char **buffer2, uint32_t *size2,
						    int32_t *samplerate, uint32_t *type);
	friend int32_t (::ScePafToplevel_C04E8309)(Framework *framework, const IDParam &id,
						    uint32_t *value1, uint32_t *value2);
	friend ui::Widget *(::ScePafToplevel_E37EC9D8)(ui::Widget *parent, const char *type,
						     const IDParam &id, const IDParam &style,
						     void *data);

	static void JobInit();

	int32_t UnloadPreaction(Plugin *plugin);
	/* the common resource's surface pool, in shared memory or video memory */
	void CreateCommonResourceSurfacePool();

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

#endif
