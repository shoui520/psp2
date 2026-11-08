/*
 * paf::ui::ListItem: a widget a list view makes for one of its cells (listview::Cell), with the
 * properties of its cell and the text it scrolls with its friends. The classes, TypeName, the
 * parameter names and the exports are Sony's (export NIDs hash as them; "list_item",
 * "list_item_base", "style_list_item", "sound_list_item", "text_auto_scroll_target" are its
 * strings); the members' names are ours.
 */
#ifndef PAF_WIDGET_W_LIST_ITEM_H
#define PAF_WIDGET_W_LIST_ITEM_H

#include <stdint.h>
#include <common/property_list.h>
#include <graphics/drawobj/plane_obj.h>
#include <math/ngp/v2.h>
#include <versatileparam/idparam.h>
#include <widget/core/widget.h>

namespace paf {
namespace ui {

class ListView;

namespace listview {
class Cell;
}

class ListItem : public Widget {
public:
	enum {
		OBJ_BG = 0
	};

	static const char m_widget_type[];
	static const char *TypeName();

	/* the list item the widget is in, or NULL */
	static ListItem *GetListItem(Widget *widget);

	ListItem(Widget *parent, CreateParam *param);
	virtual ~ListItem();

	/* a press (gesture 0x41) of a tappable item takes the focus */
	virtual int32_t HandleTouchEvent(Event *e);
	/* a drag (gesture 14) is passed to its list view's cell drag */
	virtual int32_t HandleGestureEvent(Event *e);
	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	virtual uint32_t CheckState();
	virtual void UpdateSize();
	/* its list view lets go of its cell once no touch is captured */
	virtual void OnTouchCaptureChanged(uint32_t port, uint32_t id, int32_t arg);
	virtual int32_t SetTexture(const intrusive_ptr<graph::Surface> &surf, int32_t obj,
				   uint32_t index);
	virtual int32_t GetTexture(intrusive_ptr<graph::Surface> &surf, int32_t obj, uint32_t index);
	using Widget::SetTexture;
	using Widget::GetTexture;

	int32_t GetSegmentIndex();
	int32_t GetCellIndex();
	ListView *GetListView();
	/* the touch dragged it from the item, or the touch hitting it where index is below 0 */
	bool BeginDrag(int32_t index);
	void EndDrag(int32_t index);

	/* the focus moved to the item, or from it the way given where it already has it, to the
	   widget that takes it; true where it moved (no name found) */
	bool ScePafWidget_C98D1926(const math::v2 &dir);
	/* its focus request cleared (no name found) */
	void ScePafWidget_1F9DB221();

	/* the text auto scroll target (ListItemParam's Set; name ours) */
	void SetTextAutoScrollTarget(IDParam id)
	{
		m_text_auto_scroll_target = id;
	}
	/* the text auto scroll target, a copy (name ours) */
	IDParam GetTextAutoScrollTarget()
	{
		return m_text_auto_scroll_target;
	}

	/* its cell */
	listview::Cell *m_cell;
	/* set by ScePafWidget_C98D1926, cleared by ScePafWidget_1F9DB221 */
	bool m_focus_requested;
	/* its cell's properties */
	common::property_list::PropertyList m_property_list;
	IDParam m_text_auto_scroll_target;
};

/* the widget the focus goes to from the item in the direction, or from the widget where it is
   given; NULL where it stays; reserved is not read (names ours) */
Widget *SearchListItemFocus(ListItem *item, math::v2 dir, Widget *from, int32_t reserved);
/* the child of the widget the focus may go to: a state button or button by the mode's check (0
   shown, 1 input enabled, 2 any), else a text box, text or plane shown, a plane only where it
   listens to event 0x20004 (names ours) */
Widget *FindListItemFocusChild(Widget *widget, int32_t mode);

/* a list item's parameters: the text auto scroll target */
class ListItemParam : public WidgetParam {
public:
	ListItemParam();

	virtual const char *GetParamName() const;
	virtual int32_t Set();
	virtual int32_t Get();
	virtual int32_t Create(int32_t mode);

	IDParam m_text_auto_scroll_target;
};

/* a list item's style: its background plane */
class ListItemStyleParam : public WidgetStyleParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	ListItemStyleParam();
	virtual const char *GetParamName() const;

	graph::PlaneObjParam m_bg_obj;
};

class ListItemSoundParam : public WidgetSoundParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	ListItemSoundParam();
	virtual const char *GetParamName() const;
};

}
}

#endif
