/* SceGps: the bodies of the user API, in syscall context.
 * Internal names unknown: descriptive names follow the observed behavior.
 */
#include "gps_internal.h"
#include <intrmgr/syscall.h>

/* the user API is for system programs, or any program on a tool */
static inline __attribute__((always_inline)) int permission(void)
{
	uint8_t mode = 0;
	int ret;

	if (sceSblACMgrIsSystemProgram(0) || SceSblPostSsMgrForDriver_128FB35A(0) ||
	    SceQafMgrForDriver_BFD5E463())
		return 0;
	ret = sceSblPmMgrGetProductMode(&mode);
	if (ret < 0)
		return ret;
	if (mode != 1)
		return (int)SCE_ERROR_ERRNO_ENOSYS;
	return 0;
}

int openCallback(SceUID callback)
{
	SceUInt32 saved = sceSyscallEnter();
	SceUID uid;
	int ret = permission();

	if (ret)
		goto leave;
	uid = scePUIDtoGUID(0, callback);
	if (uid < 0) {
		ret = uid | 0x40000000;
		goto leave;
	}
	if (!gpsContext()) {
		ret = GPS_ERROR(1);
		goto leave;
	}
	ret = gpsCheckState(0);
	if (ret)
		goto leave;
	gpsLock(GPS_LOCK_API);
	ret = gpsSetCallback(uid);
	gpsUnlock(GPS_LOCK_API);
leave:
	sceSyscallLeave(saved);
	return ret;
}

int closeCallback(void)
{
	SceUInt32 saved = sceSyscallEnter();
	int ret = permission();

	if (ret)
		goto leave;
	if (!gpsContext()) {
		ret = GPS_ERROR(1);
		goto leave;
	}
	ret = gpsCheckState(3);
	if (ret)
		goto leave;
	gpsLock(GPS_LOCK_API);
	ret = gpsClose();
	gpsUnlock(GPS_LOCK_API);
leave:
	sceSyscallLeave(saved);
	return ret;
}

int startAcquisition(unsigned mode)
{
	SceUInt32 saved = sceSyscallEnter();
	int ret = permission();

	if (ret)
		goto leave;
	if ((mode & 15) > 1 || (mode & ~31U)) {
		ret = GPS_ERROR(32);
		goto leave;
	}
	gpsLock(GPS_LOCK_API);
	ret = gpsStart(mode);
	gpsUnlock(GPS_LOCK_API);
leave:
	sceSyscallLeave(saved);
	return ret;
}

int stopAcquisition(void)
{
	SceUInt32 saved = sceSyscallEnter();
	int ret = permission();

	if (ret)
		goto leave;
	gpsLock(GPS_LOCK_API);
	ret = gpsStop();
	gpsUnlock(GPS_LOCK_API);
leave:
	sceSyscallLeave(saved);
	return ret;
}

int requestIoctl(unsigned command, void *data, unsigned size)
{
	SceUInt32 saved = sceSyscallEnter();
	GpsDeviceInfo info;
	int ret = permission();

	if (ret)
		goto leave;
	ret = gpsCheckState(7);
	if (ret)
		goto leave;
	ret = getDeviceInfo(&info);
	if (ret)
		goto leave;
	if ((command & 0xf000) != info.device << 12) {
		ret = GPS_ERROR(32);
		goto leave;
	}
	if ((command & 0xf0000000) == 0xa0000000) {
		/* the device answers at once: Sony keeps its output, not its result */
		deviceIoctl(command, data, size);
	} else if ((command & 0xf0000000) == 0x50000000) {
		/* the worker sends it */
		gpsLock(GPS_LOCK_API);
		ret = gpsStagePending(command, data, size);
		if (!ret)
			ret = gpsSignal(7, 0);
		gpsUnlock(GPS_LOCK_API);
	} else
		ret = GPS_ERROR(34);
leave:
	sceSyscallLeave(saved);
	return ret;
}

int chooseDevice(unsigned device)
{
	SceUInt32 saved = sceSyscallEnter();
	int ret = permission();

	if (ret)
		goto leave;
	ret = gpsCheckState(6);
	if (ret)
		goto leave;
	gpsLock(GPS_LOCK_API);
	ret = gpsSelectDevice(device);
	gpsUnlock(GPS_LOCK_API);
leave:
	sceSyscallLeave(saved);
	return ret;
}

int readState(unsigned *state)
{
	SceUInt32 saved = sceSyscallEnter();
	int ret = permission();

	if (ret)
		goto leave;
	if (!state) {
		ret = GPS_ERROR(32);
		goto leave;
	}
	gpsLock(GPS_LOCK_API);
	*state = gpsState();
	ret = gpsSuspended();
	gpsUnlock(GPS_LOCK_API);
leave:
	sceSyscallLeave(saved);
	return ret;
}

int readLocation(GpsLocation *location, GpsSatellites *satellites)
{
	SceUInt32 saved = sceSyscallEnter();
	int ret = permission();

	if (ret)
		goto leave;
	if (!location || !satellites) {
		ret = GPS_ERROR(32);
		goto leave;
	}
	ret = gpsCheckState(4);
	if (ret)
		goto leave;
	gpsLock(GPS_LOCK_API);
	ret = gpsGetData(location, satellites);
	gpsUnlock(GPS_LOCK_API);
leave:
	sceSyscallLeave(saved);
	return ret;
}

int readInfo(GpsDeviceInfo *info)
{
	SceUInt32 saved = sceSyscallEnter();
	int ret = permission();

	if (ret)
		goto leave;
	if (!info) {
		ret = GPS_ERROR(32);
		goto leave;
	}
	ret = gpsCheckState(5);
	if (ret)
		goto leave;
	gpsLock(GPS_LOCK_API);
	ret = getDeviceInfo(info);
	gpsUnlock(GPS_LOCK_API);
leave:
	sceSyscallLeave(saved);
	return ret;
}

int queryDevice(void)
{
	SceUInt32 saved = sceSyscallEnter();
	int ret = permission();

	if (!ret)
		ret = gpsIsDevice();
	sceSyscallLeave(saved);
	return ret;
}

int resumeNotices(void)
{
	SceUInt32 saved = sceSyscallEnter();
	int ret = permission();

	if (ret)
		goto leave;
	gpsLock(GPS_LOCK_API);
	ret = gpsResumeCallback();
	gpsUnlock(GPS_LOCK_API);
leave:
	sceSyscallLeave(saved);
	return ret;
}

/* stop a device that keeps running between fixes */
int specialStop(const GpsDriver *driver)
{
	if (!driver)
		return GPS_ERROR(32);
	if (!driver->ioctl)
		return GPS_ERROR(34);
	return driver->ioctl(0x53001000, NULL, 0, gpsState());
}

/* whether the device's time may set the clock */
int canSetClock(const GpsDriver *driver)
{
	unsigned result = 0;

	if (!driver)
		return GPS_ERROR(32);
	if (driver->ioctl && (driver->ioctl(0xa3001000, &result, 4, gpsState()) || !(result & 1)))
		return GPS_ERROR(9);
	return 0;
}
