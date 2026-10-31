#include <cache/cache_manager.h>
#include <cache/cache_manager_impl.h>
#include <cache/cache_image.h>
#include <job/job.h>
#include <std/new>
#include <std/string.h>

namespace paf {
namespace cache {

list<CacheManager *> CacheManager::m_cache_list;
int32_t CacheManager::m_ticks;
job::JobQueue *CacheManager::m_jobq_local;
job::JobQueue *CacheManager::m_jobq_net;
job::JobQueue *CacheManager::m_jobq_operating;

int32_t CacheManager::CreateDefaultJobQueue(const Params *param)
{
	if (m_jobq_local == NULL) {
		job::JobQueue::Option option;

		option.priority = param->local_job_prio;
		option.stackSize = 0x2000;
		option.numThreads = 1;
		m_jobq_local = new job::JobQueue("ScePafCacheLocalJob", &option);
		if (m_jobq_local == NULL)
			return SCE_PAF_ERROR_CACHE_NO_MEMORY;
	}
	if (m_jobq_net == NULL) {
		job::JobQueue::Option option;

		option.priority = param->net_job_prio;
		option.stackSize = 0x2000;
		option.numThreads = 1;
		m_jobq_net = new job::JobQueue("ScePafCacheNetJob", &option);
		if (m_jobq_net == NULL)
			return SCE_PAF_ERROR_CACHE_NO_MEMORY;
	}
	if (m_jobq_operating == NULL) {
		job::JobQueue::Option option;

		option.priority = param->operating_job_prio;
		option.stackSize = 0x2000;
		option.numThreads = 1;
		m_jobq_operating = new job::JobQueue("ScePafCacheOperatingJob", &option);
		if (m_jobq_operating == NULL)
			return SCE_PAF_ERROR_CACHE_NO_MEMORY;
	}
	return 0;
}

CacheManager::CacheManager(const Params *param) : m_impl(NULL)
{
	m_impl = new CacheManagerImpl(param);
	if (m_impl == NULL)
		return;
	m_finalizer = param->finalizer;
	CacheManagerImpl::m_rmutex.Lock();
	if (m_jobq_local == NULL || m_jobq_net == NULL || m_jobq_operating == NULL)
		CreateDefaultJobQueue(param);
	m_cache_list.push_back(this);
	CacheManagerImpl::m_rmutex.Unlock();
}

/* Waits for each queue to run dry before deleting it. */
int32_t CacheManager::DestroyDefaultJobQueue()
{
	if (m_jobq_local != NULL) {
		m_jobq_local->CancelAllItems();
		while (m_jobq_local->NumItems() != 0)
			thread::Sleep(2000);
		CacheManagerImpl::m_rmutex.Lock();
		delete m_jobq_local;
		m_jobq_local = NULL;
		CacheManagerImpl::m_rmutex.Unlock();
	}
	if (m_jobq_net != NULL) {
		m_jobq_net->CancelAllItems();
		while (m_jobq_net->NumItems() != 0)
			thread::Sleep(2000);
		CacheManagerImpl::m_rmutex.Lock();
		delete m_jobq_net;
		m_jobq_net = NULL;
		CacheManagerImpl::m_rmutex.Unlock();
	}
	if (m_jobq_operating != NULL) {
		m_jobq_operating->CancelAllItems();
		while (m_jobq_operating->NumItems() != 0)
			thread::Sleep(2000);
		CacheManagerImpl::m_rmutex.Lock();
		delete m_jobq_operating;
		m_jobq_operating = NULL;
		CacheManagerImpl::m_rmutex.Unlock();
	}
	return 0;
}

CacheManager::~CacheManager()
{
	CacheManagerImpl::m_rmutex.Lock();
	m_impl->SetShutdownFlag(true);
	CacheManagerImpl::m_rmutex.Unlock();
	if (m_cache_list.size() == 1)
		DestroyDefaultJobQueue();
	delete m_impl;
	CacheManagerImpl::m_rmutex.Lock();
	m_impl = NULL;
	m_cache_list.remove(this);
	CacheManagerImpl::m_rmutex.Unlock();
	if (m_finalizer != NULL) {
		m_finalizer->Finalize();
		delete m_finalizer;
	}
}

int32_t CacheManager::GetJobCount()
{
	if (m_impl == NULL)
		return 0;
	return m_impl->GetJobCount();
}

void CacheManager::CancelDefaultJobQueue()
{
	if (m_jobq_local != NULL)
		m_jobq_local->CancelAllItems();
	if (m_jobq_net != NULL)
		m_jobq_net->CancelAllItems();
	if (m_jobq_operating != NULL)
		m_jobq_operating->CancelAllItems();
}

void CacheManager::RequestShutdown()
{
	CacheManagerImpl::m_rmutex.Lock();
	if (m_impl != NULL && m_impl->m_is_shutdown != true) {
		m_impl->SetShutdownFlag(true);
		CancelDefaultJobQueue();
	}
	CacheManagerImpl::m_rmutex.Unlock();
}

/* Shuts down; the manager is deleted now, or by the last job to finish. */
void CacheManager::Destroy()
{
	m_impl->m_destroy_requested = true;
	RequestShutdown();
	if (m_impl == NULL || m_impl->GetJobCount() == 0)
		delete this;
}

const string &CacheManager::GetName() const
{
	if (m_impl != NULL)
		return m_impl->m_name;

	static string s_empty_name("");

	return s_empty_name;
}

CacheManager *CacheManager::GetCacheManager(const string &name)
{
	CacheManagerImpl::m_rmutex.Lock();
	for (list<CacheManager *>::iterator it = m_cache_list.begin(); it != m_cache_list.end();
	     ++it) {
		if (*it != NULL && (*it)->GetName() == name) {
			CacheManager *manager = *it;

			CacheManagerImpl::m_rmutex.Unlock();
			return manager;
		}
	}
	CacheManagerImpl::m_rmutex.Unlock();
	return NULL;
}

size_t CacheManager::GetCacheManagerCount()
{
	CacheManagerImpl::m_rmutex.Lock();

	size_t count = m_cache_list.size();

	CacheManagerImpl::m_rmutex.Unlock();
	return count;
}

int32_t CacheManager::GetQueuedJobCount()
{
	int32_t count = 0;

	if (m_jobq_local != NULL)
		count = m_jobq_local->NumItems();
	if (m_jobq_net != NULL)
		count += m_jobq_net->NumItems();
	if (m_jobq_operating != NULL)
		count += m_jobq_operating->NumItems();
	return count;
}

int32_t CacheManager::GetCacheCount()
{
	if (m_impl == NULL)
		return SCE_PAF_ERROR_CACHE_NO_JOB_QUEUE;
	if (m_impl->m_is_shutdown)
		return 0;
	return m_impl->GetCacheCount();
}

void CacheManager::Touch()
{
	++m_ticks;
}

bool CacheManager::IsHttpFile(const string &path)
{
	if (path.substr(0, 4) == "http")
		return true;
	return false;
}

job::JobQueue *CacheManager::GetJobQueue(const string &path, job::JobQueue *q)
{
	if (q == NULL)
		q = IsHttpFile(path) ? m_jobq_net : m_jobq_local;
	return q;
}

int32_t CacheManager::CreateCache(const string &path, intrusive_ptr<CacheBase> &ptr,
				  const CacheBase::Option *option, job::JobQueue *q)
{
	CacheManagerImpl::m_rmutex.Lock();
	if (m_impl == NULL) {
		CacheManagerImpl::m_rmutex.Unlock();
		return SCE_PAF_ERROR_CACHE_NO_JOB_QUEUE;
	}
	if (m_impl->m_is_shutdown) {
		CacheManagerImpl::m_rmutex.Unlock();
		return SCE_PAF_ERROR_CACHE_SHUTDOWN;
	}
	q = GetJobQueue(path, q);

	int32_t ret = m_impl->CreateCache<CacheBase>(path, ptr, option, q, NULL);

	CacheManagerImpl::m_rmutex.Unlock();
	return ret;
}

int32_t CacheManager::CreateCache(const string &path, intrusive_ptr<CacheImageInterface> &ptr,
				  const CacheImage::Option *option, job::JobQueue *q0,
				  job::JobQueue *q1)
{
	CacheManagerImpl::m_rmutex.Lock();
	if (m_impl == NULL) {
		CacheManagerImpl::m_rmutex.Unlock();
		return SCE_PAF_ERROR_CACHE_NO_JOB_QUEUE;
	}
	if (m_impl->m_is_shutdown) {
		CacheManagerImpl::m_rmutex.Unlock();
		return SCE_PAF_ERROR_CACHE_SHUTDOWN;
	}
	q0 = GetJobQueue(path, q0);

	int32_t ret = m_impl->CreateCache<CacheImage>(path, ptr, option, q0, q1);

	CacheManagerImpl::m_rmutex.Unlock();
	return ret;
}

int32_t CacheManager::CreateCache(Plugin *plugin, IDParam &id,
				  intrusive_ptr<CacheImageInterface> &ptr,
				  const CacheImage::Option *option, job::JobQueue *q)
{
	CacheManagerImpl::m_rmutex.Lock();
	if (m_impl == NULL) {
		CacheManagerImpl::m_rmutex.Unlock();
		return SCE_PAF_ERROR_CACHE_NO_JOB_QUEUE;
	}
	if (m_impl->m_is_shutdown) {
		CacheManagerImpl::m_rmutex.Unlock();
		return SCE_PAF_ERROR_CACHE_SHUTDOWN;
	}

	int32_t ret;

	{
		string path = CacheResourceImage::MakePath(plugin, id);

		q = GetJobQueue(path, q);
		ret = m_impl->CreateCache<CacheResourceImage>(path, ptr, option, q, NULL);
	}
	CacheManagerImpl::m_rmutex.Unlock();
	return ret;
}

int32_t CacheManager::FindCache(const string &path, intrusive_ptr<CacheBase> &ptr)
{
	CacheManagerImpl::m_rmutex.Lock();
	if (m_impl == NULL) {
		CacheManagerImpl::m_rmutex.Unlock();
		return SCE_PAF_ERROR_CACHE_NO_JOB_QUEUE;
	}
	if (m_impl->m_is_shutdown) {
		CacheManagerImpl::m_rmutex.Unlock();
		return SCE_PAF_ERROR_CACHE_SHUTDOWN;
	}
	ptr = intrusive_ptr<CacheBase>(m_impl->FindCache<CacheBase>(path, NULL));
	CacheManagerImpl::m_rmutex.Unlock();
	return 0;
}

int32_t CacheManager::FindCache(const string &path, intrusive_ptr<CacheImageInterface> &ptr,
				const CacheImage::Option *option)
{
	CacheManagerImpl::m_rmutex.Lock();
	if (m_impl == NULL) {
		CacheManagerImpl::m_rmutex.Unlock();
		return SCE_PAF_ERROR_CACHE_NO_JOB_QUEUE;
	}
	if (m_impl->m_is_shutdown) {
		CacheManagerImpl::m_rmutex.Unlock();
		return SCE_PAF_ERROR_CACHE_SHUTDOWN;
	}
	ptr = intrusive_ptr<CacheImageInterface>(m_impl->FindCache<CacheImage>(path, option));
	CacheManagerImpl::m_rmutex.Unlock();
	return 0;
}

int32_t CacheManager::FindCache(Plugin *plugin, IDParam &id,
				intrusive_ptr<CacheImageInterface> &ptr,
				const CacheImage::Option *option)
{
	string path = CacheResourceImage::MakePath(plugin, id);

	return FindCache(path, ptr, option);
}

int32_t CacheManager::CancelCache(const string &path)
{
	CacheManagerImpl::m_rmutex.Lock();
	if (m_impl == NULL) {
		CacheManagerImpl::m_rmutex.Unlock();
		return SCE_PAF_ERROR_CACHE_NO_JOB_QUEUE;
	}
	if (m_impl->m_is_shutdown) {
		CacheManagerImpl::m_rmutex.Unlock();
		return SCE_PAF_ERROR_CACHE_SHUTDOWN;
	}

	int32_t ret = m_impl->CancelCache(path);

	CacheManagerImpl::m_rmutex.Unlock();
	return ret;
}

int32_t CacheManager::ClearCache(intrusive_ptr<CacheBase> &ptr)
{
	CacheManagerImpl::m_rmutex.Lock();
	if (m_impl == NULL) {
		CacheManagerImpl::m_rmutex.Unlock();
		return SCE_PAF_ERROR_CACHE_NO_JOB_QUEUE;
	}
	if (m_impl->m_is_shutdown) {
		CacheManagerImpl::m_rmutex.Unlock();
		return SCE_PAF_ERROR_CACHE_SHUTDOWN;
	}
	m_impl->ClearCache(ptr.get());
	CacheManagerImpl::m_rmutex.Unlock();
	return 0;
}

int32_t CacheManager::ClearCache(intrusive_ptr<CacheImageInterface> &ptr)
{
	CacheManagerImpl::m_rmutex.Lock();
	if (m_impl == NULL) {
		CacheManagerImpl::m_rmutex.Unlock();
		return SCE_PAF_ERROR_CACHE_NO_JOB_QUEUE;
	}
	if (m_impl->m_is_shutdown) {
		CacheManagerImpl::m_rmutex.Unlock();
		return SCE_PAF_ERROR_CACHE_SHUTDOWN;
	}
	m_impl->ClearCache(static_cast<CacheImage *>(ptr.get()));
	CacheManagerImpl::m_rmutex.Unlock();
	return 0;
}

int32_t CacheManager::ClearCache(const string &path)
{
	CacheManagerImpl::m_rmutex.Lock();
	if (m_impl == NULL) {
		CacheManagerImpl::m_rmutex.Unlock();
		return SCE_PAF_ERROR_CACHE_NO_JOB_QUEUE;
	}
	if (m_impl->m_is_shutdown) {
		CacheManagerImpl::m_rmutex.Unlock();
		return SCE_PAF_ERROR_CACHE_SHUTDOWN;
	}
	m_impl->ClearCache(path);
	CacheManagerImpl::m_rmutex.Unlock();
	return 0;
}

bool CacheManager::IsRequestedShutdown()
{
	CacheManagerImpl::m_rmutex.Lock();
	if (m_impl == NULL) {
		CacheManagerImpl::m_rmutex.Unlock();
		return true;
	}

	bool shutdown = m_impl->m_is_shutdown;

	CacheManagerImpl::m_rmutex.Unlock();
	return shutdown;
}

int32_t CacheManager::MakeSpaceSurfacePool(uint32_t width, uint32_t height)
{
	if (m_impl == NULL)
		return SCE_PAF_ERROR_CACHE_NO_JOB_QUEUE;
	if (m_impl->m_is_shutdown)
		return SCE_PAF_ERROR_CACHE_SHUTDOWN;
	return m_impl->MakeSpaceSurfacePool(width, height);
}

int32_t CacheManager::MakeSpaceSurfacePool()
{
	if (m_impl == NULL)
		return SCE_PAF_ERROR_CACHE_NO_JOB_QUEUE;
	if (m_impl->m_is_shutdown)
		return SCE_PAF_ERROR_CACHE_SHUTDOWN;
	return m_impl->MakeSpaceSurfacePool();
}

job::JobQueue *CacheManager::GetJobQueue(JobQueueType type)
{
	switch (type) {
	case JobQueueType_Local:
		return m_jobq_local;
	case JobQueueType_Net:
		return m_jobq_net;
	case JobQueueType_Operating:
		return m_jobq_operating;
	}
	return NULL;
}

}
}
