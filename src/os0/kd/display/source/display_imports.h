/*
 * SceDisplay: imported functions vitasdk does not declare (or declares
 * differently from how SceDisplay calls them). Unknown Sony names are kept
 * as library/NID identifiers.
 */
#pragma once

#include <psp2kern/types.h>
#include <psp2kern/lowio/iftu.h>
#include "sce/oled.h"
#include "sce/lcd.h"
#include "sce/sysmem.h"

struct ThreadmgrWaitableClass;
int sceKernelUIDInheritedEventClassInit(struct ThreadmgrWaitableClass *cls,
	const char *name, SceSize size, SceClassCallback create, SceClassCallback destroy);
int sceKernelEventObjectInit(SceUID event, SceUInt32 attr, SceUInt64 userData,
	SceUInt32 pattern);
int ksceKernelWaitEvent(SceUID event, SceUInt32 bits, SceUInt32 *result,
	SceUInt64 *userData, SceUInt32 *timeout);
int ksceKernelWaitEventCB(SceUID event, SceUInt32 bits, SceUInt32 *result,
	SceUInt64 *userData, SceUInt32 *timeout);
int ksceKernelPulseEvent(SceUID event, SceUInt32 pattern, SceUInt64 userData);
int ksceKernelPulseEventWithNotifyCallback(SceUID event, SceUInt32 pattern,
	SceUInt64 userData, SceUInt32 arg);
int ksceKernelRegisterCallbackToEvent(SceUID event, SceUID callback, SceUInt32 notifyId);
int ksceKernelUnregisterCallbackFromEvent(SceUID event, SceUID callback);
int SceThreadmgrForDriver_B645C7EF(uint32_t size, void (*destructor)(void *memory, uintptr_t argument),
				   uintptr_t argument);
int ksceKernelProcModeVAtoPA(SceUID pid, SceUInt32 mode, const void *va, uintptr_t *pa);

/* SceSysclibForDriver */
int strncmp(const char *s1, const char *s2, SceSize n);
void *memset(void *dst, int c, SceSize n);
void *memcpy(void *dst, const void *src, SceSize n);
unsigned int __aeabi_uidiv(unsigned int n, unsigned int d);

/* SceSysrootForDriver / SceSysrootForKernel */
int ksceSysrootGetHardwareFlags(void *flags);
int SceSysrootForDriver_7918D44E(void);
int ksceSysrootIsUsbEnumWakeup(void);
int ksceSysrootIsSafeMode(void);
int ksceSysrootIsExternalBootMode(void);
int ksceSysrootIsUpdateMode(void);
int ksceKernelSysrootGetProcessTitleId(SceUID pid, char *titleId, SceSize size);
int sceKernelRegisterInitCallback(int (*callback)(void), int phase, void *argument, int *result);

/* SceProcessmgrForDriver */
int SceProcessmgrForDriver_D141C076(SceUID pid, SceUInt32 *out);
int SceProcessmgrForDriver_E1A67C86(SceUID pid, SceUInt32 *flags);

/* SceThreadmgrForDriver */
SceUID ksceKernelCreateSimpleEvent(const char *name, SceUInt32 attr, SceUInt32 init, void *opt);
int sceKernelDeleteSimpleEvent(SceUID uid);
int sceKernelGetProcessId(void);
int sceKernelIsUserModeThread(void);
int sceKernelCheckWaitableStatus(void);
int sceKernelDelayThread(SceUInt32 usec);
void *SceThreadmgrForDriver_3A72C6D8(SceUID uid);
int SceThreadmgrForDriver_20C228E4(void);

/* SceSblACMgrForDriver / SceSblAIMgrForDriver */
int ksceSblACMgrIsPspEmu(SceUID pid);
int ksceSblACMgrIsGameProgram(SceUID pid);
int ksceSblACMgrIsDevelopmentMode(void);
int ksceSblAimgrIsDolce(void);
int ksceSblAimgrIsGenuineDolce(void);

/* SceKernelUtilsForDriver */
int ksceGzipDecompress(void *dst, SceSize dstSize, const void *src, SceUInt32 *crc);

/* SceDsiForDriver */
int ksceDsiSetLanesAndPixelSize(int bus, int lanes, int pixelSize);
int ksceDsiSetVic(int bus, SceUInt32 vic);
int ksceDsiGetPixelClock(int bus);
int ksceDsiEnableHead(int bus);
int ksceDsiDisableHead(int bus);
int ksceDsiStartDisplay(int bus, int mode);
int ksceDsiSendBlankingPacket(int bus);
int ksceDsiGetVicResolution(SceUInt32 vic, SceUInt32 *width, SceUInt32 *height);
int SceDsiForDriver_6F8029A1(int bus);
int SceDsiForDriver_F2921E29(int bus, int enable);

/* ScePervasiveForDriver */
int kscePervasiveDsiSetPixelClock(int bus, int clock);
int kscePervasiveDsiClockEnable(int bus, SceUInt32 mask);
int kscePervasiveDsiClockDisable(int bus, SceUInt32 mask);
int kscePervasiveDsiResetEnable(int bus, SceUInt32 mask);
int kscePervasiveDsiResetDisable(int bus, SceUInt32 mask);

/* SceIftuForDriver */
int ksceIftuSetInputFrameBuffer(int plane, SceIftuPlaneState *state, int src, int sync);
int ksceIftuSetOutputFormat(int plane, SceUInt32 width, SceUInt32 height, SceUInt32 format,
			    int interlaced);
int ksceIftuEnable(int plane);
int ksceIftuDisable(int plane);
int ksceIftuSetMergeSetting(int plane, SceUInt32 setting);
int SceIftuForDriver_0FCBF457(int plane, const SceIftuCscParams *csc);
int SceIftuForDriver_357EAE24(int plane, SceUInt32 alpha);
int SceIftuForDriver_D64F4C6B(int plane, const SceIftuCscParams *csc);

/* SceHdmiForDriver */
int SceHdmiForDriver_08AFFB03(int enable);
int SceHdmiForDriver_5A212BB3(const void *mode);
int SceHdmiForDriver_7AD0F6C9(void *info, int count, void *mode);
int SceHdmiForDriver_99A3CABE(int enable);
