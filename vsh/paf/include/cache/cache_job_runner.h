/*
 * paf::cache::CacheJobRunner: the job a CacheManagerImpl queues for a cache,
 * named after the cache's path; it runs the cache's JobRun and passes the
 * result to its JobFinish. ScePaf-RE's name: no record or export names it.
 */
#ifndef PAF_CACHE_CACHE_JOB_RUNNER_H
#define PAF_CACHE_CACHE_JOB_RUNNER_H

#include <std/memory>
#include <job/job.h>
#include <cache/cache_base.h>

namespace paf {
namespace cache {

class CacheJobRunner : public job::JobItem {
public:
	CacheJobRunner(const intrusive_ptr<CacheBase> &cache, const job::JobItem::Option *option);
	virtual ~CacheJobRunner();
	virtual int Run();
	virtual void Finish(int result);

	intrusive_ptr<CacheBase> m_cache;
};

}
}

#endif
