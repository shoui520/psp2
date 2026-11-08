/*
 * paf::ui::ListView: a scroll view of segments of cells, each cell shown by a list item an item
 * factory makes while it is near the view (listview::Cell, listview::Segment). The classes,
 * TypeName, the parameter names and the exports are Sony's (export NIDs hash as them;
 * "list_view" is its string); the members' names are ours.
 */
#ifndef PAF_WIDGET_W_LIST_VIEW_H
#define PAF_WIDGET_W_LIST_VIEW_H

#include <stdint.h>
#include <common/property_list.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <std/vector>
#include <widget/w_list_item.h>
#include <widget/w_scroll_view_base.h>

namespace paf {

class Timer;

namespace ui {

class ListView;

namespace listview {

class Cell;
class CellList;
class Segment;
class SegmentList;

/* the cells of a segment, by index */
class CellList {
public:
	CellList(Segment *segment) : m_segment(segment)
	{
	}

	vector<Cell *> m_cells;
	uint32_t reserved;
	Segment *m_segment;
};

/* a cell of a segment: its index, the list item shown for it and where it is */
class Cell {
public:
	Cell(CellList *list, int32_t index);
	~Cell();

	Segment *GetSegment() const
	{
		return m_cell_list->m_segment;
	}

	int32_t GetSegmentIndex() const;
	ListView *GetListView();

	CellList *m_cell_list;
	int32_t m_index;
	ListItem *m_item;
	math::v2 m_pos;
	bool m_started;
	bool m_dragging;
	math::v2 m_drag_offset;
	int32_t m_drag_index;
};

/* a list view's segments, by index */
class SegmentList {
public:
	SegmentList(ListView *list_view) : m_list_view(list_view)
	{
	}

	vector<Segment *> m_segments;
	uint32_t reserved;
	ListView *m_list_view;
};

/* a segment of a list view: its cells, the size they take and its header */
class Segment {
public:
	SegmentList *m_segment_list;
	int32_t m_index;
};

/* makes and disposes of the list items of a list view's cells */
class ItemFactory {
public:
	/* what Create is given: the cell's list view, segment and index, and the parent the
	   item goes under */
	class CreateParam {
	public:
		ListView *m_list_view;
		int32_t m_segment;
		int32_t m_cell;
		Widget *m_parent;
		bool m_cell_size_set;
	};

	class StartParam {
	public:
		ListView *m_list_view;
		int32_t m_segment;
		int32_t m_cell;
		ListItem *m_item;
	};

	typedef StartParam StopParam;
	typedef StartParam DisposeParam;

	virtual ~ItemFactory()
	{
	}

	virtual ListItem *Create(CreateParam &param);
	virtual void Start(StartParam &param);
	virtual void Stop(StopParam &param);
	virtual void Dispose(DisposeParam &param);
};

}

class ListView : public ScrollViewBase {
public:
	enum FocusAlignType {
		FocusAlignType_None = 0
	};

	static const char m_widget_type[];
	static const char *TypeName();

	ListView(Widget *parent, CreateParam *param);
	virtual ~ListView();

	/* the slots after ScrollViewBase's, from 0x26c (index 155) */
	virtual bool BeginDrag(int32_t segment, int32_t cell, int32_t index);
	virtual void EndDrag(int32_t index);
	virtual int32_t SetFocus(int32_t segment, int32_t cell, FocusAlignType align, Timer *timer);
	virtual void ScePafWidget_5EEE4313();
	virtual void OnCellUnGrab(listview::Cell *cell);
	virtual void OnCellDragMove(listview::Cell *cell);
	virtual void OnCellMoved(int32_t segment, int32_t from, int32_t to);
	virtual void OnCellDragStart(int32_t segment, int32_t cell);
	virtual void OnCellDragStop(int32_t segment, int32_t cell);
};

}
}

#endif
