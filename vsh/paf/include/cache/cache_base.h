/*
 * paf::cache::CacheBase: one cached file, loaded by a job of its manager's
 * queues into the manager's long-life heap. The cache keeps its own reference
 * count under the managers' lock; a file is opened by the option's open
 * function when there is one, as HttpFile for a URL, else as LocalFile.
 */
#ifndef PAF_CACHE_CACHE_BASE_H
#define PAF_CACHE_CACHE_BASE_H

#include <stddef.h>
#include <stdint.h>
#include <std/string>
#include <common/shared_ptr.h>
#include <file/file.h>
#include <src/paf_types.h>

namespace paf {

namespace job {
class JobQueue;
class JobItem;
}

namespace cache {

class CacheManagerImpl;

class CacheBase {
public:
	/* Opens the file of path for the cache; error gets the result. */
	typedef common::SharedPtr<File> (*OpenFunc)(const char *path, void *userdata,
						    int32_t *error);

	class Option {
	public:
		bool auto_delete;
		bool retry;
		int32_t retry_count;
		uint32_t reserved;
		size_t read_size;
		size_t read_offset;
		uint16_t max_size_kb;
		OpenFunc open_func;
		void *open_userdata;
	};

	static CacheBase *CreateCacheInstance(CacheManagerImpl *manager, const Option *option);
	static void DestroyCacheInstance(CacheBase *cache);
	static bool CheckLongLifeHeap(CacheBase *cache);
	static bool IsCancel(void *cache);

	CacheBase(CacheManagerImpl *manager, const Option *option, uint8_t cache_type);

	virtual uint32_t GetJobStatus()
	{
		return m_job_state;
	}

	virtual ~CacheBase();
	virtual void DestroyInstance();
	virtual bool DiffOption(const Option *option);
	virtual int32_t RestartJob();
	virtual int32_t JobRun();
	virtual int32_t JobFinish(int32_t result, bool &is_finished);
	virtual bool IsEnableReleaseInstance();

	void Clear();
	int32_t AddRef();
	int32_t Release();
	void CancelJob();
	void Touch();
	void *GetData();
	void RemoveFromManager();
	void Term();
	void Init();
	int32_t StartJob(uint32_t index);
	int32_t LoadFile();
	string GetExtension(const string &path);
	int32_t GetTicks();
	void SetTicks(int32_t ticks);

	CacheManagerImpl *m_manager;
	uint8_t m_cache_type;
	uint8_t m_load_type;
	uint8_t m_job_state;
	int32_t m_error;
	int32_t m_ref_count;
	job::JobQueue *m_jobq[2];
	job::JobItem *m_job;
	string m_path;
	void *m_data;
	int64_t m_size;
	size_t m_read_size;
	size_t m_read_offset;
	uint16_t m_max_size_kb;
	OpenFunc m_open_func;
	void *m_open_userdata;
	int32_t m_ticks;
	bool m_auto_delete;
	bool m_regist_manager;
	bool m_instance_heap;
	bool m_removed;
	int32_t m_retry_count;
	int32_t m_retry_max;
	common::SharedPtr<File> m_file;

protected:
	void CloseFile();
	int32_t OpenFile();
};

}
}

#endif
