/*
 * SceVshBridge (Sony's vshbridge.c): kernel services for the system
 * applications (SceShell and friends), as syscalls. Every bridge runs with
 * the syscall TPIDRURO convention and refuses callers that are not a system
 * program. The exports taking user pointers are wrapped in
 * vshbridge_user.c, which copies the data in and out.
 */
#include <psp2kern/types.h>
#include <stdio.h>
#include <string.h>
#include <sysmem/debug.h>
#include <sdif/sdif.h>
#include <msif/msif.h>
#include <intrmgr/syscall.h>
#include <vshbridge/vshbridge_imports.h>
#include "vshbridge.h"

#define SCE_SBL_ERROR_INVALID_ARGUMENT		0x800f0016
#define SCE_SBL_ERROR_NO_CAPABILITY		0x800f0902	/* name unknown */
#define SCE_SBL_ERROR_NOT_PRODUCT_MODE		0x800f0c29	/* name unknown */
#define SCE_SBL_ERROR_SPFSO_TOO_LARGE		0x800f1b16	/* name unknown */
#define SCE_SYSCON_ERROR_NOT_PRODUCT_MODE	0x80250005	/* name unknown */
#define SCE_HDMI_ERROR_NOT_PRODUCT_MODE		0x8037000e	/* name unknown */
#define SCE_LED_ERROR_INVALID_MODE		0x803f0141	/* name unknown */

/* a bridge: only a system program may call it */
#define VSH_BRIDGE(_call) do {					\
	SceUInt32 _t = sceSyscallEnter();				\
	int _ret;							\
									\
	if (!sceSblACMgrIsSystemProgram(0)) {				\
		sceSyscallLeave(_t);					\
		return SCE_ERROR_ERRNO_ENOSYS;				\
	}								\
	_ret = (_call);							\
	sceSyscallLeave(_t);						\
	return _ret;							\
} while (0)

/* a bridge on a kernel object the caller holds a PUID of: uid is its GUID */
#define VSH_BRIDGE_UID(_puid, _call) do {				\
	SceUInt32 _t = sceSyscallEnter();				\
	SceUID uid;							\
	int _ret;							\
									\
	if (!sceSblACMgrIsSystemProgram(0)) {				\
		sceSyscallLeave(_t);					\
		return SCE_ERROR_ERRNO_ENOSYS;				\
	}								\
	uid = SceSysmemForDriver_45D22597(0, (_puid));				\
	if (uid < 0) {							\
		sceSyscallLeave(_t);					\
		return uid | 0x40000000;				\
	}								\
	_ret = (_call);							\
	sceSyscallLeave(_t);						\
	return _ret;							\
} while (0)

/* a bridge for the factory (product mode) only */
#define VSH_BRIDGE_PRODUCT_MODE(_error, _call) do {			\
	SceUInt32 _t = sceSyscallEnter();				\
	SceProductMode _mode = 0;					\
	int _ret;							\
									\
	if (!sceSblACMgrIsSystemProgram(0)) {				\
		_ret = SCE_ERROR_ERRNO_ENOSYS;				\
	} else if (ScePmMgrForDriver_2AC815A2(&_mode) != 0) {		\
		_mode = 0;						\
		_ret = (_error);					\
	} else {							\
		_ret = _mode ? (_call) : (int)(_error);			\
	}								\
	sceSyscallLeave(_t);						\
	return _ret;							\
} while (0)

static int vshShellReady;

/* ===== kernel ===== */

SceUInt32 vshKernelShutdownSystemForUser(const SceKernelShutdownParam *param)
{
	SceUInt32 t = sceSyscallEnter();
	int perm, ret;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(t);
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	perm = sceKernelSetThreadAccessLevel(0x80);
	if (param != NULL) {
		if (param->size != 4 && param->size != 8) {
			sceKernelSetThreadAccessLevel(perm);
			sceSyscallLeave(t);
			return SCE_ERROR_ERRNO_EINVAL;
		}
		SCE_PRINTF_WITH_INFO(0, 344, "Shutting the system down...\n");
		if (param->size == 8 && param->mode == 1)
			ret = kscePowerRequestShutdown(1);
		else if (param->size == 8 && param->mode == 2)
			ret = kscePowerRequestShutdown(0);
		else
			ret = scePowerRequestColdReset();
	} else {
		SCE_PRINTF_WITH_INFO(0, 344, "Shutting the system down...\n");
		ret = scePowerRequestColdReset();
	}
	if (ret != 0)
		sceKernelExitDeleteThread(0);
	sceKernelSetThreadAccessLevel(perm);
	sceSyscallLeave(t);
	return 0;
}

SceUInt32 vshKernelSendSysEventForUser(int event)
{
	SceUInt32 t = sceSyscallEnter();

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(t);
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	if (event != 0) {
		sceSyscallLeave(t);
		return SCE_ERROR_ERRNO_EINVAL;
	}
	if (!vshShellReady) {
		SCE_PRINTF_WITH_INFO(0, 392, "vshbridge : shell ready\n");
		SceSysrootForDriver_47724459(0);
		vshShellReady = 1;
	}
	sceSyscallLeave(t);
	return 0;
}

SceUInt32 vshKernelGetCompiledSdkVersionByPid(SceUID pid, SceUInt32 *version)
{
	SceUInt32 t = sceSyscallEnter();
	SceUInt32 v = 0;
	int ret;

	if (!sceSblACMgrIsSystemProgram(0)) {
		ret = SCE_ERROR_ERRNO_ENOSYS;
	} else {
		ret = SceProcessmgrForDriver_D141C076(pid, &v);
		if (ret >= 0)
			ret = SceSysmemForDriver_6D88EF8A(version, &v, sizeof(v));
	}
	sceSyscallLeave(t);
	return ret;
}

SceUInt32 vshKernelCheckModelCapability(int bit)
{
	SceUInt32 t = sceSyscallEnter();
	int ret;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(t);
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	ret = SceSysrootForDriver_8AA268D6(bit) != 0;
	sceSyscallLeave(t);
	return ret;
}

int vshKernelEnableAutoClockDown(void)
{
	SceUInt32 t = sceSyscallEnter();

	if (sceSblACMgrIsSystemProgram(0)) {
		sceKernelSysrootEnableAutoClockDown();
		sceSyscallLeave(t);
		return 0;
	}
	sceSyscallLeave(t);
	return SCE_ERROR_ERRNO_ENOSYS;
}

int vshKernelDisableAutoClockDown(void)
{
	SceUInt32 t = sceSyscallEnter();

	if (sceSblACMgrIsSystemProgram(0)) {
		sceKernelSysrootDisableAutoClockDown();
		sceSyscallLeave(t);
		return 0;
	}
	sceSyscallLeave(t);
	return SCE_ERROR_ERRNO_ENOSYS;
}

/* ===== rtc ===== */

SceUInt32 vshRtcSetConf(void *a, void *b, void *c, void *d)
{
	VSH_BRIDGE(ksceRtcSetConf(a, b, c, d));
}

SceUInt32 vshRtcSetCurrentTick(const SceRtcTick *tick)
{
	VSH_BRIDGE(ksceRtcSetCurrentTick(tick));
}

SceUInt32 vshRtcSetCurrentSecureTick(const SceRtcTick *tick)
{
	VSH_BRIDGE(ksceRtcSetCurrentSecureTick(tick));
}

SceUInt32 vshRtcGetCurrentSecureTick(SceRtcTick *tick)
{
	VSH_BRIDGE(ksceRtcGetCurrentSecureTick(tick));
}

SceUInt32 vshRtcSetCurrentNetworkTick(const SceRtcTick *tick)
{
	VSH_BRIDGE(ksceRtcSetCurrentNetworkTick(tick));
}

SceUInt32 vshRtcSetCurrentDebugNetworkTick(const SceRtcTick *tick)
{
	VSH_BRIDGE(ksceRtcSetCurrentDebugNetworkTick(tick));
}

SceUInt32 vshRtcSetAlarmTick(const SceRtcTick *tick, int unk)
{
	VSH_BRIDGE(ksceRtcSetAlarmTick(tick, unk));
}

SceUInt32 vshRtcGetAlarmTick(SceRtcTick *tick, int *unk)
{
	VSH_BRIDGE(ksceRtcGetAlarmTick(tick, unk));
}

SceUInt32 vshRtcIsAlarmed(void)
{
	VSH_BRIDGE(ksceRtcIsAlarmed());
}

SceUInt32 vshRtcRegisterCallback(SceUID cb)
{
	VSH_BRIDGE_UID(cb, ksceRtcRegisterCallback(uid));
}

SceUInt32 vshRtcUnregisterCallback(SceUID cb)
{
	VSH_BRIDGE_UID(cb, ksceRtcUnregisterCallback(uid));
}

SceUInt32 vshRtcSetSecureAlarmTick(const SceRtcTick *tick)
{
	VSH_BRIDGE(ksceRtcSetSecureAlarmTick(tick));
}

SceUInt32 vshRtcGetSecureAlarmTick(SceRtcTick *tick)
{
	VSH_BRIDGE(ksceRtcGetSecureAlarmTick(tick));
}

SceUInt32 vshRtcIsSecureAlarmed(void)
{
	VSH_BRIDGE(ksceRtcIsSecureAlarmed());
}

SceUInt32 vshRtcRegisterSecureAlarmCallback(SceUID cb)
{
	VSH_BRIDGE_UID(cb, ksceRtcRegisterSecureAlarmCallback(uid));
}

SceUInt32 vshRtcUnregisterSecureAlarmCallback(SceUID cb)
{
	VSH_BRIDGE_UID(cb, ksceRtcUnregisterSecureAlarmCallback(uid));
}

/* ===== display, ctrl, motion, touch, led ===== */

SceUInt32 vshDisplaySetInvertColors(int head, int enable)
{
	VSH_BRIDGE(ksceDisplaySetInvertColors(head, enable));
}

SceUInt32 vshDisplaySetViewportConf(int head, const void *conf, int unk)
{
	VSH_BRIDGE(ksceDisplaySetViewportConf(head, conf, unk));
}

SceUInt32 vshDisplayGetActualViewportConf(int head, void *conf, int unk)
{
	VSH_BRIDGE(ksceDisplayGetActualViewportConf(head, conf, unk));
}

SceUInt32 vshDisplayRegisterFrameBufCallback(SceUID cb)
{
	VSH_BRIDGE_UID(cb, ksceDisplayRegisterFrameBufCallback(uid));
}

SceUInt32 vshCtrlSetIdleCancelKey(int a, int b, int c, int d)
{
	VSH_BRIDGE(ksceCtrlSetIdleCancelKey(a, b, c, d));
}

SceUInt32 vshCtrlSetIdleCancelThreshold(int a, int b)
{
	VSH_BRIDGE(ksceCtrlSetIdleCancelThreshold(a, b));
}

SceUInt32 vshCtrlSetVibrationSetting(int setting)
{
	VSH_BRIDGE(ksceCtrlSetVibrationSetting(setting));
}

SceUInt32 vshCtrlSetTurnOffInterval(int interval)
{
	VSH_BRIDGE(ksceCtrlSetTurnOffInterval(interval));
}

SceUInt32 vshCtrlGetActiveControllerPort(void)
{
	VSH_BRIDGE(ksceCtrlGetActiveControllerPort());
}

SceUInt32 vshCtrlIsBdRemoconConnected(void)
{
	VSH_BRIDGE(ksceCtrlIsBdRemoconConnected());
}

SceUInt32 vshCtrlChangePortAssign(int a, int b)
{
	VSH_BRIDGE(ksceCtrlChangePortAssign(a, b));
}

SceUInt32 vshCtrlRegisterNotifyCallBack(SceUID cb)
{
	VSH_BRIDGE_UID(cb, ksceCtrlRegisterNotifyCallBack(uid));
}

SceUInt32 vshCtrlUnregisterNotifyCallBack(SceUID cb)
{
	VSH_BRIDGE_UID(cb, ksceCtrlUnregisterNotifyCallBack(uid));
}

SceUInt32 vshMotionNoiseFilterIsAvailable(void)
{
	return ksceMotionDevNoiseFilterIsAvailable();
}

SceUInt32 vshTouchSetTouchEmulationData(const void *front, const void *back)
{
	SceUInt32 t = sceSyscallEnter();
	int ret;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(t);
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	if (back == NULL && front == NULL) {
		sceSyscallLeave(t);
		return 0;
	}
	ret = ksceTouchSetTouchEmulationData(front, back);
	sceSyscallLeave(t);
	return ret;
}

/* LED 2 on early boards (hardware info below 0x800000) is driven in mode 1 */
SceUInt32 vshLedSetMode(int led, int mode, const void *conf)
{
	static const SceUInt32 vshLedDefaultConf[4] = { 8, 9, 0, 0 };
	SceUInt32 t = sceSyscallEnter();
	int ret;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(t);
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	if (mode == 3) {
		sceSyscallLeave(t);
		return SCE_LED_ERROR_INVALID_MODE;
	}
	if (mode == 0 && led == 2) {
		if ((ksceSysconGetHardwareInfo() & 0xff0000) < 0x800000) {
			mode = 1;
			conf = vshLedDefaultConf;
		} else {
			mode = 0;
		}
	}
	ret = ksceLedSetMode(led, mode, conf);
	sceSyscallLeave(t);
	return ret;
}

/* ===== power ===== */

SceUInt32 vshPowerRequestColdReset(void)
{
	VSH_BRIDGE(scePowerRequestColdReset());
}

SceUInt32 vshPowerSetPowerSwMode(int mode)
{
	VSH_BRIDGE(kscePowerSetPowerSwMode(mode));
}

SceUInt32 vshPowerSetStandbyButtonPushTime(int time)
{
	VSH_BRIDGE(kscePowerSetStandbyButtonPushTime(time));
}

SceUInt32 vshPowerSetPsButtonPushTime(int time)
{
	VSH_BRIDGE(kscePowerSetPsButtonPushTime(time));
}

SceUInt32 vshPowerGetWakeupFactor(void)
{
	VSH_BRIDGE(kscePowerGetWakeupFactor());
}

SceUInt32 vshPowerSetBatteryFakeStatus(const void *status)
{
	VSH_BRIDGE(kscePowerSetBatteryFakeStatus(status));
}

SceUInt32 vshPowerRequestHibernate(void)
{
	VSH_BRIDGE(kscePowerRequestHibernate());
}

/* ===== syscon ===== */

SceUInt32 vshSysconGetManufacturesStatus(int *status)
{
	VSH_BRIDGE_PRODUCT_MODE(SCE_SYSCON_ERROR_NOT_PRODUCT_MODE, ksceSysconGetManufacturesStatus(status));
}

SceUInt32 vshSysconGetManualChargeMode(int *mode)
{
	VSH_BRIDGE_PRODUCT_MODE(SCE_SYSCON_ERROR_NOT_PRODUCT_MODE, ksceSysconGetManualChargeMode(mode));
}

SceUInt32 vshSysconCtrlManualChargeMode(int mode)
{
	VSH_BRIDGE_PRODUCT_MODE(SCE_SYSCON_ERROR_NOT_PRODUCT_MODE, ksceSysconCtrlManualChargeMode(mode));
}

SceUInt32 vshSysconEnableHibernateIO(int enable)
{
	VSH_BRIDGE(ksceSysconEnableHibernateIO(enable));
}

SceUInt32 vshSysconGetLogInfo(void *info)
{
	SceUInt32 t = sceSyscallEnter();
	SceProductMode mode = 0;
	SceUInt8 buf[8];
	int ret;

	if (!sceSblACMgrIsSystemProgram(0)) {
		ret = SCE_ERROR_ERRNO_ENOSYS;
	} else if (info == NULL) {
		ret = SCE_ERROR_ERRNO_EINVAL;
	} else if (ScePmMgrForDriver_2AC815A2(&mode) != 0) {
		mode = 0;
		ret = SCE_SYSCON_ERROR_NOT_PRODUCT_MODE;
	} else if (!mode) {
		ret = SCE_SYSCON_ERROR_NOT_PRODUCT_MODE;
	} else {
		ret = ksceSysconGetLogInfo(buf);
		if (ret >= 0)
			ret = SceSysmemForDriver_6D88EF8A(info, buf, sizeof(buf));
	}
	sceSyscallLeave(t);
	return ret;
}

SceUInt32 vshSysconLogStart(void)
{
	VSH_BRIDGE_PRODUCT_MODE(SCE_SYSCON_ERROR_NOT_PRODUCT_MODE, ksceSysconLogStart());
}

SceUInt32 vshSysconLogReadData(int a, void *buf, SceSize size)
{
	VSH_BRIDGE(ksceSysconLogReadData(a, buf, size));
}

SceUInt32 vshSysconLogStartWaiting(void)
{
	VSH_BRIDGE_PRODUCT_MODE(SCE_SYSCON_ERROR_NOT_PRODUCT_MODE, ksceSysconLogStartWaiting());
}

/* a VITA has no temperature log: it reads as three empty entries */
SceUInt32 vshSysconGetTemperatureLog(SceUInt8 *log)
{
	SceUInt32 t = sceSyscallEnter();
	int ret;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(t);
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	if (!SceSblAIMgrForDriver_C6E83F34()) {
		memset(log, 0, 0x10);
		log[0] = 1;
		log[4] = 1;
		log[8] = 1;
		sceSyscallLeave(t);
		return 0;
	}
	ret = ksceSysconGetTemperatureLog(log);
	sceSyscallLeave(t);
	return ret;
}

SceUInt32 vshSysconClearTemperatureLog(int unk)
{
	SceUInt32 t = sceSyscallEnter();
	int ret;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(t);
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	if (!SceSblAIMgrForDriver_C6E83F34()) {
		sceSyscallLeave(t);
		return 0;
	}
	ret = ksceSysconClearTemperatureLog(unk);
	sceSyscallLeave(t);
	return ret;
}

/* hardware info bits: a macro, as the bridges (an inline function's own early
 * return would be predicted before inlining and move the error path) */
#define VSH_HARDWARE_INFO_BIT(_shift) do {				\
	SceUInt32 _t = sceSyscallEnter();				\
	SceUInt32 _ret;							\
									\
	if (!sceSblACMgrIsSystemProgram(0)) {				\
		sceSyscallLeave(_t);					\
		return SCE_ERROR_ERRNO_ENOSYS;				\
	}								\
	_ret = ((SceUInt32)ksceSysconGetHardwareInfo() >> (_shift)) & 1; \
	sceSyscallLeave(_t);						\
	return _ret;							\
} while (0)

SceUInt32 vshSysconHasWWAN(void)
{
	VSH_HARDWARE_INFO_BIT(1);
}

SceUInt32 vshSysconIduModeSet(void)
{
	VSH_BRIDGE(ksceSysconIduModeSet());
}

SceUInt32 vshSysconIduModeClear(void)
{
	VSH_BRIDGE(ksceSysconIduModeClear());
}

SceUInt32 vshSysconIsIduMode(void)
{
	VSH_HARDWARE_INFO_BIT(7);
}

SceUInt32 vshSysconShowModeSet(void)
{
	VSH_BRIDGE(ksceSysconShowModeSet());
}

SceUInt32 vshSysconShowModeClear(void)
{
	VSH_BRIDGE(ksceSysconShowModeClear());
}

SceUInt32 vshSysconIsShowMode(void)
{
	VSH_HARDWARE_INFO_BIT(6);
}

SceUInt32 vshSysconIsMCEmuCapable(void)
{
	VSH_HARDWARE_INFO_BIT(4);
}

SceUInt32 vshSysconIsDownLoaderMode(void)
{
	VSH_BRIDGE(ksceSysconIsDownLoaderMode());
}

SceUInt32 vshSysconGetUsbDetStatus(void *status)
{
	SceUInt32 t = sceSyscallEnter();
	int ret;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(t);
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	if (status == NULL) {
		sceSyscallLeave(t);
		return SCE_ERROR_ERRNO_EINVAL;
	}
	ret = ksceSysconGetUsbDetStatus(status);
	sceSyscallLeave(t);
	return ret;
}

SceUInt32 vshSysconGetHardwareInfo(SceUInt32 *info)
{
	SceUInt32 t = sceSyscallEnter();

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(t);
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	if (info == NULL) {
		sceSyscallLeave(t);
		return SCE_ERROR_ERRNO_EINVAL;
	}
	*info = ksceSysconGetHardwareInfo();
	sceSyscallLeave(t);
	return 0;
}

/* boards without the second word (hardware info bit 5) read zeros */
SceUInt32 vshSysconGetHardwareInfo2(void *info)
{
	SceUInt32 t = sceSyscallEnter();
	int ret;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(t);
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	if (info == NULL) {
		sceSyscallLeave(t);
		return SCE_ERROR_ERRNO_EINVAL;
	}
	if (!(ksceSysconGetHardwareInfo() & 0x20)) {
		memset(info, 0, 0x10);
		sceSyscallLeave(t);
		return 0;
	}
	ret = ksceSysconGetHardwareInfo2(info);
	sceSyscallLeave(t);
	return ret;
}

SceUInt32 vshSysconBeginConfigstorageTransaction(void)
{
	VSH_BRIDGE(ksceSysconBeginConfigstorageTransaction());
}

SceUInt32 vshSysconEndConfigstorageTransaction(void)
{
	VSH_BRIDGE(ksceSysconEndConfigstorageTransaction());
}

SceUInt32 vshSysconCommitConfigstorageTransaction(void)
{
	VSH_BRIDGE(ksceSysconCommitConfigstorageTransaction());
}

/* a configstorage script of up to 32 bytes, copied in (no system program check) */
int vshSysconLoadConfigstorageScript(int a, const void *script, SceSize size)
{
	SceUInt32 t = sceSyscallEnter();
	SceUInt8 buf[0x20];
	int ret;

	if (size > sizeof(buf)) {
		ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	} else {
		ret = SceSysmemForDriver_BC996A7A(buf, script, size);
		if (ret >= 0)
			ret = ksceSysconLoadConfigstorageScript(a, buf, size);
	}
	sceSyscallLeave(t);
	return ret;
}

int vshSysconVerifyConfigstorageScript(int a, const void *script, SceSize size)
{
	SceUInt32 t = sceSyscallEnter();
	SceUInt8 buf[0x20];
	int ret;

	if (size > sizeof(buf)) {
		ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	} else {
		ret = SceSysmemForDriver_BC996A7A(buf, script, size);
		if (ret >= 0)
			ret = ksceSysconVerifyConfigstorageScript(a, buf, size);
	}
	sceSyscallLeave(t);
	return ret;
}

SceUInt32 vshSysconGetBatteryCalibData(int *a, int *b, int *c, int *d)
{
	SceUInt32 t = sceSyscallEnter();
	int va = 0, vb = 0, vc = 0, vd = 0;
	int ret;

	if (!sceSblACMgrIsSystemProgram(0)) {
		ret = SCE_ERROR_ERRNO_ENOSYS;
	} else {
		ret = ksceSysconGetBatteryCalibData(&va, &vb, &vc, &vd);
		if (ret >= 0 && (ret = SceSysmemForDriver_6D88EF8A(a, &va, 4)) >= 0 &&
		    (ret = SceSysmemForDriver_6D88EF8A(b, &vb, 4)) >= 0 &&
		    (ret = SceSysmemForDriver_6D88EF8A(c, &vc, 4)) >= 0)
			ret = SceSysmemForDriver_6D88EF8A(d, &vd, 4);
	}
	sceSyscallLeave(t);
	return ret;
}

/* ===== idstorage ===== */

SceUInt32 vshIdStorageGetLeafSize(void)
{
	VSH_BRIDGE(ksceIdStorageGetLeafSize());
}

SceUInt32 vshIdStorageIsFormatted(void)
{
	VSH_BRIDGE(ksceIdStorageIsFormatted());
}

SceUInt32 vshIdStorageIsReadOnly(void)
{
	VSH_BRIDGE(ksceIdStorageIsReadOnly());
}

SceUInt32 vshIdStorageIsDirty(void)
{
	VSH_BRIDGE(ksceIdStorageIsDirty());
}

SceUInt32 vshIdStorageFormat(void)
{
	VSH_BRIDGE(ksceIdStorageFormat());
}

SceUInt32 vshIdStorageUnformat(void)
{
	VSH_BRIDGE(ksceIdStorageUnformat());
}

SceUInt32 vshIdStorageReadLeaf(SceSize leaf, void *buf)
{
	SceUInt32 t = sceSyscallEnter();
	SceUInt8 data[0x200];
	int ret;

	if (!sceSblACMgrIsSystemProgram(0)) {
		ret = SCE_ERROR_ERRNO_ENOSYS;
	} else {
		ret = ksceIdStorageReadLeaf(leaf, data);
		if (ret == 0)
			ret = SceSysmemForDriver_6D88EF8A(buf, data, sizeof(data));
	}
	sceSyscallLeave(t);
	return ret;
}

SceUInt32 vshIdStorageWriteLeaf(SceSize leaf, const void *buf)
{
	SceUInt32 t = sceSyscallEnter();
	SceUInt8 data[0x200];
	int ret;

	if (!sceSblACMgrIsSystemProgram(0)) {
		ret = SCE_ERROR_ERRNO_ENOSYS;
	} else {
		ret = SceSysmemForDriver_BC996A7A(data, buf, sizeof(data));
		if (ret >= 0)
			ret = ksceIdStorageWriteLeaf(leaf, data);
	}
	sceSyscallLeave(t);
	return ret;
}

SceUInt32 vshIdStorageCreateLeaf(SceSize leaf)
{
	VSH_BRIDGE(ksceIdStorageCreateLeaf(leaf));
}

/* up to 32 leaves */
SceUInt32 vshIdStorageCreateAtomicLeaves(const SceUInt16 *leaves, int num)
{
	SceUInt32 t = sceSyscallEnter();
	SceUInt16 buf[0x20];
	int ret;

	if (!sceSblACMgrIsSystemProgram(0)) {
		ret = SCE_ERROR_ERRNO_ENOSYS;
	} else if ((SceUInt32)(num - 1) < 0x20) {
		ret = SceSysmemForDriver_BC996A7A(buf, leaves, num * 2);
		if (ret >= 0)
			ret = ksceIdStorageCreateAtomicLeaves(buf, num);
	} else {
		ret = SCE_ERROR_ERRNO_EINVAL;
	}
	sceSyscallLeave(t);
	return ret;
}

SceUInt32 vshIdStorageDeleteLeaf(SceSize leaf)
{
	VSH_BRIDGE(ksceIdStorageDeleteLeaf(leaf));
}

SceUInt32 vshIdStorageGetFreeLeaves(void)
{
	VSH_BRIDGE(ksceIdStorageGetFreeLeaves());
}

SceUInt32 vshIdStorageLookup(SceSize key, SceSize offset, void *buf, SceSize len)
{
	SceUInt32 t = sceSyscallEnter();
	SceUInt8 data[0x200];
	int ret;

	if (!sceSblACMgrIsSystemProgram(0)) {
		ret = SCE_ERROR_ERRNO_ENOSYS;
	} else if ((len <= 0x200 ? offset : len) < 0x200) {
		ret = ksceIdStorageLookup(key, offset, data, len);
		if (ret == 0)
			ret = SceSysmemForDriver_6D88EF8A(buf, data, len);
	} else {
		ret = SCE_ERROR_ERRNO_EINVAL;
	}
	sceSyscallLeave(t);
	return ret;
}

SceUInt32 vshIdStorageUpdate(SceSize key, SceSize offset, const void *buf, SceSize len)
{
	SceUInt32 t = sceSyscallEnter();
	SceUInt8 data[0x200];
	int ret;

	if (!sceSblACMgrIsSystemProgram(0)) {
		ret = SCE_ERROR_ERRNO_ENOSYS;
	} else if ((len <= 0x200 ? offset : len) < 0x200) {
		ret = SceSysmemForDriver_BC996A7A(data, buf, len);
		if (ret >= 0)
			ret = ksceIdStorageUpdate(key, offset, data, len);
	} else {
		ret = SCE_ERROR_ERRNO_EINVAL;
	}
	sceSyscallLeave(t);
	return ret;
}

SceUInt32 vshIdStorageFlush(void)
{
	VSH_BRIDGE(ksceIdStorageFlush());
}

SceUInt32 vshIdStorageRestart(int unk)
{
	VSH_BRIDGE(ksceIdStorageRestart(unk));
}

/* ===== sbl ===== */

SceUInt32 vshSblAimgrIsTest(void)
{
	VSH_BRIDGE(SceSblAIMgrForDriver_3B638885());
}

SceUInt32 vshSblAimgrIsTool(void)
{
	VSH_BRIDGE(SceSblAIMgrForDriver_274663A0());
}

SceUInt32 vshSblAimgrIsDEX(void)
{
	VSH_BRIDGE(SceSblAIMgrForDriver_F4B98F66());
}

SceUInt32 vshSblAimgrIsCEX(void)
{
	VSH_BRIDGE(SceSblAIMgrForDriver_D78B04A2());
}

SceUInt32 vshSblAimgrIsVITA(void)
{
	VSH_BRIDGE(SceSblAIMgrForDriver_4273B97B());
}

SceUInt32 vshSblAimgrIsDolce(void)
{
	VSH_BRIDGE(SceSblAIMgrForDriver_71608CA3());
}

SceUInt32 vshSblAimgrIsGenuineVITA(void)
{
	VSH_BRIDGE(SceSblAIMgrForDriver_963CA644());
}

SceUInt32 vshSblAimgrIsGenuineDolce(void)
{
	VSH_BRIDGE(SceSblAIMgrForDriver_C6E83F34());
}

SceUInt32 vshSblAimgrGetConsoleId(void *id)
{
	VSH_BRIDGE(ksceSblAimgrGetConsoleId(id));
}

SceUInt32 vshSblAimgrGetPscode(void *code)
{
	VSH_BRIDGE(ksceSblAimgrGetPscode(code));
}

SceUInt32 vshSblAimgrGetPscode2(void *code)
{
	VSH_BRIDGE(ksceSblAimgrGetPscode2(code));
}

SceUInt32 vshSblAimgrGetVisibleId(void *id)
{
	VSH_BRIDGE_PRODUCT_MODE(SCE_SBL_ERROR_NOT_PRODUCT_MODE, ksceSblAimgrGetVisibleId(id));
}

SceUInt32 vshSblSsGetNvsData(SceUInt32 offset, void *buf, SceSize size)
{
	SceUInt32 t = sceSyscallEnter();
	int ret;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(t);
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	ret = ksceSblSsGetNvsData(offset, buf, size);
	sceSyscallLeave(t);
	return ret == 0 ? 0 : ret;
}

SceUInt32 vshSblSsSetNvsData(SceUInt32 offset, const void *buf, SceSize size)
{
	SceUInt32 t = sceSyscallEnter();
	int ret;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(t);
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	ret = ksceSblSsSetNvsData(offset, buf, size);
	sceSyscallLeave(t);
	return ret == 0 ? 0 : ret;
}

/* portability mode 0-20; the output size starts as the input size */
SceUInt32 vshSblSsEncryptWithPortability(SceUInt32 mode, const void *iv, const SceSize *inSize,
				   SceSize *outSize)
{
	SceUInt32 t = sceSyscallEnter();
	int ret;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(t);
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	if (mode >= 0x15 || iv == NULL || inSize == NULL || outSize == NULL) {
		sceSyscallLeave(t);
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	}
	*outSize = *inSize;
	ret = ksceSblSsEncryptWithPortability(mode, iv, inSize, outSize);
	sceSyscallLeave(t);
	return ret;
}

SceUInt32 vshSblSsDecryptWithPortability(SceUInt32 mode, const void *iv, const SceSize *inSize,
				   SceSize *outSize)
{
	SceUInt32 t = sceSyscallEnter();
	int ret;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(t);
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	if (mode >= 0x15 || iv == NULL || outSize == NULL || inSize == NULL) {
		sceSyscallLeave(t);
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	}
	*outSize = *inSize;
	ret = ksceSblSsDecryptWithPortability(mode, iv, inSize, outSize);
	sceSyscallLeave(t);
	return ret;
}

SceUInt32 vshSblQafMgrIsAllowNearTest(void)
{
	VSH_BRIDGE(SceQafMgrForDriver_9644171D());
}

SceUInt32 vshSblQafMgrIsAllowSystemAppDebug(void)
{
	VSH_BRIDGE(SceQafMgrForDriver_CAD47130());
}

SceUInt32 vshSblQafMgrIsAllowMarlinTest(void)
{
	VSH_BRIDGE(SceQafMgrForDriver_10283EB8());
}

SceUInt32 vshSblQafMgrIsAllowLoadMagicGate(void)
{
	VSH_BRIDGE(SceQafMgrForDriver_36E5312E());
}

/* these QA flags all ask SceQafMgr's one check (name unknown) */
SceUInt32 vshSblQafMgrIsAllowLoadPSPEmuWhiteList(void)
{
	VSH_BRIDGE(SceQafMgrForDriver_4BC1883F());
}

SceUInt32 vshSblQafMgrIsAllowFakeACInstall(void)
{
	VSH_BRIDGE(SceQafMgrForDriver_4BC1883F());
}

SceUInt32 vshSblQafMgrIsAllowShowTitleUpgradeInfo(void)
{
	VSH_BRIDGE(SceQafMgrForDriver_4BC1883F());
}

SceUInt32 vshSblQafMgrIsAllowKeepCoreFile(void)
{
	VSH_BRIDGE(SceQafMgrForDriver_C1EA75C8());
}

SceUInt32 vshSblQafMgrIsAllowDtcpIpReset(void)
{
	VSH_BRIDGE(SceQafMgrForDriver_E8B8F31F());
}

SceUInt32 vshSblQafMgrIsAllowControlIduAutoUpdate(void)
{
	VSH_BRIDGE(SceQafMgrForDriver_F8BFEE48());
}

SceUInt32 vshSblQafMgrIsAllowPSPEmuShowQAInfo(void)
{
	VSH_BRIDGE(SceQafMgrForDriver_B7B195B2());
}

SceUInt32 vshSblQafMgrIsAllowPSPEmuSelectNPEnv(void)
{
	VSH_BRIDGE(SceQafMgrForDriver_4BC1883F());
}

SceUInt32 vshSblQafMgrIsAllowPSPEmuDeleteParameter(void)
{
	VSH_BRIDGE(SceQafMgrForDriver_4BC1883F());
}

SceUInt32 vshSblQafMgrIsAllowPSPEmuScreenShot(void)
{
	VSH_BRIDGE(SceQafMgrForDriver_4BC1883F());
}

SceUInt32 vshSblQafMgrIsAllowLiblocationChangeModel(void)
{
	VSH_BRIDGE(SceQafMgrForDriver_4BC1883F());
}

SceUInt32 vshSblQafMgrIsAllowRemotePlayDebug(void)
{
	VSH_BRIDGE(SceQafMgrForDriver_BFD5E463());
}

SceUInt32 vshSblPmMgrSetSdModeOff(void)
{
	VSH_BRIDGE(_ksceSblPostSsMgrExecutePmSmF00dCommand(0));
}

SceUInt32 vshSblUtMgrHasComTestFlag(void)
{
	VSH_BRIDGE(ksceSblUtMgrHasComTestFlag() != 0);
}

SceUInt32 vshSblUtMgrHasStoreFlag(void)
{
	VSH_BRIDGE(ksceSblUtMgrHasStoreFlag() != 0);
}

SceUInt32 vshSblUtMgrHasNpTestFlag(void)
{
	VSH_BRIDGE(ksceSblUtMgrHasNpTestFlag() != 0);
}

/* returns whatever the development mode check left in r0 (Sony's code) */
SceUInt32 vshSblSsIsDevelopmentMode(void)
{
	SceUInt32 t = sceSyscallEnter();
	int ret = 0;

	if (sceSblACMgrIsSystemProgram(0))
		ret = SceSblACMgrForDriver_E87D1777();
	sceSyscallLeave(t);
	return ret;
}

SceUInt32 vshSblACMgrHasCapability(SceUID pid, int capability)
{
	VSH_BRIDGE(sceSblACMgrHasCapability(pid, capability) == 1 ? 0 : (int)SCE_SBL_ERROR_NO_CAPABILITY);
}

SceUInt32 vshSblGcAuthMgrMlnpsnlAuth1(void *a, void *b, void *c, void *d)
{
	VSH_BRIDGE(SceSblGcAuthMgrMlnpsnlForDriver_DABC01F5(a, b, c, d));
}

SceUInt32 vshSblGcAuthMgrMlnpsnlAuth2(void *a, void *b, void *c)
{
	VSH_BRIDGE(SceSblGcAuthMgrMlnpsnlForDriver_296ABD68(a, b, c));
}

/* ===== start ===== */

int module_start(SceSize args, void *argp)
{
	int (*getKeyInt)(const char *, const char *, int *) = sceRegMgrGetKeyInt;
	int value, vibration = 1, interval = 0;

	(void)args;
	(void)argp;

	if (getKeyInt("/CONFIG/USB", "usb_charge_enable", &value) >= 0) {
		if (SceSysrootForDriver_834439A7() || SceSysrootForDriver_B0E1FC67())
			value = 0;
		SceUdcdForDriver_2D91F8C3(value);
	}
	if (getKeyInt("/CONFIG/SYSTEM/", "controller_vibration", &vibration) >= 0) {
		ksceCtrlSetVibrationSetting(vibration);
		if (getKeyInt("/CONFIG/POWER_SAVING/", "controller_off_interval", &interval) >= 0)
			ksceCtrlSetTurnOffInterval(interval);
	}
	return 0;
}

/* ===== io ===== */

SceUInt32 vshIoMount(int id, const char *path, int permission, void *data,
		     SceUInt32 reservedLow, SceUInt32 reservedHigh)
{
	VSH_BRIDGE(SceIofilemgrForDriver_D070BC48(id, path, permission, data,
						  (SceUInt64)reservedHigh << 32 | reservedLow));
}

SceUInt32 vshIoUmount(int id, int force, SceUInt32 reservedLow, SceUInt32 reservedHigh)
{
	VSH_BRIDGE(SceIofilemgrForDriver_20574100(id, force, (SceUInt64)reservedHigh << 32 | reservedLow));
}

/* a kernel event, handed to the caller as a PUID */
SceUInt32 vshIoCreateMountEvent(SceUInt32 mountPoint, SceUInt32 events)
{
	SceUInt32 t = sceSyscallEnter();
	SceUID uid, puid;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(t);
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	uid = sceIoCreateMountEvent(mountPoint, events);
	if (uid < 0) {
		sceSyscallLeave(t);
		return uid;
	}
	puid = SceSysmemForDriver_BF209859(0, uid);
	if (puid < 0)
		sceIoDeleteMountEvent(uid);
	sceSyscallLeave(t);
	return puid;
}

SceUInt32 vshIoDeleteMountEvent(SceUID event)
{
	VSH_BRIDGE_UID(event, sceIoDeleteMountEvent(uid));
}

SceUInt32 vshIoFlock(SceUID fd, int flags)
{
	VSH_BRIDGE_UID(fd, SceIofilemgrForDriver_16336A0D(uid, flags));
}

SceUInt32 vshIoCreateErrorEvent(int mntId, SceUInt32 pattern, IoErrorEventInfo *info)
{
	SceUInt32 t = sceSyscallEnter();
	SceUID uid, puid;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(t);
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	uid = sceIoCreateErrorEvent(mntId, pattern, info);
	if (uid < 0) {
		sceSyscallLeave(t);
		return uid;
	}
	puid = SceSysmemForDriver_BF209859(0, uid);
	if (puid < 0)
		SceIofilemgrForDriver_C6158F8D(uid);
	sceSyscallLeave(t);
	return puid;
}

SceUInt32 vshIoClearErrorEvent(SceUID event)
{
	VSH_BRIDGE_UID(event, SceIofilemgrForDriver_40B933C7(uid));
}

SceUInt32 vshIoDeleteErrorEvent(SceUID event)
{
	SceUInt32 t = sceSyscallEnter();
	SceUID uid;
	int ret;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(t);
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	uid = SceSysmemForDriver_45D22597(0, event);
	if (uid < 0) {
		sceSyscallLeave(t);
		return uid | 0x40000000;
	}
	ret = SceIofilemgrForDriver_C6158F8D(uid);
	if (ret != 0) {
		sceSyscallLeave(t);
		return ret;
	}
	SceSysmemForDriver_84A4AF5E(0, event);
	sceSyscallLeave(t);
	return 0;
}

SceUInt32 vshIoGetstat(const char *path, void *stat)
{
	VSH_BRIDGE(sceIoGetstatForVshbridge(path, stat));
}

SceUInt32 vshIoChstat(const char *path, const void *stat, int bits)
{
	VSH_BRIDGE(sceIoChstatForVshbridge(path, stat, bits));
}

SceUInt32 vshIoDread(SceUID fd, void *dirent)
{
	VSH_BRIDGE_UID(fd, sceIoDreadForVshbridge(uid, dirent));
}

SceUInt32 vshMemoryCardEnableSlowMode(void)
{
	SceUInt32 t = sceSyscallEnter();

	SceSdifForDriver_F37CF8E5();
	ksceMsifEnableSlowMode();
	sceSyscallLeave(t);
	return 0;
}

SceUInt32 vshMemoryCardGetCardInsertState(int card)
{
	VSH_BRIDGE(card != 3 ? ksceSdifGetCardInsertState1(card) : ksceSdifGetCardInsertState2(3));
}

SceUInt32 vshMsifGetMsInfo(void *info)
{
	SceUInt32 t = sceSyscallEnter();
	SceMsInfo buf;
	int ret;

	if (!sceSblACMgrIsSystemProgram(0)) {
		ret = SCE_ERROR_ERRNO_ENOSYS;
	} else {
		ret = ksceMsifGetMsInfo(&buf);
		if (ret >= 0)
			ret = SceSysmemForDriver_6D88EF8A(info, &buf, sizeof(buf));
	}
	sceSyscallLeave(t);
	return ret;
}

SceUInt32 vshRemovableMemoryGetCardInsertState(void)
{
	VSH_BRIDGE(kscePervasiveRemovableMemoryGetCardInsertState());
}

/* the card info of the SD slot (Dolce: 3, else 1), 0x68 bytes from its context */
SceUInt32 vshSdGetCardInfo(void *info)
{
	SceUInt32 t = sceSyscallEnter();
	const SdifContextPart *ctx = NULL;
	SdifCardInfo kinfo;
	int ret, i;

	if (!sceSblACMgrIsSystemProgram(0)) {
		ret = SCE_ERROR_ERRNO_ENOSYS;
		goto out;
	}
	if (SceSysrootForDriver_F804F761() == 0) {
		if (SceSysrootForDriver_8AA268D6(0xb))
			ctx = ksceSdifGetSdContextPartValidateSd(3);
	} else {
		ctx = ksceSdifGetSdContextPartValidateSd(1);
	}
	if (ctx == NULL) {
		ret = SCE_ERROR_ERRNO_ENODEV;
		goto out;
	}
	kinfo.cid = ctx->info.cid;
	kinfo.csd = ctx->info.csd;
	for (i = 0; i < 4; i++)
		kinfo.ssr[i] = ctx->info.ssr[i];
	kinfo.scr = ctx->info.scr;
	ret = SceSysmemForDriver_6D88EF8A(info, &kinfo, sizeof(kinfo));
out:
	sceSyscallLeave(t);
	return ret;
}

SceUInt32 vshIoSetProcessDefaultPriorityForSystem(int priority)
{
	VSH_BRIDGE(SceIofilemgrForDriver_ABE65071(priority));
}

SceUInt32 vshIoGetProcessDefaultPriorityForSystem(void)
{
	VSH_BRIDGE(SceIofilemgrForDriver_CE397158());
}

/* ===== hdmi ===== */

SceUInt32 vshHdmiCecCmdForcedPollingMsg(void)
{
	VSH_BRIDGE(SceHdmiForDriver_6D2D10D8());
}

SceUInt32 vshHdmiEnableCec(void)
{
	SceUInt32 t = sceSyscallEnter();
	SceProductMode mode = 0;
	int ret;

	if (!sceSblACMgrIsSystemProgram(0)) {
		ret = SCE_ERROR_ERRNO_ENOSYS;
	} else {
		ScePmMgrForDriver_2AC815A2(&mode);
		ret = mode == 1 ? sceHdmiCecEnableCec() : (int)SCE_HDMI_ERROR_NOT_PRODUCT_MODE;
	}
	sceSyscallLeave(t);
	return ret;
}

SceUInt32 vshHdmiDisableCec(void)
{
	SceUInt32 t = sceSyscallEnter();
	SceProductMode mode = 0;
	int ret;

	if (!sceSblACMgrIsSystemProgram(0)) {
		ret = SCE_ERROR_ERRNO_ENOSYS;
	} else {
		ScePmMgrForDriver_2AC815A2(&mode);
		ret = mode == 1 ? SceHdmiForDriver_291F000B() : (int)SCE_HDMI_ERROR_NOT_PRODUCT_MODE;
	}
	sceSyscallLeave(t);
	return ret;
}

/* ===== npdrm ===== */

SceUInt32 vshNpDrmGetLegacyDocKey(void *a, void *b, void *c, void *d)
{
	VSH_BRIDGE(ksceNpDrmGetLegacyDocKey(a, b, c, d));
}

SceUInt32 vshNpDrmEbootSigGenPsp(void *a, void *b, void *c, void *d)
{
	VSH_BRIDGE(ksceNpDrmEbootSigGenPsp(a, b, c, d));
}

SceUInt32 vshNpDrmEbootSigGenPs1(void *a, void *b, void *c, void *d)
{
	VSH_BRIDGE(ksceNpDrmEbootSigGenPs1(a, b, c, d));
}

SceUInt32 vshNpDrmEbootSigGenMultiDisc(void *a, void *b, void *c, void *d)
{
	VSH_BRIDGE(ksceNpDrmEbootSigGenMultiDisc(a, b, c, d));
}

SceUInt32 vshNpDrmEbootSigConvert(const char *path, const void *oldSignature, void *newSignature)
{
	VSH_BRIDGE(ksceNpDrmEbootSigConvert(path, oldSignature, newSignature));
}

SceUInt32 vshNpDrmEbootSigVerify(void *a, void *b)
{
	VSH_BRIDGE(ksceNpDrmEbootSigVerify(a, b));
}

SceUInt32 vshNpDrmPspEbootSigGen(void *a, void *b, void *c)
{
	VSH_BRIDGE(ksceNpDrmPspEbootSigGen(a, b, c));
}

SceUInt32 vshNpDrmPspEbootVerify(void *a, void *b)
{
	VSH_BRIDGE(ksceNpDrmPspEbootVerify(a, b));
}

SceUInt32 vshNpDrmIsLooseAccountBind(void)
{
	VSH_BRIDGE(ksceNpDrmIsLooseAccountBind());
}

SceUInt32 vshNpDrmUpdateDebugSettings(void)
{
	VSH_BRIDGE(ksceNpDrmUpdateDebugSettings());
}

SceUInt32 vshNpDrmUpdateAccountId(void *a, void *b)
{
	VSH_BRIDGE(ksceNpDrmUpdateAccountId(a, b));
}

SceUID vshKernelSearchModuleByName(const char *name)
{
	SceUInt32 t = sceSyscallEnter();
	SceUID ret = SceModulemgrForDriver_BBE1771C(name);

	sceSyscallLeave(t);
	return ret;
}

/* the content of an SPFSO, if it fits the caller's buffer */
SceUInt32 vshSblAuthMgrVerifySpsfo(const char *path, void *buf, SceSize size)
{
	SceUInt32 t = sceSyscallEnter();
	SceUInt32 ctx[3] = { 0 };
	void *data = NULL;
	SceSize dataSize = 0;
	int ret, ret2;

	if (!sceSblACMgrIsSystemProgram(0)) {
		ret2 = SCE_ERROR_ERRNO_ENOSYS;
	} else {
		ret = ksceSblPostSsMgrInitializeSpfsoCtx(path, ctx);
		if (ret >= 0 && (ret = ksceSblPostSsMgrVerifySpfsoCtx(ctx, &data, &dataSize)) >= 0) {
			if (dataSize == 0 || dataSize > size)
				ret = SCE_SBL_ERROR_SPFSO_TOO_LARGE;
			else
				ret = SceSysmemForDriver_6D88EF8A(buf, data, dataSize);
		}
		ret2 = ksceSblPostSsMgrReleaseSpfsoCtx(ctx);
		if (ret2 >= 0)
			ret2 = ret;
	}
	sceSyscallLeave(t);
	return ret2;
}

SceUInt32 vshSDfCtlSetBootParameter(const char *param, int unused, int value)
{
	(void)unused;
	VSH_BRIDGE(SceSysrootForDriver_7AAC4EE7(param, value));
}

/* ===== appmgr ===== */

SceUInt32 vshAppMgrSystemParamDateTimeSetConf(const void *conf)
{
	VSH_BRIDGE(ksceAppMgrSystemParamDateTimeSetConf(conf));
}

SceUInt32 vshAppMgrBgdlSetQueueStatus(const void *status)
{
	VSH_BRIDGE(ksceAppMgrBgdlSetQueueStatus(status));
}

SceUInt32 vshAppMgrDebugSettingNotifyUpdate(int setting)
{
	VSH_BRIDGE(ksceAppMgrDebugSettingNotifyUpdate(setting));
}

SceUInt32 vshAppMgrCheckContentInstallPeriod(void *a)
{
	VSH_BRIDGE(ksceAppMgrCheckContentInstallPeriod(a));
}

SceUInt32 vshAppMgrUpdateRifInfo(const void *info)
{
	VSH_BRIDGE(ksceAppMgrUpdateRifInfo(info));
}

SceUInt32 vshAppMgrRegisterPath(const char *path, void *a)
{
	VSH_BRIDGE(ksceAppMgrRegisterPath(path, a));
}

SceUInt32 vshAppMgrCheckPfsMounted(const char *path)
{
	VSH_BRIDGE(ksceAppMgrCheckPfsMounted(path));
}

SceUInt32 vshAppMgrCloudDataCreateHeader(const void *a, int b, const void *c, void *d)
{
	VSH_BRIDGE(ksceAppMgrCloudDataCreateHeader(a, b, c, d));
}

SceUInt32 vshAppMgrCloudDataVerifyHeader(const void *header)
{
	VSH_BRIDGE(ksceAppMgrCloudDataVerifyHeader(header));
}

SceUInt32 vshAppMgrCloudDataSetupKey(void *key)
{
	VSH_BRIDGE(ksceAppMgrCloudDataSetupKey(key));
}

SceUInt32 vshAppMgrCloudDataSrcMount(int a, const void *b, void *c)
{
	VSH_BRIDGE(ksceAppMgrCloudDataSrcMount(a, b, c));
}

SceUInt32 vshAppMgrCloudDataDstCreateMount(const void *a, void *b)
{
	VSH_BRIDGE(ksceAppMgrCloudDataDstCreateMount(a, b));
}

SceUInt32 vshAppMgrFakeSaveDataCreateMount(const char *path, void *b)
{
	VSH_BRIDGE(ksceAppMgrFakeSaveDataCreateMount(path, b));
}

SceUInt32 vshAppMgrCloudDataSetMcId(int a, const void *id)
{
	VSH_BRIDGE(ksceAppMgrCloudDataSetMcId(a, id));
}

SceUInt32 vshAppMgrCloudDataGetMcId(int a, void *id)
{
	VSH_BRIDGE(ksceAppMgrCloudDataGetMcId(a, id));
}

SceUInt32 vshAppMgrCloudDataClearMcId(int a)
{
	VSH_BRIDGE(ksceAppMgrCloudDataClearMcId(a));
}

SceUInt32 vshAppMgrGetPfsProcessStatus(void)
{
	VSH_BRIDGE(ksceAppMgrGetPfsProcessStatus());
}

SceUInt32 vshAppMgrIsExclusiveProcessRunning(const void *a)
{
	VSH_BRIDGE(ksceAppMgrIsExclusiveProcessRunning(a));
}

SceUInt32 vshAppMgrAcInstGetAcdirParam(const void *a, const void *b, void *c)
{
	VSH_BRIDGE(ksceAppMgrAcInstGetAcdirParam(a, b, c));
}

SceUInt32 vshAppMgrSaveDataLocalBackupTargetGetList(void *list)
{
	VSH_BRIDGE(ksceAppMgrSaveDataLocalBackupTargetGetList(list));
}

SceUInt32 vshAppMgrSaveDataLocalBackupTargetRemoveItem(const void *item)
{
	VSH_BRIDGE(ksceAppMgrSaveDataLocalBackupTargetRemoveItem(item));
}

SceUInt32 vshAppMgrLocalBackupGetOfflineId(void *id, void *b)
{
	VSH_BRIDGE(ksceAppMgrLocalBackupGetOfflineId(id, b));
}

SceUInt32 vshAppMgrLocalBackupVerifyOfflineHeader(const void *header)
{
	VSH_BRIDGE(ksceAppMgrLocalBackupVerifyOfflineHeader(header));
}

SceUInt32 vshAppMgrSaveDataNotifyBackupFinished(const void *a)
{
	VSH_BRIDGE(ksceAppMgrSaveDataNotifyBackupFinished(a));
}

/* ===== coredump ===== */

SceUInt32 vshCoredumpCafInit(void)
{
	VSH_BRIDGE(ksceCoredumpCafInit());
}

SceUInt32 vshCoredumpCafFinal(void)
{
	VSH_BRIDGE(ksceCoredumpCafFinal());
}

SceUInt32 vshCoredumpCafContextCreate(void)
{
	SceUInt32 t = sceSyscallEnter();
	SceUID uid, puid = SCE_ERROR_ERRNO_ENOSYS;

	if (sceSblACMgrIsSystemProgram(0)) {
		uid = ksceCoredumpCafContextCreate();
		if (uid < 0) {
			sceSyscallLeave(t);
			return uid;
		}
		puid = SceSysmemForDriver_BF209859(0, uid);
		if (puid < 0)
			ksceCoredumpCafContextDestroy(uid);
	}
	sceSyscallLeave(t);
	return puid;
}

SceUInt32 vshCoredumpCafContextDestroy(SceUID ctx)
{
	VSH_BRIDGE_UID(ctx, ksceCoredumpCafContextDestroy(uid));
}

SceUInt32 vshCoredumpCafHeaderInit(SceUID ctx, SceUInt32 algorithm)
{
	VSH_BRIDGE_UID(ctx, ksceCoredumpCafHeaderInit(uid, algorithm));
}

SceUInt32 vshCoredumpCafHeaderTransform(SceUID ctx, const void *data, SceSize size)
{
	VSH_BRIDGE_UID(ctx, ksceCoredumpCafHeaderTransform(uid, data, size));
}

SceUInt32 vshCoredumpCafHeaderFinal(SceUID ctx, void *digest, SceSize size)
{
	VSH_BRIDGE_UID(ctx, ksceCoredumpCafHeaderFinal(uid, digest, size));
}

SceUInt32 vshCoredumpCafCreateIv(void *iv, SceSize size)
{
	VSH_BRIDGE(ksceCoredumpCafCreateIv(iv, size));
}

SceUInt32 vshCoredumpCafSegmentInit(SceUID ctx, void *a, void *b, void *c, void *d, void *e)
{
	VSH_BRIDGE_UID(ctx, ksceCoredumpCafSegmentInit(uid, a, b, c, d, e));
}

SceUInt32 vshCoredumpCafSegmentTransform(SceUID ctx, void *a, void *b, void *c)
{
	VSH_BRIDGE_UID(ctx, ksceCoredumpCafSegmentTransform(uid, a, b, c));
}

SceUInt32 vshCoredumpCafSegmentFinal(SceUID ctx, void *a, void *b)
{
	VSH_BRIDGE_UID(ctx, ksceCoredumpCafSegmentFinal(uid, a, b));
}

SceUInt32 vshCoredumpDeleteCrashReportCaf(void)
{
	VSH_BRIDGE(ksceCoredumpDeleteCrashReportCaf());
}

SceUInt32 vshCoredumpCreateDump(int a, const void *b, int c, const void *d, int e, int f, void *g, int h)
{
	return ksceCoredumpCreateDump(a, b, c, d, e, f, g, h);
}

SceUInt32 vshEventLogPut(SceUInt16 type, SceUInt16 subtype, SceUInt32 arg, const void *pBuf, SceSize bufSize)
{
	VSH_BRIDGE(k_sceEventLogPut(type, subtype, arg, pBuf, bufSize));
}

/* the system software version: a 0x28 byte record with its "X.YY" string */
SceUInt32 vshSblGetSystemSwVersion(SceKernelSystemSwVersion *info)
{
	SceUInt32 t = sceSyscallEnter();
	SceUInt32 v;
	int ret;

	if (!sceSblACMgrIsSystemProgram(0)) {
		ret = SCE_ERROR_ERRNO_ENOSYS;
	} else if (info == NULL) {
		ret = SCE_ERROR_ERRNO_EINVAL;
	} else if (info->size == sizeof(*info)) {
		v = SceSysrootForDriver_67AAB627();
		info->size = sizeof(*info);
		info->version = v;
		info->versionExtra = 0;
		snprintf(info->versionString, sizeof(info->versionString), "%d.%02d",
			 (v >> 28) * 10 + ((v << 4) >> 28), ((v << 8) >> 28) * 10 + ((v << 12) >> 28));
		sceSyscallLeave(t);
		return 0;
	} else {
		ret = SCE_KERNEL_ERROR_INVALID_FLAGS;
	}
	sceSyscallLeave(t);
	return ret;
}

SceUInt32 vshSblAimgrGetSMI(void *smi)
{
	SceUInt32 t = sceSyscallEnter();
	int ret;

	if (!sceSblACMgrIsSystemProgram(0)) {
		ret = SCE_ERROR_ERRNO_ENOSYS;
	} else if (smi != NULL) {
		ret = SceSblAIMgrForDriver_47D9CF13(smi);
		sceSyscallLeave(t);
		return ret;
	} else {
		ret = SCE_ERROR_ERRNO_EINVAL;
	}
	sceSyscallLeave(t);
	return ret;
}

SceUInt32 vshSblSsGenerateAppKey(void *a, void *b)
{
	SceUInt32 t = sceSyscallEnter();
	int ret;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(t);
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	if (b == NULL || a == NULL) {
		sceSyscallLeave(t);
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	}
	ret = SceSblPostSsMgrForDriver_2646DE64(a, b);
	sceSyscallLeave(t);
	return ret;
}

SceUInt32 vshSblSsCreatePassPhrase(void *a, void *b)
{
	SceUInt32 t = sceSyscallEnter();
	int ret;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(t);
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	if (b == NULL || a == NULL) {
		sceSyscallLeave(t);
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	}
	ret = ksceSblSsCreatePassPhrase(a, b);
	sceSyscallLeave(t);
	return ret;
}
