/*
 * SceSigninExt (vs0:sys/external/signin_ext.suprx):
 * SigninExtImageManager — thread-safe URL-keyed image fetch & Surface LRU
 * cache backed by a dedicated JobQueue and HttpImageClient.
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <std/list>
#include <std/string.h>
#include <std/string>
#include <common/shared_ptr.h>
#include <graphics/surface/surface.h>
#include <graphics/surface/surface_pool.h>
#include <image/image.h>
#include <job/job.h>
#include <thread/thread.h>
#include "signin_ext_internal.h"

using namespace paf;
using namespace paf::common;
using namespace paf::graph;
using namespace paf::image;
using namespace paf::job;
using namespace paf::thread;

static SigninExtImageManager *s_imageManager = NULL;

ImageCacheEntry::ImageCacheEntry()
	: m_url(), m_surface(), m_callbacks()
{
}

ImageCacheEntry::~ImageCacheEntry()
{
	while (m_callbacks.size() != 0) {
		ImageLoadCallback *cb = m_callbacks.front();
		m_callbacks.pop_front();
		if (cb != NULL)
			delete cb;
	}
}

SigninExtImageManager::SigninExtImageManager(SurfacePool *pool)
	: m_surfacePool(pool),
	  m_jobQueue(NULL),
	  m_httpClient(NULL),
	  m_cachedEntries(),
	  m_pendingEntries(),
	  m_mutex("SceSigninExtImageManager", 0),
	  m_cancelCallback(NULL)
{
	JobQueue::Option opt;
	opt.priority = GetPriority()->normal + 1;
	m_jobQueue = new JobQueue("SceSigninExtImageManager", &opt);
	m_httpClient = new HttpImageClient(m_jobQueue);
	m_httpClient->m_maxHttpsConns = 1;
}

SigninExtImageManager::~SigninExtImageManager()
{
	m_mutex.Lock();
	while (m_cachedEntries.size() != 0) {
		ImageCacheEntry *entry = m_cachedEntries.front();
		m_cachedEntries.pop_front();
		delete entry;
	}
	while (m_pendingEntries.size() != 0) {
		ImageCacheEntry *entry = m_pendingEntries.front();
		m_pendingEntries.pop_front();
		delete entry;
	}
	if (m_cancelCallback != NULL) {
		delete m_cancelCallback;
		m_cancelCallback = NULL;
	}
	if (m_httpClient != NULL) {
		delete m_httpClient;
		m_httpClient = NULL;
	}
	if (m_jobQueue != NULL) {
		delete m_jobQueue;
		m_jobQueue = NULL;
	}
	m_mutex.Unlock();
}

void SigninExtImageManager::NotifyEntry(ImageCacheEntry *entry, int32_t result)
{
	m_mutex.Lock();
	while (entry->m_callbacks.size() != 0) {
		ImageLoadCallback *cb = entry->m_callbacks.front();
		entry->m_callbacks.pop_front();
		if (cb != NULL) {
			if (result != -1)
				cb->Invoke(entry->m_url.c_str(), result, entry->m_surface);
			delete cb;
		}
	}
	m_mutex.Unlock();
}

void SigninExtImageManager::RequestImage(const char *url, ImageLoadCallback *callback,
					 const ImageExtentRequest *extent)
{
	m_mutex.Lock();

	for (list<ImageCacheEntry *>::iterator it = m_cachedEntries.begin();
	     it != m_cachedEntries.end(); ++it) {
		if (sce_paf_strcmp(url, (*it)->m_url.c_str()) == 0) {
			if (callback != NULL) {
				(*it)->m_callbacks.push_back(callback);
				NotifyEntry(*it, 0);
			}
			m_mutex.Unlock();
			return;
		}
	}

	for (list<ImageCacheEntry *>::iterator it = m_pendingEntries.begin();
	     it != m_pendingEntries.end(); ++it) {
		if (sce_paf_strcmp(url, (*it)->m_url.c_str()) == 0) {
			if (callback != NULL)
				(*it)->m_callbacks.push_back(callback);
			m_mutex.Unlock();
			return;
		}
	}

	ImageCacheEntry *entry = new ImageCacheEntry();
	if (entry == NULL) {
		m_mutex.Unlock();
		return;
	}
	entry->m_url = url;
	if (callback != NULL)
		entry->m_callbacks.push_back(callback);
	m_pendingEntries.push_back(entry);

	HttpImageRequestOption opt;
	opt.width = extent->width;
	opt.height = extent->height;
	opt.surfacePool = m_surfacePool;
	opt.surfaceCreator = new MemberFuncCallback3<
		SigninExtImageManager, intrusive_ptr<Surface>,
		SurfacePool *, const SharedPtr<Image> &, int32_t>(
		this, &SigninExtImageManager::CreateSurface);

	Callback2<HttpImageFetchJob *, int32_t> *doneCb =
		new MemberCallback2<SigninExtImageManager, HttpImageFetchJob *, int32_t, ImageCacheEntry *>(
			this, &SigninExtImageManager::OnImageFetched, entry);
	m_httpClient->EnqueueImage(url, doneCb, &opt);
	m_mutex.Unlock();
}

void SigninExtImageManager::CancelAll(Callback0 *onCanceled)
{
	m_mutex.Lock();
	m_cancelCallback = onCanceled;
	if (m_pendingEntries.size() == 0) {
		if (onCanceled != NULL) {
			m_cancelCallback = NULL;
			onCanceled->Invoke();
			delete onCanceled;
		}
		m_mutex.Unlock();
		return;
	}

	for (list<ImageCacheEntry *>::iterator it = m_pendingEntries.begin();
	     it != m_pendingEntries.end(); ++it) {
		ImageCacheEntry *entry = *it;
		while (entry->m_callbacks.size() != 0) {
			ImageLoadCallback *cb = entry->m_callbacks.front();
			entry->m_callbacks.pop_front();
			if (cb != NULL)
				delete cb;
		}
	}
	m_httpClient->CancelAll();
	m_mutex.Unlock();
}

void SigninExtImageManager::ClearAll(void)
{
	m_mutex.Lock();
	while (m_cachedEntries.size() != 0) {
		ImageCacheEntry *entry = m_cachedEntries.front();
		m_cachedEntries.pop_front();
		delete entry;
	}
	if (m_pendingEntries.size() != 0) {
		for (list<ImageCacheEntry *>::iterator it = m_pendingEntries.begin();
		     it != m_pendingEntries.end(); ++it) {
			ImageCacheEntry *entry = *it;
			while (entry->m_callbacks.size() != 0) {
				ImageLoadCallback *cb = entry->m_callbacks.front();
				entry->m_callbacks.pop_front();
				if (cb != NULL)
					delete cb;
			}
		}
		m_httpClient->CancelAll();
	}
	m_mutex.Unlock();
}

intrusive_ptr<Surface> SigninExtImageManager::CreateSurface(
	SurfacePool *pool, const SharedPtr<Image> &image, int32_t imageType, void *userData)
{
	(void)userData;
	m_mutex.Lock();

	for (;;) {
		intrusive_ptr<Surface> surf(new Surface(
			pool,
			image->GetExtent().w,
			image->GetExtent().h,
			image->GetMode(),
			image->GetOrder(),
			image->GetArraySize().x,
			imageType,
			false));
		if (surf.get() != NULL && surf->GetMemory() != NULL) {
			m_mutex.Unlock();
			return surf;
		}
		if (m_cachedEntries.size() == 0)
			break;
		ImageCacheEntry *oldest = m_cachedEntries.front();
		m_cachedEntries.pop_front();
		delete oldest;
	}

	m_mutex.Unlock();
	return intrusive_ptr<Surface>();
}

void SigninExtImageManager::OnImageFetched(HttpImageFetchJob *job, int32_t result,
					   ImageCacheEntry *entry)
{
	m_mutex.Lock();

	for (list<ImageCacheEntry *>::iterator it = m_pendingEntries.begin();
	     it != m_pendingEntries.end(); ++it) {
		if (*it == entry) {
			m_pendingEntries.erase(it);
			break;
		}
	}

	if (result >= 0) {
		entry->m_surface = job->m_surface;
		m_cachedEntries.push_back(entry);
	}

	NotifyEntry(entry, result);

	if (result < 0)
		delete entry;

	if (result == -1 && m_pendingEntries.size() == 0 && m_cancelCallback != NULL) {
		Callback0 *cb = m_cancelCallback;
		m_cancelCallback = NULL;
		cb->Invoke();
		delete cb;
	}

	m_mutex.Unlock();
}

void SigninExtImageManagerCreate(SurfacePool *pool)
{
	s_imageManager = new SigninExtImageManager(pool);
}

void SigninExtImageManagerDestroy(void)
{
	if (s_imageManager != NULL) {
		delete s_imageManager;
		s_imageManager = NULL;
	}
}

void SigninExtImageRequest(const char *url, ImageLoadCallback *callback,
			   const ImageExtentRequest *extent)
{
	if (s_imageManager != NULL)
		s_imageManager->RequestImage(url, callback, extent);
}

void SigninExtImageCancelAll(Callback0 *onCanceled)
{
	if (s_imageManager != NULL)
		s_imageManager->CancelAll(onCanceled);
}

void SigninExtImageClearAll(void)
{
	if (s_imageManager != NULL)
		s_imageManager->ClearAll();
}
