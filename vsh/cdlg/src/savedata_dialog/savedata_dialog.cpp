/*
 * SceSaveDataDialogPlugin: SaveDataDialog main UI controller implementation.
 */
#include <string.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <appmgr/appmgr.h>
#include <apputil/apputil_internal.h>
#include <std/stdio.h>
#include <std/string.h>
#include <framework.h>
#include <plugin.h>
#include <versatileparam/idparam.h>
#include <common/encoded_string.h>
#include <widget/w_scene.h>
#include <widget/w_progress_bar.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include "../msg_dialog/msg_dialog_util.h"
#include "savedata_dialog_ctrl.h"

using namespace paf;

static SaveDataDialog *s_saveDataDialogInstance = NULL;
static bool s_abortPending = false;

static const SceUInt32 kPageHash = 0xD078B665u;
static const SceUInt32 kHeaderContainerHash = 0xB7EF9BA3u;
static const SceUInt32 kHeaderSaveTemplateHash = 0x0D61D36Eu;
static const SceUInt32 kHeaderLoadTemplateHash = 0x80D71C71u;
static const SceUInt32 kHeaderDeleteTemplateHash = 0xCC4C4A2Eu;
static const SceUInt32 kWidgetButtonAreaHash = 0x4A66A681u;
static const SceUInt32 kWidgetDetailAreaHash = 0x20D4F409u;
static const SceUInt32 kWidgetCancelAreaHash = 0x1B5E2E5Fu;
static const SceUInt32 kWidgetBusyAreaHash = 0x5D6014AAu;
static const SceInt32 kGuiWidgetButtonOk = 12;
static const SceInt32 kGuiWidgetProgressBar = 18;

struct SysMsgNotificationEntry {
	SceInt32 sysMsgType;
	const char *label;
};

static const SysMsgNotificationEntry kSysMsgNotifications[] = {
	{SCE_SAVEDATA_DIALOG_SYSMSG_TYPE_NOSPACE, "NOSPACE"},
	{SCE_SAVEDATA_DIALOG_SYSMSG_TYPE_NOSPACE_CONTINUABLE, "NOSPC_CONTINUABLE"},
	{0, NULL}
};

class SaveDataGuiDialogHandler : public CommonGuiDialogHandler {
public:
	explicit SaveDataGuiDialogHandler(SaveDataDialog *owner)
		: m_owner(owner)
	{
	}

	virtual ~SaveDataGuiDialogHandler()
	{
	}

	virtual void operator()(SceInt32 value, SceInt32 button)
	{
		if (m_owner != NULL)
			m_owner->HandleGuiDialogButton(value, button);
	}

private:
	SaveDataDialog *m_owner;
};

SaveDataDialog::SaveDataDialog(const SceSaveDataDialogParam *param,
	SaveDataDialogResultCallback callback, void *userData)
{
	InitCommonState(callback, userData);
	m_isPsp = SCE_FALSE;
	if (param != NULL) {
		MsgDialogCheckSdkVersion(param->sdkVersion);
		UpdateVitaParam(param, callback, userData);
	}
}

SaveDataDialog::SaveDataDialog(const ScePspSaveDataDialogParam *param,
	SaveDataDialogResultCallback callback, void *userData)
{
	InitCommonState(callback, userData);
	m_isPsp = SCE_TRUE;
	SceAppUtil_C3CB128B(202, m_pspMountRoot);
	if (param != NULL)
		UpdatePspParam(param, callback, userData);
}

SaveDataDialog::~SaveDataDialog()
{
	CloseGuiDialog();
	PluginJobQueue::CancelAllJobs();
	if (m_listView != NULL) {
		delete m_listView;
		m_listView = NULL;
	}
	if (g_saveDataDialogPlugin != NULL && m_pageWidget != NULL) {
		IDParam pageId(kPageHash);
		Plugin::PageCloseParam closeParam;
		g_saveDataDialogPlugin->PageClose(pageId, closeParam);
		m_pageWidget = NULL;
		m_rootWidget = NULL;
	}
}

void SaveDataDialog::InitCommonState(SaveDataDialogResultCallback callback, void *userData)
{
	m_callback = callback;
	m_callbackUserData = userData;
	m_isPsp = SCE_FALSE;
	m_mode = SCE_SAVEDATA_DIALOG_MODE_INVALID;
	m_dispType = SCE_SAVEDATA_DIALOG_TYPE_SAVE;
	sce_paf_memset(&m_mountPoint, 0, sizeof(m_mountPoint));
	sce_paf_strncpy((char *)m_mountPoint.data, "savedata0:", sizeof(m_mountPoint.data) - 1);
	sce_paf_memset(m_pspMountRoot, 0, sizeof(m_pspMountRoot));
	m_pageWidget = NULL;
	m_rootWidget = NULL;
	m_listView = NULL;
	m_guiDialogId = -1;
	m_progressRate = 0;
	m_headerInitialized = false;
	m_isClosing = false;
	m_selectedSlot.slotId = SCE_APPUTIL_SAVEDATA_SLOT_ID_INVALID;
	m_selectedSlot.status = 0;
	m_selectedSlot.userParam = 0;
	m_selectedSlot.sizeKiB = 0;
	m_selectedSlot.existState = 0;
	sce_paf_memset(&m_selectedSlot.modifiedTime, 0, sizeof(m_selectedSlot.modifiedTime));
}

void SaveDataDialog::OpenDialogPage(SceSaveDataDialogType dispType)
{
	m_dispType = dispType;
	if (m_pageWidget != NULL || g_saveDataDialogPlugin == NULL)
		return;

	IDParam pageId(kPageHash);
	Plugin::PageOpenParam openParam;
	openParam.option = Plugin::PageOption_Create;
	m_pageWidget = g_saveDataDialogPlugin->PageOpen(pageId, openParam);
	m_rootWidget = m_pageWidget;
	SetupHeaderTemplate();
	if (m_pageWidget != NULL)
		m_listView = new SaveDataListView(g_saveDataDialogPlugin, m_pageWidget);
}

void SaveDataDialog::SetupHeaderTemplate(void)
{
	if (m_headerInitialized || m_rootWidget == NULL || g_saveDataDialogPlugin == NULL)
		return;

	SceUInt32 templateHash = kHeaderSaveTemplateHash;
	if (MsgDialogGetSdkLevel() < 3) {
		if (m_dispType == SCE_SAVEDATA_DIALOG_TYPE_LOAD)
			templateHash = kHeaderLoadTemplateHash;
		else if (m_dispType == SCE_SAVEDATA_DIALOG_TYPE_DELETE)
			templateHash = kHeaderDeleteTemplateHash;
	}

	ui::Widget *headerArea = m_rootWidget->FindChild(IDParam(kHeaderContainerHash), 0);
	if (headerArea != NULL) {
		Plugin::TemplateOpenParam openParam;
		g_saveDataDialogPlugin->TemplateOpen(headerArea, IDParam(templateHash), openParam);
		m_headerInitialized = true;
	}
}

static void ShowWidgetImmediate(ui::Widget *widget)
{
	if (widget == NULL)
		return;
	if (!widget->m_transition_complete) {
		widget->m_transition = 0xff;
		widget->m_transition_complete = true;
		widget->DoTransition_core(0.0f, true);
	}
	ScePafWidget_FF5BC957(widget, NULL, 2, 0.0f, NULL);
}

static void HideWidgetImmediate(ui::Widget *widget)
{
	if (widget == NULL)
		return;
	widget->m_transition_complete = false;
	widget->m_transition = 0xff;
	widget->DoTransitionReverse_core(0.0f, true);
	ScePafWidget_4588699E(widget, NULL, 2, 0.0f, NULL);
}

void SaveDataDialog::UpdateViewState(SceInt32 viewState)
{
	if (m_rootWidget == NULL)
		return;

	ui::Widget *buttonArea = m_rootWidget->FindChild(IDParam(kWidgetButtonAreaHash), 0);
	ui::Widget *detailArea = m_rootWidget->FindChild(IDParam(kWidgetDetailAreaHash), 0);
	ui::Widget *cancelArea = m_rootWidget->FindChild(IDParam(kWidgetCancelAreaHash), 0);
	ui::Widget *headerArea = m_rootWidget->FindChild(IDParam(kHeaderContainerHash), 0);
	ui::Widget *busyArea = m_rootWidget->FindChild(IDParam(kWidgetBusyAreaHash), 0);

	if (m_listView != NULL)
		m_listView->Hide();
	HideWidgetImmediate(busyArea);

	if (viewState == 1) {
		if (m_listView != NULL)
			m_listView->Show();
		ShowWidgetImmediate(headerArea);
		HideWidgetImmediate(buttonArea);
		HideWidgetImmediate(detailArea);
		ShowWidgetImmediate(cancelArea);
	} else if (viewState == 2) {
		HideWidgetImmediate(headerArea);
		ShowWidgetImmediate(buttonArea);
		ShowWidgetImmediate(detailArea);
		ShowWidgetImmediate(cancelArea);
	} else {
		HideWidgetImmediate(headerArea);
		HideWidgetImmediate(buttonArea);
		HideWidgetImmediate(detailArea);
		HideWidgetImmediate(cancelArea);
	}
}

void SaveDataDialog::CloseGuiDialog(void)
{
	if (m_guiDialogId != -1) {
		SceCommonGuiDialog_BC311DC0(m_guiDialogId);
		m_guiDialogId = -1;
	}
}

SceInt32 SaveDataDialog::UpdateVitaParam(const SceSaveDataDialogParam *param,
	SaveDataDialogResultCallback callback, void *userData)
{
	m_callback = callback;
	m_callbackUserData = userData;
	m_mode = param->mode;
	m_dispType = param->dispType;

	if (param->slotConfParam != NULL && param->slotConfParam->mountPoint != NULL)
		m_mountPoint = *param->slotConfParam->mountPoint;

	OpenDialogPage(param->dispType);
	CloseGuiDialog();

	if (param->mode == SCE_SAVEDATA_DIALOG_MODE_LIST && param->listParam != NULL) {
		m_slotList = common::SharedPtr<SaveDataSlotList>(new SaveDataSlotList());
		m_slotList->Reserve(param->listParam->slotListSize);
		for (SceUInt32 idx = 0; idx < param->listParam->slotListSize; ++idx)
			m_slotList->AddVitaSlot(param->listParam->slotList[idx]);
		if (m_listView != NULL)
			m_listView->BindSlotList(m_slotList, OnListSelectStatic, this);
		UpdateViewState(1);

		Job_GetSaveDataList *job = new Job_GetSaveDataList(
			m_slotList, &m_mountPoint, OnVitaListLoaded, this,
			PluginJobQueue::Instance());
		if (job != NULL)
			job->Enqueue();
	} else if (param->mode == SCE_SAVEDATA_DIALOG_MODE_FIXED && param->fixedParam != NULL) {
		UpdateViewState(2);
		Job_GetSaveDataInfo *job = new Job_GetSaveDataInfo(
			param->fixedParam->targetSlot.id, &m_mountPoint,
			param->fixedParam->targetSlot.emptyParam,
			OnVitaInfoLoaded, this, PluginJobQueue::Instance());
		if (job != NULL)
			job->Enqueue();
	}
	return SCE_OK;
}

SceInt32 SaveDataDialog::UpdatePspParam(const ScePspSaveDataDialogParam *param,
	SaveDataDialogResultCallback callback, void *userData)
{
	m_callback = callback;
	m_callbackUserData = userData;
	m_mode = (SceSaveDataDialogMode)param->mode;
	m_dispType = (SceSaveDataDialogType)param->dispType;

	OpenDialogPage(m_dispType);
	CloseGuiDialog();

	if (param->mode == SCE_SAVEDATA_DIALOG_MODE_LIST && param->listParam != NULL) {
		m_slotList = common::SharedPtr<SaveDataSlotList>(new SaveDataSlotList());
		m_slotList->Reserve(param->listParam->dirNameListSize);
		for (SceUInt32 idx = 0; idx < param->listParam->dirNameListSize; ++idx)
			m_slotList->AddPspDirName(
				(const char *)param->listParam->targetDirNameList[idx].data);
		if (m_listView != NULL)
			m_listView->BindSlotList(m_slotList, OnListSelectStatic, this);
		UpdateViewState(1);

		Job_GetSaveDataPspInfoList *job = new Job_GetSaveDataPspInfoList(
			m_slotList, m_pspMountRoot, OnPspListLoaded, this,
			PluginJobQueue::Instance());
		if (job != NULL)
			job->Enqueue();
	} else if (param->mode == SCE_SAVEDATA_DIALOG_MODE_FIXED && param->fixedParam != NULL) {
		UpdateViewState(2);
		Job_GetSaveDataPspInfo *job = new Job_GetSaveDataPspInfo(
			(const char *)param->fixedParam->targetDirName.data, m_pspMountRoot, 0xFFu,
			OnPspInfoLoaded, this, PluginJobQueue::Instance());
		if (job != NULL)
			job->Enqueue();
	}
	return SCE_OK;
}

bool SaveDataDialog::IsReadyForMode(SceSaveDataDialogMode mode) const
{
	if (m_pageWidget == NULL)
		return false;
	if (mode == SCE_SAVEDATA_DIALOG_MODE_FIXED)
		return (m_rootWidget != NULL);
	return (mode >= SCE_SAVEDATA_DIALOG_MODE_LIST &&
		mode <= SCE_SAVEDATA_DIALOG_MODE_PROGRESS_BAR);
}

SceInt32 SaveDataDialog::ShowUserMessage(const wstring &message, SceUInt32 buttonType)
{
	CloseGuiDialog();

	CommonGuiLayoutDialogParam dlgParam;
	dlgParam.text = message;
	if ((buttonType & SCE_SAVEDATA_DIALOG_BUTTON_TYPE_OK) != 0)
		dlgParam.layout = SceCommonGuiDialog_AF75CA6D;
	else if ((buttonType & SCE_SAVEDATA_DIALOG_BUTTON_TYPE_YESNO) != 0)
		dlgParam.layout = SceCommonGuiDialog_6CD802FC;
	else
		dlgParam.layout = SceCommonGuiDialog_7AEA192C;
	dlgParam.handler = new SaveDataGuiDialogHandler(this);
	m_guiDialogId = SceCommonGuiDialog_CCFCD9A6(&dlgParam);
	return SCE_OK;
}

SceInt32 SaveDataDialog::FormatSystemMessage(SceSaveDataDialogSystemMessageType sysMsgType,
	SceUInt32 value, wstring *outText, SceInt32 *outSoundId)
{
	if (outSoundId != NULL)
		*outSoundId = 0;
	if (outText == NULL)
		return SAVEDATA_DIALOG_ERROR_INTERNAL;

	if (sysMsgType == SCE_SAVEDATA_DIALOG_SYSMSG_TYPE_NOSPACE ||
	    sysMsgType == SCE_SAVEDATA_DIALOG_SYSMSG_TYPE_NOSPACE_CONTINUABLE) {
		SceUInt32 requiredKiB = value;
		if (requiredKiB == 0) {
			SceInt32 cached = sceAppMgrSaveDataGetCachedRequiredSizeKiB();
			if (cached > 0)
				requiredKiB = (SceUInt32)cached;
		}
		if (requiredKiB == 0)
			requiredKiB = 2048;
		wstring sizeStr;
		MsgDialogFormatSize(&sizeStr, requiredKiB);
		*outText = MsgDialogFormat(L"%1", sizeStr.c_str());
	} else {
		outText->clear();
	}
	return SCE_OK;
}

SceInt32 SaveDataDialog::FormatProgressMessage(SceSaveDataDialogSystemMessageType sysMsgType,
	const SceChar8 *userMsg, wstring *outText)
{
	if (outText == NULL)
		return SAVEDATA_DIALOG_ERROR_INTERNAL;
	if (sysMsgType == 0 && userMsg != NULL) {
		common::EncodedString encoded;
		ScePafMisc_C11CB5E4(&encoded, (const char *)userMsg,
			common::EncodedString::Encoding_Utf8);
		const wchar_t *wide = ScePafMisc_53948F03(&encoded);
		if (wide != NULL)
			*outText = wide;
		ScePafMisc_B45A7521(&encoded);
	} else {
		FormatSystemMessage(sysMsgType, 0, outText, NULL);
	}
	return SCE_OK;
}

SceInt32 SaveDataDialog::ShowSystemMessage(SceSaveDataDialogSystemMessageType sysMsgType,
	SceUInt32 value)
{
	if (sysMsgType == SCE_SAVEDATA_DIALOG_SYSMSG_TYPE_NOSPACE ||
	    sysMsgType == SCE_SAVEDATA_DIALOG_SYSMSG_TYPE_NOSPACE_CONTINUABLE) {
		if (!SaveDataIsMediaAvailable("savedata0:")) {
			ShowErrorCode(SAVEDATA_DIALOG_ERROR_NO_MEMORY_CARD);
			return SCE_OK;
		}
	}

	wstring msgText;
	SceInt32 soundId = 0;
	SceInt32 ret = FormatSystemMessage(sysMsgType, value, &msgText, &soundId);
	if (ret < 0)
		return ret;

	CloseGuiDialog();
	CommonGuiLayoutDialogParam dlgParam;
	dlgParam.text = msgText;
	if (sysMsgType == SCE_SAVEDATA_DIALOG_SYSMSG_TYPE_NOSPACE_CONTINUABLE ||
	    sysMsgType == SCE_SAVEDATA_DIALOG_SYSMSG_TYPE_OVERWRITE)
		dlgParam.layout = SceCommonGuiDialog_6CD802FC;
	else if (sysMsgType == SCE_SAVEDATA_DIALOG_SYSMSG_TYPE_NODATA ||
		 sysMsgType == SCE_SAVEDATA_DIALOG_SYSMSG_TYPE_FILE_CORRUPTED ||
		 sysMsgType == SCE_SAVEDATA_DIALOG_SYSMSG_TYPE_NOSPACE)
		dlgParam.layout = SceCommonGuiDialog_AF75CA6D;
	else
		dlgParam.layout = SceCommonGuiDialog_F8B2D7E1;
	dlgParam.handler = new SaveDataGuiDialogHandler(this);
	m_guiDialogId = SceCommonGuiDialog_CCFCD9A6(&dlgParam);

	if (soundId != 0 && Framework::Instance() != NULL)
		Framework::Instance()->PlayCommonSE(soundId, 1.0f, false);

	if (sysMsgType == SCE_SAVEDATA_DIALOG_SYSMSG_TYPE_NOSPACE_CONTINUABLE &&
	    m_guiDialogId != -1) {
		ui::Widget *btn = SceCommonGuiDialog_8947FC23(m_guiDialogId, kGuiWidgetButtonOk);
		HideWidgetImmediate(btn);
	}

	SendSystemMessageNotification(sysMsgType);
	return SCE_OK;
}

SceInt32 SaveDataDialog::ShowErrorCode(SceInt32 errorCode)
{
	CloseGuiDialog();
	CommonGuiMessageDialogParam dlgParam;
	dlgParam.type = errorCode;
	dlgParam.handler = new SaveDataGuiDialogHandler(this);
	m_guiDialogId = SceCommonGuiDialog_20B141F1(&dlgParam);
	SendErrorCodeNotification(errorCode);
	return SCE_OK;
}

SceInt32 SaveDataDialog::ShowProgressBar(const wstring &message, SceUInt32 barType)
{
	(void)barType;
	CloseGuiDialog();
	m_progressRate = 0;

	CommonGuiLayoutDialogParam dlgParam;
	dlgParam.text = message;
	dlgParam.layout = SceCommonGuiDialog_A0E0A437;
	dlgParam.handler = new SaveDataGuiDialogHandler(this);
	m_guiDialogId = SceCommonGuiDialog_CCFCD9A6(&dlgParam);
	return SCE_OK;
}

void SaveDataDialog::IncrementProgressBar(SceSaveDataDialogProgressBarTarget target,
	SceUInt32 delta)
{
	(void)target;
	SceUInt32 nextRate = m_progressRate + delta;
	if (nextRate > 100)
		nextRate = 100;
	SetProgressBarValue(target, nextRate);
}

void SaveDataDialog::SetProgressBarValue(SceSaveDataDialogProgressBarTarget target,
	SceUInt32 rate)
{
	(void)target;
	if (rate > 100)
		rate = 100;
	m_progressRate = rate;
	if (m_guiDialogId == -1)
		return;
	ui::Widget *barWidget = SceCommonGuiDialog_8947FC23(m_guiDialogId, kGuiWidgetProgressBar);
	if (barWidget != NULL) {
		ui::ProgressBar *bar = (ui::ProgressBar *)barWidget;
		bar->SetValue((float)rate);
	}
}

void SaveDataDialog::OnSlotUpdated(SceAppUtilSaveDataSlotId slotId)
{
	if (m_isPsp || m_slotList.get() == NULL)
		return;
	Job_GetSaveDataSlotParam *job = new Job_GetSaveDataSlotParam(
		slotId, &m_mountPoint, OnVitaInfoLoaded, this, PluginJobQueue::Instance());
	if (job != NULL)
		job->Enqueue();
}

void SaveDataDialog::BeginClose(bool isAborted)
{
	if (isAborted)
		s_abortPending = true;
	m_isClosing = true;
	CloseGuiDialog();
	NotifyResult(s_abortPending ? SCE_COMMON_DIALOG_RESULT_ABORTED : SCE_COMMON_DIALOG_RESULT_OK,
		s_abortPending ? 3 : 0);
}

void SaveDataDialog::SendSystemMessageNotification(
	SceSaveDataDialogSystemMessageType sysMsgType)
{
	for (SceSize idx = 0; kSysMsgNotifications[idx].sysMsgType != 0; ++idx) {
		if (kSysMsgNotifications[idx].sysMsgType == (SceInt32)sysMsgType) {
			AppmgrNotificationRequest req;
			sce_paf_memset(&req, 0, sizeof(req));
			sce_paf_snprintf((char *)req.payload, sizeof(req.payload),
				"SaveDataDialog:\nSystem Message -\n%s",
				kSysMsgNotifications[idx].label);
			sceAppMgrSendNotificationRequest(&req, 1);
			break;
		}
	}
}

void SaveDataDialog::SendErrorCodeNotification(SceInt32 errorCode)
{
	AppmgrNotificationRequest req;
	sce_paf_memset(&req, 0, sizeof(req));
	sce_paf_snprintf((char *)req.payload, sizeof(req.payload),
		"SaveDataDialog:\nError Code - 0x%x", (unsigned int)errorCode);
	sceAppMgrSendNotificationRequest(&req, 1);
}

void SaveDataDialog::NotifyResult(SceInt32 result, SceInt32 buttonAction)
{
	if (m_callback != NULL)
		m_callback(result, m_mode, buttonAction, &m_selectedSlot, m_callbackUserData);
}

void SaveDataDialog::HandleGuiDialogButton(SceInt32 dialogValue, SceInt32 button)
{
	(void)dialogValue;
	m_guiDialogId = -1;
	NotifyResult(SCE_COMMON_DIALOG_RESULT_OK, button);
}

void SaveDataDialog::HandleListSelection(SceSize selectedIndex)
{
	if (m_slotList.get() == NULL)
		return;
	const SaveDataListEntry *entry = m_slotList->GetEntry(selectedIndex);
	if (entry == NULL)
		return;
	m_selectedSlot.slotId = entry->slotId;
	m_selectedSlot.title = entry->title;
	m_selectedSlot.subTitle = entry->subTitle;
	m_selectedSlot.detail = entry->detail;
	m_selectedSlot.dirName = entry->dirName;
	m_selectedSlot.iconPath = entry->iconPath;
	m_selectedSlot.status = entry->status;
	m_selectedSlot.userParam = entry->userParam;
	m_selectedSlot.modifiedTime = entry->modifiedTime;
	m_selectedSlot.sizeKiB = entry->sizeKiB;
	m_selectedSlot.existState = entry->existState;
	NotifyResult(SCE_COMMON_DIALOG_RESULT_OK, 2);
}

void SaveDataDialog::OnVitaInfoLoaded(SceInt32 result,
	const SaveDataDialogSlotResultInfo &slotInfo, void *userData)
{
	SaveDataDialog *self = (SaveDataDialog *)userData;
	if (self == NULL || result != SCE_OK)
		return;
	self->m_selectedSlot = slotInfo;
}

void SaveDataDialog::OnVitaListLoaded(SceInt32 result,
	common::SharedPtr<SaveDataSlotList> list, void *userData)
{
	(void)list;
	SaveDataDialog *self = (SaveDataDialog *)userData;
	if (self == NULL || result != SCE_OK)
		return;
	if (self->m_listView != NULL)
		self->m_listView->Refresh();
}

void SaveDataDialog::OnPspInfoLoaded(SceInt32 result,
	const SaveDataDialogSlotResultInfo &slotInfo, void *userData)
{
	SaveDataDialog *self = (SaveDataDialog *)userData;
	if (self == NULL || result != SCE_OK)
		return;
	self->m_selectedSlot = slotInfo;
}

void SaveDataDialog::OnPspListLoaded(SceInt32 result,
	common::SharedPtr<SaveDataSlotList> list, void *userData)
{
	(void)list;
	SaveDataDialog *self = (SaveDataDialog *)userData;
	if (self == NULL || result != SCE_OK)
		return;
	if (self->m_listView != NULL)
		self->m_listView->Refresh();
}

void SaveDataDialog::OnListSelectStatic(SceSize selectedIndex, void *userData)
{
	SaveDataDialog *self = (SaveDataDialog *)userData;
	if (self != NULL)
		self->HandleListSelection(selectedIndex);
}

SceInt32 OpenVitaSaveDataDialog(const SceSaveDataDialogParam *param,
	SaveDataDialogResultCallback callback, void *userData)
{
	SceInt32 ret = SCE_OK;
	if (s_saveDataDialogInstance != NULL) {
		ret = s_saveDataDialogInstance->UpdateVitaParam(param, callback, userData);
	} else {
		s_saveDataDialogInstance = new SaveDataDialog(param, callback, userData);
	}
	if (s_saveDataDialogInstance == NULL ||
	    !s_saveDataDialogInstance->IsReadyForMode(param->mode))
		return SAVEDATA_DIALOG_ERROR_NOT_READY;

	switch (param->mode) {
	case SCE_SAVEDATA_DIALOG_MODE_LIST:
	case SCE_SAVEDATA_DIALOG_MODE_FIXED:
		return SCE_OK;
	case SCE_SAVEDATA_DIALOG_MODE_USER_MSG: {
		wstring msgText;
		if (param->userMsgParam != NULL && param->userMsgParam->msg != NULL) {
			common::EncodedString encoded;
			ScePafMisc_C11CB5E4(&encoded, (const char *)param->userMsgParam->msg,
				common::EncodedString::Encoding_Utf8);
			const wchar_t *wide = ScePafMisc_53948F03(&encoded);
			if (wide != NULL)
				msgText = wide;
			ScePafMisc_B45A7521(&encoded);
		}
		return s_saveDataDialogInstance->ShowUserMessage(msgText,
			param->userMsgParam->buttonType);
	}
	case SCE_SAVEDATA_DIALOG_MODE_SYSTEM_MSG:
		return s_saveDataDialogInstance->ShowSystemMessage(
			param->sysMsgParam->sysMsgType, param->sysMsgParam->value);
	case SCE_SAVEDATA_DIALOG_MODE_ERROR_CODE:
		return s_saveDataDialogInstance->ShowErrorCode(param->errorCodeParam->errorCode);
	case SCE_SAVEDATA_DIALOG_MODE_PROGRESS_BAR: {
		wstring msgText;
		s_saveDataDialogInstance->FormatProgressMessage(
			param->progBarParam->sysMsgParam.sysMsgType,
			param->progBarParam->msg, &msgText);
		return s_saveDataDialogInstance->ShowProgressBar(msgText,
			param->progBarParam->barType);
	}
	default:
		return SAVEDATA_DIALOG_ERROR_INTERNAL;
	}
}

SceInt32 OpenPspSaveDataDialog(const ScePspSaveDataDialogParam *param,
	SaveDataDialogResultCallback callback, void *userData)
{
	if (s_saveDataDialogInstance == NULL) {
		s_saveDataDialogInstance = new SaveDataDialog(param, callback, userData);
		if (s_saveDataDialogInstance == NULL)
			return SAVEDATA_DIALOG_ERROR_NOT_READY;
	} else {
		SceInt32 ret = s_saveDataDialogInstance->UpdatePspParam(param, callback, userData);
		if (ret != SCE_OK)
			return ret;
	}

	SceSaveDataDialogMode mode = (SceSaveDataDialogMode)param->mode;
	if (!s_saveDataDialogInstance->IsReadyForMode(mode))
		return SAVEDATA_DIALOG_ERROR_NOT_READY;

	switch (mode) {
	case SCE_SAVEDATA_DIALOG_MODE_LIST:
	case SCE_SAVEDATA_DIALOG_MODE_FIXED:
		return SCE_OK;
	case SCE_SAVEDATA_DIALOG_MODE_USER_MSG: {
		wstring msgText;
		if (param->userMsgParam != NULL && param->userMsgParam->msg != NULL) {
			common::EncodedString encoded;
			ScePafMisc_C11CB5E4(&encoded, (const char *)param->userMsgParam->msg,
				common::EncodedString::Encoding_Utf8);
			const wchar_t *wide = ScePafMisc_53948F03(&encoded);
			if (wide != NULL)
				msgText = wide;
			ScePafMisc_B45A7521(&encoded);
		}
		return s_saveDataDialogInstance->ShowUserMessage(msgText,
			param->userMsgParam->buttonType);
	}
	case SCE_SAVEDATA_DIALOG_MODE_SYSTEM_MSG:
		return s_saveDataDialogInstance->ShowSystemMessage(
			(SceSaveDataDialogSystemMessageType)param->sysMsgParam->sysMsgType,
			param->sysMsgParam->value);
	case SCE_SAVEDATA_DIALOG_MODE_ERROR_CODE:
		return s_saveDataDialogInstance->ShowErrorCode(param->errorCodeParam->errorCode);
	case SCE_SAVEDATA_DIALOG_MODE_PROGRESS_BAR: {
		wstring msgText;
		s_saveDataDialogInstance->FormatProgressMessage(
			(SceSaveDataDialogSystemMessageType)param->progBarParam->sysMsgParam.sysMsgType,
			param->progBarParam->msg, &msgText);
		return s_saveDataDialogInstance->ShowProgressBar(msgText,
			param->progBarParam->barType);
	}
	default:
		return SAVEDATA_DIALOG_ERROR_INTERNAL;
	}
}

SceInt32 CloseSaveDataDialog(bool isAborted)
{
	if (s_saveDataDialogInstance != NULL)
		s_saveDataDialogInstance->BeginClose(isAborted);
	return SCE_OK;
}

SceInt32 DestroySaveDataDialog(void)
{
	if (s_saveDataDialogInstance != NULL) {
		delete s_saveDataDialogInstance;
		s_saveDataDialogInstance = NULL;
	}
	s_abortPending = false;
	return SCE_OK;
}

SaveDataDialog *GetSaveDataDialogInstance(void)
{
	return s_saveDataDialogInstance;
}
