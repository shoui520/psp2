/*
 * paf::cache::CacheManager: the public face of a CacheManagerImpl. The
 * managers share the default job queues, the tick count caches are stamped
 * with, and one lock (CacheManagerImpl::m_rmutex).
 */
#ifndef PAF_CACHE_CACHE_MANAGER_H
#define PAF_CACHE_CACHE_MANAGER_H

#include <stddef.h>
#include <stdint.h>
#include <std/string>
#include <cache/cache_base.h>

namespace paf {

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

class CacheManager {
public:
	typedef bool (*CancelFunc)(void *userdata);

	/* The default job queues: local files, network files, decoding. */
	enum JobQueueType {
		JobQueueType_Local = 0,
		JobQueueType_Net = 1,
		JobQueueType_Operating = 2
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
		uint32_t reserved_40;
		uint32_t reserved_44;
		uint32_t reserved_48;
		bool cache_longlife_heap;
		bool local_file_nobuf;
		bool image_cache_divide_thread;
		uint32_t http_file_open_flag;
		uint32_t https_flag;
	};

	static bool IsHttpFile(const string &path);
	static job::JobQueue *GetJobQueue(JobQueueType type);

	static int32_t m_ticks;
	static job::JobQueue *m_jobq_local;
	static job::JobQueue *m_jobq_net;
	static job::JobQueue *m_jobq_operating;
};

}
}

#endif
