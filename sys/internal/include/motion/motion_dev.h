/* Motion device samples shared by the sensor and virtual-device drivers. */
#pragma once

#include <stdint.h>
#include <scetypes.h>

typedef struct MotionMagnDeviceInfo {
	uint16_t deviceId;
	uint8_t status;
	uint8_t sensitivity[3];
	int16_t selfTest[3];
} MotionMagnDeviceInfo;

/* Device-info wire descriptors are exposed unchanged by the public getter. */
typedef struct SceMotionDevDeviceInfo {
	uint16_t firmwareVersion;
	uint16_t hardwareInfo;
	uint32_t deviceMetadata;
	uint16_t protocolVersion;
	uint8_t attributes[4];
	union __attribute__((packed)) {
		struct __attribute__((packed)) {
			uint16_t descriptor;
			union {
				uint16_t legacy[4];
				struct {
					uint8_t attributes[4];
					uint16_t values[6];
				} current;
			} configuration;
			uint8_t extension;
			uint8_t reserved[11];
		};
		struct __attribute__((packed)) {
			uint32_t sensorStatus;
			MotionMagnDeviceInfo magnetometer;
			uint8_t reserved[14];
		} direct;
	};
} SceMotionDevDeviceInfo;

_Static_assert(sizeof(MotionMagnDeviceInfo) == 12, "magnetometer device info size");
_Static_assert(sizeof(SceMotionDevDeviceInfo) == 44, "motion device info size");

int sceMotionDevGetDeviceInfo(SceMotionDevDeviceInfo *info);

#define MOTION_DATA_ENTRIES	6
#define MOTION_DATA_OTHER_OWNER	0x40
#define MOTION_RING_READERS	16
#define MOTION_RING_SAMPLES	64

#define MOTION_ERROR(code)	((int)(0x80360000U | (code)))
#define MOTION_ERROR_INVALID_ARG	MOTION_ERROR(0x01)
#define MOTION_ERROR_NOT_PERMITTED	MOTION_ERROR(0x02)
#define MOTION_ERROR_NO_RESOURCE	MOTION_ERROR(0x05)
#define MOTION_ERROR_NOT_FOUND		MOTION_ERROR(0xff)

#define MOTION_ENTRY_GYRO	0x01
#define MOTION_ENTRY_ACCEL	0x04
#define MOTION_ENTRY_STATUS	0x10

typedef struct SceMotionDevCalibrationHeader {
	uint8_t format[2];
	uint16_t reserved0;
	uint32_t metadata;
	uint16_t recordCount;
	uint16_t reserved1;
} SceMotionDevCalibrationHeader;

typedef struct SceMotionDevCalibrationData {
	uint8_t kind;
	uint8_t channel;
	uint8_t attributes[3];
	uint8_t reserved[3];
	uint32_t parameters[4];
} SceMotionDevCalibrationData;

_Static_assert(sizeof(SceMotionDevCalibrationHeader) == 12,
	       "SceMotionDevCalibrationHeader size");
_Static_assert(sizeof(SceMotionDevCalibrationData) == 24,
	       "SceMotionDevCalibrationData size");

typedef struct SceMotionDevDataEntry {
	uint32_t timestamp;		/* 0x00 */
	uint16_t sequence;		/* 0x04 */
	uint16_t flags;			/* 0x06: MOTION_ENTRY_* */
	uint16_t gyro[3];		/* 0x08 */
	uint16_t gyroSecondary[3];	/* 0x0e: second gyro block in legacy format */
	uint16_t accel[3];		/* 0x14 */
	uint16_t magnetometer[3];		/* 0x1a */
	uint16_t statusData;		/* 0x20: first halfword of the status block */
	uint16_t status;			/* 0x22 */
} SceMotionDevDataEntry;

typedef struct SceMotionDevData {
	uint64_t time;			/* 0x00: system time of the sample */
	uint8_t flags;			/* 0x08: MOTION_DATA_* */
	uint8_t sampleFormat;		/* 0x09: packed sensor sample format */
	uint8_t timestampWraps;		/* 0x0a: low nibble */
	uint8_t count;			/* 0x0b: entries in use */
	uint32_t magnCalibrationGeneration; /* 0x0c */
	uint8_t magnStabilityLevel;	/* 0x10: current magnetometer stability */
	uint8_t generation;		/* 0x11: controller generation, low nibble */
	uint8_t reserved2[2];		/* 0x12 */
	uint32_t biasGeneration;		/* 0x14 */
	SceMotionDevDataEntry entry[MOTION_DATA_ENTRIES]; /* 0x18 */
} SceMotionDevData;

_Static_assert(sizeof(SceMotionDevDataEntry) == 0x24, "SceMotionDevDataEntry size");
_Static_assert(__builtin_offsetof(SceMotionDevDataEntry, gyroSecondary) == 0xe &&
	       __builtin_offsetof(SceMotionDevDataEntry, statusData) == 0x20,
	       "SceMotionDevDataEntry layout");
_Static_assert(sizeof(SceMotionDevData) == 0xf0, "SceMotionDevData size");
_Static_assert(__builtin_offsetof(SceMotionDevData, magnCalibrationGeneration) == 0xc &&
	       __builtin_offsetof(SceMotionDevData, magnStabilityLevel) == 0x10,
	       "SceMotionDevData generations");

/* A calibration reference input and the sensor's three axis readings. */
typedef struct MotionCalibPoint {
	float reference;
	float value[3];
} MotionCalibPoint;

typedef struct SceMotionDevGyroCalibData {
	float zeroRate[3];
	MotionCalibPoint positive[3];
	MotionCalibPoint negative[3];
} SceMotionDevGyroCalibData;

typedef struct SceMotionDevAccCalibData {
	MotionCalibPoint positive[3];
	MotionCalibPoint negative[3];
} SceMotionDevAccCalibData;

typedef struct SceMotionDevGyroBiasData {
	uint32_t generation;
	float bias[3];
} SceMotionDevGyroBiasData;

/* Samples shared by up to sixteen independent readers. */
typedef struct MotionRing {
	int readers[MOTION_RING_READERS];
	int writeIndex;
	unsigned int capacity;
	SceMotionDevData *samples;
	SceUID *owners;
} MotionRing;

typedef struct GyroFilter {
	int enabled;
	float scale[3];
	float bias[3];
	uint32_t flags;
	float shortClamp[3];
	float relaxedLimit[3];
	float gain;
	uint32_t accumulated[3];
	uint32_t acceptedWindows;
	uint32_t windows;
	float longClamp[3];
	float strictLimit[3];
	float strictGain;
	uint32_t maximum[3];
	uint32_t minimum[3];
	uint32_t sum[3];
	uint16_t windowSize;
	uint16_t samples;
} GyroFilter;

_Static_assert(sizeof(MotionCalibPoint) == 0x10, "calibration point layout");
_Static_assert(sizeof(SceMotionDevGyroCalibData) == 0x6c, "gyro calibration layout");
_Static_assert(sizeof(SceMotionDevAccCalibData) == 0x60, "acc calibration layout");
_Static_assert(sizeof(SceMotionDevGyroBiasData) == 0x10, "gyro bias layout");
_Static_assert(sizeof(MotionRing) == 0x50, "ring layout");
_Static_assert(sizeof(GyroFilter) == 0x94, "filter layout");

void *motionAllocHeap(SceSize size);
int motionFreeHeap(void *ptr);
int motionUseWideGyroFilter(void);

int motionRingInit(MotionRing *ring, SceMotionDevData *samples, SceUID *owners,
		   unsigned int capacity);
int motionRingOpenReader(MotionRing *ring);
int motionRingCloseReader(MotionRing *ring, unsigned int reader);
int motionRingPeek(MotionRing *ring, unsigned int reader, unsigned int offset,
		   SceMotionDevData **sample, SceUID **owner);
void motionRingConsume(MotionRing *ring, unsigned int reader);
int motionRingCount(MotionRing *ring, unsigned int reader);
void motionRingGetWriteSlot(MotionRing *ring, SceMotionDevData **sample, SceUID **owner);
void motionRingAdvance(MotionRing *ring);
int motionRingResetReader(MotionRing *ring, unsigned int reader);

void clearGyroFilter(GyroFilter *filter);
void initGyroFilter(GyroFilter *filter, const SceMotionDevGyroCalibData *calibration,
		    int virtual);
void resetGyroFilter(GyroFilter *filter);
int updateGyroFilter(GyroFilter *filter, const uint16_t *gyro, float *bias, float *delta);

typedef struct SceMotionDevDs3CalibData {
	uint16_t value[12];
} SceMotionDevDs3CalibData;

/* A complete sample filled by the registered external motion driver. */
typedef struct MotionDriverSample {
	uint64_t identity;
	uint16_t timestamp;
	uint16_t gyro[3];
	uint16_t accel[3];
	uint8_t status;
	uint8_t reserved;
} MotionDriverSample;

typedef struct MotionVirtualDriver {
	int (*getSample)(int port, MotionDriverSample *sample);
	int (*getGyroCalibData)(int port, SceMotionDevGyroCalibData *data);
	int (*getAccCalibData)(int port, SceMotionDevAccCalibData *data);
	int (*getControllerType)(int port, uint8_t *type);
	int (*getDs3CalibData)(int port, SceMotionDevDs3CalibData *data);
	int (*setGyroFeedBack)(int port, int feedback);
} MotionVirtualDriver;

_Static_assert(sizeof(SceMotionDevDs3CalibData) == 24, "DS3 calibration layout");
_Static_assert(sizeof(MotionDriverSample) == 24, "virtual motion sample layout");
_Static_assert(sizeof(MotionVirtualDriver) == 24, "virtual motion driver layout");

/* vitasdk: ksceMotionDevRegisterVirtualMotionDriver */
int SceMotionDevForDriver_7FD06731(const MotionVirtualDriver *driver);

int SceMotionDevForDriver_031AF7A1(void);
int sceMotionDevIsReady(void);
int SceMotionDevForDriver_11A17A96(void);	/* vitasdk: ksceMotionDevNoiseFilterIsAvailable */
int SceMotionDevForDriver_22CD6DCA(SceUID owner);
int sceMotionDevRead(SceMotionDevData *data, int count, unsigned int *info);
int SceMotionDevForDriver_3CD0CE14(SceUID *owner);
int SceMotionDevForDriver_77BA3A04(void);
int SceMotionDevForDriver_85E3C678(void);
int sceMotionDevUpdateRestart(void);
int sceMotionDevSamplingStart(void);
int SceMotionDevForDriver_D7DA3DA7(void);
int sceMotionDevSetSamplingMode(unsigned int mode);
int sceMotionDevSamplingStop(void);
int sceMotionDevUpdateInit(void);

/* SceMotionDev: the user syscalls */
struct MotionDs3Report;

int sceMotionDevGetControllerType(int port, uint8_t *type);
int sceMotionDevGetCurrentMagnStabilityLevel(SceUInt32 *level);
int sceMotionDevGetDs3CalibData(int port, SceMotionDevDs3CalibData *data);
int sceMotionDevGetEvaInfoForUser(void *info);
int sceMotionDevGetMeasMode(void *info);
int sceMotionDevIsReadyForUser(void);
int sceMotionDevMagnSamplingStart(void);
int sceMotionDevMagnSamplingStop(void);
int sceMotionDevRead3(int port, struct MotionDs3Report *report, int count, SceUInt32 *info);
int sceMotionDevSamplingStartForUser(void);
int sceMotionDevSamplingStart2(int port);
int sceMotionDevSamplingStopForUser(void);
int sceMotionDevSamplingStop2(int port);
int sceMotionDevSetGyroFeedBack(int port, int feedback);
int sceMotionDevSetSamplingModeForUser(unsigned int mode);
int sceMotionDevUpdateMagnStabilityLevel(SceUInt32 level);
