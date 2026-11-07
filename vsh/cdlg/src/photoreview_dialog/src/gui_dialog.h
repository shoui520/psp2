/*
 * The image dialogs' SceCommonGuiDialog dialogs as EventDispatchers: a dialog opened from a
 * GuiDialogParam tells its listeners how it closed. The photo apps and the image dialogs
 * (libcdlg_photoreview, libcdlg_cameraimport, libcdlg_photoimport, libcdlg_calendar_review, ...)
 * build the same code; no binary names it, so the classes, their methods and this file's name
 * are ours.
 */
#ifndef PHOTOREVIEW_DIALOG_GUI_DIALOG_H
#define PHOTOREVIEW_DIALOG_GUI_DIALOG_H

#include <stdint.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <math/ngp/v4.h>
#include <versatileparam/idparam.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include "event_dispatcher.h"

/* what a dialog tells its listeners as it closes, with a GuiDialogResult */
enum {
	GUI_DIALOG_EVENT_CLOSED = 1,	/* closed or cancelled */
	GUI_DIALOG_EVENT_DECIDED = 2	/* the confirmation's first button */
};

/* the button the library reports for a cancel */
#define GUI_DIALOG_BUTTON_CANCEL	2

/* what a dialog shows: SceCommonGuiDialog's parameters, in the form the dialogs take them */
class GuiDialogParam {
public:
	GuiDialogParam(const paf::IDParam &id = paf::IDParam())
		: plugin(NULL), textId(id), type(0), defaultButton(0), cancelButton(2),
		  color(-1.0f, -1.0f, -1.0f, -1.0f), modal(true), buttonCount(0), option()
	{
	}

	paf::Plugin *plugin;
	/* the text, by its ID in the plugin's resource when text is empty */
	paf::IDParam textId;
	paf::wstring text;
	/* given back in the dialog's GuiDialogResult */
	paf::string name;
	/* a message dialog's type (the error it shows) */
	SceInt32 type;
	/* a message dialog's buttons: 0, 1, or 2 for anything else */
	SceInt32 defaultButton;
	SceInt32 cancelButton;
	/* a layout dialog's dimmer colour; its own while x is negative */
	paf::math::v4 color;
	bool modal;
	SceInt32 buttonCount;
	CommonGuiMessageDialogOption option;
};

/* the argument a dialog's event comes with */
class GuiDialogResult {
public:
	paf::string name;
	/* the dialog closed with another button than cancel */
	SceInt32 decided;
};

/* a dialog with the given layout: Open shows it as a waiting dialog */
class GuiDialog : public EventDispatcher {
public:
	GuiDialog(const CommonGuiDialogLayout *layout);
	virtual ~GuiDialog();

	virtual int Open(const GuiDialogParam &param);
	virtual void Close();
	virtual bool IsOpen();
	/* a button was pressed */
	virtual void OnResult(SceInt32 value, SceInt32 button) = 0;

	/* OnResult on the dialog arg */
	static void ResultCB(SceInt32 value, SceInt32 button, void *arg);

	GuiDialogParam m_param;
	const CommonGuiDialogLayout *m_layout;
	/* OnResult closes the dialog */
	bool m_closeOnResult;
	/* the open dialog, or -1 */
	int m_dialogId;
};

/* a small waiting dialog without buttons (SceCommonGuiDialog_2A68B4A4) */
class GuiWaitDialog : public GuiDialog {
public:
	GuiWaitDialog();

	virtual int Open(const GuiDialogParam &param);
	virtual void OnResult(SceInt32 value, SceInt32 button);
};

/* a question with OK and cancel (SceCommonGuiDialog_92752DE2) */
class GuiConfirmDialog : public GuiDialog {
public:
	GuiConfirmDialog();

	virtual void OnResult(SceInt32 value, SceInt32 button);
};

/* a message dialog (SceCommonGuiDialog_7AEA192C) */
class GuiMessageDialog : public GuiDialog {
public:
	GuiMessageDialog();

	virtual int Open(const GuiDialogParam &param);
	virtual void OnResult(SceInt32 value, SceInt32 button);
};

#endif
