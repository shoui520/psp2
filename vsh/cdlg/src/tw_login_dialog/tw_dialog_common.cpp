/*
 * SceTwLoginDialogPlugin / SceTwitterDialogPlugin: shared dialog framework (page manager,
 * error dialog, wait dialog, infobar control, and async job runner).
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <appmgr/appmgr.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include <std/string.h>
#include <common/shared_ptr.h>
#include <job/job.h>
#include <memory/heap_allocator.h>
#include "tw_dialog_common.h"

extern "C" {
paf::memory::HeapAllocator *GetGlobalHeapAllocator();
int sceAppMgrSetInfobarStateForCommonDialog(int visibility, int color, int transparency);
}

using namespace paf;

/* Adapter for CommonGuiDialogHandler */
class TwCommonGuiHandler : public CommonGuiDialogHandler {
public:
	typedef int (*CallbackFn)(void *self, int event, int buttonId);

	TwCommonGuiHandler(void *self, CallbackFn fn) : m_self(self), m_fn(fn), m_reserved(0) {}
	virtual ~TwCommonGuiHandler() {}
	virtual void operator()(SceInt32 value, SceInt32 button)
	{
		if (m_fn != NULL)
			m_fn(m_self, value, button);
	}

	void *m_self;
	CallbackFn m_fn;
	int m_reserved;
};

/* Internal error codes displayed in TwErrorDialog */
#define TW_ERR_UNAUTHORIZED	((SceInt32)0x80106096)
#define TW_ERR_NET_405		((SceInt32)0x80106097)
#define TW_ERR_NET_401		((SceInt32)0x80106098)
#define TW_ERR_NO_MEMORY	((SceInt32)0x80106099)

/* String resource IDs */
#define TW_STR_AUTH_FAILED_ID	0xD19242C1U
#define TW_STR_CONN_FAILED_ID	0x30067CF1U
#define TW_STR_ERROR_PREFIX_ID	0xC51B20E4U

static TwPageManager *s_activePageManager = NULL;
static void *s_pluginContext = NULL;

/* Wait dialog style table (mirrors SceCommonGuiDialog layout globals) */
struct TwWaitDialogStyle {
	const void *bgLayout;
	const void *frameLayout;
	const void *contentLayout;
};

static TwWaitDialogStyle s_waitDialogStyles[5];
static bool s_waitStylesInit = false;

static void InitWaitDialogStyles()
{
	if (s_waitStylesInit)
		return;
	s_waitDialogStyles[0].bgLayout = &SceCommonGuiDialog_F8B2D7E1;
	s_waitDialogStyles[0].frameLayout = &SceCommonGuiDialog_2A68B4A4;
	s_waitDialogStyles[0].contentLayout = &SceCommonGuiDialog_EF2C5382;

	s_waitDialogStyles[1].bgLayout = &SceCommonGuiDialog_EF2C5382;
	s_waitDialogStyles[1].frameLayout = &SceCommonGuiDialog_6CD802FC;
	s_waitDialogStyles[1].contentLayout = (const void *)0x4740;

	s_waitDialogStyles[2].bgLayout = &SceCommonGuiDialog_AF75CA6D;
	s_waitDialogStyles[2].frameLayout = &SceCommonGuiDialog_F8B2D7E1;
	s_waitDialogStyles[2].contentLayout = &SceCommonGuiDialog_2A68B4A4;

	s_waitDialogStyles[3].bgLayout = &SceCommonGuiDialog_EF7F04DD;
	s_waitDialogStyles[3].frameLayout = &SceCommonGuiDialog_AF75CA6D;
	s_waitDialogStyles[3].contentLayout = &SceCommonGuiDialog_F8B2D7E1;

	s_waitDialogStyles[4].bgLayout = &SceCommonGuiDialog_6CD802FC;
	s_waitDialogStyles[4].frameLayout = (const void *)0x4740;
	s_waitDialogStyles[4].contentLayout = (const void *)0x2F01;
	s_waitStylesInit = true;
}

void TwInitPluginContext(Plugin *plugin)
{
	if (plugin != NULL)
		s_pluginContext = *(void **)((uint8_t *)plugin + 212);
}

/* TwPageManager */
TwPageManager::TwPageManager()
	: m_plugin(NULL), m_entries(NULL), m_initTable(NULL)
{
	s_activePageManager = this;
	memory::HeapAllocator *heap = GetGlobalHeapAllocator();
	memory::HeapInfo info;

	if (heap != NULL)
		heap->QueryInfo(&info);
}

TwPageManager::~TwPageManager()
{
	if (m_initTable != NULL) {
		delete[] m_initTable;
		m_initTable = NULL;
	}
	s_activePageManager = NULL;
}

void TwPageManager::Init(Plugin *plugin)
{
	m_plugin = plugin;
	int count = 0;

	while (m_entries[count].pageIdHash != 0 || m_entries[count].handler != NULL)
		++count;
	if (count == 0)
		return;

	m_initTable = new InitEventFunctionEntry[count + 1];
	for (int i = 0; i < count; ++i) {
		m_initTable[i].id = IDParam("", m_entries[i].pageIdHash);
		m_initTable[i].func = OnPluginPageInit;
	}
	m_initTable[count].id = IDParam("", 0);
	m_initTable[count].func = NULL;
	m_plugin->SetInitEventHandler(m_initTable);
}

void TwPageManager::Term()
{
	m_plugin = NULL;
}

void TwPageManager::CloseAll()
{
	if (m_plugin != NULL)
		m_plugin->PageCloseAll();
}

void TwPageManager::RegisterHandler(uint32_t pageIdHash, TwPageHandler *handler)
{
	for (int i = 0; m_entries[i].pageIdHash != 0 || m_entries[i].handler != NULL; ++i) {
		if (m_entries[i].pageIdHash == pageIdHash) {
			m_entries[i].handler = handler;
			return;
		}
	}
}

void TwPageManager::UnregisterHandler(uint32_t pageIdHash)
{
	for (int i = 0; m_entries[i].pageIdHash != 0 || m_entries[i].handler != NULL; ++i) {
		if (m_entries[i].pageIdHash == pageIdHash) {
			m_entries[i].handler = NULL;
			return;
		}
	}
}

void TwPageManager::OpenPage(uint32_t pageIdHash)
{
	Plugin::PageOpenParam param;
	IDParam id("", pageIdHash);

	m_plugin->PageOpen(id, param);
}

void TwPageManager::ClosePage(uint32_t pageIdHash)
{
	Plugin::PageCloseParam param;
	IDParam id("", pageIdHash);

	m_plugin->PageClose(id, param);
}

void TwPageManager::OpenPageWithEffect(uint32_t pageIdHash, float duration, int effectType)
{
	Plugin::PageOpenParam param;

	param.fade_time_ms = duration;
	param.transition_type = (Plugin::TransitionType)effectType;
	IDParam id("", pageIdHash);

	m_plugin->PageOpen(id, param);
}

void TwPageManager::ClosePageWithEffect(uint32_t pageIdHash, float duration, int effectType)
{
	Plugin::PageCloseParam param;

	param.fade_time_ms = duration;
	param.transition_type = (Plugin::TransitionType)effectType;
	IDParam id("", pageIdHash);

	m_plugin->PageClose(id, param);
}

void TwPageManager::OpenPageWithAnim(uint32_t pageIdHash, int animType)
{
	Plugin::PageOpenParam param;

	param.transition_type = (Plugin::TransitionType)animType;
	IDParam id("", pageIdHash);

	m_plugin->PageOpen(id, param);
}

void TwPageManager::ClosePageWithAnim(uint32_t pageIdHash, int animType)
{
	Plugin::PageCloseParam param;

	param.transition_type = (Plugin::TransitionType)animType;
	IDParam id("", pageIdHash);

	m_plugin->PageClose(id, param);
}

void TwPageManager::OnPluginPageInit(int32_t type, ui::Handler *self, ui::Event *ev,
				     void *userdata)
{
	(void)type;
	(void)ev;
	(void)userdata;
	ui::Widget *widget = (ui::Widget *)self;

	if (s_activePageManager == NULL || widget == NULL)
		return;
	uint32_t hash = *(const uint32_t *)((const uint8_t *)widget + 332);

	for (int i = 0; s_activePageManager->m_entries[i].pageIdHash != 0 ||
			s_activePageManager->m_entries[i].handler != NULL;
	     ++i) {
		if (s_activePageManager->m_entries[i].pageIdHash == hash) {
			if (s_activePageManager->m_entries[i].handler != NULL)
				s_activePageManager->m_entries[i].handler->OnPageInit(widget);
			return;
		}
	}
}

/* TwErrorDialog */
TwErrorDialog::TwErrorDialog()
	: m_plugin(NULL), m_dialogId(-1), m_errorCode(0), m_errorDetail(), m_useSystemMessage(1),
	  m_message(), m_callback(NULL), m_userData(NULL)
{
	m_padding[0] = 0;
	m_padding[1] = 0;
	m_padding[2] = 0;
}

void TwErrorDialog::Init(Plugin *plugin)
{
	m_plugin = plugin;
	m_errorCode = 0;
}

void TwErrorDialog::SetError(SceInt32 errorCode, const LxString &detail)
{
	switch (errorCode) {
	case LX_ERROR_HTTP_NO_MEM_POOL:
		errorCode = TW_ERR_NO_MEMORY;
		break;
	case LX_ERROR_NET_405:
		errorCode = TW_ERR_NET_405;
		break;
	case LX_ERROR_NET_401:
		errorCode = TW_ERR_NET_401;
		break;
	case LX_ERROR_OAUTH_UNAUTHORIZED:
		errorCode = TW_ERR_UNAUTHORIZED;
		break;
	default:
		break;
	}
	m_errorCode = errorCode;
	m_errorDetail = detail;
	m_message.clear();

	if (m_errorCode == TW_ERR_UNAUTHORIZED) {
		const wchar_t *msg = LxGetLocalizedString(IDParam("", TW_STR_AUTH_FAILED_ID));

		if (msg != NULL)
			m_message.assign(msg, sce_paf_wcslen(msg));
		m_useSystemMessage = 0;
		return;
	}
	if (m_errorCode == TW_ERR_NET_405) {
		const wchar_t *msg = LxGetLocalizedString(IDParam("", TW_STR_CONN_FAILED_ID));

		if (msg != NULL)
			m_message.assign(msg, sce_paf_wcslen(msg));
		m_useSystemMessage = 0;
		return;
	}
	if (!m_errorDetail.Empty()) {
		const wchar_t *prefix = m_plugin->GetString(IDParam("", TW_STR_ERROR_PREFIX_ID));

		if (prefix != NULL)
			m_message.assign(prefix, sce_paf_wcslen(prefix));
		LxWString detailWide;

		LxUtf8ToWString(detailWide, m_errorDetail);
		m_message += L"\n(";
		m_message += detailWide.CStr();
		m_message += L")";
		m_useSystemMessage = 0;
	}
}

void TwErrorDialog::Show(CloseCallback callback, void *userData, int16_t mode, int16_t flags)
{
	m_callback = callback;
	m_userData = userData;
	if (m_errorCode >= 0)
		return;

	CommonGuiMessageDialogParam param;

	param.type = m_errorCode;
	*(uint32_t *)&param.option =
		((uint32_t)((uint16_t)flags | ((uint16_t)mode << 12)) << 8) | 0x11U;
	param.buttonCount = 1;
	param.message = m_message;
	param.handler = new TwCommonGuiHandler(this, OnDialogResult);

	m_dialogId = SceCommonGuiDialog_20B141F1(&param);
	if (m_useSystemMessage == 0) {
		ui::Widget *textWidget = SceCommonGuiDialog_8947FC23(m_dialogId, 1);

		if (textWidget != NULL)
			textWidget->SetString(m_message);
	}
	m_message.clear();
	m_errorCode = 0;
	m_useSystemMessage = 1;
}

void TwErrorDialog::Close()
{
	SceCommonGuiDialog_BC311DC0(m_dialogId);
	m_dialogId = -1;
}

int TwErrorDialog::OnDialogResult(void *selfPtr, int event, int buttonId)
{
	(void)event;
	TwErrorDialog *self = (TwErrorDialog *)selfPtr;

	if (self->m_callback != NULL) {
		self->m_callback(buttonId, self->m_userData);
		self->m_callback = NULL;
		self->m_userData = NULL;
		return 0;
	}
	return buttonId;
}

/* TwInfobar */
int TwInfobar::Show()
{
	return sceAppMgrSetInfobarStateForCommonDialog(1, 0, 0);
}

int TwInfobar::Hide()
{
	return sceAppMgrSetInfobarStateForCommonDialog(0, 0, 0);
}

/* TwJobItem */
TwJobItem::TwJobItem(void *userData, TwJobCallback *callback, TwJobObserver *observer,
		     const char *name)
	: common::SlotJobItem(name, NULL), m_callback(callback), m_observer(observer),
	  m_userData(userData)
{
	m_callback->OnJobStart(m_userData);
	common::SharedPtr<job::JobItem> item(this);

	job::JobQueue::default_queue->Enqueue(item);
	m_observer->OnJobAdded(this);
}

TwJobItem::~TwJobItem()
{
}

int TwJobItem::Run()
{
	if (!IsCanceled())
		return m_callback->OnJobRun(m_userData, this);
	return 0;
}

void TwJobItem::Finish(int result)
{
	if (IsCanceled())
		m_callback->OnJobCancel(m_userData, result);
	else
		m_callback->OnJobSuccess(m_userData, result);
	m_observer->OnJobRemoved(this);
}

/* TwJobController */
TwJobController::TwJobController()
	: m_jobs(), m_pendingCancelCount(0), m_cancelDoneCb(NULL), m_cancelDoneArg(NULL),
	  m_startFn(NULL), m_startCtx(NULL), m_runFn(NULL), m_runCtx(NULL), m_completeFn(NULL),
	  m_completeCtx(NULL), m_cancelFn(NULL), m_cancelCtx(NULL), m_forceCancel(0)
{
}

TwJobController::~TwJobController()
{
	m_jobs.clear();
}

void TwJobController::OnJobStart(void *userData)
{
	if (m_startFn != NULL)
		m_startFn(userData, m_startCtx);
}

int TwJobController::OnJobRun(void *userData, TwJobItem *job)
{
	if (m_runFn != NULL)
		return m_runFn(userData, job, m_runCtx);
	return 0;
}

void TwJobController::OnJobSuccess(void *userData, int result)
{
	if (m_completeFn != NULL)
		m_completeFn(result, userData, m_completeCtx);
}

void TwJobController::OnJobCancel(void *userData, int result)
{
	if (m_cancelFn != NULL)
		m_cancelFn(result, userData, m_cancelCtx, m_forceCancel != 0);
}

void TwJobController::OnJobAdded(TwJobItem *job)
{
	for (size_t i = 0; i < m_jobs.size(); ++i) {
		if (m_jobs[i] == NULL) {
			m_jobs[i] = job;
			return;
		}
	}
	m_jobs.push_back(job);
}

void TwJobController::OnJobRemoved(TwJobItem *job)
{
	for (size_t i = 0; i < m_jobs.size(); ++i) {
		if (m_jobs[i] == job) {
			m_jobs[i] = NULL;
			break;
		}
	}
	if (m_cancelDoneCb != NULL && m_pendingCancelCount > 0) {
		--m_pendingCancelCount;
		if (m_pendingCancelCount == 0) {
			m_cancelDoneCb(m_cancelDoneArg);
			m_cancelDoneCb = NULL;
			m_cancelDoneArg = NULL;
		}
	}
}

int TwJobController::CancelAll(AllCanceledFn doneCb, void *userData, bool force)
{
	m_cancelDoneCb = doneCb;
	m_cancelDoneArg = userData;
	m_pendingCancelCount = 0;
	m_forceCancel = force ? 1 : 0;
	for (size_t i = 0; i < m_jobs.size(); ++i) {
		if (m_jobs[i] != NULL) {
			m_jobs[i]->Cancel();
			++m_pendingCancelCount;
		}
	}
	return m_pendingCancelCount;
}

/* TwWaitDialog */
TwWaitDialog::TwWaitDialog()
	: m_dialogId(-1), m_callback(NULL), m_userData(NULL)
{
	InitWaitDialogStyles();
}

TwWaitDialog::~TwWaitDialog()
{
	Close();
}

int TwWaitDialog::Open(Plugin *plugin, const wstring &message, int styleIndex,
		       CloseCallback callback, void *userData)
{
	InitWaitDialogStyles();
	CommonGuiLayoutDialogParam param;

	param.resourcePlugin = plugin;
	param.text = message;
	param.layout.page = (const CommonGuiDialogPage *)s_waitDialogStyles[styleIndex].bgLayout;
	param.layout.parts =
		(const ui::Dialog::PartsList *)s_waitDialogStyles[styleIndex].frameLayout;
	param.layout.buttons =
		(const CommonGuiDialogButton *)s_waitDialogStyles[styleIndex].contentLayout;
	param.handler = new TwCommonGuiHandler(this, OnDialogClosed);

	m_callback = callback;
	m_userData = userData;
	int dialogId = SceCommonGuiDialog_CCFCD9A6(&param);

	if (dialogId >= 0)
		m_dialogId = dialogId;
	return dialogId;
}

int TwWaitDialog::Close()
{
	if (m_dialogId < 0)
		return -1;
	SceCommonGuiDialog_BC311DC0(m_dialogId);
	m_dialogId = -1;
	m_callback = NULL;
	m_userData = NULL;
	return 0;
}

int TwWaitDialog::OnDialogEvent(void *selfPtr, int event, int result)
{
	(void)event;
	TwWaitDialog *self = (TwWaitDialog *)selfPtr;

	if (self->m_callback != NULL) {
		self->m_callback(result, self->m_userData);
		self->m_callback = NULL;
		self->m_userData = NULL;
		return 0;
	}
	return result;
}

int TwWaitDialog::OnDialogClosed(void *selfPtr, int event, int result)
{
	(void)event;
	TwWaitDialog *self = (TwWaitDialog *)selfPtr;

	if (self->m_callback != NULL) {
		self->m_callback(result, self->m_userData);
		self->m_callback = NULL;
		self->m_userData = NULL;
	}
	if (self->m_dialogId >= 0) {
		SceCommonGuiDialog_BC311DC0(self->m_dialogId);
		self->m_dialogId = -1;
		self->m_callback = NULL;
		self->m_userData = NULL;
		return 0;
	}
	return result;
}
