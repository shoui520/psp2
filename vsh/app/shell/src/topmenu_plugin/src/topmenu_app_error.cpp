/*
 * topmenu_app_error.cpp: application launch / license / LiveArea error dialog
 * dispatcher and PlayStation Store link handler (0x81292EF8-0x81294B23).
 */
#include "topmenu_internal.h"
#include <appmgr.h>

namespace {

struct TopmenuErrorMapEntry {
	SceInt32 errorCode;
	SceUInt32 messageHash;
	SceUInt8 showErrorCode;
};

/* 62-entry application error mapping table at 0x81516CAC */
static const TopmenuErrorMapEntry kTopmenuAppErrorMap[62] = {
	{ (SceInt32)0x80101124u, 0x33633CE3u, 0 },
	{ (SceInt32)0x80101123u, 0x073B4655u, 0 },
	{ (SceInt32)0x8010114Au, 0x546E63C5u, 0 },
	{ (SceInt32)0x8010114Cu, 0x78D452F9u, 1 },
	{ (SceInt32)0x800F0B30u, 0x8337B618u, 0 },
	{ (SceInt32)0x80870009u, 0xBB3E6D51u, 0 },
	{ (SceInt32)0x80558350u, 0xBB3E6D51u, 0 },
	{ (SceInt32)0x80558228u, 0xBB3E6D51u, 0 },
	{ (SceInt32)0x80101190u, 0x3CD248DCu, 0 },
	{ (SceInt32)0x80251002u, 0xC8C5355Cu, 0 },
	{ (SceInt32)0x80101141u, 0xE3A39BE7u, 0 },
	{ (SceInt32)0x80103941u, 0xE3A39BE7u, 0 },
	{ (SceInt32)0x80103938u, 0x42D4C19Bu, 0 },
	{ (SceInt32)0x80558322u, 0xE3A39BE7u, 1 },
	{ (SceInt32)0x808A0384u, 0xFE9FF86Au, 0 },
	{ (SceInt32)0x80103900u, 0x611188EBu, 0 },
	{ (SceInt32)0x80101107u, 0x611188EBu, 0 },
	{ (SceInt32)0x80101005u, 0x611188EBu, 0 },
	{ (SceInt32)0x80103930u, 0xDC27698Fu, 0 },
	{ (SceInt32)0x80103931u, 0x338DFC50u, 0 },
	{ (SceInt32)0x80103915u, 0xAD0AFA76u, 0 },
	{ (SceInt32)0x808F0103u, 0x1DF465B7u, 1 },
	{ (SceInt32)0x808F0104u, 0x1DF465B7u, 1 },
	{ (SceInt32)0x808F0105u, 0x1DF465B7u, 1 },
	{ (SceInt32)0x808F0106u, 0xBB3E6D51u, 0 },
	{ (SceInt32)0x808F0107u, 0xBB3E6D51u, 0 },
	{ (SceInt32)0x808F0108u, 0x3CD248DCu, 0 },
	{ (SceInt32)0x808F0109u, 0xBB3E6D51u, 0 },
	{ (SceInt32)0x808F040Eu, 0x78D452F9u, 1 },
	{ (SceInt32)0x808F040Au, 0xC8C5355Cu, 0 },
	{ (SceInt32)0x808F0483u, 0xE3A39BE7u, 0 },
	{ (SceInt32)0x808F0407u, 0xE4B54869u, 0 },
	{ (SceInt32)0x808F040Du, 0x1DF465B7u, 1 },
	{ (SceInt32)0x808F048Bu, 0x71FCAB00u, 0 },
	{ (SceInt32)0x80103A52u, 0x43E94314u, 0 },
	{ (SceInt32)0x80103A51u, 0xE8E8C3D2u, 0 },
	{ (SceInt32)0x80103928u, 0xF427480Du, 0 },
	{ (SceInt32)0x80103927u, 0xC8FAD0B6u, 0 },
	{ (SceInt32)0x8010392Bu, 0x8337B618u, 0 },
	{ (SceInt32)0x8010392Fu, 0x78D452F9u, 0 },
	{ (SceInt32)0x80101115u, 0xC8FAD0B6u, 0 },
	{ (SceInt32)0x8010393Bu, 0xAD0AFA76u, 0 },
	{ (SceInt32)0x8010393Cu, 0xAD0AFA76u, 0 },
	{ (SceInt32)0x80010002u, 0xC8FAD0B6u, 1 },
	{ (SceInt32)0x8001008Fu, 0xC8FAD0B6u, 1 },
	{ (SceInt32)0x80800005u, 0xC8FAD0B6u, 1 },
	{ (SceInt32)0x80800006u, 0xC8FAD0B6u, 1 },
	{ (SceInt32)0x8080000Du, 0xC8FAD0B6u, 1 },
	{ (SceInt32)0x80800015u, 0xC8FAD0B6u, 1 },
	{ (SceInt32)0x80870006u, 0xBB3E6D51u, 0 },
	{ (SceInt32)0x80870007u, 0xBB3E6D51u, 0 },
	{ (SceInt32)0x80800004u, 0x1DF465B7u, 1 },
	{ (SceInt32)0x80800010u, 0x1DF465B7u, 1 },
	{ (SceInt32)0x80870004u, 0x1DF465B7u, 1 },
	{ (SceInt32)0x80870003u, 0x1DF465B7u, 1 },
	{ (SceInt32)0x80870005u, 0x1DF465B7u, 1 },
	{ (SceInt32)0x80870008u, 0x3CD248DCu, 0 },
	{ (SceInt32)0x80800011u, 0xE3A39BE7u, 0 },
	{ (SceInt32)0x80800018u, 0xE4B54869u, 0 },
	{ (SceInt32)0x80800016u, 0x3BABF33Cu, 0 },
	{ (SceInt32)0x80800019u, 0x8337B618u, 0 },
	{ (SceInt32)0x80103944u, 0xE3A39BE7u, 0 }
};

static const SceUInt32 ERR_MSG_FALLBACK_ID = 0xC51B20A4u;	/* -988109660 */
static const SceUInt32 ERR_MSG_LOW_MEMORY_ID = 0x3BACC6FCu;	/* 1001124668 */
static const SceUInt32 ERR_MSG_LOW_MEMORY_MC_ID = 0x78D452F9u;	/* 2027180793 */
static const SceUInt32 ERR_MSG_DRM_LICENSE_ID = 0x1DF465B7u;	/* 502556087 */
static const SceUInt32 ERR_MSG_DRM_LICENSE_ALT_ID = 0x33043F31u;/* 855923249 */
static const SceUInt32 ERR_MSG_TROPHY_UNAVAIL_ID = 0xAD0AFA76u;	/* -1391789450 */
static const SceUInt32 ERR_MSG_TROPHY_APPEND_ID = 0x67FA0F7Au;	/* 1744473034 */
static const SceUInt32 ERR_MSG_CORRUPT_DETAIL_ID = 0xC8FAD0B6u;	/* -923086666 */
static const SceUInt32 ERR_MSG_NOT_INSTALLED_ID = 0x7F3798A2u;	/* 2134370146 */
static const SceUInt32 ERR_MSG_STORE_ALT_SRC_ID = 0xE3A39BE7u;	/* -475816985 */
static const SceUInt32 ERR_MSG_STORE_ALT_DST_ID = 0xF3FB0FC8u;	/* -201667640 */
static const SceUInt32 ERR_BTN_PS_STORE_ID = 0x2BD333C6u;	/* 735295558 */
static const SceUInt32 ERR_BTN_CANCEL_ID = 0xADCE081Fu;		/* -1379034401 */
static const SceUInt32 ERR_ICON_PLANE_ID = 0xD713BE25u;		/* -686572747 */
static const SceUInt32 ERR_LIVEAREA_ICON_TEX_ID = 0x3CAD056Fu;	/* 1017993839 */

static const CommonGuiDialogPage kLiveAreaErrorDialogPage = {
	{ 0x3A91750Cu, 0x438C9DC5u, 0x172C9498u }
};

/* Handler forwarding button 4 to PlayStation Store content page (0x81294924..0x812949C0) */
class TopmenuStoreLinkDialogHandler : public CommonGuiDialogHandler {
public:
	TopmenuStoreLinkDialogHandler(CommonGuiDialogHandler *innerHandler,
				      const paf::string &contentId)
		: m_innerHandler(innerHandler), m_contentId(contentId)
	{
	}

	/* 0x81294924 / 0x81294970 */
	virtual ~TopmenuStoreLinkDialogHandler()
	{
		if (m_innerHandler != NULL) {
			delete m_innerHandler;
			m_innerHandler = NULL;
		}
	}

	/* 0x812949C0 */
	virtual void operator()(SceInt32 value, SceInt32 button)
	{
		if (button == 4) {
			char uriBuf[1024];
			sce_paf_snprintf(uriBuf, sizeof(uriBuf), "%sbrowse?content=%s",
					 "psns:", m_contentId.c_str());
			sceAppMgrLaunchAppByUri(0x20000u, uriBuf);
		}
		if (m_innerHandler != NULL)
			(*m_innerHandler)(value, button);
	}

private:
	CommonGuiDialogHandler *m_innerHandler;
	paf::string m_contentId;
};

/* 0x81294A34 */
static void onPowerHoldErrorDismiss(int eventCode, const void *eventBuf,
				    SceSize eventSize, void *userData)
{
	(void)eventCode;
	(void)eventBuf;
	(void)eventSize;
	int dialogId = (int)(intptr_t)userData;
	SceCommonGuiDialog_BC311DC0(dialogId);
	if (g_topWindow->m_powerManageIf != NULL) {
		g_topWindow->m_powerManageIf->removePowerEventListener(
			onPowerHoldErrorDismiss, userData);
	}
	if (g_topWindow->m_indicatorIf != NULL)
		g_topWindow->m_indicatorIf->UnlockButton();
}

static bool lookupTopmenuError(SceInt32 errorCode, SceUInt32 *outHash, SceUInt8 *outShowCode)
{
	for (SceUInt32 i = 0; i < 62; ++i) {
		if (kTopmenuAppErrorMap[i].errorCode == errorCode) {
			*outHash = kTopmenuAppErrorMap[i].messageHash;
			*outShowCode = kTopmenuAppErrorMap[i].showErrorCode;
			return true;
		}
	}
	*outHash = 0;
	*outShowCode = 0;
	return false;
}

/* 0x812939E2 */
static int showAppLaunchStatusError(SceInt32 errorCode, const LsdbAppInfoPtr *appInfo,
				    CommonGuiDialogHandler *handler)
{
	LsdbAppInfo *record = appInfo->get();
	LsdbAppInfo::iterator it = record->find(LSDB_KEY_TITLE_ID);
	if (it == record->end()) {
		paf::wstring msg(g_topmenuPlugin->GetString(
			paf::IDParam(ERR_MSG_NOT_INSTALLED_ID)));
		shellErrorDialogShow(2, errorCode, &msg, false);
		return -1;
	}

	const char *titleId = (it->second.type == LSDB_VALUE_TEXT && it->second.text != NULL)
				      ? it->second.text->c_str()
				      : "";
	paf::string titleIdCopy(titleId);
	SceAppMgrAppStatus status;
	sce_paf_memset(&status, 0, sizeof(status));
	if (sceAppMgrGetStatusByName(titleIdCopy.c_str(), &status) != 0) {
		paf::wstring msg(g_topmenuPlugin->GetString(
			paf::IDParam(ERR_MSG_NOT_INSTALLED_ID)));
		shellErrorDialogShow(2, errorCode, &msg, false);
		return -1;
	}

	return topmenuShowAppErrorMessageDialog(
		errorCode, appInfo, handler, NULL, NULL, NULL, NULL, NULL);
}

/* 0x81293C54 */
static int showLiveAreaIconErrorDialog(SceInt32 errorCode,
				       const paf::intrusive_ptr<paf::graph::Surface> &iconSurf,
				       CommonGuiDialogHandler *handler,
				       const wchar_t *msgArgA, const wchar_t *msgArgB,
				       const wchar_t *msgArgC, const wchar_t *msgArgD,
				       const wchar_t *msgArgE)
{
	CommonGuiLayoutDialogParam param;
	param.resourcePlugin = paf::Plugin::Find("topmenu_plugin");
	param.priority = 33;
	param.layout.page = &kLiveAreaErrorDialogPage;
	param.layout.parts = SceCommonGuiDialog_AF75CA6D.parts;
	param.layout.buttons = SceCommonGuiDialog_AF75CA6D.buttons;

	SceUInt32 msgHash = 0;
	SceUInt8 showCode = 0;
	lookupTopmenuError(errorCode, &msgHash, &showCode);

	const wchar_t *fmt = g_topmenuPlugin->GetString(paf::IDParam(msgHash));
	param.text = shellFormatText(
		fmt,
		(msgArgA != NULL) ? msgArgA : L"",
		(msgArgB != NULL) ? msgArgB : L"",
		(msgArgC != NULL) ? msgArgC : L"",
		(msgArgD != NULL) ? msgArgD : L"",
		(msgArgE != NULL) ? msgArgE : L"");
	param.handler = handler;

	int dialogId = SceCommonGuiDialog_CCFCD9A6(&param);
	if (dialogId >= 0) {
		paf::ui::Widget *root = SceCommonGuiDialog_8947FC23(dialogId, 4);
		if (root != NULL && iconSurf.get() != NULL) {
			paf::ui::Widget *iconChild =
				root->FindChild(paf::IDParam(ERR_ICON_PLANE_ID), 0);
			if (iconChild != NULL && iconChild->IsInherit(paf::ui::Plane::TypeName()))
				iconChild->SetTexture(iconSurf);
		}
	}
	return dialogId;
}

/* 0x81294050 */
static int showStoreLinkErrorDialog(SceInt32 errorCode, const LsdbAppInfoPtr *appInfo,
				    CommonGuiDialogHandler *handler,
				    const wchar_t *msgArgA, const wchar_t *msgArgB,
				    const wchar_t *msgArgC, const wchar_t *msgArgD,
				    const wchar_t *msgArgE)
{
	CommonGuiMessageDialogParam param;
	param.resourcePlugin = paf::Plugin::Find("topmenu_plugin");
	param.type = errorCode;
	param.useLayout = true;
	param.layout = SceCommonGuiDialog_22B17F2B;
	param.priority = 33;

	SceUInt32 msgHash = 0;
	SceUInt8 showCode = 0;
	lookupTopmenuError(errorCode, &msgHash, &showCode);
	if (msgHash == ERR_MSG_STORE_ALT_SRC_ID)
		msgHash = ERR_MSG_STORE_ALT_DST_ID;

	const wchar_t *fmt = g_topmenuPlugin->GetString(paf::IDParam(msgHash));
	param.title.format = shellFormatText(
		fmt,
		(msgArgA != NULL) ? msgArgA : L"",
		(msgArgB != NULL) ? msgArgB : L"",
		(msgArgC != NULL) ? msgArgC : L"",
		(msgArgD != NULL) ? msgArgD : L"",
		(msgArgE != NULL) ? msgArgE : L"");
	param.defaultButton = (showCode != 0) ? 1 : 0;
	param.cancelButton = 0;
	param.buttonCount = 0;

	const char *contentIdStr = SceLsdb_8FDA59EA(appInfo);
	paf::string contentId((contentIdStr != NULL) ? contentIdStr : "");
	param.handler = new TopmenuStoreLinkDialogHandler(handler, contentId);

	int dialogId = SceCommonGuiDialog_20B141F1(&param);
	if (dialogId >= 0) {
		paf::ui::Widget *yesBtn = SceCommonGuiDialog_8947FC23(dialogId, 12);
		if (yesBtn != NULL && yesBtn->IsInherit(paf::ui::Button::TypeName())) {
			yesBtn->SetString(paf::wstring(
				g_topmenuPlugin->GetString(paf::IDParam(ERR_BTN_PS_STORE_ID))));
		}
		paf::ui::Widget *noBtn = SceCommonGuiDialog_8947FC23(dialogId, 13);
		if (noBtn != NULL && noBtn->IsInherit(paf::ui::Button::TypeName())) {
			noBtn->SetString(paf::wstring(
				g_topmenuPlugin->GetString(paf::IDParam(ERR_BTN_CANCEL_ID))));
		}
	}
	return dialogId;
}

} /* namespace */

/* 0x81292EF8 */
void topmenuPushPowerKeyHandler(power_manage::PowerKeyCallback callback, void *userData)
{
	if (g_topWindow->m_powerManageIf != NULL)
		g_topWindow->m_powerManageIf->pushKeyHandler(callback, userData);
}

/* 0x81292F0E */
void topmenuPopPowerKeyHandler(power_manage::PowerKeyCallback callback, void *userData)
{
	if (g_topWindow->m_powerManageIf != NULL)
		g_topWindow->m_powerManageIf->popKeyHandler(callback, userData);
}

/* 0x81292F24 */
void topmenuAddTabLeaveListener(SceInt32 pageKey, TopmenuEventCallback callback, void *userData)
{
	TopTabBase *tab = g_topWindow->FindTabByPageKey(pageKey);
	if (tab != NULL)
		tab->AddEventListener(2, callback, userData);
}

/* 0x81293080 */
void topmenuRemoveTabLeaveListener(SceInt32 pageKey, TopmenuEventCallback callback, void *userData)
{
	TopTabBase *tab = g_topWindow->FindTabByPageKey(pageKey);
	if (tab != NULL)
		tab->RemoveEventListener(2, callback, userData);
}

/* 0x8129315C / 0x812939C6 */
int topmenuShowAppErrorMessageDialog(SceInt32 errorCode, const LsdbAppInfoPtr *appInfo,
				     CommonGuiDialogHandler *handler,
				     const wchar_t *msgArgA, const wchar_t *msgArgB,
				     const wchar_t *msgArgC, const wchar_t *msgArgD,
				     const wchar_t *msgArgE)
{
	int storageKind = 0;
	if (g_topWindow->m_gameIf != NULL && appInfo != NULL) {
		typedef int (*GetStorageFn)(const LsdbAppInfoPtr *info, int *outStorage);
		((GetStorageFn)g_topWindow->m_gameIf->func[GAME_PLUGIN_FUNC_GET_STORAGE])(
			appInfo, &storageKind);
	}

	CommonGuiMessageDialogParam param;
	param.type = errorCode;
	param.message = paf::wstring(
		g_topmenuPlugin->GetString(paf::IDParam(ERR_MSG_FALLBACK_ID)));
	param.priority = 33;
	param.defaultButton = 1;
	param.cancelButton = 0;

	SceUInt32 msgHash = 0;
	SceUInt8 showCode = 0;
	lookupTopmenuError(errorCode, &msgHash, &showCode);

	if (msgHash == ERR_MSG_DRM_LICENSE_ID && (storageKind == 1 || storageKind == 2)) {
		msgHash = ERR_MSG_DRM_LICENSE_ALT_ID;
	} else if (msgHash == ERR_MSG_LOW_MEMORY_ID && storageKind == 1) {
		msgHash = ERR_MSG_LOW_MEMORY_MC_ID;
		showCode = 1;
	}

	if (msgHash != 0) {
		if (msgHash == ERR_MSG_LOW_MEMORY_ID) {
			paf::wstring sizeText;
			shellSizeTextKB(&sizeText, 2048);
			param.title.format = paf::wstring(
				g_topmenuPlugin->GetString(paf::IDParam(ERR_MSG_LOW_MEMORY_ID)));
			param.title.args.insert(param.title.args.begin(), sizeText);
		} else if (msgHash == ERR_MSG_TROPHY_UNAVAIL_ID &&
			   errorCode == (SceInt32)0x8010393Bu) {
			paf::wstring combined(
				g_topmenuPlugin->GetString(paf::IDParam(msgHash)));
			combined += L"\n\n";
			combined += g_topmenuPlugin->GetString(
				paf::IDParam(ERR_MSG_TROPHY_APPEND_ID));
			param.title.format = combined;
		} else {
			param.title.format = paf::wstring(
				g_topmenuPlugin->GetString(paf::IDParam(msgHash)));
			if (msgArgA != NULL)
				param.title.args.push_back(paf::wstring(msgArgA));
			if (msgArgB != NULL)
				param.title.args.push_back(paf::wstring(msgArgB));
			if (msgArgC != NULL)
				param.title.args.push_back(paf::wstring(msgArgC));
			if (msgArgD != NULL)
				param.title.args.push_back(paf::wstring(msgArgD));
			if (msgArgE != NULL)
				param.title.args.push_back(paf::wstring(msgArgE));
		}

		if (showCode == 0) {
			param.defaultButton = 0;
			param.cancelButton = 0;
		}
	}

	if (errorCode == (SceInt32)0x80103927u || msgHash == ERR_MSG_CORRUPT_DETAIL_ID) {
		param.messageIds.resize(3);
		param.messageIds[0] = 0x18AD652Bu;
		param.messageIds[1] = 0xFAFC475Fu;
		param.messageIds[2] = 0x6C571BC3u;
	}

	if (g_topmenuPlugin == NULL) {
		if (handler != NULL)
			(*handler)(-1, 0);
		return -1;
	}

	int dialogId = SceCommonGuiDialog_A6422883(g_topmenuPlugin, &param, handler, NULL);
	if (dialogId < 0 && handler != NULL)
		(*handler)(-1, 0);
	return dialogId;
}

/* 0x8129456C */
int topmenuShowAppError(SceInt32 errorCode, const LsdbAppInfoPtr *appInfo,
			CommonGuiDialogHandler *handler,
			const wchar_t *msgArgA, const wchar_t *msgArgB,
			const wchar_t *msgArgC, const wchar_t *msgArgD,
			const wchar_t *msgArgE)
{
	if (errorCode == (SceInt32)0x80103900u ||
	    errorCode == (SceInt32)0x80101005u ||
	    errorCode == (SceInt32)0x8010320Bu) {
		return showAppLaunchStatusError(errorCode, appInfo, handler);
	}

	if (errorCode == (SceInt32)0x8010392Bu ||
	    errorCode == (SceInt32)0x800F0B30u ||
	    errorCode == (SceInt32)0x80800019u) {
		paf::intrusive_ptr<paf::graph::Surface> iconSurf;
		paf::Plugin *liveAreaPlugin = paf::Plugin::Find("livearea_plugin");
		if (liveAreaPlugin != NULL) {
			iconSurf = liveAreaPlugin->GetTexture(
				paf::IDParam(ERR_LIVEAREA_ICON_TEX_ID));
		}
		return showLiveAreaIconErrorDialog(
			errorCode, iconSurf, handler,
			msgArgA, msgArgB, msgArgC, msgArgD, msgArgE);
	}

	if (appInfo != NULL && appInfo->get() != NULL && SceLsdb_8FDA59EA(appInfo) != NULL &&
	    (errorCode == (SceInt32)0x80103941u ||
	     errorCode == (SceInt32)0x80800011u ||
	     errorCode == (SceInt32)0x80101141u ||
	     errorCode == (SceInt32)0x80558322u)) {
		return showStoreLinkErrorDialog(
			errorCode, appInfo, handler,
			msgArgA, msgArgB, msgArgC, msgArgD, msgArgE);
	}

	return topmenuShowAppErrorMessageDialog(
		errorCode, appInfo, handler,
		msgArgA, msgArgB, msgArgC, msgArgD, msgArgE);
}

/* 0x81294754 */
void topmenuShowPowerHoldErrorDialog(SceInt32 errorCode)
{
	paf::thread::RMutex::main_thread_mutex.Lock();
	CommonGuiMessageDialogParam param;
	param.priority = 33;
	param.resourcePlugin = g_topmenuPlugin;
	param.type = errorCode;
	param.modal = false;

	int dialogId = SceCommonGuiDialog_20B141F1(&param);
	if (dialogId >= 0) {
		if (g_topWindow->m_powerManageIf != NULL) {
			g_topWindow->m_powerManageIf->addPowerEventListener(
				onPowerHoldErrorDismiss, (void *)(intptr_t)dialogId);
		}
		if (g_topWindow->m_indicatorIf != NULL)
			g_topWindow->m_indicatorIf->LockButton();
	}
	paf::thread::RMutex::main_thread_mutex.Unlock();
}
