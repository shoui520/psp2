/*
 * paf::Framework: plugins are loaded and unloaded through the list it keeps
 * under m_list_lock; it ends once a shutdown was requested and nothing runs.
 */
#include <framework.h>
#include <power.h>
#include <kernel/threadmgr.h>
#include <sysmem/memblock.h>
#include <sysmem/sysmem.h>
#include <cache/cache_manager.h>
#include <datetime/datetime.h>
#include <file/localfile.h>
#include <graphics/graphmem/graphmem.h>
#include <graphics/gutil.h>
#include <graphics/surface/surface_display.h>
#include <graphics/surface/surface_pool.h>
#include <graphics/text/text_init.h>
#include <image/image_types.h>
#include <common/common_internal.h>
#include <common/resource_hooks.h>
#include <inputdevice/inputdevice.h>
#include <inputdevice/pad/pad.h>
#include <job/job.h>
#include <resource/resource_obj.h>
#include <std/stdio.h>
#include <std/stdlib.h>
#include <system/system.h>
#include <system_param.h>
#include <timer/timerlistenerlist.h>
#include <widget/core/environment.h>
#include <widget/factory.h>
#include <widget/wutil.h>
#include "avout/sound.h"
#include "paf_types.h"

extern "C" int sceRegMgrUtilityGetInt(int id, int *value);

/* the system settings the Framework reads */
#define REGMGR_BUTTON_ASSIGN	0x683dcd
#define REGMGR_DATE_FORMAT	0x88776
#define REGMGR_TIME_FORMAT	0x668503
#define REGMGR_LANGUAGE		0x37502
#define REGMGR_ENTER_BUTTON	0x229142

/* a common dialog's threads: their attribute and CPU 3 (no SDK header names either) */
#define THREAD_ATTR_CDLG	0x02000000
#define THREAD_CPU_MASK_CDLG	0x00080000

namespace paf {

/* the display's power callback: sounds and the screen follow suspend and resume */
int32_t PowerCallback(int32_t notify_id, int32_t notify_count, int32_t power_info,
		      void *common);

Framework *Framework::framework;
/* what Update runs now: 1 while the Framework is made, 0 between the steps */
static int32_t s_update_phase = 1;

int32_t Framework::UnloadPreaction(Plugin *plugin)
{
	int32_t result = SCE_PAF_OK;

	plugin->Stop();
	plugin->PageCloseAll();
	m_list_lock->WriterLock();

	vector<Plugin *>::iterator end = m_plugin_list.end();
	vector<Plugin *>::iterator it;

	for (it = m_plugin_list.begin(); it != end; ++it) {
		if (*it == plugin)
			break;
	}
	if (it != end) {
		m_plugin_list.erase(it);
		if (plugin == m_active_plugin)
			m_active_plugin = NULL;
	} else {
		result = SCE_PAF_ERROR_NOT_FOUND;
	}
	m_list_lock->WriterUnlock();
	return result;
}

void Framework::SampleInit(InitParam *init_param)
{
}

void Framework::JobInit()
{
	job::JobQueue::Option option;

	job::JobQueue::Init(&option);
}

int32_t Framework::GetMode()
{
	return m_mode;
}

Framework::Framework(const InitParam &init_param)
{
	m_run_loop_count = 0;
	m_active_plugin = NULL;
	m_loading_plugin_count = 0;
	m_common_resource_plugin = NULL;
	m_load_finish_callback = NULL;
	m_unload_finish_callback = NULL;
	m_shutdown_callback = NULL;
	m_shutdown_callback_data = NULL;
	m_job_ready = true;
	m_decode_heap = NULL;
	m_decode_heap_created = false;
	m_plugin_heap = NULL;
	m_plugin_heap_created = false;
	m_surface_pool_addr = NULL;
	m_surface_pool = NULL;
	m_env = NULL;
	m_scr = NULL;
	m_mode = init_param.mode;
	m_option = init_param.option;
	m_locale = init_param.locale;
	m_update_mutex = NULL;
	m_update_cond = NULL;
	m_list_lock = NULL;
	m_sound_output = NULL;
	m_shutdown_requested = false;
	m_power_cb_id = -1;
	m_env_param = NULL;

	framework = this;
	if (m_mode == Mode_ApplicationB || m_mode == Mode_ApplicationC) {
		sce_paf_printf_dummy();
		sce_paf_printf_dummy();
	}

	thread::ThreadIDCache::SetCurrentType(thread::ThreadIDCache::Type_Main);
	LocalFile::SetThreadDefaultPriority(14);

	thread::Priority priority;
	thread::Thread::Option thread_option;

	switch (m_mode) {
	case Mode_Shell:
		priority.high = SCE_KERNEL_DEFAULT_PRIORITY_USER + 5;
		priority.normal = SCE_KERNEL_DEFAULT_PRIORITY_USER + 10;
		priority.low = SCE_KERNEL_DEFAULT_PRIORITY_USER + 14;
		break;
	case Mode_CommonDialog:
		thread_option.attr = THREAD_ATTR_CDLG;
		thread_option.cpuAffinityMask = THREAD_CPU_MASK_CDLG;
		thread_option.stackMemoryType = SCE_KERNEL_MEMBLOCK_TYPE_USER_CDIALOG_RW;
	default:
		priority.high = SCE_KERNEL_DEFAULT_PRIORITY_USER - 5;
		priority.normal = SCE_KERNEL_DEFAULT_PRIORITY_USER;
		priority.low = SCE_KERNEL_DEFAULT_PRIORITY_USER + 4;
		break;
	}
	thread::SetPriority(priority);
	thread::Thread::SetDefaultOption(thread_option);

	if (m_mode == Mode_CommonDialog)
		m_option |= Option_OneFrameMode;

	if (init_param.env_param == NULL)
		m_env_param = new ui::EnvironmentParam();
	else
		m_env_param = init_param.env_param->Clone();

	uint32_t width = init_param.screen_width;
	uint32_t height = init_param.screen_height;

	if (width == 0 || height == 0)
		m_env_param->GetScreenSize(&width, &height);

	gutil::DevInitParam dev_param = ui::wutil::debug::DevInit(width, height);

	dev_param.display_list_size = init_param.display_list_size;
	dev_param.parameter_buffer_size = init_param.parameter_buffer_size;
	dev_param.parameter_buffer_location = init_param.parameter_buffer_location;
	dev_param.graph_heap_size_on_main_memory = init_param.graph_heap_size_on_main_memory;
	dev_param.graph_heap_size_on_video_memory = init_param.graph_heap_size_on_video_memory;

	gutil::FrameBufferMode frame_buffer_mode;

	switch (m_mode) {
	case Mode_Shell:
		frame_buffer_mode = gutil::FrameBuffer_Shared_Create;
		break;
	case Mode_Application:
		frame_buffer_mode = gutil::FrameBuffer_Shared_Share;
		break;
	case Mode_ApplicationB:
		frame_buffer_mode = gutil::FrameBuffer_Shared_OffScreen;
		break;
	case Mode_ApplicationC:
		frame_buffer_mode = gutil::FrameBuffer_Shared_Share;
		break;
	case Mode_CommonDialog:
		frame_buffer_mode = gutil::FrameBuffer_Given;
		break;
	default:
		frame_buffer_mode = gutil::FrameBuffer_Normal;
		break;
	}
	gutil::SetFrameBufferMode(frame_buffer_mode);

	graph::TextInitParam text_param;

	text_param.text_surface_pool_size = init_param.text_surface_pool_size;
	text_param.mem_pvf_japanese_addr = init_param.mem_pvf_japanese_addr;
	text_param.mem_pvf_japanese_size = init_param.mem_pvf_japanese_size;
	text_param.mem_pvf_latin_addr = init_param.mem_pvf_latin_addr;
	text_param.mem_pvf_latin_size = init_param.mem_pvf_latin_size;
	text_param.mem_pvf_hangul_addr = init_param.mem_pvf_hangul_addr;
	text_param.mem_pvf_hangul_size = init_param.mem_pvf_hangul_size;
	text_param.mem_pvf_chinese_addr = init_param.mem_pvf_chinese_addr;
	text_param.mem_pvf_chinese_size = init_param.mem_pvf_chinese_size;
	text_param.path_pvf_japanese = init_param.path_pvf_japanese;
	text_param.path_pvf_latin = init_param.path_pvf_latin;
	text_param.mem_emoji_font_addr = init_param.mem_emoji_font_addr;
	text_param.mem_emoji_font_size = init_param.mem_emoji_font_size;
	text_param.pvf_heap_size = init_param.pvf_heap_size;
	text_param.pvf_option = init_param.pvf_option;
	text_param.locale = init_param.locale;

	gutil::InitGxm(dev_param);
	graph::InitText(text_param);
	ui::wutil::DevInit(dev_param);

	ui::wutil::InitParam wutil_param;

	wutil_param.projection_option = 0;
	ScePafWidget_5245A0F2(&wutil_param, &dev_param.whandle);
	wutil_param.surf_dev_mem_addr = init_param.surf_dev_mem_addr;
	wutil_param.surf_dev_mem_size = init_param.surf_dev_mem_size;
	wutil_param.fov = 0.80285144f;
	wutil_param.surf_def_pool_size = init_param.surface_pool_size;
	if (init_param.surface_pool_size != 0)
		wutil_param.surf_def_pool_device_type = m_mode == Mode_Shell;
	wutil_param.edit_listener = init_param.edit_listener;
	wutil_param.web_edit_listener = init_param.web_edit_listener;
	wutil_param.message_dialog_listener = init_param.message_dialog_listener;
	wutil_param.listener_A55215E5 = init_param.listener_A55215E5;
	wutil_param.webview_listener = init_param.webview_listener;
	wutil_param.listener_E638AA1F = init_param.listener_E638AA1F;
	switch (m_mode) {
	case Mode_Shell:
		wutil_param.input_device_mode = 1;
		break;
	case Mode_CommonDialog:
		wutil_param.input_device_mode = 2;
		break;
	default:
		wutil_param.input_device_mode = 0;
		break;
	}
	if (init_param.ext_control)
		wutil_param.input_device_mode = 3;
	ui::wutil::Init(wutil_param);

	if (frame_buffer_mode == gutil::FrameBuffer_Given && ui::Screen::s_root_surface != NULL &&
	    ui::Screen::s_root_surface->UsesDisplayArea())
		m_option |= Option_UseDisplayAreaSettings;

	ScePafMisc_0D1B028E(init_param.enter_button_assign);

	if (init_param.decode_heap_size != 0) {
		char name[32];
		memory::HeapAllocator::Option heap_option;

		sce_paf_snprintf(name, sizeof(name), "ScePafFwDecodeHeap");
		heap_option.entry_max = 0;
		heap_option.warning_size = 0;
		heap_option.disable_dbgprint = false;
		heap_option.disable_assert_on_alloc_failure = true;
		heap_option.memblk_type = memory::AllocationBlockType_User;
		heap_option.alignment = 0;
		if (m_mode == Mode_CommonDialog)
			heap_option.memblk_type = memory::AllocationBlockType_User_Cdlg;
		m_decode_heap = new memory::HeapAllocator(NULL, init_param.decode_heap_size, name,
							  &heap_option);
		m_decode_heap_created = true;
	}
	if (init_param.plugin_heap_size != 0) {
		char name[32];
		memory::HeapAllocator::Option heap_option;

		sce_paf_snprintf(name, sizeof(name), "ScePafFwDefaultPluginHeap");
		heap_option.entry_max = 0;
		heap_option.warning_size = 0;
		heap_option.disable_dbgprint = false;
		heap_option.disable_assert_on_alloc_failure = true;
		heap_option.memblk_type = memory::AllocationBlockType_User;
		heap_option.alignment = 0;
		if (m_mode == Mode_CommonDialog)
			heap_option.memblk_type = memory::AllocationBlockType_User_Cdlg;
		m_plugin_heap = new memory::HeapAllocator(NULL, init_param.plugin_heap_size, name,
							  &heap_option);
		m_plugin_heap_created = true;
	}

	void *env_mem = operator new(sizeof(ui::Environment));

	m_env = env_mem != NULL ? ScePafWidget_393F0EF0(env_mem, m_env_param,
							init_param.screen_orientation_mode) : NULL;
	m_env->m_sound_callback_data = this;
	m_env->m_sound_callback = ScePafToplevel_85EF6EC0;
	m_env->m_create_widget_callback_data = this;
	m_env->m_create_widget_callback = ScePafToplevel_E37EC9D8;
	if (m_option & Option_4)
		m_env->m_finish_after_draw = true;
	if ((m_option & Option_UseDisplayAreaSettings) &&
	    (m_mode == Mode_Normal || m_mode == Mode_CommonDialog))
		m_env->ApplyDisplayAreaSettings();
	ui::wutil::SetEnv(m_env);

	ui::Screen::s_draw_offset = math::v4(32.0f, 0.0f, 0.0f, 0.0f);
	if (init_param.allow_button_control) {
		int value;
		bool enable = true;

		if (sceRegMgrUtilityGetInt(REGMGR_BUTTON_ASSIGN, &value) == 0)
			enable = value == 1;
		ScePafWidget_75FB31F1(m_env, enable);
		ScePafWidget_B4A4660B(m_env, 1, -1, 1);
		if (init_param.show_focus)
			m_env->ShowFocus(-1);
		if ((m_mode == Mode_CommonDialog || !ScePafMisc_7ED8F03C()) &&
		    (m_mode != Mode_CommonDialog ||
		     system::Model::GetType() == system::Model::Type_Dolce)) {
			ui::Environment *env = m_env;

			env->m_focus_mode = 1;
			if (ScePafWidget_A6B659BB(env, 0))
				env->ShowFocus(-1);
		}
	}
	if (init_param.text_style != TEXT_STYLE_UNSET)
		system::SystemParam::SetTextStyle(init_param.text_style);
	m_env->m_env_option = init_param.env_option;

	s_update_phase = 1;
	Plugin::Init();
	Plugin::RegistFactoryTable(g_factoryGroup.m_name, g_factoryGroup);
	Plugin::RegistFactoryTable(g_factoryPlane.m_name, g_factoryPlane);
	Plugin::RegistFactoryTable(g_factoryButtonBase.m_name, g_factoryButtonBase);
	Plugin::RegistFactoryTable(g_factoryButton.m_name, g_factoryButton);
	Plugin::RegistFactoryTable(g_factoryPlaneButton.m_name, g_factoryPlaneButton);
	Plugin::RegistFactoryTable(g_factoryImageButton.m_name, g_factoryImageButton);
	Plugin::RegistFactoryTable(g_factoryRadioButton.m_name, g_factoryRadioButton);
	Plugin::RegistFactoryTable(g_factoryCornerButton.m_name, g_factoryCornerButton);
	Plugin::RegistFactoryTable(g_factoryRadioBox.m_name, g_factoryRadioBox);
	Plugin::RegistFactoryTable(g_factoryCheckBox.m_name, g_factoryCheckBox);
	Plugin::RegistFactoryTable(g_factoryText.m_name, g_factoryText);
	Plugin::RegistFactoryTable(g_factoryTextBox.m_name, g_factoryTextBox);
	Plugin::RegistFactoryTable(g_factoryScrollViewBase.m_name, g_factoryScrollViewBase);
	Plugin::RegistFactoryTable(g_factoryScrollView.m_name, g_factoryScrollView);
	Plugin::RegistFactoryTable(g_factoryListView.m_name, g_factoryListView);
	Plugin::RegistFactoryTable(g_factoryListItem.m_name, g_factoryListItem);
	Plugin::RegistFactoryTable(g_factoryBox.m_name, g_factoryBox);
	Plugin::RegistFactoryTable(g_factoryDialog.m_name, g_factoryDialog);
	Plugin::RegistFactoryTable(g_factoryScrollBar.m_name, g_factoryScrollBar);
	Plugin::RegistFactoryTable(g_factoryScrollBarTouch.m_name, g_factoryScrollBarTouch);
	Plugin::RegistFactoryTable(g_factoryScrollBar2d.m_name, g_factoryScrollBar2d);
	Plugin::RegistFactoryTable(g_factorySlideBar.m_name, g_factorySlideBar);
	Plugin::RegistFactoryTable(g_factoryImageSlideBar.m_name, g_factoryImageSlideBar);
	Plugin::RegistFactoryTable(g_factoryProgressBar.m_name, g_factoryProgressBar);
	Plugin::RegistFactoryTable(g_factoryProgressBarTouch.m_name, g_factoryProgressBarTouch);
	Plugin::RegistFactoryTable(g_factoryFileList.m_name, g_factoryFileList);
	Plugin::RegistFactoryTable(g_factorySlidingDrawer.m_name, g_factorySlidingDrawer);
	Plugin::RegistFactoryTable(g_factoryRichText.m_name, g_factoryRichText);
	Plugin::RegistFactoryTable(g_factoryDirectBar.m_name, g_factoryDirectBar);
	Plugin::RegistFactoryTable(g_factoryBusyIndicator.m_name, g_factoryBusyIndicator);
	Plugin::RegistFactoryTable(g_factorySpeechBalloon.m_name, g_factorySpeechBalloon);
	Plugin::RegistFactoryTable(g_factoryNumSpin.m_name, g_factoryNumSpin);
	Plugin::RegistFactoryTable(g_factoryCompositeDrawer.m_name, g_factoryCompositeDrawer);
	Plugin::RegistFactoryTable(g_factoryCompositeButton.m_name, g_factoryCompositeButton);
	Plugin::RegistFactoryTable(g_factoryAppIconSimple.m_name, g_factoryAppIconSimple);
	Plugin::RegistFactoryDrawObj(g_factoryPlaneObj.m_name, g_factoryPlaneObj);
	Plugin::RegistFactoryDrawObj(g_factoryTextObj.m_name, g_factoryTextObj);

	thread::RMutex::main_thread_mutex.Lock();
	m_list_lock = new thread::RWLock("ScePafFwListRWLock", 0);
	TimerListenerList::Init();

	job::JobQueue::Option job_option;

	job::JobQueue::Init(&job_option);
	if (m_option & Option_OneFrameMode) {
		m_update_mutex = new thread::Mutex("ScePafFwUpdateMutex", 0);
		m_update_cond = new thread::Cond(*m_update_mutex, "ScePafFwUpdateCond", 0);
	}

	int date_format;

	if (sceRegMgrUtilityGetInt(REGMGR_DATE_FORMAT, &date_format) == 0) {
		const wchar_t *format = NULL;

		switch ((uint32_t)date_format) {
		case 0:
			format = L"%Y/%m/%d";
			break;
		case 1:
			format = L"%d/%m/%Y";
			break;
		case 2:
			format = L"%m/%d/%Y";
			break;
		}
		if (format != NULL)
			datetime::DateTime::SetDateFormat(format);
	}

	int time_format;

	if (sceRegMgrUtilityGetInt(REGMGR_TIME_FORMAT, &time_format) == 0) {
		const wchar_t *format = NULL;

		switch ((uint32_t)time_format) {
		case 0:
			format = L"%I:%M %p";
			break;
		case 1:
			format = L"%H:%M";
			break;
		}
		if (format != NULL)
			datetime::DateTime::SetTimeFormat(format);
	}
	datetime::DateTime::SetDateTimeFormat(L"%x  %X");

	switch (init_param.sound_mode) {
	case sound::Output::Mode_Normal:
		if (m_mode != Mode_CommonDialog) {
			m_sound_output = new sound::Output(sound::Output::Mode_Normal);
			break;
		}
	case sound::Output::Mode_CommonDialog:
		m_sound_output = new sound::Output(sound::Output::Mode_CommonDialog);
		break;
	}

	m_power_cb_id = sceKernelCreateCallback("ScePafPowerCallback", 0, PowerCallback, NULL);
	if (m_power_cb_id >= 0)
		scePowerRegisterCallback(m_power_cb_id);
}

bool Framework::IsShutdownDone()
{
	if (!m_shutdown_requested)
		return false;
	if (!m_job_ready)
		return false;
	if (job::JobQueue::NumQueues() != 0 || job::JobQueue::default_queue->NumItems() != 0)
		return false;

	m_list_lock->ReaderLock();
	int32_t plugins = m_plugin_list.size();
	m_list_lock->ReaderUnlock();

	return plugins <= 0 && m_env->IsTermAvailable();
}

void Framework::RequestShutdown()
{
	if (!m_shutdown_requested && m_shutdown_callback != NULL)
		m_shutdown_callback(m_shutdown_callback_data);
	m_shutdown_requested = true;
}

Framework::~Framework()
{
	if (m_power_cb_id >= 0) {
		scePowerUnregisterCallback(m_power_cb_id);
		sceKernelDeleteCallback(m_power_cb_id);
	}
	RequestShutdown();
	while (!IsShutdownDone()) {
		thread::Sleep(17);
		TimerListenerList::s_default_list->Update();
		job::JobQueue::CheckAllQueues();
	}
	if (m_sound_output != NULL) {
		delete m_sound_output;
		m_sound_output = NULL;
	}
	if (m_option & Option_OneFrameMode) {
		delete m_update_cond;
		delete m_update_mutex;
	}
	job::JobQueue::Exit();
	TimerListenerList::Exit();
	delete m_list_lock;
	thread::RMutex::main_thread_mutex.Unlock();
	Plugin::End();
	delete m_env;
	ui::wutil::Term();
	if (m_plugin_heap_created)
		delete m_plugin_heap;
	if (m_decode_heap_created)
		delete m_decode_heap;
	ui::wutil::DevTerm();
	ui::wutil::debug::DevTerm();
	delete m_env_param;
	framework = NULL;
}

}

/*
 * The frames: the timers, the jobs, the calls posted to the main thread, the
 * widgets' update, their events and the drawing, until the framework may end.
 * Called again from inside one of these steps, it goes on from the step after
 * it, and returns once the outer call's step is reached again with the
 * framework ending or the outer frame finished (through *done).
 */
int32_t ScePafToplevel_1EEFAECD(paf::Framework *framework, bool **done)
{
	using namespace paf;

	bool finished = false;
	int32_t phase = s_update_phase;
	bool resume = true;

	s_update_phase = 0;
	*done = &finished;

	while (!framework->IsShutdownDone()) {
		if (resume) {
			resume = false;
			switch (phase) {
			case 2:
				goto job;
			case 3:
				goto sync_call;
			case 4:
				goto call_queue;
			case 5:
				goto event_loop;
			}
		}

		if (framework->m_option & Framework::Option_OneFrameMode) {
			framework->m_update_mutex->Lock();
			framework->m_update_cond->Notify();
			framework->m_update_cond->Wait(0);
			framework->m_update_mutex->Unlock();
		}
		if (!(framework->m_option & Framework::Option_DisableInternalCallbackChecks))
			sceKernelCheckCallback();
		TimerListenerList::s_default_list->Update();
		if (phase == 2 && (finished || framework->m_shutdown_requested))
			break;

job:
		s_update_phase = 2;
		job::JobQueue::CheckAllQueues();
		s_update_phase = 0;
		cache::CacheManager::Touch();
		if (phase == 3 && (finished || framework->m_shutdown_requested))
			break;

sync_call:
		s_update_phase = 3;
		thread::SyncCall::main_thread_synccall.Check();
		s_update_phase = 0;
		if (phase == 4 && (finished || framework->m_shutdown_requested))
			break;

call_queue:
		s_update_phase = 4;
		thread::CallQueue::main_thread_callqueue.Check();
		s_update_phase = 0;
		if (framework->m_sound_output != NULL)
			framework->m_sound_output->Update();
		framework->m_env->OnUpdatePre();
		if (framework->m_shutdown_requested && phase == 1 &&
		    framework->m_loading_plugin_count == 0) {
			thread::RWLock *lock = framework->m_list_lock;

			lock->ReaderLock();
			vector<Plugin *>::iterator it = framework->m_plugin_list.end();
			while (it != framework->m_plugin_list.begin()) {
				--it;
				if (!(*it)->m_unloading)
					Plugin::UnloadAsync(*it, NULL, 0);
			}
			lock->ReaderUnlock();
		}
		if (framework->m_mode == Framework::Mode_Normal ||
		    framework->m_mode == Framework::Mode_Shell ||
		    framework->m_mode == Framework::Mode_CommonDialog) {
			ScePafWidget_909850C4();
			ScePafWidget_9D4A5A0E(framework->m_env);
		} else {
			ScePafWidget_9D4A5A0E(framework->m_env);
			ScePafWidget_909850C4();
		}
		if (phase == 5 && (finished || framework->m_shutdown_requested))
			break;

event_loop:
		s_update_phase = 5;
		framework->m_env->EventLoop();
		s_update_phase = 0;
		framework->m_env->OnUpdatePost();
		ScePafWidget_DC1B19DB(framework->m_env);
		ScePafWidget_851911CB(framework->m_env);
		thread::RMutex::main_thread_mutex.Unlock();
		int32_t result = ScePafWidget_F67517A0(framework->m_env);
		thread::RMutex::main_thread_mutex.Lock();
		ScePafWidget_8E61FE0C(framework->m_env);
		if (result < 0 || ScePafWidget_998F8F48() != 0)
			framework->RequestShutdown();
		if (framework->m_shutdown_requested && phase == 1 &&
		    framework->m_loading_plugin_count == 0 && framework->m_plugin_list.size() == 0 &&
		    !framework->m_env->m_term_requested)
			framework->m_env->RequestTerm();
	}

	s_update_phase = phase;
	*done = NULL;
	return 0;
}

namespace paf {

int32_t Framework::Run()
{
	bool *done;

	if (m_mode != Mode_Shell)
		sceKernelChangeThreadPriority(0, 0x100000ED);
	if (m_option & Option_OneFrameMode) {
		m_update_mutex->Lock();
		m_run_loop_count++;
		m_update_mutex->Unlock();
	}
	done = NULL;
	ScePafToplevel_1EEFAECD(this, &done);
	if (m_option & Option_OneFrameMode) {
		m_update_mutex->Lock();
		m_update_cond->Notify();
		m_run_loop_count--;
		m_update_mutex->Unlock();
	}
	if (done != NULL)
		*done = true;
	return 0;
}

void Framework::Update(void *param)
{
	if (m_option & Option_OneFrameMode) {
		m_update_mutex->Lock();
		if (m_env->m_screen != NULL && ui::Screen::s_root_surface != NULL)
			ui::Screen::s_root_surface->SetFrameBuffer(param);
		if (m_run_loop_count != 0) {
			m_update_cond->Notify();
			m_update_cond->Wait(0);
		}
		m_update_mutex->Unlock();
	}
}

graph::SurfacePool *Framework::GetSurfacePool() const
{
	return gutil::GetDefaultSurfacePool();
}

void Framework::SetLocale(Locale locale)
{
	m_locale = locale;
	graph::SetTextLocale(locale);
}

}

bool ScePafToplevel_072EE507(void)
{
	bool changed = false;
	int32_t flags = paf::system::SystemParam::ReadParamFlags();

	if (flags != ScePafMisc_C51EF81D()) {
		paf::graph::StopTextThreads();
		paf::system::SystemParam::SetParamFlags(flags);
		paf::graph::StartTextThreads();
		changed = true;
	}
	return changed;
}

int32_t ScePafToplevel_397DD062(paf::Framework *framework, const char *dir, uint32_t effects)
{
	if (dir != NULL)
		return ScePafWidget_BA6EB0C6(effects, paf::string(dir));
	return ScePafWidget_BA6EB0C6(effects, paf::string("vs0:data/effects"));
}

namespace paf {

/* the common resource's surface pool */
#define CR_SURFACE_POOL_SIZE	0xc0000

void Framework::CreateCommonResourceSurfacePool()
{
	graph::SurfacePool::MemoryType type = graph::SurfacePool::MemoryType_SharedClient;

	if (m_mode != Mode_Shell)
		m_surface_pool_addr = GraphMem::AllocOSMemory(GraphMem::DeviceType_SharedMemory,
							      CR_SURFACE_POOL_SIZE,
							      "ScePafCRSurfacePool", NULL);
	if (m_surface_pool_addr == NULL) {
		MemBlockUserOpt memblock_opt;
		GraphMem::Option opt;

		sce_paf_memset(&memblock_opt, 0, sizeof(memblock_opt));
		memblock_opt.size = sizeof(memblock_opt);
		memblock_opt.attr = ALLOC_ATTR_CLIENT_INFO | ALLOC_ATTR_HAS_CLIENT_INFO;
		memblock_opt.openFlags = MEMBLOCK_OPEN_RW;
		for (int i = 0; i < 8; i++)
			memblock_opt.capabilities[i] = 0;
		opt.memblock_option = &memblock_opt;
		m_surface_pool_addr = GraphMem::AllocOSMemory(GraphMem::DeviceType_VideoMemory,
							      CR_SURFACE_POOL_SIZE,
							      "ScePafCRSurfacePool", &opt);
		type = graph::SurfacePool::MemoryType_SharedHost;
	}
	if (m_surface_pool_addr != NULL)
		m_surface_pool = new graph::SurfacePool(m_surface_pool_addr, CR_SURFACE_POOL_SIZE,
							"ScePafCRSurfacePool", NULL, type);
}

void Framework::LoadCommonResourceSync(LoadCRFinishCallback callback)
{
	m_load_finish_callback = callback;

	Plugin::InitParam param;

	param.name = "__system__common_resource";
	param.resource_file = "vs0:data/external/common/common_resource.rco";
	param.caller_name = "__system__";
	param.unload_callback = ScePafToplevel_C9C02705;
	CreateCommonResourceSurfacePool();
	param.surface_pool = m_surface_pool;
	Plugin::LoadSync(param, CRLoadFinishCallback, 0);
}

void Framework::LoadCommonResourceAsync(LoadCRFinishCallback callback)
{
	m_load_finish_callback = callback;

	Plugin::InitParam param;

	param.name = "__system__common_resource";
	param.resource_file = "vs0:data/external/common/common_resource.rco";
	param.caller_name = "__system__";
	param.unload_callback = ScePafToplevel_C9C02705;
	CreateCommonResourceSurfacePool();
	param.surface_pool = m_surface_pool;
	Plugin::LoadAsync(param, CRLoadFinishCallback, 0);
}

void Framework::LoadCommonResourceSync(CommonResourceInitParam &cr_param,
				       LoadCRFinishCallback callback)
{
	m_load_finish_callback = callback;

	Plugin::InitParam param;

	param.name = "__system__common_resource";
	param.resource_file = cr_param.resource_file;
	param.caller_name = "__system__";
	param.unload_callback = ScePafToplevel_C9C02705;
	param.resource_data = cr_param.resource_data;
	param.resource_data_size = cr_param.resource_data_size;
	param.private_heap_size = cr_param.private_heap_size;
	param.private_heap = cr_param.private_heap;
	param.surface_pool = cr_param.surface_pool;
	param.draw_priority = cr_param.draw_priority;
	Plugin::LoadSync(param, CRLoadFinishCallback, 0);
}

void Framework::LoadCommonResourceAsync(CommonResourceInitParam &cr_param,
					LoadCRFinishCallback callback)
{
	m_load_finish_callback = callback;

	Plugin::InitParam param;

	param.name = "__system__common_resource";
	param.resource_file = cr_param.resource_file;
	param.caller_name = "__system__";
	param.unload_callback = ScePafToplevel_C9C02705;
	param.resource_data = cr_param.resource_data;
	param.resource_data_size = cr_param.resource_data_size;
	param.private_heap_size = cr_param.private_heap_size;
	param.private_heap = cr_param.private_heap;
	param.surface_pool = cr_param.surface_pool;
	param.draw_priority = cr_param.draw_priority;
	Plugin::LoadAsync(param, CRLoadFinishCallback, 0);
}

void Framework::UnLoadCommonResourceAsync(UnloadCRFinishCallback callback)
{
	m_unload_finish_callback = callback;
	if (!m_common_resource_plugin->m_unloading)
		Plugin::UnloadAsync(m_common_resource_plugin, NULL, 0);
}

const wchar_t *Framework::GetCommonString(const IDParam &id) const
{
	if (m_common_resource_plugin != NULL)
		return m_common_resource_plugin->GetString(id);
	return L"";
}

intrusive_ptr<graph::Surface> Framework::GetCommonTexture(const IDParam &id)
{
	if (m_common_resource_plugin != NULL)
		return m_common_resource_plugin->GetTexture(id);
	return intrusive_ptr<graph::Surface>();
}

}

int32_t ScePafToplevel_95CF05AC(paf::Framework *framework, const paf::IDParam &id,
				const char **buffer1, uint32_t *size1, const char **buffer2,
				uint32_t *size2, int32_t *samplerate, uint32_t *type)
{
	paf::Plugin *plugin = framework->m_common_resource_plugin;

	if (plugin != NULL)
		return ScePafToplevel_CA3E606F(plugin, id, buffer1, size1, buffer2, size2,
					       samplerate, type);
	return -1;
}

int32_t ScePafToplevel_C04E8309(paf::Framework *framework, const paf::IDParam &id,
				paf::ui::WidgetSoundParam::Sound **sounds, uint32_t *num)
{
	if (framework->m_common_resource_plugin != NULL)
		return ScePafToplevel_DA3D4AF0(framework->m_common_resource_plugin, id, sounds,
					       num);
	return -1;
}

namespace paf {

int32_t Framework::GetCommonResourceStyles(const char *name, vector<string> *styles)
{
	if (m_common_resource_plugin != NULL)
		return m_common_resource_plugin->GetResourceStyles(name, styles);
	return -1;
}

int32_t Framework::GetCommonResourceTextures(vector<string> *textures)
{
	if (m_common_resource_plugin != NULL)
		return m_common_resource_plugin->GetResourceTextures(textures);
	return -1;
}

int32_t Framework::GetCommonResourceLocale() const
{
	if (m_common_resource_plugin != NULL)
		return m_common_resource_plugin->m_locale;
	return m_locale;
}

int32_t Framework::SetCommonResourceLocale(Locale locale)
{
	if (m_common_resource_plugin == NULL)
		return -1;
	m_common_resource_plugin->SetLocale(locale);
	return 0;
}

/* the common sounds' resource ids, by number less one; numbers without one have none */
int32_t Framework::InitCommonSE()
{
	const char *se_id[sound::Output::COMMON_SE_NUM] = {
		"_common_snd01", "_common_snd02", "_common_snd03", "_common_snd04", NULL, NULL,
		"_common_snd07", "_common_snd08", NULL, NULL, NULL, "_common_snd12",
		"_common_snd13", NULL, "_common_snd15", "_common_snd16", "_common_snd17",
		"_common_snd18", "_common_snd19", "_common_snd20", "_common_snd21",
		"_common_snd22", "_common_snd23", "_common_snd24", NULL, NULL, "_common_snd27",
		"_common_snd28", NULL, "_common_snd30", "_common_snd31", "_common_snd32",
		"_common_snd33", "_common_snd34", "_common_snd35", "_common_snd36",
		"_common_snd37", "_common_snd38", "_common_snd39", "_common_snd40",
		"_common_snd41", "_common_snd42", "_common_snd43", "_common_snd44",
		"_common_snd45", "_common_snd46", "_common_snd47", "_common_snd48",
		"_common_snd49", "_common_snd50", "_common_snd51", NULL, NULL, "_common_snd54",
		"_common_snd55"
	};

	if (m_sound_output == NULL)
		return -1;

	sce_paf_memset(m_sound_output->m_common_se, 0, sizeof(m_sound_output->m_common_se));
	for (uint32_t i = 0; i < sound::Output::COMMON_SE_NUM; i++) {
		if (se_id[i] == NULL)
			continue;

		IDParam id(se_id[i]);
		sound::Output::CommonSE *se = &m_sound_output->m_common_se[i];

		if (m_common_resource_plugin != NULL)
			ScePafToplevel_CA3E606F(m_common_resource_plugin, id, &se->buffer1, &se->size1,
						&se->buffer2, &se->size2, &se->samplerate,
						&se->type);
	}
	return 0;
}

int32_t Framework::PlayCommonSE(int32_t no, float vol, bool ignore_init)
{
	sound::Output *output = m_sound_output;

	if (output == NULL)
		return -1;
	if (no <= 0 || no > sound::Output::COMMON_SE_NUM)
		return -1;

	sound::Output::CommonSE *se = &output->m_common_se[no - 1];

	if (se->buffer1 == NULL)
		return -1;
	if (se->buffer2 != NULL)
		output->PlaySound(se->buffer1, se->size1, se->buffer2, se->size2, se->samplerate,
				  vol, ignore_init);
	else
		output->PlaySound(se->buffer1, se->size1, se->samplerate, vol, ignore_init);
	return 0;
}

int32_t Framework::StopCommonSE(int32_t no)
{
	if (m_sound_output == NULL)
		return -1;
	if (no <= 0 || no > sound::Output::COMMON_SE_NUM)
		return -1;

	const char *buffer1 = m_sound_output->m_common_se[no - 1].buffer1;

	if (buffer1 == NULL)
		return -1;
	if (m_sound_output->m_common_se[no - 1].buffer2 != NULL) {
		m_sound_output->StopSound(buffer1);
		m_sound_output->StopSound(m_sound_output->m_common_se[no - 1].buffer2);
	} else {
		m_sound_output->StopSound(buffer1);
	}
	return 0;
}

int32_t Framework::SetMode(uint32_t mode)
{
	if (m_mode != Mode_ApplicationC)
		return -1;

	ui::Screen *screen = m_env->m_screen;

	if (screen == NULL)
		return -1;

	graph::DisplaySurface *surface =
		static_cast<graph::DisplaySurface *>(screen->GetSurface().get());

	if (surface == NULL)
		return -1;

	gutil::FrameBufferMode frame_buffer_mode;

	if (mode == gutil::FrameBuffer_Shared_Share)
		frame_buffer_mode = gutil::FrameBuffer_Shared_Share;
	else if (mode == gutil::FrameBuffer_Shared_OffScreen)
		frame_buffer_mode = gutil::FrameBuffer_Shared_OffScreen;
	else
		frame_buffer_mode = gutil::FrameBuffer_Shared_Share;
	surface->SetFrameBufferMode(frame_buffer_mode);
	return 0;
}

int32_t Framework::GetMode_ApplicationC()
{
	if (m_mode != Mode_ApplicationC)
		return -1;

	ui::Screen *screen = m_env->m_screen;

	if (screen == NULL)
		return -1;

	graph::DisplaySurface *surface =
		static_cast<graph::DisplaySurface *>(screen->GetSurface().get());

	if (surface == NULL)
		return gutil::FrameBuffer_Shared_Share;
	if (surface->m_frame_buffer_mode == gutil::FrameBuffer_Shared_Share)
		return gutil::FrameBuffer_Shared_Share;
	return gutil::FrameBuffer_Shared_OffScreen;
}

}

int32_t ScePafToplevel_1AEDE4E0(int32_t language)
{
	switch (language) {
	case SCE_SYSTEM_PARAM_LANG_JAPANESE:
		return SCE_SYSTEM_PARAM_LANG_JAPANESE;
	case SCE_SYSTEM_PARAM_LANG_ENGLISH_US:
		return SCE_SYSTEM_PARAM_LANG_ENGLISH_US;
	case SCE_SYSTEM_PARAM_LANG_FRENCH:
		return SCE_SYSTEM_PARAM_LANG_FRENCH;
	case SCE_SYSTEM_PARAM_LANG_SPANISH:
		return SCE_SYSTEM_PARAM_LANG_SPANISH;
	case SCE_SYSTEM_PARAM_LANG_GERMAN:
		return SCE_SYSTEM_PARAM_LANG_GERMAN;
	case SCE_SYSTEM_PARAM_LANG_ITALIAN:
		return SCE_SYSTEM_PARAM_LANG_ITALIAN;
	case SCE_SYSTEM_PARAM_LANG_DUTCH:
		return SCE_SYSTEM_PARAM_LANG_DUTCH;
	case SCE_SYSTEM_PARAM_LANG_PORTUGUESE_PT:
		return SCE_SYSTEM_PARAM_LANG_PORTUGUESE_PT;
	case SCE_SYSTEM_PARAM_LANG_RUSSIAN:
		return SCE_SYSTEM_PARAM_LANG_RUSSIAN;
	case SCE_SYSTEM_PARAM_LANG_KOREAN:
		return SCE_SYSTEM_PARAM_LANG_KOREAN;
	case SCE_SYSTEM_PARAM_LANG_CHINESE_T:
		return SCE_SYSTEM_PARAM_LANG_CHINESE_T;
	case SCE_SYSTEM_PARAM_LANG_CHINESE_S:
		return SCE_SYSTEM_PARAM_LANG_CHINESE_S;
	case SCE_SYSTEM_PARAM_LANG_FINNISH:
		return SCE_SYSTEM_PARAM_LANG_FINNISH;
	case SCE_SYSTEM_PARAM_LANG_SWEDISH:
		return SCE_SYSTEM_PARAM_LANG_SWEDISH;
	case SCE_SYSTEM_PARAM_LANG_DANISH:
		return SCE_SYSTEM_PARAM_LANG_DANISH;
	case SCE_SYSTEM_PARAM_LANG_NORWEGIAN:
		return SCE_SYSTEM_PARAM_LANG_NORWEGIAN;
	case SCE_SYSTEM_PARAM_LANG_POLISH:
		return SCE_SYSTEM_PARAM_LANG_POLISH;
	case SCE_SYSTEM_PARAM_LANG_PORTUGUESE_BR:
		return SCE_SYSTEM_PARAM_LANG_PORTUGUESE_BR;
	case SCE_SYSTEM_PARAM_LANG_ENGLISH_GB:
		return SCE_SYSTEM_PARAM_LANG_ENGLISH_GB;
	case SCE_SYSTEM_PARAM_LANG_TURKISH:
		return SCE_SYSTEM_PARAM_LANG_TURKISH;
	default:
		return SCE_SYSTEM_PARAM_LANG_ENGLISH_GB;
	}
}

namespace paf {

/* the input devices a load or unload locks */
enum {
	INPUT_DEVICE_ALL = (1 << inputdevice::DEVICE_NUM) - 1
};

/* runs a plugin load on the default job queue: the plugin is made there, started here */
class PluginLoadJob : public job::JobItem {
public:
	PluginLoadJob(const char *name, Framework::ConstructPluginData *data)
		: job::JobItem(name, NULL), m_data(data)
	{
	}

	virtual ~PluginLoadJob()
	{
	}

	virtual int Run()
	{
		m_data->framework->LoadRunAction(m_data);
		return 0;
	}

	virtual void Finish(int result)
	{
		m_data->framework->LoadFinishedAction(m_data);
	}

	Framework::ConstructPluginData *m_data;
};

/* runs a plugin unload on the default job queue */
class PluginUnloadJob : public job::JobItem {
public:
	PluginUnloadJob(const char *name, Framework::DestructPluginData *data)
		: job::JobItem(name, NULL), m_data(data)
	{
	}

	virtual ~PluginUnloadJob()
	{
	}

	virtual int Run()
	{
		m_data->framework->UnloadRunAction(m_data);
		return 0;
	}

	virtual void Finish(int result)
	{
		m_data->framework->UnloadFinishedAction(m_data);
	}

	Framework::DestructPluginData *m_data;
};

void Framework::LoadFinishedAction(ConstructPluginData *data)
{
	if (data->plugin != NULL) {
		m_list_lock->WriterLock();
		data->framework->m_plugin_list.push_back(data->plugin);
		m_list_lock->WriterUnlock();
		data->plugin->Start();
	}
	if (data->finish_callback != NULL)
		data->finish_callback(data->plugin);
	--m_loading_plugin_count;
	if ((data->option & Plugin::LoadUnloadOption_DontLockInputDevice) == 0)
		inputdevice::Unlock(INPUT_DEVICE_ALL);
	delete data->init_param;
	delete data;
}

inline void Framework::LoadRunAction(ConstructPluginData *data)
{
	data->plugin = new Plugin(data->init_param);
}

inline void Framework::LoadInitAction(int32_t option)
{
	++m_loading_plugin_count;
	if ((option & Plugin::LoadUnloadOption_DontLockInputDevice) == 0)
		inputdevice::Lock(INPUT_DEVICE_ALL);
}

void Framework::LoadPluginSync(const Plugin::InitParam &param, Plugin::LoadFinishCallback finish,
			       int32_t option)
{
	LoadInitAction(option);

	ConstructPluginData *data = new ConstructPluginData;

	data->framework = this;
	data->plugin = NULL;
	data->init_param = new Plugin::InitParam(param);
	data->finish_callback = finish;
	data->option = option;
	LoadRunAction(data);
	LoadFinishedAction(data);
}

void Framework::RequestLoadPlugin(const Plugin::InitParam &param, Plugin::LoadFinishCallback finish,
				  int32_t option)
{
	LoadInitAction(option);

	ConstructPluginData *data = new ConstructPluginData;

	data->framework = this;
	data->plugin = NULL;
	data->init_param = new Plugin::InitParam(param);
	data->finish_callback = finish;
	data->option = option;

	common::SharedPtr<PluginLoadJob> item(new PluginLoadJob("Job_PluginLoad", data));

	job::JobQueue::default_queue->Enqueue(
		reinterpret_cast<common::SharedPtr<job::JobItem> &>(item));
}

Plugin *Framework::FindPlugin(const char *name, bool include_unloading)
{
	Plugin *plugin = NULL;

	m_list_lock->ReaderLock();
	for (vector<Plugin *>::iterator it = m_plugin_list.begin(); it != m_plugin_list.end(); ++it) {
		if (name == (*it)->m_name) {
			plugin = *it;
			break;
		}
	}
	m_list_lock->ReaderUnlock();

	if (!include_unloading && plugin != NULL && plugin->m_unloading)
		return NULL;
	return plugin;
}

inline void Framework::UnloadInitAction(Plugin *plugin, const char *name, int32_t option)
{
	if ((option & Plugin::LoadUnloadOption_DontLockInputDevice) == 0)
		inputdevice::Lock(INPUT_DEVICE_ALL);
	if (plugin != NULL) {
		plugin->m_unloading = true;
	} else if (name != NULL) {
		plugin = FindPlugin(name, false);
		if (plugin != NULL)
			plugin->m_unloading = true;
	}
}

Plugin *Framework::FindPlugin(Plugin *plugin, bool include_unloading)
{
	m_list_lock->ReaderLock();

	vector<Plugin *>::iterator it = m_plugin_list.begin();
	vector<Plugin *>::iterator end = m_plugin_list.end();

	for (; it != end; ++it) {
		if (*it == plugin)
			break;
	}
	m_list_lock->ReaderUnlock();

	if (it == m_plugin_list.end())
		return NULL;
	if (include_unloading || !(*it)->m_unloading)
		return *it;
	return NULL;
}

int32_t Framework::RequestUnloadPlugin(Plugin *plugin, const char *name,
				       Plugin::UnloadFinishCallback finish, int32_t option)
{
	if ((plugin == NULL && name == NULL) || (plugin != NULL && name != NULL)) {
		common_internal::on_error();
		return SCE_PAF_ERROR_TOPLEVEL_ARGUMENT;
	}
	if (plugin != NULL && FindPlugin(plugin, false) == NULL) {
		common_internal::on_error();
		return SCE_PAF_ERROR_NOT_FOUND;
	}

	UnloadInitAction(plugin, name, option);

	DestructPluginData *data = new DestructPluginData;

	data->framework = this;
	data->plugin = plugin;
	data->name = name;
	data->option = option;
	data->finish_callback = finish;

	common::SharedPtr<PluginUnloadJob> item(new PluginUnloadJob("Job_PluginUnLoad", data));

	return job::JobQueue::default_queue->Enqueue(
		reinterpret_cast<common::SharedPtr<job::JobItem> &>(item));
}

void Framework::SetLocaleAllPlugin(Locale locale)
{
	thread::RWLock *lock = m_list_lock;

	lock->ReaderLock();
	for (vector<Plugin *>::iterator it = m_plugin_list.begin(); it != m_plugin_list.end(); ++it)
		(*it)->SetLocale(locale);
	lock->ReaderUnlock();
}

void Framework::UnloadRunAction(DestructPluginData *data)
{
	Framework *framework = data->framework;

	if (data->plugin == NULL)
		data->plugin = framework->FindPlugin(data->name.c_str(), true);
	else
		data->plugin = framework->FindPlugin(data->plugin, true);

	Plugin *plugin = data->plugin;

	if (plugin == NULL)
		return;
	plugin->m_unloading = true;
	if (plugin->m_rscobj != NULL && plugin->m_rscobj->m_num_sounds != 0 &&
	    m_sound_output != NULL)
		m_sound_output->WaitSoundStopped();

	thread::SyncCall *synccall = &thread::SyncCall::main_thread_synccall;

	if (synccall == NULL)
		common_internal::on_error();
	synccall->Call(UnloadPreactionCall, data);
	data->plugin->Exit();
}

void Framework::UnloadFinishedAction(DestructPluginData *data)
{
	if ((data->option & Plugin::LoadUnloadOption_DontLockInputDevice) == 0)
		inputdevice::Unlock(INPUT_DEVICE_ALL);

	string name;

	if (data->name.size() != 0)
		name = data->name;
	if (data->plugin != NULL) {
		if (data->finish_callback != NULL)
			name = data->plugin->m_name;
		delete data->plugin;
	}
	if (data->finish_callback != NULL)
		data->finish_callback(name.c_str());
	delete data;
}

}

bool ScePafToplevel_CFF44F67(uint32_t value)
{
	return value != 0 && value <= 1;
}

namespace paf {

void Framework::CRLoadFinishCallback(Plugin *plugin)
{
	framework->m_common_resource_plugin = plugin;
	framework->InitCommonSE();
	if (framework->m_load_finish_callback != NULL)
		framework->m_load_finish_callback();
	SetCommonTemplateOpenFunction(CommonTemplateOpen);
	SetCommonStringFunction(CommonString);
	SetCommonTextureFunction(CommonTexture);
	ScePafWidget_B6C64486(intrusive_ptr<graph::Surface>(), 0);
}

}

void ScePafToplevel_C9C02705(void *userdata)
{
	paf::Framework::framework->m_common_resource_plugin = NULL;

	paf::Framework *framework = paf::Framework::framework;

	if (framework->m_surface_pool != NULL) {
		if (framework->m_surface_pool_addr != NULL) {
			if (framework->m_surface_pool->m_type ==
			    paf::graph::SurfacePool::MemoryType_SharedClient)
				paf::GraphMem::FreeOSMemory(paf::GraphMem::DeviceType_SharedMemory,
							    framework->m_surface_pool_addr);
			else
				paf::GraphMem::FreeOSMemory(paf::GraphMem::DeviceType_VideoMemory,
							    framework->m_surface_pool_addr);
			framework->m_surface_pool_addr = NULL;
		}
		delete framework->m_surface_pool;
		framework->m_surface_pool = NULL;
	}
	if (paf::Framework::framework->m_unload_finish_callback != NULL)
		paf::Framework::framework->m_unload_finish_callback();
}

namespace paf {

Framework *Framework::Instance()
{
	return framework;
}

/* the common resource's hooks: its templates, strings and textures */
int32_t Framework::CommonTemplateOpen(ui::Widget *parent, const IDParam &id)
{
	Plugin *plugin = framework->m_common_resource_plugin;

	if (plugin == NULL)
		return -1;
	return plugin->TemplateOpen(parent, id, Plugin::TemplateOpenParam());
}

const wchar_t *Framework::CommonString(const IDParam &id)
{
	Plugin *plugin = framework->m_common_resource_plugin;

	if (plugin == NULL)
		return L"";
	return plugin->GetString(id);
}

intrusive_ptr<graph::Surface> Framework::CommonTexture(const IDParam &id)
{
	Plugin *plugin = framework->m_common_resource_plugin;

	if (plugin == NULL)
		return intrusive_ptr<graph::Surface>();
	return plugin->GetTexture(id);
}

ui::Environment *Framework::GetEnvironmentInstance()
{
	return m_env;
}

sound::Output *Framework::GetSoundInstance()
{
	return m_sound_output;
}

}

using namespace paf;

int32_t ScePafToplevel_85EF6EC0(const char *buffer1, uint32_t size1, const char *buffer2,
				uint32_t size2, int32_t samplerate, float vol, bool ignore_init,
				void *data)
{
	Framework *framework = static_cast<Framework *>(data);

	if (framework == NULL)
		return -1;

	sound::Output *output = framework->m_sound_output;

	if (output == NULL)
		return -1;
	if (buffer1 == NULL)
		return -1;
	if (vol < 0.0f)
		vol = 1.0f;
	if (buffer2 != NULL)
		output->PlaySound(buffer1, size1, buffer2, size2, samplerate, vol, ignore_init);
	else
		output->PlaySound(buffer1, size1, samplerate, vol, ignore_init);
	return 0;
}

ui::Widget *ScePafToplevel_E37EC9D8(ui::Widget *parent, const char *type, const IDParam &id,
				    const IDParam &style, void *data)
{
	Framework *framework = static_cast<Framework *>(data);

	if (framework == NULL)
		return NULL;

	Plugin *plugin = framework->m_common_resource_plugin;

	if (plugin == NULL)
		return NULL;
	return plugin->CreateWidget(parent, type, id, style);
}

namespace paf {

Framework::InitParam::InitParam()
{
	display_list_size = 0x100000;
	parameter_buffer_size = 0x200000;
	ext_control = false;
	show_focus = false;
	parameter_buffer_location = 0;
	surface_pool_size = 0x800000;
	graph_heap_size_on_main_memory = 0x100000;
	graph_heap_size_on_video_memory = 0;
	text_surface_pool_size = 0x100000;
	surf_dev_mem_addr = NULL;
	surf_dev_mem_size = 0;
	mem_pvf_japanese_addr = NULL;
	mem_pvf_japanese_size = 0;
	mem_pvf_latin_addr = NULL;
	mem_pvf_latin_size = 0;
	mem_pvf_hangul_addr = NULL;
	mem_pvf_hangul_size = 0;
	mem_pvf_chinese_addr = NULL;
	mem_pvf_chinese_size = 0;
	path_pvf_japanese = NULL;
	path_pvf_latin = NULL;
	mem_emoji_font_addr = NULL;
	mem_emoji_font_size = 0;
	pvf_option = 0;
	pvf_heap_size = 0;
	decode_heap_size = 0;
	plugin_heap_size = 0;
	image_mode = ImageMode_RGBA8888;
	screen_width = 0;
	screen_height = 0;
	mode = Mode_Normal;
	option = 0;
	locale = Locale_JP;
	enter_button_assign = 0;
	env_option = true;
	allow_button_control = true;
	env_param = NULL;
	screen_orientation_mode = 4;
	text_style = TEXT_STYLE_UNSET;
	edit_listener = NULL;
	web_edit_listener = NULL;
	message_dialog_listener = NULL;
	listener_A55215E5 = NULL;
	webview_listener = NULL;
	listener_E638AA1F = NULL;
	sound_mode = sound::Output::Mode_Normal;

	int value;

	if (sceRegMgrUtilityGetInt(REGMGR_LANGUAGE, &value) == 0)
		locale = ScePafToplevel_1AEDE4E0(value);
	if (sceRegMgrUtilityGetInt(REGMGR_ENTER_BUTTON, &value) == 0) {
		uint32_t assign = value;

		if (assign == 0)
			enter_button_assign = 0;
		else
			enter_button_assign = assign <= 1;
	}
}

Framework::CommonResourceInitParam::CommonResourceInitParam()
{
	resource_data = NULL;
	resource_data_size = 0;
	private_heap_size = 0;
	private_heap = NULL;
	surface_pool = NULL;
	draw_priority = -1;
}


/* UnloadRunAction's step on the main thread */
int32_t Framework::UnloadPreactionCall(void *data)
{
	DestructPluginData *unload = (DestructPluginData *)data;

	return unload->framework->UnloadPreaction(unload->plugin);
}

}
