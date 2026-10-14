/* SceGps private interfaces; Sony names are unknown for the driver exports. */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <psp2kern/kernel/cpu.h>
#include <psp2kern/kernel/sysclib.h>
#include <psp2kern/kernel/threadmgr/event_flags.h>
#include <psp2kern/kernel/threadmgr/mutex.h>
#include <psp2kern/kernel/threadmgr/fast_mutex.h>
#include <psp2kern/kernel/threadmgr/thread.h>
#include <psp2kern/kernel/threadmgr/callback.h>
#include <psp2kern/kernel/threadmgr/misc.h>
#include <psp2kern/kernel/sysmem/uid_puid.h>
#include "sce/sysmem.h"
#include "sce/error.h"
#include "sce/gps.h"
/* Official SDK name, not yet present in the shared error header. */
#ifndef SCE_KERNEL_ERROR_NO_MEMORY
#define SCE_KERNEL_ERROR_NO_MEMORY 0x8002710b
#endif
int ksceSblACMgrIsSystemProgram(int pid);
int ksceSblQafMgrIsAllowRemotePlayDebug(void);
int kscePmMgrGetProductMode(uint8_t *mode);
int SceSblPostSsMgrForDriver_128FB35A(void);
int SceRtcForDriver_F139E359(const void *date, void *tick);
int SceRtcForDriver_A7236656(const void *tick);
int ksceKernelRegisterSysEventHandler(const char *name,
	int (*callback)(int, int, void *, void *), void *arg);
int ksceKernelUnregisterSysEventHandler(int uid);

int SceGpsForDriver_092AD9BF(unsigned device, const char *sentence, unsigned size);
int _sceGpsOpen(int callback);
int _sceGpsClose(void);
int _sceGpsStart(unsigned mode);
int _sceGpsStop(void);
int _sceGpsIoctl(unsigned command, void *data, unsigned size, const unsigned *args);
int _sceGpsSelectDevice(unsigned device);
int _sceGpsResumeCallback(void);
int _sceGpsGetState(unsigned *state);
int _sceGpsGetData(void *location, void *satellites);
int _sceGpsGetDeviceInfo(void *info);
int _sceGpsIsDevice(void);
