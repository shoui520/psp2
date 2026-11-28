/*
 * power_manage_auth.cpp: passcode authentication service, views, actions, and registry stores
 * for Screen Lock and Parental Control passcodes in power_manage_plugin.
 * 3.65-CEX: 0x812380F8 - 0x8123B524, 0x81240B24 - 0x81241220
 */
#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include <std/string.h>
#include <wchar.h>
#include <std/string>
#include <std/vector>
#include <common/encoded_string.h>
#include <common/shared_ptr.h>
#include <inputdevice/inputdevice.h>
#include <job/job.h>
#include <plugin.h>
#include <regmgr/regmgr.h>
#include <rtc/rtc.h>
#include <thread/thread.h>
#include <timer/timer.h>
#include <widget/w_text.h>
#include <widget/w_text_box.h>
#include <common_gui_dialog/common_gui_dialog.h>

#include "shell.h"
#include "plugins.h"
#include "power_manage_internal.h"

namespace power_manage {

static const SceUInt32 kPasscodeDigits = 4;
static const int kScreenLockMaxAttempts = 5;
static const int kScreenLockTimeoutMinutes = 60;
static const int kScreenLockEpochYearOffset = -2000;
static const int kParentalPasscodeSystemParamId = 1597730;
static const int kAuthOverlayPriority = 27;
static const SceUInt32 kAuthErrorSoundHash = 0xD74A54E5U;		/* -682994459 */
static const SceUInt32 kPasscodeDotsTemplateHash = 0x4852B637U;		/* 1213381911 */
static const SceUInt32 kPasscodeDotsBoxHash = 0x6B83E244U;		/* 1803771460 */
static const SceUInt32 kLockScreenPageHash = 0x89251908U;		/* -1993997112 */
static const SceUInt32 kLockScreenTextBoxHash = 0xE9AD0A71U;		/* -374531919 */
static const SceUInt32 kMsgEnterPasscodeHash = 0x431ECBD2U;		/* 1126062898 */
static const SceUInt32 kMsgConfirmPasscodeHash = 0xC83CA288U;		/* -935557464 */
static const SceUInt32 kMsgEnterCurrentPasscodeHash = 0x2CD90173U;	/* 752418739 */
static const SceUInt32 kMsgEnterNewPasscodeHash = 0x03A07170U;		/* 60847472 */
static const SceUInt32 kMsgScreenLockVerifyHash = 0xBAA99138U;		/* -1163249416 */
static const SceUInt32 kMsgScreenLockSetHash = 0x8BD69D69U;		/* -1948864535 */
static const SceUInt32 kMsgParentalVerifyHash = 0x925937F8U;		/* -1839596072 */
static const SceUInt32 kMsgParentalSetHash = 0x1DFC429BU;		/* 503092379 */
static const SceUInt32 kMsgWrongPasscodeHash = 0x386B2271U;		/* 946535025 */
static const SceUInt32 kMsgWrongPasscodeAttemptsHash = 0xA0765DBFU;	/* -1602792001 */
static const SceUInt32 kMsgLockedOutHash = 0x4BA57C8BU;			/* 1269115243 */
static const SceUInt32 kMsgPasscodeMismatchHash = 0xD772818FU;		/* -680354417 */

static const SceInt32 kAuthTimeoutTimerId = 0x01000000;
static const SceUInt32 kAuthAllInputDevices = 127;

static inline const wchar_t *getPowerManageString(SceUInt32 hash)
{
	paf::Plugin *plugin = paf::Plugin::Find("power_manage_plugin");
	return plugin->GetString(paf::IDParam(hash));
}

/* --- Passcode stores --- */

class AuthPasscodeStoreBase {
public:
	AuthPasscodeStoreBase();
	virtual ~AuthPasscodeStoreBase();

	virtual int Load();
	virtual int Save(const paf::string &passcode);
	virtual bool CompareMismatch(const paf::string &candidate) const;
	virtual bool IsEnabled() const;
	virtual int ResetLockout();
	virtual int GetAttemptsLeft() const;
	virtual int DecrementAttempts();
	virtual bool IsLockedOut();
	virtual int TriggerLockout();
	virtual const wchar_t *GetVerifyPrompt() const;
	virtual const wchar_t *GetSetPrompt() const;
	virtual const wchar_t *GetConfirmPrompt() const;
	virtual const wchar_t *GetChangeCurrentPrompt() const;
	virtual const wchar_t *GetChangeNewPrompt() const;
	virtual const wchar_t *GetChangeConfirmPrompt() const;
	virtual int ReadPasscode(const paf::string &keyPath, paf::string *outPasscode);
	virtual int WritePasscode(const paf::string &keyPath, const paf::string &passcode);
	virtual SceSize GetBufferSize() const = 0;
	virtual paf::string GetRegistryPath() const = 0;

	paf::string m_passcode;
};

AuthPasscodeStoreBase::AuthPasscodeStoreBase()
{
}

AuthPasscodeStoreBase::~AuthPasscodeStoreBase()
{
}

int AuthPasscodeStoreBase::Load()
{
	paf::string keyPath = GetRegistryPath();
	if (keyPath.empty())
		return -1;
	int ret = ReadPasscode(keyPath, &m_passcode);
	if (ret != 0)
		shellRegMgrLogError(ret, 1);
	return ret;
}

int AuthPasscodeStoreBase::Save(const paf::string &passcode)
{
	m_passcode = passcode;
	paf::string keyPath = GetRegistryPath();
	int ret = WritePasscode(keyPath, passcode);
	if (ret != 0)
		shellRegMgrLogError(ret, 1);
	return ret;
}

bool AuthPasscodeStoreBase::CompareMismatch(const paf::string &candidate) const
{
	return sce_paf_strcmp(m_passcode.c_str(), candidate.c_str()) != 0;
}

bool AuthPasscodeStoreBase::IsEnabled() const
{
	return !m_passcode.empty();
}

int AuthPasscodeStoreBase::ResetLockout()
{
	return 0;
}

int AuthPasscodeStoreBase::GetAttemptsLeft() const
{
	return 1;
}

int AuthPasscodeStoreBase::DecrementAttempts()
{
	return 0;
}

bool AuthPasscodeStoreBase::IsLockedOut()
{
	return false;
}

int AuthPasscodeStoreBase::TriggerLockout()
{
	return 0;
}

const wchar_t *AuthPasscodeStoreBase::GetVerifyPrompt() const
{
	return L"";
}

const wchar_t *AuthPasscodeStoreBase::GetSetPrompt() const
{
	return getPowerManageString(kMsgEnterPasscodeHash);
}

const wchar_t *AuthPasscodeStoreBase::GetConfirmPrompt() const
{
	return getPowerManageString(kMsgConfirmPasscodeHash);
}

const wchar_t *AuthPasscodeStoreBase::GetChangeCurrentPrompt() const
{
	return getPowerManageString(kMsgEnterCurrentPasscodeHash);
}

const wchar_t *AuthPasscodeStoreBase::GetChangeNewPrompt() const
{
	return getPowerManageString(kMsgEnterNewPasscodeHash);
}

const wchar_t *AuthPasscodeStoreBase::GetChangeConfirmPrompt() const
{
	return getPowerManageString(kMsgConfirmPasscodeHash);
}

int AuthPasscodeStoreBase::ReadPasscode(const paf::string &keyPath, paf::string *outPasscode)
{
	SceSize bufSize = GetBufferSize();
	char *buffer = new char[bufSize];
	sce_paf_memset(buffer, '0', bufSize);

	size_t slashPos = keyPath.rfind('/');
	paf::string category = (slashPos == paf::string::npos)
		? paf::string("")
		: keyPath.substr(0, slashPos);
	paf::string keyName = (slashPos == paf::string::npos)
		? paf::string("")
		: keyPath.substr(slashPos + 1, keyPath.length() - slashPos - 1);

	int ret = sceRegMgrGetKeyStr(category.c_str(), keyName.c_str(), buffer, bufSize);
	if (ret != 0)
		*outPasscode = "";
	else
		*outPasscode = buffer;
	delete[] buffer;
	return ret;
}

int AuthPasscodeStoreBase::WritePasscode(const paf::string &keyPath, const paf::string &passcode)
{
	SceSize bufSize = GetBufferSize();
	size_t slashPos = keyPath.rfind('/');
	paf::string category = (slashPos == paf::string::npos)
		? paf::string("")
		: keyPath.substr(0, slashPos);
	paf::string keyName = (slashPos == paf::string::npos)
		? paf::string("")
		: keyPath.substr(slashPos + 1, keyPath.length() - slashPos - 1);

	return sceRegMgrSetKeyStr(category.c_str(), keyName.c_str(), passcode.c_str(), bufSize);
}

class AuthScreenLockStore : public AuthPasscodeStoreBase {
public:
	AuthScreenLockStore();
	virtual ~AuthScreenLockStore();

	static AuthScreenLockStore *GetInstance();

	virtual int Load();
	virtual int ResetLockout();
	virtual int GetAttemptsLeft() const;
	virtual int DecrementAttempts();
	virtual bool IsLockedOut();
	virtual int TriggerLockout();
	virtual const wchar_t *GetVerifyPrompt() const;
	virtual const wchar_t *GetSetPrompt() const;
	virtual SceSize GetBufferSize() const;
	virtual paf::string GetRegistryPath() const;

	int m_attemptsLeft;
	SceUInt32 m_lockUntilSec;
	static AuthScreenLockStore *s_instance;
};

AuthScreenLockStore *AuthScreenLockStore::s_instance = NULL;

AuthScreenLockStore::AuthScreenLockStore() : m_attemptsLeft(-1), m_lockUntilSec(0)
{
}

AuthScreenLockStore::~AuthScreenLockStore()
{
}

AuthScreenLockStore *AuthScreenLockStore::GetInstance()
{
	if (s_instance == NULL)
		s_instance = new AuthScreenLockStore();
	return s_instance;
}

int AuthScreenLockStore::Load()
{
	int ret = AuthPasscodeStoreBase::Load();
	if (ret != 0)
		return ret;

	ret = sceRegMgrGetKeyInt("/CONFIG/SECURITY/SCREEN_LOCK", "times_left", &m_attemptsLeft);
	if (ret != 0) {
		shellRegMgrLogError(ret, 1);
		return ret;
	}

	int lockTime = 0;
	ret = sceRegMgrGetKeyInt("/CONFIG/SECURITY/SCREEN_LOCK", "input_lock_time", &lockTime);
	if (ret != 0)
		shellRegMgrLogError(ret, 1);
	else
		m_lockUntilSec = (SceUInt32)lockTime;
	return ret;
}

int AuthScreenLockStore::ResetLockout()
{
	m_attemptsLeft = kScreenLockMaxAttempts;
	int ret = sceRegMgrSetKeyInt("/CONFIG/SECURITY/SCREEN_LOCK", "times_left",
				     kScreenLockMaxAttempts);
	if (ret != 0) {
		shellRegMgrLogError(ret, 1);
		return ret;
	}

	m_lockUntilSec = 0;
	ret = sceRegMgrSetKeyInt("/CONFIG/SECURITY/SCREEN_LOCK", "input_lock_time", 0);
	if (ret != 0)
		shellRegMgrLogError(ret, 1);
	return ret;
}

int AuthScreenLockStore::GetAttemptsLeft() const
{
	return m_attemptsLeft;
}

int AuthScreenLockStore::DecrementAttempts()
{
	--m_attemptsLeft;
	int ret = sceRegMgrSetKeyInt("/CONFIG/SECURITY/SCREEN_LOCK", "times_left", m_attemptsLeft);
	if (ret != 0)
		shellRegMgrLogError(ret, 1);
	return ret;
}

bool AuthScreenLockStore::IsLockedOut()
{
	if (m_attemptsLeft > 0)
		return false;

	paf::rtc::Tick tick = 0;
	paf::rtc::GetCurrentTick(&tick);
	paf::rtc::TickAddMinutes(&tick, &tick, 0);
	paf::rtc::TickAddYears(&tick, &tick, kScreenLockEpochYearOffset);
	SceUInt32 nowSec = (SceUInt32)(tick / 1000000ULL);
	if (nowSec < m_lockUntilSec)
		return true;

	ResetLockout();
	return false;
}

int AuthScreenLockStore::TriggerLockout()
{
	paf::rtc::Tick tick = 0;
	paf::rtc::GetCurrentTick(&tick);
	paf::rtc::TickAddMinutes(&tick, &tick, kScreenLockTimeoutMinutes);
	paf::rtc::TickAddYears(&tick, &tick, kScreenLockEpochYearOffset);
	m_lockUntilSec = (SceUInt32)(tick / 1000000ULL);
	int ret = sceRegMgrSetKeyInt("/CONFIG/SECURITY/SCREEN_LOCK", "input_lock_time",
				     (int)m_lockUntilSec);
	if (ret != 0)
		shellRegMgrLogError(ret, 1);
	return ret;
}

const wchar_t *AuthScreenLockStore::GetVerifyPrompt() const
{
	return getPowerManageString(kMsgScreenLockVerifyHash);
}

const wchar_t *AuthScreenLockStore::GetSetPrompt() const
{
	return getPowerManageString(kMsgScreenLockSetHash);
}

SceSize AuthScreenLockStore::GetBufferSize() const
{
	return 32;
}

paf::string AuthScreenLockStore::GetRegistryPath() const
{
	return paf::string("/CONFIG/SECURITY/SCREEN_LOCK/passcode");
}

class AuthParentalStore : public AuthPasscodeStoreBase {
public:
	AuthParentalStore();
	virtual ~AuthParentalStore();

	static AuthParentalStore *GetInstance();

	virtual const wchar_t *GetVerifyPrompt() const;
	virtual const wchar_t *GetSetPrompt() const;
	virtual int ReadPasscode(const paf::string &keyPath, paf::string *outPasscode);
	virtual SceSize GetBufferSize() const;
	virtual paf::string GetRegistryPath() const;

	static AuthParentalStore *s_instance;
};

AuthParentalStore *AuthParentalStore::s_instance = NULL;

AuthParentalStore::AuthParentalStore()
{
}

AuthParentalStore::~AuthParentalStore()
{
}

AuthParentalStore *AuthParentalStore::GetInstance()
{
	if (s_instance == NULL)
		s_instance = new AuthParentalStore();
	return s_instance;
}

const wchar_t *AuthParentalStore::GetVerifyPrompt() const
{
	return getPowerManageString(kMsgParentalVerifyHash);
}

const wchar_t *AuthParentalStore::GetSetPrompt() const
{
	return getPowerManageString(kMsgParentalSetHash);
}

int AuthParentalStore::ReadPasscode(const paf::string &keyPath, paf::string *outPasscode)
{
	(void)keyPath;
	SceSize bufSize = GetBufferSize() + 1;
	char *buffer = new char[bufSize];
	sce_paf_memset(buffer, '0', bufSize);
	int ret = sceRegMgrSystemParamGetStr(kParentalPasscodeSystemParamId, buffer, bufSize);
	if (ret != 0)
		*outPasscode = "";
	else
		*outPasscode = buffer;
	delete[] buffer;
	return ret;
}

SceSize AuthParentalStore::GetBufferSize() const
{
	return 5;
}

paf::string AuthParentalStore::GetRegistryPath() const
{
	return paf::string("/CONFIG/SECURITY/PARENTAL/passcode");
}

/* --- Auth View Listener & Views --- */

class AuthViewListener {
public:
	virtual ~AuthViewListener();
	virtual void OnViewInput(int eventCode, const paf::string &passcode) = 0;
	virtual void OnViewClosed(int result) = 0;
};

AuthViewListener::~AuthViewListener()
{
}

class AuthViewBase {
public:
	AuthViewBase(float timeoutMs = 0.0f);
	virtual ~AuthViewBase();

	virtual int Open(const paf::wstring &prompt) = 0;
	virtual int Reopen(const paf::wstring &prompt) = 0;
	virtual void Close(int result) = 0;
	virtual void OnTimeout(int32_t eventType, paf::ui::Handler *handler,
			       paf::ui::Event *event) = 0;

	void SetListener(AuthViewListener *listener);
	void StartTimeout(paf::ui::Handler *target);
	void StopTimeout(paf::ui::Handler *target);

	static void OnTimeoutThunk(int32_t eventType, paf::ui::Handler *handler,
				   paf::ui::Event *event, void *userData);

	AuthViewListener *m_listener;
	paf::vector<paf::ui::Widget *> m_dotWidgets;
	float m_timeoutMs;
};

AuthViewBase::AuthViewBase(float timeoutMs) : m_listener(NULL), m_timeoutMs(timeoutMs)
{
}

AuthViewBase::~AuthViewBase()
{
}

void AuthViewBase::SetListener(AuthViewListener *listener)
{
	m_listener = listener;
}

void AuthViewBase::StartTimeout(paf::ui::Handler *target)
{
	if (m_timeoutMs > 0.0f) {
		target->SetTimerListener(kAuthTimeoutTimerId, m_timeoutMs,
					 new paf::ui::EventCBListener(OnTimeoutThunk, this));
	}
}

void AuthViewBase::StopTimeout(paf::ui::Handler *target)
{
	if (m_timeoutMs > 0.0f)
		target->KillTimer(kAuthTimeoutTimerId);
}

void AuthViewBase::OnTimeoutThunk(int32_t eventType, paf::ui::Handler *handler,
				  paf::ui::Event *event, void *userData)
{
	AuthViewBase *self = (AuthViewBase *)userData;
	if (self != NULL)
		self->OnTimeout(eventType, handler, event);
}

class AuthDialogView : public AuthViewBase {
public:
	class DialogHandler : public CommonGuiDialogHandler {
	public:
		DialogHandler(AuthDialogView *view, AuthDialogOpenCallback openCb, void *userData);
		virtual void operator()(SceInt32 dialogId, SceInt32 button);
		static void OnDelayCancel(void *arg);

		AuthDialogView *m_view;
		AuthDialogOpenCallback m_openCb;
		void *m_userData;
	};

	AuthDialogView(AuthDialogOpenCallback openCb, void *userData);
	virtual ~AuthDialogView();

	paf::ui::TextBox *FindDialogTextBox();
	void DetachDialogCallbacks();
	void OpenInputDialog(const paf::wstring &prompt);

	virtual int Open(const paf::wstring &prompt);
	virtual int Reopen(const paf::wstring &prompt);
	virtual void Close(int result);
	virtual void OnTimeout(int32_t eventType, paf::ui::Handler *handler,
			       paf::ui::Event *event);

	void HandleStringChanged(paf::ui::Text *textWidget);
	void HandleEditEnd(paf::ui::Text *textWidget);

	static void OnStringChanged(int32_t eventType, paf::ui::Handler *handler,
				    paf::ui::Event *event, void *userData);
	static void OnEditEnd(int32_t eventType, paf::ui::Handler *handler,
			      paf::ui::Event *event, void *userData);

	int m_dialogId;
	AuthDialogOpenCallback m_openCallback;
	void *m_openUserData;
	int m_priority;
};

AuthDialogView::DialogHandler::DialogHandler(AuthDialogView *view, AuthDialogOpenCallback openCb,
					     void *userData)
	: m_view(view), m_openCb(openCb), m_userData(userData)
{
}

void AuthDialogView::DialogHandler::operator()(SceInt32 dialogId, SceInt32 button)
{
	(void)dialogId;
	if (button != 0) {
		m_view->DetachDialogCallbacks();
		paf::thread::CallQueue::main_thread_callqueue.Enqueue(
			"AuthPluginDelayEditEnd", OnDelayCancel, this);
	} else if (m_openCb != NULL) {
		m_openCb(m_userData);
	}
}

void AuthDialogView::DialogHandler::OnDelayCancel(void *arg)
{
	DialogHandler *self = (DialogHandler *)arg;
	if (self != NULL && self->m_view->m_listener != NULL) {
		paf::string empty;
		self->m_view->m_listener->OnViewInput(AUTH_VIEW_EVENT_CANCELED, empty);
	}
}

AuthDialogView::AuthDialogView(AuthDialogOpenCallback openCb, void *userData)
	: AuthViewBase(0.0f),
	  m_dialogId(-1),
	  m_openCallback(openCb),
	  m_openUserData(userData),
	  m_priority(-1)
{
}

AuthDialogView::~AuthDialogView()
{
}

paf::ui::TextBox *AuthDialogView::FindDialogTextBox()
{
	if (m_dialogId == -1)
		return NULL;
	paf::ui::Widget *widget = SceCommonGuiDialog_8947FC23(m_dialogId, 22);
	if (widget != NULL && widget->IsInherit(paf::ui::TextBox::TypeName()))
		return (paf::ui::TextBox *)widget;
	return NULL;
}

void AuthDialogView::DetachDialogCallbacks()
{
	if (m_dialogId == -1)
		return;
	paf::ui::TextBox *textBox = FindDialogTextBox();
	if (textBox != NULL) {
		textBox->DeleteEventCallback(paf::ui::TextBox::CB_TEXT_BOX_EDIT_END,
					     OnEditEnd, this);
		textBox->DeleteEventCallback(paf::ui::TextBox::CB_TEXT_BOX_STRING_CHANGED,
					     OnStringChanged, this);
	}
	SceCommonGuiDialog_11D35CB5(m_dialogId);
	m_dialogId = -1;
}

void AuthDialogView::OpenInputDialog(const paf::wstring &prompt)
{
	paf::Plugin *plugin = paf::Plugin::Find("power_manage_plugin");

	CommonGuiInputDialogParam inputParam;
	inputParam.wait = false;
	inputParam.maxLength = kPasscodeDigits;
	inputParam.outText = NULL;
	inputParam.thirdButton = false;
	inputParam.resourcePlugin = plugin;
	inputParam.text = prompt;
	inputParam.handler = new DialogHandler(this, m_openCallback, m_openUserData);
	inputParam.notifyClose = true;
	inputParam.priority = m_priority;
	inputParam.imeType = (m_priority == -1 || m_priority > 35) ? 2 : 3;

	m_dialogId = SceCommonGuiDialog_7C019CF0(&inputParam);

	paf::ui::Widget *titleWidget = SceCommonGuiDialog_8947FC23(m_dialogId, 1);
	if (titleWidget != NULL && titleWidget->IsInherit(paf::ui::Text::TypeName())) {
		((paf::ui::Text *)titleWidget)->SetStyleAttribute(
			(paf::graph::TextStyleAttribute)14, 0, 0, true);
	}

	paf::ui::TextBox *textBox = FindDialogTextBox();
	textBox->AddEventListener(paf::ui::TextBox::CB_TEXT_BOX_EDIT_END,
				  new paf::ui::EventCBListener(OnEditEnd, this), true);
	textBox->AddEventListener(paf::ui::TextBox::CB_TEXT_BOX_STRING_CHANGED,
				  new paf::ui::EventCBListener(OnStringChanged, this), true);

	paf::math::v4 transparentColor(1.0f, 1.0f, 1.0f, 0.0f);
	textBox->SetColor(transparentColor);

	paf::math::v4 boxSize = *textBox->GetSize(0);
	paf::Plugin::TemplateOpenParam tplParam;
	plugin->TemplateOpen(textBox, paf::IDParam(kPasscodeDotsTemplateHash), tplParam);
	paf::ui::Widget *dotBox = textBox->FindChild(paf::IDParam(kPasscodeDotsBoxHash), 0);

	for (SceUInt32 idx = 0; idx < kPasscodeDigits; ++idx) {
		paf::ui::Widget *cell = dotBox->GetChild((int32_t)idx);
		paf::math::v4 cellSize = *cell->GetSize(0);
		cellSize.set_y(boxSize.extract_y());
		cell->SetSize(cellSize);
		paf::ui::Widget *dot = cell->GetChild(0);
		dot->Hide(0xFF, NULL, NULL, 0.0f);
		m_dotWidgets.push_back(dot);
	}

	paf::ui::Widget *okButton = SceCommonGuiDialog_8947FC23(m_dialogId, 13);
	if (okButton != NULL)
		okButton->DestroyWidget();
	paf::ui::Widget *cancelButton = SceCommonGuiDialog_8947FC23(m_dialogId, 12);
	if (cancelButton != NULL)
		cancelButton->Hide(0xFF, NULL, NULL, 0.0f);
}

int AuthDialogView::Open(const paf::wstring &prompt)
{
	paf::Plugin *plugin = paf::Plugin::Find("power_manage_plugin");
	paf::Plugin::PageOpenParam openParam;
	openParam.overwrite_draw_priority = kAuthOverlayPriority;
	openParam.fade = true;
	paf::ui::Widget *page = plugin->PageOpen(paf::IDParam("auth_dialog_page"), openParam);
	OpenInputDialog(prompt);
	StartTimeout(page);
	return 0;
}

int AuthDialogView::Reopen(const paf::wstring &prompt)
{
	Close(AUTH_VIEW_EVENT_RESET);
	return Open(prompt);
}

void AuthDialogView::Close(int result)
{
	paf::Plugin *plugin = paf::Plugin::Find("power_manage_plugin");
	paf::ui::Widget *page = plugin->PageRoot(paf::IDParam("auth_dialog_page"));
	StopTimeout(page);

	paf::Plugin::PageCloseParam closeParam;
	plugin->PageClose(paf::IDParam("auth_dialog_page"), closeParam);

	DetachDialogCallbacks();
	m_dotWidgets.clear();

	if (result != AUTH_VIEW_EVENT_RESET && m_listener != NULL)
		m_listener->OnViewClosed(result);
}

void AuthDialogView::OnTimeout(int32_t eventType, paf::ui::Handler *handler, paf::ui::Event *event)
{
	(void)eventType;
	(void)handler;
	(void)event;
	if (m_listener != NULL) {
		paf::string empty;
		m_listener->OnViewInput(AUTH_VIEW_EVENT_TIMEOUT, empty);
	}
}

void AuthDialogView::HandleStringChanged(paf::ui::Text *textWidget)
{
	if (m_dialogId == -1)
		return;

	uint32_t length = 0;
	textWidget->GetLength(length);
	for (SceUInt32 idx = 0; idx < kPasscodeDigits; ++idx) {
		if (idx < length)
			m_dotWidgets[idx]->Show(0xFF, NULL, NULL, 0.0f);
		else
			m_dotWidgets[idx]->Hide(0xFF, NULL, NULL, 0.0f);
	}

	paf::Plugin *plugin = paf::Plugin::Find("power_manage_plugin");
	paf::ui::Widget *page = plugin->PageRoot(paf::IDParam("auth_dialog_page"));
	if (length == kPasscodeDigits) {
		StopTimeout(page);
		paf::string entered = SceCommonGuiDialog_1D4E3C83(m_dialogId);
		DetachDialogCallbacks();
		if (m_listener != NULL)
			m_listener->OnViewInput(AUTH_VIEW_EVENT_SUBMITTED, entered);
	} else {
		StartTimeout(page);
	}
}

void AuthDialogView::HandleEditEnd(paf::ui::Text *textWidget)
{
	if (m_listener == NULL)
		return;

	paf::wstring wideText;
	textWidget->GetString(wideText);
	paf::common::EncodedString encoded;
	ScePafMisc_7709D1B2(&encoded, wideText, paf::common::EncodedString::Encoding_Utf16);
	paf::string utf8Text(ScePafMisc_A7C6AF5C(&encoded));
	ScePafMisc_B45A7521(&encoded);

	if (utf8Text.length() == kPasscodeDigits) {
		m_listener->OnViewInput(AUTH_VIEW_EVENT_SUBMITTED, utf8Text);
	} else {
		paf::string empty;
		m_listener->OnViewInput(AUTH_VIEW_EVENT_CANCELED, empty);
	}
}

void AuthDialogView::OnStringChanged(int32_t eventType, paf::ui::Handler *handler,
				     paf::ui::Event *event, void *userData)
{
	(void)eventType;
	(void)event;
	AuthDialogView *self = (AuthDialogView *)userData;
	if (self != NULL)
		self->HandleStringChanged((paf::ui::Text *)handler);
}

void AuthDialogView::OnEditEnd(int32_t eventType, paf::ui::Handler *handler,
			       paf::ui::Event *event, void *userData)
{
	(void)eventType;
	(void)event;
	AuthDialogView *self = (AuthDialogView *)userData;
	if (self != NULL)
		self->HandleEditEnd((paf::ui::Text *)handler);
}

class AuthLockScreenView : public AuthViewBase {
public:
	AuthLockScreenView();
	virtual ~AuthLockScreenView();

	virtual int Open(const paf::wstring &prompt);
	virtual int Reopen(const paf::wstring &prompt);
	void FinishClose(paf::ui::Widget *page);
	virtual void Close(int result);
	virtual void OnTimeout(int32_t eventType, paf::ui::Handler *handler,
			       paf::ui::Event *event);

	void HandleEditEnd(paf::ui::Text *textWidget);

	static void OnTextBoxEvent(int32_t eventType, paf::ui::Handler *handler,
				   paf::ui::Event *event, void *userData);
	static void OnCloseDelayTimer(int32_t eventType, paf::ui::Handler *handler,
				      paf::ui::Event *event, void *userData);
};

AuthLockScreenView::AuthLockScreenView() : AuthViewBase(0.0f)
{
}

AuthLockScreenView::~AuthLockScreenView()
{
	paf::ui::Widget *page = g_powerManagePlugin->PageRoot(paf::IDParam(kLockScreenPageHash));
	if (page != NULL) {
		paf::Plugin::PageCloseParam closeParam;
		g_powerManagePlugin->PageClose(paf::IDParam(kLockScreenPageHash), closeParam);
	}
}

int AuthLockScreenView::Open(const paf::wstring &prompt)
{
	(void)prompt;
	paf::Plugin *plugin = paf::Plugin::Find("power_manage_plugin");
	paf::Plugin::PageOpenParam openParam;
	openParam.overwrite_draw_priority = kAuthOverlayPriority;
	paf::ui::Widget *page = plugin->PageOpen(paf::IDParam(kLockScreenPageHash), openParam);
	if (page == NULL)
		return -1;

	paf::ui::Widget *child = page->FindChild(paf::IDParam(kLockScreenTextBoxHash), 0);
	paf::ui::TextBox *textBox =
		(child != NULL && child->IsInherit(paf::ui::TextBox::TypeName()))
			? (paf::ui::TextBox *)child
			: NULL;
	textBox->AddEventListener(paf::ui::TextBox::CB_TEXT_BOX_EDIT_END,
				  new paf::ui::EventCBListener(OnTextBoxEvent, this), true);
	textBox->AddEventListener(paf::ui::TextBox::CB_TEXT_BOX_STRING_CHANGED,
				  new paf::ui::EventCBListener(OnTextBoxEvent, this), true);
	textBox->BeginEdit();
	StartTimeout(page);
	return 0;
}

int AuthLockScreenView::Reopen(const paf::wstring &prompt)
{
	Close(AUTH_VIEW_EVENT_RESET);
	return Open(prompt);
}

void AuthLockScreenView::FinishClose(paf::ui::Widget *page)
{
	int closeResult = (int)page->m_user_data[0];
	paf::Plugin *plugin = paf::Plugin::Find("power_manage_plugin");
	paf::Plugin::PageCloseParam closeParam;
	plugin->PageClose(paf::IDParam(kLockScreenPageHash), closeParam);
	if (m_listener != NULL)
		m_listener->OnViewClosed(closeResult);
}

void AuthLockScreenView::Close(int result)
{
	paf::Plugin *plugin = paf::Plugin::Find("power_manage_plugin");
	if (result == AUTH_VIEW_EVENT_RESET) {
		paf::Plugin::PageCloseParam closeParam;
		plugin->PageClose(paf::IDParam(kLockScreenPageHash), closeParam);
		return;
	}

	paf::ui::Widget *page = plugin->PageRoot(paf::IDParam(kLockScreenPageHash));
	if (page == NULL) {
		if (m_listener != NULL)
			m_listener->OnViewClosed(result);
		return;
	}

	if (result == AUTH_VIEW_EVENT_ABORTED) {
		page->m_user_data[0] = (void *)(intptr_t)AUTH_VIEW_EVENT_ABORTED;
		FinishClose(page);
	} else {
		ScePafMisc_CAABF0B0(kAuthAllInputDevices, 300.0f);
		page->m_user_data[0] = (void *)(intptr_t)result;
		page->SetTimerListener(kAuthTimeoutTimerId, 300.0f,
				       new paf::ui::EventCBListener(OnCloseDelayTimer, this));
		page->SetMetaAlpha(0.0f, new paf::Timer(300.0f, paf::Timer::FUNC_QUAD_IN));
	}
}

void AuthLockScreenView::OnTimeout(int32_t eventType, paf::ui::Handler *handler,
				   paf::ui::Event *event)
{
	(void)eventType;
	(void)handler;
	(void)event;
	if (m_listener != NULL) {
		paf::string empty;
		m_listener->OnViewInput(AUTH_VIEW_EVENT_TIMEOUT, empty);
	}
}

void AuthLockScreenView::HandleEditEnd(paf::ui::Text *textWidget)
{
	if (m_listener == NULL)
		return;

	paf::wstring wideText;
	textWidget->GetString(wideText);
	paf::common::EncodedString encoded;
	ScePafMisc_7709D1B2(&encoded, wideText, paf::common::EncodedString::Encoding_Utf16);
	paf::string utf8Text(ScePafMisc_A7C6AF5C(&encoded));
	ScePafMisc_B45A7521(&encoded);

	if (utf8Text.length() == kPasscodeDigits)
		m_listener->OnViewInput(AUTH_VIEW_EVENT_SUBMITTED, utf8Text);
	else
		m_listener->OnViewInput(AUTH_VIEW_EVENT_CANCELED, utf8Text);
}

void AuthLockScreenView::OnTextBoxEvent(int32_t eventType, paf::ui::Handler *handler,
					paf::ui::Event *event, void *userData)
{
	(void)event;
	AuthLockScreenView *self = (AuthLockScreenView *)userData;
	if (self == NULL)
		return;

	paf::ui::TextBox *textBox = (paf::ui::TextBox *)handler;
	if (eventType == paf::ui::TextBox::CB_TEXT_BOX_STRING_CHANGED) {
		uint32_t length = 0;
		textBox->GetLength(length);
		if (length == kPasscodeDigits) {
			self->StopTimeout(textBox->m_scene);
			textBox->EndEdit();
		} else {
			self->StartTimeout(textBox->m_scene);
		}
	} else if (eventType == paf::ui::TextBox::CB_TEXT_BOX_EDIT_END) {
		self->HandleEditEnd(textBox);
	}
}

void AuthLockScreenView::OnCloseDelayTimer(int32_t eventType, paf::ui::Handler *handler,
					   paf::ui::Event *event, void *userData)
{
	(void)event;
	AuthLockScreenView *self = (AuthLockScreenView *)userData;
	if (self != NULL && eventType == kAuthTimeoutTimerId)
		self->FinishClose((paf::ui::Widget *)handler);
}

/* --- Auth Session & Actions --- */

class AuthSessionListener {
public:
	virtual ~AuthSessionListener();
	virtual void OnActionFinished(int result) = 0;
};

AuthSessionListener::~AuthSessionListener()
{
}

class AuthActionBase : public AuthViewListener {
public:
	class ErrorDialogHandler : public CommonGuiDialogHandler {
	public:
		ErrorDialogHandler(AuthActionBase *action);
		virtual void operator()(SceInt32 dialogId, SceInt32 button);

		AuthActionBase *m_action;
	};

	AuthActionBase(AuthViewBase *view, AuthPasscodeStoreBase *store,
		       AuthSessionListener *session, int priority);
	virtual ~AuthActionBase();

	int Start();
	void Finish(int result);

	virtual void OnViewInput(int eventCode, const paf::string &passcode);
	virtual void OnViewClosed(int result);
	virtual void OnErrorDialogClosed(int dialogId);
	virtual int CheckPrecondition();
	virtual const wchar_t *GetPrompt() const = 0;
	virtual void OnPasscodeEntered(const paf::string &passcode) = 0;

	void ShowErrorDialog(SceUInt32 messageHash, const wchar_t *formatArg);

	AuthViewBase *m_view;
	AuthPasscodeStoreBase *m_store;
	AuthSessionListener *m_session;
	paf::vector<int> m_unused;
	int m_errorDialogId;
	int m_priority;
};

AuthActionBase::ErrorDialogHandler::ErrorDialogHandler(AuthActionBase *action) : m_action(action)
{
}

void AuthActionBase::ErrorDialogHandler::operator()(SceInt32 dialogId, SceInt32 button)
{
	(void)button;
	m_action->OnErrorDialogClosed(dialogId);
}

AuthActionBase::AuthActionBase(AuthViewBase *view, AuthPasscodeStoreBase *store,
			       AuthSessionListener *session, int priority)
	: m_view(view),
	  m_store(store),
	  m_session(session),
	  m_errorDialogId(-1),
	  m_priority(priority)
{
}

AuthActionBase::~AuthActionBase()
{
}

int AuthActionBase::Start()
{
	int ret = CheckPrecondition();
	if (ret == 0) {
		paf::wstring prompt(GetPrompt());
		return m_view->Open(prompt);
	}
	return ret;
}

void AuthActionBase::Finish(int result)
{
	if (m_errorDialogId != -1) {
		SceCommonGuiDialog_BC311DC0(m_errorDialogId);
		m_errorDialogId = -1;
	}
	m_view->Close(result);
}

void AuthActionBase::OnViewInput(int eventCode, const paf::string &passcode)
{
	if (eventCode == AUTH_VIEW_EVENT_SUBMITTED) {
		OnPasscodeEntered(passcode);
	} else if (eventCode == AUTH_VIEW_EVENT_CANCELED || eventCode == AUTH_VIEW_EVENT_TIMEOUT) {
		Finish(eventCode);
	}
}

void AuthActionBase::OnViewClosed(int result)
{
	m_session->OnActionFinished(result);
}

void AuthActionBase::OnErrorDialogClosed(int dialogId)
{
	m_errorDialogId = -1;
	SceCommonGuiDialog_BC311DC0(dialogId);
}

int AuthActionBase::CheckPrecondition()
{
	return 0;
}

void AuthActionBase::ShowErrorDialog(SceUInt32 messageHash, const wchar_t *formatArg)
{
	paf::Plugin *plugin = paf::Plugin::Find("power_manage_plugin");
	paf::wstring message(plugin->GetString(paf::IDParam(messageHash)));
	if (formatArg != NULL && formatArg[0] != L'\0')
		message = shellFormatText(message.c_str(), formatArg);

	CommonGuiLayoutDialogParam dialogParam;
	dialogParam.resourcePlugin = plugin;
	dialogParam.text = message;
	dialogParam.layout = SceCommonGuiDialog_AF75CA6D;
	dialogParam.priority = m_priority;
	dialogParam.handler = new ErrorDialogHandler(this);
	dialogParam.soundId = kAuthErrorSoundHash;
	m_errorDialogId = SceCommonGuiDialog_CCFCD9A6(&dialogParam);
}

class AuthVerifyAction : public AuthActionBase {
public:
	AuthVerifyAction(AuthViewBase *view, AuthPasscodeStoreBase *store,
			 AuthSessionListener *session, int priority);
	virtual ~AuthVerifyAction();

	void EnableLockoutEnforcement();
	virtual void OnErrorDialogClosed(int dialogId);
	virtual int CheckPrecondition();
	virtual const wchar_t *GetPrompt() const;
	virtual void OnPasscodeEntered(const paf::string &passcode);

	bool m_enforceLockout;
};

AuthVerifyAction::AuthVerifyAction(AuthViewBase *view, AuthPasscodeStoreBase *store,
				   AuthSessionListener *session, int priority)
	: AuthActionBase(view, store, session, priority), m_enforceLockout(false)
{
}

AuthVerifyAction::~AuthVerifyAction()
{
}

void AuthVerifyAction::EnableLockoutEnforcement()
{
	m_enforceLockout = true;
}

void AuthVerifyAction::OnErrorDialogClosed(int dialogId)
{
	AuthActionBase::OnErrorDialogClosed(dialogId);
	if (m_store->GetAttemptsLeft() != 0) {
		paf::wstring prompt(GetPrompt());
		m_view->Reopen(prompt);
	} else {
		Finish(AUTH_VIEW_EVENT_LOCKED_OUT);
	}
}

int AuthVerifyAction::CheckPrecondition()
{
	return m_store->IsEnabled() ? AUTH_OK : AUTH_ERROR_NOT_SET;
}

const wchar_t *AuthVerifyAction::GetPrompt() const
{
	return m_store->GetVerifyPrompt();
}

void AuthVerifyAction::OnPasscodeEntered(const paf::string &passcode)
{
	if (m_store->CompareMismatch(passcode)) {
		if (m_enforceLockout) {
			m_store->DecrementAttempts();
			int remaining = m_store->GetAttemptsLeft();
			if (remaining <= 0) {
				m_store->TriggerLockout();
				ShowErrorDialog(kMsgLockedOutHash, L"");
			} else {
				wchar_t countBuf[4];
				sce_paf_swprintf(countBuf, 4, L"%d", remaining);
				ShowErrorDialog(kMsgWrongPasscodeAttemptsHash, countBuf);
			}
		} else {
			ShowErrorDialog(kMsgWrongPasscodeHash, L"");
		}
	} else {
		if (m_enforceLockout)
			m_store->ResetLockout();
		Finish(AUTH_OK);
	}
}

class AuthChangeAction : public AuthActionBase {
public:
	AuthChangeAction(AuthViewBase *view, AuthPasscodeStoreBase *store,
			 AuthSessionListener *session, int priority);
	virtual ~AuthChangeAction();

	virtual void OnErrorDialogClosed(int dialogId);
	virtual const wchar_t *GetPrompt() const;
	virtual void OnPasscodeEntered(const paf::string &passcode);

	int m_step;
	paf::string m_pendingPasscode;
};

AuthChangeAction::AuthChangeAction(AuthViewBase *view, AuthPasscodeStoreBase *store,
				   AuthSessionListener *session, int priority)
	: AuthActionBase(view, store, session, priority), m_step(0)
{
}

AuthChangeAction::~AuthChangeAction()
{
}

void AuthChangeAction::OnErrorDialogClosed(int dialogId)
{
	AuthActionBase::OnErrorDialogClosed(dialogId);
	paf::wstring prompt(GetPrompt());
	m_view->Reopen(prompt);
}

const wchar_t *AuthChangeAction::GetPrompt() const
{
	if (m_step == 0)
		return m_store->GetChangeCurrentPrompt();
	if (m_step == 1)
		return m_store->GetChangeNewPrompt();
	if (m_step == 2)
		return m_store->GetChangeConfirmPrompt();
	return L"";
}

void AuthChangeAction::OnPasscodeEntered(const paf::string &passcode)
{
	if (m_step == 0) {
		if (m_store->CompareMismatch(passcode)) {
			ShowErrorDialog(kMsgWrongPasscodeHash, L"");
			m_step = 0;
		} else {
			++m_step;
			paf::wstring prompt(GetPrompt());
			m_view->Reopen(prompt);
		}
	} else if (m_step == 1) {
		m_pendingPasscode = passcode;
		++m_step;
		paf::wstring prompt(GetPrompt());
		m_view->Reopen(prompt);
	} else if (m_step == 2) {
		if (m_pendingPasscode != passcode) {
			ShowErrorDialog(kMsgPasscodeMismatchHash, L"");
			m_step = 1;
		} else {
			int saveErr = m_store->Save(passcode);
			Finish(saveErr != 0 ? AUTH_ERROR_FAILED : AUTH_OK);
			m_step = 0;
		}
	}
}

class AuthSetAction : public AuthActionBase {
public:
	AuthSetAction(AuthViewBase *view, AuthPasscodeStoreBase *store,
		      AuthSessionListener *session, int priority);
	virtual ~AuthSetAction();

	virtual void OnErrorDialogClosed(int dialogId);
	virtual const wchar_t *GetPrompt() const;
	virtual void OnPasscodeEntered(const paf::string &passcode);

	int m_step;
	paf::string m_pendingPasscode;
};

AuthSetAction::AuthSetAction(AuthViewBase *view, AuthPasscodeStoreBase *store,
			     AuthSessionListener *session, int priority)
	: AuthActionBase(view, store, session, priority), m_step(0)
{
}

AuthSetAction::~AuthSetAction()
{
}

void AuthSetAction::OnErrorDialogClosed(int dialogId)
{
	AuthActionBase::OnErrorDialogClosed(dialogId);
	paf::wstring prompt(GetPrompt());
	m_view->Reopen(prompt);
}

const wchar_t *AuthSetAction::GetPrompt() const
{
	if (m_step == 0)
		return m_store->GetSetPrompt();
	if (m_step == 1)
		return m_store->GetConfirmPrompt();
	return L"";
}

void AuthSetAction::OnPasscodeEntered(const paf::string &passcode)
{
	if (m_step == 0) {
		m_pendingPasscode = passcode;
		++m_step;
		paf::wstring prompt(GetPrompt());
		m_view->Reopen(prompt);
	} else if (m_step == 1) {
		if (m_pendingPasscode != passcode) {
			ShowErrorDialog(kMsgPasscodeMismatchHash, L"");
			m_step = 0;
		} else {
			int saveErr = m_store->Save(passcode);
			Finish(saveErr != 0 ? AUTH_ERROR_FAILED : AUTH_OK);
			m_step = 0;
		}
	}
}

class AuthSession : public AuthSessionListener {
public:
	AuthSession();
	virtual ~AuthSession();

	void LoadStores();
	static AuthPasscodeStoreBase *ResolveStore(SceInt16 storeKind);
	int CreateAction(SceInt16 actionKind, AuthViewBase *view, AuthPasscodeStoreBase *store,
			 AuthActionBase **outAction, int priority);
	int CreateView(SceInt16 storeKind, AuthViewBase **outView,
		       AuthDialogOpenCallback openCb, void *userData);
	int StartOverlayDialog(SceInt16 storeKind, SceInt16 actionKind,
			       AuthResultCallback callback, void *userData,
			       AuthDialogOpenCallback openCb);
	int StartLockScreenVerify(SceInt16 storeKind, SceInt16 actionKind,
				  AuthResultCallback callback, void *userData);
	int StartStandardDialog(SceInt16 storeKind, SceInt16 actionKind,
				AuthResultCallback callback, void *userData,
				AuthDialogOpenCallback openCb);
	int Start(SceInt16 storeKind, SceInt16 actionKind, AuthResultCallback callback,
		  void *userData, AuthDialogOpenCallback openCb);
	void Cancel();
	int IsPasscodeEnabled(SceInt16 storeKind, bool *outEnabled);
	virtual void OnActionFinished(int result);

	AuthActionBase *m_action;
	AuthViewBase *m_view;
	AuthResultCallback m_resultCallback;
	void *m_resultUserData;
};

AuthSession::AuthSession()
	: m_action(NULL), m_view(NULL), m_resultCallback(NULL), m_resultUserData(NULL)
{
}

AuthSession::~AuthSession()
{
	delete m_action;
	m_action = NULL;
	delete m_view;
	m_view = NULL;
}

void AuthSession::LoadStores()
{
	AuthParentalStore::GetInstance()->Load();
	AuthScreenLockStore::GetInstance()->Load();
}

AuthPasscodeStoreBase *AuthSession::ResolveStore(SceInt16 storeKind)
{
	if (storeKind == AUTH_STORE_PARENTAL_DIALOG || storeKind == AUTH_STORE_PARENTAL_OVERLAY)
		return AuthParentalStore::GetInstance();
	if (storeKind == AUTH_STORE_SCREEN_LOCK_DIALOG ||
	    storeKind == AUTH_STORE_SCREEN_LOCK_SCREEN)
		return AuthScreenLockStore::GetInstance();
	return NULL;
}

int AuthSession::CreateAction(SceInt16 actionKind, AuthViewBase *view,
			      AuthPasscodeStoreBase *store, AuthActionBase **outAction,
			      int priority)
{
	if (actionKind == AUTH_ACTION_SET) {
		*outAction = new AuthSetAction(view, store, this, priority);
		return AUTH_OK;
	}
	if (actionKind == AUTH_ACTION_CHANGE) {
		*outAction = new AuthChangeAction(view, store, this, priority);
		return AUTH_OK;
	}
	if (actionKind == AUTH_ACTION_VERIFY) {
		*outAction = new AuthVerifyAction(view, store, this, priority);
		return AUTH_OK;
	}
	*outAction = NULL;
	return AUTH_ERROR_INVALID;
}

int AuthSession::CreateView(SceInt16 storeKind, AuthViewBase **outView,
			    AuthDialogOpenCallback openCb, void *userData)
{
	if (storeKind >= AUTH_STORE_PARENTAL_DIALOG && storeKind <= AUTH_STORE_SCREEN_LOCK_DIALOG) {
		*outView = new AuthDialogView(openCb, userData);
		return AUTH_OK;
	}
	if (storeKind == AUTH_STORE_SCREEN_LOCK_SCREEN) {
		*outView = new AuthLockScreenView();
		return AUTH_OK;
	}
	*outView = NULL;
	return AUTH_ERROR_INVALID;
}

int AuthSession::StartOverlayDialog(SceInt16 storeKind, SceInt16 actionKind,
				    AuthResultCallback callback, void *userData,
				    AuthDialogOpenCallback openCb)
{
	AuthPasscodeStoreBase *store = ResolveStore(storeKind);
	if (store == NULL)
		return AUTH_ERROR_INVALID;

	delete m_view;
	m_view = NULL;
	int ret = CreateView(storeKind, &m_view, openCb, userData);
	if (ret != AUTH_OK)
		return ret;

	((AuthDialogView *)m_view)->m_priority = kAuthOverlayPriority;

	delete m_action;
	m_action = NULL;
	ret = CreateAction(actionKind, m_view, store, &m_action, kAuthOverlayPriority);
	if (ret != AUTH_OK)
		return ret;

	m_view->SetListener(m_action);
	m_resultCallback = callback;
	m_resultUserData = userData;
	return m_action->Start();
}

int AuthSession::StartLockScreenVerify(SceInt16 storeKind, SceInt16 actionKind,
				       AuthResultCallback callback, void *userData)
{
	if (actionKind != AUTH_ACTION_VERIFY)
		return AUTH_ERROR_INVALID;

	AuthPasscodeStoreBase *store = ResolveStore(storeKind);
	if (store == NULL)
		return AUTH_ERROR_INVALID;

	delete m_view;
	m_view = NULL;
	int ret = CreateView(storeKind, &m_view, NULL, NULL);
	if (ret != AUTH_OK)
		return ret;

	delete m_action;
	m_action = NULL;
	ret = CreateAction(AUTH_ACTION_VERIFY, m_view, store, &m_action, kAuthOverlayPriority);
	if (ret != AUTH_OK)
		return ret;

	if (store->IsLockedOut())
		return AUTH_ERROR_LOCKED_OUT;

	m_view->SetListener(m_action);
	m_resultCallback = callback;
	m_resultUserData = userData;
	((AuthVerifyAction *)m_action)->EnableLockoutEnforcement();
	m_action->Start();
	return AUTH_OK;
}

int AuthSession::StartStandardDialog(SceInt16 storeKind, SceInt16 actionKind,
				     AuthResultCallback callback, void *userData,
				     AuthDialogOpenCallback openCb)
{
	AuthPasscodeStoreBase *store = ResolveStore(storeKind);
	if (store == NULL)
		return AUTH_ERROR_INVALID;

	delete m_view;
	m_view = NULL;
	int ret = CreateView(storeKind, &m_view, openCb, userData);
	if (ret != AUTH_OK)
		return ret;

	delete m_action;
	m_action = NULL;
	ret = CreateAction(actionKind, m_view, store, &m_action, -1);
	if (ret != AUTH_OK)
		return ret;

	m_view->SetListener(m_action);
	m_resultCallback = callback;
	m_resultUserData = userData;
	return m_action->Start();
}

int AuthSession::Start(SceInt16 storeKind, SceInt16 actionKind, AuthResultCallback callback,
		       void *userData, AuthDialogOpenCallback openCb)
{
	if (m_action != NULL)
		m_action->Finish(AUTH_VIEW_EVENT_ABORTED);

	if (storeKind == AUTH_STORE_PARENTAL_DIALOG ||
	    storeKind == AUTH_STORE_SCREEN_LOCK_DIALOG) {
		return StartStandardDialog(storeKind, actionKind, callback, userData, openCb);
	}
	if (storeKind == AUTH_STORE_PARENTAL_OVERLAY)
		return StartOverlayDialog(storeKind, actionKind, callback, userData, openCb);
	if (storeKind == AUTH_STORE_SCREEN_LOCK_SCREEN)
		return StartLockScreenVerify(storeKind, actionKind, callback, userData);
	return AUTH_ERROR_INVALID;
}

void AuthSession::Cancel()
{
	if (m_action != NULL)
		m_action->Finish(AUTH_VIEW_EVENT_ABORTED);
}

int AuthSession::IsPasscodeEnabled(SceInt16 storeKind, bool *outEnabled)
{
	AuthPasscodeStoreBase *store = ResolveStore(storeKind);
	if (store == NULL) {
		*outEnabled = false;
		return AUTH_ERROR_INVALID;
	}
	*outEnabled = store->IsEnabled();
	return AUTH_OK;
}

void AuthSession::OnActionFinished(int result)
{
	if (m_resultCallback != NULL)
		m_resultCallback(result, m_resultUserData);
	m_resultCallback = NULL;
	m_resultUserData = NULL;
	delete m_action;
	m_action = NULL;
	delete m_view;
	m_view = NULL;
}

class AuthRegistryLoadJob : public paf::job::JobItem {
public:
	AuthRegistryLoadJob(paf::common::SharedPtr<AuthSession> session);
	virtual ~AuthRegistryLoadJob();

	virtual int Run();
	virtual void Finish(int result);
	bool IsCompleted() const;

	paf::common::SharedPtr<AuthSession> m_session;
	bool m_completed;
};

AuthRegistryLoadJob::AuthRegistryLoadJob(paf::common::SharedPtr<AuthSession> session)
	: paf::job::JobItem("RegistryLoadJob", NULL), m_session(session), m_completed(false)
{
}

AuthRegistryLoadJob::~AuthRegistryLoadJob()
{
}

int AuthRegistryLoadJob::Run()
{
	m_session->LoadStores();
	return 0;
}

void AuthRegistryLoadJob::Finish(int result)
{
	(void)result;
	if (!IsCanceled())
		m_completed = true;
}

bool AuthRegistryLoadJob::IsCompleted() const
{
	return m_completed;
}

class AuthService {
public:
	AuthService();
	~AuthService();

	void EnqueueRegistryLoad();
	void CancelRegistryLoad();

	paf::common::SharedPtr<AuthSession> m_session;
	paf::common::SharedPtr<AuthRegistryLoadJob> m_loadJob;
};

AuthService::AuthService()
{
	m_session = paf::common::SharedPtr<AuthSession>(new AuthSession());
}

AuthService::~AuthService()
{
}

void AuthService::EnqueueRegistryLoad()
{
	if (m_loadJob.get() != NULL)
		m_loadJob->Cancel();
	m_loadJob = paf::common::SharedPtr<AuthRegistryLoadJob>(
		new AuthRegistryLoadJob(m_session));
	paf::common::SharedPtr<paf::job::JobItem> jobItem(m_loadJob);
	paf::job::JobQueue::default_queue->Enqueue(jobItem);
}

void AuthService::CancelRegistryLoad()
{
	if (m_loadJob.get() != NULL) {
		m_loadJob->Cancel();
		m_loadJob.reset();
	}
}

static paf::common::SharedPtr<AuthService> s_authService;

int authServiceCreate()
{
	if (s_authService.get() == NULL)
		s_authService = paf::common::SharedPtr<AuthService>(new AuthService());
	return 0;
}

int authServiceReloadRegistry(bool forceReload)
{
	if (s_authService.get() != NULL &&
	    (forceReload || s_authService->m_loadJob.get() == NULL)) {
		s_authService->EnqueueRegistryLoad();
	}
	return 0;
}

int authServiceDestroy()
{
	if (s_authService.get() != NULL) {
		s_authService->CancelRegistryLoad();
		s_authService.reset();
	}
	return 0;
}

bool authServiceIsRegistryLoaded()
{
	if (s_authService.get() == NULL || s_authService->m_loadJob.get() == NULL)
		return false;
	return s_authService->m_loadJob->IsCompleted();
}

int authServiceIsPasscodeEnabled(SceInt16 storeKind, bool *outEnabled)
{
	*outEnabled = false;
	if (s_authService.get() != NULL && s_authService->m_loadJob.get() != NULL &&
	    s_authService->m_loadJob->IsCompleted()) {
		return s_authService->m_session->IsPasscodeEnabled(storeKind, outEnabled);
	}
	return 0;
}

int authServiceStart(SceInt16 storeKind, SceInt16 actionKind,
		     AuthResultCallback callback, void *userData,
		     AuthDialogOpenCallback openCallback)
{
	if (s_authService.get() == NULL)
		return AUTH_ERROR_FAILED;
	return s_authService->m_session->Start(storeKind, actionKind, callback, userData,
					       openCallback);
}

void authServiceCancel()
{
	if (s_authService.get() != NULL)
		s_authService->m_session->Cancel();
}

} /* namespace power_manage */
