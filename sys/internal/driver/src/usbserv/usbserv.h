#pragma once
#include <stdint.h>
#include <psp2kern/usbserv.h>
#include <psp2kern/usbd.h>
#include <psp2kern/udcd.h>
#include <psp2kern/kernel/threadmgr/event_flags.h>
#include <psp2kern/kernel/threadmgr/mutex.h>
#include <psp2kern/kernel/threadmgr/thread.h>
#include <psp2kern/kernel/aimgr.h>
#include <sysmem/debug.h>
#include <lowio/pervasive.h>
#include <lowio/pwm.h>
#include <psp2kern/lowio/gpio.h>
#include <syscon/syscon.h>
#include <threadmgr/threadmgr.h>

/* Names unknown: hardware operations identified by their import NIDs. */
int SceUdcdForDriver_1A5A3851(void);
int SceUdcdForDriver_2A234CAF(int port);
int SceUdcdForDriver_9E7F7EFE(void);
int SceUdcdForDriver_D01CFD22(void);
int SceUdcdForDriver_E9E75C49(int port);
int ksceSysrootIsSafeMode(void);
int ksceKernelSearchModuleByName(const char *name);
int ksceKernelPowerTick(int type);
SceUID ksceKernelRegisterSysEventHandler(const char *name, int (*callback)(int, int, void *), void *arg);
int ksceKernelUnregisterSysEventHandler(SceUID uid);
SceUID ksceKernelRegisterProcEventHandler(const char *name, const void *handlers, unsigned flags);
void ksceKernelPrintfAssertLevel(unsigned level, int condition, const SceKernelDebugInfo *info,
	const void *lr, const char *format, ...);
int sceUsbServAccessoryActivate(void);
int sceUsbServAccessoryDeactivate(void);
