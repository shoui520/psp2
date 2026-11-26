/*
 * popup_list.h: the shell's popup list ("page_popup_list_view"), a stackable
 * popup menu opened over a plugin's page (0x813450BC..0x81348E6F).
 */
#ifndef _SHELL_POPUP_LIST_H
#define _SHELL_POPUP_LIST_H

#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <std/vector>
#include <graphics/surface/surface.h>
#include <widget/core/widget.h>

namespace paf {
namespace ui {
namespace listview {
class ItemFactory;
}
}
}

/* An item of the popup list (24 bytes) */
struct ShellPopupListItem {
	ShellPopupListItem() : icon(NULL), disabled(false), hasSubMenu(false), value(0)
	{
	}

	ShellPopupListItem(const ShellPopupListItem &other)
		: text(other.text), icon(other.icon)
	{
		if (icon != NULL)
			icon->AddRef();
		disabled = other.disabled;
		hasSubMenu = other.hasSubMenu;
		value = other.value;
	}

	~ShellPopupListItem()
	{
		if (icon != NULL)
			icon->Release();
	}

	paf::wstring text;
	paf::graph::Surface *icon;
	bool disabled;
	bool hasSubMenu;
	int value;
};

PAF_VECTOR_NOINLINE_PUSH_BACK(ShellPopupListItem, paf::allocator<ShellPopupListItem>)

/* Out-of-line vector destructor matching callers (0x81412E28) */
namespace paf {
template<> inline __attribute__((noinline))
vector<ShellPopupListItem, allocator<ShellPopupListItem> >::~vector()
{
	typedef ShellPopupListItem _Elem;

	PAF_VECTOR_DESTROY_ALL
}
}

/* Selection and close callbacks */
typedef void (*ShellPopupListSelectFunc)(int index, int value, void *userContext);
typedef void (*ShellPopupListCloseFunc)(int result, void *userContext);

/* Base callback functor released by the popup list */
class ShellPopupListCallback {
public:
	virtual ~ShellPopupListCallback()
	{
	}
};

class ShellPopupListSelectCallback : public ShellPopupListCallback {
public:
	virtual ~ShellPopupListSelectCallback()
	{
	}

	virtual void OnSelect(int index, const ShellPopupListItem *item) = 0;
};

class ShellPopupListCloseCallback : public ShellPopupListCallback {
public:
	virtual ~ShellPopupListCloseCallback()
	{
	}

	virtual void OnClose(int result) = 0;
};

/* Callback invoked when each item widget is constructed; released by the list */
class ShellPopupListItemCallback {
public:
	virtual ~ShellPopupListItemCallback()
	{
	}

	virtual void operator()(paf::ui::Widget *item, int index) = 0;
};

/* Callout anchor direction rule (36 bytes) */
struct ShellPopupListCalloutRule {
	SceUInt32 directionMask;
	float minAngle;
	float maxAngle;
	SceUInt32 alignX;
	SceUInt32 alignY;
	float offsetX;
	float offsetY;
	SceUInt32 textureHash;
	bool flipHorizontal;
};

/* Optional popup list style override (56 bytes) */
struct ShellPopupListStyle {
	ShellPopupListStyle() : bgColorFlags(0), bgColorRgba(0)
	{
		for (int i = 0; i < 4; ++i) {
			itemTextureHash[i][0] = 0;
			itemTextureHash[i][1] = 0;
		}
	}

	SceUInt32 bgColorFlags;
	SceUInt32 bgColorRgba;
	SceUInt32 itemTextureHash[4][2];
	paf::vector<ShellPopupListCalloutRule> customCallouts;
};

/* Popup list open parameters (232 bytes with 8-byte alignment for origin/minSize/maxSize) */
class ShellPopupListParam {
public:
	ShellPopupListParam(int listType, paf::Plugin *ownerPlugin)
		: type(listType), plugin(ownerPlugin), flags(16), selected(0), focus(-1),
		  customFactory(NULL), itemCallback(NULL), selectCallback(NULL),
		  closeCallback(NULL), checkMarkSide(listType == 1 ? 1 : 0),
		  styleOverride(NULL)
	{
		origin[0] = 0.0f;
		origin[1] = 0.0f;
		origin[2] = 0.0f;
		origin[3] = 0.0f;
		margin[0] = 0;
		margin[1] = 0;
		columns = 4;
		rows = 4;
		minSize[0] = 200.0f;
		minSize[1] = 0.0f;
		minSize[2] = 0.0f;
		minSize[3] = 0.0f;
		maxSize[0] = 760.0f;
		maxSize[1] = 0.0f;
		maxSize[2] = 0.0f;
		maxSize[3] = 0.0f;
		offset[0] = 0.0f;
		offset[1] = 0.0f;
		padding[0] = 20.0f;
		padding[1] = 20.0f;
		anchor = NULL;
		anchorOffset[0] = 0.0f;
		anchorOffset[1] = 0.0f;
		bounds[0] = 240.0f;
		bounds[1] = -240.0f;
		bounds[2] = -480.0f;
		bounds[3] = 480.0f;
		show[0] = true;
		show[1] = true;
		show[2] = true;
		show[3] = true;
		extra[0] = 0.0f;
		extra[1] = 0.0f;
	}

	__attribute__((always_inline)) ~ShellPopupListParam()
	{
	}

	/* Set item-chosen callback, deleting any previous one (0x813450BC) */
	void SetSelectCallback(ShellPopupListSelectFunc func, void *userContext);
	/* Set list-closed callback, deleting any previous one (0x813450F4) */
	void SetCloseCallback(ShellPopupListCloseFunc func, void *userContext);

	int type;
	paf::Plugin *plugin;
	int flags;
	paf::wstring title;
	int selected;
	int focus;
	paf::vector<paf::wstring> texts;
	paf::vector<ShellPopupListItem> items;
	paf::ui::listview::ItemFactory *customFactory;
	ShellPopupListItemCallback *itemCallback;
	ShellPopupListSelectCallback *selectCallback;
	ShellPopupListCloseCallback *closeCallback;
	int checkMarkSide;
	int reservedAlign;
	float origin[4];
	int margin[2];
	int columns;
	int rows;
	float minSize[4];
	float maxSize[4];
	float offset[2];
	float padding[2];
	paf::ui::Widget *anchor;
	float anchorOffset[2];
	float bounds[4];
	bool show[4];
	float extra[2];
	paf::vector<int> marks;
	const ShellPopupListStyle *styleOverride;
};

/* Open a popup list at stack depth (0x81348DA4) */
int shellPopupListOpen(ShellPopupListParam *param, unsigned int depth, int option);
/* Close the topmost popup list if stack depth exceeds depth (0x81348DAC) */
int shellPopupListClose(unsigned int depth);
/* Return the active popup list's ListView widget when depth == 0, else NULL (0x81348DDA) */
paf::ui::Widget *shellPopupListGetWidget(unsigned int depth);

typedef char ShellPopupListParamSizeCheck[sizeof(ShellPopupListParam) == 228 ? 1 : -1];
typedef char ShellPopupListItemSizeCheck[sizeof(ShellPopupListItem) == 24 ? 1 : -1];

#endif
