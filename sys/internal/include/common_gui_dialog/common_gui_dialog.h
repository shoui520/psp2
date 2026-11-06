/*
 * SceCommonGuiDialog (vs0:sys/external/common_gui_dialog.suprx): the parameters of the dialogs
 * it shows for other libraries. The library is not converted yet: what is here is what its
 * callers show of it, and the names are ours (no Sony record names these exports or types).
 */
#ifndef COMMON_GUI_DIALOG_COMMON_GUI_DIALOG_H
#define COMMON_GUI_DIALOG_COMMON_GUI_DIALOG_H

#include <scetypes.h>
#include <std/string>
#include <std/vector>

namespace paf {
class Plugin;
}

/*
 * What every dialog's parameters start with, filled by SceCommonGuiDialog_696DB9C9:
 * the framework's common resource plugin, then the dialog's placement and options, which only
 * the library reads (its layout is left to its conversion).
 */
class CommonGuiDialogParam {
public:
	CommonGuiDialogParam();

	paf::Plugin *resourcePlugin;
	SceUInt8 options[0x40];		/* the library's own; not read by its callers */
};

/* a button or line of a message dialog */
struct CommonGuiMessageDialogItem {
	paf::wstring label;
	paf::vector<paf::wstring> lines;
	paf::wstring text;
	SceUInt32 value;
};

/* the 4-byte option word after the dialog's type: a flag in its first byte */
struct CommonGuiMessageDialogOption {
	bool enabled;
	SceUInt8 reserved[3];

	CommonGuiMessageDialogOption() : enabled(false), reserved() {}
};

/* the parameters of a message dialog (SceCommonGuiDialog_20B141F1) */
class CommonGuiMessageDialogParam : public CommonGuiDialogParam {
public:
	CommonGuiMessageDialogParam()
		: type(0), option(), buttonType(-2), buttonCount(2), defaultButton(2), cancelButton(2),
		  messageId(0), closeOnSelect(false), modal(true), useLayout(false), callback(NULL),
		  callbackArg(0)
	{
	}

	SceInt32 type;
	CommonGuiMessageDialogOption option;
	SceInt32 buttonType;
	SceInt32 buttonCount;
	SceInt32 defaultButton;
	SceInt32 cancelButton;
	SceUInt32 messageId;		/* the text's resource ID */
	paf::wstring title;
	paf::vector<paf::wstring> titleLines;
	paf::wstring message;
	paf::vector<SceUInt32> messageIds;
	paf::vector<CommonGuiMessageDialogItem> items;
	bool closeOnSelect;
	bool modal;
	bool useLayout;
	const void *layout[3];		/* read when useLayout is set */
	void *callback;
	SceUInt32 callbackArg;
};

extern "C" {
CommonGuiDialogParam *SceCommonGuiDialog_696DB9C9(CommonGuiDialogParam *param);
/* shows a message dialog: returns when it is closed */
int SceCommonGuiDialog_20B141F1(CommonGuiMessageDialogParam *param);
}

inline CommonGuiDialogParam::CommonGuiDialogParam()
{
	SceCommonGuiDialog_696DB9C9(this);
}

#endif
