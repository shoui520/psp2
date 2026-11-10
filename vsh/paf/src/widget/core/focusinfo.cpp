/*
 * paf::ui::FocusInfo (TOOL 0x811416a8..0x81144516): the focus cursor of one panel and the
 * widget it is on; as far as converted, the moves it remembers. No record or string names the
 * file.
 */
#include <stddef.h>
#include <stdint.h>
#include <common/resource_hooks.h>
#include <graphics/drawobj/focus_shader.h>
#include <widget/core/environment.h>
#include <widget/core/widget.h>
#include <widget/w_button_base.h>
#include <widget/w_list_item.h>
#include <widget/w_scene.h>
#include <widget/w_text.h>
#include <timer/timer.h>
#include <widget/w_plane.h>
#include <graphics/drawobj/plane_obj.h>

#ifndef SCE_PAF_ERROR_NO_MEMORY
#define SCE_PAF_ERROR_NO_MEMORY (-2136014814) /* 0x80AF0022 */
#endif

#if !defined(FW_TYPE_CEX)
extern "C" void ScePafAutoTestTty_9E10E873(int32_t type, const char *format, ...);
#endif

namespace paf {
namespace ui {

/* the child of the list item its text auto scroll target names, if it is of the type */
template<class T>
static inline __attribute__((always_inline)) T *FindAutoScrollChild(ListItem *item)
{
	T *found = NULL;
	IDParam id(item->m_text_auto_scroll_target);

	if (item->FindChild(id, 0) != NULL) {
		IDParam id2(item->m_text_auto_scroll_target);
		Widget *child = item->FindChild(id2, 0);

		if (child->IsInherit(T::TypeName())) {
			IDParam id3(item->m_text_auto_scroll_target);

			found = (T *)item->FindChild(id3, 0);
		}
	}
	return found;
}

/* the focus cursor: a plane drawn with its draw objects, or with the part planes alone where
   its target is drawn as part shapes (TypeName "focusplane"; class name ours) */
class FocusPlane : public Plane {
public:
	FocusPlane(Widget *parent) : Plane(parent, NULL)
	{
	}

	virtual ~FocusPlane()
	{
	}

	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	virtual void UpdateSize();
	virtual int32_t OnDraw(graph::DrawInfo &info);

	/* count part planes added, drawn with the focus shader (0x811447f0; name ours) */
	int32_t AddParts(int32_t count);
};

static Widget *KeepCursor(Widget *widget, void *arg);

FocusInfo::FocusInfo()
{
	m_cursor = NULL;
	m_target = NULL;
	m_previous = NULL;
	reserved0 = 0;
	m_target_pos = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
	m_touch_move = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
	m_shape = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
	m_color = math::v4(0.0f, 191.0f / 255.0f, 1.0f, 1.0f);
	m_move_mode = 0;
	m_move_mode_old = 0;
	reserved1a = math::v4(0.0f, 0.0f, 0.0f, 1.0f);
	m_leave_time = get_time_internal();
	m_shown = false;
	m_was_shown = false;
	m_enabled = false;
	m_left_saved = false;
	m_moving_only = true;
	m_default_texture = false;
	m_show_wait = 0;
	m_history_count = 0;
	m_texture = NULL;
	graph::FocusShader::Init();
}

FocusInfo::~FocusInfo()
{
	graph::FocusShader::Term();
	if (m_texture != NULL)
		m_texture->Release();
}

void FocusInfo::CreateCursor(Widget *parent)
{
	Widget *cursor = new FocusPlane(parent);

	m_cursor = cursor;
	cursor->GetDrawObj(0)->SetShader(graph::FocusShader::GetShader());
	cursor->m_hit_detect_b_mode = 2;
	cursor->m_hit_detect_mode = 2;
	cursor->m_transition_complete = false;
	cursor->m_transition = 0xFF;
	cursor->DoTransitionReverse_core(0.0f, true);
	ScePafWidget_4588699E(cursor, NULL, 2, 0.0f, NULL);
	s_cursor_parent = parent;
	cursor->m_keep_cb = KeepCursor;
	cursor->m_parent_protected = true;
	cursor->m_keep_arg = this;
	m_leave_time = get_time_internal();
}

void FocusInfo::UpdateHighlight()
{
	if (!m_shown)
		return;

	ListItem *item = ListItem::GetListItem(m_target);

	if (item != NULL) {
		ButtonBase *button = FindAutoScrollChild<ButtonBase>(item);

		if (button != NULL && button->IsLongPressTextAutoScrollEnabled()) {
			Event e;

			button->BeginLongPressTextAutoScroll(&e);
			button->EndLongPressTextAutoScroll(&e);
		}

		Text *text = FindAutoScrollChild<Text>(item);

		if (text != NULL) {
			bool on = false;

			ScePafWidget_222D55C7(text, &on);
			if (on) {
				Event e;

				text->HandleLongPress(&e);
				text->m_touch_held = false;
			}
		}
	} else {
		ButtonBase *button = (ButtonBase *)m_target;

		if (button != NULL && button->IsInherit(ButtonBase::TypeName())) {
			button = (ButtonBase *)m_target;
			if (button != NULL && button->IsLongPressTextAutoScrollEnabled()) {
				Event e;

				button->BeginLongPressTextAutoScroll(&e);
				button->EndLongPressTextAutoScroll(&e);
			}
		}
	}

	ListItem *previous_item = ListItem::GetListItem(m_previous);
	ButtonBase *previous = (ButtonBase *)m_previous;

	if (previous_item == NULL || previous_item == item) {
		if (m_target != (Widget *)previous && previous != NULL &&
		    previous->IsInherit(ButtonBase::TypeName())) {
			ButtonBase *button = (ButtonBase *)m_previous;

			if (button != NULL && button->IsLongPressTextAutoScrollEnabled())
				button->RequestTextAutoScrollStop();
		}
	} else {
		ButtonBase *button = FindAutoScrollChild<ButtonBase>(previous_item);

		if (button != NULL && button->IsLongPressTextAutoScrollEnabled())
			button->RequestTextAutoScrollStop();

		Text *text = FindAutoScrollChild<Text>(previous_item);

		if (text != NULL) {
			bool on = false;

			ScePafWidget_222D55C7(text, &on);
			if (on)
				text->StopAutoScroll();
		}
	}
}

#if !defined(FW_TYPE_CEX)
void FocusInfo::PrintTarget()
{
	Widget *target = m_target;

	if (target != NULL) {
		if (m_shown) {
			string name = target->m_name.m_id;

			if (name == "")
				name = "NO_NAME";
			ScePafAutoTestTty_9E10E873(2, "%s", name.c_str());
		} else {
			ScePafAutoTestTty_9E10E873(2, "");
		}
	}
}
#endif

void FocusInfo::Show()
{
	if (m_enabled && !m_shown) {
		m_was_shown = m_shown;
		m_shown = true;
		if (m_cursor != NULL) {
			intrusive_ptr<graph::Surface> texture;

			m_cursor->GetTexture(texture);
			if (texture.get() == NULL) {
				{
					intrusive_ptr<graph::Surface> fallback;

					ScePafWidget_B6C64486(fallback, 0);
				}
				m_default_texture = true;
			}
		}
		UpdateHighlight();
#if !defined(FW_TYPE_CEX)
		PrintTarget();
#endif
	}
}

void FocusInfo::Hide()
{
	if (m_enabled && m_shown) {
		m_was_shown = m_shown;
		m_shown = false;

		Widget *cursor = m_cursor;

		if (cursor != NULL) {
			uint32_t mode = m_move_mode;
			Widget *target = m_target;
			float delay = 250.0f;

			if (mode == 1 || mode == 6 || mode == 7 || target == NULL)
				delay = 0.0f;
			if (target != NULL && !target->IsInputTarget())
				delay = 0.0f;
			if (delay > 0.0f) {
				Timer *timer = new Timer(delay);
				float total = timer != NULL ? timer->GetTotalTime() : 0.0f;

				cursor->m_transition_complete = false;
				cursor->m_transition = 0xff;
				cursor->DoTransitionReverse_core(total, true);
				ScePafWidget_4588699E(cursor, timer, 2, 0.0f, NULL);
			} else {
				cursor->m_transition_complete = false;
				cursor->m_transition = 0xff;
				cursor->DoTransitionReverse_core(0.0f, true);
				ScePafWidget_4588699E(cursor, NULL, 2, 0.0f, NULL);
			}
			cursor = m_cursor;
			cursor->SetMetaAlpha(1.0f, new Timer(delay), 0x10003, 2, 0.0f, NULL);
#if !defined(FW_TYPE_CEX)
			PrintTarget();
#endif
		}
	}
}

bool FocusInfo::IsMoving()
{
	if (m_cursor == NULL)
		return false;
	if (m_cursor->FindAnim(0x10000) != NULL || m_cursor->FindAnim(65540) != NULL)
		return true;
	return false;
}

bool FocusInfo::IsMoveImmediate()
{
	uint32_t mode = m_move_mode;

	if (mode == 1 || mode == 6 || mode == 7)
		return true;
	return false;
}

intrusive_ptr<graph::Surface> FocusInfo::s_textures[9];

/* the textures the common resource gives the cursor frames: the table's index and the ID (inline
   in Sony's B6C64486, a copy of the load for each) */
struct FocusTextureName {
	int32_t m_index;
	const char *m_name;
};

static const FocusTextureName s_focus_texture_names[] = {
	{ 0, "_common_texture_keyfocus_rounded_rectangle_frame" },
	{ 1, "_common_texture_keyfocus_rectangle_frame" },
	{ 2, "_common_texture_keyfocus_top_and_bottom_line_frame" },
	{ 3, "_common_texture_keyfocus_circle_frame" },
	{ 5, "_common_texture_keyfocus_oneside_rounded_rectangle_frame" },
	{ 6, "_common_texture_keyfocus_textlink_rectangle_frame" },
	{ 7, "_common_texture_keyfocus_textlink_noside_frame" },
	{ 8, "_common_texture_keyfocus_textlink_oneside_rectangle_frame" }
};

extern "C" void ScePafWidget_B6C64486(const intrusive_ptr<graph::Surface> &texture, int32_t no)
{
	if (texture.get() != NULL) {
		if (no < 9)
			FocusInfo::s_textures[no] = texture;
		return;
	}
	for (uint32_t i = 0; i < sizeof(s_focus_texture_names) / sizeof(s_focus_texture_names[0]); i++) {
		int32_t index = s_focus_texture_names[i].m_index;

		if (FocusInfo::s_textures[index].get() == NULL) {
			IDParam id(s_focus_texture_names[i].m_name);

			FocusInfo::s_textures[index] = ScePafCommon_F820A54B(id);
		}
	}
}

intrusive_ptr<graph::Surface> FocusInfo::GetMoveTexture(int32_t mode)
{
	intrusive_ptr<graph::Surface> texture = s_textures[0];

	switch (mode) {
	case 0:
	case 1:
		texture = s_textures[0];
		break;
	case 2:
		texture = s_textures[1];
		break;
	case 3:
		texture = s_textures[2];
		break;
	case 4:
	case 5:
		texture = s_textures[3];
		break;
	case 6:
		texture = s_textures[5];
		break;
	case 7:
		texture = s_textures[6];
		break;
	case 8:
		texture = s_textures[7];
		break;
	case 9:
		texture = s_textures[8];
		break;
	}
	return texture;
}

void FocusInfo::UpdateParts()
{
	if (m_target == NULL || m_target->m_focus_parts.size() == 0)
		return;

	vector<FocusShape>::iterator part;
	bool changed = false;

	for (part = m_target->m_focus_parts.begin(); part != m_target->m_focus_parts.end(); ++part) {
		if (part->m_changed) {
			changed = true;
			break;
		}
	}
	if (!changed)
		return;

	m_cursor->m_focus_parts.clear();
	ScePafWidget_13B246E0(m_cursor, m_target->m_focus_shape);
	for (part = m_target->m_focus_parts.begin(); part != m_target->m_focus_parts.end(); ++part) {
		intrusive_ptr<graph::Surface> texture = GetMoveTexture(part->m_shape);

		part->m_texture = texture;
		part->m_changed = false;
		m_cursor->m_focus_parts.push_back(*part);
	}
	m_cursor->UpdateSize();
	for (part = m_target->m_focus_parts.begin(); part != m_target->m_focus_parts.end(); ++part)
		part->m_changed = false;
}

void FocusInfo::UpdateTexture()
{
	intrusive_ptr<graph::Surface> texture;

	m_cursor->GetTexture(texture);
	if (texture.get() != m_texture)
		m_cursor->SetTexture(Texture());
	if (m_target != NULL) {
		FocusShape *shape = m_target->m_focus_shape;

		if (shape != NULL && (shape->m_shape == 4 || shape->m_shape == 5)) {
			graph::PlaneObj *obj = static_cast<graph::PlaneObj *>(m_cursor->GetDrawObj(0));

			if (obj != NULL)
				obj->m_plane_flags.m_nine_patch = true;
		} else {
			graph::PlaneObj *obj = static_cast<graph::PlaneObj *>(m_cursor->GetDrawObj(0));

			if (obj != NULL)
				obj->m_plane_flags.m_nine_patch = false;
		}
	}
}

/* the size of the widget's scene, or of the screen where it has none */
static inline math::v4 SceneSize(Widget *widget)
{
	math::v4 size;

	if (widget->m_scene != NULL) {
		size = *widget->m_scene->GetSize(0);
	} else {
		math::rect screen(Screen::s_root_screen->sp);

		size = math::v4(screen.width(), screen.height(), 0.0f, 0.0f);
	}
	return size;
}

void FocusInfo::Follow(bool resize, bool at_once)
{
	if (m_target == m_previous || m_left_saved)
		return;

	UpdateHighlight();
	if (!m_shown) {
		Widget *cursor = m_cursor;

		cursor->m_transition_complete = false;
		cursor->m_transition = 0xFF;
		cursor->DoTransitionReverse_core(0.0f, true);
		ScePafWidget_4588699E(cursor, NULL, 2, 0.0f, NULL);
		m_target_pos = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
		m_shape = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
	}

	Widget *target = m_target;

	if (target == NULL)
		return;

	{
		FocusShape *shape = target->m_focus_shape;

		Texture() = GetMoveTexture(shape != NULL ? shape->m_shape : 0);
	}

	FocusShape *shape = m_target->m_focus_shape;

	if (shape != NULL && shape->m_texture.get() != NULL)
		Texture() = shape->m_texture;
	UpdateTexture();
	if (m_target->m_focus_parts.size() != 0) {
		for (vector<FocusShape>::iterator part = m_target->m_focus_parts.begin();
		     part != m_target->m_focus_parts.end(); ++part)
			part->m_changed = true;
		UpdateParts();
	} else {
		m_cursor->m_focus_parts.clear();
		m_cursor->UpdateSize();
	}

	/* the cursor aligned to the target's origin, where the target is aligned by it */
	uint32_t origin_x, origin_y, origin_z;
	bool realign = false;

	target->GetOrigin(origin_x, origin_y, origin_z);
	if ((origin_x | origin_y | origin_z) == 0) {
		Widget *cursor = m_cursor;

		if ((cursor->m_align_x | cursor->m_align_y | cursor->m_align_z) != 0)
			cursor->SetAlign(0, 0, 0, NULL);
	} else {
		uint32_t anchor_x, anchor_y, anchor_z;

		target->GetAnchor(anchor_x, anchor_y, anchor_z);
		if (origin_x == 2 && anchor_x != 2) {
			anchor_x = 2;
			realign = true;
		} else if (origin_x == 1 && anchor_x != 1) {
			anchor_x = 1;
			realign = true;
		}
		if (origin_y == 2 && anchor_y != 2) {
			anchor_y = 2;
			realign = true;
		} else if (origin_y == 1 && anchor_y != 1) {
			anchor_y = 1;
			realign = true;
		}
		if (origin_z == 2 && anchor_z != 2) {
			anchor_z = 2;
			realign = true;
		} else if (origin_z == 1 && anchor_z != 1) {
			anchor_z = 1;
			realign = true;
		}
		if (realign)
			m_cursor->SetAlign(anchor_x, anchor_y, anchor_z, NULL);
	}

	math::v4 size = SceneSize(m_target);
	sce::Vectormath::Simd::Aos::Vector2 extent = sce::Vectormath::Simd::Aos::maxPerElem(
		sce::Vectormath::Simd::Aos::Vector2(1.0f, 1.0f), size.getImpl().getXYZ().getXY());
	math::v4 offset(0.0f, 0.0f, 0.0f, 0.0f);

	shape = m_target->m_focus_shape;
	if (shape != NULL)
		offset = math::v4(shape->m_offset, 0.0f);

	if (at_once || m_target_pos.extract_x() < -1000.0f) {
		m_cursor->SetPos(offset + m_touch_move, NULL, Widget::ANIM_MOVE, 0, 0.0f, NULL, NULL);
		m_cursor->SetRot(math::v4(0.0f, 0.0f, 0.0f, 0.0f), 0, NULL, Widget::ANIM_ROT, 0, 0.0f,
				 NULL, NULL);
	} else {
		bool animate = resize || realign;
		Timer *timer;

		/* the cursor put back where it was in the target's frame, and moved from there */
		if (m_previous != NULL) {
			math::matrix world = m_target->GetWorldMatrix();

			world.set_w(m_target->GetFocusShapeWorldPos());

			math::matrix inv = world.inverse();
			math::v4 from = math::v4(inv.getImpl() *
						 m_previous->GetFocusShapeWorldPos().getImpl()) +
					m_touch_move;
			sce::Vectormath::Simd::Aos::Vector2 rate = sce::Vectormath::Simd::Aos::divPerElem(
				from.getImpl().getXYZ().getXY(), extent);
			float far_x = __builtin_fabsf(rate.getX());
			float far_y = __builtin_fabsf(rate.getY());
			float far = far_x > far_y ? far_x : far_y;
			float time = far * 100.0f;
			float scale = animate ? 0.15f : 1.0f;

			m_cursor->SetPos(offset + from * scale, NULL, Widget::ANIM_MOVE, 0, 0.0f, NULL,
					 NULL);
			timer = new Timer(time + 150.0f, Timer::FUNC_EXPONENT);
		} else {
			math::matrix world = m_target->GetWorldMatrix();

			world.set_w(m_target->GetFocusShapeWorldPos());

			math::matrix inv = world.inverse();
			math::v4 from = math::v4(inv.getImpl() * m_target_pos.getImpl());
			sce::Vectormath::Simd::Aos::Vector2 rate = sce::Vectormath::Simd::Aos::divPerElem(
				from.getImpl().getXYZ().getXY(), extent);
			float far_x = __builtin_fabsf(rate.getX());
			float far_y = __builtin_fabsf(rate.getY());
			float far = far_x > far_y ? far_x : far_y;
			float time = far * 100.0f;
			float scale = animate ? 0.15f : 1.0f;

			m_cursor->SetPos(from * scale, NULL, Widget::ANIM_MOVE, 0, 0.0f, NULL, NULL);
			timer = new Timer(time + 150.0f, Timer::FUNC_EXPONENT);
		}
		m_cursor->SetPos(offset, timer, Widget::ANIM_MOVE, 0, 0.0f, NULL, NULL);
	}

	math::v4 rot = m_rot;

	shape = m_target->m_focus_shape;
	if (shape != NULL)
		rot = math::v4(shape->m_rot, 0.0f);
	m_cursor->SetRot(rot, 0, NULL, Widget::ANIM_ROT, 0, 0.0f, NULL, NULL);
	if (m_moving_only) {
		m_cursor->KillAnimType(Widget::ANIM_META_ALPHA);
		if (!IsMoveImmediate())
			m_cursor->SetMetaAlpha(0.3f, NULL, Widget::ANIM_META_ALPHA, 0, 0.0f, NULL, NULL);
		m_move_mode_old = 0;
	}
	m_previous = m_target;
	m_target_pos = offset;
}

void FocusInfo::MoveCursor(bool animate)
{
	bool parts = false;
	math::v4 offset = m_target_pos;
	Widget *cursor = m_cursor;

	if (m_target != NULL) {
		FocusShape *shape = m_target->m_focus_shape;

		if (shape != NULL) {
			offset = math::v4(shape->m_offset, 0.0f);
			parts = shape->m_use_parts;
		}
	}
	if (animate && parts) {
		math::v4 pos = cursor->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		math::v4 to = offset + m_touch_move;

		/* moved over where it is more than five away */
		if (!sce::Vectormath::Simd::Aos::allElemLessThanOrEqual(
			     sce::Vectormath::Simd::Aos::absPerElem((pos - to).getImpl()),
			     sce::Vectormath::Simd::Aos::Vector4(5.0f))
			     .getAsBool()) {
			m_cursor->SetPos(to, new Timer(150.0f, Timer::FUNC_EXPONENT), Widget::ANIM_MOVE,
					 0, 0.0f, NULL, NULL);
			return;
		}
		cursor = m_cursor;
	}
	cursor->SetPos(offset + m_touch_move, NULL, Widget::ANIM_MOVE, 0, 0.0f, NULL, NULL);
}

void FocusInfo::Update(float time)
{
	if (m_previous != NULL && m_previous->m_hold_count > 0)
		m_previous = NULL;
	if (m_cursor == NULL)
		return;
	if (m_cursor->m_hold_count > 0) {
		m_cursor = NULL;
		return;
	}
	if (!m_enabled)
		return;

	/* the texture of the target's shape */
	if (m_target != NULL) {
		FocusShape *shape = m_target->m_focus_shape;
		intrusive_ptr<graph::Surface> texture = GetMoveTexture(shape != NULL ? shape->m_shape :
										       0);

		shape = m_target->m_focus_shape;
		if (shape != NULL && shape->m_texture.get() != NULL)
			texture = shape->m_texture;
		if (m_texture != texture.get() || m_default_texture) {
			Texture() = texture;
			UpdateTexture();
			m_default_texture = false;
		}
	}
	m_move_mode = 0;
	if (m_target != NULL)
		m_move_mode = m_target->GetFocusDirectionMask();
	if (!m_shown) {
		m_was_shown = false;
		m_left_saved = false;
		Follow(false, true);
	}
	if (!m_shown)
		return;

	Widget *cursor = m_cursor;

	if (!(cursor->m_show_a_sum >= 0.02f) && !cursor->IsAnimRunning(Widget::ANIM_SHOW_ALPHA) &&
	    m_shown == m_was_shown) {
		cursor = m_cursor;
		if (!cursor->m_transition_complete) {
			cursor->m_transition = 0xFF;
			cursor->m_transition_complete = true;
			cursor->DoTransition_core(0.0f, true);
		}
		ScePafWidget_FF5BC957(cursor, NULL, 2, 0.0f, NULL);
	}
	if (m_target != NULL && m_texture == NULL) {
		Follow(false, true);
		return;
	}

	/* drawn at rest only, where the cursor is drawn with its widget */
	if (m_moving_only && !IsMoving() && (m_target == NULL || !m_target->m_cursor_hidden))
		m_cursor->m_color_alpha = 1.0f;
	else
		m_cursor->m_color_alpha = 0.0f;

	/* the shown state changed: taken after five frames */
	bool resized = false;

	if (m_target != NULL && m_shown != m_was_shown) {
		if (m_show_wait < 5) {
			m_show_wait++;
		} else {
			m_show_wait = 0;
			m_was_shown = m_shown;
			if (!m_shown)
				return;
			cursor = m_cursor;
			if (m_target == m_previous) {
				/* back on the same target: shown again, grown from a third of its size */
				cursor->m_transition_complete = false;
				cursor->m_transition = 0xFF;
				cursor->DoTransitionReverse_core(0.0f, true);
				ScePafWidget_4588699E(cursor, NULL, 2, 0.0f, NULL);

				Timer *timer = new Timer(50.0f);

				if (!cursor->m_transition_complete) {
					cursor->m_transition = 0xFF;
					cursor->m_transition_complete = true;
					cursor->DoTransition_core(0.0f, true);
				}
				ScePafWidget_FF5BC957(cursor, timer, 2, 0.0f, NULL);

				math::v4 size = SceneSize(m_target);
				sce::Vectormath::Simd::Aos::Vector2 extent =
					sce::Vectormath::Simd::Aos::maxPerElem(
						sce::Vectormath::Simd::Aos::Vector2(1.0f, 1.0f),
						size.getImpl().getXYZ().getXY());
				math::v4 shape_size = m_target->GetFocusShapeSize();
				sce::Vectormath::Simd::Aos::Vector2 rate =
					sce::Vectormath::Simd::Aos::divPerElem(
						shape_size.getImpl().getXYZ().getXY(), extent);
				float far = rate.getY();

				if (rate.getX() > far)
					far = rate.getX();

				float param = (1.0f - far) * 0.25f + 0.05f;
				float total = far * 100.0f + 200.0f;

				cursor->SetSize(shape_size * 0.3f, NULL, Widget::ANIM_SIZE, 0, 0.0f, NULL,
						NULL);
				cursor->SetSize(shape_size,
						new Timer(total, Timer::FUNC_CUSTOM_BACK_OUT,
							  Timer::REPEAT_NONE, 0, &param),
						Widget::ANIM_SIZE, 0, 0.0f, NULL, NULL);
				resized = true;
				m_shape = shape_size;
			} else {
				cursor->m_transition_complete = false;
				cursor->m_transition = 0xFF;
				cursor->DoTransitionReverse_core(0.0f, true);
				ScePafWidget_4588699E(cursor, NULL, 2, 0.0f, NULL);
				if (!cursor->m_transition_complete) {
					cursor->m_transition = 0xFF;
					cursor->m_transition_complete = true;
					cursor->DoTransition_core(0.0f, true);
				}
				ScePafWidget_FF5BC957(cursor, NULL, 2, 0.0f, NULL);
			}
			if (IsMoveImmediate())
				cursor->SetMetaAlpha(0.0f, NULL, Widget::ANIM_META_ALPHA, 0, 0.0f, NULL,
						     NULL);
			else
				cursor->SetMetaAlpha(1.0f, NULL, Widget::ANIM_META_ALPHA, 0, 0.0f, NULL,
						     NULL);
			if (m_target->m_focus_parts.size() != 0)
				MoveCursor(false);

			FocusShape *shape = m_target->m_focus_shape;

			if (shape != NULL && shape->m_use_parts)
				shape->m_use_parts = false;
			UpdateParts();
			graph::FocusShader::SetColor(m_color);
		}
	} else {
		m_show_wait = 0;
	}

	/* where the cursor is at rest, put at the target's focus offset and turned */
	if (m_target != NULL && m_cursor->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f)) ==
				    m_cursor->GetPos(1, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f))) {
		MoveCursor(true);

		math::v4 rot = m_rot;
		FocusShape *shape = m_target->m_focus_shape;

		if (shape != NULL)
			rot = math::v4(shape->m_rot, 0.0f);
		m_cursor->SetRot(rot, 0, NULL, Widget::ANIM_ROT, 0, 0.0f, NULL, NULL);
		UpdateParts();
	} else if (m_target != NULL && m_cursor->m_focus_parts.size() == 0 &&
		   m_target->m_focus_parts.size() != 0) {
		MoveCursor(true);
	}

	/* the cursor's frame of the move mode: blinking for modes 2 and 4, hidden for the
	   immediate ones, else faint and grey blue */
	graph::DrawObj *obj = m_cursor->GetDrawObj(0);

	if (resized || m_move_mode != m_move_mode_old) {
		if (m_move_mode == 2 || m_move_mode == 4) {
			FocusShape *shape = m_target->m_focus_shape;

			if (shape != NULL && shape->m_shape == 1)
				obj->m_draw_style.blend = 1;
			else
				obj->m_draw_style.blend = 2;
			m_cursor->KillAnim(Widget::ANIM_META_ALPHA);
			m_cursor->SetMetaAlpha(1.0f, new Timer(600.0f, Timer::FUNC_QUAD_OUT),
					       Widget::ANIM_OPTION_LOOP | Widget::ANIM_META_ALPHA, 0, 0.0f,
					       NULL, NULL);
			m_cursor->SetMetaAlpha(0.1f, new Timer(600.0f, Timer::FUNC_QUAD_IN),
					       Widget::ANIM_OPTION_LOOP | Widget::ANIM_META_ALPHA, 0, 0.0f,
					       NULL, NULL);
			graph::FocusShader::SetColor(m_color);
		} else if (m_move_mode == 1 || m_move_mode == 6 || m_move_mode == 7) {
			m_cursor->KillAnimType(Widget::ANIM_META_ALPHA);
			m_cursor->SetMetaAlpha(m_cursor->m_meta_alpha * 0.6f, NULL,
					       Widget::ANIM_META_ALPHA, 0, 0.0f, NULL, NULL);
			m_cursor->SetMetaAlpha(0.0f, NULL, Widget::ANIM_META_ALPHA, 0, 0.0f, NULL, NULL);
		} else {
			obj->m_draw_style.blend = 1;
			m_cursor->KillAnim(Widget::ANIM_META_ALPHA);
			m_cursor->SetMetaAlpha(0.3f, new Timer(600.0f, Timer::FUNC_QUAD_OUT),
					       Widget::ANIM_META_ALPHA, 0, 0.0f, NULL, NULL);
			graph::FocusShader::SetColor(math::v4(0.0f, 43.0f / 255.0f, 1.0f, 1.0f));
		}
		m_move_mode_old = m_move_mode;
	}

	/* moving: faint until it rests */
	if (m_target != NULL && !(m_cursor->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f)) ==
				  m_cursor->GetPos(1, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f)))) {
		m_cursor->KillAnimType(Widget::ANIM_META_ALPHA);
		if (!IsMoveImmediate())
			m_cursor->SetMetaAlpha(0.3f, NULL, Widget::ANIM_META_ALPHA, 0, 0.0f, NULL, NULL);
		m_move_mode_old = 0;
	}

	if (m_target == NULL) {
		/* half a second without a target: the last one forgotten */
		uint64_t elapsed = get_time_internal() - m_leave_time;

		if ((float)elapsed / 1000000.0f * 1000.0f > 500.0f) {
			m_shape = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
			m_previous = NULL;
			m_target_pos = math::v4(-10000.0f, -10000.0f, -10000.0f, -10000.0f);
			m_left_saved = false;
		}
		m_cursor->SetMetaAlpha(0.0f, NULL, Widget::ANIM_META_ALPHA, 0, 0.0f, NULL, NULL);
		return;
	}

	m_leave_time = get_time_internal();
	cursor = m_cursor;

	Widget *target = m_target;

	if (!cursor->m_input_target || !(cursor->m_show_a_sum >= 0.95f) ||
	    cursor->m_transition_in || cursor->m_transition_out || cursor->m_destroy_protected) {
		Follow(false, true);
		return;
	}
	m_left_saved = false;

	/* the cursor sized to the target's focus shape, flashed where it grows past half the
	   screen, eased from 85% where the size jumps */
	math::v4 size = *target->GetSize(1);
	FocusShape *shape = target->m_focus_shape;

	if (shape != NULL && !(shape->m_size == math::v2(0.0f, 0.0f)))
		size = math::v4(sce::Vectormath::Simd::Aos::Vector4(shape->m_size.getImpl(), 0.0f, 0.0f));

	bool jump = false;
	bool immediate = false;

	if (!resized && !(size == m_shape)) {
		immediate = IsMoveImmediate();
		jump = immediate;
		if (!immediate) {
			math::v4 screen = SceneSize(target);
			float half = screen.extract_x() * screen.extract_y() * 0.5f;
			float area = size.extract_x() * size.extract_y();
			float old_area = m_shape.extract_x() * m_shape.extract_y();

			if (half < area && old_area < area) {
				m_cursor->KillAnimType(Widget::ANIM_META_ALPHA);
				m_cursor->SetMetaAlpha(m_cursor->m_meta_alpha * 0.6f, NULL,
						       Widget::ANIM_META_ALPHA, 0, 0.0f, NULL, NULL);
				m_cursor->SetMetaAlpha(0.0f, new Timer(200.0f, Timer::FUNC_QUAD_OUT),
						       Widget::ANIM_META_ALPHA, 0, 0.0f, NULL, NULL);
				jump = true;
			} else if (half >= area && old_area >= area) {
				m_move_mode_old = 0;
			}
		}
		if (__builtin_fabsf(m_shape.extract_x() - size.extract_x()) > size.extract_x() * 0.5f ||
		    __builtin_fabsf(m_shape.extract_y() - size.extract_y()) > size.extract_y() * 0.5f)
			jump = true;

		math::v4 near = size * 0.85f;

		if (m_shape == math::v4(0.0f, 0.0f, 0.0f, 0.0f)) {
			m_shape = near;
			jump = true;
		} else if (jump) {
			math::v4 mixed = near + m_shape * 0.15f;

			mixed.set_z(m_shape.extract_z());
			m_shape = mixed;
		}
		m_cursor->SetSize(m_shape, NULL, Widget::ANIM_SIZE, 0, 0.0f, NULL, NULL);
		if (IsMoveImmediate())
			m_cursor->SetSize(size, NULL, Widget::ANIM_SIZE, 0, 0.0f, NULL, NULL);
		else
			m_cursor->SetSize(size, new Timer(200.0f, Timer::FUNC_EXPONENT), Widget::ANIM_SIZE,
					  2, 0.0f, NULL, NULL);
		m_shape = size;
	}
	Follow(resized || jump, m_moving_only ? immediate : true);
}

/* the nearest widget up the tree, this one included, that takes its children's events first,
   short of the scene (name ours) */
static inline Widget *FindEventGroup(Widget *widget)
{
	for (Widget *node = widget; node != NULL; node = node->m_parent) {
		if (node->m_pre_dispatch)
			return node;
		if (static_cast<Widget *>(widget->m_scene) == node)
			return NULL;
	}
	return NULL;
}

Widget *FocusInfo::s_cursor_parent;

int32_t FocusInfo::ClearTarget(Widget *next)
{
	Widget *target = m_target;

	if (next == target)
		return -1;
	if (target == NULL)
		return -1;

	Widget *group = FindEventGroup(target);

	if (group != NULL) {
		if (next == NULL || group != FindEventGroup(next))
			group->NotifyCursorGroup(false, target, next);
	}
	if (next == NULL)
		m_leave_time = get_time_internal();
	if (m_shown && m_texture != NULL && !m_left_saved) {
		m_target_pos = m_target->GetWorldMatrix().w();
		m_shape = m_target->GetFocusShapeSize();
		m_previous = m_target;
		m_left_saved = true;
		if (m_cursor != NULL) {
			if (!(m_shape == *m_cursor->GetSize(0)))
				m_cursor->SetSize(m_shape);
		}
	}
	m_target->SetCursorOn(false, m_target, next);
	m_target = NULL;
	m_move_mode = 0;
	return 0;
}

int32_t FocusInfo::SetTarget(Widget *next)
{
	Widget *old = m_target;

	if (old != next) {
		Widget *old_group = NULL;
		Widget *cursor;

		if (old != NULL) {
			old_group = FindEventGroup(old);
			ClearTarget(next);
		}
		m_target = next;
		if (next != NULL) {
			if (next == m_previous && m_left_saved) {
				math::v4 pos(0.0f, 0.0f, 0.0f, 0.0f);

				if (next->m_focus_shape != NULL)
					pos = math::v4(next->m_focus_shape->m_offset, 0.0f);
				m_target_pos = pos;
			}
			next->SetCursorOn(true, old, next);

			Widget *group = FindEventGroup(next);

			if (group != NULL && group != old_group)
				group->NotifyCursorGroup(true, old, next);
			cursor = m_cursor;
			if (cursor != NULL) {
				cursor->SetParent(next, NULL, 0);
				cursor = m_cursor;
			}
		} else {
			cursor = m_cursor;
			if (cursor != NULL) {
				cursor->SetParent(s_cursor_parent, NULL, 0);
				cursor = m_cursor;
			}
		}
		bool shown = m_shown;

		if (cursor != NULL && !shown) {
			cursor->m_transition_complete = false;
			cursor->m_transition = 0xff;
			cursor->DoTransitionReverse_core(0.0f, true);
			ScePafWidget_4588699E(cursor, NULL, 2, 0.0f, NULL);
			shown = m_shown;
		}
#if !defined(FW_TYPE_CEX)
		if (shown)
			PrintTarget();
#endif
	}
	return 0;
}

void FocusInfo::AddFocusHistory(Widget *widget, Widget *next, uint32_t dir)
{
	if (widget != next) {
		uint32_t count = m_history_count;

		if (count == 1)
			count = m_history_count - 1;
		m_history_count = count + 1;
		m_history[count].m_widget = widget;
		m_history[count].m_next = next;
		m_history[count].m_dir = dir;
	}
}

void FocusInfo::RemoveFocusHistory(Widget *widget)
{
	uint32_t count = m_history_count;

	if (count != 0) {
		if (widget != NULL) {
			History *end = &m_history[0] + count;
			History *begin = &m_history[0];
			History *found = end;

			if (end != begin) {
				while (found[-1].m_widget != widget && found[-1].m_next != widget) {
					found--;
					if (found == begin)
						return;
				}

				History *from = found;
				History *to = begin;

				if (found != end) {
					do {
						*to = *from;
						from++;
						to++;
					} while (from != end);
					count = m_history_count;
				}
				m_history_count = count - (found - begin);
			}
		} else {
			m_history_count = 0;
		}
	}
}

Widget *FocusInfo::FindFocusHistory(Widget *widget, uint32_t dir)
{
	/* the direction asked for and the one it was made in */
	const uint32_t opposite[4][2] = { { 1, 2 }, { 2, 1 }, { 3, 4 }, { 4, 3 } };
	Widget *result = NULL;

	if (m_history_count != 0) {
		int32_t i = 0;

		while (opposite[i][0] != dir) {
			i++;
			if (i >= 4)
				return result;
		}

		uint32_t want = opposite[i][1];

		if (want != 0) {
			History *begin = &m_history[0];
			History *entry = begin + m_history_count;

			if (entry != begin) {
				while (entry[-1].m_next != widget || want != entry[-1].m_dir) {
					entry--;
					if (entry == begin)
						return result;
				}
				result = entry[-1].m_widget;
				m_history_count = m_history_count - (begin + m_history_count - entry);
			}
		}
	}
	return result;
}

/* the cursor kept when its widget is destroyed: put under the cursor parent, its target
   left, unless it is the cursor parent itself */
static Widget *KeepCursor(Widget *widget, void *arg)
{
	FocusInfo *info = (FocusInfo *)arg;

	if (widget == FocusInfo::s_cursor_parent)
		return NULL;
	if (info->m_cursor != NULL && info->m_cursor->CheckState() == 0)
		info->m_cursor->ChangeState(1, -1);
	info->ClearTarget(NULL);
	return FocusInfo::s_cursor_parent;
}

bool FocusPlane::IsInherit(const char *type) const
{
	if (type == "focusplane")
		return true;
	return Plane::IsInherit(type);
}

char *FocusPlane::GetTypeName() const
{
	return (char *)"focusplane";
}

int32_t FocusPlane::OnDraw(graph::DrawInfo &info)
{
	if (!m_culled || m_draw_always) {
		float alpha = m_color_alpha * m_meta_a_sum * m_show_a_sum;

		for (vector<DrawObjEntry>::iterator it = m_drawobjs.begin(); it != m_drawobjs.end();
		     ++it) {
			if (m_focus_parts.size() == 0 || it->m_attr.m_id == 1)
				it->m_obj->OnDraw(m_world_matrix, m_color, alpha, info);
		}
	}
	return 0;
}

int32_t FocusPlane::AddParts(int32_t count)
{
	int32_t ret = SCE_PAF_ERROR_ALREADY_CREATED;

	for (int32_t i = 0; i < count; i++) {
		graph::PlaneObj *obj = new graph::PlaneObj(graph::PlaneObjStyle());

		if (obj == NULL) {
			ret = SCE_PAF_ERROR_NO_MEMORY;
			continue;
		}
		obj->SetShader(graph::FocusShader::GetShader());
		obj->m_draw_style.blend = 2;
		AddDrawObj(DrawObjEntry(obj, 1), false);
		ret = 0;
	}
	return ret;
}

void FocusPlane::UpdateSize()
{
	Plane::UpdateSize();

	/* the part planes hidden, then the ones of the target's parts shown */
	for (vector<DrawObjEntry>::iterator it = m_drawobjs.begin(); it != m_drawobjs.end(); ++it) {
		if (it->m_attr.m_id == 1)
			it->m_obj->SetVisible(false);
	}
	if (m_drawobjs.size() < m_focus_parts.size() + 1) {
		if (AddParts(m_focus_parts.size() + 1 - m_drawobjs.size()) == SCE_PAF_ERROR_NO_MEMORY)
			return;
	}
	if (m_drawobjs.size() < m_focus_parts.size() + 1)
		return;

	/* the parts are scaled from the focus shape's size to the cursor's */
	FocusShape *shape = m_focus_shape;
	sce::Vectormath::Simd::Aos::Vector2 scale(1.0f, 1.0f);

	if (shape != NULL && !(shape->m_size == math::v2(0.0f, 0.0f)))
		scale = sce::Vectormath::Simd::Aos::divPerElem(GetSize(0)->getImpl().getXYZ().getXY(),
							       shape->m_size.getImpl());

	uint32_t index = 1;

	for (vector<FocusShape>::iterator part = m_focus_parts.begin(); part != m_focus_parts.end();
	     ++part, ++index) {
		if (!m_drawobjs[index].m_attr.m_flag1 || m_drawobjs[index].m_attr.m_id != 1)
			continue;

		intrusive_ptr<graph::Surface> texture(part->m_texture);
		intrusive_ptr<graph::Surface> current;

		m_drawobjs[index].m_obj->GetTexture(current, 0);
		if (texture.get() != current.get())
			m_drawobjs[index].m_obj->SetTexture(texture, 0);
		m_drawobjs[index].m_obj->SetPos(part->m_offset);
		m_drawobjs[index].m_obj->SetSize(math::v3(sce::Vectormath::Simd::Aos::Vector3(
			sce::Vectormath::Simd::Aos::mulPerElem(part->m_size.getImpl(), scale), 0.0f)));
		m_drawobjs[index].m_obj->SetRot(part->m_rot);
		m_drawobjs[index].m_obj->SetVisible(true);
	}
}

}
}
