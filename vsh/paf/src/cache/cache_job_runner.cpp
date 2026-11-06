/* paf::cache::CacheJobRunner: a cache's job on its manager's queue. */
#include <cache/cache_job_runner.h>
#include <cache/cache_manager.h>
#include <cache/cache_manager_impl.h>
#include <file/file.h>
#include <thread/thread.h>
#include <src/paf_types.h>

namespace paf {
namespace cache {

/* deletes the manager whose last job is finished, on the main thread (name ours) */
static void DestroyCacheManager(void *manager)
{
	delete (CacheManager *)manager;
}

CacheJobRunner::CacheJobRunner(const intrusive_ptr<CacheBase> &cache,
			       const job::JobItem::Option *option)
	: job::JobItem(cache->m_path.c_str(), option), m_cache(cache)
{
	CacheManagerImpl::m_rmutex.Lock();
	if (m_cache.get() != NULL) {
		m_cache->m_job = this;
		m_cache->m_job_state = 1;
	}
	CacheManagerImpl::m_rmutex.Unlock();
}

CacheJobRunner::~CacheJobRunner()
{
}

int CacheJobRunner::Run()
{
	int32_t ret;

	if (m_cache.get() == NULL)
		return SCE_PAF_ERROR_CACHE_NO_JOB_QUEUE;

	if (IsCanceled())
		return SCE_PAF_ERROR_CACHE_CANCELED;

	ret = m_cache->JobRun();
	if (ret >= 0)
		return SCE_OK;

	return ret;
}

void CacheJobRunner::Cancel()
{
	job::JobItem::Cancel();
	if (m_cache.get() != NULL && m_cache->m_file.get() != NULL)
		m_cache->m_file->Abort();
}

void CacheJobRunner::Finish(int result)
{
	bool finished;

	if (IsCanceled())
		result = SCE_PAF_ERROR_CACHE_CANCELED;

	if (m_cache.get() == NULL)
		return;

	if (m_cache->m_manager != NULL && m_cache->m_manager->m_destroy_requested) {
		/* the manager is being destroyed: it goes once its last job is finished */
		CacheManagerImpl::m_rmutex.Lock();
		m_cache->m_job = NULL;
		m_cache->m_manager->AddJobCount(-1);
		m_cache->m_job = NULL;
		m_cache->m_job_state = 2;
		m_cache->RemoveFromManager();
		if (m_cache->m_manager->GetJobCount() == 0) {
			string name(m_cache->m_manager->m_name);
			CacheManager *manager = CacheManager::GetCacheManager(name);

			if (manager != NULL)
				thread::CallQueue::main_thread_callqueue.Enqueue(name.c_str(),
										 DestroyCacheManager,
										 manager);
		}
		CacheManagerImpl::m_rmutex.Unlock();
		return;
	}

	CacheManagerImpl::m_rmutex.Lock();
	finished = false;
	m_cache->JobFinish(result, finished);
	m_cache->m_manager->AddJobCount(-1);
	if (finished == true) {
		CacheManager::FinishFunc func;

		m_cache->m_job_state = 2;
		m_cache->m_job = NULL;
		func = m_cache->m_manager->m_finish_func;
		if (func != NULL) {
			string path(m_cache->m_path);
			void *userdata = m_cache->m_manager->m_finish_userdata;

			m_cache->RemoveFromManager();
			m_cache.clear();
			func(result, path, userdata);
		} else {
			m_cache->RemoveFromManager();
			m_cache.clear();
		}
	} else {
		m_cache->RemoveFromManager();
		if (m_cache->StartJob(1) < 0)
			m_cache.clear();
	}
	CacheManagerImpl::m_rmutex.Unlock();
}

}
}
