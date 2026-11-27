/*
 * live2_cell_source.cpp: Live2CellSource, Live2DataQueryJob, Live2Iterator,
 * Live2AccessorProxy, Live2StatusAccessor, Live2CommentAccessor, and their
 * factories (0x8134B9D4..0x8134D67F).
 */
#include "filelist_internal.h"
#include <std/algorithm>
#include <std/string.h>

namespace filelist {

namespace {

static const char kLive2CellSourceTypeName[] = "live2_cell_source";

/* Disposes an ActivityDbQueryHandle on the filelist task queue (0x814436B8) */
static void DisposeActivityDbHandle(void *handlePtr)
{
	if (handlePtr != NULL) {
		SceActivityDb_6E198FFA(handlePtr);
		operator delete(handlePtr);
	}
}

struct Live2IndexRowid {
	int index;
	SceInt64 rowid;

	bool operator<(const Live2IndexRowid &rhs) const
	{
		return index < rhs.index;
	}
};

struct Live2IndexRowidCell {
	int index;
	SceInt64 rowid;
	SceInt64 cellId;

	bool operator<(const Live2IndexRowidCell &rhs) const
	{
		return index < rhs.index;
	}
};

struct Live2FullRefreshContext {
	Live2SyncController *controller;
	Live2Iterator *iterator;
};

struct Live2DiffRefreshContext {
	Live2SyncController *controller;
	Live2Iterator *iterator;
	paf::vector<Live2IndexRowidCell> nextEntries;
};

/* Full replace apply callback executed on the main UI thread (0x81442948) */
static void ApplyFullRefreshOnMainThread(paf::ui::filelist::CellListAccessor &accessor,
					 void *userdata)
{
	Live2FullRefreshContext *ctx = static_cast<Live2FullRefreshContext *>(userdata);
	uint32_t currentCount = accessor.GetNum();
	accessor.Delete(0, currentCount);
	int newCount = ctx->iterator->m_accessor->GetCount();
	accessor.Insert(0, static_cast<uint32_t>(newCount));
}

/* Incremental diff apply callback executed on the main UI thread (0x81442978) */
static void ApplyDiffRefreshOnMainThread(paf::ui::filelist::CellListAccessor &accessor,
					 void *userdata)
{
	Live2DiffRefreshContext *ctx = static_cast<Live2DiffRefreshContext *>(userdata);
	paf::vector<Live2IndexRowidCell> activeEntries;

	ctx->iterator->m_mutex.Lock();
	for (paf::map<SceInt64, Live2CellMapping>::iterator it =
		     ctx->iterator->m_cellMap.begin();
	     it != ctx->iterator->m_cellMap.end(); ++it) {
		Live2IndexRowidCell entry;
		entry.index = it->second.index;
		entry.rowid = it->second.rowid;
		entry.cellId = it->first;
		activeEntries.push_back(entry);
	}
	ctx->iterator->m_mutex.Unlock();

	paf::sort(activeEntries.begin(), activeEntries.end());

	paf::vector<Live2IndexRowidCell>::iterator cur = activeEntries.begin();
	while (cur != activeEntries.end()) {
		if (cur->rowid == -1) {
			++cur;
			continue;
		}

		int targetIndex = -1;
		for (size_t i = 0; i < ctx->nextEntries.size(); ++i) {
			if (ctx->nextEntries[i].rowid == cur->rowid) {
				targetIndex = ctx->nextEntries[i].index;
				break;
			}
		}

		int curIndex = cur->index;
		if (targetIndex == -1) {
			accessor.Delete(curIndex, 1);
			for (paf::vector<Live2IndexRowidCell>::iterator shift = cur + 1;
			     shift != activeEntries.end(); ++shift)
				--shift->index;
			cur = activeEntries.erase(cur);
			continue;
		}

		if (targetIndex < curIndex) {
			int removeCount = curIndex - targetIndex;
			accessor.Delete(targetIndex, static_cast<uint32_t>(removeCount));
			paf::vector<Live2IndexRowidCell>::iterator scan = activeEntries.begin();
			while (scan != activeEntries.end()) {
				if (scan->index < targetIndex) {
					++scan;
				} else if (scan->index < curIndex) {
					scan = activeEntries.erase(scan);
				} else {
					scan->index -= removeCount;
					++scan;
				}
			}
			cur = activeEntries.begin();
			while (cur != activeEntries.end() && cur->index != targetIndex)
				++cur;
			if (cur != activeEntries.end())
				++cur;
			continue;
		}

		if (targetIndex > curIndex) {
			int insertCount = targetIndex - curIndex;
			accessor.Insert(curIndex, static_cast<uint32_t>(insertCount));
			for (paf::vector<Live2IndexRowidCell>::iterator shift = cur;
			     shift != activeEntries.end(); ++shift)
				shift->index += insertCount;
		}
		++cur;
	}

	ctx->iterator->m_mutex.Lock();
	for (size_t i = 0; i < activeEntries.size(); ++i) {
		paf::map<SceInt64, Live2CellMapping>::iterator found =
			ctx->iterator->m_cellMap.find(activeEntries[i].cellId);
		if (found != ctx->iterator->m_cellMap.end())
			found->second.index = activeEntries[i].index;
	}
	ctx->iterator->m_mutex.Unlock();

	uint32_t targetTotal = static_cast<uint32_t>(ctx->iterator->m_accessor->GetCount());
	uint32_t existingTotal = accessor.GetNum();
	if (existingTotal < targetTotal)
		accessor.Insert(static_cast<int32_t>(existingTotal),
				targetTotal - existingTotal);
	else if (existingTotal > targetTotal)
		accessor.Delete(static_cast<int32_t>(targetTotal),
				existingTotal - targetTotal);

	paf::vector<paf::ui::filelist::IDPair> existingCells;
	accessor.GetExists(existingCells);
	for (size_t i = 0; i < existingCells.size(); ++i) {
		paf::ui::ListItem *item = accessor.ScePafWidget_95A143BF(existingCells[i].m_index);
		if (item == NULL)
			continue;
		paf::intrusive_ptr<paf::ui::filelist::Visualizer> vis =
			paf::ui::filelist::GetVisualizer(item);
		if (vis.get() != NULL && vis->IsInherit(Live2Visualizer::TypeName())) {
			Live2Visualizer *liveVis = static_cast<Live2Visualizer *>(vis.get());
			if (liveVis->m_isRecordLoaded)
				liveVis->OnRecordCleared();
		}
	}
}

static void NoOpApplyCallback(paf::ui::filelist::CellListAccessor &accessor, void *userdata)
{
	(void)accessor;
	(void)userdata;
}

/*
 * ScePafFileListLive2DataQuery (152 bytes, multiple inheritance: SlotJobItem +
 * Live2SyncController, vtables 0x8153D230 / 0x8153D258).
 */
class Live2DataQueryJob : public paf::common::SlotJobItem, public Live2SyncController {
public:
	explicit Live2DataQueryJob(
		const paf::common::SharedPtr<Live2AccessorFactory> &accessorFactory)
		: paf::common::SlotJobItem("ScePafFileListLive2DataQuery", NULL),
		  m_mutex("ScePafFileListLive2DataQuery", 0),
		  m_cond(m_mutex, "ScePafFileListLive2DataQuery", 0),
		  m_pendingApply(NULL),
		  m_pendingUserdata(NULL),
		  m_accessorFactory(accessorFactory),
		  m_iterator(),
		  m_refreshMode(0),
		  m_totalCount(0),
		  m_lastReadTime(0),
		  m_lastUpdateTime(0),
		  m_latestActivityTime(0)
	{
	}

	Live2DataQueryJob(const paf::common::SharedPtr<Live2Iterator> &iterator, int refreshMode)
		: paf::common::SlotJobItem("ScePafFileListLive2DataQuery", NULL),
		  m_mutex("ScePafFileListLive2DataQuery", 0),
		  m_cond(m_mutex, "ScePafFileListLive2DataQuery", 0),
		  m_pendingApply(NULL),
		  m_pendingUserdata(NULL),
		  m_accessorFactory(),
		  m_iterator(iterator),
		  m_refreshMode(refreshMode),
		  m_totalCount(0),
		  m_lastReadTime(0),
		  m_lastUpdateTime(0),
		  m_latestActivityTime(0)
	{
	}

	virtual ~Live2DataQueryJob()
	{
	}

	/* 0x81441B58 */
	virtual void Cancel()
	{
		m_mutex.Lock();
		m_pendingApply = NULL;
		m_cond.Notify();
		paf::job::JobItem::Cancel();
		m_mutex.Unlock();
	}

	/* 0x81441B80 */
	virtual int32_t Run()
	{
		if (m_refreshMode != 0) {
			m_iterator->Refresh(this);
			paf::common::SharedPtr<Live2AccessorProxy> proxy =
				m_iterator->GetAccessorProxy();
			SceActivityDb_7ED9033C(proxy->m_queryHandle, &m_totalCount,
					       &m_lastReadTime, &m_lastUpdateTime,
					       &m_latestActivityTime);
			return 0;
		}

		paf::common::SharedPtr<Live2Iterator> created(
			new Live2Iterator(m_accessorFactory));
		m_iterator = created;
		if (m_iterator.get() == NULL)
			return 0;

		paf::common::SharedPtr<Live2AccessorProxy> proxy = m_iterator->GetAccessorProxy();
		SceActivityDb_7ED9033C(proxy->m_queryHandle, &m_totalCount,
				       &m_lastReadTime, &m_lastUpdateTime,
				       &m_latestActivityTime);
		WaitForMainThreadApply(NoOpApplyCallback, NULL);
		return 0;
	}

	/* 0x814417A0 */
	virtual bool IsCanceled()
	{
		return paf::job::JobItem::IsCanceled();
	}

	/* 0x81441B08 */
	virtual bool WaitForMainThreadApply(ApplyFunc func, void *userdata)
	{
		m_mutex.Lock();
		if (paf::job::JobItem::IsCanceled()) {
			m_mutex.Unlock();
			return false;
		}
		m_pendingApply = func;
		m_pendingUserdata = userdata;
		m_cond.Wait(0);
		bool ok = !paf::job::JobItem::IsCanceled();
		m_mutex.Unlock();
		return ok;
	}

	/* 0x812FEA98 */
	virtual int GetRefreshMode()
	{
		return m_refreshMode;
	}

	paf::thread::Mutex m_mutex;
	paf::thread::Cond m_cond;
	ApplyFunc m_pendingApply;
	void *m_pendingUserdata;
	paf::common::SharedPtr<Live2AccessorFactory> m_accessorFactory;
	paf::common::SharedPtr<Live2Iterator> m_iterator;
	int m_refreshMode;
	int m_totalCount;
	SceUInt64 m_lastReadTime;
	SceUInt64 m_lastUpdateTime;
	SceUInt64 m_latestActivityTime;
};

static void ComputeWindowBounds(int targetIndex, int windowSize, int totalCount,
				int &outStart, int &outCount)
{
	int size = windowSize;
	if (size < 0)
		size = 1;
	if (size < totalCount) {
		int start = targetIndex - size / 2;
		int maxStart = totalCount - size;
		if (start < 0)
			start = 0;
		if (start > maxStart)
			start = maxStart;
		outStart = start;
		outCount = size;
	} else {
		outStart = 0;
		outCount = totalCount;
	}
}

} /* anonymous namespace */

/* ------------------------------------------------------------------ */
/* Live2CellSource (0x8134B9D4..0x8134C4EB)                           */
/* ------------------------------------------------------------------ */

/* 0x8134B9D4 */
bool Live2CellSource::IsInherit(const char *type) const
{
	if (type == kLive2CellSourceTypeName)
		return true;
	return paf::ui::filelist::CellSource::IsInherit(type);
}

/* 0x8134B9EC */
const char *Live2CellSource::GetTypeName() const
{
	return kLive2CellSourceTypeName;
}

const char *Live2CellSource::TypeName()
{
	return kLive2CellSourceTypeName;
}

/* 0x8134B9F8 */
void Live2CellSource::SetAccessorFactory(
	const paf::common::SharedPtr<Live2AccessorFactory> &accessorFactory)
{
	m_accessorFactory = accessorFactory;
	if (m_suspended != 0) {
		m_pendingRefreshMode = 0;
		return;
	}
	paf::common::SharedPtr<paf::job::JobItem> job(new Live2DataQueryJob(m_accessorFactory));
	paf::job::JobQueue *queue =
		paf::ui::filelist::Manager::GetJobQueue(paf::ui::filelist::Manager::QUEUE_TYPE_FAST);
	m_jobSlots.Enqueue(0, queue, job);
}

/* 0x8134BBD0 */
Live2CellSource::Live2CellSource(
	const paf::common::SharedPtr<Live2VisualizerFactory> &visualizerFactory,
	const paf::common::SharedPtr<Live2AccessorFactory> &accessorFactory)
	: m_visualizerFactory(visualizerFactory),
	  m_accessorFactory(),
	  m_iterator(),
	  m_jobSlots(),
	  m_refreshMode(1),
	  m_suspended(0),
	  m_pendingRefreshMode(-1),
	  m_countCallback(NULL),
	  m_countCallbackArg(0),
	  m_totalCount(0),
	  m_lastReadTime(0),
	  m_lastUpdateTime(0),
	  m_latestActivityTime(0)
{
	m_jobSlots.SetRemovedFunction(OnQueryJobRemoved, this);
	SetAccessorFactory(accessorFactory);
}

/* 0x8134BF2A / 0x8134C04C */
Live2CellSource::~Live2CellSource()
{
	m_jobSlots.CancelAll();
}

/* 0x8134BC64 */
void Live2CellSource::RequestRefresh(int refreshMode)
{
	if (m_suspended != 0) {
		if (m_pendingRefreshMode != 0)
			m_pendingRefreshMode = refreshMode;
		return;
	}

	paf::job::JobQueue *queue =
		paf::ui::filelist::Manager::GetJobQueue(paf::ui::filelist::Manager::QUEUE_TYPE_FAST);

	if (refreshMode != 0) {
		if (m_iterator.get() == NULL)
			return;
		paf::common::SharedPtr<paf::job::JobItem> job(
			new Live2DataQueryJob(m_iterator, refreshMode));
		m_jobSlots.Enqueue(0, queue, job);
	} else {
		paf::common::SharedPtr<paf::job::JobItem> job(
			new Live2DataQueryJob(m_accessorFactory));
		m_jobSlots.Enqueue(0, queue, job);
	}
}

/* 0x8134BF04 */
void Live2CellSource::SetCountCallback(CountCallback callback, int arg)
{
	m_countCallback = callback;
	m_countCallbackArg = arg;
}

} /* namespace filelist */

class ActivityFeedCellSource;

void activityUiCellSourceSetCountCallback(ActivityFeedCellSource *cellSource,
					  void (*callback)(int, int), int pageKey)
{
	reinterpret_cast<filelist::Live2CellSource *>(cellSource)->SetCountCallback(
		callback, pageKey);
}

namespace filelist {

/* 0x8134BF0A */
void Live2CellSource::OnCountChanged()
{
	if (m_countCallback == NULL)
		return;
	int count = (m_iterator.get() != NULL) ? m_iterator->GetCount() : 0;
	m_countCallback(count, m_countCallbackArg);
}

/* 0x8134C05C */
bool Live2CellSource::IsDirty()
{
	if (m_suspended != 0)
		return false;

	if (!m_jobSlots.Contains(0)) {
		if (m_iterator.get() != NULL && m_iterator->IsChanged())
			RequestRefresh(m_refreshMode);
		return false;
	}

	paf::common::SharedPtr<paf::job::JobItem> jobRef = m_jobSlots.Get(0);
	Live2DataQueryJob *queryJob = static_cast<Live2DataQueryJob *>(jobRef.get());
	queryJob->m_mutex.Lock();
	bool ready = (queryJob->m_pendingApply != NULL);
	queryJob->m_mutex.Unlock();
	return ready;
}

/* 0x8134C124 */
void Live2CellSource::Update(paf::ui::filelist::CellListAccessor &accessor)
{
	if (!m_jobSlots.Contains(0))
		return;

	paf::common::SharedPtr<paf::job::JobItem> jobRef = m_jobSlots.Get(0);
	Live2DataQueryJob *queryJob = static_cast<Live2DataQueryJob *>(jobRef.get());

	queryJob->m_mutex.Lock();
	if (queryJob->m_refreshMode != 0) {
		queryJob->m_pendingApply(accessor, queryJob->m_pendingUserdata);
	} else {
		uint32_t oldCount = accessor.GetNum();
		accessor.Delete(0, oldCount);
		int newCount = queryJob->m_iterator->GetCount();
		accessor.Insert(0, static_cast<uint32_t>(newCount));

		paf::common::SharedPtr<Live2Iterator> previous = m_iterator;
		m_iterator = queryJob->m_iterator;
		queryJob->m_iterator = previous;
		queryJob->m_iterator.reset();
	}
	queryJob->m_pendingApply = NULL;
	queryJob->m_cond.Notify();
	queryJob->m_mutex.Unlock();
}

/* 0x8134C3FE */
int64_t Live2CellSource::GetID(int32_t index)
{
	if (m_iterator.get() != NULL)
		return m_iterator->GetOrCreateCellId(index);
	return paf::ui::filelist::CellSource::GetID(index);
}

/* 0x8134C412 */
paf::intrusive_ptr<paf::ui::filelist::Visualizer> Live2CellSource::GetVisualizer(
	paf::ui::filelist::VisualizerBindParam &param)
{
	if (m_visualizerFactory.get() != NULL)
		return m_visualizerFactory->CreateVisualizer(
			param.m_parent, m_iterator, param.m_index, param.m_id);
	return paf::ui::filelist::CellSource::GetVisualizer(param);
}

/* 0x8134C440 */
PafVec4 Live2CellSource::GetVisualSize()
{
	if (m_visualizerFactory.get() != NULL)
		return m_visualizerFactory->GetVisualSize();
	return paf::ui::filelist::CellSource::GetVisualSize();
}

/* 0x8134C458 */
void Live2CellSource::OnQueryJobRemoved(uint32_t slotIndex,
					paf::common::SharedPtr<paf::job::JobItem> job,
					void *userdata)
{
	if (slotIndex != 0)
		return;

	Live2CellSource *self = static_cast<Live2CellSource *>(userdata);
	Live2DataQueryJob *queryJob = static_cast<Live2DataQueryJob *>(job.get());
	self->m_totalCount = queryJob->m_totalCount;
	self->m_lastReadTime = queryJob->m_lastReadTime;
	self->m_lastUpdateTime = queryJob->m_lastUpdateTime;
	self->m_latestActivityTime = queryJob->m_latestActivityTime;
	self->OnCountChanged();
}

/* ------------------------------------------------------------------ */
/* Live2Iterator (0x8134C4F0..0x8134CAAF)                             */
/* ------------------------------------------------------------------ */

/* 0x8134C4FA */
Live2Iterator::Live2Iterator(const paf::common::SharedPtr<Live2AccessorFactory> &factory)
	: m_accessor(factory->CreateAccessor()),
	  m_mutex("ScePafFileListLive2IteratorMutex", 0),
	  m_cellMap()
{
	m_accessor->GetCount();
}

Live2Iterator::~Live2Iterator()
{
}

/* 0x8134C4F0 */
int Live2Iterator::GetCount()
{
	return m_accessor->GetCount();
}

/* 0x8134C54C */
bool Live2Iterator::IsChanged()
{
	return m_accessor->IsChanged();
}

/* 0x8134C558 */
SceInt64 Live2Iterator::GetOrCreateCellId(int index)
{
	m_mutex.Lock();
	for (paf::map<SceInt64, Live2CellMapping>::iterator it = m_cellMap.begin();
	     it != m_cellMap.end(); ++it) {
		if (it->second.index == index) {
			SceInt64 existingId = it->first;
			m_mutex.Unlock();
			return existingId;
		}
	}

	SceInt64 newId = ScePafCommon_AC2F3710();
	Live2CellMapping mapping;
	mapping.index = index;
	mapping.rowid = -1;
	m_cellMap.insert(paf::pair<SceInt64, Live2CellMapping>(newId, mapping));
	m_mutex.Unlock();
	return newId;
}

/* 0x8134C63A */
paf::intrusive_ptr<Live2Record> Live2Iterator::GetRecordByCellId(SceInt64 cellId)
{
	m_mutex.Lock();
	paf::map<SceInt64, Live2CellMapping>::iterator it = m_cellMap.find(cellId);
	if (it == m_cellMap.end()) {
		m_mutex.Unlock();
		return paf::intrusive_ptr<Live2Record>();
	}
	SceInt64 rowid = it->second.rowid;
	int index = it->second.index;
	m_mutex.Unlock();

	if (rowid == -1)
		rowid = m_accessor->GetRowid(index);

	return m_accessor->GetRecord(rowid);
}

/* 0x8134C6F8 */
void Live2Iterator::Refresh(Live2SyncController *controller)
{
	if (controller->GetRefreshMode() == 1) {
		Live2FullRefreshContext ctx;
		ctx.controller = controller;
		ctx.iterator = this;

		m_accessor->ClearDirty();
		m_accessor->ClearCache();
		m_accessor->GetCount();
		controller->WaitForMainThreadApply(ApplyFullRefreshOnMainThread, &ctx);
		return;
	}

	Live2DiffRefreshContext ctx;
	ctx.controller = controller;
	ctx.iterator = this;

	m_accessor->ClearDirty();
	m_accessor->ClearCache();
	m_accessor->GetCount();

	paf::vector<Live2IndexRowid> snapshot;
	m_mutex.Lock();
	for (paf::map<SceInt64, Live2CellMapping>::iterator it = m_cellMap.begin();
	     it != m_cellMap.end(); ++it) {
		Live2IndexRowid item;
		item.index = it->second.index;
		item.rowid = it->second.rowid;
		snapshot.push_back(item);
	}
	m_mutex.Unlock();

	paf::sort(snapshot.begin(), snapshot.end());

	for (size_t i = 0; i < snapshot.size(); ++i) {
		SceInt64 currentRowid = m_accessor->GetRowid(snapshot[i].index);
		Live2IndexRowidCell nextItem;
		nextItem.index = snapshot[i].index;
		nextItem.rowid = currentRowid;
		nextItem.cellId = -1;
		ctx.nextEntries.push_back(nextItem);

		if (currentRowid != snapshot[i].rowid) {
			int movedIndex = m_accessor->FindIndex(snapshot[i].rowid);
			if (movedIndex != -1) {
				Live2IndexRowidCell movedItem;
				movedItem.index = movedIndex;
				movedItem.rowid = snapshot[i].rowid;
				movedItem.cellId = -1;
				ctx.nextEntries.push_back(movedItem);
			}
		}
	}

	paf::sort(ctx.nextEntries.begin(), ctx.nextEntries.end());
	controller->WaitForMainThreadApply(ApplyDiffRefreshOnMainThread, &ctx);
}

/* 0x8134C95E */
void Live2Iterator::PinCellRowid(SceInt64 cellId)
{
	m_mutex.Lock();
	paf::map<SceInt64, Live2CellMapping>::iterator it = m_cellMap.find(cellId);
	if (it != m_cellMap.end() && it->second.rowid == -1)
		it->second.rowid = m_accessor->GetRowid(it->second.index);
	m_mutex.Unlock();
}

/* 0x8134C9F0 */
void Live2Iterator::ReleaseCellId(SceInt64 cellId)
{
	m_mutex.Lock();
	paf::map<SceInt64, Live2CellMapping>::iterator it = m_cellMap.find(cellId);
	if (it != m_cellMap.end())
		m_cellMap.erase(it);
	m_mutex.Unlock();
}

/* 0x8134CA94 */
paf::common::SharedPtr<Live2AccessorProxy> Live2Iterator::GetAccessorProxy()
{
	return m_accessor->m_proxy;
}

/* ------------------------------------------------------------------ */
/* Live2AccessorFactory & subclasses (0x8134CAC0..0x8134CDE3)         */
/* ------------------------------------------------------------------ */

/* 0x8134CAC0 / 0x8134CACE */
Live2AccessorFactory::~Live2AccessorFactory()
{
}

/* 0x8112FB08 */
paf::common::SharedPtr<Live2Accessor> Live2AccessorFactory::CreateAccessor()
{
	return paf::common::SharedPtr<Live2Accessor>();
}

/* 0x8134CAE4 */
Live2StatusAccessorFactory::Live2StatusAccessorFactory(int windowSize)
	: m_windowSize(windowSize),
	  m_condition()
{
}

Live2StatusAccessorFactory::~Live2StatusAccessorFactory()
{
}

/* 0x8134CBC0 */
paf::common::SharedPtr<Live2Accessor> Live2StatusAccessorFactory::CreateAccessor()
{
	paf::common::SharedPtr<Live2AccessorProxy> proxy(new Live2AccessorProxy(m_condition));
	return paf::common::SharedPtr<Live2Accessor>(
		new Live2StatusAccessor(proxy, m_windowSize));
}

/* 0x8134CD28 */
Live2CommentAccessorFactory::Live2CommentAccessorFactory(
	const paf::common::SharedPtr<Live2AccessorProxy> &proxy,
	SceInt64 parentRowid,
	int windowSize)
	: m_proxy(proxy),
	  m_parentRowid(parentRowid),
	  m_windowSize(windowSize)
{
}

Live2CommentAccessorFactory::~Live2CommentAccessorFactory()
{
}

/* 0x8134CD68 */
paf::common::SharedPtr<Live2Accessor> Live2CommentAccessorFactory::CreateAccessor()
{
	return paf::common::SharedPtr<Live2Accessor>(
		new Live2CommentAccessor(m_proxy, m_parentRowid, m_windowSize));
}

/* ------------------------------------------------------------------ */
/* Live2AccessorProxy (0x8134CB10, 0x8134CDE4..0x8134CEF7)            */
/* ------------------------------------------------------------------ */

Live2AccessorProxy::Live2AccessorProxy(const ActivityDbQueryCondition &condition)
	: m_queryHandle(NULL),
	  m_listeners(),
	  m_mutex("ScePafFileListLive2AccessorProxy", 0)
{
	void *mem = operator new(4);
	if (mem != NULL)
		m_queryHandle = SceActivityDb_6C98B745(mem);
	SceActivityDb_F65C0C21(m_queryHandle, &condition);
}

/* 0x8134CE64 / 0x8134CEAC */
Live2AccessorProxy::~Live2AccessorProxy()
{
	paf::ui::filelist::Manager::PostTask(
		paf::ui::filelist::Manager::QUEUE_TYPE_FAST,
		"Live2AccessorDisposer",
		DisposeActivityDbHandle,
		m_queryHandle);
}

/* 0x8134CB10 */
void Live2AccessorProxy::AddListener(ChangeCallback callback, void *userdata)
{
	m_mutex.Lock();
	Listener entry;
	entry.callback = callback;
	entry.userdata = userdata;
	m_listeners.push_back(entry);
	m_mutex.Unlock();
}

/* 0x8134CDE4 */
void Live2AccessorProxy::RemoveListener(ChangeCallback callback, void *userdata)
{
	m_mutex.Lock();
	for (paf::vector<Listener>::iterator it = m_listeners.begin();
	     it != m_listeners.end(); ++it) {
		if (it->callback == callback && it->userdata == userdata) {
			m_listeners.erase(it);
			break;
		}
	}
	m_mutex.Unlock();
}

void Live2AccessorProxy::NotifyIfChanged()
{
	if (SceActivityDb_402A5F72(m_queryHandle) == 0)
		return;

	SceActivityDb_E6601703(m_queryHandle);
	m_mutex.Lock();
	for (size_t i = 0; i < m_listeners.size(); ++i)
		m_listeners[i].callback(m_listeners[i].userdata);
	m_mutex.Unlock();
}

/* ------------------------------------------------------------------ */
/* Live2Accessor base (0x8134CB66, 0x8134CEF8..0x8134D083)            */
/* ------------------------------------------------------------------ */

/* 0x8134CB66 */
Live2Accessor::Live2Accessor(const paf::common::SharedPtr<Live2AccessorProxy> &proxy)
	: m_proxy(proxy),
	  m_windowStart(-1),
	  m_windowRowids(),
	  m_totalCount(-1),
	  m_dirty(0)
{
	m_proxy->AddListener(OnProxyChanged, this);
}

/* 0x8134CEF8 / 0x8134CF7E */
Live2Accessor::~Live2Accessor()
{
	m_proxy->RemoveListener(OnProxyChanged, this);
}

/* 0x81413118 */
paf::intrusive_ptr<Live2Record> Live2Accessor::GetRecord(SceInt64 rowid)
{
	(void)rowid;
	return paf::intrusive_ptr<Live2Record>();
}

/* 0x8134D042 */
bool Live2Accessor::IsChanged()
{
	return m_dirty != 0;
}

void Live2Accessor::FetchCount()
{
}

void Live2Accessor::FetchWindow(int index)
{
	(void)index;
}

/* 0x8134CF8E */
int Live2Accessor::GetCount()
{
	FetchCount();
	return m_totalCount;
}

/* 0x8134CF9C */
SceInt64 Live2Accessor::GetRowid(int index)
{
	FetchWindow(index);
	if (m_windowRowids.size() == 0)
		return -1;
	if (index < m_windowStart ||
	    index >= m_windowStart + static_cast<int>(m_windowRowids.size()))
		return -1;
	return m_windowRowids[static_cast<size_t>(index - m_windowStart)];
}

/* 0x8134CFD8 */
int Live2Accessor::FindIndex(SceInt64 rowid)
{
	for (size_t i = 0; i < m_windowRowids.size(); ++i) {
		if (m_windowRowids[i] == rowid)
			return m_windowStart + static_cast<int>(i);
	}
	return -1;
}

/* 0x8134D024 */
void Live2Accessor::ClearCache()
{
	m_windowStart = -1;
	m_totalCount = -1;
	m_windowRowids.clear();
}

/* 0x8134D04C */
void Live2Accessor::ClearDirty()
{
	__sync_lock_test_and_set(&m_dirty, 0u);
}

/* 0x8134D068 */
void Live2Accessor::MarkDirty()
{
	__sync_lock_test_and_set(&m_dirty, 1u);
}

void Live2Accessor::OnProxyChanged(void *userdata)
{
	static_cast<Live2Accessor *>(userdata)->MarkDirty();
}

/* ------------------------------------------------------------------ */
/* Live2StatusAccessor (0x8134D084..0x8134D36F)                       */
/* ------------------------------------------------------------------ */

Live2StatusAccessor::Live2StatusAccessor(
	const paf::common::SharedPtr<Live2AccessorProxy> &proxy,
	int windowSize)
	: Live2Accessor(proxy),
	  m_windowSize(windowSize),
	  m_statuses()
{
}

Live2StatusAccessor::~Live2StatusAccessor()
{
}

/* 0x8134D084 */
paf::intrusive_ptr<Live2Record> Live2StatusAccessor::GetRecord(SceInt64 rowid)
{
	Live2StatusRecord *record = new Live2StatusRecord();
	record->m_rowid = rowid;

	for (size_t i = 0; i < m_windowRowids.size(); ++i) {
		if (m_windowRowids[i] == rowid) {
			if (i < m_statuses.size())
				record->m_status = m_statuses[i];
			break;
		}
	}

	return paf::intrusive_ptr<Live2Record>(record);
}

/* 0x8134D268 */
bool Live2StatusAccessor::IsChanged()
{
	m_proxy->NotifyIfChanged();
	return m_dirty != 0;
}

/* 0x8134D2C6 */
void Live2StatusAccessor::FetchCount()
{
	if (m_totalCount == -1) {
		m_totalCount = 0;
		SceActivityDb_6C29F640(m_proxy->m_queryHandle, &m_totalCount);
	}
}

/* 0x8134D2E2 */
void Live2StatusAccessor::FetchWindow(int index)
{
	if (m_windowRowids.size() != 0 &&
	    index >= m_windowStart &&
	    index < m_windowStart + static_cast<int>(m_windowRowids.size()))
		return;

	m_windowRowids.clear();
	m_statuses.clear();
	FetchCount();
	if (m_totalCount <= 0)
		return;

	int windowStart = 0;
	int windowCount = 0;
	ComputeWindowBounds(index, m_windowSize, m_totalCount, windowStart, windowCount);
	m_windowStart = windowStart;
	SceActivityDb_8F2E07A1(m_proxy->m_queryHandle, windowStart, windowCount, &m_windowRowids);
	SceActivityDb_74F96707(m_proxy->m_queryHandle, &m_windowRowids, &m_statuses);
}

/* ------------------------------------------------------------------ */
/* Live2CommentAccessor (0x8134D372..0x8134D67F)                      */
/* ------------------------------------------------------------------ */

Live2CommentAccessor::Live2CommentAccessor(
	const paf::common::SharedPtr<Live2AccessorProxy> &proxy,
	SceInt64 parentRowid,
	int windowSize)
	: Live2Accessor(proxy),
	  m_windowSize(windowSize),
	  m_parentRowid(parentRowid),
	  m_comments()
{
}

Live2CommentAccessor::~Live2CommentAccessor()
{
}

/* 0x8134D372 */
paf::intrusive_ptr<Live2Record> Live2CommentAccessor::GetRecord(SceInt64 rowid)
{
	Live2CommentRecord *record = new Live2CommentRecord();
	record->m_rowid = rowid;

	for (size_t i = 0; i < m_windowRowids.size(); ++i) {
		if (m_windowRowids[i] == rowid) {
			if (i < m_comments.size())
				record->m_comment = m_comments[i];
			break;
		}
	}

	return paf::intrusive_ptr<Live2Record>(record);
}

/* 0x8134D4AC */
void Live2CommentAccessor::FetchCount()
{
	if (m_totalCount != -1)
		return;

	m_totalCount = 0;
	paf::vector<SceInt64> parentIds;
	parentIds.push_back(m_parentRowid);
	paf::vector<ActivityDbStatus> statuses;
	SceActivityDb_74F96707(m_proxy->m_queryHandle, &parentIds, &statuses);
	if (statuses.size() != 0)
		m_totalCount = statuses[0].numOfComments;
}

/* 0x8134D53C */
void Live2CommentAccessor::FetchWindow(int index)
{
	if (m_windowRowids.size() != 0 &&
	    index >= m_windowStart &&
	    index < m_windowStart + static_cast<int>(m_windowRowids.size()))
		return;

	m_windowRowids.clear();
	m_comments.clear();
	FetchCount();
	if (m_totalCount <= 0)
		return;

	int windowStart = 0;
	int windowCount = 0;
	ComputeWindowBounds(index, m_windowSize, m_totalCount, windowStart, windowCount);
	m_windowStart = windowStart;

	paf::vector<SceInt64> commentRowids;
	SceActivityDb_572A6463(m_proxy->m_queryHandle, &m_parentRowid, windowStart, windowCount,
			       &commentRowids);
	SceActivityDb_D512F568(m_proxy->m_queryHandle, &commentRowids, &m_comments);
	for (size_t i = 0; i < m_comments.size(); ++i)
		m_windowRowids.push_back(m_comments[i].rowid);
}

} /* namespace filelist */
