/*
 * utility.c: the current thread and process as DTrace sees them. Before the
 * thread manager has set TPIDRPRW for this CPU, the thread comes from the
 * thread manager's own record of what runs here.
 */
#include <dtrace/utility.h>
#include <sysmem/debug.h>
#include <sysmem/sysmem.h>
#include <threadmgr/threadcb.h>
#include <threadmgr/threadmgr.h>

kthread_t *dtrace_get_current_thread(void)
{
	SceObjectBase *object;
	ThreadCBInfo info;
	kthread_t *thread;

	thread = getCurrentThreadCB();
	if (thread != NULL)
		return thread;

	if (sceKernelGetThreadContextInfo(&info) < 0) {
		SCE_DBG_ASSERT_LEVEL(0, 0, 30);
		return NULL;
	}
	if (sceGUIDGetObjectWithClass(info.threadId, SceThreadmgrForKernel_565BD2DA(),
				      &object) < 0) {
		SCE_DBG_ASSERT_LEVEL(0, 0, 35);
		return NULL;
	}

	thread = &((struct SceUIDThreadObject *)object)->context;
	SCE_DBG_ASSERT_LEVEL(0, thread->magic == THREAD_CB_MAGIC, 39);
	return thread;
}

puid_t dtrace_get_current_thread_puid(void)
{
	kthread_t *thread = dtrace_get_current_thread();

	if (thread == NULL)
		return -1;

	return thread->userThreadId;
}

SceUID dtrace_get_current_thread_id(void)
{
	ThreadCBInfo info;
	kthread_t *thread;

	thread = getCurrentThreadCB();
	if (thread != NULL)
		return thread->threadId;

	if (sceKernelGetThreadContextInfo(&info) < 0) {
		SCE_DBG_ASSERT_LEVEL(0, 0, 98);
		return -1;
	}

	return info.threadId;
}

proc_t *dtrace_get_current_process(void)
{
	kthread_t *thread = dtrace_get_current_thread();

	if (thread == NULL)
		return NULL;

	return thread->process;
}

SceUID dtrace_get_current_process_main_thread_id(void)
{
	proc_t *p = dtrace_get_current_process();

	if (p == NULL)
		return -1;

	return p->mainThread;
}

SceUID dtrace_get_current_process_parent_id(void)
{
	proc_t *p = dtrace_get_current_process();

	if (p == NULL)
		return -1;

	return p->parentPid;
}

SceUID dtrace_get_current_process_id(void)
{
	ThreadCBInfo info;
	kthread_t *thread;

	thread = getCurrentThreadCB();
	if (thread != NULL && thread->process != NULL)
		return thread->process->pid;

	if (sceKernelGetThreadContextInfo(&info) < 0) {
		SCE_DBG_ASSERT_LEVEL(0, 0, 180);
		return -1;
	}

	return info.processId;
}
