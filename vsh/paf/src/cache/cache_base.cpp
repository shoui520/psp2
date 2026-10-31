#include <cache/cache_base.h>
#include <cache/cache_manager.h>
#include <cache/cache_manager_impl.h>
#include <file/localfile.h>
#include <file/httpfile.h>
#include <job/job.h>
#include <memory/heap_allocator.h>
#include <std/new>
#include <std/string.h>

namespace paf {
namespace cache {

CacheBase::CacheBase(CacheManagerImpl *manager, const Option *option, uint8_t cache_type)
	: m_manager(manager), m_cache_type(cache_type), m_load_type(1), m_job_state(0),
	  m_error(0), m_ref_count(0), m_job(NULL), m_path(""), m_data(NULL), m_size(0),
	  m_read_size(0), m_read_offset(0), m_max_size_kb(0), m_open_func(NULL),
	  m_open_userdata(NULL), m_ticks(0), m_auto_delete(true), m_regist_manager(false),
	  m_instance_heap(manager->m_cache_instance_heap != NULL), m_removed(false),
	  m_retry_count(-1), m_retry_max(-1)
{
	sce_paf_memset(m_jobq, 0, sizeof(m_jobq));
	if (option != NULL) {
		m_auto_delete = option->auto_delete;
		m_read_size = option->read_size;
		m_read_offset = option->read_offset;
		m_open_func = option->open_func;
		m_open_userdata = option->open_userdata;
		m_max_size_kb = option->max_size_kb;
		if (option->retry) {
			m_retry_count = option->retry_count;
			m_retry_max = option->retry_count;
		} else {
			m_retry_count = 0;
			m_retry_max = 0;
		}
	}
}

/* From the manager's instance heap when it has one. */
CacheBase *CacheBase::CreateCacheInstance(CacheManagerImpl *manager, const Option *option)
{
	void *buf;

	if (manager->m_cache_instance_heap != NULL)
		buf = manager->m_cache_instance_heap->sce_paf_malloc(sizeof(CacheBase));
	else
		buf = sce_paf_malloc(sizeof(CacheBase));
	return new (buf) CacheBase(manager, option, 0);
}

void CacheBase::DestroyCacheInstance(CacheBase *cache)
{
	if (cache == NULL)
		return;
	if (cache->m_instance_heap) {
		if (cache->m_manager != NULL) {
			memory::HeapAllocator *heap = cache->m_manager->m_cache_instance_heap;

			cache->~CacheBase();
			heap->sce_paf_free(cache);
		}
	} else {
		cache->~CacheBase();
		sce_paf_free(cache);
	}
}

/* Frees the data. */
void CacheBase::Clear()
{
	CacheManagerImpl::m_rmutex.Lock();
	if (m_data != NULL) {
		if (m_manager != NULL)
			m_manager->FreeLongLifeHeap(m_data);
		m_data = NULL;
	}
	m_size = 0;
	CacheManagerImpl::m_rmutex.Unlock();
}

int32_t CacheBase::AddRef()
{
	CacheManagerImpl::m_rmutex.Lock();

	int32_t count = ++m_ref_count;

	CacheManagerImpl::m_rmutex.Unlock();
	return count;
}

/* The last reference destroys the cache. */
int32_t CacheBase::Release()
{
	CacheManagerImpl::m_rmutex.Lock();

	int32_t count = --m_ref_count;

	if (count != 0) {
		CacheManagerImpl::m_rmutex.Unlock();
		return count;
	}
	DestroyInstance();
	CacheManagerImpl::m_rmutex.Unlock();
	return 0;
}

void CacheBase::CancelJob()
{
	CacheManagerImpl::m_rmutex.Lock();
	if (m_job != NULL && m_job->IsCanceled() != true)
		m_job->Cancel();
	CacheManagerImpl::m_rmutex.Unlock();
}

void CacheBase::Touch()
{
	CacheManagerImpl::m_rmutex.Lock();
	m_ticks = CacheManager::m_ticks;
	CacheManagerImpl::m_rmutex.Unlock();
}

/* The data, once the cache is registered and its job (re)started. */
void *CacheBase::GetData()
{
	CacheManagerImpl::m_rmutex.Lock();
	if (m_manager != NULL && m_regist_manager && RestartJob() >= 0) {
		Touch();

		void *data = m_data;

		CacheManagerImpl::m_rmutex.Unlock();
		return data;
	}
	CacheManagerImpl::m_rmutex.Unlock();
	return NULL;
}

void CacheBase::RemoveFromManager()
{
	if (m_removed && m_manager != NULL)
		m_manager->RemoveCacheEntry(this);
}

void CacheBase::Term()
{
	Clear();
}

CacheBase::~CacheBase()
{
	Term();
}

void CacheBase::DestroyInstance()
{
	CacheManagerImpl::m_rmutex.Lock();
	DestroyCacheInstance(this);
	CacheManagerImpl::m_rmutex.Unlock();
}

void CacheBase::Init()
{
	Clear();
}

bool CacheBase::DiffOption(const Option *option)
{
	return false;
}

int32_t CacheBase::StartJob(uint32_t index)
{
	if (m_removed == true)
		return SCE_PAF_ERROR_CACHE_REMOVED;
	if (m_manager == NULL)
		return SCE_PAF_ERROR_CACHE_NO_MANAGER;
	if (m_jobq[index] == NULL || index >= 2)
		return SCE_PAF_ERROR_CACHE_NO_JOB_QUEUE;
	if (m_manager->m_is_shutdown)
		return SCE_PAF_ERROR_CACHE_SHUTDOWN;

	int32_t ret = m_manager->EnqueueCache(this, index);

	if (ret >= 0)
		return 0;
	return ret;
}

/* Loads again what a failed load freed, while retries are left. */
int32_t CacheBase::RestartJob()
{
	uint32_t status = GetJobStatus();

	if (m_data != NULL || status != 2 || m_retry_max == 0 || m_retry_count == 0)
		return 0;
	return StartJob(0);
}

void CacheBase::CloseFile()
{
	if (m_file.get() != NULL) {
		m_file->Close();
		CacheManagerImpl::m_rmutex.Lock();
		m_file.reset();
		CacheManagerImpl::m_rmutex.Unlock();
	}
}

int32_t CacheBase::OpenFile()
{
	int32_t error = 0;

	CloseFile();
	if (m_open_func != NULL) {
		m_file = m_open_func(m_path.c_str(), m_open_userdata, &error);
		if (m_file.get() != NULL && !m_file->IsOpened()) {
			if (CacheManager::IsHttpFile(m_path)) {
				HttpFile::OpenArg arg;

				arg.ParseUrl(m_path.c_str());
				arg.open_flag = m_manager->m_http_file_open_flag;
				error = m_file->Open(&arg);
			} else {
				LocalFile::OpenArg arg;

				arg.file_type = File::Type_Local;
				arg.filename = m_path.c_str();
				arg.flag = m_manager->m_local_file_nobuf ? 0x10001 : 1;
				error = m_file->Open(&arg);
			}
			if (error != 0)
				m_file.reset();
		}
	} else if (CacheManager::IsHttpFile(m_path)) {
		m_file = common::SharedPtr<HttpFile>(new HttpFile);
		if (m_file.get() != NULL) {
			HttpFile::OpenArg arg;

			arg.ParseUrl(m_path.c_str());
			arg.open_flag = m_manager->m_http_file_open_flag;
			error = m_file->Open(&arg);
			if (error != 0)
				m_file.reset();
		} else {
			error = SCE_PAF_ERROR_CACHE_NO_MEMORY;
		}
	} else {
		m_file = LocalFile::Open(m_path.c_str(), m_manager->m_local_file_nobuf ? 0x10001 : 1,
					 0, &error);
	}
	if (m_file.get() != NULL)
		return 0;
	Clear();
	return error;
}

/*
 * Reads read_size bytes from read_offset, or the whole file when that range is
 * not inside it.
 */
int32_t CacheBase::LoadFile()
{
	if (m_size > 0)
		return 0;

	size_t seek = 0;
	int32_t ret = OpenFile();

	if (ret < 0)
		goto error;

	{
		int64_t size = m_file->GetFileSize();

		ret = (int32_t)size;
		if (ret < 0) {
			CloseFile();
			return ret;
		}
		m_size = size;
		if (m_max_size_kb != 0 && m_max_size_kb < (uint16_t)(size / 1024)) {
			CloseFile();
			return SCE_PAF_ERROR_CACHE_TOO_LARGE;
		}

		size_t offset = m_read_offset;
		size_t length;

		if ((size_t)ret < offset)
			seek = ret;
		if ((size_t)ret >= offset + m_read_size || (length = ret - offset) == 0)
			length = ret;

		bool retry = m_retry_max != 0 && m_retry_count != 0;

		m_data = m_manager->AllocLongLifeHeap(ret, retry, this);
		if (m_data == NULL) {
			m_size = 0;
			CloseFile();
			return SCE_PAF_ERROR_CACHE_NO_MEMORY;
		}
		if (seek != 0) {
			ret = (int32_t)m_file->Seek(seek, 0);
			if (ret < 0)
				goto error;
		}
		ret = m_file->Read(m_data, length);
		if (ret >= 0) {
			CloseFile();
			return 0;
		}
	}
error:
	CloseFile();
	m_manager->FreeLongLifeHeap(m_data);
	m_data = NULL;
	return ret;
}

/* The part of path after its last '.', or "". */
string CacheBase::GetExtension(const string &path)
{
	int32_t count = path.size();
	int32_t dot = -1;

	for (int32_t pos = count; pos > 0; --pos) {
		char ch = path.c_str()[pos - 1];

		if (ch == '/' || ch == '\\')
			continue;
		if (ch == '.') {
			dot = pos;
			break;
		}
	}
	if (dot == -1)
		return string("");
	return path.substr(dot, count - dot);
}

bool CacheBase::IsCancel(void *userdata)
{
	CacheBase *cache = (CacheBase *)userdata;

	if (cache == NULL)
		return false;

	CacheManagerImpl *manager = cache->m_manager;
	bool cancel = false;

	if (manager == NULL ||
	    ((manager->m_cancel_func == NULL ||
	      (cancel = manager->m_cancel_func(manager->m_cancel_userdata)) == false) &&
	     (cancel = manager->m_is_shutdown) == false)) {
		if (cache->m_job != NULL)
			return cache->m_job->IsCanceled();
	}
	return cancel;
}

/* Only a cache that may be deleted, has data and is held by its manager alone. */
bool CacheBase::CheckLongLifeHeap(CacheBase *cache)
{
	if (!cache->m_auto_delete)
		return false;
	if (cache->m_size != 0)
		return cache->m_ref_count == 1;
	return false;
}

int32_t CacheBase::GetTicks()
{
	return m_ticks;
}

void CacheBase::SetTicks(int32_t ticks)
{
	CacheManagerImpl::m_rmutex.Lock();
	m_ticks = ticks;
	CacheManagerImpl::m_rmutex.Unlock();
}

int32_t CacheBase::JobRun()
{
	if (m_manager == NULL)
		return SCE_PAF_ERROR_CACHE_NO_MANAGER;
	if (m_manager->m_is_shutdown)
		return SCE_PAF_ERROR_CACHE_SHUTDOWN;
	return LoadFile();
}

/*
 * A failed load frees the data and counts a retry; a missing file or device
 * is not retried.
 */
int32_t CacheBase::JobFinish(int32_t result, bool &is_finished)
{
	int32_t ret;

	CacheManagerImpl::m_rmutex.Lock();
	if (m_manager != NULL) {
		m_error = result;
		if (result < 0) {
			if (result == SCE_PAF_ERROR_FILE_NO_ENTRY ||
			    result == SCE_PAF_ERROR_FILE_NO_DEVICE) {
				m_retry_count = 0;
			} else {
				int32_t count = m_retry_count;

				if (count > 0)
					m_retry_count = --count;
				if (m_retry_max == 0 || count == 0)
					m_error = SCE_PAF_ERROR_CACHE_LOAD_FAILED;
			}
			Clear();
		}
		is_finished = true;
		Touch();
		ret = 0;
	} else {
		ret = SCE_PAF_ERROR_CACHE_NO_MANAGER;
	}
	m_job = NULL;
	CacheManagerImpl::m_rmutex.Unlock();
	return ret;
}

bool CacheBase::IsEnableReleaseInstance()
{
	return m_ref_count <= 1;
}

}
}
