/*
 * patch_chk_util.cpp: plugin handle accessors, CommonGuiDialog wrappers,
 * byte-size formatting, and short version parser (0x812ABBD4-0x812AC3A7).
 */
#include <stddef.h>
#include <scetypes.h>
#include <appmgr/appmgr.h>
#include <common/char_encoding.h>
#include <graphics/surface/surface.h>
#include <std/stdio.h>
#include <std/string.h>
#include "patch_chk_internal.h"

using namespace paf;

extern "C" {
int sceAppMgrGetStatusByName(const char *appName, SceAppMgrAppStatus *outStatus);
}

static Plugin *s_patchChkPlugin = NULL;	/* 0x81545B68 */

/* Stores the active patch_chk_plugin instance (0x812ABBD4) */
void patchChkSetPlugin(Plugin *plugin)
{
	s_patchChkPlugin = plugin;
}

/* Clears the stored plugin pointer (0x812ABBE0) */
void patchChkClearPlugin(void)
{
	s_patchChkPlugin = NULL;
}

/* Returns the stored patch_chk_plugin instance (0x812ABBEE) */
Plugin *patchChkGetPlugin(void)
{
	return s_patchChkPlugin;
}

/* Looks up a localized wide string from patch_chk_plugin's RCO (0x812ABBFA) */
const wchar_t *patchChkGetString(const IDParam &id)
{
	return s_patchChkPlugin->GetString(id);
}

/* Retrieves a layout part widget from an open CommonGuiDialog (0x812ABC10) */
ui::Widget *patchChkGetDialogPart(int dialogId, int partIndex)
{
	return SceCommonGuiDialog_8947FC23(dialogId, partIndex);
}

/* Formats a byte count as "(%lluKB)", "(%lluMB)", or "(%lluGB)" (0x812ABC18) */
void patchChkFormatSizeParen(wstring *outText, SceUInt64 sizeBytes)
{
	wchar_t formatted[16];
	if (sizeBytes > 0x9C3C00ULL) {
		if (sizeBytes > 0x270F00000ULL) {
			SceUInt64 gb = (sizeBytes + 0x3FFFFFFFULL) / 0x40000000ULL;
			sce_paf_swprintf(formatted, 16, L"%lluGB", gb);
		} else {
			SceUInt64 mb = (sizeBytes + 0xFFFFFULL) / 0x100000ULL;
			sce_paf_swprintf(formatted, 16, L"%lluMB", mb);
		}
	} else {
		SceUInt64 kb = (sizeBytes + 0x3FFULL) / 0x400ULL;
		if (kb == 0)
			kb = 1;
		sce_paf_swprintf(formatted, 16, L"%lluKB", kb);
	}

	outText->assign(L"(");
	outText->append(formatted);
	outText->append(L")");
}

/* Opens a CommonGuiLayoutDialog with priority 45 (0x812ABD5E) */
int patchChkShowLayoutDialog(const wstring &text, const CommonGuiDialogLayout *layout,
			     CommonGuiDialogHandler *handler)
{
	CommonGuiLayoutDialogParam param;
	param.resourcePlugin = s_patchChkPlugin;
	param.text = text;
	param.layout = *layout;
	param.priority = 45;
	param.handler = handler;
	return SceCommonGuiDialog_CCFCD9A6(&param);
}

/* Closes a CommonGuiDialog handle (0x812ABE5E) */
void patchChkCloseDialog(int dialogId)
{
	SceCommonGuiDialog_BC311DC0(dialogId);
}

/* Shows a Yes/No layout dialog with localized RCO string (0x812ABE66) */
int patchChkShowYesNoDialog(const IDParam &textId, CommonGuiDialogHandler *handler)
{
	const wchar_t *msg = s_patchChkPlugin->GetString(textId);
	wstring text;
	if (msg != NULL)
		text.assign(msg);
	return patchChkShowLayoutDialog(text, &SceCommonGuiDialog_22B17F2B, handler);
}

/* Shows an OK layout dialog with localized RCO string (0x812ABEE6) */
int patchChkShowOkDialog(const IDParam &textId, CommonGuiDialogHandler *handler)
{
	const wchar_t *msg = s_patchChkPlugin->GetString(textId);
	wstring text;
	if (msg != NULL)
		text.assign(msg);
	return patchChkShowLayoutDialog(text, &SceCommonGuiDialog_AF75CA6D, handler);
}

/* Shows a Cancel-only layout dialog with localized RCO string (0x812ABF66) */
int patchChkShowCancelDialog(const IDParam &textId, CommonGuiDialogHandler *handler)
{
	const wchar_t *msg = s_patchChkPlugin->GetString(textId);
	wstring text;
	if (msg != NULL)
		text.assign(msg);
	return patchChkShowLayoutDialog(text, &SceCommonGuiDialog_F8B2D7E1, handler);
}

/* Shows a CommonGuiMessageDialog for an error code (0x812ABFE6) */
int patchChkShowErrorDialog(const IDParam &messageId, int errorCode,
			    CommonGuiDialogHandler *handler,
			    CommonGuiMessageDialogOpenHandler *openHandler)
{
	CommonGuiMessageDialogParam param;
	param.type = errorCode;
	param.priority = 45;
	if (messageId.GetIDHash() != 0xC51B20E4u)
		param.messageId = messageId.GetIDHash();
	if (errorCode == 0)
		param.defaultButton = 0;
	return SceCommonGuiDialog_A6422883(s_patchChkPlugin, &param, handler, openHandler);
}

/* Shows an OK dialog displaying an app/badge icon texture pair (0x812AC110) */
int patchChkShowIconDialog(const IDParam &titleId, const IDParam &textId,
			   const IDParam &iconId, const IDParam &badgeIconId,
			   CommonGuiDialogHandler *handler)
{
	(void)titleId;
	static const CommonGuiDialogPage iconDialogPage = {
		{ 0x3A918C2Cu, 0x438C7285u, 0x172CA8B8u }
	};

	CommonGuiLayoutDialogParam param;
	param.heading.assign(L"");
	param.resourcePlugin = s_patchChkPlugin;
	const wchar_t *msg = s_patchChkPlugin->GetString(textId);
	if (msg != NULL)
		param.text.assign(msg);

	param.layout.page = &iconDialogPage;
	param.layout.parts = SceCommonGuiDialog_7D8FE85E.parts;
	param.layout.buttons = SceCommonGuiDialog_7D8FE85E.buttons;
	param.image = s_patchChkPlugin->GetTexture(iconId);
	param.secondImage = s_patchChkPlugin->GetTexture(badgeIconId);
	param.handler = handler;
	param.priority = 45;
	return SceCommonGuiDialog_CCFCD9A6(&param);
}

/* Registers a one-shot callback invoked once dialogId opens (0x812AC2D4) */
int patchChkSetDialogOpenCallback(int dialogId, CommonGuiDialogOpenFunction func, void *userData)
{
	return SceCommonGuiDialog_B1BA8D9E(dialogId, func, userData);
}

/* Returns true if the application titleId is currently running (0x812AC2DC) */
bool patchChkIsAppRunning(const char *titleId)
{
	SceAppMgrAppStatus status;
	sce_paf_memset(&status, 0, sizeof(status));
	return sceAppMgrGetStatusByName(titleId, &status) == 0;
}

/* Parses "MM.mm" (or " M.mm") into 16-bit major/minor integers (0x812AC31E) */
int patchChkParseVersionShort(char *versionStr, SceSize maxLen,
			      SceUInt16 *outMajor, SceUInt16 *outMinor)
{
	*outMajor = 0;
	*outMinor = 0;
	if (versionStr[0] == ' ')
		versionStr[0] = '0';

	SceSize pos = 0;
	int mult = 1;
	while (pos < maxLen && versionStr[pos] != '\0') {
		char ch = versionStr[pos++];
		if (ch == '.')
			break;
		*outMajor = (SceUInt16)((ch - '0') + (*outMajor) * mult);
		mult *= 10;
	}

	mult = 1;
	while (pos < maxLen && versionStr[pos] != '\0') {
		char ch = versionStr[pos++];
		if (ch == '.')
			break;
		*outMinor = (SceUInt16)((ch - '0') + (*outMinor) * mult);
		mult *= 10;
	}
	return 0;
}
