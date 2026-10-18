/* SceGps private interfaces; Sony names are unknown for the driver exports. */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <sysmem/cpu.h>
#include <psp2kern/kernel/sysclib.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/sysmem.h>
#include <kernel/error.h>
#include <gps/gps.h>
#include <sysmem/qafmgr.h>
#include <sysmem/sysevent.h>
#include <sysmem/pmmgr.h>
#include <acmgr/acmgr.h>
#include <rtc/rtc.h>
#include <post_ss_mgr/sblpostssmgr.h>
/* Official SDK name, not yet present in the shared error header. */
#ifndef SCE_KERNEL_ERROR_NO_MEMORY
#define SCE_KERNEL_ERROR_NO_MEMORY 0x8002710b
#endif

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
