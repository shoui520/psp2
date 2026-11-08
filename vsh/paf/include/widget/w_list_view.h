/*
 * paf::ui::ListView: a scroll view of segments of cells, each cell shown by a list item an item
 * factory makes while it is near the view (listview::Cell, listview::Segment), each segment
 * with a header a segment header factory makes. The classes, their methods, TypeName, the
 * parameter names and the exports are Sony's (export NIDs hash as them; "list_view" is its
 * string); the members' names are ours.
 */
#ifndef PAF_WIDGET_W_LIST_VIEW_H
#define PAF_WIDGET_W_LIST_VIEW_H

#include <stdint.h>
#include <common/property_list.h>
#include <common/shared_ptr.h>
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
class CoordinateEffector;
class ItemFactory;
class Segment;
class SegmentHeaderFactory;
class SegmentList;

/* a cell's place: its segment and its index there */
class ListPos {
public:
	int32_t m_segment;
	int32_t m_cell;
};

/* what a segment's sync is given: the range the cells are made in, the range shown and how the
   cells are placed */
class SyncParam {
public:
	math::v2 m_generate_range;
	math::v2 m_visible_range;
	const CoordinateEffector *m_effector;
};

/* a cell's size along the view's axis and where it starts from the segment's head (names
   ours) */
class CellSize {
public:
	float m_size;
	float m_pos;
};

/* the sizes of a variable layout's cells, by index, and their sum */
class CellSizeList : public vector<CellSize> {
public:
	iterator LowerBound(float pos);
	/* the cell at the position: the last one starting at or before it */
	iterator Find(float pos);

	float m_total;
};

/* the cells of a segment made so far, by index */
class CellList : public vector<common::SharedPtr<Cell> > {
public:
	CellList(Segment *segment);

	iterator LowerBound(int32_t index);
	/* the cell of the index, made where there is none */
	Cell *CreateCell(int32_t index);
	Cell *GetLast();
	void Sort();
	iterator Find(int32_t index);
	Cell *GetCell(int32_t index);

	Segment *m_segment;
};

/* a run of cells of a segment: the first and how many follow it (names ours) */
class CellRange {
public:
	int32_t m_first;
	int32_t m_num;
};

/* orders runs of cells by where they end (name ours) */
class CellRangeLess {
public:
	bool operator()(const CellRange &range, int32_t cell) const
	{
		return range.m_first + range.m_num < cell;
	}
};

/* the same order where cells are inserted (its own instance of lower_bound in Sony's binary;
   name ours) */
class CellRangeInsertLess {
public:
	bool operator()(const CellRange &range, int32_t cell) const
	{
		return range.m_first + range.m_num < cell;
	}
};

/* the runs of cells of a segment given a size (names ours) */
class SegmentCellRange {
public:
	/* its runs copied by a copy swapped in */
	SegmentCellRange &operator=(const SegmentCellRange &src)
	{
		m_segment = src.m_segment;
		vector<CellRange>(src.m_ranges).swap(m_ranges);
		return *this;
	}

	bool operator<(int32_t segment) const
	{
		return m_segment < segment;
	}

	int32_t m_segment;
	vector<CellRange> m_ranges;
};

/* orders a segment's cells by index, and cell sizes by where they start (names ours) */
class CellIndexLess {
public:
	bool operator()(const common::SharedPtr<Cell> &cell, int32_t index) const;
	/* the made cells sorted by index (CellList::Sort) */
	bool operator()(const common::SharedPtr<Cell> &a, const common::SharedPtr<Cell> &b) const;
};

class CellSizeLess {
public:
	bool operator()(const CellSize &size, float pos) const
	{
		return size.m_pos < pos;
	}
};

/* orders cells by segment, then index (name ours) */
class CellLess {
public:
	bool operator()(const Cell *a, const Cell *b) const;
	bool operator()(const Cell *cell, const ListPos &pos) const;
	bool operator()(const ListPos &pos, const Cell *cell) const;
};

/* every cell a list view has made, by segment and index */
class CellMap : public vector<Cell *> {
public:
	CellMap(ListView *list_view);

	iterator LowerBound(Cell *cell);
	iterator Insert(Cell *cell);
	iterator Find(Cell *cell);
	iterator Erase(Cell *cell);
	Cell *Find(int32_t segment, int32_t index);
	void Sort();
	Cell *Find(ListItem *item);

	~CellMap()
	{
		m_list_view = (ListView *)-1;
	}

	ListView *m_list_view;
};

/* a cell of a segment: its index, the list item shown for it and where it is */
class Cell {
public:
	Cell(CellList *list, int32_t index);
	~Cell();

	__declspec(dllexport) Segment *GetSegment() const
	{
		return m_cell_list->m_segment;
	}

	int32_t GetSegmentIndex() const;
	math::v2 GetPos(bool current);
	math::v4 GetRect(bool current);
	ListView *GetListView();
	void UpdateDragRange();
	void ResetPos(bool anim);
	/* moved by the offset, its item with it (no name found) */
	void ScePafWidget_87B4273E(const math::v2 &offset);
	void SetPos(const math::v2 &pos, bool anim);
	bool BeginDrag(int32_t index);
	void EndDrag(int32_t index);
	void FreeCellPosition(bool on_off);
	/* its item moved to where the list view places it, by the timer (name ours) */
	void MoveItem(Timer *timer);

	CellList *m_cell_list;
	int32_t m_index;
	ListItem *m_item;
	/* where the list view places it */
	math::v2 m_pos;
	bool m_started;
	/* placed by its owner, not by the list view (FreeCellPosition) */
	bool m_free_position;
	math::v2 m_effector_pos;
	float m_effector_value;
};

/* the cell a sync keeps in place, its segment, and where it was along the axis (names
   ours) */
class SyncAnchor {
public:
	Segment *m_segment;
	Cell *m_cell;
	float m_pos;
};

/* the segments of a list view, by index */
class SegmentList : public vector<common::SharedPtr<Segment> > {
public:
	SegmentList(ListView *list_view);
	~SegmentList()
	{
		m_list_view = (ListView *)-1;
	}

	Segment *GetSegment(int32_t index);
	bool InsertSegment(int32_t index, uint32_t num);
	bool DeleteSegment(int32_t index, uint32_t num);

	ListView *m_list_view;
};

/* how a list view places its cells and headers along its axis: where they are, the range kept
   within the limit (static objects of ListView::GetCoordinateEffector; names ours) */
class CoordinateEffector {
public:
	virtual ~CoordinateEffector()
	{
	}

	/* the cell moved to the position, which it keeps with half its size */
	virtual void SetCellPos(Cell *cell, const math::v2 &pos, float half) const
	{
		cell->SetPos(pos, true);
		cell->m_effector_pos = pos;
		cell->m_effector_value = half;
	}

	/* the header put at the position */
	virtual void SetHeaderPos(Widget *header, const math::v4 &pos, float half) const
	{
		header->SetPos(pos);
	}

	/* the range moved back within the limit */
	virtual math::v2 ClampRange(const math::v2 &range) const
	{
		using namespace sce::Vectormath::Simd;
		using namespace sce::Vectormath::Simd::Aos;

		Vector2 value = range.getImpl();
		math::v2 result = range;
		floatInVec head = m_limit.getImpl().getX();
		floatInVec tail = m_limit.getImpl().getY();

		if ((head > tail).getAsBool()) {
			if ((tail > value.getY()).getAsBool())
				result = math::v2(value + Vector2(tail - value.getY()));
			else if ((value.getX() > head).getAsBool())
				result = math::v2(value - Vector2(value.getX() - head));
		} else {
			if ((value.getY() > tail).getAsBool())
				result = math::v2(value - Vector2(value.getY() - tail));
			else if ((head > value.getX()).getAsBool())
				result = math::v2(value + Vector2(head - value.getX()));
		}
		return result;
	}

	/* the range a floating header of the length is kept in */
	virtual math::v2 GetHeaderRange(const math::v2 &range, float length) const
	{
		return range;
	}

	math::v2 m_limit;
};

}

class ListView : public ScrollViewBase {
public:
	/* what it sends its listeners: a cell moved, a cell drag begins and ends, it snaps (names
	   ours) */
	enum {
		CB_LIST_VIEW_CELL_MOVED = 0x10000007,
		CB_LIST_VIEW_DRAG_START = 0x10000008,
		CB_LIST_VIEW_DRAG_STOP = 0x10000009,
		CB_LIST_VIEW_SNAP = 0x1000000A
	};

	enum {
		/* the padding along the axis (SetAxisPadding; name ours) */
		ANIM_AXIS_PADDING = 0x1000E
	};

	enum LayoutType {
		LAYOUT_TYPE_LIST = 0,
		LAYOUT_TYPE_GRID = 1,
		LAYOUT_TYPE_LIST_VARIABLE = 2
	};

	enum ScrollType {
		SCROLL_TYPE_VERTICAL = 0,
		SCROLL_TYPE_HORIZONTAL = 1
	};

	/* how it snaps once a flick ends: to its scroll range's ends, to the cell the view stops
	   at, to the cell after the one shown when the touch let go, or to its segments (names
	   ours) */
	enum SnapMode {
		SNAP_MODE_NONE = 0,
		SNAP_MODE_CELL = 1,
		SNAP_MODE_PAGE = 2,
		SNAP_MODE_SEGMENT = 3
	};


	enum FocusAlignType {
		FOCUS_ALIGN_TYPE_HEAD = 0,
		FOCUS_ALIGN_TYPE_CENTER = 1,
		FOCUS_ALIGN_TYPE_TAIL = 2,
		/* moved only as far as the cell needs to be shown, where it fits (name ours) */
		FOCUS_ALIGN_TYPE_NEAR = 3
	};

	static const char m_widget_type[];
	static const char *TypeName();

	ListView(Widget *parent, CreateParam *param);
	virtual ~ListView();

	virtual bool IsInherit(const char *type) const;
	virtual int32_t HandleTouchEvent(Event *e);
	virtual char *GetTypeName() const;
	virtual void DestroyWidget();
	virtual void UpdateSize();
	virtual void OnPartsLayoutUpdated(bool is_dirty);
	virtual void UpdateMovableDirection();
	virtual int32_t HandleKeycodeEvent(Event *e);
	virtual void ScePafWidget_22EF63CE();
	/* each frame: the focus moved on into its cells, taken or let go */
	virtual void ScePafWidget_B6CECFDF();
	/* the position the focus takes the view to: the cell, segment or end of the scroll range
	   it snaps to (no name found) */
	virtual bool ScePafWidget_C8C679BB(math::v2 &pos, const math::v2 &focus);
	/* the focus root's focus set; a change between two widgets is passed to OnFocusLeave */
	virtual void SetFocusRootState(uint32_t state);
	virtual bool OnFocusIn(Event *e);
	/* the view moved, its cells synced to it */
	virtual void MoveViewParent(const math::v2 &pos, Timer *timer);
	virtual void ScePafWidget_D95AFF11(math::v2 &move, bool dragging, bool locked, bool reset);
	/* a touch let go: its focus is dropped, and where the view still moves the time and the
	   cell it goes to are kept */
	virtual void ScePafWidget_E9C7617A(uint32_t touch_id);
	/* the focus moved the way given from the widget (or the focus): to a header, along its
	   cells or out of it, the view scrolling where it cannot move */
	virtual bool ScePafWidget_AD706F2A(const math::v2 &dir, Widget *from);
	/* a flick along its axis keeps the focus where it is (m_focus_pending) */
	virtual void ScePafWidget_D9FE36A6(int32_t id, math::v2 &dir, math::v2 &velocity);
	virtual bool OnFocusEnter(Event *e);
	virtual bool OnFocusLeave(Event *e);
	virtual bool OnFocusOut(Event *e);

	/* the slots after ScrollViewBase's, from 0x26c (index 155) */
	virtual void BeginDrag(int32_t segment, int32_t cell, int32_t index);
	virtual void EndDrag(int32_t index);
	virtual void SetFocus(int32_t segment, int32_t cell, FocusAlignType align, Timer *timer);
	/* a key moving the cell dragged: it moves in its segment (17 press, 19 repeat) and the
	   view shows it; true where it took the key (no name found) */
	virtual bool ScePafWidget_5EEE4313(Event *e);
	virtual void OnCellUnGrab(listview::Cell *cell);
	virtual void OnCellDragMove(listview::Cell *cell);
	virtual void OnCellMoved(int32_t segment, int32_t from, int32_t to);
	virtual void OnCellDragStart(int32_t segment, int32_t cell);
	virtual void OnCellDragStop(int32_t segment, int32_t cell);

	/* the range of the view along its axis (name ours) */
	math::v2 GetVisibleRange(int32_t opt);
	/* the cells shown, counted (no name found) */
	int32_t ScePafWidget_9D5DB5D6();
	/* each segment's header made or disposed of as it comes into or leaves the range, the
	   first kept and the made ones put in front of the one before (no name found) */
	void ScePafWidget_433FAAD4(const math::v2 &range);
	/* the touches it holds let go of where the view has gone past its scroll box (name ours) */
	void ReleaseTouchOutOfRange();
	/* the cell or segment header a sync keeps in place, where the layout changed (no name
	   found) */
	void ScePafWidget_96B46364(listview::SyncAnchor &anchor);
	/* the view moved to keep the anchor's cell where it was (no name found) */
	void ScePafWidget_B04BFBAB(const listview::SyncAnchor &anchor);
	/* the view moved back by the offset, the cells from the segment's cell on with it
	   (name ours) */
	void MoveAnchored(int32_t segment, int32_t cell, math::v2 offset);
	math::v2 GetGenerateRange(const math::v2 &visible_range);
	const listview::CoordinateEffector *GetCoordinateEffector();
	void SyncViewSize();
	void Sync();
	/* the padding at the head and the tail along the axis (name ours) */
	void SetAxisPadding(const math::v2 &padding);
	/* the padding at the head and the tail along the axis, or where its animation goes (no
	   name found) */
	math::v2 ScePafWidget_B778A1D6(bool target);
	/* the padding at the head and the tail along the axis set, animated by the timer where
	   there is one (no name found) */
	void ScePafWidget_5B94F2B4(const math::v2 &padding, Timer *timer);
	void Rebuild();
	void SetScrollType(ScrollType type);
	void SetGenerateMargin(const math::v2 &margin);
	listview::Segment *GetSegment(int32_t index);
	Widget *GetSegmentHeader(int32_t segment);
	void InsertCell(int32_t segment, int32_t cell, uint32_t num);
	void DeleteCell(int32_t segment, int32_t cell, uint32_t num);
	void MoveCell(int32_t segment, int32_t from, int32_t to);
	uint32_t GetCellNum(int32_t segment);
	void InsertSegment(int32_t segment, uint32_t num);
	void DeleteSegment(int32_t segment, uint32_t num);
	uint32_t GetSegmentNum();
	/* the cell size new segments start with (no name found) */
	void ScePafWidget_FFD97FCD(const math::v4 &size);
	math::v4 ScePafWidget_C6559467();
	void SetCellSizeDefault(int32_t segment, const math::v4 &size);
	math::v4 GetCellSizeDefault(int32_t segment);
	void SetCellSize(int32_t segment, int32_t cell, const math::v4 &size);
	math::v4 GetCellSize(int32_t segment, int32_t cell);
	void FreeCellPosition(int32_t segment, int32_t cell, bool on_off);
	/* the item of the cell moved to its place by the timer (no name found) */
	void ScePafWidget_E16070BC(int32_t segment, int32_t cell, Timer *timer);
	void SetSegmentLayoutType(int32_t segment, LayoutType type);
	LayoutType GetSegmentLayoutType(int32_t segment);
	/* the columns a grid segment is given, 0 to fit the view (no name found) */
	void ScePafWidget_27EB2AEB(int32_t segment, int32_t num);
	/* the least size a segment takes (no name found) */
	void ScePafWidget_7E9DC5B1(int32_t segment, const math::v2 &size);
	math::v2 ScePafWidget_1568FEF6(int32_t segment);
	/* a segment joined to the segments next to it (name ours) */
	void SetSegmentJoined(int32_t segment, bool joined);
	void SetSegmentHeader(int32_t segment, Widget *header, bool floating, bool disappearance);
	/* a segment given a header made by the header factory, of the size along the axis (no
	   name found) */
	void ScePafWidget_7AC07A92(int32_t segment, const math::v2 &size, bool floating,
				   bool disappearance);
	/* the size a segment's header takes (no name found) */
	math::v4 ScePafWidget_89DDBD65(int32_t segment);
	/* the factory that makes the segments' headers (no name found) */
	void ScePafWidget_9556E18A(listview::SegmentHeaderFactory *factory);
	void SetItemFactory(listview::ItemFactory *factory);
	/* where the view goes to show the cell aligned so, kept in the scroll box; -1 for no
	   such segment (no name found) */
	int32_t ScePafWidget_B3B40B75(math::v2 *pos, int32_t segment, int32_t cell,
				      FocusAlignType align);
	/* the view's focus set, the focus kept on a cell shown: where the focused one is not,
	   the first one shown after it, else the last one shown */
	virtual void SetViewFocus(const math::v2 &pos, Timer *timer, bool flag_a, bool flag_b);
	/* the scroll range given, the coordinate effector and the over scroll off; or the range
	   dropped (no name found) */
	void ScePafWidget_E1824581(bool on, const math::v2 &range);
	/* the coordinate effector on, the scroll range dropped and the over scroll off (no name
	   found) */
	__attribute__((noinline)) void ScePafWidget_9D82904C(bool on);
	/* the cells given a size kept, by segment, while on (no name found) */
	void ScePafWidget_40347A42(bool on);
	/* the least size its view box takes, synced where it changes (no name found) */
	void ScePafWidget_98FD53BF(const math::v2 &size);
	/* the segment and cell at the position; cell -1 on a segment's header, both -1 for none
	   (no name found) */
	listview::ListPos ScePafWidget_FD8DE7ED(const math::v2 &pos);
	/* the first child of a cell's item the focus may go to, NULL for none (no name found) */
	Widget *ScePafWidget_20824179();
	/* once a flick ends: the view snapped to the ends of its scroll range, or by its snap mode
	   to a cell or a segment (no name found) */
	void ScePafWidget_697CD413();
	/* each interval-th frame the focus moved the way it goes, to a cell shown where search is
	   set, and taken by the list view where take is set (name ours) */
	void UpdateFocus(bool search, bool take, uint32_t interval);
	/* the item of the index-th cell shown (no name found) */
	ListItem *ScePafWidget_E744731A(int32_t index);
	ListItem *GetListItem(int32_t segment, int32_t cell);
	int32_t GetGeneratedNum();
	listview::ListPos GetGeneratedPos(int32_t index);
	common::property_list::PropertyList *GetSegmentPropertyList(int32_t segment);
	void BlockSync();
	void UnblockSync();
	void Clear();
	/* the visible range as a rectangle grown by 3 (name ours) */
	math::v4 GetVisibleRect(int32_t opt);
	/* the item of the cell the focus is to go to given the focus (no name found) */
	void ScePafWidget_A838831E();
	/* the cell shown whose item takes the focus, searched from the end the direction points
	   to; -1, -1 for none (name ours) */
	listview::ListPos FindFocusCell(const math::v2 &dir);
	/* the cell is shown, wholly or with an end in the visible range (name ours) */
	bool IsCellVisible(listview::Cell *cell, bool whole);
	/* the focus target dropped, the focus given to a cell shown (no name found) */
	void ScePafWidget_7F28F5B7();
	/* the first segment header kept (no name found) */
	void ScePafWidget_8AB4F072();
	/* the place before the given one, headers included where asked (no name found) */
	listview::ListPos ScePafWidget_8DC5A8AE(const listview::ListPos &pos, bool header);
	/* the place after the given one, headers included where asked (no name found) */
	listview::ListPos ScePafWidget_E60EAA9F(const listview::ListPos &pos, bool header);
	/* the place of the segment before the given one's (no name found) */
	listview::ListPos ScePafWidget_814B970E(const listview::ListPos &pos, bool header);
	/* the place of the segment after the given one's (no name found) */
	listview::ListPos ScePafWidget_DDC428E9(const listview::ListPos &pos, bool header);
	void FocusRect(const math::v4 &rect, Timer *timer);
	math::v2 GetFocusedViewPos();

	ScrollType m_scroll_type;
	math::v2 m_generate_margin;
	/* the least size its view box takes (name ours) */
	math::v2 m_min_size;
	/* the cell size a new segment starts with */
	math::v4 m_cell_size_default;
	/* syncs are left for later while it is not 0 (BlockSync) */
	int32_t m_sync_block;
	/* set while it syncs itself */
	int32_t m_syncing;
	listview::SegmentList *m_segment_list;
	listview::SegmentHeaderFactory *m_header_factory;
	listview::ItemFactory *m_item_factory;
	listview::CellMap *m_cell_map;
	common::property_list::PropertyList m_property_list;
	/* counts its layout changes */
	int32_t m_layout_count;
	/* the length its segments take along its axis (name ours) */
	float m_length;
	/* the cells given a size, by segment */
	vector<listview::SegmentCellRange> *m_cell_size_set;
	/* the first header and the cell being dragged */
	Widget *m_first_header;
	listview::Cell *m_drag_cell;
	/* where the dragged cell's item was when the drag began (name ours) */
	math::v2 m_drag_item_pos;
	/* the coordinate effector's overshoot, and the effector used last */
	float m_effector_offset;
	const listview::CoordinateEffector *m_effector;
	bool m_effector_enabled;
	/* the scroll range given with ScePafWidget_E1824581, and whether it is set */
	math::v2 m_scroll_range;
	bool m_scroll_range_set;
	/* how it snaps once a flick ends (names ours) */
	SnapMode m_snap_mode;
	/* when the last touch let go of it while it moved, 0 for not, and the cell it showed then
	   (names ours) */
	SceUInt64 m_release_time;
	listview::ListPos m_release_pos;
	bool m_focus_pending;
	/* the cell (or, at -1, the header) the focus is on (name ours) */
	listview::ListPos m_focus_list_pos;
	listview::Cell *m_grab_cell;
	/* a sync is due, and the syncs left for later are being run */
	bool m_sync_pending;
	bool m_sync_running;
	math::v2 m_focus_pos;
	bool m_sync_enabled;
};

namespace listview {

/* a segment of a list view: its cells, how they are laid out, the size they take and its
   header */
class Segment {
public:
	Segment(SegmentList *list, int32_t index);
	~Segment();

	math::v4 GetCellSize(int32_t index);
	void SetHeaderPos(float pos, const CoordinateEffector &effector);
	math::v4 GetHeaderSize();
	void SetHeaderVisible(bool visible);
	void ResetHeaderPosition(const math::v2 &range, const CoordinateEffector &effector);
	void ResetCellPosition(const CoordinateEffector &effector);
	void GenerateCell(int32_t first, int32_t last);
	void GetGenerationIndex(int32_t &first, int32_t &last, const math::v2 &range);
	void Sync(const SyncParam &param);
	/* the header made where the segment is in the range, disposed of where it is not; true
	   where it was made (no name found) */
	bool ScePafWidget_E8717C5D(const math::v2 &range);
	/* its header given back to the header factory where a size is given to it (no name
	   found) */
	void ScePafWidget_F53FD2D0();
	/* where the cell is put to be aligned the way given, and its length (no name found) */
	math::v2 ScePafWidget_7A37CEDA(int32_t index, ListView::FocusAlignType align, float *size);
	void UpdateViewSize();
	void DestroyCell(int32_t index, uint32_t num);
	bool InsertCell(int32_t index, uint32_t num);
	bool DeleteCell(int32_t index, uint32_t num);
	bool MoveCell(int32_t from, int32_t &to);
	bool SetCellSize(int32_t index, const math::v4 &size);
	bool SetLayoutType(ListView::LayoutType type);
	int32_t GetCellIndex(const math::v2 &pos);
	int32_t GetDropIndex(const math::v2 &pos);
	/* where the view goes to show the cell aligned so, a header (index below 0) by its size
	   (no name found) */
	math::v2 ScePafWidget_CA6247CF(int32_t index, ListView::FocusAlignType align);
	/* the view's range along the axis over the segments joined to it, its middle where they
	   fit in the view (no name found) */
	math::v2 ScePafWidget_D33B4AF3();

	SegmentList *m_segment_list;
	int32_t m_index;
	int32_t m_cell_num;
	math::v4 m_cell_size_default;
	ListView::LayoutType m_layout_type;
	CellList m_cell_list;
	CellSizeList *m_cell_size_list;
	/* the size its cells and header take */
	math::v4 m_view_size;
	common::property_list::PropertyList m_property_list;
	Widget *m_header;
	bool m_header_floating;
	bool m_header_disappearance;
	bool m_header_visible;
	float m_header_size;
	/* the columns a grid is given, or 0 to fit the view (ScePafWidget_27EB2AEB) */
	int32_t m_fixed_column_num;
	/* the least size it takes (ScePafWidget_7E9DC5B1) */
	math::v2 m_min_view_size;
	/* joined to the segments next to it where they are too (sub_810FA7BA) */
	bool m_joined;
	/* where it starts along the view's axis, and a grid's columns and rows */
	float m_pos;
	int32_t m_column_num;
	int32_t m_row_num;
	bool m_synced;
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
		/* a size was given to the cell */
		bool m_cell_size_set;
		bool reserved;
		uint16_t reserved2;

		CreateParam() : m_cell_size_set(false), reserved(false), reserved2(0)
		{
		}
	};

	class StartParam {
	public:
		ListView *m_list_view;
		int32_t m_segment;
		int32_t m_cell;
		ListItem *m_item;
	};

	class StopParam {
	public:
		ListView *m_list_view;
		int32_t m_segment;
		int32_t m_cell;
		ListItem *m_item;
	};

	class DisposeParam {
	public:
		ListView *m_list_view;
		int32_t m_segment;
		int32_t m_cell;
		ListItem *m_item;
	};

	virtual ~ItemFactory();
	/* a list item of the cell's segment's default size */
	virtual ListItem *Create(CreateParam &param);
	virtual void Start(StartParam &param);
	virtual void Stop(StopParam &param);
	/* the item destroyed */
	virtual void Dispose(DisposeParam &param);
};

/* makes and disposes of the headers of a list view's segments */
class SegmentHeaderFactory {
public:
	class CreateParam {
	public:
		ListView *m_list_view;
		int32_t m_segment;
		Widget *m_parent;
	};

	class DisposeParam {
	public:
		ListView *m_list_view;
		int32_t m_segment;
		Widget *m_header;
	};

	virtual ~SegmentHeaderFactory();
	/* none by default */
	virtual Widget *Create(CreateParam &param);
	/* the header destroyed */
	virtual void Dispose(DisposeParam &param);
};

}

class ListViewParam : public ScrollViewBaseParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	virtual const char *GetParamName() const;
};

class ListViewStyleParam : public ScrollViewBaseStyleParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	ListViewStyleParam();
	virtual const char *GetParamName() const;
	virtual int32_t Create(int32_t mode);
	virtual int32_t SetEach(Widget *widget);
	virtual int32_t GetEach(Widget *widget);

	/* the axis padding (its resource parameter "view_margin") */
	math::v2 m_view_margin;
	math::v4 m_cell_size_default;
};

}
}

#endif
