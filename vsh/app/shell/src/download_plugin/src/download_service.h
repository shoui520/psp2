/*
 * download_service.h: the download service's calls (download_service_api.cpp, 0x81138F80 to
 * 0x8113A11A), what the shell's "SceDownloadService" IPMI server runs for each method and what
 * the shell's own plugins call directly. Each checks its arguments and passes the call to the
 * bgdl library (esecron) or to download_plugin. The file's name and every name in it are ours
 * (PLACEHOLDER): no record or message names them.
 */
#ifndef _DOWNLOAD_SERVICE_H
#define _DOWNLOAD_SERVICE_H

#include <stddef.h>
#include <scetypes.h>
#include <std/list>
#include <thread/thread.h>
#include <job/job.h>
#include <ipmi/server.h>
#include <ipmi/session.h>
#include <file/localfile.h>
#include <dir/dir.h>

#include "../../esecron/include/esetask.hpp"
#include "system_update_svc.h"
#include "download_plugin.h"

/* the service's own errors (PLACEHOLDER names) */
#define DOWNLOAD_SVC_ERROR_INVALID_ARGUMENT	0x80103B01
#define DOWNLOAD_SVC_ERROR_NO_PARAM		0x80103B03	/* the task's param file not opened */
#define DOWNLOAD_SVC_ERROR_PATH_LONG		0x80103B0D	/* the path does not fit */

/* a package task's progress, from its param file (PLACEHOLDER names) */
struct BgdlPackageStatus {
	int state;			/* 1 to 4, from keys 101 and 104 */
	SceUInt32 key500;
	SceUInt32 key501;
	SceUInt16 key503;
	SceUInt16 key504;
	char titleId[14];		/* key 220 */
	char name[256];			/* key 105 */
};

/* the title whose storage is asked for (PLACEHOLDER names) */
struct BgdlStorageQuery {
	int reserved;
	char titleId[1];		/* PLACEHOLDER: its length is not known yet */
};

/* a title's patch to register (PLACEHOLDER names) */
struct BgdlPatchRequest {
	char reserved[12];
	char titleId[14];		/* key 220 */
	char contentId[1];		/* key 217; PLACEHOLDER: its length is not known yet */
};

/* a task's progress as the service gives it (BgdlTaskStatus less filesBase; PLACEHOLDER) */
struct BgdlTaskProgress {
	SceUInt32 id;
	int reserved;			/* PLACEHOLDER: padding before the 64-bit fields */
	SceUInt64 total;		/* key 206 */
	SceUInt64 current;		/* key 208 */
	SceUInt64 filesTotal;		/* key 505 */
	SceUInt64 filesCurrent;
	SceUInt16 fileCount;		/* key 501, plus 1 */
	SceUInt16 fileNumber;		/* key 500 */
	int remaining;			/* seconds left */
	int filesRemaining;
};

/* a patch file's path asked for: ur0:'s when set (PLACEHOLDER name) */
#define DOWNLOAD_SVC_FLAG_UR0		0x08

/* what ux0: has and what the downloads need (PLACEHOLDER names) */
struct BgdlSpaceInfo {
	SceUInt64 needed;		/* the clusters asked for, and the licences' room */
	SceUInt64 max;
	SceUInt64 used;
	SceUInt64 free;
};

/* what indicator_plugin is told of a task (PLACEHOLDER names) */
struct IndicatorTaskEvent {
	IndicatorTaskEvent() : taskId(0)
	{
		reserved[0] = 0;
		reserved[1] = 0;
	}

	SceUInt32 taskId;
	int reserved[2];
};

/* the table given as indicator_plugin's GetInterface(1), as far as the service uses it */
struct IndicatorPluginInterface {
	void *reserved[12];		/* PLACEHOLDER: not called here */
	void (*TaskEvent)(const IndicatorTaskEvent *event);	/* PLACEHOLDER name */
};

int Bgdl_81138FE4(IPMI::Session *session, int kind, const BgdlTaskDetail *detail);
int Bgdl_8113903E(IPMI::Session *session, SceUInt32 id);
int Bgdl_811391C0(IPMI::Session *session, SceUInt32 id);
int Bgdl_811391F0(IPMI::Session *session, bool doneOnly);
int Bgdl_81139216(IPMI::Session *session);
int Bgdl_81139238(IPMI::Session *session);
int Bgdl_8113925A(IPMI::Session *session, SceUInt32 id, BgdlTaskProgress *progress);
int Bgdl_81139318(IPMI::Session *session, int max, int unused, int *count, BgdlTaskId *ids);
int Bgdl_811394CC(IPMI::Session *session, const BgdlServerQuery *query, const BgdlServerIds *ids,
		  BgdlContentInfo *info);
int Bgdl_81139536(IPMI::Session *session, const SceUInt32 *flags, char *path);
int Bgdl_8113961C(IPMI::Session *session, const BgdlPackageRequest *request, BgdlPackageResult *result);
int Bgdl_81139680(IPMI::Session *session, SceUInt32 id);
int Bgdl_811396EE(IPMI::Session *session, const BgdlContentRequest *request,
		  const BgdlContentAccount *account, const BgdlContentInfo *info, SceUInt32 *id);
int Bgdl_81139766(IPMI::Session *session, const BgdlDownloadRequest *request, BgdlContentInfo *info,
		  SceUInt32 *id);
int Bgdl_81139906(IPMI::Session *session, SceUInt32 id, BgdlPackageStatus *status);
int Bgdl_81139AE8(IPMI::Session *session, SceUInt32 id, int index, BgdlPackageEntry *entry);
int Bgdl_81139BD6(IPMI::Session *session, SceUInt32 id, const BgdlPackageRequest *request);
int Bgdl_81139C0C(IPMI::Session *session, const BgdlContentQuery *query);
int Bgdl_81139C36(IPMI::Session *session, const BgdlContentQuery *query);
int Bgdl_81139C60(IPMI::Session *session, SceUInt32 id, BgdlDispatcherState *state, int *result);
int Bgdl_81139CBE(IPMI::Session *session, SceUInt32 id);
int Bgdl_81139CDC(IPMI::Session *session, SceUInt32 cluster, BgdlSpaceInfo *info);
int Bgdl_81139E2A(IPMI::Session *session, SceUInt32 cluster);
int Bgdl_81139E52(IPMI::Session *session, const BgdlStorageQuery *query, bool *grw0);
int Bgdl_81139E92(IPMI::Session *session, const FgdlTitleRequest *request, int arg);
int Bgdl_81139EFE(IPMI::Session *session, int unused, SceInt64 rowid);
int Bgdl_81139F46(IPMI::Session *session, const void *arg);
int Bgdl_81139F58(IPMI::Session *session, int unused, const void *arg);
int Bgdl_81139F7E(IPMI::Session *session, int arg, SceUInt64 *needed);
int Bgdl_81139FFE(IPMI::Session *session, const BgdlPatchRequest *request);
int Bgdl_8113A0E2(IPMI::Session *session, const BgdlMtpRequest *request, SceUInt32 *id);
int Bgdl_8113A110(IPMI::Session *session, int arg);

/* the server's own errors (download_service.cpp; PLACEHOLDER names) */
#define DOWNLOAD_SVC_ERROR_INTERNAL		0x80103B00
#define DOWNLOAD_SVC_ERROR_BUSY			0x80103B02	/* made already, or clients left */
#define DOWNLOAD_SVC_ERROR_NOT_STARTED		0x80103B04
#define DOWNLOAD_SVC_ERROR_NO_JOB		0x80103B07	/* a job could not be made */
#define DOWNLOAD_SVC_ERROR_NO_MEMORY		0x80103B20
#define DOWNLOAD_SVC_ERROR_TOO_MANY_CLIENTS	0x80103B22	/* 8 sessions open */

/* what a client connects with (paramSize) */
#define DOWNLOAD_SVC_CLIENT_PARAM_SIZE		20

/* what an auto download job is asked for (PLACEHOLDER names; the lengths are the room
   between the fields the job reads) */
struct DownloadServiceParam {
	char id[52];			/* at most 48 */
	char url[257];
	char name[259];			/* at most 512 kept */
	SceUInt64 value;		/* PLACEHOLDER: what it means is not known */
	SceUInt64 size;
};

/* the request the next file sends for a job (PLACEHOLDER names) */
class DownloadServiceRequest {
public:
	DownloadServiceRequest(const char *id, int kind, const SceUInt64 *size, const char *url,
			       const SceUInt64 *value, const char *name);	/* (0x8113AE20) */
	DownloadServiceRequest(const char *id);				/* (0x8113AF58) */
	DownloadServiceRequest();					/* (0x8113B006) */
	~DownloadServiceRequest();					/* (0x8113AC58) */

	/* the id, the URL, the icon's URL and the icon's path: Sony's calls go to getters its
	   linker folded with others like them (0x814098DC, 0x8142C4C0, 0x811FE008, 0x813E0D4C) */
	__attribute__((noinline)) paf::string *GetId(void)
	{
		return &m_id;
	}

	__attribute__((noinline)) paf::string *GetUrl(void)
	{
		return &m_url;
	}

	__attribute__((noinline)) paf::string *GetIconUrl(void)
	{
		return &m_iconUrl;
	}

	__attribute__((noinline)) paf::string *GetName(void)
	{
		return &m_name;
	}

	paf::string *GetString1(void);					/* (0x8113ACFE) */
	const SceUInt64 *GetSize(void);					/* (0x8113AD04) */
	const SceUInt64 *GetValue(void);				/* (0x8113AD08) */
	void SetId(const char *id);					/* (0x8113AD0C) */
	void SetKind(int kind);						/* (0x8113AD3C) */
	void SetUrl(const char *url);					/* (0x8113AD40) */
	void SetName(const char *name);					/* (0x8113AD74) */
	void SetSize(const SceUInt64 *size);				/* (0x8113ADAA) */
	void SetValue(const SceUInt64 *value);				/* (0x8113ADB4) */
	int Clear(void);						/* (0x8113ADBE) */

	int Bgdl_8113B096(void);
	int Bgdl_8113B164(void);

	paf::string m_id;
	paf::string m_string1;		/* PLACEHOLDER: not set by the constructors */
	paf::string m_url;
	paf::string m_iconUrl;		/* PLACEHOLDER: the icon downloaded when m_name is empty */
	paf::string m_name;		/* the icon's path, when there is one */
	paf::string m_string5;		/* PLACEHOLDER: not set by the constructors */
	SceUInt64 m_size;
	SceUInt64 m_value;
	int m_kind;
	int m_reserved;			/* PLACEHOLDER: only cleared */
	bool m_flag;			/* PLACEHOLDER: only cleared */
};

/* the next file's (0x8113C278..; PLACEHOLDER names): the lock on the podl cache, while a
   request is handed over */
class PodlCacheLock {
public:
	PodlCacheLock();						/* (0x8113C9CC) */
	~PodlCacheLock();						/* (0x8113C9D4) */
	int Lock(void);							/* (0x8113C9F0) */

	SceUID m_fd;
};

/* a cache entry's info.dat (PLACEHOLDER names): what a request's value, size and URL were */
struct PodlCacheInfo {
	SceUInt64 value;
	SceUInt64 size;
	char url[257];
};

/* the bytes of PodlCacheInfo info.dat holds */
#define PODL_CACHE_INFO_SIZE	272

int Bgdl_8113C278(const char *id);
int Bgdl_8113D5CC(paf::string *path);
int Bgdl_8113D6F8(const char *path, void *buf, SceSize size);

/* the podl cache's entries, one folder each, read one by one into a request (PLACEHOLDER
   names); m_index is negative once the last one was read */
class PodlCacheIterator {
public:
	PodlCacheIterator() : m_index(-1)
	{
	}

	~PodlCacheIterator()
	{
		Close();
	}

	void Close(void)
	{
		m_path.clear();
		if (m_index >= 0) {
			m_dir.Close();
			m_index = -1;
		}
	}

	/* the next entry's id, value and size in request; with withIcon its URL and icon too
	   (0x81419780) */
	__attribute__((noinline)) int Next(DownloadServiceRequest *request, bool withIcon)
	{
		if (m_index < 0) {
			m_path = "/podl/cache";
			if (Bgdl_8113D5CC(&m_path) >= 0 && m_dir.Open(m_path.c_str()) >= 0)
				m_index = 0;
		}
		m_index++;
		request->Clear();
		{
			paf::DirEnt ent;

			for (;;) {
				int ret = m_dir.Read(&ent);

				if (ret < 0) {
					Close();
					if (ret == SCE_PAF_ERROR_DIR_NO_MORE_ENTRY)
						break;
					return ret;
				}
				if (ent.type == paf::DirEnt::Type_Dir) {
					request->SetId(ent.name.c_str());
					break;
				}
			}
		}
		if (m_index >= 0) {
			paf::string path = m_path + '/' + *request->GetId() + "/info.dat";
			PodlCacheInfo info = {0};
			int ret = Bgdl_8113D6F8(path.c_str(), &info, PODL_CACHE_INFO_SIZE);

			if (ret != PODL_CACHE_INFO_SIZE) {
				Close();
				return ret;
			}
			request->SetValue(&info.value);
			request->SetSize(&info.size);
			if (withIcon) {
				request->SetUrl(info.url);
				path = m_path + '/' + *request->GetId() + "/icon.dds";
				if (paf::LocalFile::IsFile(path.c_str()))
					request->SetName(path.c_str());
			}
		}
		return 0;
	}

	int m_index;
	paf::string m_path;
	paf::Dir m_dir;
};

int Bgdl_8113C5B2(DownloadServiceRequest *request, bool autoDownload);
int Bgdl_8113C9B4(DownloadServiceRequest *request);
int Bgdl_8113E22C(const char *id);
int Bgdl_8113E288(const char *id);
int Bgdl_8113B456(const char *loginId, const char *productId, int flag);

/* (0x81409E2C) methods that do nothing: inline "return 0"s, folded with others like them */
inline __attribute__((noinline)) int Bgdl_81409E2C(IPMI::Session *session)
{
	return 0;
}

inline __attribute__((noinline)) int Bgdl_81409E2C(IPMI::Session *session, SceUInt32 requestId,
						   bool flag)
{
	return 0;
}

/* download_service_drm.cpp's */
/* (0x8113B08A, 0x8113B090; PLACEHOLDER names) this file's: the podl cache's size, and a buffer's
   output size; both folded by Sony's linker with other code like them */
int Bgdl_8113B08A(void);
SceSize Bgdl_8113B090(const IPMI::BufferInfo *info);

int Bgdl_8113BC9A(IPMI::Session *session, SceUInt32 requestId, int result);
int Bgdl_8113BCBC(IPMI::Session *session, SceUInt32 requestId, int result);
int Bgdl_8113BCDE(IPMI::Session *session, SceUInt32 requestId, int result);
int Bgdl_8113BCFE(IPMI::Session *session, SceUInt32 methodId, const IPMI::DataInfo *pInList,
		  SceUInt32 inCount, IPMI::BufferInfo *pOutList, SceUInt32 outCount);
int Bgdl_8113C220(IPMI::Session *session, SceUInt32 requestId, SceUInt32 methodId,
		   const IPMI::DataInfo *pInList, SceUInt32 inCount);

/* a client's job on the service's queue, listed until it is destroyed so that a closed
   session's jobs can be cancelled (PLACEHOLDER names) */
class DownloadServiceJob : public paf::job::JobItem {
public:
	DownloadServiceJob(IPMI::Session *session, SceUInt32 requestId)
		: paf::job::JobItem("SceDownloadServiceJob", NULL), m_session(session),
		  m_requestId(requestId)
	{
		s_lock.Lock();
		s_jobs.push_back(this);
		s_lock.Unlock();
	}

	virtual ~DownloadServiceJob()
	{
		s_lock.Lock();
		s_jobs.remove(this);
		s_lock.Unlock();
	}

	virtual int Run() = 0;

	virtual void Finish(int result)
	{
	}

	IPMI::Session *m_session;
	SceUInt32 m_requestId;
	int m_reserved;			/* PLACEHOLDER: not read or written */

	static paf::list<DownloadServiceJob *> s_jobs;	/* (0x81564AD0) */
	static paf::thread::Mutex s_lock;		/* (0x81564AE0) */
};

/* the job of Bgdl_8113A78C: an auto download's request sent (PLACEHOLDER names) */
class DownloadServiceAutoJob : public DownloadServiceJob {
public:
	DownloadServiceAutoJob(IPMI::Session *session, SceUInt32 requestId,
			       const DownloadServiceParam *param)
		: DownloadServiceJob(session, requestId), m_param(param)
	{
	}

	virtual int Run()
	{
		const DownloadServiceParam *param = m_param;
		int ret;

		if (param == NULL)
			return DOWNLOAD_SVC_ERROR_INVALID_ARGUMENT;

		DownloadServiceRequest request(param->id, 1, &param->size, param->url,
					       &param->value, param->name);

		ret = request.Bgdl_8113B096();
		return ret;
	}

	virtual void Finish(int result)
	{
		s_lock.Lock();
		if (IsCanceled()) {
			s_lock.Unlock();
			return;
		}
		Bgdl_8113BC9A(m_session, m_requestId, result);
		s_lock.Unlock();
	}

	const DownloadServiceParam *m_param;
};

/* the job of Bgdl_8113A982: a title's request sent (PLACEHOLDER names) */
class DownloadServiceTitleJob : public DownloadServiceJob {
public:
	DownloadServiceTitleJob(IPMI::Session *session, SceUInt32 requestId, const char *id)
		: DownloadServiceJob(session, requestId), m_id(id)
	{
	}

	virtual int Run()
	{
		const char *id = m_id;
		int ret;

		if (id == NULL)
			return DOWNLOAD_SVC_ERROR_INVALID_ARGUMENT;

		DownloadServiceRequest request(id);

		ret = request.Bgdl_8113B164();
		return ret;
	}

	virtual void Finish(int result)
	{
		s_lock.Lock();
		if (IsCanceled()) {
			s_lock.Unlock();
			return;
		}
		Bgdl_8113BCBC(m_session, m_requestId, result);
		s_lock.Unlock();
	}

	const char *m_id;
};

int Bgdl_8113A76C(IPMI::Session *session, const void *arg);
int Bgdl_8113A78C(IPMI::Session *session, SceUInt32 requestId, const DownloadServiceParam *param);
int Bgdl_8113A982(IPMI::Session *session, SceUInt32 requestId, const char *id);

#endif
