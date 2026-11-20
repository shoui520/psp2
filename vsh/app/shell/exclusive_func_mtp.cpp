/*
 * exclusive_func_mtp.cpp: MTP's exclusive function, "ExclusiveFuncMtp" (0x810E206C-0x810E26CC):
 * libmtp's heap in the manager's block while MTP runs, ended by a job that waits until the last
 * of its memory is freed. The file's name and the names not quoted are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/stdlib.h>
#include <common/shared_ptr.h>
#include <job/job.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include "shell.h"
#include "exclusive_func.h"

using namespace paf;

/* how long MTP's start waits for the manager to start it: 5 seconds */
#define EXCLUSIVE_FUNC_MTP_START_TIMEOUT	5000000

/* the event flag's bit: all of MTP's memory freed */
#define EXCLUSIVE_FUNC_MTP_EVENT_FREE	0x01

/* what MTP's callback is told (PLACEHOLDER names) */
#define EXCLUSIVE_FUNC_MTP_STARTED	1
#define EXCLUSIVE_FUNC_MTP_STOPPING	3

/* the kind of function MTP is */
#define EXCLUSIVE_FUNC_TYPE_MTP		1

static SceUID s_mtpEvent = -1;
static void *s_mtpMspace = NULL;
static int s_mtpAllocCount;

/*
 * ExclusiveFuncMtp (28 bytes): libmtp's heap in the block while MTP runs, the semaphore its start
 * waits on, the job queue that stops it, and the callback told when it has started and when it
 * stops.
 */
class ExclusiveFuncMtp : public ExclusiveFunc {
public:
	typedef void (*Callback)(int event);

	ExclusiveFuncMtp() : ExclusiveFunc(EXCLUSIVE_FUNC_TYPE_MTP, true), m_sema(-1),
		m_jobQueue(NULL), m_callback(NULL)
	{
		m_mem.base = NULL;
		m_mem.size = 0;
	}

	virtual ~ExclusiveFuncMtp();

	virtual int Start(const ExclusiveFuncMemBlock *mem);
	virtual int Stop();

	static int Begin(Callback callback);
	static int End();
	static void Memstat();

	ExclusiveFuncMemBlock m_mem;
	SceUID m_sema;
	job::JobQueue *m_jobQueue;
	Callback m_callback;
};

/* PLACEHOLDER: the job stopping MTP once the last of its memory is freed (32 bytes) */
class ExclusiveFuncMtpJob : public job::JobItem {
public:
	ExclusiveFuncMtpJob(ExclusiveFuncMtp *func)
		: job::JobItem("SceShellExclusiveFuncMtpJob", NULL), m_func(func)
	{
	}

	virtual ~ExclusiveFuncMtpJob()
	{
	}

	virtual int Run()
	{
		ExclusiveFuncMtp *func = m_func;

		if (func->m_callback != NULL)
			func->m_callback(EXCLUSIVE_FUNC_MTP_STOPPING);

		SceUInt32 bits = 0;

		sceKernelWaitEventFlag(s_mtpEvent, EXCLUSIVE_FUNC_MTP_EVENT_FREE,
				       SCE_KERNEL_EVF_WAITMODE_OR, &bits, NULL);
		if (s_mtpMspace != NULL) {
			ExclusiveFuncMtp::Memstat();
			sceClibMspaceDestroy(s_mtpMspace);
			s_mtpMspace = NULL;
		}
		func->m_mem.base = NULL;
		func->m_mem.size = 0;
		func->SetStopped();
		return 0;
	}

	virtual void Finish(int result)
	{
	}

	ExclusiveFuncMtp *m_func;
};

static ExclusiveFuncPtr s_mtpFunc;

/* (0x810E206C) */
int ExclusiveFuncMtp::Begin(Callback callback)
{
	if (s_mtpMspace != NULL)
		return 0;

	ExclusiveFuncMgr *mgr = ExclusiveFuncMgr::GetInstance();

	if (mgr == NULL)
		return -1;

	ExclusiveFuncMtp *func = new ExclusiveFuncMtp();

	s_mtpFunc = common::SharedPtr<ExclusiveFuncMtp>(func);
	if (s_mtpFunc.get() == NULL)
		return -1;

	int ret = sceKernelCreateEventFlag("SceShellExclusiveFuncMtpEvent", SCE_KERNEL_ATTR_OPENABLE,
					   EXCLUSIVE_FUNC_MTP_EVENT_FREE, NULL);

	if (ret >= 0) {
		s_mtpEvent = ret;
		mgr->Request(&s_mtpFunc);
		ret = sceKernelCreateSema("SceShellExclusiveFuncMtp", 0, 0, 1, NULL);
		if (ret >= 0) {
			func->m_sema = ret;

			SceUInt32 timeout = EXCLUSIVE_FUNC_MTP_START_TIMEOUT;

			ret = sceKernelWaitSema(func->m_sema, 1, &timeout);
			sceKernelDeleteSema(func->m_sema);
			func->m_sema = -1;
			if (ret == 0) {
				if (s_mtpMspace == NULL)
					return -1;
				func->m_callback = callback;
				func->m_jobQueue = new job::JobQueue("SceShellExclusiveFuncMtpJobQueue",
								     NULL);
			}
		}
	}
	return ret;
}

/* (0x810E22CA) */
int shellExclusiveFuncMtpBegin(void (*callback)(int event))
{
	return ExclusiveFuncMtp::Begin(callback);
}

/* (0x810E22D2) */
int ExclusiveFuncMtp::End()
{
	if (s_mtpFunc.get() == NULL)
		return 0;

	ExclusiveFuncMgr *mgr = ExclusiveFuncMgr::GetInstance();

	if (mgr == NULL) {
		s_mtpFunc.reset();
		return -1;
	}
	mgr->Cancel(&s_mtpFunc);

	ExclusiveFuncMtp *func = (ExclusiveFuncMtp *)s_mtpFunc.get();

	if (func->m_sema != -1)
		sceKernelSignalSema(func->m_sema, 1);
	return 0;
}

/* (0x810E2392) */
int shellExclusiveFuncMtpEnd(void)
{
	return ExclusiveFuncMtp::End();
}

/* (0x810E239C) */
void *shellExclusiveFuncMtpMalloc(SceSize size)
{
	void *ptr = sceClibMspaceMalloc(s_mtpMspace, size);

	if (ptr != NULL) {
		s_mtpAllocCount++;
		sceKernelClearEventFlag(s_mtpEvent, 0);
	}
	return ptr;
}

/* (0x810E23D8) */
void *shellExclusiveFuncMtpMemalign(SceSize alignment, SceSize size)
{
	void *ptr = sceClibMspaceMemalign(s_mtpMspace, alignment, size);

	if (ptr != NULL) {
		s_mtpAllocCount++;
		sceKernelClearEventFlag(s_mtpEvent, 0);
	}
	return ptr;
}

/* the heap's use, printed in Sony's debug build ("ExclusiveFuncMtp::Memstat: %u, %u, %u, %u\n") */
inline void ExclusiveFuncMtp::Memstat()
{
	struct {
		struct MspaceStats stats;
		SceUInt32 reserved[4];
	} stats;

	sce_paf_memset(&stats, 0, sizeof(stats));
	sceClibMspaceMallocStats(s_mtpMspace, &stats.stats);
	sce_paf_printf_dummy();
}

/* (0x810E2414) */
void shellExclusiveFuncMtpPrintStats(void)
{
	ExclusiveFuncMtp::Memstat();
}

/* (0x810E2458) */
void shellExclusiveFuncMtpFree(void *ptr)
{
	if (ptr == NULL)
		return;

	sceClibMspaceFree(s_mtpMspace, ptr);
	if (--s_mtpAllocCount == 0)
		sceKernelSetEventFlag(s_mtpEvent, EXCLUSIVE_FUNC_MTP_EVENT_FREE);
	ExclusiveFuncMtp::Memstat();
}

/* (0x810E24CC, 0x810E2520) */
ExclusiveFuncMtp::~ExclusiveFuncMtp()
{
	if (m_jobQueue != NULL) {
		delete m_jobQueue;
		m_jobQueue = NULL;
	}
	if (s_mtpEvent != -1) {
		sceKernelDeleteEventFlag(s_mtpEvent);
		s_mtpEvent = -1;
	}
	m_callback = NULL;
}

/* (0x810E2578) */
int ExclusiveFuncMtp::Start(const ExclusiveFuncMemBlock *mem)
{
	m_mem = *mem;
	s_mtpMspace = sceClibMspaceCreate(m_mem.base, m_mem.size);
	SetStarted();
	if (m_sema != -1)
		sceKernelSignalSema(m_sema, 1);
	if (s_mtpMspace != NULL && m_callback != NULL)
		m_callback(EXCLUSIVE_FUNC_MTP_STARTED);
	return 0;
}

/* (0x810E25C2) */
int ExclusiveFuncMtp::Stop()
{
	common::SharedPtr<job::JobItem> job(new ExclusiveFuncMtpJob(this));

	m_jobQueue->Enqueue(job);
	return 0;
}
