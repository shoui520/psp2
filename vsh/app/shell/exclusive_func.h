/*
 * exclusive_func.h: the exclusive functions, modes that take the shell's 4 MiB memory block one at
 * a time (MTP, the network's), and the manager that queues them by priority and runs them on the
 * main thread. "ExclusiveFuncMtp" and the objects' kernel names ("SceShellExclusiveFuncMgr",
 * "SceShellExclusiveFuncMemBlock") are Sony's; the rest of the names are ours (PLACEHOLDER).
 */
#ifndef _SHELL_EXCLUSIVE_FUNC_H
#define _SHELL_EXCLUSIVE_FUNC_H

#include <stddef.h>
#include <scetypes.h>
#include <std/list>
#include <common/shared_ptr.h>
#include <thread/thread.h>

/* the memory block a function runs in */
struct ExclusiveFuncMemBlock {
	void *base;
	SceSize size;
};

/* an exclusive function's state bits (PLACEHOLDER names) */
#define EXCLUSIVE_FUNC_STATE_REQUESTED	0x01	/* taken from the queue: to be started */
#define EXCLUSIVE_FUNC_STATE_STARTING	0x02	/* Start called */
#define EXCLUSIVE_FUNC_STATE_STARTED	0x04	/* the function runs in the block */
#define EXCLUSIVE_FUNC_STATE_CANCELED	0x08	/* to be stopped */
#define EXCLUSIVE_FUNC_STATE_STOPPING	0x10	/* Stop called */
#define EXCLUSIVE_FUNC_STATE_STOPPED	0x20	/* the function gave the block back */

/* the kinds of function, each with its priority (PLACEHOLDER: 3 of them) */
#define EXCLUSIVE_FUNC_TYPE_COUNT	3

/*
 * PLACEHOLDER: an exclusive function (8 bytes, constructors 0x810E4B28 and 0x810E4B40): its kind,
 * whether it waits in the queue while another runs, and its state.
 */
class ExclusiveFunc {
public:
	ExclusiveFunc();
	ExclusiveFunc(int type, bool queueable);
	virtual ~ExclusiveFunc();

	/* the block taken (0 when it is started), given back (0 when it is stopping) */
	virtual int Start(const ExclusiveFuncMemBlock *mem) = 0;
	virtual int Stop() = 0;
	/* the manager's pending events (power) told while it is idle: 0 when they are taken */
	virtual int Notify(int events);

	void SetState(int state);		/* (0x810E4B7C) */
	void SetStarted();			/* (0x810E4B86) */
	void SetStopped();			/* (0x810E4B92) */
	int GetType();				/* (0x810E4B9E) */
	bool IsQueueable();			/* (0x810E4BA4) */
	void ClearState(int state);		/* (0x810E4BA8) */
	int GetState();				/* (0x810E4BB4) */

	signed char m_type;
	bool m_queueable;
	signed char m_state;
};

typedef paf::common::SharedPtr<ExclusiveFunc> ExclusiveFuncPtr;

/*
 * PLACEHOLDER: the exclusive function manager (88 bytes, constructor 0x810E1944): the function
 * running and the queue of those waiting, by priority, the events to tell an idle function, and
 * the memory block they run in.
 */
class ExclusiveFuncMgr {
public:
	ExclusiveFuncMgr();
	virtual ~ExclusiveFuncMgr();

	static ExclusiveFuncMgr *GetInstance();

	int Update();
	int Remove(ExclusiveFuncPtr *func);
	int Cancel(ExclusiveFuncPtr *func);
	int Insert(ExclusiveFuncPtr *func);
	int Request(ExclusiveFuncPtr *func);
	int SetPriority(int type, int priority);

	/* a function's priority: its kind and the kind's offset */
	int GetPriority(ExclusiveFunc *func)
	{
		int type = func->GetType();

		return type + m_priority[type];
	}

	ExclusiveFuncPtr m_current;
	paf::list<ExclusiveFuncPtr> m_queue;
	bool m_powerCallbackAdded;
	int m_events;				/* the events to tell the function */
	paf::thread::RMutex m_mutex;
	int m_reserved;				/* PLACEHOLDER: -1 from the constructor, not read */
	void *m_memBlock;
	int m_priority[EXCLUSIVE_FUNC_TYPE_COUNT];
};

/* the event bits of ExclusiveFuncMgr::m_events (PLACEHOLDER name) */
#define EXCLUSIVE_FUNC_EVENT_POWER	0x01

/* PLACEHOLDER: what the photo import gives its caller (1040 bytes) */
struct ExclusiveFuncPhotoImportResult {
	int result;				/* EXCLUSIVE_FUNC_PHOTO_IMPORT_* */
	int error;				/* the dialog's error with ..._ERROR */
	char path[1024];			/* the photo's file */
	int orientation;			/* its ScePhotoImportDialogOrientation */
	int format;				/* its ScePhotoImportDialogFormatType */
};

/* the photo import's results (PLACEHOLDER names) */
#define EXCLUSIVE_FUNC_PHOTO_IMPORT_OK		0
#define EXCLUSIVE_FUNC_PHOTO_IMPORT_CANCELED	1
#define EXCLUSIVE_FUNC_PHOTO_IMPORT_ABORTED	2
#define EXCLUSIVE_FUNC_PHOTO_IMPORT_ERROR	3

typedef void (*ExclusiveFuncPhotoImportCallback)(ExclusiveFuncPhotoImportResult *result,
						 void *arg);

/* the photo import requested with the function to call with the photo, and given up
   (0x810E2F4E, 0x810E3174) */
int StartExclusiveFuncPhotoImport(ExclusiveFuncPhotoImportCallback callback, void *arg);
int AbortExclusiveFuncPhotoImport(void);

#endif /* _SHELL_EXCLUSIVE_FUNC_H */
