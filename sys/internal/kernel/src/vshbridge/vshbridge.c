/*
 * SceVshBridge (Sony's vshbridge.c): kernel services for the system
 * applications (SceShell and friends), as syscalls. Every bridge runs with
 * the syscall TPIDRURO convention and refuses callers that are not a system
 * program. The exports taking user pointers are wrapped in
 * vshbridge_user.c, which copies the data in and out.
 */
#include <scetypes.h>
#include <stdio.h>
#include <string.h>
#include <sysmem/debug.h>
#include <sdif/sdif.h>
#include <msif/msif.h>
#include <intrmgr/syscall.h>
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
	} else if (sceSblPmMgrGetProductMode(&_mode) != 0) {		\
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
			ret = ScePowerForDriver_8921A7A0(1);
		else if (param->size == 8 && param->mode == 2)
			ret = ScePowerForDriver_8921A7A0(0);
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
			ret = sceKernelCopyToUser(version, &v, sizeof(v));
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

SceUInt32 vshRtcSetConf(unsigned int dateFormat, unsigned int timeFormat, int timezone,
			unsigned int daylightSaving)
{
	VSH_BRIDGE(SceRtcForDriver_ACFE5A9F(dateFormat, timeFormat, timezone, daylightSaving));
}

SceUInt32 vshRtcSetCurrentTick(const SceRtcTick *tick)
{
	VSH_BRIDGE(sceRtcSetCurrentTick(tick));
}

SceUInt32 vshRtcSetCurrentSecureTick(const SceRtcTick *tick)
{
	VSH_BRIDGE(SceRtcForDriver_7D431C87(tick));
}

SceUInt32 vshRtcGetCurrentSecureTick(SceRtcTick *tick)
{
	VSH_BRIDGE(SceRtcForDriver_401C0954(tick));
}

SceUInt32 vshRtcSetCurrentNetworkTick(const SceRtcTick *tick)
{
	VSH_BRIDGE(SceRtcForDriver_162E486E(tick));
}

SceUInt32 vshRtcSetCurrentDebugNetworkTick(const SceRtcTick *tick)
{
	VSH_BRIDGE(SceRtcForDriver_AAB6053F(tick));
}

SceUInt32 vshRtcSetAlarmTick(const SceRtcTick *tick, int unk)
{
	VSH_BRIDGE(sceRtcSetAlarmTick(tick, unk));
}

SceUInt32 vshRtcGetAlarmTick(SceRtcTick *tick, int *unk)
{
	VSH_BRIDGE(SceRtcForDriver_C838275A(tick, unk));
}

SceUInt32 vshRtcIsAlarmed(void)
{
	VSH_BRIDGE(SceRtcForDriver_CD295F0C());
}

SceUInt32 vshRtcRegisterCallback(SceUID cb)
{
	VSH_BRIDGE_UID(cb, SceRtcForDriver_5220870D(uid));
}

SceUInt32 vshRtcUnregisterCallback(SceUID cb)
{
	VSH_BRIDGE_UID(cb, SceRtcForDriver_9546C68B(uid));
}

SceUInt32 vshRtcSetSecureAlarmTick(const SceRtcTick *tick)
{
	VSH_BRIDGE(SceRtcForDriver_8333790D(tick));
}

SceUInt32 vshRtcGetSecureAlarmTick(SceRtcTick *tick)
{
	VSH_BRIDGE(SceRtcForDriver_BD53731C(tick));
}

SceUInt32 vshRtcIsSecureAlarmed(void)
{
	VSH_BRIDGE(SceRtcForDriver_E39F2ABE());
}

SceUInt32 vshRtcRegisterSecureAlarmCallback(SceUID cb)
{
	VSH_BRIDGE_UID(cb, SceRtcForDriver_CF84DFC0(uid));
}

SceUInt32 vshRtcUnregisterSecureAlarmCallback(SceUID cb)
{
	VSH_BRIDGE_UID(cb, SceRtcForDriver_EDDC2325(uid));
}

/* ===== display, ctrl, motion, touch, led ===== */

SceUInt32 vshDisplaySetInvertColors(int head, int enable)
{
	VSH_BRIDGE(SceDisplayForDriver_19140ACD(head, enable));
}

SceUInt32 vshDisplaySetViewportConf(SceUInt32 head, SceUInt32 plane,
				    const SceDisplayViewportConf *conf)
{
	VSH_BRIDGE(SceDisplayForDriver_EE5EB52D(head, plane, conf));
}

SceUInt32 vshDisplayGetActualViewportConf(SceUInt32 head, SceUInt32 plane,
					  SceDisplayViewportConf *conf)
{
	VSH_BRIDGE(SceDisplayForDriver_40ACFE51(head, plane, conf));
}

SceUInt32 vshDisplayRegisterFrameBufCallback(SceUID cb)
{
	VSH_BRIDGE_UID(cb, SceDisplayForDriver_6E22990E(uid));
}

SceUInt32 vshCtrlSetIdleCancelKey(int a, int b, int c, int d)
{
	VSH_BRIDGE(SceCtrlForDriver_0A679512(a, b, c, d));
}

SceUInt32 vshCtrlSetIdleCancelThreshold(int a, int b)
{
	VSH_BRIDGE(SceCtrlForDriver_345E74FD(a, b));
}

SceUInt32 vshCtrlSetVibrationSetting(int setting)
{
	VSH_BRIDGE(SceCtrlForDriver_4D8FE670(setting));
}

SceUInt32 vshCtrlSetTurnOffInterval(int interval)
{
	VSH_BRIDGE(SceCtrlForDriver_ED8F0129(interval));
}

SceUInt32 vshCtrlGetActiveControllerPort(void)
{
	VSH_BRIDGE(SceCtrlForDriver_FA88D837());
}

SceUInt32 vshCtrlIsBdRemoconConnected(void)
{
	VSH_BRIDGE(SceCtrlForDriver_5B849B22());
}

SceUInt32 vshCtrlChangePortAssign(int a, int b)
{
	VSH_BRIDGE(SceCtrlForDriver_53381364(a, b));
}

SceUInt32 vshCtrlRegisterNotifyCallBack(SceUID cb)
{
	VSH_BRIDGE_UID(cb, SceCtrlForDriver_E170883E(uid));
}

SceUInt32 vshCtrlUnregisterNotifyCallBack(SceUID cb)
{
	VSH_BRIDGE_UID(cb, SceCtrlForDriver_9DAB51C4(uid));
}

SceUInt32 vshMotionNoiseFilterIsAvailable(void)
{
	return SceMotionDevForDriver_11A17A96();
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
	ret = SceTouchForDriver_A32529B8(front, back);
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
		if ((SceSysconForDriver_CBD6D8BC() & 0xff0000) < 0x800000) {
			mode = 1;
			conf = vshLedDefaultConf;
		} else {
			mode = 0;
		}
	}
	ret = SceLedForDriver_EA24BE03(led, mode, conf);
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
	VSH_BRIDGE(ScePowerForDriver_C1853BA7(mode));
}

SceUInt32 vshPowerSetStandbyButtonPushTime(int time)
{
	VSH_BRIDGE(ScePowerForDriver_675A84ED(time));
}

SceUInt32 vshPowerSetPsButtonPushTime(int time)
{
	VSH_BRIDGE(ScePowerForDriver_CF8F0529(time));
}

SceUInt32 vshPowerGetWakeupFactor(void)
{
	VSH_BRIDGE(ScePowerForDriver_9F26222A());
}

SceUInt32 vshPowerSetBatteryFakeStatus(const struct PowerBatteryFakeStatus *status)
{
	VSH_BRIDGE(ScePowerForDriver_0C6973B8(status));
}

SceUInt32 vshPowerRequestHibernate(void)
{
	VSH_BRIDGE(ScePowerForDriver_08951418());
}

/* ===== syscon ===== */

SceUInt32 vshSysconGetManufacturesStatus(uint32_t *status)
{
	VSH_BRIDGE_PRODUCT_MODE(SCE_SYSCON_ERROR_NOT_PRODUCT_MODE, SceSysconForDriver_3E09A1F4(status));
}

SceUInt32 vshSysconGetManualChargeMode(uint32_t *mode)
{
	VSH_BRIDGE_PRODUCT_MODE(SCE_SYSCON_ERROR_NOT_PRODUCT_MODE, SceSysconForDriver_4FEC564C(mode));
}

SceUInt32 vshSysconCtrlManualChargeMode(int mode)
{
	VSH_BRIDGE_PRODUCT_MODE(SCE_SYSCON_ERROR_NOT_PRODUCT_MODE, SceSysconForDriver_C6A2C9EF(mode));
}

SceUInt32 vshSysconEnableHibernateIO(int enable)
{
	VSH_BRIDGE(SceSysconForDriver_4946538A(enable));
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
	} else if (sceSblPmMgrGetProductMode(&mode) != 0) {
		mode = 0;
		ret = SCE_SYSCON_ERROR_NOT_PRODUCT_MODE;
	} else if (!mode) {
		ret = SCE_SYSCON_ERROR_NOT_PRODUCT_MODE;
	} else {
		ret = SceSysconForDriver_701535FC((uint32_t *)buf);
		if (ret >= 0)
			ret = sceKernelCopyToUser(info, buf, sizeof(buf));
	}
	sceSyscallLeave(t);
	return ret;
}

SceUInt32 vshSysconLogStart(void)
{
	VSH_BRIDGE_PRODUCT_MODE(SCE_SYSCON_ERROR_NOT_PRODUCT_MODE, SceSysconForDriver_4E55CF5E());
}

SceUInt32 vshSysconLogReadData(int a, void *buf, SceSize size)
{
	VSH_BRIDGE(SceSysconForDriver_487D97F3(a, buf, size));
}

SceUInt32 vshSysconLogStartWaiting(void)
{
	VSH_BRIDGE_PRODUCT_MODE(SCE_SYSCON_ERROR_NOT_PRODUCT_MODE, SceSysconForDriver_9C0B1E61());
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
	ret = SceSysconForDriver_3B354824((uint32_t *)log);
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
	ret = SceSysconForDriver_3843D657(unk);
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
	_ret = ((SceUInt32)SceSysconForDriver_CBD6D8BC() >> (_shift)) & 1; \
	sceSyscallLeave(_t);						\
	return _ret;							\
} while (0)

SceUInt32 vshSysconHasWWAN(void)
{
	VSH_HARDWARE_INFO_BIT(1);
}

SceUInt32 vshSysconIduModeSet(void)
{
	VSH_BRIDGE(SceSysconForDriver_956D07CB());
}

SceUInt32 vshSysconIduModeClear(void)
{
	VSH_BRIDGE(SceSysconForDriver_34574496());
}

SceUInt32 vshSysconIsIduMode(void)
{
	VSH_HARDWARE_INFO_BIT(7);
}

SceUInt32 vshSysconShowModeSet(void)
{
	VSH_BRIDGE(SceSysconForDriver_6D65B70F());
}

SceUInt32 vshSysconShowModeClear(void)
{
	VSH_BRIDGE(SceSysconForDriver_8D7724C0());
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
	VSH_BRIDGE(SceSysconForDriver_9ADD60D2());
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
	ret = SceSysconForDriver_EF810687(status);
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
	*info = SceSysconForDriver_CBD6D8BC();
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
	if (!(SceSysconForDriver_CBD6D8BC() & 0x20)) {
		memset(info, 0, 0x10);
		sceSyscallLeave(t);
		return 0;
	}
	ret = SceSysconForDriver_965C68C3(info);
	sceSyscallLeave(t);
	return ret;
}

SceUInt32 vshSysconBeginConfigstorageTransaction(void)
{
	VSH_BRIDGE(SceSysconForDriver_A4968B8C());
}

SceUInt32 vshSysconEndConfigstorageTransaction(void)
{
	VSH_BRIDGE(SceSysconForDriver_FCC3E8EE());
}

SceUInt32 vshSysconCommitConfigstorageTransaction(void)
{
	VSH_BRIDGE(SceSysconForDriver_7B9B3617());
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
		ret = sceKernelCopyFromUser(buf, script, size);
		if (ret >= 0)
			ret = SceSysconForDriver_89C5CFD6(a, buf, size);
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
		ret = sceKernelCopyFromUser(buf, script, size);
		if (ret >= 0)
			ret = SceSysconForDriver_CC6F90A8(a, buf, size);
	}
	sceSyscallLeave(t);
	return ret;
}

SceUInt32 vshSysconGetBatteryCalibData(uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d)
{
	SceUInt32 t = sceSyscallEnter();
	uint32_t va = 0, vb = 0, vc = 0, vd = 0;
	int ret;

	if (!sceSblACMgrIsSystemProgram(0)) {
		ret = SCE_ERROR_ERRNO_ENOSYS;
	} else {
		ret = SceSysconForDriver_9ADC9936(&va, &vb, &vc, &vd);
		if (ret >= 0 && (ret = sceKernelCopyToUser(a, &va, 4)) >= 0 &&
		    (ret = sceKernelCopyToUser(b, &vb, 4)) >= 0 &&
		    (ret = sceKernelCopyToUser(c, &vc, 4)) >= 0)
			ret = sceKernelCopyToUser(d, &vd, 4);
	}
	sceSyscallLeave(t);
	return ret;
}

/* ===== idstorage ===== */

SceUInt32 vshIdStorageGetLeafSize(void)
{
	VSH_BRIDGE(SceIdStorageForDriver_EB830733());
}

SceUInt32 vshIdStorageIsFormatted(void)
{
	VSH_BRIDGE(SceIdStorageForDriver_FEFA40C2());
}

SceUInt32 vshIdStorageIsReadOnly(void)
{
	VSH_BRIDGE(SceIdStorageForDriver_2D633688());
}

SceUInt32 vshIdStorageIsDirty(void)
{
	VSH_BRIDGE(SceIdStorageForDriver_B9069BAD());
}

SceUInt32 vshIdStorageFormat(void)
{
	VSH_BRIDGE(SceIdStorageForDriver_958089DB());
}

SceUInt32 vshIdStorageUnformat(void)
{
	VSH_BRIDGE(SceIdStorageForDriver_F4BCB3EE());
}

SceUInt32 vshIdStorageReadLeaf(SceSize leaf, void *buf)
{
	SceUInt32 t = sceSyscallEnter();
	SceUInt8 data[0x200];
	int ret;

	if (!sceSblACMgrIsSystemProgram(0)) {
		ret = SCE_ERROR_ERRNO_ENOSYS;
	} else {
		ret = SceIdStorageForDriver_EB00C509(leaf, data);
		if (ret == 0)
			ret = sceKernelCopyToUser(buf, data, sizeof(data));
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
		ret = sceKernelCopyFromUser(data, buf, sizeof(data));
		if (ret >= 0)
			ret = SceIdStorageForDriver_1FA4D135(leaf, data);
	}
	sceSyscallLeave(t);
	return ret;
}

SceUInt32 vshIdStorageCreateLeaf(SceSize leaf)
{
	VSH_BRIDGE(SceIdStorageForDriver_08A471A6(leaf));
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
		ret = sceKernelCopyFromUser(buf, leaves, num * 2);
		if (ret >= 0)
			ret = SceIdStorageForDriver_99ACCB71(buf, num);
	} else {
		ret = SCE_ERROR_ERRNO_EINVAL;
	}
	sceSyscallLeave(t);
	return ret;
}

SceUInt32 vshIdStorageDeleteLeaf(SceSize leaf)
{
	VSH_BRIDGE(SceIdStorageForDriver_2C97AB36(leaf));
}

SceUInt32 vshIdStorageGetFreeLeaves(void)
{
	VSH_BRIDGE(SceIdStorageForDriver_37833CB8());
}

SceUInt32 vshIdStorageLookup(SceSize key, SceSize offset, void *buf, SceSize len)
{
	SceUInt32 t = sceSyscallEnter();
	SceUInt8 data[0x200];
	int ret;

	if (!sceSblACMgrIsSystemProgram(0)) {
		ret = SCE_ERROR_ERRNO_ENOSYS;
	} else if ((len <= 0x200 ? offset : len) < 0x200) {
		ret = sceIdStorageLookup(key, offset, data, len);
		if (ret == 0)
			ret = sceKernelCopyToUser(buf, data, len);
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
		ret = sceKernelCopyFromUser(data, buf, len);
		if (ret >= 0)
			ret = SceIdStorageForDriver_683AAC10(key, offset, data, len);
	} else {
		ret = SCE_ERROR_ERRNO_EINVAL;
	}
	sceSyscallLeave(t);
	return ret;
}

SceUInt32 vshIdStorageFlush(void)
{
	VSH_BRIDGE(SceIdStorageForDriver_3AD32523());
}

SceUInt32 vshIdStorageRestart(int unk)
{
	VSH_BRIDGE(SceIdStorageForDriver_60B647A5(unk));
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
	VSH_BRIDGE(sceSblAimgrGetConsoleId(id));
}

SceUInt32 vshSblAimgrGetPscode(void *code)
{
	VSH_BRIDGE(SceSblSsMgrForDriver_E0DC2587(code));
}

SceUInt32 vshSblAimgrGetPscode2(void *code)
{
	VSH_BRIDGE(sceSblAimgrGetPscode2(code));
}

SceUInt32 vshSblAimgrGetVisibleId(void *id)
{
	VSH_BRIDGE_PRODUCT_MODE(SCE_SBL_ERROR_NOT_PRODUCT_MODE, sceSblAimgrGetVisibleId(id));
}

SceUInt32 vshSblSsGetNvsData(SceUInt32 offset, void *buf, SceSize size)
{
	SceUInt32 t = sceSyscallEnter();
	int ret;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(t);
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	ret = sceSblSsGetNvsData(offset, buf, size);
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
	ret = sceSblSsSetNvsData(offset, buf, size);
	sceSyscallLeave(t);
	return ret == 0 ? 0 : ret;
}

/* portability mode 0-20; the output size starts as the input size */
SceUInt32 vshSblSsEncryptWithPortability(SceUInt32 mode, void *iv, ScePortabilityData *in,
					 ScePortabilityData *out)
{
	SceUInt32 t = sceSyscallEnter();
	int ret;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(t);
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	if (mode >= 0x15 || iv == NULL || in == NULL || out == NULL) {
		sceSyscallLeave(t);
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	}
	out->msg_size = in->msg_size;
	ret = sceSblSsEncryptWithPortability(mode, iv, in, out);
	sceSyscallLeave(t);
	return ret;
}

SceUInt32 vshSblSsDecryptWithPortability(SceUInt32 mode, void *iv, ScePortabilityData *in,
					 ScePortabilityData *out)
{
	SceUInt32 t = sceSyscallEnter();
	int ret;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(t);
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	if (mode >= 0x15 || iv == NULL || out == NULL || in == NULL) {
		sceSyscallLeave(t);
		return SCE_SBL_ERROR_INVALID_ARGUMENT;
	}
	out->msg_size = in->msg_size;
	ret = SceSblSsMgrForDriver_934DB6B5(mode, iv, in, out);
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
	VSH_BRIDGE(SceSblPostSsMgrForDriver_FE92A318(0));
}

SceUInt32 vshSblUtMgrHasComTestFlag(void)
{
	VSH_BRIDGE(SceSblPostSsMgrForDriver_7ACCAA50() != 0);
}

SceUInt32 vshSblUtMgrHasStoreFlag(void)
{
	VSH_BRIDGE(SceSblPostSsMgrForDriver_9D2E2D39() != 0);
}

SceUInt32 vshSblUtMgrHasNpTestFlag(void)
{
	VSH_BRIDGE(SceSblPostSsMgrForDriver_9FD835B0() != 0);
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
	VSH_BRIDGE(sceSblGcAuthMgrMlnpsnlAuth1ForDriver(a, b, c, d));
}

SceUInt32 vshSblGcAuthMgrMlnpsnlAuth2(void *a, void *b, void *c)
{
	VSH_BRIDGE(sceSblGcAuthMgrMlnpsnlAuth2ForDriver(a, b, c));
}

/* ===== start ===== */

int module_start(SceSize args, void *argp)
{
	int (*getKeyInt)(const char *, const char *, int *) = sceRegMgrGetKeyInt;
	int value, vibration = 1, interval = 0;

	(void)args;
	(void)argp;

	if (getKeyInt("/CONFIG/USB", "usb_charge_enable", &value) >= 0) {
		if (sceKernelSysrootIsSafeMode() || SceSysrootForDriver_B0E1FC67())
			value = 0;
		SceUdcdForDriver_2D91F8C3(value);
	}
	if (getKeyInt("/CONFIG/SYSTEM/", "controller_vibration", &vibration) >= 0) {
		SceCtrlForDriver_4D8FE670(vibration);
		if (getKeyInt("/CONFIG/POWER_SAVING/", "controller_off_interval", &interval) >= 0)
			SceCtrlForDriver_ED8F0129(interval);
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

SceUInt32 vshIoChstat(const char *path, SceIoStat *stat, int bits)
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

	sceSdifEnableSlowCardMode();
	sceMsifEnableSlowCardMode();
	sceSyscallLeave(t);
	return 0;
}

SceUInt32 vshMemoryCardGetCardInsertState(int card)
{
	VSH_BRIDGE(card != 3 ? SceSdifForDriver_36A2B01B(card) : SceSdifForDriver_FD9E5CFA(3));
}

SceUInt32 vshMsifGetMsInfo(void *info)
{
	SceUInt32 t = sceSyscallEnter();
	SceMsInfo buf;
	int ret;

	if (!sceSblACMgrIsSystemProgram(0)) {
		ret = SCE_ERROR_ERRNO_ENOSYS;
	} else {
		ret = SceMsifForDriver_D0307849(&buf);
		if (ret >= 0)
			ret = sceKernelCopyToUser(info, &buf, sizeof(buf));
	}
	sceSyscallLeave(t);
	return ret;
}

SceUInt32 vshRemovableMemoryGetCardInsertState(void)
{
	VSH_BRIDGE(ScePervasiveForDriver_551EEE82());
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
			ctx = SceSdifForDriver_B9EA5B1E(3);
	} else {
		ctx = SceSdifForDriver_B9EA5B1E(1);
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
	ret = sceKernelCopyToUser(info, &kinfo, sizeof(kinfo));
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
		sceSblPmMgrGetProductMode(&mode);
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
		sceSblPmMgrGetProductMode(&mode);
		ret = mode == 1 ? SceHdmiForDriver_291F000B() : (int)SCE_HDMI_ERROR_NOT_PRODUCT_MODE;
	}
	sceSyscallLeave(t);
	return ret;
}

/* ===== npdrm ===== */

SceUInt32 vshNpDrmGetLegacyDocKey(void *a, void *b, void *c, void *d)
{
	VSH_BRIDGE(SceNpDrmForDriver_4E321BDE(a, b, c, d));
}

SceUInt32 vshNpDrmEbootSigGenPsp(const char *ebootPath, const void *sha256, void *signature, int swVersion)
{
	VSH_BRIDGE(SceNpDrmForDriver_90B1A6D3(ebootPath, sha256, signature, swVersion));
}

SceUInt32 vshNpDrmEbootSigGenPs1(const char *ebootPath, const void *sha256, void *signature, int swVersion)
{
	VSH_BRIDGE(SceNpDrmForDriver_6D9223E1(ebootPath, sha256, signature, swVersion));
}

SceUInt32 vshNpDrmEbootSigGenMultiDisc(const char *ebootPath, const void *discInfo, void *signature, int swVersion)
{
	VSH_BRIDGE(SceNpDrmForDriver_39A7A666(ebootPath, discInfo, signature, swVersion));
}

SceUInt32 vshNpDrmEbootSigConvert(const char *path, const void *oldSignature, void *newSignature)
{
	VSH_BRIDGE(SceNpDrmForDriver_A29B75F9(path, oldSignature, newSignature));
}

SceUInt32 vshNpDrmEbootSigVerify(void *a, void *b)
{
	VSH_BRIDGE(SceNpDrmForDriver_7A319692(a, b));
}

SceUInt32 vshNpDrmPspEbootSigGen(void *a, void *b, void *c)
{
	VSH_BRIDGE(SceNpDrmForDriver_EF387FC4(a, b, c));
}

SceUInt32 vshNpDrmPspEbootVerify(void *a, void *b)
{
	VSH_BRIDGE(SceNpDrmForDriver_B6CA3A2C(a, b));
}

SceUInt32 vshNpDrmIsLooseAccountBind(void)
{
	VSH_BRIDGE(SceNpDrmForDriver_FC84CA1A());
}

SceUInt32 vshNpDrmUpdateDebugSettings(void)
{
	VSH_BRIDGE(SceNpDrmForDriver_A91C7443());
}

SceUInt32 vshNpDrmUpdateAccountId(SceUInt64 accountId)
{
	VSH_BRIDGE(SceNpDrmForDriver_116FC0D6(accountId));
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
	SceSblSpfsoCtx ctx = { 0 };
	void *data = NULL;
	SceSize dataSize = 0;
	int ret, ret2;

	if (!sceSblACMgrIsSystemProgram(0)) {
		ret2 = SCE_ERROR_ERRNO_ENOSYS;
	} else {
		ret = SceSblPostSsMgrForDriver_BDF18922(path, &ctx);
		if (ret >= 0 && (ret = SceSblPostSsMgrForDriver_686B9461(&ctx, &data, &dataSize)) >= 0) {
			if (dataSize == 0 || dataSize > size)
				ret = SCE_SBL_ERROR_SPFSO_TOO_LARGE;
			else
				ret = sceKernelCopyToUser(buf, data, dataSize);
		}
		ret2 = SceSblPostSsMgrForDriver_AD3B0078(&ctx);
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
	VSH_BRIDGE(SceAppMgrForDriver_A9AA3A68(conf));
}

SceUInt32 vshAppMgrBgdlSetQueueStatus(const void *status)
{
	VSH_BRIDGE(SceAppMgrForDriver_5B1BE482(status));
}

SceUInt32 vshAppMgrDebugSettingNotifyUpdate(int setting)
{
	VSH_BRIDGE(SceAppMgrForDriver_7554330F(setting));
}

SceUInt32 vshAppMgrCheckContentInstallPeriod(void *a)
{
	VSH_BRIDGE(SceAppMgrForDriver_193784A9(a));
}

SceUInt32 vshAppMgrUpdateRifInfo(const void *info)
{
	VSH_BRIDGE(sceAppMgrUpdateRifInfo(info));
}

SceUInt32 vshAppMgrRegisterPath(const char *path, void *a)
{
	VSH_BRIDGE(SceAppMgrForDriver_B53FB98E(path, a));
}

SceUInt32 vshAppMgrCheckPfsMounted(const char *path)
{
	VSH_BRIDGE(SceAppMgrForDriver_0B1BEEE0(path));
}

SceUInt32 vshAppMgrCloudDataCreateHeader(const void *a, int b, const void *c, void *d)
{
	VSH_BRIDGE(SceAppMgrForDriver_ED87F191(a, b, c, d));
}

SceUInt32 vshAppMgrCloudDataVerifyHeader(const void *header)
{
	VSH_BRIDGE(SceAppMgrForDriver_55D48190(header));
}

SceUInt32 vshAppMgrCloudDataSetupKey(void *key)
{
	VSH_BRIDGE(SceAppMgrForDriver_2FAA6ABD(key));
}

SceUInt32 vshAppMgrCloudDataSrcMount(int a, const void *b, void *c)
{
	VSH_BRIDGE(sceAppMgrCloudDataSrcMount(a, b, c));
}

SceUInt32 vshAppMgrCloudDataDstCreateMount(const void *a, void *b)
{
	VSH_BRIDGE(sceAppMgrCloudDataDstCreateMount(a, b));
}

SceUInt32 vshAppMgrFakeSaveDataCreateMount(const char *path, void *b)
{
	VSH_BRIDGE(sceAppMgrFakeSaveDataCreateMount(path, b));
}

SceUInt32 vshAppMgrCloudDataSetMcId(int a, const void *id)
{
	VSH_BRIDGE(SceAppMgrForDriver_0B94BB90(a, id));
}

SceUInt32 vshAppMgrCloudDataGetMcId(int a, void *id)
{
	VSH_BRIDGE(SceAppMgrForDriver_1286AF64(a, id));
}

SceUInt32 vshAppMgrCloudDataClearMcId(int a)
{
	VSH_BRIDGE(SceAppMgrForDriver_EC408FEF(a));
}

SceUInt32 vshAppMgrGetPfsProcessStatus(void)
{
	VSH_BRIDGE(sceAppMgrGetPfsProcessStatus());
}

SceUInt32 vshAppMgrIsExclusiveProcessRunning(const void *a)
{
	VSH_BRIDGE(sceAppMgrIsExclusiveProcessRunning(a));
}

SceUInt32 vshAppMgrAcInstGetAcdirParam(const void *a, const void *b, void *c)
{
	VSH_BRIDGE(sceAppMgrAcInstGetAcdirParam(a, b, c));
}

SceUInt32 vshAppMgrSaveDataLocalBackupTargetGetList(void *list)
{
	VSH_BRIDGE(SceAppMgrForDriver_9B3F7D66(list));
}

SceUInt32 vshAppMgrSaveDataLocalBackupTargetRemoveItem(const void *item)
{
	VSH_BRIDGE(SceAppMgrForDriver_C4EF95BB(item));
}

SceUInt32 vshAppMgrLocalBackupGetOfflineId(void *id, void *b)
{
	VSH_BRIDGE(SceAppMgrForDriver_61B13981(id, b));
}

SceUInt32 vshAppMgrLocalBackupVerifyOfflineHeader(const void *header)
{
	VSH_BRIDGE(SceAppMgrForDriver_62E98C42(header));
}

SceUInt32 vshAppMgrSaveDataNotifyBackupFinished(const void *a)
{
	VSH_BRIDGE(sceAppMgrSaveDataNotifyBackupFinished(a));
}

/* ===== coredump ===== */

SceUInt32 vshCoredumpCafInit(void)
{
	VSH_BRIDGE(SceCoredumpForDriver_9336009B());
}

SceUInt32 vshCoredumpCafFinal(void)
{
	VSH_BRIDGE(SceCoredumpForDriver_C90F61AF());
}

SceUInt32 vshCoredumpCafContextCreate(void)
{
	SceUInt32 t = sceSyscallEnter();
	SceUID uid, puid = SCE_ERROR_ERRNO_ENOSYS;

	if (sceSblACMgrIsSystemProgram(0)) {
		uid = sceCoredumpCafContextCreate();
		if (uid < 0) {
			sceSyscallLeave(t);
			return uid;
		}
		puid = SceSysmemForDriver_BF209859(0, uid);
		if (puid < 0)
			sceCoredumpCafContextDestroy(uid);
	}
	sceSyscallLeave(t);
	return puid;
}

SceUInt32 vshCoredumpCafContextDestroy(SceUID ctx)
{
	VSH_BRIDGE_UID(ctx, sceCoredumpCafContextDestroy(uid));
}

SceUInt32 vshCoredumpCafHeaderInit(SceUID ctx, SceUInt32 algorithm)
{
	VSH_BRIDGE_UID(ctx, SceCoredumpForDriver_7C8120C5(uid, algorithm));
}

SceUInt32 vshCoredumpCafHeaderTransform(SceUID ctx, const void *data, SceSize size)
{
	VSH_BRIDGE_UID(ctx, SceCoredumpForDriver_AE2C2793(uid, data, size));
}

SceUInt32 vshCoredumpCafHeaderFinal(SceUID ctx, void *digest, SceSize size)
{
	VSH_BRIDGE_UID(ctx, SceCoredumpForDriver_65AA4991(uid, digest, size));
}

SceUInt32 vshCoredumpCafCreateIv(void *iv, SceSize size)
{
	VSH_BRIDGE(SceCoredumpForDriver_E1BCBE8F(iv, size));
}

SceUInt32 vshCoredumpCafSegmentInit(SceUID ctx, SceUInt32 hashKeySelector, SceUInt32 cipherMode,
	SceUInt32 cipherKeySelector, const void *iv, SceSize size)
{
	VSH_BRIDGE_UID(ctx, SceCoredumpForDriver_07185515(uid, hashKeySelector, cipherMode,
		cipherKeySelector, iv, size));
}

SceUInt32 vshCoredumpCafSegmentTransform(SceUID ctx, const void *source, void *destination, SceSize size)
{
	VSH_BRIDGE_UID(ctx, SceCoredumpForDriver_FB7AEBFE(uid, source, destination, size));
}

SceUInt32 vshCoredumpCafSegmentFinal(SceUID ctx, void *digest, SceSize size)
{
	VSH_BRIDGE_UID(ctx, SceCoredumpForDriver_DF17420A(uid, digest, size));
}

SceUInt32 vshCoredumpDeleteCrashReportCaf(void)
{
	VSH_BRIDGE(SceCoredumpForDriver_AD070837());
}

SceUInt32 vshCoredumpCreateDump(int a, const void *b, int c, const void *d, int e, int f, void *g, int h)
{
	return SceCoredumpForDriver_0C10313F(a, b, c, d, e, f, g, h);
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
	ret = SceSblSsMgrForDriver_B8B298FD(a, b);
	sceSyscallLeave(t);
	return ret;
}
