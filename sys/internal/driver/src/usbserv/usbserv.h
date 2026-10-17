#pragma once
#include <stdint.h>
#include <psp2kern/usbserv.h>
#include <psp2kern/usbd.h>
#include <psp2kern/udcd.h>
#include <psp2kern/kernel/threadmgr/event_flags.h>
#include <psp2kern/kernel/threadmgr/mutex.h>
#include <psp2kern/kernel/threadmgr/thread.h>
#include <psp2kern/kernel/aimgr.h>
int ksceGpioSetPortMode(int bus, int port, int mode);
int ksceGpioPortSet(int bus, int port);
int ksceGpioPortClear(int bus, int port);
#include <sysmem/debug.h>

/* Names unknown: hardware operations identified by their import NIDs. */
int ScePwmForDriver_13161CCC(unsigned channel, uint32_t flags, uint32_t period, uint32_t duty);
int ScePervasiveForDriver_13CC07C9(int port, int value);
int ScePervasiveForDriver_29E65AE1(int port, unsigned value, unsigned size);
int ScePervasiveForDriver_4AF7A01E(int port, int value);
int ScePervasiveForDriver_4F64DEF6(int port);
int ScePervasiveForDriver_6B2BE37E(int port, int mode);
int ScePervasiveForDriver_A2EFD7AF(int port, int value);
int ScePervasiveForDriver_AD1E81EB(int port, int value);
int ScePervasiveForDriver_BB67EF58(int port);
int SceUdcdForDriver_1A5A3851(void);
int SceUdcdForDriver_2A234CAF(int port);
int SceUdcdForDriver_9E7F7EFE(void);
int SceUdcdForDriver_D01CFD22(void);
int SceUdcdForDriver_E9E75C49(int port);
int SceSysconForDriver_4A42712F(void (*callback)(unsigned, void *), void *arg);
int SceSysconForDriver_67A4CB9F(void (*callback)(unsigned, void *), void *arg);
int SceSysconForDriver_A26586B2(void (*callback)(unsigned, void *), void *arg);
int SceSysconForDriver_B9EA2FA8(void);
int ksceSysconGetMultiCnInfo(unsigned *info);
int ksceSysconGetMicroUsbInfo(unsigned *info);
int ksceSysconGetHardwareInfo(void);
int ksceSysconCtrlDolceUsbPower(int enable);
int ksceSysconCtrlAccPower(int enable);
int ksceSysconCtrlDevKitUsbPower(int enable);
int ksceSysrootIsSafeMode(void);
int sceKernelIsUserModeThread(void);
int sceKernelGetProcessId(void);
int ksceKernelSearchModuleByName(const char *name);
int ksceKernelPowerTick(int type);
SceUID ksceKernelRegisterTimer(const char *name, unsigned delay, int (*callback)(void *), void *arg);
SceUID ksceKernelRegisterSysEventHandler(const char *name, int (*callback)(int, int, void *), void *arg);
int ksceKernelUnregisterSysEventHandler(SceUID uid);
SceUID ksceKernelRegisterProcEventHandler(const char *name, const void *handlers, unsigned flags);
void ksceKernelPrintfAssertLevel(unsigned level, int condition, const SceKernelDebugInfo *info,
	const void *lr, const char *format, ...);
int sceUsbServAccessoryActivate(void);
int sceUsbServAccessoryDeactivate(void);
