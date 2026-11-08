/*
 * paf::ui::TextBox (TOOL 0x81133f30..0x81137004): a text edited through an edit listener (the
 * IME's, by default), with its caret blinking and scrolling, its scroll bars, its glow, its
 * maximum length and text filter; its resource, style and sound parameters and its factories. No
 * record or string names the file.
 */
#include <stddef.h>
#include <stdint.h>
#include <common/shared_ptr.h>
#include <graphics/drawobj/plane_obj.h>
#include <graphics/drawobj/text_obj.h>
#include <math/ngp/v2.h>
#include <math/ngp/v3.h>
#include <math/ngp/v4.h>
#include <std/string.h>
#include <timer/timer.h>
#include <versatileparam/resourceparam.h>
#include <widget/core/animlistener.h>
#include <widget/core/environment.h>
#include <widget/core/event.h>
#include <widget/core/handler.h>
#include <widget/core/widget.h>
#include <widget/factory.h>
#include <widget/w_scroll_bar.h>
#include <widget/w_scroll_view_base.h>
#include <widget/w_text.h>
#include <widget/w_text_box.h>
#include "../paf_types.h"

extern paf::ui::AdjustParam ScePafWidget_BE1CEA2E;

namespace paf {
namespace ui {

const char TextBox::m_widget_type[] = "text_box";

const float TextBox::c_caretWidth = 2.0f;

/* the scroll bar shown (a transition in) or hidden (a transition out) at once; inlined in Sony's
   code (names ours) */
static inline __attribute__((always_inline)) void ShowBar(ScrollBar *bar)
{
	if (!bar->m_transition_complete) {
		bar->m_transition = 0xff;
		bar->m_transition_complete = true;
		bar->DoTransition_core(0.0f, true);
	}
	ScePafWidget_FF5BC957(bar, NULL, 2, 0.0f, NULL);
}

static inline __attribute__((always_inline)) void HideBar(ScrollBar *bar)
{
	bar->m_transition_complete = false;
	bar->m_transition = 0xff;
	bar->DoTransitionReverse_core(0.0f, true);
	ScePafWidget_4588699E(bar, NULL, 2, 0.0f, NULL);
}

TextBox::TextBox(Widget *parent, CreateParam *param)
	: Text(parent, param), m_glow(NULL), m_hscroll_bar(NULL), m_vscroll_bar(NULL),
	  m_max_length(1024), m_edit_listener(wutil::GetEditListener()), m_edit_auto_transition(false),
	  m_ellipsis_saved(false), m_text_filter(NULL), m_scroll_max(0.0f, 0.0f),
	  m_caret_scroll_to(0.0f, 0.0f), m_caret_scroll_from(0.0f, 0.0f)
{
	sce_paf_memset(&m_ime_param, 0, sizeof(m_ime_param));
	m_touch_state1 = false;
	if (!(m_event_enable & 0x400))
		static_cast<Widget *>(this)->EnableFocusEvent(true);
	m_event_enable |= 0xFFF0;
	m_hit_detect_mode = 0;
	m_hit_detect_b_mode = 0;
	m_hit_check = 1;
	m_sound_mask |= 2;
	TextBox::UpdateTouchMode();
}

TextBox::~TextBox()
{
	m_glow = (graph::PlaneObj *)-1;
	m_hscroll_bar = (ScrollBar *)-1;
	m_vscroll_bar = (ScrollBar *)-1;
}

bool TextBox::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return Text::IsInherit(type);
}

char *TextBox::GetTypeName() const
{
	return (char *)m_widget_type;
}

const char *TextBox::TypeName()
{
	return m_widget_type;
}

TextBoxEditListener::TextBoxEditListener()
{
}

TextBoxEditListener::~TextBoxEditListener()
{
}

int32_t TextBox::EndEdit()
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL) {
		if (m_edit_mode && m_touch_state2 && m_edit_listener != NULL) {
			if (m_edit_menu_listener != NULL)
				m_edit_menu_listener->EndEdit(this);
			ret = m_edit_listener->EndEdit(this);
			if (ret == 0) {
				m_touch_state2 = false;
				if (m_ellipsis_saved) {
					m_ellipsis_saved = false;
					SetLayoutAttribute(graph::TextLayoutAttribute_Ellipsis, true);
					SetVisiblePos(math::v2(0.0f, 0.0f));
				}
				SetCaretVisible(false);
				SetVisiblePos(math::v2(0.0f, 0.0f));

				Event ev(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0, 0, 0, 0, 0);

				DoEvent(CB_TEXT_BOX_EDIT_END, &ev);
			}
		} else {
			ret = SCE_PAF_ERROR_NO_ENTRY;
		}
	}
	return ret;
}

void TextBox::DestroyWidget()
{
	EndEdit();
	Text::DestroyWidget();
}

int32_t TextBox::SetCaretAlpha(float alpha)
{
	math::v4 color;

	m_text->GetCaretColor(color);
	color.set_w(alpha);
	m_text->SetCaretColor(color);
	return 0;
}

int32_t TextBox::SetCaretBlink()
{
	bool visible;

	m_text->GetCaretVisible(visible);
	if (visible && ScePafWidget_56384DA3(this, ANIM_CARET_BLINK) == 0) {
		math::v4 color;

		m_text->GetCaretColor(color);
		color.set_w(1.0f);
		m_text->SetCaretColor(color);
		SetAnimListener(ANIM_CARET_BLINK | ANIM_OPTION_LOOP,
				new AnimListener(new Timer(Text::c_caretBlinkIdleMsec), this, NULL, NULL, NULL), 0);
		SetAnimListener(ANIM_CARET_BLINK | ANIM_OPTION_LOOP,
				new AnimListener(new Timer(Text::c_caretBlinkFadeMsec), this, OnCaretBlinkFadeOut, NULL, NULL),
				0);
		SetAnimListener(ANIM_CARET_BLINK | ANIM_OPTION_LOOP,
				new AnimListener(new Timer(Text::c_caretBlinkIdleMsec), this, NULL, NULL, NULL), 0);
		SetAnimListener(ANIM_CARET_BLINK | ANIM_OPTION_LOOP,
				new AnimListener(new Timer(Text::c_caretBlinkFadeMsec), this, OnCaretBlinkFadeIn, NULL, NULL),
				0);
	}
	return 0;
}

int32_t TextBox::SetCaretScroll()
{
	if (ScePafWidget_56384DA3(this, ANIM_CARET_SCROLL) == 0) {
		uint32_t caret;
		graph::TextLayoutCharInfo ci;
		graph::TextLayoutLineInfo line;
		math::v2 size;
		float top = 0.0f, bottom = 0.0f, left = 0.0f, right = 0.0f;

		m_text->GetCaretIndex(caret);
		m_text->GetCharInfo(caret, ci);
		m_text->GetLineInfo(ci.line, line);
		m_text->GetLayoutAttribute(graph::TextLayoutAttribute_VisiblePos, m_caret_scroll_from);
		m_caret_scroll_to = m_caret_scroll_from;
		if (m_scroll_view != NULL) {
			size = math::v2(m_scroll_view->GetSize(0)->getImpl().getXY());
			if (m_adjust_param != NULL) {
				top = m_adjust_param->m_top_margin;
				bottom = m_adjust_param->m_bottom_margin;
				left = m_adjust_param->m_left_margin;
				right = m_adjust_param->m_right_margin;
			} else {
				top = ScePafWidget_BE1CEA2E.m_top_margin;
				bottom = ScePafWidget_BE1CEA2E.m_bottom_margin;
				left = ScePafWidget_BE1CEA2E.m_left_margin;
				right = ScePafWidget_BE1CEA2E.m_right_margin;
			}
		} else {
			size = math::v2(m_text->m_size.getImpl().getXY());
		}

		float max_x = ci.x - size.extract_x() * 0.5f;
		float min_x = (ci.x + 2.0f + right) - size.extract_x();

		if (m_caret_scroll_to.extract_x() > max_x)
			m_caret_scroll_to.set_x(max_x);
		if (m_caret_scroll_to.extract_x() < min_x)
			m_caret_scroll_to.set_x(min_x);

		float line_y = line.frame.extract_y();
		float max_y = line_y - size.extract_y() * 0.5f;
		float min_y = ((line_y + line.frame.extract_w()) + bottom) - size.extract_y();

		if (m_caret_scroll_to.extract_y() > max_y)
			m_caret_scroll_to.set_y(max_y);
		if (m_caret_scroll_to.extract_y() < min_y)
			m_caret_scroll_to.set_y(min_y);
		if (m_scroll_view != NULL) {
			math::v2 pos = (m_caret_scroll_to + math::v2(left, top) -
					(math::v2(m_size.getImpl().getXY()) - size) * 0.5f) *
				       math::v2(1.0f, -1.0f);
			math::v4 center = GetPosCenter(0, m_scroll_view->GetInternalWidget(),
						       math::v4(0.0f, 0.0f, 0.0f, 0.0f));

			pos = pos + math::v2(center.getImpl().getXY());

			math::v2 limit = math::v2(m_scroll_view->m_view_box.getImpl().getXY()) +
					 math::v2(size.extract_x() * 0.5f, size.extract_y() * -0.5f);

			if (pos.extract_x() < limit.extract_x())
				pos.set_x(limit.extract_x());
			if (pos.extract_y() > limit.extract_y())
				pos.set_y(limit.extract_y());
			m_scroll_view->SetViewFocus(pos, new Timer(Text::c_caretScrollPosMsec), true, true);
		} else {
			if (m_caret_scroll_to.extract_x() < 0.0f)
				m_caret_scroll_to.set_x(0.0f);
			if (m_caret_scroll_to.extract_y() < 0.0f)
				m_caret_scroll_to.set_y(0.0f);
			if (!(m_caret_scroll_from == m_caret_scroll_to))
				SetAnimListener(ANIM_CARET_SCROLL,
						new AnimListener(new Timer(Text::c_caretScrollPosMsec), this,
								 OnCaretScrollPos,
								 NULL, NULL),
						0);
		}
	}
	return 0;
}

int32_t TextBox::KillCaretScroll()
{
	if (ScePafWidget_56384DA3(this, ANIM_CARET_SCROLL) != 0)
		KillAnim(ANIM_CARET_SCROLL);
	return 0;
}

int32_t TextBox::KillCaretBlink()
{
	bool visible;

	m_text->GetCaretVisible(visible);
	if (visible && ScePafWidget_56384DA3(this, ANIM_CARET_BLINK) != 0) {
		math::v4 color;

		KillAnim(ANIM_CARET_BLINK);
		m_text->GetCaretColor(color);
		color.set_w(1.0f);
		m_text->SetCaretColor(color);
	}
	return 0;
}

uint32_t TextBox::ChangeState(uint32_t state, int32_t ev_type)
{
	uint32_t old = m_state;
	uint32_t ret = Text::ChangeState(state, ev_type);

	if (m_text != NULL && state != old && state == 0 && m_touch_state2) {
		KillCaretBlink();
		if (ScePafWidget_56384DA3(this, ANIM_CARET_SCROLL) != 0)
			KillAnim(ANIM_CARET_SCROLL);
		SetCaretScroll();
		SetCaretBlink();
	}
	return ret;
}

void TextBox::Update()
{
	if (m_touch_state2 && m_edit_listener != NULL)
		m_edit_listener->Update(this);
	Widget::Update();
}

void TextBox::UpdateSize()
{
	if (m_plane != NULL && m_text != NULL && !m_text->m_flags.m_prepare)
		m_plane->SetSize(math::v3(m_size.getImpl().getXYZ()));

	if (m_text != NULL) {
		math::v4 size = m_size;

		if (m_hscroll_bar != NULL) {
			float h = m_hscroll_bar->GetSize(0)->extract_y();

			if (size.extract_y() > h)
				size.set_y(size.extract_y() - h);
		}
		if (m_vscroll_bar != NULL) {
			float w = m_vscroll_bar->GetSize(0)->extract_x();

			if (size.extract_x() > w)
				size.set_x(size.extract_x() - w);
		}

		float top, bottom, left, right;

		if (m_adjust_param != NULL) {
			top = m_adjust_param->m_top_margin;
			bottom = m_adjust_param->m_bottom_margin;
			left = m_adjust_param->m_left_margin;
			right = m_adjust_param->m_right_margin;
		} else {
			top = ScePafWidget_BE1CEA2E.m_top_margin;
			bottom = ScePafWidget_BE1CEA2E.m_bottom_margin;
			left = ScePafWidget_BE1CEA2E.m_left_margin;
			right = ScePafWidget_BE1CEA2E.m_right_margin;
		}

		math::v2 visible((size.extract_x() - left) - right, (size.extract_y() - bottom) - top);

		SetLayoutAttribute(graph::TextLayoutAttribute_Size, visible);
	}
	if (m_glow != NULL && m_text != NULL && !m_text->m_flags.m_prepare)
		m_glow->SetSize(math::v3(m_size.getImpl().getXYZ()));
}

int32_t TextBox::OnDraw(graph::DrawInfo &info)
{
	float alpha = m_color_alpha * m_meta_a_sum * m_show_a_sum;

	if (m_plane != NULL)
		m_plane->OnDraw(m_world_matrix, m_color, alpha, info);
	if (m_text != NULL)
		m_text->OnDraw(m_world_matrix, m_color, alpha, info);
	if (m_glow != NULL)
		m_glow->OnDraw(m_world_matrix, m_color, alpha, info);
	return 0;
}

}
}

int32_t ScePafWidget_AFE7A212(paf::ui::TextBox *text_box)
{
	using namespace paf;
	using namespace paf::ui;

	if (text_box->m_glow != NULL)
		return SCE_PAF_ERROR_ALREADY_CREATED;

	text_box->m_glow = new graph::PlaneObj(graph::PlaneObjStyle());
	if (text_box->m_glow == NULL)
		return SCE_PAF_ERROR_NO_OBJECT;

	text_box->m_glow->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f));
	text_box->AddDrawObj(Widget::DrawObjEntry(text_box->m_glow, TextBox::OBJ_GLOW), true);
	return 0;
}

namespace paf {
namespace ui {

int32_t TextBox::SetTexture(const intrusive_ptr<graph::Surface> &surf, int32_t obj, uint32_t index)
{
	int32_t ret = 0;

	if (obj != OBJ_GLOW)
		return Text::SetTexture(surf, obj, index);
	if (surf.get() != NULL)
		ret = ScePafWidget_AFE7A212(this);
	if (m_glow != NULL) {
		m_glow->SetTexture(surf, index);
		return 0;
	}
	return ret;
}

}
}

int32_t ScePafWidget_C4157A69(paf::ui::TextBox *text_box, const paf::wstring &str, int32_t obj)
{
	using namespace paf;
	using namespace paf::ui;

	if (text_box->m_touch_state2 && text_box->m_edit_listener != NULL)
		text_box->m_edit_listener->Commit(text_box);

	int32_t ret;
	uint32_t max = text_box->m_max_length;

	if (max != 0 && max < str.size()) {
		wstring cut = str.substr(0, max);

		ret = text_box->Text::SetString(cut, obj);
	} else {
		ret = text_box->Text::SetString(str, obj);
	}
	if (text_box->m_touch_state2 && text_box->m_edit_listener != NULL)
		text_box->m_edit_listener->NotifyCaretMove(text_box);

	Event ev(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0, 0, 0, 0, 0);

	text_box->DoEvent(TextBox::CB_TEXT_BOX_STRING_CHANGED, &ev);
	return ret;
}

namespace paf {
namespace ui {

int32_t TextBox::SetString(const wstring &str, int32_t obj)
{
	int32_t ret;

	if (m_text_filter != NULL) {
		wstring filtered = m_text_filter(str);

		ret = ScePafWidget_C4157A69(this, filtered, obj);
	} else {
		ret = ScePafWidget_C4157A69(this, str, obj);
	}
	return ret;
}

int32_t TextBox::ReplaceStringCut(uint32_t index, uint32_t len, const wstring &str)
{
	uint32_t cur;
	int32_t ret = GetLength(cur);

	if (ret == 0) {
		if (index + len > cur) {
			ret = SCE_PAF_ERROR_INVALID_ARGUMENT;
		} else {
			uint32_t max = m_max_length;

			if (max != 0 && max < cur - len + str.size()) {
				wstring cut = str.substr(0, max - cur + len);

				ret = Text::ReplaceString(index, len, cut);
			} else {
				ret = Text::ReplaceString(index, len, str);
			}
			if (m_touch_state2 && m_edit_listener != NULL)
				m_edit_listener->NotifyCaretMove(this);

			Event ev(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0, 0, 0, 0, 0);

			DoEvent(CB_TEXT_BOX_STRING_CHANGED, &ev);
		}
	}
	return ret;
}

int32_t TextBox::ReplaceString(uint32_t index, uint32_t len, const wstring &str)
{
	int32_t ret;

	if (m_text_filter != NULL) {
		wstring filtered = m_text_filter(str);

		ret = ReplaceStringCut(index, len, filtered);
	} else {
		ret = ReplaceStringCut(index, len, str);
	}
	return ret;
}

int32_t TextBox::ReplaceStringNotify(uint32_t index, uint32_t len, const wstring &str)
{
	int32_t ret = Text::ReplaceString(index, len, str);
	Event ev(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0, 0, 0, 0, 0);

	DoEvent(CB_TEXT_BOX_STRING_REPLACED, &ev);
	return ret;
}

void TextBox::UpdateTouchMode()
{
	int32_t mode = 4;

	if (m_focus_link != NULL || m_touch_modes == 0 || (m_edit_mode && !m_touch_state2))
		mode = 1;
	m_touch_gesture_mask = mode;
}

int32_t TextBox::MoveCaretUp()
{
	KillCaretBlink();

	int32_t ret = Text::MoveCaretUp();

	if (ret == 0) {
		if (ScePafWidget_56384DA3(this, ANIM_CARET_SCROLL) != 0)
			KillAnim(ANIM_CARET_SCROLL);
		if (m_touch_state2 && m_edit_listener != NULL)
			m_edit_listener->NotifyCaretMove(this);

		Event ev(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0, 0, 0, 0, 0);

		DoEvent(CB_TEXT_BOX_CARET_UP, &ev);
		SetCaretScroll();
	}
	SetCaretBlink();
	return ret;
}

int32_t TextBox::MoveCaretDown()
{
	KillCaretBlink();

	int32_t ret = Text::MoveCaretDown();

	if (ret == 0) {
		if (ScePafWidget_56384DA3(this, ANIM_CARET_SCROLL) != 0)
			KillAnim(ANIM_CARET_SCROLL);
		if (m_touch_state2 && m_edit_listener != NULL)
			m_edit_listener->NotifyCaretMove(this);

		Event ev(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0, 0, 0, 0, 0);

		DoEvent(CB_TEXT_BOX_CARET_DOWN, &ev);
		SetCaretScroll();
	}
	SetCaretBlink();
	return ret;
}

int32_t TextBox::MoveCaretLeft()
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL) {
		uint32_t caret;

		m_text->GetCaretIndex(caret);
		if (caret != 0) {
			SetCaretIndex(caret - 1);

			Event ev(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0, 0, 0, 0, 0);

			DoEvent(CB_TEXT_BOX_CARET_LEFT, &ev);
		}
		ret = 0;
	}
	return ret;
}

int32_t TextBox::MoveCaretRight()
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL) {
		uint32_t caret;
		uint32_t len;

		m_text->GetCaretIndex(caret);
		m_text->GetLength(len);
		if (caret < len) {
			SetCaretIndex(caret + 1);

			Event ev(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0, 0, 0, 0, 0);

			DoEvent(CB_TEXT_BOX_CARET_RIGHT, &ev);
		}
		ret = 0;
	}
	return ret;
}

int32_t TextBox::SetCaretPos(const math::v2 &pos)
{
	KillCaretBlink();

	int32_t ret = Text::SetCaretPos(pos);

	if (ret == 0) {
		if (ScePafWidget_56384DA3(this, ANIM_CARET_SCROLL) != 0)
			KillAnim(ANIM_CARET_SCROLL);
		if (m_touch_state2 && m_edit_listener != NULL)
			m_edit_listener->NotifyCaretMove(this);

		Event ev(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0, 0, 0, 0, 0);

		DoEvent(CB_TEXT_BOX_CARET_POS_CHANGED, &ev);
		SetCaretScroll();
	}
	SetCaretBlink();
	return ret;
}

int32_t TextBox::SetCaretIndex(uint32_t index)
{
	KillCaretBlink();

	int32_t ret = Text::SetCaretIndex(index);

	if (ret == 0) {
		if (ScePafWidget_56384DA3(this, ANIM_CARET_SCROLL) != 0)
			KillAnim(ANIM_CARET_SCROLL);
		SetCaretScroll();
	}
	SetCaretBlink();
	if (m_touch_state2 && m_edit_listener != NULL)
		m_edit_listener->NotifyCaretMove(this);
	return ret;
}

int32_t TextBox::SetSelectionRange(uint32_t index, uint32_t len)
{
	int32_t ret = Text::SetSelectionRange(index, len);

	if (ret == 0) {
		if (len != 0)
			m_touch_state1 = true;
		else
			m_touch_state1 = false;
	}
	UpdateTouchMode();
	return ret;
}

int32_t TextBox::SetCaretVisible(bool visible)
{
	KillCaretBlink();

	int32_t ret = Text::SetCaretVisible(visible);

	SetCaretBlink();
	return ret;
}

int32_t TextBox::DetachHScrollBar()
{
	int32_t ret = SCE_PAF_ERROR_ALREADY_CREATED;

	if (m_hscroll_bar != NULL) {
		if (m_hscroll_bar->IsInherit(ScrollBarTouch::TypeName()))
			m_hscroll_bar->DeleteEventCallback(0x10000000, OnHScrollBarChanged, this);
		ret = 0;
		HideBar(m_hscroll_bar);
		m_hscroll_bar = NULL;
		UpdateSize();
	}
	return ret;
}

int32_t TextBox::AttachHScrollBar(ScrollBar *bar)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL) {
		if (bar != NULL) {
			DetachHScrollBar();
			m_hscroll_bar = bar;
			HideBar(bar);
			m_hscroll_bar->SetScrollType(ScrollBar::SCROLL_TYPE_HORIZONTAL);
			if (m_hscroll_bar->IsInherit(ScrollBarTouch::TypeName()))
				m_hscroll_bar->AddEventListener(
					0x10000000, new EventCBListener(OnHScrollBarChanged, this), false);
			UpdateSize();
			if (!m_text->m_flags.m_prepare)
				AdjustSize();
			ret = 0;
		} else {
			ret = SCE_PAF_ERROR_INVALID_ARGUMENT;
		}
	}
	return ret;
}

int32_t TextBox::DetachVScrollBar()
{
	int32_t ret = SCE_PAF_ERROR_ALREADY_CREATED;

	if (m_vscroll_bar != NULL) {
		if (m_vscroll_bar->IsInherit(ScrollBarTouch::TypeName()))
			m_vscroll_bar->DeleteEventCallback(0x10000000, OnVScrollBarChanged, this);
		ret = 0;
		HideBar(m_vscroll_bar);
		m_vscroll_bar = NULL;
		UpdateSize();
	}
	return ret;
}

int32_t TextBox::AttachVScrollBar(ScrollBar *bar)
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL) {
		if (bar != NULL) {
			DetachVScrollBar();
			m_vscroll_bar = bar;
			HideBar(bar);
			m_vscroll_bar->SetScrollType(ScrollBar::SCROLL_TYPE_VERTICAL);
			if (m_vscroll_bar->IsInherit(ScrollBarTouch::TypeName()))
				m_vscroll_bar->AddEventListener(
					0x10000000, new EventCBListener(OnVScrollBarChanged, this), false);
			UpdateSize();
			if (!m_text->m_flags.m_prepare)
				AdjustSize();
			ret = 0;
		} else {
			ret = SCE_PAF_ERROR_INVALID_ARGUMENT;
		}
	}
	return ret;
}

int32_t TextBox::SetMaxLength(uint32_t len)
{
	int32_t ret = 0;

	if (len != m_max_length) {
		wstring str;

		ret = GetString(str, 0);
		m_max_length = len;
		if (ret == 0)
			ret = SetString(str, 0);
	}
	return ret;
}

int32_t TextBox::GetMaxLength(uint32_t &len)
{
	len = m_max_length;
	return 0;
}

int32_t TextBox::SetPasswordMode(bool mode)
{
	int32_t ret = SetStyleAttribute(graph::TextStyleAttribute_Mask, 0, 0, mode);

	if (ret == 0 && m_touch_state2 && m_edit_listener != NULL)
		m_edit_listener->NotifyPasswordMode(this);
	return ret;
}

int32_t TextBox::GetPasswordMode(bool &mode)
{
	return GetStyleAttribute(graph::TextStyleAttribute_Mask, 0, mode);
}

int32_t TextBox::SetEditMode(bool mode)
{
	int32_t ret = 0;

	if (mode != ((m_touch_modes & 8) != 0)) {
		EndEdit();
		if (mode)
			m_touch_modes |= 8;
		else
			m_touch_modes &= ~8;
		UpdateTouchMode();
	}
	return ret;
}

int32_t TextBox::GetEditMode(bool &mode)
{
	mode = m_edit_mode;
	return 0;
}

int32_t TextBox::SetEditAutoTransitionMode(bool mode)
{
	if (mode != m_edit_auto_transition)
		m_edit_auto_transition = mode;
	return 0;
}

int32_t TextBox::GetEditAutoTransitionMode(bool &mode)
{
	mode = m_edit_auto_transition;
	return 0;
}

int32_t TextBox::SetEditListener(TextBoxEditListener *listener)
{
	if (listener != m_edit_listener) {
		EndEdit();
		m_edit_listener = listener;
	}
	return 0;
}

int32_t TextBox::GetEditListener(TextBoxEditListener *&listener)
{
	listener = m_edit_listener;
	return 0;
}

int32_t TextBox::SetImeParam(const ImeParam &param)
{
	m_ime_param.supportedLanguages = param.supportedLanguages;
	m_ime_param.type = param.type;
	m_ime_param.languagesForced = param.languagesForced;
	m_ime_param.option = param.option;
	m_ime_param.enterLabel = param.enterLabel;
	m_ime_param.filter = param.filter;
	return 0;
}

int32_t TextBox::GetImeParam(ImeParam &param)
{
	param.supportedLanguages = m_ime_param.supportedLanguages;
	param.type = m_ime_param.type;
	param.languagesForced = m_ime_param.languagesForced;
	param.option = m_ime_param.option;
	param.enterLabel = m_ime_param.enterLabel;
	param.filter = m_ime_param.filter;
	return 0;
}

int32_t TextBox::BeginEdit()
{
	int32_t ret = SCE_PAF_ERROR_NO_OBJECT;

	if (m_text != NULL) {
		if (m_edit_mode && !m_touch_state2 && m_edit_listener != NULL) {
			ret = m_edit_listener->BeginEdit(this);
			if (ret == 0) {
				bool ellipsis;

				m_touch_state2 = true;
				m_text->GetLayoutAttribute(graph::TextLayoutAttribute_Ellipsis, ellipsis);
				if (ellipsis) {
					m_ellipsis_saved = true;
					SetLayoutAttribute(graph::TextLayoutAttribute_Ellipsis, false);
				}
				SetCaretVisible(true);

				uint32_t len;

				m_text->GetLength(len);
				SetCaretIndex(len);

				Event ev(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0, 0, 0, 0, 0);

				DoEvent(CB_TEXT_BOX_EDIT_BEGIN, &ev);
			}
		} else {
			ret = SCE_PAF_ERROR_NO_ENTRY;
		}
	}
	UpdateTouchMode();
	return ret;
}

bool TextBox::IsEditing()
{
	return m_touch_state2;
}

}
}

int32_t ScePafWidget_48D5B0D2(uint16_t *dst, uint32_t *dst_len, const uint16_t *src,
			      uint32_t src_len)
{
	int32_t ret = 0;

	if (*dst_len >= src_len) {
		uint32_t copied = 0;
		int32_t found = 0;

		if (src_len != 0) {
			for (uint32_t i = 0; i < src_len; i++) {
				if (src[i] == 32) {
					found = 1;
				} else {
					*dst++ = src[i];
					copied++;
				}
			}
			if (found != 0)
				*dst_len = copied;
		}
		return found;
	}
	return ret;
}

int32_t ScePafWidget_3EB376F2(paf::ui::TextBox *text_box, paf::ui::TextBoxTextFilter filter)
{
	text_box->m_text_filter = filter;
	return 0;
}

int32_t ScePafWidget_D459D8B6(paf::ui::TextBox *text_box, paf::ui::TextBoxTextFilter *filter)
{
	*filter = text_box->m_text_filter;
	return 0;
}

int32_t ScePafWidget_10E22E5A(paf::ui::TextBox *text_box, bool onoff)
{
	return SCE_PAF_ERROR_NO_ENTRY;
}

int32_t ScePafWidget_E0D414F7(paf::ui::TextBox *text_box, bool *enabled)
{
	*enabled = text_box->m_paste_enabled;
	return 0;
}

namespace paf {
namespace ui {

int32_t TextBox::HandleTouchPress(Event *e)
{
	uint32_t port = (e->m_detail >> 20) & 0xf;
	uint32_t id = (uint8_t)e->m_detail;
	int32_t ret = SCE_PAF_ERROR_BUSY;

	if (id < 8) {
		HitInfo *hit = &m_env->m_touch_hit[0][id];

		if (hit != NULL && hit->m_widget == this) {
			math::v2 pos;

			ret = GetTouchTextPos(port, id, pos);
			if (ret == 0) {
				ScePafWidget_55EC0090(this, pos);
				if (m_touch_state2) {
					SetCaretPos(pos);
				} else if (m_glow != NULL) {
					m_glow->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f));
				}
			}
		}
	}
	return ret;
}

int32_t TextBox::HandleTouchRelease()
{
	if (m_glow != NULL)
		m_glow->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.0f));
	return Text::HandleTouchRelease();
}

}
}

namespace paf {
namespace ui {

int32_t TextBox::ScePafWidget_695200BA(Event *e)
{
	int32_t ret = Text::ScePafWidget_695200BA(e);

	if (ret < 0 && !m_touch_state2) {
		if (m_glow != NULL) {
			m_glow->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f));
			SetObjAnim(OBJ_GLOW, 0x20300, math::v4(1.0f, 1.0f, 1.0f, 0.0f),
				   new Timer(500.0f, Timer::FUNC_EXPONENT), 2);
		}
		ScePafWidget_2B1A1493(this, 1);

		Event ev(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0, 0, 0, 0, 0);

		DoEvent(CB_TEXT_BOX_TAPPED, &ev);
		ret = BeginEdit();
	}
	return ret;
}

int32_t TextBox::HandleKeycodeEvent(Event *e)
{
	if (m_env != NULL && ScePafWidget_A6B659BB(m_env, 0) && (uint8_t)e->m_detail == 27 &&
	    (uint16_t)e->m_type == 17) {
		if (m_glow != NULL) {
			m_glow->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f));
			SetObjAnim(OBJ_GLOW, 0x20300, math::v4(1.0f, 1.0f, 1.0f, 0.0f),
				   new Timer(500.0f, Timer::FUNC_EXPONENT), 2);
		}
		ScePafWidget_2B1A1493(this, 1);

		Event ev(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0, 0, 0, 0, 0);

		DoEvent(CB_TEXT_BOX_TAPPED, &ev);
		BeginEdit();
		e->m_type = (e->m_type & 0xf0ffffff) | 0x1000000;
	}
	return Text::HandleKeycodeEvent(e);
}

int32_t TextBox::UpdateTouch(const math::v2 &pos)
{
	int32_t ret = 0;

	if (m_touch_state2) {
		SetCaretPos(pos);
		ret = Text::UpdateTouch(pos);
	} else {
		math::v2 visible;

		m_text->GetLayoutAttribute(graph::TextLayoutAttribute_VisiblePos, visible);
		visible = visible + (m_touch_pos - pos);
		if (visible.extract_x() >= 0.0f) {
			if (visible.extract_x() > m_scroll_max.extract_x())
				visible.set_x(m_scroll_max.extract_x());
		} else {
			visible.set_x(0.0f);
		}
		if (visible.extract_y() >= 0.0f) {
			if (visible.extract_y() > m_scroll_max.extract_y())
				visible.set_y(m_scroll_max.extract_y());
		} else {
			visible.set_y(0.0f);
		}
		SetVisiblePos(visible);
	}
	return ret;
}

int32_t TextBox::SetVisiblePos(const math::v2 &pos)
{
	if (m_hscroll_bar != NULL)
		m_hscroll_bar->SetValue(pos.extract_x(), false);
	if (m_vscroll_bar != NULL)
		m_vscroll_bar->SetValue(pos.extract_y(), false);
	return Text::SetVisiblePos(pos);
}

void TextBox::OnVScrollBarChanged(int32_t type, Handler *self, Event *e, void *data)
{
	TextBox *text_box = (TextBox *)data;
	math::v2 visible;

	text_box->m_text->GetLayoutAttribute(graph::TextLayoutAttribute_VisiblePos, visible);
	visible.set_y(text_box->m_vscroll_bar->GetValue());
	text_box->m_text->SetLayoutAttribute(graph::TextLayoutAttribute_VisiblePos, visible);
}

void TextBox::OnHScrollBarChanged(int32_t type, Handler *self, Event *e, void *data)
{
	TextBox *text_box = (TextBox *)data;
	math::v2 visible;

	text_box->m_text->GetLayoutAttribute(graph::TextLayoutAttribute_VisiblePos, visible);
	visible.set_x(text_box->m_hscroll_bar->GetValue());
	text_box->m_text->SetLayoutAttribute(graph::TextLayoutAttribute_VisiblePos, visible);
}

void TextBox::OnCaretScrollPos(Timer *t, AnimListener *listener, void *data1, void *data2,
			       void *data3)
{
	TextBox *text_box = (TextBox *)data1;

	text_box->SetVisiblePos(text_box->m_caret_scroll_from +
				(text_box->m_caret_scroll_to - text_box->m_caret_scroll_from) *
					t->m_mapped_x);
}

void TextBox::OnCaretBlinkFadeIn(Timer *t, AnimListener *listener, void *data1, void *data2,
				 void *data3)
{
	TextBox *text_box = (TextBox *)data1;
	math::v4 color;

	text_box->m_text->GetCaretColor(color);
	color.set_w(t->m_mapped_x);
	text_box->m_text->SetCaretColor(color);
}

void TextBox::OnCaretBlinkFadeOut(Timer *t, AnimListener *listener, void *data1, void *data2,
				  void *data3)
{
	TextBox *text_box = (TextBox *)data1;
	math::v4 color;
	float alpha = t->m_mapped_x;

	text_box->m_text->GetCaretColor(color);
	color.set_w(1.0f - alpha);
	text_box->m_text->SetCaretColor(color);
}

int32_t TextBox::AdjustSize()
{
	float top, bottom, left, right;

	if (m_adjust_param != NULL) {
		top = m_adjust_param->m_top_margin;
		bottom = m_adjust_param->m_bottom_margin;
		left = m_adjust_param->m_left_margin;
		right = m_adjust_param->m_right_margin;
	} else {
		top = ScePafWidget_BE1CEA2E.m_top_margin;
		bottom = ScePafWidget_BE1CEA2E.m_bottom_margin;
		left = ScePafWidget_BE1CEA2E.m_left_margin;
		right = ScePafWidget_BE1CEA2E.m_right_margin;
	}

	math::v4 half = math::v4(left - right, bottom - top, 0.0f, 0.0f) * 0.5f;
	math::v2 bounds;

	m_text->GetBounds(bounds);

	math::v3 text_size = m_text->m_size;

	m_scroll_max = bounds - math::v2(text_size.getImpl().getXY());
	if (m_scroll_max.extract_x() < 0.0f)
		m_scroll_max.set_x(0.0f);
	if (m_scroll_max.extract_y() < 0.0f)
		m_scroll_max.set_y(0.0f);

	math::v4 hsize(0.0f, 0.0f, 0.0f, 0.0f);
	math::v4 vsize(0.0f, 0.0f, 0.0f, 0.0f);

	if (m_hscroll_bar != NULL)
		hsize = *m_hscroll_bar->GetSize(0);
	if (m_vscroll_bar != NULL)
		vsize = *m_vscroll_bar->GetSize(0);

	math::v4 bar_half = math::v4(-vsize.extract_x(), hsize.extract_y(), 0.0f, 0.0f) * 0.5f;

	if (m_hscroll_bar != NULL) {
		m_hscroll_bar->SetPos(
			math::v4(half.extract_x() + bar_half.extract_x(),
				 half.extract_y() - text_size.extract_y() * 0.5f, 0.0f, 0.0f),
			NULL, ANIM_MOVE, 0, 0.0f, NULL, NULL);
		m_hscroll_bar->SetSize(math::v4(text_size.extract_x(), hsize.extract_y(),
						hsize.extract_z(), hsize.extract_w()),
				       NULL, ANIM_SIZE, 0, 0.0f, NULL, NULL);
		m_hscroll_bar->SetMaxValue(bounds.extract_x());
		m_hscroll_bar->SetVisibleAmount(text_size.extract_x());

		math::v2 visible;

		m_text->GetLayoutAttribute(graph::TextLayoutAttribute_VisiblePos, visible);
		visible.set_x(m_hscroll_bar->GetValue());
		m_text->SetLayoutAttribute(graph::TextLayoutAttribute_VisiblePos, visible);
		if (bounds.extract_x() > text_size.extract_x()) {
			ShowBar(m_hscroll_bar);
		} else {
			HideBar(m_hscroll_bar);
			if (m_vscroll_bar == NULL) {
				int32_t align;

				m_text->GetLayoutAttribute(graph::TextLayoutAttribute_AlignY, align);
				if (align > 0) {
					if (align <= 1)
						bar_half.set_y(0.0f);
					else if (align <= 2)
						bar_half.set_y(-bar_half.extract_y());
				}
			}
		}
	}
	if (m_vscroll_bar != NULL) {
		m_vscroll_bar->SetPos(
			math::v4(half.extract_x() + text_size.extract_x() * 0.5f,
				 half.extract_y() + bar_half.extract_y(), 0.0f, 0.0f),
			NULL, ANIM_MOVE, 0, 0.0f, NULL, NULL);
		m_vscroll_bar->SetSize(math::v4(vsize.extract_x(), text_size.extract_y(),
						vsize.extract_z(), vsize.extract_w()),
				       NULL, ANIM_SIZE, 0, 0.0f, NULL, NULL);
		m_vscroll_bar->SetMaxValue(bounds.extract_y());
		m_vscroll_bar->SetVisibleAmount(text_size.extract_y());

		math::v2 visible;

		m_text->GetLayoutAttribute(graph::TextLayoutAttribute_VisiblePos, visible);
		visible.set_y(m_vscroll_bar->GetValue());
		m_text->SetLayoutAttribute(graph::TextLayoutAttribute_VisiblePos, visible);
		if (bounds.extract_y() > text_size.extract_y()) {
			ShowBar(m_vscroll_bar);
		} else {
			HideBar(m_vscroll_bar);
			if (m_hscroll_bar == NULL) {
				int32_t align;

				m_text->GetLayoutAttribute(graph::TextLayoutAttribute_AlignX, align);
				if (align > 0) {
					if (align <= 1)
						bar_half.set_x(0.0f);
					else if (align <= 2)
						bar_half.set_x(-bar_half.extract_x());
				}
			}
		}
	}

	math::v4 size = m_size;

	m_size = math::v4(-1.0f, -1.0f, size.extract_z(), size.extract_w());
	SetSize_core_force(math::v4(((text_size.extract_x() + vsize.extract_x()) + left) + right,
				    ((text_size.extract_y() + hsize.extract_y()) + bottom) + top,
				    size.extract_z(), size.extract_w()),
			   0xFF02, false);
	m_text->SetPos(math::v3((half + bar_half).getImpl().getXYZ()));
	return 0;
}

}
}

namespace paf {
namespace ui {

const char TextBoxParam::m_param_name[] = "text_box_base";

TextBoxParam::TextBoxParam()
	: m_max_length(1024), m_password_mode(false), m_paste_mode(true), m_edit_mode(false),
	  m_edit_auto_transition_mode(false)
{
	m_selection_mode = true;
}

const char *TextBoxParam::GetParamName() const
{
	return m_param_name;
}

const char *TextBoxParam::ParamName()
{
	return m_param_name;
}

TextBoxParam::~TextBoxParam()
{
}

int32_t TextBoxParam::Create(int32_t mode)
{
	int32_t ret = TextParam::Create(mode);

	if (ret == 0 && m_serial != NULL) {
		m_serial->AddParam("max_length", &m_max_length, ResourceParam::SEMANTIC_INT);
		if (m_serial != NULL) {
			m_serial->AddParam("password_mode", &m_password_mode, ResourceParam::SEMANTIC_BOOL);
			if (m_serial != NULL) {
				m_serial->AddParam("paste_mode", &m_paste_mode, ResourceParam::SEMANTIC_BOOL);
				if (m_serial != NULL) {
					m_serial->AddParam("edit_mode", &m_edit_mode,
							   ResourceParam::SEMANTIC_BOOL);
					if (m_serial != NULL) {
						m_serial->AddParam("edit_auto_transition_mode",
								   &m_edit_auto_transition_mode,
								   ResourceParam::SEMANTIC_BOOL);
						if (m_serial != NULL) {
							m_serial->AddParam("ime_supported_languages",
									   &m_ime.supportedLanguages,
									   ResourceParam::SEMANTIC_INT);
							if (m_serial != NULL) {
								m_serial->AddParam("ime_languages_forced",
										   &m_ime.languagesForced,
										   ResourceParam::SEMANTIC_BOOL);
								if (m_serial != NULL) {
									m_serial->AddParam(
										"ime_type", &m_ime.type,
										ResourceParam::SEMANTIC_INT);
									if (m_serial != NULL) {
										m_serial->AddParam(
											"ime_option",
											&m_ime.option,
											ResourceParam::SEMANTIC_INT);
										if (m_serial != NULL) {
											m_serial->AddParam(
												"h_scrollbar",
												&m_h_scrollbar,
												ResourceParam::
													SEMANTIC_WIDGETREF);
											if (m_serial != NULL)
												m_serial->AddParam(
													"v_scrollbar",
													&m_v_scrollbar,
													ResourceParam::
														SEMANTIC_WIDGETREF);
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
	return ret;
}

int32_t TextBoxParam::Set()
{
	int32_t ret = TextParam::Set();

	if (ret == 0) {
		TextBox *text_box = (TextBox *)m_widget;

		text_box->SetMaxLength(m_max_length);
		text_box->SetPasswordMode(m_password_mode);
		text_box->SetEditMode(m_edit_mode);
		text_box->SetEditAutoTransitionMode(m_edit_auto_transition_mode);
		text_box->SetImeParam(m_ime);
		if (text_box->FindChild(m_h_scrollbar, 0) != NULL) {
			if (text_box->FindChild(m_h_scrollbar, 0)->IsInherit(ScrollBar::TypeName())) {
				ScrollBar *bar = (ScrollBar *)text_box->FindChild(m_h_scrollbar, 0);

				if (bar != NULL)
					text_box->AttachHScrollBar(bar);
			}
		}
		if (text_box->FindChild(m_v_scrollbar, 0) != NULL) {
			if (text_box->FindChild(m_v_scrollbar, 0)->IsInherit(ScrollBar::TypeName())) {
				ScrollBar *bar = (ScrollBar *)text_box->FindChild(m_v_scrollbar, 0);

				if (bar != NULL)
					text_box->AttachVScrollBar(bar);
			}
		}
	}
	return ret;
}

int32_t TextBoxParam::Get()
{
	int32_t ret = TextParam::Get();

	if (ret == 0) {
		TextBox *text_box = (TextBox *)m_widget;

		m_max_length = text_box->m_max_length;
		text_box->GetPasswordMode(m_password_mode);

		int8_t flags = text_box->m_touch_modes;

		m_paste_mode = (flags & 4) != 0;
		m_edit_mode = (flags & 8) != 0;
		m_edit_auto_transition_mode = text_box->m_edit_auto_transition;
		text_box->GetImeParam(m_ime);
	}
	return ret;
}

const char TextBoxStyleParam::m_param_name[] = "style_text_box";

TextBoxStyleParam::TextBoxStyleParam()
{
	m_glow_param.m_name = "glowobj";
	m_glow_param.m_index = TextBox::OBJ_GLOW;
	m_drawobj_param_list.push_back(&m_glow_param);
}

const char *TextBoxStyleParam::GetParamName() const
{
	return m_param_name;
}

const char *TextBoxStyleParam::ParamName()
{
	return m_param_name;
}

TextBoxStyleParam::~TextBoxStyleParam()
{
}

int32_t TextBoxStyleParam::SetEach(Widget *widget)
{
	if (m_glow_param.m_id.GetIDHash() != 0) {
		ScePafWidget_AFE7A212(static_cast<TextBox *>(widget));
		m_glow_param.m_color = math::v4(1.0f, 1.0f, 1.0f, 0.0f);
	}
	return TextStyleParam::SetEach(widget);
}

const char TextBoxSoundParam::m_param_name[] = "sound_text_box";

TextBoxSoundParam::TextBoxSoundParam()
{
	m_sound_list.push_back(Sound("tapped", 1));
}

const char *TextBoxSoundParam::GetParamName() const
{
	return m_param_name;
}

const char *TextBoxSoundParam::ParamName()
{
	return m_param_name;
}

TextBoxSoundParam::~TextBoxSoundParam()
{
}

}
}

using namespace paf;
using namespace paf::ui;

static Widget *NewTextBox(Widget *parent)
{
	return new TextBox(parent, NULL);
}

static WidgetParam *NewTextBoxParam()
{
	return new TextBoxParam;
}

static VersatileParam *NewTextBoxLayoutParam()
{
	return new WidgetLayoutParam;
}

static VersatileParam *NewTextBoxFocusParam()
{
	return new WidgetFocusParam;
}

static VersatileParam *NewTextBoxStyleParam()
{
	return new TextBoxStyleParam;
}

static VersatileParam *NewTextBoxHandlerParam()
{
	return new WidgetHandlerParam;
}

static VersatileParam *NewTextBoxSoundParam()
{
	return new TextBoxSoundParam;
}

FactoryFuncTable g_factoryTextBox = {
	TextBox::TypeName(),
	NewTextBox,
	NewTextBoxParam,
	NewTextBoxLayoutParam,
	NewTextBoxFocusParam,
	NewTextBoxStyleParam,
	NewTextBoxHandlerParam,
	NewTextBoxSoundParam,
};
