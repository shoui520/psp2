/*
 * The common functions of msg_dialog_util.cpp, which Sony linked into SceMsgDialogPlugin,
 * SceCommonGuiDialog, safemode and most of the system software's applications: the check of the
 * caller's SDK version, the size and %1..%11 formatters, and the countries' number formats. No
 * Sony record names the file or its functions (the names are ours).
 */
#ifndef MSG_DIALOG_MSG_DIALOG_UTIL_H
#define MSG_DIALOG_MSG_DIALOG_UTIL_H

#include <scetypes.h>
#include <std/string>

/* the errors of the SDK version check (names ours): the caller's process has no SDK version, the
   system allows none, and the caller's is newer than the system allows */
#define MSG_DIALOG_PLUGIN_ERROR_NO_SDK_VERSION		-2146432352	/* 0x80100AA0 */
#define MSG_DIALOG_PLUGIN_ERROR_NO_ALLOWED_SDK_VERSION	-2146432351	/* 0x80100AA1 */
#define MSG_DIALOG_PLUGIN_ERROR_SDK_VERSION_TOO_NEW	-2146432350	/* 0x80100AA2 */

int MsgDialogCheckSdkVersion(SceUInt32 sdkVersion);
int MsgDialogGetSdkLevel(void);
void MsgDialogFormatSize(paf::wstring *out, SceUInt32 sizeKiB);
paf::wstring MsgDialogFormat(const wchar_t *format, const wchar_t *arg1 = L"",
			     const wchar_t *arg2 = L"", const wchar_t *arg3 = L"",
			     const wchar_t *arg4 = L"", const wchar_t *arg5 = L"",
			     const wchar_t *arg6 = L"", const wchar_t *arg7 = L"",
			     const wchar_t *arg8 = L"", const wchar_t *arg9 = L"",
			     const wchar_t *arg10 = L"", const wchar_t *arg11 = L"");

/* what MsgDialogFormat puts for %A..%Z and %a..%z: nothing while it is not set */
extern const wchar_t *(*g_msgDialogFormatLetter)(wchar_t letter);

#endif
