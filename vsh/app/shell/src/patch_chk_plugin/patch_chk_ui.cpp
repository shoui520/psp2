/*
 * patch_chk_ui.cpp: manages all CommonGuiDialogs, RichText changelog viewer,
 * BusyIndicator, and overlay page for patch_chk_plugin (0x812AE87C-0x812AFCAA).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string.h>
#include <widget/w_rich_text.h>
#include <widget/w_scroll_view_base.h>
#include "../../../../paf/include/rtc/rtc.h"
#include "../../plugins.h"
#include "src/livespace_db/lsdb_keys.h"
#include "patch_chk_internal.h"

using namespace paf;

/* RCO localized string hashes used by patch_chk_plugin */
#define MSG_CHECKING_FOR_UPDATE		0x632B2E93u
#define MSG_SYSUPDATE_REQUIRED		0xAC71F5F6u
#define MSG_UP_TO_DATE			0xA7CB0218u
#define MSG_DISCONTINUED		0xCFA88B67u
#define MSG_ALREADY_DOWNLOADING		0x9DB88322u
#define MSG_QUIET_UPDATE_TITLE		0x805C6D26u
#define MSG_QUIET_UPDATE_AVAILABLE	0x3B96EA09u
#define MSG_QUIET_DOWNLOADING		0x16C20144u
#define TEX_QUIET_UPDATE_ICON		0x22E3E8CDu
#define MSG_BGDL_TASK_LIMIT		0x45CEE15Cu
#define MSG_BGDL_NO_MEMORY_CARD		0xDC272B0Fu
#define MSG_BGDL_NO_FREE_SPACE		0x14CAF5AFu
#define MSG_BGDL_ALREADY_QUEUED		0x162E9516u
#define MSG_BGDL_GENERIC_ERROR		0xE9FADDFCu
#define MSG_BGDL_NOT_SUPPORTED		0x26FC6666u

/* RichText dialog widget & button resource hashes */
#define WIDGET_RICHTEXT_CONTAINER	0x019BCA5Du
#define WIDGET_RICHTEXT_VIEW		0xD07943E5u
#define WIDGET_BUSY_INDICATOR		0x531E49E7u
#define PAGE_PATCH_CHK_OVERLAY		0xC3440684u
#define WIDGET_PATCH_CHK_ROOT		0x3F07D2ECu

#define BTN_LABEL_CANCEL		0xADCD75AFu
#define BTN_LABEL_UPDATE		0xC7A9CE17u
#define BTN_LABEL_DOWNLOAD		0xA3C8F777u
#define BTN_STYLE_DEFAULT		0x20413274u
#define BTN_STYLE_CANCEL		0xC17D7980u

/* Dialog handler: closes error dialog and notifies CONFIRM (0x812AFC3A) */
class PatchChkErrorDialogHandler : public CommonGuiDialogHandler {
public:
	explicit PatchChkErrorDialogHandler(PatchChkUi *ui) : m_ui(ui) {}
	virtual ~PatchChkErrorDialogHandler() {}

	virtual void operator()(SceInt32 value, SceInt32 button)
	{
		(void)value;
		(void)button;
		if (m_ui != NULL) {
			m_ui->CloseErrorDialog();
			m_ui->NotifyEvent(PATCH_CHK_UI_EVENT_CONFIRM);
		}
	}

private:
	PatchChkUi *m_ui;
};

/* Dialog handler: button 4 -> CONFIRM, anything else -> CANCEL (0x812AFC5E) */
class PatchChkYesNoDialogHandler : public CommonGuiDialogHandler {
public:
	explicit PatchChkYesNoDialogHandler(PatchChkUi *ui) : m_ui(ui) {}
	virtual ~PatchChkYesNoDialogHandler() {}

	virtual void operator()(SceInt32 value, SceInt32 button)
	{
		(void)value;
		if (m_ui != NULL) {
			if (button == 4)
				m_ui->NotifyEvent(PATCH_CHK_UI_EVENT_CONFIRM);
			else
				m_ui->NotifyEvent(PATCH_CHK_UI_EVENT_CANCEL);
		}
	}

private:
	PatchChkUi *m_ui;
};

/* Dialog handler: always notifies CONFIRM (0x812AFC82) */
class PatchChkOkDialogHandler : public CommonGuiDialogHandler {
public:
	explicit PatchChkOkDialogHandler(PatchChkUi *ui) : m_ui(ui) {}
	virtual ~PatchChkOkDialogHandler() {}

	virtual void operator()(SceInt32 value, SceInt32 button)
	{
		(void)value;
		(void)button;
		if (m_ui != NULL)
			m_ui->NotifyEvent(PATCH_CHK_UI_EVENT_CONFIRM);
	}

private:
	PatchChkUi *m_ui;
};

/* Dialog handler: always notifies CANCEL (0x812AFC96) */
class PatchChkCancelDialogHandler : public CommonGuiDialogHandler {
public:
	explicit PatchChkCancelDialogHandler(PatchChkUi *ui) : m_ui(ui) {}
	virtual ~PatchChkCancelDialogHandler() {}

	virtual void operator()(SceInt32 value, SceInt32 button)
	{
		(void)value;
		(void)button;
		if (m_ui != NULL)
			m_ui->NotifyEvent(PATCH_CHK_UI_EVENT_CANCEL);
	}

private:
	PatchChkUi *m_ui;
};

/* Constructor (0x812AE87C) */
PatchChkUi::PatchChkUi()
	: m_eventCallback(NULL),
	  m_callbackUserData(NULL),
	  m_reservedState(0),
	  m_activeDialogId(0),
	  m_errorDialogId(0),
	  m_busyIndicator(NULL),
	  m_overlayRoot(NULL)
{
}

/* Destructor (0x812AE890) */
PatchChkUi::~PatchChkUi()
{
	if (m_busyIndicator != NULL)
		m_busyIndicator->Stop();
	m_eventCallback = NULL;
}

/* Sets UI event notification callback (0x812AF918) */
void PatchChkUi::SetEventCallback(UiEventCallback callback, void *userData)
{
	m_eventCallback = callback;
	m_callbackUserData = userData;
}

void PatchChkUi::NotifyEvent(int eventId)
{
	if (m_eventCallback != NULL)
		m_eventCallback(eventId, m_callbackUserData);
}

/* Closes the active layout dialog if open (0x812AF91E) */
void PatchChkUi::CloseCurrentDialog()
{
	if (m_activeDialogId >= 0) {
		patchChkCloseDialog(m_activeDialogId);
		m_activeDialogId = -1;
	}
}

void PatchChkUi::CloseErrorDialog()
{
	if (m_errorDialogId >= 0) {
		patchChkCloseDialog(m_errorDialogId);
		m_errorDialogId = -1;
	}
}

/* Dialog open callback forwarding PATCH_CHK_UI_EVENT_OPENED (0x812AFB7A) */
int PatchChkUi::OnCheckingDialogOpened(SceInt32 dialogId, void *userData)
{
	(void)dialogId;
	PatchChkUi *ui = (PatchChkUi *)userData;
	if (ui != NULL)
		ui->NotifyEvent(PATCH_CHK_UI_EVENT_OPENED);
	return 0;
}

/* RichText layout-ready listener: reveals RichText and stops BusyIndicator (0x812AFB8C) */
void PatchChkUi::OnRichTextLayoutReady(SceInt32 eventType, ui::Handler *handler,
				       ui::Event *event, void *userData)
{
	(void)eventType;
	(void)event;
	PatchChkUi *ui = (PatchChkUi *)userData;
	ui::Widget *widget = (ui::Widget *)handler;
	if (widget != NULL) {
		widget->Show(0xff, new Timer(0.0f), NULL, 0.0f);
		if (ui->m_busyIndicator != NULL)
			ui->m_busyIndicator->Stop();
	}
}

/* Shows "Checking for update..." cancelable dialog (0x812AE8A8) */
void PatchChkUi::ShowCheckingDialog()
{
	IDParam msgId(MSG_CHECKING_FOR_UPDATE);
	m_activeDialogId = patchChkShowCancelDialog(msgId, new PatchChkCancelDialogHandler(this));
	patchChkSetDialogOpenCallback(m_activeDialogId, OnCheckingDialogOpened, this);
}

/* Shows system update required Yes/No prompt (0x812AE95C) */
void PatchChkUi::ShowSysUpdatePromptDialog()
{
	IDParam msgId(MSG_SYSUPDATE_REQUIRED);
	m_activeDialogId = patchChkShowYesNoDialog(msgId, new PatchChkYesNoDialogHandler(this));
}

/* Shows "The latest version is installed" OK dialog (0x812AE9F6) */
void PatchChkUi::ShowUpToDateDialog()
{
	IDParam msgId(MSG_UP_TO_DATE);
	m_activeDialogId = patchChkShowOkDialog(msgId, new PatchChkOkDialogHandler(this));
}

/* Shows the scrollable RichText update changelog & Download/Update dialog (0x812AEA90) */
int PatchChkUi::ShowUpdateAvailableDialog(const string &richTextHtml, bool hasExistingDownload)
{
	static const CommonGuiDialogPage richTextPage = {
		{ 0x3A918C2Cu, 0x438C7285u, 0xB505C20Au }
	};
	static const CommonGuiDialogButton buttonsUpdate[3] = {
		{ 12, BTN_LABEL_CANCEL, 5, BTN_STYLE_CANCEL },
		{ 13, BTN_LABEL_UPDATE, 4, BTN_STYLE_DEFAULT },
		{ 0, 0, 0, 0 }
	};
	static const CommonGuiDialogButton buttonsDownload[3] = {
		{ 12, BTN_LABEL_CANCEL, 5, BTN_STYLE_CANCEL },
		{ 13, BTN_LABEL_DOWNLOAD, 4, BTN_STYLE_DEFAULT },
		{ 0, 0, 0, 0 }
	};

	CommonGuiDialogLayout layout;
	layout.page = &richTextPage;
	layout.parts = SceCommonGuiDialog_22B17F2B.parts;
	layout.buttons = hasExistingDownload ? buttonsUpdate : buttonsDownload;

	wstring emptyText;
	m_activeDialogId =
		patchChkShowLayoutDialog(emptyText, &layout, new PatchChkYesNoDialogHandler(this));
	if (m_activeDialogId < 0)
		return m_activeDialogId;

	ui::Widget *contentRoot = patchChkGetDialogPart(m_activeDialogId, 4);
	if (contentRoot != NULL) {
		IDParam containerId(WIDGET_RICHTEXT_CONTAINER);
		ui::Widget *container = contentRoot->FindChild(containerId, 0);
		if (container != NULL) {
			math::v4 clearColor(0.0f, 0.0f, 0.0f, 0.0f);
			container->SetColor(clearColor, 0, 0x10001, 0, 0, 0);
		}

		IDParam richTextId(WIDGET_RICHTEXT_VIEW);
		ui::RichText *richText = (ui::RichText *)contentRoot->FindChild(richTextId, 0);
		if (richText != NULL) {
			math::v4 clearColor(0.0f, 0.0f, 0.0f, 0.0f);
			richText->SetColor(clearColor, 0, 0x10001, 0, 0, 0);
			richText->Hide(0.0f, 0.0f);
			math::v4 zeroMargin(0.0f, 0.0f, 0.0f, 0.0f);
			richText->SetVerticalBarMargin(zeroMargin);
			richText->SetColor(clearColor, 0, 0x10001, 0, 0, 0);
			richText->SetCText(richTextHtml.c_str(), richTextHtml.length());
			richText->AddEventListener(
				0x10000008,
				new ui::EventCBListener(OnRichTextLayoutReady, this), false);
		}

		IDParam busyId(WIDGET_BUSY_INDICATOR);
		ui::Widget *busyWidget = contentRoot->FindChild(busyId, 0);
		if (busyWidget != NULL && busyWidget->IsInherit(ui::BusyIndicator::TypeName()))
			m_busyIndicator = (ui::BusyIndicator *)busyWidget;
		else
			m_busyIndicator = NULL;

		if (m_busyIndicator != NULL)
			m_busyIndicator->Start();
	}
	return 0;
}

/* Shows quiet-mode icon dialog for available update (0x812AEF26) */
void PatchChkUi::ShowQuietUpdateIconDialog()
{
	IDParam titleId(MSG_QUIET_UPDATE_TITLE);
	IDParam textId(MSG_QUIET_UPDATE_AVAILABLE);
	IDParam iconId(TEX_QUIET_UPDATE_ICON);
	IDParam emptyBadge("");
	m_activeDialogId = patchChkShowIconDialog(
		titleId, textId, iconId, emptyBadge, new PatchChkOkDialogHandler(this));
}

/* Shows debug Patch Test dialog (0x812AF0E8) */
void PatchChkUi::ShowDebugPatchTestDialog()
{
	static const wchar_t debugPatchTestMsg[] = {
		L'[', 0x2605, L'D', L'e', L'b', L'u', L'g', L' ', L'S', L'e', L't', L't', L'i',
		L'n', L'g', L's', L']', L'-', L'>', L'[', L'P', L'l', L'a', L'y', L'S', L't',
		L'a', L't', L'i', L'o', L'n', L'(', L'r', L')', L'N', L'e', L't', L'w', L'o',
		L'r', L'k', L']', L'-', L'>', L'[', L'P', L'a', L't', L'c', L'h', L' ', L'T',
		L'e', L's', L't', L']', L' ', L'i', L's', L' ', L'[', L'O', L'n', L']', L'\0'
	};
	wstring msg(debugPatchTestMsg);
	m_activeDialogId = patchChkShowLayoutDialog(
		msg, &SceCommonGuiDialog_AF75CA6D, new PatchChkYesNoDialogHandler(this));
}

/* Shows discontinued title service OK dialog (0x812AF170) */
void PatchChkUi::ShowDiscontinuedDialog()
{
	IDParam msgId(MSG_DISCONTINUED);
	m_activeDialogId = patchChkShowOkDialog(msgId, new PatchChkOkDialogHandler(this));
}

/* Shows already-downloading OK dialog (0x812AF20A) */
void PatchChkUi::ShowAlreadyDownloadingDialog()
{
	IDParam msgId(MSG_ALREADY_DOWNLOADING);
	m_activeDialogId = patchChkShowOkDialog(msgId, new PatchChkOkDialogHandler(this));
}

/* Shows quiet-mode icon dialog for active download (0x812AF2A4) */
void PatchChkUi::ShowQuietDownloadingIconDialog()
{
	IDParam titleId(MSG_QUIET_UPDATE_TITLE);
	IDParam textId(MSG_QUIET_DOWNLOADING);
	IDParam iconId(TEX_QUIET_UPDATE_ICON);
	IDParam emptyBadge("");
	m_activeDialogId = patchChkShowIconDialog(
		titleId, textId, iconId, emptyBadge, new PatchChkOkDialogHandler(this));
}

/* Shows a Yes/No prompt dialog for the given message hash (0x812AF466) */
void PatchChkUi::ShowYesNoPromptDialog(const IDParam &textId)
{
	m_activeDialogId = patchChkShowYesNoDialog(textId, new PatchChkYesNoDialogHandler(this));
}

/* Shows BGDL max-tasks dialog (0x812AF494) */
void PatchChkUi::ShowBgdlTaskLimitDialog()
{
	IDParam msgId(MSG_BGDL_TASK_LIMIT);
	m_activeDialogId = patchChkShowOkDialog(msgId, new PatchChkOkDialogHandler(this));
}

/* Shows BGDL missing memory card dialog (0x812AF52E) */
void PatchChkUi::ShowBgdlNoMemoryCardDialog()
{
	IDParam msgId(MSG_BGDL_NO_MEMORY_CARD);
	m_activeDialogId = patchChkShowOkDialog(msgId, new PatchChkOkDialogHandler(this));
}

/* Shows BGDL insufficient free space dialog with formatted size (0x812AF5C8) */
void PatchChkUi::ShowBgdlNoFreeSpaceDialog(SceUInt64 requiredBytes)
{
	wstring sizeText;
	shellSizeText(&sizeText, requiredBytes);

	IDParam msgId(MSG_BGDL_NO_FREE_SPACE);
	const wchar_t *fmt = patchChkGetString(msgId);
	wstring formatted = shellFormatText(fmt, sizeText.c_str());

	m_activeDialogId = patchChkShowLayoutDialog(
		formatted, &SceCommonGuiDialog_AF75CA6D, new PatchChkOkDialogHandler(this));
}

/* Shows BGDL already-queued dialog (0x812AF712) */
void PatchChkUi::ShowBgdlAlreadyQueuedDialog()
{
	IDParam msgId(MSG_BGDL_ALREADY_QUEUED);
	m_activeDialogId = patchChkShowOkDialog(msgId, new PatchChkOkDialogHandler(this));
}

/* Shows BGDL generic download error dialog (0x812AF7AC) */
void PatchChkUi::ShowBgdlGenericErrorDialog()
{
	IDParam msgId(MSG_BGDL_GENERIC_ERROR);
	m_activeDialogId = patchChkShowOkDialog(msgId, new PatchChkOkDialogHandler(this));
}

/* Shows BGDL unsupported content dialog (0x812AF846) */
void PatchChkUi::ShowBgdlNotSupportedDialog()
{
	IDParam msgId(MSG_BGDL_NOT_SUPPORTED);
	m_activeDialogId = patchChkShowOkDialog(msgId, new PatchChkOkDialogHandler(this));
}

/* Shows a CommonGuiMessageDialog error box (0x812AF8E0) */
void PatchChkUi::ShowErrorCodeDialog(const IDParam &messageId, int errorCode)
{
	m_errorDialogId = patchChkShowErrorDialog(
		messageId, errorCode, new PatchChkErrorDialogHandler(this), NULL);
}

/* Opens the quiet-mode patch_chk_plugin overlay page (0x812AF934) */
void PatchChkUi::OpenOverlayPage()
{
	Plugin *plugin = Plugin::Find("patch_chk_plugin");
	if (plugin == NULL)
		return;

	Plugin::PageOpenParam openParam;
	openParam.overwrite_draw_priority = 80;
	IDParam pageId(PAGE_PATCH_CHK_OVERLAY);
	ui::Widget *page = (ui::Widget *)plugin->PageOpen(pageId, openParam);
	if (page != NULL) {
		IDParam rootId(WIDGET_PATCH_CHK_ROOT);
		m_overlayRoot = page->FindChild(rootId, 0);
	}
}

/* Closes the quiet-mode overlay page if open (0x812AFA4E) */
void PatchChkUi::CloseOverlayPage()
{
	Plugin *plugin = Plugin::Find("patch_chk_plugin");
	if (plugin == NULL)
		return;

	IDParam pageId(PAGE_PATCH_CHK_OVERLAY);
	if (plugin->PageRoot(pageId) != NULL) {
		Plugin::PageCloseParam closeParam;
		plugin->PageClose(pageId, closeParam);
	}
	m_overlayRoot = NULL;
}

/* Returns part 4 (content container) of the currently open layout dialog (0x812AFB62) */
ui::Widget *PatchChkUi::GetDialogContentWidget() const
{
	if (m_activeDialogId >= 0)
		return patchChkGetDialogPart(m_activeDialogId, 4);
	return NULL;
}
