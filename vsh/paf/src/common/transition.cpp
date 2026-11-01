/*
 * paf::common::transition: the animations that show a widget (Do, which
 * Widget::Show calls) and hide it (DoReverse, from Widget::Hide), and the time
 * each type takes by default.
 */
#include <stdint.h>
#include <math/ngp/v4.h>
#include <math/rect.h>
#include <widget/core/environment.h>
#include <widget/core/widget.h>
#include <widget/w_scene.h>
#include <widget/w_camera.h>
#include <graphics/drawobj/draw_obj.h>
#include <timer/timer.h>
#include <common/transition.h>

using namespace paf;
using namespace paf::common::transition;
using sce::Vectormath::Simd::floatInVec;

/*
 * The states the 3D slides move between: x (from the scene's centre), z, the
 * turn about the y axis, and the alpha.
 */
static const float s_slide_3d_state[5][4] = {
	{ 700.0f, 200.0f, -1.2566371f, 0.0f },
	{ 0.0f, 0.0f, 0.0f, 1.0f },
	{ -116.0f, -150.0f, 1.5707964f, 0.15f },
	{ -116.0f, -150.0f, 1.8534743f, 0.0f },
	{ -116.0f, -150.0f, 1.5707964f, 0.0f }
};

/* GetDefaultDelayReverse: a popup takes longer the larger it is in its scene */
float ScePafCommon_C85E4F3A(Type type, ui::Widget *widget)
{
	float delay = 0.0f;

	switch (type) {
	case Type_FadeinFast:
		delay = 350.0f;
		break;
	case Type_FadeinSlow:
		delay = 350.0f;
		break;
	case Type_Popup3:
	case Type_Popup4:
		if (widget == NULL) {
			delay = 300.0f;
		} else {
			math::v4 scene_size;

			delay = 350.0f;
			if (widget->m_scene != NULL) {
				scene_size = *widget->m_scene->GetSize(0);
			} else {
				math::rect rect(ui::Screen::s_root_screen->sp);

				scene_size = math::v4(rect.width(), rect.height(), 0.0f, 0.0f);
			}
			math::v4 size = *widget->GetSize(0);

			if (size.extract_y() < scene_size.extract_y() * 0.2f)
				delay = 220.0f;
			else if (size.extract_y() < scene_size.extract_y() * 0.4f)
				delay = 270.0f;
			else if (size.extract_y() < scene_size.extract_y() * 0.6f)
				delay = 320.0f;
		}
		break;
	case Type_SlideFromRight1:
		delay = 350.0f;
		break;
	case Type_SlideFromBottom1:
	case Type_SlideFromTop3:
	case Type_SlideFromBottom4:
	case Type_SlideFromTop4:
	case Type_SlideFromBottom5:
		delay = 400.0f;
		break;
	case Type_SlideFromLeft1:
	case Type_SlideFromRight2:
	case Type_SlideFromLeft2:
	case Type_SlideFromRight3:
		delay = 450.0f;
		break;
	case Type_SlideFromTop1:
	case Type_SlideFromTop2:
	case Type_SlideFromBottom2:
	case Type_SlideFromBottom3:
		delay = 250.0f;
		break;
	case Type_Popup5:
		delay = 350.0f;
		break;
	case Type_Popup1:
	case Type_Popup2:
	case 12:
		delay = 450.0f;
		break;
	case Type_3D_SlideFromFront:
	case Type_3D_SlideToBack1:
	case Type_18:
	case Type_3D_SlideToBack2:
		delay = 500.0f;
		break;
	}
	return delay;
}

/*
 * Where a slide in the direction starts (from) and ends (to): the widget's own
 * position when to is zero, moved to the scene's edge by the widget's anchor
 * and alignment and kept inside the scene's margins. Directions 0 to 3 slide
 * from just outside the widget's place, 4 to 7 from outside the scene: 0 and 4
 * from above, 1 and 5 from the left, 2 and 6 from below, 3 and 7 from the
 * right.
 */
static void CalcSlidePos(ui::Widget *widget, int32_t direction, math::v4 &from, math::v4 &to,
			 math::v4 &size, math::v4 &scene_size)
{
	uint32_t anchor_x, anchor_y, anchor_z;

	widget->GetAnchor(anchor_x, anchor_y, anchor_z);
	uint32_t align_x = widget->m_align_x;
	uint32_t align_y = widget->m_align_y;
	math::v4 margin = widget->m_scene->GetRolledMargin();
	bool at_pos = false;

	if (to == math::v4(0.0f, 0.0f, 0.0f, 0.0f)) {
		at_pos = true;
		to = widget->GetPos(ui::Widget::GET_CURRENT, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		to.set_z(0.0f);
	}
	size = *widget->GetSize(0);
	if (widget->m_scene != NULL) {
		scene_size = *widget->m_scene->GetSize(0);
	} else {
		math::rect rect(ui::Screen::s_root_screen->sp);

		scene_size = math::v4(rect.width(), rect.height(), 0.0f, 0.0f);
	}

	float offset, align;

	if (direction == 0) {
		if (anchor_y == ui::Widget::ANCHOR_TOP || anchor_x == 111)
			offset = 0.0f;
		else if (anchor_y == ui::Widget::ANCHOR_BOTTOM || anchor_x == 110)
			offset = scene_size.extract_y();
		else
			offset = scene_size.extract_y() * 0.5f;
		if (align_y == ui::Widget::ALIGN_TOP)
			align = 0.0f;
		else if (align_y == ui::Widget::ALIGN_BOTTOM)
			align = -size.extract_y();
		else
			align = size.extract_y() * -0.5f;
		if (at_pos)
			to.set_y(offset + align - margin.extract_x());
		from = to;
		from.set_y(to.extract_y() + size.extract_y() + 0.0f);
	} else if (direction == 4) {
		if (anchor_y == ui::Widget::ANCHOR_TOP || anchor_x == 111)
			offset = scene_size.extract_y() * -0.5f;
		else if (anchor_y == ui::Widget::ANCHOR_BOTTOM || anchor_x == 110)
			offset = scene_size.extract_y() * 0.5f;
		else
			offset = 0.0f;
		if (align_y == ui::Widget::ALIGN_TOP)
			align = size.extract_y() * 0.5f;
		else if (align_y == ui::Widget::ALIGN_BOTTOM)
			align = size.extract_y() * -0.5f;
		else
			align = 0.0f;
		if (at_pos)
			to.set_y(offset + align);
		from = to;
		from.set_y(to.extract_y() + scene_size.extract_y() + -margin.extract_x());
	} else if (direction == 2) {
		if (anchor_y == ui::Widget::ANCHOR_BOTTOM || anchor_x == 110)
			offset = 0.0f;
		else if (anchor_y == ui::Widget::ANCHOR_TOP || anchor_x == 111)
			offset = -scene_size.extract_y();
		else
			offset = scene_size.extract_y() * -0.5f;
		if (align_y == ui::Widget::ALIGN_BOTTOM) {
			align = 0.0f;
		} else {
			align = size.extract_y();
			if (align_y != ui::Widget::ALIGN_TOP)
				align = align * 0.5f;
		}
		if (at_pos)
			to.set_y(offset + align + margin.extract_z());
		from = to;
		from.set_y(to.extract_y() - size.extract_y() - 0.0f);
	} else if (direction == 6) {
		if (anchor_y == ui::Widget::ANCHOR_BOTTOM || anchor_x == 110)
			offset = scene_size.extract_y() * 0.5f;
		else if (anchor_y == ui::Widget::ANCHOR_TOP || anchor_x == 111)
			offset = scene_size.extract_y() * -0.5f;
		else
			offset = 0.0f;
		if (align_y == ui::Widget::ALIGN_BOTTOM)
			align = size.extract_y() * -0.5f;
		else if (align_y == ui::Widget::ALIGN_TOP)
			align = size.extract_y() * 0.5f;
		else
			align = 0.0f;
		if (at_pos)
			to.set_y(offset + align);
		from = to;
		from.set_y(to.extract_y() - scene_size.extract_y() - margin.extract_z());
	} else if (direction == 3) {
		if (anchor_x == ui::Widget::ANCHOR_RIGHT || anchor_x == 111)
			offset = 0.0f;
		else if (anchor_x == ui::Widget::ANCHOR_LEFT || anchor_x == 110)
			offset = scene_size.extract_x();
		else
			offset = scene_size.extract_x() * 0.5f;
		if (align_x == ui::Widget::ALIGN_RIGHT)
			align = 0.0f;
		else if (align_x == ui::Widget::ALIGN_LEFT)
			align = -size.extract_x();
		else
			align = size.extract_x() * -0.5f;
		if (at_pos)
			to.set_x(offset + align - margin.extract_w());
		from = to;
		from.set_x(to.extract_x() + size.extract_x() + 0.0f);
	} else if (direction == 7) {
		if (anchor_x == ui::Widget::ANCHOR_RIGHT || anchor_x == 111)
			offset = scene_size.extract_x() * -0.5f;
		else if (anchor_x == ui::Widget::ANCHOR_LEFT || anchor_x == 110)
			offset = scene_size.extract_x() * 0.5f;
		else
			offset = 0.0f;
		if (align_x == ui::Widget::ALIGN_RIGHT)
			align = size.extract_x() * 0.5f;
		else if (align_x == ui::Widget::ALIGN_LEFT)
			align = size.extract_x() * -0.5f;
		else
			align = 0.0f;
		if (at_pos)
			to.set_x(offset + align);
		from = to;
		from.set_x(to.extract_x() + scene_size.extract_x() + -margin.extract_w());
	} else if (direction == 1) {
		if (anchor_x == ui::Widget::ANCHOR_LEFT || anchor_x == 110)
			offset = 0.0f;
		else if (anchor_x == ui::Widget::ANCHOR_RIGHT || anchor_x == 111)
			offset = -scene_size.extract_x();
		else
			offset = scene_size.extract_x() * -0.5f;
		if (align_x == ui::Widget::ALIGN_LEFT) {
			align = 0.0f;
		} else {
			align = size.extract_x();
			if (align_x != ui::Widget::ALIGN_RIGHT)
				align = align * 0.5f;
		}
		if (at_pos)
			to.set_x(offset + align + margin.extract_y());
		from = to;
		from.set_x(to.extract_x() - size.extract_x() - 0.0f);
	} else if (direction == 5) {
		if (anchor_x == ui::Widget::ANCHOR_LEFT || anchor_x == 110)
			offset = scene_size.extract_x() * 0.5f;
		else if (anchor_x == ui::Widget::ANCHOR_RIGHT || anchor_x == 111)
			offset = scene_size.extract_x() * -0.5f;
		else
			offset = 0.0f;
		if (align_x == ui::Widget::ALIGN_LEFT)
			align = size.extract_x() * -0.5f;
		else if (align_x == ui::Widget::ALIGN_RIGHT)
			align = size.extract_x() * 0.5f;
		else
			align = 0.0f;
		if (at_pos)
			to.set_x(offset + align);
		from = to;
		from.set_x(to.extract_x() - scene_size.extract_x() - margin.extract_y());
	}
}

/*
 * Moves the widget in 3D from one state of s_slide_3d_state to another, about
 * the centre of the scene's resolution. Without animate the widget is put in
 * the end state at once when the default delay is ignored.
 */
static void Slide3D(ui::Widget *widget, int32_t from, int32_t to, bool animate,
		    bool ignore_default_delay, float delay)
{
	float half_width = 0.0f;
	ui::SceneResolution *resolution = ScePafWidget_CDC90BA2(
		widget->m_scene->m_env_param, widget->m_scene->m_resolution_id);

	if (resolution != NULL)
		half_width = resolution->m_width * 0.5f;

	const float *end = s_slide_3d_state[to];
	math::v4 center = widget->GetPosCenter(ui::Widget::GET_CURRENT, NULL,
					       math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	uint32_t align_y = widget->m_align_y;
	uint32_t align_z = widget->m_align_z;
	math::v4 offs = widget->GetAlignOffsetToCenter(0);

	offs.set_x(half_width);
	widget->SetAlignOffset(offs, NULL, ui::Widget::ANIM_ALIGN, 0, 0.0f, NULL, NULL);
	widget->SetAlign(widget->m_align_x, align_y, widget->m_align_z, NULL);
	widget->SetAlign(widget->m_align_x, widget->m_align_y, align_z, NULL);
	widget->SetPosCenter(center);

	const float *start = s_slide_3d_state[from];
	math::v4 pos = widget->GetPos(ui::Widget::GET_CURRENT, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

	widget->SetPos(math::v4(start[0] - half_width, pos.extract_y(), start[1], 0.0f));
	widget->SetQuat(math::quaternion::FromEulerAnglesYXZ(math::v3(0.0f, start[2], 0.0f)));
	widget->SetMetaAlpha(start[3]);

	if (ignore_default_delay) {
		if (!animate) {
			pos = widget->GetPos(ui::Widget::GET_CURRENT, NULL,
					     math::v4(0.0f, 0.0f, 0.0f, 0.0f));
			widget->SetPos(math::v4(end[0] - half_width, pos.extract_y(), end[1], 0.0f));
			widget->SetQuat(math::quaternion::FromEulerAnglesYXZ(
				math::v3(0.0f, end[2], 0.0f)));
			widget->SetMetaAlpha(end[3]);
			return;
		}
		delay = 0.0f;
	}

	pos = widget->GetPos(ui::Widget::GET_CURRENT, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	widget->SetPos(math::v4(end[0] - half_width, pos.extract_y(), end[1], 0.0f),
		       new Timer3D(delay, Timer::FUNC_EXPO_OUT_30, Timer::FUNC_LINEAR,
				   Timer::FUNC_EXPO_OUT_29, Timer::REPEAT_NONE, -1, NULL, NULL, NULL));
	widget->SetQuat(math::quaternion::FromEulerAnglesYXZ(math::v3(0.0f, end[2], 0.0f)),
			new Timer(delay, Timer::FUNC_EXPO_OUT_30));
	widget->SetMetaAlpha(end[3], new Timer(delay * 0.8f, Timer::FUNC_QUAD_OUT));
}

/*
 * Do: shows the widget by the effect, after the parameter's time.
 * The widget is put in place at once when the default delay is ignored and no
 * transition was started; otherwise its animations are started from where the
 * running ones are, or from the effect's start. Returns when it ends.
 */
float ScePafCommon_6CF3A3C8(float parameter, ui::Widget *widget, Type effect, bool auto_delete,
			    bool ignore_default_delay)
{
	if (widget == NULL)
		return 0.0f;

	bool started = false;

	if (auto_delete || parameter > 0.0f) {
		ScePafWidget_8AF26B1F(widget, auto_delete, parameter);
		started = true;
	}

	float delay = 0.0f;
	float finish = 0.0f;

	if (!ignore_default_delay) {
		delay = ScePafCommon_C85E4F3A(effect, widget);
		finish = delay * 0.7f;
	}

	switch (effect) {
	case Type_FadeinFast:
	case Type_FadeinSlow:
		if (ignore_default_delay && !started) {
			widget->SetMetaAlpha(1.0f);
			break;
		}
		if (!ignore_default_delay)
			finish = 0.0f;
		if (widget->FindAnim(ui::Widget::ANIM_META_ALPHA) != NULL)
			widget->KillAnimType(ui::Widget::ANIM_META_ALPHA);
		else
			widget->SetMetaAlpha(0.0f);
		widget->SetMetaAlpha(1.0f, new Timer(delay, Timer::FUNC_EXPO_OUT_30));
		break;
	case Type_Popup3:
	case Type_Popup4: {
		math::v4 scale(1.0f, 1.0f, 1.0f, 0.0f);

		if (ignore_default_delay && !started) {
			widget->SetScale(scale);
			widget->SetMetaAlpha(1.0f);
			break;
		}

		math::v4 scene_size;

		if (widget->m_scene != NULL) {
			scene_size = *widget->m_scene->GetSize(0);
		} else {
			math::rect rect(ui::Screen::s_root_screen->sp);

			scene_size = math::v4(rect.width(), rect.height(), 0.0f, 0.0f);
		}
		math::v4 size = *widget->GetSize(0);
		float param = 0.025f;

		if (size.extract_y() < scene_size.extract_y() * 0.4f)
			param = 0.035f;
		else if (size.extract_y() < scene_size.extract_y() * 0.6f)
			param = 0.03f;

		if (widget->FindAnim(ui::Widget::ANIM_SCALE) != NULL) {
			widget->KillAnimType(ui::Widget::ANIM_SCALE);
			widget->KillAnimType(ui::Widget::ANIM_META_ALPHA);
		} else {
			widget->SetMetaAlpha(0.0f);
			widget->SetScale(scale * 0.4f);
		}
		widget->SetMetaAlpha(1.0f, new Timer(delay * 0.2f, Timer::FUNC_LINEAR));
		widget->SetScale(scale, new Timer(delay * 0.6f, Timer::FUNC_EXPO_OUT_29),
				 0x0e000000 | ui::Widget::ANIM_SCALE);
		widget->SetScale(scale, new Timer(delay, Timer::FUNC_CUSTOM_BACK_OUT,
						  Timer::REPEAT_NONE, 0, &param),
				 0x0d000000 | ui::Widget::ANIM_SCALE);
		break;
	}
	case Type_SlideFromRight1: {
		math::v4 from, to(0.0f, 0.0f, 0.0f, 0.0f), size, scene_size;

		CalcSlidePos(widget, 3, from, to, size, scene_size);
		ui::Camera *camera = widget->GetCamera();

		ScePafWidget_72F479CA(camera);
		float distance = camera->m_distance;

		if (ignore_default_delay && !started) {
			widget->SetPos(to);
			widget->SetMetaAlpha(1.0f);
			break;
		}
		if (widget->FindAnim(ui::Widget::ANIM_MOVE) != NULL) {
			widget->KillAnimType(ui::Widget::ANIM_MOVE);
			widget->KillAnimType(ui::Widget::ANIM_META_ALPHA);
		} else {
			float x = fmaxf(floatInVec(size.extract_x()),
					floatInVec(scene_size.extract_x() * 0.45f)).getAsFloat();

			widget->SetPos(to + math::v4(x * 0.8f, 0.0f, distance * 0.25f, 0.0f));
			widget->SetMetaAlpha(0.0f);
		}
		widget->SetPos(to, new Timer(delay, Timer::FUNC_EXPO_OUT_29),
			       0x0e000000 | ui::Widget::ANIM_MOVE);
		widget->SetPos(to, new Timer(delay * 0.6f, Timer::FUNC_EXPO_OUT_29),
			       0x0b000000 | ui::Widget::ANIM_MOVE);
		widget->SetMetaAlpha(1.0f, new Timer(delay * 0.8f, Timer::FUNC_LINEAR));
		break;
	}
	case Type_SlideFromBottom1:
	case Type_SlideFromTop3:
	case Type_SlideFromBottom4:
	case Type_SlideFromTop4:
	case Type_SlideFromBottom5: {
		math::v4 from, to(0.0f, 0.0f, 0.0f, 0.0f), size, scene_size;

		if (effect == Type_SlideFromTop3 || effect == Type_SlideFromTop4)
			CalcSlidePos(widget, 4, from, to, size, scene_size);
		else
			CalcSlidePos(widget, 6, from, to, size, scene_size);
		if (effect == Type_SlideFromTop3 || effect == Type_SlideFromBottom4 ||
		    effect == Type_SlideFromBottom1) {
			math::v4 prev = to;
			ui::ResolutionData *res = widget->get_resolution_data(
				widget->m_pos_array, widget->m_pos_num, widget->m_resolution_id);

			if (res != NULL)
				to = math::v4(res->m_value[0], res->m_value[1], res->m_value[2], 0.0f);
			if (effect == Type_SlideFromTop3) {
				math::v4 target = widget->GetPos(ui::Widget::GET_ANIMTARGET, NULL,
								 math::v4(0.0f, 0.0f, 0.0f, 0.0f));

				if (__builtin_fabsf(target.extract_y() - to.extract_y()) < 64.0f)
					to = target;
			} else {
				math::v4 target = widget->GetPos(ui::Widget::GET_ANIMTARGET, NULL,
								 math::v4(0.0f, 0.0f, 0.0f, 0.0f));

				if (__builtin_fabsf(target.extract_y() - to.extract_y()) <
				    scene_size.extract_y() * 0.51f)
					to = target;
			}
			if (!(to == prev)) {
				if (effect == Type_SlideFromTop3)
					CalcSlidePos(widget, 4, from, to, size, scene_size);
				else
					CalcSlidePos(widget, 6, from, to, size, scene_size);
			}
		}
		if (ignore_default_delay && !started) {
			widget->SetPos(to);
			widget->SetMetaAlpha(1.0f);
			break;
		}
		if (widget->FindAnim(ui::Widget::ANIM_MOVE) != NULL) {
			widget->KillAnimType(ui::Widget::ANIM_MOVE);
			widget->KillAnimType(ui::Widget::ANIM_META_ALPHA);
		} else {
			widget->SetPos(from);
			widget->SetMetaAlpha(0.0f);
		}
		widget->SetMetaAlpha(1.0f, new Timer(delay * 0.8f, Timer::FUNC_QUAD_OUT));
		widget->SetPos(to, new Timer(delay, Timer::FUNC_EXPO_OUT_29));
		break;
	}
	case Type_SlideFromLeft1:
	case Type_SlideFromRight2:
	case Type_SlideFromLeft2:
	case Type_SlideFromRight3: {
		math::v4 from, to(0.0f, 0.0f, 0.0f, 0.0f), size, scene_size;

		if (effect == Type_SlideFromLeft1 || effect == Type_SlideFromLeft2)
			CalcSlidePos(widget, 5, from, to, size, scene_size);
		else
			CalcSlidePos(widget, 7, from, to, size, scene_size);
		if (effect == Type_SlideFromLeft1 || effect == Type_SlideFromRight2) {
			math::v4 prev = to;
			ui::ResolutionData *res = widget->get_resolution_data(
				widget->m_pos_array, widget->m_pos_num, widget->m_resolution_id);

			if (res != NULL)
				to = math::v4(res->m_value[0], res->m_value[1], res->m_value[2], 0.0f);
			math::v4 target = widget->GetPos(ui::Widget::GET_ANIMTARGET, NULL,
							 math::v4(0.0f, 0.0f, 0.0f, 0.0f));

			if (__builtin_fabsf(target.extract_x() - to.extract_x()) < 128.0f)
				to = target;
			if (!(to == prev)) {
				if (effect == Type_SlideFromLeft1)
					CalcSlidePos(widget, 5, from, to, size, scene_size);
				else
					CalcSlidePos(widget, 7, from, to, size, scene_size);
			}
		}
		if (ignore_default_delay && !started) {
			widget->SetPos(to);
			widget->SetMetaAlpha(1.0f);
			break;
		}
		if (widget->FindAnim(ui::Widget::ANIM_MOVE) != NULL) {
			widget->KillAnimType(ui::Widget::ANIM_MOVE);
			widget->KillAnimType(ui::Widget::ANIM_META_ALPHA);
		} else {
			widget->SetPos(from);
		}
		widget->SetMetaAlpha(1.0f, new Timer(delay * 0.3f, Timer::FUNC_QUAD_OUT));
		widget->SetPos(to, new Timer(delay, Timer::FUNC_EXPO_OUT_29));
		break;
	}
	case Type_SlideFromTop1:
	case Type_SlideFromTop2:
	case Type_SlideFromBottom2:
	case Type_SlideFromBottom3: {
		math::v4 from, to(0.0f, 0.0f, 0.0f, 0.0f), size, scene_size;

		if (effect == Type_SlideFromTop1 || effect == Type_SlideFromTop2)
			CalcSlidePos(widget, 0, from, to, size, scene_size);
		else
			CalcSlidePos(widget, 2, from, to, size, scene_size);
		if (ignore_default_delay && !started) {
			widget->SetMetaAlpha(1.0f);
			widget->SetPos(to);
			break;
		}
		if (widget->FindAnim(ui::Widget::ANIM_MOVE) != NULL) {
			widget->KillAnimType(ui::Widget::ANIM_MOVE);
			widget->KillAnimType(ui::Widget::ANIM_META_ALPHA);
		} else {
			widget->SetPos(from);
			widget->SetMetaAlpha(0.0f);
		}
		widget->SetPos(to, new Timer(delay, Timer::FUNC_EXPO_OUT_29));
		widget->SetMetaAlpha(1.0f, new Timer(delay * 0.8f, Timer::FUNC_QUAD_OUT));
		break;
	}
	case Type_Popup5:
		if (ignore_default_delay && !started) {
			widget->SetMetaAlpha(1.0f);
			widget->SetScale(math::v4(1.0f, 1.0f, 1.0f, 0.0f));
			break;
		}
		if (!ignore_default_delay)
			finish = delay * 0.6f;
		if (widget->FindAnim(ui::Widget::ANIM_SCALE) != NULL) {
			widget->KillAnimType(ui::Widget::ANIM_SCALE);
			widget->KillAnimType(ui::Widget::ANIM_META_ALPHA);
		} else {
			widget->SetMetaAlpha(0.0f);
			widget->SetScale(math::v4(0.3f, 0.3f, 0.3f, 0.0f));
		}
		widget->SetMetaAlpha(1.0f, new Timer(delay * 0.3f, Timer::FUNC_LINEAR));
		widget->SetScale(math::v4(1.0f, 1.0f, 1.0f, 0.0f),
				 new Timer(delay, Timer::FUNC_BACK_OUT));
		break;
	case Type_3D_SlideFromFront:
		Slide3D(widget, 0, 1, started, ignore_default_delay, delay);
		break;
	case Type_3D_SlideToBack1:
		Slide3D(widget, 1, 2, started, ignore_default_delay, delay);
		break;
	case Type_18:
		Slide3D(widget, 2, 3, started, ignore_default_delay, delay);
		break;
	case Type_3D_SlideToBack2:
		Slide3D(widget, 1, 4, started, ignore_default_delay, delay);
		break;
	default: {
		ui::Camera *camera = widget->GetCamera();

		ScePafWidget_72F479CA(camera);
		float distance = camera->m_distance;
		math::v4 to = widget->GetPos(ui::Widget::GET_CURRENT, NULL,
					     math::v4(0.0f, 0.0f, 0.0f, 0.0f));

		to.set_z(0.0f);
		if (ignore_default_delay && !started) {
			widget->SetMetaAlpha(1.0f);
			widget->SetPos(to);
			break;
		}

		bool killed = false;

		if (widget->FindAnim(ui::Widget::ANIM_MOVE) != NULL) {
			killed = true;
			widget->KillAnimType(ui::Widget::ANIM_MOVE);
			widget->KillAnimType(ui::Widget::ANIM_META_ALPHA);
		} else {
			math::v4 from = to;

			from.set_z(distance * -0.16f);
			widget->SetPos(from);
		}
		widget->SetPos(to, new Timer(delay, Timer::FUNC_BACK_OUT));

		graph::DrawObj *obj = widget->GetDrawObj(0);

		if (obj != NULL && obj->m_draw_style.blend != 0) {
			if (!killed)
				widget->SetMetaAlpha(0.0f);
			widget->SetMetaAlpha(1.0f, new Timer(delay * 0.5f, Timer::FUNC_LINEAR));
		}
		break;
	}
	}

	if (!ignore_default_delay || started)
		ScePafWidget_AC0C81D5(widget, parameter + finish);
	return parameter + delay;
}

/* GetDefaultDelay */
float ScePafCommon_DE5322EB(Type type)
{
	float delay = 0.0f;

	switch (type) {
	case Type_FadeinFast:
		delay = 150.0f;
		break;
	case Type_FadeinSlow:
		delay = 250.0f;
		break;
	case Type_Popup3:
	case Type_Popup4:
		delay = 150.0f;
		break;
	case Type_SlideFromRight1:
		delay = 250.0f;
		break;
	case Type_SlideFromBottom1:
	case Type_SlideFromTop3:
	case Type_SlideFromBottom4:
	case Type_SlideFromTop4:
	case Type_SlideFromBottom5:
		delay = 250.0f;
		break;
	case Type_SlideFromLeft1:
	case Type_SlideFromRight2:
	case Type_SlideFromLeft2:
	case Type_SlideFromRight3:
		delay = 450.0f;
		break;
	case Type_SlideFromTop1:
	case Type_SlideFromTop2:
	case Type_SlideFromBottom2:
	case Type_SlideFromBottom3:
		delay = 150.0f;
		break;
	case Type_Popup5:
		delay = 150.0f;
		break;
	case Type_Popup1:
	case Type_Popup2:
	case 12:
		delay = 250.0f;
		break;
	case Type_3D_SlideFromFront:
	case Type_3D_SlideToBack1:
	case Type_18:
	case Type_3D_SlideToBack2:
		delay = 500.0f;
		break;
	}
	return delay;
}

/*
 * DoReverse: hides the widget by the effect, after the parameter's time. The widget
 * is put in its end state at once when the default delay is ignored and the
 * parameter is no time; otherwise its animations start after the parameter.
 * With auto_delete the widget is deleted when they end. Returns when they end.
 */
float ScePafCommon_B48C9BDC(float parameter, ui::Widget *widget, Type effect, bool auto_delete,
			    bool ignore_default_delay)
{
	if (widget == NULL)
		return 0.0f;

	float delay = 0.0f;
	bool started = false;

	if (parameter > 0.0f)
		started = true;
	if (!ignore_default_delay)
		delay = ScePafCommon_DE5322EB(effect);

	float end = parameter + delay;

	switch (effect) {
	case Type_FadeinFast:
	case Type_FadeinSlow:
		if (ignore_default_delay && !started) {
			widget->SetMetaAlpha(0.0f);
			break;
		}
		widget->SetMetaAlpha(0.0f, new Timer(delay, Timer::FUNC_LINEAR),
				     ui::Widget::ANIM_META_ALPHA, 2, parameter);
		break;
	case Type_Popup3:
	case Type_Popup4:
		if (ignore_default_delay && !started) {
			widget->SetScale(math::v4(1.0f, 1.0f, 1.0f, 0.0f) * 0.3f);
			widget->SetMetaAlpha(0.0f);
			break;
		}
		if (widget->FindAnim(ui::Widget::ANIM_SCALE) != NULL) {
			widget->KillAnimType(ui::Widget::ANIM_SCALE);
			widget->KillAnimType(ui::Widget::ANIM_META_ALPHA);
		}
		widget->SetScale(math::v4(1.0f, 1.0f, 1.0f, 0.0f) * 0.3f,
				 new Timer2D(delay, Timer::FUNC_EXPO_IN_15, Timer::FUNC_QUAD_IN,
					     Timer::REPEAT_NONE, -1, NULL, NULL),
				 ui::Widget::ANIM_SCALE, 2, parameter);
		widget->SetMetaAlpha(0.0f, new Timer(delay * 0.7f, Timer::FUNC_QUAD_IN),
				     ui::Widget::ANIM_META_ALPHA, 2, parameter + delay * 0.3f);
		break;
	case Type_SlideFromRight1: {
		math::v4 from, to(0.0f, 0.0f, 0.0f, 0.0f), size, scene_size;

		CalcSlidePos(widget, 3, from, to, size, scene_size);
		ui::Camera *camera = widget->GetCamera();

		ScePafWidget_72F479CA(camera);
		math::v4 pos = from;

		pos.set_x(from.extract_x() - size.extract_x() * 0.35f);
		pos.set_z(camera->m_distance * 0.18f);
		if (ignore_default_delay && !started) {
			widget->SetMetaAlpha(0.0f);
			widget->SetPos(pos);
			break;
		}
		widget->SetPos(pos, new Timer(delay, Timer::FUNC_QUAD_IN),
			       0x0e000000 | ui::Widget::ANIM_MOVE, 2, parameter);
		widget->SetPos(pos, new Timer(delay * 0.65f, Timer::FUNC_QUAD_IN),
			       0x0b000000 | ui::Widget::ANIM_MOVE, 0, parameter + delay * 0.3f);
		widget->SetMetaAlpha(0.0f, new Timer(delay * 0.8f, Timer::FUNC_LINEAR),
				     ui::Widget::ANIM_META_ALPHA, 2, parameter + delay * 0.2f);
		break;
	}
	case Type_SlideFromBottom1:
	case Type_SlideFromTop3:
	case Type_SlideFromBottom4:
	case Type_SlideFromTop4:
	case Type_SlideFromBottom5: {
		math::v4 from, to(0.0f, 0.0f, 0.0f, 0.0f), size, scene_size;

		if (effect == Type_SlideFromTop3 || effect == Type_SlideFromTop4)
			CalcSlidePos(widget, 4, from, to, size, scene_size);
		else
			CalcSlidePos(widget, 6, from, to, size, scene_size);
		if (ignore_default_delay && !started) {
			widget->SetMetaAlpha(0.0f);
			widget->SetPos(from);
			break;
		}
		widget->SetPos(from, new Timer(delay, Timer::FUNC_QUAD_IN), ui::Widget::ANIM_MOVE, 2,
			       parameter);
		widget->SetMetaAlpha(0.0f, new Timer(delay * 0.6f, Timer::FUNC_QUAD_IN),
				     ui::Widget::ANIM_META_ALPHA, 2, parameter + delay * 0.4f);
		break;
	}
	case Type_SlideFromLeft1:
	case Type_SlideFromRight2:
	case Type_SlideFromLeft2:
	case Type_SlideFromRight3: {
		math::v4 from, to(0.0f, 0.0f, 0.0f, 0.0f), size, scene_size;

		if (effect == Type_SlideFromLeft1 || effect == Type_SlideFromLeft2)
			CalcSlidePos(widget, 5, from, to, size, scene_size);
		else
			CalcSlidePos(widget, 7, from, to, size, scene_size);
		if (ignore_default_delay && !started) {
			widget->SetMetaAlpha(0.0f);
			widget->SetPos(from);
			break;
		}
		widget->SetPos(from, new Timer(delay, Timer::FUNC_EXPO_OUT_29), ui::Widget::ANIM_MOVE,
			       2, parameter);
		widget->SetMetaAlpha(0.0f, new Timer(delay * 0.6f, Timer::FUNC_QUAD_IN),
				     ui::Widget::ANIM_META_ALPHA, 2, parameter + delay * 0.4f);
		break;
	}
	case Type_SlideFromTop1:
	case Type_SlideFromTop2:
	case Type_SlideFromBottom2:
	case Type_SlideFromBottom3: {
		math::v4 from, to(0.0f, 0.0f, 0.0f, 0.0f), size, scene_size;

		if (effect == Type_SlideFromTop1 || effect == Type_SlideFromTop2)
			CalcSlidePos(widget, 0, from, to, size, scene_size);
		else
			CalcSlidePos(widget, 2, from, to, size, scene_size);
		if (ignore_default_delay && !started) {
			widget->SetMetaAlpha(0.0f);
			widget->SetPos(from);
			break;
		}
		widget->SetPos(from, new Timer(delay, Timer::FUNC_QUAD_IN), ui::Widget::ANIM_MOVE, 2,
			       parameter);
		widget->SetMetaAlpha(0.0f, new Timer(0.0f, Timer::FUNC_LINEAR),
				     ui::Widget::ANIM_META_ALPHA, 2, end);
		break;
	}
	case Type_Popup5:
		if (ignore_default_delay && !started) {
			widget->SetMetaAlpha(0.0f);
			widget->SetScale(math::v4(0.6f, 0.6f, 0.6f, 0.0f));
			break;
		}
		widget->SetMetaAlpha(0.0f, new Timer(delay * 0.8f, Timer::FUNC_QUAD_IN),
				     ui::Widget::ANIM_META_ALPHA, 2, parameter + delay * 0.2f);
		widget->SetScale(math::v4(0.6f, 0.6f, 0.6f, 0.0f), new Timer(delay, Timer::FUNC_QUAD_IN),
				 ui::Widget::ANIM_SIZE, 2, parameter);
		break;
	case Type_3D_SlideFromFront:
		Slide3D(widget, 1, 0, started, ignore_default_delay, delay);
		break;
	case Type_3D_SlideToBack1:
		Slide3D(widget, 2, 1, started, ignore_default_delay, delay);
		break;
	case Type_18:
		Slide3D(widget, 3, 2, started, ignore_default_delay, delay);
		break;
	case Type_3D_SlideToBack2:
		Slide3D(widget, 4, 1, started, ignore_default_delay, delay);
		break;
	default: {
		ui::Camera *camera = widget->GetCamera();

		ScePafWidget_72F479CA(camera);
		float distance = camera->m_distance;
		math::v4 pos = widget->GetPos(ui::Widget::GET_CURRENT, NULL,
					      math::v4(0.0f, 0.0f, 0.0f, 0.0f));

		pos.set_z(distance * -0.18f);
		if (ignore_default_delay && !started) {
			widget->SetMetaAlpha(0.0f);
			widget->SetPos(pos);
			break;
		}
		widget->SetPos(pos, new Timer(delay, Timer::FUNC_QUAD_IN), ui::Widget::ANIM_MOVE, 2,
			       parameter);

		graph::DrawObj *obj = widget->GetDrawObj(0);

		if (obj != NULL && obj->m_draw_style.blend != 0)
			widget->SetMetaAlpha(0.0f, new Timer(delay * 0.7f, Timer::FUNC_QUAD_IN),
					     ui::Widget::ANIM_META_ALPHA, 2, parameter + delay * 0.3f);
		break;
	}
	}

	if (auto_delete) {
		ScePafWidget_5A9793B0(widget, end);
		ScePafWidget_A96A0B4A(widget, 1);
	} else if (!ignore_default_delay || started) {
		ScePafWidget_AC0C81D5(widget, end);
	}
	return end;
}
