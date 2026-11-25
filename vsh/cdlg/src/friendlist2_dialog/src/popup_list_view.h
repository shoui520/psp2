/*
 * SceFriendList2DialogPlugin: PopupListView ("page_popup_list_view",
 * "popup_list_view_list_item", "popup_list_view_sub_menu_mark").
 */
#ifndef FRIENDLIST2_DIALOG_POPUP_LIST_VIEW_H
#define FRIENDLIST2_DIALOG_POPUP_LIST_VIEW_H

#include <stdint.h>
#include <scetypes.h>
#include <std/string>
#include <std/vector>
#include <plugin.h>
#include <graphics/surface/surface.h>
#include <widget/core/widget.h>

/* Single entry in a popup menu list */
struct PopupListEntry {
	PopupListEntry() : icon(), hasSubMenu(false), disabled(false), userValue(0) {}

	paf::wstring label;
	paf::intrusive_ptr<paf::graph::Surface> icon;
	bool hasSubMenu;
	bool disabled;
	int32_t userValue;
};

/* Callback invoked when the user chooses an item or cancels the popup list */
class PopupListCallback {
public:
	virtual ~PopupListCallback() {}
	virtual void OnSelect(int32_t selectedIndex) = 0;
};

/* Parameters for opening a popup list menu */
struct PopupListOpenParam {
	PopupListOpenParam()
		: depth(1), plugin(NULL), anchorWidget(NULL), selectedIndex(-1), callback(NULL)
	{
	}

	int32_t depth;
	paf::Plugin *plugin;
	paf::ui::Widget *anchorWidget;
	int32_t selectedIndex;
	paf::vector<PopupListEntry> items;
	PopupListCallback *callback;
};

void PopupListInit(void);
void PopupListClose(void);
int PopupListOpen(const PopupListOpenParam &param);
bool PopupListIsOpen(void);

#endif /* FRIENDLIST2_DIALOG_POPUP_LIST_VIEW_H */
