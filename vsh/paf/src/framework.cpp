/*
 * paf::Framework: plugins are loaded and unloaded through the list it keeps
 * under m_list_lock; it ends once a shutdown was requested and nothing runs.
 */
#include <framework.h>
#include <power.h>
#include <kernel/threadmgr.h>
#include <sysmem/memblock.h>
#include <datetime/datetime.h>
#include <file/localfile.h>
#include <graphics/gutil.h>
#include <graphics/surface/surface_display.h>
#include <graphics/text/text_init.h>
#include <image/image_types.h>
#include <inputdevice/pad/pad.h>
#include <job/job.h>
#include <std/stdio.h>
#include <std/stdlib.h>
#include <system/system.h>
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
	text_param.path_pvf_hangul = init_param.path_pvf_hangul;
	text_param.path_pvf_chinese = init_param.path_pvf_chinese;
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

graph::SurfacePool *Framework::GetSurfacePool() const
{
	return gutil::GetDefaultSurfacePool();
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
	path_pvf_hangul = NULL;
	path_pvf_chinese = NULL;
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

}
