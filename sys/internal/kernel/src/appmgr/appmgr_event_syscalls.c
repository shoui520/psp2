/* Application system-event syscall dispatch. */

#include "appmgr_shell_event.h"

extern int sceAppMgrReceiveEventNum(int *eventNum);
extern int sceAppMgrReceiveEventInternal(void *appEvent);
extern int sceAppMgrSendSystemEvent(const char *appName, int event);
extern int sceAppMgrSendSystemEvent2(const char *appName, const char *titleId,
				  int event);

int appmgrReceiveEventNumSyscall(int *eventNum)
{
	return sceAppMgrReceiveEventNum(eventNum);
}

int appmgrReceiveShellEventNumSyscall(void)
{
	return sceAppMgrReceiveShellEventNum();
}

int appmgrReceiveShellEventSyscall(AppmgrShellEvent *event)
{
	return sceAppMgrReceiveShellEvent(event);
}

int appmgrReceiveEventSyscall(void *appEvent)
{
	return sceAppMgrReceiveEventInternal(appEvent);
}

int appmgrSendSystemEventSyscall(const char *appName, int event)
{
	return sceAppMgrSendSystemEvent(appName, event);
}

int appmgrSendSystemEvent2Syscall(const char *appName, const char *titleId, int event)
{
	return sceAppMgrSendSystemEvent2(appName, titleId, event);
}
