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
#include <versatileparam/idparam.h>
#include <widget/w_dialog.h>

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

class CommonGuiMessageDialogParam;

/*
 * What a message dialog calls as it opens, with its parameters, which the object may still change
 * (its title and lines, its buttons): the library keeps the object and deletes it with the dialog.
 */
class CommonGuiMessageDialogOpenHandler {
public:
	virtual ~CommonGuiMessageDialogOpenHandler()
	{
	}

	virtual void operator()(CommonGuiMessageDialogParam *param) = 0;
};

/* what a dialog with its own layout calls as it opens, kept and deleted like its handler */
class CommonGuiLayoutDialogOpenHandler {
public:
	virtual ~CommonGuiLayoutDialogOpenHandler()
	{
	}

	virtual void operator()() = 0;
};

/* the function form of a handler, which SceCommonGuiDialog_6CE14F66 wraps in one itself */
typedef void (*CommonGuiDialogFunction)(SceInt32 value, SceInt32 button, void *arg);

/*
 * An entry of a dialog's button table: the button's widget (by its index in the layout, as
 * SceCommonGuiDialog_8947FC23 takes it), its label's resource ID, the button the handler is
 * called with, and the resource ID of its look (0x20413274 for the dialogs' buttons, 0xC17D7980
 * for their cancel buttons); an entry of zeros ends it.
 */
struct CommonGuiDialogButton {
	SceInt32 index;
	SceUInt32 labelId;
	SceInt32 button;
	SceUInt32 styleId;
};

/* a dialog's page: three resource IDs only the library reads */
struct CommonGuiDialogPage {
	SceUInt32 id[3];
};

/*
 * A dialog's layout: its page; its parts, each index (as SceCommonGuiDialog_8947FC23 takes it) to
 * its widget's ID, which the library gives the paf::ui::Dialog; and its buttons. The library
 * exports its own layouts as variables (below).
 */
struct CommonGuiDialogLayout {
	const CommonGuiDialogPage *page;
	const paf::ui::Dialog::PartsList *parts;
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

/* the 4-byte option word after the dialog's type, in bit fields (Sony's copies it field by field):
   flags in its first byte (the trophy setup dialog sets 8 for its error dialog; what the bits
   mean is not known) */
struct CommonGuiMessageDialogOption {
	SceUInt32 flags : 8;
	SceUInt32 reserved : 24;

	CommonGuiMessageDialogOption() : flags(0), reserved(0) {}
};

/* the parameters of a message dialog (SceCommonGuiDialog_20B141F1) */
class CommonGuiMessageDialogParam : public CommonGuiDialogParam {
public:
	CommonGuiMessageDialogParam()
		: type(0), option(), buttonType(-2), buttonCount(2), defaultButton(2), cancelButton(2),
		  messageId(0), closeOnSelect(false), modal(true), useLayout(false), handler(NULL),
		  openHandler(NULL)
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
	CommonGuiMessageDialogOpenHandler *openHandler;
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
	CommonGuiLayoutDialogOpenHandler *openHandler;
	CommonGuiDialogHandler *updateHandler;
};

typedef char CommonGuiLayoutDialogParamSize[sizeof(CommonGuiLayoutDialogParam) == 160 ? 1 : -1];

extern "C" {
CommonGuiDialogParam *SceCommonGuiDialog_696DB9C9(CommonGuiDialogParam *param);
/* shows a message dialog: returns when it is closed */
int SceCommonGuiDialog_20B141F1(CommonGuiMessageDialogParam *param);
/* shows a message dialog with the plugin's resources and the two handlers, which the library
   takes over: its ID, or a negative error */
int SceCommonGuiDialog_A6422883(paf::Plugin *plugin, CommonGuiMessageDialogParam *param,
				CommonGuiDialogHandler *handler,
				CommonGuiMessageDialogOpenHandler *openHandler);
/* called on a message dialog just opened with a 16-byte block twice, zeroed by its caller */
int SceCommonGuiDialog_206623FF(int dialogId, void *arg1, void *arg2);
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
/* sets an open dialog's text */
int SceCommonGuiDialog_273A476A(int dialogId, const paf::wstring &text);
/* a dialog's widget by its index in the layout (1: its text, 4: its waiting animation, 10: its
   contents, 12: a part the processing dialog removes, as the trophy setup dialog uses them) */
paf::ui::Widget *SceCommonGuiDialog_8947FC23(int dialogId, int index);
/* called on a dialog just opened with a resource ID (the trophy setup dialog's 0xE0D9F918) */
int SceCommonGuiDialog_D3F78BA5(int dialogId, SceUInt32 id);

/*
 * The library's layouts, by their parts (1: the text, 2: a second text, 12 to 14: the buttons,
 * 18 and 19: the progress bar; the others are not known yet) and their buttons. The pages differ
 * between layouts with the same parts.
 */
extern const CommonGuiDialogLayout SceCommonGuiDialog_AF75CA6D;	/* text; OK (the default) */
extern const CommonGuiDialogLayout SceCommonGuiDialog_EF7F04DD;	/* text; cancel */
extern const CommonGuiDialogLayout SceCommonGuiDialog_6CD802FC;	/* text; no, yes */
extern const CommonGuiDialogLayout SceCommonGuiDialog_22B17F2B;	/* text; yes, no */
extern const CommonGuiDialogLayout SceCommonGuiDialog_78399261;	/* text; yes, no, cancel */
extern const CommonGuiDialogLayout SceCommonGuiDialog_92752DE2;	/* text; cancel, OK */
extern const CommonGuiDialogLayout SceCommonGuiDialog_446F8B49;	/* text, progress; cancel */
extern const CommonGuiDialogLayout SceCommonGuiDialog_25BD4084;	/* text, progress; cancel */
extern const CommonGuiDialogLayout SceCommonGuiDialog_95087497;	/* two texts, progress; cancel */
extern const CommonGuiDialogLayout SceCommonGuiDialog_EA557911;	/* two texts, progress; cancel */
extern const CommonGuiDialogLayout SceCommonGuiDialog_A0E0A437;	/* text, progress */
extern const CommonGuiDialogLayout SceCommonGuiDialog_7AEA192C;	/* text */
extern const CommonGuiDialogLayout SceCommonGuiDialog_5C8A2999;	/* text */
extern const CommonGuiDialogLayout SceCommonGuiDialog_E872FB5D;	/* parts 9, 10; part 8 */
extern const CommonGuiDialogLayout SceCommonGuiDialog_1424609E;	/* parts 9, 10; OK */
extern const CommonGuiDialogLayout SceCommonGuiDialog_3AC06BC6;	/* parts 9, 10; no, yes */
extern const CommonGuiDialogLayout SceCommonGuiDialog_DDC59760;	/* part 11; part 8 */
extern const CommonGuiDialogLayout SceCommonGuiDialog_3785AD25;	/* part 11; OK */
extern const CommonGuiDialogLayout SceCommonGuiDialog_2A68B4A4;	/* text (the small waiting dialog) */
extern const CommonGuiDialogLayout SceCommonGuiDialog_F8B2D7E1;	/* text; cancel */
extern const CommonGuiDialogLayout SceCommonGuiDialog_7D8FE85E;	/* part 5, text; OK */
extern const CommonGuiDialogLayout SceCommonGuiDialog_C4A714DE;	/* part 5, text; cancel, OK */
extern const CommonGuiDialogLayout SceCommonGuiDialog_52A53037;	/* part 5, text */
extern const CommonGuiDialogLayout SceCommonGuiDialog_B04C8C1A;	/* part 5, text; OK */
extern const CommonGuiDialogLayout SceCommonGuiDialog_DEDA75E4;	/* two texts; OK */
extern const CommonGuiDialogLayout SceCommonGuiDialog_04D71470;	/* text; buttons 7, cancel and 8 */
/* the colour that leaves a dialog's own */
extern const paf::math::v4 SceCommonGuiDialog_EF2C5382;
}

inline CommonGuiDialogParam::CommonGuiDialogParam()
{
	SceCommonGuiDialog_696DB9C9(this);
}

inline CommonGuiLayoutDialogParam::CommonGuiLayoutDialogParam()
	: wait(false), layout(SceCommonGuiDialog_AF75CA6D), textColor(SceCommonGuiDialog_EF2C5382),
	  scale(1.0f), handler(NULL), openHandler(NULL), updateHandler(NULL)
{
}

#endif
