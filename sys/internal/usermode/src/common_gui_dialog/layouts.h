/*
 * The library's layouts as its own code takes them: the pages, parts and buttons of the layouts
 * it opens itself, and the layouts as constants made of them, which its code copies field by
 * field. Included before the public header, so a layout dialog's parameters take their default
 * layout from here.
 */
#ifndef COMMON_GUI_DIALOG_LAYOUTS_H
#define COMMON_GUI_DIALOG_LAYOUTS_H

#include <versatileparam/idparam.h>
#include <widget/w_dialog.h>

struct CommonGuiDialogPage;
struct CommonGuiDialogButton;

extern const CommonGuiDialogPage g_pageAF75CA6D;
extern const CommonGuiDialogPage g_pageInternal;
extern const CommonGuiDialogPage g_pageE872FB5D;
extern const CommonGuiDialogPage g_page1424609E;
extern const CommonGuiDialogPage g_page3AC06BC6;
extern const CommonGuiDialogPage g_page04D71470;

extern const CommonGuiDialogButton g_okButtons[];
extern const CommonGuiDialogButton g_noYesButtons[];
extern const CommonGuiDialogButton g_04D71470Buttons[];
extern const CommonGuiDialogButton g_panelButtons[];
extern const CommonGuiDialogButton g_noButtons[];

extern paf::ui::Dialog::PartsList g_partsAF75CA6D[];
extern paf::ui::Dialog::PartsList g_partsInternal[];
extern paf::ui::Dialog::PartsList g_partsE872FB5D[];
extern paf::ui::Dialog::PartsList g_parts1424609E[];
extern paf::ui::Dialog::PartsList g_parts3AC06BC6[];
extern paf::ui::Dialog::PartsList g_parts04D71470[];

/* the default layout a layout dialog's parameters start with: the constant below */
struct CommonGuiDialogLayout;
static inline const CommonGuiDialogLayout &DefaultLayout();

#define COMMON_GUI_DIALOG_DEFAULT_LAYOUT	DefaultLayout()

#include <common_gui_dialog/common_gui_dialog.h>

/* the exported layouts the library's own code opens, as constants */
static const CommonGuiDialogLayout s_layoutAF75CA6D = {
	&g_pageAF75CA6D, g_partsAF75CA6D, g_okButtons
};

static inline const CommonGuiDialogLayout &DefaultLayout()
{
	return s_layoutAF75CA6D;
}

static const CommonGuiDialogLayout s_layoutE872FB5D = {
	&g_pageE872FB5D, g_partsE872FB5D, g_panelButtons
};

static const CommonGuiDialogLayout s_layout1424609E = {
	&g_page1424609E, g_parts1424609E, g_okButtons
};

static const CommonGuiDialogLayout s_layout3AC06BC6 = {
	&g_page3AC06BC6, g_parts3AC06BC6, g_noYesButtons
};

static const CommonGuiDialogLayout s_layout04D71470 = {
	&g_page04D71470, g_parts04D71470, g_04D71470Buttons
};

/* parts 9 and 10 without buttons, which SceCommonGuiDialog_F3D37CAE takes for its type 2: the
   library's one layout it does not export */
extern const CommonGuiDialogLayout g_contentsLayoutNoButtons;

static const CommonGuiDialogLayout s_contentsLayoutNoButtons = {
	&g_pageInternal, g_partsInternal, g_noButtons
};

#endif
