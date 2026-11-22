/*
 * indicator_event_trophy.cpp: the item of a trophy in the notices' list, from NPXS10008: a
 * notified one looks like the icon item, the others like a friend's event with a white title
 * (0x8117ED90-0x8117EF12). The file's and all names here are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <math/ngp/v4.h>
#include <versatileparam/idparam.h>
#include <widget/core/widget.h>
#include <widget/w_composite_drawer.h>
#include <graphics/drawobj/text_obj.h>
#include "indicator.h"

using namespace paf;

/* (0x8117ED90) */
int IndicatorEventTrophyItem::Attach(ui::Widget *widget, IndicatorEventItemLook look)
{
	if (IsNotified())
		return IndicatorEventIconItem::Attach(widget, look);
	return IndicatorEventFriendItem::Attach(widget, look);
}

/* (0x8117EDC6) */
int IndicatorEventTrophyItem::SetText(ui::Widget *widget, IDParam id)
{
	if (IsNotified())
		return IndicatorEventIconItem::SetText(widget, id);

	int ret = IndicatorEventFriendItem::SetText(widget, id);

	if (ret < 0)
		return ret;
	if (widget != NULL && widget->IsInherit(ui::CompositeDrawer::TypeName())) {
		graph::TextObj *text = INDICATOR_ICON_TEXT(widget, INDICATOR_ICON_OBJ_TITLE);

		if (text != NULL)
			text->SetStyleAttribute(graph::TextStyleAttribute_Color, 0, 1,
						math::v4(1.0f, 1.0f, 1.0f, 1.0f));
	}
	return 0;
}

/* (0x8117EEEA) */
int IndicatorEventTrophyItem::Layout(ui::Widget *widget, bool animate)
{
	if (IsNotified())
		return IndicatorEventIconItem::Layout(widget, animate);
	return IndicatorEventFriendItem::Layout(widget, animate);
}
