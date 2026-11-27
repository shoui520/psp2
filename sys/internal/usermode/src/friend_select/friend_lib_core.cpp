/*
 * SceFriendLib: NP login/NpId task ("SceFriendLibNp"), clock-jump watcher,
 * single-active async task scheduler ("SceFriendLibJobQueue"), and
 * surface-cached image manager ("SceFriendLibImageManager").
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <std/list>
#include <std/string.h>
#include <std/string>
#include <buffer/buffer.h>
#include <common/main_thread_call_list.h>
#include <common/shared_ptr.h>
#include <framework.h>
#include <graphics/surface/surface.h>
#include <graphics/surface/surface_pool.h>
#include <image/image.h>
#include <job/job.h>
#include <memory/heap_allocator.h>
#include <thread/thread.h>
#include <timer/timer.h>
#include <widget/core/widget.h>
#include "friend_select_internal.h"

extern "C" {
SceInt32 sceKernelGetProcessTime(SceKernelSysClock *pSysClock);
SceInt32 SceNpManager_B24FC028(int *state);
SceInt32 SceNpManager_A775A452(void);
SceInt32 SceNpManager_E45E4003(void *npIdOut);
paf::memory::HeapAllocator *GetGlobalHeapAllocator(void);
}

using namespace paf;
using namespace paf::common;
using namespace paf::graph;
using namespace paf::image;
using namespace paf::job;
using namespace paf::memory;
using namespace paf::thread;
using namespace paf::ui;

/* FriendLibNpTaskBase & FriendLibGetSelfNpIdTask */
FriendLibNpTaskBase::FriendLibNpTaskBase(Callback1<int32_t> *callback)
	: FriendLibQueuedTask(FriendLibGetJobQueue(), callback, "SceFriendLibNp")
{
}

FriendLibGetSelfNpIdTask::FriendLibGetSelfNpIdTask(SceNpId *outNpId, Callback1<int32_t> *callback)
	: FriendLibNpTaskBase(callback), m_outNpId(outNpId)
{
}

int32_t FriendLibGetSelfNpIdTask::Run()
{
	int npState = 0;
	if (SceNpManager_B24FC028(&npState) < 0 || npState == 0 || SceNpManager_A775A452() <= 0)
		return FRIEND_SELECT_ERROR_NP_OFFLINE;

	uint8_t profileBuf[328];
	int32_t ret = SceNpManager_E45E4003(profileBuf);
	if (ret >= 0 && m_outNpId != NULL)
		sce_paf_memcpy(m_outNpId, profileBuf, sizeof(SceNpId));
	return ret;
}

/* Clock-jump watcher */
static list<FriendLibClockListener *> s_clockListeners;
static list<FriendLibClockListener *>::iterator s_clockIter;
static uint64_t s_lastProcessClock = 0;
static uint64_t s_lastNetworkTick = 0;
static bool s_clockRegistered = false;
static bool s_clockIterErased = false;
static RMutex s_clockUnregMutex("SceFriendLibClock", 0);

static void ClockWatcherTick(void *userData)
{
	(void)userData;
	SceKernelSysClock procClock;
	SceRtcTick netTick;
	if (sceKernelGetProcessTime(&procClock) < 0 || FriendLibGetCurrentNetworkTick(&netTick) < 0)
		return;

	if (s_lastProcessClock != 0 && s_lastNetworkTick != 0) {
		uint64_t procDelta = procClock.quad - s_lastProcessClock;
		uint64_t netDelta = netTick.tick - s_lastNetworkTick;
		uint64_t hi = (procDelta >= netDelta) ? procDelta : netDelta;
		uint64_t lo = (procDelta >= netDelta) ? netDelta : procDelta;
		if (hi - lo > 100000ULL) {
			s_clockIter = s_clockListeners.begin();
			while (s_clockIter != s_clockListeners.end()) {
				(*s_clockIter)->OnClockChanged();
				if (s_clockIterErased)
					s_clockIterErased = false;
				else
					++s_clockIter;
			}
		}
	}
	s_lastProcessClock = procClock.quad;
	s_lastNetworkTick = netTick.tick;
}

void FriendLibClockInit(void)
{
	s_clockRegistered = false;
	s_clockIterErased = false;
	s_lastProcessClock = 0;
	s_lastNetworkTick = 0;
}

void FriendLibClockTerm(void)
{
	if (s_clockRegistered) {
		s_clockUnregMutex.Lock();
		MainThreadCallList::Unregister(ClockWatcherTick, NULL);
		s_clockUnregMutex.Unlock();
	}
	for (list<FriendLibClockListener *>::iterator it = s_clockListeners.begin();
	     it != s_clockListeners.end(); ++it)
		delete *it;
	s_clockListeners.clear();
}

void FriendLibClockRegister(FriendLibClockListener *listener)
{
	s_clockListeners.push_back(listener);
	if (!s_clockRegistered) {
		MainThreadCallList::Register(ClockWatcherTick, NULL);
		s_clockRegistered = true;
	}
}

namespace paf {
template <>
inline __attribute__((always_inline))
list<FriendLibClockListener *, allocator<FriendLibClockListener *> >::iterator
list<FriendLibClockListener *, allocator<FriendLibClockListener *> >::erase(iterator where)
{
	_Node *node = where.node;
	_Node *next = node->next;
	node->next->prev = node->prev;
	node->prev->next = node->next;
	destroy(&node->value);
	operator delete(node);
	--count;
	return iterator(next);
}
}

void FriendLibClockUnregister(void *owner)
{
	for (list<FriendLibClockListener *>::iterator it = s_clockListeners.begin();
	     it != s_clockListeners.end(); ++it) {
		if ((*it)->MatchesOwner(owner)) {
			FriendLibClockListener *removed = *it;
			if (it == s_clockIter) {
				s_clockIter = s_clockListeners.erase(it);
				s_clockIterErased = true;
			} else {
				s_clockListeners.erase(it);
			}
			delete removed;
			return;
		}
	}
}

static const int32_t s_commonSeTable[5] = { 1, 2, 3, 12, 13 };

void FriendLibPlayCommonSE(uint32_t index)
{
	int32_t idx = (int32_t)index;
	if (idx >= 0 && idx < 5)
		Framework::Instance()->PlayCommonSE(s_commonSeTable[idx], 0.0f, false);
}

void FriendLibNormalizeNewlines(string &text)
{
	size_t len = text.size();
	char *buf = new char[len + 1U];
	if (buf == NULL)
		return;
	size_t outPos = 0;
	size_t i = 0;
	while (i < len) {
		char ch = text[i];
		if (ch == '\r') {
			buf[outPos++] = ' ';
			if (i + 1U < len && text[i + 1U] == '\n')
				i += 2U;
			else
				++i;
		} else if (ch == '\n') {
			buf[outPos++] = ' ';
			++i;
		} else {
			buf[outPos++] = ch;
			++i;
		}
	}
	buf[outPos] = '\0';
	text.assign(buf, outPos);
	delete[] buf;
}

/* Single-active async task scheduler */
static FriendLibAsyncTask *s_activeTask = NULL;
static HeapAllocator *s_coreAllocator = NULL;
static SurfacePool *s_coreSurfacePool = NULL;
static JobQueue *s_coreJobQueue = NULL;

bool FriendLibIsTaskRunning(void)
{
	return s_activeTask != NULL;
}

void FriendLibCancelActiveTask(void)
{
	if (s_activeTask != NULL)
		s_activeTask->RequestCancel();
}

void FriendLibStartTask(FriendLibAsyncTask *task)
{
	if (task == NULL)
		return;
	if (s_activeTask != NULL) {
		task->NotifyCallback(FRIEND_SELECT_ERROR_BUSY);
		delete task;
		return;
	}
	s_activeTask = task;
	task->Start();
}

FriendLibAsyncTask::~FriendLibAsyncTask()
{
	if (m_callback != NULL) {
		delete m_callback;
		m_callback = NULL;
	}
}

void FriendLibAsyncTask::OnComplete(int32_t result)
{
	FriendLibAsyncTask *finished = s_activeTask;
	s_activeTask = NULL;
	NotifyCallback(result);
	if (finished != NULL)
		delete finished;
}

void FriendLibAsyncTask::NotifyCallback(int32_t result)
{
	if (m_callback != NULL) {
		Callback1<int32_t> *cb = m_callback;
		m_callback = NULL;
		cb->Invoke(result);
		delete cb;
	}
}

FriendLibQueuedTask::FriendLibQueuedTask(JobQueue *queue, Callback1<int32_t> *callback,
					 const char *jobName)
	: FriendLibAsyncTask(callback), JobItem(jobName, NULL), m_queue(queue)
{
	if (m_queue == NULL)
		m_queue = FriendLibGetJobQueue();
}

FriendLibQueuedTask::~FriendLibQueuedTask()
{
}

void FriendLibQueuedTask::Start()
{
	SharedPtr<JobItem> item(static_cast<JobItem *>(this));
	m_queue->Enqueue(item);
}

void FriendLibQueuedTask::RequestCancel()
{
	JobItem::Cancel();
}

void FriendLibQueuedTask::OnComplete(int32_t result)
{
	s_activeTask = NULL;
	NotifyCallback(result);
}

void FriendLibQueuedTask::Finish(int32_t result)
{
	if (IsCanceled())
		result = -1;
	OnComplete(result);
}

void FriendLibCoreInit(HeapAllocator *allocator, SurfacePool *surfacePool)
{
	s_coreAllocator = (allocator != NULL) ? allocator : GetGlobalHeapAllocator();
	s_coreSurfacePool = surfacePool;
	s_coreJobQueue = new JobQueue("SceFriendLibJobQueue", NULL);
	FriendLibHttpInit();
	FriendLibClockInit();
}

void FriendLibCoreTerm(void)
{
	FriendLibClockTerm();
	FriendLibHttpTerm();
	if (s_coreJobQueue != NULL) {
		if (s_coreJobQueue->NumItems() != 0) {
			s_coreJobQueue->CancelAllItems();
			s_coreJobQueue->WaitEmpty();
		}
		delete s_coreJobQueue;
		s_coreJobQueue = NULL;
	}
	s_coreSurfacePool = NULL;
	s_coreAllocator = NULL;
}

SharedPtr<Buffer> FriendLibAllocBuffer(size_t size)
{
	return HeapAllocBuffer::Allocate(size, s_coreAllocator);
}

SurfacePool *FriendLibGetSurfacePool(void)
{
	return s_coreSurfacePool;
}

JobQueue *FriendLibGetJobQueue(void)
{
	return s_coreJobQueue;
}

/* SceFriendLibImageManager */
class FriendLibImageReceiver {
public:
	virtual ~FriendLibImageReceiver() {}
	virtual void OnImageReady(const char *url, int32_t result,
				  intrusive_ptr<Surface> *surface) = 0;
	virtual bool MatchesWidget(Widget *widget) const = 0;
};

class ImageWidgetReceiver : public FriendLibImageReceiver {
public:
	explicit ImageWidgetReceiver(Widget *widget) : m_widget(widget) {}
	virtual ~ImageWidgetReceiver() {}

	virtual void OnImageReady(const char *url, int32_t result,
				  intrusive_ptr<Surface> *surface)
	{
		(void)url;
		if (result >= 0 && m_widget != NULL) {
			intrusive_ptr<Surface> prev;
			m_widget->GetTexture(prev);
			m_widget->SetTexture(*surface);
			if (prev.get() != surface->get()) {
				m_widget->Hide(0xFF, NULL, NULL, 0.0f);
				m_widget->Show(0xFF, new Timer(200.0f), NULL, 0.0f);
			}
		}
	}

	virtual bool MatchesWidget(Widget *widget) const
	{
		return m_widget == widget;
	}

	Widget *m_widget;
};

struct ImageCacheEntry {
	ImageCacheEntry() : url(), surface(), receivers() {}
	~ImageCacheEntry();

	string url;
	intrusive_ptr<Surface> surface;
	list<FriendLibImageReceiver *> receivers;
};

ImageCacheEntry::~ImageCacheEntry()
{
	for (list<FriendLibImageReceiver *>::iterator it = receivers.begin();
	     it != receivers.end(); ++it)
		delete *it;
	receivers.clear();
}

class FriendLibImageManager {
public:
	explicit FriendLibImageManager(SurfacePool *surfacePool);
	virtual ~FriendLibImageManager();

	void NotifyEntry(ImageCacheEntry *entry, int32_t result);
	void Request(const char *url, FriendLibImageReceiver *receiver, const int32_t maxDims[2]);
	void CancelAll(Callback0 *onDone);
	void CancelForWidget(const char *url, Widget *widget);
	intrusive_ptr<Surface> AllocateSurface(SurfacePool *pool, Image *img, int32_t numPages,
					       void *arg);
	void OnImageLoaded(FriendLibImageJob *job, int32_t result, ImageCacheEntry *entry);

	SurfacePool *m_surfacePool;
	JobQueue *m_jobQueue;
	FriendLibImageLoader *m_loader;
	list<ImageCacheEntry *> m_cached;
	list<ImageCacheEntry *> m_pending;
	RMutex m_mutex;
	Callback0 *m_cancelCallback;
};

FriendLibImageManager::FriendLibImageManager(SurfacePool *surfacePool)
	: m_surfacePool(surfacePool), m_jobQueue(NULL), m_loader(NULL), m_cached(), m_pending(),
	  m_mutex("SceFriendLibImageManager", 0), m_cancelCallback(NULL)
{
	JobQueue::Option opt;
	opt.priority = GetPriority()->normal + 1;
	m_jobQueue = new JobQueue("SceFriendLibImageManager", &opt);
	m_loader = new FriendLibImageLoader(m_jobQueue);
	m_loader->m_maxHttpConns = 1;
	m_loader->m_maxHttpsConns = 1;
}

FriendLibImageManager::~FriendLibImageManager()
{
	m_mutex.Lock();
	while ((m_cached.size() != 0)) {
		ImageCacheEntry *e = m_cached.front();
		m_cached.pop_front();
		delete e;
	}
	while ((m_pending.size() != 0)) {
		ImageCacheEntry *e = m_pending.front();
		m_pending.pop_front();
		delete e;
	}
	if (m_cancelCallback != NULL) {
		delete m_cancelCallback;
		m_cancelCallback = NULL;
	}
	if (m_loader != NULL) {
		delete m_loader;
		m_loader = NULL;
	}
	if (m_jobQueue != NULL) {
		delete m_jobQueue;
		m_jobQueue = NULL;
	}
	m_mutex.Unlock();
}

void FriendLibImageManager::NotifyEntry(ImageCacheEntry *entry, int32_t result)
{
	m_mutex.Lock();
	for (list<FriendLibImageReceiver *>::iterator it = entry->receivers.begin();
	     it != entry->receivers.end(); ++it) {
		FriendLibImageReceiver *r = *it;
		if (r != NULL) {
			if (result != -1)
				r->OnImageReady(entry->url.c_str(), result, &entry->surface);
			delete r;
		}
	}
	entry->receivers.clear();
	m_mutex.Unlock();
}

void FriendLibImageManager::Request(const char *url, FriendLibImageReceiver *receiver,
				    const int32_t maxDims[2])
{
	m_mutex.Lock();
	for (list<ImageCacheEntry *>::iterator it = m_cached.begin(); it != m_cached.end(); ++it) {
		if (sce_paf_strcmp(url, (*it)->url.c_str()) == 0) {
			if (receiver != NULL) {
				(*it)->receivers.push_back(receiver);
				NotifyEntry(*it, 0);
			}
			m_mutex.Unlock();
			return;
		}
	}
	for (list<ImageCacheEntry *>::iterator it = m_pending.begin(); it != m_pending.end();
	     ++it) {
		if (sce_paf_strcmp(url, (*it)->url.c_str()) == 0) {
			if (receiver != NULL)
				(*it)->receivers.push_back(receiver);
			m_mutex.Unlock();
			return;
		}
	}

	ImageCacheEntry *entry = new ImageCacheEntry();
	if (entry == NULL) {
		m_mutex.Unlock();
		return;
	}
	entry->url = url;
	if (receiver != NULL)
		entry->receivers.push_back(receiver);
	m_pending.push_back(entry);

	FriendLibImageLoadOption opt;
	opt.maxWidth = maxDims[0];
	opt.maxHeight = maxDims[1];
	opt.surfacePool = m_surfacePool;
	opt.allocCb = new MemberFuncCallback3<FriendLibImageManager, intrusive_ptr<Surface>,
					      SurfacePool *, Image *, int32_t>(
		this, &FriendLibImageManager::AllocateSurface);
	m_loader->EnqueueImage(
		url,
		new MemberCallback2<FriendLibImageManager, FriendLibImageJob *, int32_t,
				    ImageCacheEntry *>(
			this, &FriendLibImageManager::OnImageLoaded, entry),
		&opt);
	m_mutex.Unlock();
}

void FriendLibImageManager::CancelAll(Callback0 *onDone)
{
	m_mutex.Lock();
	m_cancelCallback = onDone;
	if ((m_pending.size() != 0)) {
		for (list<ImageCacheEntry *>::iterator it = m_pending.begin();
		     it != m_pending.end(); ++it) {
			ImageCacheEntry *e = *it;
			for (list<FriendLibImageReceiver *>::iterator rit = e->receivers.begin();
			     rit != e->receivers.end(); ++rit)
				delete *rit;
			e->receivers.clear();
		}
		m_loader->CancelAllJobs();
	} else if (onDone != NULL) {
		m_cancelCallback = NULL;
		onDone->Invoke();
		delete onDone;
	}
	m_mutex.Unlock();
}

void FriendLibImageManager::CancelForWidget(const char *url, Widget *widget)
{
	m_mutex.Lock();
	if ((m_pending.size() != 0)) {
		for (list<ImageCacheEntry *>::iterator it = m_pending.begin();
		     it != m_pending.end(); ++it) {
			if (sce_paf_strcmp(url, (*it)->url.c_str()) == 0) {
				ImageCacheEntry *e = *it;
				for (list<FriendLibImageReceiver *>::iterator rit =
					     e->receivers.begin();
				     rit != e->receivers.end(); ++rit) {
					if (*rit != NULL && (*rit)->MatchesWidget(widget)) {
						FriendLibImageReceiver *rem = *rit;
						e->receivers.erase(rit);
						delete rem;
						break;
					}
				}
				if ((e->receivers.size() == 0))
					m_loader->CancelJobByUrl(url);
				break;
			}
		}
	}
	m_mutex.Unlock();
}

intrusive_ptr<Surface> FriendLibImageManager::AllocateSurface(SurfacePool *pool, Image *img,
							      int32_t numPages, void *arg)
{
	(void)arg;
	m_mutex.Lock();
	for (;;) {
		ImageExtent ext = img->GetExtent();
		intrusive_ptr<Surface> surf(new Surface(pool, ext.w, ext.h, img->GetMode(),
							img->GetOrder(), img->GetArraySize().x,
							numPages, false));
		if (surf.get() != NULL && surf->GetMemory() != NULL) {
			m_mutex.Unlock();
			return surf;
		}
		if ((m_cached.size() == 0))
			break;
		ImageCacheEntry *evict = m_cached.front();
		m_cached.pop_front();
		delete evict;
	}
	m_mutex.Unlock();
	return intrusive_ptr<Surface>();
}

void FriendLibImageManager::OnImageLoaded(FriendLibImageJob *job, int32_t result,
					  ImageCacheEntry *entry)
{
	m_mutex.Lock();
	for (list<ImageCacheEntry *>::iterator it = m_pending.begin(); it != m_pending.end();
	     ++it) {
		if (*it == entry) {
			m_pending.erase(it);
			break;
		}
	}
	if (result >= 0) {
		entry->surface = job->m_surface;
		m_cached.push_back(entry);
	}
	NotifyEntry(entry, result);
	if (result < 0)
		delete entry;
	if (result == -1 && (m_pending.size() == 0) && m_cancelCallback != NULL) {
		Callback0 *cb = m_cancelCallback;
		m_cancelCallback = NULL;
		cb->Invoke();
		delete cb;
	}
	m_mutex.Unlock();
}

static FriendLibImageManager *s_imageManager = NULL;

void FriendLibImageManagerInit(SurfacePool *surfacePool)
{
	s_imageManager = new FriendLibImageManager(surfacePool);
}

void FriendLibImageManagerTerm(void)
{
	if (s_imageManager != NULL) {
		delete s_imageManager;
		s_imageManager = NULL;
	}
}

void FriendLibRequestWidgetImage(const char *url, Widget *widget)
{
	if (widget == NULL)
		return;
	math::v4 sz = *widget->GetSize(0);
	int32_t dims[2];
	dims[0] = (int32_t)sz.extract_x();
	dims[1] = (int32_t)sz.extract_y();
	ImageWidgetReceiver *recv = new ImageWidgetReceiver(widget);
	if (s_imageManager != NULL)
		s_imageManager->Request(url, recv, dims);
	else
		delete recv;
}

void FriendLibCancelAllImages(Callback0 *onDone)
{
	if (s_imageManager != NULL)
		s_imageManager->CancelAll(onDone);
}

void FriendLibCancelWidgetImage(const char *url, Widget *widget)
{
	if (s_imageManager != NULL)
		s_imageManager->CancelForWidget(url, widget);
}

bool FriendLibHasPendingImages(void)
{
	return (s_imageManager != NULL) && (s_imageManager->m_pending.size() != 0);
}
