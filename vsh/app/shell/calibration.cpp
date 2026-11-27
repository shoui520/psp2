/*
 * The motion sensors' calibration, updated by a thread of its own while the shell runs
 * (0x810E181C-0x810E1944): it starts the calibration library linked into the shell, reads its
 * state every 125 vertical blanks, every 2 while the sensors measure in its fast mode, and ends
 * it when the thread stops. The thread's name is Sony's ("SceCalibrationUpdate"); the file's, the
 * class's and the library functions' names are ours (PLACEHOLDER): no record names them.
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/math.h>
#include <std/string.h>
#include <kernel/threadmgr.h>
#include <kernel/processmgr.h>
#include <display.h>
#include "shell.h"

struct ShellCalibrationState {
	int count;
	int reserved;
	int version;
	int updated;
	SceUInt8 measMode;
};

/* Sensor calibration structures matching SceMotionDev user syscalls */
struct MotionCalibVec3 {
	float x;
	float y;
	float z;
};

struct MotionCalibQuat {
	float x;
	float y;
	float z;
	float w;
};

struct MotionDevMagnCalibRecord {
	MotionCalibVec3 offset[2];
	MotionCalibVec3 scale[2];
	SceUInt32 flags;
};

struct MotionDevGyroCalibPoint {
	float value;
	MotionCalibVec3 raw;
};

struct MotionDevGyroCalibRecord {
	MotionCalibVec3 zero;
	MotionDevGyroCalibPoint point[6];
	SceUInt32 flags;
};

struct MotionDevDeviceInfoRecord {
	SceUInt8 version;
	SceUInt8 revision;
	SceUInt8 subType;
	SceUInt8 deviceType;
	SceUInt8 extra[12];
};

struct MotionDevMeasModeRecord {
	SceUInt8 mode;
	SceUInt8 extra[7];
};

struct MotionDevMagnCalibSample {
	SceInt16 accX;
	SceInt16 accY;
	SceInt16 accZ;
	SceInt16 gyroX;
	SceInt16 gyroY;
	SceInt16 gyroZ;
	SceInt16 magnX;
	SceInt16 magnY;
	SceInt16 magnZ;
	SceUInt16 reservedFlags;
	SceUInt32 timestampUs;
};

extern "C" {
int sceMotionDevGetCurrentMagnCalibData(MotionDevMagnCalibRecord *data);
int sceMotionDevUpdateMagnCalibData(const MotionDevMagnCalibRecord *data, SceUInt32 *outStatus);
int sceMotionDevGetGyroCalibData(MotionDevGyroCalibRecord *data);
int sceMotionDevGetDeviceInfo(MotionDevDeviceInfoRecord *info);
int sceMotionDevGetMeasMode(MotionDevMeasModeRecord *info);
int sceMotionDevReadForMagnCalib(MotionDevMagnCalibSample *samples, int maxCount,
				 SceUInt32 *outFlags);
int sceMotionDevUpdateMagnStabilityLevel(SceUInt32 level);
}

/* Online 4-point sphere-fitting magnetometer calibration engine (0x8144AB70..0x8144CFD0) */
namespace {

static const double kPi = 3.1415926535;
static const int kStabilityWindowSize = 64;
static const int kSampleRingCapacity = 100;
static const int kMinAngularSpreadDeg = 60;
static const int kMaxAngularSpreadDeg = 80;
static const SceUInt64 kSphereFitTimeoutUs = 9000000ULL;

struct MagnCalibContext {
	int minSpreadDeg;
	int ringCapacity;
	int fitCount;
	int ringWritePos;
	int ringUsedCount;
	int totalSamples;
	SceUInt8 activeSetIndex;
	SceUInt8 deviceType;
	SceUInt8 hasFullRing;
	SceUInt8 fitValid;
	SceUInt8 calibUpdated;
	SceUInt8 stabilityLevel;
	SceUInt8 measMode;
	SceUInt8 stabilityBufferFull;
	int stabilityWritePos;
	int stabilityState;
	int stabilityLowCount;
	int stabilityHighCount;

	MotionCalibVec3 magnOffset;
	double magnScaleX;
	double magnScaleY;
	double magnScaleZ;
	double fittedCenterX;
	double fittedCenterY;
	double fittedCenterZ;
	double fittedRadius;
	double rmsResidualX;
	double rmsResidualY;
	double rmsResidualZ;

	MotionCalibVec3 gyroZero;
	MotionCalibVec3 gyroGain;

	MotionCalibQuat currentQuat;
	MotionCalibQuat lastSampleQuat;
	MotionCalibQuat axisRollQuat;
	MotionCalibQuat axisPitchQuat;
	MotionCalibQuat axisYawQuat;
	float maxRollAngleDeg;
	float maxPitchAngleDeg;
	float maxYawAngleDeg;

	MotionCalibVec3 fitMagn[4];
	MotionCalibQuat fitQuat[4];
	MotionCalibVec3 ringMagn[kSampleRingCapacity];
	MotionCalibVec3 lastScaledMagn;
	MotionCalibVec3 latestMagn;

	float stabilityMagnNorms[kStabilityWindowSize];
	double stabilityMean;
	double stabilityBaseline;
	double stabilityStdDev;

	SceUInt64 windowStartTimeUs;
	SceUInt64 lastUpdateTimeUs;
	SceUInt32 lastSampleTimestampUs;
};

static MagnCalibContext s_mc;

static MotionCalibQuat makeIdentityQuat(void)
{
	MotionCalibQuat q;
	q.x = 0.0f;
	q.y = 0.0f;
	q.z = 0.0f;
	q.w = 1.0f;
	return q;
}

static MotionCalibQuat makeInitialFitQuat(void)
{
	MotionCalibQuat q;
	q.x = 0.0f;
	q.y = 0.0f;
	q.z = 0.0f;
	q.w = 0.0078125f;
	return q;
}

static float quatAngleDeg(const MotionCalibQuat &lhs, const MotionCalibQuat &rhs)
{
	float dot = lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z + lhs.w * rhs.w;
	if (dot > 1.0f)
		dot = 1.0f;
	else if (dot < -1.0f)
		dot = -1.0f;
	return (float)(((double)(2.0f * sce_paf_acosf(dot)) * 180.0) / kPi);
}

/* (0x8144B020) integrate gyro angular rate into unit quaternion */
static void integrateQuat(const MotionCalibQuat &qIn, const MotionCalibVec3 &omega,
			  float dtSec, MotionCalibQuat *qOut)
{
	float halfX = 0.5f * qIn.x;
	float halfY = 0.5f * qIn.y;
	float halfZ = 0.5f * qIn.z;
	float halfW = 0.5f * qIn.w;

	float dqX = halfW * omega.x + halfY * omega.z - halfZ * omega.y;
	float dqY = halfW * omega.y + halfZ * omega.x - halfX * omega.z;
	float dqZ = halfW * omega.z + halfX * omega.y - halfY * omega.x;
	float dqW = -(halfX * omega.x + halfY * omega.y + halfZ * omega.z);

	float nx = qIn.x + dqX * dtSec;
	float ny = qIn.y + dqY * dtSec;
	float nz = qIn.z + dqZ * dtSec;
	float nw = qIn.w + dqW * dtSec;

	float norm = __builtin_sqrtf(nx * nx + ny * ny + nz * nz + nw * nw);
	if (norm > 0.0f) {
		float invNorm = 1.0f / norm;
		qOut->x = nx * invNorm;
		qOut->y = ny * invNorm;
		qOut->z = nz * invNorm;
		qOut->w = nw * invNorm;
	} else {
		*qOut = makeIdentityQuat();
	}
}

/* (0x8144B1F8) 4x4 matrix determinant */
static double det4x4(const double m[4][4])
{
	double minorAB = m[0][0] * m[1][1] - m[1][0] * m[0][1];
	double minorAC = m[0][0] * m[1][2] - m[1][0] * m[0][2];
	double minorAD = m[0][0] * m[1][3] - m[1][0] * m[0][3];
	double minorBC = m[0][1] * m[1][2] - m[1][1] * m[0][2];
	double minorBD = m[0][1] * m[1][3] - m[1][1] * m[0][3];
	double minorCD = m[0][2] * m[1][3] - m[1][2] * m[0][3];

	double cofCD = m[2][2] * m[3][3] - m[3][2] * m[2][3];
	double cofBD = m[2][1] * m[3][3] - m[3][1] * m[2][3];
	double cofBC = m[2][1] * m[3][2] - m[3][1] * m[2][2];
	double cofAD = m[2][0] * m[3][3] - m[3][0] * m[2][3];
	double cofAC = m[2][0] * m[3][2] - m[3][0] * m[2][2];
	double cofAB = m[2][0] * m[3][1] - m[3][0] * m[2][1];

	return minorAB * cofCD - minorAC * cofBD + minorAD * cofBC + minorBC * cofAD -
	       minorBD * cofAC + minorCD * cofAB;
}

/* (0x8144B2B8) 4-point sphere fit via Cramer's rule */
static void fitSphere4Points(const double pts[4][4], double *outCx, double *outCy,
			     double *outCz, double *outR)
{
	double mat[4][4];
	for (int r = 0; r < 4; r++) {
		mat[r][0] = pts[r][0];
		mat[r][1] = pts[r][1];
		mat[r][2] = pts[r][2];
		mat[r][3] = 1.0;
	}
	double detBase = det4x4(mat);
	if (detBase < 350.0 && detBase > -350.0) {
		s_mc.fitValid = 0;
		return;
	}

	double sqNorm[4];
	for (int r = 0; r < 4; r++) {
		sqNorm[r] = pts[r][0] * pts[r][0] + pts[r][1] * pts[r][1] + pts[r][2] * pts[r][2];
	}

	for (int r = 0; r < 4; r++) {
		mat[r][0] = sqNorm[r];
		mat[r][1] = pts[r][1];
		mat[r][2] = pts[r][2];
		mat[r][3] = 1.0;
	}
	double detX = det4x4(mat);

	for (int r = 0; r < 4; r++) {
		mat[r][0] = pts[r][0];
		mat[r][1] = sqNorm[r];
		mat[r][2] = pts[r][2];
		mat[r][3] = 1.0;
	}
	double detY = det4x4(mat);

	for (int r = 0; r < 4; r++) {
		mat[r][0] = pts[r][0];
		mat[r][1] = pts[r][1];
		mat[r][2] = sqNorm[r];
		mat[r][3] = 1.0;
	}
	double detZ = det4x4(mat);

	for (int r = 0; r < 4; r++) {
		mat[r][0] = sqNorm[r];
		mat[r][1] = pts[r][0];
		mat[r][2] = pts[r][1];
		mat[r][3] = pts[r][2];
	}
	double detD = det4x4(mat);

	double cx = 0.5 * detX / detBase;
	double cy = 0.5 * detY / detBase;
	double cz = 0.5 * detZ / detBase;
	double radSq = cx * cx + cy * cy + cz * cz - detD / detBase;
	*outCx = cx;
	*outCy = cy;
	*outCz = cz;
	*outR = (radSq > 0.0) ? (double)__builtin_sqrtf((float)radSq) : 0.0;
	s_mc.fitValid = 1;
}

/* (0x8144B100) mean residual of ring samples against fitted sphere */
static double computeSphereResidual(void)
{
	double totalErr = 0.0;
	for (int i = 0; i < s_mc.ringCapacity; i++) {
		double dx = (double)s_mc.ringMagn[i].x - s_mc.fittedCenterX;
		double dy = (double)s_mc.ringMagn[i].y - s_mc.fittedCenterY;
		double dz = (double)s_mc.ringMagn[i].z - s_mc.fittedCenterZ;
		double dist = (double)__builtin_sqrtf((float)(dx * dx + dy * dy + dz * dz));
		totalErr += (s_mc.fittedRadius - dist);
	}
	return (double)((totalErr < 0.0) ? -totalErr : totalErr);
}

/* (0x8144C7F0) update rolling 64-sample magnetometer stability detector */
static void updateStabilityDetector(double sx, double sy, double sz)
{
	float norm = __builtin_sqrtf((float)(sx * sx + sy * sy + sz * sz));
	s_mc.stabilityMagnNorms[s_mc.stabilityWritePos++] = norm;
	if (s_mc.stabilityWritePos >= kStabilityWindowSize) {
		s_mc.stabilityBufferFull = 1;
		s_mc.stabilityWritePos = 0;
	}
	if (!s_mc.stabilityBufferFull)
		return;

	double sum = 0.0;
	for (int i = 0; i < kStabilityWindowSize; i++)
		sum += (double)s_mc.stabilityMagnNorms[i];
	double mean = sum * (1.0 / (double)kStabilityWindowSize);
	s_mc.stabilityMean = mean;

	double varSum = 0.0;
	for (int i = 0; i < kStabilityWindowSize; i++) {
		double diff = mean - (double)s_mc.stabilityMagnNorms[i];
		varSum += diff * diff;
	}
	double stdDev = (double)__builtin_sqrtf((float)(varSum * (1.0 / (double)kStabilityWindowSize)));
	s_mc.stabilityStdDev = stdDev;

	switch (s_mc.stabilityState) {
	case 0:
		if (mean >= 50.0) {
			s_mc.stabilityHighCount = 0;
		}
		if (stdDev <= 3.0) {
			s_mc.stabilityHighCount = 0;
			s_mc.stabilityLowCount = 0;
			s_mc.stabilityBaseline = mean;
		} else if (++s_mc.stabilityHighCount > 19) {
			s_mc.stabilityLowCount = 0;
			s_mc.stabilityHighCount = 0;
			s_mc.stabilityState = 1;
		}
		s_mc.stabilityLevel = 0;
		break;
	case 1:
		if (stdDev <= 7.0) {
			if (stdDev >= 3.0) {
				s_mc.stabilityHighCount = 0;
				s_mc.stabilityLowCount = 0;
			} else if (++s_mc.stabilityLowCount > 19) {
				s_mc.stabilityState = 0;
				s_mc.stabilityHighCount = 0;
				s_mc.stabilityLowCount = 0;
			}
		} else {
			s_mc.stabilityLowCount = 0;
			if (++s_mc.stabilityHighCount > 14)
				s_mc.stabilityState = 2;
		}
		s_mc.stabilityLevel = 0;
		break;
	case 2: {
		double diffMean = mean - s_mc.stabilityBaseline;
		float delta = (float)((diffMean < 0.0) ? -diffMean : diffMean);
		if (delta >= 6.0f || stdDev >= 3.0 || mean >= 70.0) {
			s_mc.stabilityHighCount = 0;
			s_mc.stabilityLowCount = 0;
		} else if (++s_mc.stabilityLowCount > 20) {
			s_mc.stabilityState = 0;
		}
		s_mc.stabilityLevel = 1;
		break;
	}
	default:
		break;
	}
}

/* (0x8144BD38 & 0x8144B9E0) apply converged offset to motion device */
static int applySphereCalibration(double residual)
{
	MotionDevMagnCalibRecord calib;
	int ret = sceMotionDevGetCurrentMagnCalibData(&calib);
	if (ret < 0)
		return ret;

	unsigned int idx = s_mc.activeSetIndex;
	if (idx > 1U)
		idx = 0U;

	if (residual < 10.0) {
		if (s_mc.maxYawAngleDeg > 80.0f && calib.scale[idx].x != 0.0f) {
			s_mc.calibUpdated = 1;
			s_mc.magnOffset.x = (float)(s_mc.fittedCenterX * 10.0 / (double)calib.scale[idx].x);
		}
		if (s_mc.maxPitchAngleDeg > 80.0f && calib.scale[idx].y != 0.0f) {
			s_mc.calibUpdated = 1;
			s_mc.magnOffset.y = (float)(s_mc.fittedCenterY * 10.0 / (double)calib.scale[idx].y);
		}
		if (s_mc.maxRollAngleDeg > 80.0f && calib.scale[idx].z != 0.0f) {
			s_mc.calibUpdated = 1;
			s_mc.magnOffset.z = (float)(s_mc.fittedCenterZ * 10.0 / (double)calib.scale[idx].z);
		}
	}

	if (s_mc.calibUpdated && s_mc.minSpreadDeg < kMaxAngularSpreadDeg) {
		s_mc.minSpreadDeg += 2;
		s_mc.ringCapacity = (4 * s_mc.minSpreadDeg) / 5;
		if (s_mc.ringCapacity > kSampleRingCapacity)
			s_mc.ringCapacity = kSampleRingCapacity;
	}

	s_mc.maxRollAngleDeg = 0.0f;
	s_mc.maxPitchAngleDeg = 0.0f;
	s_mc.maxYawAngleDeg = 0.0f;
	s_mc.axisRollQuat = makeIdentityQuat();
	s_mc.axisPitchQuat = makeIdentityQuat();
	s_mc.axisYawQuat = makeIdentityQuat();

	if (!s_mc.calibUpdated)
		return 0;

	calib.offset[idx] = s_mc.magnOffset;
	SceUInt32 statusFlags = 0;
	ret = sceMotionDevUpdateMagnCalibData(&calib, &statusFlags);
	s_mc.stabilityLevel = 0;
	s_mc.stabilityState = 0;
	s_mc.stabilityHighCount = 0;
	s_mc.stabilityLowCount = 0;
	return (ret < 0) ? ret : 0;
}

static void resetCalibrationState(bool resetTimers)
{
	s_mc.fitCount = 0;
	s_mc.ringWritePos = 0;
	s_mc.ringUsedCount = 0;
	s_mc.hasFullRing = 0;
	s_mc.calibUpdated = 0;
	s_mc.rmsResidualX = 0.0;
	s_mc.rmsResidualY = 0.0;
	s_mc.rmsResidualZ = 0.0;
	s_mc.maxRollAngleDeg = 0.0f;
	s_mc.maxPitchAngleDeg = 0.0f;
	s_mc.maxYawAngleDeg = 0.0f;

	for (int i = 0; i < 4; i++) {
		s_mc.fitMagn[i].x = 0.0f;
		s_mc.fitMagn[i].y = 0.0f;
		s_mc.fitMagn[i].z = 0.0f;
		s_mc.fitQuat[i] = makeInitialFitQuat();
	}
	sce_paf_memset(s_mc.ringMagn, 0, sizeof(s_mc.ringMagn));

	s_mc.currentQuat = makeIdentityQuat();
	s_mc.lastSampleQuat = makeIdentityQuat();
	s_mc.axisRollQuat = makeIdentityQuat();
	s_mc.axisPitchQuat = makeIdentityQuat();
	s_mc.axisYawQuat = makeIdentityQuat();
	s_mc.latestMagn.x = 0.0f;
	s_mc.latestMagn.y = 0.0f;
	s_mc.latestMagn.z = 0.0f;

	if (resetTimers) {
		s_mc.windowStartTimeUs = sceKernelGetProcessTimeWide();
		s_mc.lastUpdateTimeUs = sceKernelGetProcessTimeWide();
	}
}

/* (0x8144C090) accumulate angular diversity and run 4-point sphere fit */
static int processCalibrationSample(void)
{
	s_mc.lastUpdateTimeUs = sceKernelGetProcessTimeWide();

	float deltaDeg = quatAngleDeg(s_mc.lastSampleQuat, s_mc.currentQuat);
	MotionCalibQuat identity = makeIdentityQuat();
	float rollDeg = quatAngleDeg(identity, s_mc.axisRollQuat);
	float pitchDeg = quatAngleDeg(identity, s_mc.axisPitchQuat);
	float yawDeg = quatAngleDeg(identity, s_mc.axisYawQuat);
	if (rollDeg > s_mc.maxRollAngleDeg)
		s_mc.maxRollAngleDeg = rollDeg;
	if (pitchDeg > s_mc.maxPitchAngleDeg)
		s_mc.maxPitchAngleDeg = pitchDeg;
	if (yawDeg > s_mc.maxYawAngleDeg)
		s_mc.maxYawAngleDeg = yawDeg;

	if (deltaDeg > 5.0f) {
		s_mc.lastSampleQuat = s_mc.currentQuat;
		if (s_mc.ringWritePos < kSampleRingCapacity) {
			s_mc.ringMagn[s_mc.ringWritePos].x = s_mc.latestMagn.x * 0.1f;
			s_mc.ringMagn[s_mc.ringWritePos].y = s_mc.latestMagn.y * 0.1f;
			s_mc.ringMagn[s_mc.ringWritePos].z = s_mc.latestMagn.z * 0.1f;
		}
		s_mc.ringUsedCount = s_mc.ringWritePos;
		++s_mc.ringWritePos;
		if (s_mc.ringWritePos >= s_mc.ringCapacity) {
			s_mc.hasFullRing = 1;
			s_mc.ringWritePos = 0;
			s_mc.ringUsedCount = 0;
		}
	}

	for (int i = 0; i < 4; i++) {
		if (quatAngleDeg(s_mc.fitQuat[i], s_mc.currentQuat) < (float)s_mc.minSpreadDeg)
			return 0;
	}

	if (s_mc.lastUpdateTimeUs - s_mc.windowStartTimeUs > kSphereFitTimeoutUs) {
		resetCalibrationState(true);
		return 0;
	}

	bool batchReady = false;
	++s_mc.fitCount;
	if (s_mc.fitCount == 4) {
		batchReady = true;
		s_mc.fitCount = 0;
		s_mc.windowStartTimeUs = sceKernelGetProcessTimeWide();
	}

	int slot = s_mc.fitCount;
	s_mc.fitMagn[slot].x = s_mc.latestMagn.x * 0.1f;
	s_mc.fitMagn[slot].y = s_mc.latestMagn.y * 0.1f;
	s_mc.fitMagn[slot].z = s_mc.latestMagn.z * 0.1f;
	s_mc.fitQuat[slot] = s_mc.currentQuat;

	if (!batchReady)
		return 0;

	double pts[4][4];
	for (int i = 0; i < 4; i++) {
		pts[i][0] = (double)s_mc.fitMagn[i].x;
		pts[i][1] = (double)s_mc.fitMagn[i].y;
		pts[i][2] = (double)s_mc.fitMagn[i].z;
		pts[i][3] = 1.0;
	}
	fitSphere4Points(pts, &s_mc.fittedCenterX, &s_mc.fittedCenterY,
			 &s_mc.fittedCenterZ, &s_mc.fittedRadius);
	if (!s_mc.hasFullRing || !s_mc.fitValid)
		return 0;

	s_mc.hasFullRing = 0;
	double residual = computeSphereResidual();
	return applySphereCalibration(residual);
}

/* (0x8144CB90) read buffered motion samples and step calibration */
static int stepMotionCalibration(void)
{
	MotionDevMagnCalibSample samples[16];
	SceUInt32 readFlags = 0;
	int count = sceMotionDevReadForMagnCalib(samples, 16, &readFlags);
	if (count < 0)
		return count;

	for (int i = 0; i < count; i++) {
		const MotionDevMagnCalibSample &sample = samples[i];
		SceUInt32 dtUs = (s_mc.lastSampleTimestampUs != 0)
				 ? (sample.timestampUs - s_mc.lastSampleTimestampUs)
				 : 0U;
		s_mc.lastSampleTimestampUs = sample.timestampUs;

		float dtSec = (dtUs < 1000000U) ? ((float)dtUs * 1e-6f) : 0.0f;

		MotionCalibVec3 omega;
		omega.x = ((float)sample.gyroX - s_mc.gyroZero.x) * s_mc.gyroGain.x;
		omega.y = ((float)sample.gyroY - s_mc.gyroZero.y) * s_mc.gyroGain.y;
		omega.z = ((float)sample.gyroZ - s_mc.gyroZero.z) * s_mc.gyroGain.z;

		integrateQuat(s_mc.currentQuat, omega, dtSec, &s_mc.currentQuat);

		MotionCalibVec3 rollOnly = { omega.x, 0.0f, 0.0f };
		MotionCalibVec3 pitchOnly = { 0.0f, omega.y, 0.0f };
		MotionCalibVec3 yawOnly = { 0.0f, 0.0f, omega.z };
		integrateQuat(s_mc.axisRollQuat, rollOnly, dtSec, &s_mc.axisRollQuat);
		integrateQuat(s_mc.axisPitchQuat, pitchOnly, dtSec, &s_mc.axisPitchQuat);
		integrateQuat(s_mc.axisYawQuat, yawOnly, dtSec, &s_mc.axisYawQuat);

		double mx = (double)sample.magnX;
		double my = (double)sample.magnY;
		double mz = (double)sample.magnZ;
		s_mc.latestMagn.x = (float)mx;
		s_mc.latestMagn.y = (float)my;
		s_mc.latestMagn.z = (float)mz;

		double sx = mx * s_mc.magnScaleX;
		double sy = my * s_mc.magnScaleY;
		double sz = mz * s_mc.magnScaleZ;
		s_mc.lastScaledMagn.x = (float)sx;
		s_mc.lastScaledMagn.y = (float)sy;
		s_mc.lastScaledMagn.z = (float)sz;

		updateStabilityDetector(sx, sy, sz);
		sceMotionDevUpdateMagnStabilityLevel((SceUInt32)s_mc.stabilityLevel);

		++s_mc.totalSamples;
		int err = processCalibrationSample();
		if (err < 0)
			return err;
	}
	return 0;
}

} /* anonymous namespace */

extern "C" {

/* (0x8144AB70) */
int shellMotionCalibInit(void)
{
	sce_paf_memset(&s_mc, 0, sizeof(s_mc));
	s_mc.minSpreadDeg = kMinAngularSpreadDeg;
	s_mc.ringCapacity = (4 * s_mc.minSpreadDeg) / 5;
	resetCalibrationState(false);

	MotionDevMagnCalibRecord magnCalib;
	int ret = sceMotionDevGetCurrentMagnCalibData(&magnCalib);
	if (ret < 0)
		return ret;

	unsigned int idx = s_mc.activeSetIndex;
	if (idx > 1U)
		idx = 0U;

	s_mc.magnOffset = magnCalib.offset[idx];
	s_mc.magnScaleX = (double)magnCalib.scale[idx].x;
	s_mc.magnScaleY = (double)magnCalib.scale[idx].y;
	s_mc.magnScaleZ = (double)magnCalib.scale[idx].z;
	s_mc.fittedCenterX = (double)(s_mc.magnOffset.x / 10.0f);
	s_mc.fittedCenterY = (double)(s_mc.magnOffset.y / 10.0f);
	s_mc.fittedCenterZ = (double)(s_mc.magnOffset.z / 10.0f);

	MotionDevGyroCalibRecord gyroCalib;
	ret = sceMotionDevGetGyroCalibData(&gyroCalib);
	if (ret < 0)
		return ret;

	s_mc.gyroZero = gyroCalib.zero;
	float spanX = gyroCalib.point[0].raw.x - gyroCalib.zero.x;
	float spanY = gyroCalib.point[1].raw.y - gyroCalib.zero.y;
	float spanZ = gyroCalib.point[2].raw.z - gyroCalib.zero.z;
	s_mc.gyroGain.x = (spanX != 0.0f) ? (gyroCalib.point[0].value / spanX) : 0.0f;
	s_mc.gyroGain.y = (spanY != 0.0f) ? (gyroCalib.point[1].value / spanY) : 0.0f;
	s_mc.gyroGain.z = (spanZ != 0.0f) ? (gyroCalib.point[2].value / spanZ) : 0.0f;

	MotionDevDeviceInfoRecord devInfo;
	ret = sceMotionDevGetDeviceInfo(&devInfo);
	if (ret < 0)
		return ret;

	s_mc.deviceType = devInfo.deviceType;
	s_mc.windowStartTimeUs = sceKernelGetProcessTimeWide();
	s_mc.lastUpdateTimeUs = sceKernelGetProcessTimeWide();
	s_mc.totalSamples = 0;

	MotionDevMeasModeRecord modeInfo;
	sce_paf_memset(&modeInfo, 0, sizeof(modeInfo));
	sceMotionDevGetMeasMode(&modeInfo);
	s_mc.measMode = modeInfo.mode;
	return 0;
}

/* (0x8144AE78) */
int shellMotionCalibTerm(void)
{
	resetCalibrationState(true);
	s_mc.totalSamples = 0;
	return 0;
}

/* (0x8144CF70) */
int shellMotionCalibGetState(ShellCalibrationState *state)
{
	if (state == NULL)
		return -1;

	s_mc.calibUpdated = 0;
	int ret = stepMotionCalibration();

	MotionDevMeasModeRecord modeInfo;
	sce_paf_memset(&modeInfo, 0, sizeof(modeInfo));
	sceMotionDevGetMeasMode(&modeInfo);
	s_mc.measMode = modeInfo.mode;

	state->count = s_mc.totalSamples;
	state->reserved = 0;
	state->version = 1;
	state->updated = (int)s_mc.calibUpdated;
	state->measMode = s_mc.measMode;
	return (ret >= 0) ? 0 : ret;
}

} /* extern "C" */

/* PLACEHOLDER: the calibration library's state (0x8144CF70 fills it) */
#define SHELL_CALIBRATION_MEAS_FAST	0x04

/* PLACEHOLDER: the thread updating the calibration (8 bytes) */
class ShellCalibrationUpdater {
public:
	ShellCalibrationUpdater() : thread(-1), running(false)
	{
	}

	~ShellCalibrationUpdater()
	{
		Reset();
	}

	void Reset()
	{
		running = false;
		thread = -1;
	}

	SceUID thread;
	bool running;
};

static ShellCalibrationUpdater *s_calibrationUpdater;

/* (0x810E18E6) */
static int shellCalibrationUpdateThread(SceSize args, void *argp)
{
	if (shellMotionCalibInit() == 0) {
		ShellCalibrationUpdater *updater = *(ShellCalibrationUpdater **)argp;

		while (updater->running) {
			ShellCalibrationState state;

			shellMotionCalibGetState(&state);
			if ((state.measMode & SHELL_CALIBRATION_MEAS_FAST) != 0)
				sceDisplayWaitVblankStartMulti(2);
			else
				sceDisplayWaitVblankStartMulti(125);
		}
	}
	shellMotionCalibTerm();
	return 0;
}

/* (0x810E181C) */
void shellCalibrationUpdateThreadCreate(void)
{
	if (s_calibrationUpdater != NULL)
		return;

	ShellCalibrationUpdater *updater = new ShellCalibrationUpdater();

	s_calibrationUpdater = updater;
	if (updater == NULL)
		return;

	int ret = sceKernelCreateThread("SceCalibrationUpdate", shellCalibrationUpdateThread,
					0x1000010A, 0x1000, 0, 0, NULL);

	/* the thread's argument, the updater; Sony passes 8 bytes from the 4-byte pointer */
	ShellCalibrationUpdater *arg = updater;

	if (ret >= 0) {
		updater->thread = ret;
		updater->running = true;
		if (sceKernelStartThread(ret, 8, &arg) >= 0)
			return;
		ret = sceKernelDeleteThread(updater->thread);
		updater->Reset();
	}
	if (ret < 0) {
		delete s_calibrationUpdater;
		s_calibrationUpdater = NULL;
	}
}
