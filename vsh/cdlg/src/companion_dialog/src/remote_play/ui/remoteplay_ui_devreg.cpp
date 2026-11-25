/*
 * SceCompanionDialogPlugin: RemotePlay device registration UI
 * (vsh/cdlg/src/companion_dialog/src/remote_play/ui/remoteplay_ui_devreg.cpp).
 */
#include "remoteplay_ui_devreg.h"
#include "../remoteplay_db.h"
#include <versatileparam/idparam.h>
#include <common/main_thread_call_list.h>
#include <widget/w_button.h>
#include <widget/w_text_box.h>
#include <math/ngp/v4.h>
#include <thread/thread.h>
#include "../../../../tw_login_dialog/tw_dialog_common.h"

extern "C" {
void *sce_paf_memcpy(void *dst, const void *src, SceSize size);
void *sce_paf_memset(void *dst, int c, SceSize size);
SceInt32 CompanionRegistrySaveOrbis(const char *ps4Mac, const char *ps4RegistKey);
bool CompanionIsRemotePlayMode();
}

static const SceUInt32 kHashBtnCancel = 0x8518E281U;
static const SceUInt32 kHashBtnRegister = 0x961B7E9CU;
static const SceUInt32 kHashPinTextBox = 0x57B8480DU;
static const SceUInt32 kHashPinContainer = 0x3E43E7C1U;
static const SceUInt32 kHashWaitCancelBtn = 0x084D3801U;
static const SceUInt32 kHashCommonErrorMsg = 0xC51AE344U;

static RemotePlayApp *g_remotePlayApp = 0;
static RemotePlayDB *g_remotePlayDb = 0;
static SceInt32 g_netInitResult = 0;
static SceInt32 g_pinPageState = 0;
static SceInt32 g_registWaitState = 0;

static void OnRegistCompleteMainThread(void *userData)
{
	SceInt32 result = (SceInt32)(SceIntPtr)userData;
	if (result == 0) {
		g_registWaitState = 2;
	} else {
		g_registWaitState = 3;
		RemotePlayUiShowErrorDialog(result);
	}
}

static void OnRegistWorkerFinished(SceInt32 result, const RemotePlayRecord *record, const uint8_t *mac)
{
	(void)mac;
	if (g_remotePlayApp == 0)
		return;
	SceInt32 finalResult = result;
	if (finalResult == 0 && record != 0) {
		SceInt32 regRet = CompanionRegistrySaveOrbis(record->ps4Mac, record->ps4RegistKey);
		if (regRet < 0) {
			finalResult = regRet;
		} else if (CompanionIsRemotePlayMode()) {
			SceInt32 dbRet = RemotePlayUiSaveRegistration(
				record->ps4Mac, record->ps4RegistKey,
				record->apSsid, record->apBssid, record->apKey, record->rpKey);
			if (dbRet < 0)
				finalResult = dbRet;
		}
	}
	paf::common::MainThreadCallList::Register(OnRegistCompleteMainThread, (void *)(SceIntPtr)finalResult);
}

static void OnPinCancelClicked(SceInt32 eventId, paf::ui::Handler *handler,
	paf::ui::Event *event, void *userData)
{
	(void)eventId;
	(void)event;
	(void)userData;
	paf::ui::ButtonBase *btn = static_cast<paf::ui::ButtonBase *>(handler);
	if (btn != 0)
		btn->Disable(false);
	g_pinPageState = 2;
}

static void OnPinRegisterClicked(SceInt32 eventId, paf::ui::Handler *handler,
	paf::ui::Event *event, void *userData)
{
	(void)eventId;
	(void)event;
	(void)userData;
	paf::ui::ButtonBase *btn = static_cast<paf::ui::ButtonBase *>(handler);
	if (btn != 0)
		btn->Disable(false);
	g_pinPageState = 3;
}

static void OnWaitCancelClicked(SceInt32 eventId, paf::ui::Handler *handler,
	paf::ui::Event *event, void *userData)
{
	(void)eventId;
	(void)event;
	(void)userData;
	paf::ui::ButtonBase *btn = static_cast<paf::ui::ButtonBase *>(handler);
	if (btn != 0)
		btn->Disable(false);
	if (g_remotePlayApp != 0)
		g_remotePlayApp->AbortDeviceRegister();
}

static void OnPinKeyboardOpen(SceInt32 eventId, paf::ui::Handler *handler,
	paf::ui::Event *event, void *userData)
{
	(void)eventId;
	(void)handler;
	(void)event;
	paf::ui::Widget *page = static_cast<paf::ui::Widget *>(userData);
	if (page == 0)
		return;
	paf::ui::Widget *container = page->FindChild(paf::IDParam(kHashPinContainer), 0);
	if (container != 0) {
		paf::math::v4 shifted(0.0f, 180.0f, 0.0f, 0.0f);
		container->SetPos(shifted);
	}
}

static void OnPinKeyboardClose(SceInt32 eventId, paf::ui::Handler *handler,
	paf::ui::Event *event, void *userData)
{
	(void)eventId;
	(void)handler;
	(void)event;
	paf::ui::Widget *page = static_cast<paf::ui::Widget *>(userData);
	if (page == 0)
		return;
	paf::ui::Widget *container = page->FindChild(paf::IDParam(kHashPinContainer), 0);
	if (container != 0) {
		paf::math::v4 origin(0.0f, 0.0f, 0.0f, 0.0f);
		container->SetPos(origin);
	}
}

RemotePlayDeviceRegisterThread::RemotePlayDeviceRegisterThread(SceUInt32 pin, RemotePlayRegistCallback cb)
	: paf::thread::Thread(0x10000104, 0x4000, "SceRemotePlayDeviceRegister", 0)
	, m_pin(pin)
	, m_callback(cb)
{
	RemotePlaySessionInit(&m_session, 2);
}

RemotePlayDeviceRegisterThread::~RemotePlayDeviceRegisterThread()
{
}

void RemotePlayDeviceRegisterThread::EntryFunction()
{
	RemotePlayRecord record;
	uint8_t localMac[8];
	sce_paf_memset(&record, 0, sizeof(record));
	sce_paf_memset(localMac, 0, sizeof(localMac));
	RemotePlaySessionInit(&m_session, 2);
	SceInt32 ret = RemotePlaySessionRegister(&m_session, m_pin, &record, localMac);
	if (m_callback != 0)
		m_callback(ret, &record, localMac);
}

void RemotePlayDeviceRegisterThread::RequestAbort()
{
	RemotePlaySessionAbort(&m_session);
}

RemotePlayApp::RemotePlayApp()
	: m_plugin(0)
	, m_mutex("SceRemotePlayApp", 0)
	, m_pin(0)
	, m_worker(0)
{
}

RemotePlayApp::~RemotePlayApp()
{
	while (m_worker != 0)
		paf::thread::Sleep(16);
}

void RemotePlayApp::OnMainThreadTick(void *userData)
{
	(void)userData;
	RemotePlayNetCheckCallback();
}

SceInt32 RemotePlayApp::OnLoad(paf::Plugin *plugin)
{
	g_netInitResult = RemotePlayNetInit();
	if (g_netInitResult == 0) {
		RemotePlayNetSetKeepAlive(0);
		m_plugin = plugin;
		paf::common::MainThreadCallList::Register(OnMainThreadTick, 0);
	}
	return g_netInitResult;
}

SceInt32 RemotePlayApp::OnStart(paf::Plugin *plugin)
{
	if (g_netInitResult != 0 && plugin != 0)
		RemotePlayUiShowErrorDialog(g_netInitResult);
	return 0;
}

SceInt32 RemotePlayApp::OnStop(paf::Plugin *plugin)
{
	(void)plugin;
	if (g_netInitResult == 0) {
		paf::common::MainThreadCallList::Unregister(OnMainThreadTick, 0);
		RemotePlayNetTerm();
	}
	return g_netInitResult;
}

SceInt32 RemotePlayApp::OnUnload(paf::Plugin *plugin)
{
	(void)plugin;
	return 0;
}

void RemotePlayApp::StartDeviceRegister(RemotePlayRegistCallback cb)
{
	m_mutex.Lock();
	if (m_worker == 0) {
		m_worker = new RemotePlayDeviceRegisterThread(m_pin, cb);
		if (m_worker != 0)
			m_worker->Start();
	}
	m_mutex.Unlock();
}

void RemotePlayApp::AbortDeviceRegister()
{
	m_mutex.Lock();
	if (m_worker != 0)
		m_worker->RequestAbort();
	m_mutex.Unlock();
}

void RemotePlayApp::JoinDeviceRegister()
{
	m_mutex.Lock();
	if (m_worker != 0) {
		m_worker->Join();
		delete m_worker;
		m_worker = 0;
	}
	m_mutex.Unlock();
}

SceInt32 RemotePlayUiStart(paf::Plugin *plugin)
{
	if (g_remotePlayApp == 0) {
		g_remotePlayApp = new RemotePlayApp();
		if (g_remotePlayApp != 0) {
			g_remotePlayApp->OnLoad(plugin);
			g_remotePlayApp->OnStart(plugin);
		}
	}
	return 0;
}

void RemotePlayUiStop(paf::Plugin *plugin)
{
	if (g_remotePlayApp != 0) {
		g_remotePlayApp->AbortDeviceRegister();
		g_remotePlayApp->JoinDeviceRegister();
		g_remotePlayApp->OnStop(plugin);
		g_remotePlayApp->OnUnload(plugin);
		delete g_remotePlayApp;
		g_remotePlayApp = 0;
	}
}

SceInt32 RemotePlayUiGetPinState()
{
	return g_pinPageState;
}

SceInt32 RemotePlayUiGetRegistState()
{
	return g_registWaitState;
}

void RemotePlayUiInitPinPage(paf::ui::Widget *pageWidget)
{
	if (pageWidget == 0)
		return;
	paf::ui::Widget *cancelBtn = pageWidget->FindChild(paf::IDParam(kHashBtnCancel), 0);
	if (cancelBtn != 0)
		cancelBtn->AddEventCallback(0x10000008, OnPinCancelClicked, 0);

	paf::ui::Widget *regBtn = pageWidget->FindChild(paf::IDParam(kHashBtnRegister), 0);
	if (regBtn != 0) {
		regBtn->AddEventCallback(0x10000008, OnPinRegisterClicked, 0);
		static_cast<paf::ui::ButtonBase *>(regBtn)->Disable(false);
	}

	paf::ui::Widget *pinBox = pageWidget->FindChild(paf::IDParam(kHashPinTextBox), 0);
	if (pinBox != 0) {
		pinBox->AddEventCallback(0x10000007, OnPinKeyboardOpen, pageWidget);
		pinBox->AddEventCallback(0x10000008, OnPinKeyboardClose, pageWidget);
		g_pinPageState = 1;
	}
}

void RemotePlayUiInitWaitPage(paf::ui::Widget *pageWidget)
{
	if (pageWidget == 0)
		return;
	paf::ui::Widget *cancelBtn = pageWidget->FindChild(paf::IDParam(kHashWaitCancelBtn), 0);
	if (cancelBtn != 0)
		cancelBtn->AddEventCallback(0x10000008, OnWaitCancelClicked, 0);
	if (g_remotePlayApp != 0)
		g_remotePlayApp->StartDeviceRegister(OnRegistWorkerFinished);
	g_registWaitState = 1;
}

extern "C" {
void SceCommonGuiDialog_696DB9C9(void *param);
SceInt32 SceCommonGuiDialog_A6422883(paf::Plugin *plugin, void *param, void *handler, SceInt32 flags);
}

void RemotePlayUiShowErrorDialog(SceInt32 errorCode)
{
	(void)errorCode;
	uint8_t paramBuf[200];
	SceCommonGuiDialog_696DB9C9(paramBuf);
	SceCommonGuiDialog_A6422883(0, paramBuf, 0, 0);
}

SceInt32 RemotePlayUiSaveRegistration(const char *ps4Mac, const char *ps4RegistKey,
	const char *apSsid, const char *apBssid, const char *apKey, const uint8_t *rpKey)
{
	if (g_remotePlayDb == 0) {
		g_remotePlayDb = new RemotePlayDB();
		if (g_remotePlayDb == 0)
			return -1;
	}
	RemotePlayRecord rec;
	sce_paf_memcpy(&rec, g_remotePlayDb->GetRecord(), sizeof(rec));
	if (g_remotePlayDb->IsLocked() == 0) {
		sce_paf_memcpy(rec.apSsid, apSsid, sizeof(rec.apSsid));
		sce_paf_memcpy(rec.apBssid, apBssid, sizeof(rec.apBssid));
		sce_paf_memcpy(rec.apKey, apKey, sizeof(rec.apKey));
		sce_paf_memset(rec.apName, 0, sizeof(rec.apName));
		rec.rpKeyType = 0;
		sce_paf_memcpy(rec.rpKey, rpKey, sizeof(rec.rpKey));
		sce_paf_memset(rec.ps4Nickname, 0, sizeof(rec.ps4Nickname));
		sce_paf_memcpy(rec.ps4Mac, ps4Mac, sizeof(rec.ps4Mac));
		sce_paf_memcpy(rec.ps4RegistKey, ps4RegistKey, sizeof(rec.ps4RegistKey));
		g_remotePlayDb->SetRecord(&rec);
		g_remotePlayDb->Flush();
	}
	return 0;
}
