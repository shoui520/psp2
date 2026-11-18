/*
 * SceVshBridge's system calls a user program reaches (vshbridge's SceVshBridge library),
 * declared as vshbridge.c defines them: whether the console is a test, tool, development,
 * retail, Vita or PlayStation TV unit, its PS code, and whether it is in development mode. A
 * call with a leading underscore is reached through the stub library's wrapper of the same name
 * without it. The header's name is ours.
 */
#ifndef VSHBRIDGE_VSHBRIDGE_H
#define VSHBRIDGE_VSHBRIDGE_H

#include <scetypes.h>

#ifdef __cplusplus
extern "C" {
#endif

SceUInt32 vshSblAimgrIsTest(void);
SceUInt32 vshSblAimgrIsTool(void);
SceUInt32 vshSblAimgrIsDEX(void);
SceUInt32 vshSblAimgrIsCEX(void);
SceUInt32 vshSblAimgrIsVITA(void);
SceUInt32 vshSblAimgrIsDolce(void);
SceUInt32 vshSblSsIsDevelopmentMode(void);
SceUInt32 vshSblAimgrGetPscode(void *code);
SceUInt32 vshIoMount(int id, const char *path, int permission, void *data,
		     SceUInt32 reservedLow, SceUInt32 reservedHigh);
SceUInt32 vshIoUmount(int id, int force, SceUInt32 reservedLow, SceUInt32 reservedHigh);
SceUInt32 vshKernelSendSysEvent(int event);
SceUInt32 vshSblQafMgrIsAllowKeepCoreFile(void);
SceUInt32 vshSblUtMgrHasComTestFlag(void);
SceUInt32 vshPowerSetPowerSwMode(int mode);
SceUInt32 vshPowerSetStandbyButtonPushTime(int time);
SceUInt32 vshPowerGetWakeupFactor(void);
SceUInt32 vshSysconIsIduMode(void);
SceUInt32 vshIdStorageLookup(SceSize key, SceSize offset, void *buf, SceSize len);
SceUInt32 vshRtcIsSecureAlarmed(void);
SceUInt32 vshRtcRegisterSecureAlarmCallback(SceUID cb);
SceUInt32 vshRtcUnregisterSecureAlarmCallback(SceUID cb);
SceUInt32 vshAppMgrCheckContentInstallPeriod(SceUID pid);

/* sceIoMount's data and its 64-bit reserved word, in two halves, and the path's size */
typedef struct VshIoMountArgs {
	void *data;
	SceUInt32 reservedLow;
	SceUInt32 reservedHigh;
	SceSize pathSize;
	SceUInt32 reserved[2];
} __attribute__((aligned(8))) VshIoMountArgs;

/* the system calls behind the stub library's wrappers */
int _vshSblAimgrGetPscode(void *code);
int _vshIoMount(int id, const char *path, int permission, const VshIoMountArgs *uargs);

#ifdef __cplusplus
}
#endif

#endif
