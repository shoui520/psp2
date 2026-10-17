/* User entry points reuse the motion driver's syscall argument checks. */
#include "motion_manager.h"
#include <sysmem/sysmem.h>
#include <acmgr/acmgr.h>
#include <intrmgr/syscall.h>

struct SceMotionDevDs3CalibData;

int ksceMotionDevIsReady(void);
int ksceMotionDevGetDeviceInfo(SceMotionDevDeviceInfo *info);
int ksceMotionDevGetEvaInfo(void *statistics);
int ksceMotionDevGetCalibrationHeader(unsigned int block,
	SceMotionDevCalibrationHeader *header);
int ksceMotionDevGetCalibrationData(unsigned int block,
	SceMotionDevCalibrationData *data, unsigned int count);
int motionRead(SceMotionDevData *data, int count, unsigned int *info);
int motionReadForMagnCalib(SceMotionDevData *data, int count);
int ksceMotionDevSetSamplingMode(unsigned int mode);
int ksceMotionDevSamplingStart(void);
int ksceMotionDevSamplingStop(void);
int motionMagnSamplingStart(void);
int motionMagnSamplingStop(void);
int motionUpdateMagnCalibData(const SceMotionDevMagnCalibData *data, uint32_t *tag);
int motionGetCurrentMagnCalibData(SceMotionDevMagnCalibData *data);
int motionGetFactoryMagnCalibData(SceMotionDevMagnCalibData *data);
int motionUpdateMagnStabilityLevel(unsigned int level);
int motionGetCurrentMagnStabilityLevel(uint32_t *level);
int motionGetGyroCalibData(SceMotionDevGyroCalibData *data);
int motionGetAccCalibData(SceMotionDevAccCalibData *data);
int motionGetGyroBias(SceMotionDevGyroBiasData *bias);
int motionGetDeviceLocation(SceMotionDevDeviceLocation *location);
uint32_t motionGetMeasMode(uint8_t *mode);
int motionGetControllerType(int port, uint8_t *type);

int sceMotionDevIsReady(void)
{
	SceUInt32 state = sceSyscallEnter();
	int result;

	result = ksceMotionDevIsReady();
	sceSyscallLeave(state);
	return result;
}

int sceMotionDevGetDeviceInfo(SceMotionDevDeviceInfo *info)
{
	SceMotionDevDeviceInfo buffer;
	SceUInt32 state;
	int result;

	if (info == NULL)
		return (int)0x80360001;
	state = sceSyscallEnter();
	result = ksceMotionDevGetDeviceInfo(&buffer);
	if (result >= 0)
		result = SceSysmemForDriver_6D88EF8A(info, &buffer, sizeof(buffer));
	sceSyscallLeave(state);
	return result;
}

int sceMotionDevGetEvaInfo(void *info)
{
	MotionDeviceStatistics buffer;
	SceUInt32 state;
	int result;

	if (info == NULL)
		return (int)0x80360001;
	state = sceSyscallEnter();
	result = ksceMotionDevGetEvaInfo(&buffer);
	if (result >= 0)
		result = SceSysmemForDriver_6D88EF8A(info, &buffer, sizeof(buffer));
	sceSyscallLeave(state);
	return result;
}

int sceMotionDevGetCalibrationHeader(unsigned int block,
	SceMotionDevCalibrationHeader *header)
{
	SceMotionDevCalibrationHeader buffer;
	SceUInt32 state;
	int result;

	if (header == NULL)
		return (int)0x80360001;
	state = sceSyscallEnter();
	result = ksceMotionDevGetCalibrationHeader(block, &buffer);
	if (result >= 0)
		result = SceSysmemForDriver_6D88EF8A(header, &buffer, sizeof(buffer));
	sceSyscallLeave(state);
	return result;
}

int sceMotionDevGetCalibrationData(unsigned int block,
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
	result = ksceMotionDevGetCalibrationData(block, buffer, count);
	if (result > 0) {
		if (count < (unsigned int)result)
			result = count;
		copy = SceSysmemForDriver_6D88EF8A(data, buffer, result * sizeof(*buffer));
		if (copy != 0)
			result = copy;
	}
	motionFreeHeap(buffer);
	sceSyscallLeave(state);
	return result;
}

int sceMotionDevRead(SceMotionDevData *data, int count, SceUInt32 *info)
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

int sceMotionDevSetSamplingMode(unsigned int mode)
{
	SceUInt32 state = sceSyscallEnter();
	int result;

	result = ksceMotionDevSetSamplingMode(mode);
	sceSyscallLeave(state);
	return result;
}

int sceMotionDevSamplingStart(void)
{
	SceUInt32 state = sceSyscallEnter();
	int result;

	result = ksceMotionDevSamplingStart();
	sceSyscallLeave(state);
	return result;
}

int sceMotionDevSamplingStart2(int port)
{
	return 0;
}

int sceMotionDevSamplingStop(void)
{
	SceUInt32 state = sceSyscallEnter();
	int result;

	result = ksceMotionDevSamplingStop();
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
	result = SceSysmemForDriver_BC996A7A(&buffer, data, sizeof(buffer));
	if (result >= 0) {
		result = motionUpdateMagnCalibData(&buffer, &value);
		if (result >= 0)
			result = SceSysmemForDriver_6D88EF8A(tag, &value, sizeof(value));
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
		result = SceSysmemForDriver_6D88EF8A(data, &buffer, sizeof(buffer));
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
		result = SceSysmemForDriver_6D88EF8A(data, &buffer, sizeof(buffer));
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
	copy = SceSysmemForDriver_6D88EF8A(level, &value, sizeof(value));
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
		result = SceSysmemForDriver_6D88EF8A(data, &buffer, sizeof(buffer));
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
		result = SceSysmemForDriver_6D88EF8A(data, &buffer, sizeof(buffer));
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
		result = SceSysmemForDriver_6D88EF8A(bias, &buffer, sizeof(buffer));
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
		result = SceSysmemForDriver_6D88EF8A(location, &buffer, sizeof(buffer));
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
		result = SceSysmemForDriver_6D88EF8A(info, buffer, sizeof(buffer));
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
		result = SceSysmemForDriver_6D88EF8A(type, &value, sizeof(value));
	sceSyscallLeave(state);
	return result;
}

int sceMotionDevSetGyroFeedBack(int port, int feedback)
{
	return 0;
}
