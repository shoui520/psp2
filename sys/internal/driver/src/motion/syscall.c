/* User entry points reuse the motion driver's syscall argument checks. */
#include "motion_manager.h"
#include <sysmem/sysmem.h>
#include <acmgr/acmgr.h>
#include <intrmgr/syscall.h>
#include <motion/motion_dev.h>

struct SceMotionDevDs3CalibData;

int sceMotionDevIsReadyForUser(void)
{
	SceUInt32 state = sceSyscallEnter();
	int result;

	result = sceMotionDevIsReady();
	sceSyscallLeave(state);
	return result;
}

int sceMotionDevGetDeviceInfoForUser(SceMotionDevDeviceInfo *info)
{
	SceMotionDevDeviceInfo buffer;
	SceUInt32 state;
	int result;

	if (info == NULL)
		return (int)0x80360001;
	state = sceSyscallEnter();
	result = sceMotionDevGetDeviceInfo(&buffer);
	if (result >= 0)
		result = sceKernelCopyToUser(info, &buffer, sizeof(buffer));
	sceSyscallLeave(state);
	return result;
}

int sceMotionDevGetEvaInfoForUser(void *info)
{
	MotionDeviceStatistics buffer;
	SceUInt32 state;
	int result;

	if (info == NULL)
		return (int)0x80360001;
	state = sceSyscallEnter();
	result = sceMotionDevGetEvaInfo(&buffer);
	if (result >= 0)
		result = sceKernelCopyToUser(info, &buffer, sizeof(buffer));
	sceSyscallLeave(state);
	return result;
}

int sceMotionDevGetCalibrationHeaderForUser(unsigned int block,
	SceMotionDevCalibrationHeader *header)
{
	SceMotionDevCalibrationHeader buffer;
	SceUInt32 state;
	int result;

	if (header == NULL)
		return (int)0x80360001;
	state = sceSyscallEnter();
	result = sceMotionDevGetCalibrationHeader(block, &buffer);
	if (result >= 0)
		result = sceKernelCopyToUser(header, &buffer, sizeof(buffer));
	sceSyscallLeave(state);
	return result;
}

int sceMotionDevGetCalibrationDataForUser(unsigned int block,
	SceMotionDevCalibrationData *data, unsigned int count)
{
	SceMotionDevCalibrationData *buffer;
	SceUInt32 state;
	int result, copy;

	if (data == NULL)
		return (int)0x80360001;
	if (count > 25)
		return (int)0x80360001;
	state = sceSyscallEnter();
	buffer = motionAllocHeap(count * sizeof(*buffer));
	if (buffer == NULL) {
		sceSyscallLeave(state);
		return (int)0x80360005;
	}
	result = sceMotionDevGetCalibrationData(block, buffer, count);
	if (result > 0) {
		if (count < (unsigned int)result)
			result = count;
		copy = sceKernelCopyToUser(data, buffer, result * sizeof(*buffer));
		if (copy != 0)
			result = copy;
	}
	motionFreeHeap(buffer);
	sceSyscallLeave(state);
	return result;
}

int sceMotionDevReadForUser(SceMotionDevData *data, int count, SceUInt32 *info)
{
	SceUInt32 state = sceSyscallEnter();
	int result;

	if (data == NULL || info == NULL || count > MOTION_RING_SAMPLES) {
		sceSyscallLeave(state);
		return (int)0x80360001;
	}
	result = motionRead(data, count, info);
	sceSyscallLeave(state);
	return result;
}

int sceMotionDevRead2(int port, SceMotionDevData *data, int count, SceUInt32 *info)
{
	return 0;
}

struct MotionDs3Report;

int sceMotionDevRead3(int port, struct MotionDs3Report *report, int count,
	SceUInt32 *info)
{
	return 0;
}

int sceMotionDevReadForMagnCalib(SceMotionDevData *data, int count)
{
	SceUInt32 state = sceSyscallEnter();
	int result;

	if (data == NULL) {
		sceSyscallLeave(state);
		return (int)0x80360001;
	}
	if (count > MOTION_RING_SAMPLES) {
		sceSyscallLeave(state);
		return (int)0x80360001;
	}
	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(state);
		return (int)0x80360002;
	}
	result = motionReadForMagnCalib(data, count);
	sceSyscallLeave(state);
	return result;
}

int sceMotionDevSetSamplingModeForUser(unsigned int mode)
{
	SceUInt32 state = sceSyscallEnter();
	int result;

	result = sceMotionDevSetSamplingMode(mode);
	sceSyscallLeave(state);
	return result;
}

int sceMotionDevSamplingStartForUser(void)
{
	SceUInt32 state = sceSyscallEnter();
	int result;

	result = sceMotionDevSamplingStart();
	sceSyscallLeave(state);
	return result;
}

int sceMotionDevSamplingStart2(int port)
{
	return 0;
}

int sceMotionDevSamplingStopForUser(void)
{
	SceUInt32 state = sceSyscallEnter();
	int result;

	result = sceMotionDevSamplingStop();
	sceSyscallLeave(state);
	return result;
}

int sceMotionDevSamplingStop2(int port)
{
	return 0;
}

int sceMotionDevMagnSamplingStart(void)
{
	SceUInt32 state = sceSyscallEnter();
	int result;

	result = motionMagnSamplingStart();
	sceSyscallLeave(state);
	return result;
}

int sceMotionDevMagnSamplingStop(void)
{
	SceUInt32 state = sceSyscallEnter();
	int result;

	result = motionMagnSamplingStop();
	sceSyscallLeave(state);
	return result;
}

int sceMotionDevUpdateMagnCalibData(const SceMotionDevMagnCalibData *data,
	SceUInt32 *tag)
{
	SceMotionDevMagnCalibData buffer;
	SceUInt32 state = sceSyscallEnter();
	SceUInt32 value;
	int result;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(state);
		return (int)0x80360002;
	}
	if (data == NULL || tag == NULL) {
		sceSyscallLeave(state);
		return (int)0x80360001;
	}
	result = sceKernelCopyFromUser(&buffer, data, sizeof(buffer));
	if (result >= 0) {
		result = motionUpdateMagnCalibData(&buffer, &value);
		if (result >= 0)
			result = sceKernelCopyToUser(tag, &value, sizeof(value));
	}
	sceSyscallLeave(state);
	return result;
}

int sceMotionDevGetCurrentMagnCalibData(SceMotionDevMagnCalibData *data)
{
	SceMotionDevMagnCalibData buffer;
	SceUInt32 state = sceSyscallEnter();
	int result;

	if (data == NULL) {
		sceSyscallLeave(state);
		return (int)0x80360001;
	}
	result = motionGetCurrentMagnCalibData(&buffer);
	if (result >= 0)
		result = sceKernelCopyToUser(data, &buffer, sizeof(buffer));
	sceSyscallLeave(state);
	return result;
}

int sceMotionDevGetFactoryMagnCalibData(SceMotionDevMagnCalibData *data)
{
	SceMotionDevMagnCalibData buffer;
	SceUInt32 state = sceSyscallEnter();
	int result;

	if (data == NULL) {
		sceSyscallLeave(state);
		return (int)0x80360001;
	}
	result = motionGetFactoryMagnCalibData(&buffer);
	if (result >= 0)
		result = sceKernelCopyToUser(data, &buffer, sizeof(buffer));
	sceSyscallLeave(state);
	return result;
}

int sceMotionDevUpdateMagnStabilityLevel(SceUInt32 level)
{
	SceUInt32 state = sceSyscallEnter();
	int result;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(state);
		return (int)0x80360002;
	}
	result = motionUpdateMagnStabilityLevel(level);
	sceSyscallLeave(state);
	return result;
}

int sceMotionDevGetCurrentMagnStabilityLevel(SceUInt32 *level)
{
	SceUInt32 state = sceSyscallEnter();
	SceUInt32 value;
	int result, copy;

	if (level == NULL) {
		sceSyscallLeave(state);
		return (int)0x80360001;
	}
	result = motionGetCurrentMagnStabilityLevel(&value);
	copy = sceKernelCopyToUser(level, &value, sizeof(value));
	sceSyscallLeave(state);
	if (copy >= 0)
		return result;
	return copy;
}

int sceMotionDevGetGyroCalibData(SceMotionDevGyroCalibData *data)
{
	SceMotionDevGyroCalibData buffer;
	SceUInt32 state = sceSyscallEnter();
	int result;

	if (data == NULL) {
		sceSyscallLeave(state);
		return (int)0x80360001;
	}
	result = motionGetGyroCalibData(&buffer);
	if (result == 0)
		result = sceKernelCopyToUser(data, &buffer, sizeof(buffer));
	sceSyscallLeave(state);
	return result;
}

int sceMotionDevGetGyroCalibData2(int port, SceMotionDevGyroCalibData *data)
{
	return 0;
}

int sceMotionDevGetAccCalibData(SceMotionDevAccCalibData *data)
{
	SceMotionDevAccCalibData buffer;
	SceUInt32 state = sceSyscallEnter();
	int result;

	if (data == NULL) {
		sceSyscallLeave(state);
		return (int)0x80360001;
	}
	result = motionGetAccCalibData(&buffer);
	if (result == 0)
		result = sceKernelCopyToUser(data, &buffer, sizeof(buffer));
	sceSyscallLeave(state);
	return result;
}

int sceMotionDevGetAccCalibData2(int port, SceMotionDevAccCalibData *data)
{
	return 0;
}

int sceMotionDevGetGyroBias(SceMotionDevGyroBiasData *bias)
{
	SceMotionDevGyroBiasData buffer;
	SceUInt32 state = sceSyscallEnter();
	int result;

	if (bias == NULL) {
		sceSyscallLeave(state);
		return (int)0x80360001;
	}
	result = motionGetGyroBias(&buffer);
	if (result >= 0)
		result = sceKernelCopyToUser(bias, &buffer, sizeof(buffer));
	sceSyscallLeave(state);
	return result;
}

int sceMotionDevGetGyroBias2(int port, SceMotionDevGyroBiasData *bias)
{
	return 0;
}

int sceMotionDevGetDeviceLocation(SceMotionDevDeviceLocation *location)
{
	SceMotionDevDeviceLocation buffer;
	SceUInt32 state = sceSyscallEnter();
	int result;

	if (location == NULL) {
		sceSyscallLeave(state);
		return (int)0x80360001;
	}
	result = motionGetDeviceLocation(&buffer);
	if (result >= 0)
		result = sceKernelCopyToUser(location, &buffer, sizeof(buffer));
	sceSyscallLeave(state);
	return result;
}

int sceMotionDevGetMeasMode(void *info)
{
	uint8_t buffer[8];
	SceUInt32 state = sceSyscallEnter();
	int result;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(state);
		return (int)0x80360002;
	}
	if (info == NULL) {
		sceSyscallLeave(state);
		return (int)0x80360001;
	}
	result = motionGetMeasMode(buffer);
	if (result >= 0)
		result = sceKernelCopyToUser(info, buffer, sizeof(buffer));
	sceSyscallLeave(state);
	return result;
}

int sceMotionDevGetDs3CalibData(int port, struct SceMotionDevDs3CalibData *data)
{
	return 0;
}

int sceMotionDevGetControllerType(int port, uint8_t *type)
{
	SceUInt32 state = sceSyscallEnter();
	uint8_t value;
	int result;

	if (type == NULL) {
		sceSyscallLeave(state);
		return (int)0x80360001;
	}
	result = motionGetControllerType(port, &value);
	if (result >= 0)
		result = sceKernelCopyToUser(type, &value, sizeof(value));
	sceSyscallLeave(state);
	return result;
}

int sceMotionDevSetGyroFeedBack(int port, int feedback)
{
	return 0;
}
