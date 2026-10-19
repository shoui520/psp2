/*
 * SceMotionDevDummy: the motion device on hardware without motion sensors
 * (PS TV). The built-in sensors read as zero; a virtual motion driver, once
 * registered, supplies samples of DUALSHOCK controllers on ports 1-4.
 *
 * No debug records or strings name the private functions and structures;
 * the names are ours.
 */
#pragma once

#include <stdint.h>
#include <scetypes.h>
#include <motion/motion_dev.h>

#define MOTION_PROCESS_MAX	16
#define MOTION_PORT_MAX		4

/* sizes of the user structures the dummy only zero-fills */
#define MOTION_DEVICE_INFO_SIZE		0x2c
#define MOTION_EVA_INFO_SIZE		0x48
#define MOTION_CALIBRATION_HEADER_SIZE	0xc
#define MOTION_CALIBRATION_DATA_SIZE	0x18
#define MOTION_MAGN_CALIB_DATA_SIZE	0x34
#define MOTION_DEVICE_LOCATION_SIZE	0x30
#define MOTION_MODE_INFO_SIZE		0x8

/* MotionState.state */
#define MOTION_STATE_SAMPLING	1
#define MOTION_STATE_STOPPED	2
#define MOTION_STATE_SUSPENDED	5
#define MOTION_STATE_RESUMING	6

/* controller types reported by the virtual motion driver */
#define MOTION_CONTROLLER_DS3	4	/* three-axis accelerometer, one-axis gyro */
#define MOTION_CONTROLLER_DS4	8	/* three-axis accelerometer and gyro */

/* what sceMotionDevRead3 returns for a DS3 */
typedef struct MotionDs3Report {
	uint16_t accel[3];		/* 0x0 */
	uint16_t gyro;			/* 0x6 */
	uint8_t generation;		/* 0x8: low nibble */
	uint8_t reserved[3];		/* 0x9 */
} MotionDs3Report;

typedef struct MotionProcess {
	SceUID pid;
	int readers[MOTION_PORT_MAX];	/* reader in each port's ring */
	int stopped;
	int sampling;
	uint8_t reserved[3];
	uint8_t rebase;			/* recompute timestampOffset on the next read */
	uint32_t lastTimestamp;
	uint32_t timestampOffset;
} MotionProcess;

typedef struct MotionController {
	uint64_t identity;				/* 0x000 */
	uint32_t timestamp;				/* 0x008 */
	uint16_t sequence;				/* 0x00c */
	uint8_t generation;				/* 0x00e */
	uint8_t type;					/* 0x00f: MOTION_CONTROLLER_* */
	MotionRing *ring;				/* 0x010 */
	SceMotionDevGyroCalibData gyroCalibration;	/* 0x014 */
	SceMotionDevAccCalibData accCalibration;	/* 0x080 */
	SceMotionDevGyroBiasData gyroBias;		/* 0x0e0 */
	GyroFilter filter;				/* 0x0f0 */
	SceMotionDevDs3CalibData ds3Calibration;	/* 0x184 */
	MotionDs3Report ds3Report;			/* 0x19c */
} MotionController;

typedef struct MotionState {
	int state;			/* MOTION_STATE_* */
	int savedState;			/* state while suspended */
	int lock;
	SceUID timer;
	unsigned int period;		/* sampling period, microseconds */
	MotionProcess processes[MOTION_PROCESS_MAX];
	SceUID sysEventHandler;
	SceUID procEventHandler;
	SceUID owner;			/* process that owns new samples */
	const MotionVirtualDriver *driver;
	uint32_t activePorts;		/* bit (port) set while sampling */
	MotionController controllers[MOTION_PORT_MAX];
} MotionState;

_Static_assert(sizeof(SceMotionDevDataEntry) == 0x24, "entry layout");
_Static_assert(sizeof(SceMotionDevGyroCalibData) == 0x6c, "gyro calibration layout");
_Static_assert(sizeof(SceMotionDevAccCalibData) == 0x60, "acc calibration layout");
_Static_assert(sizeof(SceMotionDevData) == 0xf0, "data layout");
_Static_assert(sizeof(MotionRing) == 0x50, "ring layout");
_Static_assert(sizeof(MotionProcess) == 0x28, "process layout");
_Static_assert(sizeof(GyroFilter) == 0x94, "filter layout");
_Static_assert(sizeof(MotionController) == 0x1a8, "controller layout");
_Static_assert(sizeof(MotionState) == 0x948, "state layout");

/* motion_dummy.c: the work behind the user syscalls */
int motionRead(SceMotionDevData *data, int count, SceUInt32 *info);
int motionReadForMagnCalib(SceMotionDevData *data, int count);
int motionReadPort(int port, SceMotionDevData *data, int count, SceUInt32 *info);
int motionRead2(int port, SceMotionDevData *data, int count, SceUInt32 *info);
int motionRead3(int port, MotionDs3Report *report, int count, SceUInt32 *info);
int motionSamplingStart2(int port);
int motionSamplingStop2(int port);
int motionMagnSamplingStart(void);
int motionMagnSamplingStop(void);
int motionUpdateMagnCalibData(const void *data, SceUInt32 *tag);
int motionGetCurrentMagnCalibData(void *data);
int motionGetFactoryMagnCalibData(void *data);
int motionUpdateMagnStabilityLevel(void);
int motionGetCurrentMagnStabilityLevel(SceUInt32 *level);
int motionGetGyroCalibData(SceMotionDevGyroCalibData *data);
int motionGetAccCalibData(SceMotionDevAccCalibData *data);
int motionGetGyroBias(SceMotionDevGyroBiasData *bias);
int motionGetDeviceLocation(void *location);
int motionGetMeasMode(void *info);
int motionGetControllerType(int port, uint8_t *type);
int motionSetGyroFeedBack(int port, int feedback);
int motionGetGyroCalibData2(int port, SceMotionDevGyroCalibData *data);
int motionGetAccCalibData2(int port, SceMotionDevAccCalibData *data);
int motionGetGyroBias2(int port, SceMotionDevGyroBiasData *bias);
int motionGetDs3CalibData(int port, SceMotionDevDs3CalibData *data);

/* heap.c */

/* motion_dummy.c */
int SceMotionDevForDriver_3B23DF55(SceUInt32 blockId, uint8_t *header);
int SceMotionDevForDriver_F0251700(SceUInt32 blockId, void *data, SceUInt32 count);
int SceMotionDevForDriver_5B53AC26(void);
