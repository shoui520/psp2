/*
 * tel_general_logic_gui.cpp: general telephony UI observer and system dialog
 * manager (COM firmware update LSDB notice, error dialogs, SIM/overheat/MM
 * reject dialogs, and operator app launch)
 * (3.65-CEX: 0x8124EE24..0x8124F5D7, 0x81254AC0..0x81255773).
 */
#include <std/new>
#include <std/string.h>
#include <scetypes.h>
#include <plugin.h>
#include <appmgr/appmgr.h>
#include <widget/w_button.h>
#include <common/string_util.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include "plugins.h"
#include "shell_util_server.h"
#include "src/livespace_db/livespace_db_internal.h"
#include "../tel_internal.h"

extern "C" {
int SceShellUtil_636544FB(int param);
}

namespace telephony {

enum TelGeneralGuiResourceHash {
	TEL_GUI_COM_UPDATE_MSG_HASH = 0x28EA97D0,	/* 686461776 */
	TEL_GUI_ERROR_HEADER_HASH = 0x1A5F3D79,		/* 442434281 */
	TEL_GUI_SIM_ERROR_MSG_HASH = 0x4313D568,
	TEL_GUI_SIM_REMOVED_MSG_HASH = 0x66BE45EC,
	TEL_GUI_SIM_PIN_MSG_HASH = 0x12690FE6,
	TEL_GUI_OVERHEAT_MSG_HASH = 0xFD3F5273,
	TEL_GUI_BTN_SETTINGS_HASH = 0x6F043729,		/* 1862487849 */
	TEL_GUI_BTN_CANCEL_HASH = 0xADCD32FF		/* -1379034401 */
};

static TelGeneralLogicDialogManager *s_dialogManagerInstance = NULL;

/* Button press handler forwarding back into TelGeneralLogicDialogManager (0x8142CF70) */
class TelGeneralDialogButtonHandler : public CommonGuiDialogHandler {
public:
	explicit TelGeneralDialogButtonHandler(TelGeneralLogicDialogManager *manager)
		: m_manager(manager)
	{
	}
	virtual ~TelGeneralDialogButtonHandler() {}

	virtual void operator()(SceInt32 dialogId, SceInt32 buttonId)
	{
		if (m_manager != NULL)
			m_manager->OnDialogButtonPressed(dialogId, buttonId);
	}

private:
	TelGeneralLogicDialogManager *m_manager;
};

/* Periodic callback used by MM reject dialogs to auto-dismiss via manager (0x8142CFCC) */
class TelGeneralDialogTimeoutHandler : public CommonGuiLayoutDialogOpenHandler {
public:
	explicit TelGeneralDialogTimeoutHandler(TelGeneralLogicDialogManager *manager)
		: m_manager(manager)
	{
	}
	virtual ~TelGeneralDialogTimeoutHandler() {}

	virtual void operator()(SceInt32 dialogId)
	{
		if (m_manager != NULL)
			m_manager->OnDialogButtonPressed(dialogId, 0);
	}

private:
	TelGeneralLogicDialogManager *m_manager;
};

/* Open handler installing the timeout update callback on MM reject dialogs (0x8142CF80) */
class TelGeneralDialogOpenTimeoutInstaller : public CommonGuiLayoutDialogOpenHandler {
public:
	explicit TelGeneralDialogOpenTimeoutInstaller(TelGeneralLogicDialogManager *manager)
		: m_manager(manager)
	{
	}
	virtual ~TelGeneralDialogOpenTimeoutInstaller() {}

	virtual void operator()(SceInt32 dialogId)
	{
		void *mem = telMalloc(sizeof(TelGeneralDialogTimeoutHandler));
		CommonGuiLayoutDialogOpenHandler *timeoutHandler = (mem != NULL)
			? new (mem) TelGeneralDialogTimeoutHandler(s_dialogManagerInstance) : NULL;
		SceCommonGuiDialog_17091699(dialogId, timeoutHandler, 10000.0f);
	}

private:
	TelGeneralLogicDialogManager *m_manager;
};

typedef void (*TelDialogSetupFunc)(CommonGuiLayoutDialogParam *param);
typedef void (*TelDialogPostOpenFunc)(int dialogId);

struct TelDialogDescriptor {
	int dialogType;
	int exclusiveGroup;
	SceUInt32 messageHash;
	int dialogLevel;
	TelDialogSetupFunc setupFunc;
	TelDialogPostOpenFunc postOpenFunc;
};

static CommonGuiDialogHandler *telAllocDialogButtonHandler(void)
{
	void *mem = telMalloc(sizeof(TelGeneralDialogButtonHandler));
	return (mem != NULL)
		? new (mem) TelGeneralDialogButtonHandler(s_dialogManagerInstance) : NULL;
}

static CommonGuiLayoutDialogOpenHandler *telAllocTimeoutInstaller(void)
{
	void *mem = telMalloc(sizeof(TelGeneralDialogOpenTimeoutInstaller));
	return (mem != NULL)
		? new (mem) TelGeneralDialogOpenTimeoutInstaller(s_dialogManagerInstance) : NULL;
}

/* 0x81254FC8: standard OK dialog */
static void telSetupSimpleOkDialog(CommonGuiLayoutDialogParam *param)
{
	paf::Plugin *telPlugin = paf::Plugin::Find("telephony_plugin");
	param->resourcePlugin = telPlugin;
	param->dimmer = 1;
	param->handler = telAllocDialogButtonHandler();
	param->layout = SceCommonGuiDialog_AF75CA6D;
}

/* 0x81255092: two-button confirmation dialog (SIM PIN / operator app prompt) */
static void telSetupTwoButtonDialog(CommonGuiLayoutDialogParam *param)
{
	paf::Plugin *telPlugin = paf::Plugin::Find("telephony_plugin");
	param->resourcePlugin = telPlugin;
	param->dimmer = 1;
	param->handler = telAllocDialogButtonHandler();
	param->layout = SceCommonGuiDialog_22B17F2B;
}

/* 0x8125515C: MM reject #2 dialog */
static void telSetupMmRejectNotProvisionedDialog(CommonGuiLayoutDialogParam *param)
{
	paf::Plugin *telPlugin = paf::Plugin::Find("telephony_plugin");
	param->resourcePlugin = telPlugin;
	param->dimmer = 1;
	param->handler = telAllocDialogButtonHandler();
	param->openHandler = telAllocTimeoutInstaller();
	param->layout = SceCommonGuiDialog_AF75CA6D;
	param->text = L"SIM not provisioned MM#2";
}

/* 0x81255274: MM reject #3 dialog */
static void telSetupMmRejectSimNotAllowedDialog(CommonGuiLayoutDialogParam *param)
{
	paf::Plugin *telPlugin = paf::Plugin::Find("telephony_plugin");
	param->resourcePlugin = telPlugin;
	param->dimmer = 1;
	param->handler = telAllocDialogButtonHandler();
	param->openHandler = telAllocTimeoutInstaller();
	param->layout = SceCommonGuiDialog_AF75CA6D;
	param->text = L"SIM not allowed MM#3";
}

/* 0x8125538C: MM reject #6 dialog */
static void telSetupMmRejectPhoneNotAllowedDialog(CommonGuiLayoutDialogParam *param)
{
	paf::Plugin *telPlugin = paf::Plugin::Find("telephony_plugin");
	param->resourcePlugin = telPlugin;
	param->dimmer = 1;
	param->handler = telAllocDialogButtonHandler();
	param->openHandler = telAllocTimeoutInstaller();
	param->layout = SceCommonGuiDialog_AF75CA6D;
	param->text = L"Phone not allowed MM#6";
}

/* 0x812554A4: alert OK dialog (SIM removed / overheat) */
static void telSetupAlertOkDialog(CommonGuiLayoutDialogParam *param)
{
	paf::Plugin *telPlugin = paf::Plugin::Find("telephony_plugin");
	param->resourcePlugin = telPlugin;
	param->dimmer = 1;
	param->handler = telAllocDialogButtonHandler();
	param->layout = SceCommonGuiDialog_B04C8C1A;
}

/* 0x8125556E: customize labels of buttons 12 and 13 on the two-button dialog */
static void telCustomizeTwoButtonLabels(int dialogId)
{
	paf::Plugin *telPlugin = paf::Plugin::Find("telephony_plugin");
	if (telPlugin == NULL)
		return;

	paf::ui::Widget *leftBtn = SceCommonGuiDialog_8947FC23(dialogId, 12);
	if (leftBtn != NULL && leftBtn->IsInherit(paf::ui::Button::TypeName())) {
		paf::IDParam labelId(TEL_GUI_BTN_SETTINGS_HASH);
		const wchar_t *labelText = telPlugin->GetString(labelId);
		paf::wstring wideLabel(labelText != NULL ? labelText : L"");
		leftBtn->SetString(wideLabel);
	}

	paf::ui::Widget *rightBtn = SceCommonGuiDialog_8947FC23(dialogId, 13);
	if (rightBtn != NULL && rightBtn->IsInherit(paf::ui::Button::TypeName())) {
		paf::IDParam labelId(TEL_GUI_BTN_CANCEL_HASH);
		const wchar_t *labelText = telPlugin->GetString(labelId);
		paf::wstring wideLabel(labelText != NULL ? labelText : L"");
		rightBtn->SetString(wideLabel);
	}
}

/* 0x81515980: 7 general dialog descriptors */
static const TelDialogDescriptor s_dialogTable[TEL_DIALOG_COUNT] = {
	{ TEL_DIALOG_SIM_ERROR,                   0, TEL_GUI_SIM_ERROR_MSG_HASH,   2,
	  telSetupSimpleOkDialog,                NULL },
	{ TEL_DIALOG_SIM_REMOVED,                 0, TEL_GUI_SIM_REMOVED_MSG_HASH, 2,
	  telSetupAlertOkDialog,                 NULL },
	{ TEL_DIALOG_SIM_PIN_REQUIRED,            1, TEL_GUI_SIM_PIN_MSG_HASH,     1,
	  telSetupTwoButtonDialog,               telCustomizeTwoButtonLabels },
	{ TEL_DIALOG_OVERHEAT,                    2, TEL_GUI_OVERHEAT_MSG_HASH,    2,
	  telSetupAlertOkDialog,                 NULL },
	{ TEL_DIALOG_MM_REJECT_NOT_PROVISIONED,   3, 0,                            1,
	  telSetupMmRejectNotProvisionedDialog,  NULL },
	{ TEL_DIALOG_MM_REJECT_SIM_NOT_ALLOWED,   4, 0,                            1,
	  telSetupMmRejectSimNotAllowedDialog,   NULL },
	{ TEL_DIALOG_MM_REJECT_PHONE_NOT_ALLOWED, 5, 0,                            1,
	  telSetupMmRejectPhoneNotAllowedDialog, NULL }
};

/* ------------------------------------------------------------------
 * TelGeneralLogicDialogManager (0x81254AC0..0x81254FC7)
 * ------------------------------------------------------------------ */

TelGeneralLogicDialogManager::TelGeneralLogicDialogManager()
	: TelObservable("SceTelGenLogicDialogMgr")
{
	s_dialogManagerInstance = this;
}

TelGeneralLogicDialogManager::~TelGeneralLogicDialogManager()
{
	s_dialogManagerInstance = NULL;
	m_activeDialogs.clear();
}

TelGeneralLogicDialogManager *TelGeneralLogicDialogManager::GetInstance()
{
	return s_dialogManagerInstance;
}

/* 0x81254BBA */
TelephonyResult TelGeneralLogicDialogManager::ShowDialog(int dialogType)
{
	TelephonyResult res(TEL_OK);
	if (dialogType < 0 || dialogType >= TEL_DIALOG_COUNT) {
		res.SetCode(TEL_ERR_IO_FAILED);
		return res;
	}

	const TelDialogDescriptor &targetDesc = s_dialogTable[dialogType];

	/* Close any currently open dialog in the same exclusive group */
	for (int idx = 0; idx < TEL_DIALOG_COUNT; ++idx) {
		if (s_dialogTable[idx].exclusiveGroup == targetDesc.exclusiveGroup) {
			paf::map<int, int>::iterator it =
				m_activeDialogs.find(s_dialogTable[idx].dialogType);
			if (it != m_activeDialogs.end()) {
				shellStateWatcherCloseDialog(it->second);
				m_activeDialogs.erase(it);
			}
		}
	}

	CommonGuiLayoutDialogParam dialogParam;
	targetDesc.setupFunc(&dialogParam);

	if (dialogParam.text.empty() && targetDesc.messageHash != 0) {
		paf::Plugin *telPlugin = paf::Plugin::Find("telephony_plugin");
		if (telPlugin != NULL) {
			paf::IDParam msgId(targetDesc.messageHash);
			const wchar_t *msgText = telPlugin->GetString(msgId);
			dialogParam.text = (msgText != NULL) ? msgText : L"";
		}
	}

	int dialogId = shellStateWatcherOpenDialog(&dialogParam, true, targetDesc.dialogLevel);
	if (dialogId < 0) {
		res.SetCode(dialogId);
		return res;
	}

	m_activeDialogs[dialogType] = dialogId;
	res.SetCode(dialogId);
	if (targetDesc.postOpenFunc != NULL)
		targetDesc.postOpenFunc(dialogId);

	return res;
}

/* 0x81254EAC */
void TelGeneralLogicDialogManager::OnDialogButtonPressed(int dialogId, int buttonId)
{
	int matchedType = -1;
	for (paf::map<int, int>::iterator it = m_activeDialogs.begin();
	     it != m_activeDialogs.end(); ++it) {
		if (it->second == dialogId) {
			matchedType = it->first;
			m_activeDialogs.erase(it);
			break;
		}
	}

	if (buttonId == 3 || buttonId == 5) {
		TelEventInfo cancelEv(TEL_GUI_EVENT_DIALOG_CANCELLED, matchedType, dialogId);
		NotifyObservers(cancelEv);
	} else if (buttonId == 2 || buttonId == 4) {
		TelEventInfo acceptEv(TEL_GUI_EVENT_DIALOG_ACCEPTED, matchedType, dialogId);
		NotifyObservers(acceptEv);
	}

	shellStateWatcherCloseDialog(dialogId);
}

/* ------------------------------------------------------------------
 * TelGeneralLogicGui (0x8124EE24..0x8124F5D7)
 * ------------------------------------------------------------------ */

TelGeneralLogicGui::TelGeneralLogicGui(TelGeneralLogicMediatorBase *generalMediator)
	: m_generalMediator(generalMediator), m_simNotReadyShown(false)
{
}

TelGeneralLogicGui::~TelGeneralLogicGui()
{
}

/* 0x8124EEA2 */
TelephonyResult TelGeneralLogicGui::Attach()
{
	m_generalMediator->AddObserver(this);
	m_dialogManager.AddObserver(this);
	return TelephonyResult(TEL_OK);
}

/* 0x8124EEC0 */
TelephonyResult TelGeneralLogicGui::Detach()
{
	m_generalMediator->RemoveObserver(this);
	m_dialogManager.RemoveObserver(this);
	return TelephonyResult(TEL_OK);
}

/* 0x8124EEE0 */
void TelGeneralLogicGui::PostComUpdateNotification()
{
	LsdbNewEvent notice;
	notice.titleId = "com_update_title";
	notice.itemId = "com_update_item";
	notice.rowid = 0;
	notice.newFlag = true;

	paf::Plugin *telPlugin = paf::Plugin::Find("telephony_plugin");
	if (telPlugin != NULL) {
		paf::IDParam msgId(TEL_GUI_COM_UPDATE_MSG_HASH);
		const wchar_t *wideMsg = telPlugin->GetString(msgId);
		paf::wstring wideText(wideMsg != NULL ? wideMsg : L"");
		paf::string utf8Text;
		paf::common::Utf16ToUtf8(wideText, &utf8Text);
		notice.desc = utf8Text;
	}

	notice.execTitleId = "NPXS10016";
	notice.execArg = "updater::com";
	notice.iconPath = "vs0:app/NPXS10015/sce_sys/icon0.png";
	notice.actType = 2;
	notice.notifyFlags = 16;

	SceLsdb_315B9FD6(&notice, true);
}

/* 0x8124F1A0 */
void TelGeneralLogicGui::HandleMmRejectCause(const TelEventInfo &event)
{
	int cause = event.GetEventResult();
	if (cause == 2)
		m_dialogManager.ShowDialog(TEL_DIALOG_MM_REJECT_NOT_PROVISIONED);
	else if (cause == 3)
		m_dialogManager.ShowDialog(TEL_DIALOG_MM_REJECT_SIM_NOT_ALLOWED);
	else if (cause == 6)
		m_dialogManager.ShowDialog(TEL_DIALOG_MM_REJECT_PHONE_NOT_ALLOWED);
}

/* 0x8124F21A */
void TelGeneralLogicGui::HandleDialogAccepted(const TelEventInfo &event)
{
	int dialogType = event.GetEventArg();
	if (dialogType == TEL_DIALOG_SIM_REMOVED) {
		SceShellUtil_636544FB(0);
	} else if (dialogType == TEL_DIALOG_SIM_PIN_REQUIRED) {
		const char *appName = NULL;
		if (m_generalMediator != NULL) {
			TelephonyResult res = m_generalMediator->GetOperatorAppTitleId(&appName);
			if (res.IsSuccess() && appName != NULL)
				sceAppMgrLaunchAppByName(0x20000, appName, NULL);
		}
	}
}

/* 0x8124F2C8 */
void TelGeneralLogicGui::ShowErrorDialog(const TelEventInfo &event)
{
	CommonGuiMessageDialogParam errParam;
	errParam.resourcePlugin = paf::Plugin::Find("telephony_plugin");
	errParam.dimmer = 1;
	errParam.messageId = TEL_GUI_ERROR_HEADER_HASH;
	errParam.closeOnSelect = false;
	errParam.type = event.GetEventArg();
	errParam.buttonCount = 1;
	errParam.defaultButton = 1;
	errParam.modal = true;

	SceCommonGuiDialog_20B141F1(&errParam);
}

/* 0x8124F47A */
void TelGeneralLogicGui::OnNotify(const TelEventInfo &event)
{
	int eventId = event.GetEventId();
	switch (eventId) {
	case TEL_GUI_EVENT_ERROR_DIALOG:
		ShowErrorDialog(event);
		break;
	case TEL_GUI_EVENT_OVERHEAT_DIALOG:
		m_dialogManager.ShowDialog(TEL_DIALOG_OVERHEAT);
		break;
	case TEL_GUI_EVENT_SIM_STATE_RESET:
		m_simNotReadyShown = false;
		break;
	case TEL_GUI_EVENT_MM_REJECT_CAUSE:
		HandleMmRejectCause(event);
		break;
	case TEL_GUI_EVENT_COM_FW_UPDATE:
		PostComUpdateNotification();
		break;
	case TEL_GUI_EVENT_SIM_REMOVED:
		if (event.GetEventArg() == TEL_ERR_SIM_REMOVED)
			m_dialogManager.ShowDialog(TEL_DIALOG_SIM_REMOVED);
		break;
	case TEL_GUI_EVENT_SIM_ERROR:
		if (event.GetEventArg() != TEL_ERR_SIM_INIT_FAILED)
			m_dialogManager.ShowDialog(TEL_DIALOG_SIM_ERROR);
		break;
	case TEL_GUI_EVENT_SIM_NOT_READY:
		if (!m_simNotReadyShown) {
			m_dialogManager.ShowDialog(TEL_DIALOG_SIM_PIN_REQUIRED);
			m_simNotReadyShown = true;
		}
		break;
	case TEL_GUI_EVENT_DIALOG_ACCEPTED:
		HandleDialogAccepted(event);
		break;
	case TEL_GUI_EVENT_DIALOG_CANCELLED:
	default:
		break;
	}
}

} /* namespace telephony */
