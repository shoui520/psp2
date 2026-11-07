/*
 * SceMsgDialogPlugin's own interface between its files: the message dialog, a common GUI dialog
 * (SceCommonGuiDialog) on the plugin's page, and the checks of the caller's parameter. Sony's
 * names here are the plugin's function names it writes into the work memory (OnUpdate,
 * MsgDialogCallback); the class, its methods, this header's name and the files' are ours.
 */
#ifndef MSG_DIALOG_MSG_DIALOG_PLUGIN_H
#define MSG_DIALOG_MSG_DIALOG_PLUGIN_H

#include <scetypes.h>
#include <message_dialog.h>
#include <plugin.h>
#include <std/string>
#include <widget/w_scene.h>
#include <common_gui_dialog/common_gui_dialog.h>

/* the plugin's page */
#define MSG_DIALOG_PAGE_ID		0x9573096E

/* the plugin's own errors (names ours): a mode or system message it does not know, no memory for
   the dialog, and the error code it shows for a save data message off a memory card */
#define MSG_DIALOG_PLUGIN_ERROR_INVALID		-2146432384	/* 0x80100A80 */
#define MSG_DIALOG_PLUGIN_ERROR_NO_MEMORY	-2146432383	/* 0x80100A81 */
#define MSG_DIALOG_PLUGIN_ERROR_NOSPACE_MEDIA	-2146432352	/* 0x80100AA0 */

/* the errors of the SDK version check (names ours): the caller's process has no SDK version, the
   system allows none, and the caller's is newer than the system allows */
#define MSG_DIALOG_PLUGIN_ERROR_NO_SDK_VERSION		-2146432352	/* 0x80100AA0 */
#define MSG_DIALOG_PLUGIN_ERROR_NO_ALLOWED_SDK_VERSION	-2146432351	/* 0x80100AA1 */
#define MSG_DIALOG_PLUGIN_ERROR_SDK_VERSION_TOO_NEW	-2146432350	/* 0x80100AA2 */

/* what the dialog calls when it has closed: its result, its mode, the button that closed it and
   the user data it was made with */
typedef void (*MsgDialogFunction)(SceInt32 result, SceMsgDialogMode mode, SceInt32 button,
				  void *userdata);

/* the plugin (msg_dialog_plugin.cpp), which the dialog opens its page from */
extern paf::Plugin *g_msgDialogPlugin;

/* the checks of the caller's parameter (msg_dialog_param.cpp) */
void MsgDialogPrintParamError(void);
int MsgDialogParamCheck(const SceMsgDialogParam *param);
int MsgDialogMsgCheck(SceMsgDialogProgressBarTarget target, const SceChar8 *msg);

/* what a dialog shows: its text, its layout and the sound it opens with (0: none) */
struct MsgDialogContent {
	paf::wstring text;
	CommonGuiDialogLayout layout;
	SceInt32 sound;
};

/* the one dialog the plugin has open (msg_dialog.cpp) */
class MsgDialog {
public:
	MsgDialog(SceMsgDialogMode mode, MsgDialogFunction callback, void *userdata);
	~MsgDialog();

	int OpenLayout(const paf::wstring &text, const CommonGuiDialogLayout *layout);
	int OpenErrorCode(SceInt32 errorCode);
	/* sets the progress bar's value, with and without its animation */
	void ProgInc(SceMsgDialogProgressBarTarget target, SceUInt32 rate);
	void SetValue(SceMsgDialogProgressBarTarget target, SceUInt32 rate);
	void ResetProgress(void);
	void SetMsg(SceMsgDialogProgressBarTarget target, const SceChar8 *msg);
	void SetButtonText(paf::ui::Widget *button, const SceChar8 *msg, SceInt32 fontSize);
	void SetButtonMsg(const SceMsgDialogParam *param, const SceMsgDialogButtonsParam *buttonParam);

	static int Create(const SceMsgDialogParam *param, MsgDialogFunction callback, void *userdata);
	static int Abort(bool soon);
	static int Destroy(void);
	static MsgDialog *Get(void);

	MsgDialogFunction m_callback;
	void *m_userdata;
	SceMsgDialogMode m_mode;
	paf::ui::Scene *m_scene;
	/* the common GUI dialog (SceCommonGuiDialog), -1 while none is open */
	SceInt32 m_dialogId;
	/* the dialog has opened (its open handler ran): its progress bar takes values from then */
	SceInt32 m_opened;
	SceUInt32 m_progressValue;
	SceInt32 m_reserved;
	/* the button that closed the dialog, for the callback */
	SceInt32 m_button;
};

/* the common functions the plugin shares with the system software's applications
   (msg_dialog_util.cpp) */
int MsgDialogCheckSdkVersion(SceUInt32 sdkVersion);
int MsgDialogGetSdkLevel(void);
void MsgDialogFormatSize(paf::wstring *out, SceUInt32 sizeKiB);
paf::wstring MsgDialogFormat(const wchar_t *format, const wchar_t *arg1 = L"",
			     const wchar_t *arg2 = L"", const wchar_t *arg3 = L"",
			     const wchar_t *arg4 = L"", const wchar_t *arg5 = L"",
			     const wchar_t *arg6 = L"", const wchar_t *arg7 = L"",
			     const wchar_t *arg8 = L"", const wchar_t *arg9 = L"",
			     const wchar_t *arg10 = L"", const wchar_t *arg11 = L"");

#endif
