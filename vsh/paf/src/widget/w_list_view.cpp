/*
 * paf::ui::ListView (DEBUGGER 0x810f4384..0x81100d0c): a scroll view of segments of cells, the
 * cells made by an item factory while they are near the view, and its parameters
 * (listview::Cell, CellList, CellMap, CellSizeList, Segment, SegmentList, ItemFactory,
 * SegmentHeaderFactory). No record or string names the file.
 */
#include <stddef.h>
#include <stdint.h>
#include <common/shared_ptr.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <std/algorithm>
#include <std/math.h>
#include <timer/timer.h>
#include <widget/core/animlistener.h>
#include <widget/core/environment.h>
#include <widget/core/event.h>
#include <widget/core/focus.h>
#include <widget/factory.h>
#include <widget/w_button.h>
#include <widget/w_button_base.h>
#include <widget/w_list_item.h>
#include <widget/w_list_view.h>
#include <widget/w_text_box.h>
#include <inputdevice/inputlistener.h>
#include <versatileparam/resourceparam.h>
#include "../paf_types.h"

using namespace sce::Vectormath::Simd;
using namespace sce::Vectormath::Simd::Aos;

namespace paf {
namespace ui {

/* the vector below, made in the static initializer with the constructor inlined (name ours) */
static inline __attribute__((always_inline)) math::v4 Vector(float x, float y, float z, float w)
{
	return math::v4(x, y, z, w);
}

/* the cell size a list view starts with (name ours) */
static math::v4 s_cell_size_default = Vector(80.0f, 80.0f, 0.0f, 0.0f);

static int32_t ListViewFocusCB(Focuser *focuser, Widget *widget, void *data, void *move);
static void AxisPaddingSet(AnimListener *anim, math::v4 data);
static math::v4 AxisPaddingGet(AnimListener *anim);

/* how a list view's axis padding is animated (name ours) */
static AnimPropertyV4 s_axis_padding_prop = {
	ListView::ANIM_AXIS_PADDING,
	AxisPaddingSet,
	AxisPaddingGet,
};

/* whether the two are equal, compared lane by lane */
static inline bool IsSame(const math::v2 &a, const math::v2 &b)
{
	return ((a.getImpl().getX() == b.getImpl().getX()) &
		(a.getImpl().getY() == b.getImpl().getY()))
		.getAsBool();
}
namespace listview {

math::v4 Segment::GetCellSize(int32_t index)
{
	if (index < 0 || !(m_cell_num > index))
		return math::v4(0.0f, 0.0f, 0.0f, 0.0f);
	if (m_layout_type == ListView::LAYOUT_TYPE_LIST_VARIABLE) {
		if (m_segment_list->m_list_view->m_scroll_type == ListView::SCROLL_TYPE_VERTICAL) {
			floatInVec length(m_cell_size_list->at(index).m_size);
			Vector4 size = m_cell_size_default.getImpl();

			size.setElem(1, length);
			return math::v4(size);
		} else {
			floatInVec length(m_cell_size_list->at(index).m_size);
			Vector4 size = m_cell_size_default.getImpl();

			size.setElem(0, length);
			return math::v4(size);
		}
	}
	return m_cell_size_default;
}

math::v2 Cell::GetPos(bool current)
{
	if (current)
		return math::v2(m_item->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f))
					.getImpl().getXY());
	return m_pos;
}

/* left, top, right and bottom */
math::v4 Cell::GetRect(bool current)
{
	math::v2 pos;

	if (current)
		pos = math::v2(m_item->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f))
				       .getImpl().getXY());
	else
		pos = m_pos;

	math::v2 size(GetSegment()->GetCellSize(m_index).getImpl().getXY());
	math::v2 head = pos + size * math::v2(-0.5f, 0.5f);
	math::v2 tail = pos + size * math::v2(0.5f, -0.5f);

	return math::v4(Vector4(head.getImpl(), tail.getImpl()));
}

}

/* the range of the view along its axis, from its head to its tail (name ours) */
math::v2 ListView::GetVisibleRange(int32_t opt)
{
	math::v4 pos = m_view_parent->GetPos(opt, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

	if (m_scroll_type == SCROLL_TYPE_VERTICAL) {
		float head = -pos.extract_y();
		float half = m_size.extract_y() * 0.5f;

		return math::v2(half + head, head - half);
	} else {
		float head = -pos.extract_x();
		float half = m_size.extract_x() * 0.5f;

		return math::v2(head - half, half + head);
	}
}

/* the cells shown, counted (no name found) */
int32_t ListView::ScePafWidget_9D5DB5D6()
{
	int32_t num = 0;
	math::v2 range = GetVisibleRange(0);

	if (m_scroll_type == SCROLL_TYPE_VERTICAL) {
		for (listview::CellMap::iterator it = m_cell_map->begin(); it != m_cell_map->end();
		     ++it) {
			math::v4 rect = (*it)->GetRect(false);

			if ((range.getImpl().getX() >= rect.getImpl().getW()).getAsBool() &&
			    (rect.getImpl().getY() >= range.getImpl().getY()).getAsBool())
				++num;
		}
	} else {
		for (listview::CellMap::iterator it = m_cell_map->begin(); it != m_cell_map->end();
		     ++it) {
			math::v4 rect = (*it)->GetRect(false);

			if ((rect.getImpl().getZ() >= range.getImpl().getX()).getAsBool() &&
			    (range.getImpl().getY() >= rect.getImpl().getX()).getAsBool())
				++num;
		}
	}
	return num;
}

namespace listview {

int32_t Cell::GetSegmentIndex() const
{
	return GetSegment()->m_index;
}

}

/* the touches it holds let go of where the view has gone past its scroll box while a scroll
   range is set (name ours) */
void ListView::ReleaseTouchOutOfRange()
{
	if (!m_scroll_range_set || m_track_port == -1)
		return;

	math::v2 box(m_view_box.getImpl().getXY());

	if (m_scroll_type == SCROLL_TYPE_VERTICAL) {
		if (box.getImpl().getY() == 0.0f)
			return;

		Vector2 pos = m_view_parent->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f))
				      .getImpl().getXY();
		Vector4 over = m_scroll_box.getImpl() - (m_view_half.getImpl() - Vector4(pos, pos));

		if ((over.getY() >= floatInVec(0.0f)).getAsBool() &&
		    (over.getW() <= floatInVec(0.0f)).getAsBool())
			return;
	} else {
		if (box.getImpl().getX() == 0.0f)
			return;

		Vector2 pos = m_view_parent->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f))
				      .getImpl().getXY();
		Vector4 over = m_scroll_box.getImpl() - (m_view_half.getImpl() - Vector4(pos, pos));

		if ((over.getX() <= floatInVec(0.0f)).getAsBool() &&
		    (over.getZ() >= floatInVec(0.0f)).getAsBool())
			return;
	}
	++m_sync_block;
	ScePafWidget_931AD41E(this, false);
	--m_sync_block;
}

namespace listview {

void Segment::SetHeaderPos(float pos, const CoordinateEffector &effector)
{
	if (m_segment_list->m_list_view->m_scroll_type == ListView::SCROLL_TYPE_VERTICAL) {
		math::v4 *size = m_header->GetSize(0);
		float half = size->extract_y() * 0.5f;

		effector.SetHeaderPos(m_header, math::v4(0.0f, pos - half, 0.0f, 0.0f), half);
	} else {
		math::v4 *size = m_header->GetSize(0);
		float half = size->extract_x() * 0.5f;

		effector.SetHeaderPos(m_header, math::v4(half + pos, 0.0f, 0.0f, 0.0f), half);
	}
}

/* nothing where the header disappears with no cells */
math::v4 Segment::GetHeaderSize()
{
	math::v4 size = m_header != NULL ? *m_header->GetSize(0)
					 : math::v4(Vector4(m_header_size));

	if (m_cell_num == 0 && m_header_disappearance)
		return math::v4(0.0f, 0.0f, 0.0f, 0.0f);
	return size;
}

void Segment::SetHeaderVisible(bool visible)
{
	if (m_header_visible == visible)
		return;
	m_header_visible = visible;
	if (visible) {
		Widget *header = m_header;

		if (!header->m_transition_complete) {
			header->m_transition = 0xff;
			header->m_transition_complete = true;
			header->DoTransition_core(0.0f, true);
		}
		ScePafWidget_FF5BC957(header, NULL, 2, 0.0f, NULL);
	} else {
		Widget *header = m_header;

		header->m_transition_complete = false;
		header->m_transition = 0xff;
		header->DoTransitionReverse_core(0.0f, true);
		ScePafWidget_4588699E(header, NULL, 2, 0.0f, NULL);
	}
}

/* a floating header kept in the range shown, within its segment */
void Segment::ResetHeaderPosition(const math::v2 &range, const CoordinateEffector &effector)
{
	if (m_header == NULL)
		return;

	ListView::ScrollType scroll_type = m_segment_list->m_list_view->m_scroll_type;

	SetHeaderVisible(m_cell_num != 0 || !m_header_disappearance);

	float pos;

	if (m_header_floating) {
		if (scroll_type == ListView::SCROLL_TYPE_VERTICAL) {
			math::v2 seen = effector.GetHeaderRange(range, GetHeaderSize().extract_y());
			math::v4 size = GetHeaderSize();

			pos = fminf(fmaxf(seen.getImpl().getX(),
					  floatInVec(m_pos) - m_view_size.getImpl().getY() +
						  size.getImpl().getY()),
				    floatInVec(m_pos));
		} else {
			math::v2 seen = effector.GetHeaderRange(range, GetHeaderSize().extract_x());
			floatInVec head = fmaxf(seen.getImpl().getX(), floatInVec(m_pos));
			math::v4 size = GetHeaderSize();

			pos = fminf(head, floatInVec(m_pos) + m_view_size.getImpl().getX() -
						  size.getImpl().getX());
		}
	} else {
		pos = m_pos;
	}
	SetHeaderPos(pos, effector);
}

/* its cells placed after its header, one after another or in a grid */
void Segment::ResetCellPosition(const CoordinateEffector &effector)
{
	if (m_cell_list.size() == 0)
		return;

	if (m_segment_list->m_list_view->m_scroll_type == ListView::SCROLL_TYPE_VERTICAL) {
		float head = floatInVec(m_pos) - GetHeaderSize().getImpl().getY();

		if (m_layout_type == ListView::LAYOUT_TYPE_LIST_VARIABLE) {
			for (CellList::iterator it = m_cell_list.begin(); it != m_cell_list.end(); ++it) {
				Cell *cell = it->get();
				CellSize &size = m_cell_size_list->at(cell->m_index);
				float half = size.m_size * 0.5f;

				effector.SetCellPos(cell, math::v2(0.0f, head - size.m_pos - half), half);
			}
		} else {
			math::v4 size = m_cell_size_default;
			float height = size.extract_y();
			float half = height * 0.5f;
			float top = head - half;

			if (m_layout_type == ListView::LAYOUT_TYPE_LIST) {
				for (CellList::iterator it = m_cell_list.begin(); it != m_cell_list.end();
				     ++it) {
					Cell *cell = it->get();

					effector.SetCellPos(
						cell,
						math::v2(0.0f, top - height * (float)cell->m_index),
						half);
				}
			} else {
				float width = size.extract_x();
				float left = (float)(m_column_num - 1) * width * -0.5f;

				for (CellList::iterator it = m_cell_list.begin(); it != m_cell_list.end();
				     ++it) {
					Cell *cell = it->get();
					float x = left + width * (float)(cell->m_index % m_column_num);
					float y = top - height * (float)(cell->m_index / m_column_num);

					effector.SetCellPos(cell, math::v2(x, y), half);
				}
			}
		}
	} else {
		float head = floatInVec(m_pos) + GetHeaderSize().getImpl().getX();

		if (m_layout_type == ListView::LAYOUT_TYPE_LIST_VARIABLE) {
			for (CellList::iterator it = m_cell_list.begin(); it != m_cell_list.end(); ++it) {
				Cell *cell = it->get();
				CellSize &size = m_cell_size_list->at(cell->m_index);
				float half = size.m_size * 0.5f;

				effector.SetCellPos(cell, math::v2(size.m_pos + head + half, 0.0f), half);
			}
		} else {
			math::v4 size = m_cell_size_default;
			float width = size.extract_x();
			float half = width * 0.5f;
			float left = head + half;

			if (m_layout_type == ListView::LAYOUT_TYPE_LIST) {
				for (CellList::iterator it = m_cell_list.begin(); it != m_cell_list.end();
				     ++it) {
					Cell *cell = it->get();

					effector.SetCellPos(
						cell,
						math::v2(width * (float)cell->m_index + left, 0.0f),
						half);
				}
			} else {
				float height = size.extract_y();
				float span = (float)(m_column_num - 1) * height;

				for (CellList::iterator it = m_cell_list.begin(); it != m_cell_list.end();
				     ++it) {
					Cell *cell = it->get();
					float x = width * (float)(cell->m_index / m_column_num) + left;
					float y = span * 0.5f - height * (float)(cell->m_index % m_column_num);

					effector.SetCellPos(cell, math::v2(x, y), half);
				}
			}
		}
	}
}


inline bool CellLess::operator()(const Cell *a, const Cell *b) const
{
	if (a->GetSegment()->m_index == b->GetSegment()->m_index)
		return a->m_index < b->m_index;
	return a->GetSegment()->m_index < b->GetSegment()->m_index;
}

inline bool CellLess::operator()(const Cell *cell, const ListPos &pos) const
{
	if (cell->GetSegment()->m_index == pos.m_segment)
		return cell->m_index < pos.m_cell;
	return cell->GetSegment()->m_index < pos.m_segment;
}

inline bool CellLess::operator()(const ListPos &pos, const Cell *cell) const
{
	if (pos.m_segment == cell->GetSegment()->m_index)
		return pos.m_cell < cell->m_index;
	return pos.m_segment < cell->GetSegment()->m_index;
}

CellMap::iterator CellMap::LowerBound(Cell *cell)
{
	return lower_bound(begin(), end(), cell, CellLess());
}

/* the cell put in its place, where it is not there yet */
CellMap::iterator CellMap::Insert(Cell *cell)
{
	iterator it = LowerBound(cell);

	if (it == end() || *it != cell)
		it = insert(it, cell);
	return it;
}

ListView *Cell::GetListView()
{
	return GetSegment()->m_segment_list->m_list_view;
}

Cell::Cell(CellList *list, int32_t index)
	: m_cell_list(list), m_index(index), m_started(false), m_free_position(false),
	  m_effector_pos(0.0f, 0.0f), m_effector_value(0.0f)
{
	ItemFactory::CreateParam param;
	ListView *list_view = GetListView();

	param.m_list_view = list_view;
	param.m_segment = GetSegment()->m_index;
	param.m_cell = index;
	param.m_parent = list_view->m_view_parent;
	if (list_view->m_cell_size_set != NULL) {
		vector<SegmentCellRange>::iterator it =
			lower_bound(list_view->m_cell_size_set->begin(),
				    list_view->m_cell_size_set->end(), param.m_segment);

		if (it != list_view->m_cell_size_set->end() && it->m_segment == param.m_segment) {
			vector<CellRange>::iterator range =
				lower_bound(it->m_ranges.begin(), it->m_ranges.end(), index,
					    CellRangeLess());

			param.m_cell_size_set = range != it->m_ranges.end() && range->m_first <= index;
		} else {
			param.m_cell_size_set = false;
		}
	}
	m_item = list_view->m_item_factory->Create(param);
	m_item->m_cell = this;
	if (list_view->m_drag_cell != NULL)
		m_item->SetBehind(list_view->m_drag_cell->m_item);
	else if (list_view->m_first_header != NULL)
		m_item->SetBehind(list_view->m_first_header);
	m_pos = math::v2(m_item->GetPos(1, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f))
				 .getImpl().getXY());
	if (list_view->m_env != NULL && ScePafWidget_A6B659BB(list_view->m_env, 0) &&
	    list_view->m_focus_list_pos.m_segment == param.m_segment && list_view->m_focus_list_pos.m_cell == m_index)
		m_item->ScePafWidget_C98D1926(math::v2(0.0f, 0.0f));
	list_view->m_cell_map->Insert(this);
}

inline bool CellIndexLess::operator()(const common::SharedPtr<Cell> &cell, int32_t index) const
{
	return cell->m_index < index;
}

CellList::iterator CellList::LowerBound(int32_t index)
{
	return lower_bound(begin(), end(), index, CellIndexLess());
}

Cell *CellList::CreateCell(int32_t index)
{
	iterator it = LowerBound(index);

	if (it == end() || (*it)->m_index != index)
		it = insert(it, common::SharedPtr<Cell>(new Cell(this, index)));
	return it->get();
}

/* the cells from first up to last made, the others destroyed but the one being dragged */
void Segment::GenerateCell(int32_t first, int32_t last)
{
	Cell *drag = m_segment_list->m_list_view->m_drag_cell;

	for (CellList::iterator it = m_cell_list.begin(); it != m_cell_list.end();) {
		int32_t index = (*it)->m_index;

		if ((first <= index && index < last) || it->get() == drag)
			++it;
		else
			it = m_cell_list.erase(it);
	}
	for (; first != last; ++first)
		m_cell_list.CreateCell(first);
}

CellSizeList::iterator CellSizeList::LowerBound(float pos)
{
	return lower_bound(begin(), end(), pos, CellSizeLess());
}

CellSizeList::iterator CellSizeList::Find(float pos)
{
	if (size() == 0)
		return end();

	iterator it = LowerBound(pos);

	if (it == begin() || (it != end() && it->m_pos == pos))
		return it;
	return --it;
}

/* the cells from first up to last that come into the range */
void Segment::GetGenerationIndex(int32_t &first, int32_t &last, const math::v2 &range)
{
	last = 0;
	first = 0;
	if (m_cell_num == 0)
		return;

	int32_t index;
	int32_t num;

	if (m_segment_list->m_list_view->m_scroll_type == ListView::SCROLL_TYPE_VERTICAL) {
		math::v4 header = GetHeaderSize();
		floatInVec pos(m_pos);
		Vector2 span(pos - header.getImpl().getY(), pos - m_view_size.getImpl().getY());
		Vector2 seen = clampPerElem(span, Vector2(range.getImpl().getY()),
				     Vector2(range.getImpl().getX()));

		if ((float)(seen.getY() - seen.getX()) == 0.0f)
			return;

		float from = span.getX() - seen.getX();
		float to = span.getX() - seen.getY();

		if (m_layout_type == ListView::LAYOUT_TYPE_LIST_VARIABLE) {
			CellSizeList::iterator it = m_cell_size_list->Find(from);

			index = it - m_cell_size_list->begin();
			num = m_cell_size_list->LowerBound(to) - it;
		} else {
			math::v4 size = m_cell_size_default;
			float height = size.extract_y();

			index = (int32_t)(from / height);
			num = (int32_t)sce_paf_ceilf((to - height * (float)index) / height);
		}
	} else {
		math::v4 header = GetHeaderSize();
		floatInVec pos(m_pos);
		Vector2 span(pos + header.getImpl().getX(), pos + m_view_size.getImpl().getX());
		Vector2 seen = clampPerElem(span, Vector2(range.getImpl().getX()),
				     Vector2(range.getImpl().getY()));

		if ((float)(seen.getY() - seen.getX()) == 0.0f)
			return;

		float from = seen.getX() - span.getX();
		float to = seen.getY() - span.getX();

		if (m_layout_type == ListView::LAYOUT_TYPE_LIST_VARIABLE) {
			CellSizeList::iterator it = m_cell_size_list->Find(from);

			index = it - m_cell_size_list->begin();
			num = m_cell_size_list->LowerBound(to) - it;
		} else {
			math::v4 size = m_cell_size_default;
			float width = size.extract_x();

			index = (int32_t)(from / width);
			num = (int32_t)sce_paf_ceilf((to - width * (float)index) / width);
		}
	}
	if (m_layout_type == ListView::LAYOUT_TYPE_GRID) {
		first = index * m_column_num;
		last = first + num * m_column_num;
	} else {
		first = index;
		last = index + num;
	}
	if (m_cell_num < last)
		last = m_cell_num;
}

void Segment::Sync(const SyncParam &param)
{
	int32_t first;
	int32_t last;

	GetGenerationIndex(first, last, param.m_generate_range);
	GenerateCell(first, last);
	ResetCellPosition(*param.m_effector);
	ResetHeaderPosition(param.m_visible_range, *param.m_effector);
	m_synced = true;
}

bool Segment::ScePafWidget_E8717C5D(const math::v2 &range)
{
	if (m_header_size == 0.0f)
		return false;

	ListView *list_view = m_segment_list->m_list_view;
	floatInVec pos(m_pos);
	Widget *header = m_header;
	Vector2 seen;

	if (list_view->m_scroll_type == ListView::SCROLL_TYPE_VERTICAL)
		seen = clampPerElem(Vector2(pos, pos - m_view_size.getImpl().getY()),
			     Vector2(range.getImpl().getY()), Vector2(range.getImpl().getX()));
	else
		seen = clampPerElem(Vector2(pos, pos + m_view_size.getImpl().getX()),
			     Vector2(range.getImpl().getX()), Vector2(range.getImpl().getY()));

	if ((float)(seen.getY() - seen.getX()) == 0.0f) {
		if (header != NULL) {
			SegmentHeaderFactory::DisposeParam param;

			param.m_list_view = list_view;
			param.m_header = header;
			param.m_segment = m_index;
			m_header = NULL;
			list_view->m_header_factory->Dispose(param);
		}
		return false;
	}
	if (header == NULL) {
		SegmentHeaderFactory::CreateParam param;

		param.m_list_view = list_view;
		param.m_segment = m_index;
		param.m_parent = list_view->m_view_parent;
		m_header = list_view->m_header_factory->Create(param);
		if (m_header != NULL)
			return true;
	}
	return false;
}

}

void ListView::ScePafWidget_433FAAD4(const math::v2 &range)
{
	if (m_header_factory == NULL)
		return;

	m_first_header = NULL;

	Widget *prev = NULL;
	bool made = false;

	for (listview::SegmentList::iterator it = m_segment_list->begin();
	     it != m_segment_list->end(); ++it) {
		listview::Segment *segment = it->get();

		made |= segment->ScePafWidget_E8717C5D(range);

		Widget *header = segment->m_header;

		if (m_first_header == NULL)
			m_first_header = header;
		if (header != NULL) {
			if (made && prev != NULL)
				header->SetForward(prev);
			prev = segment->m_header;
		}
	}
}

namespace listview {

/* places through a transform of the position, the same one by default (names ours) */
class TransformEffector : public CoordinateEffector {
public:
	virtual void SetCellPos(Cell *cell, const math::v2 &pos, float half) const
	{
		math::v2 moved = Transform(pos, half);

		cell->SetPos(moved, false);
		cell->m_effector_pos = pos;
		cell->m_effector_value = half;
	}

	virtual void SetHeaderPos(Widget *header, const math::v4 &pos, float half) const
	{
		math::v2 moved = Transform(math::v2(pos.getImpl().getXY()), half);

		Vector4 value = pos.getImpl();

		value.setXY(moved.getImpl());
		header->SetPos(math::v4(value));
	}

	virtual math::v2 Transform(const math::v2 &pos, float half) const
	{
		return pos;
	}
};

/* the elastic pull past the view's head or tail, by how far it goes (m_offset): what is placed
   is drawn back by the scale for its distance from the end (names ours) */
class VerticalHeadEffector : public TransformEffector {
public:
	virtual math::v2 ClampRange(const math::v2 &range) const
	{
		return math::v2(range.getImpl() + Vector2(m_offset));
	}

	virtual math::v2 Transform(const math::v2 &pos, float half) const
	{
		Vector2 value = pos.getImpl();
		float y = pos.extract_y();

		return math::v2(Vector2(value.getX(), floatInVec((y - m_offset) + (half + y) * m_scale)));
	}

	float m_offset;
	float m_scale;
};

class VerticalTailEffector : public TransformEffector {
public:
	virtual math::v2 ClampRange(const math::v2 &range) const
	{
		return math::v2(range.getImpl() + Vector2(m_offset));
	}

	virtual math::v2 GetHeaderRange(const math::v2 &range, float length) const
	{
		return math::v2(m_end + (m_length + length * m_scale) / (m_scale + 1.0f), m_end);
	}

	virtual math::v2 Transform(const math::v2 &pos, float half) const
	{
		Vector2 value = pos.getImpl();
		float y = pos.extract_y();

		return math::v2(Vector2(value.getX(),
					floatInVec((y - m_offset) - (m_end - (y - half)) * m_scale)));
	}

	float m_offset;
	float m_scale;
	/* the view's end, and its length */
	float m_end;
	float m_length;
};

class HorizontalHeadEffector : public TransformEffector {
public:
	virtual math::v2 ClampRange(const math::v2 &range) const
	{
		return math::v2(range.getImpl() + Vector2(m_offset));
	}

	virtual math::v2 Transform(const math::v2 &pos, float half) const
	{
		Vector2 value = pos.getImpl();
		float x = pos.extract_x();

		value.setX((x - m_offset) + (x - half) * m_scale);
		return math::v2(value);
	}

	float m_offset;
	float m_scale;
};

class HorizontalTailEffector : public TransformEffector {
public:
	virtual math::v2 ClampRange(const math::v2 &range) const
	{
		return math::v2(range.getImpl() + Vector2(m_offset));
	}

	virtual math::v2 GetHeaderRange(const math::v2 &range, float length) const
	{
		return math::v2(m_end - (m_length + length * m_scale) / (m_scale + 1.0f), m_end);
	}

	virtual math::v2 Transform(const math::v2 &pos, float half) const
	{
		Vector2 value = pos.getImpl();
		float x = pos.extract_x();

		value.setX((x - m_offset) - (m_end - (half + x)) * m_scale);
		return math::v2(value);
	}

	float m_offset;
	float m_scale;
	float m_end;
	float m_length;
};

}

/* how far past the end it is pulled to how much it gives (names ours) */
static inline float ElasticScale(float over)
{
	return __builtin_sqrtf(over / 960.0f + 0.06666667f) - 0.2581989f;
}

/* the cells placed as they are, or pulled elastically while the view is dragged past its head
   or tail; the overshoot the drag may reach is kept */
const listview::CoordinateEffector *ListView::GetCoordinateEffector()
{
	static listview::CoordinateEffector s_effector;
	static listview::TransformEffector s_transform;
	static listview::VerticalHeadEffector s_vertical_head;
	static listview::VerticalTailEffector s_vertical_tail;
	static listview::HorizontalHeadEffector s_horizontal_head;
	static listview::HorizontalTailEffector s_horizontal_tail;

	if (m_scroll_type == SCROLL_TYPE_VERTICAL) {
		Vector4 box = m_scroll_box.getImpl();

		s_effector.m_limit = math::v2(Vector2(box.getY(), box.getW()));
		if (m_over_scroll != NULL || !m_effector_enabled ||
		    (m_move_dir == 0 &&
		     (m_size.getImpl().getY() >= m_scroll_box_size.getImpl().getY()).getAsBool()))
			return &s_effector;

		math::v4 pos = m_view_parent->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

		if (m_track_port != -1) {
			int32_t num;
			int32_t i;

			m_effector_offset = GetTouchPos().extract_y() - pos.extract_y();
			num = m_segment_list->size();
			for (i = 0; i < num; ++i) {
				listview::Segment *segment = m_segment_list->at(i).get();

				if (segment->m_view_size.extract_y() != 0.0f) {
					float length = segment->GetHeaderSize().extract_y();

					if (length == 0.0f)
						length = segment->GetCellSize(0).extract_y();
					if (!(m_effector_offset <= -length)) {
						m_effector_offset = -m_size.extract_y();
						goto pulled;
					}
					break;
				}
			}
			for (i = num - 1; i >= 0; --i) {
				listview::Segment *segment = m_segment_list->at(i).get();

				if (segment->m_view_size.extract_y() != 0.0f) {
					float length;

					if (segment->m_cell_num != 0)
						length = segment->GetCellSize(segment->m_cell_num - 1)
								 .extract_y();
					else
						length = segment->GetHeaderSize().extract_y();

					float end = m_scroll_box.extract_w();

					if (m_effector_offset < end + length)
						m_effector_offset = end + m_size.extract_y();
					break;
				}
			}
		}
pulled:
		Vector2 limit(m_scroll_box.getImpl().getY(), m_scroll_box.getImpl().getW());
		Vector2 size = m_size.getImpl().getXY();
		Vector2 over = limit + Vector2(pos.getImpl().getY()) +
			       Vector2(-0.5f, 0.5f) * size.getY();

		if ((float)over.getX() < 0.0f) {
			s_vertical_head.m_offset = over.getX();
			s_vertical_head.m_scale = ElasticScale(-(float)over.getX());
			return &s_vertical_head;
		}
		if ((float)over.getY() > 0.0f) {
			s_vertical_tail.m_offset = over.getY();
			s_vertical_tail.m_scale = ElasticScale(over.getY());
			s_vertical_tail.m_end = limit.getY();
			s_vertical_tail.m_length = size.getY();
			return &s_vertical_tail;
		}

		float half = m_scroll_box_size.extract_y() * 0.5f;
		float offset = size.getY() - m_scroll_box_size.extract_y();

		if (pos.extract_y() < half)
			offset = -size.getY();
		m_effector_offset = offset;
		if (((float)over.getX() == 0.0f || (float)over.getY() == 0.0f) &&
		    (m_effector == &s_vertical_head || m_effector == &s_vertical_tail)) {
			s_transform.m_limit = math::v2(limit);
			return &s_transform;
		}
	} else {
		Vector4 box = m_scroll_box.getImpl();

		s_effector.m_limit = math::v2(Vector2(box.getX(), box.getZ()));
		if (m_over_scroll != NULL || !m_effector_enabled ||
		    (m_move_dir == 0 &&
		     (m_size.getImpl().getX() >= m_scroll_box_size.getImpl().getX()).getAsBool()))
			return &s_effector;

		math::v4 pos = m_view_parent->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

		if (m_track_port != -1) {
			int32_t num;
			int32_t i;

			m_effector_offset = GetTouchPos().extract_x() - pos.extract_x();
			num = m_segment_list->size();
			for (i = 0; i < num; ++i) {
				listview::Segment *segment = m_segment_list->at(i).get();

				if (segment->m_view_size.extract_x() != 0.0f) {
					float length = segment->GetHeaderSize().extract_x();

					if (length == 0.0f)
						length = segment->GetCellSize(0).extract_x();
					if (!(m_effector_offset >= length)) {
						m_effector_offset = m_size.extract_x();
						goto pulled_x;
					}
					break;
				}
			}
			for (i = num - 1; i >= 0; --i) {
				listview::Segment *segment = m_segment_list->at(i).get();

				if (segment->m_view_size.extract_x() != 0.0f) {
					float length;

					if (segment->m_cell_num != 0)
						length = segment->GetCellSize(segment->m_cell_num - 1)
								 .extract_x();
					else
						length = segment->GetHeaderSize().extract_x();

					float end = m_scroll_box.extract_z();

					if (m_effector_offset > end - length)
						m_effector_offset = end - m_size.extract_x();
					break;
				}
			}
		}
pulled_x:
		Vector4 limits = m_scroll_box.getImpl();
		math::v4 now = m_view_parent->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		Vector2 size = m_size.getImpl().getXY();
		Vector2 limit(limits.getX(), limits.getZ());
		Vector2 over = limit + Vector2(now.getImpl().getX()) +
			       Vector2(0.5f, -0.5f) * size.getX();

		if ((float)over.getX() > 0.0f) {
			s_horizontal_head.m_offset = over.getX();
			s_horizontal_head.m_scale = ElasticScale(over.getX());
			return &s_horizontal_head;
		}
		if ((float)over.getY() < 0.0f) {
			s_horizontal_tail.m_offset = over.getY();
			s_horizontal_tail.m_scale = ElasticScale(-(float)over.getY());
			s_horizontal_tail.m_end = m_scroll_box.extract_z();
			s_horizontal_tail.m_length = size.getX();
			return &s_horizontal_tail;
		}

		float half = m_scroll_box_size.extract_x() * -0.5f;
		float offset = m_scroll_box_size.extract_x() - size.getX();

		if (pos.extract_x() > half)
			offset = size.getX();
		m_effector_offset = offset;
		if (((float)over.getX() == 0.0f || (float)over.getY() == 0.0f) &&
		    (m_effector == &s_horizontal_head || m_effector == &s_horizontal_tail)) {
			Vector4 box_now = m_scroll_box.getImpl();

			s_transform.m_limit = math::v2(Vector2(box_now.getX(), box_now.getZ()));
			return &s_transform;
		}
	}
	return &s_effector;
}

/* the visible range widened by the generate margin */
math::v2 ListView::GetGenerateRange(const math::v2 &visible_range)
{
	floatInVec head = visible_range.getImpl().getX();
	floatInVec tail = visible_range.getImpl().getY();
	floatInVec margin_head = m_generate_margin.getImpl().getX();
	floatInVec margin_tail = m_generate_margin.getImpl().getY();

	if (m_scroll_type == SCROLL_TYPE_VERTICAL)
		return math::v2(Vector2(head + margin_head, tail - margin_tail));
	return math::v2(Vector2(head - margin_head, tail + margin_tail));
}

namespace listview {

/* the range its item's drag keeps it in: along the axis from the segment's head past the
   header to its tail, across it the room the segment leaves the cell; a list's cells are
   dragged along the axis only */
void Cell::UpdateDragRange()
{
	Segment *segment = GetSegment();
	float pos = segment->m_pos;
	ListView::ScrollType scroll_type = segment->m_segment_list->m_list_view->m_scroll_type;
	ListView::LayoutType layout_type = segment->m_layout_type;
	math::v4 header_size;

	if (segment->m_header != NULL)
		header_size = *segment->m_header->GetSize(0);
	else
		header_size = math::v4(Vector4(segment->m_header_size));

	Vector4 view = segment->m_view_size.getImpl();
	Vector4 header;

	if (segment->m_cell_num == 0 && segment->m_header_disappearance)
		header = Vector4(0.0f);
	else
		header = header_size.getImpl();

	Vector2 cell = segment->GetCellSize(m_index).getImpl().getXY();
	Vector4 cells(cell, cell);
	Vector4 views(view.getXY(), view.getXY());
	Vector4 half(0.5f, -0.5f, -0.5f, 0.5f);
	Vector4 range;
	uint32_t mode;

	if (scroll_type == ListView::SCROLL_TYPE_VERTICAL) {
		Vector4 offset(0.0f, pos - (float)header.getY(), 0.0f, pos);

		range = mulPerElem(views, Vector4(-0.5f, 0.0f, 0.5f, -1.0f)) + offset +
			mulPerElem(cells, half);
		if (layout_type == ListView::LAYOUT_TYPE_GRID) {
			mode = 0x110;
		} else {
			mode = 0x121;
			range = Vector4(Vector2(floatInVec(0.0f), range.getY()),
					Vector2(floatInVec(0.0f), range.getW()));
		}
	} else {
		Vector4 offset(pos + (float)header.getX(), 0.0f, pos, 0.0f);

		range = mulPerElem(views, Vector4(0.0f, 0.5f, 1.0f, -0.5f)) + offset +
			mulPerElem(cells, half);
		if (layout_type == ListView::LAYOUT_TYPE_GRID) {
			mode = 0x110;
		} else {
			mode = 0x120;
			range = Vector4(Vector2(range.getX(), floatInVec(0.0f)),
					Vector2(range.getZ(), floatInVec(0.0f)));
		}
	}
	m_item->SetDragMode(mode);
	m_item->SetDragLimitMin(math::v4(range.getX(), range.getW(), 0.0f, 0.0f));
	m_item->SetDragLimitMax(math::v4(range.getZ(), range.getY(), 0.0f, 0.0f));
}

/* its item moved back to where the list view places it, animated where asked and it was
   moving or the list view is not syncing */
void Cell::ResetPos(bool anim)
{
	if (m_free_position)
		return;

	Timer *timer = NULL;

	if (anim && (m_item->KillAnim(Widget::ANIM_MOVE) == 0 || GetListView()->m_syncing == 0))
		timer = new Timer(200.0f, Timer::FUNC_QUAD_OUT);
	m_item->SetPos(math::v4(Vector4(m_pos.getImpl(), Vector2(0.0f))), timer, Widget::ANIM_MOVE);
}

/* moved by the offset, its item with it unless it is placed by its owner or dragged (no name
   found) */
void Cell::ScePafWidget_87B4273E(const math::v2 &offset)
{
	m_pos = math::v2(m_pos.getImpl() + offset.getImpl());
	if (m_free_position || GetListView()->m_drag_cell == this)
		return;

	bool anim = m_item->KillAnim(Widget::ANIM_MOVE) == 0;
	Widget *item = m_item;
	math::v4 pos = item->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

	item->SetPos(math::v4(pos.getImpl() + Vector4(offset.getImpl(), Vector2(0.0f))), NULL,
		     Widget::ANIM_MOVE);
	ResetPos(anim);
}


/* where the cell of the index is put to be aligned the way given, from the segment's head; its
   length along the axis where size is given (no name found) */
math::v2 Segment::ScePafWidget_7A37CEDA(int32_t index, ListView::FocusAlignType align,
					 float *size)
{
	math::v2 pos(0.0f, 0.0f);
	float length;
	float offset;
	float ratio;

	if (m_segment_list->m_list_view->m_scroll_type == ListView::SCROLL_TYPE_VERTICAL) {
		if (index < 0 || m_cell_num <= index) {
			pos.set_y(m_pos);
			return pos;
		}
		if (m_layout_type == ListView::LAYOUT_TYPE_LIST_VARIABLE) {
			CellSize &cell = m_cell_size_list->at(index);

			length = cell.m_size;
			offset = cell.m_pos;
		} else {
			math::v4 cell = m_cell_size_default;

			length = cell.extract_y();
			if (m_layout_type == ListView::LAYOUT_TYPE_LIST) {
				offset = length * (float)index;
			} else {
				int32_t column_num = m_column_num;

				offset = length * (float)(index / column_num);

				float width = (float)column_num * cell.extract_x();

				pos.set_x(cell.extract_x() * (float)(index % column_num) + width * -0.5f);
			}
		}
		ratio = align == ListView::FOCUS_ALIGN_TYPE_HEAD ? 0.0f :
			align == ListView::FOCUS_ALIGN_TYPE_TAIL ? 1.0f : 0.5f;
		offset += length * ratio;

		math::v4 header_size = m_header != NULL ? *m_header->GetSize(0)
						: math::v4(Vector4(m_header_size));
		Vector4 header;

		if (m_cell_num == 0 && m_header_disappearance)
			header = Vector4(0.0f);
		else
			header = header_size.getImpl();

		pos.set_y(m_pos - (float)header.getY() - offset);
		if (size != NULL)
			*size = length;
	} else {
		if (index < 0 || m_cell_num <= index) {
			pos.set_x(m_pos);
			return pos;
		}
		if (m_layout_type == ListView::LAYOUT_TYPE_LIST_VARIABLE) {
			CellSize &cell = m_cell_size_list->at(index);

			length = cell.m_size;
			offset = cell.m_pos;
		} else {
			math::v4 cell = m_cell_size_default;

			length = cell.extract_x();
			if (m_layout_type == ListView::LAYOUT_TYPE_LIST) {
				offset = length * (float)index;
			} else {
				int32_t column_num = m_column_num;

				offset = length * (float)(index / column_num);

				float height = (float)column_num * cell.extract_y();

				pos.set_y(height * 0.5f - cell.extract_y() * (float)(index % column_num));
			}
		}
		ratio = align == ListView::FOCUS_ALIGN_TYPE_HEAD ? 0.0f :
			align == ListView::FOCUS_ALIGN_TYPE_TAIL ? 1.0f : 0.5f;
		offset += length * ratio;

		math::v4 header_size = m_header != NULL ? *m_header->GetSize(0)
						: math::v4(Vector4(m_header_size));
		Vector4 header;

		if (m_cell_num == 0 && m_header_disappearance)
			header = Vector4(0.0f);
		else
			header = header_size.getImpl();

		pos.set_x(m_pos + (float)header.getX() + offset);
		if (size != NULL)
			*size = length;
	}
	return pos;
}


/* the size its cells and header take: along the view's axis the cells' lengths and the header,
   across it the cells' (a grid's columns fit to the view where not given), at least the least
   size given; the rest the list view's */
void Segment::UpdateViewSize()
{
	ListView *list_view = m_segment_list->m_list_view;
	ListView::ScrollType scroll_type = list_view->m_scroll_type;
	Vector4 view = list_view->GetSize(0)->getImpl();
	ListView::LayoutType layout_type = m_layout_type;
	Vector4 cell = m_cell_size_default.getImpl();
	float width;
	float height;

	if (scroll_type == ListView::SCROLL_TYPE_VERTICAL) {
		if (layout_type == ListView::LAYOUT_TYPE_LIST) {
			width = cell.getX();
			height = (float)cell.getY() * (float)(uint32_t)m_cell_num;
		} else if (layout_type == ListView::LAYOUT_TYPE_GRID) {
			float cell_width = cell.getX();
			int32_t column_num = m_fixed_column_num;

			if (column_num <= 0)
				column_num = (int32_t)((float)view.getX() / cell_width);
			m_column_num = column_num;
			if (column_num <= 0) {
				column_num = 1;
				m_column_num = 1;
			}

			int32_t row_num = ((uint32_t)m_cell_num + column_num - 1) / column_num;

			m_row_num = row_num;
			height = (float)cell.getY() * (float)row_num;
			width = (float)column_num * cell_width;
		} else {
			width = cell.getX();
			height = m_cell_size_list->m_total;
		}

		math::v4 header_size = m_header != NULL ? *m_header->GetSize(0)
						: math::v4(Vector4(m_header_size));
		Vector4 header;

		if (m_cell_num == 0 && m_header_disappearance)
			header = Vector4(0.0f);
		else
			header = header_size.getImpl();
		view.setXY(maxPerElem(m_min_view_size.getImpl(), Vector2(width, height + (float)header.getY())));
		m_view_size = math::v4(view);
	} else {
		if (layout_type == ListView::LAYOUT_TYPE_LIST) {
			width = (float)cell.getX() * (float)(uint32_t)m_cell_num;
			height = cell.getY();
		} else if (layout_type == ListView::LAYOUT_TYPE_GRID) {
			float cell_height = cell.getY();
			int32_t column_num = m_fixed_column_num;

			if (column_num <= 0)
				column_num = (int32_t)((float)view.getY() / cell_height);
			m_column_num = column_num;
			if (column_num <= 0) {
				column_num = 1;
				m_column_num = 1;
			}

			int32_t row_num = ((uint32_t)m_cell_num + column_num - 1) / column_num;

			m_row_num = row_num;
			width = (float)cell.getX() * (float)row_num;
			height = (float)column_num * cell_height;
		} else {
			width = m_cell_size_list->m_total;
			height = cell.getY();
		}

		math::v4 header_size = m_header != NULL ? *m_header->GetSize(0)
						: math::v4(Vector4(m_header_size));
		Vector4 header;

		if (m_cell_num == 0 && m_header_disappearance)
			header = Vector4(0.0f);
		else
			header = header_size.getImpl();
		view.setXY(maxPerElem(m_min_view_size.getImpl(), Vector2(width + (float)header.getX(), height)));
		m_view_size = math::v4(view);
	}
}

}

/* the view moved back by the offset, the cells from the segment's cell on moved with it and
   the ones before it put back in place (name ours) */
inline void ListView::MoveAnchored(int32_t segment, int32_t cell, math::v2 offset)
{
	m_sync_block++;
	MoveViewParent(math::v2(m_view_parent->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f))
					.getImpl()
					.getXY()) -
			       offset,
		       NULL);
	m_sync_block--;
	for (listview::CellMap::iterator it = m_cell_map->begin(); it != m_cell_map->end(); ++it) {
		listview::Cell *moving = *it;
		int32_t moving_segment = moving->GetSegment()->m_index;

		if (segment < moving_segment ||
		    (segment == moving_segment && cell <= moving->m_index)) {
			moving->ScePafWidget_87B4273E(offset);
		} else {
			math::v2 now = moving->GetSegment()->ScePafWidget_7A37CEDA(
				moving->m_index, FOCUS_ALIGN_TYPE_CENTER, NULL);

			if (!(now == moving->m_pos))
				moving->ScePafWidget_87B4273E(now - moving->m_pos);
		}
	}
}

/* the view moved to keep the anchor's cell where it was, the cells after it moved with it and
   the ones before it put back in place (no name found) */
void ListView::ScePafWidget_B04BFBAB(const listview::SyncAnchor &anchor)
{
	if (anchor.m_segment == NULL)
		return;

	int32_t segment = anchor.m_segment->m_index;
	int32_t cell = anchor.m_cell != NULL ? anchor.m_cell->m_index : -1;
	math::v2 pos = anchor.m_segment->ScePafWidget_7A37CEDA(cell, FOCUS_ALIGN_TYPE_HEAD, NULL);

	if (m_scroll_type == SCROLL_TYPE_VERTICAL) {
		float moved = pos.extract_y() - anchor.m_pos;

		if (moved != 0.0f)
			MoveAnchored(segment, cell, math::v2(0.0f, moved));
	} else {
		float moved = pos.extract_x() - anchor.m_pos;

		if (moved != 0.0f)
			MoveAnchored(segment, cell, math::v2(moved, 0.0f));
	}
}


/* each segment's size updated and the segments put one after the other, the view box made to
   hold them all (within the scroll range, where they take more than the view) and the view
   focus kept at the head where the view holds them */
void ListView::SyncViewSize()
{
	Vector2 box_size = m_scroll_box_size.getImpl();

	for (listview::SegmentList::iterator it = m_segment_list->begin();
	     it != m_segment_list->end(); ++it)
		(*it)->UpdateViewSize();

	Vector2 size = maxPerElem(m_min_size.getImpl(), m_size.getImpl().getXY());

	if (m_scroll_type == SCROLL_TYPE_VERTICAL) {
		float length = 0.0f;
		float head = 0.0f;

		for (listview::SegmentList::iterator it = m_segment_list->begin();
		     it != m_segment_list->end(); ++it) {
			Vector4 view_size = (*it)->m_view_size.getImpl();

			(*it)->m_pos = length;
			length -= (float)view_size.getY();
		}
		m_length = length;
		if (length == 0.0f) {
			SetViewBox(math::v4(0.0f, 0.0f, 0.0f, 0.0f));
			return;
		}

		Vector4 padding = m_padding.getImpl();
		float padding_length = (float)padding.getY() + (float)padding.getW();
		float tail;

		if (padding_length - length < (float)size.getY()) {
			tail = padding_length - (float)size.getY();
		} else {
			Vector2 range = m_scroll_range.getImpl();

			head += (float)range.getX();
			tail = length - (float)range.getY();
		}
		SetViewBox(math::v4((float)size.getX() * -0.5f, head, (float)size.getX() * 0.5f,
				    tail));
		if ((float)box_size.getY() <= (float)size.getY()) {
			Vector4 box = m_scroll_box.getImpl();
			Vector4 view = m_size.getImpl();
			math::v2 focus(0.0f, ((float)box.getY() - head) - (float)view.getY() * 0.5f);

			if (!ScePafWidget_28AF15BB() && !(GetViewFocusTarget() == focus)) {
				SetViewFocus(focus, NULL, true, true);
				UpdateScrollRange();
			}
		}
	} else {
		float length = 0.0f;
		float head = 0.0f;

		for (listview::SegmentList::iterator it = m_segment_list->begin();
		     it != m_segment_list->end(); ++it) {
			Vector4 view_size = (*it)->m_view_size.getImpl();

			(*it)->m_pos = length;
			length += (float)view_size.getX();
		}
		m_length = length;
		if (length == 0.0f) {
			SetViewBox(math::v4(0.0f, 0.0f, 0.0f, 0.0f));
			return;
		}

		Vector4 padding = m_padding.getImpl();
		float padding_length = (float)padding.getX() + (float)padding.getZ();
		float tail;

		if (padding_length + length < (float)size.getX()) {
			tail = (float)size.getX() - padding_length;
		} else {
			Vector2 range = m_scroll_range.getImpl();

			head -= (float)range.getX();
			tail = length + (float)range.getY();
		}
		SetViewBox(math::v4(head, (float)size.getY() * 0.5f, tail,
				    (float)size.getY() * -0.5f));
		if ((float)box_size.getX() <= (float)size.getX()) {
			Vector4 box = m_scroll_box.getImpl();
			Vector4 view = m_size.getImpl();
			math::v2 focus(((float)box.getX() - head) + (float)view.getX() * 0.5f, 0.0f);

			if (!ScePafWidget_28AF15BB() && !(GetViewFocusTarget() == focus)) {
				ScrollViewBase::SetViewFocus(focus, NULL, true, true);
				UpdateScrollRange();
			}
		}
	}
}


/* where the layout changed, the cell or the segment header at the head of the view a sync keeps
   in place: a cell across the visible range, else the first synced segment whose header comes
   into it (no name found) */
void ListView::ScePafWidget_96B46364(listview::SyncAnchor &anchor)
{
	anchor.m_segment = NULL;
	if (m_layout_count == 0)
		return;

	math::v4 view_box = m_view_box;

	m_layout_count = 0;
	if (view_box == math::v4(0.0f, 0.0f, 0.0f, 0.0f))
		return;

	if (m_scroll_type == SCROLL_TYPE_VERTICAL) {
		math::v2 range = GetVisibleRange(1);
		Vector4 size = m_size.getImpl();
		float length = m_length;
		Vector2 visible = maxPerElem(
			Vector2(length + (float)size.getY(), length),
			minPerElem(Vector2(floatInVec(0.0f), size.getY()), range.getImpl()));
		float head = visible.getX();
		float tail = visible.getY();

		for (listview::CellMap::iterator it = m_cell_map->begin(); it != m_cell_map->end();
		     ++it) {
			listview::Cell *cell = *it;
			float pos = cell->m_effector_pos.extract_y();
			float half = cell->m_effector_value;

			if (pos - half <= head && pos + half >= tail) {
				anchor.m_pos = pos + half;
				anchor.m_segment = cell->GetSegment();
				anchor.m_cell = cell;
				break;
			}
		}
		for (listview::SegmentList::iterator it = m_segment_list->begin();
		     it != m_segment_list->end(); ++it) {
			listview::Segment *segment = it->get();

			if (!segment->m_synced)
				continue;

			float pos = segment->m_pos;

			if (tail > pos)
				return;

			math::v4 header_size = segment->m_header != NULL ?
						       *segment->m_header->GetSize(0) :
						       math::v4(Vector4(segment->m_header_size));
			Vector4 header;

			if (segment->m_cell_num == 0 && segment->m_header_disappearance)
				header = Vector4(0.0f);
			else
				header = header_size.getImpl();

			float header_length = header.getY();

			if (header_length != 0.0f && pos - header_length <= head) {
				if (anchor.m_segment == NULL ||
				    segment->m_index <= anchor.m_segment->m_index) {
					anchor.m_segment = segment;
					anchor.m_pos = pos;
					anchor.m_cell = NULL;
				}
				return;
			}
		}
	} else {
		math::v2 range = GetVisibleRange(1);
		Vector4 size = m_size.getImpl();
		float length = m_length;
		Vector2 visible =
			maxPerElem(Vector2(floatInVec(0.0f), size.getX()),
				   minPerElem(Vector2(length - (float)size.getX(), length),
					      range.getImpl()));
		float head = visible.getX();
		float tail = visible.getY();

		for (listview::CellMap::iterator it = m_cell_map->begin(); it != m_cell_map->end();
		     ++it) {
			listview::Cell *cell = *it;
			float pos = cell->m_effector_pos.extract_x();
			float half = cell->m_effector_value;

			if (pos + half >= head && pos - half <= tail) {
				anchor.m_pos = pos - half;
				anchor.m_segment = cell->GetSegment();
				anchor.m_cell = cell;
				break;
			}
		}
		for (listview::SegmentList::iterator it = m_segment_list->begin();
		     it != m_segment_list->end(); ++it) {
			listview::Segment *segment = it->get();
			float pos = segment->m_pos;

			if (tail < pos)
				return;

			math::v4 header_size = segment->m_header != NULL ?
						       *segment->m_header->GetSize(0) :
						       math::v4(Vector4(segment->m_header_size));
			Vector4 header;

			if (segment->m_cell_num == 0 && segment->m_header_disappearance)
				header = Vector4(0.0f);
			else
				header = header_size.getImpl();

			float header_length = header.getX();

			if (header_length != 0.0f && header_length + pos >= head) {
				if (anchor.m_segment == NULL ||
				    segment->m_index <= anchor.m_segment->m_index) {
					anchor.m_segment = segment;
					anchor.m_pos = pos;
					anchor.m_cell = NULL;
				}
				return;
			}
		}
	}
}


/* the segments laid out, the view kept on the cell it was on, the cells in range made and put
   in place and the new ones started; the syncs asked for meanwhile run after it */
void ListView::Sync()
{
	if (m_hold_count > 0)
		return;
	if (m_sync_block != 0) {
		m_sync_pending = true;
		return;
	}
	m_sync_pending = false;
	m_sync_block++;

	listview::SyncAnchor anchor;

	ScePafWidget_96B46364(anchor);
	SyncViewSize();
	ScePafWidget_B04BFBAB(anchor);
	if (m_drag_cell != NULL)
		m_drag_cell->UpdateDragRange();

	const listview::CoordinateEffector *effector = GetCoordinateEffector();
	math::v2 visible;
	math::v2 generate;

	visible = GetVisibleRange(0);
	generate = effector->ClampRange(GetGenerateRange(visible));

	ScePafWidget_433FAAD4(generate);
	for (listview::SegmentList::iterator it = m_segment_list->begin();
	     it != m_segment_list->end(); ++it) {
		listview::Segment *segment = it->get();
		int32_t first;
		int32_t last;

		segment->GetGenerationIndex(first, last, generate);
		segment->GenerateCell(first, last);
		segment->ResetCellPosition(*effector);
		segment->ResetHeaderPosition(visible, *effector);
		segment->m_synced = true;
	}
	m_effector = effector;
	for (listview::CellMap::iterator it = m_cell_map->begin(); it != m_cell_map->end(); ++it) {
		listview::Cell *cell = *it;

		if (!cell->m_started) {
			listview::ItemFactory::StartParam param;

			param.m_list_view = this;
			param.m_segment = cell->GetSegment()->m_index;
			param.m_cell = cell->m_index;
			param.m_item = cell->m_item;
			cell->m_started = true;
			m_item_factory->Start(param);
		}
	}
	if (m_cell_size_set != NULL)
		m_cell_size_set->clear();
	m_sync_block--;
	if (!m_sync_running) {
		m_sync_running = true;
		while (m_sync_pending && m_sync_block == 0 && m_hold_count <= 0)
			Sync();
		ReleaseTouchOutOfRange();
		m_sync_running = false;
	}
}

/* the padding at the head and the tail along the axis, the view laid out again and synced where
   it changed (name ours) */
void ListView::SetAxisPadding(const math::v2 &padding)
{
	Vector2 value = padding.getImpl();
	Vector4 now = m_padding.getImpl();
	math::v4 set;

	if (m_scroll_type == SCROLL_TYPE_VERTICAL)
		set = math::v4(Vector4(Vector2(now.getX(), value.getX()),
				       Vector2(now.getZ(), value.getY())));
	else
		set = math::v4(Vector4(Vector2(value.getX(), now.getY()),
				       Vector2(value.getY(), now.getW())));
	if (!(m_padding == set)) {
		SetPadding(set);
		UpdateScrollLayout();
		Sync();
	}
}

/* the padding at the head and the tail along the axis, where its animation goes where target is
   set and it runs (no name found) */
math::v2 ListView::ScePafWidget_B778A1D6(bool target)
{
	if (target) {
		AnimQueue *queue = FindAnim(ANIM_AXIS_PADDING);

		if (queue != NULL && queue->m_queue[0] != NULL) {
			math::v4 *value = queue->m_queue[0]->GetTargetV4();

			if (value != NULL)
				return math::v2(value->getImpl().getXY());
		}
	}

	Vector4 padding = m_padding.getImpl();

	if (m_scroll_type != SCROLL_TYPE_VERTICAL)
		return math::v2(Vector2(padding.getX(), padding.getZ()));
	return math::v2(Vector2(padding.getY(), padding.getW()));
}


namespace listview {

CellMap::CellMap(ListView *list_view) : m_list_view(list_view)
{
}

SegmentList::SegmentList(ListView *list_view) : m_list_view(list_view)
{
}

}

static inline listview::ListPos MakeListPos(int32_t segment, int32_t cell)
{
	listview::ListPos pos;

	pos.m_segment = segment;
	pos.m_cell = cell;
	return pos;
}

ListView::ListView(Widget *parent, CreateParam *param)
	: ScrollViewBase(parent, param), m_scroll_type(SCROLL_TYPE_VERTICAL),
	  m_generate_margin(0.0f, 0.0f), m_min_size(0.0f, 0.0f),
	  m_cell_size_default(s_cell_size_default), m_sync_block(0), m_syncing(0),
	  m_header_factory(NULL), m_layout_count(0), m_length(0.0f), m_cell_size_set(NULL),
	  m_first_header(NULL), m_drag_cell(NULL), m_effector_offset(0.0f), m_effector(NULL),
	  m_effector_enabled(true), m_scroll_range(0.0f, 0.0f), m_scroll_range_set(false),
	  m_snap_mode(SNAP_MODE_NONE), m_release_time(0), m_release_pos(MakeListPos(-1, -1)),
	  m_focus_pending(false),
	  m_focus_list_pos(MakeListPos(-1, -1)), m_grab_cell(NULL), m_sync_pending(false),
	  m_sync_running(false), m_focus_pos(-1.0f, -1.0f), m_sync_enabled(true)
{
	m_key_step = 2.2f;
	EnableOverScroll(false);
	m_segment_list = new listview::SegmentList(this);
	m_item_factory = new listview::ItemFactory;
	m_cell_map = new listview::CellMap(this);

	Focuser *focuser = new Focuser(ListViewFocusCB);

	delete m_focuser;
	m_focuser = focuser;
	if (focuser != NULL)
		focuser->BindWidget(this);
}


namespace listview {

/* placed where the list view puts it; its item moved there, animated where asked and it was
   not at the origin, unless it is dragged */
void Cell::SetPos(const math::v2 &pos, bool anim)
{
	if (m_pos == pos)
		return;

	bool move = false;

	if (anim)
		move = !(m_pos == math::v2(0.0f, 0.0f));
	m_pos = pos;
	if (GetListView()->m_drag_cell != this)
		ResetPos(move);
}

/* its header given back to the header factory where it has one and a size is given to it (no
   name found) */
void Segment::ScePafWidget_F53FD2D0()
{
	Widget *header = m_header;

	if (header == NULL || m_header_size == 0.0f)
		return;

	ListView *list_view = m_segment_list->m_list_view;
	SegmentHeaderFactory *factory = list_view->m_header_factory;

	if (factory != NULL) {
		SegmentHeaderFactory::DisposeParam param;

		param.m_list_view = list_view;
		param.m_segment = m_index;
		param.m_header = header;
		m_header = NULL;
		factory->Dispose(param);
	}
}

}


namespace listview {

Segment::~Segment()
{
	if (m_header != NULL) {
		ScePafWidget_F53FD2D0();
		if (m_header != NULL)
			m_header->DestroyWidget();
	}
	m_header = (Widget *)-1;
	if (m_cell_size_list != NULL)
		delete m_cell_size_list;
	m_cell_size_list = (CellSizeList *)-1;
	m_segment_list = (SegmentList *)-1;
}

/* the cell's place, or the end */
CellMap::iterator CellMap::Find(Cell *cell)
{
	iterator found = LowerBound(cell);

	if (found == end() || *found != cell)
		return end();
	return found;
}

/* the cell taken out, where it is there; what follows it */
CellMap::iterator CellMap::Erase(Cell *cell)
{
	iterator found = LowerBound(cell);
	iterator it = found;

	if (found == end() || *found != cell)
		it = end();
	if (it != end())
		it = erase(it);
	return it;
}

void Cell::EndDrag(int32_t index)
{
	m_item->EndDrag(index);
}

}


namespace listview {

/* the item let go of, the focus taken from it, the item stopped where it was started, the
   cell taken from the list view and the item given back to the item factory */
Cell::~Cell()
{
	ListView *list_view = GetListView();

	m_item->EndDrag(-1);

	ListItem *item = m_item;
	Widget *target = list_view->m_env->m_focus_cursor.m_target;

	if (list_view->m_grab_cell == this)
		list_view->m_grab_cell = NULL;
	if (item != NULL && item->FindChild(GetRootFocus(list_view), 0) != NULL &&
	    list_view->FindChild(target, 0) != NULL) {
		Widget::SetFocusInfo info;

		info.m_mode = 0;
		info.m_notify = false;
		info.m_direction = math::v2(0.0f, 0.0f);
		info.m_keep = false;
		static_cast<Widget *>(list_view)->SetFocus(&info);
	}
	if (m_item != NULL && m_item->FindChild(GetRootFocus(list_view), 0) != NULL)
		list_view->SetFocusRootState(0);
	if (m_started) {
		ItemFactory::StopParam param;

		param.m_list_view = list_view;
		param.m_segment = GetSegment()->m_index;
		param.m_cell = m_index;
		param.m_item = m_item;
		list_view->m_item_factory->Stop(param);
	}
	list_view->m_cell_map->Erase(this);
	m_item->m_cell = NULL;

	ItemFactory::DisposeParam param;

	param.m_list_view = list_view;
	param.m_segment = GetSegment()->m_index;
	param.m_cell = m_index;
	param.m_item = m_item;
	list_view->m_item_factory->Dispose(param);
	m_cell_list = (CellList *)-1;
	m_index = -1;
	m_item = (ListItem *)-1;
}

}

const char ListView::m_widget_type[] = "list_view";

bool ListView::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return ScrollViewBase::IsInherit(type);
}

char *ListView::GetTypeName() const
{
	return (char *)m_widget_type;
}

const char *ListView::TypeName()
{
	return m_widget_type;
}

namespace listview {

/* the cell of the segment and index, NULL for none */
Cell *CellMap::Find(int32_t segment, int32_t index)
{
	ListPos pos;

	pos.m_segment = segment;
	pos.m_cell = index;

	iterator it = lower_bound(begin(), end(), pos, CellLess());

	if (it == end() || CellLess()(pos, *it))
		return NULL;
	return *it;
}

}


/* the item of the cell the focus is to go to given the focus where it takes it, the view
   following it; the other items' focus requests cleared (no name found) */
void ListView::ScePafWidget_A838831E()
{
	listview::Cell *focused = NULL;

	if (m_focus_list_pos.m_segment >= 0) {
		focused = m_cell_map->Find(m_focus_list_pos.m_segment, m_focus_list_pos.m_cell);
		if (focused != NULL) {
			if (FindListItemFocusChild(focused->m_item, 1) == NULL) {
				Environment *env = m_env;

				env->m_front_touch_move = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
				env->m_rear_touch_move = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
				m_focus_pos = math::v2(-1.0f, -1.0f);
				env->m_focus_cursor_moving_only = true;
				env->m_rear_focus_cursor_moving_only = true;
			}

			math::v2 dir = m_focus_target;

			if (focused->m_item->ScePafWidget_C98D1926(dir)) {
				m_focus_start = m_focus_target;
				m_focus_target = math::v2(0.0f, 0.0f);
				m_grab_cell = focused;
				if (!(m_focus_pos == math::v2(-1.0f, -1.0f))) {
					m_focus_pos = math::v2(
						m_view_parent->GetPos(1, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f))
							.getImpl()
							.getXY());

					Environment *env = m_env;

					env->m_focus_cursor_moving_only = false;
					env->m_rear_focus_cursor_moving_only = false;
				}
			}
			if (!m_focus_in_group)
				ReleaseRootFocus(this);
		}
	}
	for (listview::CellMap::iterator it = m_cell_map->begin(); it != m_cell_map->end(); ++it) {
		if (*it != focused)
			(*it)->m_item->ScePafWidget_1F9DB221();
	}
}


/* the visible range as a rectangle, left, top, right and bottom, across the axis a line at the
   center, grown by 3 on each side (name ours) */
math::v4 ListView::GetVisibleRect(int32_t opt)
{
	math::v2 range = GetVisibleRange(opt);
	Vector2 head(range.getImpl().getX());
	Vector2 tail(range.getImpl().getY());
	Vector2 margin(0.0f);
	Vector2 left_top;
	Vector2 right_bottom;

	if (m_scroll_type == SCROLL_TYPE_VERTICAL) {
		left_top = Vector2(floatInVec(0.0f), (head - margin).getY());
		right_bottom = Vector2(floatInVec(0.0f), (tail + margin).getY());
	} else {
		left_top = Vector2((head + margin).getX(), floatInVec(0.0f));
		right_bottom = Vector2((tail - margin).getX(), floatInVec(0.0f));
	}
	return math::v4(Vector4(left_top, right_bottom) + Vector4(-3.0f, 3.0f, 3.0f, -3.0f));
}

/* the cells of the map in the rectangle, along the axis (vertical: top and bottom, else left and
   right): the first one and how many follow it; 0, -1 for none (name ours) */
static int32_t FindVisibleCells(listview::CellMap *map, const math::v4 &rect, bool vertical,
				listview::CellMap::iterator *first, int32_t *count)
{
	*count = 0;
	if (map->size() == 0)
		return -1;

	listview::CellMap::iterator it = map->begin();
	int32_t num;

	if (vertical) {
		float top = rect.getImpl().getY();
		float bottom = rect.getImpl().getW();
		bool found = false;

		num = 0;
		for (listview::CellMap::iterator cur = it; cur != map->end(); cur = ++it) {
			float cell_top = (*cur)->GetRect(false).getImpl().getY();
			math::v4 cell = (*it)->GetRect(false);

			if (top < cell_top) {
				if (!(cell.getImpl().getW() >= bottom)) {
					if (!found) {
						found = true;
						*first = it;
					}
					num++;
				} else if (num > 0) {
					break;
				}
			} else if (!(bottom > cell_top) && !(cell.getImpl().getW() < bottom)) {
				if (!found) {
					found = true;
					*first = it;
				}
				num++;
			} else if (num > 0) {
				break;
			}
		}
	} else {
		float left = rect.getImpl().getX();
		float right = rect.getImpl().getZ();
		bool found = false;

		num = 0;
		for (listview::CellMap::iterator cur = it; cur != map->end(); cur = ++it) {
			float cell_left = (*cur)->GetRect(false).getImpl().getX();
			math::v4 cell = (*it)->GetRect(false);

			if (cell_left < left) {
				if (!(right >= cell.getImpl().getZ())) {
					if (!found) {
						found = true;
						*first = it;
					}
					num++;
				} else if (num > 0) {
					break;
				}
			} else if (!(cell_left > right) && !(right < cell.getImpl().getZ())) {
				if (!found) {
					found = true;
					*first = it;
				}
				num++;
			} else if (num > 0) {
				break;
			}
		}
	}
	*count = num;
	return num != 0 ? 0 : -1;
}

__attribute__((noinline)) listview::ListPos ListView::FindFocusCell(const math::v2 &dir)
{
	listview::CellMap *map = m_cell_map;
	listview::ListPos pos;
	int32_t i = 0;

	if (map->size() == 0) {
		pos.m_segment = -1;
		pos.m_cell = -1;
		return pos;
	}

	listview::CellMap::iterator it = NULL;
	int32_t num;

	FindVisibleCells(map, GetVisibleRect(1), m_scroll_type == SCROLL_TYPE_VERTICAL, &it, &num);
	if (num <= 0) {
		pos.m_segment = -1;
		pos.m_cell = -1;
		return pos;
	}

	float d = dir.getImpl().getX();

	if (m_scroll_type == SCROLL_TYPE_VERTICAL)
		d = -dir.getImpl().getY();

	Widget *found = NULL;
	Widget *focus = FindRootFocus(this);

	if (d > 0.0f) {
		for (int32_t j = 0; j < num; j++)
			++it;
		for (; i < num; i++) {
			--it;
			if (focus == NULL || !ScePafWidget_DB0FFD33(focus) ||
			    (*it)->m_item->FindChild(focus, 0) != NULL) {
				found = FindListItemFocusChild((*it)->m_item, 0);
				if (found != NULL)
					break;
			}
		}
	} else {
		for (; i < num; i++) {
			if (focus == NULL || !ScePafWidget_DB0FFD33(focus) ||
			    (*it)->m_item->FindChild(focus, 0) != NULL) {
				found = FindListItemFocusChild((*it)->m_item, 0);
				if (found != NULL)
					break;
				++it;
			}
		}
	}
	if (found != NULL) {
		pos.m_segment = (*it)->GetSegment()->m_index;
		pos.m_cell = (*it)->m_index;
		return pos;
	}
	pos.m_segment = -1;
	pos.m_cell = -1;
	return pos;
}

void ListView::ScePafWidget_7F28F5B7()
{
	FindFocusCell(math::v2(0.0f, 0.0f));
	if (m_focus_root != NULL)
		m_focus_root->m_focus_moved = 0;
	m_focus_start = m_focus_target;
	m_focus_target = math::v2(0.0f, 0.0f);
	ScePafWidget_A838831E();
}

namespace listview {

/* the cells from the index on, num of them, destroyed */
void Segment::DestroyCell(int32_t index, uint32_t num)
{
	if (index < 0 || index >= m_cell_num)
		return;
	if (num > (uint32_t)(m_cell_num - index))
		num = m_cell_num - index;

	int32_t last = index + num;
	CellList::iterator it =
		lower_bound(m_cell_list.begin(), m_cell_list.end(), index, CellIndexLess());

	while (it != m_cell_list.end()) {
		if ((*it)->m_index >= last)
			break;
		it = m_cell_list.erase(it);
	}
}

}

void ListView::Rebuild()
{
	EndDrag(-1);
	while (m_cell_map->size() != 0) {
		listview::Cell *cell = *m_cell_map->begin();

		cell->GetSegment()->DestroyCell(cell->m_index, 0xffffffff);
	}
	ScePafWidget_7F28F5B7();
	ScrollViewBase::MoveViewParent(math::v2(0.0f, 0.0f), NULL);
	UpdateScrollLayout();
	Sync();
}

void ListView::SetScrollType(ScrollType type)
{
	if (m_scroll_type != type) {
		m_scroll_type = type;
		Rebuild();
	}
}

void ListView::SetGenerateMargin(const math::v2 &margin)
{
	if (!(m_generate_margin == margin)) {
		m_generate_margin = margin;
		Sync();
	}
}

/* the padding at the head and the tail along the axis set, animated by the timer where there
   is one (no name found) */
void ListView::ScePafWidget_5B94F2B4(const math::v2 &padding, Timer *timer)
{
	if (timer != NULL) {
		AnimListenerV4 *anim = new AnimListenerV4(
			timer, this,
			Vector4(ScePafWidget_B778A1D6(false).getImpl(), Vector2(0.0f)),
			Vector4(padding.getImpl(), Vector2(0.0f)), &s_axis_padding_prop, NULL, NULL);

		SetAnimListener(ANIM_AXIS_PADDING, anim, 2);
	} else {
		KillAnimType(ANIM_AXIS_PADDING);
		SetAxisPadding(padding);
	}
}

/* the cell is shown: across the whole of the visible range, or where it is not to be wholly
   shown, with one end in it (name ours) */
bool ListView::IsCellVisible(listview::Cell *cell, bool whole)
{
	if (cell == NULL || m_cell_map->size() == 0)
		return false;

	math::v4 visible = GetVisibleRect(1);

	if (m_scroll_type != SCROLL_TYPE_VERTICAL) {
		float left = visible.getImpl().getX();
		float right = visible.getImpl().getZ();
		float cell_left = cell->GetRect(false).getImpl().getX();
		float cell_right = cell->GetRect(false).getImpl().getZ();

		if (left < cell_left) {
			if (cell_right < right)
				return true;
		} else if (left <= cell_right && cell_right >= right) {
			return true;
		}
		if (!whole) {
			if (left < cell_right && cell_right <= right)
				return true;
			if (left <= cell_left && cell_left < right)
				return true;
		}
	} else {
		float top = visible.getImpl().getY();
		float bottom = visible.getImpl().getW();
		float cell_top = cell->GetRect(false).getImpl().getY();
		float cell_bottom = cell->GetRect(false).getImpl().getW();

		if (cell_top < top) {
			if (bottom < cell_bottom)
				return true;
		} else if (cell_bottom <= top && bottom >= cell_bottom) {
			return true;
		}
		if (!whole) {
			if (cell_bottom < top && bottom <= cell_bottom)
				return true;
			if (cell_top <= top && bottom < cell_top)
				return true;
		}
	}
	return false;
}

namespace listview {

Segment *SegmentList::GetSegment(int32_t index)
{
	if (index < 0 || index >= (int32_t)size())
		return NULL;
	return at(index).get();
}

}

Widget *ListView::GetSegmentHeader(int32_t segment)
{
	listview::Segment *seg = m_segment_list->GetSegment(segment);

	if (seg == NULL)
		return NULL;
	return seg->m_header;
}

namespace listview {

/* num cells inserted at the index: their sizes (the default one), the indices of the cells made
   after them, and the runs of cells given a size moved or grown; false for none */
bool Segment::InsertCell(int32_t index, uint32_t num)
{
	if (num == 0)
		return false;
	if (index < 0)
		index = 0;
	if (index > m_cell_num)
		index = m_cell_num;

	if (m_cell_size_list != NULL) {
		float size = m_cell_size_default.getImpl().getY();

		if (m_segment_list->m_list_view->m_scroll_type == ListView::SCROLL_TYPE_HORIZONTAL)
			size = m_cell_size_default.getImpl().getX();
		m_cell_size_list->m_total += size * (float)num;

		CellSize cell_size;

		cell_size.m_size = size;
		cell_size.m_pos = 0.0f;
		m_cell_size_list->insert(m_cell_size_list->begin() + index, num, cell_size);

		CellSizeList::iterator prev = m_cell_size_list->begin();

		if (index != 0)
			prev = m_cell_size_list->begin() + (index - 1);
		for (CellSizeList::iterator it = prev + 1; it != m_cell_size_list->end(); ++it, ++prev)
			it->m_pos = prev->m_pos + prev->m_size;
	}

	for (CellList::iterator it = lower_bound(m_cell_list.begin(), m_cell_list.end(), index,
						 CellIndexLess());
	     it != m_cell_list.end(); ++it)
		(*it)->m_index += num;
	m_cell_num += num;

	vector<SegmentCellRange> *set = m_segment_list->m_list_view->m_cell_size_set;

	if (set != NULL) {
		CellRange range;

		range.m_first = index;

		vector<SegmentCellRange>::iterator seg = lower_bound(set->begin(), set->end(), m_index);

		if (seg != set->end() && seg->m_segment == m_index) {
			vector<CellRange>::iterator it =
				lower_bound(seg->m_ranges.begin(), seg->m_ranges.end(), range.m_first,
					    CellRangeInsertLess());

			if (it == seg->m_ranges.end() || it->m_first > range.m_first) {
				for (vector<CellRange>::iterator next = it; next != seg->m_ranges.end();
				     ++next)
					next->m_first += num;
				range.m_num = num;
				seg->m_ranges.insert(it, range);
			} else {
				vector<CellRange>::iterator next = it;

				for (++next; next != seg->m_ranges.end(); ++next)
					next->m_first += num;
				it->m_num += num;
			}
		} else {
			{
				SegmentCellRange add;

				add.m_segment = m_index;
				seg = set->insert(seg, add);
			}
			range.m_num = num;
			seg->m_ranges.push_back(range);
		}
	}
	return true;
}

}

void ListView::InsertCell(int32_t segment, int32_t cell, uint32_t num)
{
	listview::Segment *seg = m_segment_list->GetSegment(segment);

	if (seg == NULL || !seg->InsertCell(cell, num))
		return;
	m_layout_count++;
	Sync();
	if (m_sync_block != 0)
		return;

	if (m_focus_list_pos.m_segment < 0 && segment == 0 && cell == 0) {
		m_focus_list_pos.m_segment = 0;
		m_focus_list_pos.m_cell = 0;
		if (m_focus_root != NULL)
			m_focus_root->m_focus_moved = 0;
		ScePafWidget_A838831E();
	}
	if (m_grab_cell != NULL) {
		int32_t grab_segment = m_grab_cell->GetSegment()->m_index;
		int32_t grab_cell = m_grab_cell->m_index;
		Widget *header = GetSegmentHeader(grab_segment);
		bool visible;

		if (grab_cell == -1 && ScePafWidget_DB0FFD33(header))
			visible = true;
		else
			visible = IsCellVisible(m_cell_map->Find(grab_segment, grab_cell), true);
		if (visible) {
			m_focus_list_pos.m_segment = grab_segment;
			m_focus_list_pos.m_cell = grab_cell;
		}
	}
	if (!IsCellVisible(m_grab_cell, true) && FindChild(m_env->m_focus_cursor.m_target, 0) != NULL)
		m_env->HideFocus(-1);
}

/* the place before the given one: the cell before it in its segment (a grid's only in its row),
   its header where there is one to go to, else the last cell (or header) of a segment before
   it that has one; -1, -1 for none (no name found) */
listview::ListPos ListView::ScePafWidget_8DC5A8AE(const listview::ListPos &pos, bool header)
{
	listview::Segment *seg = m_segment_list->GetSegment(pos.m_segment);

	if (seg == NULL)
		return MakeListPos(-1, -1);

	bool grid = seg->m_layout_type == LAYOUT_TYPE_GRID;
	float header_size = 0.0f;

	if (header) {
		if (m_scroll_type != SCROLL_TYPE_VERTICAL)
			header_size = seg->GetHeaderSize().getImpl().getX();
		else
			header_size = seg->GetHeaderSize().getImpl().getY();
	}
	if (pos.m_cell > 0 || (pos.m_cell == 0 && header_size != 0.0f)) {
		int32_t prev = pos.m_cell - 1;

		if (!grid || pos.m_cell / seg->m_column_num == prev / seg->m_column_num)
			return MakeListPos(pos.m_segment, prev);
		return MakeListPos(-1, -1);
	}
	if (grid)
		return MakeListPos(-1, -1);

	listview::Segment *it;

	if (!header) {
		for (it = m_segment_list->GetSegment(seg->m_index - 1); it != NULL;
		     it = m_segment_list->GetSegment(it->m_index - 1)) {
			if (it->m_cell_num != 0)
				return MakeListPos(it->m_index, it->m_cell_num - 1);
		}
	} else if (m_scroll_type == SCROLL_TYPE_VERTICAL) {
		for (it = m_segment_list->GetSegment(seg->m_index - 1); it != NULL;
		     it = m_segment_list->GetSegment(it->m_index - 1)) {
			if (it->m_view_size.getImpl().getY() != 0.0f)
				return MakeListPos(it->m_index, it->m_cell_num - 1);
		}
	} else {
		for (it = m_segment_list->GetSegment(seg->m_index - 1); it != NULL;
		     it = m_segment_list->GetSegment(it->m_index - 1)) {
			if (it->m_view_size.getImpl().getX() != 0.0f)
				return MakeListPos(it->m_index, it->m_cell_num - 1);
		}
	}
	return MakeListPos(-1, -1);
}

/* the place after the given one: the cell after it in its segment (a grid's only in its row),
   else the header or the first cell of a segment after it that has one; -1, -1 for none (no
   name found) */
listview::ListPos ListView::ScePafWidget_E60EAA9F(const listview::ListPos &pos, bool header)
{
	listview::Segment *seg = m_segment_list->GetSegment(pos.m_segment);

	if (seg == NULL)
		return MakeListPos(-1, -1);
	int32_t cell = pos.m_cell;

	if (cell < seg->m_cell_num - 1) {
		int32_t next = cell + 1;

		if (seg->m_layout_type != LAYOUT_TYPE_GRID ||
		    cell / seg->m_column_num == next / seg->m_column_num)
			return MakeListPos(pos.m_segment, next);
		return MakeListPos(-1, -1);
	}
	if (seg->m_layout_type == LAYOUT_TYPE_GRID)
		return MakeListPos(-1, -1);

	listview::Segment *it;

	if (header) {
		if (m_scroll_type == SCROLL_TYPE_VERTICAL) {
			for (it = m_segment_list->GetSegment(seg->m_index + 1); it != NULL;
			     it = m_segment_list->GetSegment(it->m_index + 1)) {
				if (it->m_view_size.getImpl().getY() != 0.0f) {
					float size = it->GetHeaderSize().getImpl().getY();

					return MakeListPos(it->m_index, size != 0.0f ? -1 : 0);
				}
			}
		} else {
			for (it = m_segment_list->GetSegment(seg->m_index + 1); it != NULL;
			     it = m_segment_list->GetSegment(it->m_index + 1)) {
				if (it->m_view_size.getImpl().getX() != 0.0f) {
					float size = it->GetHeaderSize().getImpl().getX();

					return MakeListPos(it->m_index, size != 0.0f ? -1 : 0);
				}
			}
		}
	} else {
		for (it = m_segment_list->GetSegment(seg->m_index + 1); it != NULL;
		     it = m_segment_list->GetSegment(it->m_index + 1)) {
			if (it->m_cell_num != 0)
				return MakeListPos(it->m_index, 0);
		}
	}
	return MakeListPos(-1, -1);
}

namespace listview {

/* num cells deleted from the index: their sizes, the cells made for them (the ones after them
   renumbered) and their part of the runs of cells given a size; false for none */
bool Segment::DeleteCell(int32_t index, uint32_t num)
{
	if (index < 0 || index >= m_cell_num)
		return false;
	if (num > (uint32_t)(m_cell_num - index))
		num = m_cell_num - index;
	if (num == 0)
		return false;

	if (m_cell_size_list != NULL) {
		CellSizeList::iterator first = m_cell_size_list->begin() + index;
		CellSizeList::iterator last = first + num;
		CellSizeList::iterator back = last;

		--back;
		float size = (back->m_pos + back->m_size) - first->m_pos;

		m_cell_size_list->m_total -= size;
		m_cell_size_list->erase(first, last);
		for (CellSizeList::iterator it = m_cell_size_list->begin() + index;
		     it != m_cell_size_list->end(); ++it)
			it->m_pos -= size;
	}

	int32_t last = index + num;

	for (CellList::iterator it = lower_bound(m_cell_list.begin(), m_cell_list.end(), index,
						 CellIndexLess());
	     it != m_cell_list.end();) {
		Cell *cell = it->get();

		if (cell->m_index < last) {
			it = m_cell_list.erase(it);
		} else {
			cell->m_index -= num;
			++it;
		}
	}
	m_cell_num -= num;

	vector<SegmentCellRange> *set = m_segment_list->m_list_view->m_cell_size_set;

	if (set != NULL) {
		vector<SegmentCellRange>::iterator seg = lower_bound(set->begin(), set->end(), m_index);

		if (seg != set->end() && seg->m_segment == m_index) {
			int32_t tail = index + num - 1;

			for (vector<CellRange>::iterator it = seg->m_ranges.begin();
			     it != seg->m_ranges.end();) {
				int32_t first = it->m_first;
				int32_t range_num = it->m_num;
				int32_t end = first + range_num;

				if (end < index) {
					++it;
				} else if (first > tail) {
					it->m_first = first - num;
					++it;
				} else {
					int32_t from = first > index ? first : index;

					if (end > tail)
						end = tail;
					it->m_num = range_num - (end - from) - 1;
					if (it->m_num != 0) {
						if (first > index)
							it->m_first -= first - index;
						++it;
					} else {
						it = seg->m_ranges.erase(it);
					}
				}
			}
		}
	}
	return true;
}

}

void ListView::DeleteCell(int32_t segment, int32_t cell, uint32_t num)
{
	listview::Segment *seg = m_segment_list->GetSegment(segment);

	if (seg == NULL)
		return;

	listview::Cell *found = m_cell_map->Find(segment, cell);

	if (found != NULL && found == m_drag_cell)
		OnCellUnGrab(m_drag_cell);
	if (!seg->DeleteCell(cell, num))
		return;
	if (m_sync_block == 0 && segment == m_focus_list_pos.m_segment) {
		int32_t focus = m_focus_list_pos.m_cell;

		if (cell <= focus) {
			if (focus >= (int32_t)(cell + num)) {
				m_focus_list_pos.m_cell = focus - num;
			} else {
				listview::ListPos pos;

				pos.m_segment = segment;
				pos.m_cell = cell - 1;

				listview::ListPos next = ScePafWidget_E60EAA9F(pos, false);

				m_focus_list_pos.m_segment = next.m_segment;
				m_focus_list_pos.m_cell = next.m_cell;
				if (next.m_segment < 0) {
					pos.m_segment = segment;
					pos.m_cell = cell;

					listview::ListPos prev = ScePafWidget_8DC5A8AE(pos, false);

					m_focus_list_pos.m_segment = prev.m_segment;
					m_focus_list_pos.m_cell = prev.m_cell;
				}
			}
			if (m_focus_root != NULL)
				m_focus_root->m_focus_moved = 0;
			ScePafWidget_A838831E();
		}
	}
	m_layout_count++;
	Sync();
}

namespace listview {

void CellMap::Sort()
{
	sort(begin(), end(), CellLess());
}

inline bool CellIndexLess::operator()(const common::SharedPtr<Cell> &a,
				      const common::SharedPtr<Cell> &b) const
{
	return a->m_index < b->m_index;
}

void CellList::Sort()
{
	sort(begin(), end(), CellIndexLess());
}

CellList::iterator CellList::Find(int32_t index)
{
	iterator it = lower_bound(begin(), end(), index, CellIndexLess());

	if (it == end() || (*it)->m_index != index)
		return end();
	return it;
}

Cell *CellList::GetCell(int32_t index)
{
	iterator it = Find(index);

	if (it == end())
		return NULL;
	return it->get();
}

}

namespace listview {

/* the cell at from moved to to (kept in the segment, set to where it went): the sizes, the
   indices of the cells between and the cell maps sorted again; false where it stays */
bool Segment::MoveCell(int32_t from, int32_t &to)
{
	if (from < 0 || from >= m_cell_num)
		return false;

	int32_t dst = to;
	int32_t last = m_cell_num - 1;

	if (dst < 0)
		dst = 0;
	if (last > dst)
		last = dst;
	to = last;
	if (from == last)
		return false;

	if (m_cell_size_list != NULL) {
		CellSize &a = m_cell_size_list->at(from);
		CellSize &b = m_cell_size_list->at(to);

		swap(a.m_size, b.m_size);

		CellSizeList::iterator first;
		CellSizeList::iterator last;

		if (from < to) {
			first = m_cell_size_list->begin() + from;
			last = m_cell_size_list->begin() + to;
		} else {
			first = m_cell_size_list->begin() + to;
			last = m_cell_size_list->begin() + from;
		}
		++last;

		CellSizeList::iterator it = first;

		for (++it; it != last; ++it, ++first)
			it->m_pos = first->m_pos + first->m_size;
	}

	CellList::iterator found = m_cell_list.Find(from);
	Cell *cell = NULL;

	if (found != m_cell_list.end())
		cell = found->get();
	if (from >= to) {
		for (CellList::iterator it = lower_bound(m_cell_list.begin(), m_cell_list.end(),
							 to, CellIndexLess());
		     it != m_cell_list.end(); ++it) {
			if ((*it)->m_index >= from)
				break;
			(*it)->m_index++;
		}
	} else {
		for (CellList::iterator it = lower_bound(m_cell_list.begin(), m_cell_list.end(),
							 from + 1, CellIndexLess());
		     it != m_cell_list.end(); ++it) {
			if ((*it)->m_index > to)
				break;
			(*it)->m_index--;
		}
	}
	if (cell != NULL)
		cell->m_index = to;
	m_cell_list.Sort();
	m_segment_list->m_list_view->m_cell_map->Sort();
	return true;
}

}

void ListView::MoveCell(int32_t segment, int32_t from, int32_t to)
{
	listview::Segment *seg = m_segment_list->GetSegment(segment);

	if (seg != NULL && seg->MoveCell(from, to)) {
		OnCellMoved(segment, from, to);
		Sync();
	}
}

uint32_t ListView::GetCellNum(int32_t segment)
{
	listview::Segment *seg = m_segment_list->GetSegment(segment);

	if (seg == NULL)
		return 0;
	return seg->m_cell_num;
}

/* the first segment header kept (no name found) */
void ListView::ScePafWidget_8AB4F072()
{
	for (listview::SegmentList::iterator it = m_segment_list->begin();
	     it != m_segment_list->end(); ++it) {
		Widget *header = (*it)->m_header;

		if (header != NULL) {
			m_first_header = header;
			return;
		}
	}
	m_first_header = NULL;
}

namespace listview {

CellList::CellList(Segment *segment) : m_segment(segment)
{
}

Segment::Segment(SegmentList *list, int32_t index)
	: m_segment_list(list), m_index(index), m_cell_num(0),
	  m_cell_size_default(list->m_list_view->m_cell_size_default),
	  m_layout_type(ListView::LAYOUT_TYPE_LIST), m_cell_list(this), m_cell_size_list(NULL),
	  m_view_size(0.0f, 0.0f, 0.0f, 0.0f), m_header(NULL), m_header_size(0.0f),
	  m_fixed_column_num(0), m_min_view_size(0.0f, 0.0f), m_joined(false), m_synced(false)
{
}

}

namespace listview {

/* num segments inserted at the index: the ones from it on renumbered (made where new), the
   segments of the cells given a size after it moved; false for none */
bool SegmentList::InsertSegment(int32_t index, uint32_t num)
{
	if (num == 0 || index < 0 || index > (int32_t)size())
		return false;

	SegmentList add(m_list_view);

	add.resize(num, common::SharedPtr<Segment>());
	insert(begin() + index, add.begin(), add.end());
	for (int32_t i = index; i < (int32_t)size(); i++) {
		common::SharedPtr<Segment> &segment = at(i);

		if (segment.get() != NULL)
			segment->m_index = i;
		else
			segment.reset(new Segment(this, i));
	}

	vector<SegmentCellRange> *set = m_list_view->m_cell_size_set;

	if (set != NULL) {
		for (vector<SegmentCellRange>::iterator it = set->begin(); it != set->end(); ++it) {
			if (index <= it->m_segment)
				it->m_segment += num;
		}
	}
	return true;
}

}

void ListView::InsertSegment(int32_t segment, uint32_t num)
{
	if (m_segment_list->InsertSegment(segment, num))
		ScePafWidget_8AB4F072();
}

/* the segment before the given one's: its header where headers are asked for and it takes
   room, else its first cell; -1, -1 for none (no name found) */
listview::ListPos ListView::ScePafWidget_814B970E(const listview::ListPos &pos, bool header)
{
	listview::Segment *seg = m_segment_list->GetSegment(pos.m_segment);

	if (seg == NULL)
		return MakeListPos(-1, -1);

	listview::Segment *it;

	if (!header) {
		for (it = m_segment_list->GetSegment(seg->m_index - 1); it != NULL;
		     it = m_segment_list->GetSegment(it->m_index - 1)) {
			if (it->m_cell_num != 0)
				return MakeListPos(it->m_index, 0);
		}
	} else if (m_scroll_type == SCROLL_TYPE_VERTICAL) {
		for (it = m_segment_list->GetSegment(seg->m_index - 1); it != NULL;
		     it = m_segment_list->GetSegment(it->m_index - 1)) {
			if (it->m_view_size.getImpl().getY() != 0.0f)
				return MakeListPos(it->m_index, -1);
		}
	} else {
		for (it = m_segment_list->GetSegment(seg->m_index - 1); it != NULL;
		     it = m_segment_list->GetSegment(it->m_index - 1)) {
			if (it->m_view_size.getImpl().getX() != 0.0f)
				return MakeListPos(it->m_index, -1);
		}
	}
	return MakeListPos(-1, -1);
}

/* the segment after the given one's: its header where headers are asked for and it takes
   room, else its first cell; -1, -1 for none (no name found) */
listview::ListPos ListView::ScePafWidget_DDC428E9(const listview::ListPos &pos, bool header)
{
	listview::Segment *seg = m_segment_list->GetSegment(pos.m_segment);

	if (seg == NULL)
		return MakeListPos(-1, -1);

	listview::Segment *it;

	if (!header) {
		for (it = m_segment_list->GetSegment(seg->m_index + 1); it != NULL;
		     it = m_segment_list->GetSegment(it->m_index + 1)) {
			if (it->m_cell_num != 0)
				return MakeListPos(it->m_index, 0);
		}
	} else if (m_scroll_type == SCROLL_TYPE_VERTICAL) {
		for (it = m_segment_list->GetSegment(seg->m_index + 1); it != NULL;
		     it = m_segment_list->GetSegment(it->m_index + 1)) {
			if (it->m_view_size.getImpl().getY() != 0.0f)
				return MakeListPos(it->m_index, -1);
		}
	} else {
		for (it = m_segment_list->GetSegment(seg->m_index + 1); it != NULL;
		     it = m_segment_list->GetSegment(it->m_index + 1)) {
			if (it->m_view_size.getImpl().getX() != 0.0f)
				return MakeListPos(it->m_index, -1);
		}
	}
	return MakeListPos(-1, -1);
}

namespace listview {

/* num segments deleted from the index, their cells with them: the ones after them renumbered,
   their cells given a size dropped and the later ones' moved; false for none */
bool SegmentList::DeleteSegment(int32_t index, uint32_t num)
{
	int32_t count = size();

	if (index < 0 || index >= count)
		return false;

	uint32_t left = count - index;

	if (num > left)
		num = left;
	if (num == 0)
		return false;

	iterator first = begin() + index;
	iterator last = first + num;

	for (iterator it = first; it != last; ++it)
		(*it)->DeleteCell(0, (*it)->m_cell_num);
	for (iterator it = last; it != end(); ++it)
		(*it)->m_index -= num;
	erase(first, last);

	vector<SegmentCellRange> *set = m_list_view->m_cell_size_set;

	if (set != NULL) {
		for (vector<SegmentCellRange>::iterator it = set->begin(); it != set->end();) {
			int32_t segment = it->m_segment;

			if (segment < index) {
				++it;
			} else if (segment >= (int32_t)(index + num)) {
				it->m_segment = segment - num;
				++it;
			} else {
				it = set->erase(it);
			}
		}
	}
	return true;
}

}

void ListView::DeleteSegment(int32_t segment, uint32_t num)
{
	if (!m_segment_list->DeleteSegment(segment, num))
		return;

	int32_t focus = m_focus_list_pos.m_segment;

	if (segment <= focus) {
		if (focus >= (int32_t)(segment + num)) {
			m_focus_list_pos.m_segment = focus - num;
		} else {
			listview::ListPos pos;

			pos.m_segment = segment - 1;
			pos.m_cell = -1;

			listview::ListPos next = ScePafWidget_DDC428E9(pos, false);

			m_focus_list_pos.m_segment = next.m_segment;
			m_focus_list_pos.m_cell = next.m_cell;
			if (next.m_segment == -1) {
				pos.m_segment = segment;
				pos.m_cell = -1;

				listview::ListPos prev = ScePafWidget_814B970E(pos, false);

				m_focus_list_pos.m_segment = prev.m_segment;
				m_focus_list_pos.m_cell = prev.m_cell;
			}
		}
		if (m_focus_root != NULL)
			m_focus_root->m_focus_moved = 0;
		ScePafWidget_A838831E();
	}
	ScePafWidget_8AB4F072();
	m_layout_count++;
	Sync();
}

uint32_t ListView::GetSegmentNum()
{
	return m_segment_list->size();
}

/* the cell size new segments start with (no name found) */
void ListView::ScePafWidget_FFD97FCD(const math::v4 &size)
{
	m_cell_size_default = size;
}

/* the cell size new segments start with (no name found) */
math::v4 ListView::ScePafWidget_C6559467()
{
	return m_cell_size_default;
}

void ListView::SetCellSizeDefault(int32_t segment, const math::v4 &size)
{
	listview::Segment *seg = m_segment_list->GetSegment(segment);

	if (seg == NULL)
		return;

	math::v4 value = maxPerElem(size.getImpl(), Vector4(1.0f));

	if (!(value == seg->m_cell_size_default)) {
		seg->m_cell_size_default = value;
		m_layout_count++;
		m_syncing++;
		Sync();
		m_syncing--;
	}
}

math::v4 ListView::GetCellSizeDefault(int32_t segment)
{
	listview::Segment *seg = m_segment_list->GetSegment(segment);

	if (seg == NULL)
		return math::v4(0.0f, 0.0f, 0.0f, 0.0f);
	return seg->m_cell_size_default;
}

namespace listview {

/* the size of a variable layout's cell along the axis set, the ones after it moved; false where
   it is not variable or the size stays */
bool Segment::SetCellSize(int32_t index, const math::v4 &size)
{
	if (index < 0 || index >= m_cell_num)
		return false;

	CellSizeList *sizes = m_cell_size_list;

	if (sizes == NULL)
		return false;

	float value = size.getImpl().getY();

	if (m_segment_list->m_list_view->m_scroll_type == ListView::SCROLL_TYPE_HORIZONTAL)
		value = size.getImpl().getX();

	CellSizeList::iterator it = sizes->begin() + index;
	CellSizeList::iterator prev = it;

	if (it->m_size == value)
		return false;
	sizes->m_total = (value - it->m_size) + sizes->m_total;
	it->m_size = value;
	for (++it; it != sizes->end(); ++it, ++prev)
		it->m_pos = prev->m_pos + prev->m_size;
	return true;
}

}

void ListView::SetCellSize(int32_t segment, int32_t cell, const math::v4 &size)
{
	listview::Segment *seg = m_segment_list->GetSegment(segment);

	if (seg != NULL && seg->SetCellSize(cell, size)) {
		m_layout_count++;
		m_syncing++;
		Sync();
		m_syncing--;
	}
}

math::v4 ListView::GetCellSize(int32_t segment, int32_t cell)
{
	listview::Segment *seg = m_segment_list->GetSegment(segment);

	if (seg == NULL)
		return math::v4(0.0f, 0.0f, 0.0f, 0.0f);
	return seg->GetCellSize(cell);
}

namespace listview {

void Cell::FreeCellPosition(bool on_off)
{
	if (m_free_position != on_off) {
		m_free_position = on_off;
		if (!on_off)
			ResetPos(true);
	}
}

}

void ListView::FreeCellPosition(int32_t segment, int32_t cell, bool on_off)
{
	listview::Cell *found = m_cell_map->Find(segment, cell);

	if (found != NULL)
		found->FreeCellPosition(on_off);
}

namespace listview {

/* its item moved to where the list view places it, by the timer (name ours) */
__attribute__((noinline)) void Cell::MoveItem(Timer *timer)
{
	m_item->KillAnim(Widget::ANIM_MOVE);
	m_item->SetPos(math::v4(Vector4(m_pos.getImpl(), Vector2(0.0f))), timer, Widget::ANIM_MOVE);
}

}

/* the item of the cell moved to where the list view places it, by the timer; the timer
   deleted where there is no such cell (no name found) */
void ListView::ScePafWidget_E16070BC(int32_t segment, int32_t cell, Timer *timer)
{
	listview::Cell *found = m_cell_map->Find(segment, cell);

	if (found != NULL)
		found->MoveItem(timer);
	else if (timer != NULL)
		delete timer;
}

namespace listview {

/* the layout set: a variable layout's cell sizes made from the default one, dropped for the
   others; false where it stays */
bool Segment::SetLayoutType(ListView::LayoutType type)
{
	if (m_layout_type == type)
		return false;
	m_layout_type = type;
	if (type == ListView::LAYOUT_TYPE_LIST_VARIABLE) {
		if (m_cell_size_list == NULL)
			m_cell_size_list = new CellSizeList;
		m_cell_size_list->resize(m_cell_num);

		int32_t num = m_cell_num;
		CellSizeList *sizes = m_cell_size_list;
		float size = m_cell_size_default.getImpl().getY();

		if (m_segment_list->m_list_view->m_scroll_type == ListView::SCROLL_TYPE_HORIZONTAL)
			size = m_cell_size_default.getImpl().getX();

		float pos = 0.0f;

		for (CellSizeList::iterator it = sizes->begin(); it != sizes->end(); ++it) {
			it->m_size = size;
			it->m_pos = pos;
			pos = pos + size;
		}
		sizes->m_total = size * (float)num;
	} else if (m_cell_size_list != NULL) {
		delete m_cell_size_list;
		m_cell_size_list = NULL;
	}
	return true;
}

}

void ListView::SetSegmentLayoutType(int32_t segment, LayoutType type)
{
	listview::Segment *seg = m_segment_list->GetSegment(segment);

	if (seg != NULL && seg->SetLayoutType(type))
		Sync();
}

ListView::LayoutType ListView::GetSegmentLayoutType(int32_t segment)
{
	listview::Segment *seg = m_segment_list->GetSegment(segment);

	if (seg == NULL)
		return LAYOUT_TYPE_LIST;
	return seg->m_layout_type;
}

/* the columns a grid segment is given, 0 to fit the view (no name found) */
void ListView::ScePafWidget_27EB2AEB(int32_t segment, int32_t num)
{
	listview::Segment *seg = m_segment_list->GetSegment(segment);

	if (seg != NULL && seg->m_fixed_column_num != num) {
		seg->m_fixed_column_num = num;
		m_layout_count++;
		Sync();
	}
}

/* the least size a segment takes (no name found) */
void ListView::ScePafWidget_7E9DC5B1(int32_t segment, const math::v2 &size)
{
	listview::Segment *seg = m_segment_list->GetSegment(segment);

	if (seg != NULL && !(seg->m_min_view_size == size)) {
		seg->m_min_view_size = size;
		m_layout_count++;
		Sync();
	}
}

/* the least size a segment takes (no name found) */
math::v2 ListView::ScePafWidget_1568FEF6(int32_t segment)
{
	listview::Segment *seg = m_segment_list->GetSegment(segment);

	if (seg == NULL)
		return math::v2(0.0f, 0.0f);
	return seg->m_min_view_size;
}

/* a segment joined to the segments next to it (name ours) */
void ListView::SetSegmentJoined(int32_t segment, bool joined)
{
	listview::Segment *seg = m_segment_list->GetSegment(segment);

	if (seg != NULL && seg->m_joined != joined)
		seg->m_joined = joined;
}

void ListView::SetSegmentHeader(int32_t segment, Widget *header, bool floating,
				bool disappearance)
{
	listview::Segment *seg = m_segment_list->GetSegment(segment);

	if (seg == NULL)
		return;
	if (seg->m_header_size != 0.0f) {
		seg->ScePafWidget_F53FD2D0();
		seg->m_header_size = 0.0f;
	}
	if (seg->m_header == header)
		return;
	if (seg->m_header != NULL)
		seg->m_header->DestroyWidget();
	seg->m_header = header;
	seg->m_header_floating = floating;
	seg->m_header_disappearance = disappearance;
	seg->m_header_visible = true;
	if (header != NULL) {
		header->SetParent(m_view_parent, NULL, 0);

		Widget *added = seg->m_header;

		for (int32_t i = segment + 1; i < (int32_t)m_segment_list->size(); i++) {
			Widget *next = m_segment_list->at(i)->m_header;

			if (next != NULL) {
				next->SetForward(added);
				break;
			}
		}
	}
	ScePafWidget_8AB4F072();
	m_layout_count++;
	Sync();
}

/* a segment given a header made by the header factory, of the size along the axis (no name
   found) */
void ListView::ScePafWidget_7AC07A92(int32_t segment, const math::v4 &size, bool floating,
				     bool disappearance)
{
	listview::Segment *seg = m_segment_list->GetSegment(segment);

	if (seg == NULL)
		return;
	if (seg->m_header_size == 0.0f && seg->m_header != NULL) {
		seg->m_header->DestroyWidget();
		seg->m_header = NULL;
	}

	float value = size.getImpl().getX();

	if (m_scroll_type == SCROLL_TYPE_VERTICAL)
		value = size.getImpl().getY();
	seg->m_header_size = value;
	seg->m_header_floating = floating;
	seg->m_header_disappearance = disappearance;
	seg->m_header_visible = true;
	Sync();
}

/* the size a segment's header takes (no name found) */
math::v4 ListView::ScePafWidget_89DDBD65(int32_t segment)
{
	listview::Segment *seg = m_segment_list->GetSegment(segment);

	if (seg == NULL)
		return math::v4(0.0f, 0.0f, 0.0f, 0.0f);
	return seg->GetHeaderSize();
}

/* the factory that makes the segments' headers: the headers the old one made disposed of, and
   it deleted (no name found) */
void ListView::ScePafWidget_9556E18A(listview::SegmentHeaderFactory *factory)
{
	if (m_header_factory != NULL) {
		for (listview::SegmentList::iterator it = m_segment_list->begin();
		     it != m_segment_list->end(); ++it)
			(*it)->ScePafWidget_F53FD2D0();
		if (m_header_factory != NULL)
			delete m_header_factory;
	}
	m_header_factory = factory;
	ScePafWidget_8AB4F072();
	m_layout_count++;
	Sync();
}

void ListView::SetItemFactory(listview::ItemFactory *factory)
{
	if (m_item_factory != NULL)
		delete m_item_factory;
	if (factory == NULL)
		factory = new listview::ItemFactory;
	m_item_factory = factory;
	Rebuild();
}

ListItem *ListView::GetListItem(int32_t segment, int32_t cell)
{
	listview::Cell *found = m_cell_map->Find(segment, cell);

	if (found == NULL)
		return NULL;
	return found->m_item;
}

int32_t ListView::GetGeneratedNum()
{
	return m_cell_map->size();
}

listview::ListPos ListView::GetGeneratedPos(int32_t index)
{
	if (index < 0 || (uint32_t)index >= m_cell_map->size())
		return MakeListPos(-1, -1);

	listview::Cell *cell = m_cell_map->at(index);

	return MakeListPos(cell->GetSegment()->m_index, cell->m_index);
}

ListItem *ListView::ScePafWidget_E744731A(int32_t index)
{
	int32_t n = 0;
	math::v2 range = GetVisibleRange(0);

	if (m_scroll_type != SCROLL_TYPE_VERTICAL) {
		for (listview::CellMap::iterator cur = m_cell_map->begin(), it = cur;
		     cur != m_cell_map->end(); cur = ++it) {
			math::v4 rect = (*it)->GetRect(false);

			if (rect.getImpl().getZ() >= range.getImpl().getX() &&
			    range.getImpl().getY() >= rect.getImpl().getX()) {
				if (n == index)
					return (*it)->m_item;
				n++;
			}
		}
	} else {
		for (listview::CellMap::iterator cur = m_cell_map->begin(), it = cur;
		     cur != m_cell_map->end(); cur = ++it) {
			math::v4 rect = (*it)->GetRect(false);

			if (range.getImpl().getX() >= rect.getImpl().getW() &&
			    rect.getImpl().getY() >= range.getImpl().getY()) {
				if (n == index)
					return (*it)->m_item;
				n++;
			}
		}
	}
	return NULL;
}

common::property_list::PropertyList *ListView::GetSegmentPropertyList(int32_t segment)
{
	listview::Segment *seg = m_segment_list->GetSegment(segment);

	if (seg == NULL)
		return &m_property_list;
	return &seg->m_property_list;
}

void ListView::BlockSync()
{
	m_sync_block++;
}

void ListView::UnblockSync()
{
	if (--m_sync_block == 0)
		Sync();
}

void ListView::Clear()
{
	DeleteSegment(0, m_segment_list->size());
}

namespace listview {

/* the drag of its item begun: the item put behind the first header, or on top of its parent */
bool Cell::BeginDrag(int32_t index)
{
	if (!m_item->BeginDrag(index))
		return false;

	Widget *header = GetListView()->m_first_header;

	if (header != NULL)
		m_item->SetBehind(header);
	else
		m_item->SetParent(m_item->m_parent, NULL, 6);
	UpdateDragRange();
	return true;
}

}

void ListView::BeginDrag(int32_t segment, int32_t cell, int32_t index)
{
	EndDrag(-1);

	listview::Cell *found = m_cell_map->Find(segment, cell);

	if (found == NULL)
		return;

	Widget *root = ScePafWidget_CFA1EB7A(this);
	uint32_t focus = 0;

	if (root != NULL && root->m_focus_root != NULL)
		focus = root->m_focus_root->m_state;
	if (found->BeginDrag(index)) {
		ScePafWidget_1E6F658F();
		m_drag_cell = found;
		m_drag_item_pos = math::v2(found->m_item->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f))
					      .getImpl()
					      .getXY());
		OnCellDragStart(segment, cell);
	} else if (focus != 0) {
		m_drag_cell = found;
		OnCellDragStart(segment, cell);
		m_drag_begun = true;
		m_focus_direction_off = false;
		Widget::m_focus_target = false;
	}
}

void ListView::EndDrag(int32_t index)
{
	listview::Cell *cell = m_drag_cell;

	if (cell != NULL) {
		cell->m_item->EndDrag(index);
		OnCellUnGrab(m_drag_cell);
	}
}

int32_t ListView::ScePafWidget_B3B40B75(math::v2 *pos, int32_t segment, int32_t cell,
				       FocusAlignType align)
{
	listview::Segment *seg = m_segment_list->GetSegment(segment);

	if (seg == NULL)
		return -1;

	float size;
	math::v2 cell_pos = seg->ScePafWidget_7A37CEDA(cell, align, &size);
	math::v4 view = m_size;

	if (m_scroll_type == SCROLL_TYPE_VERTICAL) {
		float height = view.getImpl().getY();
		float half = height * 0.5f;
		float p = cell_pos.getImpl().getY();

		if (align == FOCUS_ALIGN_TYPE_HEAD) {
			p = p - half;
		} else if (align == FOCUS_ALIGN_TYPE_TAIL) {
			p = half + p;
		} else if (align == FOCUS_ALIGN_TYPE_NEAR) {
			float ratio = 0.8f;

			if (size > 100.0f)
				ratio = 0.6f;
			if (height * ratio > size) {
				float center = p;
				float margin = height * ratio * 0.5f;
				math::v4 parent = m_view_parent->GetPos(1, NULL,
									 math::v4(0.0f, 0.0f, 0.0f, 0.0f));
				math::v4 box = m_scroll_box;
				float now = -parent.getImpl().getY();
				float tail = center + size * 0.5f;
				float head = center - size * 0.5f;
				float to;

				if (tail <= margin - parent.getImpl().getY()) {
					to = now;
					if (head < -parent.getImpl().getY() - margin)
						to = head + margin;
				} else {
					to = tail - margin;
				}

				float low = to;
				float high = m_scroll_box.getImpl().getY() - height * 0.5f;

				if (box.getImpl().getW() + height * 0.5f > to)
					low = box.getImpl().getW() + height * 0.5f;
				if (high > low)
					high = low;
				p = high;

				math::v2 near;
				math::v2 focus = math::v2(mulPerElem(Vector2(0.0f, -1.0f), Vector2(high)));

				if (ScePafWidget_C8C679BB(near, focus) &&
				    __builtin_fabsf(high - now) > __builtin_fabsf(near.getImpl().getY() - now))
					p = near.getImpl().getY();
			}
		}

		float high = m_scroll_box.getImpl().getY() - height * 0.5f;

		if (m_scroll_box.getImpl().getW() + height * 0.5f > p)
			p = m_scroll_box.getImpl().getW() + height * 0.5f;
		if (high > p)
			high = p;
		*pos = math::v2(0.0f, high);
	} else {
		float width = view.getImpl().getX();
		float half = width * 0.5f;
		float p = cell_pos.getImpl().getX();

		if (align == FOCUS_ALIGN_TYPE_HEAD) {
			p = half + p;
		} else if (align == FOCUS_ALIGN_TYPE_TAIL) {
			p = p - half;
		} else if (align == FOCUS_ALIGN_TYPE_NEAR) {
			float ratio = 0.8f;

			if (size > 100.0f)
				ratio = 0.6f;
			if (width * ratio > size) {
				float center = p;
				float margin = width * ratio * 0.5f;
				math::v4 parent = m_view_parent->GetPos(1, NULL,
									 math::v4(0.0f, 0.0f, 0.0f, 0.0f));
				math::v4 box = m_scroll_box;
				float now = -parent.getImpl().getX();
				float head = center - size * 0.5f;
				float tail = center + size * 0.5f;
				float to;

				if (head >= -parent.getImpl().getX() - margin) {
					to = now;
					if (tail > margin - parent.getImpl().getX())
						to = tail - margin;
				} else {
					to = head + margin;
				}

				float low = to;
				float high = m_scroll_box.getImpl().getZ() - width * 0.5f;

				if (box.getImpl().getX() + width * 0.5f > to)
					low = box.getImpl().getX() + width * 0.5f;
				if (high > low)
					high = low;
				p = high;

				math::v2 near;
				math::v2 focus = math::v2(mulPerElem(Vector2(-1.0f, 0.0f), Vector2(high)));

				if (ScePafWidget_C8C679BB(near, focus) &&
				    __builtin_fabsf(high - now) > __builtin_fabsf(near.getImpl().getX() - now))
					p = near.getImpl().getX();
			}
		}

		float high = m_scroll_box.getImpl().getZ() - width * 0.5f;

		if (m_scroll_box.getImpl().getX() + width * 0.5f > p)
			p = m_scroll_box.getImpl().getX() + width * 0.5f;
		if (high > p)
			high = p;
		*pos = math::v2(high, 0.0f);
	}
	return 0;
}

void ListView::SetFocus(int32_t segment, int32_t cell, FocusAlignType align, Timer *timer)
{
	math::v2 pos;

	if (ScePafWidget_B3B40B75(&pos, segment, cell, align) < 0) {
		if (timer != NULL)
			delete timer;
		return;
	}
	if (align == FOCUS_ALIGN_TYPE_NEAR) {
		if (pos == GetViewFocusTarget()) {
			if (timer != NULL)
				delete timer;
		} else {
			float size = 0.0f;
			listview::Segment *seg = m_segment_list->GetSegment(segment);

			if (seg != NULL)
				seg->ScePafWidget_7A37CEDA(cell, align, &size);

			float d;

			if (m_scroll_type != SCROLL_TYPE_VERTICAL)
				d = (pos - GetViewFocusTarget()).getImpl().getX();
			else
				d = (pos - GetViewFocusTarget()).getImpl().getY();
			if (__builtin_fabsf(d) > size * 0.75f && m_focus_list_pos.m_segment == segment) {
				if (m_focus_timer == NULL ||
				    m_focus_timer->m_type != ScrollViewTimer::TIMER_TYPE_MOTION ||
				    m_focus_pos == math::v2(-1.0f, -1.0f)) {
					m_focus_pos = math::v2(
						m_view_parent->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f))
							.getImpl()
							.getXY());

					Environment *env = m_env;

					env->m_focus_cursor_moving_only = false;
					env->m_rear_focus_cursor_moving_only = false;
				}
			} else {
				m_focus_pos = math::v2(-1.0f, -1.0f);

				Environment *env = m_env;

				env->m_focus_cursor_moving_only = true;
				env->m_rear_focus_cursor_moving_only = true;
			}
			SetViewFocus(pos, timer, true, true);
		}
	} else {
		SetViewFocus(pos, timer, true, false);
	}
	m_focus_list_pos.m_segment = segment;
	m_focus_list_pos.m_cell = cell;
	if (m_focus_root != NULL)
		m_focus_root->m_focus_moved = 0;
	ScePafWidget_A838831E();
}

void ListView::SetViewFocus(const math::v2 &pos, Timer *timer, bool flag_a, bool flag_b)
{
	ScrollViewBase::SetViewFocus(pos, timer, flag_a, flag_b);
	Widget *header = GetSegmentHeader(m_focus_list_pos.m_segment);

	if (m_focus_list_pos.m_cell == -1 && ScePafWidget_DB0FFD33(header))
		return;
	if (IsCellVisible(m_cell_map->Find(m_focus_list_pos.m_segment, m_focus_list_pos.m_cell), true))
		return;

	listview::CellMap::iterator it = NULL;
	int32_t num = 0;

	FindVisibleCells(m_cell_map, GetVisibleRect(1), m_scroll_type == SCROLL_TYPE_VERTICAL, &it,
			 &num);
	if (num == 0)
		return;

	ListItem *item = (*it)->m_item;
	int32_t seg = item->GetSegmentIndex();
	int32_t cell = item->GetCellIndex();

	if (FindListItemFocusChild(item, 0) != NULL) {
		if (seg > m_focus_list_pos.m_segment) {
			m_focus_list_pos.m_segment = seg;
			m_focus_list_pos.m_cell = cell;
			goto moved;
		}
		if (cell > m_focus_list_pos.m_cell) {
			m_focus_list_pos.m_segment = seg;
			m_focus_list_pos.m_cell = cell;
			goto moved;
		}
	}
	for (int32_t i = 0; i < num - 1; i++)
		++it;
	item = (*it)->m_item;
	if (FindListItemFocusChild(item, 0) != NULL) {
		m_focus_list_pos.m_segment = item->GetSegmentIndex();
		m_focus_list_pos.m_cell = item->GetCellIndex();
	}
moved:
	if (m_focus_root != NULL)
		m_focus_root->m_focus_moved = 0;
	ScePafWidget_A838831E();
}

void ListView::ScePafWidget_E1824581(bool on, const math::v2 &range)
{
	if (on) {
		ScePafWidget_9D82904C(false);
		EnableOverScroll(false);
		m_scroll_range_set = true;
		m_scroll_range = range;
	} else {
		m_scroll_range_set = false;
		m_scroll_range = math::v2(0.0f, 0.0f);
	}
}

void ListView::ScePafWidget_9D82904C(bool on)
{
	if (on) {
		m_scroll_range_set = false;
		m_scroll_range = math::v2(0.0f, 0.0f);
		EnableOverScroll(false);
		m_effector_enabled = true;
	} else {
		m_effector_enabled = false;
	}
}

void ListView::ScePafWidget_40347A42(bool on)
{
	if (on) {
		if (m_cell_size_set == NULL)
			m_cell_size_set = new vector<listview::SegmentCellRange>();
	} else if (m_cell_size_set != NULL) {
		delete m_cell_size_set;
		m_cell_size_set = NULL;
	}
}

void ListView::ScePafWidget_98FD53BF(const math::v2 &size)
{
	if (!(m_min_size == size)) {
		m_min_size = size;
		Sync();
	}
}

namespace listview {

/* the cell at the position, clamped to the segment's cells; -1 for none */
int32_t Segment::GetCellIndex(const math::v2 &pos)
{
	if (m_cell_num == 0)
		return -1;
	if (m_segment_list->m_list_view->m_scroll_type != ListView::SCROLL_TYPE_VERTICAL) {
		float head = GetHeaderSize().getImpl().getX();
		math::v2 p = pos;
		float d = p.getImpl().getX() - (m_pos + head);

		if (m_layout_type == ListView::LAYOUT_TYPE_LIST_VARIABLE)
			return m_cell_size_list->Find(d) - m_cell_size_list->begin();

		math::v4 size = m_cell_size_default;
		float width = size.getImpl().getX();
		float height = size.getImpl().getY();
		int32_t last = m_cell_num - 1;
		int32_t col = (int32_t)(d / width);

		if (m_layout_type != ListView::LAYOUT_TYPE_LIST) {
			int32_t row = (int32_t)(((float)m_column_num * height * 0.5f -
						 p.getImpl().getY()) / height);
			int32_t cols = m_row_num;

			if (col < 0)
				col = 0;
			if (cols > col)
				cols = col;
			if (row < 0)
				row = 0;

			int32_t rows = m_column_num;

			if (rows > row)
				rows = row;

			int32_t index = rows + cols * m_column_num;

			if (index < 0)
				index = 0;
			if (last > index)
				last = index;
			return last;
		}
		if (col < 0)
			col = 0;
		if (last > col)
			last = col;
		return last;
	} else {
		float head = GetHeaderSize().getImpl().getY();
		math::v2 p = pos;
		float d = (m_pos - head) - p.getImpl().getY();

		if (m_layout_type == ListView::LAYOUT_TYPE_LIST_VARIABLE)
			return m_cell_size_list->Find(d) - m_cell_size_list->begin();

		math::v4 size = m_cell_size_default;
		float width = size.getImpl().getX();
		float height = size.getImpl().getY();
		int32_t last = m_cell_num - 1;
		int32_t row = (int32_t)(d / height);

		if (m_layout_type != ListView::LAYOUT_TYPE_LIST) {
			int32_t col = (int32_t)((p.getImpl().getX() -
						 (float)m_column_num * width * -0.5f) / width);
			int32_t rows = m_row_num;

			if (row < 0)
				row = 0;
			if (rows > row)
				rows = row;
			if (col < 0)
				col = 0;

			int32_t cols = m_column_num;

			if (cols > col)
				cols = col;

			int32_t index = cols + rows * m_column_num;

			if (index < 0)
				index = 0;
			if (last > index)
				last = index;
			return last;
		}
		if (row < 0)
			row = 0;
		if (last > row)
			last = row;
		return last;
	}
}

}

/* the segment and cell at the position; cell -1 on a segment's header, both -1 for none (no
   name found) */
listview::ListPos ListView::ScePafWidget_FD8DE7ED(const math::v2 &pos)
{
	listview::ListPos found;
	math::v2 p = pos;
	if (m_scroll_type != SCROLL_TYPE_VERTICAL) {
		float x = p.getImpl().getX();
		listview::SegmentList::iterator end = m_segment_list->end();

		for (listview::SegmentList::iterator it = m_segment_list->begin(); it != end; ++it) {
			listview::Segment *seg = it->get();

			if (seg->m_pos <= x && x < seg->m_pos + seg->m_view_size.getImpl().getX()) {
				if (seg->m_pos + seg->GetHeaderSize().getImpl().getX() <= x) {
					found.m_segment = seg->m_index;
					found.m_cell = seg->GetCellIndex(pos);
					return found;
				}
				found.m_segment = seg->m_index;
				found.m_cell = -1;
				return found;
			}
		}
	} else {
		float y = p.getImpl().getY();
		listview::SegmentList::iterator end = m_segment_list->end();

		for (listview::SegmentList::iterator it = m_segment_list->begin(); it != end; ++it) {
			listview::Segment *seg = it->get();

			if (seg->m_pos >= y && y > seg->m_pos - seg->m_view_size.getImpl().getY()) {
				if (seg->m_pos - seg->GetHeaderSize().getImpl().getY() >= y) {
					found.m_segment = seg->m_index;
					found.m_cell = seg->GetCellIndex(pos);
					return found;
				}
				found.m_segment = seg->m_index;
				found.m_cell = -1;
				return found;
			}
		}
	}
	found.m_segment = -1;
	found.m_cell = -1;
	return found;
}

/* the first child of a cell's item the focus may go to, NULL for none (no name found) */
Widget *ListView::ScePafWidget_20824179()
{
	Widget *found = NULL;

	for (listview::CellMap::iterator it = m_cell_map->begin(); it != m_cell_map->end(); ++it) {
		found = FindListItemFocusChild((*it)->m_item, 2);
		if (found != NULL)
			break;
	}
	return found;
}

void ListView::DestroyWidget()
{
	DeleteSegment(0, m_segment_list->size());
	ScrollViewBase::DestroyWidget();
}

void ListView::UpdateSize()
{
	Sync();
	ScrollViewBase::UpdateSize();
}

/* the long press text auto scroll the touch of the event holds handed from the button or text it
   captures to the item's text auto scroll target, from the origin it scrolled at (name ours) */
static void PassTextAutoScroll(ListView *view, Event *e, ListItem *item)
{
	uint32_t detail = e->m_detail;
	uint32_t id = (uint8_t)detail;
	uint32_t port = (detail >> 20) & 0xF;

	if (item == NULL)
		return;

	Widget *captured =
		port < 2 && id < 8 ? view->m_env->m_touch_capture[port][id].m_widget : NULL;
	ButtonBase *button = NULL;

	if (captured != NULL && captured->IsInherit(ButtonBase::TypeName()))
		button = (ButtonBase *)captured;

	Text *text = NULL;

	if (captured != NULL && captured->IsInherit(Text::TypeName()))
		text = (Text *)captured;

	Event ev(0x40, (Handler *)NULL, Event::ROUTE_TERM, 0);

	ev.m_data[0] = e->m_data[0];
	ev.m_data[1] = e->m_data[1];
	ev.m_detail = (ev.m_detail & 0xffffff00) | id;
	ev.m_detail = (ev.m_detail & 0xff0fffff) | (port << 20);

	ButtonBase *target_button =
		item->FindChild(item->GetTextAutoScrollTarget(), 0) != NULL &&
				item->FindChild(item->GetTextAutoScrollTarget(), 0)
					->IsInherit(ButtonBase::TypeName())
			? (ButtonBase *)item->FindChild(item->GetTextAutoScrollTarget(), 0)
			: NULL;
	Text *target_text =
		item->FindChild(item->GetTextAutoScrollTarget(), 0) != NULL &&
				item->FindChild(item->GetTextAutoScrollTarget(), 0)
					->IsInherit(Text::TypeName())
			? (Text *)item->FindChild(item->GetTextAutoScrollTarget(), 0)
			: NULL;
	bool follow = false;

	if (target_text != NULL)
		ScePafWidget_222D55C7(target_text, &follow);
	if (target_button != NULL && target_button != button &&
	    target_button->IsLongPressTextAutoScrollEnabled() &&
	    !target_button->IsTextAutoScrollLocked()) {
		ev.SetTarget(target_button);

		math::v2 origin(0.0f, 0.0f);

		if (button != NULL) {
			button->GetTextAutoScrollOrigin(origin);
			button->EndLongPressTextAutoScroll(&ev);
		} else if (text != NULL) {
			ScePafWidget_4608F7EF(text, &origin);
			text->m_touch_held = false;
		}
		ScePafWidget_D3D0A7C3(target_button, port, id, true, true);

		uint32_t touch = e->m_detail;
		inputdevice::InputGestureListener *input =
			view->m_env->GetTouchGestureListener((touch >> 20) & 0xF).get();

		if (input != NULL)
			inputdevice::ScePafMisc_CB4DB658(input, (uint8_t)touch, NULL);
		target_button->BeginLongPressTextAutoScroll(&ev);
		target_button->SetTextAutoScrollOrigin(origin);
	}
	if (target_text == NULL || target_text == text || !follow)
		return;

	bool on = false;

	ScePafWidget_222D55C7(target_text, &on);
	if (on && target_text->m_touch_held)
		return;
	ev.SetTarget(target_text);

	math::v2 origin(0.0f, 0.0f);

	if (button != NULL) {
		button->GetTextAutoScrollOrigin(origin);
		button->EndLongPressTextAutoScroll(&ev);
	} else if (text != NULL) {
		ScePafWidget_4608F7EF(text, &origin);
		text->m_touch_held = false;
	}
	ScePafWidget_D3D0A7C3(target_text, port, id, true, true);

	uint32_t touch = e->m_detail;
	inputdevice::InputGestureListener *input =
		view->m_env->GetTouchGestureListener((touch >> 20) & 0xF).get();

	if (input != NULL)
		inputdevice::ScePafMisc_CB4DB658(input, (uint8_t)touch, NULL);
	target_text->HandleLongPress(&ev);
	ScePafWidget_8A5AE1CD(target_text, origin);
}

/* a move of a front touch whose button or text auto scrolls by a long press: the scroll handed
   to the item the touch is on, and on to the cell next to it where the touch has left it */
int32_t ListView::HandleTouchEvent(Event *e)
{
	if (e != NULL) {
		uint32_t detail = e->m_detail;
		uint32_t port = (detail >> 20) & 0xF;

		if (port != 0 || e->GetType() != 0x46)
			goto base;
		if (m_drag_cell != NULL)
			OnCellUnGrab(m_drag_cell);

		uint32_t id = (uint8_t)detail;
		Widget *captured = port < 2 && id < 8 ? m_env->m_touch_capture[port][id].m_widget : NULL;
		ButtonBase *button = NULL;

		if (captured != NULL && captured->IsInherit(ButtonBase::TypeName()))
			button = (ButtonBase *)captured;

		Text *text = NULL;

		if (captured != NULL && captured->IsInherit(Text::TypeName()))
			text = (Text *)captured;

		bool on;

		if (!((button != NULL && button->IsTextAutoScrollLocked()) ||
		      (text != NULL && (on = false, ScePafWidget_222D55C7(text, &on), on) &&
		       text->m_touch_held)))
			goto base;

		ListItem *item = ListItem::GetListItem(captured);

		if (item == NULL)
			goto base;
		PassTextAutoScroll(this, e, item);

		int32_t segment = item->GetSegmentIndex();
		int32_t cell = item->GetCellIndex();
		ListItem *next = NULL;
		math::v4 pos = item->GetWorldMatrix().w();
		listview::Segment *seg = m_segment_list->GetSegment(segment);
		math::v4 size = seg != NULL ? seg->GetCellSize(cell) : math::v4(0.0f, 0.0f, 0.0f, 0.0f);

		if (m_scroll_type != SCROLL_TYPE_VERTICAL) {
			math::v4 half = size / 2.0f;
			float x = (float)e->m_data[0];

			if (x - (pos - half).getImpl().getX() < 0.0f) {
				listview::Cell *found = m_cell_map->Find(segment, cell - 1);

				if (found != NULL)
					next = found->m_item;
			} else if (x - (pos + half).getImpl().getX() > 0.0f) {
				listview::Cell *found = m_cell_map->Find(segment, cell + 1);

				if (found != NULL)
					next = found->m_item;
			}
		} else {
			math::v4 half = size / 2.0f;
			float y = (float)e->m_data[1];

			if (y - (pos + half).getImpl().getY() > 0.0f) {
				listview::Cell *found = m_cell_map->Find(segment, cell - 1);

				if (found != NULL)
					next = found->m_item;
			} else if (y - (pos - half).getImpl().getY() < 0.0f) {
				listview::Cell *found = m_cell_map->Find(segment, cell + 1);

				if (found != NULL)
					next = found->m_item;
			}
		}
		PassTextAutoScroll(this, e, next);
		e->Consume();
		return Widget::HandleTouchEvent(e);
	}
base:
	return ScrollViewBase::HandleTouchEvent(e);
}

int32_t ListView::HandleKeycodeEvent(Event *e)
{
	if (ScePafWidget_5EEE4313(e))
		return 0;
	return ScrollViewBase::HandleKeycodeEvent(e);
}

void ListView::OnPartsLayoutUpdated(bool is_dirty)
{
	if (is_dirty)
		Sync();
	ScrollViewBase::OnPartsLayoutUpdated(is_dirty);
}

ListView::~ListView()
{
	if (m_cell_size_set != NULL)
		delete m_cell_size_set;
	m_cell_size_set = (vector<listview::SegmentCellRange> *)-1;
	if (m_segment_list != NULL)
		delete m_segment_list;
	m_segment_list = (listview::SegmentList *)-1;
	if (m_header_factory != NULL)
		delete m_header_factory;
	m_header_factory = (listview::SegmentHeaderFactory *)-1;
	if (m_item_factory != NULL)
		delete m_item_factory;
	m_item_factory = (listview::ItemFactory *)-1;
	if (m_cell_map != NULL)
		delete m_cell_map;
	m_cell_map = (listview::CellMap *)-1;
}

void ListView::UpdateMovableDirection()
{
	ScrollViewBase::UpdateMovableDirection();
}

namespace listview {

math::v2 Segment::ScePafWidget_CA6247CF(int32_t index, ListView::FocusAlignType align)
{
	math::v2 pos = ScePafWidget_7A37CEDA(index, align, NULL);

	if (index >= 0)
		return pos;
	if (m_segment_list->m_list_view->m_scroll_type != ListView::SCROLL_TYPE_VERTICAL) {
		float head = GetHeaderSize().getImpl().getX();
		float ratio;

		if (align != ListView::FOCUS_ALIGN_TYPE_HEAD) {
			ratio = 0.5f;
			if (align == ListView::FOCUS_ALIGN_TYPE_TAIL)
				ratio = 1.0f;
		} else {
			ratio = 0.0f;
		}
		pos = math::v2(pos.getImpl().getX() + head * ratio, pos.getImpl().getY());
	} else {
		float head = GetHeaderSize().getImpl().getY();
		float ratio;

		if (align != ListView::FOCUS_ALIGN_TYPE_HEAD) {
			ratio = 0.5f;
			if (align == ListView::FOCUS_ALIGN_TYPE_TAIL)
				ratio = 1.0f;
		} else {
			ratio = 0.0f;
		}
		pos = math::v2(pos.getImpl().getX(), pos.getImpl().getY() - head * ratio);
	}
	return pos;
}

math::v2 Segment::ScePafWidget_D33B4AF3()
{
	Segment *first = this;
	Segment *last = this;

	if (m_joined) {
		for (int32_t i = m_index - 1;; i = first->m_index - 1) {
			Segment *seg = m_segment_list->GetSegment(i);

			if (seg == NULL || !seg->m_joined)
				break;
			first = seg;
		}
		for (int32_t i = m_index + 1;; i = last->m_index + 1) {
			Segment *seg = m_segment_list->GetSegment(i);

			if (seg == NULL || !seg->m_joined)
				break;
			last = seg;
		}
	}

	math::v4 size = m_view_size;
	ListView *view = m_segment_list->m_list_view;
	float head = first->m_pos;
	float tail = last->m_pos;
	math::v2 view_size = view->m_view_size;

	if (view->m_scroll_type != ListView::SCROLL_TYPE_VERTICAL) {
		float end = tail + size.getImpl().getX();
		float length = end - head;

		if (view_size.getImpl().getX() >= length) {
			float middle = head + length * 0.5f;

			return math::v2(middle, middle);
		}
		return math::v2(view_size.getImpl().getX() * 0.5f + head,
				end - view_size.getImpl().getX() * 0.5f);
	}

	float end = tail - size.getImpl().getY();
	float length = head - end;

	if (view_size.getImpl().getY() >= length) {
		float middle = head - length * 0.5f;

		return math::v2(middle, middle);
	}
	return math::v2(head - view_size.getImpl().getY() * 0.5f,
			view_size.getImpl().getY() * 0.5f + end);
}

}

/* once a flick ends: the view snapped to the ends of its scroll range, or by its snap mode to a
   cell or a segment within the scroll box, over the flick's time at the least (no name found) */
void ListView::ScePafWidget_697CD413()
{
	if (m_track_port != -1)
		return;
	if (m_velocity == math::v2(0.0f, 0.0f))
		return;
	if (m_snap_mode != SNAP_MODE_NONE) {
		bool released = false;

		if (m_snap_mode == SNAP_MODE_PAGE && m_release_pos.m_segment != -1)
			released = ScePafMisc_256F9060[0] - m_release_time < 134000;

		math::v4 box = m_scroll_box;
		math::v2 view_size = m_view_size;

		m_release_time = 0;
		if (m_scroll_type != SCROLL_TYPE_VERTICAL) {
			float head = box.getImpl().getX() + view_size.getImpl().getX() * 0.5f;
			float tail = box.getImpl().getZ() - view_size.getImpl().getX() * 0.5f;
			math::v2 now = math::v2(
				m_view_parent->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f))
					.getImpl()
					.getXY());
			float x = -now.getImpl().getX();

			if (head > x || tail < x)
				return;

			math::v4 flick;

			if (m_snap_mode == SNAP_MODE_PAGE)
				flick = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
			else
				flick = ScePafWidget_19AA6FB5();

			math::v2 back(-1.0f, -1.0f);
			math::v2 target = (now + math::v2(flick.getImpl().getXY())) * back;

			if (head > target.getImpl().getX() || tail < target.getImpl().getX())
				return;

			float snap = 3.4028235e+38f;

			if (m_snap_mode == SNAP_MODE_SEGMENT) {
				listview::ListPos at = ScePafWidget_FD8DE7ED(target);
				math::v2 shown = now * back;
				listview::ListPos from = ScePafWidget_FD8DE7ED(shown);

				if (from.m_segment == at.m_segment) {
					listview::Segment *seg = m_segment_list->GetSegment(at.m_segment);

					if (seg != NULL) {
						math::v2 range = seg->ScePafWidget_D33B4AF3();

						if (shown.getImpl().getX() >= range.getImpl().getX()) {
							if (shown.getImpl().getX() > range.getImpl().getY() &&
							    m_velocity.getImpl().getX() < 0.0f) {
								listview::ListPos next =
									ScePafWidget_DDC428E9(at, true);
								listview::Segment *other =
									m_segment_list->GetSegment(
										next.m_segment);

								if (other != NULL)
									snap = other->ScePafWidget_D33B4AF3()
										       .getImpl()
										       .getX();
							}
						} else if (m_velocity.getImpl().getX() > 0.0f) {
							listview::ListPos prev =
								ScePafWidget_814B970E(at, true);
							listview::Segment *other =
								m_segment_list->GetSegment(prev.m_segment);

							if (other != NULL)
								snap = other->ScePafWidget_D33B4AF3()
									       .getImpl()
									       .getY();
						}
					}
				}
				if (snap == 3.4028235e+38f) {
					listview::Segment *seg = m_segment_list->GetSegment(at.m_segment);

					if (seg == NULL)
						return;

					math::v2 range = seg->ScePafWidget_D33B4AF3();

					snap = range.getImpl().getX();
					if (target.getImpl().getX() >= range.getImpl().getX()) {
						snap = range.getImpl().getY();
						if (target.getImpl().getX() <= range.getImpl().getY())
							return;
					}
				}
			} else if (released) {
				listview::ListPos pos = m_velocity.getImpl().getX() > 0.0f
								? ScePafWidget_8DC5A8AE(m_release_pos, true)
								: ScePafWidget_E60EAA9F(m_release_pos, true);
				listview::Segment *seg = m_segment_list->GetSegment(pos.m_segment);

				if (seg == NULL)
					return;
				snap = seg->ScePafWidget_CA6247CF(pos.m_cell, FOCUS_ALIGN_TYPE_CENTER)
					       .getImpl()
					       .getX();
			} else {
				listview::ListPos at = ScePafWidget_FD8DE7ED(target);
				listview::Segment *seg = m_segment_list->GetSegment(at.m_segment);

				if (seg == NULL)
					return;
				snap = seg->ScePafWidget_CA6247CF(at.m_cell, FOCUS_ALIGN_TYPE_CENTER)
					       .getImpl()
					       .getX();
				if ((snap - target.getImpl().getX()) * m_velocity.getImpl().getX() >
				    0.0f) {
					listview::ListPos pos =
						snap - target.getImpl().getX() > 0.0f
							? ScePafWidget_8DC5A8AE(at, true)
							: ScePafWidget_E60EAA9F(at, true);
					listview::Segment *other =
						m_segment_list->GetSegment(pos.m_segment);

					if (other != NULL)
						snap = other->ScePafWidget_CA6247CF(
								    pos.m_cell, FOCUS_ALIGN_TYPE_CENTER)
							       .getImpl()
							       .getX();
				}
			}
			if (head <= snap && tail >= snap) {
				float time = CalcScrollTime(
					__builtin_fabsf(snap - target.getImpl().getX()),
					m_snap_mode == SNAP_MODE_PAGE || m_snap_mode == SNAP_MODE_SEGMENT);

				if (flick.getImpl().getZ() > time)
					time = flick.getImpl().getZ();
				SetViewFocus(math::v2(snap, 0.0f),
					     new Timer(time, Timer::FUNC_QUART_OUT, Timer::REPEAT_NONE, -1,
						       NULL),
					     false, true);
				SendEvent(CB_LIST_VIEW_SNAP, 0, 0, 0, 0);
			}
			return;
		}

		float head = box.getImpl().getW() + view_size.getImpl().getY() * 0.5f;
		float tail = box.getImpl().getY() - view_size.getImpl().getY() * 0.5f;
		math::v2 now = math::v2(m_view_parent->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f))
						.getImpl()
						.getXY());
		float y = -now.getImpl().getY();

		if (head > y || tail < y)
			return;

		math::v4 flick;

		if (m_snap_mode == SNAP_MODE_PAGE)
			flick = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
		else
			flick = ScePafWidget_19AA6FB5();

		math::v2 back(-1.0f, -1.0f);
		math::v2 target = (now + math::v2(flick.getImpl().getXY())) * back;

		if (head > target.getImpl().getY() || tail < target.getImpl().getY())
			return;

		float snap = 3.4028235e+38f;

		if (m_snap_mode == SNAP_MODE_SEGMENT) {
			listview::ListPos at = ScePafWidget_FD8DE7ED(target);
			math::v2 shown = now * back;
			listview::ListPos from = ScePafWidget_FD8DE7ED(shown);

			if (from.m_segment == at.m_segment) {
				listview::Segment *seg = m_segment_list->GetSegment(at.m_segment);

				if (seg != NULL) {
					math::v2 range = seg->ScePafWidget_D33B4AF3();

					if (shown.getImpl().getY() <= range.getImpl().getX()) {
						if (shown.getImpl().getY() < range.getImpl().getY() &&
						    m_velocity.getImpl().getY() > 0.0f) {
							listview::ListPos next =
								ScePafWidget_DDC428E9(at, true);
							listview::Segment *other =
								m_segment_list->GetSegment(next.m_segment);

							if (other != NULL)
								snap = other->ScePafWidget_D33B4AF3()
									       .getImpl()
									       .getX();
						}
					} else if (m_velocity.getImpl().getY() < 0.0f) {
						listview::ListPos prev = ScePafWidget_814B970E(at, true);
						listview::Segment *other =
							m_segment_list->GetSegment(prev.m_segment);

						if (other != NULL)
							snap = other->ScePafWidget_D33B4AF3()
								       .getImpl()
								       .getY();
					}
				}
			}
			if (snap == 3.4028235e+38f) {
				listview::Segment *seg = m_segment_list->GetSegment(at.m_segment);

				if (seg == NULL)
					return;

				math::v2 range = seg->ScePafWidget_D33B4AF3();

				snap = range.getImpl().getX();
				if (target.getImpl().getY() <= range.getImpl().getX()) {
					snap = range.getImpl().getY();
					if (target.getImpl().getY() >= range.getImpl().getY())
						return;
				}
			}
		} else if (released) {
			listview::ListPos pos = m_velocity.getImpl().getY() < 0.0f
							? ScePafWidget_8DC5A8AE(m_release_pos, true)
							: ScePafWidget_E60EAA9F(m_release_pos, true);
			listview::Segment *seg = m_segment_list->GetSegment(pos.m_segment);

			if (seg == NULL)
				return;
			snap = seg->ScePafWidget_CA6247CF(pos.m_cell, FOCUS_ALIGN_TYPE_CENTER)
				       .getImpl()
				       .getY();
		} else {
			listview::ListPos at = ScePafWidget_FD8DE7ED(target);
			listview::Segment *seg = m_segment_list->GetSegment(at.m_segment);

			if (seg == NULL)
				return;
			snap = seg->ScePafWidget_CA6247CF(at.m_cell, FOCUS_ALIGN_TYPE_CENTER)
				       .getImpl()
				       .getY();
			if ((snap - target.getImpl().getY()) * m_velocity.getImpl().getY() > 0.0f) {
				listview::ListPos pos = snap - target.getImpl().getY() < 0.0f
								? ScePafWidget_8DC5A8AE(at, true)
								: ScePafWidget_E60EAA9F(at, true);
				listview::Segment *other = m_segment_list->GetSegment(pos.m_segment);

				if (other != NULL)
					snap = other->ScePafWidget_CA6247CF(pos.m_cell,
									    FOCUS_ALIGN_TYPE_CENTER)
						       .getImpl()
						       .getY();
			}
		}
		if (head <= snap && tail >= snap) {
			float time = CalcScrollTime(
				__builtin_fabsf(snap - target.getImpl().getY()),
				m_snap_mode == SNAP_MODE_PAGE || m_snap_mode == SNAP_MODE_SEGMENT);

			if (flick.getImpl().getZ() > time)
				time = flick.getImpl().getZ();
			SetViewFocus(math::v2(0.0f, snap),
				     new Timer(time, Timer::FUNC_QUART_OUT, Timer::REPEAT_NONE, -1, NULL),
				     false, true);
			SendEvent(CB_LIST_VIEW_SNAP, 0, 0, 0, 0);
		}
		return;
	}
	if (!m_scroll_range_set)
		return;

	math::v4 view_box = m_view_box;

	if (m_scroll_type != SCROLL_TYPE_VERTICAL) {
		if (view_box.getImpl().getX() == 0.0f)
			return;

		Vector2 now = m_view_parent->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f))
				      .getImpl()
				      .getXY();
		Vector4 room = m_scroll_box.getImpl() - (m_view_half.getImpl() - Vector4(now, now));

		if (room.getX() > 0.0f || room.getZ() < 0.0f)
			return;

		math::v4 flick = ScePafWidget_19AA6FB5();
		Vector2 to = m_view_parent->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f))
				     .getImpl()
				     .getXY() +
			     flick.getImpl().getXY();

		room = m_scroll_box.getImpl() - (m_view_half.getImpl() - Vector4(to, to));

		float limit;

		if (room.getX() > 0.0f && room.getX() < 272.0f) {
			limit = m_view_limit.getImpl().getX();
		} else {
			if (room.getZ() >= 0.0f || room.getZ() >= -272.0f)
				return;
			limit = m_view_limit.getImpl().getZ();
		}

		float time = CalcScrollTime(__builtin_fabsf(-limit - to.getX()), false);

		if (flick.getImpl().getZ() > time)
			time = flick.getImpl().getZ();
		SetViewFocus(math::v2(-limit, 0.0f),
			     new Timer(time, Timer::FUNC_QUART_OUT, Timer::REPEAT_NONE, -1, NULL),
			     false, true);
	} else {
		if (view_box.getImpl().getY() == 0.0f)
			return;

		Vector2 now = m_view_parent->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f))
				      .getImpl()
				      .getXY();
		Vector4 room = m_scroll_box.getImpl() - (m_view_half.getImpl() - Vector4(now, now));

		if (room.getY() < 0.0f || room.getW() > 0.0f)
			return;

		math::v4 flick = ScePafWidget_19AA6FB5();
		Vector2 to = m_view_parent->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f))
				     .getImpl()
				     .getXY() +
			     flick.getImpl().getXY();

		room = m_scroll_box.getImpl() - (m_view_half.getImpl() - Vector4(to, to));

		float limit;

		if (room.getY() < 0.0f && room.getY() > -272.0f) {
			limit = m_view_limit.getImpl().getY();
		} else {
			if (room.getW() <= 0.0f || room.getW() >= 272.0f)
				return;
			limit = m_view_limit.getImpl().getW();
		}

		float time = CalcScrollTime(__builtin_fabsf(-limit - to.getY()), false);

		if (flick.getImpl().getZ() > time)
			time = flick.getImpl().getZ();
		SetViewFocus(math::v2(0.0f, -limit),
			     new Timer(time, Timer::FUNC_QUART_OUT, Timer::REPEAT_NONE, -1, NULL),
			     false, true);
	}
}

void ListView::ScePafWidget_22EF63CE()
{
	ScrollViewBase::ScePafWidget_22EF63CE();
	ScePafWidget_697CD413();
}

/* the widget the focus may go to: one that takes events, is focusable and is shown */
static inline bool CanFocus(Widget *widget)
{
	return widget->m_hold_count <= 0 && (widget->m_event_enable & 0x400) != 0 &&
	       widget->IsTextEventEnabled() && ScePafWidget_DB0FFD33(widget);
}

/* the widget the focuser's hint names for the direction, where the focus may go to it */
static inline Widget *GetHintFocus(Widget *widget, int32_t dir)
{
	Focuser *focuser = widget->m_focuser;

	if (focuser == NULL)
		return NULL;

	Widget *hint = focuser->GetHintWidget(dir);

	if (hint == NULL || !CanFocus(hint))
		return NULL;
	return hint;
}

/* the widget in the segment's header the focus goes to from the widget given (or the focus) in
   the direction: one the search finds in the header, or out of it only to the widget the
   focus's hint names that way; NULL for none; reserved is not read (names ours) */
static Widget *SearchHeaderFocus(ListView *view, const math::v2 &dir, int32_t segment,
				 int32_t reserved, Widget *from)
{
	listview::Segment *seg = view->m_segment_list->GetSegment(segment);

	if (seg == NULL)
		return NULL;

	Widget *header = seg->m_header;

	if (header == NULL)
		return NULL;

	Widget *origin = from;

	if (origin == NULL) {
		origin = view->m_env->m_focus_cursor.m_target;
		if (origin == NULL || view->FindChild(origin, 0) == NULL)
			origin = GetRootFocus(view);
		if (origin == NULL)
			origin = view;
	}

	Widget::FocusSearchInfo info(255);

	info.m_dir = GetFocusDir(dir);
	ScePafWidget_EA117CF8(&info, origin);
	if (view->FindChild(origin, 0) == NULL)
		info.m_search_outside = false;

	math::v2 d = dir;

	if ((d.extract_x() != 0.0f && view->m_scroll_type == ListView::SCROLL_TYPE_HORIZONTAL) ||
	    (d.extract_y() != 0.0f && view->m_scroll_type == ListView::SCROLL_TYPE_VERTICAL))
		info.m_search_outside = false;

	Widget *found;

	if (header->FindChild(origin, 0) != NULL)
		found = origin->SetFocusOption(&info, 0);
	else
		found = header->SetFocusOption(&info, 1);
	if (found != NULL && header->FindChild(found, 0) != NULL)
		return found;
	if (header->FindChild(origin, 0) == NULL)
		return NULL;

	int32_t focus_dir = info.m_dir;
	Widget *hint;
	Widget *w;

	if (focus_dir == 1 && (w = GetHintFocus(origin, 0)) != NULL)
		hint = w;
	else if (focus_dir == 2 && (w = GetHintFocus(origin, 1)) != NULL)
		hint = w;
	else if (focus_dir == 3 && (w = GetHintFocus(origin, 2)) != NULL)
		hint = w;
	else if (focus_dir == 4 && (w = GetHintFocus(origin, 3)) != NULL)
		hint = w;
	else
		hint = NULL;
	if (found == hint)
		return found;
	return NULL;
}

/* the focus move under way given up: the way it went kept, its frames counted again (name
   ours) */
static inline void ResetFocusMove(ListView *view)
{
	math::v2 dir = view->m_focus_target;

	view->m_focus_root->m_focus_moved = 0;
	view->m_focus_start = dir;
	view->m_focus_target = math::v2(0.0f, 0.0f);
}

/* each interval-th frame: the focus moved the way it is going to the focused segment's header
   or cell, else (where search is set) to the cell shown that way, and taken by the list view
   itself where take is set and it is idle long enough (name ours) */
void ListView::UpdateFocus(bool search, bool take, uint32_t interval)
{
	FocusRoot *root = m_focus_root;

	if (root == NULL)
		return;
	if (interval == 0)
		interval = 1;

	uint32_t frames = ++root->m_focus_moved;

	if (frames % interval != 0) {
		if (frames >= 60) {
			math::v2 dir = m_focus_target;

			root->m_focus_moved = 0;
			m_focus_start = dir;
			m_focus_target = math::v2(0.0f, 0.0f);
		}
		return;
	}
	if (FindRootFocus(this) != NULL && m_focus_target == math::v2(0.0f, 0.0f) &&
	    m_focus_root->m_focus_moved <= 30)
		return;

	int32_t segment = m_focus_list_pos.m_segment;
	int32_t cell = m_focus_list_pos.m_cell;
	listview::Cell *focused = m_cell_map->Find(segment, cell);
	Widget *header = GetSegmentHeader(segment);
	bool visible;

	if (cell == -1 && ScePafWidget_DB0FFD33(header))
		visible = true;
	else
		visible = IsCellVisible(m_cell_map->Find(segment, cell), false);

	listview::Cell *grab;

	if (((focused != NULL && focused != m_grab_cell) || cell == -1) && visible) {
		if (cell <= 0) {
			listview::Segment *seg = m_segment_list->GetSegment(segment);

			if (seg != NULL && seg->m_header != NULL) {
				math::v2 to = m_focus_target;
				Widget *found = SearchHeaderFocus(this, to, segment, cell, NULL);

				if (found != NULL) {
					SetFocusInfo info;

					info.m_mode = 0;
					info.m_notify = false;
					info.m_direction = math::v2(0.0f, 0.0f);
					info.m_keep = false;
					found->SetFocus(&info);
					m_focus_list_pos.m_cell = -1;
					grab = NULL;
					goto moved;
				}
			}
		}
		if (focused != NULL && focused != m_grab_cell) {
			math::v2 to = m_focus_target;
			bool done = focused->m_item->ScePafWidget_C98D1926(to);
			math::v2 dir = m_focus_target;

			if (m_focus_timer != NULL &&
			    m_focus_timer->m_type == ScrollViewTimer::TIMER_TYPE_MOTION &&
			    !(m_focus_pos == math::v2(-1.0f, -1.0f)) && !done &&
			    m_focus_root->m_focus_moved >= 3 * interval) {
				SetFocusInfo info;

				info.m_mode = 0;
				info.m_notify = false;
				info.m_direction = dir;
				info.m_keep = false;
				static_cast<Widget *>(this)->SetFocus(&info);
				grab = focused;
				goto moved;
			}
			if (done) {
				grab = focused;
				goto moved;
			}
		}
	}
	goto after;
moved:
	{
		math::v2 dir = m_focus_target;

		m_grab_cell = grab;
		m_focus_root->m_focus_moved = 0;
		m_focus_start = dir;
		m_focus_target = math::v2(0.0f, 0.0f);
		if (m_drag_cell != NULL)
			OnCellUnGrab(m_drag_cell);
		if (!(m_focus_pos == math::v2(-1.0f, -1.0f))) {
			m_focus_pos = math::v2(
				m_view_parent->GetPos(1, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f))
					.getImpl()
					.getXY());

			Environment *env = m_env;

			env->m_focus_cursor_moving_only = false;
			env->m_rear_focus_cursor_moving_only = false;
		}
	}
after:
	frames = m_focus_root->m_focus_moved;
	if (search && frames != 0) {
		if (!m_focus_lock && (!visible || frames >= 3 * interval)) {
			math::v2 dir(0.0f, 0.0f);

			if (!(m_focus_start == math::v2(0.0f, 0.0f)))
				dir = m_focus_start;

			listview::ListPos pos = FindFocusCell(dir);

			if (pos.m_segment < 0) {
				if (m_env->m_focus_cursor.m_target == this) {
					listview::CellMap::iterator it = NULL;
					int32_t num;

					FindVisibleCells(m_cell_map, GetVisibleRect(1),
							 m_scroll_type == SCROLL_TYPE_VERTICAL, &it, &num);
					if (num > 0) {
						int32_t index = (*it)->m_index;

						m_focus_list_pos.m_segment = (*it)->GetSegment()->m_index;
						m_focus_list_pos.m_cell = index;
					}
				}
			} else {
				m_focus_list_pos.m_segment = pos.m_segment;
				m_focus_list_pos.m_cell = pos.m_cell;

				listview::Cell *found = m_cell_map->Find(pos.m_segment, pos.m_cell);

				if (found != NULL) {
					math::v2 way = m_focus_target;

					if (found->m_item->ScePafWidget_C98D1926(way)) {
						math::v2 to = m_focus_target;

						m_focus_target = math::v2(0.0f, 0.0f);
						m_grab_cell = found;
						m_focus_root->m_focus_moved = 0;
						m_focus_start = to;
						if (m_drag_cell != NULL)
							OnCellUnGrab(m_drag_cell);
						if (!(m_focus_pos == math::v2(-1.0f, -1.0f))) {
							m_focus_pos = math::v2(
								m_view_parent
									->GetPos(1, NULL,
										 math::v4(0.0f, 0.0f, 0.0f,
											  0.0f))
									.getImpl()
									.getXY());

							Environment *env = m_env;

							env->m_focus_cursor_moving_only = false;
							env->m_rear_focus_cursor_moving_only = false;
						}
					}
				}
			}
		}
		if (!m_focus_lock && m_focus_root->m_focus_moved >= 4 * interval) {
			Widget::UpdateAfterChildren();
			if (m_focused)
				ResetFocusMove(this);
			if (take && !m_focused && m_focus_root->m_focus_moved >= 6 * interval &&
			    m_movable_dir != MoveDir_None) {
				SetFocusInfo info;

				info.m_mode = 0;
				info.m_notify = false;
				info.m_direction = m_focus_target;
				info.m_keep = false;
				static_cast<Widget *>(this)->SetFocus(&info);

				math::v2 to = m_focus_target;

				m_focus_target = math::v2(0.0f, 0.0f);
				m_focus_root->m_focus_moved = 0;
				m_focus_start = to;
				if (m_drag_cell != NULL)
					OnCellUnGrab(m_drag_cell);
			}
		}
		if (!m_focus_in_group)
			ReleaseRootFocus(this);
		if (m_focus_root->m_focus_moved >= 60)
			ResetFocusMove(this);
	} else {
		if (frames >= 60)
			ResetFocusMove(this);
		if (!m_focus_in_group)
			ReleaseRootFocus(this);
	}
}

void ListView::ScePafWidget_B6CECFDF()
{
	FocusRoot *root = m_focus_root;

	if (root == NULL)
		return;
	if (!m_sync_enabled) {
		ScrollViewBase::ScePafWidget_B6CECFDF();
		return;
	}

	Environment *env = m_env;
	Widget *cursor = env->m_focus_cursor.m_target;

	if (cursor == this && Widget::m_focus_target) {
		if (!IsSame(m_focus_start, math::v2(0.0f, 0.0f))) {
			UpdateFocus(true, false, 1);
			return;
		}
		if (m_focus_timer == NULL ||
		    m_focus_timer->m_type != ScrollViewTimer::TIMER_TYPE_MOTION ||
		    IsSame(m_focus_pos, math::v2(-1.0f, -1.0f))) {
			UpdateFocus(true, false, 5);
			return;
		}
	}
	if (root->m_focus_hold == 0 && env != NULL && ScePafWidget_A6B659BB(env, 0) &&
	    !m_env->m_focus_auto_off && ScePafWidget_DB0FFD33(this) && Widget::m_focus_target &&
	    ((m_event_enable & 0x400) != 0 || m_focus_count != 0) && m_env->IsOverModal(this) &&
	    m_hold_count <= 0 && m_focus_in_group) {
		if (m_track_port == -1 && IsSame(m_velocity, math::v2(0.0f, 0.0f))) {
			Widget *focus = GetRootFocus(this);
			bool keep = false;

			if (FindChild(cursor, 0) != NULL ||
			    (focus != NULL && ScePafWidget_DB0FFD33(focus)))
				keep = true;

			uint32_t interval = 2;
			listview::Cell *cell = m_cell_map->Find(m_focus_list_pos.m_segment, m_focus_list_pos.m_cell);

			if ((m_grab_cell != NULL && cell != m_grab_cell) || m_focus_list_pos.m_segment == -1) {
				interval = 5;
				keep = false;
			}

			bool idle = IsSame(m_focus_target, math::v2(0.0f, 0.0f));

			if (idle && !keep) {
				UpdateFocus(true, true, interval);
			} else if (m_focus_list_pos.m_segment >= 0) {
				if (!idle && (FindChild(cursor, 0) != NULL || GetRootFocus(this) == NULL))
					UpdateFocus(false, false, 1);

				Widget *target = m_env->m_focus_cursor.m_target;

				if (target != NULL && FindChild(target, 0) != NULL) {
					Environment *e = m_env;
					math::v4 move;

					if (m_focus_timer != NULL &&
					    m_focus_timer->m_type == ScrollViewTimer::TIMER_TYPE_MOTION &&
					    !IsSame(m_focus_pos, math::v2(-1.0f, -1.0f))) {
						math::v4 at = m_view_parent->GetPos(
							0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

						math::v2 off = m_focus_pos - math::v2(at.getImpl().getXY());

						move = math::v4(off.extract_x(), off.extract_y(), 0.0f, 0.0f);
					} else {
						move = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
						m_focus_pos = math::v2(-1.0f, -1.0f);
						e->m_focus_cursor_moving_only = true;
						e->m_rear_focus_cursor_moving_only = true;
					}
					m_env->m_front_touch_move = move;
					m_env->m_rear_touch_move = move;
				}
			} else {
				m_focus_root->m_focus_moved = 0;
			}
		} else {
			Widget *focus = GetRootFocus(this);

			if (focus == NULL || !ScePafWidget_DB0FFD33(focus))
				UpdateFocus(true, true, 1);
		}
	} else {
		m_focus_root->m_focus_moved = 0;
	}
}

bool ListView::ScePafWidget_C8C679BB(math::v2 &pos, const math::v2 &focus)
{
	if (m_snap_mode == SNAP_MODE_NONE) {
		if (!m_scroll_range_set)
			return false;
		if (m_focus_pending) {
			pos = focus * -1.0f;
			return true;
		}

		math::v4 view_box = m_view_box;

		/* the focus past an end of the scroll range: the view goes to that end */
		if (m_scroll_type != SCROLL_TYPE_VERTICAL) {
			if (view_box.getImpl().getX() == 0.0f)
				return false;

			Vector2 at = focus.getImpl();
			Vector4 room = m_scroll_box.getImpl() - (m_view_half.getImpl() - Vector4(at, at));

			if (room.getX() > 0.0f) {
				pos = math::v2(view_box.extract_x() + m_scroll_range.extract_x() +
						       m_view_size.extract_x() * 0.5f,
					       0.0f);
				return true;
			}
			if (room.getZ() < 0.0f) {
				pos = math::v2(view_box.extract_z() - m_scroll_range.extract_y() -
						       m_view_size.extract_x() * 0.5f,
					       0.0f);
				return true;
			}
		} else {
			if (view_box.getImpl().getY() == 0.0f)
				return false;

			Vector2 at = focus.getImpl();
			Vector4 room = m_scroll_box.getImpl() - (m_view_half.getImpl() - Vector4(at, at));

			if (room.getY() < 0.0f) {
				pos = math::v2(0.0f, view_box.extract_y() - m_scroll_range.extract_x() -
							     m_view_size.extract_y() * 0.5f);
				return true;
			}
			if (room.getW() > 0.0f) {
				pos = math::v2(0.0f, view_box.extract_w() + m_scroll_range.extract_y() +
							     m_view_size.extract_y() * 0.5f);
				return true;
			}
		}
		return false;
	}
	if (m_track_port != -1)
		return false;
	if (m_focus_pending)
		return false;
	if (!IsSame(m_velocity, math::v2(0.0f, 0.0f)))
		return false;

	math::v2 at = focus * -1.0f;
	math::v4 scroll_box = m_scroll_box;
	math::v2 size = m_view_size;

	/* the focus outside the scroll box stays where it is */
	if (m_scroll_type != SCROLL_TYPE_VERTICAL) {
		if (scroll_box.extract_x() + size.extract_x() * 0.5f > at.extract_x() ||
		    scroll_box.extract_z() - size.extract_x() * 0.5f < at.extract_x())
			return false;
	} else {
		if (scroll_box.extract_w() + size.extract_y() * 0.5f > at.extract_y() ||
		    scroll_box.extract_y() - size.extract_y() * 0.5f < at.extract_y())
			return false;
	}

	listview::ListPos list_pos = ScePafWidget_FD8DE7ED(at);
	listview::Segment *segment = m_segment_list->GetSegment(list_pos.m_segment);

	if (segment == NULL)
		return false;
	if (m_snap_mode == SNAP_MODE_SEGMENT) {
		/* it snaps to the segment's ends while the focus is past them */
		math::v2 range = segment->ScePafWidget_D33B4AF3();

		if (m_scroll_type != SCROLL_TYPE_VERTICAL) {
			if (at.extract_x() < range.extract_x()) {
				pos = math::v2(range.extract_x(), 0.0f);
			} else {
				if (!(at.extract_x() > range.extract_y()))
					return false;
				pos = math::v2(range.extract_y(), 0.0f);
			}
		} else {
			if (at.extract_y() > range.extract_x()) {
				pos = math::v2(0.0f, range.extract_x());
			} else {
				if (!(at.extract_y() < range.extract_y()))
					return false;
				pos = math::v2(0.0f, range.extract_y());
			}
		}
	} else {
		/* it snaps to the cell at the focus, centered */
		math::v2 cell = segment->ScePafWidget_CA6247CF(list_pos.m_cell, FOCUS_ALIGN_TYPE_CENTER);

		pos = cell;
		if (m_scroll_type != SCROLL_TYPE_VERTICAL)
			pos = math::v2(cell.extract_x(), 0.0f);
		else
			pos = math::v2(0.0f, cell.extract_y());
	}
	return true;
}

bool ListView::OnFocusIn(Event *e)
{
	if (m_track_port == -1 && IsSame(m_velocity, math::v2(0.0f, 0.0f)) &&
	    (m_focus_timer == NULL || m_focus_timer->m_type != ScrollViewTimer::TIMER_TYPE_MOTION ||
	     IsSame(m_focus_pos, math::v2(-1.0f, -1.0f)))) {
		ScePafWidget_AD706F2A(math::v2(m_focus_target), (Widget *)e->m_data[0]);
		ScePafWidget_A838831E();
	}
	return true;
}

bool ListView::OnFocusEnter(Event *e)
{
	return true;
}

bool ListView::OnFocusLeave(Event *e)
{
	if (m_focused) {
		m_grab_cell = NULL;
		return true;
	}

	Widget *from = (Widget *)e->m_data[1];

	if (from != NULL && FindChild(from, 0) == NULL) {
		m_grab_cell = NULL;
		return true;
	}

	/* the focus left a header: its segment keeps it */
	for (listview::SegmentList::iterator it = m_segment_list->begin();
	     it != m_segment_list->end(); ++it) {
		Widget *header = (*it)->m_header;

		if (header != NULL && header->FindChild(from, 0) != NULL) {
			m_focus_list_pos.m_segment = (*it)->m_index;
			m_focus_list_pos.m_cell = -1;
			m_grab_cell = NULL;
			m_focus_root->m_focus_moved = 0;
			return true;
		}
	}

	/* the focus left a cell: it keeps the cell while it is shown */
	for (listview::CellMap::iterator it = m_cell_map->begin(); it != m_cell_map->end();
	     ++it) {
		if ((*it)->m_item->FindChild(from, 0) != NULL) {
			listview::Cell *cell = *it;
			int32_t index = (*it)->m_index;
			int32_t segment = (*it)->GetSegment()->m_index;

			if (cell == NULL || !IsCellVisible(cell, false))
				break;
			if (cell != m_grab_cell || m_focus_list_pos.m_segment != segment || m_focus_list_pos.m_cell != index) {
				m_grab_cell = cell;
				m_focus_list_pos.m_cell = index;
				m_focus_list_pos.m_segment = segment;
				if (m_focus_root != NULL)
					m_focus_root->m_focus_moved = 0;
			}
			return true;
		}
	}
	Widget *focus = m_env->m_focus_cursor.m_target;

	if (from == NULL || focus == from)
		m_grab_cell = NULL;
	return true;
}

bool ListView::OnFocusOut(Event *e)
{
	if (e == NULL)
		return false;
	if (m_drag_cell != NULL)
		OnCellUnGrab(m_drag_cell);

	Widget *from = (Widget *)e->m_data[1];
	Widget *to = (Widget *)e->m_data[0];

	if (from == NULL || FindChild(from, 0) == NULL) {
		listview::Cell *cell = m_grab_cell;

		/* the focus left it: the cell it had is where it comes back to */
		if (cell != NULL && to != this) {
			m_focus_list_pos.m_segment = cell->GetSegment()->m_index;
			m_focus_list_pos.m_cell = cell->m_index;
		}
		m_grab_cell = NULL;
	}
	m_focus_pos = math::v2(-1.0f, -1.0f);
	m_focus_start = math::v2(0.0f, 0.0f);
	m_focus_target = math::v2(0.0f, 0.0f);

	Environment *env = m_env;

	if (env != NULL) {
		env->m_focus_cursor_moving_only = true;
		env->m_front_touch_move = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
		env->m_rear_focus_cursor_moving_only = true;
		env->m_rear_touch_move = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
	}
	return ScrollViewBase::OnFocusOut(e);
}


void ListView::MoveViewParent(const math::v2 &pos, Timer *timer)
{
	ScrollViewBase::MoveViewParent(pos, timer);
	Sync();
}

void ListView::ScePafWidget_D95AFF11(math::v2 &move, bool dragging, bool locked, bool reset)
{
	ScrollViewBase::ScePafWidget_D95AFF11(move, dragging, locked, reset);
}

void ListView::ScePafWidget_E9C7617A(uint32_t touch_id)
{
	ScrollViewBase::ScePafWidget_E9C7617A(touch_id);
	FindFocusCell(math::v2(0.0f, 0.0f));
	if (m_focus_root != NULL)
		m_focus_root->m_focus_moved = 0;
	m_focus_start = m_focus_target;
	m_focus_target = math::v2(0.0f, 0.0f);
	ScePafWidget_A838831E();
	if (m_view_parent->FindAnim(0x10000) != NULL) {
		m_release_time = ScePafMisc_256F9060[0];

		math::v4 to = m_view_parent->GetPos(1, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

		m_release_pos = ScePafWidget_FD8DE7ED(math::v2(to.getImpl().getXY()) * -1.0f);
	} else {
		m_release_time = 0;
	}
}

void ListView::ScePafWidget_D9FE36A6(int32_t id, math::v2 &dir, math::v2 &velocity)
{
	if (m_scroll_type != SCROLL_TYPE_VERTICAL) {
		if (velocity.extract_x() != 0.0f)
			m_focus_pending = true;
	} else {
		if (velocity.extract_y() != 0.0f)
			m_focus_pending = true;
	}
	ScrollViewBase::ScePafWidget_D9FE36A6(id, dir, velocity);
	m_focus_pending = false;
}


/* the focus going off the first or last cell of its segment: the view snaps to that end of its
   scroll box where it is not there yet; false where it stays (name ours) */
static __attribute__((noinline)) bool ShowListEnd(ListView *view, const math::v2 &dir)
{
	listview::Segment *segment = view->m_segment_list->GetSegment(view->m_focus_list_pos.m_segment);

	if (segment == NULL)
		return false;
	if (view->m_scroll_type == ListView::SCROLL_TYPE_VERTICAL) {
		float half = view->m_view_size.extract_y() * 0.5f;
		math::v4 box = view->m_scroll_box;
		float top = box.extract_y() - half;

		if (IsSame(dir, math::v2(0.0f, -1.0f)) &&
		    view->m_focus_list_pos.m_cell == segment->m_cell_num - 1) {
			float bottom = box.extract_w() + half;

			if (view->GetViewFocusTarget().extract_y() != bottom) {
				view->SetViewFocus(math::v2(0.0f, bottom),
						   new Timer(view->m_snap_time, Timer::FUNC_QUART_OUT),
						   true, true);
				return true;
			}
		}
		if (IsSame(dir, math::v2(0.0f, 1.0f)) && view->m_focus_list_pos.m_cell == 0) {
			if (view->GetViewFocusTarget().extract_y() != top) {
				view->SetViewFocus(math::v2(0.0f, top),
						   new Timer(view->m_snap_time, Timer::FUNC_QUART_OUT),
						   true, true);
				return true;
			}
		}
	} else {
		float half = view->m_view_size.extract_x() * 0.5f;
		math::v4 box = view->m_scroll_box;
		float left = box.extract_x() + half;
		float right = box.extract_z() - half;

		if (IsSame(dir, math::v2(-1.0f, 0.0f)) &&
		    view->m_focus_list_pos.m_cell == segment->m_cell_num - 1 &&
		    view->GetViewFocusTarget().extract_x() != left) {
			view->SetViewFocus(math::v2(left, 0.0f),
					   new Timer(view->m_snap_time, Timer::FUNC_QUART_OUT), true,
					   true);
			return true;
		}
		if (IsSame(dir, math::v2(1.0f, 0.0f)) && view->m_focus_list_pos.m_cell == 0 &&
		    view->GetViewFocusTarget().extract_x() != right) {
			view->SetViewFocus(math::v2(right, 0.0f),
					   new Timer(view->m_snap_time, Timer::FUNC_QUART_OUT), true,
					   true);
			return true;
		}
	}
	return false;
}


/* the view is where it snaps to for the focus, not with the focus cell above a grid's last,
   partial row (name ours) */
static __attribute__((noinline)) bool IsAtFocusSnap(ListView *view)
{
	listview::Segment *segment = view->m_segment_list->GetSegment(view->m_focus_list_pos.m_segment);
	Widget *view_parent = view->m_view_parent;
	bool snap = true;

	if (segment != NULL) {
		int32_t column_num = segment->m_column_num;

		if (column_num != 0) {
			int32_t next = column_num + view->m_focus_list_pos.m_cell / column_num * column_num;
			int32_t end = column_num + next;

			if (segment->m_layout_type == ListView::LAYOUT_TYPE_GRID) {
				int32_t cell_num = segment->m_cell_num;

				if (next < cell_num && end >= cell_num)
					snap = false;
			}
		}
	}

	math::v4 at = view_parent->GetPos(1, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	bool same = false;

	if (snap) {
		math::v2 pos;
		math::v2 focus(at.getImpl().getXY());

		if (view->ScePafWidget_C8C679BB(pos, focus))
			same = IsSame(focus * -1.0f, pos);
	}
	return same;
}


/* the size a segment's header takes along the view's axis, 0 for an empty segment whose header
   disappears (name ours) */
static inline math::v4 GetHeaderSize(listview::Segment *segment)
{
	if (segment->m_header != NULL)
		return *segment->m_header->GetSize(0);
	return math::v4(Vector4(segment->m_header_size));
}

/* the position a row before the grid cell: the cell above it, its segment's header (where it
   has one and the header is wanted) or the last cell of the nearest segment before it that
   has cells (with header: that takes size along the axis); (-1, -1) for none (name ours) */
static __attribute__((noinline)) listview::ListPos GetPrevRowPos(ListView *view, const listview::ListPos &pos, bool header)
{
	listview::SegmentList *list = view->m_segment_list;
	listview::Segment *segment = list->GetSegment(pos.m_segment);

	if (segment == NULL)
		return MakeListPos(-1, -1);

	int32_t column_num = segment->m_column_num;

	if (segment->m_layout_type != ListView::LAYOUT_TYPE_GRID)
		goto none;
	if (header) {
		if (list->m_list_view->m_scroll_type == ListView::SCROLL_TYPE_VERTICAL) {
			if (pos.m_cell > column_num - 1)
				return MakeListPos(pos.m_segment, pos.m_cell - column_num);
			if (pos.m_cell == 0) {
				math::v4 size = GetHeaderSize(segment);
				float length = segment->m_cell_num == 0 && segment->m_header_disappearance
						       ? 0.0f
						       : size.extract_y();

				if (length != 0.0f)
					return MakeListPos(pos.m_segment, pos.m_cell - 1);
			}
			listview::Segment *prev = view->m_segment_list->GetSegment(segment->m_index - 1);

			while (prev != NULL) {
				int32_t index = prev->m_index;

				if (prev->m_view_size.extract_y() != 0.0f)
					return MakeListPos(index, prev->m_cell_num - 1);
				prev = view->m_segment_list->GetSegment(index - 1);
			}
			goto none;
		}
		if (pos.m_cell > column_num - 1)
			return MakeListPos(pos.m_segment, pos.m_cell - column_num);
		if (pos.m_cell == 0) {
			math::v4 size = GetHeaderSize(segment);
			float length = segment->m_cell_num == 0 && segment->m_header_disappearance
					       ? 0.0f
					       : size.extract_y();

			if (length != 0.0f)
				return MakeListPos(pos.m_segment, pos.m_cell - 1);
		}
		listview::Segment *prev = view->m_segment_list->GetSegment(segment->m_index - 1);

		while (prev != NULL) {
			int32_t index = prev->m_index;

			if (prev->m_view_size.extract_x() != 0.0f)
				return MakeListPos(index, prev->m_cell_num - 1);
			prev = view->m_segment_list->GetSegment(index - 1);
		}
		goto none;
	}
	if (pos.m_cell > column_num - 1)
		return MakeListPos(pos.m_segment, pos.m_cell - column_num);

	{
		listview::Segment *prev = view->m_segment_list->GetSegment(segment->m_index - 1);

		while (prev != NULL) {
			int32_t index = prev->m_index;

			if (prev->m_cell_num != 0)
				return MakeListPos(index, prev->m_cell_num - 1);
			prev = view->m_segment_list->GetSegment(index - 1);
		}
	}
none:
	return MakeListPos(-1, -1);
}


/* the position a row after the grid cell: the cell below it, or the first of the nearest
   segment after it that has cells (with header: that takes size along the axis, at its header
   where the header takes size); (-1, -1) for none (name ours) */
static __attribute__((noinline)) listview::ListPos GetNextRowPos(ListView *view, const listview::ListPos &pos, bool header)
{
	listview::SegmentList *list = view->m_segment_list;
	listview::Segment *segment = list->GetSegment(pos.m_segment);

	if (segment == NULL)
		return MakeListPos(-1, -1);

	int32_t column_num = segment->m_column_num;

	if (segment->m_layout_type != ListView::LAYOUT_TYPE_GRID)
		goto none;

	int32_t cell = pos.m_cell;
	int32_t last_row = segment->m_cell_num - column_num;

	if (header) {
		if (list->m_list_view->m_scroll_type == ListView::SCROLL_TYPE_VERTICAL) {
			if (cell < last_row)
				return MakeListPos(pos.m_segment, cell + column_num);

			listview::Segment *next = view->m_segment_list->GetSegment(segment->m_index + 1);

			while (next != NULL) {
				int32_t index = next->m_index;

				if (next->m_view_size.extract_y() != 0.0f) {
					math::v4 size = GetHeaderSize(next);
					float length = next->m_cell_num == 0 &&
								       next->m_header_disappearance
							       ? 0.0f
							       : size.extract_y();

					return MakeListPos(index, length != 0.0f ? -1 : 0);
				}
				next = view->m_segment_list->GetSegment(index + 1);
			}
			goto none;
		}
		if (cell < last_row)
			return MakeListPos(pos.m_segment, cell + column_num);

		listview::Segment *next = view->m_segment_list->GetSegment(segment->m_index + 1);

		while (next != NULL) {
			int32_t index = next->m_index;

			if (next->m_view_size.extract_x() != 0.0f) {
				math::v4 size = GetHeaderSize(next);
				float length = next->m_cell_num == 0 && next->m_header_disappearance
						       ? 0.0f
						       : size.extract_x();

				return MakeListPos(index, length != 0.0f ? -1 : 0);
			}
			next = view->m_segment_list->GetSegment(index + 1);
		}
		goto none;
	}
	if (cell < last_row)
		return MakeListPos(pos.m_segment, cell + column_num);

	{
		listview::Segment *next = view->m_segment_list->GetSegment(segment->m_index + 1);

		while (next != NULL) {
			int32_t index = next->m_index;

			if (next->m_cell_num != 0)
				return MakeListPos(index, 0);
			next = view->m_segment_list->GetSegment(index + 1);
		}
	}
none:
	return MakeListPos(-1, -1);
}

/* the position the focus goes to from the one given in the direction, stepping through a
   grid's rows and columns or along a list: it stops at a cell with a child to focus (not the
   grabbed one, but from a header), at any cell while a focus motion runs, at the focus
   position, at the end, and at a cell that is neither a shown header nor wholly visible; the
   cell FindFocusCell gives without a position, (-1, cell) where a list is not moved along its
   axis (name ours) */
static __attribute__((noinline)) listview::ListPos FindFocusPos(ListView *view, const math::v2 &dir,
				      const listview::ListPos &from)
{
	if (view->m_cell_map->size() == 0)
		return MakeListPos(-1, -1);

	listview::ListPos pos = from;
	Widget *focus = GetRootFocus(view);
	listview::Segment *segment = view->m_segment_list->GetSegment(pos.m_segment);
	Widget *header = segment != NULL ? segment->m_header : NULL;
	bool on_header = false;

	if (header->FindChild(focus, 0) != NULL)
		on_header = true;
	if (pos.m_segment < 0)
		return view->FindFocusCell(dir);

	listview::Cell *cell = view->m_cell_map->Find(pos.m_segment, on_header ? 0 : pos.m_cell);

	if (cell == NULL)
		return view->FindFocusCell(dir);

	ListView::LayoutType layout = cell->GetSegment()->m_layout_type;

	if (IsSame(dir, math::v2(0.0f, 0.0f)))
		return pos;

	float along = dir.extract_x();

	if (view->m_scroll_type == ListView::SCROLL_TYPE_VERTICAL)
		along = -dir.extract_y();
	if (along == 0.0f && layout != ListView::LAYOUT_TYPE_GRID)
		return MakeListPos(-1, pos.m_cell);
	/* down a grid from its header: along the cells */
	if (along > 0.0f && on_header && layout == ListView::LAYOUT_TYPE_GRID)
		layout = ListView::LAYOUT_TYPE_LIST;
	for (;;) {
		if (layout == ListView::LAYOUT_TYPE_GRID) {
			if (view->m_scroll_type == ListView::SCROLL_TYPE_VERTICAL) {
				if (dir.extract_x() < 0.0f)
					pos = view->ScePafWidget_8DC5A8AE(pos, on_header);
				else if (dir.extract_x() > 0.0f)
					pos = view->ScePafWidget_E60EAA9F(pos, on_header);
				if (dir.extract_y() < 0.0f)
					pos = GetNextRowPos(view, pos, on_header);
				else if (dir.extract_y() > 0.0f)
					pos = GetPrevRowPos(view, pos, on_header);
			} else {
				if (dir.extract_y() > 0.0f)
					pos = view->ScePafWidget_8DC5A8AE(pos, on_header);
				else if (dir.extract_y() < 0.0f)
					pos = view->ScePafWidget_E60EAA9F(pos, on_header);
				if (dir.extract_x() > 0.0f)
					pos = GetNextRowPos(view, pos, on_header);
				else if (dir.extract_x() < 0.0f)
					pos = GetPrevRowPos(view, pos, on_header);
			}
		} else if (along < 0.0f) {
			pos = view->ScePafWidget_8DC5A8AE(pos, on_header);
		} else if (along > 0.0f) {
			pos = view->ScePafWidget_E60EAA9F(pos, on_header);
		}
		if (pos.m_segment < 0)
			return pos;

		listview::Cell *next = view->m_cell_map->Find(pos.m_segment, pos.m_cell);

		if (next != NULL && (next != view->m_grab_cell || on_header)) {
			if (FindListItemFocusChild(next->m_item, 0) != NULL)
				return pos;
			if (view->m_focus_timer != NULL &&
			    view->m_focus_timer->m_type == ScrollViewTimer::TIMER_TYPE_MOTION &&
			    !IsSame(view->m_focus_pos, math::v2(-1.0f, -1.0f)))
				return pos;
		}
		if (view->m_focus_list_pos.m_segment == pos.m_segment && view->m_focus_list_pos.m_cell == pos.m_cell)
			return pos;
		if (next == NULL)
			continue;

		listview::Segment *at = view->m_segment_list->GetSegment(pos.m_segment);
		Widget *at_header = at != NULL ? at->m_header : NULL;

		if (pos.m_cell == -1 && ScePafWidget_DB0FFD33(at_header))
			continue;
		if (!view->IsCellVisible(view->m_cell_map->Find(pos.m_segment, pos.m_cell), true))
			return pos;
	}
}


bool ListView::ScePafWidget_AD706F2A(const math::v2 &dir, Widget *from)
{
	if (!m_sync_enabled)
		return ScrollViewBase::ScePafWidget_AD706F2A(math::v2(dir), NULL);
	if (m_snap_list.size() != 0) {
		if (ScrollByKey(math::v2(dir)))
			return true;
		m_focus_start = m_focus_target;
		m_focus_target = math::v2(0.0f, 0.0f);
		return ScrollParentByKey(math::v2(dir));
	}

	Widget *found = NULL;
	Widget *focus;

	if (IsSame(dir, math::v2(0.0f, 0.0f))) {
		/* no move: the focus stays on a header that shows or a cell that is visible */
		if (m_focus_list_pos.m_segment >= 0) {
			listview::Segment *segment =
				m_segment_list->GetSegment(m_focus_list_pos.m_segment);
			int32_t cell = m_focus_list_pos.m_cell;
			Widget *header = segment != NULL ? segment->m_header : NULL;

			if (cell == -1 && ScePafWidget_DB0FFD33(header))
				return true;
			if (IsCellVisible(m_cell_map->Find(m_focus_list_pos.m_segment,
							    m_focus_list_pos.m_cell),
					  false))
				return true;
		}
		goto step;
	}
	if (!m_focus_lock) {
		m_focus_start = m_focus_target;
		m_focus_target = math::v2(0.0f, 0.0f);
		/* the focus moved off the focus cell to another cell: that is the focus cell */
		if (m_focus_list_pos.m_segment >= 0) {
			listview::Cell *cell =
				m_cell_map->Find(m_focus_list_pos.m_segment, m_focus_list_pos.m_cell);
			Widget *root_focus = GetRootFocus(this);

			if (cell != NULL && root_focus != NULL &&
			    cell->m_item->FindChild(root_focus, 0) == NULL &&
			    ScePafWidget_DB0FFD33(root_focus)) {
				for (listview::CellMap::iterator it = m_cell_map->begin();
				     it != m_cell_map->end(); ++it) {
					if ((*it)->m_item->FindChild(root_focus, 0) != NULL) {
						int32_t index = (*it)->m_index;

						m_focus_list_pos.m_segment = (*it)->GetSegment()->m_index;
						m_focus_list_pos.m_cell = index;
						break;
					}
				}
			}
		}
	}

	focus = from;
	if (focus == NULL)
		focus = m_env->m_focus_cursor.m_target;
	if (focus != NULL && FindChild(focus, 0) != NULL) {
		int32_t focus_dir = GetFocusDir(dir);
		Widget *hint = NULL;

		/* where the focus's hint names a widget that way, the search finds it */
		if (focus_dir == 1)
			hint = GetHintFocus(focus, 0);
		else if (focus_dir == 2)
			hint = GetHintFocus(focus, 1);
		else if (focus_dir == 3)
			hint = GetHintFocus(focus, 2);
		else if (focus_dir == 4)
			hint = GetHintFocus(focus, 3);
		if (hint == NULL) {
			Environment *env = m_env;

			if (env->m_focus_history_on) {
				int32_t history_dir = GetFocusDir(dir);

				found = history_dir != 255 ?
						env->m_focus_cursor.FindFocusHistory(focus, history_dir) :
						NULL;
			}
			if (found != NULL) {
				if (FindChild(found, 0) != NULL && found != this && CanFocus(found)) {
					/* it goes back the way it came */
					focus = NULL;
					goto set;
				}
				found = NULL;
			}
		}
	}

	/* the focus on a header: into the header */
	if (m_focus_list_pos.m_cell == -1) {
		listview::Segment *segment = m_segment_list->GetSegment(m_focus_list_pos.m_segment);

		if (segment != NULL && segment->m_header != NULL) {
			found = SearchHeaderFocus(this, math::v2(dir), m_focus_list_pos.m_segment,
						  m_focus_list_pos.m_cell, from);
			if (found != NULL)
				m_grab_cell = NULL;
		}
	}

set: {
	listview::Cell *cell = m_cell_map->Find(m_focus_list_pos.m_segment, m_focus_list_pos.m_cell);

	/* within the focus cell's item */
	if (found == NULL && cell != NULL)
		found = SearchListItemFocus(cell->m_item, math::v2(dir), from, 0);
	if (found != NULL) {
		SetFocusInfo info;

		info.m_mode = 0;
		info.m_notify = true;
		info.m_direction = dir;
		info.m_keep = false;
		found->SetFocus(&info);

		math::v2 to = m_focus_target;

		m_focus_target = math::v2(0.0f, 0.0f);
		m_focus_start = to;
		if (focus != NULL && found != this) {
			Environment *env = m_env;
			int32_t focus_dir = GetFocusDir(dir);

			if (env->m_focus_history_on && focus_dir != 255)
				env->m_focus_cursor.AddFocusHistory(focus, found, focus_dir);
		}
		return true;
	}
	if (!IsSame(m_focus_target, math::v2(0.0f, 0.0f))) {
		ScrollViewTimer *timer = m_focus_timer;

		/* the focus motion is still under way */
		if (timer != NULL && timer->m_type == ScrollViewTimer::TIMER_TYPE_MOTION &&
		    timer->m_mapped_x < 0.3f)
			return true;
	}
}

step: {
	/* to another cell */
	listview::ListPos to = FindFocusPos(this, dir, m_focus_list_pos);
	float time = m_snap_time;
	int32_t segment = m_focus_list_pos.m_segment;

	if (to.m_segment != segment) {
		Widget *header_focus = NULL;

		/* into the header between them */
		if (to.m_segment < segment)
			header_focus = SearchHeaderFocus(this, math::v2(dir), segment,
							 m_focus_list_pos.m_cell, NULL);
		else if (to.m_segment > segment)
			header_focus =
				SearchHeaderFocus(this, math::v2(dir), to.m_segment, to.m_cell, NULL);
		if (header_focus != NULL) {
			header_focus->SetFocus(NULL);

			math::v2 start = m_focus_target;

			if (to.m_segment > m_focus_list_pos.m_segment)
				m_focus_list_pos.m_segment = to.m_segment;
			m_grab_cell = NULL;
			m_focus_start = start;
			m_focus_target = math::v2(0.0f, 0.0f);
			m_focus_list_pos.m_cell = -1;
			return true;
		}

		listview::Segment *next = m_segment_list->GetSegment(to.m_segment);

		if (next != NULL) {
			math::v2 at = next->ScePafWidget_7A37CEDA(to.m_cell, FOCUS_ALIGN_TYPE_NEAR, NULL);
			math::v2 move = at - GetViewFocusTarget();

			if (m_scroll_type != SCROLL_TYPE_VERTICAL)
				time = CalcScrollTime(__builtin_fabsf(move.extract_x()), false);
			else
				time = CalcScrollTime(__builtin_fabsf(move.extract_y()), false);
		}
	}
	if (to.m_segment < 0) {
		/* none: the view scrolls, to its end first */
		if (!IsAtFocusSnap(this)) {
			if (ShowListEnd(this, math::v2(dir))) {
				Environment *env;

				m_focus_pos = math::v2(-1.0f, -1.0f);
				env = m_env;
				env->m_focus_cursor_moving_only = true;
				env->m_rear_focus_cursor_moving_only = true;
			} else if (ScrollByKey(math::v2(dir))) {
				return true;
			}
		}

		math::v2 start = dir;

		m_focus_target = math::v2(0.0f, 0.0f);
		m_focus_start = start;
		return ScrollParentByKey(math::v2(dir));
	}
	if (m_focus_list_pos.m_segment == to.m_segment && m_focus_list_pos.m_cell == to.m_cell)
		return false;
	m_focus_target = dir;
	SetFocus(to.m_segment, to.m_cell, FOCUS_ALIGN_TYPE_NEAR,
		 new Timer(time, Timer::FUNC_QUART_OUT));

	listview::Cell *cell = NULL;

	if (m_focus_list_pos.m_segment >= 0)
		cell = m_cell_map->Find(m_focus_list_pos.m_segment, m_focus_list_pos.m_cell);
	if (cell != m_grab_cell && m_focus_timer != NULL &&
	    m_focus_timer->m_type == ScrollViewTimer::TIMER_TYPE_MOTION) {
		if (!m_focus_in_group) {
			SetFocusInfo info;

			info.m_mode = 0;
			info.m_notify = true;
			info.m_direction = m_focus_target;
			info.m_keep = false;
			static_cast<Widget *>(this)->SetFocus(&info);
		}
	} else {
		math::v2 start = m_focus_target;

		m_focus_target = math::v2(0.0f, 0.0f);
		m_focus_start = start;
	}
	return true;
}
}

void ListView::SetFocusRootState(uint32_t state)
{
	FocusRoot *root = m_focus_root;
	uint32_t last = 0;

	if (root != NULL) {
		last = root->m_state;
		root->m_state = state;
	}
	if (state != 0 && last != 0 && state != last) {
		Event e;

		e.m_data[0] = last;
		e.m_data[1] = state;
		OnFocusLeave(&e);
	}
}


void ListView::FocusRect(const math::v4 &rect, Timer *timer)
{
	math::v4 at = m_view_parent->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	math::v2 pos(at.getImpl().getXY());
	math::v2 range = GetVisibleRange(0);

	/* the view moves so the rect shows: centered where it fits, else at its near edge */
	if (m_scroll_type != SCROLL_TYPE_VERTICAL) {
		float left = rect.extract_x();
		float right = rect.extract_z();
		float x;

		if (range.extract_y() - range.extract_x() > right - left) {
			float half = m_size.extract_x() * 0.5f;
			float to;

			if (range.extract_x() > left)
				to = left + half;
			else if (right > range.extract_y())
				to = right - half;
			else
				to = -at.extract_x();
			x = -to;
		} else {
			x = -((left + right) * 0.5f);
		}
		pos = math::v2(x, pos.extract_y());
	} else {
		float top = rect.extract_y();
		float bottom = rect.extract_w();
		float y;

		if (range.extract_x() - range.extract_y() > top - bottom) {
			float half = m_size.extract_y() * 0.5f;
			float to;

			if (top > range.extract_x())
				to = top - half;
			else if (range.extract_y() > bottom)
				to = bottom + half;
			else
				to = -at.extract_y();
			y = -to;
		} else {
			y = -((top + bottom) * 0.5f);
		}
		pos = math::v2(pos.extract_x(), y);
	}
	if (IsSame(pos, math::v2(at.getImpl().getXY()))) {
		if (timer != NULL)
			delete timer;
	} else {
		MoveViewParent(pos, timer);
	}
}

bool ListView::ScePafWidget_5EEE4313(Event *e)
{
	if (e == NULL)
		return false;
	if (e->GetType() == 17) {
		Environment *env = m_env;

		/* a key down ends the touches */
		if (env->m_touch_hits.size() > 0) {
			env->m_touch_hits.clear();
			env->m_hit_stack.clear();
		}
	}
	if (e == NULL || m_drag_cell == NULL)
		return false;

	uint32_t key = e->m_detail & 0xff;
	int32_t from = m_drag_cell->m_index;
	int32_t to = from;

	if (key > 4 || key == 0)
		return false;

	ScrollType scroll_type = m_scroll_type;
	uint32_t type = e->GetType();

	if ((scroll_type == SCROLL_TYPE_VERTICAL && key == 1) ||
	    (scroll_type == SCROLL_TYPE_HORIZONTAL && key == 3))
		to = from - 1;
	else if ((scroll_type == SCROLL_TYPE_VERTICAL && key == 2) ||
		 (scroll_type == SCROLL_TYPE_HORIZONTAL && key == 4))
		to = from + 1;
	if (type == 17 || type == 19) {
		int32_t segment_index = m_drag_cell->GetSegment()->m_index;

		if (from != to) {
			int32_t moved = to;
			listview::Segment *segment = m_segment_list->GetSegment(segment_index);

			if (segment != NULL && segment->MoveCell(from, moved)) {
				OnCellMoved(segment_index, from, moved);
				Sync();
			}

			listview::Cell *cell = m_cell_map->Find(segment_index, to);

			if (cell != NULL) {
				ListItem *item = cell->m_item;

				m_focus_list_pos.m_cell = to;
				m_drag_item_pos = math::v2(
					item->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f))
						.getImpl()
						.getXY());
			}
		}
	}
	m_drag_cell->ResetPos(true);
	FocusRect(m_drag_cell->GetRect(false), new Timer(100.0f));
	e->Consume();
	return true;
}

math::v2 ListView::GetFocusedViewPos()
{
	math::v2 touch(0.0f, 0.0f);

	if (m_track_port != -1)
		touch = GetTouchPos();

	math::v4 at = m_view_parent->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

	return touch - math::v2(at.getImpl().getXY());
}

void ListView::OnCellUnGrab(listview::Cell *cell)
{
	if (m_drag_cell != cell)
		return;

	listview::Cell *found = m_cell_map->Find(cell->GetSegment()->m_index, cell->m_index);

	/* it lets go of the cell once its item holds no touch */
	if (found == NULL || found->m_item == NULL || found->m_item->m_touch_captures.size() == 0) {
		m_drag_cell = NULL;
		cell->ResetPos(true);
		OnCellDragStop(cell->GetSegment()->m_index, cell->m_index);
		m_focus_direction_off = true;
		m_drag_begun = false;
		Widget::m_focus_target = true;
	}
}


/* where the dragged cell drops at the position: next to the cell it passed the middle of */
int32_t listview::Segment::GetDropIndex(const math::v2 &pos)
{
	if (m_cell_size_list == NULL)
		return GetCellIndex(pos);

	ListView *view = m_segment_list->m_list_view;
	float d;

	if (view->m_scroll_type != ListView::SCROLL_TYPE_VERTICAL) {
		float head = GetHeaderSize().getImpl().getX();

		d = pos.getImpl().getX() - (m_pos + head);
	} else {
		float head = GetHeaderSize().getImpl().getY();

		d = (m_pos - head) - pos.getImpl().getY();
	}

	int32_t index = view->m_drag_cell->m_index;
	listview::CellSize &size = m_cell_size_list->at(index);
	bool back = false;
	float at = d + size.m_size * 0.5f;

	if (d < size.m_pos + size.m_size * 0.5f) {
		back = true;
		at = d - size.m_size * 0.5f;
	}

	listview::CellSizeList::iterator it = m_cell_size_list->Find(at);

	if (it == m_cell_size_list->end())
		return index;

	int32_t drop = it - m_cell_size_list->begin();

	if (index == drop)
		return index;

	listview::CellSize &cell = m_cell_size_list->at(drop);
	float middle = cell.m_pos + cell.m_size * 0.5f;

	if (back) {
		if (middle < at)
			drop++;
	} else if (middle > at) {
		drop--;
	}
	return drop;
}

/* the dragged cell follows its item, moving where it drops */
void ListView::OnCellDragMove(listview::Cell *cell)
{
	if (m_drag_cell != cell)
		return;

	math::v4 at = cell->m_item->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	math::v2 pos(at.getImpl().getXY());

	if (IsSame(m_drag_item_pos, pos))
		return;

	m_drag_item_pos = pos;

	int32_t to = m_drag_cell->GetSegment()->GetDropIndex(m_drag_item_pos);
	int32_t from = m_drag_cell->m_index;

	if (from != to) {
		int32_t segment_index = m_drag_cell->GetSegment()->m_index;
		int32_t moved = to;
		listview::Segment *segment = m_segment_list->GetSegment(segment_index);

		if (segment != NULL && segment->MoveCell(from, moved)) {
			OnCellMoved(segment_index, from, moved);
			Sync();
		}
	}
	FocusRect(m_drag_cell->GetRect(true), new Timer(100.0f));
}

void ListView::OnCellMoved(int32_t segment, int32_t from, int32_t to)
{
	SendEvent(CB_LIST_VIEW_CELL_MOVED, segment, from, to, 0);
}

void ListView::OnCellDragStart(int32_t segment, int32_t cell)
{
	SendEvent(CB_LIST_VIEW_DRAG_START, segment, cell, 0, 0);
}

void ListView::OnCellDragStop(int32_t segment, int32_t cell)
{
	SendEvent(CB_LIST_VIEW_DRAG_STOP, segment, cell, 0, 0);
}


const char ListViewParam::m_param_name[] = "list_view_base";

const char *ListViewParam::GetParamName() const
{
	return m_param_name;
}

const char *ListViewParam::ParamName()
{
	return m_param_name;
}

const char ListViewStyleParam::m_param_name[] = "style_list_view";

const char *ListViewStyleParam::GetParamName() const
{
	return m_param_name;
}

const char *ListViewStyleParam::ParamName()
{
	return m_param_name;
}

ListViewStyleParam::ListViewStyleParam()
	: m_view_margin(0.0f, 0.0f), m_cell_size_default(s_cell_size_default)
{
}

int32_t ListViewStyleParam::Create(int32_t mode)
{
	int32_t ret = ScrollViewBaseStyleParam::Create(mode);

	if (ret < 0)
		return ret;
	if (m_serial != NULL) {
		m_serial->AddParam("view_margin", &m_view_margin, ResourceParam::SEMANTIC_VEC2);
		if (m_serial != NULL)
			m_serial->AddParam("cell_size_initial_default", &m_cell_size_default,
					   ResourceParam::SEMANTIC_VEC4);
	}
	return 0;
}

int32_t ListViewStyleParam::SetEach(Widget *widget)
{
	int32_t ret = ScrollViewBaseStyleParam::SetEach(widget);

	if (ret < 0)
		return ret;

	ListView *view = (ListView *)widget;

	view->ScePafWidget_5B94F2B4(m_view_margin, NULL);
	view->m_cell_size_default = m_cell_size_default;
	return 0;
}

int32_t ListViewStyleParam::GetEach(Widget *widget)
{
	int32_t ret = ScrollViewBaseStyleParam::GetEach(widget);

	if (ret < 0)
		return ret;

	ListView *view = (ListView *)widget;

	m_view_margin = view->ScePafWidget_B778A1D6(false);
	m_cell_size_default = view->m_cell_size_default;
	return 0;
}


namespace listview {

Cell *CellList::GetLast()
{
	if (size() == 0)
		return NULL;
	return (*this)[size() - 1].get();
}

Cell *CellMap::Find(ListItem *item)
{
	for (iterator it = begin(); it != end(); ++it)
		if ((*it)->m_item == item)
			return *it;
	return NULL;
}

ItemFactory::~ItemFactory()
{
}

ListItem *ItemFactory::Create(CreateParam &param)
{
	ListItem *item = new ListItem(param.m_parent, NULL);
	Segment *segment = param.m_list_view->m_segment_list->GetSegment(param.m_segment);
	math::v4 size;

	if (segment != NULL)
		size = segment->m_cell_size_default;
	else
		size = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
	item->SetSize(size);
	return item;
}

void ItemFactory::Start(StartParam &param)
{
}

void ItemFactory::Stop(StopParam &param)
{
}

void ItemFactory::Dispose(DisposeParam &param)
{
	param.m_item->DestroyWidget();
}

}


namespace listview {

SegmentHeaderFactory::~SegmentHeaderFactory()
{
}

Widget *SegmentHeaderFactory::Create(CreateParam &param)
{
	return NULL;
}

void SegmentHeaderFactory::Dispose(DisposeParam &param)
{
	if (param.m_header != NULL)
		param.m_header->DestroyWidget();
}

}

/* the focus moves on from the view only where it is not in the view and the view can take it
   (name ours) */
static int32_t ListViewFocusCB(Focuser *focuser, Widget *widget, void *data, void *move)
{
	Widget::FocusSearchInfo *info = (Widget::FocusSearchInfo *)move;

	if (info == NULL || widget->FindChild(info->m_origin, 0) != NULL ||
	    ((ListView *)widget)->ScePafWidget_9D5DB5D6() == 0)
		return 0;
	return DefaultFocusCB(focuser, widget, data, move);
}

static void AxisPaddingSet(AnimListener *anim, math::v4 data)
{
	((ListView *)anim->m_widget)->SetAxisPadding(math::v2(data.getImpl().getXY()));
}

static math::v4 AxisPaddingGet(AnimListener *anim)
{
	math::v2 padding = ((ListView *)anim->m_widget)->ScePafWidget_B778A1D6(false);

	return math::v4(Vector4(padding.getImpl(), Vector2(0.0f, 0.0f)));
}


}
}


using namespace paf;
using namespace paf::ui;

static Widget *NewListView(Widget *parent)
{
	return new ListView(parent, NULL);
}

static WidgetParam *NewListViewParam()
{
	return new ListViewParam;
}

static VersatileParam *NewListViewLayoutParam()
{
	return new WidgetLayoutParam;
}

static VersatileParam *NewListViewFocusParam()
{
	return new WidgetFocusParam;
}

static VersatileParam *NewListViewStyleParam()
{
	return new ListViewStyleParam;
}

static VersatileParam *NewListViewHandlerParam()
{
	return new WidgetHandlerParam;
}

static VersatileParam *NewListViewSoundParam()
{
	return new ScrollViewBaseSoundParam;
}

FactoryFuncTable g_factoryListView = {
	ListView::TypeName(),
	NewListView,
	NewListViewParam,
	NewListViewLayoutParam,
	NewListViewFocusParam,
	NewListViewStyleParam,
	NewListViewHandlerParam,
	NewListViewSoundParam,
};
