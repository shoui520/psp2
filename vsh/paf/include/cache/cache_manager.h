/*
 * paf::cache::CacheManager: the public face of a CacheManagerImpl. The
 * managers share the default job queues, the tick count caches are stamped
 * with, and one lock (CacheManagerImpl::m_rmutex); every manager is in
 * m_cache_list, found there by its name.
 */
#ifndef PAF_CACHE_CACHE_MANAGER_H
#define PAF_CACHE_CACHE_MANAGER_H

#include <stddef.h>
#include <stdint.h>
#include <std/string>
#include <std/list>
#include <std/memory>
#include <cache/cache_base.h>
#include <cache/cache_image.h>

namespace paf {

class Plugin;
class IDParam;

namespace memory {
class HeapAllocator;
}

namespace graph {
class SurfacePool;
}

namespace job {
class JobQueue;
}

namespace cache {

class CacheManagerImpl;

class CacheManager {
public:
	typedef bool (*CancelFunc)(void *userdata);

	/* The default job queues: local files, network files, decoding. */
	enum JobQueueType {
		JobQueueType_Local = 0,
		JobQueueType_Net = 1,
		JobQueueType_Operating = 2
	};

	/*
	 * An object the manager takes from its Params and terminates and deletes
	 * with itself (a name of ours: nothing names the class or its method).
	 */
	class Finalizer {
	public:
		virtual ~Finalizer();
		virtual void Finalize() = 0;
	};

	class Params {
	public:
		string name;
		memory::HeapAllocator *longlife_heap;
		memory::HeapAllocator *instant_heap;
		memory::HeapAllocator *cache_instance_heap;
		uint8_t longlife_heap_align_size;
		uint8_t instant_heap_align_size;
		graph::SurfacePool *surface_pool;
		CancelFunc cancel_func;
		void *cancel_userdata;
		uint32_t reserved0;
		uint32_t reserved1;
		Finalizer *finalizer;
		bool cache_longlife_heap;
		bool local_file_nobuf;
		bool image_cache_divide_thread;
		uint32_t http_file_open_flag;
		uint32_t https_flag;
		int32_t local_job_prio;
		int32_t net_job_prio;
		int32_t operating_job_prio;
	};

	CacheManager(const Params *param);
	virtual ~CacheManager();

	int32_t CreateDefaultJobQueue(const Params *param);
	int32_t DestroyDefaultJobQueue();
	static void CancelDefaultJobQueue();

	int32_t GetJobCount();
	void RequestShutdown();
	void Destroy();
	bool IsRequestedShutdown();

	const string &GetName() const;
	static CacheManager *GetCacheManager(const string &name);
	static size_t GetCacheManagerCount();
	static int32_t GetQueuedJobCount();
	int32_t GetCacheCount();
	static void Touch();

	static bool IsHttpFile(const string &path);
	job::JobQueue *GetJobQueue(const string &path, job::JobQueue *q);
	static job::JobQueue *GetJobQueue(JobQueueType type);

	int32_t CreateCache(const string &path, intrusive_ptr<CacheBase> &ptr,
			    const CacheBase::Option *option, job::JobQueue *q);
	int32_t CreateCache(const string &path, intrusive_ptr<CacheImageInterface> &ptr,
			    const CacheImage::Option *option, job::JobQueue *q0, job::JobQueue *q1);
	int32_t CreateCache(Plugin *plugin, IDParam &id, intrusive_ptr<CacheImageInterface> &ptr,
			    const CacheImage::Option *option, job::JobQueue *q);
	int32_t FindCache(const string &path, intrusive_ptr<CacheBase> &ptr);
	int32_t FindCache(const string &path, intrusive_ptr<CacheImageInterface> &ptr,
			  const CacheImage::Option *option);
	int32_t FindCache(Plugin *plugin, IDParam &id, intrusive_ptr<CacheImageInterface> &ptr,
			  const CacheImage::Option *option);
	int32_t CancelCache(const string &path);
	int32_t ClearCache(intrusive_ptr<CacheBase> &ptr);
	int32_t ClearCache(intrusive_ptr<CacheImageInterface> &ptr);
	int32_t ClearCache(const string &path);

	int32_t MakeSpaceSurfacePool(uint32_t width, uint32_t height);
	int32_t MakeSpaceSurfacePool();

	static list<CacheManager *> m_cache_list;
	static int32_t m_ticks;
	static job::JobQueue *m_jobq_local;
	static job::JobQueue *m_jobq_net;
	static job::JobQueue *m_jobq_operating;

	CacheManagerImpl *m_impl;
	Finalizer *m_finalizer;
};

}
}

#endif
