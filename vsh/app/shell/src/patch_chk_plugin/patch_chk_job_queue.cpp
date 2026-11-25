/*
 * patch_chk_job_queue.cpp: singleton manager owning the five worker job queues
 * used by patch_chk_plugin (0x812AE696-0x812AE87B).
 */
#include <stddef.h>
#include <scetypes.h>
#include <job/job.h>
#include "patch_chk_internal.h"

using namespace paf;

static PatchChkJobQueueMgr *s_jobQueueMgr = NULL;	/* 0x81545B6C */

static const char *const s_queueNames[PATCH_CHK_QUEUE_COUNT] = {	/* 0x81545B70 */
	"ScePatchChkRootJobQueue",
	"ScePatchChkDefaultJobQueue",
	"ScePatchChkGameUpdateJobQueue",
	"ScePatchChkCreateTaskJobQueue",
	"ScePatchChkGameOperateJobQueue"
};

PatchChkJobQueueMgr::PatchChkJobQueueMgr()
{
	for (int i = 0; i < PATCH_CHK_QUEUE_COUNT; ++i)
		m_queues[i] = NULL;
}

/* Cancels, drains, and deletes all five job queues (0x812AE7EE, 0x812AE832) */
PatchChkJobQueueMgr::~PatchChkJobQueueMgr()
{
	for (int i = 0; i < PATCH_CHK_QUEUE_COUNT; ++i) {
		if (m_queues[i] != NULL) {
			m_queues[i]->CancelAllItems();
			m_queues[i]->WaitEmpty();
			delete m_queues[i];
			m_queues[i] = NULL;
		}
	}
}

/* Creates the singleton manager if not yet allocated (0x812AE696) */
PatchChkJobQueueMgr *PatchChkJobQueueMgr::Create(void)
{
	if (s_jobQueueMgr == NULL)
		s_jobQueueMgr = new PatchChkJobQueueMgr();
	return s_jobQueueMgr;
}

/* Destroys the singleton manager (0x812AE6D4) */
void PatchChkJobQueueMgr::Destroy(void)
{
	if (s_jobQueueMgr != NULL) {
		delete s_jobQueueMgr;
		s_jobQueueMgr = NULL;
	}
}

/* Returns the singleton manager (0x812AE6FA) */
PatchChkJobQueueMgr *PatchChkJobQueueMgr::GetInstance(void)
{
	return s_jobQueueMgr;
}

/* Returns one of the five queues by PatchChkQueueIndex (0x812AE706) */
job::JobQueue *PatchChkJobQueueMgr::GetQueue(int queueIndex)
{
	return m_queues[queueIndex];
}

/* Allocates all five single-threaded job queues with 16 KiB stacks (0x812AE70E) */
void PatchChkJobQueueMgr::InitializeQueues(void)
{
	job::JobQueue::Option opt;
	opt.priority = 0x10000100;
	opt.stackSize = 0x4000;
	opt.numThreads = 1;

	for (int i = 0; i < PATCH_CHK_QUEUE_COUNT; ++i)
		m_queues[i] = new job::JobQueue(s_queueNames[i], &opt);
}
