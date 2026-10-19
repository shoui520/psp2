/* Bluetooth platform adapters; the original source filename is unavailable. */
#include <threadmgr/threadmgr.h>
#include <sysmem/sysevent.h>
#include <sysmem/sysroot_bt.h>
#include <sysmem/procevent.h>
#include <sysmem/pmmgr.h>
#include <sysmem/sblaimgr.h>
#include <iofilemgr/iofilemgr.h>
#include "bt_internal.h"
#include <sysmem/sysroot.h>

int BtInitializeTimers(const char *threadName);
int BtInitializeAudio(void);
void BtInitializeHidCapture(void);
int BtSystemEvent(int resume, int eventId, void *args, void *opt);
extern SceUID gBtSysEventHandlerId;
extern const SceKernelProcEventHandler gBtProcEventHandler;
extern const SysrootBtOps gBtSysrootOps;

int BtInitializeEventHandlers(void)
{
	SceProductMode productMode;
	int result;

	result = sceKernelRegisterSysEventHandler("SceBt", BtSystemEvent, NULL);
	if (result < 0)
		return result;
	gBtSysEventHandlerId = result;
	result = sceKernelRegisterProcEventHandler("SceBt", &gBtProcEventHandler, NULL);
	if (result < 0)
		goto unregisterSystem;
	gBtStack.inquiryProcessId = -1;
	gBtStack.inquiryCompletionProcessId = -1;
	gBtStack.procEventHandlerId = result;
	SceSysrootForDriver_5A4A7F7E(&gBtSysrootOps);
	result = BtInitializeAudio();
	if (result < 0) {
		sceKernelUnregisterSysEventHandler(gBtStack.procEventHandlerId);
		goto unregisterSystem;
	}
	BtInitializeHidCapture();
	if (ScePmMgrForDriver_2AC815A2(&productMode) >= 0 && productMode == 1)
		gBtStack.mode |= 0x1000;
	if (SceSblAIMgrForDriver_71608CA3())
		gBtStack.mode |= 0x10000;
	return 0;

unregisterSystem:
	sceKernelUnregisterSysEventHandler(gBtSysEventHandlerId);
	return result;
}

int BtInitializeServices(const char *logName, const char *timerThreadName)
{
	int result;

	(void)logName;
	result = BtInitializeTimers(timerThreadName);
	if (!result)
		return BtInitializeEventHandlers();
	return result;
}

SceUID BtCreateThread(SceKernelThreadEntry entry, SceSize argumentSize,
		     void *argument, const char *name, int priority)
{
	SceUID thread = sceKernelCreateThread(name, entry, priority, 0x2000,
					     0, 0x80000, NULL);
	int result;

	if (thread < 0)
		return thread;
	result = sceKernelStartThread(thread, argumentSize, argument);
	if (result < 0)
		return result;
	return thread;
}

int BtExitThread(void)
{
	return sceKernelExitThread(0);
}

int BtDelayThread(SceUInt32 microseconds)
{
	return sceKernelDelayThread(microseconds);
}

SceInt64 BtGetSystemTimeWide(void)
{
	return sceKernelGetSystemTimeWide();
}

SceUID BtOpenFile(const char *path, int mode)
{
	if (mode != 1) {
		if (mode == 2)
			mode = 0x602;
		else if (mode == 3)
			mode = 0x100;
		else
			mode = 0;
	}
	return sceIoOpen(path, mode, 0666);
}

int BtCloseFile(SceUID file)
{
	return sceIoClose(file);
}

int BtReadFile(SceUID file, void *destination, SceSize length)
{
	return sceIoRead(file, destination, length);
}

int BtWriteFile(SceUID file, const void *source, SceSize length)
{
	return sceIoWrite(file, source, length);
}

int BtSeekFile(SceUID file, int offset, int whence)
{
	return (int)sceIoLseek(file, (SceOff)offset, whence);
}
