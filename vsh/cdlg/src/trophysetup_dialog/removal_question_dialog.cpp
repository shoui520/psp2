/*
 * The question before an installed title is removed, with the title's icon when it has one. The
 * file's name is ours: no Sony path names it.
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
#include "dialogs.h"

using namespace paf;

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
