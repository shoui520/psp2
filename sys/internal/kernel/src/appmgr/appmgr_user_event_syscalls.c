/* User-memory copies for application system-event delivery. */

#include <intrmgr/syscall_copy.h>
#include <appmgr/appmgr.h>
#include "appmgr_event.h"

typedef struct AppmgrSystemEventArguments {
	SceUInt32 reserved[4];
} AppmgrSystemEventArguments;

_Static_assert(sizeof(AppmgrSystemEventArguments) == 16,
	       "system event argument block");

int _sceAppMgrReceiveEventNum(int *eventNum)
{
	int ret;

	BLOCK_OUT(eventNum, sizeof(*eventNum),
		  appmgrReceiveEventNumSyscall(NULL),
		  appmgrReceiveEventNumSyscall((int *)k));
}

int _sceAppMgrReceiveEvent(void *appEvent)
{
	int ret;

	BLOCK_OUT(appEvent, 64,
		  appmgrReceiveEventSyscall(NULL),
		  appmgrReceiveEventSyscall(k));
}

int _sceAppMgrSendSystemEvent(const char *appName, int event,
			    const SyscallUnusedArgs *userArguments)
{
	SyscallUnusedArgs arguments;
	char *name = (char *)appName;
	int ret;

	ARGS_IN(arguments, userArguments);
	STRING_IN(name, appName, 32);
	return appmgrSendSystemEventSyscall(name, event);
}

int _sceAppMgrSendSystemEvent2(const char *appName, const char *titleId, int event,
			     const AppmgrSystemEventArguments *userArguments)
{
	AppmgrSystemEventArguments arguments;
	char *name = (char *)appName;
	char *title = (char *)titleId;
	int ret;

	ARGS_IN(arguments, userArguments);
	STRING_IN(name, appName, 32);
	STRING_IN(title, titleId, 13);
	return appmgrSendSystemEvent2Syscall(name, title, event);
}
