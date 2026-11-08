/*
 * paf::ui::FileList (TOOL 0x810ee9ec..0x810f013c): a list view whose segments and cells come
 * from a segment source and its cell sources, the cells' items from their visualizers, and the
 * job queues and the cache of list items the sources share (filelist::Manager). No record or
 * string names the file.
 */
#include <stddef.h>
#include <stdint.h>
#include <sce_atomic.h>
#include <common/job_call_queue.h>
#include <common/main_thread_call_list.h>
#include <common/property_list.h>
#include <common/shared_ptr.h>
#include <job/job.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <std/map>
#include <std/memory>
#include <std/stdio.h>
#include <std/string>
#include <std/vector>
#include <thread/thread.h>
#include <timer/timer.h>
#include <versatileparam/resourceparam.h>
#include <widget/factory.h>
#include <widget/w_file_list.h>
#include <widget/w_list_item.h>
#include <widget/w_list_view.h>

namespace paf {
namespace ui {
namespace filelist {

/* a segment's cell source, kept alive by the segment's properties (names ours; its type name
   is Sony's string) */
class CellSourceElement : public common::property_list::PropertyElement {
public:
	CellSourceElement(intrusive_ptr<CellSource> source) : m_source(source)
	{
	}

	virtual bool IsInherit(const char *type_name) const;
	virtual const char *GetTypeName() const;

	virtual ~CellSourceElement()
	{
	}

	static const char m_type_name[];

	intrusive_ptr<CellSource> m_source;
};

/* a job that counts itself done once its queue has run it: Finalize waits for the queues so
   (names ours; "ScePafFileListCountJob" is Sony's string) */
class CountJob : public job::JobItem {
public:
	CountJob(const char *name, volatile int32_t *counter) : JobItem(name, NULL), m_counter(counter)
	{
	}

	virtual ~CountJob()
	{
	}

	virtual int Run()
	{
		return 0;
	}

	virtual void Finish(int result)
	{
		sceAtomicIncrement32AcqRel(m_counter);
	}

	volatile int32_t *m_counter;
};

/* the list items kept for later, by their parent and type, each parent's dropped when it is
   deleted (names ours) */
class ListItemCache {
public:
	typedef vector<ListItem *> ItemList;
	typedef map<uint32_t, common::SharedPtr<ItemList> > TypeMap;
	typedef map<Widget *, common::SharedPtr<TypeMap> > ParentMap;

	static void OnParentDeleted(Widget *parent, void *data)
	{
		ParentMap *items = (ParentMap *)data;

		items->erase(items->find(parent));
	}

	/* the items of the type under the parent, an empty list made where there is none */
	ItemList &Get(uint32_t type, Widget *parent)
	{
		ParentMap::iterator it = m_items.find(parent);

		if (it == m_items.end()) {
			parent->AddDeleteCB(OnParentDeleted, &m_items);
			it = m_items.insert(ParentMap::value_type(
						    parent, common::SharedPtr<TypeMap>(new TypeMap)))
				     .first;
		}

		common::SharedPtr<TypeMap> &types = it->second;
		TypeMap::iterator jt = types->find(type);

		if (jt == types->end())
			jt = types->insert(TypeMap::value_type(
						   type, common::SharedPtr<ItemList>(new ItemList)))
				     .first;
		return *jt->second;
	}

	ParentMap m_items;
};

/* what filelist::Manager keeps (names ours): the job queues, made or given, a call queue on
   each, the storage path, the file lists there are, and the list items kept for later */
class ManagerImpl {
public:
	ManagerImpl(const Manager::InitParam &param);
	~ManagerImpl();

	/* file lists remain, or a queue has work */
	bool IsBusy();
	void PushCacheItem(uint32_t type, ListItem *item);
	ListItem *PopCacheItem(uint32_t type, Widget *parent);
	/* the manager deleted once its last user finalizes and the queues are idle; else every
	   queue run empty of what came before */
	static void Finalize();

	job::JobQueue *m_queue[Manager::QUEUE_TYPE_NUM];
	bool m_own_queue[Manager::QUEUE_TYPE_NUM];
	common::JobCallQueue *m_call_queue[Manager::QUEUE_TYPE_NUM];
	string m_storage_path;
	volatile int32_t m_file_list_count;
	ListItemCache m_cache;
};

static void CheckCallQueues(void *data);
static int SyncNop(void *arg);

/* the list view's syncs held off while blocked, let go of at the end (names ours) */
class SyncBlocker {
public:
	SyncBlocker(ListView *view) : m_view(view), m_blocked(false)
	{
	}

	~SyncBlocker()
	{
		if (m_blocked)
			m_view->UnblockSync();
	}

	void Block()
	{
		if (!m_blocked) {
			m_blocked = true;
			m_view->BlockSync();
		}
	}

	void Unblock()
	{
		if (m_blocked) {
			m_blocked = false;
			m_view->UnblockSync();
		}
	}

	ListView *m_view;
	bool m_blocked;
};

}

using filelist::CellSource;
using filelist::SegmentSource;
using filelist::Visualizer;
using common::property_list::PropertyList;
using common::property_list::PropertyValue;

/* the manager the file lists and the sources share (names ours) */
static filelist::ManagerImpl *s_manager;

FileList::FileList(Widget *parent, CreateParam *param) : ListView(parent, param)
{
	sceAtomicIncrement32AcqRel(&s_manager->m_file_list_count);
	SetItemFactory(new filelist::FileItemFactory);
	ScePafWidget_9556E18A(new filelist::FileSegmentHeaderFactory);
}

const char FileList::m_widget_type[] = "file_list";

bool FileList::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return ListView::IsInherit(type);
}

char *FileList::GetTypeName() const
{
	return (char *)m_widget_type;
}

const char *FileList::TypeName()
{
	return m_widget_type;
}

void FileList::SetSegmentSource(intrusive_ptr<SegmentSource> source)
{
	Clear();
	m_segment_source = source;
}

int64_t FileList::GetSegmentID(int32_t segment)
{
	return GetSegmentPropertyList(segment)->GetValue(0).m_value;
}

intrusive_ptr<CellSource> FileList::GetCellSource(int32_t segment)
{
	return intrusive_ptr<CellSource>(
		(CellSource *)GetSegmentPropertyList(segment)->GetValue(1).m_pointer);
}

void FileList::BeginDrag(int32_t segment, int32_t cell, int32_t index)
{
	intrusive_ptr<CellSource> source = GetCellSource(segment);

	if (source.get() != NULL && source->IsDraggable())
		ListView::BeginDrag(segment, cell, index);
}

FileList::~FileList()
{
	sceAtomicDecrement32AcqRel(&s_manager->m_file_list_count);
}

void FileList::Update()
{
	ScePafWidget_69860A6F();
	ListView::Update();
}

void FileList::OnCellMoved(int32_t segment, int32_t from, int32_t to)
{
	intrusive_ptr<CellSource> source = GetCellSource(segment);

	if (source.get() != NULL)
		source->OnMoved(from, to);
	ListView::OnCellMoved(segment, from, to);
}

void FileList::OnCellDragStart(int32_t segment, int32_t cell)
{
	intrusive_ptr<CellSource> source = GetCellSource(segment);

	if (source.get() != NULL)
		source->OnDragStart(cell);
	ListView::OnCellDragStart(segment, cell);
}

void FileList::OnCellDragStop(int32_t segment, int32_t cell)
{
	intrusive_ptr<CellSource> source = GetCellSource(segment);

	if (source.get() != NULL)
		source->OnDragStop(cell);
	ListView::OnCellDragStop(segment, cell);
}

namespace filelist {

void CellFocusParam::Apply(FileList *file_list)
{
	if (m_segment >= 0) {
		file_list->SetFocus(m_segment, m_cell, m_align, m_timer);
		m_segment = -1;
		m_timer = NULL;
	}
}

}

void FileList::ScePafWidget_69860A6F()
{
	filelist::SyncBlocker blocker(this);

	if (m_segment_source.get() != NULL && m_segment_source->IsDirty()) {
		blocker.Block();

		filelist::SegmentListAccessor accessor(this);

		m_segment_source->Update(accessor);
		m_segment_source->OnUpdated();

		int32_t num = GetSegmentNum();

		for (int32_t i = 0; i < num; i++) {
			PropertyList *list = GetSegmentPropertyList(i);

			if (!list->m_value_list.empty())
				continue;

			int64_t id = m_segment_source->GetID(i);
			intrusive_ptr<CellSource> source = m_segment_source->GetCellSource(i, id);

			if (source.get() != NULL)
				SetCellSizeDefault(i, source->GetVisualSize());

			PropertyValue value;

			value.m_value = id;
			list->SetValue(0, value);
			value.m_pointer = source.get();
			list->SetValue(1, value);
			list->SetElement(1, common::SharedPtr<common::property_list::PropertyElement>(
						    (common::property_list::PropertyElement *)
							    new filelist::CellSourceElement(source)));
		}
	}

	filelist::CellFocusParam focus;
	int32_t num = GetSegmentNum();

	for (int32_t i = 0; i < num; i++) {
		intrusive_ptr<CellSource> source = GetCellSource(i);

		if (source.get() != NULL && source->IsDirty()) {
			blocker.Block();

			filelist::CellListAccessor accessor(this, i, &focus);

			source->Update(accessor);
			source->OnUpdated();
		}
	}
	blocker.Unblock();
	focus.Apply(this);
}

ListItem *FileList::CreateItem(listview::ItemFactory::CreateParam &param)
{
	int64_t id = -1;
	intrusive_ptr<Visualizer> visualizer;
	intrusive_ptr<CellSource> source = GetCellSource(param.m_segment);
	ListItem *item = NULL;

	if (source.get() != NULL) {
		id = source->GetID(param.m_cell);

		filelist::VisualizerBindParam bind;

		bind.m_index = param.m_cell;
		bind.m_id = id;
		bind.m_parent = param.m_parent;
		visualizer = source->GetVisualizer(bind);
		if (visualizer.get() != NULL) {
			item = visualizer->GetListItem();
			if (item != NULL)
				visualizer->AddRef();
			else
				visualizer.clear();
		}
	}
	if (item == NULL)
		item = new ListItem(param.m_parent, NULL);

	PropertyValue value;

	value.m_value = id;
	item->m_property_list.SetValue(0, value);
	value.m_pointer = visualizer.get();
	item->m_property_list.SetValue(1, value);
	return item;
}

namespace filelist {

intrusive_ptr<Visualizer> GetVisualizer(ListItem *item)
{
	return intrusive_ptr<Visualizer>((Visualizer *)item->m_property_list.GetValue(1).m_pointer);
}

}

void FileList::StartItem(listview::ItemFactory::StartParam &param)
{
	intrusive_ptr<Visualizer> visualizer = filelist::GetVisualizer(param.m_item);

	if (visualizer.get() != NULL)
		visualizer->Start();
}

void FileList::StopItem(listview::ItemFactory::StopParam &param)
{
	intrusive_ptr<Visualizer> visualizer = filelist::GetVisualizer(param.m_item);

	if (visualizer.get() != NULL)
		visualizer->Stop();
}

/* the reference the item held let go of, or the plain item destroyed */
void FileList::DisposeItem(listview::ItemFactory::DisposeParam &param)
{
	intrusive_ptr<Visualizer> visualizer = filelist::GetVisualizer(param.m_item);

	if (visualizer.get() != NULL)
		visualizer->Release();
	else
		param.m_item->DestroyWidget();
}

Widget *FileList::ScePafWidget_2FF0AEC2(listview::SegmentHeaderFactory::CreateParam &param)
{
	if (m_segment_source.get() == NULL)
		return NULL;

	filelist::HeaderBindParam bind;

	bind.m_index = param.m_segment;
	bind.m_id = GetSegmentID(param.m_segment);
	bind.m_parent = param.m_parent;
	bind.m_size = ScePafWidget_89DDBD65(param.m_segment);
	return m_segment_source->ScePafWidget_BC736EE9(bind);
}

void FileList::ScePafWidget_789C5142(listview::SegmentHeaderFactory::DisposeParam &param)
{
	SegmentSource *source = m_segment_source.get();

	if (source != NULL)
		source->ScePafWidget_6B4F7595(param.m_segment, GetSegmentID(param.m_segment),
					      param.m_header);
	else
		param.m_header->DestroyWidget();
}

namespace filelist {

CellFocusParam::CellFocusParam() : m_segment(-1), m_timer(NULL)
{
}

CellFocusParam::~CellFocusParam()
{
	delete m_timer;
	m_timer = (Timer *)-1;
}

void CellFocusParam::Set(int32_t segment, int32_t cell, ListView::FocusAlignType align,
			 Timer *timer)
{
	delete m_timer;
	m_segment = segment;
	m_cell = cell;
	m_align = align;
	m_timer = timer;
}

CellListAccessor::CellListAccessor(FileList *file_list, int32_t segment,
				   CellFocusParam *focus_param)
	: m_file_list(file_list), m_segment(segment), m_focus_param(focus_param)
{
}

void CellListAccessor::Insert(int32_t index, uint32_t num)
{
	m_file_list->InsertCell(m_segment, index, num);
}

void CellListAccessor::Delete(int32_t index, uint32_t num)
{
	m_file_list->DeleteCell(m_segment, index, num);
}

uint32_t CellListAccessor::GetNum()
{
	return m_file_list->GetCellNum(m_segment);
}

int64_t GetCellID(ListItem *item)
{
	return item->m_property_list.GetValue(0).m_value;
}

void CellListAccessor::GetExists(vector<IDPair> &list)
{
	list.clear();

	int32_t num = m_file_list->GetGeneratedNum();

	for (int32_t i = 0; i < num; i++) {
		FileList *file_list = m_file_list;
		listview::ListPos pos = file_list->GetGeneratedPos(i);
		ListItem *item = file_list->GetListItem(pos.m_segment, pos.m_cell);

		if (item != NULL && item->GetSegmentIndex() == m_segment)
			list.push_back(IDPair(GetCellID(item), item->GetCellIndex()));
	}
}

ListItem *CellListAccessor::ScePafWidget_95A143BF(int32_t index)
{
	return m_file_list->GetListItem(m_segment, index);
}

void CellListAccessor::SetFocus(int32_t index, ListView::FocusAlignType align, Timer *timer)
{
	m_focus_param->Set(m_segment, index, align, timer);
}

SegmentListAccessor::SegmentListAccessor(FileList *file_list) : m_file_list(file_list)
{
}

void SegmentListAccessor::Insert(int32_t index, uint32_t num)
{
	m_file_list->InsertSegment(index, num);
}

void SegmentListAccessor::Delete(int32_t index, uint32_t num)
{
	m_file_list->DeleteSegment(index, num);
}

uint32_t SegmentListAccessor::GetNum()
{
	return m_file_list->GetSegmentNum();
}

void SegmentListAccessor::GetExists(vector<IDPair> &list)
{
	list.clear();

	int32_t last = -1;
	int32_t num = m_file_list->GetGeneratedNum();

	for (int32_t i = 0; i < num; i++) {
		FileList *file_list = m_file_list;
		listview::ListPos pos = file_list->GetGeneratedPos(i);
		ListItem *item = file_list->GetListItem(pos.m_segment, pos.m_cell);

		if (item == NULL)
			continue;

		int32_t segment = item->GetSegmentIndex();

		if (last != segment) {
			last = segment;
			list.push_back(IDPair(m_file_list->GetSegmentID(segment), segment));
		}
	}
}

void SegmentListAccessor::SetOption(int32_t index, OptionParam &param)
{
	m_file_list->SetSegmentLayoutType(index, param.m_layout_type);
	if (param.m_header_size == math::v4(0.0f, 0.0f, 0.0f, 0.0f))
		m_file_list->SetSegmentHeader(index, param.m_header, param.m_header_floating,
					      param.m_header_disappearance);
	else
		m_file_list->ScePafWidget_7AC07A92(index, param.m_header_size,
						   param.m_header_floating,
						   param.m_header_disappearance);
	m_file_list->ScePafWidget_27EB2AEB(index, param.m_column_num);
	m_file_list->ScePafWidget_7E9DC5B1(index, param.m_min_size);
	m_file_list->SetSegmentJoined(index, param.m_joined);
}

const char Visualizer::m_type_name[] = "visualizer";

bool Visualizer::IsInherit(const char *type) const
{
	return type == m_type_name;
}

const char *Visualizer::GetTypeName() const
{
	return m_type_name;
}

const char *Visualizer::TypeName()
{
	return m_type_name;
}

void Visualizer::Start()
{
}

void Visualizer::Stop()
{
}

ListItem *Visualizer::GetListItem()
{
	return NULL;
}

const char CellSource::m_type_name[] = "cell_source";

bool CellSource::IsInherit(const char *type) const
{
	return type == m_type_name;
}

const char *CellSource::GetTypeName() const
{
	return m_type_name;
}

const char *CellSource::TypeName()
{
	return m_type_name;
}

bool CellSource::IsDirty()
{
	return false;
}

void CellSource::Update(CellListAccessor &accessor)
{
}

int64_t CellSource::GetID(int32_t index)
{
	return -1;
}

intrusive_ptr<Visualizer> CellSource::GetVisualizer(VisualizerBindParam &param)
{
	return intrusive_ptr<Visualizer>();
}

math::v4 CellSource::GetVisualSize()
{
	return math::v4(64.0f, 64.0f, 64.0f, 64.0f);
}

bool CellSource::IsDraggable()
{
	return false;
}

void CellSource::OnMoved(int32_t from, int32_t to)
{
}

void CellSource::OnDragStart(int32_t index)
{
}

void CellSource::OnDragStop(int32_t index)
{
}

void CellSource::OnUpdated()
{
}

const char SegmentSource::m_type_name[] = "segment_source";

bool SegmentSource::IsInherit(const char *type) const
{
	return type == m_type_name;
}

const char *SegmentSource::GetTypeName() const
{
	return m_type_name;
}

const char *SegmentSource::TypeName()
{
	return m_type_name;
}

bool SegmentSource::IsDirty()
{
	return false;
}

void SegmentSource::Update(SegmentListAccessor &accessor)
{
}

int64_t SegmentSource::GetID(int32_t index)
{
	return -1;
}

intrusive_ptr<CellSource> SegmentSource::GetCellSource(int32_t index, int64_t id)
{
	return intrusive_ptr<CellSource>();
}

void SegmentSource::OnUpdated()
{
}

Widget *SegmentSource::ScePafWidget_BC736EE9(HeaderBindParam &param)
{
	return NULL;
}

void SegmentSource::ScePafWidget_6B4F7595(int32_t index, int64_t id, Widget *header)
{
	if (header != NULL)
		header->DestroyWidget();
}

ManagerImpl::ManagerImpl(const Manager::InitParam &param)
	: m_storage_path(param.m_storage_path), m_file_list_count(0)
{
	job::JobQueue::Option option;
	char name[32];

	for (uint32_t i = 0; i < Manager::QUEUE_TYPE_NUM; i++) {
		sce_paf_snprintf(name, sizeof(name), "ScePafFileListJobQueue%u", i);
		if (param.m_queue[i] != NULL) {
			m_queue[i] = param.m_queue[i];
			m_own_queue[i] = false;
		} else if (!param.m_create_queue) {
			m_queue[i] = NULL;
			m_own_queue[i] = false;
		} else {
			option.priority = param.m_priority[i];
			option.stackSize = param.m_stack_size[i];
			m_queue[i] = new job::JobQueue(name, &option);
			m_own_queue[i] = true;
		}
		if (m_queue[i] != NULL)
			m_call_queue[i] = new common::JobCallQueue(string(name), m_queue[i]);
		else
			m_call_queue[i] = NULL;
	}
	common::MainThreadCallList::Register(CheckCallQueues, this);
}

}

/* the file lists' mutex, and how many users have initialized the manager (names ours) */
static thread::Mutex s_init_mutex("ScePafFileListInitMutex", 0);
static int32_t s_init_count;

namespace filelist {

void Manager::Initialize(const InitParam &param)
{
	s_init_mutex.Lock();
	s_init_count++;
	if (s_manager == NULL)
		s_manager = new ManagerImpl(param);
	s_init_mutex.Unlock();
}

ManagerImpl::~ManagerImpl()
{
	common::MainThreadCallList::Unregister(CheckCallQueues, this);
	for (uint32_t i = 0; i < Manager::QUEUE_TYPE_NUM; i++) {
		delete m_call_queue[i];
		m_call_queue[i] = (common::JobCallQueue *)-1;
		if (m_queue[i] != NULL && m_own_queue[i])
			delete m_queue[i];
		m_queue[i] = (job::JobQueue *)-1;
	}
}

bool ManagerImpl::IsBusy()
{
	if (m_file_list_count != 0)
		return true;
	for (uint32_t i = 0; i < Manager::QUEUE_TYPE_NUM; i++) {
		if (m_call_queue[i] != NULL && m_call_queue[i]->NumItems() != 0)
			return true;
	}
	for (uint32_t i = 0; i < Manager::QUEUE_TYPE_NUM; i++) {
		if (m_queue[i] != NULL && m_queue[i]->NumItems() != 0)
			return true;
	}
	return false;
}

void ManagerImpl::Finalize()
{
	s_init_mutex.Lock();
	if (s_init_count <= 0) {
		s_init_mutex.Unlock();
		return;
	}
	if (--s_init_count > 0) {
		thread::SyncCall *synccall = &thread::SyncCall::main_thread_synccall;

		if (synccall != NULL) {
			synccall->Call(SyncNop, NULL);
			synccall->Call(SyncNop, NULL);
		}

		volatile int32_t done = 0;

		for (uint32_t i = 0; i < Manager::QUEUE_TYPE_NUM; i++) {
			job::JobQueue *queue = s_manager->m_queue[i];

			if (queue != NULL) {
				common::SharedPtr<job::JobItem> job(
					(job::JobItem *)new CountJob("ScePafFileListCountJob", &done));

				queue->Enqueue(job);
			}
		}
		while (done != Manager::QUEUE_TYPE_NUM)
			thread::Sleep(10);
	} else {
		while (s_manager->IsBusy()) {
			s_init_mutex.Unlock();
			thread::Sleep(10);
			s_init_mutex.Lock();
		}
		delete s_manager;
		s_manager = NULL;
	}
	s_init_mutex.Unlock();
}

void Manager::Finalize()
{
	ManagerImpl::Finalize();
}

job::JobQueue *Manager::GetJobQueue(QueueType type)
{
	return s_manager->m_queue[type];
}

string *Manager::GetStoragePath()
{
	return &s_manager->m_storage_path;
}

void Manager::PostTask(QueueType type, const char *name, void (*func)(void *), void *userdata)
{
	common::JobCallQueue *queue = s_manager->m_call_queue[type];

	if (queue != NULL)
		queue->Enqueue(name, func, userdata);
}

void ManagerImpl::PushCacheItem(uint32_t type, ListItem *item)
{
	item->m_transition_complete = false;
	item->m_transition = 0xff;
	item->DoTransitionReverse_core(0.0f, true);
	ScePafWidget_4588699E(item, NULL, 2, 0.0f, NULL);
	m_cache.Get(type, item->m_parent).push_back(item);
}

void Manager::PushCacheItem(uint32_t type, ListItem *item)
{
	s_manager->PushCacheItem(type, item);
}

ListItem *ManagerImpl::PopCacheItem(uint32_t type, Widget *parent)
{
	ListItemCache::ItemList &list = m_cache.Get(type, parent);

	if (list.size() == 0)
		return NULL;

	ListItem *item = list.back();

	list.pop_back();
	item->SetPos(math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	if (!item->m_transition_complete) {
		item->m_transition = 0xff;
		item->m_transition_complete = true;
		item->DoTransition_core(0.0f, true);
	}
	ScePafWidget_FF5BC957(item, NULL, 2, 0.0f, NULL);
	return item;
}

ListItem *Manager::PopCacheItem(uint32_t type, Widget *parent)
{
	return s_manager->PopCacheItem(type, parent);
}

/* the job queues made at the thread priorities from high to low, a quarter of the range apart,
   with the default stack size */
Manager::InitParam::InitParam(const string &storage_path)
	: m_storage_path(storage_path), m_create_queue(true)
{
	job::JobQueue::Option option;
	static int32_t step = (thread::GetPriority()->low - thread::GetPriority()->high) / 4;
	int32_t priority = thread::GetPriority()->high;

	for (uint32_t i = 0; i < QUEUE_TYPE_NUM; i++) {
		m_priority[i] = priority;

		int32_t next = priority + step;
		int32_t high = thread::GetPriority()->high;
		int32_t low = thread::GetPriority()->low;

		if (next < high)
			next = high;
		if (next > low)
			next = low;
		m_stack_size[i] = option.stackSize;
		m_queue[i] = NULL;
		priority = next;
	}
}

const char CellSourceElement::m_type_name[] = "cell_source_element";

bool CellSourceElement::IsInherit(const char *type_name) const
{
	if (type_name == m_type_name)
		return true;
	return PropertyElement::IsInherit(type_name);
}

const char *CellSourceElement::GetTypeName() const
{
	return m_type_name;
}

/* the call queues' next functions handed on, at each main thread tick */
static void CheckCallQueues(void *data)
{
	ManagerImpl *manager = (ManagerImpl *)data;

	for (uint32_t i = 0; i < Manager::QUEUE_TYPE_NUM; i++) {
		if (manager->m_call_queue[i] != NULL)
			manager->m_call_queue[i]->Check();
	}
}

}

}
}

using namespace paf;
using namespace paf::ui;

static Widget *NewFileList(Widget *parent)
{
	return new FileList(parent, NULL);
}

static WidgetParam *NewFileListParam()
{
	return new ListViewParam;
}

static VersatileParam *NewFileListLayoutParam()
{
	return new WidgetLayoutParam;
}

static VersatileParam *NewFileListFocusParam()
{
	return new WidgetFocusParam;
}

static VersatileParam *NewFileListStyleParam()
{
	return new ListViewStyleParam;
}

static VersatileParam *NewFileListHandlerParam()
{
	return new WidgetHandlerParam;
}

static VersatileParam *NewFileListSoundParam()
{
	return new ScrollViewBaseSoundParam;
}

namespace paf {
namespace ui {
namespace filelist {

/* what the main thread is made to run, to wait for what it has queued (name ours) */
static int SyncNop(void *arg)
{
	return 0;
}

}
}
}

FactoryFuncTable g_factoryFileList = {
	FileList::TypeName(),
	NewFileList,
	NewFileListParam,
	NewFileListLayoutParam,
	NewFileListFocusParam,
	NewFileListStyleParam,
	NewFileListHandlerParam,
	NewFileListSoundParam,
};
