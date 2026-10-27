/*
 * The SceMotionDev syscalls: check the user's arguments, then do the work
 * on kernel buffers and copy the result out.
 */
#include <sysmem/sysmem.h>
#include <intrmgr/syscall.h>
#include <motion_dummy/motion_dummy.h>
#include "motion_dummy.h"
#include <motion/motion_dev.h>

#define CALIBRATION_DATA_MAX	25

int sceMotionDevIsReadyForUser(void)
{
	SceUInt32 state = sceSyscallEnter();
	int ret;

	ret = sceMotionDevIsReady();
	sceSyscallLeave(state);
	return ret;
}

int sceMotionDevGetDeviceInfoForUser(void *info)
{
	SceMotionDevDeviceInfo buffer;
	SceUInt32 state;
	int ret;

	if (info == NULL)
		return MOTION_ERROR_INVALID_ARG;
	state = sceSyscallEnter();
	ret = sceMotionDevGetDeviceInfo(&buffer);
	if (ret >= 0)
		ret = sceKernelCopyToUser(info, &buffer, sizeof(buffer));
	sceSyscallLeave(state);
	return ret;
}

/* Sony bug kept: the kernel call fills nothing, the buffer goes out uninitialised */
int sceMotionDevGetEvaInfoForUser(void *info)
{
	uint8_t buffer[MOTION_EVA_INFO_SIZE];
	SceUInt32 state;
	int ret;

	if (info == NULL)
		return MOTION_ERROR_INVALID_ARG;
	state = sceSyscallEnter();
	ret = sceMotionDevGetEvaInfo();
	if (ret >= 0)
		ret = sceKernelCopyToUser(info, buffer, sizeof(buffer));
	sceSyscallLeave(state);
	return ret;
}

int sceMotionDevGetCalibrationHeaderForUser(SceUInt32 blockId, void *header)
{
	uint8_t buffer[MOTION_CALIBRATION_HEADER_SIZE];
	SceUInt32 state;
	int ret;

	if (header == NULL)
		return MOTION_ERROR_INVALID_ARG;
	state = sceSyscallEnter();
	ret = sceMotionDevGetCalibrationHeader(blockId, buffer);
	if (ret >= 0)
		ret = sceKernelCopyToUser(header, buffer, sizeof(buffer));
	sceSyscallLeave(state);
	return ret;
}

int sceMotionDevGetCalibrationDataForUser(SceUInt32 blockId, void *data, SceUInt32 count)
{
	SceUInt32 state;
	void *buffer;
	int ret, copy;

	if (data == NULL)
		return MOTION_ERROR_INVALID_ARG;
	if (count > CALIBRATION_DATA_MAX)
		return MOTION_ERROR_INVALID_ARG;
	state = sceSyscallEnter();
	buffer = motionAllocHeap(count * MOTION_CALIBRATION_DATA_SIZE);
	if (buffer == NULL) {
		sceSyscallLeave(state);
		return MOTION_ERROR_NO_RESOURCE;
	}
	ret = sceMotionDevGetCalibrationData(blockId, buffer, count);
	if (ret > 0) {
		if (ret >= (int)count)
			ret = count;
		copy = sceKernelCopyToUser(data, buffer, ret * MOTION_CALIBRATION_DATA_SIZE);
		if (copy != 0)
			ret = copy;
	}
	motionFreeHeap(buffer);
	sceSyscallLeave(state);
	return ret;
}

int sceMotionDevReadForUser(SceMotionDevData *data, int count, SceUInt32 *info)
{
	SceUInt32 state = sceSyscallEnter();
	int ret;

	if (info == NULL || data == NULL || count > MOTION_RING_SAMPLES) {
		sceSyscallLeave(state);
		return MOTION_ERROR_INVALID_ARG;
	}
	ret = motionRead(data, count, info);
	sceSyscallLeave(state);
	return ret;
}

int sceMotionDevRead2(int port, SceMotionDevData *data, int count, SceUInt32 *info)
{
	SceUInt32 state = sceSyscallEnter();
	int ret;

	if (info == NULL || data == NULL || count > MOTION_RING_SAMPLES) {
		sceSyscallLeave(state);
		return MOTION_ERROR_INVALID_ARG;
	}
	ret = motionRead2(port, data, count, info);
	sceSyscallLeave(state);
	return ret;
}

int sceMotionDevRead3(int port, MotionDs3Report *report, int count, SceUInt32 *info)
{
	SceUInt32 state = sceSyscallEnter();
	int ret;

	if (info == NULL || report == NULL || count > MOTION_RING_SAMPLES) {
		sceSyscallLeave(state);
		return MOTION_ERROR_INVALID_ARG;
	}
	ret = motionRead3(port, report, count, info);
	sceSyscallLeave(state);
	return ret;
}

int sceMotionDevReadForMagnCalib(SceMotionDevData *data, int count)
{
	SceUInt32 state = sceSyscallEnter();
	int ret;

	if (data == NULL) {
		sceSyscallLeave(state);
		return MOTION_ERROR_INVALID_ARG;
	}
	if (count > MOTION_RING_SAMPLES) {
		sceSyscallLeave(state);
		return MOTION_ERROR_INVALID_ARG;
	}
	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(state);
		return MOTION_ERROR_NOT_PERMITTED;
	}
	ret = motionReadForMagnCalib(data, count);
	sceSyscallLeave(state);
	return ret;
}

int sceMotionDevSetSamplingModeForUser(SceUInt32 mode)
{
	SceUInt32 state = sceSyscallEnter();
	int ret;

	ret = sceMotionDevSetSamplingMode(mode);
	sceSyscallLeave(state);
	return ret;
}

int sceMotionDevSamplingStartForUser(void)
{
	SceUInt32 state = sceSyscallEnter();
	int ret;

	ret = sceMotionDevSamplingStart();
	sceSyscallLeave(state);
	return ret;
}

int sceMotionDevSamplingStart2(int port)
{
	SceUInt32 state = sceSyscallEnter();
	int ret;

	ret = motionSamplingStart2(port);
	sceSyscallLeave(state);
	return ret;
}

int sceMotionDevSamplingStopForUser(void)
{
	SceUInt32 state = sceSyscallEnter();
	int ret;

	ret = sceMotionDevSamplingStop();
	sceSyscallLeave(state);
	return ret;
}

int sceMotionDevSamplingStop2(int port)
{
	SceUInt32 state = sceSyscallEnter();
	int ret;

	ret = motionSamplingStop2(port);
	sceSyscallLeave(state);
	return ret;
}

int sceMotionDevMagnSamplingStart(void)
{
	SceUInt32 state = sceSyscallEnter();
	int ret;

	ret = motionMagnSamplingStart();
	sceSyscallLeave(state);
	return ret;
}

int sceMotionDevMagnSamplingStop(void)
{
	SceUInt32 state = sceSyscallEnter();
	int ret;

	ret = motionMagnSamplingStop();
	sceSyscallLeave(state);
	return ret;
}

int sceMotionDevUpdateMagnCalibData(const void *data, SceUInt32 *tag)
{
	uint8_t buffer[MOTION_MAGN_CALIB_DATA_SIZE];
	SceUInt32 state = sceSyscallEnter();
	SceUInt32 result;
	int ret;

	if (!sceSblACMgrIsSystemProgram(0)) {
		ret = MOTION_ERROR_NOT_PERMITTED;
	} else if (tag == NULL || data == NULL) {
		ret = MOTION_ERROR_INVALID_ARG;
	} else {
		ret = sceKernelCopyFromUser(buffer, data, sizeof(buffer));
		if (ret >= 0) {
			ret = motionUpdateMagnCalibData(buffer, &result);
			if (ret >= 0)
				ret = sceKernelCopyToUser(tag, &result, sizeof(result));
		}
	}
	sceSyscallLeave(state);
	return ret;
}

int sceMotionDevGetCurrentMagnCalibData(void *data)
{
	uint8_t buffer[MOTION_MAGN_CALIB_DATA_SIZE];
	SceUInt32 state = sceSyscallEnter();
	int ret;

	if (data == NULL) {
		ret = MOTION_ERROR_INVALID_ARG;
	} else {
		ret = motionGetCurrentMagnCalibData(buffer);
		if (ret >= 0)
			ret = sceKernelCopyToUser(data, buffer, sizeof(buffer));
	}
	sceSyscallLeave(state);
	return ret;
}

int sceMotionDevGetFactoryMagnCalibData(void *data)
{
	uint8_t buffer[MOTION_MAGN_CALIB_DATA_SIZE];
	SceUInt32 state = sceSyscallEnter();
	int ret;

	if (data == NULL) {
		ret = MOTION_ERROR_INVALID_ARG;
	} else {
		ret = motionGetFactoryMagnCalibData(buffer);
		if (ret >= 0)
			ret = sceKernelCopyToUser(data, buffer, sizeof(buffer));
	}
	sceSyscallLeave(state);
	return ret;
}

int sceMotionDevUpdateMagnStabilityLevel(SceUInt32 level)
{
	SceUInt32 state = sceSyscallEnter();
	int ret;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(state);
		return MOTION_ERROR_NOT_PERMITTED;
	}
	ret = motionUpdateMagnStabilityLevel();
	sceSyscallLeave(state);
	return ret;
}

int sceMotionDevGetCurrentMagnStabilityLevel(SceUInt32 *level)
{
	SceUInt32 state = sceSyscallEnter();
	SceUInt32 value;
	int ret, result;

	if (level == NULL) {
		sceSyscallLeave(state);
		return MOTION_ERROR_INVALID_ARG;
	}
	result = motionGetCurrentMagnStabilityLevel(&value);
	ret = sceKernelCopyToUser(level, &value, sizeof(value));
	sceSyscallLeave(state);
	if (ret >= 0)
		return result;
	return ret;
}

int sceMotionDevGetGyroCalibData(SceMotionDevGyroCalibData *data)
{
	SceMotionDevGyroCalibData buffer;
	SceUInt32 state = sceSyscallEnter();
	int ret;

	if (data == NULL) {
		ret = MOTION_ERROR_INVALID_ARG;
	} else {
		ret = motionGetGyroCalibData(&buffer);
		if (ret == 0)
			ret = sceKernelCopyToUser(data, &buffer, sizeof(buffer));
	}
	sceSyscallLeave(state);
	return ret;
}

int sceMotionDevGetGyroCalibData2(int port, SceMotionDevGyroCalibData *data)
{
	SceMotionDevGyroCalibData buffer;
	SceUInt32 state = sceSyscallEnter();
	int ret;

	if (data == NULL) {
		ret = MOTION_ERROR_INVALID_ARG;
	} else {
		ret = motionGetGyroCalibData2(port, &buffer);
		if (ret == 0)
			ret = sceKernelCopyToUser(data, &buffer, sizeof(buffer));
	}
	sceSyscallLeave(state);
	return ret;
}

int sceMotionDevGetAccCalibData(SceMotionDevAccCalibData *data)
{
	SceMotionDevAccCalibData buffer;
	SceUInt32 state = sceSyscallEnter();
	int ret;

	if (data == NULL) {
		ret = MOTION_ERROR_INVALID_ARG;
	} else {
		ret = motionGetAccCalibData(&buffer);
		if (ret == 0)
			ret = sceKernelCopyToUser(data, &buffer, sizeof(buffer));
	}
	sceSyscallLeave(state);
	return ret;
}

int sceMotionDevGetAccCalibData2(int port, SceMotionDevAccCalibData *data)
{
	SceMotionDevAccCalibData buffer;
	SceUInt32 state = sceSyscallEnter();
	int ret;

	if (data == NULL) {
		ret = MOTION_ERROR_INVALID_ARG;
	} else {
		ret = motionGetAccCalibData2(port, &buffer);
		if (ret == 0)
			ret = sceKernelCopyToUser(data, &buffer, sizeof(buffer));
	}
	sceSyscallLeave(state);
	return ret;
}

int sceMotionDevGetGyroBias(SceMotionDevGyroBiasData *bias)
{
	SceMotionDevGyroBiasData buffer;
	SceUInt32 state = sceSyscallEnter();
	int ret;

	if (bias == NULL) {
		ret = MOTION_ERROR_INVALID_ARG;
	} else {
		ret = motionGetGyroBias(&buffer);
		if (ret >= 0)
			ret = sceKernelCopyToUser(bias, &buffer, sizeof(buffer));
	}
	sceSyscallLeave(state);
	return ret;
}

int sceMotionDevGetGyroBias2(int port, SceMotionDevGyroBiasData *bias)
{
	SceMotionDevGyroBiasData buffer;
	SceUInt32 state = sceSyscallEnter();
	int ret;

	if (bias == NULL) {
		ret = MOTION_ERROR_INVALID_ARG;
	} else {
		ret = motionGetGyroBias2(port, &buffer);
		if (ret >= 0)
			ret = sceKernelCopyToUser(bias, &buffer, sizeof(buffer));
	}
	sceSyscallLeave(state);
	return ret;
}

int sceMotionDevGetDeviceLocation(void *location)
{
	uint8_t buffer[MOTION_DEVICE_LOCATION_SIZE];
	SceUInt32 state = sceSyscallEnter();
	int ret;

	if (location == NULL) {
		ret = MOTION_ERROR_INVALID_ARG;
	} else {
		ret = motionGetDeviceLocation(buffer);
		if (ret >= 0)
			ret = sceKernelCopyToUser(location, buffer, sizeof(buffer));
	}
	sceSyscallLeave(state);
	return ret;
}

int sceMotionDevGetMeasMode(void *info)
{
	uint8_t buffer[MOTION_MODE_INFO_SIZE];
	SceUInt32 state = sceSyscallEnter();
	int ret;

	if (!sceSblACMgrIsSystemProgram(0)) {
		ret = MOTION_ERROR_NOT_PERMITTED;
	} else if (info == NULL) {
		ret = MOTION_ERROR_INVALID_ARG;
	} else {
		ret = motionGetMeasMode(buffer);
		if (ret >= 0)
			ret = sceKernelCopyToUser(info, buffer, sizeof(buffer));
	}
	sceSyscallLeave(state);
	return ret;
}

int sceMotionDevGetDs3CalibData(int port, SceMotionDevDs3CalibData *data)
{
	SceMotionDevDs3CalibData buffer;
	SceUInt32 state = sceSyscallEnter();
	int ret;

	if (data == NULL) {
		ret = MOTION_ERROR_INVALID_ARG;
	} else {
		ret = motionGetDs3CalibData(port, &buffer);
		if (ret == 0)
			ret = sceKernelCopyToUser(data, &buffer, sizeof(buffer));
	}
	sceSyscallLeave(state);
	return ret;
}

int sceMotionDevGetControllerType(int port, uint8_t *type)
{
	uint8_t buffer[1];
	SceUInt32 state = sceSyscallEnter();
	int ret;

	if (type == NULL) {
		ret = MOTION_ERROR_INVALID_ARG;
	} else {
		ret = motionGetControllerType(port, buffer);
		if (ret >= 0)
			ret = sceKernelCopyToUser(type, buffer, sizeof(buffer));
	}
	sceSyscallLeave(state);
	return ret;
}

int sceMotionDevSetGyroFeedBack(int port, int feedback)
{
	SceUInt32 state = sceSyscallEnter();
	int ret;

	ret = motionSetGyroFeedBack(port, feedback);
	sceSyscallLeave(state);
	return ret;
}
