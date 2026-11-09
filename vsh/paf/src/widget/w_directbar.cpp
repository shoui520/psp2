/*
 * paf::ui::DirectBar (DEBUGGER 0x810e9668..0x810ee9ec): a bar of entries held from its launch
 * button, its balloon following the touch or the direct keys. No record or string names the
 * file.
 */
#include <stddef.h>
#include <stdint.h>
#include <graphics/drawobj/plane_obj.h>
#include <graphics/drawobj/text_obj.h>
#include <math/ngp/v2.h>
#include <std/string>
#include <std/vector>
#include <timer/timer.h>
#include <versatileparam/resourceparam.h>
#include <widget/core/animlistener.h>
#include <widget/core/environment.h>
#include <widget/core/event.h>
#include <widget/core/focus.h>
#include <widget/factory.h>
#include <widget/w_directbar.h>

namespace paf {
namespace ui {

/* the color as RGBA bytes, each saturated, as DrawObj::SetColor keeps it */
static inline uint32_t ColorRGBA8(const math::v4 &color)
{
	uint16x4_t rgba = vqmovn_u32(
		vcvtq_u32_f32(vmulq_f32(color.getImpl().get128(), vdupq_n_f32(255.0f))));

	return vget_lane_u32(vreinterpret_u32_u8(vqmovn_u16(vcombine_u16(rgba, rgba))), 0);
}

void DirectBar::SendEvent(int32_t type, int32_t v0, int32_t v1, int32_t v2, int32_t v3)
{
	Event e(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0, v0, v1, v2, v3);

	DoEvent(type, &e);
}

void DirectBar::AnimDBarState(int32_t state, float time)
{
	switch (state) {
	case ST_DBAR_OPEN:
	case ST_DBAR_HOLD: {
		float first = time * 0.4f;
		float second = time - time * 0.4f;

		SetHitBox(3, m_size, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		for (int32_t i = 0; i < OBJ_DIRECT_BAR_END; i++) {
			graph::DrawObj *obj = GetDrawObj(i);

			if (obj == NULL)
				continue;

			math::v4 color = obj->GetColor();
			math::v4 size(obj->m_size, 0.0f);
			math::v4 pos(obj->m_pos, 0.0f);
			math::v4 color_to = color;
			math::v4 size_to = size;
			math::v4 pos_to = pos;

			if (i > 2) {
				if (i == OBJ_LAUNCHBTN) {
					size_to = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
					color_to.set_w(0.0f);
					pos_to.set_y(m_launchbtn_y);
				}
			} else if (i >= 0) {
				pos_to.set_x(0.0f);
				if (i <= 0) {
					size_to = m_size;
				} else if (m_type & 1) {
					pos_to.set_x(size.extract_x() * 0.1f);
				} else {
					pos_to.set_x(size.extract_x() * -0.1f);
				}
				color_to.set_w(1.0f);
			}
			if (!(color_to == color))
				SetAnimListener(0x20300 + i,
						new AnimListenerObj(new Timer(first), this, obj, color,
								    color_to,
								    &AnimListenerObj::s_prop_obj_color,
								    NULL, NULL),
						2);
			if (!(size_to == size)) {
				obj->SetSize(size.getImpl().getXYZ());

				math::v4 over(size.extract_x() + 10.0f, size_to.extract_y(), 0.0f, 0.0f);

				SetAnimListener(0x20200 + i,
						new AnimListenerObj(new Timer(first, Timer::FUNC_QUAD_OUT),
								    this, obj, size, over,
								    &AnimListenerObj::s_prop_obj_size,
								    NULL, NULL),
						2);
				SetAnimListener(0x20200 + i,
						new AnimListenerObj(new Timer(second, Timer::FUNC_BACK_OUT),
								    this, obj, over, size_to,
								    &AnimListenerObj::s_prop_obj_size,
								    NULL, NULL),
						0);
			}
			if (!(pos_to == pos)) {
				AnimListenerObj *hold =
					new AnimListenerObj(new Timer(first, Timer::FUNC_QUAD_OUT), this,
							    obj, pos, pos,
							    &AnimListenerObj::s_prop_obj_move, NULL, NULL);

				SetAnimListener(0x20100 + i, hold, 0);

				AnimListenerObj *move =
					new AnimListenerObj(new Timer(second, Timer::FUNC_BACK_OUT), this,
							    obj, pos, pos_to,
							    &AnimListenerObj::s_prop_obj_move, NULL, NULL);

				SetAnimListener(0x20100 + i, move, 0);
				if (i == OBJ_LABEL) {
					hold->m_cb_anim = OnBalloonMove;
					move->m_cb_anim = OnBalloonMove;
				}
			}
		}
		break;
	}
	case ST_DBAR_DRAG_OUT:
		break;
	case ST_DBAR_CLOSED: {
		ReleaseDBar();
		SetHitBox(3, math::v4(m_closed_hit_width, m_closed_hit_height, 0.0f, 0.0f),
			  math::v4(m_closed_hit_x, m_launchbtn_y + m_closed_hit_y, 0.0f, 0.0f));

		float fade = time * 1.2f;

		for (int32_t i = 0; i < OBJ_DIRECT_BAR_END; i++) {
			graph::DrawObj *obj = GetDrawObj(i);

			if (obj == NULL)
				continue;

			math::v4 color = obj->GetColor();
			math::v4 color_to = color;
			math::v4 size(obj->m_size, 0.0f);
			math::v4 size_to = size;
			math::v4 pos(obj->m_pos, 0.0f);
			math::v4 pos_to = pos;

			switch (i) {
			case OBJ_BG:
				size_to = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
			case OBJ_LABEL:
			case OBJ_BLANK_MARK:
				pos_to.set_x((m_type & 1) ? m_size.extract_x() * 0.5f :
							    m_size.extract_x() * -0.5f);
			case OBJ_BALLOON:
			case OBJ_BALLOON_LABEL:
				color_to.set_w(0.0f);
				break;
			case OBJ_LAUNCHBTN:
				pos_to.set_y(m_launchbtn_y);
				size_to = math::v4(64.0f, 64.0f, 0.0f, 0.0f);
				color_to.set_w(1.0f);
				break;
			}
			SetAnimListener(0x20300 + i,
					new AnimListenerObj(new Timer(fade), this, obj, color, color_to,
							    &AnimListenerObj::s_prop_obj_color, NULL,
							    NULL),
					2);
			if (!(size_to == size))
				SetAnimListener(0x20200 + i,
						new AnimListenerObj(new Timer(time, Timer::FUNC_QUAD_OUT),
								    this, obj, size, size_to,
								    &AnimListenerObj::s_prop_obj_size,
								    NULL, NULL),
						2);
			if (!(pos_to == pos))
				SetAnimListener(0x20100 + i,
						new AnimListenerObj(new Timer(time, Timer::FUNC_QUAD_OUT),
								    this, obj, pos, pos_to,
								    &AnimListenerObj::s_prop_obj_move,
								    NULL, NULL),
						2);
		}
		break;
	}
	case ST_DBAR_HIDDEN:
		for (int32_t i = 0; i < OBJ_DIRECT_BAR_END; i++) {
			graph::DrawObj *obj = GetDrawObj(i);

			if (obj == NULL)
				continue;

			math::v4 color = obj->GetColor();
			math::v4 size(obj->m_size, 0.0f);
			math::v4 pos(obj->m_pos, 0.0f);
			math::v4 color_to = color;
			math::v4 size_to = size;
			math::v4 pos_to = pos;

			color_to.set_w(0.0f);
			if (i > 2) {
				if (i == OBJ_LAUNCHBTN) {
					size_to = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
					pos_to.set_y(m_launchbtn_y);
				}
			} else if (i >= 0) {
				if (i <= 0)
					size_to = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
				pos_to.set_x((m_type & 1) ? m_size.extract_x() * 0.5f :
							    m_size.extract_x() * -0.5f);
			}
			if (!(color_to == color))
				SetAnimListener(0x20300 + i,
						new AnimListenerObj(new Timer(time), this, obj, color,
								    color_to,
								    &AnimListenerObj::s_prop_obj_color,
								    NULL, NULL),
						2);
			if (!(size_to == size))
				SetAnimListener(0x20200 + i,
						new AnimListenerObj(new Timer(time, Timer::FUNC_QUAD_OUT),
								    this, obj, size, size_to,
								    &AnimListenerObj::s_prop_obj_size,
								    NULL, NULL),
						2);
			if (!(pos_to == pos))
				SetAnimListener(0x20100 + i,
						new AnimListenerObj(new Timer(time, Timer::FUNC_QUAD_OUT),
								    this, obj, pos, pos_to,
								    &AnimListenerObj::s_prop_obj_move,
								    NULL, NULL),
						2);
		}
		break;
	}
}

void DirectBar::UpdateFocusShape()
{
	float x = (m_type & 1) ? 7.5f : -7.5f;
	FocusShape local;
	FocusShape *shape = m_focus_shape;

	if (shape == NULL)
		shape = &local;
	if (m_state == ST_DBAR_HOLD) {
		if (m_focus_shape != NULL) {
			m_focus_shape->m_offset = math::v3(x, 0.0f, 0.0f);
			m_focus_shape->m_shape = 2;
			m_focus_shape->m_size =
				math::v2(m_size.extract_x() - 15.0f, m_size.extract_y());
		}
	} else if (m_type & 0xf0) {
		shape->m_offset = math::v3(x, 0.0f, 0.0f);
		shape->m_shape = 2;
		shape->m_size = math::v2(m_size.extract_x() - 15.0f, m_size.extract_y());
	} else {
		shape->m_shape = 0;
		shape->m_size = math::v2(64.0f, 64.0f);
		shape->m_offset = math::v3(0.0f, m_launchbtn_y, 0.0f);
	}
	ScePafWidget_13B246E0(this, shape);
}

}
}

using namespace paf;
using namespace paf::ui;

void ScePafWidget_0926399C(DirectBar *bar, uint32_t type)
{
	graph::PlaneObj::AlignMode align = graph::PlaneObj::ALIGN_LEFT;

	for (int32_t i = DirectBar::OBJ_LABEL; i <= DirectBar::OBJ_BLANK_MARK; i++) {
		if (bar->GetDrawObj(i) != NULL &&
		    bar->GetDrawObj(i)->IsInherit(graph::TextObj::DrawObjType())) {
			graph::TextObj *text = (graph::TextObj *)bar->GetDrawObj(i);

			if (text != NULL) {
				text->SetLayoutAttribute(graph::TextLayoutAttribute_AlignX, 1);
				text->SetLayoutAttribute(graph::TextLayoutAttribute_AlignY, 0);
				text->SetLayoutAttribute(graph::TextLayoutAttribute_TextAlign, 1);
			}
		}
	}
	if (bar->GetDrawObj(DirectBar::OBJ_BALLOON) != NULL &&
	    bar->GetDrawObj(DirectBar::OBJ_BALLOON)->IsInherit(graph::PlaneObj::DrawObjType())) {
		graph::PlaneObj *balloon = (graph::PlaneObj *)bar->GetDrawObj(DirectBar::OBJ_BALLOON);

		if (balloon != NULL) {
			math::v4 size = bar->m_size;
			float x;

			if (type & 1) {
				balloon->SetRot(math::v3(0.0f, 3.14159274f, 0.0f));
				x = size.extract_x() * 0.5f + 108.0f + 20.0f;
				if (bar->GetDrawObj(DirectBar::OBJ_BG) != NULL &&
				    bar->GetDrawObj(DirectBar::OBJ_BG)
					    ->IsInherit(graph::PlaneObj::DrawObjType())) {
					graph::DrawObj *bg = bar->GetDrawObj(DirectBar::OBJ_BG);

					if (bg != NULL)
						bg->SetRot(math::v3(0.0f, 3.14159274f, 0.0f));
				}
			} else {
				align = graph::PlaneObj::ALIGN_RIGHT;
				x = -(size.extract_x() * 0.5f + 108.0f + 20.0f);
			}

			math::v2 pos(x, 0.0f);
			math::v4 balloon_size(0.0f, 0.0f, 0.0f, 0.0f);
			graph::PlaneObj::AlignMode align_y = graph::PlaneObj::ALIGN_CENTER;

			if (type & 2) {
				balloon_size = math::v4(108.0f, 108.0f, 0.0f, 0.0f);
				align_y = graph::PlaneObj::ALIGN_TOP;
				pos = math::v2(x, (size.extract_y() - 108.0f) * 0.5f);
			}

			math::v4 color = balloon->GetColor();

			color.set_w(0.0f);
			balloon->SetColor(color);
			balloon->SetAlign(align, align_y);
			balloon->SetPos(math::v3(pos.extract_x(), pos.extract_y(), 0.0f));
			balloon->SetSize(balloon_size.getImpl().getXYZ());
		}
	}
	bar->m_type = type;
	bar->UpdateFocusShape();
}

namespace paf {
namespace ui {

DirectBar::DirectBar(Widget *parent, CreateParam *param)
	: Widget(parent, param), m_state(ST_DBAR_NONE), m_touch_id(-1),
	  m_hit_margin(30.0f), m_btnNum(1), m_select(-1), m_top(-1), m_blank(-1),
	  m_layout_done(false), m_launchbtn_y(0.0f), m_closed_hit_x(0.0f), m_closed_hit_y(0.0f),
	  m_closed_hit_height(64.0f), m_closed_hit_width(64.0f),
	  m_disable_color(1.0f, 1.0f, 1.0f, 0.3f), m_repeat_count(0), m_balloon_clamped(false)
{
	m_hit_check = 1;
	m_hit_detect_b_mode = 0;
	m_hit_detect_mode = 0;
	m_hit_find_mode = 0;
	m_event_enable |= 0xF0D0;
	ReserveDrawObj(OBJ_DIRECT_BAR_END);

	graph::DrawStyle plane_style;

	plane_style.blend = 1;
	plane_style.depth = 1;
	plane_style.lighting = 1;
	plane_style.face_cull = 0;
	plane_style.scissor = 0;
	plane_style.deepen_color = 0;
	plane_style.reduce_transparency = 0;
	plane_style.surface_draw = 0;
	plane_style.reserved1 = 0;

	graph::PlaneObj *plane = new graph::PlaneObj(plane_style);

	if (plane != NULL)
		AddDrawObj(DrawObjEntry(plane, OBJ_BG), false);

	graph::DrawStyle text_style;

	text_style.blend = 1;
	text_style.depth = 1;
	text_style.lighting = 1;
	text_style.face_cull = 0;
	text_style.scissor = 0;
	text_style.deepen_color = 0;
	text_style.reduce_transparency = 0;
	text_style.surface_draw = 0;
	text_style.reserved1 = 0;
	for (int32_t i = OBJ_LABEL; i <= OBJ_BLANK_MARK; i++) {
		graph::TextObj *text = new graph::TextObj(text_style);

		if (text == NULL)
			continue;
		text->SetLayoutAttribute(graph::TextLayoutAttribute_AlignX, 1);
		text->SetLayoutAttribute(graph::TextLayoutAttribute_AlignY, 0);
		text->SetLayoutAttribute(graph::TextLayoutAttribute_TextAlign, 1);

		common::SharedPtr<graph::DrawObj> ptr((graph::DrawObj *)text);
		DrawObjAttr attr(i);

		attr.m_flag1 = false;
		AddDrawObj(DrawObjEntry(ptr, attr), false);
	}

	graph::PlaneObj *balloon = new graph::PlaneObj(plane_style);

	if (balloon != NULL) {
		balloon->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.0f));
		AddDrawObj(DrawObjEntry(balloon, OBJ_BALLOON), false);
	}

	graph::TextObj *balloon_label = new graph::TextObj(text_style);

	if (balloon_label != NULL) {
		balloon_label->SetLayoutAttribute(graph::TextLayoutAttribute_AlignX, 1);
		balloon_label->SetLayoutAttribute(graph::TextLayoutAttribute_AlignY, 1);
		balloon_label->SetLayoutAttribute(graph::TextLayoutAttribute_TextAlign, 1);
		balloon_label->SetLayoutAttribute(graph::TextLayoutAttribute_AdjustX, true);
		balloon_label->SetLayoutAttribute(graph::TextLayoutAttribute_AdjustY, true);
		balloon_label->SetStyleAttribute(graph::TextStyleAttribute_Point, 0, 0, 36.0f);
		balloon_label->SetStyleAttribute(graph::TextStyleAttribute_Color, 0, 0,
						 math::v4(1.0f, 1.0f, 1.0f, 1.0f));
		balloon_label->SetStyleAttribute(graph::TextStyleAttribute_BackColor, 0, 0,
						 math::v4(0.0f, 0.0f, 0.0f, 0.0f));

		common::SharedPtr<graph::DrawObj> ptr((graph::DrawObj *)balloon_label);
		DrawObjAttr attr(OBJ_BALLOON_LABEL);

		attr.m_flag1 = false;
		AddDrawObj(DrawObjEntry(ptr, attr), false);
	}

	graph::PlaneObj *launchbtn = new graph::PlaneObj(plane_style);

	if (launchbtn != NULL)
		AddDrawObj(DrawObjEntry(launchbtn, OBJ_LAUNCHBTN), false);
	ScePafWidget_0926399C(this, 2);
	SetDBarIdleState();
}

/* the strings a directbar's resource gives its draw objects: the label's */
static const struct {
	const char *name;
	int32_t index;
} s_directbar_strings[] = {
	{ "label", DirectBar::OBJ_LABEL },
};

DirectBarParam::DirectBarParam()
	: m_hitmargin(30.0f), m_launchbtn_y(0.0f), m_balloon_type(2), m_bar_type(0)
{
	m_label.clear();
	for (uint32_t i = 0; i < sizeof(s_directbar_strings) / sizeof(s_directbar_strings[0]); i++)
		m_string_list.push_back(
			IndexedRef(s_directbar_strings[i].name, s_directbar_strings[i].index));
}

DirectBarStyleParam::DirectBarStyleParam()
{
	m_bg_obj.m_name = "bg_obj";
	m_bg_obj.m_index = DirectBar::OBJ_BG;
	m_drawobj_param_list.push_back(&m_bg_obj);
	m_label_obj.m_name = "label_obj";
	m_label_obj.m_index = DirectBar::OBJ_LABEL;
	m_drawobj_param_list.push_back(&m_label_obj);
	m_balloon_obj.m_name = "balloon_obj";
	m_balloon_obj.m_index = DirectBar::OBJ_BALLOON;
	m_drawobj_param_list.push_back(&m_balloon_obj);
	m_launchbtn_obj.m_name = "launchbtn_obj";
	m_launchbtn_obj.m_index = DirectBar::OBJ_LAUNCHBTN;
	m_drawobj_param_list.push_back(&m_launchbtn_obj);
}

const char DirectBar::m_widget_type[] = "directbar";

bool DirectBar::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return Widget::IsInherit(type);
}

char *DirectBar::GetTypeName() const
{
	return (char *)m_widget_type;
}

const char *DirectBar::TypeName()
{
	return m_widget_type;
}

int32_t DirectBar::SetObjColor(const math::v4 &color, int32_t obj)
{
	graph::DrawObj *draw = GetDrawObj(obj);

	if (draw != NULL)
		draw->SetColor(color);
	return 0;
}

int32_t DirectBar::GetObjColor(math::v4 &color, int32_t obj)
{
	graph::DrawObj *draw = GetDrawObj(obj);

	if (draw != NULL)
		color = draw->GetColor();
	return 0;
}

int32_t DirectBar::SetString(const wstring &str, int32_t obj)
{
	graph::DrawObj *draw = GetDrawObj(obj);

	if (draw == NULL || !draw->IsInherit(graph::TextObj::DrawObjType()))
		return 0x80AF4101;

	graph::TextObj *text = (graph::TextObj *)draw;

	if (obj != OBJ_LABEL)
		return text->SetString(str);

	wstring marks;

	m_btnNum = 0;
	m_blank = -1;
	m_label.clear();
	m_btn_strings.clear();

	uint32_t pos = 0;

	for (;;) {
		uint32_t found = str.find(L",", pos);

		if (found == (uint32_t)-1)
			break;
		if (found == pos) {
			if (m_blank < 0)
				m_blank = m_btnNum;
			marks.append(L"\xfffd" L"E\n");
		} else {
			wstring btn = str.substr(pos, found - pos);

			m_label.append(btn);
			m_label.append(L"\n");
			m_btn_strings.push_back(btn);
			m_btnNum++;
		}
		pos = found + 1;
	}
	if (pos < str.size()) {
		wstring btn = str.substr(pos, str.size() - pos);

		m_label.append(btn);
		m_btn_strings.push_back(btn);
		m_btnNum++;
	}

	int32_t ret = text->SetString(m_label);

	if (GetDrawObj(OBJ_BLANK_MARK) != NULL &&
	    GetDrawObj(OBJ_BLANK_MARK)->IsInherit(graph::TextObj::DrawObjType())) {
		graph::TextObj *mark = (graph::TextObj *)GetDrawObj(OBJ_BLANK_MARK);

		if (mark != NULL)
			mark->SetString(marks);
	}
	m_layout_done = false;
	UpdateSize();
	return ret;
}

int32_t DirectBar::GetString(wstring &str, int32_t obj)
{
	graph::DrawObj *draw = GetDrawObj(obj);

	if (draw == NULL || !draw->IsInherit(graph::TextObj::DrawObjType()))
		return 0x80AF4101;
	return ((graph::TextObj *)draw)->GetString(str);
}

int32_t DirectBar::SetTexture(const intrusive_ptr<graph::Surface> &surf, int32_t obj,
			      uint32_t index)
{
	if (obj != OBJ_BG && obj != OBJ_BALLOON && obj != OBJ_LAUNCHBTN)
		return 0x80AF4101;

	graph::DrawObj *draw = GetDrawObj(obj);

	if (draw == NULL)
		return 0x80AF4101;
	draw->SetTexture(surf, index);
	return 0;
}

void DirectBar::DestroyWidget()
{
	if (m_touch_id >= 0)
		SendDBarEvent(CB_DIRECT_BAR_RELEASE);
	Widget::DestroyWidget();
}

}
}

int32_t ScePafWidget_8D86C86E(DirectBar *bar)
{
	return bar->m_btnNum;
}

namespace paf {
namespace ui {

uint32_t DirectBar::CheckState()
{
	if (GetDrawObj(OBJ_LABEL) != NULL &&
	    GetDrawObj(OBJ_LABEL)->IsInherit(graph::TextObj::DrawObjType())) {
		graph::TextObj *text = (graph::TextObj *)GetDrawObj(OBJ_LABEL);

		if (text != NULL && !text->m_flags.m_prepare && !m_layout_done) {
			graph::TextLayoutLineInfo info;

			text->GetLineInfo(0, info);

			math::v4 size = m_size - math::v4(52.0f, 52.0f, 52.0f, 52.0f);
			float pitch = size.extract_y() / (float)(uint32_t)m_btnNum;
			float space = pitch - (info.frame.extract_w() - info.frame.extract_y());
			math::v3 pos = text->m_pos;

			text->SetPos(math::v3(pos.extract_x(), -26.0f, pos.extract_z()));
			text->SetLayoutAttribute(graph::TextLayoutAttribute_LineSpace, space);
			if (GetDrawObj(OBJ_BLANK_MARK) != NULL &&
			    GetDrawObj(OBJ_BLANK_MARK)->IsInherit(graph::TextObj::DrawObjType())) {
				graph::TextObj *mark = (graph::TextObj *)GetDrawObj(OBJ_BLANK_MARK);

				if (mark != NULL) {
					mark->SetPos(math::v3(pos.extract_x(),
							      pitch * 0.5f + -26.0f -
								      (float)m_blank * pitch,
							      pos.extract_z()));
					mark->SetLayoutAttribute(graph::TextLayoutAttribute_LineSpace,
								 space);
				}
			}
			m_layout_done = true;
			if (m_top < 0)
				m_top = 0;
		}
	}

	uint32_t state = Widget::m_state;

	if (state < 2 || state == 3) {
		state = 0;
		if (IsPrepareObjs())
			return 3;
	}
	return state;
}

void DirectBar::UpdateSize()
{
	math::v4 size = m_size;
	math::v2 launchbtn(108.0f, 108.0f);

	for (int32_t i = 0; i < OBJ_DIRECT_BAR_END; i++) {
		graph::DrawObj *obj = GetDrawObj(i);

		if (obj == NULL)
			continue;
		switch (i) {
		case OBJ_BG:
			obj->SetSize(size.getImpl().getXYZ());
			break;
		case OBJ_LABEL:
		case OBJ_BLANK_MARK: {
			math::v2 text_size(size.extract_x(), size.extract_y());

			if (obj->IsInherit(graph::TextObj::DrawObjType())) {
				((graph::TextObj *)obj)
					->SetLayoutAttribute(graph::TextLayoutAttribute_Size, text_size);
				((graph::TextObj *)obj)
					->SetLayoutAttribute(graph::TextLayoutAttribute_VisibleSize,
							     text_size);
			}
			break;
		}
		case OBJ_BALLOON:
			if (m_type & 2)
				obj->SetSize(math::v3(108.0f, 108.0f, 0.0f));
			break;
		case OBJ_BALLOON_LABEL:
			if (obj->IsInherit(graph::TextObj::DrawObjType()) && (m_type & 2)) {
				((graph::TextObj *)obj)
					->SetLayoutAttribute(graph::TextLayoutAttribute_Size, launchbtn);
				((graph::TextObj *)obj)
					->SetLayoutAttribute(graph::TextLayoutAttribute_VisibleSize,
							     launchbtn);
			}
			break;
		case OBJ_LAUNCHBTN:
			obj->SetSize(math::v3(64.0f, 64.0f, 0.0f));
			break;
		}
	}
}

void DirectBar::OnTouchCaptureChanged(uint32_t port, uint32_t id, int32_t arg)
{
	if (m_touch_captures.size() != 0)
		return;
	if (m_type & 0xf0) {
		if (m_state != ST_DBAR_OPEN) {
			Event e(0x47, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0);

			e.m_detail = (e.m_detail & 0xff0fffff) | ((port & 0xf) << 20);
			e.m_detail = (e.m_detail & 0xffffff00) | (uint8_t)id;
			HandleTouchEvent(&e);
		}
	} else if (m_state != ST_DBAR_CLOSED) {
		Event e(0x47, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0);

		e.m_detail = (e.m_detail & 0xff0fffff) | ((port & 0xf) << 20);
		e.m_detail = (e.m_detail & 0xffffff00) | (uint8_t)id;
		HandleTouchEvent(&e);
	}
}

int32_t DirectBar::OnDraw(graph::DrawInfo &info)
{
	float alpha = m_color_alpha * m_meta_a_sum * m_show_a_sum;

	for (vector<DrawObjEntry>::iterator it = m_drawobjs.begin(); it != m_drawobjs.end();
	     ++it) {
		uint32_t color = m_color;

		if (it->m_attr.m_id != OBJ_BALLOON)
			color = 0xffffffff;
		it->m_obj->OnDraw(m_world_matrix, color, alpha, info);
	}
	return 0;
}

void DirectBar::Update()
{
	Widget::Update();
}

int32_t DirectBar::GetBtnNum()
{
	return m_btnNum;
}

math::v4 DirectBar::GetPartsSize()
{
	math::v4 size = m_size - math::v4(52.0f, 52.0f, 52.0f, 52.0f);

	size.set_y(size.extract_y() / (float)m_btnNum);
	return size;
}

math::v4 DirectBar::GetBalloonPos(int32_t index)
{
	math::v4 pos(0.0f, 0.0f, 0.0f, 0.0f);
	float height = m_size.extract_y();
	math::v4 parts = GetPartsSize();

	pos.set_y(height * 0.5f - (float)(index - m_top) * parts.extract_y() - 26.0f - 9.0f - 1.0f);
	return pos;
}

}
}

void ScePafWidget_A0CEFD28(DirectBar *bar, int32_t index, bool key, float time)
{
	float width = bar->m_size.extract_x();
	math::v4 balloon_pos = bar->GetBalloonPos(index);

	bar->GetPartsSize();

	math::v4 size = bar->m_size;

	if (index < 0 || index >= bar->m_btnNum)
		return;
	if (bar->m_top < 0)
		bar->m_top = 0;
	if (index < bar->m_top)
		bar->m_top = index;
	if (bar->m_top + bar->m_btnNum <= index)
		bar->m_top = index - bar->m_btnNum + 1;

	float top = (size.extract_y() - 88.0f) * 0.5f;
	float bottom = -top - 10.0f;
	float y;

	if (balloon_pos.extract_y() < bottom) {
		y = bottom;
		bar->m_balloon_clamped = true;
	} else if (top < balloon_pos.extract_y()) {
		y = top;
		bar->m_balloon_clamped = true;
	} else {
		y = balloon_pos.extract_y();
		bar->m_balloon_clamped = false;
	}

	float label_y = y + 4.0f;
	float balloon_x;
	float label_x;

	if (!(bar->m_type & 1)) {
		balloon_x = width + 20.0f;
		label_x = 108.0f * 0.7f + 2.0f + 20.0f;
	} else {
		balloon_x = -width - 20.0f;
		label_x = 108.0f * -0.7f - 2.0f - 20.0f;
	}

	graph::DrawObj *balloon = bar->GetDrawObj(DirectBar::OBJ_BALLOON);
	bool appear = false;

	if (balloon != NULL) {
		bar->KillAnim(0x20303);

		math::v4 color = balloon->GetColor();

		if (color.extract_w() == 0.0f)
			appear = true;
		color.set_w(1.0f);
		bar->SetAnimListener(0x20303,
				     new AnimListenerObj(new Timer(60.0f), bar, balloon,
							 math::v4(0.0f, 0.0f, 0.0f, 0.0f), color,
							 &AnimListenerObj::s_prop_obj_color, NULL, NULL),
				     2);
	}
	if (bar->m_type & 2) {
		if (bar->GetDrawObj(DirectBar::OBJ_LABEL) != NULL &&
		    bar->GetDrawObj(DirectBar::OBJ_LABEL)->IsInherit(graph::TextObj::DrawObjType())) {
			graph::TextObj *text = (graph::TextObj *)bar->GetDrawObj(DirectBar::OBJ_LABEL);

			if (text != NULL) {
				text->SetSelectionColor(math::v4(0.5f, 0.8f, 1.0f, 0.8f));

				uint32_t pos = 0;

				for (int32_t i = 0; i < index; i++)
					pos = bar->m_label.find(L"\n", pos) + 1;
				text->SetSelectionRange(pos, bar->m_btn_strings.at(index).size());
			}
		}
	} else if (balloon != NULL) {
		bar->KillAnim(0x20103);
		if (appear) {
			balloon->SetSize(math::v3(0.0f, 0.0f, 0.0f));
			bar->SetAnimListener(0x20203,
					     new AnimListenerObj(new Timer(240.0f, Timer::FUNC_BACK_OUT),
								 bar, balloon,
								 math::v4(0.0f, 0.0f, 0.0f, 0.0f),
								 math::v4(108.0f, 108.0f, 0.0f, 0.0f),
								 &AnimListenerObj::s_prop_obj_size, NULL,
								 NULL),
					     2);
			balloon->SetPos(math::v3(0.0f, y, 0.0f));

			AnimListenerObj *move = new AnimListenerObj(
				new Timer(120.0f, Timer::FUNC_QUART_OUT), bar, balloon,
				math::v4(balloon->m_pos, 0.0f), math::v4(balloon_x, y, 0.0f, 0.0f),
				&AnimListenerObj::s_prop_obj_move, NULL, &bar->m_balloon_clamped);

			bar->SetAnimListener(0x20103, move, 2);
			move->m_cb_anim = DirectBar::OnBalloonMove;
			if (bar->m_focus_shape != NULL) {
				bar->m_focus_shape->m_shape = 3;
				ScePafWidget_13B246E0(bar, bar->m_focus_shape);
			}
		} else {
			AnimListenerObj *move = new AnimListenerObj(
				key ? (time == 240.0f ? new Timer(bar->m_repeat_count < 5 ? 140.0f :
											      160.0f,
								 Timer::FUNC_EXPO_OUT_30) :
							new Timer(100.0f)) :
				      new Timer(100.0f, Timer::FUNC_EXPO_OUT_29),
				bar, balloon, math::v4(balloon->m_pos, 0.0f),
				math::v4(balloon_x, y, 0.0f, 0.0f), &AnimListenerObj::s_prop_obj_move,
				NULL, &bar->m_balloon_clamped);

			bar->SetAnimListener(0x20103, move, 2);
			move->m_cb_anim = DirectBar::OnBalloonMove;
		}
	}

	graph::DrawObj *label = bar->GetDrawObj(DirectBar::OBJ_BALLOON_LABEL);

	if (label != NULL) {
		bar->KillAnim(0x20104);
		bar->KillAnim(0x20304);
		bar->SetString(bar->m_btn_strings.at(index), DirectBar::OBJ_BALLOON_LABEL);

		AnimListenerObj *move;

		if (appear) {
			label->SetPos(math::v3(0.0f, label_y, 0.0f));
			move = new AnimListenerObj(new Timer(120.0f, Timer::FUNC_QUART_OUT), bar, label,
						   math::v4(label->m_pos, 0.0f),
						   math::v4(label_x, label_y, 0.0f, 0.0f),
						   &AnimListenerObj::s_prop_obj_move, NULL, NULL);
		} else {
			move = new AnimListenerObj(
				key ? (time == 240.0f ? new Timer(bar->m_repeat_count < 5 ? 140.0f :
											      160.0f,
								 Timer::FUNC_EXPO_OUT_30) :
							new Timer(100.0f)) :
				      new Timer(100.0f, Timer::FUNC_EXPO_OUT_29),
				bar, label, math::v4(label->m_pos, 0.0f),
				math::v4(label_x, label_y, 0.0f, 0.0f), &AnimListenerObj::s_prop_obj_move,
				NULL, NULL);
		}
		bar->SetAnimListener(0x20104, move, 2);

		math::v4 color = label->GetColor();
		float alpha = 1.0f;

		if (bar->GetDrawObj(DirectBar::OBJ_LABEL) != NULL &&
		    bar->GetDrawObj(DirectBar::OBJ_LABEL)->IsInherit(graph::TextObj::DrawObjType())) {
			graph::TextObj *text = (graph::TextObj *)bar->GetDrawObj(DirectBar::OBJ_LABEL);

			if (text != NULL) {
				text->GetStyleAttribute(graph::TextStyleAttribute_Color, index * 2, color);
				alpha = color.extract_w();
			}
		}
		if (appear || bar->m_select != index) {
			math::v4 hidden = color;

			hidden.set_w(0.0f);
			label->SetColor(hidden);
		}

		math::v4 from = color;

		from.set_w(0.0f);
		color.set_w(alpha);
		bar->SetAnimListener(0x20304,
				     new AnimListenerObj(new Timer(appear ? 120.0f : 40.0f), bar, label,
							 from, color, &AnimListenerObj::s_prop_obj_color,
							 NULL, NULL),
				     2);
	}
	bar->m_select = index;
}

namespace paf {
namespace ui {

bool DirectBar::IsBtnEnable(int32_t index)
{
	if (GetDrawObj(OBJ_LABEL) == NULL ||
	    !GetDrawObj(OBJ_LABEL)->IsInherit(graph::TextObj::DrawObjType()))
		return true;

	graph::TextObj *text = (graph::TextObj *)GetDrawObj(OBJ_LABEL);

	if (text == NULL)
		return true;
	if (index < 0 || index > m_btnNum - 1)
		return false;

	math::v4 color(1.0f, 1.0f, 1.0f, 1.0f);

	text->GetStyleAttribute(graph::TextStyleAttribute_Color, index * 2, color);
	return ColorRGBA8(m_disable_color) != ColorRGBA8(color);
}

__attribute__((noinline)) int32_t DirectBar::GetHitNumber(HitInfo *info)
{
	float height = m_size.extract_y() - 52.0f;
	int32_t num = m_btnNum;
	float hit = (float)m_top + ((height * 0.5f - info->m_touch_pos.extract_y()) * (float)num) /
					  height;

	if (hit < 0.0f)
		return (int32_t)0.0f;
	if (hit > (float)(num - 1))
		return (int32_t)(float)(num - 1);
	return (int32_t)hit;
}

void DirectBar::OrientClosedState(Event *e)
{
	uint32_t id = (uint8_t)e->m_detail;

	if (m_touch_id >= 0 || (e->m_type & 0xffff) != 0x41)
		return;

	HitInfo *hit = NULL;

	if (id < 8)
		hit = &m_env->m_touch_hit[0][id];
	m_touch_id = id;
	SendDBarEvent(CB_DIRECT_BAR_DECIDE);
	SetDBarState(ST_DBAR_HOLD);
	m_touch_pos = hit->m_touch_pos;
	m_hit_pos = hit->m_hit_pos;
	m_select = GetHitNumber(hit);
	if (IsBtnEnable(m_select))
		ScePafWidget_A0CEFD28(this, m_select, false, 60.0f);
	e->Consume();
}

}
}

void ScePafWidget_0E46CEE0(DirectBar *bar)
{
	switch (bar->m_type & 0xf) {
	case 0:
	case 1: {
		bar->KillAnim(0x20303);
		bar->KillAnim(0x20103);

		graph::DrawObj *balloon = bar->GetDrawObj(DirectBar::OBJ_BALLOON);

		if (balloon != NULL) {
			math::v4 color = balloon->GetColor();

			color.set_w(0.0f);
			balloon->SetColor(color);
		}
		bar->KillAnim(0x20304);

		graph::DrawObj *label = bar->GetDrawObj(DirectBar::OBJ_BALLOON_LABEL);

		if (label != NULL) {
			math::v4 color = label->GetColor();

			color.set_w(0.0f);
			label->SetColor(color);
		}
		break;
	}
	case 2:
	case 3: {
		if (bar->GetDrawObj(DirectBar::OBJ_LABEL) != NULL &&
		    bar->GetDrawObj(DirectBar::OBJ_LABEL)->IsInherit(graph::TextObj::DrawObjType())) {
			graph::TextObj *text = (graph::TextObj *)bar->GetDrawObj(DirectBar::OBJ_LABEL);

			if (text != NULL)
				text->SetSelectionColor(math::v4(1.0f, 1.0f, 1.0f, 0.0f));
		}
		bar->KillAnim(0x20303);

		graph::DrawObj *balloon = bar->GetDrawObj(DirectBar::OBJ_BALLOON);

		if (balloon != NULL) {
			math::v4 color = balloon->GetColor();
			math::v4 to = color;

			to.set_w(0.0f);
			bar->SetAnimListener(0x20303,
					     new AnimListenerObj(new Timer(200.0f), bar, balloon,
								 math::v4(0.0f, 0.0f, 0.0f, 0.0f), to,
								 &AnimListenerObj::s_prop_obj_color,
								 NULL, NULL),
					     1);
		}
		bar->KillAnim(0x20304);
		if (bar->GetDrawObj(DirectBar::OBJ_BALLOON_LABEL) != NULL &&
		    bar->GetDrawObj(DirectBar::OBJ_BALLOON_LABEL)
			    ->IsInherit(graph::TextObj::DrawObjType())) {
			graph::DrawObj *label = bar->GetDrawObj(DirectBar::OBJ_BALLOON_LABEL);

			if (label != NULL) {
				math::v4 color = label->GetColor();
				math::v4 to = color;

				to.set_w(0.0f);
				bar->SetAnimListener(0x20304,
						     new AnimListenerObj(
							     new Timer(200.0f), bar, label,
							     math::v4(0.0f, 0.0f, 0.0f, 0.0f), to,
							     &AnimListenerObj::s_prop_obj_color, NULL,
							     NULL),
						     1);
			}
		}
		break;
	}
	}
}

namespace paf {
namespace ui {

void DirectBar::OrientDragOutState(Event *e)
{
	uint32_t id = (uint8_t)e->m_detail;

	if (m_touch_id != id)
		return;

	TouchInfo *touch = NULL;

	if (id < 8) {
		m_env->m_touch_state[0][id].m_port = 0;
		m_env->m_touch_state[0][id].m_id = id;
		touch = &m_env->m_touch_state[0][id];
	}

	float height = m_size.extract_y();
	float dx = touch->m_pos.extract_x() - m_pos.extract_z();
	float y = height * 0.5f - touch->m_pos.extract_y();
	int32_t index = -1;
	bool in;

	if (dx < 0.0f)
		dx = -dx;
	if (dx >= m_hit_margin || y <= 0.0f || y >= height) {
		ScePafWidget_0E46CEE0(this);
		m_select = -1;
		in = false;
	} else {
		in = true;
		index = (int32_t)(y * (float)m_btnNum / height);
	}
	switch (e->m_type & 0xffff) {
	case 0x42:
	case 0x47:
		SetDBarIdleState();
		ScePafWidget_0E46CEE0(this);
		if (in && (e->m_type & 0xffff) == 0x42)
			SendDBarEvent(CB_DIRECT_BAR_PRESS, index);
		else
			SendDBarEvent(CB_DIRECT_BAR_CANCEL);
		ReleaseDBar();
		m_select = -1;
		UpdateFocusShape();
		e->Consume();
		break;
	case 0x43:
		if (in && index != m_select) {
			if (IsBtnEnable(index)) {
				ScePafWidget_A0CEFD28(this, index, false, 60.0f);
				SendDBarEvent(CB_DIRECT_BAR_DRAG_OUT, index);
			} else if (IsBtnEnable(m_select)) {
				graph::DrawObj *balloon = GetDrawObj(OBJ_BALLOON);

				if (balloon != NULL && balloon->GetColor().extract_w() == 0.0f) {
					ScePafWidget_A0CEFD28(this, m_select, false, 60.0f);
					SendDBarEvent(CB_DIRECT_BAR_DRAG_OUT, m_select);
				}
			}
		}
		e->Consume();
		break;
	case 0x44:
		SetDBarState(ST_DBAR_HOLD);
		e->Consume();
		break;
	}
}

void DirectBar::OrientDragState(Event *e)
{
	uint32_t id = (uint8_t)e->m_detail;

	if (m_touch_id != id)
		return;

	HitInfo *hit = NULL;

	if (id < 8)
		hit = &m_env->m_touch_hit[0][id];
	switch (e->m_type & 0xffff) {
	case 0x42:
	case 0x47: {
		SetDBarIdleState();

		int32_t index = GetHitNumber(hit);

		ScePafWidget_0E46CEE0(this);
		if ((e->m_type & 0xffff) == 0x42)
			SendDBarEvent(CB_DIRECT_BAR_PRESS, index);
		else
			SendDBarEvent(CB_DIRECT_BAR_CANCEL);
		ReleaseDBar();
		UpdateFocusShape();
		e->Consume();
		break;
	}
	case 0x43: {
		int32_t index = GetHitNumber(hit);

		if (index == m_top && index > 0)
			index--;
		else if (index == m_btnNum + m_top && index < m_btnNum - 1)
			index++;
		if (index != m_select) {
			if (IsBtnEnable(index)) {
				ScePafWidget_A0CEFD28(this, index, false, 60.0f);
				SendDBarEvent(CB_DIRECT_BAR_DRAG_OUT, index);
			} else if (IsBtnEnable(m_select)) {
				graph::DrawObj *balloon = GetDrawObj(OBJ_BALLOON);

				if (balloon != NULL && balloon->GetColor().extract_w() == 0.0f) {
					ScePafWidget_A0CEFD28(this, m_select, false, 60.0f);
					SendDBarEvent(CB_DIRECT_BAR_DRAG_OUT, m_select);
				}
			}
		}
		e->Consume();
		break;
	}
	case 0x44:
	case 0x46:
		break;
	case 0x45:
		SetDBarState(ST_DBAR_DRAG_OUT);
		SendDBarEvent(CB_DIRECT_BAR_END);
		e->Consume();
		break;
	}
}

void DirectBar::OrientNormalState(Event *e)
{
	uint32_t id = (uint8_t)e->m_detail;

	if (m_touch_id >= 0 || (e->m_type & 0xffff) != 0x41)
		return;

	HitInfo *hit = NULL;

	if (id < 8)
		hit = &m_env->m_touch_hit[0][id];
	m_touch_id = id;
	SetDBarState(ST_DBAR_HOLD);
	m_touch_pos = hit->m_touch_pos;
	m_hit_pos = hit->m_hit_pos;
	ScePafWidget_A0CEFD28(this, GetHitNumber(hit), false, 0.0f);
	e->Consume();
}

int32_t DirectBar::HandleTouchEvent(Event *e)
{
	if (ScePafWidget_529FB877(m_env, -1)) {
		Widget *focus = m_env->m_focus_cursor.m_cursor;

		focus->m_transition_complete = false;
		focus->m_transition = 0xff;
		focus->DoTransitionReverse_core(0.0f, true);
		ScePafWidget_4588699E(focus, NULL, 2, 0.0f, NULL);
	}
	switch (m_state) {
	case ST_DBAR_OPEN:
		OrientNormalState(e);
		break;
	case ST_DBAR_HOLD:
		OrientDragState(e);
		break;
	case ST_DBAR_DRAG_OUT:
		OrientDragOutState(e);
		break;
	case ST_DBAR_CLOSED:
		OrientClosedState(e);
		break;
	}
	return Widget::HandleTouchEvent(e);
}

bool DirectBar::FindEnableBtn(int32_t *index, uint32_t key)
{
	bool found = true;

	if (GetDrawObj(OBJ_LABEL) != NULL &&
	    GetDrawObj(OBJ_LABEL)->IsInherit(graph::TextObj::DrawObjType()) &&
	    GetDrawObj(OBJ_LABEL) != NULL) {
		int32_t step = key == 2 ? 1 : -1;
		int32_t i = *index;

		for (;;) {
			found = IsBtnEnable(i);
			if (*index == 0 || *index == m_btnNum - 1 || found)
				break;
			i = *index + step;
			*index = i;
		}
	}
	return found;
}

int32_t DirectBar::HandleKeycodeEvent(Event *e)
{
	uint32_t key = (uint8_t)e->m_detail;
	Event unused(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0);
	uint32_t type = e->m_type & 0xffff;

	switch (type) {
	case 0x11:
	case 0x13:
		if (m_state == ST_DBAR_HOLD) {
			int32_t index = m_select;

			if (key == 1 || key == 2) {
				int32_t next = index;

				if (type == 0x13)
					m_repeat_count++;
				if (key == 1 && index > 0)
					next = index - 1;
				else if (key == 2 && index < m_btnNum - 1)
					next = index + 1;
				index = next;
				if (next != m_select && FindEnableBtn(&index, key)) {
					int32_t after = key == 1 ? index - 1 : index + 1;
					float time;

					if (index != 0 && index != m_btnNum - 1 &&
					    FindEnableBtn(&after, key))
						time = 60.0f;
					else
						time = 240.0f;
					ScePafWidget_A0CEFD28(this, index, (e->m_type & 0xffff) == 0x13,
							      time);
					if (time == 240.0f)
						SendDBarEvent(CB_DIRECT_BAR_DRAG_OUT, m_select);
				}
				e->Consume();
			} else if (key == 3 || key == 4 || key == 27 || key == 28) {
				if (key != 27 || type != 0x13) {
					SetDBarIdleState();
					ScePafWidget_0E46CEE0(this);
					if (key == 27)
						SendDBarEvent(CB_DIRECT_BAR_PRESS, index);
					else
						SendDBarEvent(CB_DIRECT_BAR_CANCEL);
					ReleaseDBar();
					UpdateFocusShape();
				}
				e->Consume();
			}
			break;
		}
		if (m_state != ST_DBAR_OPEN && m_state != ST_DBAR_CLOSED)
			break;
		if (type == 0x11 && key == 27) {
			if (m_touch_id < 0)
				SendDBarEvent(CB_DIRECT_BAR_KEY_HOLD);
			m_touch_id = 0;
			if (m_select < 0)
				m_select = 0;
			if (m_state == ST_DBAR_CLOSED)
				SendDBarEvent(CB_DIRECT_BAR_DECIDE);
			SetDBarState(ST_DBAR_HOLD);
			ScePafWidget_A0CEFD28(this, m_select, false, 0.0f);
			e->Consume();
		}
		break;
	case 0x12:
	case 0x14:
		if (m_state == ST_DBAR_HOLD && (key == 1 || key == 2)) {
			int32_t index = m_select;
			int32_t next = key == 1 ? index - 1 : index + 1;

			if (index != 0 && index != m_btnNum - 1 && FindEnableBtn(&next, key)) {
				SendDBarEvent(CB_DIRECT_BAR_DRAG_OUT, m_select);
				if (m_repeat_count > 0)
					ScePafWidget_A0CEFD28(this, m_select, true, 240.0f);
			}
		}
		m_repeat_count = 0;
		break;
	}
	return Widget::HandleKeycodeEvent(e);
}

int32_t DirectBar::HandleFocusEvent(Event *e)
{
	if ((e->m_type & 0xffff) == 0x402 && m_state == ST_DBAR_HOLD) {
		SetDBarIdleState();
		ScePafWidget_0E46CEE0(this);
		SendDBarEvent(CB_DIRECT_BAR_CANCEL);
		ReleaseDBar();
		UpdateFocusShape();
	}
	return Handler::HandleFocusEvent(e);
}

void DirectBar::UpdateDBarLayout()
{
}

int32_t DirectBar::GetBtnString(wstring &str, int32_t index)
{
	str = m_btn_strings.at(index);
	return 0;
}

int32_t DirectBar::Show(float time)
{
	SetDBarIdleState(time);
	if (time > 0.0f) {
		Timer *t = new Timer(time);

		if (!m_transition_complete) {
			m_transition = 0xff;
			m_transition_complete = true;
			DoTransition_core(0.0f, true);
		}
		return ScePafWidget_FF5BC957(this, t, 2, 0.0f, NULL);
	}
	if (!m_transition_complete) {
		m_transition = 0xff;
		m_transition_complete = true;
		DoTransition_core(0.0f, true);
	}
	return ScePafWidget_FF5BC957(this, NULL, 2, 0.0f, NULL);
}

int32_t DirectBar::Hide(float time)
{
	SetDBarState(ST_DBAR_HIDDEN, time);
	if (time > 0.0f) {
		Timer *t = new Timer(time);

		m_transition_complete = false;
		m_transition = 0xff;
		DoTransitionReverse_core(t != NULL ? t->GetTotalTime() : 0.0f, true);
		return ScePafWidget_4588699E(this, t, 2, 0.0f, NULL);
	}
	m_transition_complete = false;
	m_transition = 0xff;
	DoTransitionReverse_core(0.0f, true);
	return ScePafWidget_4588699E(this, NULL, 2, 0.0f, NULL);
}

}
}

void ScePafWidget_7C2ABBCB(DirectBar *bar)
{
	bar->SetDBarIdleState();
	if (bar->GetDrawObj(DirectBar::OBJ_BALLOON_LABEL) != NULL &&
	    bar->GetDrawObj(DirectBar::OBJ_BALLOON_LABEL)->IsInherit(graph::TextObj::DrawObjType())) {
		graph::TextObj *label = (graph::TextObj *)bar->GetDrawObj(DirectBar::OBJ_BALLOON_LABEL);

		if (label != NULL)
			label->SetStyleAttribute(graph::TextStyleAttribute_Bold, 0, 0, true);
	}
}

namespace paf {
namespace ui {

DirectBar::~DirectBar()
{
}

void DirectBar::SetLaunchBtnY(float y)
{
	m_launchbtn_y = y;
	AnimDBarState(m_state, 10.0f);
	UpdateFocusShape();
}

void DirectBar::OnBalloonMove(Timer *t, AnimListener *listener, void *data1, void *data2,
			      void *data3)
{
	Widget *widget = listener->m_widget;

	if (widget == NULL || !widget->IsInherit(DirectBar::m_widget_type))
		return;

	DirectBar *bar = (DirectBar *)widget;

	if (bar == NULL || bar->m_state == ST_DBAR_CLOSED)
		return;

	FocusShape *shape = bar->m_focus_shape;

	if (shape == NULL)
		return;
	shape->m_offset.set_x(bar->GetDrawObj(OBJ_LABEL)->m_pos.extract_x());

	bool *clamped = (bool *)data3;

	if (clamped != NULL && *clamped) {
		TimerListener *l = (TimerListener *)listener;
		math::v4 *from = ((AnimListener *)l)->GetStartV4();
		math::v4 value = t->GetValueV4();
		math::v4 pos = bar->GetBalloonPos(bar->m_select);
		float y = from->extract_y() + (pos.extract_y() - from->extract_y()) *
						      value.extract_y();

		if (t->m_state == 2)
			y = pos.extract_y();
		shape->m_offset.set_y(y);
	} else if (!bar->m_balloon_clamped) {
		shape->m_offset.set_y(bar->GetDrawObj(OBJ_BALLOON)->m_pos.extract_y());
	}
	shape->m_size = math::v2(56.0f, 18.0f);
	ScePafWidget_13B246E0(bar, shape);
}

const char DirectBarParam::m_param_name[] = "directbar_base";

const char *DirectBarParam::GetParamName() const
{
	return m_param_name;
}

const char *DirectBarParam::ParamName()
{
	return m_param_name;
}

int32_t DirectBarParam::Create(int32_t mode)
{
	int32_t ret = WidgetParam::Create(mode);

	if (ret < 0)
		return ret;
	if (m_serial != NULL)
		m_serial->AddParam("hitmargin", &m_hitmargin, ResourceParam::SEMANTIC_FLOAT);
	if (m_serial != NULL)
		m_serial->AddParam("launchbtn_y", &m_launchbtn_y, ResourceParam::SEMANTIC_FLOAT);
	if (m_serial != NULL)
		m_serial->AddParam("balloon_type", &m_balloon_type, ResourceParam::SEMANTIC_INT);
	if (m_serial != NULL)
		m_serial->AddParam("bar_type", &m_bar_type, ResourceParam::SEMANTIC_INT);
	return 0;
}

int32_t DirectBarParam::Set()
{
	int32_t ret = WidgetParam::Set();

	if (ret < 0)
		return ret;

	DirectBar *bar = (DirectBar *)m_widget;

	if (bar != NULL) {
		ScePafWidget_0926399C(bar, (m_balloon_type & 0xf) | (bar->m_type & 0xf0));
		ScePafWidget_0926399C(bar, (bar->m_type & 0xf) | ((m_bar_type & 0xf) << 4));
		bar->m_hit_margin = m_hitmargin;
		if (m_launchbtn_y > 0.0f)
			bar->SetLaunchBtnY(m_launchbtn_y);
	}
	return 0;
}

int32_t DirectBarParam::Get()
{
	int32_t ret = WidgetParam::Get();

	if (ret < 0)
		return ret;

	DirectBar *bar = (DirectBar *)m_widget;

	if (bar != NULL) {
		bar->GetString(m_label, DirectBar::OBJ_LABEL);
		m_launchbtn_y = bar->m_launchbtn_y;
		m_hitmargin = bar->m_hit_margin;
		m_bar_type = (uint8_t)bar->m_type >> 4;
		m_balloon_type = bar->m_type & 0xf;
	}
	return 0;
}

const char DirectBarStyleParam::m_param_name[] = "style_directbar";

const char *DirectBarStyleParam::GetParamName() const
{
	return m_param_name;
}

const char *DirectBarStyleParam::ParamName()
{
	return m_param_name;
}

int32_t DirectBarStyleParam::Create(int32_t mode)
{
	return WidgetStyleParam::Create(mode);
}

int32_t DirectBarStyleParam::SetEach(Widget *widget)
{
	int32_t ret = WidgetStyleParam::SetEach(widget);

	if (ret < 0)
		return ret;
	if (widget != NULL) {
		m_label_obj.SetEach(widget->GetDrawObj(DirectBar::OBJ_BLANK_MARK));
		ScePafWidget_7C2ABBCB((DirectBar *)widget);
	}
	return 0;
}

int32_t DirectBarStyleParam::GetEach(Widget *widget)
{
	int32_t ret = WidgetStyleParam::GetEach(widget);

	if (ret < 0)
		return ret;
	return 0;
}

}
}

static Widget *NewDirectBar(Widget *parent)
{
	return new DirectBar(parent, NULL);
}

static WidgetParam *NewDirectBarParam()
{
	return new DirectBarParam;
}

static VersatileParam *NewDirectBarLayoutParam()
{
	return new WidgetLayoutParam;
}

static VersatileParam *NewDirectBarFocusParam()
{
	return new WidgetFocusParam;
}

static VersatileParam *NewDirectBarStyleParam()
{
	return new DirectBarStyleParam;
}

static VersatileParam *NewDirectBarHandlerParam()
{
	return new WidgetHandlerParam;
}

static VersatileParam *NewDirectBarSoundParam()
{
	return new WidgetSoundParam;
}

FactoryFuncTable g_factoryDirectBar = {
	DirectBar::TypeName(),
	NewDirectBar,
	NewDirectBarParam,
	NewDirectBarLayoutParam,
	NewDirectBarFocusParam,
	NewDirectBarStyleParam,
	NewDirectBarHandlerParam,
	NewDirectBarSoundParam,
};
