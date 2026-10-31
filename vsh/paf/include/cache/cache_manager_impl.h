/*
 * paf::cache::CacheManagerImpl: a named set of caches, a list of them per
 * path, with the heaps their data and instances come from. Everything is
 * done under m_rmutex, which all managers share.
 */
#ifndef PAF_CACHE_CACHE_MANAGER_IMPL_H
#define PAF_CACHE_CACHE_MANAGER_IMPL_H

#include <stddef.h>
#include <stdint.h>
#include <std/string>
#include <std/list>
#include <std/map>
#include <std/memory>
#include <thread/thread.h>
#include <cache/cache_base.h>
#include <cache/cache_manager.h>
#include <src/paf_types.h>

namespace paf {
namespace cache {

class CacheManagerImpl {
public:
	typedef list<intrusive_ptr<CacheBase> > CacheList;
	typedef map<string, CacheList> CacheMap;

	static void ClearCacheAll(CacheManagerImpl *manager);

	CacheManagerImpl(const CacheManager::Params *param);
	virtual ~CacheManagerImpl();

	int32_t RemoveCache(CacheBase *cache);
	int32_t CancelCache(const string &path);
	void AddJobCount(int32_t count);
	int32_t EnqueueCache(CacheBase *cache, uint32_t index);
	bool CheckCacheAll(CacheBase **found);
	bool CheckCacheAll(bool (*check)(CacheBase *cache), CacheBase **found);
	int32_t ClearCache(CacheBase *cache);
	int32_t RemoveCacheEntry(CacheBase *cache);
	int32_t ClearCache(const string &path);
	uint32_t GetCacheCount();
	void SetShutdownFlag(bool flag);
	int32_t MakeSpaceSurfacePool(uint32_t width, uint32_t height);
	int32_t MakeSpaceSurfacePool();
	int32_t GetJobCount();
	void *AllocLongLifeHeap(size_t size, bool retry, CacheBase *cache);
	void FreeLongLifeHeap(void *ptr);
	void *AllocInstantHeap(size_t size);
	void FreeInstantHeap(void *ptr);
	int32_t AddCache(CacheBase *cache, job::JobQueue *q0, job::JobQueue *q1);

	/* The first cache of path whose option is option's, NULL for an empty path. */
	template<class T>
	T *FindCache(const string &path, const typename T::Option *option);

	/*
	 * Puts the cache of path in ptr, made and queued when there is none; when
	 * no instance can be made, the oldest releasable cache makes room.
	 */
	template<class T, class P>
	int32_t CreateCache(const string &path, intrusive_ptr<P> &ptr,
			    const typename T::Option *option, job::JobQueue *q0,
			    job::JobQueue *q1);

	static thread::RMutex m_rmutex;

	memory::HeapAllocator *m_longlife_heap;
	memory::HeapAllocator *m_instant_heap;
	memory::HeapAllocator *m_cache_instance_heap;
	size_t m_longlife_align_size;
	size_t m_instant_align_size;
	string m_name;
	CacheMap m_caches;
	CacheManager::CancelFunc m_cancel_func;
	void *m_cancel_userdata;
	uint32_t m_reserved0;
	uint32_t m_reserved1;
	bool m_destroy_requested;
	bool m_is_shutdown;
	bool m_cache_longlife_heap;
	bool m_local_file_nobuf;
	int32_t m_job_count;
	bool m_image_cache_divide_thread;
	uint32_t m_http_file_open_flag;
	uint32_t m_https_flag;
	graph::SurfacePool *m_surface_pool;
};

template<class T>
T *CacheManagerImpl::FindCache(const string &path, const typename T::Option *option)
{
	if (path == "")
		return NULL;

	CacheMap::iterator where = m_caches.find(path);

	if (where == m_caches.end())
		return NULL;

	CacheList &caches = where->second;

	for (CacheList::iterator it = caches.begin(); it != caches.end(); it++) {
		CacheBase *cache = it->get();

		if (!cache->DiffOption(option) && !cache->m_removed)
			return static_cast<T *>(cache);
	}
	return NULL;
}

template<class T, class P>
int32_t CacheManagerImpl::CreateCache(const string &path, intrusive_ptr<P> &ptr,
				      const typename T::Option *option, job::JobQueue *q0,
				      job::JobQueue *q1)
{
	int32_t ret;

	ptr.clear();

	T *cache = FindCache<T>(path, option);

	if (cache == NULL) {
		uint32_t retry = 0;

		for (;;) {
			cache = T::CreateCacheInstance(this, option);
			if (cache != NULL) {
				cache->m_path = path;
				break;
			}
			if (option != NULL && !option->retry)
				return SCE_PAF_ERROR_CACHE_NO_MEMORY;
			if (m_caches.begin() == m_caches.end())
				break;

			CacheBase *found = NULL;

			if (CheckCacheAll(&found)) {
				found->Clear();
				RemoveCache(found);
			}
			if (++retry >= 10)
				break;
		}
	}
	if (cache == NULL)
		return SCE_PAF_ERROR_CACHE_PLUGIN_NOT_FOUND;

	uint32_t status = cache->GetJobStatus();

	if (status == 0) {
		ret = AddCache(cache, q0, q1);
		if (ret < 0) {
			ptr.clear();
			return ret;
		}
	} else if (status == 2) {
		if (cache->m_retry_max != 0)
			cache->m_retry_count = cache->m_retry_max;
		ret = cache->RestartJob();
		if (ret < 0) {
			ptr.clear();
			return ret;
		}
	}
	ptr = intrusive_ptr<P>(cache);
	return 0;
}

}
}

#endif
