/*
 * rtcmgr.c: SceSblRtcMgr, the secure RTC of a development kit's
 * communication processor (CP), reached through SceDeci4pCpup.
 */

#include <sysmem/sysmem.h>
#include <threadmgr/threadmgr.h>
#include <intrmgr/syscall.h>
#include "post_ss_mgr_internal.h"
#include <post_ss_mgr/sblrtcmgr.h>
#include <deci4p_cpup/cpup.h>

#define SCE_SBL_ERROR_RTC_CP			0x800f1005	/* names unknown */
#define SCE_SBL_ERROR_RTC_INVALID_ARGUMENT	0x800f1016
#define SCE_SBL_ERROR_RTC_UNSUPPORTED		0x800f1025
#define SCE_SBL_ERROR_RTC_CP_VERSION		0x800f1028

/* the times a CP RTC may be set to (names unknown) */
#define RTC_CP_PHYSICAL_MIN	0x4b3d6530
#define RTC_CP_PHYSICAL_MAX	0x7fe841b0
#define RTC_CP_LOGICAL_MIN	0x386cc4f0
#define RTC_CP_LOGICAL_MAX	0xf485d86f

#define RTC_CP_KEY_VERSION	0x1000500	/* names unknown: CP firmware versions */
#define RTC_CP_SERIAL_VERSION	0x1000600

#define RTC_CP_SERIAL_ID_SIZE	0x80

/* SceDeci4pCpup's import: NULL when no CP (SceDeci4pCpup) is there to ask */
int (*g_rtcMgrCpup)(SceUInt32 *protocolVersion, SceUInt32 *packageVersion,
		    SceUInt32 *boardVersion) =
	sceDeci4pCpupInquireVersionForDriver;	/* name unknown */
SceUID g_rtcMgrCpMutex = -1;		/* name unknown */

int rtcMgrGetCpRtc(SceUInt32 *time, int physical)	/* name unknown */
{
	int ret;

	if (g_rtcMgrCpup == NULL)
		return SCE_SBL_ERROR_RTC_NO_CP;
	SceKernelSuspendForDriver_4DF40893(0);
	ret = sceDeci4pCpupGetRtcForDriver(time, physical);
	SceKernelSuspendForDriver_2BB92967(0);
	return ret == 0 ? 0 : SCE_SBL_ERROR_RTC_CP;
}

int _sceSblRtcMgrSetCpRtc(SceUInt32 time, int physical, const void *key)	/* name unknown */
{
	SceUInt32 version, packageVersion, boardVersion;
	int ret;

	if (g_rtcMgrCpup == NULL)
		return SCE_SBL_ERROR_RTC_NO_CP;
	SceKernelSuspendForDriver_4DF40893(0);
	ret = sceDeci4pCpupInquireVersionForDriver(&version, &packageVersion, &boardVersion);
	SceKernelSuspendForDriver_2BB92967(0);
	if (ret != 0)
		return SCE_SBL_ERROR_RTC_CP_VERSION;
	ret = sceKernelLockMutex(g_rtcMgrCpMutex, 1, NULL);
	if (ret < 0)
		return ret;
	SceKernelSuspendForDriver_4DF40893(0);
	if (key == NULL)
		sceDeci4pCpupSetRtcForDriver(time, physical);
	else if (version >= RTC_CP_KEY_VERSION)
		sceDeci4pCpupSetRtcWithActivationKeyForDriver(time, physical, key);
	SceKernelSuspendForDriver_2BB92967(0);
	return sceKernelUnlockMutex(g_rtcMgrCpMutex, 1);
}

int rtcMgrSetCpActivationKey(void)	/* name unknown */
{
	SceActivationKey key;
	int ret;

	ret = SceSblPostSsMgrForDriver_F7F1015B(&key);
	if (ret == 0)
		ret = _sceSblRtcMgrSetCpRtc(0, 0, &key);
	return ret;
}

int rtcMgrSetCpRtcPhysicalAndKey(SceUInt32 time)	/* name unknown */
{
	SceActivationKey key;
	int ret;

	ret = SceSblPostSsMgrForDriver_F7F1015B(&key);
	if (ret == 0)
		ret = _sceSblRtcMgrSetCpRtc(time, 1, &key);
	return ret;
}

int SceSblPostSsMgrForDriver_3F9BDEDF(SceUInt32 time)
{
	return _sceSblRtcMgrSetCpRtc(time, 1, NULL);
}

int SceSblPostSsMgrForDriver_942010A0(SceUInt32 *time)
{
	return rtcMgrGetCpRtc(time, 1);
}

int SceSblPostSsMgrForDriver_DC68EE5C(void)
{
	return SCE_SBL_ERROR_RTC_UNSUPPORTED;
}

int SceSblPostSsMgrForDriver_DE5150FE(SceUInt32 *time)
{
	return rtcMgrGetCpRtc(time, 0);
}

int rtcMgrGetCpSerialId(void *serialId)	/* name unknown */
{
	SceUInt32 version, packageVersion, boardVersion;
	SceSize size;
	int ret;

	if (g_rtcMgrCpup == NULL)
		return SCE_SBL_ERROR_RTC_NO_CP;
	SceKernelSuspendForDriver_4DF40893(0);
	ret = sceDeci4pCpupInquireVersionForDriver(&version, &packageVersion, &boardVersion);
	SceKernelSuspendForDriver_2BB92967(0);
	if (ret != 0)
		return SCE_SBL_ERROR_RTC_CP_VERSION;
	if (version < RTC_CP_SERIAL_VERSION)
		return SCE_SBL_ERROR_RTC_UNSUPPORTED;
	SceKernelSuspendForDriver_4DF40893(0);
	size = RTC_CP_SERIAL_ID_SIZE;
	ret = sceDeci4pCpupGetCpSerialIdForDriver(serialId, &size);
	SceKernelSuspendForDriver_2BB92967(0);
	return ret == 0 ? 0 : SCE_SBL_ERROR_RTC_CP;
}

int sceSblRtcMgrInit(void)
{
	g_rtcMgrCpMutex = sceKernelCreateMutex("SceSblRtcmgrCp", 0, 0, NULL);
	return g_rtcMgrCpMutex >> 31;
}

static inline int rtcMgrIsPhysicalTimeValid(SceUInt32 time)	/* name unknown */
{
	return time >= RTC_CP_PHYSICAL_MIN && time <= RTC_CP_PHYSICAL_MAX;
}

int sceSblRtcMgrSetCpActivationKeyForUser(void)
{
	SceUInt32 state = sceSyscallEnter();
	int ret;

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(state);
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	ret = rtcMgrSetCpActivationKey();
	sceSyscallLeave(state);
	return ret;
}

int sceSblRtcMgrSetCpRtcPhysicalAndKeyForUser(SceUInt32 time)
{
	SceUInt32 state = sceSyscallEnter();
	SceUInt8 productMode;
	int ret;

	if (SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0)) {
		productMode = 0;
		if (pmMgrGetProductMode(&productMode) == 0 && productMode) {
			if (rtcMgrIsPhysicalTimeValid(time)) {
				ret = rtcMgrSetCpRtcPhysicalAndKey(time);
				sceSyscallLeave(state);
			} else {
				sceSyscallLeave(state);
				ret = SCE_SBL_ERROR_RTC_INVALID_ARGUMENT;
			}
			return ret;
		}
	}
	sceSyscallLeave(state);
	return SCE_ERROR_ERRNO_ENOSYS;
}

int sceSblRtcMgrGetCpSerialIdForUser(void *serialId)
{
	SceUInt32 state = sceSyscallEnter();
	SceUInt8 buf[RTC_CP_SERIAL_ID_SIZE];
	SceUInt8 productMode;
	int ret;

	if (SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0)) {
		if (serialId == NULL) {
			sceSyscallLeave(state);
			return SCE_SBL_ERROR_RTC_INVALID_ARGUMENT;
		}
		productMode = 0;
		if (pmMgrGetProductMode(&productMode) == 0 && productMode) {
			ret = rtcMgrGetCpSerialId(buf);
			if (ret == 0)
				ret = sceKernelCopyToUser(serialId, buf, sizeof(buf));
			sceSyscallLeave(state);
			return ret;
		}
	}
	sceSyscallLeave(state);
	return SCE_ERROR_ERRNO_ENOSYS;
}

int sceSblRtcMgrSetCpRtcPhysicalForUser(SceUInt32 time)
{
	SceUInt32 state = sceSyscallEnter();
	SceUInt8 productMode;
	int ret;

	if (SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0)) {
		productMode = 0;
		if (pmMgrGetProductMode(&productMode) == 0 && productMode) {
			if (rtcMgrIsPhysicalTimeValid(time)) {
				ret = _sceSblRtcMgrSetCpRtc(time, 1, NULL);
				sceSyscallLeave(state);
			} else {
				sceSyscallLeave(state);
				ret = SCE_SBL_ERROR_RTC_INVALID_ARGUMENT;
			}
			return ret;
		}
	}
	sceSyscallLeave(state);
	return SCE_ERROR_ERRNO_ENOSYS;
}

int sceSblRtcMgrGetCpRtcPhysicalForUser(SceUInt32 *time)
{
	SceUInt32 state = sceSyscallEnter();
	SceUInt32 value;
	SceUInt8 productMode;
	int ret;

	if (SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0)) {
		if (time == NULL) {
			sceSyscallLeave(state);
			return SCE_SBL_ERROR_RTC_INVALID_ARGUMENT;
		}
		productMode = 0;
		if (pmMgrGetProductMode(&productMode) == 0 && productMode) {
			ret = rtcMgrGetCpRtc(&value, 1);
			if (ret == 0)
				ret = sceKernelCopyToUser(time, &value, sizeof(value));
			sceSyscallLeave(state);
			return ret;
		}
	}
	sceSyscallLeave(state);
	return SCE_ERROR_ERRNO_ENOSYS;
}

int sceSblRtcMgrSetCpRtcLogicalForUser(SceUInt32 time)
{
	SceUInt32 state = sceSyscallEnter();

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(state);
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	sceSyscallLeave(state);
	if (time >= RTC_CP_LOGICAL_MIN && time <= RTC_CP_LOGICAL_MAX)
		return SCE_SBL_ERROR_RTC_UNSUPPORTED;
	return SCE_SBL_ERROR_RTC_INVALID_ARGUMENT;
}

int sceSblRtcMgrGetCpRtcLogicalForUser(SceUInt32 *time)
{
	SceUInt32 state = sceSyscallEnter();
	SceUInt32 value;
	int ret;

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(state);
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	if (time == NULL) {
		sceSyscallLeave(state);
		return SCE_SBL_ERROR_RTC_INVALID_ARGUMENT;
	}
	ret = rtcMgrGetCpRtc(&value, 0);
	if (ret == 0)
		ret = sceKernelCopyToUser(time, &value, sizeof(value));
	sceSyscallLeave(state);
	return ret;
}
