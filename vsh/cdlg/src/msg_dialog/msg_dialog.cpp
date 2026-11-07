/*
 * SceMsgDialogPlugin's dialog: the message dialog the caller's parameter describes, shown as a
 * common GUI dialog (SceCommonGuiDialog) on the plugin's page. A user message, a system message
 * or a progress bar takes a layout dialog with the text, layout and sound its mode gives; an error
 * code takes the library's message dialog. The dialog calls the plugin back when a button closes
 * it or the caller aborts it. The class, its methods and the file's name are ours.
 */
#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include <message_dialog.h>
#include <apputil.h>
#include <liblocation.h>
#include <rtc.h>
#include <np/np_error.h>
#include <screenshot/error.h>
#include <photo_export/error.h>
#include <sysmodule/sysmodule.h>
#include <appmgr/appmgr.h>
#include <apputil/apputil_internal.h>
#include <av_config/av_config.h>
#include <liblocation/liblocation_internal.h>
#include <std/stdio.h>
#include <std/string.h>
#include <std/string>
#include <framework.h>
#include <plugin.h>
#include <versatileparam/idparam.h>
#include <common/char_encoding.h>
#include <common/encoded_string.h>
#include <datetime/datetime.h>
#include <timer/timer.h>
#include <math/math.h>
#include <graphics/drawobj/text_obj.h>
#include <widget/core/handler.h>
#include <widget/core/widget.h>
#include <widget/w_scene.h>
#include <widget/w_button.h>
#include <widget/w_progress_bar.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include "../../../app/common/src/np/np_common_ps4/np_common_ps4.h"
#include "msg_dialog_plugin.h"

using namespace paf;

/* the dialog's widgets, by their index in the layout (SceCommonGuiDialog_8947FC23) */
#define MSG_DIALOG_WIDGET_BUTTON1	12
#define MSG_DIALOG_WIDGET_BUTTON2	13
#define MSG_DIALOG_WIDGET_BUTTON3	14
#define MSG_DIALOG_WIDGET_PROGRESS_BAR	18

/* the button text's draw object */
#define MSG_DIALOG_BUTTON_TEXT_OBJ	1
/* a button text wider than this is fitted to its button */
#define MSG_DIALOG_BUTTON_TEXT_WIDTH	606.0f

/* the text of a dialog that waits (the user message's empty text too) */
#define MSG_DIALOG_MSG_WAIT		0x632B6733

/* the error code dialog's texts */
#define MSG_DIALOG_MSG_TITLE_BANNED		0x4805A290
#define MSG_DIALOG_MSG_TITLE_BANNED_UNTIL	0x6EBE5C6E
#define MSG_DIALOG_MSG_UNLOCK_DENIED		0x5C2163B5
#define MSG_DIALOG_MSG_MEDIA_FULL		0xE2A2DDCC
#define MSG_DIALOG_MSG_CONTENT_FULL		0xE5EA6CFC
#define MSG_DIALOG_MSG_CONTENT_FULL_DETAIL	0x93777E84
#define MSG_DIALOG_MSG_NO_MEDIA			0xDC27698F

/* the location messages: the data could not be obtained, with location data off, with no
   network, or otherwise; each without and with a retry button */
#define MSG_DIALOG_MSG_LOCATION_DISABLED		0x8C381E9E
#define MSG_DIALOG_MSG_LOCATION_DISABLED_RETRY		0xA249FC93
#define MSG_DIALOG_MSG_LOCATION_NO_NETWORK		0xA7BB34D9
#define MSG_DIALOG_MSG_LOCATION_NO_NETWORK_RETRY	0x1F2B913E
#define MSG_DIALOG_MSG_LOCATION_FAILURE			0x7EBE715A
#define MSG_DIALOG_MSG_LOCATION_FAILURE_RETRY		0xBCCBBDC5

/* the location service's state bits (SceLibLocation_C1CFEC92) */
#define MSG_DIALOG_LOCATION_NO_NETWORK		0x2
#define MSG_DIALOG_LOCATION_DISABLED		0x4

/* the sound the location messages open with */
#define MSG_DIALOG_SOUND_LOCATION		13

/* how long a closed dialog waits before calling back, and an abort: soon, a progress bar's, and
   any other's (milliseconds) */
#define MSG_DIALOG_CLOSE_TIME		400.0f
#define MSG_DIALOG_ABORT_TIME_SOON	16.66f
#define MSG_DIALOG_ABORT_TIME_PROGRESS	600.0f

/* the mount point a save data message is about */
#define MSG_DIALOG_SAVEDATA_DRIVE	"savedata0:"
/* the media types a save data message is shown for as it is (a memory card's, the game card's) */
#define MSG_DIALOG_MEDIA_TYPE_GAME_CARD		1
#define MSG_DIALOG_MEDIA_TYPE_MEMORY_CARD	4

/* the space a save data message asks for when the system does not say (KiB) */
#define MSG_DIALOG_SAVEDATA_DEFAULT_SIZE	2048

/* the widget of the dialog with the index, when it is a button */
#define MSG_DIALOG_FIND_BUTTON(dialogId, index)						\
	(SceCommonGuiDialog_8947FC23((dialogId), (index)) != NULL &&				\
	 SceCommonGuiDialog_8947FC23((dialogId), (index))->IsInherit(ui::Button::TypeName()) ?	\
	 SceCommonGuiDialog_8947FC23((dialogId), (index)) : NULL)

/* a widget's text draw object, when it is a text object */
#define MSG_DIALOG_TEXT_OBJ(widget)							\
	((widget)->GetDrawObj(MSG_DIALOG_BUTTON_TEXT_OBJ) != NULL &&			\
	 (widget)->GetDrawObj(MSG_DIALOG_BUTTON_TEXT_OBJ)->IsInherit(			\
		graph::TextObj::DrawObjType()) ?					\
	 (graph::TextObj *)(widget)->GetDrawObj(MSG_DIALOG_BUTTON_TEXT_OBJ) : NULL)

/* shows and hides a widget over the timer's time (inline in paf's callers) */
static inline void ShowWidget(ui::Widget *widget, Timer *timer)
{
	if (!widget->m_transition_complete) {
		widget->m_transition = 0xff;
		widget->m_transition_complete = true;
		widget->DoTransition_core(0.0f, true);
	}
	ScePafWidget_FF5BC957(widget, timer, 2, 0.0f, NULL);
}

static inline void HideWidget(ui::Widget *widget, Timer *timer)
{
	widget->m_transition_complete = false;
	widget->m_transition = 0xff;
	widget->DoTransitionReverse_core(timer != NULL ? timer->GetTotalTime() : 0.0f, true);
	ScePafWidget_4588699E(widget, timer, 2, 0.0f, NULL);
}

/* what a layout dialog calls with its button: the dialog closes, and calls back after a while */
class MsgDialogLayoutHandler : public CommonGuiDialogHandler {
public:
	MsgDialogLayoutHandler(MsgDialog *dialog, MsgDialogFunction callback, void *userdata)
		: m_dialog(dialog), m_callback(callback), m_userdata(userdata)
	{
	}

	virtual void operator()(SceInt32 value, SceInt32 button);

	MsgDialog *m_dialog;
	MsgDialogFunction m_callback;
	void *m_userdata;
};

/* the same for the error code's message dialog, which lets itself go */
class MsgDialogMessageHandler : public CommonGuiDialogHandler {
public:
	MsgDialogMessageHandler(MsgDialog *dialog, MsgDialogFunction callback, void *userdata)
		: m_dialog(dialog), m_callback(callback), m_userdata(userdata)
	{
	}

	virtual void operator()(SceInt32 value, SceInt32 button);

	MsgDialog *m_dialog;
	MsgDialogFunction m_callback;
	void *m_userdata;
};

/* what a layout dialog calls as it opens: its progress bar takes values from then */
class MsgDialogLayoutOpenHandler : public CommonGuiLayoutDialogOpenHandler {
public:
	MsgDialogLayoutOpenHandler(MsgDialog *dialog, void *userdata)
		: m_dialog(dialog), m_userdata(userdata)
	{
	}

	virtual void operator()();

	MsgDialog *m_dialog;
	void *m_userdata;
};

/* what the error code's dialog calls as it opens: the text of the errors that have their own */
class MsgDialogMessageOpenHandler : public CommonGuiMessageDialogOpenHandler {
public:
	MsgDialogMessageOpenHandler(MsgDialog *dialog, void *userdata)
		: m_dialog(dialog), m_userdata(userdata)
	{
	}

	virtual void operator()(CommonGuiMessageDialogParam *param);

	MsgDialog *m_dialog;
	void *m_userdata;
};

/* a system message's text and sound */
struct MsgDialogSysMsg {
	SceUInt32 textId;
	SceInt32 sound;
};

/* the system messages' entries in s_sysMsgTable */
enum {
	MSG_DIALOG_SYSMSG_WAIT,
	MSG_DIALOG_SYSMSG_NOSPACE,
	MSG_DIALOG_SYSMSG_NOSPACE_CONTINUABLE,
	MSG_DIALOG_SYSMSG_NEED_MC_CONTINUE,
	MSG_DIALOG_SYSMSG_NEED_MC_OPERATION,
	MSG_DIALOG_SYSMSG_MAGNETIC_CALIBRATION,
	MSG_DIALOG_SYSMSG_LOCATION_DATA_OBTAINING,
	MSG_DIALOG_SYSMSG_MIC_DISABLED,
	MSG_DIALOG_SYSMSG_TRC_WIFI_REQUIRED_OPERATION,
	MSG_DIALOG_SYSMSG_TRC_WIFI_REQUIRED_APPLICATION,
	MSG_DIALOG_SYSMSG_TRC_EMPTY_STORE,
	MSG_DIALOG_SYSMSG_TRC_PSN_AGE_RESTRICTION,
	MSG_DIALOG_SYSMSG_TRC_PSN_CHAT_RESTRICTION,
	MSG_DIALOG_SYSMSG_PATCH_FOUND,
	MSG_DIALOG_SYSMSG_PATCH_NOT_FOUND,
	MSG_DIALOG_SYSMSG_NO_AUDIO_DEVICE,
	MSG_DIALOG_SYSMSG_MIC_DISABLED_CONTINUABLE,
	MSG_DIALOG_SYSMSG_COUNT
};

/* a TRC system message's name in the notice the dialog sends; {0, ""} ends the table */
struct MsgDialogTrcMsg {
	SceMsgDialogSystemMessageType type;
	const char *name;
};

/* the user message's 3-button layout: its page and buttons (its style is the library's
   two-button one) */
static const CommonGuiDialogButton s_3ButtonsButtons[] = {
	{ MSG_DIALOG_WIDGET_BUTTON1, 0x805BECA6, 2, 0x20413274 },
	{ MSG_DIALOG_WIDGET_BUTTON2, 0xADCD9ADF, 3, 0x20413274 },
	{ MSG_DIALOG_WIDGET_BUTTON3, 0x9354DFA7, 6, 0x20413274 },
	{ 0, 0, 0, 0 },
};

static const CommonGuiDialogPage s_3ButtonsPage = {
	{ 0x3A918C2C, 0x438C7285, 0xD5446F76 }
};

/* the magnetic calibration message's layout: a cancel button */
static const CommonGuiDialogPage s_magneticPage = {
	{ 0x3A918C2C, 0x438C7285, 0x722EAC4F }
};

static const CommonGuiDialogButton s_magneticButtons[] = {
	{ MSG_DIALOG_WIDGET_BUTTON1, 0xADCD9ADF, 3, 0xC17D7980 },
	{ 0, 0, 0, 0 },
};

static CommonGuiDialogStyle s_magneticStyle[] = {
	{ 1, IDParam((uint32_t)0xD0791765) },
	{ MSG_DIALOG_WIDGET_BUTTON1, IDParam((uint32_t)0x5049F5E4) },
	{ 0, IDParam() },
};

static const CommonGuiDialogLayout s_magneticLayout = {
	&s_magneticPage, s_magneticStyle, s_magneticButtons
};

/* the location failure message's layout with a retry button: cancel and retry */
static const CommonGuiDialogPage s_locationRetryPage = {
	{ 0x3A918C2C, 0x438C7285, 0x72F4CDB2 }
};

static const CommonGuiDialogButton s_locationRetryButtons[] = {
	{ MSG_DIALOG_WIDGET_BUTTON1, 0xADCD9ADF, 3, 0xC17D7980 },
	{ MSG_DIALOG_WIDGET_BUTTON2, 0x9354DFA7, 6, 0x20413274 },
	{ 0, 0, 0, 0 },
};

static CommonGuiDialogStyle s_locationRetryStyle[] = {
	{ 1, IDParam((uint32_t)0xD0791765) },
	{ MSG_DIALOG_WIDGET_BUTTON1, IDParam((uint32_t)0x5049F5E4) },
	{ MSG_DIALOG_WIDGET_BUTTON2, IDParam((uint32_t)0xBD559A97) },
	{ 0, IDParam() },
};

static const CommonGuiDialogLayout s_locationRetryLayout = {
	&s_locationRetryPage, s_locationRetryStyle, s_locationRetryButtons
};

/* the patch message's layout: a yes button */
static const CommonGuiDialogPage s_patchPage = {
	{ 0x3A918C2C, 0x438C7285, 0x0F8CF7F4 }
};

static const CommonGuiDialogButton s_patchButtons[] = {
	{ MSG_DIALOG_WIDGET_BUTTON1, 0x805BECA6, 2, 0x20413274 },
	{ 0, 0, 0, 0 },
};

static CommonGuiDialogStyle s_patchStyle[] = {
	{ 1, IDParam((uint32_t)0xD0791765) },
	{ MSG_DIALOG_WIDGET_BUTTON1, IDParam((uint32_t)0x5049F5E4) },
	{ 0, IDParam() },
};

static const CommonGuiDialogLayout s_patchLayout = {
	&s_patchPage, s_patchStyle, s_patchButtons
};

static MsgDialog *s_dialog = NULL;

static MsgDialogSysMsg s_sysMsgTable[MSG_DIALOG_SYSMSG_COUNT] = {
	{ MSG_DIALOG_MSG_WAIT, 0 },
	{ 0x5BCB9248, 13 },
	{ 0xF9106F83, 13 },
	{ 0x168C16F5, 13 },
	{ 0x1AC16F95, 13 },
	{ 0xB639F7A7, 13 },
	{ 0x49E1FB6D, 0 },
	{ 0x12D2FCCA, 13 },
	{ 0x43A87C3C, 13 },
	{ 0xC29786D5, 13 },
	{ 0xE0B62D22, 13 },
	{ 0x8F07A8DC, 13 },
	{ 0xA1C22C6D, 13 },
	{ 0x16C20204, 13 },
	{ 0x3B975349, 13 },
	{ 0xB2BAFC8B, 13 },
	{ 0x9E608CE0, 13 },
};

static MsgDialogTrcMsg s_trcMsgTable[] = {
	{ SCE_MSG_DIALOG_SYSMSG_TYPE_TRC_MIC_DISABLED, "MIC DISABLED" },
	{ SCE_MSG_DIALOG_SYSMSG_TYPE_TRC_MIC_DISABLED_CONTINUABLE, "MIC DISABLED CONTINUABLE" },
	{ SCE_MSG_DIALOG_SYSMSG_TYPE_MIC_DISABLED, "MIC DISABLED" },
	{ SCE_MSG_DIALOG_SYSMSG_TYPE_TRC_WIFI_REQUIRED_OPERATION, "WIFI REQUIRED OPERATION" },
	{ SCE_MSG_DIALOG_SYSMSG_TYPE_TRC_WIFI_REQUIRED_APPLICATION, "WIFI REQUIRED APPLICATION" },
	{ SCE_MSG_DIALOG_SYSMSG_TYPE_TRC_EMPTY_STORE, "EMPTY STORE" },
	{ SCE_MSG_DIALOG_SYSMSG_TYPE_TRC_PSN_AGE_RESTRICTION, "PSN AGE RESTRICTION" },
	{ SCE_MSG_DIALOG_SYSMSG_TYPE_TRC_PSN_CHAT_RESTRICTION, "PSN CHAT RESTRICTION" },
	{ SCE_MSG_DIALOG_SYSMSG_TYPE_NOSPACE, "NOSPACE" },
	{ SCE_MSG_DIALOG_SYSMSG_TYPE_NOSPACE_CONTINUABLE, "NOSPC_CONTINUABLE" },
	{ SCE_MSG_DIALOG_SYSMSG_TYPE_INVALID, "" },
};

static void OnClosed(int32_t type, ui::Handler *self, ui::Event *e, void *userdata);
static void OnAbort(int32_t type, ui::Handler *self, ui::Event *e, void *userdata);
static void OnTextReady(int32_t type, ui::Handler *self, ui::Event *e, void *userdata);

/* the dialog opens the plugin's page */
MsgDialog::MsgDialog(SceMsgDialogMode mode, MsgDialogFunction callback, void *userdata)
	: m_callback(callback), m_userdata(userdata), m_mode(mode), m_scene(NULL), m_dialogId(-1),
	  m_opened(0), m_progressValue(0), m_reserved(0), m_button(-1)
{
	Plugin::PageOpenParam openParam;

	m_scene = g_msgDialogPlugin->PageOpen(IDParam(s_empty_name, MSG_DIALOG_PAGE_ID), openParam);
	if (openParam.timer != NULL)
		delete openParam.timer;
}

MsgDialog::~MsgDialog()
{
	if (m_scene != NULL) {
		g_msgDialogPlugin->PageClose(IDParam(s_empty_name, MSG_DIALOG_PAGE_ID),
					     Plugin::PageCloseParam());
		m_scene = NULL;
	}
}

/* opens a layout dialog with the text in place of the one open: its ID */
int MsgDialog::OpenLayout(const wstring &text, const CommonGuiDialogLayout *layout)
{
	if (m_dialogId != -1) {
		SceCommonGuiDialog_BC311DC0(m_dialogId);
		m_dialogId = -1;
	}
	CommonGuiLayoutDialogParam param;

	param.resourcePlugin = g_msgDialogPlugin;
	param.text = text;
	param.layout = *layout;
	memset(param.dimmerColor, 0, sizeof(param.dimmerColor));
	param.openHandler = new MsgDialogLayoutOpenHandler(this, m_userdata);
	param.handler = new MsgDialogLayoutHandler(this, m_callback, m_userdata);
	m_dialogId = SceCommonGuiDialog_CCFCD9A6(&param);
	return m_dialogId;
}

/* opens the message dialog of an error code in place of the one open, and tells the shell: its
   ID */
int MsgDialog::OpenErrorCode(SceInt32 errorCode)
{
	CommonGuiMessageDialogParam param;
	AppmgrNotificationRequest request;

	param.type = errorCode;
	param.option.flags = 1;
	param.buttonCount = 1;
	if (m_dialogId != -1) {
		SceCommonGuiDialog_BC311DC0(m_dialogId);
		m_dialogId = -1;
	}
	sce_paf_memset(&request, 0, sizeof(request));
	sce_paf_snprintf((char *)request.payload, sizeof(request.payload),
			 "MsgDialog:\nError Code - 0x%x", errorCode);
	sceAppMgrSendNotificationRequest(&request, 1);
	param.resourcePlugin = g_msgDialogPlugin;
	memset(param.dimmerColor, 0, sizeof(param.dimmerColor));
	param.openHandler = new MsgDialogMessageOpenHandler(this, m_userdata);
	param.handler = new MsgDialogMessageHandler(this, m_callback, m_userdata);
	m_dialogId = SceCommonGuiDialog_20B141F1(&param);
	return m_dialogId;
}

void MsgDialog::ProgInc(SceMsgDialogProgressBarTarget target, SceUInt32 rate)
{
	if (target == SCE_MSG_DIALOG_PROGRESSBAR_TARGET_BAR_DEFAULT) {
		ui::ProgressBar *bar = (ui::ProgressBar *)SceCommonGuiDialog_8947FC23(
			m_dialogId, MSG_DIALOG_WIDGET_PROGRESS_BAR);

		m_progressValue = rate;
		if (bar != NULL && m_opened == 1)
			bar->SetValueAsync((int32_t)rate, true, ui::ProgressBar::BAR_FORE);
	}
}

void MsgDialog::SetValue(SceMsgDialogProgressBarTarget target, SceUInt32 rate)
{
	if (target == SCE_MSG_DIALOG_PROGRESSBAR_TARGET_BAR_DEFAULT) {
		ui::ProgressBar *bar = (ui::ProgressBar *)SceCommonGuiDialog_8947FC23(
			m_dialogId, MSG_DIALOG_WIDGET_PROGRESS_BAR);

		m_progressValue = rate;
		if (bar != NULL && m_opened == 1)
			bar->SetValueAsync((int32_t)rate, false, ui::ProgressBar::BAR_FORE);
	}
}

/* the progress bar of a dialog just opened: empty at once, then the value it was given */
__attribute__((noinline)) void MsgDialog::ResetProgress(void)
{
	SetValue(SCE_MSG_DIALOG_PROGRESSBAR_TARGET_BAR_DEFAULT, 0);
	ProgInc(SCE_MSG_DIALOG_PROGRESSBAR_TARGET_BAR_DEFAULT, m_progressValue);
}

/* the dialog's new text: the wait message's when the caller's is empty */
void MsgDialog::SetMsg(SceMsgDialogProgressBarTarget target, const SceChar8 *msg)
{
	common::EncodedString encoded;
	wstring text(ScePafMisc_53948F03(ScePafMisc_C11CB5E4(&encoded, (const char *)msg, 1)));

	ScePafMisc_B45A7521(&encoded);
	if (text.length() == 0) {
		IDParam id(s_empty_name, MSG_DIALOG_MSG_WAIT);
		common::EncodedString wait;

		text = ScePafMisc_53948F03(ScePafMisc_DCBA2BD4(&wait, g_msgDialogPlugin->GetString(id),
							      2));
		ScePafMisc_B45A7521(&wait);
	}
	if (target == SCE_MSG_DIALOG_PROGRESSBAR_TARGET_BAR_DEFAULT)
		SceCommonGuiDialog_273A476A(m_dialogId, text);
}

/* a button's own text in the font size given; the button shows once its text is laid out */
void MsgDialog::SetButtonText(ui::Widget *button, const SceChar8 *msg, SceInt32 fontSize)
{
	graph::TextObj *obj = MSG_DIALOG_TEXT_OBJ(button);

	if (obj == NULL)
		return;
	obj->SetStyleAttribute(graph::TextStyleAttribute_Point, 0, 0,
			       fontSize == SCE_MSG_DIALOG_FONT_SIZE_SMALL ? 20.0f : 28.0f);
	wstring text;

	common::Utf8ToUtf16(string((const char *)msg), &text, 8);
	obj->SetString(text.c_str());
	button->AddEventListener(ui::Handler::CB_STATE_READY,
				 new ui::EventCBListener(OnTextReady, NULL));
	HideWidget(button, NULL);
}

/* a 3-button user message's buttons take the caller's texts */
void MsgDialog::SetButtonMsg(const SceMsgDialogParam *param,
			     const SceMsgDialogButtonsParam *buttonParam)
{
	ui::Widget *button;

	if (param == NULL || buttonParam == NULL)
		return;
	if (param->mode != SCE_MSG_DIALOG_MODE_USER_MSG || param->userMsgParam == NULL ||
	    param->userMsgParam->buttonType != SCE_MSG_DIALOG_BUTTON_TYPE_3BUTTONS)
		return;
	button = MSG_DIALOG_FIND_BUTTON(m_dialogId, MSG_DIALOG_WIDGET_BUTTON1);
	if (button != NULL)
		SetButtonText(button, buttonParam->msg1, buttonParam->fontSize1);
	button = MSG_DIALOG_FIND_BUTTON(m_dialogId, MSG_DIALOG_WIDGET_BUTTON2);
	if (button != NULL)
		SetButtonText(button, buttonParam->msg2, buttonParam->fontSize2);
	button = MSG_DIALOG_FIND_BUTTON(m_dialogId, MSG_DIALOG_WIDGET_BUTTON3);
	if (button != NULL)
		SetButtonText(button, buttonParam->msg3, buttonParam->fontSize3);
}

/* the dialog has closed: the plugin hears with which button */
static void OnClosed(int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
{
	MsgDialog *dialog = (MsgDialog *)userdata;

	if (dialog != NULL && dialog->m_callback != NULL)
		dialog->m_callback(SCE_OK, dialog->m_mode, dialog->m_button, dialog->m_userdata);
}

/* the caller aborted: the dialog goes, and the plugin hears with no button */
static void OnAbort(int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
{
	MsgDialog *dialog = (MsgDialog *)userdata;

	if (dialog != NULL) {
		SceCommonGuiDialog_BC311DC0(dialog->m_dialogId);
		dialog->m_dialogId = -1;
		if (dialog->m_callback != NULL)
			dialog->m_callback(SCE_OK, dialog->m_mode, -1, dialog->m_userdata);
	}
}

/* a button's text is laid out: a text too wide is fitted, and the button shows */
static void OnTextReady(int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
{
	ui::Widget *button = (ui::Widget *)self;
	graph::TextObj *obj;

	if (button == NULL)
		return;
	button->DeleteEventCallback(ui::Handler::CB_STATE_READY, NULL, NULL);
	obj = MSG_DIALOG_TEXT_OBJ(button);
	if (obj != NULL) {
		math::v2 bounds;

		obj->GetBounds(bounds);
		if (bounds.extract_x() > MSG_DIALOG_BUTTON_TEXT_WIDTH) {
			obj->SetLayoutAttribute(graph::TextLayoutAttribute_AdjustX, false);
			obj->Update();
		}
	}
	ShowWidget(button, NULL);
}

void MsgDialogLayoutHandler::operator()(SceInt32 value, SceInt32 button)
{
	if (m_dialog->m_dialogId == value) {
		SceCommonGuiDialog_BC311DC0(value);
		m_dialog->m_dialogId = -1;
	}
	if (m_dialog->m_scene != NULL) {
		m_dialog->m_button = button;
		m_dialog->m_scene->SetTimeoutEvent(1, MSG_DIALOG_CLOSE_TIME, OnClosed, m_dialog);
	}
}

void MsgDialogMessageHandler::operator()(SceInt32 value, SceInt32 button)
{
	if (m_dialog->m_scene != NULL) {
		m_dialog->m_button = button;
		m_dialog->m_scene->SetTimeoutEvent(1, MSG_DIALOG_CLOSE_TIME, OnClosed, m_dialog);
	}
}

void MsgDialogLayoutOpenHandler::operator()()
{
	if (s_dialog != NULL) {
		m_dialog->m_opened = 1;
		if (m_dialog->m_mode == SCE_MSG_DIALOG_MODE_PROGRESS_BAR)
			m_dialog->ResetProgress();
	}
}

void MsgDialogMessageOpenHandler::operator()(CommonGuiMessageDialogParam *param)
{
	if (s_dialog == NULL)
		return;
	if (param != NULL && g_msgDialogPlugin != NULL) {
		switch (param->type) {
		case (SceInt32)SCE_PHOTO_EXPORT_ERROR_CONTENT_FULL:
			param->title = g_msgDialogPlugin->GetString(
				IDParam(s_empty_name, MSG_DIALOG_MSG_CONTENT_FULL));
			param->titleLines.push_back(g_msgDialogPlugin->GetString(
				IDParam(s_empty_name, MSG_DIALOG_MSG_CONTENT_FULL_DETAIL)));
			param->buttonCount = 0;
			param->defaultButton = 1;
			break;
		case SCE_APPUTIL_ERROR_PHOTO_DEVICE_NOT_FOUND:
		case SCE_APPUTIL_INTERNAL_ERROR_DEVICE_NOT_FOUND:
		case SCE_APPUTIL_ERROR_MUSIC_DEVICE_NOT_FOUND:
		case (SceInt32)SCE_PHOTO_EXPORT_ERROR_MEDIA_NOT_EXIST:
			param->title = g_msgDialogPlugin->GetString(
				IDParam(s_empty_name, MSG_DIALOG_MSG_NO_MEDIA));
			param->buttonCount = 0;
			param->defaultButton = 1;
			break;
		case (SceInt32)SCE_SCREENSHOT_ERROR_MEDIA_FULL:
		case (SceInt32)SCE_PHOTO_EXPORT_ERROR_MEDIA_FULL:
			param->title = g_msgDialogPlugin->GetString(
				IDParam(s_empty_name, MSG_DIALOG_MSG_MEDIA_FULL));
			param->buttonCount = 0;
			param->defaultButton = 1;
			break;
		case SCE_NP_TROPHY_ERROR_UNLOCK_DENIED:
			param->title = g_msgDialogPlugin->GetString(
				IDParam(s_empty_name, MSG_DIALOG_MSG_UNLOCK_DENIED));
			param->buttonCount = 0;
			param->defaultButton = 1;
			break;
		case SCE_NP_WEBAPI_ERROR_TITLE_IS_BANNED:
			param->title = g_msgDialogPlugin->GetString(
				IDParam(s_empty_name, MSG_DIALOG_MSG_TITLE_BANNED));
			param->buttonCount = 0;
			param->defaultButton = 0;
			if (sceSysmoduleIsLoaded(SCE_SYSMODULE_NP_WEBAPI) >= 0) {
				SceUInt8 isSet;
				SceRtcTick utc;
				wstring date;

				if (SceNpCommonPs4_3C570D94(&isSet, &utc) == SCE_OK && isSet) {
					SceRtcTick local;
					int ret;

					param->title = g_msgDialogPlugin->GetString(
						IDParam(s_empty_name, MSG_DIALOG_MSG_TITLE_BANNED_UNTIL));
					param->title += L"\n\n";
					local = utc;
					/* the ban's end as a local date (Sony's code tests the result twice:
					   the date's own check, then this one) */
					ret = sceRtcConvertUtcToLocalTime(&utc, &local);
					if (ret >= 0) {
						wchar_t buf[36];
						datetime::DateTime dateTime =
							datetime::DateTime::ConvertRtcTickToDateTime(&local.tick);

						dateTime.Format(buf, 36, L"%c", false);
						date = buf;
					}
					if (ret >= 0)
						param->title.append(date);
					else
						param->title = g_msgDialogPlugin->GetString(
							IDParam(s_empty_name, MSG_DIALOG_MSG_TITLE_BANNED));
				}
			}
			break;
		}
	}
	m_dialog->m_opened = 1;
	if (m_dialog->m_mode == SCE_MSG_DIALOG_MODE_PROGRESS_BAR)
		m_dialog->ResetProgress();
}

/* a location failure message: its text by what the location service says */
static __attribute__((noinline)) int SysMsgLocation(SceMsgDialogSystemMessageType type, MsgDialogContent *content)
{
	SceUInt32 state = 0;
	SceUInt32 textId;
	bool retry;
	int ret;

	switch (type) {
	case SCE_MSG_DIALOG_SYSMSG_TYPE_LOCATION_DATA_FAILURE:
		retry = false;
		break;
	case SCE_MSG_DIALOG_SYSMSG_TYPE_LOCATION_DATA_FAILURE_RETRY:
		retry = true;
		break;
	default:
		return MSG_DIALOG_PLUGIN_ERROR_INVALID;
	}
	if (sceSysmoduleIsLoaded(SCE_SYSMODULE_LOCATION) < 0) {
		ret = sceSysmoduleIsLoaded(SCE_SYSMODULE_LOCATION_EXTENSION);
		if (ret < 0) {
			if (ret == SCE_SYSMODULE_ERROR_UNLOADED)
				ret = SCE_MSG_DIALOG_ERROR_MODULE;
			return ret;
		}
	}
	ret = SceLibLocation_C1CFEC92(&state);
	if (ret < 0) {
		if (ret == SCE_LOCATION_ERROR_UNINITIALIZED)
			ret = SCE_MSG_DIALOG_ERROR_MODULE;
		return ret;
	}
	if (state & MSG_DIALOG_LOCATION_DISABLED)
		textId = retry ? MSG_DIALOG_MSG_LOCATION_DISABLED_RETRY : MSG_DIALOG_MSG_LOCATION_DISABLED;
	else if (state & MSG_DIALOG_LOCATION_NO_NETWORK)
		textId = retry ? MSG_DIALOG_MSG_LOCATION_NO_NETWORK_RETRY :
				 MSG_DIALOG_MSG_LOCATION_NO_NETWORK;
	else
		textId = retry ? MSG_DIALOG_MSG_LOCATION_FAILURE_RETRY : MSG_DIALOG_MSG_LOCATION_FAILURE;
	content->text = g_msgDialogPlugin->GetString(IDParam(s_empty_name, textId));
	content->layout = *(retry ? &s_locationRetryLayout : &SceCommonGuiDialog_AF75CA6D);
	content->sound = MSG_DIALOG_SOUND_LOCATION;
	return ret;
}

/* a system message's text, layout and sound */
static int SysMsg(const SceMsgDialogParam *param, MsgDialogContent *content)
{
	const SceMsgDialogSystemMessageParam *sysMsgParam = param->sysMsgParam;
	const MsgDialogSysMsg *msg;
	SceUInt32 textId = 0;
	int ret = SCE_OK;

	switch (sysMsgParam->sysMsgType) {
	case SCE_MSG_DIALOG_SYSMSG_TYPE_WAIT:
		textId = s_sysMsgTable[MSG_DIALOG_SYSMSG_WAIT].textId;
		content->layout = SceCommonGuiDialog_F8B2D7E1;
		content->sound = s_sysMsgTable[MSG_DIALOG_SYSMSG_WAIT].sound;
		break;
	case SCE_MSG_DIALOG_SYSMSG_TYPE_WAIT_SMALL:
		textId = s_sysMsgTable[MSG_DIALOG_SYSMSG_WAIT].textId;
		content->layout = SceCommonGuiDialog_2A68B4A4;
		content->sound = s_sysMsgTable[MSG_DIALOG_SYSMSG_WAIT].sound;
		break;
	case SCE_MSG_DIALOG_SYSMSG_TYPE_WAIT_CANCEL:
		textId = s_sysMsgTable[MSG_DIALOG_SYSMSG_WAIT].textId;
		content->layout = SceCommonGuiDialog_F8B2D7E1;
		content->sound = s_sysMsgTable[MSG_DIALOG_SYSMSG_WAIT].sound;
		break;
	case SCE_MSG_DIALOG_SYSMSG_TYPE_NEED_MC_CONTINUE:
		textId = s_sysMsgTable[MSG_DIALOG_SYSMSG_NEED_MC_CONTINUE].textId;
		content->layout = SceCommonGuiDialog_AF75CA6D;
		content->sound = s_sysMsgTable[MSG_DIALOG_SYSMSG_NEED_MC_CONTINUE].sound;
		break;
	case SCE_MSG_DIALOG_SYSMSG_TYPE_NEED_MC_OPERATION:
		textId = s_sysMsgTable[MSG_DIALOG_SYSMSG_NEED_MC_OPERATION].textId;
		content->layout = SceCommonGuiDialog_AF75CA6D;
		content->sound = s_sysMsgTable[MSG_DIALOG_SYSMSG_NEED_MC_OPERATION].sound;
		break;
	case SCE_MSG_DIALOG_SYSMSG_TYPE_NOSPACE:
		msg = &s_sysMsgTable[MSG_DIALOG_SYSMSG_NOSPACE];
		goto noSpace;
	case SCE_MSG_DIALOG_SYSMSG_TYPE_NOSPACE_CONTINUABLE:
		msg = &s_sysMsgTable[MSG_DIALOG_SYSMSG_NOSPACE_CONTINUABLE];
noSpace: {
		SceUInt32 sizeKiB;

		textId = msg->textId;
		content->layout = SceCommonGuiDialog_AF75CA6D;
		content->sound = msg->sound;
		sizeKiB = sysMsgParam->value;
		wstring size(L"");

		if (MsgDialogGetSdkLevel() >= 2) {
			int cached = sceAppMgrSaveDataGetCachedRequiredSizeKiB();

			sizeKiB = MSG_DIALOG_SAVEDATA_DEFAULT_SIZE;
			if (cached >= 0)
				sizeKiB = cached;
		}
		MsgDialogFormatSize(&size, sizeKiB);
		content->text = MsgDialogFormat(g_msgDialogPlugin->GetString(
			IDParam(s_empty_name, textId)), size.c_str());
		return ret;
	}
	case SCE_MSG_DIALOG_SYSMSG_TYPE_MAGNETIC_CALIBRATION:
		textId = s_sysMsgTable[MSG_DIALOG_SYSMSG_MAGNETIC_CALIBRATION].textId;
		content->layout = s_magneticLayout;
		content->sound = s_sysMsgTable[MSG_DIALOG_SYSMSG_MAGNETIC_CALIBRATION].sound;
		break;
	case SCE_MSG_DIALOG_SYSMSG_TYPE_LOCATION_DATA_OBTAINING:
		textId = s_sysMsgTable[MSG_DIALOG_SYSMSG_LOCATION_DATA_OBTAINING].textId;
		content->layout = SceCommonGuiDialog_F8B2D7E1;
		content->sound = s_sysMsgTable[MSG_DIALOG_SYSMSG_LOCATION_DATA_OBTAINING].sound;
		break;
	case SCE_MSG_DIALOG_SYSMSG_TYPE_LOCATION_DATA_FAILURE:
	case SCE_MSG_DIALOG_SYSMSG_TYPE_LOCATION_DATA_FAILURE_RETRY:
		return SysMsgLocation(sysMsgParam->sysMsgType, content);
	case SCE_MSG_DIALOG_SYSMSG_TYPE_PATCH_FOUND: {
		SceInt32 value = 0;
		SceInt32 value2 = 0;

		ret = SceAppUtil_B2FE3BB1(&value, &value2);
		content->layout = s_patchLayout;
		msg = &s_sysMsgTable[MSG_DIALOG_SYSMSG_PATCH_NOT_FOUND];
		if (ret >= 0 && value > 0)
			msg = &s_sysMsgTable[MSG_DIALOG_SYSMSG_PATCH_FOUND];
		textId = msg->textId;
		content->sound = msg->sound;
		ret = SCE_OK;
		break;
	}
	case SCE_MSG_DIALOG_SYSMSG_TYPE_MIC_DISABLED:
	case SCE_MSG_DIALOG_SYSMSG_TYPE_TRC_MIC_DISABLED:
	case SCE_MSG_DIALOG_SYSMSG_TYPE_TRC_MIC_DISABLED_CONTINUABLE: {
		SceUInt32 devices = 0;

		if (sceAVConfigGetConnectedAudioDevice(&devices) != SCE_OK || (devices & 0xFF00) != 0) {
			if (param->sysMsgParam->sysMsgType ==
			    SCE_MSG_DIALOG_SYSMSG_TYPE_TRC_MIC_DISABLED_CONTINUABLE)
				msg = &s_sysMsgTable[MSG_DIALOG_SYSMSG_MIC_DISABLED_CONTINUABLE];
			else
				msg = &s_sysMsgTable[MSG_DIALOG_SYSMSG_MIC_DISABLED];
		} else {
			msg = &s_sysMsgTable[MSG_DIALOG_SYSMSG_NO_AUDIO_DEVICE];
		}
		textId = msg->textId;
		content->sound = msg->sound;
		content->layout = SceCommonGuiDialog_AF75CA6D;
		break;
	}
	case SCE_MSG_DIALOG_SYSMSG_TYPE_TRC_WIFI_REQUIRED_OPERATION:
		textId = s_sysMsgTable[MSG_DIALOG_SYSMSG_TRC_WIFI_REQUIRED_OPERATION].textId;
		content->layout = SceCommonGuiDialog_AF75CA6D;
		content->sound = s_sysMsgTable[MSG_DIALOG_SYSMSG_TRC_WIFI_REQUIRED_OPERATION].sound;
		break;
	case SCE_MSG_DIALOG_SYSMSG_TYPE_TRC_WIFI_REQUIRED_APPLICATION:
		textId = s_sysMsgTable[MSG_DIALOG_SYSMSG_TRC_WIFI_REQUIRED_APPLICATION].textId;
		content->layout = SceCommonGuiDialog_AF75CA6D;
		content->sound = s_sysMsgTable[MSG_DIALOG_SYSMSG_TRC_WIFI_REQUIRED_APPLICATION].sound;
		break;
	case SCE_MSG_DIALOG_SYSMSG_TYPE_TRC_EMPTY_STORE:
		textId = s_sysMsgTable[MSG_DIALOG_SYSMSG_TRC_EMPTY_STORE].textId;
		content->layout = SceCommonGuiDialog_AF75CA6D;
		content->sound = s_sysMsgTable[MSG_DIALOG_SYSMSG_TRC_EMPTY_STORE].sound;
		break;
	case SCE_MSG_DIALOG_SYSMSG_TYPE_TRC_PSN_AGE_RESTRICTION:
		textId = s_sysMsgTable[MSG_DIALOG_SYSMSG_TRC_PSN_AGE_RESTRICTION].textId;
		content->layout = SceCommonGuiDialog_AF75CA6D;
		content->sound = s_sysMsgTable[MSG_DIALOG_SYSMSG_TRC_PSN_AGE_RESTRICTION].sound;
		break;
	case SCE_MSG_DIALOG_SYSMSG_TYPE_TRC_PSN_CHAT_RESTRICTION:
		textId = s_sysMsgTable[MSG_DIALOG_SYSMSG_TRC_PSN_CHAT_RESTRICTION].textId;
		content->layout = SceCommonGuiDialog_AF75CA6D;
		content->sound = s_sysMsgTable[MSG_DIALOG_SYSMSG_TRC_PSN_CHAT_RESTRICTION].sound;
		break;
	default:
		ret = MSG_DIALOG_PLUGIN_ERROR_INVALID;
		break;
	}
	content->text = g_msgDialogPlugin->GetString(IDParam(s_empty_name, textId));
	return ret;
}

/* a user message's text and its buttons' layout */
static __attribute__((noinline)) int UserMsg(const SceMsgDialogParam *param, MsgDialogContent *content)
{
	common::EncodedString encoded;
	SceMsgDialogButtonType buttonType;
	CommonGuiDialogLayout layout;

	content->text = ScePafMisc_53948F03(ScePafMisc_C11CB5E4(
		&encoded, (const char *)param->userMsgParam->msg, 1));
	ScePafMisc_B45A7521(&encoded);
	buttonType = param->userMsgParam->buttonType;
	if (buttonType == SCE_MSG_DIALOG_BUTTON_TYPE_YESNO) {
		layout = SceCommonGuiDialog_6CD802FC;
	} else if (buttonType == SCE_MSG_DIALOG_BUTTON_TYPE_NONE) {
		layout = SceCommonGuiDialog_7AEA192C;
	} else if (buttonType == SCE_MSG_DIALOG_BUTTON_TYPE_OK_CANCEL) {
		layout = SceCommonGuiDialog_92752DE2;
	} else if (buttonType == SCE_MSG_DIALOG_BUTTON_TYPE_CANCEL) {
		layout = SceCommonGuiDialog_F8B2D7E1;
	} else if (buttonType == SCE_MSG_DIALOG_BUTTON_TYPE_3BUTTONS) {
		layout.page = &s_3ButtonsPage;
		layout.style = SceCommonGuiDialog_78399261.style;
		layout.buttons = s_3ButtonsButtons;
	} else {
		layout = SceCommonGuiDialog_AF75CA6D;
	}
	content->layout = layout;
	return SCE_OK;
}

/* a progress bar's text and layout */
static __attribute__((noinline)) int ProgressBar(const SceMsgDialogParam *param, MsgDialogContent *content)
{
	const SceMsgDialogProgressBarParam *progBarParam = param->progBarParam;
	int ret = SCE_OK;

	switch (progBarParam->sysMsgParam.sysMsgType) {
	case SCE_MSG_DIALOG_SYSMSG_TYPE_INVALID: {
		common::EncodedString encoded;

		content->text = ScePafMisc_53948F03(ScePafMisc_C11CB5E4(
			&encoded, (const char *)progBarParam->msg, 1));
		ScePafMisc_B45A7521(&encoded);
		content->layout = SceCommonGuiDialog_A0E0A437;
		break;
	}
	case SCE_MSG_DIALOG_SYSMSG_TYPE_WAIT:
	case SCE_MSG_DIALOG_SYSMSG_TYPE_WAIT_SMALL:
		content->text = g_msgDialogPlugin->GetString(
			IDParam(s_empty_name, s_sysMsgTable[MSG_DIALOG_SYSMSG_WAIT].textId));
		content->layout = SceCommonGuiDialog_A0E0A437;
		break;
	case SCE_MSG_DIALOG_SYSMSG_TYPE_WAIT_CANCEL:
		content->text = g_msgDialogPlugin->GetString(
			IDParam(s_empty_name, s_sysMsgTable[MSG_DIALOG_SYSMSG_WAIT].textId));
		content->layout = SceCommonGuiDialog_446F8B49;
		break;
	default:
		ret = MSG_DIALOG_PLUGIN_ERROR_INVALID;
		break;
	}
	return ret;
}

/* opens the dialog the caller's parameter describes */
int MsgDialog::Create(const SceMsgDialogParam *param, MsgDialogFunction callback, void *userdata)
{
	int ret = SCE_OK;

	if (s_dialog == NULL) {
		s_dialog = new MsgDialog(param->mode, callback, userdata);
		if (s_dialog == NULL)
			return MSG_DIALOG_PLUGIN_ERROR_NO_MEMORY;
	}
	switch (param->mode) {
	case SCE_MSG_DIALOG_MODE_INVALID:
		ret = MSG_DIALOG_PLUGIN_ERROR_INVALID;
		break;
	case SCE_MSG_DIALOG_MODE_USER_MSG: {
		MsgDialogContent content;

		ret = UserMsg(param, &content);
		if (ret < 0)
			break;
		s_dialog->OpenLayout(content.text, &content.layout);
		ret = SCE_OK;
		break;
	}
	case SCE_MSG_DIALOG_MODE_SYSTEM_MSG: {
		MsgDialogContent content;
		char drive[16] = MSG_DIALOG_SAVEDATA_DRIVE;
		int mediaType;
		SceMsgDialogSystemMessageType type;
		const MsgDialogTrcMsg *trc;

		if ((param->sysMsgParam->sysMsgType == SCE_MSG_DIALOG_SYSMSG_TYPE_NOSPACE ||
		     param->sysMsgParam->sysMsgType == SCE_MSG_DIALOG_SYSMSG_TYPE_NOSPACE_CONTINUABLE) &&
		    sceAppMgrGetMediaTypeFromDrive(drive, &mediaType) >= 0 &&
		    mediaType != MSG_DIALOG_MEDIA_TYPE_MEMORY_CARD &&
		    mediaType != MSG_DIALOG_MEDIA_TYPE_GAME_CARD) {
			s_dialog->OpenErrorCode(MSG_DIALOG_PLUGIN_ERROR_NOSPACE_MEDIA);
			break;
		}
		ret = SysMsg(param, &content);
		if (ret < 0)
			break;
		s_dialog->OpenLayout(content.text, &content.layout);
		if (content.sound != 0)
			Framework::Instance()->PlayCommonSE(content.sound, 1.0f, false);
		if (param->sysMsgParam->sysMsgType == SCE_MSG_DIALOG_SYSMSG_TYPE_WAIT &&
		    s_dialog->m_dialogId != -1) {
			ui::Widget *button = SceCommonGuiDialog_8947FC23(s_dialog->m_dialogId,
									 MSG_DIALOG_WIDGET_BUTTON1);

			if (button != NULL)
				HideWidget(button, NULL);
		}
		type = param->sysMsgParam->sysMsgType;
		for (trc = s_trcMsgTable; trc->type != SCE_MSG_DIALOG_SYSMSG_TYPE_INVALID; trc++) {
			if (trc->type == type) {
				AppmgrNotificationRequest request;

				sce_paf_memset(&request, 0, sizeof(request));
				sce_paf_snprintf((char *)request.payload, sizeof(request.payload),
						 "MsgDialog:\nTRC System Message -\n%s", trc->name);
				sceAppMgrSendNotificationRequest(&request, 1);
				break;
			}
		}
		ret = SCE_OK;
		break;
	}
	case SCE_MSG_DIALOG_MODE_ERROR_CODE:
		s_dialog->OpenErrorCode(param->errorCodeParam->errorCode);
		break;
	case SCE_MSG_DIALOG_MODE_PROGRESS_BAR: {
		MsgDialogContent content;

		ret = ProgressBar(param, &content);
		if (ret < 0)
			break;
		s_dialog->OpenLayout(content.text, &content.layout);
		ret = SCE_OK;
		break;
	}
	}
	return ret;
}

/* closes the dialog after a while: soon when the caller asks, later for a progress bar */
int MsgDialog::Abort(bool soon)
{
	MsgDialog *dialog = s_dialog;

	if (dialog != NULL) {
		float timeout = MSG_DIALOG_CLOSE_TIME;

		if (soon)
			timeout = MSG_DIALOG_ABORT_TIME_SOON;
		else if (dialog->m_mode == SCE_MSG_DIALOG_MODE_PROGRESS_BAR)
			timeout = MSG_DIALOG_ABORT_TIME_PROGRESS;
		dialog->m_scene->SetTimeoutEvent(1, timeout, OnAbort, dialog);
	}
	return SCE_OK;
}

int MsgDialog::Destroy(void)
{
	if (s_dialog != NULL) {
		delete s_dialog;
		s_dialog = NULL;
	}
	return SCE_OK;
}

MsgDialog *MsgDialog::Get(void)
{
	return s_dialog;
}
