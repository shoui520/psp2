/*
 * SceDriverUser: SceMotion sensor calibration and raw sample conversion.
 *
 * The motion device reports raw accelerometer, gyro and magnetometer counts
 * with factory calibration records; these turn them into scaled values in
 * the device's axis convention, which depends on the device type.
 */

#include "driver_user.h"
#include "motion.h"

int sceKernelDelayThread(SceUInt32 usec);

static inline int IsNonZero(float value)
{
	return value > 0.0f || value < 0.0f;
}

/* accelerometer counts to the device frame */
MOTION_VFP int MotionConvertAcc(MotionSample *sample, const MotionAxisCalibration *calibration,
				double x, double y, double z)
{
	x -= calibration->offset.x;
	y -= calibration->offset.y;
	z -= calibration->offset.z;
	x *= x >= 0.0 ? calibration->positive.x : calibration->negative.x;
	y *= y >= 0.0 ? calibration->positive.y : calibration->negative.y;
	z *= z >= 0.0 ? calibration->positive.z : calibration->negative.z;
	if (g_motion.deviceType <= 4) {
		y = -y;
		z = -z;
	}
	sample->acc.x = y;
	sample->acc.y = x;
	sample->acc.z = z;
	sample->accValid = 1;
	return 0;
}

/* gyro counts to the device frame */
MOTION_VFP int MotionConvertGyro(MotionSample *sample, const MotionAxisCalibration *calibration,
				 double x, double y, double z)
{
	x -= calibration->offset.x;
	y -= calibration->offset.y;
	z -= calibration->offset.z;
	x *= x >= 0.0 ? calibration->positive.x : calibration->negative.x;
	y *= y >= 0.0 ? calibration->positive.y : calibration->negative.y;
	z *= z >= 0.0 ? calibration->positive.z : calibration->negative.z;
	if (g_motion.deviceType > 6 || g_motion.deviceType == 5) {
		sample->gyro.x = -y;
		sample->gyro.y = -x;
		sample->gyro.z = -z;
	} else if (g_motion.deviceType == 6) {
		sample->gyro.y = -x;
		sample->gyro.z = -z;
		sample->gyro.x = y;
	} else {
		sample->gyro.x = y;
		sample->gyro.y = -x;
		sample->gyro.z = z;
	}
	return 0;
}

/* magnetometer counts to the device frame, with offset and scale set index */
MOTION_VFP int MotionConvertMagn(int index, MotionSample *sample, const MotionMagnCalibration *calibration,
				 double x, double y, double z)
{
	const SceFVector3 *offset = &calibration->vector[index];
	const SceFVector3 *scale = &calibration->vector[index + 2];

	x = (x - offset->x) * scale->x / 10.0;
	y = (y - offset->y) * scale->y / 10.0;
	z = (z - offset->z) * scale->z / 10.0;
	if (g_motion.deviceType > 5) {
		sample->magn.x = -y;
		sample->magn.y = -x;
		sample->magn.z = -z;
	} else if (g_motion.deviceType == 5) {
		sample->magn.y = x;
		sample->magn.x = -y;
		sample->magn.z = z;
	} else {
		sample->magn.y = y;
		sample->magn.x = -x;
		sample->magn.z = z;
	}
	sample->magnValid = 1;
	return 0;
}

void MotionMagnOffset(SceFVector4 *out, const MotionMagnCalibration *calibration)
{
	SceFVector4 vector = { calibration->vector[1].x, calibration->vector[1].y,
			       calibration->vector[1].z, 0.0f };

	*out = vector;
}

void MotionMagnScale(SceFVector4 *out, const MotionMagnCalibration *calibration)
{
	SceFVector4 vector = { calibration->vector[3].x, calibration->vector[3].y,
			       calibration->vector[3].z, 0.0f };

	*out = vector;
}

void MotionReadMagnCalibration(MotionMagnCalibration *calibration)
{
	MotionDevMagnCalibData data;

	sceMotionDevGetCurrentMagnCalibData(&data);
	*calibration = data.calibration;
}

void MotionReadGyroBias(SceFVector3 *bias)
{
	MotionDevGyroBiasData data;

	sceMotionDevGetGyroBias(&data);
	*bias = data.bias;
}

void MotionSetVector3(SceFVector3 *a, SceFVector3 *b, SceFVector3 *c, const SceFVector3 *value)
{
	*a = *value;
	*b = *value;
	*c = *value;
}

int MotionReadGyroCalibration(int port, MotionAxisCalibration *calibration)
{
	MotionDevGyroCalibData data;
	float spanX, spanY, spanZ;
	int ret;

	if (port == -1)
		ret = sceMotionDevGetGyroCalibData(&data);
	else
		ret = sceMotionDevGetGyroCalibData2(port, &data);
	if (ret < 0)
		return ret;
	calibration->offset = data.zero;
	spanX = data.point[0].raw.x - data.zero.x;
	if (!IsNonZero(spanX))
		return SCE_MOTION_ERROR_CALIB_READ_FAIL;
	spanY = data.point[1].raw.y - data.zero.y;
	if (!IsNonZero(spanY))
		return SCE_MOTION_ERROR_CALIB_READ_FAIL;
	spanZ = data.point[2].raw.z - data.zero.z;
	if (!IsNonZero(spanZ))
		return SCE_MOTION_ERROR_CALIB_READ_FAIL;
	calibration->negative.x = data.point[3].value / (data.point[3].raw.x - data.zero.x);
	calibration->negative.y = data.point[4].value / (data.point[4].raw.y - data.zero.y);
	calibration->negative.z = data.point[5].value / (data.point[5].raw.z - data.zero.z);
	calibration->positive.y = data.point[1].value / spanY;
	calibration->positive.x = data.point[0].value / spanX;
	calibration->positive.z = data.point[2].value / spanZ;
	return 0;
}

int MotionReadAccCalibration(int port, MotionAxisCalibration *calibration)
{
	MotionDevAccCalibData data;
	float spanX, spanY, spanZ;
	int ret;

	if (port == -1)
		ret = sceMotionDevGetAccCalibData(&data);
	else
		ret = sceMotionDevGetAccCalibData2(port, &data);
	if (ret < 0)
		return ret;
	/* each axis rests at the mean of the four records that do not excite it */
	calibration->offset.x = (data.point[1].raw.x + data.point[2].raw.x + data.point[4].raw.x +
				 data.point[5].raw.x) * 0.25f;
	calibration->offset.y = (data.point[0].raw.y + data.point[2].raw.y + data.point[3].raw.y +
				 data.point[5].raw.y) * 0.25f;
	calibration->offset.z = (data.point[0].raw.z + data.point[1].raw.z + data.point[3].raw.z +
				 data.point[4].raw.z) * 0.25f;
	spanX = data.point[0].raw.x - calibration->offset.x;
	if (!IsNonZero(spanX))
		return SCE_MOTION_ERROR_CALIB_READ_FAIL;
	spanY = data.point[1].raw.y - calibration->offset.y;
	if (!IsNonZero(spanY))
		return SCE_MOTION_ERROR_CALIB_READ_FAIL;
	spanZ = data.point[2].raw.z - calibration->offset.z;
	if (!IsNonZero(spanZ))
		return SCE_MOTION_ERROR_CALIB_READ_FAIL;
	calibration->negative.x = data.point[3].value / (data.point[3].raw.x - calibration->offset.x);
	calibration->negative.y = data.point[4].value / (data.point[4].raw.y - calibration->offset.y);
	calibration->negative.z = data.point[5].value / (data.point[5].raw.z - calibration->offset.z);
	calibration->positive.x = data.point[0].value / spanX;
	calibration->positive.y = data.point[1].value / spanY;
	calibration->positive.z = data.point[2].value / spanZ;
	return 0;
}

/* waits up to a second for the device, then reads all its calibration */
int MotionReadCalibration(MotionAxisCalibration *gyro, MotionAxisCalibration *acc,
			  MotionMagnCalibration *magn)
{
	MotionDevDeviceInfo info;
	MotionDevMagnCalibData data;
	int retry = 0;
	int ret;

	while (!sceMotionDevIsReady()) {
		retry++;
		sceKernelDelayThread(100000);
		if (retry > 9)
			return SCE_MOTION_ERROR_CALIB_READ_FAIL;
	}
	ret = sceMotionDevGetDeviceInfo(&info);
	if (ret < 0)
		return ret;
	g_motion.deviceType = info.type;
	if (MotionReadAccCalibration(-1, acc) < 0 || MotionReadGyroCalibration(-1, gyro) < 0)
		return SCE_MOTION_ERROR_CALIB_READ_FAIL;
	if (sceMotionDevGetCurrentMagnCalibData(&data) >= 0)
		*magn = data.calibration;
	return 0;
}
