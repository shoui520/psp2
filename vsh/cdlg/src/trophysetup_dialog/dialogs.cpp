/*
 * The dialogs the plugin shows through SceCommonGuiDialog, with trophysetup_dialog_plugin's
 * strings: an error, the processing dialog, the last notice, and the question before an
 * installed title is removed (with the title's icon when it has one). The file's name and the
 * dialogs' names are ours: no Sony path names them.
 */
#include <stddef.h>
#include <scetypes.h>
#include <np/np_error.h>
#include <plugin.h>
#include <versatileparam/idparam.h>
#include <std/string>
#include <std/memory>
#include <widget/core/widget.h>
#include <widget/core/handler.h>
#include <widget/w_text_box.h>
#include <graphics/text/text_layout.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include "np_trophy_setup_dialog_plugin.h"

using namespace paf;

/* the strings and templates of trophysetup_dialog_plugin's resource the dialogs show */
#define TROPHY_SETUP_MSG_ERROR			0x0176210C
#define TROPHY_SETUP_MSG_NO_SPACE		0xE2A2DDCC
#define TROPHY_SETUP_MSG_PROCESSING		0xD1512F9B
#define TROPHY_SETUP_MSG_EXIT			0xC67DC466
#define TROPHY_SETUP_MSG_REMOVAL_QUESTION	0x4B303AAB
#define TROPHY_SETUP_MSG_REMOVAL_NOTE		0xC667793A
#define TROPHY_SETUP_QUESTION_FOCUS		0xE0D9F918
#define TROPHY_SETUP_TEMPLATE_TITLE		0xEEC81AFE
#define TROPHY_SETUP_TITLE_ICON			0xA359C587
#define TROPHY_SETUP_TITLE_NAME			0x0EC240C5
#define TROPHY_SETUP_TEMPLATE_NOTE		0x956D0FC2
#define TROPHY_SETUP_NOTE_TEXT			0x145CAB23

/* the dialogs' widgets (SceCommonGuiDialog_8947FC23) */
#define TROPHY_SETUP_DIALOG_WIDGET_TEXT		1
#define TROPHY_SETUP_DIALOG_WIDGET_ANIMATION	4
#define TROPHY_SETUP_DIALOG_WIDGET_CONTENTS	10
#define TROPHY_SETUP_DIALOG_WIDGET_REMOVED	12

/* the processing dialog shows at least this long, in milliseconds */
#define TROPHY_SETUP_PROCESSING_TIME		2000.0f

/* a handler that calls a function */
class TrophySetupDialogHandler : public CommonGuiDialogHandler {
public:
	TrophySetupDialogHandler(CommonGuiDialogFunction func, void *arg) : m_func(func), m_arg(arg)
	{
	}

	virtual void operator()(SceInt32 value, SceInt32 button)
	{
		m_func(value, button, m_arg);
	}

	CommonGuiDialogFunction m_func;
	void *m_arg;
};

/* the error dialog's other handler, which does nothing */
class TrophySetupCloseHandler : public CommonGuiDialogCloseHandler {
public:
	virtual void operator()(SceInt32 value)
	{
	}
};

/* a handler that calls a function if it has one */
class TrophySetupQuestionHandler : public CommonGuiDialogHandler {
public:
	TrophySetupQuestionHandler(CommonGuiDialogFunction func, void *arg) : m_func(func), m_arg(arg)
	{
	}

	virtual void operator()(SceInt32 value, SceInt32 button)
	{
		if (m_func != NULL)
			m_func(value, button, m_arg);
	}

	CommonGuiDialogFunction m_func;
	void *m_arg;
};

/* the text an error is shown with, or 0 for the library's own */
static __attribute__((noinline)) SceUInt32 GetErrorMessageId(SceUInt32 error)
{
	if (((error >> 16) & 0xFFF) == 0x014 ||
	    error == 0x80551688 || error == 0x8055168A || error == 0x8055168C ||
	    error == 0x805516B4 || error == SCE_NP_TROPHY_ERROR_BROKEN_DATA ||
	    error == 0x805516CF || error == 0x805516B7 || error == 0x805516B5 ||
	    error == 0x805516B6 || error == 0x805516CE || error == 0x80551696 ||
	    error == 0x805516CC || error == SCE_NP_TROPHY_ERROR_INVALID_TROPHY_CONF_FORMAT)
		return TROPHY_SETUP_MSG_ERROR;
	if (error == SCE_NP_TROPHY_ERROR_INSUFFICIENT_MC_SPACE)
		return TROPHY_SETUP_MSG_NO_SPACE;
	return 0;
}

int OpenErrorDialog(SceInt32 error, CommonGuiDialogFunction func, void *arg)
{
	Plugin *plugin = Plugin::Find(TROPHY_SETUP_PLUGIN_NAME);
	SceUInt32 messageId;
	int dialogId;

	if (plugin == NULL)
		return -1;
	CommonGuiMessageDialogParam param;

	param.type = error;
	param.option.flags = 8;
	messageId = GetErrorMessageId(error);
	if (messageId != 0)
		param.message = plugin->GetString(IDParam(s_empty_name, messageId));
	dialogId = SceCommonGuiDialog_A6422883(plugin, &param, new TrophySetupDialogHandler(func, arg),
					       new TrophySetupCloseHandler);
	if (dialogId >= 0) {
		math::v4 block(0.0f, 0.0f, 0.0f, 0.0f);

		SceCommonGuiDialog_206623FF(dialogId, &block, &block);
	}
	return dialogId;
}

int OpenProcessingDialog(ui::HandlerCB func, void *data)
{
	Plugin *plugin = Plugin::Find(TROPHY_SETUP_PLUGIN_NAME);
	CommonGuiLayoutDialogParam param;
	ui::Widget *widget;
	int dialogId;

	param.text = plugin->GetString(IDParam(s_empty_name, TROPHY_SETUP_MSG_PROCESSING));
	param.dimmerColor[0] = 0.0f;
	param.dimmerColor[1] = 0.0f;
	param.dimmerColor[2] = 0.0f;
	param.dimmerColor[3] = 0.0f;
	param.layout = SceCommonGuiDialog_F8B2D7E1;
	dialogId = SceCommonGuiDialog_CCFCD9A6(&param);
	if (dialogId >= 0) {
		widget = SceCommonGuiDialog_8947FC23(dialogId, TROPHY_SETUP_DIALOG_WIDGET_REMOVED);
		if (widget != NULL)
			widget->DestroyWidget();
		widget = SceCommonGuiDialog_8947FC23(dialogId, TROPHY_SETUP_DIALOG_WIDGET_ANIMATION);
		if (widget != NULL && func != NULL)
			widget->SetTimerListener(0x01000000 | (dialogId & 0xFFFF), TROPHY_SETUP_PROCESSING_TIME,
						 new ui::EventCBListener(func, data));
	}
	return dialogId;
}

int OpenExitNoticeDialog(void)
{
	Plugin *plugin = Plugin::Find(TROPHY_SETUP_PLUGIN_NAME);

	if (plugin == NULL)
		return -1;
	CommonGuiLayoutDialogParam param;

	param.text = plugin->GetString(IDParam(s_empty_name, TROPHY_SETUP_MSG_EXIT));
	param.layout = SceCommonGuiDialog_52A53037;
	return SceCommonGuiDialog_CCFCD9A6(&param);
}

int OpenRemovalQuestionDialog(const wstring &title, CommonGuiDialogFunction func, void *arg)
{
	Plugin *plugin = Plugin::Find(TROPHY_SETUP_PLUGIN_NAME);
	int dialogId;

	if (plugin == NULL)
		return -1;
	wstring text(plugin->GetString(IDParam(s_empty_name, TROPHY_SETUP_MSG_REMOVAL_QUESTION)));

	text += L"\n";
	text.append(title);
	text += L"\n";
	text += plugin->GetString(IDParam(s_empty_name, TROPHY_SETUP_MSG_REMOVAL_NOTE));
	CommonGuiLayoutDialogParam param;

	param.text = text;
	param.dimmerColor[0] = 0.0f;
	param.dimmerColor[1] = 0.0f;
	param.dimmerColor[2] = 0.0f;
	param.dimmerColor[3] = 0.0f;
	param.layout = SceCommonGuiDialog_6CD802FC;
	param.handler = new TrophySetupQuestionHandler(func, arg);
	dialogId = SceCommonGuiDialog_CCFCD9A6(&param);
	if (dialogId >= 0)
		SceCommonGuiDialog_D3F78BA5(dialogId, TROPHY_SETUP_QUESTION_FOCUS);
	return dialogId;
}

/* the title's icon and name, and the note under them, in the question's contents */
void SetRemovalQuestionContents(Plugin *plugin, ui::Widget *contents, ui::Widget *text,
				const wstring &title, intrusive_ptr<graph::Surface> &icon)
{
	ui::Widget *widget;
	int ret;

	if (contents == NULL || text == NULL || icon.get() == NULL)
		return;
	if (text->IsInherit(ui::Text::TypeName()))
		((ui::Text *)text)->SetLayoutAttribute(graph::TextLayoutAttribute_AlignX, 0);
	ret = plugin->TemplateOpen(contents, IDParam(s_empty_name, TROPHY_SETUP_TEMPLATE_TITLE),
				   Plugin::TemplateOpenParam());
	if (ret < 0)
		return;
	widget = contents->FindChild(IDParam(s_empty_name, TROPHY_SETUP_TITLE_ICON), 0);
	if (widget != NULL)
		widget->SetTexture(icon);
	widget = contents->FindChild(IDParam(s_empty_name, TROPHY_SETUP_TITLE_NAME), 0);
	if (widget != NULL)
		widget->SetString(title);
	ret = plugin->TemplateOpen(contents, IDParam(s_empty_name, TROPHY_SETUP_TEMPLATE_NOTE),
				   Plugin::TemplateOpenParam());
	if (ret < 0)
		return;
	widget = contents->FindChild(IDParam(s_empty_name, TROPHY_SETUP_NOTE_TEXT), 0);
	if (widget != NULL)
		widget->SetString(wstring(plugin->GetString(IDParam(s_empty_name, TROPHY_SETUP_MSG_REMOVAL_NOTE))));
}

int OpenRemovalQuestionDialogWithIcon(const wstring &title, intrusive_ptr<graph::Surface> &icon,
				      CommonGuiDialogFunction func, void *arg)
{
	Plugin *plugin = Plugin::Find(TROPHY_SETUP_PLUGIN_NAME);
	int dialogId;

	if (plugin == NULL)
		return -1;
	CommonGuiLayoutDialogParam param;

	param.text = plugin->GetString(IDParam(s_empty_name, TROPHY_SETUP_MSG_REMOVAL_QUESTION));
	param.dimmerColor[0] = 0.0f;
	param.dimmerColor[1] = 0.0f;
	param.dimmerColor[2] = 0.0f;
	param.dimmerColor[3] = 0.0f;
	param.layout = SceCommonGuiDialog_6CD802FC;
	param.handler = new TrophySetupQuestionHandler(func, arg);
	dialogId = SceCommonGuiDialog_CCFCD9A6(&param);
	if (dialogId >= 0) {
		SceCommonGuiDialog_D3F78BA5(dialogId, TROPHY_SETUP_QUESTION_FOCUS);
		SetRemovalQuestionContents(plugin,
					   SceCommonGuiDialog_8947FC23(dialogId, TROPHY_SETUP_DIALOG_WIDGET_CONTENTS),
					   SceCommonGuiDialog_8947FC23(dialogId, TROPHY_SETUP_DIALOG_WIDGET_TEXT),
					   title, icon);
	}
	return dialogId;
}
