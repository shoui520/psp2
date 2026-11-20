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

/*
 * What a dialog with its own layout calls with its ID as it opens, or every interval while it is
 * open: kept and deleted like its handler.
 */
class CommonGuiLayoutDialogOpenHandler {
public:
	virtual ~CommonGuiLayoutDialogOpenHandler()
	{
	}

	virtual void operator()(SceInt32 id) = 0;
};

/* what makes a widget for SceCommonGuiDialog_061DD12B, which deletes it */
class CommonGuiDialogWidgetFactory {
public:
	virtual ~CommonGuiDialogWidgetFactory()
	{
	}

	/* the widget made under the parent, or NULL */
	virtual paf::ui::Widget *Create(paf::ui::Widget *parent) = 0;
};

/* the factory of a plugin's template's widget */
class CommonGuiDialogTemplateFactory : public CommonGuiDialogWidgetFactory {
public:
	CommonGuiDialogTemplateFactory(paf::Plugin *plugin, const paf::IDParam &id)
		: m_plugin(plugin), m_template(id)
	{
	}

	virtual paf::ui::Widget *Create(paf::ui::Widget *parent);

	paf::Plugin *m_plugin;
	paf::IDParam m_template;
};

/* the function form of a handler, which SceCommonGuiDialog_6CE14F66 wraps in one itself */
typedef void (*CommonGuiDialogFunction)(SceInt32 value, SceInt32 button, void *arg);

/* the function form of an open handler (SceCommonGuiDialog_B1BA8D9E, SceCommonGuiDialog_F1A40838) */
typedef int (*CommonGuiDialogOpenFunction)(SceInt32 id, void *arg);

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
 * the framework's common resource plugin, the dialog's colours, and how its page opens.
 */
class CommonGuiDialogParam {
public:
	CommonGuiDialogParam();

	paf::Plugin *resourcePlugin;
	/* neither filled nor copied by the library */
	SceUInt32 reserved;
	/* the background's and the dimmer's colours, RGBA from 0 to 1; SceCommonGuiDialog_EF2C5382 by
	   default (the dialog's own) */
	float bgColor[4];
	float dimmerColor[4];
	/* the resource ID of the sound the dialog plays as it closes; 0 for none */
	SceUInt32 soundId;
	/* the plane behind the dialog: 0 dims the screen, 1 leaves it */
	SceInt32 dimmer;
	/* the page's priority; -1 for the library's default */
	SceInt32 priority;
	/* the call returns once the dialog's transition has ended */
	bool waitTransition;
	/* the handler is called (with button 0) when the dialog closes without one */
	bool notifyClose;
	/* an open dialog whose page is used again; -1 for a new one */
	SceInt32 dialogId;
	/* where the new dialog's ID is written, or NULL */
	SceInt32 *outId;
	/* the page's PageOpenParam option */
	SceInt32 pageOption;
};

typedef char CommonGuiDialogParamSize[sizeof(CommonGuiDialogParam) == 68 ? 1 : -1];

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

	/* declared, so SNC inlines it into large callers as in Sony's builds */
	~CommonGuiLayoutDialogParam()
	{
	}

	/* the call waits until the dialog closes, and returns its result */
	bool wait;
	CommonGuiDialogLayout layout;
	paf::wstring heading;
	paf::wstring text;
	/* the text's colour; SceCommonGuiDialog_EF2C5382 by default */
	paf::math::v4 textColor;
	/* the milliseconds between the update handler's calls */
	float interval;
	paf::intrusive_ptr<paf::graph::Surface> image;
	paf::intrusive_ptr<paf::cache::CacheImageInterface> cachedImage;
	paf::intrusive_ptr<paf::graph::Surface> secondImage;
	CommonGuiDialogHandler *handler;
	CommonGuiLayoutDialogOpenHandler *openHandler;
	/* called every interval milliseconds while the dialog is open */
	CommonGuiLayoutDialogOpenHandler *updateHandler;
};

typedef char CommonGuiLayoutDialogParamSize[sizeof(CommonGuiLayoutDialogParam) == 160 ? 1 : -1];

/*
 * The parameters of the PlayStation Plus dialog (SceCommonGuiDialog_D6B6809B): its two buttons
 * open the PlayStation Store's PlayStation Plus page, and every button is passed on to the handler,
 * which the library takes over.
 */
class CommonGuiPlusDialogParam : public CommonGuiDialogParam {
public:
	CommonGuiPlusDialogParam() : handler(NULL)
	{
	}

	CommonGuiDialogHandler *handler;
};

/*
 * A line of a list dialog: a mark and a text, each with its colour (SceCommonGuiDialog_EF2C5382
 * leaves the layout's), and a button when its label is not empty, whose value the dialog closes
 * with.
 */
struct CommonGuiListDialogItem {
	CommonGuiListDialogItem();

	/* the line has its mark; both texts are drawn with a shadow */
	bool mark : 1;
	bool shadow : 1;
	paf::math::v4 markColor;
	paf::wstring text;
	paf::math::v4 textColor;
	paf::wstring buttonLabel;
	paf::math::v4 buttonColor;
	SceInt32 buttonValue;
};

typedef char CommonGuiListDialogItemSize[sizeof(CommonGuiListDialogItem) == 96 ? 1 : -1];

/* the parameters of a list dialog (SceCommonGuiDialog_F3D37CAE) */
class CommonGuiListDialogParam : public CommonGuiDialogParam {
public:
	/* the list dialog's defaults (SceCommonGuiDialog_BAC27433 and SceCommonGuiDialog_149B7ABB) */
	CommonGuiListDialogParam();

	/* the layout: 1 with an OK button, 2 without buttons, 3 with no and yes; else the panel's */
	SceInt32 type;
	/* the dialog is fitted to its lines */
	bool fit;
	paf::wstring heading;
	paf::math::v4 headingColor;
	/* the call waits until the dialog closes, and returns its result */
	bool wait;
	SceInt32 *outId;
	paf::vector<CommonGuiListDialogItem> items;
	CommonGuiDialogHandler *handler;
};

/*
 * The parameters of a text input dialog (SceCommonGuiDialog_7C019CF0): its text and a second
 * text, the text box's length and IME type, where the text typed is written when OK closes it,
 * and its handler.
 */
class CommonGuiInputDialogParam : public CommonGuiDialogParam {
public:
	/* the call waits until the dialog closes, and returns its result */
	bool wait;
	paf::wstring text;
	paf::wstring subText;
	CommonGuiDialogHandler *handler;
	/* the text box's maximum length */
	SceUInt32 maxLength;
	/* where the text typed is written (UTF-8) when OK closes the dialog, or NULL */
	paf::string *outText;
	/* the layout with a third button, which the text box and the other buttons move down to */
	bool thirdButton;
	/* the text box's IME: 1 numbers (SCE_IME_TYPE_NUMBER), 2 extended numbers
	   (SCE_IME_TYPE_EXTENDED_NUMBER), 3 the IME's type 9; else the text box's own */
	SceInt32 imeType;
};

/* an image of the video dialog: its picture (or its cached picture) and the image beside it */
struct CommonGuiVideoDialogImage {
	paf::intrusive_ptr<paf::graph::Surface> image;
	paf::intrusive_ptr<paf::cache::CacheImageInterface> cachedImage;
	paf::intrusive_ptr<paf::graph::Surface> icon;
};

/*
 * The parameters of the video player's dialog (SceCommonGuiDialog_5B9511E9): its heading and
 * text, two images with their titles, its check box, and its buttons.
 */
class CommonGuiVideoDialogParam : public CommonGuiDialogParam {
public:
	CommonGuiVideoDialogImage firstImage;
	paf::wstring heading;
	paf::wstring firstTitle;
	CommonGuiVideoDialogImage secondImage;
	paf::wstring secondTitle;
	paf::wstring secondDetail;
	paf::wstring text;
	/* the check box (SceCommonGuiDialog_ED620EF1 reads it) is shown */
	bool checkBox;
	/* the buttons: 0 the first pair, 1 the second pair, 2 all three; else the panel's */
	SceInt32 type;
	CommonGuiDialogHandler *handler;
};

typedef char CommonGuiVideoDialogParamSize[sizeof(CommonGuiVideoDialogParam) == 164 ? 1 : -1];

extern "C" {
/* the parameters' defaults: both NIDs are the one constructor (C2 and C1) */
CommonGuiDialogParam *SceCommonGuiDialog_3EA370C9(CommonGuiDialogParam *param);
CommonGuiDialogParam *SceCommonGuiDialog_696DB9C9(CommonGuiDialogParam *param);
/* lets a dialog go: once nothing holds it, it closes */
void SceCommonGuiDialog_BC311DC0(int dialogId);
/* sets the result a call waiting for the dialog returns: 0, or -1 without the dialog */
int SceCommonGuiDialog_2CA16BBF(int dialogId, int result);
/* the page priority of the dialogs opened without one */
int SceCommonGuiDialog_694696F3(void);
void SceCommonGuiDialog_5057376C(int priority);
/* waits until the dialog has closed: its result, or -1 without the dialog */
int SceCommonGuiDialog_D2381B88(int dialogId);
/* has the handler called with the dialog's ID every interval milliseconds while it is open */
void SceCommonGuiDialog_17091699(int dialogId, CommonGuiLayoutDialogOpenHandler *handler,
				 float interval);
/* has the handler called with the dialog's ID once it has opened: 0, or -1 without the dialog */
int SceCommonGuiDialog_0BA67FE0(int dialogId, CommonGuiLayoutDialogOpenHandler *handler);
/* the resource ID of the sound the dialog plays as it closes */
int SceCommonGuiDialog_D3F78BA5(int dialogId, SceUInt32 id);
/* a dialog's widget by its index in the layout (1: its text, 4: its waiting animation, 10: its
   contents, 12: a part the processing dialog removes, as the trophy setup dialog uses them) */
paf::ui::Widget *SceCommonGuiDialog_8947FC23(int dialogId, int index);
/* the colour of the dialog's dimmer (part 3) and of its heading (part 6); the trophy setup
   dialog passes a third argument the library does not read */
void SceCommonGuiDialog_206623FF(int dialogId, const paf::math::v4 &color,
				 const paf::math::v4 &unused);
void SceCommonGuiDialog_2BBC4734(int dialogId, const paf::math::v4 &color);
/* the colour of the dialog's background and dividers */
void SceCommonGuiDialog_0C2B57DC(int dialogId, const paf::math::v4 &color);
/* the image of the dialog's part */
void SceCommonGuiDialog_52AB228A(int dialogId, int index,
				 paf::intrusive_ptr<paf::graph::Surface> image);
/* shows a dialog with its own layout: its ID, or a negative error */
int SceCommonGuiDialog_CCFCD9A6(CommonGuiLayoutDialogParam *param);
/* shows a dialog with the heading, text and layout, which calls the handler (the library takes
   it over): its ID, or a negative error */
int SceCommonGuiDialog_FA31832E(paf::Plugin *plugin, const paf::wstring &heading,
				const paf::wstring &text, const CommonGuiDialogLayout *layout,
				CommonGuiDialogHandler *handler);
/* the same with the function form of the handler */
int SceCommonGuiDialog_6CE14F66(paf::Plugin *plugin, const paf::wstring &heading,
				const paf::wstring &text, const CommonGuiDialogLayout *layout,
				CommonGuiDialogFunction func, void *arg);
/* holds a dialog: one more SceCommonGuiDialog_BC311DC0 before it closes */
void SceCommonGuiDialog_D0B729B2(int dialogId);
/* the same as SceCommonGuiDialog_BC311DC0 */
void SceCommonGuiDialog_D222DCE5(int dialogId);
/* has func(id, arg) called once the dialog has opened: 0, or -1 without the dialog */
int SceCommonGuiDialog_B1BA8D9E(int dialogId, CommonGuiDialogOpenFunction func, void *arg);
/* has func(id, arg) called every interval milliseconds while the dialog is open */
void SceCommonGuiDialog_F1A40838(int dialogId, CommonGuiDialogOpenFunction func, void *arg,
				 float interval);
/* stops the interval's calls */
void SceCommonGuiDialog_47BEE7F8(int dialogId);
/* the page's debug mode */
void SceCommonGuiDialog_F9903161(int dialogId, bool on);
/* sets an open dialog's heading, text, or the text part at the index: 0, or an error */
int SceCommonGuiDialog_7775C6B6(int dialogId, const paf::wstring &heading);
int SceCommonGuiDialog_273A476A(int dialogId, const paf::wstring &text);
int SceCommonGuiDialog_22475C30(int dialogId, int index, const paf::wstring &text);
/* the colour of the dialog's background: 0, or an error */
int SceCommonGuiDialog_6A6F5DEE(int dialogId, paf::math::v4 *color);
/* shows or hides the dialog's dividers */
void SceCommonGuiDialog_052D0230(int dialogId, bool show);
/* closes a dialog with a result: 0, or -1 without the dialog */
int SceCommonGuiDialog_01702008(int dialogId, int result);
/* shows a dialog, calls the handler once it has opened (the library takes it over) and waits
   until it closes: its result, or a negative error */
int SceCommonGuiDialog_B59AE4AD(paf::Plugin *plugin, const paf::wstring &heading,
				const paf::wstring &text, const CommonGuiDialogLayout *layout,
				CommonGuiLayoutDialogOpenHandler *handler);
/* the same with func(id, arg) for the handler */
int SceCommonGuiDialog_EB559AFE(paf::Plugin *plugin, const paf::wstring &heading,
				const paf::wstring &text, const CommonGuiDialogLayout *layout,
				CommonGuiDialogOpenFunction func, void *arg);
/* adds the widget the factory makes beside the dialog's part, and deletes the factory: 0, or
   -1 */
int SceCommonGuiDialog_061DD12B(int dialogId, int index, CommonGuiDialogWidgetFactory *factory);
/* shows a message dialog: returns when it is closed */
int SceCommonGuiDialog_20B141F1(CommonGuiMessageDialogParam *param);
/* shows a message dialog with the plugin's resources and the two handlers, which the library
   takes over: its ID, or a negative error */
int SceCommonGuiDialog_A6422883(paf::Plugin *plugin, CommonGuiMessageDialogParam *param,
				CommonGuiDialogHandler *handler,
				CommonGuiMessageDialogOpenHandler *openHandler);
/* shows the PlayStation Plus dialog: its ID, or a negative error */
int SceCommonGuiDialog_D6B6809B(const CommonGuiPlusDialogParam *param);
/* shows a list dialog: its ID (its result when it waits), or a negative error */
int SceCommonGuiDialog_F3D37CAE(const CommonGuiListDialogParam *param);
/* sets a line: its text, with its mark unless plain, in the list dialog's colour, without a
   button */
void SceCommonGuiDialog_96B9C9F6(CommonGuiListDialogItem *item, SceInt32 plain,
				const paf::wstring &text);
/* sets a line with a button: its label and the value the dialog closes with */
void SceCommonGuiDialog_2FA91F45(CommonGuiListDialogItem *item, SceInt32 plain,
				const paf::wstring &text, const paf::wstring &buttonLabel,
				SceInt32 buttonValue);
/* the same on the parameters' line, which is added (with the lines before it) if missing */
void SceCommonGuiDialog_D44D1EFB(CommonGuiListDialogParam *param, SceUInt32 index,
				SceInt32 plain, const paf::wstring &text);
void SceCommonGuiDialog_6A0E7F14(CommonGuiListDialogParam *param, SceUInt32 index,
				SceInt32 plain, const paf::wstring &text,
				const paf::wstring &buttonLabel, SceInt32 buttonValue);
/* shows a list dialog whose cancel button takes the focus within its group: as
   SceCommonGuiDialog_F3D37CAE */
int SceCommonGuiDialog_BACBA7F2(const CommonGuiListDialogParam *param);
/* the text typed into a text input dialog, as UTF-8 */
paf::string SceCommonGuiDialog_1D4E3C83(int dialogId);
/* shows a text input dialog: its ID (its result when it waits), or a negative error */
int SceCommonGuiDialog_7C019CF0(const CommonGuiInputDialogParam *param);
/* the same as SceCommonGuiDialog_BC311DC0 */
void SceCommonGuiDialog_11D35CB5(int dialogId);
/* whether the video dialog's check box is checked: 0, or an error */
int SceCommonGuiDialog_ED620EF1(int dialogId, bool *checked);
/* shows the video player's dialog: its ID, or a negative error */
int SceCommonGuiDialog_5B9511E9(const CommonGuiVideoDialogParam *param);
/* the same as SceCommonGuiDialog_BC311DC0 */
void SceCommonGuiDialog_C4601FBE(int dialogId);

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

inline CommonGuiListDialogItem::CommonGuiListDialogItem()
	: mark(true), shadow(false), markColor(SceCommonGuiDialog_EF2C5382),
	  textColor(SceCommonGuiDialog_EF2C5382), buttonColor(SceCommonGuiDialog_EF2C5382)
{
}

/* the layout a layout dialog takes by default; the library's own code takes its parts as
   constants (layouts.h) */
#ifndef COMMON_GUI_DIALOG_DEFAULT_LAYOUT
#define COMMON_GUI_DIALOG_DEFAULT_LAYOUT	SceCommonGuiDialog_AF75CA6D
#endif

inline CommonGuiLayoutDialogParam::CommonGuiLayoutDialogParam()
	: wait(false), layout(COMMON_GUI_DIALOG_DEFAULT_LAYOUT), textColor(SceCommonGuiDialog_EF2C5382),
	  interval(1.0f), handler(NULL), openHandler(NULL), updateHandler(NULL)
{
}

#endif
