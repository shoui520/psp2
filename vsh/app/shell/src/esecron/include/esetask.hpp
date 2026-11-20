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

#include "esebase.hpp"

/* a task's state as the manager shows it (the manager copies 60 bytes of it) */
struct BgdlTaskStatus {
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
struct BgdlTaskCopy {
	paf::string name;
	int state[5];			/* PLACEHOLDER: copied from libmtp's progress, not read yet */
};

class EseTask {
public:
	SceUInt32 id;			/* the task's number, its folder's name */
	int result;			/* key 104; 0x80103A1A until it is read */
	SceUInt64 reserved0;		/* PLACEHOLDER: only cleared so far */
	BgdlProgress transfer;		/* keys 206 and 208 */
	BgdlProgress files;		/* keys 505 and 5002 on */
	bool reserved1;			/* PLACEHOLDER: only cleared so far */
	SceUInt32 type : 4;		/* key 100 */
	SceUInt32 key107 : 4;
	SceUInt32 key101 : 2;
	SceUInt32 key102 : 1;
	SceUInt32 ready : 1;		/* key 104 and key 113 both 0 */
	SceUInt32 key108 : 2;
	SceUInt32 key109 : 3;
	SceUInt32 key112 : 1;
	SceUInt32 key113 : 1;
	SceUInt32 key110 : 1;
	SceUInt32 key111 : 3;
	SceUInt32 kind : 3;		/* key 114 (BgdlTaskInfo::kind) */
	SceUInt32 key115 : 1;
	SceUInt32 key116 : 1;
	SceUInt32 flagsReserved : 4;	/* PLACEHOLDER: kept as they are */
	paf::string key220;
	paf::string key217;
	int reserved2;			/* PLACEHOLDER: only cleared so far */
	int reserved3;
	BgdlTaskStatus status;
	int copyResult;			/* from libmtp's progress */
	BgdlTaskCopy copy;
};

/* a function told of the manager's events */
struct BgdlListener {
	void (*func)(void *event, void *arg);
	void *arg;
};

#define BGDL_LISTENER_MAX	16

/*
 * The manager: one, made by bgdlManagerCreate (0x811041EC; constructor 0x8110401E), its
 * thread "SceBgdlManagerThread" running the tasks (0.990's startMng). The mutexes' names are
 * Sony's ("SceBgdlMutexEseTaskList", ...); the class's and the fields' names are ours
 * (PLACEHOLDER).
 */
class BgdlManager {
public:
	__attribute__((noinline)) void UpdateFlags(SceUInt32 value, SceUInt32 mask);	/* (0x810FD758) */

	int reserved0;			/* PLACEHOLDER: only cleared so far */
	BgdlTaskList m_tasks;
	char reserved1[0x980];		/* PLACEHOLDER: cleared by the constructor, not read yet */
	paf::thread::Mutex m_taskListMutex;	/* "SceBgdlMutexEseTaskList" */
	paf::thread::Cond m_taskListCond;	/* "SceBgdlCondEseTaskList" */
	paf::thread::RMutex m_cancelMutex;	/* "SceBgdlMutexEseCancel" */
	paf::thread::Mutex m_listenersMutex;	/* "SceBgdlMutexEseListeners" */
	paf::thread::RMutex m_syncHandlerMutex;	/* "SceBgdlRMutexEseSyncHandler" */
	paf::thread::RMutex m_progressMutex;	/* "SceBgdlRMutexProgerss" */
	int m_error;			/* 0x80103A15 until it is started */
	bool reserved2;			/* PLACEHOLDER: only cleared so far */
	bool reserved3;
	bool m_cancelled;		/* the running task was told to stop */
	void (*m_cancelHandler)(void *arg);	/* stops the running task */
	void *m_cancelHandlerArg;
	int m_running;			/* a task is running */
	int reserved4;			/* PLACEHOLDER: only cleared so far */
	SceUInt32 m_nextId;		/* ux0:'s next task id */
	SceUInt32 m_nextInternalId;	/* ur0:'s, from 0x80000000 */
	SceUInt32 m_flags;		/* what the tasks may do (UpdateFlags) */
	bool m_ux0Ready;		/* ux0: is there */
	bool reserved5;			/* PLACEHOLDER: only cleared so far */
	bool reserved6;
	bool reserved7;
	BgdlTaskStatus m_status;	/* the running task's (60 bytes copied) */
	int m_copyResult;
	BgdlTaskCopy m_copy;
	bool reserved8;			/* PLACEHOLDER: only cleared so far */
	BgdlListener m_listeners[BGDL_LISTENER_MAX];
	int m_listenerCount;
	char reserved9[0x40];		/* PLACEHOLDER: cleared by the constructor, not read yet */
	char reserved10[0x40];
	int reserved11;
	char reserved12[0x700];
	char reserved13[0x400];		/* PLACEHOLDER: not even cleared so far */
};

#endif
