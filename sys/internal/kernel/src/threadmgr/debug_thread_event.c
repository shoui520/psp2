/* SceKernelThreadMgr (threadmgr.c): the early debugger event notifier. */
#include "private.h"

/* PLACEHOLDER: name; tells both groups of the debugger's handlers of a thread event */
void callDebuggerThreadEventHook(SceUID processId, SceUID threadId, uint32_t event, int *status)
{
	struct ThreadmgrDebugThreadEvent message;
	ThreadmgrDebugHandler handler;
	int group;

	for (group = 0; group < 2; group++) {
		handler = s_threadmgrRuntime.manager->
			debugHandlers[group][DEBUG_HANDLER_THREAD_EVENT];
		if (handler == NULL)
			continue;
		message.processId = processId;
		message.threadId = threadId;
		message.event = event;
		if (event == DEBUG_THREAD_EVENT_EXIT)
			message.status = *status;
		handler(sizeof(message), &message);
	}
}
