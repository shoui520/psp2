/*
 * indicator_event_friend.cpp: the item of a friend's event in the notices' list: the icon
 * item with the event's description in a second text object under its title, both laid out
 * by the title's lines (0x8117E12C-0x8117ED90). The file's and all names here are ours
 * (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <common/shared_ptr.h>
#include <common/string_util.h>
#include <math/ngp/v4.h>
#include <versatileparam/idparam.h>
#include <widget/core/widget.h>
#include <widget/w_composite_drawer.h>
#include <graphics/drawobj/text_obj.h>
#include <graphics/drawobj/plane_obj.h>
#include "indicator.h"

using namespace paf;

/* the event's description in UTF-16 */
static inline wstring indicatorFriendUtf16(const string &text)
{
	wstring result;

	common::Utf8ToUtf16(text, &result, 0);
	return result;
}

/* the icon item's drawer with the description's text object under the title (0x8117E12C) */
int IndicatorEventFriendItem::Attach(ui::Widget *widget, IndicatorEventItemLook look)
{
	int ret = IndicatorEventIconItem::Attach(widget, look);

	if (ret != 0)
		return ret;

	ui::Widget *drawer = INDICATOR_ICON_DRAWER(widget);

	if (drawer != NULL)
		SetSubText(drawer, IDParam(s_empty_name, INDICATOR_TASK_SUBTEXT));
	return ret;
}

/* the icon item's title, the event's own title in it, moved up for the description
   (0x8117E372) */
int IndicatorEventFriendItem::SetText(ui::Widget *widget, IDParam id)
{
	int ret = IndicatorEventIconItem::SetText(widget, id);

	if (ret < 0)
		return ret;
	if (widget != NULL && widget->IsInherit(ui::CompositeDrawer::TypeName())) {
		graph::TextObj *text = INDICATOR_ICON_TEXT(widget, INDICATOR_ICON_OBJ_TITLE);

		if (text != NULL) {
			if (m_event.title.length() != 0) {
				common::String title(m_event.title);

				text->SetString(title.GetWString());
			}
			if (GetMode() == 2) {
				math::v3 pos = text->m_pos;

				pos.set_y(-29.0f);
				text->SetPos(pos);
				text->SetStyleAttribute(graph::TextStyleAttribute_Point, 0, 0, 20.0f);
			} else {
				float y = GetMode() == 0 ? -37.0f : -41.0f;
				math::v3 pos = text->m_pos;

				pos.set_y(y);
				text->SetPos(pos);
			}
			text->SetLayoutAttribute(graph::TextLayoutAttribute_WordWrap, false);
		}
	}
	return 0;
}

/* the description's text object, made when the drawer has none, placed by the list's mode
   (0x8117E518) */
int IndicatorEventFriendItem::SetSubText(ui::Widget *widget, IDParam id)
{
	int ret = -1;

	if (widget == NULL || !widget->IsInherit(ui::CompositeDrawer::TypeName()))
		return ret;

	common::String str(indicatorFriendUtf16(m_event.desc));

	if (str.GetWString().length() == 0)
		return ret;

	graph::TextObj *text = INDICATOR_ICON_TEXT(widget, INDICATOR_ICON_OBJ_SUBTEXT);

	if (text == NULL) {
		common::SharedPtr<graph::DrawObj> obj(new graph::TextObj(graph::TextObjStyle()));

		((ui::CompositeDrawer *)widget)->InsertDrawObj(obj, INDICATOR_ICON_OBJ_SUBTEXT,
							       false);
		text = obj.get() != NULL && obj->IsInherit(graph::TextObj::DrawObjType()) ?
			       (graph::TextObj *)obj.get() : NULL;
	}
	if (text != NULL) {
		text->SetVisible(false);
		text->SetString(str.GetWString());
		text->SetStyleAttribute(graph::TextStyleAttribute_Point, 0, 0, 16.0f);
		text->SetLayoutAttribute(graph::TextLayoutAttribute_AdjustY, true);
		text->SetLayoutAttribute(graph::TextLayoutAttribute_Ellipsis, true);
		text->SetLayoutAttribute(graph::TextLayoutAttribute_WordWrap, false);
		text->SetLayoutAttribute(graph::TextLayoutAttribute_MaxLineCount, 2);

		int mode = GetMode();

		if (mode == 2) {
			text->SetSize(math::v3(337.0f, 0.0f, 0.0f));
			text->SetPos(math::v3(20.5f, -47.0f, 0.0f));
			text->SetLayoutAttribute(graph::TextLayoutAttribute_LinePitch, 16.0f);
			text->SetStyleAttribute(graph::TextStyleAttribute_Color, 0, 0,
						math::v4(0.43137255f, 0.43137255f, 0.43137255f, 1.0f));
			text->SetStyleAttribute(graph::TextStyleAttribute_Bold, 0, 0, false);
			text->SetStyleAttribute(graph::TextStyleAttribute_EnlargeFontSize, 0, 0, false);
		} else {
			float width = 590.0f;
			float left = mode != 0 ? 298.0f : 261.0f;

			text->SetSize(math::v3(width, 0.0f, 0.0f));
			text->SetPos(math::v3(width * 0.5f - left, -59.0f, 0.0f));
			text->SetLayoutAttribute(graph::TextLayoutAttribute_LinePitch, 19.0f);
			text->SetLayoutAttribute(graph::TextLayoutAttribute_EnlargeLinePitch, true);
			text->SetStyleAttribute(graph::TextStyleAttribute_Bold, 0, 0, true);
			if (mode == 0) {
				text->SetStyleAttribute(graph::TextStyleAttribute_Color, 0, 0, m_color);
				text->SetStyleAttribute(graph::TextStyleAttribute_Shadow, 0, 0, true);
			} else if (m_event.newFlag) {
				text->SetStyleAttribute(graph::TextStyleAttribute_Color, 0, 0,
							math::v4(0.98039216f, 0.98039216f, 0.98039216f, 1.0f));
			} else {
				text->SetStyleAttribute(graph::TextStyleAttribute_Color, 0, 0,
							math::v4(0.98039216f, 0.98039216f, 0.98039216f, 1.0f));
			}
		}
		ret = 0;
	}
	return ret;
}

/* the description under the title, the title placed by its lines and the frame beside it,
   and the time below the icon (0x8117E928) */
int IndicatorEventFriendItem::Layout(ui::Widget *widget, bool animate)
{
	if (widget == NULL || !widget->IsInherit(ui::CompositeDrawer::TypeName()))
		return 1;

	unsigned int lines = 0;
	int mode = GetMode();
	graph::TextObj *sub = INDICATOR_ICON_TEXT(widget, INDICATOR_ICON_OBJ_SUBTEXT);

	if (sub != NULL) {
		if (sub->GetLineCount(lines) < 0)
			lines = 0;

		int align = mode == 2 ? 6 : 4;
		float y = mode == 2 ? -47.0f : (mode == 0 ? -59.0f : -68.0f) + 7.0f;

		math::v4 size(sub->m_size, 0.0f);
		math::v4 offset;

		AlignText(sub, align, size, &offset);

		math::v3 pos = sub->m_pos;

		pos.set_y(y + offset.extract_y());
		sub->SetPos(pos);
		sub->SetVisible(true);
	}

	graph::TextObj *text = INDICATOR_ICON_TEXT(widget, INDICATOR_ICON_OBJ_TITLE);

	if (text != NULL) {
		float y;

		if (mode == 2)
			y = -26.0f;
		else if (lines == 2)
			y = mode == 0 ? -29.0f : -38.0f;
		else
			y = mode == 0 ? -37.0f : -41.0f;

		math::v4 size(text->m_size, 0.0f);
		math::v4 offset;

		AlignText(text, 6, size, &offset);

		math::v3 pos = text->m_pos;

		pos.set_y(y + offset.extract_y());
		text->SetPos(pos);
		text->SetVisible(true);

		graph::PlaneObj *frame = INDICATOR_ICON_PLANE(widget, INDICATOR_ICON_OBJ_FRAME);

		if (frame != NULL) {
			AlignText(text, 9, size, &offset);

			math::v3 framePos = frame->m_pos;

			framePos.set_y(pos.extract_y() - offset.extract_y());
			frame->SetPos(framePos);
			frame->SetVisible(true);
		}
	}

	graph::TextObj *time = INDICATOR_ICON_TEXT(widget, INDICATOR_ICON_OBJ_TIME);

	if (time != NULL) {
		math::v4 size(time->m_size, 0.0f);
		math::v4 offset;

		AlignText(time, 5, size, &offset);

		math::v3 pos = time->m_pos;

		pos.set_y(offset.extract_y() + -80.0f);
		time->SetPos(pos);
		time->SetVisible(true);
	}
	return 1;
}
