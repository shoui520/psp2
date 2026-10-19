/* Application system-event syscall dispatch. */

#include "appmgr_shell_event.h"
#include "appmgr_app_event.h"

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
