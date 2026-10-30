/* The debugger's SceThreadmgr thread event handler (Sony's thread_handler.c). */

#include <sysmem/debug.h>
#include <threadmgr/threadmgr.h>
#include <threadmgr/threadmgr_debugger.h>
#include "dbgp_internal.h"

static int _threadHandler(SceSize size, void *message)
{
	const struct ThreadmgrDebugThreadEvent *event;
	ThreadInfo info;
	SceUInt64 time;
	DbgpProcCB *proc;
	int result;

	time = sceKernelGetSystemTimeWide();
	if (message == NULL) {
		SCE_PRINTF_WITH_INFO(15, 74, "0x%08x\n", 0);
		return 0;
	}
	if (size != sizeof(*event)) {
		SCE_PRINTF_WITH_INFO(15, 79, "0x%08x\n", 0);
		return 0;
	}
	event = message;
	info.type = event->event;
	info.pid = event->processId;
	info.thid = event->threadId;
	if (event->event == DEBUG_THREAD_EVENT_EXIT)
		info.exitStatus = event->status;
	info.time = time;
	proc = dbgpGetProcCB(info.pid);
	if (proc != NULL && info.pid == proc->pid &&
	    (info.type == DEBUG_THREAD_EVENT_CREATE ||
	     info.type == DEBUG_THREAD_EVENT_EXIT)) {
		result = threadInfoSave(proc, &info);
		if (result < 0)
			SCE_PRINTF_WITH_INFO(15, 135, "0x%08x\n", result);
	}
	return 0;
}

int threadHandlerInit(DbgpCB *dbgp)
{
	int result;

	result = SceThreadmgrForDriver_6C2E3A49(1, 0, _threadHandler);
	if (result < 0) {
		SCE_PRINTF_WITH_INFO(15, 39, "0x%08x\n", result);
		return result;
	}
	return 0;
}

/* threadHandlerTerm is left out of Sony's link; its record stays */
SCE_DBGINFO_DEFINE_ORPHAN(threadHandlerTermRecord56, 56, "threadHandlerTerm");
