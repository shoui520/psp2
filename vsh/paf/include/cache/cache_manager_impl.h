/*
 * paf::cache::CacheManagerImpl: a named set of caches, keyed by path, with
 * the heaps their data and instances come from.
 */
#ifndef PAF_CACHE_CACHE_MANAGER_IMPL_H
#define PAF_CACHE_CACHE_MANAGER_IMPL_H

#include <stddef.h>
#include <stdint.h>
#include <std/string>
#include <std/list>
#include <std/map>
#include <thread/thread.h>
#include <cache/cache_base.h>
#include <cache/cache_manager.h>

namespace paf {
namespace cache {

class CacheManagerImpl {
public:
	CacheManagerImpl(const CacheManager::Params *param);
	virtual ~CacheManagerImpl();

	int32_t EnqueueCache(CacheBase *cache, uint32_t index);
	int32_t RemoveCache(CacheBase *cache);
	int32_t RemoveCacheEntry(CacheBase *cache);
	bool CheckCacheAll(bool (*check)(CacheBase *cache), CacheBase **found);
	void *AllocLongLifeHeap(size_t size, bool retry, CacheBase *cache);
	void FreeLongLifeHeap(void *ptr);
	void *AllocInstantHeap(size_t size);
	void FreeInstantHeap(void *ptr);

	static thread::RMutex m_rmutex;

	memory::HeapAllocator *m_longlife_heap;
	memory::HeapAllocator *m_instant_heap;
	memory::HeapAllocator *m_cache_instance_heap;
	size_t m_longlife_align_size;
	size_t m_instant_align_size;
	string m_name;
	map<string, list<CacheBase *> > m_caches;
	CacheManager::CancelFunc m_cancel_func;
	void *m_cancel_userdata;
	uint32_t m_reserved_40;
	uint32_t m_reserved_44;
	bool m_busy;
	bool m_is_shutdown;
	bool m_cache_longlife_heap;
	bool m_local_file_nobuf;
	int32_t m_queued_job_count;
	bool m_image_cache_divide_thread;
	uint32_t m_http_file_open_flag;
	uint32_t m_https_flag;
	graph::SurfacePool *m_surface_pool;
};

}
}

#endif
