/*
 * paf::Framework: plugins are loaded and unloaded through the list it keeps
 * under m_list_lock; it ends once a shutdown was requested and nothing runs.
 */
#include <framework.h>
#include <power.h>
#include <kernel/threadmgr.h>
#include <graphics/gutil.h>
#include <job/job.h>
#include <timer/timerlistenerlist.h>
#include <widget/core/environment.h>
#include <widget/core/wutil.h>
#include "avout/sound.h"
#include "paf_types.h"

namespace paf {

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
