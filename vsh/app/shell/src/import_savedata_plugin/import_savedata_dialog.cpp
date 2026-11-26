/*
 * import_savedata_dialog.cpp: CommonGuiDialog presentation, target selection
 * ListView item factory, and dialog handlers for import_savedata_plugin
 * (0x813968FC..0x8139913D).
 */
#include "import_savedata_internal.h"

#include <appmgr/appmgr.h>
#include <kernel/iofilemgr.h>
#include <regmgr/regmgr.h>
#include <rtc/rtc.h>
#include <common/char_encoding.h>
#include <std/stdio.h>
#include <std/string.h>
#include <widget/w_box.h>
#include <widget/w_list_item.h>
#include <widget/w_list_view.h>
#include <widget/w_text.h>

extern "C" {
int sceAppMgrInstallDirMount(int mountId, const char *path, const char *key, char *mountPoint);
int sceAppMgrUmount(const char *mountPoint);
int sceRtcConvertUtcToLocalTime(const SceRtcTick *utc, SceRtcTick *localTime);
int sceRtcSetTick(SceDateTime *dt, const SceRtcTick *tick);
const char *sceLsdbGetTitle(const LsdbAppInfoPtr *app);
}

/* Resource ID hashes for import_savedata_plugin.rco */
static const SceUInt32 kIdIntroHeadingWidget = 0xD0794AA5u;		/* -797370523 */
static const SceUInt32 kIdIntroHeadingText = 0x9CA0317Bu;		/* -1667280229 */
static const SceUInt32 kIdIntroBodyWidget = 0x66EA46AFu;		/* 1726642735 */
static const SceUInt32 kIdIntroNoteWidget = 0x54EDE5CBu;		/* 1424845259 */
static const SceUInt32 kIdIntroNoteText = 0xD9D6A9DAu;			/* -640234278 */
static const SceUInt32 kIdTargetListView = 0xD635B1EFu;			/* -701117073 */
static const SceUInt32 kIdTargetCancelBtn = 0x504B0FA4u;		/* 1347024356 */
static const SceUInt32 kIdTargetImportBtn = 0xBD562FB7u;		/* -1118463337 */
static const SceUInt32 kIdMsgNoTarget = 0x621330AAu;			/* 1645368394 */
static const SceUInt32 kIdMsgNoMemoryCard = 0x9A7EDB47u;		/* -1702942393 */
static const SceUInt32 kIdMsgCloseRunningApp = 0xDBD9C290u;		/* -606497072 */
static const SceUInt32 kIdTemplateAppCard = 0x0DF27CE2u;		/* 233988610 */
static const SceUInt32 kIdAppCardBox = 0x3F521DB5u;			/* 1062353013 */
static const SceUInt32 kIdAppCardIcon = 0x08CBE64Bu;			/* 147575179 */
static const SceUInt32 kIdAppCardTitle = 0x6D5477CEu;			/* 1834253806 */
static const SceUInt32 kIdMsgOverwriteConfirm = 0xE09AD6FEu;		/* -526710274 */
static const SceUInt32 kIdTemplateOverwriteCard = 0x1E5840E9u;		/* 509107785 */
static const SceUInt32 kIdOverwriteDateWidget = 0x64C4C5C4u;		/* 1690598980 */
static const SceUInt32 kIdOverwriteDatePrefix = 0x11B4AE0Bu;		/* 297054923 */
static const SceUInt32 kIdMsgInsufficientSpace = 0x3BABF33Cu;		/* 1001124668 */
static const SceUInt32 kIdMsgCopying = 0xF5D590D9u;			/* -170554615 */
static const SceUInt32 kIdMsgComplete = 0xBEAC9269u;			/* -1095971799 */
static const SceUInt32 kIdMsgCardRemoved = 0x96F3CD16u;			/* -1762389994 */
static const SceUInt32 kIdMsgInfoNotFound = 0xF4275D0Du;		/* -198752243 */
static const SceUInt32 kIdEmptyMessage = 0xC51B20E4u;			/* -988109660 */
static const SceUInt32 kIdTemplateHeaderItem = 0xBC7C30C7u;		/* -1132718585 */
static const SceUInt32 kIdListHeaderItem = 0x3F0050EEu;			/* 1057009070 */
static const SceUInt32 kIdListHeaderBox = 0x9AA0261Eu;			/* -1700759842 */
static const SceUInt32 kIdListHeaderText = 0x8B130B72u;			/* -1961753790 */
static const SceUInt32 kIdListHeaderMsg = 0xA32D490Cu;			/* -1557271348 */
static const SceUInt32 kIdTemplateTargetItem = 0x6718805Bu;		/* 1729675675 */
static const SceUInt32 kIdListTargetItem = 0x3F7EB3CFu;			/* 1065275215 */
static const SceUInt32 kIdListTargetInner = 0x10C22E24u;		/* 281169828 */
static const SceUInt32 kIdListTargetIcon = 0x654FBC7Cu;			/* 1699705404 */

static inline void hideWidgetImmediate(paf::ui::Widget *widget)
{
	widget->Hide(0xFF, NULL, NULL, 0.0f);
}

static inline void showWidgetImmediate(paf::ui::Widget *widget)
{
	widget->Show(0xFF, NULL, NULL, 0.0f);
}

namespace {

/* Single-button dialog handler forwarding CONFIRM on any press (0x815401D0) */
class ImportSavedataConfirmHandler : public CommonGuiDialogHandler {
public:
	explicit ImportSavedataConfirmHandler(ImportSaveDataDialog *dialog)
		: m_dialog(dialog) {}
	virtual ~ImportSavedataConfirmHandler() {}
	virtual void operator()(SceInt32 value, SceInt32 button)
	{
		(void)value;
		(void)button;
		if (m_dialog != NULL)
			m_dialog->NotifyEvent(IMPORT_SAVEDATA_UI_CONFIRM);
	}

private:
	ImportSaveDataDialog *m_dialog;
};

/* Cancel/OK dialog handler: button 2 -> CONFIRM, else CANCEL (0x8139870A, 0x815401E4) */
class ImportSavedataCancelOkHandler : public CommonGuiDialogHandler {
public:
	explicit ImportSavedataCancelOkHandler(ImportSaveDataDialog *dialog)
		: m_dialog(dialog) {}
	virtual ~ImportSavedataCancelOkHandler() {}
	virtual void operator()(SceInt32 value, SceInt32 button)
	{
		(void)value;
		if (m_dialog != NULL) {
			SceInt32 ev = (button == 2) ? IMPORT_SAVEDATA_UI_CONFIRM
						    : IMPORT_SAVEDATA_UI_CANCEL;
			m_dialog->NotifyEvent(ev);
		}
	}

private:
	ImportSaveDataDialog *m_dialog;
};

/* Yes/No dialog handler: button 4 -> CONFIRM, else CANCEL (0x815401F8) */
class ImportSavedataYesNoHandler : public CommonGuiDialogHandler {
public:
	explicit ImportSavedataYesNoHandler(ImportSaveDataDialog *dialog)
		: m_dialog(dialog) {}
	virtual ~ImportSavedataYesNoHandler() {}
	virtual void operator()(SceInt32 value, SceInt32 button)
	{
		(void)value;
		if (m_dialog != NULL) {
			SceInt32 ev = (button == 4) ? IMPORT_SAVEDATA_UI_CONFIRM
						    : IMPORT_SAVEDATA_UI_CANCEL;
			m_dialog->NotifyEvent(ev);
		}
	}

private:
	ImportSaveDataDialog *m_dialog;
};

/* Dialog open callback forwarding OPENED (0x812AFB7A equivalent) */
static SceInt32 onCopyingDialogOpened(SceInt32 dialogId, void *userData)
{
	(void)dialogId;
	ImportSaveDataDialog *dialog = static_cast<ImportSaveDataDialog *>(userData);
	if (dialog != NULL)
		dialog->NotifyEvent(IMPORT_SAVEDATA_UI_OPENED);
	return 0;
}

/* Callback fired when an async icon load finishes (0x813990F2) */
static void onTargetIconLoaded(SceInt32 cellIndex, SceInt32 result, void *userData)
{
	(void)result;
	if (userData != NULL) {
		ImportSaveDataJobManager *jobMgr = ImportSaveDataJobManager::GetInstance();
		if (jobMgr != NULL)
			jobMgr->RemoveJob(cellIndex, false);
	}
}

/* ListView ItemFactory populating the savedata target selection list (0x8139872E, 0x813990DA) */
class ImportSavedataListItemFactory : public paf::ui::listview::ItemFactory {
public:
	ImportSavedataListItemFactory(ImportSaveDataDialog *dialog,
				      const paf::common::SharedPtr<LsdbAppInfo> &appInfo,
				      const ImportSavedataTargetMap &targets)
		: m_dialog(dialog), m_appInfo(appInfo), m_targets(targets) {}

	virtual ~ImportSavedataListItemFactory() {}

	/* 0x8139872E */
	virtual paf::ui::ListItem *Create(CreateParam &param)
	{
		paf::Plugin *plugin = paf::Plugin::Find("import_savedata_plugin");
		if (plugin == NULL)
			return NULL;

		if (param.m_cell == 0) {
			paf::IDParam tplId(kIdTemplateHeaderItem);
			paf::Plugin::TemplateOpenParam openParam;
			plugin->TemplateOpen(param.m_parent, tplId, openParam);

			paf::IDParam itemId(kIdListHeaderItem);
			paf::ui::Widget *found = param.m_parent->FindChild(itemId, 0);
			paf::ui::ListItem *item = NULL;
			if (found != NULL && found->IsInherit(paf::ui::ListItem::TypeName()))
				item = static_cast<paf::ui::ListItem *>(found);
			if (item == NULL)
				return NULL;

			item->m_name = paf::IDParam("");

			paf::IDParam boxId(kIdListHeaderBox);
			if (item->FindChild(boxId, 0) != NULL) {
				paf::IDParam textId(kIdListHeaderText);
				paf::ui::Widget *textWidget = item->FindChild(textId, 0);
				if (textWidget != NULL) {
					paf::IDParam msgId(kIdListHeaderMsg);
					const wchar_t *msg = importSavedataGetString(msgId);
					paf::wstring wideText(msg != NULL ? msg : L"");
					textWidget->SetString(wideText);
				}
			}
			return item;
		}

		if (param.m_cell < 0)
			return NULL;

		ImportSavedataTargetMap::iterator it = m_targets.begin();
		SceInt32 idx = 1;
		while (idx < param.m_cell && it != m_targets.end()) {
			++it;
			++idx;
		}
		if (it == m_targets.end())
			return NULL;

		paf::IDParam tplId(kIdTemplateTargetItem);
		paf::Plugin::TemplateOpenParam openParam;
		plugin->TemplateOpen(param.m_parent, tplId, openParam);

		paf::IDParam itemId(kIdListTargetItem);
		paf::ui::Widget *found = param.m_parent->FindChild(itemId, 0);
		paf::ui::ListItem *item = NULL;
		if (found != NULL && found->IsInherit(paf::ui::ListItem::TypeName()))
			item = static_cast<paf::ui::ListItem *>(found);
		if (item == NULL)
			return NULL;

		item->m_name = paf::IDParam("");

		paf::IDParam innerId(kIdListTargetInner);
		paf::ui::Widget *innerBox = item->FindChild(innerId, 0);
		if (innerBox != NULL) {
			paf::IDParam iconId(kIdListTargetIcon);
			paf::ui::Widget *iconWidget = innerBox->FindChild(iconId, 0);
			if (iconWidget != NULL) {
				hideWidgetImmediate(iconWidget);
				ImportSaveDataJobManager *jobMgr =
					ImportSaveDataJobManager::GetInstance();
				if (jobMgr != NULL) {
					ImportSaveDataLoadIconJobItem *iconJob =
						new ImportSaveDataLoadIconJobItem(
							onTargetIconLoaded, iconWidget,
							importSavedataGetPlugin(), this);
					if (iconJob != NULL) {
						iconJob->SetTarget(param.m_cell,
								   it->second.storageType,
								   it->second.titleId);
						jobMgr->EnqueueJob(param.m_cell, iconJob);
					}
				}
			}

			paf::IDParam titleId(kIdAppCardTitle);
			paf::ui::Widget *titleWidget = innerBox->FindChild(titleId, 0);
			if (titleWidget != NULL) {
				paf::wstring wideTitle;
				paf::common::Utf8ToUtf16(it->second.title, &wideTitle);
				titleWidget->SetString(wideTitle);
			}
		}
		return item;
	}

	/* 0x813990DA */
	virtual void Stop(StopParam &param)
	{
		ImportSaveDataJobManager *jobMgr = ImportSaveDataJobManager::GetInstance();
		if (jobMgr != NULL)
			jobMgr->RemoveJob(param.m_cell, true);
	}

private:
	ImportSaveDataDialog *m_dialog;
	paf::common::SharedPtr<LsdbAppInfo> m_appInfo;
	ImportSavedataTargetMap m_targets;
};

} /* namespace */

/* 0x813968FC */
ImportSaveDataDialog::ImportSaveDataDialog(ImportSavedataDialogCallback eventCb, void *userData)
	: m_eventCallback(eventCb),
	  m_callbackUserData(userData),
	  m_dialogId(-1)
{
}

/* 0x81396928 */
ImportSaveDataDialog::~ImportSaveDataDialog()
{
}

void ImportSaveDataDialog::NotifyEvent(SceInt32 eventCode)
{
	if (m_eventCallback != NULL)
		m_eventCallback(eventCode, m_callbackUserData);
}

/* 0x8139694A */
SceInt32 ImportSaveDataDialog::ShowIntroDialog(const char *titleId)
{
	(void)titleId;
	CloseCurrentDialog();

	static const CommonGuiDialogPage introPage = {
		{ 0x3A918C2Cu, 0x438C7285u, 0xA03CD0E3u }
	};
	CommonGuiDialogLayout layout = SceCommonGuiDialog_AF75CA6D;
	layout.page = &introPage;

	paf::wstring emptyText;
	SceUInt32 soundId = importSavedataResolveSoundId(IMPORT_SAVEDATA_SOUND_NONE);
	m_dialogId = importSavedataShowLayoutDialog(emptyText, soundId, &layout,
						    new ImportSavedataConfirmHandler(this));
	if (m_dialogId < 0)
		return 0;

	paf::ui::Widget *content = SceCommonGuiDialog_8947FC23(m_dialogId, 10);
	if (content != NULL && content->IsInherit(paf::ui::Box::TypeName())) {
		paf::IDParam headingId(kIdIntroHeadingWidget);
		paf::ui::Widget *headingWidget = content->FindChild(headingId, 0);
		if (headingWidget != NULL) {
			paf::IDParam headingTextId(kIdIntroHeadingText);
			const wchar_t *headingStr = importSavedataGetString(headingTextId);
			paf::wstring wideHeading(headingStr != NULL ? headingStr : L"");
			headingWidget->SetString(wideHeading);
		}

		paf::IDParam bodyId(kIdIntroBodyWidget);
		paf::ui::Widget *bodyWidget = content->FindChild(bodyId, 0);
		if (bodyWidget != NULL) {
			paf::wstring wideInfo;
			paf::common::Utf8ToUtf16(m_infoText, &wideInfo);
			bodyWidget->SetString(wideInfo);
		}
	}

	paf::ui::Widget *footer = SceCommonGuiDialog_8947FC23(m_dialogId, 4);
	if (footer != NULL) {
		paf::IDParam noteId(kIdIntroNoteWidget);
		paf::ui::Widget *noteWidget = footer->FindChild(noteId, 0);
		if (noteWidget != NULL && noteWidget->IsInherit(paf::ui::Text::TypeName())) {
			paf::IDParam noteTextId(kIdIntroNoteText);
			const wchar_t *noteStr = importSavedataGetString(noteTextId);
			paf::wstring wideNote(noteStr != NULL ? noteStr : L"");
			noteWidget->SetString(wideNote);
		}
	}
	return 0;
}

/* 0x81396E4C */
SceInt32 ImportSaveDataDialog::ShowTargetListDialog(ImportSavedataTargetMap *targets)
{
	CloseCurrentDialog();

	static const CommonGuiDialogPage listPage = {
		{ 0x3A918C2Cu, 0x438C7285u, 0x0AFB83C8u }
	};
	static const paf::ui::Dialog::PartsList listParts[4] = {
		{ 11, paf::IDParam(kIdTargetListView) },
		{ 12, paf::IDParam(kIdTargetCancelBtn) },
		{ 13, paf::IDParam(kIdTargetImportBtn) },
		{ 0, paf::IDParam(0u) }
	};

	CommonGuiDialogLayout layout;
	layout.page = &listPage;
	layout.parts = listParts;
	layout.buttons = SceCommonGuiDialog_6CD802FC.buttons;

	paf::wstring emptyText;
	SceUInt32 soundId = importSavedataResolveSoundId(IMPORT_SAVEDATA_SOUND_ALERT);
	m_dialogId = importSavedataShowLayoutDialog(emptyText, soundId, &layout,
						    new ImportSavedataYesNoHandler(this));
	if (m_dialogId < 0)
		return IMPORT_SAVEDATA_ERR_GENERAL;

	paf::ui::Widget *root = SceCommonGuiDialog_8947FC23(m_dialogId, 4);
	if (root == NULL)
		return IMPORT_SAVEDATA_ERR_GENERAL;

	paf::IDParam listId(kIdTargetListView);
	paf::ui::ListView *listView = static_cast<paf::ui::ListView *>(root->FindChild(listId, 0));
	if (listView == NULL)
		return IMPORT_SAVEDATA_ERR_GENERAL;

	listView->BlockSync();
	listView->Clear();

	LsdbAppInfoPtr appInfo(new LsdbAppInfo());
	ImportSavedataListItemFactory *factory =
		new ImportSavedataListItemFactory(this, appInfo, *targets);
	listView->SetItemFactory(factory);
	listView->InsertSegment(0, 1);
	listView->SetSegmentLayoutType(0, paf::ui::ListView::LAYOUT_TYPE_LIST_VARIABLE);

	paf::math::v2 viewMargin(30.0f, 30.0f);
	listView->ScePafWidget_5B94F2B4(viewMargin, 0);
	listView->InsertCell(0, 0, targets->size() + 1);

	paf::math::v4 cellSize(0.0f, 128.0f, 0.0f, 0.0f);
	listView->SetCellSize(0, 0, cellSize);
	listView->UnblockSync();

	showWidgetImmediate(listView);
	return 0;
}

/* 0x813972CE */
SceInt32 ImportSaveDataDialog::ShowNoTargetDialog(void)
{
	CloseCurrentDialog();
	paf::IDParam msgId(kIdMsgNoTarget);
	m_dialogId = importSavedataShowOkDialog(msgId, IMPORT_SAVEDATA_SOUND_ALERT,
						new ImportSavedataConfirmHandler(this));
	return 0;
}

/* 0x8139737E */
SceInt32 ImportSaveDataDialog::ShowNoMemoryCardDialog(void)
{
	CloseCurrentDialog();
	paf::IDParam msgId(kIdMsgNoMemoryCard);
	m_dialogId = importSavedataShowOkDialog(msgId, IMPORT_SAVEDATA_SOUND_ALERT,
						new ImportSavedataConfirmHandler(this));
	return 0;
}

/* 0x8139742E */
SceInt32 ImportSaveDataDialog::ShowCloseRunningAppDialog(const char *runningTitleId)
{
	CloseCurrentDialog();
	paf::IDParam msgId(kIdMsgCloseRunningApp);
	m_dialogId = importSavedataShowCancelOkDialog(msgId, IMPORT_SAVEDATA_SOUND_ALERT,
						      new ImportSavedataCancelOkHandler(this));
	if (m_dialogId < 0)
		return IMPORT_SAVEDATA_ERR_GENERAL;

	paf::ui::Widget *content = SceCommonGuiDialog_8947FC23(m_dialogId, 10);
	if (content == NULL || !content->IsInherit(paf::ui::Box::TypeName()))
		return IMPORT_SAVEDATA_ERR_GENERAL;

	paf::ui::Widget *msgWidget = SceCommonGuiDialog_8947FC23(m_dialogId, 1);
	if (msgWidget != NULL && msgWidget->IsInherit(paf::ui::Text::TypeName())) {
		paf::ui::Text *textWidget = static_cast<paf::ui::Text *>(msgWidget);
		textWidget->SetAdjust(0, textWidget->m_adjust_y, textWidget->m_adjust_z);
		textWidget->SetLayoutAttribute(paf::graph::TextLayoutAttribute_TextAlign, 0);
		textWidget->SetLayoutAttribute(paf::graph::TextLayoutAttribute_AlignX, 0);
	}

	paf::IDParam cardTplId(kIdTemplateAppCard);
	importSavedataOpenTemplate(content, cardTplId);

	paf::IDParam cardBoxId(kIdAppCardBox);
	paf::ui::Widget *cardBox = content->FindChild(cardBoxId, 0);
	if (cardBox != NULL) {
		LsdbAppInfoPtr appInfo(new LsdbAppInfo());
		if (importSavedataQueryLsdbAppInfo(runningTitleId, &appInfo) == 0) {
			paf::IDParam iconId(kIdAppCardIcon);
			paf::ui::Widget *iconWidget = cardBox->FindChild(iconId, 0);
			if (iconWidget != NULL) {
				hideWidgetImmediate(iconWidget);
				paf::intrusive_ptr<paf::graph::Surface> iconSurface;
				if (importSavedataLoadLsdbIcon(&iconSurface, &appInfo) == 0) {
					iconWidget->SetTexture(iconSurface);
					showWidgetImmediate(iconWidget);
				}
			}

			paf::IDParam titleId(kIdAppCardTitle);
			paf::ui::Widget *titleWidget = cardBox->FindChild(titleId, 0);
			if (titleWidget != NULL) {
				const char *titleUtf8 = sceLsdbGetTitle(&appInfo);
				if (titleUtf8 != NULL) {
					paf::string utf8Str(titleUtf8);
					paf::wstring wideTitle;
					paf::common::Utf8ToUtf16(utf8Str, &wideTitle);
					titleWidget->SetString(wideTitle);
				}
			}
		}
	}
	return 0;
}

/* 0x81397904 */
SceInt32 ImportSaveDataDialog::ShowOverwriteConfirmDialog(const char *titleId)
{
	(void)titleId;
	CloseCurrentDialog();

	paf::IDParam msgId(kIdMsgOverwriteConfirm);
	m_dialogId = importSavedataShowYesNoDialog(msgId, IMPORT_SAVEDATA_SOUND_ALERT,
						   new ImportSavedataYesNoHandler(this));
	if (m_dialogId < 0)
		return 0;

	paf::ui::Widget *content = SceCommonGuiDialog_8947FC23(m_dialogId, 10);
	if (content == NULL || !content->IsInherit(paf::ui::Box::TypeName()))
		return 0;

	paf::ui::Widget *msgWidget = SceCommonGuiDialog_8947FC23(m_dialogId, 1);
	if (msgWidget != NULL && msgWidget->IsInherit(paf::ui::Text::TypeName())) {
		paf::ui::Text *textWidget = static_cast<paf::ui::Text *>(msgWidget);
		textWidget->SetAdjust(0, textWidget->m_adjust_y, textWidget->m_adjust_z);
		textWidget->SetLayoutAttribute(paf::graph::TextLayoutAttribute_TextAlign, 0);
		textWidget->SetLayoutAttribute(paf::graph::TextLayoutAttribute_AlignX, 0);
	}

	paf::IDParam tplId(kIdTemplateOverwriteCard);
	importSavedataOpenTemplate(content, tplId);

	char mountPoint[16];
	char filePath[512];
	sce_paf_memset(mountPoint, 0, sizeof(mountPoint));
	sce_paf_memset(filePath, 0, sizeof(filePath));

	SceInt32 ret = sceAppMgrInstallDirMount(504, NULL, NULL, mountPoint);
	if (ret != 0)
		return ret;

	paf::IDParam dateWidgetId(kIdOverwriteDateWidget);
	paf::ui::Widget *dateWidget = content->FindChild(dateWidgetId, 0);
	if (dateWidget != NULL && dateWidget->IsInherit(paf::ui::Text::TypeName())) {
		sce_paf_snprintf(filePath, sizeof(filePath), "%s/%s", mountPoint, "meta.dat");
		SceUID fd = sceIoOpen(filePath, 1, 0);
		if (fd < 0) {
			sceAppMgrUmount(mountPoint);
			return fd;
		}

		ImportSavedataMetaHeader meta;
		sce_paf_memset(&meta, 0, sizeof(meta));
		SceSSize bytesRead = sceIoRead(fd, &meta, sizeof(meta));
		SceInt32 closeRet = sceIoClose(fd);
		if (bytesRead < 0) {
			sceAppMgrUmount(mountPoint);
			return bytesRead;
		}
		if (bytesRead != static_cast<SceSSize>(sizeof(meta))) {
			sceAppMgrUmount(mountPoint);
			return IMPORT_SAVEDATA_ERR_GENERAL;
		}
		if (closeRet != 0) {
			sceAppMgrUmount(mountPoint);
			return closeRet;
		}
		if (meta.version != 0) {
			sceAppMgrUmount(mountPoint);
			return IMPORT_SAVEDATA_ERR_BAD_META;
		}

		SceRtcTick localTick;
		SceDateTime localDt;
		sceRtcConvertUtcToLocalTime(&meta.timestamp, &localTick);
		sceRtcSetTick(&localDt, &localTick);

		paf::IDParam prefixId(kIdOverwriteDatePrefix);
		const wchar_t *prefixStr = importSavedataGetString(prefixId);
		paf::wstring dateText(prefixStr != NULL ? prefixStr : L"");
		paf::wstring formattedDate = importSavedataFormatDateTime(&localDt);
		dateText.append(formattedDate.c_str(), formattedDate.size());
		dateWidget->SetString(dateText);
	}

	paf::IDParam cardBoxId(kIdAppCardBox);
	paf::ui::Widget *cardBox = content->FindChild(cardBoxId, 0);
	if (cardBox == NULL)
		return sceAppMgrUmount(mountPoint);

	paf::IDParam titleWidgetId(kIdAppCardTitle);
	paf::ui::Widget *titleWidget = cardBox->FindChild(titleWidgetId, 0);
	if (titleWidget != NULL && titleWidget->IsInherit(paf::ui::Text::TypeName())) {
		sce_paf_memset(filePath, 0, sizeof(filePath));
		sce_paf_snprintf(filePath, sizeof(filePath), "%s/%s", mountPoint, "param.sfo");

		ImportSavedataSfoFile sfoFile;
		ret = importSavedataOpenSfoFile(filePath, &sfoFile);
		if (ret != 0) {
			sceAppMgrUmount(mountPoint);
			return ret;
		}

		SceInt32 languageId = -1;
		if (sceRegMgrGetKeyInt("/CONFIG/SYSTEM", "language", &languageId) != 0)
			languageId = -1;

		const void *titleVal = NULL;
		SceSize titleLen = 0;
		SceSize titleMaxLen = 0;
		SceInt32 lookupRet = importSavedataGetSfoParam(
			&sfoFile, importSavedataGetTitleKeyName(languageId),
			&titleVal, &titleLen, &titleMaxLen);
		if (lookupRet != 0 && languageId != -1) {
			lookupRet = importSavedataGetSfoParam(
				&sfoFile, importSavedataGetTitleKeyName(-1),
				&titleVal, &titleLen, &titleMaxLen);
		}
		if (lookupRet == 0 && titleVal != NULL) {
			paf::string utf8Title(static_cast<const char *>(titleVal));
			paf::wstring wideTitle;
			paf::common::Utf8ToUtf16(utf8Title, &wideTitle);
			titleWidget->SetString(wideTitle);
		}

		ret = importSavedataCloseSfoFile(&sfoFile);
		if (ret != 0) {
			sceAppMgrUmount(mountPoint);
			return ret;
		}
	}

	paf::IDParam iconId(kIdAppCardIcon);
	paf::ui::Widget *iconWidget = cardBox->FindChild(iconId, 0);
	if (iconWidget == NULL)
		return sceAppMgrUmount(mountPoint);

	hideWidgetImmediate(iconWidget);
	sce_paf_memset(filePath, 0, sizeof(filePath));
	sce_paf_snprintf(filePath, sizeof(filePath), "%s/%s", mountPoint, "icon0.png");

	paf::intrusive_ptr<paf::graph::Surface> iconSurface;
	SceInt32 loadRet = importSavedataLoadSurfaceFromFile(filePath, &iconSurface);
	if (loadRet == 0) {
		iconWidget->SetTexture(iconSurface);
		showWidgetImmediate(iconWidget);
		return sceAppMgrUmount(mountPoint);
	}

	showWidgetImmediate(iconWidget);
	sceAppMgrUmount(mountPoint);
	return loadRet;
}

/* 0x8139825C */
SceInt32 ImportSaveDataDialog::ShowInsufficientSpaceDialog(SceInt64 shortageBytes)
{
	CloseCurrentDialog();
	paf::IDParam msgId(kIdMsgInsufficientSpace);
	m_dialogId = importSavedataShowOkSizeDialog(msgId, IMPORT_SAVEDATA_SOUND_ALERT,
						    shortageBytes,
						    new ImportSavedataConfirmHandler(this));
	return 0;
}

/* 0x81398318 */
SceInt32 ImportSaveDataDialog::ShowCopyingDialog(void)
{
	CloseCurrentDialog();
	paf::IDParam msgId(kIdMsgCopying);
	m_dialogId = importSavedataShowWaitingDialog(msgId, new ImportSavedataConfirmHandler(this));
	SceCommonGuiDialog_B1BA8D9E(m_dialogId, onCopyingDialogOpened, this);

	paf::ui::Widget *cancelBtn = SceCommonGuiDialog_8947FC23(m_dialogId, 12);
	if (cancelBtn != NULL)
		hideWidgetImmediate(cancelBtn);
	return 0;
}

/* 0x8139842C */
SceInt32 ImportSaveDataDialog::ShowCompleteDialog(void)
{
	CloseCurrentDialog();
	paf::IDParam msgId(kIdMsgComplete);
	m_dialogId = importSavedataShowOkDialog(msgId, IMPORT_SAVEDATA_SOUND_COMPLETE,
						new ImportSavedataConfirmHandler(this));
	return 0;
}

/* 0x813984DC */
SceInt32 ImportSaveDataDialog::ShowCardRemovedDialog(void)
{
	CloseCurrentDialog();
	paf::IDParam msgId(kIdMsgCardRemoved);
	m_dialogId = importSavedataShowOkDialog(msgId, IMPORT_SAVEDATA_SOUND_ALERT,
						new ImportSavedataConfirmHandler(this));
	return 0;
}

/* 0x8139858C */
SceInt32 ImportSaveDataDialog::ShowInfoNotFoundDialog(void)
{
	CloseCurrentDialog();
	paf::IDParam msgId(kIdMsgInfoNotFound);
	m_dialogId = importSavedataShowOkDialog(msgId, IMPORT_SAVEDATA_SOUND_ALERT,
						new ImportSavedataConfirmHandler(this));
	return 0;
}

/* 0x8139863C */
SceInt32 ImportSaveDataDialog::ShowErrorDialog(SceInt32 errorCode)
{
	CloseCurrentDialog();
	paf::IDParam msgId(kIdEmptyMessage);
	m_dialogId = importSavedataShowErrorDialog(msgId, errorCode,
						   new ImportSavedataConfirmHandler(this), NULL);
	return 0;
}

/* 0x813986F0 */
SceInt32 ImportSaveDataDialog::CloseCurrentDialog(void)
{
	if (m_dialogId != -1) {
		importSavedataCloseDialog(m_dialogId);
		m_dialogId = -1;
	}
	return m_dialogId;
}
