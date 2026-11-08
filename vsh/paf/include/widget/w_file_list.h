/*
 * paf::ui::FileList: a list view whose segments and cells come from sources (filelist::
 * SegmentSource, filelist::CellSource), each cell shown by a list item a visualizer gives
 * (filelist::Visualizer) or a plain one, and the job queues the sources load on
 * (filelist::Manager). The classes, their methods, TypeName and the exports are Sony's (export
 * NIDs hash as them; "file_list", "cell_source", "segment_source", "visualizer" are its
 * strings); the members' names are ours.
 */
#ifndef PAF_WIDGET_W_FILE_LIST_H
#define PAF_WIDGET_W_FILE_LIST_H

#include <stddef.h>
#include <stdint.h>
#include <common/ref_obj.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <std/memory>
#include <std/string>
#include <std/vector>
#include <widget/w_list_item.h>
#include <widget/w_list_view.h>

namespace paf {

class Timer;

namespace job {
class JobQueue;
}

namespace ui {

class FileList;

namespace filelist {

class CellSource;
class SegmentSource;
class Visualizer;

/* what a cell source is given to find its cell's visualizer: the cell, its ID and the parent
   its list item goes under */
class VisualizerBindParam {
public:
	int32_t m_index;
	int64_t m_id;
	Widget *m_parent;
};

/* what a segment source is given to make its segment's header: the segment, its ID, the parent
   the header goes under and the size the list view gives it (names ours) */
class HeaderBindParam {
public:
	int32_t m_index;
	int64_t m_id;
	Widget *m_parent;
	math::v4 m_size;
};

/* the ID of a segment or cell the list view has an item for, and its index */
class IDPair {
public:
	IDPair(int64_t id, int32_t index) : m_id(id), m_index(index)
	{
	}

	int64_t m_id;
	int32_t m_index;
};

/* the cell a cell source asks the focus to go to once the sources are updated */
class CellFocusParam {
public:
	CellFocusParam();
	~CellFocusParam();

	/* the focus set on the cell, where one is asked for */
	void Apply(FileList *file_list);
	void Set(int32_t segment, int32_t cell, ListView::FocusAlignType align, Timer *timer);

	int32_t m_segment;
	int32_t m_cell;
	ListView::FocusAlignType m_align;
	Timer *m_timer;
};

/* what a cell source updates its segment's cells through */
class CellListAccessor {
public:
	CellListAccessor(FileList *file_list, int32_t segment, CellFocusParam *focus_param);

	virtual ~CellListAccessor()
	{
	}

	virtual void Insert(int32_t index, uint32_t num);
	virtual void Delete(int32_t index, uint32_t num);
	virtual uint32_t GetNum();
	/* the IDs of the cells the list view has items for */
	virtual void GetExists(vector<IDPair> &list);
	/* the list item of the cell, or NULL (no name found) */
	virtual ListItem *ScePafWidget_95A143BF(int32_t index);
	virtual void SetFocus(int32_t index, ListView::FocusAlignType align, Timer *timer);

	FileList *m_file_list;
	int32_t m_segment;
	CellFocusParam *m_focus_param;
};

/* what a segment source updates the segments through */
class SegmentListAccessor {
public:
	/* how a segment is laid out, its header and its columns */
	class OptionParam {
	public:
		ListView::LayoutType m_layout_type;
		Widget *m_header;
		/* a header the header factory makes, of this size, where it is not 0 */
		math::v4 m_header_size;
		bool m_header_floating;
		bool m_header_disappearance;
		int32_t m_column_num;
		math::v2 m_min_size;
		bool m_joined;
	};

	SegmentListAccessor(FileList *file_list);

	virtual ~SegmentListAccessor()
	{
	}

	virtual void Insert(int32_t index, uint32_t num);
	virtual void Delete(int32_t index, uint32_t num);
	virtual uint32_t GetNum();
	/* the IDs of the segments the list view has items for */
	virtual void GetExists(vector<IDPair> &list);
	virtual void SetOption(int32_t index, OptionParam &param);

	FileList *m_file_list;
};

/* what shows a cell: the list item it gives, started and stopped with the cell */
class Visualizer : public common::RefObj {
public:
	static const char *TypeName();

	virtual ~Visualizer()
	{
	}

	virtual bool IsInherit(const char *type) const;
	virtual const char *GetTypeName() const;
	virtual void Start();
	virtual void Stop();
	virtual ListItem *GetListItem();

	static const char m_type_name[];
};

/* a segment's cells: their IDs, visualizers and sizes */
class CellSource : public common::RefObj {
public:
	static const char *TypeName();

	virtual ~CellSource()
	{
	}

	virtual bool IsInherit(const char *type) const;
	virtual const char *GetTypeName() const;
	virtual bool IsDirty();
	virtual void Update(CellListAccessor &accessor);
	virtual int64_t GetID(int32_t index);
	virtual intrusive_ptr<Visualizer> GetVisualizer(VisualizerBindParam &param);
	virtual math::v4 GetVisualSize();
	virtual bool IsDraggable();
	virtual void OnMoved(int32_t from, int32_t to);
	virtual void OnDragStart(int32_t index);
	virtual void OnDragStop(int32_t index);
	virtual void OnUpdated();

	static const char m_type_name[];
};

/* the segments: their IDs, cell sources and headers */
class SegmentSource : public common::RefObj {
public:
	static const char *TypeName();

	virtual ~SegmentSource()
	{
	}

	virtual bool IsInherit(const char *type) const;
	virtual const char *GetTypeName() const;
	virtual bool IsDirty();
	virtual void Update(SegmentListAccessor &accessor);
	virtual int64_t GetID(int32_t index);
	virtual intrusive_ptr<CellSource> GetCellSource(int32_t index, int64_t id);
	virtual void OnUpdated();
	/* the segment's header, none by default (no name found) */
	virtual Widget *ScePafWidget_BC736EE9(HeaderBindParam &param);
	/* the segment's header disposed of, destroyed by default (no name found) */
	virtual void ScePafWidget_6B4F7595(int32_t index, int64_t id, Widget *header);

	static const char m_type_name[];
};

/* the list items of a file list: made by its CreateItem, ... */
class FileItemFactory : public listview::ItemFactory {
public:
	virtual ~FileItemFactory()
	{
	}

	virtual ListItem *Create(CreateParam &param);
	virtual void Start(StartParam &param);
	virtual void Stop(StopParam &param);
	virtual void Dispose(DisposeParam &param);
};

/* the segment headers of a file list, from its segment source (name ours) */
class FileSegmentHeaderFactory : public listview::SegmentHeaderFactory {
public:
	virtual ~FileSegmentHeaderFactory()
	{
	}

	virtual Widget *Create(CreateParam &param);
	virtual void Dispose(DisposeParam &param);
};

int64_t GetCellID(ListItem *item);
intrusive_ptr<Visualizer> GetVisualizer(ListItem *item);

namespace Manager {

enum QueueType {
	QUEUE_TYPE_FAST = 0,
	QUEUE_TYPE_NORMAL = 1,
	QUEUE_TYPE_SLOW = 2,
	QUEUE_TYPE_LONG = 3,
	QUEUE_TYPE_NUM = 4
};

/* where the sources keep their files, and the job queues: given, made (with these priorities
   and stack sizes) or none */
class InitParam {
public:
	InitParam(const string &storage_path);

	string m_storage_path;
	bool m_create_queue;
	int32_t m_priority[QUEUE_TYPE_NUM];
	size_t m_stack_size[QUEUE_TYPE_NUM];
	job::JobQueue *m_queue[QUEUE_TYPE_NUM];
};

/* a list item of the type kept for later under the parent, or NULL */
ListItem *PopCacheItem(uint32_t type, Widget *parent);
/* the list item hidden and kept for later, by type, under its parent */
void PushCacheItem(uint32_t type, ListItem *item);
void Initialize(const InitParam &param);
void Finalize();
job::JobQueue *GetJobQueue(QueueType type);
string *GetStoragePath();
void PostTask(QueueType type, const char *name, void (*func)(void *), void *userdata);

}

}

class FileList : public ListView {
public:
	static const char m_widget_type[];
	static const char *TypeName();

	FileList(Widget *parent, CreateParam *param);
	virtual ~FileList();

	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	/* the sources updated first */
	virtual void Update();
	/* only a cell its source lets drag */
	virtual void BeginDrag(int32_t segment, int32_t cell, int32_t index);
	virtual void OnCellMoved(int32_t segment, int32_t from, int32_t to);
	virtual void OnCellDragStart(int32_t segment, int32_t cell);
	virtual void OnCellDragStop(int32_t segment, int32_t cell);
	/* the segments and cells their sources say are dirty updated (no name found) */
	virtual void ScePafWidget_69860A6F();

	void SetSegmentSource(intrusive_ptr<filelist::SegmentSource> source);
	int64_t GetSegmentID(int32_t segment);
	intrusive_ptr<filelist::CellSource> GetCellSource(int32_t segment);

	ListItem *CreateItem(listview::ItemFactory::CreateParam &param);
	void StartItem(listview::ItemFactory::StartParam &param);
	void StopItem(listview::ItemFactory::StopParam &param);
	void DisposeItem(listview::ItemFactory::DisposeParam &param);
	/* the segment's header, from the segment source (no name found) */
	Widget *ScePafWidget_2FF0AEC2(listview::SegmentHeaderFactory::CreateParam &param);
	/* the segment's header disposed of by the segment source (no name found) */
	void ScePafWidget_789C5142(listview::SegmentHeaderFactory::DisposeParam &param);

	intrusive_ptr<filelist::SegmentSource> m_segment_source;
};

namespace filelist {

inline ListItem *FileItemFactory::Create(CreateParam &param)
{
	return static_cast<FileList *>(param.m_list_view)->CreateItem(param);
}

inline void FileItemFactory::Start(StartParam &param)
{
	static_cast<FileList *>(param.m_list_view)->StartItem(param);
}

inline void FileItemFactory::Stop(StopParam &param)
{
	static_cast<FileList *>(param.m_list_view)->StopItem(param);
}

inline void FileItemFactory::Dispose(DisposeParam &param)
{
	static_cast<FileList *>(param.m_list_view)->DisposeItem(param);
}

inline Widget *FileSegmentHeaderFactory::Create(CreateParam &param)
{
	return static_cast<FileList *>(param.m_list_view)->ScePafWidget_2FF0AEC2(param);
}

inline void FileSegmentHeaderFactory::Dispose(DisposeParam &param)
{
	static_cast<FileList *>(param.m_list_view)->ScePafWidget_789C5142(param);
}

}

}
}

#endif
