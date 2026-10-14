/*
 * SceDriverUser: SceMotion, the motion sensor library.
 *
 * Samples read from the SceMotionDev driver become states in a ring of 64;
 * each state carries the filtered orientation. sceMotionGetState first lets
 * its own view of the ring catch up, then reads and filters new samples,
 * then reports the newest state. One recursive lightweight mutex guards it all.
 */

#include "driver_user.h"
#include "motion.h"

#define MOTION_LAST		(MOTION_ENTRIES - 1)
#define MOTION_DEG_PI		3.1415926535

int sceKernelCreateLwMutex(MotionLwMutex *work, const char *name, unsigned int attr, int count,
			   const void *option);
int sceKernelDeleteLwMutex(MotionLwMutex *work);
int sceKernelLockLwMutex(MotionLwMutex *work, int count, unsigned int *timeout);
int sceKernelUnlockLwMutex(MotionLwMutex *work, int count);

int sceMotionDevSamplingStart(void);
int sceMotionDevSamplingStop(void);
int sceMotionDevMagnSamplingStart(void);
int sceMotionDevMagnSamplingStop(void);
int sceMotionDevRead(MotionDevData *data, int count, int *info);
int sceMotionDevGetDeviceLocation(MotionDevDeviceLocation *location);

MotionGlobals g_motion;

static const SceFQuaternion s_motionIdentity = { 0.0f, 0.0f, 0.0f, 1.0f };

static __attribute__((noinline)) int MotionCreateLock(void)
{
	return sceKernelCreateLwMutex(&g_motion.mutex, "SceLibMotion", 2, 0, NULL);
}

static __attribute__((noinline)) int MotionDeleteLock(void)
{
	return sceKernelDeleteLwMutex(&g_motion.mutex);
}

__attribute__((noinline)) int MotionLock(void)
{
	return sceKernelLockLwMutex(&g_motion.mutex, 1, NULL);
}

__attribute__((noinline)) int MotionUnlock(void)
{
	return sceKernelUnlockLwMutex(&g_motion.mutex, 1);
}

static inline int ErrorOrZero(int ret)
{
	return ret < 0 ? ret : 0;
}

static __attribute__((noinline)) int MotionInit(void)
{
	MotionEntry *entry;
	int ret, i;

	g_motion.count = 0;
	g_motion.writeIndex = 0;
	ret = MotionCreateLock();
	if (ret < 0)
		return ret;
	g_motion.config.angleThreshold[0] = 20.0f;
	g_motion.config.angleThreshold[1] = 20.0f;
	g_motion.config.angleThreshold[2] = 20.0f;
	g_motion.config.reserve = 0;
	g_motion.config.magnetometer = 0;
	g_motion.config.tiltCorrection = 1;
	g_motion.config.deadband = 1;
	g_motion.config.gyroBiasCorrection = 1;
	ret = MotionReadCalibration(&g_motion.gyroCal, &g_motion.accCal, &g_motion.magnCal);
	if (ret < 0) {
		MotionDeleteLock();
		return ret;
	}
	for (i = 0; i < MOTION_ENTRIES; i++) {
		entry = &g_motion.entries[i];
		entry->acc = (SceFVector4){ 0.0f, 0.0f, 0.0f, 0.0f };
		entry->accNew = 0;
		entry->gyro = (SceFVector4){ 0.0f, 0.0f, 0.0f, 0.0f };
		entry->gyroStill[0] = 0;
		entry->gyroStill[1] = 0;
		entry->magn = (SceFVector4){ 0.0f, 0.0f, 0.0f, 0.0f };
		entry->gyroStill[2] = 0;
		entry->quat = s_motionIdentity;
		entry->gyroNew = 0;
		entry->magnNew = 0;
		entry->seq = 0;
		entry->timestamp = 0;
		entry->reserve1 = 0;
		entry->sample.magnValid = 0;
		entry->sample.reserve = 0;
		entry->sample.accValid = 0;
		entry->sample.magn = (MotionDVector3){ 0.0, 0.0, 0.0 };
		entry->sample.acc = (MotionDVector3){ 0.0, 0.0, 0.0 };
		entry->sample.gyro = (MotionDVector3){ 0.0, 0.0, 0.0 };
		entry->basicOrientation.x = 0.0f;
		entry->basicOrientation.y = 1.0f;
		entry->basicOrientation.z = 0.0f;
		entry->ned[0] = (SceFVector4){ 1.0f, 0.0f, 0.0f, 0.0f };
		entry->ned[1] = (SceFVector4){ 0.0f, 1.0f, 0.0f, 0.0f };
		entry->ned[2] = (SceFVector4){ 0.0f, 0.0f, 1.0f, 0.0f };
		entry->magnRaw.x = 0.0f;
		entry->magnRaw.y = 0.0f;
		entry->magnRaw.z = 0.0f;
		entry->dataInfo = 0;
	}
	g_motion.initialized = 1;
	return 0;
}

static __attribute__((noinline)) int MotionStart(void)
{
	return MotionInit() < 0 ? 2 : 0;
}

static __attribute__((noinline)) int MotionStop(void)
{
	g_motion.initialized = 0;
	MotionDeleteLock();
	return 0;
}

int module_start(SceSize args, void *argp)
{
	MotionStart();
	return 0;
}

int module_stop(SceSize args, void *argp)
{
	MotionStop();
	return 0;
}

/* --- configuration --- */

int sceMotionReset(void)
{
	int ret = MotionLock();

	if (ret < 0)
		return ret;
	g_motion.entries[g_motion.writeIndex].quat = s_motionIdentity;
	g_motion.entries[g_motion.prevIndex].quat = s_motionIdentity;
	return ErrorOrZero(MotionUnlock());
}

MOTION_VFP int sceMotionRotateYaw(float radians)
{
	if (!g_motion.initialized)
		return SCE_MOTION_ERROR_NON_INIT_ERR;
	return MotionRotateEntryYaw(&g_motion.entries[g_motion.writeIndex], radians);
}

SceBool sceMotionGetTiltCorrection(void)
{
	return g_motion.config.tiltCorrection;
}

int sceMotionSetTiltCorrection(SceBool setValue)
{
	int ret = MotionLock();

	if (ret < 0)
		return ret;
	g_motion.config.tiltCorrection = setValue;
	return ErrorOrZero(MotionUnlock());
}

SceBool sceMotionGetDeadband(void)
{
	return g_motion.config.deadband;
}

int sceMotionSetDeadband(SceBool setValue)
{
	int ret = MotionLock();

	if (ret < 0)
		return ret;
	g_motion.config.deadband = setValue;
	return ErrorOrZero(MotionUnlock());
}

int sceMotionSetGyroBiasCorrection(SceBool setValue)
{
	int ret = MotionLock();

	if (ret < 0)
		return ret;
	g_motion.config.gyroBiasCorrection = setValue;
	return ErrorOrZero(MotionUnlock());
}

SceBool sceMotionGetGyroBiasCorrection(void)
{
	return g_motion.config.gyroBiasCorrection;
}

MOTION_VFP int sceMotionSetAngleThreshold(float angle)
{
	int ret = MotionLock();

	if (ret < 0)
		return ret;
	if (!(angle > 0.0f) || !(angle <= 45.0f)) {
		ret = MotionUnlock();
		return ret < 0 ? ret : SCE_MOTION_ERROR_ANGLE_OUT_OF_RANGE;
	}
	g_motion.config.angleThreshold[0] = angle;
	g_motion.config.angleThreshold[1] = angle;
	g_motion.config.angleThreshold[2] = angle;
	return ErrorOrZero(MotionUnlock());
}

/* name unknown: all three thresholds, the first given from the horizontal */
int SceMotion_CA0A798B(const SceFVector3 *angles)
{
	int ret;

	if (angles == NULL)
		return SCE_MOTION_ERROR_NULL_PARAMETER;
	ret = MotionLock();
	if (ret < 0)
		return ret;
	g_motion.config.angleThreshold[1] = angles->y;
	g_motion.config.angleThreshold[2] = angles->z;
	g_motion.config.angleThreshold[0] = 90.0f - angles->x;
	return ErrorOrZero(MotionUnlock());
}

/* name unknown: reads the three thresholds back */
int SceMotion_F4E9D62E(SceFVector3 *angles)
{
	angles->y = g_motion.config.angleThreshold[1];
	angles->z = g_motion.config.angleThreshold[2];
	angles->x = 90.0f - g_motion.config.angleThreshold[0];
	return 0;
}

MOTION_VFP float sceMotionGetAngleThreshold(void)
{
	return g_motion.config.angleThreshold[2];
}

int sceMotionMagnetometerOff(void)
{
	sceMotionDevMagnSamplingStop();
	g_motion.config.magnetometer = 0;
	return 0;
}

int sceMotionMagnetometerOn(void)
{
	sceMotionDevMagnSamplingStart();
	g_motion.config.magnetometer = 1;
	return 0;
}

SceBool sceMotionGetMagnetometerState(void)
{
	return g_motion.config.magnetometer;
}

/* --- sampling: each client has its own flag, the device a reference count --- */

static inline __attribute__((always_inline)) int MotionStartSampling(int *active)
{
	int ret;

	if (!g_motion.initialized)
		return SCE_MOTION_ERROR_NON_INIT_ERR;
	if (*active)
		return SCE_MOTION_ERROR_ALREADY_SAMPLING;
	ret = MotionLock();
	if (ret < 0)
		return ret;
	if (g_motion.samplingRefs != 0) {
		g_motion.samplingRefs++;
		*active = 1;
		return ErrorOrZero(MotionUnlock());
	}
	if (sceMotionDevSamplingStart() == 0) {
		*active = 1;
		g_motion.samplingRefs++;
	}
	return MotionUnlock();
}

static inline __attribute__((always_inline)) int MotionStopSampling(int *active)
{
	int ret;

	if (!g_motion.initialized)
		return SCE_MOTION_ERROR_NON_INIT_ERR;
	if (!*active)
		return SCE_MOTION_ERROR_NOT_SAMPLING;
	ret = MotionLock();
	if (ret < 0)
		return ret;
	if (g_motion.samplingRefs > 1) {
		g_motion.samplingRefs--;
		*active = 0;
		return ErrorOrZero(MotionUnlock());
	}
	g_motion.samplingRefs = 0;
	if (sceMotionDevSamplingStop() == 0)
		*active = 0;
	return MotionUnlock();
}

int sceMotionStartSampling(void)
{
	return MotionStartSampling(&g_motion.sampling);
}

int sceMotionStopSampling(void)
{
	return MotionStopSampling(&g_motion.sampling);
}

int SceMotion_ACEFA121(void)
{
	return MotionStartSampling(&g_motion.samplingExtra);
}

int SceMotion_BC5C86C5(void)
{
	return MotionStopSampling(&g_motion.samplingExtra);
}

int SceMotion_0D9FA7EA(void)
{
	return MotionStartSampling(&g_motion.samplingShell);
}

int SceMotion_D88B731D(void)
{
	return MotionStopSampling(&g_motion.samplingShell);
}

int sceMotionGetDeviceLocation(SceMotionDeviceLocation *location)
{
	MotionDevDeviceLocation device;
	int ret;

	if (location == NULL)
		return SCE_MOTION_ERROR_NULL_PARAMETER;
	ret = sceMotionDevGetDeviceLocation(&device);
	if (ret < 0)
		return ret;
	location->accelerometer = device.accelerometer;
	location->gyro = device.gyro;
	return 0;
}

/* name unknown: the gyro zero offsets, three times */
int SceMotion_02AC0F4B(SceFVector3 *a, SceFVector3 *b, SceFVector3 *c)
{
	SceFVector3 va, vb, vc;

	MotionSetVector3(&va, &vb, &vc, &g_motion.gyroCal.offset);
	*a = va;
	*b = vb;
	*c = vc;
	return 0;
}

int sceMotionGetBasicOrientation(SceFVector3 *basicOrientation)
{
	const MotionEntry *entry;
	int ret;

	if (!g_motion.initialized)
		return SCE_MOTION_ERROR_NON_INIT_ERR;
	if (!g_motion.sampling)
		return SCE_MOTION_ERROR_NOT_SAMPLING;
	if (basicOrientation == NULL)
		return SCE_MOTION_ERROR_NULL_PARAMETER;
	ret = MotionLock();
	if (ret < 0)
		return ret;
	entry = &g_motion.entries[g_motion.prevIndex];
	basicOrientation->x = entry->basicOrientation.x;
	basicOrientation->y = entry->basicOrientation.y;
	basicOrientation->z = entry->basicOrientation.z;
	return ErrorOrZero(MotionUnlock());
}

/* --- state ring --- */

static inline SceUInt32 PrevIndex(SceUInt32 index)
{
	return index != 0 ? index - 1 : MOTION_LAST;
}

/* filters the states this reader has not seen; ahead of both other readers only */
static inline __attribute__((always_inline)) void MotionCatchUp(SceUInt32 *position,
								const SceUInt32 *readerA,
								const SceUInt32 *readerB)
{
	MotionEntry *entries = g_motion.entries;
	SceUInt32 seen = entries[*readerA].seq;
	SceUInt32 index = *position, prev;

	while (index != g_motion.readIndex) {
		index++;
		*position = index;
		if (index > MOTION_LAST) {
			index = 0;
			prev = MOTION_LAST;
			*position = 0;
		} else {
			prev = PrevIndex(index);
		}
		if (entries[index].seq <= seen || entries[index].seq <= entries[*readerB].seq)
			continue;
		MotionUpdateEntry(prev, index);
		seen = entries[*readerA].seq;
		index = *position;
	}
	if (index > MOTION_LAST) {
		*position = 0;
		index = 0;
		prev = MOTION_LAST;
	} else {
		prev = PrevIndex(index);
	}
	if (entries[index].seq > seen && entries[index].seq > entries[*readerB].seq)
		MotionUpdateEntry(prev, index);
}

/* a new state that repeats the previous one, the gyro at rest */
static inline __attribute__((always_inline)) void MotionRepeatEntry(MotionEntry *entry,
								    const MotionEntry *prev)
{
	entry->reserve1 = 0;
	entry->quat = prev->quat;
	entry->acc = prev->acc;
	entry->gyro = (SceFVector4){ 0.0f, 0.0f, 0.0f, 0.0f };
	entry->magn = prev->magn;
	entry->ned[0] = prev->ned[0];
	entry->ned[1] = prev->ned[1];
	entry->ned[2] = prev->ned[2];
	entry->basicOrientation = prev->basicOrientation;
}

static inline void SetXYZ(SceFVector4 *vector, float x, float y, float z)
{
	vector->x = x;
	vector->y = y;
	vector->z = z;
}

/* one driver sample into a new state */
static inline __attribute__((always_inline)) void MotionIngestSample(const MotionDevData *data,
								     const MotionDevSample *sample,
								     int filter, SceUInt32 *position)
{
	MotionEntry *entries = g_motion.entries;
	MotionEntry *entry, *prevEntry;
	SceUInt32 index, prev, count, last, now, delta, step;
	float interval;
	SceFVector4 offset, scale;

	index = g_motion.writeIndex + 1;
	if (index > MOTION_LAST)
		index = 0;
	g_motion.writeIndex = index;
	prev = index - 1;
	count = g_motion.count;
	if (count <= MOTION_LAST - 1) {
		count++;
		g_motion.count = count;
	}
	if (index == 0)
		prev = MOTION_LAST;
	g_motion.readIndex = index;
	last = g_motion.lastTimestamp;
	now = sample->timestamp;
	g_motion.prevTimestamp = last;
	entry = &entries[index];
	prevEntry = &entries[prev];
	entry->acc = prevEntry->acc;
	g_motion.prevIndex = prev;
	g_motion.lastTimestamp = now;
	entry->magn = prevEntry->magn;
	entry->gyro = prevEntry->gyro;
	entry->ned[0] = prevEntry->ned[0];
	entry->ned[1] = prevEntry->ned[1];
	entry->ned[2] = prevEntry->ned[2];
	entry->basicOrientation = prevEntry->basicOrientation;
	entry->magnRaw = prevEntry->magnRaw;
	entry->magnOffset = prevEntry->magnOffset;
	entry->quat = prevEntry->quat;
	if (count == 1) {
		step = 0;
		interval = 0.0f;
	} else {
		if (last > now)
			delta = (1 - last) + (data->timestampWrap & 0xf) * 0xffff + now;
		else
			delta = now - last;
		step = delta * 1000;
		interval = (float)delta;
	}
	entry->timestamp = prevEntry->timestamp + step;
	g_motion.magnStability = data->magnStability;
	entry->interval = interval;
	entry->hostTimestamp = data->hostTimestamp;
	if (sample->flags & MOTION_SAMPLE_GYRO) {
		entry->gyroNew = 1;
		MotionConvertGyro(&entry->sample, &g_motion.gyroCal, (double)sample->gyro[0],
				  (double)sample->gyro[1], (double)sample->gyro[2]);
		entry = &entries[g_motion.writeIndex];
		SetXYZ(&entry->gyro, (float)(entry->sample.gyro.x * MOTION_DEG_PI / 180.0),
		       (float)(entry->sample.gyro.y * MOTION_DEG_PI / 180.0),
		       (float)(entry->sample.gyro.z * MOTION_DEG_PI / 180.0));
	}
	if (sample->flags & MOTION_SAMPLE_ACC) {
		entry = &entries[g_motion.writeIndex];
		entry->accNew = 1;
		MotionConvertAcc(&entry->sample, &g_motion.accCal, (double)sample->acc[0],
				 (double)sample->acc[1], (double)sample->acc[2]);
		entry = &entries[g_motion.writeIndex];
		SetXYZ(&entry->acc, (float)entry->sample.acc.x, (float)entry->sample.acc.y,
		       (float)entry->sample.acc.z);
	}
	if (sample->flags & MOTION_SAMPLE_MAGN) {
		entry = &entries[g_motion.writeIndex];
		entry->magnNew = 1;
		SetXYZ(&entry->magnRaw, (float)sample->magn[0], (float)sample->magn[1],
		       (float)sample->magn[2]);
		MotionMagnOffset(&offset, &g_motion.magnCal);
		entry = &entries[g_motion.writeIndex];
		SetXYZ(&entry->magnOffset, offset.x, offset.y, offset.z);
		MotionMagnScale(&scale, &g_motion.magnCal);
		entry = &entries[g_motion.writeIndex];
		SetXYZ(&entry->magnScale, scale.x, scale.y, scale.z);
		MotionConvertMagn(data->magnSet & 1, &entry->sample, &g_motion.magnCal,
				  (double)sample->magn[0], (double)sample->magn[1],
				  (double)sample->magn[2]);
		entry = &entries[g_motion.writeIndex];
		SetXYZ(&entry->magn, (float)entry->sample.magn.x, (float)entry->sample.magn.y,
		       (float)entry->sample.magn.z);
		if (g_motion.config.magnetometer == 0)
			SetXYZ(&entry->magn, 0.0f, 0.0f, 0.0f);
	}
	if (filter) {
		int ret = MotionUpdateEntry(g_motion.prevIndex, g_motion.writeIndex);

		*position = g_motion.readIndex;
		if (ret < 0)
			MotionRepeatEntry(&entries[g_motion.writeIndex], &entries[g_motion.prevIndex]);
	} else {
		*position = g_motion.readIndex;
	}
	entries[g_motion.writeIndex].seq = entries[g_motion.prevIndex].seq + 1;
}

/* reads what the driver has; returns how many records it gave */
static inline __attribute__((always_inline)) int MotionReadSamples(int filter,
								   SceUInt32 *position)
{
	const MotionDevData *data;
	SceUInt32 index, prev, count;
	int info = 0, records, r, k;

	records = sceMotionDevRead(g_motion.records, 64, &info);
	if (info == 1)
		g_motion.dataInfo = 1;
	else if (info == 0)
		g_motion.dataInfo = 0;
	if (records > 0) {
		g_motion.writeIndex = g_motion.readIndex;
		for (r = 0; r < records; r++) {
			data = &g_motion.records[r];
			if (data->magnCalibTag != g_motion.magnCalibTag && data->magnCalibTag != 0) {
				MotionReadMagnCalibration(&g_motion.magnCal);
				g_motion.magnCalibTag = data->magnCalibTag;
			}
			if (g_motion.config.gyroBiasCorrection == 1 &&
			    data->gyroBiasTag != g_motion.gyroBiasTag && data->gyroBiasTag != 0) {
				MotionReadGyroBias(&g_motion.gyroCal.offset);
				g_motion.gyroBiasTag = data->gyroBiasTag;
			}
			for (k = 0; k < data->count; k++)
				MotionIngestSample(data, &data->sample[k], filter, position);
		}
		*position = g_motion.readIndex;
	} else if (records < 0) {
		/* nothing new: the state repeats */
		index = g_motion.writeIndex + 1;
		if (index > MOTION_LAST)
			index = 0;
		count = g_motion.count;
		g_motion.writeIndex = index;
		if (count <= MOTION_LAST - 1)
			g_motion.count = count + 1;
		prev = PrevIndex(index);
		g_motion.prevIndex = prev;
		MotionRepeatEntry(&g_motion.entries[index], &g_motion.entries[prev]);
	}
	return records;
}

/* rotation matrix of a unit quaternion, rows as the library stores them */
static inline void QuatToMatrix(const SceFQuaternion *q, SceFMatrix4 *m)
{
	float x2 = q->x + q->x, y2 = q->y + q->y, z2 = q->z + q->z;

	m->x.x = (1.0f - q->y * y2) - q->z * z2;
	m->x.y = z2 * q->w + q->x * y2;
	m->x.z = q->x * z2 - y2 * q->w;
	m->x.w = 0.0f;
	m->y.x = q->y * x2 - z2 * q->w;
	m->y.y = (1.0f - q->z * z2) - q->x * x2;
	m->y.z = x2 * q->w + q->y * z2;
	m->y.w = 0.0f;
	m->z.x = y2 * q->w + q->z * x2;
	m->z.y = q->z * y2 - x2 * q->w;
	m->z.z = (1.0f - q->x * x2) - q->y * y2;
	m->z.w = 0.0f;
	m->w = (SceFVector4){ 0.0f, 0.0f, 0.0f, 1.0f };
}

static inline __attribute__((always_inline)) void MotionFillState(SceMotionState *state, int magn)
{
	MotionEntry *entry = &g_motion.entries[g_motion.writeIndex];
	SceFMatrix4 matrix;

	if (g_motion.dataInfo == 1)
		entry->dataInfo |= 1;
	else if (g_motion.dataInfo == 0)
		entry->dataInfo &= ~1;
	state->magnFieldStability = g_motion.magnStability;
	state->acceleration.x = entry->acc.x;
	state->acceleration.y = entry->acc.y;
	state->acceleration.z = entry->acc.z;
	state->angularVelocity.x = entry->gyro.x;
	state->angularVelocity.y = entry->gyro.y;
	state->angularVelocity.z = entry->gyro.z;
	if (magn) {
		/* the extended reader gets the magnetic field in reserve1 */
		SceFVector3 *field = (SceFVector3 *)state->reserve1;

		field->x = entry->magn.x;
		field->y = entry->magn.y;
		field->z = entry->magn.z;
		/* and its strength in reserve2 */
		*(float *)state->reserve2 = SafeSqrt(Dot3(entry->magn.x, entry->magn.y, entry->magn.z));
	}
	state->deviceQuat = entry->quat;
	state->basicOrientation.x = entry->basicOrientation.x;
	state->basicOrientation.y = entry->basicOrientation.y;
	state->basicOrientation.z = entry->basicOrientation.z;
	QuatToMatrix(&entry->quat, &matrix);
	state->rotationMatrix = matrix;
	matrix.x = entry->ned[0];
	matrix.x.w = 0.0f;
	matrix.y = entry->ned[1];
	matrix.y.w = 0.0f;
	matrix.z = entry->ned[2];
	matrix.z.w = 0.0f;
	matrix.w = (SceFVector4){ 0.0f, 0.0f, 0.0f, 1.0f };
	state->nedMatrix = matrix;
	state->timestamp = entry->timestamp;
	state->hostTimestamp = entry->hostTimestamp;
	state->dataInfo = entry->dataInfo;
}

static inline __attribute__((always_inline)) int MotionGetState(SceMotionState *state,
								int *sampling,
								SceUInt32 *position,
								const SceUInt32 *readerA,
								const SceUInt32 *readerB, int magn)
{
	int ret = MotionLock();

	if (ret < 0)
		return ret;
	if (*sampling && MotionLock() >= 0) {
		MotionCatchUp(position, readerA, readerB);
		MotionReadSamples(1, position);
		MotionUnlock();
	}
	MotionFillState(state, magn);
	return ErrorOrZero(MotionUnlock());
}

int sceMotionGetState(SceMotionState *state)
{
	if (!g_motion.initialized)
		return SCE_MOTION_ERROR_NON_INIT_ERR;
	if (!g_motion.sampling)
		return SCE_MOTION_ERROR_NOT_SAMPLING;
	if (state == NULL)
		return SCE_MOTION_ERROR_NULL_PARAMETER;
	return MotionGetState(state, &g_motion.sampling, &g_motion.stateIndex, &g_motion.extraIndex,
			      &g_motion.shellIndex, 0);
}

/* name unknown: sceMotionGetState for the second client */
int SceMotion_D23686B0(SceMotionState *state)
{
	if (!g_motion.initialized)
		return SCE_MOTION_ERROR_NON_INIT_ERR;
	if (state == NULL)
		return SCE_MOTION_ERROR_NULL_PARAMETER;
	if (!g_motion.samplingShell)
		return SCE_MOTION_ERROR_NOT_SAMPLING;
	return MotionGetState(state, &g_motion.samplingShell, &g_motion.shellIndex,
			      &g_motion.stateIndex, &g_motion.extraIndex, 0);
}

/* name unknown: sceMotionGetState for the extended client, magnetic field included */
int SceMotion_AD0B62B7(SceMotionState *state)
{
	if (!g_motion.initialized)
		return SCE_MOTION_ERROR_NON_INIT_ERR;
	if (!g_motion.samplingExtra)
		return SCE_MOTION_ERROR_NOT_SAMPLING;
	if (state == NULL)
		return SCE_MOTION_ERROR_NULL_PARAMETER;
	return MotionGetState(state, &g_motion.samplingExtra, &g_motion.extraIndex,
			      &g_motion.stateIndex, &g_motion.shellIndex, 1);
}

static inline void MotionFillSensorState(SceMotionSensorState *state, const MotionEntry *entry,
					 int host)
{
	state->accelerometer.x = entry->acc.x;
	state->accelerometer.y = entry->acc.y;
	state->accelerometer.z = entry->acc.z;
	state->gyro.x = entry->gyro.x;
	state->gyro.y = entry->gyro.y;
	state->gyro.z = entry->gyro.z;
	state->timestamp = entry->timestamp;
	state->counter = entry->seq;
	if (host)
		state->hostTimestamp = entry->hostTimestamp;
	state->dataInfo = entry->dataInfo;
}

/* the newest states, newest first, without running the filter */
static inline __attribute__((always_inline)) int MotionGetSensorState(SceMotionSensorState *states,
								      int count, int extra)
{
	MotionEntry *entry;
	int ret = MotionLock();
	int index, last, k;

	if (ret < 0)
		return ret;
	if (MotionLock() >= 0) {
		g_motion.sensorIndex = g_motion.readIndex;
		MotionReadSamples(0, &g_motion.sensorIndex);
		MotionUnlock();
	}
	index = g_motion.writeIndex;
	entry = &g_motion.entries[index];
	if (g_motion.dataInfo == 1)
		entry->dataInfo |= 1;
	else if (g_motion.dataInfo == 0)
		entry->dataInfo &= ~1;
	MotionFillSensorState(&states[0], entry, 1);
	/*
	 * Older states, stepping back past the oldest to the count of states as
	 * Sony's does. The extended reader leaves out their host timestamps and
	 * fills one state more than asked for (unless asked for one).
	 */
	last = extra && count != 1 ? count : count - 1;
	for (k = 1; k <= last; k++) {
		index--;
		if (index < 0)
			index = g_motion.count;
		MotionFillSensorState(&states[k], &g_motion.entries[index], !extra);
	}
	return ErrorOrZero(MotionUnlock());
}

int sceMotionGetSensorState(SceMotionSensorState *sensorState, int numRecords)
{
	if (!g_motion.initialized)
		return SCE_MOTION_ERROR_NON_INIT_ERR;
	if (!g_motion.sampling)
		return SCE_MOTION_ERROR_NOT_SAMPLING;
	if ((unsigned int)(numRecords - 1) > MOTION_LAST)
		return SCE_MOTION_ERROR_OUT_OF_BOUNDS;
	if (sensorState == NULL)
		return SCE_MOTION_ERROR_NULL_PARAMETER;
	return MotionGetSensorState(sensorState, numRecords, 0);
}

/* name unknown: sceMotionGetSensorState for the extended client */
int SceMotion_D66D5192(SceMotionSensorState *sensorState, int numRecords)
{
	if (!g_motion.initialized)
		return SCE_MOTION_ERROR_NON_INIT_ERR;
	if (!g_motion.samplingExtra)
		return SCE_MOTION_ERROR_NOT_SAMPLING;
	if (sensorState == NULL)
		return SCE_MOTION_ERROR_NULL_PARAMETER;
	if ((unsigned int)(numRecords - 1) > MOTION_LAST)
		return SCE_MOTION_ERROR_OUT_OF_BOUNDS;
	return MotionGetSensorState(sensorState, numRecords, 1);
}

/* --- extra controllers: ports 1-4, port 0 being the device itself --- */

int sceKernelAllocMemBlock(const char *name, unsigned int type, SceSize size, const void *opt);
int sceKernelGetMemBlockBase(SceUID uid, void **base);
int sceKernelFreeMemBlock(SceUID uid);
void *sceClibMemset(void *dst, int value, SceSize size);
int sceMotionDevRead2(int port, MotionDevData *data, int count, int *info);
int sceMotionDevRead3(int port, MotionDs3Raw *data, int count, int *info);
int sceMotionDevSamplingStart2(int port);
int sceMotionDevSamplingStop2(int port);
int sceMotionDevGetGyroBias2(int port, MotionDevGyroBiasData *bias);
int sceMotionDevGetControllerType(int port, SceUInt8 *type);
int sceMotionDevGetDs3CalibData(int port, SceUInt16 *calibration);
int sceMotionDevSetGyroFeedBack(int port, SceUInt8 offset);

#define MOTION_PORTS		4
#define MOTION_CONTROLLER_DS3	4
#define MOTION_CONTROLLER_EXT	8

static inline MotionPort *MotionGetPort(int port)
{
	if ((unsigned int)(port - 1) > MOTION_PORTS - 1)
		return NULL;
	return g_motion.ds3Port[port - 1];
}

int SceMotion_3C970E36(void)
{
	static const SceUInt32 offset[MOTION_PORTS] = {
		0, sizeof(MotionPort), 2 * sizeof(MotionPort), 3 * sizeof(MotionPort)
	};
	MotionPort *port;
	void *base;
	int ret, i;

	g_motion.ds3Block = sceKernelAllocMemBlock("SceMotionExtResources", 0x0c20d060, 0x10000, NULL);
	if (g_motion.ds3Block < 0)
		return g_motion.ds3Block;
	ret = sceKernelGetMemBlockBase(g_motion.ds3Block, &base);
	if (ret < 0) {
		sceKernelFreeMemBlock(g_motion.ds3Block);
		return ret;
	}
	for (i = 0; i < MOTION_PORTS; i++)
		g_motion.ds3Port[i] = (MotionPort *)((SceUInt8 *)base + offset[i]);
	for (i = 0; i < MOTION_PORTS; i++) {
		port = g_motion.ds3Port[i];
		sceClibMemset(port, 0, sizeof(MotionPort));
		port->tiltCorrection = 1;
		port->deadband = 1;
		port->quat = s_motionIdentity;
		port->gyro = (SceFVector4){ 0.0f, 0.0f, 0.0f, 0.0f };
		port->acc = (SceFVector4){ 0.0f, -1.0f, 0.0f, 0.0f };
	}
	return 0;
}

int SceMotion_452F92F7(void)
{
	if (g_motion.ds3Block > 0) {
		sceKernelFreeMemBlock(g_motion.ds3Block);
		g_motion.ds3Block = 0;
	}
	g_motion.ds3Port[0] = NULL;
	g_motion.ds3Port[1] = NULL;
	g_motion.ds3Port[2] = NULL;
	g_motion.ds3Port[3] = NULL;
	return 0;
}

int SceMotion_833FD8DC(int port)
{
	MotionPort *context;
	int ret;

	if (port == 0)
		return sceMotionStartSampling();
	if (port > MOTION_PORTS)
		return SCE_MOTION_ERROR_INVALID_ARG;
	ret = MotionLock();
	if (ret < 0)
		return ret;
	context = MotionGetPort(port);
	if (context == NULL) {
		MotionUnlock();
		return SCE_MOTION_ERROR_NON_INIT_ERR;
	}
	if (context->sampling) {
		MotionUnlock();
		return SCE_MOTION_ERROR_ALREADY_SAMPLING;
	}
	ret = sceMotionDevSamplingStart2(port);
	if (ret < 0) {
		MotionUnlock();
		return ret;
	}
	context->sampling = 1;
	return ErrorOrZero(MotionUnlock());
}

int SceMotion_09338B80(int port)
{
	MotionPort *context;
	int ret;

	if (port == 0)
		return sceMotionStopSampling();
	if (port > MOTION_PORTS)
		return SCE_MOTION_ERROR_INVALID_ARG;
	ret = MotionLock();
	if (ret < 0)
		return ret;
	context = MotionGetPort(port);
	if (context == NULL) {
		MotionUnlock();
		return SCE_MOTION_ERROR_NON_INIT_ERR;
	}
	if (!context->sampling) {
		MotionUnlock();
		return 0;
	}
	ret = sceMotionDevSamplingStop2(port);
	if (ret < 0) {
		MotionUnlock();
		return ret;
	}
	context->sampling = 0;
	return ErrorOrZero(MotionUnlock());
}

static inline void Calibrate(const MotionAxisCalibration *calibration, float x, float y, float z,
			     float *ox, float *oy, float *oz)
{
	x -= calibration->offset.x;
	y -= calibration->offset.y;
	z -= calibration->offset.z;
	*ox = x * (x > 0.0f ? calibration->positive.x : calibration->negative.x);
	*oy = y * (y > 0.0f ? calibration->positive.y : calibration->negative.y);
	*oz = z * (z > 0.0f ? calibration->positive.z : calibration->negative.z);
}

/* below about one degree per second an axis reads as still */
static inline float Deadband(float rate)
{
	float magnitude;

	if (rate == 0.0f)
		return 0.0f;
	magnitude = rate;
	if (magnitude <= 0.0f)
		magnitude = -magnitude;
	return magnitude - 0.0175f < 0.0f ? 0.0f : rate;
}

/* the extra controller's own filter: gyro integration and tilt correction only */
static int MotionPortGetState(int port, SceMotionState *state)
{
	MotionDevGyroBiasData bias;
	const MotionDevData *data;
	const MotionDevSample *sample;
	MotionPort *context;
	SceUInt32 now, last, delta;
	SceFMatrix4 matrix;
	float x, y, z;
	int info, records, ret, r, k;

	if (port > MOTION_PORTS)
		return SCE_MOTION_ERROR_INVALID_ARG;
	if (state == NULL)
		return SCE_MOTION_ERROR_NULL_PARAMETER;
	ret = MotionLock();
	if (ret < 0)
		return ret;
	context = MotionGetPort(port);
	if (context == NULL) {
		MotionUnlock();
		return SCE_MOTION_ERROR_NON_INIT_ERR;
	}
	if (!context->sampling) {
		MotionUnlock();
		return SCE_MOTION_ERROR_NOT_SAMPLING;
	}
	records = sceMotionDevRead2(port, context->records, 64, &info);
	if (records < 0) {
		MotionUnlock();
		return records;
	}
	for (r = 0; r < records; r++) {
		data = &context->records[r];
		for (k = 0; k < data->count; k++) {
			sample = &data->sample[k];
			if (context->calibTag != data->calibTag) {
				/*
				 * The lock stays held on failure, and the accelerometer
				 * result replaces the record count (ending the read after
				 * this record), both as in Sony's.
				 */
				if (MotionReadGyroCalibration(port, &context->gyroCal) < 0)
					return 0;
				records = MotionReadAccCalibration(port, &context->accCal);
				if (records < 0)
					return 0;
				context->lastTimestamp = sample->timestamp;
				context->calibTag = data->calibTag;
			}
			if (context->gyroBiasTag != data->gyroBiasTag) {
				sceMotionDevGetGyroBias2(port, &bias);
				context->gyroCal.offset = bias.bias;
				context->gyroBiasTag = data->gyroBiasTag;
			}
			if (sample->flags & MOTION_SAMPLE_GYRO) {
				Calibrate(&context->gyroCal, (float)sample->gyro[0],
					  (float)sample->gyro[1], (float)sample->gyro[2], &x, &y, &z);
				context->gyro.x = x * 3.14159274f / 180.0f;
				context->gyro.y = y * 3.14159274f / 180.0f;
				context->gyro.z = z * 3.14159274f / 180.0f;
				if (context->deadband) {
					context->gyro.x = Deadband(context->gyro.x);
					context->gyro.y = Deadband(context->gyro.y);
					context->gyro.z = Deadband(context->gyro.z);
				}
			}
			if (sample->flags & MOTION_SAMPLE_ACC) {
				Calibrate(&context->accCal, (float)sample->acc[0], (float)sample->acc[1],
					  (float)sample->acc[2], &x, &y, &z);
				context->acc.x = -x;
				context->acc.y = -y;
				context->acc.z = -z;
			}
			last = context->lastTimestamp;
			now = sample->timestamp;
			context->lastTimestamp = now;
			delta = last > now ? 0x10000 - last + now : now - last;
			MotionIntegrate(&context->quat, &context->gyro, &context->quat,
					(float)delta * 5.33f * 1e-6f);
			if (context->tiltCorrection)
				MotionTiltCorrect(&context->quat, &context->acc);
		}
		context->hostTimestamp = data->hostTimestamp;
	}
	state->timestamp = 0;
	state->acceleration.x = context->acc.x;
	state->acceleration.y = context->acc.y;
	state->acceleration.z = context->acc.z;
	state->angularVelocity.x = context->gyro.x;
	state->angularVelocity.y = context->gyro.y;
	state->angularVelocity.z = context->gyro.z;
	state->deviceQuat = context->quat;
	QuatToMatrix(&context->quat, &matrix);
	state->rotationMatrix = matrix;
	matrix.x = (SceFVector4){ 1.0f, 0.0f, 0.0f, 0.0f };
	matrix.y = (SceFVector4){ 0.0f, 1.0f, 0.0f, 0.0f };
	matrix.z = (SceFVector4){ 0.0f, 0.0f, 1.0f, 0.0f };
	matrix.w = (SceFVector4){ 0.0f, 0.0f, 0.0f, 1.0f };
	state->nedMatrix = matrix;
	state->magnFieldStability = 0;
	state->dataInfo = 0;
	state->hostTimestamp = context->hostTimestamp;
	return ErrorOrZero(MotionUnlock());
}

int SceMotion_D87E2B64(int port, SceMotionState *state)
{
	if (port == 0)
		return sceMotionGetState(state);
	return MotionPortGetState(port, state);
}

int SceMotion_58C0187B(int port, SceBool setValue)
{
	MotionPort *context;
	int ret;

	if (port == 0)
		return sceMotionSetTiltCorrection(setValue);
	if (port > MOTION_PORTS)
		return SCE_MOTION_ERROR_INVALID_ARG;
	ret = MotionLock();
	if (ret < 0)
		return ret;
	context = MotionGetPort(port);
	if (context == NULL) {
		MotionUnlock();
		return SCE_MOTION_ERROR_NON_INIT_ERR;
	}
	context->tiltCorrection = setValue;
	return ErrorOrZero(MotionUnlock());
}

int SceMotion_CBD9CA97(int port, SceBool *value)
{
	MotionPort *context;
	int ret;

	if (port == 0) {
		if (value == NULL)
			return SCE_MOTION_ERROR_NULL_PARAMETER;
		*value = sceMotionGetTiltCorrection();
		return 0;
	}
	if (port > MOTION_PORTS)
		return SCE_MOTION_ERROR_INVALID_ARG;
	if (value == NULL)
		return SCE_MOTION_ERROR_NULL_PARAMETER;
	ret = MotionLock();
	if (ret < 0)
		return ret;
	context = MotionGetPort(port);
	if (context == NULL) {
		MotionUnlock();
		return SCE_MOTION_ERROR_NON_INIT_ERR;
	}
	*value = context->tiltCorrection;
	return ErrorOrZero(MotionUnlock());
}

int SceMotion_EB345158(int port, SceBool setValue)
{
	MotionPort *context;
	int ret;

	if (port == 0)
		return sceMotionSetDeadband(setValue);
	if (port > MOTION_PORTS)
		return SCE_MOTION_ERROR_INVALID_ARG;
	ret = MotionLock();
	if (ret < 0)
		return ret;
	context = MotionGetPort(port);
	if (context == NULL) {
		MotionUnlock();
		return SCE_MOTION_ERROR_NON_INIT_ERR;
	}
	context->deadband = setValue;
	return ErrorOrZero(MotionUnlock());
}

int SceMotion_C0586367(int port, SceBool *value)
{
	MotionPort *context;
	int ret;

	if (port == 0) {
		if (value == NULL)
			return SCE_MOTION_ERROR_NULL_PARAMETER;
		*value = sceMotionGetDeadband();
		return 0;
	}
	if (port > MOTION_PORTS)
		return SCE_MOTION_ERROR_INVALID_ARG;
	if (value == NULL)
		return SCE_MOTION_ERROR_NULL_PARAMETER;
	ret = MotionLock();
	if (ret < 0)
		return ret;
	context = MotionGetPort(port);
	if (context == NULL) {
		MotionUnlock();
		return SCE_MOTION_ERROR_NON_INIT_ERR;
	}
	*value = context->deadband;
	return ErrorOrZero(MotionUnlock());
}

int SceMotion_B7F7535F(int port)
{
	MotionPort *context;
	int ret;

	if (port == 0)
		return sceMotionReset();
	if (port > MOTION_PORTS)
		return SCE_MOTION_ERROR_INVALID_ARG;
	ret = MotionLock();
	if (ret < 0)
		return ret;
	context = MotionGetPort(port);
	if (context == NULL) {
		MotionUnlock();
		return SCE_MOTION_ERROR_NON_INIT_ERR;
	}
	context->quat = s_motionIdentity;
	return ErrorOrZero(MotionUnlock());
}

static inline SceUInt16 Clamp10(int value)
{
	if (value < 0)
		return 0;
	if (value >= 0x3ff)
		return 0x3ff;
	return value;
}

/* a state as DualShock 3 style 10-bit readings */
static inline void MotionStateToRaw(const SceMotionState *state, SceUInt16 *raw)
{
	raw[0] = Clamp10((int)(512.0f + state->acceleration.x * 113.0f));
	raw[1] = Clamp10((int)(512.0f + state->acceleration.y * 113.0f));
	raw[2] = Clamp10((int)(512.0f + state->acceleration.z * 113.0f));
	raw[3] = Clamp10((int)(512.0 - (double)state->angularVelocity.y * 246.0 / 3.1415926535));
}

/* name unknown: raw 10-bit accelerometer and gyro of a port, DualShock 3 style */
int SceMotion_340BFBE6(int port, SceUInt16 *raw)
{
	static const SceUInt16 s_rest[4] = { 512, 399, 512, 512 };
	SceMotionState state;
	MotionPort *context;
	SceUInt8 type = 0;
	int info = 0, ret;

	if (port == 0) {
		ret = sceMotionGetState(&state);
		if (ret != 0)
			return ret;
		MotionStateToRaw(&state, raw);
		return 0;
	}
	if (port > MOTION_PORTS)
		return SCE_MOTION_ERROR_INVALID_ARG;
	if (raw == NULL)
		return SCE_MOTION_ERROR_NULL_PARAMETER;
	ret = MotionLock();
	if (ret < 0)
		return ret;
	context = MotionGetPort(port);
	if (context == NULL) {
		MotionUnlock();
		return SCE_MOTION_ERROR_NON_INIT_ERR;
	}
	if (!context->sampling) {
		MotionUnlock();
		return SCE_MOTION_ERROR_NOT_SAMPLING;
	}
	raw[0] = s_rest[0];
	raw[1] = s_rest[1];
	raw[2] = s_rest[2];
	raw[3] = s_rest[3];
	ret = sceMotionDevGetControllerType(port, &type);
	if (ret < 0)
		return ret;	/* the lock stays held, as in Sony's */
	if (type == MOTION_CONTROLLER_DS3) {
		ret = sceMotionDevRead3(port, &context->ds3Raw, 64, &info);
		if (ret < 0) {
			MotionUnlock();
			return ret;
		}
		if (context->calibTag != context->ds3Raw.calibTag) {
			sceMotionDevGetDs3CalibData(port, context->ds3Raw.calibration);
			Ds3SensorInit(&context->ds3, context->ds3Raw.calibration, &context->ds3Offset);
			context->calibTag = context->ds3Raw.calibTag;
		}
		if (Ds3SensorProcess(&context->ds3, context->ds3Raw.sample, &context->ds3Offset) > 0)
			sceMotionDevSetGyroFeedBack(port, (SceUInt8)context->ds3Offset);
		raw[0] = context->ds3Raw.sample[0];
		raw[1] = context->ds3Raw.sample[1];
		raw[2] = context->ds3Raw.sample[2];
		raw[3] = context->ds3Raw.sample[3];
	} else if (type == MOTION_CONTROLLER_EXT) {
		MotionUnlock();
		ret = MotionPortGetState(port, &state);
		if (ret != 0)
			return ret;
		MotionStateToRaw(&state, raw);
		return 0;
	}
	return ErrorOrZero(MotionUnlock());
}
