/*
 * UpdateGuiView (sys/internal/usermode/src/psp2swu/update_gui_view.cpp).
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <framework.h>
#include <plugin.h>
#include <inputdevice/inputlistener.h>
#include <thread/thread.h>
#include <src/avout/sound.h>
#include "psp2swu_app.h"
#include "update_gui_view.h"
#include "updater_plugin.h"

#define COMMON_RESOURCE_RCO_SIZE	261440U
#define UPDATER_PLUGIN_RCO_SIZE		185808U
#define LATIN_FONT_OTF_SIZE		15456U
#define JAPANESE_FONT_OTF_SIZE		132608U
#define KOREAN_FONT_OTF_SIZE		46016U
#define CHINESE_FONT_TTF_SIZE		50916U

extern "C" {
extern uint8_t g_commonResourceRco[];
extern uint8_t g_updaterPluginRco[];
extern uint8_t g_latinFontOtf[];
extern uint8_t g_japaneseFontOtf[];
extern uint8_t g_koreanFontOtf[];
extern uint8_t g_chineseFontTtf[];
}

UpdateGuiView *UpdateGuiView::s_instance;

static void OnCommonResourceLoaded()
{
}

static void OnPluginLoaded(paf::Plugin *plugin)
{
}

UpdateGuiView::UpdateGuiView()
	: UpdateViewBase(), m_framework(NULL), m_updateResult(0)
{
	s_instance = this;
}

UpdateGuiView::~UpdateGuiView()
{
}

SceInt32 UpdateGuiView::RunUpdate()
{
	m_updateResult = ExecuteUpdate();
	return m_updateResult;
}

SceInt32 UpdateGuiView::GetProgress(SceInt32 *outRate, SceUInt64 *outRemainingMs)
{
	return SwuApplication::GetInstance()->GetProgress(outRate, outRemainingMs);
}

SceInt32 UpdateGuiView::GetEstimateUpdateTime(SceUInt64 *outUpdateMs)
{
	SwuApplication *app = SwuApplication::GetInstance();
	const char *pupPath = app->GetPupPath();

	return app->GetEstimateUpdateTime(pupPath, outUpdateMs);
}

SceInt32 UpdateGuiView::Initialize()
{
	paf::Framework::InitParam initParam;
	paf::common::SharedPtr<paf::inputdevice::InputListener> inputListener;

	initParam.mode = paf::Framework::Mode_Normal;
	initParam.mem_pvf_latin_addr = g_latinFontOtf;
	initParam.mem_pvf_latin_size = LATIN_FONT_OTF_SIZE;
	initParam.mem_pvf_japanese_addr = g_japaneseFontOtf;
	initParam.mem_pvf_japanese_size = JAPANESE_FONT_OTF_SIZE;
	initParam.mem_pvf_chinese_addr = g_chineseFontTtf;
	initParam.mem_pvf_chinese_size = CHINESE_FONT_TTF_SIZE;
	initParam.mem_pvf_hangul_addr = g_koreanFontOtf;
	initParam.mem_pvf_hangul_size = KOREAN_FONT_OTF_SIZE;

	paf::Framework::SampleInit(&initParam);
	m_framework = new paf::Framework(initParam);

	paf::Framework::CommonResourceInitParam commonResParam;

	commonResParam.resource_data = g_commonResourceRco;
	commonResParam.resource_data_size = COMMON_RESOURCE_RCO_SIZE;
	m_framework->LoadCommonResourceSync(commonResParam, OnCommonResourceLoaded);

	paf::Plugin::InitParam pluginParam;

	pluginParam.name = "updater_plugin";
	pluginParam.resource_data = g_updaterPluginRco;
	pluginParam.resource_data_size = UPDATER_PLUGIN_RCO_SIZE;
	pluginParam.caller_name = "__main__";
	pluginParam.set_param_func = OnUpdaterPluginSetParam;
	pluginParam.init_func = OnUpdaterPluginInit;
	pluginParam.start_func = OnUpdaterPluginStart;
	pluginParam.stop_func = OnUpdaterPluginStop;
	pluginParam.exit_func = OnUpdaterPluginExit;
	paf::Plugin::LoadSync(pluginParam, OnPluginLoaded, 0);

	paf::ui::Environment *env = m_framework->GetEnvironmentInstance();

	inputListener = env->m_pad_gesture_listener;
	paf::inputdevice::AddInputListener(inputListener);
	m_framework->GetSoundInstance()->EnableSound(false);
	return 0;
}

SceInt32 UpdateGuiView::Operate()
{
	m_framework->Run();
	return 0;
}

SceInt32 UpdateGuiView::Finalize()
{
	if (m_framework != NULL)
		delete m_framework;
	return 0;
}

SceInt32 UpdateGuiView::OnVerifyFinished()
{
	paf::thread::CallQueue::main_thread_callqueue.Enqueue(
		"System Updater", OnVerifyFinishedMainThread, NULL);
	return 0;
}

SceInt32 UpdateGuiView::OnUpdateFinished()
{
	SceInt32 delayRet = 0;

	g_updaterComplete = 1;
	for (SceInt32 step = 5; step != 0; step--) {
		sceKernelDelayThread(100000);
		delayRet = sceKernelDelayThread(100000);
	}
	m_framework->RequestShutdown();
	if (m_updateResult != 0)
		return m_updateResult;
	return delayRet;
}

void UpdateGuiView::OnReleaseCheckError(SceInt32 errorCode)
{
}

SceInt32 UpdateGuiView::GetRetryCount(SceInt32 *outRetryCount)
{
	return SwuApplication::GetInstance()->GetRetryCount(outRetryCount);
}

SceInt32 UpdateGuiView::IsDiffUpdate()
{
	return SwuApplication::GetInstance()->IsDiffUpdate();
}

SceInt32 UpdateGuiView::HasUpdateFlagError()
{
	return SwuApplication::GetInstance()->HasUpdateFlagError();
}

bool UpdateGuiView::IsFactoryMode()
{
	return SwuApplication::GetInstance()->IsFactoryMode();
}

UpdateGuiView *UpdateGuiView::GetInstance()
{
	return s_instance;
}
