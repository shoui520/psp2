/*
 * paf::ui::Focuser (TOOL 0x811460e8..0x81146af6): a widget's focus request and what it names
 * for the focus directions, the callback that moves the focus to the nearest widget and the
 * score it gives each candidate. No record or string names the file.
 */
#include <stddef.h>
#include <stdint.h>
#include <math.h>
#include <widget/core/focus.h>
#include <widget/core/widget.h>
#include <widget/w_scene.h>

namespace paf {
namespace ui {

/* the widget of the direction, the first for any other value (ScePafWidget_71349B04 inlined) */
static inline __attribute__((always_inline)) Widget *DirectionWidgetOf(FocusDirectionWidget *data,
								      int32_t dir)
{
	if (dir == 0)
		return data->m_widget[0];
	if (dir == 1)
		return data->m_widget[1];
	if (dir == 2)
		return data->m_widget[2];
	if (dir == 3)
		return data->m_widget[3];
	return data->m_widget[0];
}

/* the ID of the direction in the hint, the first for any other value */
static inline __attribute__((always_inline)) IDParam *DirectionIDOf(FocusHint *hint, int32_t dir)
{
	IDParam *id = &hint->m_direction_id[0];

	if (dir != 0) {
		if (dir == 1)
			id += 1;
		else if (dir == 2)
			id += 2;
		else if (dir == 3)
			id += 3;
	}
	return id;
}

Widget *Focuser::GetHintWidget(int32_t dir)
{
	if (m_widget == NULL)
		return NULL;

	Widget *root = ScePafWidget_CFA1EB7A(m_widget);

	if (m_hint != NULL) {
		Widget *found = NULL;

		if (root == m_widget && m_widget->m_parent != NULL)
			root = ScePafWidget_CFA1EB7A(m_widget->m_parent);
		if (root != NULL)
			found = root->FindChild(*DirectionIDOf(m_hint, dir), 0);
		if (found == NULL && m_widget->m_scene != NULL)
			found = m_widget->m_scene->FindChild(*DirectionIDOf(m_hint, dir), 0);
		if (found != NULL)
			return found;
	}
	if (m_data == NULL)
		return NULL;
	return DirectionWidgetOf(m_data, dir);
}

extern "C" Widget *ScePafWidget_71349B04(Focuser *focuser, int32_t dir)
{
	if (focuser->m_widget == NULL)
		return NULL;
	if (focuser->m_data == NULL)
		return NULL;
	return DirectionWidgetOf(focuser->m_data, dir);
}

void Focuser::GetHintID(int32_t dir, IDParam *id)
{
	if (m_widget != NULL && m_hint != NULL)
		*id = *DirectionIDOf(m_hint, dir);
}

int32_t Focuser::ScePafWidget_97020684(void *move)
{
	if (m_widget == NULL)
		return 0;

	int32_t ret = 0;

	if (m_cb != NULL)
		ret = m_cb(this, m_widget, NULL, move);
	return ret;
}

extern "C" int32_t ScePafWidget_5D4941E8(const FocusCandidate *candidate,
					 Widget::FocusSearchInfo *info)
{
	math::v4 from_pos = candidate->m_from_pos;
	math::v4 from_size = candidate->m_from_size;
	math::v4 to_pos = candidate->m_to_pos;
	math::v4 to_size = candidate->m_to_size;
	float half_x = candidate->m_half_view.extract_x();
	float half_y = candidate->m_half_view.extract_y();
	int32_t dir = info->m_dir;
	float ratio = info->m_ratio;
	float left = to_pos.extract_x() - to_size.extract_x() * 0.5f;
	float top = to_pos.extract_y() - to_size.extract_y() * 0.5f;
	float right = to_pos.extract_x() + to_size.extract_x() * 0.5f;
	float bottom = to_pos.extract_y() + to_size.extract_y() * 0.5f;

	if (info->m_rect != math::rect()) {
		if (right < info->m_rect.left() || left > info->m_rect.right() ||
		    bottom < info->m_rect.bottom() || top > info->m_rect.top())
			return -2147483647;
	}

	if (dir == 1 || dir == 2) {
		float from_left = from_pos.extract_x() - from_size.extract_x() * 0.5f;
		float from_right = from_pos.extract_x() + from_size.extract_x() * 0.5f;

		if (from_left > left && from_left < right)
			left = from_left;
		if (from_right > left && from_right < right)
			right = from_right;
		if (__builtin_fabsf(from_pos.extract_y() - to_pos.extract_y()) > 5.0f)
			to_pos.set_x((left + right) * 0.5f);
	} else if (dir == 3 || dir == 4) {
		float from_top = from_pos.extract_y() - from_size.extract_y() * 0.5f;
		float from_bottom = from_pos.extract_y() + from_size.extract_y() * 0.5f;

		if (from_top > top && from_top < bottom)
			top = from_top;
		if (from_bottom > top && from_bottom < bottom)
			bottom = from_bottom;
		if (__builtin_fabsf(from_pos.extract_x() - to_pos.extract_x()) > 5.0f)
			to_pos.set_y((top + bottom) * 0.5f);
	}

	float dx = to_pos.extract_x() - from_pos.extract_x();
	float dy = to_pos.extract_y() - from_pos.extract_y();
	float adx = __builtin_fabsf(dx);
	float ady = __builtin_fabsf(dy);

	if (adx > 1000.0f)
		adx = 1000.0f;
	if (ady > 1000.0f)
		ady = 1000.0f;

	math::v4 to_half = to_size * 0.5f;
	math::v4 from_half = from_size * 0.5f;

	switch (dir) {
	case 1: {
		bool enabled = info->m_enabled;
		bool outside = info->m_search_outside;

		if (dy <= 1.0f)
			return -2147483647;

		float limit = (half_x + adx) / (half_y * 0.8f - from_pos.extract_y());

		if (!enabled || limit < 1.2f)
			limit = 1.2f;
		if (outside && adx > limit * ady)
			return -2147483647;

		math::v4 edge = from_pos + from_half;
		math::v4 far_gap = (to_pos + to_half) - edge;

		ady = __builtin_fabsf(((to_pos - to_half) - edge).extract_y() * ratio);
		if (outside && far_gap.extract_y() <= 1.0f)
			return -2147483647;
		break;
	}
	case 2: {
		bool enabled = info->m_enabled;
		bool outside = info->m_search_outside;

		if (dy >= -1.0f)
			return -2147483647;

		float limit = (half_x + adx) / (from_pos.extract_y() + half_y * 0.8f);

		if (!enabled || limit < 1.2f)
			limit = 1.2f;
		if (outside && adx > limit * ady)
			return -2147483647;

		math::v4 edge = from_pos - from_half;
		math::v4 far_gap = (to_pos - to_half) - edge;

		ady = __builtin_fabsf(((to_pos + to_half) - edge).extract_y() * ratio);
		if (outside && far_gap.extract_y() >= -1.0f)
			return -2147483647;
		break;
	}
	case 3: {
		bool enabled = info->m_enabled;
		bool outside = info->m_search_outside;

		if (dx >= -1.0f)
			return -2147483647;

		float limit = (half_y + ady) / (from_pos.extract_x() + half_x * 0.8f);

		if (!enabled || limit < 1.0f)
			limit = 1.0f;
		if (outside && limit * adx < ady)
			return -2147483647;

		math::v4 edge = from_pos - from_half;
		float far_gap = ((to_pos - to_half) - edge).extract_x();

		adx = __builtin_fabsf(((to_pos + to_half) - edge).extract_x() * ratio);
		if (outside && far_gap >= -1.0f)
			return -2147483647;
		break;
	}
	case 4: {
		bool enabled = info->m_enabled;
		bool outside = info->m_search_outside;

		if (dx <= 1.0f)
			return -2147483647;

		float limit = (half_y + ady) / (half_x * 0.8f - from_pos.extract_x());

		if (!enabled || limit < 1.0f)
			limit = 1.0f;
		if (outside && limit * adx < ady)
			return -2147483647;

		math::v4 edge = from_pos + from_half;
		float far_gap = ((to_pos + to_half) - edge).extract_x();

		adx = __builtin_fabsf(((to_pos - to_half) - edge).extract_x() * ratio);
		if (outside && far_gap <= 1.0f)
			return -2147483647;
		break;
	}
	default:
		break;
	}
	return (int32_t)((((1000.0f - adx) + 1000.0f) - ady) * 255.0f / 1000.0f);
}

/* the focus of a widget and the place of the origin of the search, in the world's space: the
   widget's focus shape, or its own place and size where its hint does not search a rectangle */
static inline __attribute__((always_inline)) void GetFocusPlace(Widget *widget, math::v4 *pos,
								  math::v4 *size)
{
	Focuser *focuser = widget->m_focuser;
	FocusHint *hint = focuser != NULL ? focuser->m_hint : NULL;

	if (hint != NULL && !hint->m_search_rect_enable) {
		*pos = widget->GetWorldMatrix().w();
		*size = *widget->GetSize(0);
	} else {
		*pos = widget->GetFocusShapeWorldPos();
		*size = widget->GetFocusShapeSize();
	}
}

int32_t DefaultFocusCB(Focuser *focuser, Widget *widget, void *data, void *move)
{
	Widget::FocusSearchInfo *info = (Widget::FocusSearchInfo *)move;

	if (info == NULL)
		return 0;

	math::v4 from_pos(info->m_pos.extract_x(), info->m_pos.extract_y(),
			  info->m_pos_zw.extract_x(), info->m_pos_zw.extract_y());
	math::v4 from_size(info->m_size.extract_x(), info->m_size.extract_y(),
			   info->m_size_zw.extract_x(), info->m_size_zw.extract_y());
	Widget *origin = info->m_origin;

	if (origin == NULL) {
		origin = widget->m_scene;
		if (origin == NULL)
			return 0;
		GetFocusPlace(origin, &from_pos, &from_size);
	}
	if (origin == widget)
		return 0;

	math::v4 to_pos = widget->GetFocusShapeWorldPos();
	math::v4 to_size = widget->GetFocusShapeSize();
	Focuser *own = widget->m_focuser;

	if (own != NULL) {
		FocusHint *hint = own->m_hint;

		if (hint != NULL && !hint->m_search_rect_enable) {
			to_pos = widget->GetWorldMatrix().w();
			to_size = *widget->GetSize(0);
		}
	}
	if (!info->m_search_outside) {
		from_pos = origin->GetWorldMatrix().w();
		from_size = *origin->GetSize(0);
		to_pos = widget->GetWorldMatrix().w();
		to_size = *widget->GetSize(0);
	}

	if (__builtin_fabsf(from_pos.extract_x() - to_pos.extract_x()) <= 5.0f &&
	    __builtin_fabsf(from_pos.extract_y() - to_pos.extract_y()) <= 5.0f &&
	    __builtin_fabsf(from_size.extract_x() - to_size.extract_x()) <= 5.0f &&
	    __builtin_fabsf(from_size.extract_y() - to_size.extract_y()) <= 5.0f &&
	    origin != ScePafWidget_CFA1EB7A(widget))
		return 0;

	math::v4 scale = *widget->GetScale(0);

	if (__builtin_fabsf(to_size.extract_x() * scale.extract_x()) <= 5.0f &&
	    __builtin_fabsf(to_size.extract_y() * scale.extract_y()) <= 5.0f)
		return 0;

	math::v4 view = *origin->m_scene->GetSize(0);
	FocusCandidate candidate;

	candidate.m_from_pos = from_pos;
	candidate.m_from_size = from_size;
	candidate.m_to_pos = to_pos;
	candidate.m_to_size = to_size;
	candidate.m_half_view = math::v2(view.extract_x() * 0.5f, view.extract_y() * 0.5f);
	return ScePafWidget_5D4941E8(&candidate, info);
}

/* whether the widget's hint says its own place and size are searched, not a rectangle */
static inline __attribute__((always_inline)) bool SearchesOwnPlace(Widget *widget)
{
	Focuser *focuser = widget->m_focuser;

	if (focuser != NULL && focuser->m_hint != NULL)
		return !focuser->m_hint->m_search_rect_enable;
	return false;
}

extern "C" void ScePafWidget_EA117CF8(Widget::FocusSearchInfo *info, Widget *widget)
{
	info->m_origin = widget;
	if (widget == NULL) {
		info->m_pos = math::v2(0.0f, 0.0f);
		info->m_pos_zw = math::v2(0.0f, 0.0f);
		info->m_size = math::v2(0.0f, 0.0f);
		info->m_size_zw = math::v2(0.0f, 0.0f);
		return;
	}

	math::v4 pos;

	if (SearchesOwnPlace(widget))
		pos = widget->GetWorldMatrix().w();
	else
		pos = widget->GetFocusShapeWorldPos();
	info->m_pos = math::v2(pos.extract_x(), pos.extract_y());
	info->m_pos_zw = math::v2(pos.extract_z(), pos.extract_w());

	math::v4 size;

	if (GetRootFocus(widget) == widget) {
		if (SearchesOwnPlace(widget))
			size = *widget->GetSize(0);
		else
			size = widget->GetFocusShapeSize();
	} else {
		size = math::v4(1.0f, 1.0f, 1.0f, 1.0f);
	}
	info->m_size = math::v2(size.extract_x(), size.extract_y());
	info->m_size_zw = math::v2(size.extract_z(), size.extract_w());

	Focuser *focuser = widget->m_focuser;

	if (focuser != NULL && focuser->m_hint != NULL) {
		math::rect rect(focuser->m_hint->m_search_rect);

		info->m_rect = rect;
	} else {
		math::rect rect;

		info->m_rect = rect;
	}
}

}
}
