/*
 * The image dialogs' SceCommonGuiDialog dialogs: a waiting dialog with the layout it was made
 * with, a small one without buttons, a confirmation and a message, each telling its listeners
 * how it closed. Names ours; see gui_dialog.h.
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <scetypes.h>
#include <std/new>
#include <std/string>
#include <plugin.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include "event_dispatcher.h"
#include "gui_dialog.h"

using namespace paf;

/* a handler that calls its function once */
class GuiDialogHandler : public CommonGuiDialogHandler {
public:
	GuiDialogHandler(CommonGuiDialogFunction func, void *arg) : m_func(func), m_arg(arg)
	{
	}

	virtual void operator()(SceInt32 value, SceInt32 button)
	{
		if (m_func != NULL) {
			m_func(value, button, m_arg);
			m_func = NULL;
		}
	}

	CommonGuiDialogFunction m_func;
	void *m_arg;
};

/* a GuiDialogParam button as SceCommonGuiDialog takes it */
static inline SceInt32 GuiDialogButton(SceInt32 button)
{
	if (button == 0)
		return 0;
	return button == 1 ? 1 : 2;
}

/* the dialog's text: its own, else the string of its ID */
static inline const wchar_t *GuiDialogText(const GuiDialogParam &param)
{
	if (param.text.length() != 0)
		return param.text.c_str();
	return param.plugin->GetString(param.textId);
}

GuiDialog::GuiDialog(const CommonGuiDialogLayout *layout)
	: m_layout(layout), m_closeOnResult(true), m_dialogId(-1)
{
}

GuiDialog::~GuiDialog()
{
}

int GuiDialog::Open(const GuiDialogParam &param)
{
	int dialogId;

	if (param.plugin == NULL)
		return -1;
	if (IsOpen())
		return -1;
	const wchar_t *text = GuiDialogText(param);

	dialogId = SceCommonGuiDialog_6CE14F66(param.plugin, wstring(), wstring(text), m_layout,
					       ResultCB, this);
	if (dialogId < 0)
		return dialogId;
	m_dialogId = dialogId;
	return 0;
}

void GuiDialog::Close()
{
	if (m_dialogId != -1)
		SceCommonGuiDialog_BC311DC0(m_dialogId);
	m_dialogId = -1;
}

bool GuiDialog::IsOpen()
{
	return m_dialogId != -1;
}

void GuiDialog::ResultCB(SceInt32 value, SceInt32 button, void *arg)
{
	((GuiDialog *)arg)->OnResult(value, button);
}

GuiWaitDialog::GuiWaitDialog() : GuiDialog(&SceCommonGuiDialog_2A68B4A4)
{
}

int GuiWaitDialog::Open(const GuiDialogParam &param)
{
	int dialogId;

	if (param.plugin == NULL)
		return -1;
	if (IsOpen())
		return -1;
	const wchar_t *text = GuiDialogText(param);
	CommonGuiLayoutDialogParam layoutParam;

	layoutParam.resourcePlugin = param.plugin;
	layoutParam.layout = SceCommonGuiDialog_2A68B4A4;
	layoutParam.text = text;
	if (param.color.extract_x() >= 0.0f)
		memcpy(layoutParam.dimmerColor, &param.color, sizeof(layoutParam.dimmerColor));
	dialogId = SceCommonGuiDialog_CCFCD9A6(&layoutParam);
	if (dialogId < 0)
		return dialogId;
	m_dialogId = dialogId;
	return 0;
}

void GuiWaitDialog::OnResult(SceInt32 value, SceInt32 button)
{
}

GuiConfirmDialog::GuiConfirmDialog() : GuiDialog(&SceCommonGuiDialog_92752DE2)
{
}

void GuiConfirmDialog::OnResult(SceInt32 value, SceInt32 button)
{
	bool decided = button != GUI_DIALOG_BUTTON_CANCEL;
	int32_t event = button == GUI_DIALOG_BUTTON_CANCEL ? GUI_DIALOG_EVENT_CLOSED :
							       GUI_DIALOG_EVENT_DECIDED;

	if (IsOpen()) {
		if (m_closeOnResult)
			Close();
		GuiDialogResult result;

		result.name = m_param.name;
		result.decided = decided;
		Notify(event, &result);
	}
}

GuiMessageDialog::GuiMessageDialog() : GuiDialog(&SceCommonGuiDialog_7AEA192C)
{
}

int GuiMessageDialog::Open(const GuiDialogParam &param)
{
	int dialogId;

	if (param.plugin == NULL)
		return -1;
	if (IsOpen())
		return -1;
	CommonGuiMessageDialogParam messageParam;

	messageParam.resourcePlugin = param.plugin;
	messageParam.type = param.type;
	messageParam.messageId = param.textId.GetIDHash();
	messageParam.defaultButton = GuiDialogButton(param.defaultButton);
	messageParam.cancelButton = GuiDialogButton(param.cancelButton);
	if (param.text.length() != 0)
		messageParam.title = param.text;
	messageParam.handler = new GuiDialogHandler(ResultCB, this);
	messageParam.modal = param.modal;
	messageParam.buttonCount = GuiDialogButton(param.buttonCount);
	messageParam.option.flags = param.option.flags;
	messageParam.option.reserved = param.option.reserved;
	dialogId = SceCommonGuiDialog_20B141F1(&messageParam);
	if (dialogId < 0)
		return dialogId;
	m_dialogId = dialogId;
	return 0;
}

void GuiMessageDialog::OnResult(SceInt32 value, SceInt32 button)
{
	bool decided = button != GUI_DIALOG_BUTTON_CANCEL;

	if (IsOpen()) {
		if (m_closeOnResult)
			Close();
		GuiDialogResult result;

		result.name = m_param.name;
		result.decided = decided;
		Notify(GUI_DIALOG_EVENT_CLOSED, &result);
	}
}
