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
#include <std/memory>
#include <math/math.h>
#include <graphics/surface/surface.h>
#include <cache/cache_image.h>

namespace paf {
class Plugin;
}

/*
 * What a dialog calls when one of its buttons is pressed: the dialog's own value and the button.
 * The library keeps the object and deletes it with the dialog.
 */
class CommonGuiDialogHandler {
public:
	virtual ~CommonGuiDialogHandler()
	{
	}

	virtual void operator()(SceInt32 value, SceInt32 button) = 0;
};

/* the function form of a handler, which SceCommonGuiDialog_6CE14F66 wraps in one itself */
typedef void (*CommonGuiDialogFunction)(SceInt32 value, SceInt32 button, void *arg);

/* an entry of a dialog's button table: the button and its label's resource ID; {0, 0} ends it */
struct CommonGuiDialogButton {
	SceInt32 button;
	SceUInt32 labelId;
};

/*
 * A dialog's layout: its page and style tables, which only the library reads, and its buttons.
 * The library exports its own layouts as variables (below).
 */
struct CommonGuiDialogLayout {
	const void *page;
	const void *style;
	const CommonGuiDialogButton *buttons;
};

/*
 * What every dialog's parameters start with, filled by SceCommonGuiDialog_696DB9C9:
 * the framework's common resource plugin, then the dialog's placement and options, which only
 * the library reads (its layout is left to its conversion).
 */
class CommonGuiDialogParam {
public:
	CommonGuiDialogParam();

	paf::Plugin *resourcePlugin;
	SceUInt32 reserved4;
	/* the background's and the dimmer's colours, RGBA from 0 to 1; SceCommonGuiDialog_EF2C5382 by
	   default (the dialog's own) */
	float bgColor[4];
	float dimmerColor[4];
	/* the library's own placement and options; not read by its callers */
	SceUInt8 options[28];
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
		  messageId(0), closeOnSelect(false), modal(true), useLayout(false), handler(NULL),
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
	CommonGuiDialogHandler *handler;
	SceUInt32 callbackArg;
};

typedef char CommonGuiMessageDialogParamSize[sizeof(CommonGuiMessageDialogParam) == 192 ? 1 : -1];

/*
 * The parameters of a dialog with its own layout and buttons (SceCommonGuiDialog_CCFCD9A6):
 * its heading and text, its colour and images, and its handlers. The library takes the handlers
 * over; the images are released with the parameters.
 */
class CommonGuiLayoutDialogParam : public CommonGuiDialogParam {
public:
	CommonGuiLayoutDialogParam();

	/* the call waits until the dialog closes, and returns its result */
	bool wait;
	CommonGuiDialogLayout layout;
	paf::wstring heading;
	paf::wstring text;
	/* the text's colour; SceCommonGuiDialog_EF2C5382 by default */
	paf::math::v4 textColor;
	float scale;
	paf::intrusive_ptr<paf::graph::Surface> image;
	paf::intrusive_ptr<paf::cache::CacheImageInterface> cachedImage;
	paf::intrusive_ptr<paf::graph::Surface> secondImage;
	CommonGuiDialogHandler *handler;
	CommonGuiDialogHandler *closeHandler;
	CommonGuiDialogHandler *updateHandler;
};

typedef char CommonGuiLayoutDialogParamSize[sizeof(CommonGuiLayoutDialogParam) == 160 ? 1 : -1];

extern "C" {
CommonGuiDialogParam *SceCommonGuiDialog_696DB9C9(CommonGuiDialogParam *param);
/* shows a message dialog: returns when it is closed */
int SceCommonGuiDialog_20B141F1(CommonGuiMessageDialogParam *param);
/* shows a dialog with its own layout: its ID, or a negative error */
int SceCommonGuiDialog_CCFCD9A6(CommonGuiLayoutDialogParam *param);
/* shows a waiting dialog with the given heading, text and layout: its ID, or a negative error */
int SceCommonGuiDialog_6CE14F66(paf::Plugin *plugin, const paf::wstring &heading,
				const paf::wstring &text, const CommonGuiDialogLayout *layout,
				CommonGuiDialogFunction func, void *arg);
/* has func(value, arg) called every interval milliseconds while the dialog is open */
int SceCommonGuiDialog_F1A40838(int dialogId, int (*func)(int value, void *arg), void *arg, float interval);
/* closes a dialog with a result: 0, or -1 without the dialog */
int SceCommonGuiDialog_01702008(int dialogId, int result);
/* lets a dialog go */
int SceCommonGuiDialog_BC311DC0(int dialogId);

/* the library's layouts: a dialog with buttons, a waiting dialog without and with a cancel
   button, and the default */
extern const CommonGuiDialogLayout SceCommonGuiDialog_22B17F2B;
extern const CommonGuiDialogLayout SceCommonGuiDialog_2A68B4A4;
extern const CommonGuiDialogLayout SceCommonGuiDialog_F8B2D7E1;
extern const CommonGuiDialogLayout SceCommonGuiDialog_AF75CA6D;
/* the colour that leaves a dialog's own */
extern const paf::math::v4 SceCommonGuiDialog_EF2C5382;
}

inline CommonGuiDialogParam::CommonGuiDialogParam()
{
	SceCommonGuiDialog_696DB9C9(this);
}

inline CommonGuiLayoutDialogParam::CommonGuiLayoutDialogParam()
	: wait(false), layout(SceCommonGuiDialog_AF75CA6D), textColor(SceCommonGuiDialog_EF2C5382),
	  scale(1.0f), handler(NULL), closeHandler(NULL), updateHandler(NULL)
{
}

#endif
