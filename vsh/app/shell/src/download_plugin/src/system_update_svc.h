/*
 * system_update_svc.h: the background download of a system update (system_update_svc.cpp,
 * Sony's file by 0.990's records; SystemUpdateSvc is Sony's name, from its "[Shell][UpdateSvc]
 * Delete SystemUpdateSvc" message). It keeps the update's task in the download list, serves
 * the "SceSystemUpdate" IPMI service and queues the jobs that fetch the update list and the
 * update itself. Every other name is ours (PLACEHOLDER) until the system_update files that own
 * the types are written.
 */
#ifndef _SYSTEM_UPDATE_SVC_H
#define _SYSTEM_UPDATE_SVC_H

#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <std/vector>
#include <thread/thread.h>
#include <job/job.h>
#include <plugin.h>
#include <ipmi/server.h>
#include <ipmi/session.h>
#include <system_update/system_update_job.h>

class DownloadPluginObject;

/* the error the service's calls give (PLACEHOLDER name) */
#define SYSTEM_UPDATE_SVC_ERROR		0x801825FF

/* an update being fetched (PLACEHOLDER names) */
struct SystemUpdateInfo {
	SystemUpdateInfo() : offset(0), size(0), count(1), done(false)
	{
	}

	char reserved[32];
	char path[1024];		/* where the update is written */
	char version[32];
	SceInt64 offset;		/* what of it is there */
	SceInt64 size;
	int count;
	int reserved2;
	bool done;
};

/* an image of an update list entry (PLACEHOLDER names) */
struct SystemUpdateImage {
	int reserved[2];
	paf::string url;
	int reserved2[3];
};

PAF_VECTOR_INLINE_DTOR(SystemUpdateImage)

/* an update list entry (PLACEHOLDER names) */
struct SystemUpdateData {
	int type;
	paf::string version;
	paf::vector<SystemUpdateImage> images;
};

PAF_VECTOR_NOINLINE_CLEAR(SystemUpdateData, paf::allocator<SystemUpdateData>)

/* a download task's detail, as the bgdl library gives it (PLACEHOLDER names) */
struct BgdlTaskDetail {
	char reserved[32];
	SceInt64 offset;		/* what of it is there */
	SceInt64 size;
	SceInt64 times[8];
	char path[2048];		/* where it is written */
	char reserved2[256];
	char title[257];
	char fileName[193];
	char dir[256];
	char type[13];
	char reserved3[385];
};

/* a download task's state, as the bgdl library gives it (PLACEHOLDER names) */
struct BgdlTaskState {
	SceInt64 value208;
	SceInt64 value206;
	char value204[64];
};

/* a task listed by the bgdl library (PLACEHOLDER names) */
struct BgdlTaskId {
	SceUInt32 id;
	int reserved;
};

/* the update's state, as the update's files give it (PLACEHOLDER names) */
struct SystemUpdateStatus {
	SystemUpdateStatus() : state(3), value(0)
	{
		first.erase(0, paf::string::npos);
		second.erase(0, paf::string::npos);
	}

	int state;			/* 0 started, 2 downloaded, 3 none */
	SceInt64 value;			/* what of it is there */
	paf::string first;
	paf::string second;
};

/* the update's local files (PLACEHOLDER names; 0x81131052..) */
class SystemUpdateCtrl {
public:
	SystemUpdateCtrl(int reserved, const char *infoPath, const char *pupPath);	/* (0x81131052) */

	~SystemUpdateCtrl();						/* (0x81131074) */
	SystemUpdateResult Bgdl_811310CA(void);				/* removes them */
	bool Bgdl_81131ABC(void);					/* the update is there */
	bool Bgdl_81131B54(void);					/* its information is there */
	/* (0x81131A18) */
	SystemUpdateResult Bgdl_81131A18(paf::string url, paf::string version, paf::string name,
					 SceInt64 offset, SceInt64 size, int count,
					 SystemUpdateInfo *info);
	/* (0x81131CE6) */
	SystemUpdateResult Bgdl_81131CE6(int type, SystemUpdateInfo *info, SceInt64 reserved);
	/* (0x81131280) the update's information written */
	SystemUpdateResult Bgdl_81131280(SystemUpdateInfo info);
	/* (0x81132734) the update's state */
	SystemUpdateResult Bgdl_81132734(SystemUpdateStatus *status);
	/* (0x8113276C) the update at path checked against its version and size */
	SystemUpdateResult Bgdl_8113276C(paf::string path, paf::string version, SceInt64 size,
					 bool *ok);

	int m_reserved;
	const char *m_infoPath;
	const char *m_pupPath;
	paf::string m_string1;
	paf::string m_string2;
	paf::string m_string3;
	int m_reserved2[6];
};

class SystemUpdateIpmiServer;

class SystemUpdateSvc : public SystemUpdateListener {
public:
	SystemUpdateSvc()
	{
	}

	virtual ~SystemUpdateSvc()
	{
	}

	virtual int Bgdl_8112CFD8(SystemUpdateResult result);

	static bool Bgdl_8112D38C(void);
	static void Bgdl_8112D42A(void);
	__attribute__((noinline)) SystemUpdateResult Bgdl_8112D450(void);
	int Bgdl_8112D508(paf::Plugin *plugin, DownloadPluginObject *object);
	int Bgdl_8112D7F0(void);
	__attribute__((noinline)) SystemUpdateResult Bgdl_8112D88C(SceUInt32 id, BgdlTaskState *state);
	__attribute__((noinline)) SystemUpdateResult Bgdl_8112D9CA(void);
	void Bgdl_8112DA56(void);
	__attribute__((noinline)) SystemUpdateResult Bgdl_8112DA94(SystemUpdateInfo *info);
	SystemUpdateResult Bgdl_8112E7F8(SystemUpdateInfo *info);
	__attribute__((noinline)) int Bgdl_8112EBB0(void);
	/* (0x81413CB0) an inline "return 1", kept out of line and folded with others like it */
	__attribute__((noinline)) int Bgdl_81413CB0(bool flag)
	{
		return 1;
	}

	bool m_started;
	SceUInt32 m_taskId;		/* the update's download task */
	bool m_busy;
	paf::Plugin *m_plugin;
	int m_reserved;
	SystemUpdateInfo m_info;
	paf::string m_infoPath;		/* .../PupInfo */
	SystemUpdateCtrl *m_ctrl;
	int m_state;
	int m_autoUpdate;		/* 1 or 2 when the download plugin said so */
	bool m_hasTask;
	paf::thread::RMutex *m_registLock;
	paf::thread::Mutex *m_seqLock;
	paf::job::JobItem *m_job;
	paf::string m_listUrl;
	paf::vector<SystemUpdateData> m_list;
	paf::string m_url;
	paf::string m_version;
	SceInt64 m_size;
	paf::string m_name;
	bool m_reserved3;
	bool m_checking;
	DownloadPluginObject *m_object;
	SystemUpdateIpmiServer *m_server;
	BgdlTaskDetail m_detail;
};

extern SystemUpdateSvc *g_systemUpdateSvc;		/* (0x81544898) PLACEHOLDER name */

/* the job that fetches an update (PLACEHOLDER names) */
class SystemUpdateSvcJob : public paf::job::JobItem {
public:
	SystemUpdateSvcJob(SystemUpdateSvc *svc, SystemUpdateInfo info)
		: paf::job::JobItem("sec_Job_Update", NULL), m_svc(svc), m_info(info)
	{
	}

	virtual ~SystemUpdateSvcJob()
	{
	}

	virtual int Run()
	{
		if (m_svc != NULL)
			m_svc->Bgdl_8112E7F8(&m_info);
		return -1;
	}

	virtual void Finish(int result)
	{
	}

	SystemUpdateSvc *m_svc;
	SystemUpdateInfo m_info;
};

/* what the service's IPMI clients ask for (PLACEHOLDER names) */
#define SYSTEM_UPDATE_METHOD_CHECK	1001
#define SYSTEM_UPDATE_METHOD_RESUME	1002
#define SYSTEM_UPDATE_METHOD_CANCEL	1003
#define SYSTEM_UPDATE_METHOD_START	1004

typedef void (*SystemUpdateHandler)(int reserved, void *arg, const void *data, SceSize size);

/* the thread the IPMI server's dispatcher runs on (PLACEHOLDER names) */
class SystemUpdateServerThread : public paf::thread::Thread {
public:
	__attribute__((noinline)) SystemUpdateServerThread(const paf::thread::Thread::Option *option);	/* (0x8112F20C) */
	virtual ~SystemUpdateServerThread()
	{
	}

	virtual void EntryFunction();				/* (0x8112F264) */
	__attribute__((noinline)) bool Start(int (*func)(void *), void *arg);		/* (0x8112F248) */

	void *m_arg;
	int (*m_func)(void *);
	bool m_running;
};

/* the "SceSystemUpdate" IPMI service (PLACEHOLDER names) */
class SystemUpdateIpmiServer : public IPMI::Server::EventHandler {
public:
	SystemUpdateIpmiServer() : m_server(NULL), m_reserved(0)
	{
	}

	virtual ~SystemUpdateIpmiServer()
	{
	}

	__attribute__((noinline)) int Init(void *arg);							/* (0x8112EEDA) */
	int Stop(void);								/* (0x8112EFE6) */
	__attribute__((noinline)) int SetHandler(int method, SystemUpdateHandler handler);		/* (0x8112F03A) */

	virtual void onSyncMethodDispatch(IPMI::Session *pSession, SceUInt32 methodId, void *pIn,
					  SceSize inSize, SceUInt32 outCount, void *pOut,
					  SceSize outSize);
	virtual void onAsyncMethodDispatch(IPMI::Session *pSession, SceUInt32 requestId,
					   SceUInt32 methodId, void *pIn, SceSize inSize,
					   void *pOut, SceSize outSize)
	{
	}
	virtual int onNewClient(IPMI::Server *pServer, const IPMI::Session::Config *pConfig,
				const void *pParam, SceSize paramSize);
	__attribute__((noinline)) virtual void onSessionClosedByClient(IPMI::Session *pSession);
	virtual void onSessionKilled(IPMI::Session *pSession);

	IPMI::Server *m_server;
	int m_reserved;
	IPMI::Session *m_sessions[8];
	int m_sessionCount;
	bool m_stopped;
	SceUInt8 *m_workBuffer;
	SceSize m_workBufferSize;
	SceUInt8 *m_serverMemory;
	SystemUpdateServerThread *m_thread;
	SystemUpdateHandler m_onCancel;		/* 1003 */
	SystemUpdateHandler m_onResume;		/* 1002 */
	SystemUpdateHandler m_onStart;		/* 1004 */
	SystemUpdateHandler m_onCheck;		/* 1001 */
	int m_reserved2;
	void *m_arg;
};

/*
 * Other files' (PLACEHOLDER names until they are written): the shell's settings flags
 * (0x812A4C6C..), the bgdl library's task queries (0x81139190..), the update's paths
 * (0x8137C40C..) and the system update jobs' state (0x81132986..).
 */
void *Bgdl_8137C448(void);
const char *Bgdl_8137C40C(void *paths);
const char *Bgdl_8137C484(int reserved);
int Bgdl_812A4C6C(const char *key);
int Bgdl_812A4FBA(const char *key, bool value);
int Bgdl_812A4FC8(const char *key, bool *value);
int Bgdl_812A5032(const char *key, void (*func)(bool value, void *arg), void *arg);
int Bgdl_812A509E(const char *key);
int Bgdl_81139392(int reserved, int kind, int max, int *count, BgdlTaskId *ids);
int Bgdl_8113942C(int reserved, int type, SceUInt32 id, BgdlTaskDetail *detail);
int Bgdl_8113906E(int reserved, SceUInt32 id, BgdlTaskState *state);
int Bgdl_81139190(int reserved, SceUInt32 id);
void Bgdl_81132986(bool value);
void Bgdl_81132992(void);
void Bgdl_811329A2(int value);
SystemUpdateResult Bgdl_81130ECC(const char *id);
int Bgdl_81130EC4(const char *path);
int Bgdl_81138F80(int reserved, int kind, BgdlTaskDetail *detail, SceUInt32 *id);

/* system_update_svc.cpp's callbacks */
void Bgdl_8112ED3C(bool value, void *arg);
void Bgdl_8112ED58(int kind, SceUInt32 id, SceUInt64 rowid, void *arg);
void Bgdl_8112EE04(int reserved, void *arg, const void *data, SceSize size);
void Bgdl_8112EE58(int reserved, void *arg, const void *data, SceSize size);
void Bgdl_8112EE72(int reserved, void *arg, const void *data, SceSize size);
void Bgdl_8112EE7C(int reserved, void *arg, const void *data, SceSize size);
int Bgdl_8112F1F8(void *arg);

#endif
