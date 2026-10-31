#include <cache/cache_manager_impl.h>
#include <cache/cache_image.h>
#include <cache/cache_job_runner.h>
#include <common/shared_ptr.h>
#include <graphics/surface/surface.h>
#include <job/job.h>
#include <memory/heap_allocator.h>
#include <std/new>
#include <std/stdlib.h>

namespace paf {
namespace cache {

thread::RMutex CacheManagerImpl::m_rmutex("ScePafCacheRmutex", 0);

CacheManagerImpl::CacheManagerImpl(const CacheManager::Params *param)
	: m_longlife_heap(param->longlife_heap), m_instant_heap(param->instant_heap),
	  m_cache_instance_heap(param->cache_instance_heap),
	  m_longlife_align_size(param->longlife_heap_align_size),
	  m_instant_align_size(param->instant_heap_align_size), m_name(param->name),
	  m_cancel_func(param->cancel_func), m_cancel_userdata(param->cancel_userdata),
	  m_reserved0(param->reserved0), m_reserved1(param->reserved1),
	  m_destroy_requested(false), m_is_shutdown(false),
	  m_cache_longlife_heap(param->cache_longlife_heap),
	  m_local_file_nobuf(param->local_file_nobuf), m_job_count(0),
	  m_image_cache_divide_thread(param->image_cache_divide_thread),
	  m_http_file_open_flag(param->http_file_open_flag), m_https_flag(param->https_flag),
	  m_surface_pool(param->surface_pool)
{
	if (param == NULL)
		return;
	if (param->longlife_heap != NULL)
		param->longlife_heap->SetDisableAssertOnAllocFailure(true);
	else
		m_cache_longlife_heap = false;
	if (param->instant_heap != NULL)
		param->instant_heap->SetDisableAssertOnAllocFailure(true);
	if (param->cache_instance_heap != NULL)
		param->cache_instance_heap->SetDisableAssertOnAllocFailure(true);
	if (m_surface_pool != NULL)
		ScePafGraphics_83ABDFF5(m_surface_pool, 1);
}

/* Cancels every job, waits for them to end, then drops every cache. */
CacheManagerImpl::~CacheManagerImpl()
{
	m_rmutex.Lock();
	for (CacheMap::iterator where = m_caches.begin(); where != m_caches.end(); ++where) {
		CacheList &caches = where->second;

		for (CacheList::iterator it = caches.begin(); it != caches.end(); it++) {
			CacheBase *cache = it->get();

			if (cache != NULL && cache->GetJobStatus() != 2)
				cache->CancelJob();
		}
	}
	m_rmutex.Unlock();
	for (;;) {
		int32_t busy = 0;

		m_rmutex.Lock();
		for (CacheMap::iterator where = m_caches.begin(); where != m_caches.end(); ++where) {
			CacheList &caches = where->second;

			for (CacheList::iterator it = caches.begin(); it != caches.end(); it++) {
				CacheBase *cache = it->get();

				if (cache != NULL && cache->GetJobStatus() != 2)
					busy++;
			}
		}
		m_rmutex.Unlock();
		if (busy == 0)
			break;
		thread::Sleep(2000);
	}
	m_rmutex.Lock();
	for (CacheMap::iterator where = m_caches.begin(); where != m_caches.end(); ++where) {
		CacheList &caches = where->second;
		CacheList::iterator it = caches.begin();

		while (it != caches.end())
			it = caches.erase(it);
	}
	m_rmutex.Unlock();
	m_rmutex.Lock();
	m_caches.erase(m_caches.begin(), m_caches.end());
	m_rmutex.Unlock();
}

/* Drops cache from its path's list once its job is done. */
int32_t CacheManagerImpl::RemoveCache(CacheBase *cache)
{
	m_rmutex.Lock();
	if (cache == NULL) {
		m_rmutex.Unlock();
		return SCE_PAF_ERROR_CACHE_NO_JOB_QUEUE;
	}

	CacheMap::iterator where = m_caches.find(cache->m_path);

	if (where != m_caches.end()) {
		CacheList &caches = where->second;

		for (CacheList::iterator it = caches.begin(); it != caches.end(); it++) {
			if (it->get() == cache) {
				if (cache->GetJobStatus() == 2) {
					caches.erase(it);
					if (caches.size() == 0)
						m_caches.erase(where);
				}
				break;
			}
		}
	}
	m_rmutex.Unlock();
	return 0;
}

int32_t CacheManagerImpl::CancelCache(const string &path)
{
	CacheBase *cache = FindCache<CacheBase>(path, NULL);

	if (cache == NULL)
		return SCE_PAF_ERROR_CACHE_NO_JOB;
	cache->CancelJob();
	return 0;
}

void CacheManagerImpl::AddJobCount(int32_t count)
{
	m_rmutex.Lock();
	m_job_count += count;
	m_rmutex.Unlock();
}

/* Queues a CacheJobRunner for cache on its queue index (0 or 1). */
int32_t CacheManagerImpl::EnqueueCache(CacheBase *cache, uint32_t index)
{
	if (cache == NULL)
		return SCE_PAF_ERROR_CACHE_INVALID_ARG;
	if (cache->GetJobStatus() == 1 && index != 1)
		return 0;

	intrusive_ptr<CacheBase> ptr(cache);
	common::SharedPtr<job::JobItem> item(new CacheJobRunner(ptr, NULL));

	if (item.get() == NULL)
		return SCE_PAF_ERROR_CACHE_NO_MEMORY;

	job::JobQueue *q = NULL;

	if (index >= 2)
		sce_paf_printf_dummy();
	else
		q = cache->m_jobq[index];
	if (q == NULL)
		return SCE_PAF_ERROR_CACHE_INVALID_ARG;

	int32_t ret = q->Enqueue(item);

	if (ret < 0)
		return ret;
	m_rmutex.Lock();
	++m_job_count;
	m_rmutex.Unlock();
	return 0;
}

/* The releasable cache touched least recently, not one being loaded. */
bool CacheManagerImpl::CheckCacheAll(CacheBase **found)
{
	m_rmutex.Lock();
	if (m_caches.size() != 0) {
		int32_t max_age = 0;
		CacheBase *oldest = NULL;

		for (CacheMap::iterator where = m_caches.begin(); where != m_caches.end();
		     ++where) {
			CacheList &caches = where->second;

			for (CacheList::iterator it = caches.begin(); it != caches.end(); it++) {
				CacheBase *cache = it->get();

				if (cache != NULL && cache->GetJobStatus() != 1 &&
				    cache->IsEnableReleaseInstance()) {
					int32_t ticks = cache->GetTicks();

					if (max_age < CacheManager::m_ticks - ticks) {
						oldest = it->get();
						max_age = CacheManager::m_ticks - ticks;
					}
				}
			}
		}
		if (oldest == NULL) {
			m_rmutex.Unlock();
			return false;
		}
		if (found != NULL)
			*found = oldest;
	} else if (found != NULL) {
		*found = NULL;
	}
	m_rmutex.Unlock();
	return true;
}

/* As above, among the caches check accepts. */
bool CacheManagerImpl::CheckCacheAll(bool (*check)(CacheBase *cache), CacheBase **found)
{
	if (check == NULL)
		return false;
	m_rmutex.Lock();
	if (m_caches.size() != 0) {
		int32_t max_age = 0;
		CacheBase *oldest = NULL;

		for (CacheMap::iterator where = m_caches.begin(); where != m_caches.end();
		     ++where) {
			CacheList &caches = where->second;

			for (CacheList::iterator it = caches.begin(); it != caches.end(); it++) {
				CacheBase *cache = it->get();

				if (cache != NULL && cache->GetJobStatus() != 1 && check(cache)) {
					int32_t ticks = cache->GetTicks();

					if (max_age < CacheManager::m_ticks - ticks) {
						oldest = it->get();
						max_age = CacheManager::m_ticks - ticks;
					}
				}
			}
		}
		if (oldest == NULL) {
			m_rmutex.Unlock();
			return false;
		}
		if (found != NULL)
			*found = oldest;
	} else if (found != NULL) {
		*found = NULL;
	}
	m_rmutex.Unlock();
	return true;
}

/*
 * Cancels cache and drops every cache of its path; one still loading is
 * marked removed instead and keeps the path's entry.
 */
int32_t CacheManagerImpl::ClearCache(CacheBase *cache)
{
	if (cache == NULL)
		return SCE_PAF_ERROR_CACHE_NO_JOB_QUEUE;

	CacheMap::iterator where = m_caches.find(cache->m_path);

	if (where == m_caches.end())
		return SCE_PAF_ERROR_CACHE_NOT_REGISTERED;
	cache->CancelJob();

	CacheList &caches = where->second;
	int32_t busy = 0;
	CacheList::iterator it = caches.begin();

	while (it != caches.end()) {
		CacheBase *entry = it->get();

		if (entry == NULL) {
			it = caches.erase(it);
		} else if (entry->GetJobStatus() == 1) {
			entry->m_removed = true;
			busy++;
			it++;
		} else {
			entry->m_manager = NULL;
			it = caches.erase(it);
		}
	}
	if (busy == 0)
		m_caches.erase(where);
	return 0;
}

/* Drops cache alone from its path's list. */
int32_t CacheManagerImpl::RemoveCacheEntry(CacheBase *cache)
{
	if (cache == NULL)
		return SCE_PAF_ERROR_CACHE_NO_JOB_QUEUE;

	CacheMap::iterator where = m_caches.find(cache->m_path);

	if (where == m_caches.end())
		return SCE_PAF_ERROR_CACHE_NOT_REGISTERED;
	cache->CancelJob();

	CacheList &caches = where->second;

	for (CacheList::iterator it = caches.begin(); it != caches.end(); it++) {
		if (it->get() == cache) {
			cache->m_manager = NULL;
			caches.erase(it);
			break;
		}
	}
	if (caches.size() == 0)
		m_caches.erase(where);
	return 0;
}

int32_t CacheManagerImpl::ClearCache(const string &path)
{
	CacheBase *cache = FindCache<CacheBase>(path, NULL);

	if (cache == NULL)
		return 0;
	return ClearCache(cache);
}

uint32_t CacheManagerImpl::GetCacheCount()
{
	m_rmutex.Lock();

	uint32_t count = 0;

	for (CacheMap::iterator where = m_caches.begin(); where != m_caches.end(); ++where)
		count += where->second.size();
	m_rmutex.Unlock();
	return count;
}

void CacheManagerImpl::SetShutdownFlag(bool flag)
{
	m_rmutex.Lock();
	m_is_shutdown = flag;
	m_rmutex.Unlock();
}

/* Drops loaded, releasable images until the pool can hold a width x height surface. */
int32_t CacheManagerImpl::MakeSpaceSurfacePool(uint32_t width, uint32_t height)
{
	if (m_surface_pool == NULL)
		return SCE_PAF_ERROR_CACHE_NO_JOB_QUEUE;
	m_rmutex.Lock();

	CacheMap::iterator where = m_caches.begin();

	while (where != m_caches.end()) {
		CacheList &caches = where->second;
		CacheList::iterator it = caches.begin();
		bool erased = false;

		while (it != caches.end()) {
			CacheBase *cache = it->get();

			if (cache->m_cache_type == 1) {
				int32_t ret = graph::Surface::IsAllocatableSurfacePool(m_surface_pool,
										       width, height);

				if (ret != SCE_PAF_ERROR_GRAPHICS_NO_SPACE) {
					m_rmutex.Unlock();
					return ret;
				}
				if (CacheImage::CheckSurfacePool(cache) && cache != NULL &&
				    cache->IsEnableReleaseInstance() && cache->GetJobStatus() == 2) {
					it = caches.erase(it);
					if (caches.size() == 0) {
						m_caches.erase(where++);
						erased = true;
						break;
					}
					continue;
				}
			}
			++it;
		}
		if (!erased)
			++where;
	}
	m_rmutex.Unlock();
	return SCE_PAF_ERROR_CACHE_NO_TEXTURE;
}

/* Drops every loaded, releasable image that holds pool memory. */
int32_t CacheManagerImpl::MakeSpaceSurfacePool()
{
	if (m_surface_pool == NULL)
		return SCE_PAF_ERROR_CACHE_NO_JOB_QUEUE;
	m_rmutex.Lock();

	CacheMap::iterator where = m_caches.begin();

	while (where != m_caches.end()) {
		CacheList &caches = where->second;
		CacheList::iterator it = caches.begin();
		bool erased = false;

		while (it != caches.end()) {
			CacheBase *cache = it->get();

			if (cache->m_cache_type == 1 && CacheImage::CheckSurfacePool(cache) &&
			    cache != NULL && cache->IsEnableReleaseInstance() &&
			    cache->GetJobStatus() == 2) {
				it = caches.erase(it);
				if (caches.size() == 0) {
					m_caches.erase(where++);
					erased = true;
					break;
				}
				continue;
			}
			++it;
		}
		if (!erased)
			++where;
	}
	m_rmutex.Unlock();
	return 0;
}

int32_t CacheManagerImpl::GetJobCount()
{
	m_rmutex.Lock();

	int32_t count = m_job_count;

	m_rmutex.Unlock();
	return count;
}

/*
 * Long-life memory for cache's data; with retry, the oldest cache holding
 * long-life memory is cleared (and removed when releasable) until it fits.
 */
void *CacheManagerImpl::AllocLongLifeHeap(size_t size, bool retry, CacheBase *cache)
{
	m_rmutex.Lock();

	memory::HeapAllocator *heap = m_longlife_heap;

	while (cache == NULL || CacheBase::IsCancel(cache) != true) {
		void *ptr;

		if (heap != NULL)
			ptr = heap->sce_paf_memalign(m_longlife_align_size, size);
		else
			ptr = sce_paf_memalign(m_instant_align_size, size);
		if (ptr != NULL) {
			m_rmutex.Unlock();
			return ptr;
		}
		if (!retry)
			break;

		CacheBase *found = NULL;

		if (!CheckCacheAll(CacheBase::CheckLongLifeHeap, &found))
			break;
		if (found == NULL)
			continue;
		found->Clear();
		if (!found->IsEnableReleaseInstance())
			continue;
		RemoveCache(found);
	}
	m_rmutex.Unlock();
	return NULL;
}

void CacheManagerImpl::FreeLongLifeHeap(void *ptr)
{
	m_rmutex.Lock();
	if (m_longlife_heap != NULL)
		m_longlife_heap->sce_paf_free(ptr);
	else
		sce_paf_free(ptr);
	m_rmutex.Unlock();
}

void *CacheManagerImpl::AllocInstantHeap(size_t size)
{
	m_rmutex.Lock();

	void *ptr;

	if (m_instant_heap != NULL)
		ptr = m_instant_heap->sce_paf_memalign(m_instant_align_size, size);
	else
		ptr = sce_paf_memalign(m_instant_align_size, size);
	m_rmutex.Unlock();
	return ptr;
}

void CacheManagerImpl::FreeInstantHeap(void *ptr)
{
	m_rmutex.Lock();
	if (m_instant_heap != NULL)
		m_instant_heap->sce_paf_free(ptr);
	else
		sce_paf_free(ptr);
	m_rmutex.Unlock();
}

/* Detaches every cache of manager from it. */
void CacheManagerImpl::ClearCacheAll(CacheManagerImpl *manager)
{
	m_rmutex.Lock();
	for (CacheMap::iterator where = manager->m_caches.begin();
	     where != manager->m_caches.end(); ++where) {
		CacheList &caches = where->second;

		for (CacheList::iterator it = caches.begin(); it != caches.end(); it++)
			it->get()->m_manager = NULL;
	}
	m_rmutex.Unlock();
}

/* Registers cache under its path and queues its first job. */
int32_t CacheManagerImpl::AddCache(CacheBase *cache, job::JobQueue *q0, job::JobQueue *q1)
{
	if (cache == NULL || q0 == NULL || cache->m_path.size() == 0)
		return SCE_PAF_ERROR_CACHE_INVALID_ARG;
	if (!cache->m_regist_manager) {
		m_caches[cache->m_path].push_back(intrusive_ptr<CacheBase>(cache));
		cache->m_regist_manager = true;
	}
	cache->m_jobq[0] = q0;
	cache->m_jobq[1] = q1 != NULL && m_image_cache_divide_thread ? q1 : q0;

	int32_t ret = EnqueueCache(cache, 0);

	if (ret >= 0)
		ret = 0;
	return ret;
}

}
}
