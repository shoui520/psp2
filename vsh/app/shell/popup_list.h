/*
 * popup_list.h: the shell's popup list ("page_popup_list_view"), a menu of items opened over a
 * plugin's page, one per depth, as its callers fill it in. The code is at 0x81345000-0x81349000
 * and not written yet; this declares what its callers use. Every name here is ours
 * (PLACEHOLDER): no record or string of the shell names the list's class or fields.
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

/* PLACEHOLDER: an item of the list (24 bytes) */
struct ShellPopupListItem {
	ShellPopupListItem() : icon(NULL), flag0(false), flag1(false), value(0)
	{
	}

	ShellPopupListItem(const ShellPopupListItem &other)
		: text(other.text), icon(other.icon)
	{
		if (icon != NULL)
			icon->AddRef();
		flag0 = other.flag0;
		flag1 = other.flag1;
		value = other.value;
	}

	~ShellPopupListItem()
	{
		if (icon != NULL)
			icon->Release();
	}

	paf::wstring text;			/* its label */
	paf::graph::Surface *icon;		/* its image, released with it */
	bool flag0;				/* PLACEHOLDER: not read by its callers yet */
	bool flag1;				/* PLACEHOLDER: from the caller's item table */
	int value;
};

PAF_VECTOR_NOINLINE_PUSH_BACK(ShellPopupListItem, paf::allocator<ShellPopupListItem>)

/* the items' destructor out of line, as Sony calls it from every list's caller (0x81412E28) */
namespace paf {
template<> inline __attribute__((noinline))
vector<ShellPopupListItem, allocator<ShellPopupListItem> >::~vector()
{
	typedef ShellPopupListItem _Elem;

	PAF_VECTOR_DESTROY_ALL
}
}

/* PLACEHOLDER: what is called back: an item chosen (its index), and the list closed (its
   result) */
typedef void (*ShellPopupListSelectFunc)(int index, int value, void *arg);
typedef void (*ShellPopupListCloseFunc)(int result, void *arg);

/* PLACEHOLDER: a callback with its argument, as the list keeps it (vtables 0x8153D06C,
   0x8153D080); released by the list */
class ShellPopupListCallback {
public:
	virtual ~ShellPopupListCallback();
};

/* PLACEHOLDER: what the list calls for each item's widget it makes, with the item's index;
   released by the list (vtable 0x81530E84 for impose_plugin's) */
class ShellPopupListItemCallback {
public:
	virtual ~ShellPopupListItemCallback()
	{
	}

	virtual void operator()(paf::ui::Widget *item, int index) = 0;
};

/* PLACEHOLDER: the list's parameters (224 bytes), filled by its caller and given to
   shellPopupListOpen */
class ShellPopupListParam {
public:
	ShellPopupListParam(int type, paf::Plugin *plugin)
		: type(type), plugin(plugin), flags(16), selected(0), focus(-1), selectCallback(NULL)
	{
		callback[0] = NULL;
		callback[1] = NULL;
		callback[2] = NULL;
		itemCallback = NULL;
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
		offset[0] = 0;
		offset[1] = 0;
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
		extra[0] = 0;
		extra[1] = 0;
	}

	__attribute__((always_inline)) ~ShellPopupListParam()
	{
	}

	/* a chosen item's callback, the one kept before released (0x813450BC) */
	void SetSelectCallback(ShellPopupListSelectFunc func, void *arg);
	/* the closed list's callback, the one kept before released (0x813450F4) */
	void SetCloseCallback(ShellPopupListCloseFunc func, void *arg);

	int type;
	paf::Plugin *plugin;			/* whose strings and pages the list uses */
	int flags;
	paf::wstring title;
	int selected;
	int focus;
	paf::vector<paf::wstring> texts;	/* PLACEHOLDER: one per item, sized to them */
	paf::vector<ShellPopupListItem> items;
	ShellPopupListCallback *callback[3];	/* PLACEHOLDER: not set by its callers yet */
	ShellPopupListItemCallback *itemCallback;
	ShellPopupListCallback *selectCallback;	/* SetSelectCallback's */
	ShellPopupListCallback *closeCallback;	/* SetCloseCallback's */
	float origin[4];
	int margin[2];
	int columns;
	int rows;
	float minSize[4];
	float maxSize[4];
	int offset[2];
	float padding[2];
	paf::ui::Widget *anchor;		/* the widget the list opens at, when set */
	float anchorOffset[2];
	float bounds[4];
	bool show[4];
	int extra[2];
	paf::vector<int> marks;
};

/* PLACEHOLDER: the list opened at a depth (the count of lists open), the one under it
   dimmed (0x81348DA4) */
int shellPopupListOpen(ShellPopupListParam *param, unsigned int depth, int arg);
/* PLACEHOLDER: the lists from a depth up closed (0x81348DAC) */
int shellPopupListClose(unsigned int depth);
/* PLACEHOLDER: the widget of the list open on top when depth is 0, else NULL (0x81348DDA) */
paf::ui::Widget *shellPopupListGetWidget(unsigned int depth);

typedef char ShellPopupListParamSizeCheck[sizeof(ShellPopupListParam) == 224 ? 1 : -1];
typedef char ShellPopupListItemSizeCheck[sizeof(ShellPopupListItem) == 24 ? 1 : -1];

#endif
