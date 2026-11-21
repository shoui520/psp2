/*
 * system_update_download_updatelist.h: the jobs that download the update list
 * (system_update_download_updatelist.cpp, Sony's file by 0.990's records), the list they fill,
 * and the other files' classes they use. Every name is ours (PLACEHOLDER) until a record or
 * string names it.
 */
#ifndef _SYSTEM_UPDATE_DOWNLOAD_UPDATELIST_H
#define _SYSTEM_UPDATE_DOWNLOAD_UPDATELIST_H

#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <std/vector>
#include <common/shared_ptr.h>
#include <file/file.h>
#include <thread/thread.h>
#include <bXCe/bxce_internal.h>
#include <system_update/system_update_job.h>
#include <system_update/system_update_updatelistprocessor.h>
#include <system_update/system_update_mtphttpfile.h>
#include <system_update/system_update_updatelistparser.h>
#include <system_update/system_update_version.h>

/*
 * a job that downloads the update list (vtable 0x815304C4, PLACEHOLDER names): over HTTP
 * (type 0), or over the PC's MTP connection (1, 2)
 */
class SystemUpdateDownloadJob : public SystemUpdateJob {
public:
	SystemUpdateDownloadJob(SystemUpdateListener *listener, SystemUpdateList *list, int type);
	virtual ~SystemUpdateDownloadJob();

	virtual void Cancel();
	virtual int Run();
	virtual void Finish(int result);

	virtual void OnFinished(int result);
	virtual SystemUpdateResult Download(bool *reserved);
	virtual SystemUpdateResult Prepare(void);
	virtual SystemUpdateResult Parse(void);
	virtual SystemUpdateResult Fetch(paf::common::SharedPtr<paf::File> file, paf::string url);

	int (*m_func)(int reserved, void *arg);	/* called on the main thread over a phone line */
	void *m_arg;
	SystemUpdateList *m_list;
	int m_type;
	paf::common::SharedPtr<paf::File> m_file;
};

/* a download that gives the first entry's version and url (vtable 0x815304F4, PLACEHOLDER) */
class SystemUpdateListDownloadJob : public SystemUpdateDownloadJob {
public:
	SystemUpdateListDownloadJob(SystemUpdateListener *listener, SystemUpdateList *list, int type)
		: SystemUpdateDownloadJob(listener, list, type)
	{
	}

	virtual ~SystemUpdateListDownloadJob()
	{
	}

	virtual void Finish(int result);

	virtual SystemUpdateResult Bgdl_8112FB08(void);
	virtual SystemUpdateResult CheckType(int type);

	paf::string *m_url;
	paf::string *m_version;
};

/* the service's update list job (vtable 0x8153052C, PLACEHOLDER names) */
class SystemUpdateListJob : public SystemUpdateListDownloadJob {
public:
	SystemUpdateListJob(SystemUpdateListener *listener, paf::string *version, paf::string *url,
			    SceInt64 *size, paf::string *lastModified, SystemUpdateList *list,
			    int type);

	virtual void Finish(int result);
	virtual void OnFinished(int result);
	virtual SystemUpdateResult Parse(void);
	virtual SystemUpdateResult CheckType(int type);

	SystemUpdateResult Bgdl_81130466(const char *url, SceInt64 *size,
					 paf::string *lastModified, SceInt64 knownSize);

	SceInt64 *m_size;
	paf::string *m_lastModified;
};

SystemUpdateResult Bgdl_8112FFEA(SystemUpdateData *data, paf::string *url, SceInt64 *size);

/* other files' */
int Bgdl_8126E2AE(int kind);
int Bgdl_8137C22E(int type, int *kind);
int Bgdl_8137C2A6(int type, int *kind);

#endif
