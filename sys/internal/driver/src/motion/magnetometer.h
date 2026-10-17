/* Motion sensor state; private to SceMotionDev's compass driver. */
#pragma once

#include <stdint.h>
#include <motion/motion_dev.h>

typedef struct MotionMagnStatistics {
	uint32_t samples;
	uint32_t configurationErrors;
	uint32_t identificationErrors;
	uint32_t readErrors;
	uint32_t timeouts;
	uint32_t diagnosticCount[2];
} MotionMagnStatistics;

typedef struct MotionMagnContext {
	MotionMagnDeviceInfo device;
	MotionMagnStatistics statistics;
	uint8_t measuring;
	uint8_t reserved[3];
} MotionMagnContext;

_Static_assert(sizeof(MotionMagnContext) == 44, "magnetometer state size");

int motionMagnInitialize(void);
int motionMagnGetDeviceInfo(MotionMagnDeviceInfo *info);
int motionMagnStartSampling(void);
int motionMagnCancelMeasurement(void);
int motionMagnStop(void);
int motionMagnReset(void);
int motionMagnStartMeasurement(void);
int motionMagnReadSample(SceMotionDevData *data);
int motionMagnGetStatistics(MotionMagnStatistics *statistics);
