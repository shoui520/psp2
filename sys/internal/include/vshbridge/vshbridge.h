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
SceUInt32 vshSblQafMgrIsAllowFakeACInstall(void);
SceUInt32 vshSblQafMgrIsAllowShowTitleUpgradeInfo(void);
SceUInt32 vshSblUtMgrHasStoreFlag(void);
SceUInt32 vshSblUtMgrHasComTestFlag(void);
SceUInt32 vshPowerSetPowerSwMode(int mode);
SceUInt32 vshPowerSetStandbyButtonPushTime(int time);
SceUInt32 vshPowerGetWakeupFactor(void);
SceUInt32 vshSysconIsIduMode(void);
int vshSysconClearTemperatureLog(int kind);
/* through the stub library's wrapper, which passes the size's word (0x81417CF8 in SceShell) */
SceUInt32 vshSblSsGetNvsData(int area, void *buf, SceSize size);
/* through the stub library's wrapper, which passes the size's word (0x81417D0C in SceShell) */
SceUInt32 vshSblSsSetNvsData(int area, const void *buf, SceSize size);
struct SceMsInfo;
SceUInt32 vshMsifGetMsInfo(struct SceMsInfo *info);
SceUInt32 vshIdStorageLookup(SceSize key, SceSize offset, void *buf, SceSize len);
SceUInt32 vshRtcIsSecureAlarmed(void);
SceUInt32 vshRtcRegisterCallback(SceUID cb);
SceUInt32 vshRtcUnregisterCallback(SceUID cb);
/* through the stub library's wrapper (0x81417C78 in SceShell); NULL clears the alarm */
struct SceRtcTick;
SceUInt32 vshRtcSetAlarmTick(const struct SceRtcTick *tick, int flag);
/* through the stub library's wrapper (0x81417C70 in SceShell) */
int vshRtcGetCurrentSecureTick(struct SceRtcTick *tick);
int vshIoFlock(SceUID fd, int flags);
SceUInt32 vshRtcRegisterSecureAlarmCallback(SceUID cb);
SceUInt32 vshRtcUnregisterSecureAlarmCallback(SceUID cb);
SceUInt32 vshAppMgrCheckContentInstallPeriod(SceUID pid);
/* through the stub library's wrapper (0x81417D9C in SceShell): the 0x700-byte bgdl queue */
SceUInt32 vshAppMgrBgdlSetQueueStatus(const void *status);
SceUInt32 vshDisplaySetInvertColors(int head, int enable);
/* through the stub library's wrapper (0x81417DA4 in SceShell): a title's rif re-read */
int vshAppMgrUpdateRifInfo(const char *titleId);

/* the display's viewport, through the stub library's wrappers (they pass the conf's size) */
struct SceDisplayViewportConf;
int vshDisplaySetViewportConf(SceUInt32 head, SceUInt32 plane,
			      const struct SceDisplayViewportConf *conf);
int vshDisplayGetActualViewportConf(SceUInt32 head, SceUInt32 plane,
				    struct SceDisplayViewportConf *conf);

/* the touch emulation's touches of the front and the back panel, through the stub library's
   wrapper (0x81425E50 in SceShell) */
struct SceTouchData;
int vshTouchSetTouchEmulationData(const struct SceTouchData *front, const struct SceTouchData *back);

/* the controller of a port given another port, and the controllers' vibration on or off */
int vshCtrlChangePortAssign(SceInt8 port, SceInt8 newPort);
int vshCtrlSetVibrationSetting(int enable);

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
int _vshSblSsDecryptWithPortability(SceUInt32 mode, const void *iv, const void *in, void *out);
int _vshIoMount(int id, const char *path, int permission, const VshIoMountArgs *uargs);

#ifdef __cplusplus
}
#endif

#endif
