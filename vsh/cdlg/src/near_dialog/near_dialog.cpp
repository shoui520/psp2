/*
 * SceNearDialogPlugin's dialog: Sony's copy of the message dialog's (libcdlg_msg), shown as a
 * common GUI dialog (SceCommonGuiDialog) on the plugin's page. A user message, a system message or
 * a progress bar takes a layout dialog with the text, layout and sound its type gives; an error
 * code takes the library's message dialog. The check-in's end screen adds its results to the
 * dialog's contents. The dialog calls the plugin back when a button closes it or the caller aborts
 * it. The class, its methods and the file's name are ours.
 */
#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include <appmgr/appmgr.h>
#include <std/stdio.h>
#include <std/string.h>
#include <std/string>
#include <framework.h>
#include <plugin.h>
#include <versatileparam/idparam.h>
#include <common/char_encoding.h>
#include <common/encoded_string.h>
#include <timer/timer.h>
#include <math/math.h>
#include <widget/core/handler.h>
#include <widget/core/widget.h>
#include <widget/w_scene.h>
#include <widget/w_box.h>
#include <widget/w_progress_bar.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include "near_dialog_plugin.h"

using namespace paf;

/* the dialog's widgets, by their index in the layout (SceCommonGuiDialog_8947FC23) */
#define NEAR_DIALOG_WIDGET_CONTENTS	4
#define NEAR_DIALOG_WIDGET_BUTTON1	12
#define NEAR_DIALOG_WIDGET_BUTTON2	13
#define NEAR_DIALOG_WIDGET_PROGRESS_BAR	18

/* the system message that is the check-in's end screen */
#define NEAR_DIALOG_SYSMSG_FINISH	23

/* the end screen's texts: its title, the number of posts, the distances and their units */
#define NEAR_DIALOG_MSG_FINISH_TITLE	0x638B05B1
#define NEAR_DIALOG_MSG_POST_COUNT	0x403AFC51
#define NEAR_DIALOG_MSG_TOTAL_DISTANCE	0xA115FDFE
#define NEAR_DIALOG_MSG_TODAY_DISTANCE	0x4CE3D41C
#define NEAR_DIALOG_MSG_KM		0x32C4E1C0
#define NEAR_DIALOG_MSG_KM_ZERO		0x6CF9BAC8
#define NEAR_DIALOG_MSG_MILES_ZERO	0x77C82257
#define NEAR_DIALOG_MSG_MILE		0x031C477D
#define NEAR_DIALOG_MSG_MILES		0x829F6675

/* the end screen's list: its font size and width, and where it sits in the contents */
#define NEAR_DIALOG_FINISH_FONT_SIZE	24.0f
#define NEAR_DIALOG_FINISH_WIDTH	736.0f
#define NEAR_DIALOG_FINISH_TITLE_WIDTH	660.0f
#define NEAR_DIALOG_FINISH_POS_Y	40.0f
#define NEAR_DIALOG_FINISH_HEIGHT	52.0f

/* a kilometre in miles */
#define NEAR_DIALOG_MILES_PER_KM	0.6214f

/* how long a closed dialog waits before calling back, and an abort: soon, a progress bar's, and
   any other's (milliseconds) */
#define NEAR_DIALOG_CLOSE_TIME		400.0f
#define NEAR_DIALOG_ABORT_TIME_SOON	16.66f
#define NEAR_DIALOG_ABORT_TIME_PROGRESS	600.0f

/* what a layout dialog calls with its button: the dialog closes, and calls back after a while */
class NearDialogLayoutHandler : public CommonGuiDialogHandler {
public:
	NearDialogLayoutHandler(NearDialog *dialog, NearDialogFunction callback, void *userdata)
		: m_dialog(dialog), m_callback(callback), m_userdata(userdata)
	{
	}

	virtual void operator()(SceInt32 value, SceInt32 button);

	NearDialog *m_dialog;
	NearDialogFunction m_callback;
	void *m_userdata;
};

/* the same for the error code's message dialog, which lets itself go */
class NearDialogMessageHandler : public CommonGuiDialogHandler {
public:
	NearDialogMessageHandler(NearDialog *dialog, NearDialogFunction callback, void *userdata)
		: m_dialog(dialog), m_callback(callback), m_userdata(userdata)
	{
	}

	virtual void operator()(SceInt32 value, SceInt32 button);

	NearDialog *m_dialog;
	NearDialogFunction m_callback;
	void *m_userdata;
};

/* what a layout dialog calls as it opens: its progress bar takes values from then */
class NearDialogLayoutOpenHandler : public CommonGuiLayoutDialogOpenHandler {
public:
	NearDialogLayoutOpenHandler(NearDialog *dialog, void *userdata)
		: m_dialog(dialog), m_userdata(userdata)
	{
	}

	virtual void operator()();

	NearDialog *m_dialog;
	void *m_userdata;
};

/* the same for the error code's dialog */
class NearDialogMessageOpenHandler : public CommonGuiMessageDialogOpenHandler {
public:
	NearDialogMessageOpenHandler(NearDialog *dialog, void *userdata)
		: m_dialog(dialog), m_userdata(userdata)
	{
	}

	virtual void operator()(CommonGuiMessageDialogParam *param);

	NearDialog *m_dialog;
	void *m_userdata;
};

/* a system message's text and sound */
struct NearDialogSysMsg {
	SceUInt32 textId;
	SceInt32 sound;
};

/* a TRC system message's name in the notice the dialog sends; {0, ""} ends the table */
struct NearDialogTrcMsg {
	SceInt32 type;
	const char *name;
};

/* the location failure message's layout with a retry button: cancel and retry */
static const CommonGuiDialogPage s_locationRetryPage = {
	{ 0x3A918C2C, 0x438C7285, 0x72F4CDB2 }
};

static const CommonGuiDialogButton s_locationRetryButtons[] = {
	{ NEAR_DIALOG_WIDGET_BUTTON1, 0xADCD9ADF, 3, 0xC17D7980 },
	{ NEAR_DIALOG_WIDGET_BUTTON2, 0x9354DFA7, 6, 0x20413274 },
	{ 0, 0, 0, 0 },
};

static ui::Dialog::PartsList s_locationRetryParts[] = {
	{ 1, IDParam((uint32_t)0xD0791765) },
	{ NEAR_DIALOG_WIDGET_BUTTON1, IDParam((uint32_t)0x5049F5E4) },
	{ NEAR_DIALOG_WIDGET_BUTTON2, IDParam((uint32_t)0xBD559A97) },
	{ 0, IDParam() },
};

static const CommonGuiDialogLayout s_locationRetryLayout = {
	&s_locationRetryPage, s_locationRetryParts, s_locationRetryButtons
};

/* the buttons the library's two-button page takes for the check-in's questions, the gift's and
   the gift's cancel */
static const CommonGuiDialogButton s_confirmButtons[] = {
	{ NEAR_DIALOG_WIDGET_BUTTON1, 0xBED8E7AB, 2, 0x20413274 },
	{ NEAR_DIALOG_WIDGET_BUTTON2, 0xD31A08E6, 3, 0xC17D7980 },
	{ 0, 0, 0, 0 },
};

static const CommonGuiDialogButton s_giftButtons[] = {
	{ NEAR_DIALOG_WIDGET_BUTTON1, 0x5B1C22D8, 2, 0x20413274 },
	{ NEAR_DIALOG_WIDGET_BUTTON2, 0x0B7EA1A0, 3, 0xC17D7980 },
	{ 0, 0, 0, 0 },
};

static const CommonGuiDialogButton s_giftCancelButtons[] = {
	{ NEAR_DIALOG_WIDGET_BUTTON1, 0x0B7EA1A0, 3, 0x20413274 },
	{ 0, 0, 0, 0 },
};

static NearDialog *s_dialog = NULL;

/* the end screen's list, made by the end screen */
static NearFinishInfoList *s_finishInfoList = NULL;

/* the system messages' texts (no sound): each type's entry, and the progress bar's (25) */
static NearDialogSysMsg s_sysMsgTable[] = {
	{ 0x1E66B191, 0 },	/* 0 */
	{ 0x2DE5514A, 0 },
	{ 0x690CFAB8, 0 },
	{ 0xA1E6E987, 0 },
	{ 0xFDBFAA9A, 0 },
	{ 0x03FC5E10, 0 },	/* 5 */
	{ 0x6238C2C8, 0 },
	{ 0x502E0C7F, 0 },
	{ 0x99B0916A, 0 },
	{ 0x82AB838F, 0 },
	{ 0x8907CE0B, 0 },	/* 10 */
	{ 0x7EBE715A, 0 },
	{ 0x1F2B913E, 0 },
	{ 0xA249FC93, 0 },
	{ 0x45089E5E, 0 },
	{ 0x200BC521, 0 },	/* 15 */
	{ 0x7AC4BC69, 0 },
	{ 0x3B1AD304, 0 },
	{ 0x38C7AB60, 0 },
	{ 0xFAFF7775, 0 },
	{ 0x4B2F6059, 0 },	/* 20 */
	{ 0xE3E3FE7D, 0 },
	{ 0x5D1410FF, 0 },
	{ 0xEFCC21CE, 0 },
	{ 0xEC2103B6, 0 },
	{ 0xC673D10D, 0 },	/* 25 */
	{ 0x2095C0A6, 0 },
	{ 0xC51AA4A4, 0 },
	{ 0x0176210C, 0 },
	{ 0xE3B86E16, 0 },
	{ 0x06F853EC, 0 },	/* 30 */
	{ 0xD679B275, 0 },
	{ 0x57F1E4C6, 0 },
	{ NEAR_DIALOG_MSG_FINISH_TITLE, 0 },
	{ NEAR_DIALOG_MSG_POST_COUNT, 0 },
	{ NEAR_DIALOG_MSG_TOTAL_DISTANCE, 0 },	/* 35 */
	{ NEAR_DIALOG_MSG_TODAY_DISTANCE, 0 },
	{ NEAR_DIALOG_MSG_KM, 0 },
	{ NEAR_DIALOG_MSG_KM_ZERO, 0 },
	{ NEAR_DIALOG_MSG_MILES, 0 },
	{ NEAR_DIALOG_MSG_MILE, 0 },		/* 40 */
	{ NEAR_DIALOG_MSG_MILES_ZERO, 0 },
	{ 0xE0B6F62B, 0 },
	{ 0x76040224, 0 },
	{ 0xBED8E7AB, 0 },
	{ 0xD31A08E6, 0 },	/* 45 */
	{ 0x5B1C22D8, 0 },
	{ 0x0B7EA1A0, 0 },
	{ 0xADCD9ADF, 0 },
	{ 0x9354DFA7, 0 },
	{ 0x632B6733, 0 },	/* 50 */
};

static NearDialogTrcMsg s_trcMsgTable[] = {
	{ 0, "" },
};

static void OnClosed(int32_t type, ui::Handler *self, ui::Event *e, void *userdata);
static void OnAbort(int32_t type, ui::Handler *self, ui::Event *e, void *userdata);

/* the dialog opens the plugin's page */
NearDialog::NearDialog(SceInt32 mode, NearDialogFunction callback, void *userdata)
	: m_callback(callback), m_userdata(userdata), m_mode(mode), m_scene(NULL), m_dialogId(-1),
	  m_opened(0), m_progressValue(0), m_reserved(0), m_button(-1)
{
	Plugin::PageOpenParam openParam;

	m_scene = g_nearDialogPlugin->PageOpen(IDParam(s_empty_name, NEAR_DIALOG_PAGE_ID), openParam);
}

/* opens a layout dialog with the text in place of the one open: its ID */
int NearDialog::OpenLayout(const wstring &text, const CommonGuiDialogLayout *layout)
{
	if (m_dialogId != -1) {
		SceCommonGuiDialog_BC311DC0(m_dialogId);
		m_dialogId = -1;
	}
	CommonGuiLayoutDialogParam param;

	param.resourcePlugin = g_nearDialogPlugin;
	param.text = text;
	param.layout = *layout;
	memset(param.dimmerColor, 0, sizeof(param.dimmerColor));
	param.openHandler = new NearDialogLayoutOpenHandler(this, m_userdata);
	param.handler = new NearDialogLayoutHandler(this, m_callback, m_userdata);
	m_dialogId = SceCommonGuiDialog_CCFCD9A6(&param);
	m_opened = 0;
	return m_dialogId;
}

/* opens the message dialog of an error code in place of the one open, and tells the shell: its
   ID */
int NearDialog::OpenErrorCode(SceInt32 errorCode)
{
	CommonGuiMessageDialogParam param;
	AppmgrNotificationRequest request;

	param.type = errorCode;
	param.option.flags = 0x12;
	param.buttonCount = 1;
	if (m_dialogId != -1) {
		SceCommonGuiDialog_BC311DC0(m_dialogId);
		m_dialogId = -1;
	}
	sce_paf_memset(&request, 0, sizeof(request));
	sce_paf_snprintf((char *)request.payload, sizeof(request.payload),
			 "NearDialog:\nError Code - 0x%x", errorCode);
	sceAppMgrSendNotificationRequest(&request, 1);
	param.resourcePlugin = g_nearDialogPlugin;
	memset(param.dimmerColor, 0, sizeof(param.dimmerColor));
	param.openHandler = new NearDialogMessageOpenHandler(this, m_userdata);
	param.handler = new NearDialogMessageHandler(this, m_callback, m_userdata);
	m_dialogId = SceCommonGuiDialog_20B141F1(&param);
	m_opened = 0;
	return m_dialogId;
}

void NearDialog::ProgInc(SceInt32 target, SceUInt32 rate)
{
	if (target == 0) {
		ui::ProgressBar *bar = (ui::ProgressBar *)SceCommonGuiDialog_8947FC23(
			m_dialogId, NEAR_DIALOG_WIDGET_PROGRESS_BAR);

		m_progressValue = rate;
		if (bar != NULL && m_opened == 1)
			bar->SetValueAsync((int32_t)rate, true, ui::ProgressBar::BAR_FORE);
	}
}

inline void NearDialog::SetValue(SceInt32 target, SceUInt32 rate)
{
	if (target == 0) {
		ui::ProgressBar *bar = (ui::ProgressBar *)SceCommonGuiDialog_8947FC23(
			m_dialogId, NEAR_DIALOG_WIDGET_PROGRESS_BAR);

		m_progressValue = rate;
		if (bar != NULL && m_opened == 1)
			bar->SetValueAsync((int32_t)rate, false, ui::ProgressBar::BAR_FORE);
	}
}

/* the progress bar of a dialog just opened: empty at once, then the value it was given */
__attribute__((noinline)) void NearDialog::ResetProgress(void)
{
	SetValue(0, 0);
	ProgInc(0, m_progressValue);
}

/* the dialog has closed: the plugin hears with which button */
static void OnClosed(int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
{
	NearDialog *dialog = (NearDialog *)userdata;

	if (dialog != NULL) {
		SceInt32 button = dialog->m_button;

		if (dialog->m_callback != NULL)
			dialog->m_callback(SCE_OK, dialog->m_mode, button, dialog->m_userdata);
	}
}

/* the caller aborted: the dialog goes, and the plugin hears with no button */
static void OnAbort(int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
{
	NearDialog *dialog = (NearDialog *)userdata;

	if (dialog != NULL) {
		SceCommonGuiDialog_BC311DC0(dialog->m_dialogId);
		dialog->m_dialogId = -1;
		if (dialog->m_callback != NULL)
			dialog->m_callback(SCE_OK, dialog->m_mode, -1, dialog->m_userdata);
	}
}

void NearDialogLayoutHandler::operator()(SceInt32 value, SceInt32 button)
{
	NearDialog *dialog = m_dialog;

	if (dialog->m_dialogId == value) {
		SceCommonGuiDialog_BC311DC0(value);
		dialog->m_dialogId = -1;
	}
	if (m_dialog->m_scene != NULL) {
		m_dialog->m_button = button;
		m_dialog->m_scene->SetTimeoutEvent(1, NEAR_DIALOG_CLOSE_TIME, OnClosed, m_dialog);
	}
}

void NearDialogMessageHandler::operator()(SceInt32 value, SceInt32 button)
{
	if (m_dialog->m_scene != NULL) {
		m_dialog->m_button = button;
		m_dialog->m_scene->SetTimeoutEvent(1, NEAR_DIALOG_CLOSE_TIME, OnClosed, m_dialog);
	}
}

void NearDialogLayoutOpenHandler::operator()()
{
	if (s_dialog != NULL) {
		m_dialog->m_opened = 1;
		if (m_dialog->m_mode == 4)
			m_dialog->ResetProgress();
	}
}

void NearDialogMessageOpenHandler::operator()(CommonGuiMessageDialogParam *param)
{
	if (s_dialog != NULL)
		m_dialog->m_opened = 1;
}

/* the page and style of one of the library's layouts with the plugin's own buttons */
static inline void SetLayout(CommonGuiDialogLayout *layout, const CommonGuiDialogLayout &base,
			     const CommonGuiDialogButton *buttons)
{
	layout->page = base.page;
	layout->parts = base.parts;
	layout->buttons = buttons;
}

/* a system message's text, layout and sound */
static int SysMsg(SceInt32 type, NearDialogContent *content)
{
	SceUInt32 textId = 0;
	int ret = SCE_OK;

	switch (type) {
	case 1:
		textId = s_sysMsgTable[0].textId;
		content->layout = SceCommonGuiDialog_92752DE2;
		content->sound = s_sysMsgTable[0].sound;
		break;
	case 4:
		textId = s_sysMsgTable[1].textId;
		content->layout = SceCommonGuiDialog_AF75CA6D;
		content->sound = s_sysMsgTable[1].sound;
		break;
	case 5:
		textId = s_sysMsgTable[2].textId;
		SetLayout(&content->layout, SceCommonGuiDialog_22B17F2B, s_confirmButtons);
		content->sound = s_sysMsgTable[2].sound;
		break;
	case 7:
		textId = s_sysMsgTable[4].textId;
		SetLayout(&content->layout, SceCommonGuiDialog_22B17F2B, s_confirmButtons);
		content->sound = s_sysMsgTable[4].sound;
		break;
	case 8:
		textId = s_sysMsgTable[5].textId;
		SetLayout(&content->layout, SceCommonGuiDialog_22B17F2B, s_confirmButtons);
		content->sound = s_sysMsgTable[5].sound;
		break;
	case 9:
		textId = s_sysMsgTable[6].textId;
		SetLayout(&content->layout, SceCommonGuiDialog_22B17F2B, s_confirmButtons);
		content->sound = s_sysMsgTable[6].sound;
		break;
	case 10:
		textId = s_sysMsgTable[7].textId;
		SetLayout(&content->layout, SceCommonGuiDialog_22B17F2B, s_confirmButtons);
		content->sound = s_sysMsgTable[7].sound;
		break;
	case 11:
		textId = s_sysMsgTable[8].textId;
		content->layout = SceCommonGuiDialog_F8B2D7E1;
		content->sound = s_sysMsgTable[8].sound;
		break;
	case 12:
	case 43:
		textId = s_sysMsgTable[9].textId;
		content->layout = SceCommonGuiDialog_2A68B4A4;
		content->sound = s_sysMsgTable[9].sound;
		break;
	case 13:
		textId = s_sysMsgTable[10].textId;
		content->layout = SceCommonGuiDialog_AF75CA6D;
		content->sound = s_sysMsgTable[10].sound;
		break;
	case 14:
		textId = s_sysMsgTable[11].textId;
		content->layout = SceCommonGuiDialog_AF75CA6D;
		content->sound = s_sysMsgTable[11].sound;
		break;
	case 15:
		textId = s_sysMsgTable[12].textId;
		content->layout = s_locationRetryLayout;
		content->sound = s_sysMsgTable[12].sound;
		break;
	case 16:
		textId = s_sysMsgTable[13].textId;
		content->layout = s_locationRetryLayout;
		content->sound = s_sysMsgTable[13].sound;
		break;
	case 17:
		textId = s_sysMsgTable[14].textId;
		content->layout = SceCommonGuiDialog_F8B2D7E1;
		content->sound = s_sysMsgTable[14].sound;
		break;
	case 18:
		textId = s_sysMsgTable[15].textId;
		content->layout = SceCommonGuiDialog_F8B2D7E1;
		content->sound = s_sysMsgTable[15].sound;
		break;
	case 19:
		textId = s_sysMsgTable[17].textId;
		content->layout = SceCommonGuiDialog_AF75CA6D;
		content->sound = s_sysMsgTable[17].sound;
		break;
	case 20:
		textId = s_sysMsgTable[18].textId;
		content->layout = SceCommonGuiDialog_F8B2D7E1;
		content->sound = s_sysMsgTable[18].sound;
		break;
	case 21:
		textId = s_sysMsgTable[19].textId;
		content->layout = SceCommonGuiDialog_AF75CA6D;
		content->sound = s_sysMsgTable[19].sound;
		break;
	case 24:
		textId = s_sysMsgTable[20].textId;
		SetLayout(&content->layout, SceCommonGuiDialog_22B17F2B, s_giftButtons);
		content->sound = s_sysMsgTable[20].sound;
		break;
	case 25:
		textId = s_sysMsgTable[22].textId;
		SetLayout(&content->layout, SceCommonGuiDialog_F8B2D7E1, s_giftCancelButtons);
		content->sound = s_sysMsgTable[22].sound;
		break;
	case 28:
		textId = s_sysMsgTable[23].textId;
		content->layout = SceCommonGuiDialog_AF75CA6D;
		content->sound = s_sysMsgTable[23].sound;
		break;
	case 33:
		textId = s_sysMsgTable[24].textId;
		SetLayout(&content->layout, SceCommonGuiDialog_22B17F2B, s_confirmButtons);
		content->sound = s_sysMsgTable[24].sound;
		break;
	case 35:
		textId = s_sysMsgTable[26].textId;
		content->layout = SceCommonGuiDialog_AF75CA6D;
		content->sound = s_sysMsgTable[26].sound;
		break;
	case 36:
		textId = s_sysMsgTable[27].textId;
		content->layout = SceCommonGuiDialog_AF75CA6D;
		content->sound = s_sysMsgTable[27].sound;
		break;
	case 37:
		textId = s_sysMsgTable[28].textId;
		content->layout = SceCommonGuiDialog_AF75CA6D;
		content->sound = s_sysMsgTable[28].sound;
		break;
	case 38:
		textId = s_sysMsgTable[29].textId;
		content->layout = SceCommonGuiDialog_AF75CA6D;
		content->sound = s_sysMsgTable[29].sound;
		break;
	case 39:
		textId = s_sysMsgTable[30].textId;
		content->layout = SceCommonGuiDialog_AF75CA6D;
		content->sound = s_sysMsgTable[30].sound;
		break;
	case 6:
	case 32:
		textId = s_sysMsgTable[3].textId;
		content->layout = SceCommonGuiDialog_AF75CA6D;
		content->sound = s_sysMsgTable[3].sound;
		break;
	case 26:
	case 40:
		textId = s_sysMsgTable[31].textId;
		content->layout = SceCommonGuiDialog_AF75CA6D;
		content->sound = s_sysMsgTable[31].sound;
		break;
	case 27:
	case 41:
		textId = s_sysMsgTable[32].textId;
		content->layout = SceCommonGuiDialog_AF75CA6D;
		content->sound = s_sysMsgTable[32].sound;
		break;
	case 29:
	case 42:
		textId = s_sysMsgTable[50].textId;
		content->layout = SceCommonGuiDialog_F8B2D7E1;
		content->sound = s_sysMsgTable[50].sound;
		break;
	case 22:
		textId = s_sysMsgTable[21].textId;
		content->layout = SceCommonGuiDialog_AF75CA6D;
		content->sound = s_sysMsgTable[21].sound;
		break;
	case NEAR_DIALOG_SYSMSG_FINISH:
		/* the end screen's text is its list */
		content->layout = SceCommonGuiDialog_AF75CA6D;
		content->sound = s_sysMsgTable[33].sound;
		break;
	case 2:
	case 30:
		textId = s_sysMsgTable[42].textId;
		SetLayout(&content->layout, SceCommonGuiDialog_22B17F2B, s_confirmButtons);
		content->sound = s_sysMsgTable[42].sound;
		break;
	case 3:
	case 31:
		textId = s_sysMsgTable[43].textId;
		content->layout = SceCommonGuiDialog_AF75CA6D;
		content->sound = s_sysMsgTable[43].sound;
		break;
	default:
		ret = NEAR_DIALOG_PLUGIN_ERROR_INVALID;
		break;
	}
	content->text = g_nearDialogPlugin->GetString(IDParam(s_empty_name, textId));
	return ret;
}

/* a progress bar's text and layout */
static __attribute__((noinline)) int ProgressBar(SceInt32 type, NearDialogContent *content)
{
	int ret = SCE_OK;

	switch (type) {
	case 0:
	case 34:
		content->text = g_nearDialogPlugin->GetString(
			IDParam(s_empty_name, s_sysMsgTable[25].textId));
		content->layout = SceCommonGuiDialog_446F8B49;
		break;
	default:
		ret = NEAR_DIALOG_PLUGIN_ERROR_INVALID;
		break;
	}
	return ret;
}

/* a distance in whole kilometres or miles, in the country's number format */
/* the UTF-16 text of a UTF-8 string */
static inline wstring Utf8ToUtf16String(const string &src)
{
	wstring text;

	common::Utf8ToUtf16(src, &text, 0);
	return text;
}

static wstring FormatDistance(Plugin *plugin, SceUInt32 distance, const string &country)
{
	if (plugin == NULL || country.length() == 0)
		return wstring();

	string number;

	number = NearDialogFormatNumber(distance, plugin->m_locale, country);
	return Utf8ToUtf16String(number);
}

/* a distance in metres as the end screen shows it */
inline SceUInt32 NearDialogFinishScreen::Distance(float metres)
{
	float distance = 0.0f;

	if (metres != 0.0f) {
		distance = metres * 0.001f;
		if (!m_metric)
			distance = distance * NEAR_DIALOG_MILES_PER_KM;
	}
	return (SceUInt32)distance;
}

/* the end screen's results in the dialog's contents: the number of posts and the distances */
int NearDialogFinishScreen::Build(void)
{
	ui::Widget *contents;
	ui::Widget *box;

	if (m_plugin == NULL || m_dialogId == -1)
		return NEAR_DIALOG_PLUGIN_ERROR_STATE;
	contents = SceCommonGuiDialog_8947FC23(m_dialogId, NEAR_DIALOG_WIDGET_CONTENTS);
	if (contents == NULL)
		return NEAR_DIALOG_PLUGIN_ERROR_STATE;

	NearFinishInfoList::Param param = {
		"SceNearFinishInfoList", contents, m_plugin, 0, NEAR_DIALOG_FINISH_FONT_SIZE,
		NEAR_DIALOG_FINISH_WIDTH
	};

	m_list = new NearFinishInfoList(param);
	if (m_list == NULL)
		return NEAR_DIALOG_PLUGIN_ERROR_NO_MEMORY;

	string name("SceNearFinishInfoList");

	name += "_base_box";
	box = contents->FindChild(IDParam(name), 0) != NULL &&
	      contents->FindChild(IDParam(name), 0)->IsInherit(ui::Box::TypeName()) ?
	      contents->FindChild(IDParam(name), 0) : NULL;
	if (box == NULL)
		return NEAR_DIALOG_PLUGIN_ERROR_STATE;
	box->SetAlign(0, 0, 0, NULL);
	box->SetPos(math::v4(0.0f, NEAR_DIALOG_FINISH_POS_Y, 0.0f, 0.0f), NULL,
		    ui::Widget::ANIM_MOVE, 0, 0.0f, NULL, NULL);
	if (m_list->AddTitle(m_plugin->GetString(IDParam(s_empty_name, NEAR_DIALOG_MSG_FINISH_TITLE)),
			     "SceNearFinishTitle", NEAR_DIALOG_FINISH_TITLE_WIDTH) != SCE_OK)
		return NEAR_DIALOG_PLUGIN_ERROR_STATE;
	((ui::Box *)box)->SetChildrenAdjustY(ui::Box::CHILDREN_ADJUST_FIXED, NEAR_DIALOG_FINISH_HEIGHT);

	wchar_t buf[32] = { 0 };

	sce_paf_swprintf(buf, 32, L": %d", m_postCount);
	wstring text(buf);

	if (m_list->AddItem(m_plugin->GetString(IDParam(s_empty_name, NEAR_DIALOG_MSG_POST_COUNT)),
			    text.c_str(), "SceNearFinishPostCount") != SCE_OK)
		return NEAR_DIALOG_PLUGIN_ERROR_STATE;

	SceUInt32 total = Distance(m_totalDistance);
	wstring totalText = FormatDistance(m_plugin, total, m_country);
	SceUInt32 today = Distance(m_todayDistance);
	wstring todayText = FormatDistance(m_plugin, today, m_country);

	sce_paf_swprintf(buf, 32, L": ");
	if (m_metric) {
		text = buf;
		if (total == 0)
			text.append(m_plugin->GetString(IDParam(s_empty_name, NEAR_DIALOG_MSG_KM_ZERO)));
		else
			text.append(NearDialogFormat(m_plugin->GetString(IDParam(s_empty_name,
									      NEAR_DIALOG_MSG_KM)),
						 totalText.c_str()));
		if (m_list->AddItem(m_plugin->GetString(IDParam(s_empty_name,
								NEAR_DIALOG_MSG_TOTAL_DISTANCE)),
				    text.c_str(), "SceNearFinishInfoTotalDistanceTitle") != SCE_OK)
			return NEAR_DIALOG_PLUGIN_ERROR_STATE;
		text = buf;
		if (today == 0)
			text.append(m_plugin->GetString(IDParam(s_empty_name, NEAR_DIALOG_MSG_KM_ZERO)));
		else
			text.append(NearDialogFormat(m_plugin->GetString(IDParam(s_empty_name,
									      NEAR_DIALOG_MSG_KM)),
						 todayText.c_str()));
		if (m_list->AddItem(m_plugin->GetString(IDParam(s_empty_name,
								NEAR_DIALOG_MSG_TODAY_DISTANCE)),
				    text.c_str(), "SceNearFinishInfoTodayDistanceTitle") != SCE_OK)
			return NEAR_DIALOG_PLUGIN_ERROR_STATE;
	} else {
		text = buf;
		if (total == 0)
			text.append(m_plugin->GetString(IDParam(s_empty_name, NEAR_DIALOG_MSG_MILES_ZERO)));
		else if (total == 1)
			text.append(NearDialogFormat(m_plugin->GetString(IDParam(s_empty_name,
									      NEAR_DIALOG_MSG_MILE)),
						 totalText.c_str()));
		else
			text.append(NearDialogFormat(m_plugin->GetString(IDParam(s_empty_name,
									      NEAR_DIALOG_MSG_MILES)),
						 totalText.c_str()));
		if (m_list->AddItem(m_plugin->GetString(IDParam(s_empty_name,
								NEAR_DIALOG_MSG_TOTAL_DISTANCE)),
				    text.c_str(), "SceNearFinishInfoTotalDistanceTitle") != SCE_OK)
			return NEAR_DIALOG_PLUGIN_ERROR_STATE;
		text = buf;
		if (today == 0)
			text.append(m_plugin->GetString(IDParam(s_empty_name, NEAR_DIALOG_MSG_MILES_ZERO)));
		else if (today == 1)
			text.append(NearDialogFormat(m_plugin->GetString(IDParam(s_empty_name,
									      NEAR_DIALOG_MSG_MILE)),
						 todayText.c_str()));
		else
			text.append(NearDialogFormat(m_plugin->GetString(IDParam(s_empty_name,
									      NEAR_DIALOG_MSG_MILES)),
						 todayText.c_str()));
		if (m_list->AddItem(m_plugin->GetString(IDParam(s_empty_name,
								NEAR_DIALOG_MSG_TODAY_DISTANCE)),
				    text.c_str(), "SceNearFinishInfoTodayDistanceTitle") != SCE_OK)
			return NEAR_DIALOG_PLUGIN_ERROR_STATE;
	}
	return SCE_OK;
}

/* opens the dialog the plugin's message parameter describes */
int NearDialog::Create(const NearDialogMsgParam *param, NearDialogFunction callback,
		       void *userdata)
{
	int ret = SCE_OK;

	if (s_dialog == NULL) {
		s_dialog = new NearDialog(param->mode, callback, userdata);
		if (s_dialog == NULL)
			return NEAR_DIALOG_PLUGIN_ERROR_NO_MEMORY;
	} else {
		ret = NEAR_DIALOG_PLUGIN_ERROR_DIALOG_OPEN;
		if (s_dialog->m_dialogId == -1) {
			s_dialog->m_mode = param->mode;
			ret = SCE_OK;
			s_dialog->m_callback = callback;
			s_dialog->m_userdata = userdata;
		}
		if (ret != SCE_OK)
			return ret;
	}
	switch (param->mode) {
	case 0:
		ret = NEAR_DIALOG_PLUGIN_ERROR_INVALID;
		break;
	case 1: {
		common::EncodedString encoded;
		wstring text(ScePafMisc_53948F03(ScePafMisc_C11CB5E4(&encoded,
								     (const char *)param->msg, 1)));

		ScePafMisc_B45A7521(&encoded);
		CommonGuiDialogLayout layout = SceCommonGuiDialog_AF75CA6D;

		s_dialog->OpenLayout(text, &layout);
		ret = SCE_OK;
		break;
	}
	case 2: {
		NearDialogContent content;
		const NearDialogTrcMsg *trc;
		SceInt32 type;

		ret = SysMsg(param->sysMsgType, &content);
		if (ret < 0)
			break;
		s_dialog->OpenLayout(content.text, &content.layout);
		if (param->sysMsgType == NEAR_DIALOG_SYSMSG_FINISH) {
			SceInt32 dialogId = s_dialog->m_dialogId;

			s_finishInfoList = NULL;
			float today = param->finishInfo.todayDistance;
			float total = param->finishInfo.totalDistance;
			SceInt32 postCount = param->finishInfo.postCount;
			bool metric = param->finishInfo.imperial == 0;
			string country(param->finishInfo.country);
			NearDialogFinishScreen screen(g_nearDialogPlugin, dialogId, s_finishInfoList,
						      postCount, metric, total, today, country);

			if (screen.Build() != SCE_OK) {
				/* the dialog shows without the results */
			}
		}
		if (content.sound != 0)
			Framework::Instance()->PlayCommonSE(content.sound, 1.0f, false);
		type = param->sysMsgType;
		for (trc = s_trcMsgTable; trc->type != 0; trc++) {
			if (trc->type == type) {
				AppmgrNotificationRequest request;

				sce_paf_memset(&request, 0, sizeof(request));
				sceAppMgrSendNotificationRequest(&request, 1);
				break;
			}
		}
		ret = SCE_OK;
		break;
	}
	case 3:
		s_dialog->OpenErrorCode(param->errorCode);
		ret = SCE_OK;
		break;
	case 4: {
		NearDialogContent content;

		ret = ProgressBar(param->sysMsgType, &content);
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
int NearDialog::Abort(bool soon)
{
	NearDialog *dialog = s_dialog;

	if (dialog != NULL) {
		float timeout = NEAR_DIALOG_CLOSE_TIME;

		if (soon)
			timeout = NEAR_DIALOG_ABORT_TIME_SOON;
		else if (dialog->m_mode == 4)
			timeout = NEAR_DIALOG_ABORT_TIME_PROGRESS;
		dialog->m_scene->SetTimeoutEvent(1, timeout, OnAbort, dialog);
	}
	return SCE_OK;
}

NearDialog *NearDialog::Get(void)
{
	return s_dialog;
}
