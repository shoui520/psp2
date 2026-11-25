/*
 * online_storage_delete_launcher.cpp: PS Plus online storage savedata deletion
 * prompt, online_storage_plugin async loader, and fallback savedata removal
 * (0x812A13AC-0x812A276F).
 */
#include "topmenu_internal.h"

namespace {

const SceUInt32 MSG_ONLINE_STORAGE_WAIT		= 0x632BDF33u; /* 1663788851 */
const SceUInt32 MSG_ONLINE_STORAGE_DELETE_ERR	= 0x3367A828u; /* 862442792 */
const SceUInt32 MSG_ONLINE_STORAGE_PROMPT	= 0x13961BC6u; /* 328588998 */
const SceUInt32 TEMPLATE_SAVEDATA_INFO_BOX	= 0x4ADBC4BAu; /* 1255902010 */
const SceUInt32 WIDGET_SAVEDATA_CONTAINER	= 0xB88ED3BEu; /* -1198575362 */
const SceUInt32 WIDGET_SAVEDATA_ICON		= 0x08CBCA8Bu; /* 147575179 */
const SceUInt32 WIDGET_SAVEDATA_TITLE		= 0x958DE772u; /* -1785852110 */
const SceUInt32 WIDGET_SAVEDATA_DATE_SIZE	= 0x64C54F44u; /* 1690598980 */

typedef bool (*ApplauncherGetIconFunc)(paf::intrusive_ptr<paf::cache::CacheImageInterface> *outImg,
				       const paf::string *titleId, int optA, int optB);

static bool g_onlineStorageDeleteActive = false;	/* 0x815ACAC8 */
static char g_onlineStorageTitleId[16];			/* 0x815ACAC9 */
static SceInt32 g_onlineStorageWaitDialogId = -1;	/* 0x81545A48 */
static SceInt32 g_onlineStoragePluginResult = -1;	/* 0x81545A4C */
static SceInt32 g_onlineStorageLaunchAppId = -1;	/* 0x81545A50 */
static LsdbAppInfoPtr g_onlineStorageAppInfo;		/* 0x815ACAD8 */
static SceInt32 g_onlineStorageExistingFlags = 0;	/* 0x815ACAE0 */
static SceInt32 g_onlineStorageExtraFlags = 0;		/* 0x815ACAE4 */

static void onOnlineStorageDialogDeleted(paf::ui::Widget *widget, void *userData);
static int showOnlineStorageDeleteResultDialog(const LsdbAppInfoPtr *appInfo, SceInt32 launchAppId,
					       SceInt32 existingFlags, SceInt32 extraFlags,
					       SceInt32 deleteResult);

/* (0x812A18B2) Append "\n(<external error string>)" to a wide dialog string */
static void appendFormattedErrorCode(paf::wstring *text, SceInt32 errorCode)
{
	*text += L"\n(";
	char codeBuf[24];
	sceErrorGetExternalString(codeBuf, errorCode);
	paf::string codeUtf8(codeBuf);
	paf::wstring codeWide;
	paf::common::Utf8ToUtf16(codeUtf8, &codeWide);
	*text += codeWide.c_str();
	*text += L")";
}

/* (0x812A1D5A) */
static void onOnlineStorageDialogDeleted(paf::ui::Widget *widget, void *userData)
{
	(void)widget;
	if (g_topWindow->m_indicatorIf != NULL && g_topWindow->m_indicatorIf->UnlockButton != NULL)
		g_topWindow->m_indicatorIf->UnlockButton();
	SceShell_D4C7D2B7(9, topmenuSpawnOnInstallNoticeEvent, userData);
}

/* (0x812A198A) Lock indicator button and register power-close listener on dialog */
static void registerOnlineStorageDialogHold(SceInt32 dialogId)
{
	paf::ui::Widget *dialogWidget = SceCommonGuiDialog_8947FC23(dialogId, 4);
	if (dialogWidget == NULL)
		return;

	if (g_topWindow->m_indicatorIf != NULL && g_topWindow->m_indicatorIf->LockButton != NULL)
		g_topWindow->m_indicatorIf->LockButton();
	SceShell_7FE4E732(9, topmenuSpawnOnInstallNoticeEvent,
			  reinterpret_cast<void *>(static_cast<intptr_t>(dialogId)));
	dialogWidget->AddDeleteCB(onOnlineStorageDialogDeleted,
				  reinterpret_cast<void *>(static_cast<intptr_t>(dialogId)));
}

/* (0x812A1D4C) Notify callback receiving online_storage_plugin operation result */
static void onOnlineStorageNotify(int result)
{
	g_onlineStoragePluginResult = result;
}

/* (0x812A1C40) Unload callback finishing online_storage_plugin delete flow */
static void onOnlineStorageUnloaded(void *userData)
{
	(void)userData;
	g_onlineStorageDeleteActive = false;
	topmenuEndBusy();
	if (g_onlineStorageWaitDialogId != -1)
		SceCommonGuiDialog_BC311DC0(g_onlineStorageWaitDialogId);

	showOnlineStorageDeleteResultDialog(&g_onlineStorageAppInfo, g_onlineStorageLaunchAppId,
					    g_onlineStorageExistingFlags, g_onlineStorageExtraFlags,
					    g_onlineStoragePluginResult);

	g_onlineStorageAppInfo.reset();
	g_onlineStorageLaunchAppId = -1;
	g_onlineStoragePluginResult = -1;
	g_onlineStorageExistingFlags = 0;
	g_onlineStorageExtraFlags = 0;
}

/* (0x812A13AC) Load online_storage_plugin asynchronously in delete mode (operate_mode=1) */
static int loadOnlineStorageDeletePlugin(const LsdbAppInfoPtr *appInfo)
{
	const char *titleName = sceLsdbGetName(appInfo);
	sce_paf_strncpy(g_onlineStorageTitleId, titleName != NULL ? titleName : "", 13);
	g_onlineStorageTitleId[12] = '\0';

	paf::Plugin::InitParam param;
	param.name = "online_storage_plugin";
	param.resource_file = "vs0:vsh/online_storage/online_storage_plugin.rco";
	param.caller_name = "__main__";
	param.module_file = "vs0:vsh/online_storage/online_storage_plugin.suprx";
	param.module_interface_version = 1;
	param.module_option = 1;
	param.notify_callback = reinterpret_cast<paf::Plugin::NotifyCallback>(onOnlineStorageNotify);
	param.unload_callback = onOnlineStorageUnloaded;

	paf::Plugin::Argument titleArg(static_cast<int32_t>(
		reinterpret_cast<intptr_t>(g_onlineStorageTitleId)));
	param.arguments["title_id"] = titleArg;

	paf::Plugin::Argument modeArg(1);
	param.arguments["operate_mode"] = modeArg;

	paf::Plugin::LoadAsync(param, NULL, 0);
	return 0;
}

/* (0x812A15EA) Show waiting dialog and start online_storage_plugin deletion */
static int launchOnlineStorageDelete(const LsdbAppInfoPtr *appInfo, SceInt32 launchAppId,
				     SceInt32 existingFlags, SceInt32 extraFlags)
{
	g_onlineStorageAppInfo = *appInfo;
	g_onlineStorageLaunchAppId = launchAppId;
	g_onlineStorageExistingFlags = existingFlags;
	g_onlineStorageExtraFlags = extraFlags;

	CommonGuiLayoutDialogParam param;
	param.resourcePlugin = g_topmenuPlugin;
	param.priority = 33;
	param.layout = SceCommonGuiDialog_2A68B4A4;
	const wchar_t *waitStr =
		g_topmenuPlugin->GetString(paf::IDParam(MSG_ONLINE_STORAGE_WAIT));
	param.text = (waitStr != NULL) ? waitStr : L"";
	g_onlineStorageWaitDialogId = SceCommonGuiDialog_CCFCD9A6(&param);

	if (g_onlineStorageDeleteActive)
		return 0;

	if (loadOnlineStorageDeletePlugin(appInfo) == 0) {
		g_onlineStorageDeleteActive = true;
		topmenuBeginBusy();
	}
	return 1;
}

/* (0x812A2586 / 0x812A25A2 / 0x812A25C2) Handler for the prompt asking to delete via online storage */
class OnlineStorageDeletePromptHandler : public TopmenuSpawnConfirmHandler {
public:
	OnlineStorageDeletePromptHandler(const LsdbAppInfoPtr *appInfo, SceInt32 launchAppId,
					 SceInt32 existingFlags, SceInt32 extraFlags)
		: TopmenuSpawnConfirmHandler(appInfo, launchAppId, existingFlags, extraFlags)
	{
	}

	virtual ~OnlineStorageDeletePromptHandler() {}

	virtual void operator()(SceInt32 dialogId, SceInt32 button)
	{
		if (button == 4) {
			launchOnlineStorageDelete(&m_appInfo, m_launchAppId,
						  m_existingFlags, m_extraFlags);
			SceCommonGuiDialog_BC311DC0(dialogId);
		} else if (button == 5) {
			CancelLaunchAndReturnHome();
			SceCommonGuiDialog_BC311DC0(dialogId);
			consoleInfoFileAddInt(279, 1, 0);
		}
	}
};

/* (0x812A25FA / 0x812A2616 / 0x812A2636) Handler for online storage deletion failure dialog */
class OnlineStorageDeleteResultHandler : public TopmenuSpawnConfirmHandler {
public:
	OnlineStorageDeleteResultHandler(const LsdbAppInfoPtr *appInfo, SceInt32 launchAppId,
					 SceInt32 existingFlags, SceInt32 extraFlags)
		: TopmenuSpawnConfirmHandler(appInfo, launchAppId, existingFlags, extraFlags)
	{
	}

	virtual ~OnlineStorageDeleteResultHandler() {}

	virtual void operator()(SceInt32 dialogId, SceInt32 button)
	{
		if (button == 4) {
			paf::common::SharedPtr<paf::job::JobItem> job;
			topmenuStartRemoveSavedataJob(&job, &m_appInfo, m_launchAppId,
						      m_existingFlags, 2);
			SceCommonGuiDialog_BC311DC0(dialogId);
		} else if (button == 5) {
			CancelLaunchAndReturnHome();
			SceCommonGuiDialog_BC311DC0(dialogId);
		}
	}
};

/* (0x812A1A1C) Resume app on success (deleteResult == 0) or ask to force-delete local savedata */
static int showOnlineStorageDeleteResultDialog(const LsdbAppInfoPtr *appInfo, SceInt32 launchAppId,
					       SceInt32 existingFlags, SceInt32 extraFlags,
					       SceInt32 deleteResult)
{
	OnlineStorageDeleteResultHandler *handler =
		new OnlineStorageDeleteResultHandler(appInfo, launchAppId,
						     existingFlags, extraFlags);
	if (deleteResult == 0) {
		handler->ResumePendingLaunch();
		delete handler;
		return -1;
	}

	CommonGuiLayoutDialogParam param;
	param.resourcePlugin = g_topmenuPlugin;
	param.priority = 33;
	param.layout = SceCommonGuiDialog_22B17F2B;
	const wchar_t *msg =
		g_topmenuPlugin->GetString(paf::IDParam(MSG_ONLINE_STORAGE_DELETE_ERR));
	param.text = (msg != NULL) ? msg : L"";
	appendFormattedErrorCode(&param.text, deleteResult);
	param.handler = handler;

	SceInt32 dialogId = SceCommonGuiDialog_CCFCD9A6(&param);
	if (dialogId >= 0)
		registerOnlineStorageDialogHold(dialogId);
	return dialogId;
}

} /* namespace */

/* (0x812A1D82) Show savedata ownership prompt with app icon, title, timestamp, and size */
int topmenuShowOnlineStorageDeletePrompt(const LsdbAppInfoPtr *appInfo, SceInt32 launchAppId,
					 SceInt32 existingFlags, SceInt32 extraFlags,
					 const paf::LocalFileStat *saveStat, SceInt32 errorCode)
{
	CommonGuiLayoutDialogParam param;
	param.resourcePlugin = g_topmenuPlugin;
	param.priority = 33;
	param.layout = SceCommonGuiDialog_22B17F2B;
	const wchar_t *msg = g_topmenuPlugin->GetString(paf::IDParam(MSG_ONLINE_STORAGE_PROMPT));
	param.text = (msg != NULL) ? msg : L"";
	appendFormattedErrorCode(&param.text, errorCode);
	param.handler = new OnlineStorageDeletePromptHandler(appInfo, launchAppId,
							     existingFlags, extraFlags);

	SceInt32 dialogId = SceCommonGuiDialog_CCFCD9A6(&param);
	if (dialogId < 0)
		return dialogId;

	registerOnlineStorageDialogHold(dialogId);

	paf::ui::Widget *rootBox = SceCommonGuiDialog_8947FC23(dialogId, 10);
	if (rootBox != NULL && rootBox->IsInherit(paf::ui::Box::TypeName())) {
		paf::Plugin::TemplateOpenParam openParam;
		g_topmenuPlugin->TemplateOpen(rootBox, paf::IDParam(TEMPLATE_SAVEDATA_INFO_BOX),
					      openParam);

		paf::ui::Widget *infoBox =
			rootBox->FindChild(paf::IDParam(WIDGET_SAVEDATA_CONTAINER), 0);
		if (infoBox != NULL) {
			paf::ui::Widget *iconWidget =
				infoBox->FindChild(paf::IDParam(WIDGET_SAVEDATA_ICON), 0);
			if (iconWidget != NULL) {
				ApplauncherInterface *launcherIf =
					static_cast<ApplauncherInterface *>(
						applauncherPluginGetInterface(
							"applauncher_plugin", 1));
				if (launcherIf != NULL) {
					paf::intrusive_ptr<paf::cache::CacheImageInterface> iconImg;
					const char *titleName = sceLsdbGetName(appInfo);
					paf::string titleStr(titleName != NULL ? titleName : "");
					ApplauncherGetIconFunc getIcon =
						reinterpret_cast<ApplauncherGetIconFunc>(
							launcherIf->func[3]);
					if (getIcon(&iconImg, &titleStr, 0, 0) &&
					    iconImg.get() != NULL) {
						iconWidget->SetDrawObjMaskBit(&iconImg, 0);
					}
				}
			}

			paf::ui::Widget *titleWidget =
				infoBox->FindChild(paf::IDParam(WIDGET_SAVEDATA_TITLE), 0);
			if (titleWidget != NULL) {
				const char *appTitle = sceLsdbGetTitle(appInfo);
				if (appTitle != NULL) {
					paf::string titleUtf8(appTitle);
					paf::wstring titleWide;
					paf::common::Utf8ToUtf16(titleUtf8, &titleWide);
					titleWidget->SetString(titleWide);
				}
			}

			paf::ui::Widget *metaWidget =
				rootBox->FindChild(paf::IDParam(WIDGET_SAVEDATA_DATE_SIZE), 0);
			if (metaWidget != NULL && saveStat != NULL) {
				paf::datetime::DateTime mtime = saveStat->st_mtime;
				paf::rtc::Tick utcTick =
					paf::datetime::DateTime::ConvertDateTimeToRtcTick(mtime);
				paf::rtc::Tick localTick = 0;
				paf::rtc::DateTime localDt;
				if (paf::rtc::ConvertUtcToLocalTime(&utcTick, &localTick) == 0 &&
				    paf::rtc::ConvertTickToDateTime(&localTick, &localDt, NULL) == 0) {
					paf::datetime::DateTime converted(
						localDt.year, localDt.month, localDt.day,
						localDt.hour, localDt.minute, localDt.second,
						localDt.microsecond);
					mtime = converted;
				}

				wchar_t dateBuf[64];
				sce_paf_memset(dateBuf, 0, sizeof(dateBuf));
				mtime.Format(dateBuf, sizeof(dateBuf), L"%c", false);

				paf::wstring dateWide(dateBuf);
				paf::string dateUtf8;
				paf::common::Utf16ToUtf8(dateWide, &dateUtf8);
				paf::wstring dateNormalized;
				paf::common::Utf8ToUtf16(dateUtf8, &dateNormalized);

				paf::wstring sizeText;
				shellSizeText(&sizeText, static_cast<SceUInt64>(saveStat->st_size));

				paf::wstring combined = dateNormalized;
				combined += L"  ";
				combined += sizeText.c_str();
				metaWidget->SetString(combined);
			}
		}
	}

	consoleInfoFileAddInt(278, 1, 0);
	return dialogId;
}
