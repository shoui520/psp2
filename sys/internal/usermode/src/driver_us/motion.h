/*
 * SceDriverUser: SceMotion library internals.
 */

#ifndef _MOTION_H
#define _MOTION_H

#include <stddef.h>
#include <scetypes.h>
#include <arm_neon.h>

/*
 * The library passes floating-point values in VFP registers (the user ABI);
 * the module may be compiled for soft-float argument passing, so every
 * function taking or returning one says so.
 */
#define MOTION_VFP	__attribute__((pcs("aapcs-vfp")))

/* --- vector math shared by the filter and the readers --- */

#define FLT_MIN_BITS	0x00800000
#define FLT_INF_BITS	0x7f800000

typedef union MotionFloatBits {
	float f;
	SceUInt32 u;
} MotionFloatBits;

static inline SceUInt32 MotionBits(float value)
{
	MotionFloatBits bits = { .f = value };

	return bits.u;
}

static inline float MotionFromBits(SceUInt32 value)
{
	MotionFloatBits bits = { .u = value };

	return bits.f;
}

/* 1 / sqrt(s): estimate and two refinement steps */
static inline float RsqrtNR(float s)
{
	float32x2_t v = vdup_n_f32(s);
	float32x2_t e = vrsqrte_f32(v);

	e = vmul_f32(e, vrsqrts_f32(vmul_f32(e, e), v));
	e = vmul_f32(e, vrsqrts_f32(vmul_f32(e, e), v));
	return vget_lane_f32(e, 0);
}

/* sqrt(s), 0 for s at or below FLT_MIN and infinity kept */
static inline float SafeSqrt(float s)
{
	if (!(s > MotionFromBits(FLT_MIN_BITS)))
		return 0.0f;
	if (MotionBits(s) == FLT_INF_BITS)
		return s;
	return s * RsqrtNR(s);
}

static inline float Dot3(float x, float y, float z)
{
	return (x * x + y * y) + z * z;
}


#define SCE_MOTION_ERROR_DATA_INVALID		((int)0x80360200)
#define SCE_MOTION_ERROR_READING		((int)0x80360201)
#define SCE_MOTION_ERROR_NON_INIT_ERR		((int)0x80360202)
#define SCE_MOTION_ERROR_STATE_INVALID		((int)0x80360203)
#define SCE_MOTION_ERROR_CALIB_READ_FAIL	((int)0x80360204)
#define SCE_MOTION_ERROR_OUT_OF_BOUNDS		((int)0x80360205)
#define SCE_MOTION_ERROR_NOT_SAMPLING		((int)0x80360206)
#define SCE_MOTION_ERROR_ALREADY_SAMPLING	((int)0x80360207)
#define SCE_MOTION_ERROR_MEM_IN_USE		((int)0x80360208)
#define SCE_MOTION_ERROR_NULL_PARAMETER		((int)0x80360209)
#define SCE_MOTION_ERROR_ANGLE_OUT_OF_RANGE	((int)0x8036020A)
#define SCE_MOTION_ERROR_INVALID_ARG		((int)0x8036020B)

typedef struct MotionDVector3 {
	double x;
	double y;
	double z;
} MotionDVector3;

/* --- SceMotionDev (kernel driver) records --- */

typedef struct MotionCalibPoint {
	float value;		/* reference g or angular rate */
	SceFVector3 raw;	/* counts measured at it */
} MotionCalibPoint;

/* points: +x, +y, +z, -x, -y, -z */
typedef struct MotionDevGyroCalibData {
	SceFVector3 zero;
	MotionCalibPoint point[6];
} MotionDevGyroCalibData;

typedef struct MotionDevAccCalibData {
	MotionCalibPoint point[6];
	SceUInt32 reserve;
} MotionDevAccCalibData;

/* offset and scale sets for the magnetometer */
typedef struct MotionMagnCalibration {
	SceFVector3 vector[4];
} MotionMagnCalibration;

typedef struct MotionDevMagnCalibData {
	SceUInt32 reserve;
	MotionMagnCalibration calibration;
} MotionDevMagnCalibData;

typedef struct MotionDevGyroBiasData {
	SceUInt32 reserve;
	SceFVector3 bias;
} MotionDevGyroBiasData;

typedef struct MotionDevDeviceInfo {
	SceUInt8 reserve0[3];
	SceUInt8 type;		/* selects the axis convention */
	SceUInt8 reserve1[44];
} MotionDevDeviceInfo;

int sceMotionDevIsReady(void);
int sceMotionDevGetDeviceInfo(MotionDevDeviceInfo *info);
int sceMotionDevGetAccCalibData(MotionDevAccCalibData *data);
int sceMotionDevGetAccCalibData2(int port, MotionDevAccCalibData *data);
int sceMotionDevGetGyroCalibData(MotionDevGyroCalibData *data);
int sceMotionDevGetGyroCalibData2(int port, MotionDevGyroCalibData *data);
int sceMotionDevGetCurrentMagnCalibData(MotionDevMagnCalibData *data);
int sceMotionDevGetGyroBias(MotionDevGyroBiasData *bias);

/* --- calibration and conversion (motion_calib.c) --- */

/* scale = reference / (counts - offset), separately for each direction */
typedef struct MotionAxisCalibration {
	SceFVector3 offset;
	SceFVector3 positive;
	SceFVector3 negative;
} MotionAxisCalibration;

typedef struct MotionSample {
	MotionDVector3 magn;
	MotionDVector3 acc;
	MotionDVector3 gyro;
	int magnValid;
	int reserve;
	int accValid;
} MotionSample;


MOTION_VFP int MotionConvertAcc(MotionSample *sample, const MotionAxisCalibration *calibration,
				double x, double y, double z);
MOTION_VFP int MotionConvertGyro(MotionSample *sample, const MotionAxisCalibration *calibration,
				 double x, double y, double z);
MOTION_VFP int MotionConvertMagn(int index, MotionSample *sample, const MotionMagnCalibration *calibration,
				 double x, double y, double z);
void MotionMagnOffset(SceFVector4 *out, const MotionMagnCalibration *calibration);
void MotionMagnScale(SceFVector4 *out, const MotionMagnCalibration *calibration);
void MotionReadMagnCalibration(MotionMagnCalibration *calibration);
void MotionReadGyroBias(SceFVector3 *bias);
void MotionSetVector3(SceFVector3 *a, SceFVector3 *b, SceFVector3 *c, const SceFVector3 *value);
int MotionReadGyroCalibration(int port, MotionAxisCalibration *calibration);
int MotionReadAccCalibration(int port, MotionAxisCalibration *calibration);
int MotionReadCalibration(MotionAxisCalibration *gyro, MotionAxisCalibration *acc,
			  MotionMagnCalibration *magn);

/* --- DualShock 3 integer sensor processing (motion_ds3.c) --- */

typedef struct Ds3Window {
	int length;		/* samples per window */
	int count;
	int sum;
	int min;
	int max;
	int windowMean;		/* of the last complete window */
	int windowMin;
	int windowMax;
} Ds3Window;

typedef struct Ds3Ring {
	int *buffer;
	int size;
	int count;
	int index;
	int sum;
	int range;		/* maximum - minimum once full */
} Ds3Ring;

typedef struct Ds3Average {
	int length;
	int count;
	int sum;
	int mean;
} Ds3Average;

/* the gyro's rest offset: whole steps reported to the caller, and raw rest value */
typedef struct Ds3TrackerState {
	int offset;
	int bias;
} Ds3TrackerState;

typedef struct Ds3TrackerConfig {
	int center;
	int offset;
	int bias;
	int stepScale;
	int holdCount;
	int holdReload;
	int rangeMax;
	int rangeMin;
	int reserve0;
	int windowLength;
	int spreadLimit;
	int ringSize;
	int rangeLimit;
	int averageLength;
	int reserve1;
	int countdownSteps;
	int countdown;
} Ds3TrackerConfig;

/* gyro rest-offset tracker */
typedef struct Ds3Tracker {
	int center;
	Ds3TrackerState state;
	int stepScale;
	int lastInput;
	int output;
	int holdReload;
	int holdCount;		/* samples left before tracking resumes */
	int initialOffset;
	int initialBias;
	int rangeMax;
	int rangeMin;
	SceUInt8 outOfRange;
	SceUInt8 padding0[3];
	int enabled;
	Ds3Window window;
	int windowMean;
	int spread;
	int spreadLimit;
	Ds3Ring ring;
	int ringBuffer[4];
	int ringMean;
	int ringRange;
	int rangeLimit;
	Ds3Average average;
	int averageMean;
	int countdownStep;
	int countdown;
	int pendingCountdown;
	Ds3TrackerState pending;
	SceUInt8 isPending;
	SceUInt8 padding1[3];
} Ds3Tracker;

typedef struct Ds3Calibration {
	int rawSpan;
	int outSpan;
	int outRef;
	int rawRef;
	int reserve0;
	int reserve1;
	int min;
	int max;
} Ds3Calibration;

typedef struct Ds3LowPassCoefficients {
	int a1;
	int a2;
	int b0;
	int b1;
	int b2;
	int reserve;
} Ds3LowPassCoefficients;

/* second-order low-pass, direct form II: Q14 state, Q18 output */
typedef struct Ds3LowPass {
	int reserve;
	int w1;
	int w2;
	int a1;
	int a2;
	int b0;
	int b1;
	int b2;
	int center;
} Ds3LowPass;

typedef struct Ds3LowPassHeader {
	int reserve;
	Ds3LowPass filter;
} Ds3LowPassHeader;

typedef struct Ds3Axis {
	int reserve;
	Ds3Calibration calibration;
	int output;
	Ds3LowPassHeader lowPass;
	int *pOutput;
	Ds3LowPassHeader *pLowPass;
	int filterMode;		/* 1: low-pass the calibrated value */
	int center;
} Ds3Axis;

typedef struct Ds3Axes {
	int reserve;
	Ds3Axis storage[4];
	Ds3Axis *axis[4];
} Ds3Axes;

typedef struct Ds3AxisRecord {
	int type;		/* 3: points 0..2, 2: points 0..1 */
	int point[6];		/* raw, calibrated pairs */
} Ds3AxisRecord;

typedef struct Ds3SpanRecord {
	int rawRef;
	int outRef;
	int rawSpan;
	int outSpan;
} Ds3SpanRecord;

typedef struct Ds3CalibrationRecords {
	Ds3AxisRecord accel[3];
	Ds3SpanRecord gyro;
} Ds3CalibrationRecords;

typedef struct Ds3Sensor {
	Ds3Tracker tracker;
	Ds3Axes axes;
	SceUInt8 trackerEnabled;
	SceUInt8 initialized;
	SceUInt16 gyroCenter;
} Ds3Sensor;

int Ds3SensorInit(Ds3Sensor *sensor, const SceUInt16 *calibration, int *offset);
int Ds3SensorProcess(Ds3Sensor *sensor, SceUInt16 *sample, int *offset);


/* --- orientation state (motion_fusion.c) --- */

/* one processed state; the library keeps a ring of them */
typedef struct MotionEntry {
	SceFVector4 acc;
	SceFVector4 gyro;
	SceFVector4 magn;
	SceFVector4 magnRaw;
	SceFVector4 magnOffset;
	SceFVector4 magnScale;
	MotionSample sample;		/* 0x54 bytes, padded to 0x58 */
	SceUInt8 reserve0[8];
	SceUInt32 timestamp;		/* state timestamp, sample time units x 1000 */
	float interval;			/* since the previous state, sample time units */
	SceUInt64 hostTimestamp;
	SceFQuaternion quat;
	SceFVector4 ned[3];		/* rows: north, east, down */
	SceFVector4 basicOrientation;
	SceUInt32 seq;			/* states processed so far */
	int gyroStill[3];		/* per axis, after the deadband test */
	int gyroNew;
	int accNew;
	int magnNew;
	int reserve1;
	SceUInt8 dataInfo;
	SceUInt8 padding[7];
} MotionEntry;

typedef struct MotionConfig {
	int tiltCorrection;
	int reserve;
	int deadband;
	float angleThreshold[3];	/* basic orientation hysteresis, degrees */
	int magnetometer;
	int gyroBiasCorrection;
} MotionConfig;

#define MOTION_ENTRIES		64

int MotionGyroDeadband(MotionEntry *entries, MotionEntry *entry);
int MotionTiltCorrect(SceFQuaternion *quat, const SceFVector4 *acc);
MOTION_VFP int MotionIntegrate(const SceFQuaternion *quat, const SceFVector4 *gyro,
			       SceFQuaternion *out, float dt);
MOTION_VFP int MotionRotateEntryYaw(MotionEntry *entry, float yaw);
int MotionUpdateNed(MotionEntry *entry);
MOTION_VFP int MotionUpdateBasicOrientation(MotionEntry *entry, float standThreshold,
					    float flatThreshold, float sideThreshold);

MOTION_VFP float MotionAtanPositive(float x);
MOTION_VFP float MotionSin(float angle);
MOTION_VFP float MotionCos(float angle);
MOTION_VFP float MotionAtan(float x);
MOTION_VFP float MotionAtan2(float y, float x);
MOTION_VFP float MotionAsin(float x);
MOTION_VFP float MotionAcos(float x);
MOTION_VFP float MotionTan(float angle);

/* --- SceMotion API records (SDK motion.h) --- */

typedef struct SceMotionState {
	SceUInt32 timestamp;
	SceFVector3 acceleration;
	SceFVector3 angularVelocity;
	SceUInt8 reserve1[12];
	SceFQuaternion deviceQuat;
	SceFMatrix4 rotationMatrix;
	SceFMatrix4 nedMatrix;
	SceUInt8 reserve2[4];
	SceFVector3 basicOrientation;
	SceUInt64 hostTimestamp;
	SceUInt8 reserve3[36];
	SceUInt8 magnFieldStability;
	SceUInt8 dataInfo;
	SceUInt8 reserve4[2];
} SceMotionState;

typedef struct SceMotionSensorState {
	SceFVector3 accelerometer;
	SceFVector3 gyro;
	SceUInt8 reserve1[12];
	SceUInt32 timestamp;
	SceUInt32 counter;
	SceUInt8 reserve2[4];
	SceUInt64 hostTimestamp;
	SceUInt8 dataInfo;
	SceUInt8 reserve3[7];
} SceMotionSensorState;

typedef struct SceMotionDeviceLocation {
	SceFVector3 accelerometer;
	SceFVector3 gyro;
	SceUInt8 reserve[24];
} SceMotionDeviceLocation;

/* --- SceMotionDev sample records --- */

typedef struct MotionDevSample {
	SceUInt32 timestamp;
	SceUInt16 reserve0;
	SceUInt16 flags;		/* MOTION_SAMPLE_* */
	SceUInt16 gyro[3];
	SceUInt16 reserve1[3];
	SceUInt16 acc[3];
	SceInt16 magn[3];
	SceUInt32 reserve2;
} MotionDevSample;

#define MOTION_SAMPLE_GYRO	0x1
#define MOTION_SAMPLE_ACC	0x4
#define MOTION_SAMPLE_MAGN	0x8

typedef struct MotionDevData {
	SceUInt64 hostTimestamp;
	SceUInt8 magnSet;		/* bit 0: magnetometer calibration set */
	SceUInt8 reserve0;
	SceUInt8 timestampWrap;		/* low nibble: wraps of the 16-bit tick */
	SceUInt8 count;
	SceUInt32 magnCalibTag;
	SceUInt8 magnStability;
	SceUInt8 calibTag;
	SceUInt8 reserve1[2];
	SceUInt32 gyroBiasTag;
	MotionDevSample sample[6];
} MotionDevData;

typedef struct MotionDevDeviceLocation {
	SceFVector3 accelerometer;
	SceFVector3 gyro;
	SceUInt8 reserve[24];
} MotionDevDeviceLocation;

typedef struct MotionLwMutex {
	SceUInt64 data[4];
} MotionLwMutex;

/* --- extra controllers (ports 1-4) --- */

typedef struct MotionDs3Raw {
	SceUInt16 sample[4];		/* accelerometer x, y, z and gyro, 10-bit */
	SceUInt8 calibTag;
	SceUInt8 padding[3];
	SceUInt16 calibration[10];	/* DualShock 3 calibration record */
	SceUInt8 reserve[4];
} MotionDs3Raw;

typedef struct MotionPort {
	MotionDevData records[64];
	MotionAxisCalibration gyroCal;
	MotionAxisCalibration accCal;
	int sampling;
	int tiltCorrection;
	int deadband;
	int reserve0;
	SceFQuaternion quat;
	SceFVector4 gyro;
	SceFVector4 acc;
	SceUInt32 lastTimestamp;
	SceUInt32 gyroBiasTag;
	SceUInt8 calibTag;
	SceUInt8 padding[7];
	SceUInt64 hostTimestamp;
	MotionDs3Raw ds3Raw;
	Ds3Sensor ds3;
	int ds3Offset;
	int reserve1;
} MotionPort;

/* the library's whole state, laid out as Sony's */
typedef struct MotionGlobals {
	SceUInt32 prevIndex;
	SceUInt32 shellIndex;			/* catch-up position of SceMotion_D23686B0 */
	SceUInt8 magnStability;
	SceUInt8 padding0[7];
	MotionEntry entries[MOTION_ENTRIES];
	SceUInt32 magnCalibTag;
	MotionAxisCalibration gyroCal;
	SceUInt32 stateIndex;			/* catch-up position of sceMotionGetState */
	SceUInt32 writeIndex;
	MotionConfig config;
	MotionMagnCalibration magnCal;
	SceUInt32 readIndex;
	int sampling;
	SceUInt32 gyroBiasTag;
	SceUInt32 prevTimestamp;
	int samplingShell;
	int initialized;
	int dataInfo;
	MotionAxisCalibration accCal;
	int samplingExtra;
	SceUInt32 lastTimestamp;
	SceUInt32 samplingRefs;
	SceUInt32 count;
	MotionDevData records[64];
	SceUInt32 extraIndex;			/* catch-up position of SceMotion_AD0B62B7 */
	SceUInt32 sensorIndex;			/* catch-up position of the sensor-state readers */
	SceUID ds3Block;
	int reserve0;
	MotionPort *ds3Port[4];
	SceUInt8 deviceType;
	SceUInt8 padding1[3];
	int orientX;
	int orientZ;
	int reserve1;
	MotionLwMutex mutex;
	const Ds3LowPassCoefficients *ds3LowPass[3];
	int reserve2;
} MotionGlobals;

extern MotionGlobals g_motion;

int MotionUpdateEntry(SceUInt32 prev, SceUInt32 index);

#endif /* _MOTION_H */
