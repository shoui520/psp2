/*
 * esetask.hpp: the background download library's task (Sony's EseTask, "PAF_NEW EseTask" in
 * the 0.990 prototype): what the manager keeps of each download (304
 * bytes, made in 0x811000D4, filled from the task's param file in 0x810FF6F4), kept in a
 * BgdlTaskList. The param file's keys are Sony's numbers (100-116, 206, 208, 217, 220, 500,
 * 501, 505, 5002 on); no record names the other types or the fields: the names are ours
 * (PLACEHOLDER), and a field known only by the key it is read from is named by the key.
 */
#ifndef ESECRON_ESETASK_HPP
#define ESECRON_ESETASK_HPP
#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <thread/thread.h>
#include <job/job.h>

#include "esebase.hpp"
#include "shell.h"

/* a task's state as the manager shows it (the manager copies 60 bytes of it) */
struct BgdlTaskStatus {
	BgdlTaskStatus() : id(0), total(0), current(0), filesTotal(0), filesCurrent(0), filesBase(0),
		fileCount(1), fileNumber(1), remaining(0), filesRemaining(0)
	{
	}

	SceUInt32 id;
	int reserved;			/* PLACEHOLDER: padding before the 64-bit fields */
	SceUInt64 total;		/* key 206 */
	SceUInt64 current;		/* key 208 */
	SceUInt64 filesTotal;		/* key 505 */
	SceUInt64 filesCurrent;
	SceUInt64 filesBase;		/* the sizes of the files already done (keys 5002 on) */
	SceUInt16 fileCount;		/* key 501, plus 1 */
	SceUInt16 fileNumber;		/* key 500 */
	int remaining;			/* seconds left (BgdlProgress::GetRemaining) */
	int filesRemaining;
};

/* libmtp's background copy, as the task keeps it (PLACEHOLDER names) */
/* a copy's progress as libmtp reports it (libmtp's MtpBgCopyProgress, less the id) */
struct BgdlTaskCopy {
	BgdlTaskCopy() : progress(0), number(0), total(1), remaining(0), reserved(0)
	{
		name = "";
	}

	paf::string name;
	SceUInt32 progress;		/* in percent */
	int number;			/* the object being copied, from 1 */
	SceUInt32 total;
	int remaining;			/* seconds left */
	int reserved;
};

/*
 * libmtp's background copy (SceLibMtp_BF311CC8), as the shell gives it: libmtp's own
 * mtp_internal.h has the strings at +0x14 and +0x2C as reserved words
 */
struct MtpBgCopyProgress {
	SceUInt32 id;
	paf::string name;
	SceUInt32 progress;
	int number;
	SceUInt32 total;
	int remaining;
	int reserved;
};

class MtpBgCopyHost {
public:
	virtual ~MtpBgCopyHost() {}
	virtual int isRequested() = 0;
};

struct MtpBgCopyRunParam {
	SceUInt32 id;
	void *arg;
	void (*notify)(void *arg, const MtpBgCopyProgress *progress);
	int (*accept)(void);
	MtpBgCopyHost *host;
	paf::string file;		/* the task's file (key 203) */
	paf::string path;		/* the task's folder */
	paf::string paramFolder;	/* the folder its param file names (key 229) */
};

/* a task's flags from its param file (Sony's m_ParamCache, with its "type"; the rest ours) */
struct EseParamCache {
	SceUInt32 type : 4;		/* key 100 */
	SceUInt32 key107 : 4;
	SceUInt32 runStatus : 2;	/* key 101 (setRunStatus) */
	SceUInt32 key102 : 1;
	SceUInt32 ready : 1;		/* key 104 and key 113 both 0 */
	SceUInt32 key108 : 2;
	SceUInt32 state : 3;		/* key 109 (setState) */
	SceUInt32 key112 : 1;
	SceUInt32 key113 : 1;
	SceUInt32 key110 : 1;
	SceUInt32 key111 : 3;
	SceUInt32 kind : 3;		/* key 114 (BgdlTaskInfo::kind) */
	SceUInt32 key115 : 1;
	SceUInt32 key116 : 1;
	SceUInt32 reserved : 4;		/* PLACEHOLDER: kept as they are */
};

/* what the manager's listeners are told */
struct BgdlEvent {
	int type;			/* 0 added, 3 state, 4 run status (PLACEHOLDER: others) */
	SceUInt32 id;
	EseParamCache param;
	int result;
	int reserved;			/* PLACEHOLDER: always 0 so far */
	SceUInt64 value;
};

/* what a task says of its package (PLACEHOLDER names), copied under "SceBgdlRMutexPackage" */
struct BgdlTaskPackage {
	BgdlTaskPackage() : key220((const char *)NULL), reserved2(0), reserved3(0)
	{
	}

	BgdlTaskPackage &operator=(const BgdlTaskPackage &src)
	{
		key220 = !src.key220.empty() ? src.key220.c_str() : NULL;
		key217 = !src.key217.empty() ? src.key217.c_str() : NULL;
		reserved2 = src.reserved2;
		reserved3 = src.reserved3;
		return *this;
	}

	paf::string key220;		/* the title id (13 characters kept) */
	paf::string key217;		/* the content id */
	int reserved2;			/* PLACEHOLDER: copied, not read yet */
	int reserved3;
};

class EseTask {
public:
	EseTask(SceUInt32 taskId)
		: id(taskId), result(BGDL_ERROR_NOT_LOADED), value(0), transfer(), files(),
		  m_removing(false), copyResult(0)
	{
	}

	int setRunStatus(int status);					/* (0x810FD94C) */
	int setState(int state, int *current, bool force);		/* (0x810FDA08) */
	int GetFolder(BgdlTaskPath *path, bool fromParam);		/* (0x810FDAEC) */
	int getFilePath(BgdlTaskPath *path, bool fromParam);		/* (0x810FDD12) */
	int copyMtp(int *runStatus);					/* (0x810FE06C) PLACEHOLDER name */
	int invoke(void);						/* (0x810FE448) */
	int Load(void);			/* (0x810FF6F4) PLACEHOLDER name: the task read from its param file */
	int execute(int *runStatus);					/* (0x8111FEBC) esehttp.cpp */

	SceUInt32 id;			/* the task's number, its folder's name */
	int result;			/* key 104; 0x80103A1A until it is read */
	SceUInt64 value;		/* PLACEHOLDER: passed in the task's events */
	BgdlProgress transfer;		/* keys 206 and 208 */
	BgdlProgress files;		/* keys 505 and 5002 on */
	bool m_removing;		/* PLACEHOLDER name: RemoveTask is removing it */
	EseParamCache m_ParamCache;
	BgdlTaskPackage package;	/* guarded by "SceBgdlRMutexPackage" */
	BgdlTaskStatus status;
	int copyResult;			/* from libmtp's progress */
	BgdlTaskCopy copy;
};

/* a function told of the manager's events */
struct BgdlListener {
	void (*func)(BgdlEvent *event, void *arg);
	void *arg;
};

#define BGDL_LISTENER_MAX	16

/*
 * The manager: one, made by bgdlManagerCreate (0x811041EC; constructor 0x8110401E), its
 * thread "SceBgdlManagerThread" running the tasks (0.990's startMng). The mutexes' names are
 * Sony's ("SceBgdlMutexEseTaskList", ...); the class's and the fields' names are ours
 * (PLACEHOLDER).
 */
#define BGDL_QUEUE_MAX		32

/* a task as the manager last listed it (PLACEHOLDER names) */
struct BgdlTaskSummary {
	bool ready;			/* done: no error, not to run again, key 113 set */
	EseParamCache param;
	char titleId[13];
	char padding0;
	char contentId[51];
	char padding1[3];
};

/* the manager's list of its tasks, cleared before anything else is made (PLACEHOLDER name) */
struct BgdlTaskSummaryList {
	BgdlTaskSummaryList()
	{
		sce_paf_memset(entries, 0, sizeof(entries));
	}

	BgdlTaskSummary entries[BGDL_QUEUE_MAX];
};

/* an entry of appmgr's background download queue (vshAppMgrBgdlSetQueueStatus) */
struct BgdlQueueItem {
	SceUInt32 type;			/* 1, 3: an application; 2: additional content; 4 */
	SceUInt32 state;		/* 1: ready */
	char contentId[48];
};

class BgdlManager {
public:
	BgdlManager();							/* (0x8110401E) */
	int prepare_device(const char *device, bool clean, int quota);	/* (0x81103834) probable Sony name */
	void Bgdl_811033C4(void);
	void Bgdl_81103224(bool clean);
	__attribute__((noinline)) void UpdateFlags(SceUInt32 value, SceUInt32 mask);	/* (0x810FD758) */
	bool IsCancelled(void);						/* (0x810FD8EC) */
	void NotifyListeners(BgdlEvent *event);				/* (0x810FD90C) */
	__attribute__((noinline)) EseTask *FindTask(SceUInt32 id);	/* (0x810FDC1C) */
	/* (0x810FE038) PLACEHOLDER name: false when the running task is already cancelled */
	bool SetCancelHandler(void (*handler)(void *arg), void *arg);
	void UpdateQueueStatus(void);					/* (0x810FE5A4) PLACEHOLDER name */
	int deleteTask(SceUInt32 id, const EseParamCache *param);	/* (0x810FE8F0) probable */
	int finishTask(EseTask *task, int result);			/* (0x810FED12) PLACEHOLDER name */
	bool IsSuspended(void);						/* (0x810FF102) PLACEHOLDER name */
	bool NextTask(EseTask **next);					/* (0x810FF108) PLACEHOLDER name */
	int RemoveTask(SceUInt32 id);					/* (0x810FF37C) PLACEHOLDER name */
	void SetStatus(const BgdlTaskStatus *status);			/* (0x810FF6CC) PLACEHOLDER name */
	int ReloadTask(SceUInt32 id);					/* (0x810FFF22) PLACEHOLDER name */
	/* (0x811000D4) PLACEHOLDER name: a task made from its first keys; *id its new id */
	int AddTask(SceUInt32 *id, int type, SceUInt32 key111, const char *key105, bool key115, bool key116);
	/* (0x8110088A) PLACEHOLDER name: the title's storage is there; *grw0 when it is grw0: */
	int CheckStorage(const char *titleId, bool *grw0);

	int reserved0;			/* PLACEHOLDER: only cleared so far */
	BgdlTaskList m_tasks;
	BgdlTaskSummaryList m_summaries;
	paf::thread::Mutex m_taskListMutex;	/* "SceBgdlMutexEseTaskList" */
	paf::thread::Cond m_taskListCond;	/* "SceBgdlCondEseTaskList" */
	paf::thread::RMutex m_cancelMutex;	/* "SceBgdlMutexEseCancel" */
	paf::thread::Mutex m_listenersMutex;	/* "SceBgdlMutexEseListeners" */
	paf::thread::RMutex m_syncHandlerMutex;	/* "SceBgdlRMutexEseSyncHandler" */
	paf::thread::RMutex m_progressMutex;	/* "SceBgdlRMutexProgerss" */
	int m_error;			/* 0x80103A15 until it is started */
	bool m_stopping;		/* PLACEHOLDER name: the thread is to end */
	bool m_removeRunning;		/* PLACEHOLDER name: the running task is to be removed */
	bool m_cancelled;		/* the running task was told to stop */
	void (*m_cancelHandler)(void *arg);	/* stops the running task */
	void *m_cancelHandlerArg;
	EseTask *m_runningTask;		/* PLACEHOLDER name: the task running, or NULL */
	SceUInt32 m_lastId;		/* PLACEHOLDER name: the last task run (one less when cancelled) */
	SceUInt32 m_nextId;		/* ux0:'s next task id */
	SceUInt32 m_nextInternalId;	/* ur0:'s, from 0x80000000 */
	SceUInt32 m_flags;		/* what the tasks may do (UpdateFlags) */
	bool m_ux0Ready;		/* ux0: is there */
	bool reserved5;			/* PLACEHOLDER: only cleared so far */
	bool m_resumed;			/* PLACEHOLDER name: the system resumed (SHELL_EVENT_POWER) */
	bool m_suspended;		/* PLACEHOLDER name: no task is started while it is set */
	BgdlTaskStatus m_status;	/* the running task's (60 bytes copied) */
	int m_copyResult;
	BgdlTaskCopy m_copy;
	bool m_hasPending;		/* PLACEHOLDER name: a task of kind 5 or key 111 = 2 is listed */
	BgdlListener m_listeners[BGDL_LISTENER_MAX];
	int m_listenerCount;
	char reserved9[0x40];		/* PLACEHOLDER: cleared by the constructor, not read yet */
	char reserved10[0x40];
	int reserved11;
	BgdlQueueItem m_queue[BGDL_QUEUE_MAX];
	char reserved13[0x400];		/* PLACEHOLDER: not even cleared so far */
};

/* UpdateFlags run by the job queue (0.990's name; made in 0x81104574) */
class Job_EseSetPermission : public paf::job::JobItem {
public:
	virtual ~Job_EseSetPermission()
	{
	}

	virtual int Run();					/* (0x810FD8A4) */

	virtual void Finish(int result)
	{
	}

	SceUInt32 m_value;
	SceUInt32 m_mask;
};

/* the next file's (0x8113AF58): a content's auto download (PLACEHOLDER names) */
class BgdlAutoDownload {
public:
	BgdlAutoDownload(const char *contentId);
	~BgdlAutoDownload();
	int Bgdl_8113B164(void);

	char reserved[104];		/* PLACEHOLDER: the next file's */
};

/* a content's auto download turned off ("Job_EseUnsetAutoDownload") */
class Job_EseUnsetAutoDownload : public paf::job::JobItem {
public:
	Job_EseUnsetAutoDownload(const char *contentId)
		: paf::job::JobItem("Job_EseUnsetAutoDownload", NULL), m_contentId(contentId)
	{
	}

	virtual ~Job_EseUnsetAutoDownload()
	{
	}

	virtual int Run()
	{
		BgdlAutoDownload download(m_contentId.c_str());

		return download.Bgdl_8113B164();
	}

	virtual void Finish(int result)
	{
	}

	paf::string m_contentId;
};

bool Bgdl_81114524(SceUInt32 id);	/* the next file's */

/* a finished game download's event ("Job_EseGDEvent"); the shell is told when it is not handled */
class Job_EseGDEvent : public paf::job::JobItem {
public:
	Job_EseGDEvent(SceUInt32 id)
		: paf::job::JobItem("Job_EseGDEvent", NULL), m_id(id), m_handled(false)
	{
	}

	virtual ~Job_EseGDEvent()
	{
	}

	virtual int Run()
	{
		m_handled = Bgdl_81114524(m_id);
		return 0;
	}

	virtual void Finish(int result)
	{
		if (m_handled == false)
			shellEventServerSend(1, NULL, 0);
	}

	SceUInt32 m_id;
	bool m_handled;
};

/* the manager's thread, "SceBgdlManagerThread" (PLACEHOLDER class name) */
class BgdlManagerThread : public paf::thread::Thread {
public:
	virtual void EntryFunction();					/* (0x810FF214) */

	BgdlManager *m_manager;
};

/* a patch registered by title id ("Job_EseRegistPatchByTitleID") */
class Job_EseRegistPatchByTitleID : public paf::job::JobItem {
public:
	Job_EseRegistPatchByTitleID(const BgdlTaskPackage &package, const bool *flag);	/* (0x810FF2AE) */

	virtual ~Job_EseRegistPatchByTitleID()
	{
	}

	virtual int Run();						/* (0x8110313E) */

	virtual void Finish(int result)
	{
	}

	BgdlTaskPackage m_package;
	bool m_flag;			/* PLACEHOLDER name */
};

BgdlManager *bgdlManagerGet(void);				/* (0x810FD8E0) */
paf::string bgdlTaskFolder(SceUInt32 id, bool fromParam);	/* (0x810FDC72) */
void bgdlCountFinished(const EseTask *task);			/* (0x810FEA80) PLACEHOLDER name */
void bgdlUnsetAutoDownload(const EseTask *task);		/* (0x810FEB22) PLACEHOLDER name */
void bgdlManagerCreate(bool clean, const int *quota);		/* (0x811041EC) */

#endif
