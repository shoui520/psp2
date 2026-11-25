/*
 * SceTwLoginDialogPlugin (vs0:sys/external/libcdlg_tw_login.suprx): module entry,
 * TwLoginDialogApp, TwLoginPage, and CommonDialogMain slot 21 integration.
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <common_dialog.h>
#include <libkernel/libkernel.h>
#include <std/string.h>
#include <std/string>
#include <math/ngp/v4.h>
#include <plugin.h>
#include <module/module.h>
#include <common/main_thread_call_list.h>
#include <common/string_util.h>
#include <graphics/surface/surface.h>
#include <graphics/text/text_style.h>
#include <widget/core/widget.h>
#include <widget/w_text.h>
#include <widget/w_text_box.h>
#include <widget/w_button.h>
#include <widget/core/event.h>
#include "tw_lx.h"
#include "tw_lx_oauth.h"
#include "tw_dialog_common.h"

using namespace paf;

/* Common dialog slot index for Twitter login */
#define COMMON_DIALOG_TYPE_TW_LOGIN	21

/* Work memory layout (matches vsh/cdlg/src/cdlg_mgr/tw_login_dialog.cpp) */
struct TwLoginDialogParam {
	SceUInt32 sdkVersion;
	SceCommonDialogParam commonParam;
	SceInt32 mode;
	SceCommonDialogColor textColor;
	SceInt32 colorFlag;
	SceInt32 dimHeader;
	SceChar8 reserved[32];
};

struct TwLoginDialogResult {
	SceInt32 result;
	SceInt32 dialogData;
	SceChar8 reserved[32];
};

struct CommonDialogTwLoginParam {
	SceInt32 state;
	SceUInt32 flags;
	SceInt32 abortResult;
	SceInt32 abortRequested;
	TwLoginDialogParam param;
	SceUInt8 dialogData[8];
	TwLoginDialogResult result;
	char apiName[64];
};

/* Page and widget IDParam hashes */
#define TW_PAGE_LOGIN_ID		0x92E70FC2U
#define TW_WIDGET_USERNAME_BOX_ID	0x8D70E8EBU
#define TW_WIDGET_PASSWORD_BOX_ID	0x1B200227U
#define TW_WIDGET_CANCEL_BTN_ID		0x32677CFBU
#define TW_WIDGET_LOGIN_BTN_ID		0xDB8B2F06U
#define TW_WIDGET_LABEL_TITLE_ID	0xD9D61E4EU
#define TW_WIDGET_LABEL_USER_ID		0xFE740444U
#define TW_WIDGET_LABEL_PASS_ID		0xABDF9993U
#define TW_WIDGET_LABEL_DESC_ID		0xA0B92597U
#define TW_WIDGET_BG_PLANE_ID		0x95FE8E53U
#define TW_WIDGET_HEADER_PLANE_ID	0xE2A94158U
#define TW_TEX_BG_ID			0x3167BD32U
#define TW_STR_WAIT_LOGIN_ID		0x632B9313U

#define TW_EV_BUTTON_DECIDE		0x10000008
#define TW_EV_TEXTBOX_CHANGE		0x1000000B

/* SceCommonDialogMain imports */
extern "C" {
int SceCommonDialogMain_05EFC635(int dialogType);
int SceCommonDialogMain_5739BC42(int dialogType);
int SceCommonDialogMain_75F10F83(int dialogType);
int SceCommonDialogMain_B9D0E5A8(int dialogType);
CommonDialogTwLoginParam *SceCommonDialogMain_DD2CAC30(int dialogType);
void __cxa_finalize(void *dsoHandle);
extern void (*__start__Zinit_array[])(void);
extern void (*__stop__Zinit_array[])(void);
void *__dso_handle = &__dso_handle;
}

/* Plugin interface table registered via paf::Module::SetInterface */
struct TwPluginInterface {
	void (*OnBoot)();
	void (*OnLoad)(Plugin *plugin);
	void (*OnStart)();
	void (*OnStop)();
	void (*OnUnload)();
};

/* Forward declarations */
class TwLoginDialogApp;

/* Page table for TwPageManager */
static TwPageHandlerEntry s_twPageEntries[] = {
	{ TW_PAGE_LOGIN_ID, NULL, 0 },
	{ 0, NULL, 0 },
};

static math::v4 s_headerDimColor(0.0f, 173.0f / 255.0f, 238.0f / 255.0f, 1.0f);

/* TextBox change event listeners */
class TwTextBoxNoopListener : public ui::EventListener {
public:
	TwTextBoxNoopListener() {}
	virtual ~TwTextBoxNoopListener() {}
	virtual int32_t Do(int32_t event, ui::Handler *handler, ui::Event *ev)
	{
		(void)event;
		(void)handler;
		(void)ev;
		return 0;
	}
};

class TwLoginPage;

class TwUsernameChangeListener : public ui::EventListener {
public:
	explicit TwUsernameChangeListener(TwLoginPage *page) : m_page(page) {}
	virtual ~TwUsernameChangeListener() {}
	virtual int32_t Do(int32_t event, ui::Handler *handler, ui::Event *ev);

	TwLoginPage *m_page;
};

class TwPasswordChangeListener : public ui::EventListener {
public:
	explicit TwPasswordChangeListener(TwLoginPage *page) : m_page(page) {}
	virtual ~TwPasswordChangeListener() {}
	virtual int32_t Do(int32_t event, ui::Handler *handler, ui::Event *ev);

	TwLoginPage *m_page;
};

/* Login page controller */
class TwLoginPage : public TwJobController {
public:
	TwLoginPage();
	virtual ~TwLoginPage();

	void OnPageInit(ui::Widget *pageWidget);
	void OnUsernameChanged();
	void OnPasswordChanged();
	void ResetPassword();

	static void OnCancelClick(int event, ui::Handler *handler, ui::Event *ev, void *userData);
	static void OnLoginClick(int event, ui::Handler *handler, ui::Event *ev, void *userData);

	TwLoginDialogApp *m_app;
	ui::Widget *m_pageWidget;
	string m_username;
	string m_password;
	TwLoginDialogApp *m_appBackRef;
	SceInt32 m_mode;
	SceCommonDialogColor m_bgColor;
	SceCommonDialogColor m_textColor;
	uint32_t m_bgTextureId;
	SceInt32 m_dimHeader;
	SceInt32 m_colorFlag;
};

/* Callback forwarding TwPageHandler::OnPageInit -> TwLoginPage::OnPageInit */
static void OnLoginPageInit(ui::Widget *pageWidget, void *userData)
{
	TwLoginPage *loginPage = (TwLoginPage *)userData;

	if (pageWidget != NULL && loginPage != NULL)
		loginPage->OnPageInit(pageWidget);
}

/* Base dialog application */
class TwLoginDialogBase {
public:
	TwLoginDialogBase();
	virtual ~TwLoginDialogBase();

	virtual void FinishDialog(SceInt32 resultCode);
	virtual void OpenPage(uint32_t pageIdHash) { m_pageMgr.OpenPage(pageIdHash); }
	virtual void ClosePage(uint32_t pageIdHash) { m_pageMgr.ClosePage(pageIdHash); }
	virtual void OpenPageWithEffect(uint32_t pageIdHash, float dur, int type)
	{
		m_pageMgr.OpenPageWithEffect(pageIdHash, dur, type);
	}
	virtual void ClosePageWithEffect(uint32_t pageIdHash, float dur, int type)
	{
		m_pageMgr.ClosePageWithEffect(pageIdHash, dur, type);
	}
	virtual void OpenPageWithAnim(uint32_t pageIdHash, int type)
	{
		m_pageMgr.OpenPageWithAnim(pageIdHash, type);
	}
	virtual void ClosePageWithAnim(uint32_t pageIdHash, int type)
	{
		m_pageMgr.ClosePageWithAnim(pageIdHash, type);
	}
	virtual intrusive_ptr<graph::Surface> GetTexture(uint32_t texIdHash);
	virtual const wchar_t *GetString(const IDParam &id);

	void OnLoad(Plugin *plugin);
	void OnStart();
	void OnStop();
	void OnUnload();

	static void OnUpdate(void *arg);
	static void OnCancelJobsDone(void *arg);

	TwPageManager m_pageMgr;
	SceInt32 m_dialogMode;
	TwPageHandler m_pageHandler;
	TwLoginPage m_loginPage;
	TwWaitDialog m_waitDialog;
	TwErrorDialog m_errorDialog;
};

/* Concrete Twitter login dialog application */
class TwLoginDialogApp : public TwLoginDialogBase {
public:
	TwLoginDialogApp();
	virtual ~TwLoginDialogApp();

	void ShowWaitDialog();

	static void OnJobStart(void *userData, void *ctx);
	static int OnJobRun(void *userData, TwJobItem *job, void *ctx);
	static void OnJobComplete(int result, void *userData, void *ctx);
	static void OnErrorDialogClosed(int buttonId, void *userData);
};

/* Singleton instance */
static TwLoginDialogApp s_twLoginApp;
static TwPluginInterface s_pluginInterface;

/* Helpers for finding typed child widgets */
static ui::TextBox *FindTextBox(ui::Widget *root, uint32_t idHash)
{
	IDParam id("", idHash);
	ui::Widget *child = root->FindChild(id, 0);

	if (child != NULL && child->IsInherit(ui::TextBox::TypeName()))
		return (ui::TextBox *)child;
	return NULL;
}

static ui::Button *FindButton(ui::Widget *root, uint32_t idHash)
{
	IDParam id("", idHash);
	ui::Widget *child = root->FindChild(id, 0);

	if (child != NULL && child->IsInherit(ui::Button::TypeName()))
		return (ui::Button *)child;
	return NULL;
}

static ui::Text *FindText(ui::Widget *root, uint32_t idHash)
{
	IDParam id("", idHash);
	ui::Widget *child = root->FindChild(id, 0);

	if (child != NULL && child->IsInherit(ui::Text::TypeName()))
		return (ui::Text *)child;
	return NULL;
}

/* Event listener dispatches */
int32_t TwUsernameChangeListener::Do(int32_t event, ui::Handler *handler, ui::Event *ev)
{
	(void)event;
	(void)handler;
	(void)ev;
	m_page->OnUsernameChanged();
	return 0;
}

int32_t TwPasswordChangeListener::Do(int32_t event, ui::Handler *handler, ui::Event *ev)
{
	(void)event;
	(void)handler;
	(void)ev;
	m_page->OnPasswordChanged();
	return 0;
}

/* TwLoginPage */
TwLoginPage::TwLoginPage()
	: m_app(NULL), m_pageWidget(NULL), m_username(), m_password(), m_appBackRef(NULL),
	  m_mode(0), m_bgTextureId(0), m_dimHeader(0), m_colorFlag(0)
{
	sce_paf_memset(&m_bgColor, 0, sizeof(m_bgColor));
	sce_paf_memset(&m_textColor, 0, sizeof(m_textColor));
}

TwLoginPage::~TwLoginPage()
{
}

void TwLoginPage::OnPageInit(ui::Widget *pageWidget)
{
	math::v4 textColor((float)m_textColor.r / 255.0f, (float)m_textColor.g / 255.0f,
			   (float)m_textColor.b / 255.0f, (float)m_textColor.a / 255.0f);
	math::v4 bgColor((float)m_bgColor.r / 255.0f, (float)m_bgColor.g / 255.0f,
			 (float)m_bgColor.b / 255.0f, (float)m_bgColor.a / 255.0f);

	m_pageWidget = pageWidget;

	ui::TextBox *userBox = FindTextBox(pageWidget, TW_WIDGET_USERNAME_BOX_ID);

	if (userBox != NULL) {
		userBox->AddEventListener(TW_EV_TEXTBOX_CHANGE, new TwUsernameChangeListener(this),
					  false);
		userBox->AddEventListener(TW_EV_BUTTON_DECIDE, new TwTextBoxNoopListener(), false);
		userBox->SetStyleAttribute(graph::TextStyleAttribute_Color, 0, 0, textColor);
		userBox->SetCaretIndex(0);
	}

	ui::TextBox *passBox = FindTextBox(pageWidget, TW_WIDGET_PASSWORD_BOX_ID);

	if (passBox != NULL) {
		passBox->AddEventListener(TW_EV_TEXTBOX_CHANGE, new TwPasswordChangeListener(this),
					  false);
		passBox->AddEventListener(TW_EV_BUTTON_DECIDE, new TwTextBoxNoopListener(), false);
		passBox->SetStyleAttribute(graph::TextStyleAttribute_Color, 0, 0, textColor);
	}

	ui::Button *cancelBtn = FindButton(pageWidget, TW_WIDGET_CANCEL_BTN_ID);

	if (cancelBtn != NULL) {
		cancelBtn->m_event_enable |= 0xFFF0;
		cancelBtn->AddEventListener(TW_EV_BUTTON_DECIDE,
					    new ui::EventCBListener(OnCancelClick, this), true);
		ScePafWidget_21125870(cancelBtn, 32);
		if (m_mode != 2 && m_colorFlag != 0) {
			graph::DrawObj *drawObj = cancelBtn->GetDrawObj(0);

			if (drawObj != NULL)
				drawObj->SetColor(textColor);
		}
	}

	ui::Button *loginBtn = FindButton(pageWidget, TW_WIDGET_LOGIN_BTN_ID);

	if (loginBtn != NULL) {
		loginBtn->m_event_enable |= 0xFFF0;
		loginBtn->AddEventListener(TW_EV_BUTTON_DECIDE,
					   new ui::EventCBListener(OnLoginClick, this), true);
		loginBtn->Disable(false);
		if (m_mode != 2 && m_colorFlag != 0) {
			graph::DrawObj *drawObj = loginBtn->GetDrawObj(0);

			if (drawObj != NULL)
				drawObj->SetColor(textColor);
		}
	}

	if (m_mode != 2) {
		static const uint32_t labelIds[4] = {
			TW_WIDGET_LABEL_TITLE_ID,
			TW_WIDGET_LABEL_USER_ID,
			TW_WIDGET_LABEL_PASS_ID,
			TW_WIDGET_LABEL_DESC_ID,
		};

		for (int i = 0; i < 4; ++i) {
			ui::Text *label = FindText(pageWidget, labelIds[i]);

			if (label != NULL)
				label->SetStyleAttribute(graph::TextStyleAttribute_Color, 0, 0,
							 textColor);
		}
	}

	ui::Widget *bgPlane = pageWidget->FindChild(IDParam("", TW_WIDGET_BG_PLANE_ID), 0);

	if (bgPlane != NULL) {
		if (m_bgTextureId != 0) {
			intrusive_ptr<graph::Surface> surf = m_app->GetTexture(m_bgTextureId);

			bgPlane->SetTexture(surf);
		}
		if (m_mode != 2)
			bgPlane->SetColor(bgColor, NULL, 0, 0x10001, 0.0f, NULL, NULL);
	}

	if (m_mode != 2) {
		ui::Widget *headerPlane =
			pageWidget->FindChild(IDParam("", TW_WIDGET_HEADER_PLANE_ID), 0);

		if (headerPlane != NULL && m_dimHeader == 1)
			headerPlane->SetColor(s_headerDimColor, NULL, 0, 0x10001, 0.0f, NULL,
					      NULL);
	}

	TwInfobar::Show();
}

void TwLoginPage::OnUsernameChanged()
{
	wstring text;
	ui::TextBox *userBox = FindTextBox(m_pageWidget, TW_WIDGET_USERNAME_BOX_ID);
	ui::TextBox *passBox = FindTextBox(m_pageWidget, TW_WIDGET_PASSWORD_BOX_ID);
	ui::Button *loginBtn = FindButton(m_pageWidget, TW_WIDGET_LOGIN_BTN_ID);

	if (userBox != NULL) {
		userBox->GetString(text);
		common::Utf16ToUtf8(text, &m_username);
	}
	if (passBox != NULL) {
		passBox->GetString(text);
		common::Utf16ToUtf8(text, &m_password);
	}
	if (loginBtn != NULL) {
		loginBtn->Disable(false);
		if (!m_username.empty() && !m_password.empty())
			loginBtn->Enable(false);
	}
	if (passBox != NULL)
		passBox->BeginEdit();
}

void TwLoginPage::OnPasswordChanged()
{
	wstring text;
	ui::TextBox *userBox = FindTextBox(m_pageWidget, TW_WIDGET_USERNAME_BOX_ID);
	ui::TextBox *passBox = FindTextBox(m_pageWidget, TW_WIDGET_PASSWORD_BOX_ID);
	ui::Button *loginBtn = FindButton(m_pageWidget, TW_WIDGET_LOGIN_BTN_ID);

	if (userBox != NULL) {
		userBox->GetString(text);
		common::Utf16ToUtf8(text, &m_username);
	}
	if (passBox != NULL) {
		passBox->GetString(text);
		common::Utf16ToUtf8(text, &m_password);
	}
	if (loginBtn != NULL) {
		loginBtn->Disable(false);
		if (!m_username.empty() && !m_password.empty())
			loginBtn->Enable(false);
	}
	if (passBox != NULL)
		passBox->EndEdit();
}

void TwLoginPage::ResetPassword()
{
	wstring empty;
	ui::Widget *passBox = m_pageWidget->FindChild(IDParam("", TW_WIDGET_PASSWORD_BOX_ID), 0);

	if (passBox != NULL)
		passBox->SetString(empty);
	m_password.clear();

	ui::Button *loginBtn = FindButton(m_pageWidget, TW_WIDGET_LOGIN_BTN_ID);

	if (loginBtn != NULL)
		loginBtn->Disable(false);
}

void TwLoginPage::OnCancelClick(int event, ui::Handler *handler, ui::Event *ev, void *userData)
{
	(void)event;
	(void)handler;
	(void)ev;
	TwLoginPage *self = (TwLoginPage *)userData;

	TwInfobar::Hide();
	self->m_app->ClosePage(*(const uint32_t *)((const uint8_t *)self->m_pageWidget + 332));
	self->m_app->FinishDialog(SCE_COMMON_DIALOG_RESULT_USER_CANCELED);
}

void TwLoginPage::OnLoginClick(int event, ui::Handler *handler, ui::Event *ev, void *userData)
{
	(void)event;
	(void)handler;
	(void)ev;
	TwLoginPage *self = (TwLoginPage *)userData;

	new TwJobItem(self->m_appBackRef, self, self, "TwitterLoginJob");
}

/* TwLoginDialogBase */
TwLoginDialogBase::TwLoginDialogBase()
	: m_pageMgr(), m_dialogMode(0), m_pageHandler(), m_loginPage(), m_waitDialog(),
	  m_errorDialog()
{
	m_pageMgr.m_entries = s_twPageEntries;
	m_pageMgr.RegisterHandler(TW_PAGE_LOGIN_ID, &m_pageHandler);
	m_pageHandler.m_onPageInit = OnLoginPageInit;
	m_pageHandler.m_userData = &m_loginPage;
	m_loginPage.m_app = (TwLoginDialogApp *)this;
	LxLoginSession::InitHeap();
}

TwLoginDialogBase::~TwLoginDialogBase()
{
	LxLoginSession::TermHeap();
	m_pageMgr.UnregisterHandler(TW_PAGE_LOGIN_ID);
}

void TwLoginDialogBase::OnLoad(Plugin *plugin)
{
	m_pageMgr.Init(plugin);
	TwInitPluginContext(plugin);
	LxLoginSession::Init(plugin);
	m_errorDialog.Init(plugin);
}

void TwLoginDialogBase::OnUnload()
{
	LxLoginSession::Term();
	m_pageMgr.Term();
}

void TwLoginDialogBase::OnStart()
{
	common::MainThreadCallList::Register(OnUpdate, this);
	if (SceCommonDialogMain_B9D0E5A8(COMMON_DIALOG_TYPE_TW_LOGIN) != 0)
		return;
	CommonDialogTwLoginParam *work = SceCommonDialogMain_DD2CAC30(COMMON_DIALOG_TYPE_TW_LOGIN);

	if (work == NULL) {
		SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_TW_LOGIN);
		return;
	}
	sce_paf_strncpy(work->apiName, "Start", sizeof(work->apiName));
	work->apiName[sizeof(work->apiName) - 1] = '\0';

	m_dialogMode = work->param.mode;
	m_loginPage.m_mode = work->param.mode;

	m_loginPage.m_textColor.r = work->param.textColor.r;
	m_loginPage.m_textColor.g = work->param.textColor.g;
	m_loginPage.m_textColor.b = work->param.textColor.b;
	m_loginPage.m_textColor.a = 0xFF;

	const SceCommonDialogColor *bgColor = work->param.commonParam.bgColor;

	if (bgColor != NULL) {
		m_loginPage.m_bgColor.r = bgColor->r;
		m_loginPage.m_bgColor.g = bgColor->g;
		m_loginPage.m_bgColor.b = bgColor->b;
		m_loginPage.m_bgColor.a = 0xFF;
	}
	m_loginPage.m_colorFlag = work->param.colorFlag;
	m_loginPage.m_dimHeader = work->param.dimHeader;
	if (work->param.mode == 2)
		m_loginPage.m_bgTextureId = TW_TEX_BG_ID;

	OpenPage(TW_PAGE_LOGIN_ID);
	if (sce_paf_strncmp(work->apiName, "Start", sizeof(work->apiName)) == 0) {
		sce_paf_memset(work->apiName, 0, sizeof(work->apiName));
		SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_TW_LOGIN);
	}
}

void TwLoginDialogBase::OnStop()
{
	common::MainThreadCallList::Unregister(OnUpdate, this);
	m_pageMgr.CloseAll();
}

void TwLoginDialogBase::FinishDialog(SceInt32 resultCode)
{
	if (SceCommonDialogMain_B9D0E5A8(COMMON_DIALOG_TYPE_TW_LOGIN) != 0)
		return;
	CommonDialogTwLoginParam *work = SceCommonDialogMain_DD2CAC30(COMMON_DIALOG_TYPE_TW_LOGIN);

	if (work == NULL) {
		SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_TW_LOGIN);
		return;
	}
	sce_paf_strncpy(work->apiName, "Term", sizeof(work->apiName));
	work->apiName[sizeof(work->apiName) - 1] = '\0';

	sce_paf_memset(&work->result, 0, sizeof(work->result));
	work->result.result = m_dialogMode;
	if (SceCommonDialogMain_75F10F83(COMMON_DIALOG_TYPE_TW_LOGIN) != 0) {
		work->result.dialogData = SCE_COMMON_DIALOG_RESULT_ABORTED;
	} else if (resultCode >= 0 || resultCode == (SceInt32)0x80106001) {
		work->result.dialogData = resultCode;
	} else {
		work->result.dialogData = SCE_COMMON_DIALOG_ERROR_UNEXPECTED_FATAL;
	}
	work->abortResult = 0;

	common::MainThreadCallList::Unregister(OnUpdate, NULL);
	SceCommonDialogMain_05EFC635(COMMON_DIALOG_TYPE_TW_LOGIN);
	if (sce_paf_strncmp(work->apiName, "Term", sizeof(work->apiName)) == 0) {
		sce_paf_memset(work->apiName, 0, sizeof(work->apiName));
		SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_TW_LOGIN);
	}
}

intrusive_ptr<graph::Surface> TwLoginDialogBase::GetTexture(uint32_t texIdHash)
{
	IDParam id("", texIdHash);

	return m_pageMgr.m_plugin->GetTexture(id);
}

const wchar_t *TwLoginDialogBase::GetString(const IDParam &id)
{
	return m_pageMgr.m_plugin->GetString(id);
}

void TwLoginDialogBase::OnCancelJobsDone(void *arg)
{
	(void)arg;
	SceCommonDialogMain_05EFC635(COMMON_DIALOG_TYPE_TW_LOGIN);
}

void TwLoginDialogBase::OnUpdate(void *arg)
{
	TwLoginDialogBase *self = (TwLoginDialogBase *)arg;

	if (SceCommonDialogMain_B9D0E5A8(COMMON_DIALOG_TYPE_TW_LOGIN) != 0) {
		self->m_errorDialog.Close();
		self->m_waitDialog.Close();
		common::MainThreadCallList::Unregister(OnUpdate, self);
		self->m_pageMgr.CloseAll();
		if (self->m_loginPage.CancelAll(OnCancelJobsDone, self, true) <= 0)
			SceCommonDialogMain_05EFC635(COMMON_DIALOG_TYPE_TW_LOGIN);
		return;
	}
	CommonDialogTwLoginParam *work = SceCommonDialogMain_DD2CAC30(COMMON_DIALOG_TYPE_TW_LOGIN);

	if (work == NULL) {
		SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_TW_LOGIN);
		self->m_errorDialog.Close();
		self->m_waitDialog.Close();
		common::MainThreadCallList::Unregister(OnUpdate, self);
		self->m_pageMgr.CloseAll();
		if (self->m_loginPage.CancelAll(OnCancelJobsDone, self, true) <= 0)
			SceCommonDialogMain_05EFC635(COMMON_DIALOG_TYPE_TW_LOGIN);
		return;
	}

	sce_paf_strncpy(work->apiName, "OnUpdate", sizeof(work->apiName));
	work->apiName[sizeof(work->apiName) - 1] = '\0';
	if (work->abortResult == 1) {
		sce_paf_memset(&work->result, 0, sizeof(work->result));
		work->result.result = SCE_COMMON_DIALOG_RESULT_ABORTED;
		SceCommonDialogMain_75F10F83(COMMON_DIALOG_TYPE_TW_LOGIN);
		work->abortResult = 2;
		work->result.dialogData = SCE_COMMON_DIALOG_RESULT_ABORTED;

		self->m_errorDialog.Close();
		self->m_waitDialog.Close();
		common::MainThreadCallList::Unregister(OnUpdate, self);
		self->m_pageMgr.CloseAll();
		if (self->m_loginPage.CancelAll(OnCancelJobsDone, self, true) <= 0)
			SceCommonDialogMain_05EFC635(COMMON_DIALOG_TYPE_TW_LOGIN);
	}
	if (sce_paf_strncmp(work->apiName, "OnUpdate", sizeof(work->apiName)) == 0) {
		sce_paf_memset(work->apiName, 0, sizeof(work->apiName));
		SceCommonDialogMain_5739BC42(COMMON_DIALOG_TYPE_TW_LOGIN);
	}
}

/* TwLoginDialogApp */
TwLoginDialogApp::TwLoginDialogApp()
{
	m_loginPage.m_appBackRef = this;
	m_loginPage.m_startFn = OnJobStart;
	m_loginPage.m_startCtx = this;
	m_loginPage.m_runFn = OnJobRun;
	m_loginPage.m_runCtx = this;
	m_loginPage.m_completeFn = OnJobComplete;
	m_loginPage.m_completeCtx = this;
}

TwLoginDialogApp::~TwLoginDialogApp()
{
}

void TwLoginDialogApp::ShowWaitDialog()
{
	const wchar_t *msg = GetString(IDParam("", TW_STR_WAIT_LOGIN_ID));
	wstring waitMsg;

	if (msg != NULL)
		waitMsg.assign(msg, sce_paf_wcslen(msg));
	m_waitDialog.Open(m_pageMgr.m_plugin, waitMsg, 3, NULL, NULL);
}

void TwLoginDialogApp::OnJobStart(void *userData, void *ctx)
{
	(void)userData;
	TwLoginDialogApp *self = (TwLoginDialogApp *)ctx;

	self->ShowWaitDialog();
}

int TwLoginDialogApp::OnJobRun(void *userData, TwJobItem *job, void *ctx)
{
	(void)userData;
	TwLoginDialogApp *self = (TwLoginDialogApp *)ctx;
	const char *username = self->m_loginPage.m_username.c_str();
	const char *password = self->m_loginPage.m_password.c_str();

	if (username[0] == '\0' || password[0] == '\0')
		return -1;

	LxString errorDetail;
	int ret = LxLoginSession::Login(job, username, password, errorDetail);

	if (ret < 0)
		self->m_errorDialog.SetError(ret, errorDetail);
	return ret;
}

void TwLoginDialogApp::OnJobComplete(int result, void *userData, void *ctx)
{
	(void)userData;
	TwLoginDialogApp *self = (TwLoginDialogApp *)ctx;

	self->m_waitDialog.Close();
	if (result < 0) {
		self->m_errorDialog.Show(OnErrorDialogClosed, self, 3, (int16_t)result);
		return;
	}
	self->ClosePage(TW_PAGE_LOGIN_ID);
	self->FinishDialog(0);
}

void TwLoginDialogApp::OnErrorDialogClosed(int buttonId, void *userData)
{
	(void)buttonId;
	TwLoginDialogApp *self = (TwLoginDialogApp *)userData;

	self->m_loginPage.ResetPassword();
}

/* Plugin interface callbacks */
static void TwPluginOnBoot()
{
}

static void TwPluginOnLoad(Plugin *plugin)
{
	s_twLoginApp.OnLoad(plugin);
}

static void TwPluginOnStart()
{
	s_twLoginApp.OnStart();
}

static void TwPluginOnStop()
{
	s_twLoginApp.OnStop();
}

static void TwPluginOnUnload()
{
	s_twLoginApp.OnUnload();
}

static void TwPluginInit(SceSize arglen, const void *argp)
{
	(void)arglen;
	__dso_handle = &__dso_handle;
	for (void (**fn)(void) = __start__Zinit_array; fn < __stop__Zinit_array; ++fn) {
		if (*fn != NULL)
			(*fn)();
	}
	Module *module = *(Module *const *)argp;

	s_pluginInterface.OnBoot = TwPluginOnBoot;
	s_pluginInterface.OnLoad = TwPluginOnLoad;
	s_pluginInterface.OnStart = TwPluginOnStart;
	s_pluginInterface.OnStop = TwPluginOnStop;
	s_pluginInterface.OnUnload = TwPluginOnUnload;
	module->SetInterface(0, &s_pluginInterface);
}

static void TwPluginTerm()
{
	__cxa_finalize(&__dso_handle);
}

extern "C" int module_start(SceSize arglen, const void *argp)
{
	sceClibPrintf("[TwLoginDialogPlugin] Module Start!\n");
	TwPluginInit(arglen, argp);
	return 0;
}

extern "C" int module_stop(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	sceClibPrintf("[TwLoginDialogPlugin] Module Stop!\n");
	TwPluginTerm();
	return 0;
}
