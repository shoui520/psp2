/* Bluetooth platform adapters; the original source filename is unavailable. */
#include <sce/threadmgr.h>
#include <sce/sysevent.h>
#include <sce/sysroot_bt.h>
#include <psp2kern/kernel/proc_event.h>
#include <psp2kern/kernel/pm_mgr.h>
#include <psp2kern/kernel/aimgr.h>
#include <psp2kern/kernel/threadmgr/thread.h>
#include <psp2kern/io/fcntl.h>
#include "bt_internal.h"

int BtInitializeTimers(const char *threadName);
int BtInitializeAudio(void);
void BtInitializeHidCapture(void);
int BtSystemEvent(int resume, int eventId, void *args, void *opt);
int SceSysrootForDriver_5A4A7F7E(const SysrootBtOps *ops);
extern SceUID gBtSysEventHandlerId;
extern const SceProcEventHandler gBtProcEventHandler;
extern const SysrootBtOps gBtSysrootOps;

int BtInitializeEventHandlers(void)
{
	SceProductMode productMode;
	int result;

	result = ksceKernelRegisterSysEventHandler("SceBt", BtSystemEvent, NULL);
	if (result < 0)
		return result;
	gBtSysEventHandlerId = result;
	result = ksceKernelRegisterProcEventHandler("SceBt", &gBtProcEventHandler, 0);
	if (result < 0)
		goto unregisterSystem;
	gBtStack.inquiryProcessId = -1;
	gBtStack.inquiryCompletionProcessId = -1;
	gBtStack.procEventHandlerId = result;
	SceSysrootForDriver_5A4A7F7E(&gBtSysrootOps);
	result = BtInitializeAudio();
	if (result < 0) {
		ksceKernelUnregisterSysEventHandler(gBtStack.procEventHandlerId);
		goto unregisterSystem;
	}
	BtInitializeHidCapture();
	if (kscePmMgrGetProductMode(&productMode) >= 0 && productMode == 1)
		gBtStack.mode |= 0x1000;
	if (ksceSblAimgrIsDolce())
		gBtStack.mode |= 0x10000;
	return 0;

unregisterSystem:
	ksceKernelUnregisterSysEventHandler(gBtSysEventHandlerId);
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
	SceUID thread = ksceKernelCreateThread(name, entry, priority, 0x2000,
					     0, 0x80000, NULL);
	int result;

	if (thread < 0)
		return thread;
	result = ksceKernelStartThread(thread, argumentSize, argument);
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
	return ksceIoOpen(path, mode, 0666);
}

int BtCloseFile(SceUID file)
{
	return ksceIoClose(file);
}

int BtReadFile(SceUID file, void *destination, SceSize length)
{
	return ksceIoRead(file, destination, length);
}

int BtWriteFile(SceUID file, const void *source, SceSize length)
{
	return ksceIoWrite(file, source, length);
}

int BtSeekFile(SceUID file, int offset, int whence)
{
	return (int)ksceIoLseek(file, (SceOff)offset, whence);
}
