/*
 * The analog inputs: the sticks' four axes (ANI15-ANI12, adcResults[0-3]) and the multi
 * connector's level (ANI11, adcResults[4]), converted by adc.c when the input scan asks
 * (analogStart) and taken by task 3 when the conversions end (analogEvent). Each axis keeps
 * its last three samples and takes their median, which widens the axis's calibration and is
 * scaled into each set the mode computes (analogSticks0, analogSticks1). The multi connector's
 * level is looked up in analogMultiCnLevels, and a new one that two readings agree on wakes
 * the system.
 */
#pragma sfr
#pragma DI
#pragma EI
#pragma NOP

#include <string.h>

#include "ernie.h"
#include "adc.h"
#include "analog.h"
#include "chip.h"
#include "config.h"
#include "input.h"
#include "inputreq.h"
#include "kernel.h"
#include "nvsreq.h"
#include "sleepfactor.h"
#include "task.h"

#define LENGTH(list)	(sizeof(list) / sizeof((list)[0]))


void analogScale00(unsigned int position);
void analogScale01(unsigned int position);
void analogScale10(unsigned int position);
void analogScale11(unsigned int position);
void analogScale20(unsigned int position);
void analogScale21(unsigned int position);
void analogScale30(unsigned int position);
void analogScale31(unsigned int position);
static void analogCalibrationLoaded(unsigned char command, unsigned char result);
static void analogCalibrationSaved(unsigned char command, unsigned char result);

#if BOARD_USS1001 || BOARD_DOL1001
/* a level by a whole result (DOL-1001 keeps analogLevelFindWide, with no table) */
struct AnalogLevelWide {
	unsigned int	limit;
	unsigned int	level;
};
#endif

#if BOARD_USS1001
/* the slim's multi connector: analog input 0x0010's levels by its whole result, 0xff between
   two */
const struct AnalogLevelWide analogInput10Levels[12] = {
	{ 0xe900, 0 }, { 0xde40, 1 }, { 0xce00, 0xff }, { 0xc440, 2 }, { 0xb640, 3 },
	{ 0xa1c0, 0xff }, { 0x9140, 4 }, { 0x8840, 0xff }, { 0x7a80, 5 }, { 0x0580, 0xff },
	{ 0x0140, 6 }, { 0x0000, 7 },
};
#else
#if !BOARD_DOL1001
/* the levels of an older table, never read */
const struct AnalogLevel analogLevels5[5] = {
	{ 0xdf, 0 }, { 0xae, 1 }, { 0x7d, 1 }, { 0x4b, 2 }, { 0x1a, 3 },
};
#endif

const struct AnalogLevel analogMultiCnLevels[7] = {
	{ 0xe3, 6 }, { 0xc4, 5 }, { 0x99, 4 }, { 0x69, 3 }, { 0x40, 2 }, { 0x1d, 1 }, { 0x00, 0 },
};
#endif

static __near struct AnalogAxis analogAxes[ANALOG_AXES];	/* ANI15 first */
static __near unsigned int analogWorkLow;	/* the margin's product, then the value's quotient */
static __near unsigned int analogWorkHigh;	/* and its divisor */
/* a work word's high byte, read from memory */
#define ANALOG_HIGH(word)	(((unsigned char __near *)&(word))[1])
static __near unsigned char analogSample;	/* the samples[] the next conversions go to */
static __near unsigned char analogMinRange;	/* the high byte of a range that calibrates
						   (0x00090009: unused) */
#if FW_SINCE(0x00090206)
static __near unsigned int analogScales[ANALOG_SETS];	/* each set's margin, 1/1000 of the range */
#else
static __near unsigned char analogScale;	/* the margin, range * analogScale >> */
static __near unsigned char analogScaleShift;	/* analogScaleShift: 0x57 >> 9 or (mode 2) 0x31 */
#endif
static __near unsigned char analogMultiCnInput;	/* the high byte of ANI11's result */
#if !BOARD_USS1001 && !BOARD_DOL1001
static __near unsigned char analogMultiCnReserved;	/* cleared by analogMultiCnInit, otherwise
							   unused */
#endif
static __near unsigned char analogMultiCn;		/* the level of the last reading */
static __near unsigned char analogMultiCnPrevious;	/* of the one before */
static __near unsigned char analogMultiCnReported;	/* the level the host reads; 0xff: none */
__near unsigned char analogSticks0[ANALOG_AXES];
#if FW_SINCE(0x00090206)
__near unsigned char analogSticks1[ANALOG_AXES];
#endif
static __near struct AnalogCalibration analogCalibration;
#if FW_SINCE(0x00090206)
__near unsigned char analogFlags;
#endif
#if BOARD_USS1001
/* the slim's multi connector, analog input 0x0010: its result, its level, the one before and
   the one reported (0xfe: none yet) */
static struct {
	unsigned int	input;
	unsigned char	level;		/* of the last reading */
	unsigned char	previous;	/* of the one before */
	unsigned char	reported;
} analogInput10;
#endif

#if ANALOG_LIMITS
#if ANALOG_SNAPSHOT
static void analogFault(unsigned int sample, struct AnalogAxis *axis, struct AnalogFault *fault,
			struct AnalogSnapshot *snapshot);
static void analogSnapshotTake(struct AnalogAxis *axis, struct AnalogSnapshot *snapshot);
#define ANALOG_FAULT(sample, n, fault, snapshot)					\
	analogFault(sample, &analogAxes[n], &analogAxes[n].fault, snapshot)
#else
static void analogFault(unsigned int sample, unsigned int min, unsigned int max,
			struct AnalogFault *fault);
#define ANALOG_FAULT(sample, n, fault, snapshot)					\
	analogFault(sample, analogAxes[n].limitMin, analogAxes[n].limitMax, &analogAxes[n].fault)
#endif
#endif
#if BOARD_USS1001 || BOARD_DOL1001
static unsigned int analogLevelFindWide(unsigned int input, const struct AnalogLevelWide *levels,
					unsigned char count);
#endif

/* the sticks' power: P9.7, DOL-1001's pin that is not there */
#if BOARD_DOL1001
#define ANALOG_POWER		boardDummyPin
#else
#define ANALOG_POWER		P97
#endif

#if !BOARD_USS1001 && !BOARD_DOL1001
/* inputKeys.state's bit 0x0080 (analogStart80, never called): only reset, except where
   BOARD_ANALOG_CHANNEL8 converts it (channel 8's level, analogLevels5) */
static struct {
	unsigned char	input;
	unsigned char	level;		/* of the last reading */
	unsigned char	previous;	/* of the one before */
} analogInput80;

/* a reading against 0x3d, reported when two agree (analogLatchUpdate, never called) */
static struct {
	unsigned char	value;
	unsigned char	state;		/* the value reached 0x3d */
	unsigned char	previous;
	unsigned char	reported;
} analogLatch;

static unsigned char analogSuspend;	/* 0xff (analogInput80Reset): the sticks' samples are not
					   taken */
#endif

#if FW_SINCE(0x00090206)
/* the axes' samples centred and the calibration's limits from the settings */
void analogInit(void)
{
	analogCenter();
	analogAxes[0].samples[0] = ANALOG_SAMPLE_CENTER;
	analogAxes[0].samples[1] = ANALOG_SAMPLE_CENTER;
	analogAxes[0].samples[2] = ANALOG_SAMPLE_CENTER;
	analogAxes[0].median = ANALOG_SAMPLE_CENTER;
	analogAxes[1].samples[0] = ANALOG_SAMPLE_CENTER;
	analogAxes[1].samples[1] = ANALOG_SAMPLE_CENTER;
	analogAxes[1].samples[2] = ANALOG_SAMPLE_CENTER;
	analogAxes[1].median = ANALOG_SAMPLE_CENTER;
	analogAxes[2].samples[0] = ANALOG_SAMPLE_CENTER;
	analogAxes[2].samples[1] = ANALOG_SAMPLE_CENTER;
	analogAxes[2].samples[2] = ANALOG_SAMPLE_CENTER;
	analogAxes[2].median = ANALOG_SAMPLE_CENTER;
	analogAxes[3].samples[0] = ANALOG_SAMPLE_CENTER;
	analogAxes[3].samples[1] = ANALOG_SAMPLE_CENTER;
	analogAxes[3].samples[2] = ANALOG_SAMPLE_CENTER;
	analogAxes[3].median = ANALOG_SAMPLE_CENTER;
	analogSample = 0;
	analogAxes[0].range = 0;
	analogAxes[1].range = 0;
	analogAxes[2].range = 0;
	analogAxes[3].range = 0;
	analogAxes[0].calibrated = 0;
	analogAxes[1].calibrated = 0;
	analogAxes[2].calibrated = 0;
	analogAxes[3].calibrated = 0;
#if ANALOG_LIMITS
	analogAxes[0].limitMin = analogAxes[1].limitMin = analogAxes[2].limitMin =
		analogAxes[3].limitMin = configGet()->analogLimitMin;
	analogAxes[0].limitMax = analogAxes[1].limitMax = analogAxes[2].limitMax =
		analogAxes[3].limitMax = configGet()->analogLimitMax;
#endif
	analogFlags = 0;
	analogMinRange = configGet()->analogMinRange;
#if FW_SINCE(0x01000104)
	if (analogMinRange < 0x32)
		analogMinRange = 0x32;
#endif
	analogScales[0] = configGet()->analogScale0;
#if FW_SINCE(0x01000104)
	if (analogScales[0] > 400)
		analogScales[0] = 400;
#endif
	analogScales[1] = configGet()->analogScale1;
#if FW_SINCE(0x01000104)
	if (analogScales[1] > 400)
		analogScales[1] = 400;
#endif
	analogResetSet0();
	analogResetSet1();
}

void analogCenterSet0(void)
{
	analogSticks0[0] = ANALOG_VALUE_CENTER;
	analogSticks0[1] = ANALOG_VALUE_CENTER;
	analogSticks0[2] = ANALOG_VALUE_CENTER;
	analogSticks0[3] = ANALOG_VALUE_CENTER;
	analogAxes[0].output[0].value = ANALOG_VALUE_CENTER;
	analogAxes[1].output[0].value = ANALOG_VALUE_CENTER;
	analogAxes[2].output[0].value = ANALOG_VALUE_CENTER;
	analogAxes[3].output[0].value = ANALOG_VALUE_CENTER;
}

void analogResetSet0(void)
{
	analogAxes[0].output[0].position = ANALOG_SAMPLE_CENTER;
	analogAxes[0].output[0].last = ANALOG_SAMPLE_CENTER;
	analogAxes[1].output[0].position = ANALOG_SAMPLE_CENTER;
	analogAxes[1].output[0].last = ANALOG_SAMPLE_CENTER;
	analogAxes[2].output[0].position = ANALOG_SAMPLE_CENTER;
	analogAxes[2].output[0].last = ANALOG_SAMPLE_CENTER;
	analogAxes[3].output[0].position = ANALOG_SAMPLE_CENTER;
	analogAxes[3].output[0].last = ANALOG_SAMPLE_CENTER;
	analogAxes[0].output[0].margin = 0;
	analogAxes[0].output[0].high = 0;
	analogAxes[0].output[0].low = 0;
	analogAxes[1].output[0].margin = 0;
	analogAxes[1].output[0].high = 0;
	analogAxes[1].output[0].low = 0;
	analogAxes[2].output[0].margin = 0;
	analogAxes[2].output[0].high = 0;
	analogAxes[2].output[0].low = 0;
	analogAxes[3].output[0].margin = 0;
	analogAxes[3].output[0].high = 0;
	analogAxes[3].output[0].low = 0;
}

void analogCenterSet1(void)
{
	analogSticks1[0] = ANALOG_VALUE_CENTER;
	analogSticks1[1] = ANALOG_VALUE_CENTER;
	analogSticks1[2] = ANALOG_VALUE_CENTER;
	analogSticks1[3] = ANALOG_VALUE_CENTER;
	analogAxes[0].output[1].value = ANALOG_VALUE_CENTER;
	analogAxes[1].output[1].value = ANALOG_VALUE_CENTER;
	analogAxes[2].output[1].value = ANALOG_VALUE_CENTER;
	analogAxes[3].output[1].value = ANALOG_VALUE_CENTER;
}

void analogResetSet1(void)
{
	analogAxes[0].output[1].position = ANALOG_SAMPLE_CENTER;
	analogAxes[0].output[1].last = ANALOG_SAMPLE_CENTER;
	analogAxes[1].output[1].position = ANALOG_SAMPLE_CENTER;
	analogAxes[1].output[1].last = ANALOG_SAMPLE_CENTER;
	analogAxes[2].output[1].position = ANALOG_SAMPLE_CENTER;
	analogAxes[2].output[1].last = ANALOG_SAMPLE_CENTER;
	analogAxes[3].output[1].position = ANALOG_SAMPLE_CENTER;
	analogAxes[3].output[1].last = ANALOG_SAMPLE_CENTER;
	analogAxes[0].output[1].margin = 0;
	analogAxes[0].output[1].high = 0;
	analogAxes[0].output[1].low = 0;
	analogAxes[1].output[1].margin = 0;
	analogAxes[1].output[1].high = 0;
	analogAxes[1].output[1].low = 0;
	analogAxes[2].output[1].margin = 0;
	analogAxes[2].output[1].high = 0;
	analogAxes[2].output[1].low = 0;
	analogAxes[3].output[1].margin = 0;
	analogAxes[3].output[1].high = 0;
	analogAxes[3].output[1].low = 0;
}

#if ANALOG_LIMITS
/* the slim's: the fault records cleared */
void analogAxesReset(void)
{
	analogAxes[0].sampleFault.below = 0;
	analogAxes[0].sampleFault.above = 0;
	analogAxes[1].sampleFault.below = 0;
	analogAxes[1].sampleFault.above = 0;
	analogAxes[2].sampleFault.below = 0;
	analogAxes[2].sampleFault.above = 0;
	analogAxes[3].sampleFault.below = 0;
	analogAxes[3].sampleFault.above = 0;
	analogAxes[0].medianFault.below = 0;
	analogAxes[0].medianFault.above = 0;
	analogAxes[1].medianFault.below = 0;
	analogAxes[1].medianFault.above = 0;
	analogAxes[2].medianFault.below = 0;
	analogAxes[2].medianFault.above = 0;
	analogAxes[3].medianFault.below = 0;
	analogAxes[3].medianFault.above = 0;
	analogAxes[0].sampleFault.noise = ANALOG_SAMPLE_CENTER;
	analogAxes[0].sampleFault.low = ANALOG_SAMPLE_CENTER;
	analogAxes[0].sampleFault.high = ANALOG_SAMPLE_CENTER;
	analogAxes[1].sampleFault.noise = ANALOG_SAMPLE_CENTER;
	analogAxes[1].sampleFault.low = ANALOG_SAMPLE_CENTER;
	analogAxes[1].sampleFault.high = ANALOG_SAMPLE_CENTER;
	analogAxes[2].sampleFault.noise = ANALOG_SAMPLE_CENTER;
	analogAxes[2].sampleFault.low = ANALOG_SAMPLE_CENTER;
	analogAxes[2].sampleFault.high = ANALOG_SAMPLE_CENTER;
	analogAxes[3].sampleFault.noise = ANALOG_SAMPLE_CENTER;
	analogAxes[3].sampleFault.low = ANALOG_SAMPLE_CENTER;
	analogAxes[3].sampleFault.high = ANALOG_SAMPLE_CENTER;
	analogAxes[0].medianFault.noise = ANALOG_SAMPLE_CENTER;
	analogAxes[0].medianFault.low = ANALOG_SAMPLE_CENTER;
	analogAxes[0].medianFault.high = ANALOG_SAMPLE_CENTER;
	analogAxes[1].medianFault.noise = ANALOG_SAMPLE_CENTER;
	analogAxes[1].medianFault.low = ANALOG_SAMPLE_CENTER;
	analogAxes[1].medianFault.high = ANALOG_SAMPLE_CENTER;
	analogAxes[2].medianFault.noise = ANALOG_SAMPLE_CENTER;
	analogAxes[2].medianFault.low = ANALOG_SAMPLE_CENTER;
	analogAxes[2].medianFault.high = ANALOG_SAMPLE_CENTER;
	analogAxes[3].medianFault.noise = ANALOG_SAMPLE_CENTER;
	analogAxes[3].medianFault.low = ANALOG_SAMPLE_CENTER;
	analogAxes[3].medianFault.high = ANALOG_SAMPLE_CENTER;
#if ANALOG_SNAPSHOT
	analogAxes[0].medianSnapshot.samples[0] = ANALOG_SAMPLE_CENTER;
	analogAxes[0].medianSnapshot.samples[1] = ANALOG_SAMPLE_CENTER;
	analogAxes[0].medianSnapshot.samples[2] = ANALOG_SAMPLE_CENTER;
	analogAxes[1].medianSnapshot.samples[0] = ANALOG_SAMPLE_CENTER;
	analogAxes[1].medianSnapshot.samples[1] = ANALOG_SAMPLE_CENTER;
	analogAxes[1].medianSnapshot.samples[2] = ANALOG_SAMPLE_CENTER;
	analogAxes[2].medianSnapshot.samples[0] = ANALOG_SAMPLE_CENTER;
	analogAxes[2].medianSnapshot.samples[1] = ANALOG_SAMPLE_CENTER;
	analogAxes[2].medianSnapshot.samples[2] = ANALOG_SAMPLE_CENTER;
	analogAxes[3].medianSnapshot.samples[0] = ANALOG_SAMPLE_CENTER;
	analogAxes[3].medianSnapshot.samples[1] = ANALOG_SAMPLE_CENTER;
	analogAxes[3].medianSnapshot.samples[2] = ANALOG_SAMPLE_CENTER;
	analogAxes[0].medianSnapshot.check = 0x7fff;
	analogAxes[1].medianSnapshot.check = 0x7fff;
	analogAxes[2].medianSnapshot.check = 0x7fff;
	analogAxes[3].medianSnapshot.check = 0x7fff;
#endif
}
#endif

void analogCenter(void)
{
	analogCenterSet0();
	analogCenterSet1();
}

/* every axis's calibration back to the settings' limits, not yet calibrated */
void analogCalibrationReset(void)
{
	unsigned int limit;

	limit = configGet()->analogMin;
	analogCalibration.axis[0].min = limit;
	analogCalibration.axis[1].min = limit;
	analogCalibration.axis[2].min = limit;
	analogCalibration.axis[3].min = limit;
	limit = configGet()->analogMax;
	analogCalibration.axis[0].max = limit;
	analogCalibration.axis[1].max = limit;
	analogCalibration.axis[2].max = limit;
	analogCalibration.axis[3].max = limit;
	analogAxes[0].calibrated = 0;
	analogAxes[1].calibrated = 0;
	analogAxes[2].calibrated = 0;
	analogAxes[3].calibrated = 0;
}

#if ANALOG_LIMITS
/* the slim's: each axis's limits from the settings' calibration, widened by its range *
   analogLimitScale / 100 + analogLimitMargin within analogLimitMin and analogLimitMax; a
   calibration's range below 0x7ac0 takes those two */
void analogLimitsSet(void)
{
	unsigned int range;
	union {
		unsigned long	value;
		unsigned int	low;	/* its low word, all that is used */
	} margin;

	range = configGet()->analogCalibration.axis[0].max -
		configGet()->analogCalibration.axis[0].min;
	if (range < 0x7ac0) {
		analogAxes[0].limitMin = configGet()->analogLimitMin;
		analogAxes[0].limitMax = configGet()->analogLimitMax;
	} else {
		margin.value = (unsigned long)range * configGet()->analogLimitScale / 100 +
			 configGet()->analogLimitMargin;
		if (margin.value > 0xffc0)
			margin.low = 0xffc0;
		if (configGet()->analogCalibration.axis[0].min >
		    configGet()->analogLimitMin + margin.low)
			analogAxes[0].limitMin = configGet()->analogCalibration.axis[0].min -
						 margin.low;
		else
			analogAxes[0].limitMin = configGet()->analogLimitMin;
		if (configGet()->analogCalibration.axis[0].max <
		    configGet()->analogLimitMax - margin.low)
			analogAxes[0].limitMax = configGet()->analogCalibration.axis[0].max +
						 margin.low;
		else
			analogAxes[0].limitMax = configGet()->analogLimitMax;
	}

	range = configGet()->analogCalibration.axis[1].max -
		configGet()->analogCalibration.axis[1].min;
	if (range < 0x7ac0) {
		analogAxes[1].limitMin = configGet()->analogLimitMin;
		analogAxes[1].limitMax = configGet()->analogLimitMax;
	} else {
		margin.value = (unsigned long)range * configGet()->analogLimitScale / 100 +
			 configGet()->analogLimitMargin;
		if (margin.value > 0xffc0)
			margin.low = 0xffc0;
		if (configGet()->analogCalibration.axis[1].min >
		    configGet()->analogLimitMin + margin.low)
			analogAxes[1].limitMin = configGet()->analogCalibration.axis[1].min -
						 margin.low;
		else
			analogAxes[1].limitMin = configGet()->analogLimitMin;
		if (configGet()->analogCalibration.axis[1].max <
		    configGet()->analogLimitMax - margin.low)
			analogAxes[1].limitMax = configGet()->analogCalibration.axis[1].max +
						 margin.low;
		else
			analogAxes[1].limitMax = configGet()->analogLimitMax;
	}

	range = configGet()->analogCalibration.axis[2].max -
		configGet()->analogCalibration.axis[2].min;
	if (range < 0x7ac0) {
		analogAxes[2].limitMin = configGet()->analogLimitMin;
		analogAxes[2].limitMax = configGet()->analogLimitMax;
	} else {
		margin.value = (unsigned long)range * configGet()->analogLimitScale / 100 +
			 configGet()->analogLimitMargin;
		if (margin.value > 0xffc0)
			margin.low = 0xffc0;
		if (configGet()->analogCalibration.axis[2].min >
		    configGet()->analogLimitMin + margin.low)
			analogAxes[2].limitMin = configGet()->analogCalibration.axis[2].min -
						 margin.low;
		else
			analogAxes[2].limitMin = configGet()->analogLimitMin;
		if (configGet()->analogCalibration.axis[2].max <
		    configGet()->analogLimitMax - margin.low)
			analogAxes[2].limitMax = configGet()->analogCalibration.axis[2].max +
						 margin.low;
		else
			analogAxes[2].limitMax = configGet()->analogLimitMax;
	}

	range = configGet()->analogCalibration.axis[3].max -
		configGet()->analogCalibration.axis[3].min;
	if (range < 0x7ac0) {
		analogAxes[3].limitMin = configGet()->analogLimitMin;
		analogAxes[3].limitMax = configGet()->analogLimitMax;
	} else {
		margin.value = (unsigned long)range * configGet()->analogLimitScale / 100 +
			 configGet()->analogLimitMargin;
		if (margin.value > 0xffc0)
			margin.low = 0xffc0;
		if (configGet()->analogCalibration.axis[3].min >
		    configGet()->analogLimitMin + margin.low)
			analogAxes[3].limitMin = configGet()->analogCalibration.axis[3].min -
						 margin.low;
		else
			analogAxes[3].limitMin = configGet()->analogLimitMin;
		if (configGet()->analogCalibration.axis[3].max <
		    configGet()->analogLimitMax - margin.low)
			analogAxes[3].limitMax = configGet()->analogCalibration.axis[3].max +
						 margin.low;
		else
			analogAxes[3].limitMax = configGet()->analogLimitMax;
	}
}

void analogLimitsGet(unsigned int *out)
{
	out[0] = analogAxes[0].limitMin;
	out[1] = analogAxes[0].limitMax;
	out[2] = analogAxes[1].limitMin;
	out[3] = analogAxes[1].limitMax;
	out[4] = analogAxes[2].limitMin;
	out[5] = analogAxes[2].limitMax;
	out[6] = analogAxes[3].limitMin;
	out[7] = analogAxes[3].limitMax;
}
#endif

#else
/* 0x00090009: the axes' samples and positions centred, the margin of mode 1 */
void analogInit(void)
{
	analogCenterSet0();
	analogAxes[0].samples[0] = ANALOG_SAMPLE_CENTER;
	analogAxes[0].samples[1] = ANALOG_SAMPLE_CENTER;
	analogAxes[0].samples[2] = ANALOG_SAMPLE_CENTER;
	analogAxes[0].median = ANALOG_SAMPLE_CENTER;
	analogAxes[0].position = ANALOG_SAMPLE_CENTER;
	analogAxes[0].last = ANALOG_SAMPLE_CENTER;
	analogAxes[1].samples[0] = ANALOG_SAMPLE_CENTER;
	analogAxes[1].samples[1] = ANALOG_SAMPLE_CENTER;
	analogAxes[1].samples[2] = ANALOG_SAMPLE_CENTER;
	analogAxes[1].median = ANALOG_SAMPLE_CENTER;
	analogAxes[1].position = ANALOG_SAMPLE_CENTER;
	analogAxes[1].last = ANALOG_SAMPLE_CENTER;
	analogAxes[2].samples[0] = ANALOG_SAMPLE_CENTER;
	analogAxes[2].samples[1] = ANALOG_SAMPLE_CENTER;
	analogAxes[2].samples[2] = ANALOG_SAMPLE_CENTER;
	analogAxes[2].median = ANALOG_SAMPLE_CENTER;
	analogAxes[2].position = ANALOG_SAMPLE_CENTER;
	analogAxes[2].last = ANALOG_SAMPLE_CENTER;
	analogAxes[3].samples[0] = ANALOG_SAMPLE_CENTER;
	analogAxes[3].samples[1] = ANALOG_SAMPLE_CENTER;
	analogAxes[3].samples[2] = ANALOG_SAMPLE_CENTER;
	analogAxes[3].median = ANALOG_SAMPLE_CENTER;
	analogAxes[3].position = ANALOG_SAMPLE_CENTER;
	analogAxes[3].last = ANALOG_SAMPLE_CENTER;
	analogSample = 0;
	analogAxes[0].range = 0;
	analogAxes[0].margin = 0;
	analogAxes[0].high = 0;
	analogAxes[0].low = 0;
	analogAxes[1].range = 0;
	analogAxes[1].margin = 0;
	analogAxes[1].high = 0;
	analogAxes[1].low = 0;
	analogAxes[2].range = 0;
	analogAxes[2].margin = 0;
	analogAxes[2].high = 0;
	analogAxes[2].low = 0;
	analogAxes[3].range = 0;
	analogAxes[3].margin = 0;
	analogAxes[3].high = 0;
	analogAxes[3].low = 0;
	analogAxes[0].calibrated = 0;
	analogAxes[1].calibrated = 0;
	analogAxes[2].calibrated = 0;
	analogAxes[3].calibrated = 0;
	analogScale = 0x57;
	analogScaleShift = 9;
}

void analogCenterSet0(void)
{
	analogSticks0[0] = ANALOG_VALUE_CENTER;
	analogSticks0[1] = ANALOG_VALUE_CENTER;
	analogSticks0[2] = ANALOG_VALUE_CENTER;
	analogSticks0[3] = ANALOG_VALUE_CENTER;
	analogAxes[0].value = ANALOG_VALUE_CENTER;
	analogAxes[1].value = ANALOG_VALUE_CENTER;
	analogAxes[2].value = ANALOG_VALUE_CENTER;
	analogAxes[3].value = ANALOG_VALUE_CENTER;
}

/* every axis's calibration back to the centre, not yet calibrated */
void analogCalibrationReset(void)
{
	analogCalibration.axis[0].min = ANALOG_SAMPLE_CENTER;
	analogCalibration.axis[0].max = ANALOG_SAMPLE_CENTER;
	analogCalibration.axis[1].min = ANALOG_SAMPLE_CENTER;
	analogCalibration.axis[1].max = ANALOG_SAMPLE_CENTER;
	analogCalibration.axis[2].min = ANALOG_SAMPLE_CENTER;
	analogCalibration.axis[2].max = ANALOG_SAMPLE_CENTER;
	analogCalibration.axis[3].min = ANALOG_SAMPLE_CENTER;
	analogCalibration.axis[3].max = ANALOG_SAMPLE_CENTER;
	analogAxes[0].calibrated = 0;
	analogAxes[1].calibrated = 0;
	analogAxes[2].calibrated = 0;
	analogAxes[3].calibrated = 0;
}
#endif

/* the settings' calibration */
void analogCalibrationDefault(void)
{
	analogCalibration = configGet()->analogCalibration;
}

/* reads the calibration kept in the NVS's syscon area into data; done(command, result) when
   it is read */
int analogStoreRead(unsigned char *data, unsigned char command,
		    void (*done)(unsigned char command, unsigned char result))
{
	return nvsJobRead(0x30, sizeof(struct AnalogCalibration), data, NVS_AREA_SYSCON, command,
			  done);
}

/* writes data to the NVS's calibration */
int analogStoreWrite(unsigned char *data, unsigned char command,
		     void (*done)(unsigned char command, unsigned char result))
{
	return nvsJobWrite(0x30, sizeof(struct AnalogCalibration), data, NVS_AREA_SYSCON, command,
			   done);
}

/* the stored calibration, checked by analogCalibrationLoaded */
void analogCalibrationLoad(void)
{
	analogStoreRead((unsigned char *)&analogCalibration, 0, analogCalibrationLoaded);
}

/* the settings' calibration, unless every axis's stored one reaches the settings' limits */
static void analogCalibrationLoaded(unsigned char command, unsigned char result)
{
	unsigned char i;
	unsigned int range;

	if (result == 0) {
		for (i = 0; i < ANALOG_AXES; i++) {
#if FW_SINCE(0x00090206)
			range = analogCalibration.axis[i].max - analogCalibration.axis[i].min;
			if (analogCalibration.axis[i].min > configGet()->analogMin ||
			    analogCalibration.axis[i].max < configGet()->analogMax ||
#if BOARD_USS1001
			    ANALOG_HIGH(range) < configGet()->analogMinRange) {
#else
			    configGet()->analogMinRange > ANALOG_HIGH(range)) {
#endif
#else
			range = analogCalibration.axis[i].max - analogCalibration.axis[i].min;
			if (analogCalibration.axis[i].min > ANALOG_SAMPLE_CENTER ||
			    analogCalibration.axis[i].max < ANALOG_SAMPLE_CENTER ||
			    ANALOG_HIGH(range) < 0x64) {
#endif
				analogCalibrationDefault();
				return;
			}
		}
	} else
		analogCalibrationDefault();
}

/* stores the calibration and waits until it is written */
void analogCalibrationSave(void)
{
	ER ercd;
	FLGPTN ptn;

#if FW_SINCE(0x0100060B)
	clr_flg(2, ~0x0020);
#endif
	analogStoreWrite((unsigned char *)&analogCalibration, 0, analogCalibrationSaved);
	ercd = wai_flg(2, 0x0020, TWF_ORW, &ptn);
	if (ercd == E_OK && (ptn & 0x0020))
		clr_flg(2, ~0x0020);
}

static void analogCalibrationSaved(unsigned char command, unsigned char result)
{
	task3SetFlag(2, 0x0020);
}

void analogCalibrationGet(unsigned char *out)
{
	memcpy(out, &analogCalibration, sizeof(analogCalibration));
}

#if !BOARD_USS1001 && !BOARD_DOL1001
void analogInput80Reset(void)
{
	analogSuspend = 0xff;
	analogInput80.level = 0xff;
	analogInput80.previous = 0xff;
	analogInput80.input = 0;
}
#endif

void analogMultiCnInit(void)
{
	sleepFactorSet6(0);
	analogMultiCnReported = 0xff;
	analogMultiCn = 0xff;
	analogMultiCnPrevious = 0xff;
	analogMultiCnInput = 0;
#if !BOARD_USS1001 && !BOARD_DOL1001
	analogMultiCnReserved = 0;
#endif
}

#if BOARD_USS1001
void analogInput10Init(void)
{
	sleepFactorSet14(0);
	analogInput10.reported = 0xfe;
	analogInput10.level = 0xfe;
	analogInput10.previous = 0xfe;
	analogInput10.input = 0;
}
#endif

#if BOARD_USS1001 || BOARD_DOL1001
/* converts the inputs (inputKeys' bits) asked and on: the sticks (0x0002), the multi
   connector (0x0010; DOL-1001's 0x0020); 1 if the converter is busy */
unsigned char analogStart(unsigned int inputs)
{
	unsigned char mask;
	unsigned char skip;
	unsigned char busy;

	busy = 0;
	mask = 0;
	skip = 1;
	if ((inputs & 0x0002) && (inputKeys.state & 0x0002)) {
#if BOARD_DOL1001
		mask |= 0xf0;
		ANALOG_POWER = 1;
#else
		mask |= 0x0f;
#if !FW_SINCE(0x01030511)
		P10.2 = 0;	/* 0x01030505: low here, high again in analogEvent */
#endif
#endif
		skip = 4;
	}
#if BOARD_DOL1001
	if ((inputs & 0x0020) && (inputKeys.state & 0x0020))
		mask |= 0x08;
#else
	if ((inputs & 0x0010) && (inputKeys.state & 0x0010))
		mask |= 0x10;
#endif
	busy = adcStart(mask, skip);
	if (busy)
		return 1;
	return 0;
}
#else
/* converts the inputs (inputKeys' bits) asked: the sticks (0x0002, powered by P9.7), the multi
   connector (0x0020); 1 if the converter is busy */
unsigned char analogStart(unsigned int inputs)
{
	unsigned char mask;
	unsigned char skip;
#if FW_SINCE(0x0100060B)
	unsigned char busy;

	busy = 0;
#endif

	mask = 0;
	skip = 1;
	if (inputs & 0x0002) {
		mask |= 0xf0;
		ANALOG_POWER = 1;
		skip = 2;
	}
#if BOARD_102603
	if (inputs & 0x0020)
		mask |= 0x0c;
	if (inputs & 0x0001) {
#if HW_IS(0x00315200)
		P127 = 0;
#else
		P12 &= (unsigned char)~0x80;
#endif
		mask |= 0x02;
	}
#else
	if (inputs & 0x0020)
		mask |= 0x08;
	if (inputs & 0x0001)
		;	/* Sony's tests bit 0 and does nothing */
#endif
#if BOARD_ANALOG_CHANNEL8
	if (inputs & 0x0080)
		mask |= 0x01;
#endif
#if FW_SINCE(0x0100060B)
	busy = adcStart(mask, skip);
	if (busy)
		return 1;
#else
	if (adcStart(mask, skip))
		return 1;
#endif
	return 0;
}
#endif

#if BOARD_USS1001
/* the conversions of the inputs asked ended: the sticks' samples taken (those outside their
   limits recorded) and scaled, the multi connector's level reported when two readings agree on
   a new one */
void analogEvent(unsigned int inputs)
{
	if ((inputs & 0x0002) && (inputKeys.state & 0x0002)) {
#if ANALOG_LIMITS
		analogAxes[0].samples[analogSample] = adcResults[0];
		if ((analogAxes[0].samples[analogSample] & 0x003f) || analogAxes[0].limitMin >= analogAxes[0].samples[analogSample] ||
		    analogAxes[0].samples[analogSample] >= analogAxes[0].limitMax)
			ANALOG_FAULT(analogAxes[0].samples[analogSample], 0, sampleFault, NULL);
		analogAxes[1].samples[analogSample] = adcResults[1];
		if ((analogAxes[1].samples[analogSample] & 0x003f) || analogAxes[1].limitMin >= analogAxes[1].samples[analogSample] ||
		    analogAxes[1].samples[analogSample] >= analogAxes[1].limitMax)
			ANALOG_FAULT(analogAxes[1].samples[analogSample], 1, sampleFault, NULL);
		analogAxes[2].samples[analogSample] = adcResults[2];
		if ((analogAxes[2].samples[analogSample] & 0x003f) || analogAxes[2].limitMin >= analogAxes[2].samples[analogSample] ||
		    analogAxes[2].samples[analogSample] >= analogAxes[2].limitMax)
			ANALOG_FAULT(analogAxes[2].samples[analogSample], 2, sampleFault, NULL);
		analogAxes[3].samples[analogSample] = adcResults[3];
		if ((analogAxes[3].samples[analogSample] & 0x003f) || analogAxes[3].limitMin >= analogAxes[3].samples[analogSample] ||
		    analogAxes[3].samples[analogSample] >= analogAxes[3].limitMax)
			ANALOG_FAULT(analogAxes[3].samples[analogSample], 3, sampleFault, NULL);
#else
		P10.2 = 1;	/* 0x01030505 */
		analogAxes[0].samples[analogSample] = adcResults[0];
		analogAxes[1].samples[analogSample] = adcResults[1];
		analogAxes[2].samples[analogSample] = adcResults[2];
		analogAxes[3].samples[analogSample] = adcResults[3];
#endif
		if (++analogSample >= ANALOG_SAMPLES)
			analogSample = 0;
		analogFilter0();
		analogFilter1();
		analogFilter2();
		analogFilter3();
		analogReport();
	}
	if ((inputs & 0x0010) && (inputKeys.state & 0x0010)) {
		analogInput10.input = adcResults[4];
		analogInput10.level = analogLevelFindWide(analogInput10.input, analogInput10Levels,
							 LENGTH(analogInput10Levels));
		if (analogInput10.level == analogInput10.previous) {
			if (analogInput10.level != analogInput10.reported) {
				analogInput10.reported = analogInput10.level;
				analogStop10();
				if (analogInput10.reported == 0)
					sleepFactorSet14(0);
				else
					sleepFactorSet14(1);
				task2SetFlag(3, 0x0001);
			} else
				analogStop10();
		} else
			inputScanHold(5);
		analogInput10.previous = analogInput10.level;
	}
}
#else
/* the conversions of the inputs asked ended: the sticks' samples taken and scaled, the multi
   connector's level reported when two readings agree on a new one */
void analogEvent(unsigned int inputs)
{
#if BOARD_102603
	unsigned char high;
	unsigned char low;

#endif
#if BOARD_DOL1001
	if ((inputs & 0x0002) && (inputKeys.state & 0x0002)) {
#elif FW_SINCE(0x0100010B)
	if ((inputs & 0x0002) && analogSuspend != 0xff && (inputKeys.state & 0x0002)) {
#else
	if ((inputs & 0x0002) && analogSuspend != 0xff) {
#endif
		ANALOG_POWER = 0;
		analogAxes[0].samples[analogSample] = adcResults[0];
		analogAxes[1].samples[analogSample] = adcResults[1];
		analogAxes[2].samples[analogSample] = adcResults[2];
		analogAxes[3].samples[analogSample] = adcResults[3];
		if (++analogSample >= ANALOG_SAMPLES)
			analogSample = 0;
		analogFilter0();
		analogFilter1();
		analogFilter2();
		analogFilter3();
#if FW_SINCE(0x00090206)
		analogReport();
#else
		analogSticks0[0] = 0xff - analogAxes[0].value;
		analogSticks0[1] = 0xff - analogAxes[1].value;
		analogSticks0[2] = analogAxes[2].value;
		analogSticks0[3] = analogAxes[3].value;
#endif
	}
#if FW_SINCE(0x0100010B)
	if ((inputs & 0x0020) && (inputKeys.state & 0x0020)) {
#else
	if (inputs & 0x0020) {
#endif
		analogMultiCnInput = ANALOG_HIGH(adcResults[4]);
#if BOARD_102603
		/* two connector lines: channel 4's level in the high nibble, channel 5's in the low */
		analogMultiCnReserved = ANALOG_HIGH(adcResults[5]);
		high = analogLevelFind(analogMultiCnInput, analogMultiCnLevels,
				       LENGTH(analogMultiCnLevels));
		low = analogLevelFind(analogMultiCnReserved, analogMultiCnLevels,
				      LENGTH(analogMultiCnLevels));
		analogMultiCn = (high << 4) | (low & 0x0f);
#else
		analogMultiCn = analogLevelFind(analogMultiCnInput, analogMultiCnLevels,
						LENGTH(analogMultiCnLevels));
#endif
		if (analogMultiCn == analogMultiCnPrevious && analogMultiCn != analogMultiCnReported) {
			analogMultiCnReported = analogMultiCn;
			analogMultiCnStop();
#if !FW_SINCE(0x0100010B) && FW_SINCE(0x00090009)	/* 0x00080504 keeps the ADC open */
			if (stateGet() != 0x0020)
				adcClose();
#endif
			sleepFactorSet6(1);
			sleepFactorSet5(1);
#if FW_SINCE(0x00090804)
			task2SetFlag(3, 0x0001);
#else
			switch (analogMultiCnReported) {
#if BOARD_102603
			case 0x51:	/* its levels are two inputs' nibbles */
#else
			case 2:
#endif
				task2SetFlag(3, 0x0001);
			}
#endif
		}
		analogMultiCnPrevious = analogMultiCn;
	}
#if BOARD_102603
	if (inputs & 0x0001) {
#if HW_IS(0x00315200)
		P127 = 1;
#else
		P12 |= 0x80;
#endif
		analogLatchUpdate(ANALOG_HIGH(adcResults[6]));
	}
#elif !BOARD_DOL1001
	if (inputs & 0x0001)
		;	/* Sony's tests bit 0 and does nothing */
#endif
#if BOARD_ANALOG_CHANNEL8
	/* a level two readings agree on stops the sticks' samples, or (0xff) lets them go on */
	if (inputs & 0x0080) {
		analogInput80.input = ANALOG_HIGH(adcResults[7]);
		analogInput80.level = analogLevelFind(analogInput80.input, analogLevels5,
						      LENGTH(analogLevels5));
		if (analogInput80.level == analogInput80.previous &&
		    analogInput80.level != analogSuspend) {
			analogSuspend = analogInput80.level;
			analogStop80();
		}
		analogInput80.previous = analogInput80.level;
	}
#endif
}
#endif

#if !BOARD_USS1001 && !BOARD_DOL1001
void analogLatchUpdate(unsigned char value)
{
	analogLatch.value = value;
	if (analogLatch.value >= 0x3d)
		analogLatch.state = 1;
	else
		analogLatch.state = 0;
	if (analogLatch.state == analogLatch.previous) {
		analogLatch.reported = analogLatch.state;
		analogLatchOff();
	}
	analogLatch.previous = analogLatch.state;
}
#endif

#if FW_SINCE(0x00090206)
#if SOURCE_SLIM
/* axis 0: the median of its samples; one within the limits widens the calibration and is
   scaled into its sets, one outside them is recorded */
void analogFilter0(void)
{
	unsigned int median;

	if (analogAxes[0].samples[0] < analogAxes[0].samples[1]) {
		if (analogAxes[0].samples[1] < analogAxes[0].samples[2])
			analogAxes[0].median = analogAxes[0].samples[1];
		else if (analogAxes[0].samples[0] < analogAxes[0].samples[2])
			analogAxes[0].median = analogAxes[0].samples[2];
		else
			analogAxes[0].median = analogAxes[0].samples[0];
	} else {
		if (analogAxes[0].samples[2] < analogAxes[0].samples[1])
			analogAxes[0].median = analogAxes[0].samples[1];
		else if (analogAxes[0].samples[2] < analogAxes[0].samples[0])
			analogAxes[0].median = analogAxes[0].samples[2];
		else
			analogAxes[0].median = analogAxes[0].samples[0];
	}
#if ANALOG_LIMITS
	if (!(analogAxes[0].median & 0x003f) && analogAxes[0].limitMin < analogAxes[0].median &&
	    analogAxes[0].median < analogAxes[0].limitMax) {
#else
	if (analogAxes[0].median >= 0 && analogAxes[0].median <= ADC_RESULT_MASK) {
#endif
		median = analogAxes[0].median;
		if (ANALOG_HIGH(analogCalibration.axis[0].max) < ANALOG_HIGH(median))
			analogCalibration.axis[0].max = median;
		if (ANALOG_HIGH(median) < ANALOG_HIGH(analogCalibration.axis[0].min))
			analogCalibration.axis[0].min = median;
		if (analogCalibration.axis[0].min < analogCalibration.axis[0].max) {
			analogAxes[0].range = analogCalibration.axis[0].max - analogCalibration.axis[0].min;
			if (ANALOG_HIGH(analogAxes[0].range) >= analogMinRange)
				analogAxes[0].calibrated = 1;
		}
		if (analogFlags & 0x01)
			analogScale00(median);
		if (analogFlags & 0x02)
			analogScale01(median);
#if ANALOG_LIMITS
	} else
		ANALOG_FAULT(analogAxes[0].median, 0, medianFault, &analogAxes[0].medianSnapshot);
#else
	}
#endif
}

/* axis 0 in set 0: the position held between the calibration's ends less the set's margin,
   and its value 0-0xff between them */
void analogScale00(unsigned int position)
{
	unsigned long margin;

	margin = (unsigned long)analogAxes[0].range * analogScales[0] / 1000;
	analogAxes[0].output[0].margin = margin;
	analogAxes[0].output[0].margin &= ADC_RESULT_MASK;
	analogAxes[0].output[0].high = analogCalibration.axis[0].max - analogAxes[0].output[0].margin;
	if (analogAxes[0].output[0].high < position)
		position = analogAxes[0].output[0].high;
	analogAxes[0].output[0].low = analogCalibration.axis[0].min + analogAxes[0].output[0].margin;
	if (position < analogAxes[0].output[0].low)
		position = analogAxes[0].output[0].low;
	analogAxes[0].output[0].position = position;
	if (analogAxes[0].output[0].position != analogAxes[0].output[0].last && analogAxes[0].calibrated == 1) {
		analogAxes[0].output[0].last = analogAxes[0].output[0].position;
		analogWorkLow = analogAxes[0].output[0].last - analogAxes[0].output[0].low;
		analogWorkLow -= ANALOG_HIGH(analogWorkLow);
		analogWorkHigh = analogAxes[0].output[0].high - analogAxes[0].output[0].low;
		analogWorkLow = analogWorkLow / ANALOG_HIGH(analogWorkHigh);
		if (ANALOG_HIGH(analogWorkLow))
			analogAxes[0].output[0].value = 0xff;
		else
			analogAxes[0].output[0].value = analogWorkLow;
	}
}

/* axis 0 in set 1: the position held between the calibration's ends less the set's margin,
   and its value 0-0xff between them */
void analogScale01(unsigned int position)
{
	unsigned long margin;

	margin = (unsigned long)analogAxes[0].range * analogScales[1] / 1000;
	analogAxes[0].output[1].margin = margin;
	analogAxes[0].output[1].margin &= ADC_RESULT_MASK;
	analogAxes[0].output[1].high = analogCalibration.axis[0].max - analogAxes[0].output[1].margin;
	if (analogAxes[0].output[1].high < position)
		position = analogAxes[0].output[1].high;
	analogAxes[0].output[1].low = analogCalibration.axis[0].min + analogAxes[0].output[1].margin;
	if (position < analogAxes[0].output[1].low)
		position = analogAxes[0].output[1].low;
	analogAxes[0].output[1].position = position;
	if (analogAxes[0].output[1].position != analogAxes[0].output[1].last && analogAxes[0].calibrated == 1) {
		analogAxes[0].output[1].last = analogAxes[0].output[1].position;
		analogWorkLow = analogAxes[0].output[1].last - analogAxes[0].output[1].low;
		analogWorkLow -= ANALOG_HIGH(analogWorkLow);
		analogWorkHigh = analogAxes[0].output[1].high - analogAxes[0].output[1].low;
		analogWorkLow = analogWorkLow / ANALOG_HIGH(analogWorkHigh);
		if (ANALOG_HIGH(analogWorkLow))
			analogAxes[0].output[1].value = 0xff;
		else
			analogAxes[0].output[1].value = analogWorkLow;
	}
}

/* axis 1: the median of its samples; one within the limits widens the calibration and is
   scaled into its sets, one outside them is recorded */
void analogFilter1(void)
{
	unsigned int median;

	if (analogAxes[1].samples[0] < analogAxes[1].samples[1]) {
		if (analogAxes[1].samples[1] < analogAxes[1].samples[2])
			analogAxes[1].median = analogAxes[1].samples[1];
		else if (analogAxes[1].samples[0] < analogAxes[1].samples[2])
			analogAxes[1].median = analogAxes[1].samples[2];
		else
			analogAxes[1].median = analogAxes[1].samples[0];
	} else {
		if (analogAxes[1].samples[2] < analogAxes[1].samples[1])
			analogAxes[1].median = analogAxes[1].samples[1];
		else if (analogAxes[1].samples[2] < analogAxes[1].samples[0])
			analogAxes[1].median = analogAxes[1].samples[2];
		else
			analogAxes[1].median = analogAxes[1].samples[0];
	}
#if ANALOG_LIMITS
	if (!(analogAxes[1].median & 0x003f) && analogAxes[1].limitMin < analogAxes[1].median &&
	    analogAxes[1].median < analogAxes[1].limitMax) {
#else
	if (analogAxes[1].median >= 0 && analogAxes[1].median <= ADC_RESULT_MASK) {
#endif
		median = analogAxes[1].median;
		if (ANALOG_HIGH(analogCalibration.axis[1].max) < ANALOG_HIGH(median))
			analogCalibration.axis[1].max = median;
		if (ANALOG_HIGH(median) < ANALOG_HIGH(analogCalibration.axis[1].min))
			analogCalibration.axis[1].min = median;
		if (analogCalibration.axis[1].min < analogCalibration.axis[1].max) {
			analogAxes[1].range = analogCalibration.axis[1].max - analogCalibration.axis[1].min;
			if (ANALOG_HIGH(analogAxes[1].range) >= analogMinRange)
				analogAxes[1].calibrated = 1;
		}
		if (analogFlags & 0x01)
			analogScale10(median);
		if (analogFlags & 0x02)
			analogScale11(median);
#if ANALOG_LIMITS
	} else
		ANALOG_FAULT(analogAxes[1].median, 1, medianFault, &analogAxes[1].medianSnapshot);
#else
	}
#endif
}

/* axis 1 in set 0: the position held between the calibration's ends less the set's margin,
   and its value 0-0xff between them */
void analogScale10(unsigned int position)
{
	unsigned long margin;

	margin = (unsigned long)analogAxes[1].range * analogScales[0] / 1000;
	analogAxes[1].output[0].margin = margin;
	analogAxes[1].output[0].margin &= ADC_RESULT_MASK;
	analogAxes[1].output[0].high = analogCalibration.axis[1].max - analogAxes[1].output[0].margin;
	if (analogAxes[1].output[0].high < position)
		position = analogAxes[1].output[0].high;
	analogAxes[1].output[0].low = analogCalibration.axis[1].min + analogAxes[1].output[0].margin;
	if (position < analogAxes[1].output[0].low)
		position = analogAxes[1].output[0].low;
	analogAxes[1].output[0].position = position;
	if (analogAxes[1].output[0].position != analogAxes[1].output[0].last && analogAxes[1].calibrated == 1) {
		analogAxes[1].output[0].last = analogAxes[1].output[0].position;
		analogWorkLow = analogAxes[1].output[0].last - analogAxes[1].output[0].low;
		analogWorkLow -= ANALOG_HIGH(analogWorkLow);
		analogWorkHigh = analogAxes[1].output[0].high - analogAxes[1].output[0].low;
		analogWorkLow = analogWorkLow / ANALOG_HIGH(analogWorkHigh);
		if (ANALOG_HIGH(analogWorkLow))
			analogAxes[1].output[0].value = 0xff;
		else
			analogAxes[1].output[0].value = analogWorkLow;
	}
}

/* axis 1 in set 1: the position held between the calibration's ends less the set's margin,
   and its value 0-0xff between them */
void analogScale11(unsigned int position)
{
	unsigned long margin;

	margin = (unsigned long)analogAxes[1].range * analogScales[1] / 1000;
	analogAxes[1].output[1].margin = margin;
	analogAxes[1].output[1].margin &= ADC_RESULT_MASK;
	analogAxes[1].output[1].high = analogCalibration.axis[1].max - analogAxes[1].output[1].margin;
	if (analogAxes[1].output[1].high < position)
		position = analogAxes[1].output[1].high;
	analogAxes[1].output[1].low = analogCalibration.axis[1].min + analogAxes[1].output[1].margin;
	if (position < analogAxes[1].output[1].low)
		position = analogAxes[1].output[1].low;
	analogAxes[1].output[1].position = position;
	if (analogAxes[1].output[1].position != analogAxes[1].output[1].last && analogAxes[1].calibrated == 1) {
		analogAxes[1].output[1].last = analogAxes[1].output[1].position;
		analogWorkLow = analogAxes[1].output[1].last - analogAxes[1].output[1].low;
		analogWorkLow -= ANALOG_HIGH(analogWorkLow);
		analogWorkHigh = analogAxes[1].output[1].high - analogAxes[1].output[1].low;
		analogWorkLow = analogWorkLow / ANALOG_HIGH(analogWorkHigh);
		if (ANALOG_HIGH(analogWorkLow))
			analogAxes[1].output[1].value = 0xff;
		else
			analogAxes[1].output[1].value = analogWorkLow;
	}
}

/* axis 2: the median of its samples; one within the limits widens the calibration and is
   scaled into its sets, one outside them is recorded */
void analogFilter2(void)
{
	unsigned int median;

	if (analogAxes[2].samples[0] < analogAxes[2].samples[1]) {
		if (analogAxes[2].samples[1] < analogAxes[2].samples[2])
			analogAxes[2].median = analogAxes[2].samples[1];
		else if (analogAxes[2].samples[0] < analogAxes[2].samples[2])
			analogAxes[2].median = analogAxes[2].samples[2];
		else
			analogAxes[2].median = analogAxes[2].samples[0];
	} else {
		if (analogAxes[2].samples[2] < analogAxes[2].samples[1])
			analogAxes[2].median = analogAxes[2].samples[1];
		else if (analogAxes[2].samples[2] < analogAxes[2].samples[0])
			analogAxes[2].median = analogAxes[2].samples[2];
		else
			analogAxes[2].median = analogAxes[2].samples[0];
	}
#if ANALOG_LIMITS
	if (!(analogAxes[2].median & 0x003f) && analogAxes[2].limitMin < analogAxes[2].median &&
	    analogAxes[2].median < analogAxes[2].limitMax) {
#else
	if (analogAxes[2].median >= 0 && analogAxes[2].median <= ADC_RESULT_MASK) {
#endif
		median = analogAxes[2].median;
		if (ANALOG_HIGH(analogCalibration.axis[2].max) < ANALOG_HIGH(median))
			analogCalibration.axis[2].max = median;
		if (ANALOG_HIGH(median) < ANALOG_HIGH(analogCalibration.axis[2].min))
			analogCalibration.axis[2].min = median;
		if (analogCalibration.axis[2].min < analogCalibration.axis[2].max) {
			analogAxes[2].range = analogCalibration.axis[2].max - analogCalibration.axis[2].min;
			if (ANALOG_HIGH(analogAxes[2].range) >= analogMinRange)
				analogAxes[2].calibrated = 1;
		}
		if (analogFlags & 0x01)
			analogScale20(median);
		if (analogFlags & 0x02)
			analogScale21(median);
#if ANALOG_LIMITS
	} else
		ANALOG_FAULT(analogAxes[2].median, 2, medianFault, &analogAxes[2].medianSnapshot);
#else
	}
#endif
}

/* axis 2 in set 0: the position held between the calibration's ends less the set's margin,
   and its value 0-0xff between them */
void analogScale20(unsigned int position)
{
	unsigned long margin;

	margin = (unsigned long)analogAxes[2].range * analogScales[0] / 1000;
	analogAxes[2].output[0].margin = margin;
	analogAxes[2].output[0].margin &= ADC_RESULT_MASK;
	analogAxes[2].output[0].high = analogCalibration.axis[2].max - analogAxes[2].output[0].margin;
	if (analogAxes[2].output[0].high < position)
		position = analogAxes[2].output[0].high;
	analogAxes[2].output[0].low = analogCalibration.axis[2].min + analogAxes[2].output[0].margin;
	if (position < analogAxes[2].output[0].low)
		position = analogAxes[2].output[0].low;
	analogAxes[2].output[0].position = position;
	if (analogAxes[2].output[0].position != analogAxes[2].output[0].last && analogAxes[2].calibrated == 1) {
		analogAxes[2].output[0].last = analogAxes[2].output[0].position;
		analogWorkLow = analogAxes[2].output[0].last - analogAxes[2].output[0].low;
		analogWorkLow -= ANALOG_HIGH(analogWorkLow);
		analogWorkHigh = analogAxes[2].output[0].high - analogAxes[2].output[0].low;
		analogWorkLow = analogWorkLow / ANALOG_HIGH(analogWorkHigh);
		if (ANALOG_HIGH(analogWorkLow))
			analogAxes[2].output[0].value = 0xff;
		else
			analogAxes[2].output[0].value = analogWorkLow;
	}
}

/* axis 2 in set 1: the position held between the calibration's ends less the set's margin,
   and its value 0-0xff between them */
void analogScale21(unsigned int position)
{
	unsigned long margin;

	margin = (unsigned long)analogAxes[2].range * analogScales[1] / 1000;
	analogAxes[2].output[1].margin = margin;
	analogAxes[2].output[1].margin &= ADC_RESULT_MASK;
	analogAxes[2].output[1].high = analogCalibration.axis[2].max - analogAxes[2].output[1].margin;
	if (analogAxes[2].output[1].high < position)
		position = analogAxes[2].output[1].high;
	analogAxes[2].output[1].low = analogCalibration.axis[2].min + analogAxes[2].output[1].margin;
	if (position < analogAxes[2].output[1].low)
		position = analogAxes[2].output[1].low;
	analogAxes[2].output[1].position = position;
	if (analogAxes[2].output[1].position != analogAxes[2].output[1].last && analogAxes[2].calibrated == 1) {
		analogAxes[2].output[1].last = analogAxes[2].output[1].position;
		analogWorkLow = analogAxes[2].output[1].last - analogAxes[2].output[1].low;
		analogWorkLow -= ANALOG_HIGH(analogWorkLow);
		analogWorkHigh = analogAxes[2].output[1].high - analogAxes[2].output[1].low;
		analogWorkLow = analogWorkLow / ANALOG_HIGH(analogWorkHigh);
		if (ANALOG_HIGH(analogWorkLow))
			analogAxes[2].output[1].value = 0xff;
		else
			analogAxes[2].output[1].value = analogWorkLow;
	}
}

/* axis 3: the median of its samples; one within the limits widens the calibration and is
   scaled into its sets, one outside them is recorded */
void analogFilter3(void)
{
	unsigned int median;

	if (analogAxes[3].samples[0] < analogAxes[3].samples[1]) {
		if (analogAxes[3].samples[1] < analogAxes[3].samples[2])
			analogAxes[3].median = analogAxes[3].samples[1];
		else if (analogAxes[3].samples[0] < analogAxes[3].samples[2])
			analogAxes[3].median = analogAxes[3].samples[2];
		else
			analogAxes[3].median = analogAxes[3].samples[0];
	} else {
		if (analogAxes[3].samples[2] < analogAxes[3].samples[1])
			analogAxes[3].median = analogAxes[3].samples[1];
		else if (analogAxes[3].samples[2] < analogAxes[3].samples[0])
			analogAxes[3].median = analogAxes[3].samples[2];
		else
			analogAxes[3].median = analogAxes[3].samples[0];
	}
#if ANALOG_LIMITS
	if (!(analogAxes[3].median & 0x003f) && analogAxes[3].limitMin < analogAxes[3].median &&
	    analogAxes[3].median < analogAxes[3].limitMax) {
#else
	if (analogAxes[3].median >= 0 && analogAxes[3].median <= ADC_RESULT_MASK) {
#endif
		median = analogAxes[3].median;
		if (ANALOG_HIGH(analogCalibration.axis[3].max) < ANALOG_HIGH(median))
			analogCalibration.axis[3].max = median;
		if (ANALOG_HIGH(median) < ANALOG_HIGH(analogCalibration.axis[3].min))
			analogCalibration.axis[3].min = median;
		if (analogCalibration.axis[3].min < analogCalibration.axis[3].max) {
			analogAxes[3].range = analogCalibration.axis[3].max - analogCalibration.axis[3].min;
			if (ANALOG_HIGH(analogAxes[3].range) >= analogMinRange)
				analogAxes[3].calibrated = 1;
		}
		if (analogFlags & 0x01)
			analogScale30(median);
		if (analogFlags & 0x02)
			analogScale31(median);
#if ANALOG_LIMITS
	} else
		ANALOG_FAULT(analogAxes[3].median, 3, medianFault, &analogAxes[3].medianSnapshot);
#else
	}
#endif
}

#if ANALOG_LIMITS
/* a reading outside an axis's limits recorded */
#if ANALOG_SNAPSHOT
static void analogFault(unsigned int sample, struct AnalogAxis *axis, struct AnalogFault *fault,
			struct AnalogSnapshot *snapshot)
{
	if (sample & 0x003f)
		fault->noise = sample;
	if (axis->limitMin >= sample) {
		if (fault->below != 0xff)
			fault->below++;
		if (sample < fault->low) {
			fault->low = sample;
			if (snapshot)
				analogSnapshotTake(axis, snapshot);
		}
	} else if (sample >= axis->limitMax) {
		if (fault->above != 0xff)
			fault->above++;
		if (fault->high < sample) {
			fault->high = sample;
			if (snapshot)
				analogSnapshotTake(axis, snapshot);
		}
	}
}

static void analogSnapshotTake(struct AnalogAxis *axis, struct AnalogSnapshot *snapshot)
{
	snapshot->samples[0] = axis->samples[0];
	snapshot->samples[1] = axis->samples[1];
	snapshot->samples[2] = axis->samples[2];
	snapshot->check = 0xffff - (snapshot->samples[0] + snapshot->samples[1] +
				    snapshot->samples[2]);
}
#else
static void analogFault(unsigned int sample, unsigned int min, unsigned int max,
			struct AnalogFault *fault)
{
	if (sample & 0x003f)
		fault->noise = sample;
	if (min >= sample) {
		if (fault->below != 0xff)
			fault->below++;
		if (sample < fault->low)
			fault->low = sample;
	} else if (sample >= max) {
		if (fault->above != 0xff)
			fault->above++;
		if (fault->high < sample)
			fault->high = sample;
	}
}
#endif
#endif

/* axis 3 in set 0: the position held between the calibration's ends less the set's margin,
   and its value 0-0xff between them */
void analogScale30(unsigned int position)
{
	unsigned long margin;

	margin = (unsigned long)analogAxes[3].range * analogScales[0] / 1000;
	analogAxes[3].output[0].margin = margin;
	analogAxes[3].output[0].margin &= ADC_RESULT_MASK;
	analogAxes[3].output[0].high = analogCalibration.axis[3].max - analogAxes[3].output[0].margin;
	if (analogAxes[3].output[0].high < position)
		position = analogAxes[3].output[0].high;
	analogAxes[3].output[0].low = analogCalibration.axis[3].min + analogAxes[3].output[0].margin;
	if (position < analogAxes[3].output[0].low)
		position = analogAxes[3].output[0].low;
	analogAxes[3].output[0].position = position;
	if (analogAxes[3].output[0].position != analogAxes[3].output[0].last && analogAxes[3].calibrated == 1) {
		analogAxes[3].output[0].last = analogAxes[3].output[0].position;
		analogWorkLow = analogAxes[3].output[0].last - analogAxes[3].output[0].low;
		analogWorkLow -= ANALOG_HIGH(analogWorkLow);
		analogWorkHigh = analogAxes[3].output[0].high - analogAxes[3].output[0].low;
		analogWorkLow = analogWorkLow / ANALOG_HIGH(analogWorkHigh);
		if (ANALOG_HIGH(analogWorkLow))
			analogAxes[3].output[0].value = 0xff;
		else
			analogAxes[3].output[0].value = analogWorkLow;
	}
}

/* axis 3 in set 1: the position held between the calibration's ends less the set's margin,
   and its value 0-0xff between them */
void analogScale31(unsigned int position)
{
	unsigned long margin;

	margin = (unsigned long)analogAxes[3].range * analogScales[1] / 1000;
	analogAxes[3].output[1].margin = margin;
	analogAxes[3].output[1].margin &= ADC_RESULT_MASK;
	analogAxes[3].output[1].high = analogCalibration.axis[3].max - analogAxes[3].output[1].margin;
	if (analogAxes[3].output[1].high < position)
		position = analogAxes[3].output[1].high;
	analogAxes[3].output[1].low = analogCalibration.axis[3].min + analogAxes[3].output[1].margin;
	if (position < analogAxes[3].output[1].low)
		position = analogAxes[3].output[1].low;
	analogAxes[3].output[1].position = position;
	if (analogAxes[3].output[1].position != analogAxes[3].output[1].last && analogAxes[3].calibrated == 1) {
		analogAxes[3].output[1].last = analogAxes[3].output[1].position;
		analogWorkLow = analogAxes[3].output[1].last - analogAxes[3].output[1].low;
		analogWorkLow -= ANALOG_HIGH(analogWorkLow);
		analogWorkHigh = analogAxes[3].output[1].high - analogAxes[3].output[1].low;
		analogWorkLow = analogWorkLow / ANALOG_HIGH(analogWorkHigh);
		if (ANALOG_HIGH(analogWorkLow))
			analogAxes[3].output[1].value = 0xff;
		else
			analogAxes[3].output[1].value = analogWorkLow;
	}
}
#else
/* axis 0: the median of its samples, the calibration widened to it, then its sets */
void analogFilter0(void)
{
	unsigned int median;

	if (analogAxes[0].samples[0] < analogAxes[0].samples[1]) {
		if (analogAxes[0].samples[1] < analogAxes[0].samples[2])
			analogAxes[0].median = analogAxes[0].samples[1];
		else if (analogAxes[0].samples[0] < analogAxes[0].samples[2])
			analogAxes[0].median = analogAxes[0].samples[2];
		else
			analogAxes[0].median = analogAxes[0].samples[0];
	} else {
		if (analogAxes[0].samples[2] < analogAxes[0].samples[1])
			analogAxes[0].median = analogAxes[0].samples[1];
		else if (analogAxes[0].samples[2] < analogAxes[0].samples[0])
			analogAxes[0].median = analogAxes[0].samples[2];
		else
			analogAxes[0].median = analogAxes[0].samples[0];
	}
	if (analogAxes[0].median >= 0 && analogAxes[0].median <= ADC_RESULT_MASK) {
		median = analogAxes[0].median;
		if (ANALOG_HIGH(analogCalibration.axis[0].max) < ANALOG_HIGH(median))
			analogCalibration.axis[0].max = median;
		if (ANALOG_HIGH(median) < ANALOG_HIGH(analogCalibration.axis[0].min))
			analogCalibration.axis[0].min = median;
		if (analogCalibration.axis[0].min < analogCalibration.axis[0].max) {
			analogAxes[0].range = analogCalibration.axis[0].max -
					     analogCalibration.axis[0].min;
			if (ANALOG_HIGH(analogAxes[0].range) >= analogMinRange)
				analogAxes[0].calibrated = 1;
		}
		if (analogFlags & 0x01)
			analogScale00(median);
		if (analogFlags & 0x02)
			analogScale01(median);
	}
}

/* axis 0 in set 0: the position held between the calibration's ends less the set's margin,
   and its value 0-0xff between them */
void analogScale00(unsigned int position)
{
	MDUC.7 = 0;
	MDAH = analogAxes[0].range;
	MDAL = analogScales[0];
	NOP();
	analogWorkLow = MDBL;
	analogWorkHigh = MDBH;
	MDUC.7 = 1;
	MDAH = analogWorkHigh;
	MDAL = analogWorkLow;
	MDBH = 0;
	MDBL = 1000;
	MDUC.0 = 1;
	NOP(); NOP(); NOP(); NOP(); NOP(); NOP(); NOP(); NOP();
	NOP(); NOP(); NOP(); NOP(); NOP(); NOP(); NOP(); NOP();
	analogAxes[0].output[0].margin = MDAL;
	analogAxes[0].output[0].margin &= ADC_RESULT_MASK;
	analogAxes[0].output[0].high = analogCalibration.axis[0].max -
					analogAxes[0].output[0].margin;
	if (analogAxes[0].output[0].high < position)
		position = analogAxes[0].output[0].high;
	analogAxes[0].output[0].low = analogCalibration.axis[0].min +
				       analogAxes[0].output[0].margin;
	if (position < analogAxes[0].output[0].low)
		position = analogAxes[0].output[0].low;
	analogAxes[0].output[0].position = position;
	if (analogAxes[0].output[0].position != analogAxes[0].output[0].last &&
	    analogAxes[0].calibrated == 1) {
		analogAxes[0].output[0].last = analogAxes[0].output[0].position;
		analogWorkLow = analogAxes[0].output[0].last - analogAxes[0].output[0].low;
		analogWorkLow -= ANALOG_HIGH(analogWorkLow);
		analogWorkHigh = analogAxes[0].output[0].high - analogAxes[0].output[0].low;
		analogWorkLow = analogWorkLow / ANALOG_HIGH(analogWorkHigh);
		if (ANALOG_HIGH(analogWorkLow))
			analogAxes[0].output[0].value = 0xff;
		else
			analogAxes[0].output[0].value = analogWorkLow;
	}
}

/* axis 0 in set 1: the position held between the calibration's ends less the set's margin,
   and its value 0-0xff between them */
void analogScale01(unsigned int position)
{
	MDUC.7 = 0;
	MDAH = analogAxes[0].range;
	MDAL = analogScales[1];
	NOP();
	analogWorkLow = MDBL;
	analogWorkHigh = MDBH;
	MDUC.7 = 1;
	MDAH = analogWorkHigh;
	MDAL = analogWorkLow;
	MDBH = 0;
	MDBL = 1000;
	MDUC.0 = 1;
	NOP(); NOP(); NOP(); NOP(); NOP(); NOP(); NOP(); NOP();
	NOP(); NOP(); NOP(); NOP(); NOP(); NOP(); NOP(); NOP();
	analogAxes[0].output[1].margin = MDAL;
	analogAxes[0].output[1].margin &= ADC_RESULT_MASK;
	analogAxes[0].output[1].high = analogCalibration.axis[0].max -
					analogAxes[0].output[1].margin;
	if (analogAxes[0].output[1].high < position)
		position = analogAxes[0].output[1].high;
	analogAxes[0].output[1].low = analogCalibration.axis[0].min +
				       analogAxes[0].output[1].margin;
	if (position < analogAxes[0].output[1].low)
		position = analogAxes[0].output[1].low;
	analogAxes[0].output[1].position = position;
	if (analogAxes[0].output[1].position != analogAxes[0].output[1].last &&
	    analogAxes[0].calibrated == 1) {
		analogAxes[0].output[1].last = analogAxes[0].output[1].position;
		analogWorkLow = analogAxes[0].output[1].last - analogAxes[0].output[1].low;
		analogWorkLow -= ANALOG_HIGH(analogWorkLow);
		analogWorkHigh = analogAxes[0].output[1].high - analogAxes[0].output[1].low;
		analogWorkLow = analogWorkLow / ANALOG_HIGH(analogWorkHigh);
		if (ANALOG_HIGH(analogWorkLow))
			analogAxes[0].output[1].value = 0xff;
		else
			analogAxes[0].output[1].value = analogWorkLow;
	}
}

/* axis 1: the median of its samples, the calibration widened to it, then its sets */
void analogFilter1(void)
{
	unsigned int median;

	if (analogAxes[1].samples[0] < analogAxes[1].samples[1]) {
		if (analogAxes[1].samples[1] < analogAxes[1].samples[2])
			analogAxes[1].median = analogAxes[1].samples[1];
		else if (analogAxes[1].samples[0] < analogAxes[1].samples[2])
			analogAxes[1].median = analogAxes[1].samples[2];
		else
			analogAxes[1].median = analogAxes[1].samples[0];
	} else {
		if (analogAxes[1].samples[2] < analogAxes[1].samples[1])
			analogAxes[1].median = analogAxes[1].samples[1];
		else if (analogAxes[1].samples[2] < analogAxes[1].samples[0])
			analogAxes[1].median = analogAxes[1].samples[2];
		else
			analogAxes[1].median = analogAxes[1].samples[0];
	}
	if (analogAxes[1].median >= 0 && analogAxes[1].median <= ADC_RESULT_MASK) {
		median = analogAxes[1].median;
		if (ANALOG_HIGH(analogCalibration.axis[1].max) < ANALOG_HIGH(median))
			analogCalibration.axis[1].max = median;
		if (ANALOG_HIGH(median) < ANALOG_HIGH(analogCalibration.axis[1].min))
			analogCalibration.axis[1].min = median;
		if (analogCalibration.axis[1].min < analogCalibration.axis[1].max) {
			analogAxes[1].range = analogCalibration.axis[1].max -
					     analogCalibration.axis[1].min;
			if (ANALOG_HIGH(analogAxes[1].range) >= analogMinRange)
				analogAxes[1].calibrated = 1;
		}
		if (analogFlags & 0x01)
			analogScale10(median);
		if (analogFlags & 0x02)
			analogScale11(median);
	}
}

/* axis 1 in set 0: the position held between the calibration's ends less the set's margin,
   and its value 0-0xff between them */
void analogScale10(unsigned int position)
{
	MDUC.7 = 0;
	MDAH = analogAxes[1].range;
	MDAL = analogScales[0];
	NOP();
	analogWorkLow = MDBL;
	analogWorkHigh = MDBH;
	MDUC.7 = 1;
	MDAH = analogWorkHigh;
	MDAL = analogWorkLow;
	MDBH = 0;
	MDBL = 1000;
	MDUC.0 = 1;
	NOP(); NOP(); NOP(); NOP(); NOP(); NOP(); NOP(); NOP();
	NOP(); NOP(); NOP(); NOP(); NOP(); NOP(); NOP(); NOP();
	analogAxes[1].output[0].margin = MDAL;
	analogAxes[1].output[0].margin &= ADC_RESULT_MASK;
	analogAxes[1].output[0].high = analogCalibration.axis[1].max -
					analogAxes[1].output[0].margin;
	if (analogAxes[1].output[0].high < position)
		position = analogAxes[1].output[0].high;
	analogAxes[1].output[0].low = analogCalibration.axis[1].min +
				       analogAxes[1].output[0].margin;
	if (position < analogAxes[1].output[0].low)
		position = analogAxes[1].output[0].low;
	analogAxes[1].output[0].position = position;
	if (analogAxes[1].output[0].position != analogAxes[1].output[0].last &&
	    analogAxes[1].calibrated == 1) {
		analogAxes[1].output[0].last = analogAxes[1].output[0].position;
		analogWorkLow = analogAxes[1].output[0].last - analogAxes[1].output[0].low;
		analogWorkLow -= ANALOG_HIGH(analogWorkLow);
		analogWorkHigh = analogAxes[1].output[0].high - analogAxes[1].output[0].low;
		analogWorkLow = analogWorkLow / ANALOG_HIGH(analogWorkHigh);
		if (ANALOG_HIGH(analogWorkLow))
			analogAxes[1].output[0].value = 0xff;
		else
			analogAxes[1].output[0].value = analogWorkLow;
	}
}

/* axis 1 in set 1: the position held between the calibration's ends less the set's margin,
   and its value 0-0xff between them */
void analogScale11(unsigned int position)
{
	MDUC.7 = 0;
	MDAH = analogAxes[1].range;
	MDAL = analogScales[1];
	NOP();
	analogWorkLow = MDBL;
	analogWorkHigh = MDBH;
	MDUC.7 = 1;
	MDAH = analogWorkHigh;
	MDAL = analogWorkLow;
	MDBH = 0;
	MDBL = 1000;
	MDUC.0 = 1;
	NOP(); NOP(); NOP(); NOP(); NOP(); NOP(); NOP(); NOP();
	NOP(); NOP(); NOP(); NOP(); NOP(); NOP(); NOP(); NOP();
	analogAxes[1].output[1].margin = MDAL;
	analogAxes[1].output[1].margin &= ADC_RESULT_MASK;
	analogAxes[1].output[1].high = analogCalibration.axis[1].max -
					analogAxes[1].output[1].margin;
	if (analogAxes[1].output[1].high < position)
		position = analogAxes[1].output[1].high;
	analogAxes[1].output[1].low = analogCalibration.axis[1].min +
				       analogAxes[1].output[1].margin;
	if (position < analogAxes[1].output[1].low)
		position = analogAxes[1].output[1].low;
	analogAxes[1].output[1].position = position;
	if (analogAxes[1].output[1].position != analogAxes[1].output[1].last &&
	    analogAxes[1].calibrated == 1) {
		analogAxes[1].output[1].last = analogAxes[1].output[1].position;
		analogWorkLow = analogAxes[1].output[1].last - analogAxes[1].output[1].low;
		analogWorkLow -= ANALOG_HIGH(analogWorkLow);
		analogWorkHigh = analogAxes[1].output[1].high - analogAxes[1].output[1].low;
		analogWorkLow = analogWorkLow / ANALOG_HIGH(analogWorkHigh);
		if (ANALOG_HIGH(analogWorkLow))
			analogAxes[1].output[1].value = 0xff;
		else
			analogAxes[1].output[1].value = analogWorkLow;
	}
}

/* axis 2: the median of its samples, the calibration widened to it, then its sets */
void analogFilter2(void)
{
	unsigned int median;

	if (analogAxes[2].samples[0] < analogAxes[2].samples[1]) {
		if (analogAxes[2].samples[1] < analogAxes[2].samples[2])
			analogAxes[2].median = analogAxes[2].samples[1];
		else if (analogAxes[2].samples[0] < analogAxes[2].samples[2])
			analogAxes[2].median = analogAxes[2].samples[2];
		else
			analogAxes[2].median = analogAxes[2].samples[0];
	} else {
		if (analogAxes[2].samples[2] < analogAxes[2].samples[1])
			analogAxes[2].median = analogAxes[2].samples[1];
		else if (analogAxes[2].samples[2] < analogAxes[2].samples[0])
			analogAxes[2].median = analogAxes[2].samples[2];
		else
			analogAxes[2].median = analogAxes[2].samples[0];
	}
	if (analogAxes[2].median >= 0 && analogAxes[2].median <= ADC_RESULT_MASK) {
		median = analogAxes[2].median;
		if (ANALOG_HIGH(analogCalibration.axis[2].max) < ANALOG_HIGH(median))
			analogCalibration.axis[2].max = median;
		if (ANALOG_HIGH(median) < ANALOG_HIGH(analogCalibration.axis[2].min))
			analogCalibration.axis[2].min = median;
		if (analogCalibration.axis[2].min < analogCalibration.axis[2].max) {
			analogAxes[2].range = analogCalibration.axis[2].max -
					     analogCalibration.axis[2].min;
			if (ANALOG_HIGH(analogAxes[2].range) >= analogMinRange)
				analogAxes[2].calibrated = 1;
		}
		if (analogFlags & 0x01)
			analogScale20(median);
		if (analogFlags & 0x02)
			analogScale21(median);
	}
}

/* axis 2 in set 0: the position held between the calibration's ends less the set's margin,
   and its value 0-0xff between them */
void analogScale20(unsigned int position)
{
	MDUC.7 = 0;
	MDAH = analogAxes[2].range;
	MDAL = analogScales[0];
	NOP();
	analogWorkLow = MDBL;
	analogWorkHigh = MDBH;
	MDUC.7 = 1;
	MDAH = analogWorkHigh;
	MDAL = analogWorkLow;
	MDBH = 0;
	MDBL = 1000;
	MDUC.0 = 1;
	NOP(); NOP(); NOP(); NOP(); NOP(); NOP(); NOP(); NOP();
	NOP(); NOP(); NOP(); NOP(); NOP(); NOP(); NOP(); NOP();
	analogAxes[2].output[0].margin = MDAL;
	analogAxes[2].output[0].margin &= ADC_RESULT_MASK;
	analogAxes[2].output[0].high = analogCalibration.axis[2].max -
					analogAxes[2].output[0].margin;
	if (analogAxes[2].output[0].high < position)
		position = analogAxes[2].output[0].high;
	analogAxes[2].output[0].low = analogCalibration.axis[2].min +
				       analogAxes[2].output[0].margin;
	if (position < analogAxes[2].output[0].low)
		position = analogAxes[2].output[0].low;
	analogAxes[2].output[0].position = position;
	if (analogAxes[2].output[0].position != analogAxes[2].output[0].last &&
	    analogAxes[2].calibrated == 1) {
		analogAxes[2].output[0].last = analogAxes[2].output[0].position;
		analogWorkLow = analogAxes[2].output[0].last - analogAxes[2].output[0].low;
		analogWorkLow -= ANALOG_HIGH(analogWorkLow);
		analogWorkHigh = analogAxes[2].output[0].high - analogAxes[2].output[0].low;
		analogWorkLow = analogWorkLow / ANALOG_HIGH(analogWorkHigh);
		if (ANALOG_HIGH(analogWorkLow))
			analogAxes[2].output[0].value = 0xff;
		else
			analogAxes[2].output[0].value = analogWorkLow;
	}
}

/* axis 2 in set 1: the position held between the calibration's ends less the set's margin,
   and its value 0-0xff between them */
void analogScale21(unsigned int position)
{
	MDUC.7 = 0;
	MDAH = analogAxes[2].range;
	MDAL = analogScales[1];
	NOP();
	analogWorkLow = MDBL;
	analogWorkHigh = MDBH;
	MDUC.7 = 1;
	MDAH = analogWorkHigh;
	MDAL = analogWorkLow;
	MDBH = 0;
	MDBL = 1000;
	MDUC.0 = 1;
	NOP(); NOP(); NOP(); NOP(); NOP(); NOP(); NOP(); NOP();
	NOP(); NOP(); NOP(); NOP(); NOP(); NOP(); NOP(); NOP();
	analogAxes[2].output[1].margin = MDAL;
	analogAxes[2].output[1].margin &= ADC_RESULT_MASK;
	analogAxes[2].output[1].high = analogCalibration.axis[2].max -
					analogAxes[2].output[1].margin;
	if (analogAxes[2].output[1].high < position)
		position = analogAxes[2].output[1].high;
	analogAxes[2].output[1].low = analogCalibration.axis[2].min +
				       analogAxes[2].output[1].margin;
	if (position < analogAxes[2].output[1].low)
		position = analogAxes[2].output[1].low;
	analogAxes[2].output[1].position = position;
	if (analogAxes[2].output[1].position != analogAxes[2].output[1].last &&
	    analogAxes[2].calibrated == 1) {
		analogAxes[2].output[1].last = analogAxes[2].output[1].position;
		analogWorkLow = analogAxes[2].output[1].last - analogAxes[2].output[1].low;
		analogWorkLow -= ANALOG_HIGH(analogWorkLow);
		analogWorkHigh = analogAxes[2].output[1].high - analogAxes[2].output[1].low;
		analogWorkLow = analogWorkLow / ANALOG_HIGH(analogWorkHigh);
		if (ANALOG_HIGH(analogWorkLow))
			analogAxes[2].output[1].value = 0xff;
		else
			analogAxes[2].output[1].value = analogWorkLow;
	}
}

/* axis 3: the median of its samples, the calibration widened to it, then its sets */
void analogFilter3(void)
{
	unsigned int median;

	if (analogAxes[3].samples[0] < analogAxes[3].samples[1]) {
		if (analogAxes[3].samples[1] < analogAxes[3].samples[2])
			analogAxes[3].median = analogAxes[3].samples[1];
		else if (analogAxes[3].samples[0] < analogAxes[3].samples[2])
			analogAxes[3].median = analogAxes[3].samples[2];
		else
			analogAxes[3].median = analogAxes[3].samples[0];
	} else {
		if (analogAxes[3].samples[2] < analogAxes[3].samples[1])
			analogAxes[3].median = analogAxes[3].samples[1];
		else if (analogAxes[3].samples[2] < analogAxes[3].samples[0])
			analogAxes[3].median = analogAxes[3].samples[2];
		else
			analogAxes[3].median = analogAxes[3].samples[0];
	}
	if (analogAxes[3].median >= 0 && analogAxes[3].median <= ADC_RESULT_MASK) {
		median = analogAxes[3].median;
		if (ANALOG_HIGH(analogCalibration.axis[3].max) < ANALOG_HIGH(median))
			analogCalibration.axis[3].max = median;
		if (ANALOG_HIGH(median) < ANALOG_HIGH(analogCalibration.axis[3].min))
			analogCalibration.axis[3].min = median;
		if (analogCalibration.axis[3].min < analogCalibration.axis[3].max) {
			analogAxes[3].range = analogCalibration.axis[3].max -
					     analogCalibration.axis[3].min;
			if (ANALOG_HIGH(analogAxes[3].range) >= analogMinRange)
				analogAxes[3].calibrated = 1;
		}
		if (analogFlags & 0x01)
			analogScale30(median);
		if (analogFlags & 0x02)
			analogScale31(median);
	}
}

/* axis 3 in set 0: the position held between the calibration's ends less the set's margin,
   and its value 0-0xff between them */
void analogScale30(unsigned int position)
{
	MDUC.7 = 0;
	MDAH = analogAxes[3].range;
	MDAL = analogScales[0];
	NOP();
	analogWorkLow = MDBL;
	analogWorkHigh = MDBH;
	MDUC.7 = 1;
	MDAH = analogWorkHigh;
	MDAL = analogWorkLow;
	MDBH = 0;
	MDBL = 1000;
	MDUC.0 = 1;
	NOP(); NOP(); NOP(); NOP(); NOP(); NOP(); NOP(); NOP();
	NOP(); NOP(); NOP(); NOP(); NOP(); NOP(); NOP(); NOP();
	analogAxes[3].output[0].margin = MDAL;
	analogAxes[3].output[0].margin &= ADC_RESULT_MASK;
	analogAxes[3].output[0].high = analogCalibration.axis[3].max -
					analogAxes[3].output[0].margin;
	if (analogAxes[3].output[0].high < position)
		position = analogAxes[3].output[0].high;
	analogAxes[3].output[0].low = analogCalibration.axis[3].min +
				       analogAxes[3].output[0].margin;
	if (position < analogAxes[3].output[0].low)
		position = analogAxes[3].output[0].low;
	analogAxes[3].output[0].position = position;
	if (analogAxes[3].output[0].position != analogAxes[3].output[0].last &&
	    analogAxes[3].calibrated == 1) {
		analogAxes[3].output[0].last = analogAxes[3].output[0].position;
		analogWorkLow = analogAxes[3].output[0].last - analogAxes[3].output[0].low;
		analogWorkLow -= ANALOG_HIGH(analogWorkLow);
		analogWorkHigh = analogAxes[3].output[0].high - analogAxes[3].output[0].low;
		analogWorkLow = analogWorkLow / ANALOG_HIGH(analogWorkHigh);
		if (ANALOG_HIGH(analogWorkLow))
			analogAxes[3].output[0].value = 0xff;
		else
			analogAxes[3].output[0].value = analogWorkLow;
	}
}

/* axis 3 in set 1: the position held between the calibration's ends less the set's margin,
   and its value 0-0xff between them */
void analogScale31(unsigned int position)
{
	MDUC.7 = 0;
	MDAH = analogAxes[3].range;
	MDAL = analogScales[1];
	NOP();
	analogWorkLow = MDBL;
	analogWorkHigh = MDBH;
	MDUC.7 = 1;
	MDAH = analogWorkHigh;
	MDAL = analogWorkLow;
	MDBH = 0;
	MDBL = 1000;
	MDUC.0 = 1;
	NOP(); NOP(); NOP(); NOP(); NOP(); NOP(); NOP(); NOP();
	NOP(); NOP(); NOP(); NOP(); NOP(); NOP(); NOP(); NOP();
	analogAxes[3].output[1].margin = MDAL;
	analogAxes[3].output[1].margin &= ADC_RESULT_MASK;
	analogAxes[3].output[1].high = analogCalibration.axis[3].max -
					analogAxes[3].output[1].margin;
	if (analogAxes[3].output[1].high < position)
		position = analogAxes[3].output[1].high;
	analogAxes[3].output[1].low = analogCalibration.axis[3].min +
				       analogAxes[3].output[1].margin;
	if (position < analogAxes[3].output[1].low)
		position = analogAxes[3].output[1].low;
	analogAxes[3].output[1].position = position;
	if (analogAxes[3].output[1].position != analogAxes[3].output[1].last &&
	    analogAxes[3].calibrated == 1) {
		analogAxes[3].output[1].last = analogAxes[3].output[1].position;
		analogWorkLow = analogAxes[3].output[1].last - analogAxes[3].output[1].low;
		analogWorkLow -= ANALOG_HIGH(analogWorkLow);
		analogWorkHigh = analogAxes[3].output[1].high - analogAxes[3].output[1].low;
		analogWorkLow = analogWorkLow / ANALOG_HIGH(analogWorkHigh);
		if (ANALOG_HIGH(analogWorkLow))
			analogAxes[3].output[1].value = 0xff;
		else
			analogAxes[3].output[1].value = analogWorkLow;
	}
}

#endif

/* the sets' values as reported: axis 1 inverted, axis 0, axis 3, axis 2 inverted (0x00315200:
   axes 0 and 1 inverted, 2 and 3) */
void analogReport(void)
{
#if BOARD_ANALOG_CHANNEL8
	/* channel 8's level 2: the axes as they are, otherwise each turned over */
	if (analogSuspend == 2) {
		if (analogFlags & ANALOG_SET0) {
			analogSticks0[0] = analogAxes[0].output[0].value;
			analogSticks0[1] = analogAxes[1].output[0].value;
			analogSticks0[2] = analogAxes[2].output[0].value;
			analogSticks0[3] = analogAxes[3].output[0].value;
		}
		if (analogFlags & ANALOG_SET1) {
			analogSticks1[0] = analogAxes[0].output[1].value;
			analogSticks1[1] = analogAxes[1].output[1].value;
			analogSticks1[2] = analogAxes[2].output[1].value;
			analogSticks1[3] = analogAxes[3].output[1].value;
		}
	} else {
		if (analogFlags & ANALOG_SET0) {
			analogSticks0[0] = 0xff - analogAxes[0].output[0].value;
			analogSticks0[1] = 0xff - analogAxes[1].output[0].value;
			analogSticks0[2] = 0xff - analogAxes[2].output[0].value;
			analogSticks0[3] = 0xff - analogAxes[3].output[0].value;
		}
		if (analogFlags & ANALOG_SET1) {
			analogSticks1[0] = 0xff - analogAxes[0].output[1].value;
			analogSticks1[1] = 0xff - analogAxes[1].output[1].value;
			analogSticks1[2] = 0xff - analogAxes[2].output[1].value;
			analogSticks1[3] = 0xff - analogAxes[3].output[1].value;
		}
	}
#elif HW_IS(0x00315200)
	/* axes 0 and 1 inverted, 2 and 3 as they are */
	if (analogFlags & ANALOG_SET0) {
		analogSticks0[0] = 0xff - analogAxes[0].output[0].value;
		analogSticks0[1] = 0xff - analogAxes[1].output[0].value;
		analogSticks0[2] = analogAxes[2].output[0].value;
		analogSticks0[3] = analogAxes[3].output[0].value;
	}
	if (analogFlags & ANALOG_SET1) {
		analogSticks1[0] = 0xff - analogAxes[0].output[1].value;
		analogSticks1[1] = 0xff - analogAxes[1].output[1].value;
		analogSticks1[2] = analogAxes[2].output[1].value;
		analogSticks1[3] = analogAxes[3].output[1].value;
	}
#else
	if (analogFlags & ANALOG_SET0) {
		analogSticks0[0] = 0xff - analogAxes[1].output[0].value;
		analogSticks0[1] = analogAxes[0].output[0].value;
		analogSticks0[2] = analogAxes[3].output[0].value;
		analogSticks0[3] = 0xff - analogAxes[2].output[0].value;
	}
	if (analogFlags & ANALOG_SET1) {
		analogSticks1[0] = 0xff - analogAxes[1].output[1].value;
		analogSticks1[1] = analogAxes[0].output[1].value;
		analogSticks1[2] = analogAxes[3].output[1].value;
		analogSticks1[3] = 0xff - analogAxes[2].output[1].value;
	}
#endif
}

#else
/* axis 0: the median of its samples, the calibration widened to it, the position held
   between the calibration's ends less the margin, and its value 0-0xff between them */
void analogFilter0(void)
{
	union {
		unsigned long	product;
		unsigned int	word[2];
	} work;

	if (analogAxes[0].samples[0] < analogAxes[0].samples[1]) {
		if (analogAxes[0].samples[1] < analogAxes[0].samples[2])
			analogAxes[0].median = analogAxes[0].samples[1];
		else if (analogAxes[0].samples[0] < analogAxes[0].samples[2])
			analogAxes[0].median = analogAxes[0].samples[2];
		else
			analogAxes[0].median = analogAxes[0].samples[0];
	} else {
		if (analogAxes[0].samples[2] < analogAxes[0].samples[1])
			analogAxes[0].median = analogAxes[0].samples[1];
		else if (analogAxes[0].samples[2] < analogAxes[0].samples[0])
			analogAxes[0].median = analogAxes[0].samples[2];
		else
			analogAxes[0].median = analogAxes[0].samples[0];
	}
	if (analogAxes[0].median >= 0 && analogAxes[0].median <= ADC_RESULT_MASK)
		analogAxes[0].position = analogAxes[0].median;
	if (ANALOG_HIGH(analogCalibration.axis[0].max) < ANALOG_HIGH(analogAxes[0].position))
		analogCalibration.axis[0].max = analogAxes[0].position;
	if (ANALOG_HIGH(analogAxes[0].position) < ANALOG_HIGH(analogCalibration.axis[0].min))
		analogCalibration.axis[0].min = analogAxes[0].position;
	analogAxes[0].range = analogCalibration.axis[0].max - analogCalibration.axis[0].min;
	if (ANALOG_HIGH(analogAxes[0].range) >= 0x64)
		analogAxes[0].calibrated = 1;
	MDUC.7 = 0;
	MDAH = analogAxes[0].range;
	MDAL = analogScale;
	NOP();
	work.word[0] = MDBL;
	work.word[1] = MDBH;
	analogAxes[0].margin = work.product >> analogScaleShift;
	analogAxes[0].margin &= ADC_RESULT_MASK;
	analogAxes[0].high = analogCalibration.axis[0].max - analogAxes[0].margin;
	if (analogAxes[0].high < analogAxes[0].position)
		analogAxes[0].position = analogAxes[0].high;
	analogAxes[0].low = analogCalibration.axis[0].min + analogAxes[0].margin;
	if (analogAxes[0].position < analogAxes[0].low)
		analogAxes[0].position = analogAxes[0].low;
	if (analogAxes[0].position != analogAxes[0].last && analogAxes[0].calibrated == 1) {
		analogAxes[0].last = analogAxes[0].position;
		analogWorkLow = analogAxes[0].last - analogAxes[0].low;
		analogWorkLow -= ANALOG_HIGH(analogWorkLow);
		analogWorkHigh = analogAxes[0].high - analogAxes[0].low;
		analogWorkLow = analogWorkLow / ANALOG_HIGH(analogWorkHigh);
		if (ANALOG_HIGH(analogWorkLow))
			analogAxes[0].value = 0xff;
		else
			analogAxes[0].value = analogWorkLow;
	}
}

/* axis 1: the median of its samples, the calibration widened to it, the position held
   between the calibration's ends less the margin, and its value 0-0xff between them */
void analogFilter1(void)
{
	union {
		unsigned long	product;
		unsigned int	word[2];
	} work;

	if (analogAxes[1].samples[0] < analogAxes[1].samples[1]) {
		if (analogAxes[1].samples[1] < analogAxes[1].samples[2])
			analogAxes[1].median = analogAxes[1].samples[1];
		else if (analogAxes[1].samples[0] < analogAxes[1].samples[2])
			analogAxes[1].median = analogAxes[1].samples[2];
		else
			analogAxes[1].median = analogAxes[1].samples[0];
	} else {
		if (analogAxes[1].samples[2] < analogAxes[1].samples[1])
			analogAxes[1].median = analogAxes[1].samples[1];
		else if (analogAxes[1].samples[2] < analogAxes[1].samples[0])
			analogAxes[1].median = analogAxes[1].samples[2];
		else
			analogAxes[1].median = analogAxes[1].samples[0];
	}
	if (analogAxes[1].median >= 0 && analogAxes[1].median <= ADC_RESULT_MASK)
		analogAxes[1].position = analogAxes[1].median;
	if (ANALOG_HIGH(analogCalibration.axis[1].max) < ANALOG_HIGH(analogAxes[1].position))
		analogCalibration.axis[1].max = analogAxes[1].position;
	if (ANALOG_HIGH(analogAxes[1].position) < ANALOG_HIGH(analogCalibration.axis[1].min))
		analogCalibration.axis[1].min = analogAxes[1].position;
	analogAxes[1].range = analogCalibration.axis[1].max - analogCalibration.axis[1].min;
	if (ANALOG_HIGH(analogAxes[1].range) >= 0x64)
		analogAxes[1].calibrated = 1;
	MDUC.7 = 0;
	MDAH = analogAxes[1].range;
	MDAL = analogScale;
	NOP();
	work.word[0] = MDBL;
	work.word[1] = MDBH;
	analogAxes[1].margin = work.product >> analogScaleShift;
	analogAxes[1].margin &= ADC_RESULT_MASK;
	analogAxes[1].high = analogCalibration.axis[1].max - analogAxes[1].margin;
	if (analogAxes[1].high < analogAxes[1].position)
		analogAxes[1].position = analogAxes[1].high;
	analogAxes[1].low = analogCalibration.axis[1].min + analogAxes[1].margin;
	if (analogAxes[1].position < analogAxes[1].low)
		analogAxes[1].position = analogAxes[1].low;
	if (analogAxes[1].position != analogAxes[1].last && analogAxes[1].calibrated == 1) {
		analogAxes[1].last = analogAxes[1].position;
		analogWorkLow = analogAxes[1].last - analogAxes[1].low;
		analogWorkLow -= ANALOG_HIGH(analogWorkLow);
		analogWorkHigh = analogAxes[1].high - analogAxes[1].low;
		analogWorkLow = analogWorkLow / ANALOG_HIGH(analogWorkHigh);
		if (ANALOG_HIGH(analogWorkLow))
			analogAxes[1].value = 0xff;
		else
			analogAxes[1].value = analogWorkLow;
	}
}

/* axis 2: the median of its samples, the calibration widened to it, the position held
   between the calibration's ends less the margin, and its value 0-0xff between them */
void analogFilter2(void)
{
	union {
		unsigned long	product;
		unsigned int	word[2];
	} work;

	if (analogAxes[2].samples[0] < analogAxes[2].samples[1]) {
		if (analogAxes[2].samples[1] < analogAxes[2].samples[2])
			analogAxes[2].median = analogAxes[2].samples[1];
		else if (analogAxes[2].samples[0] < analogAxes[2].samples[2])
			analogAxes[2].median = analogAxes[2].samples[2];
		else
			analogAxes[2].median = analogAxes[2].samples[0];
	} else {
		if (analogAxes[2].samples[2] < analogAxes[2].samples[1])
			analogAxes[2].median = analogAxes[2].samples[1];
		else if (analogAxes[2].samples[2] < analogAxes[2].samples[0])
			analogAxes[2].median = analogAxes[2].samples[2];
		else
			analogAxes[2].median = analogAxes[2].samples[0];
	}
	if (analogAxes[2].median >= 0 && analogAxes[2].median <= ADC_RESULT_MASK)
		analogAxes[2].position = analogAxes[2].median;
	if (ANALOG_HIGH(analogCalibration.axis[2].max) < ANALOG_HIGH(analogAxes[2].position))
		analogCalibration.axis[2].max = analogAxes[2].position;
	if (ANALOG_HIGH(analogAxes[2].position) < ANALOG_HIGH(analogCalibration.axis[2].min))
		analogCalibration.axis[2].min = analogAxes[2].position;
	analogAxes[2].range = analogCalibration.axis[2].max - analogCalibration.axis[2].min;
	if (ANALOG_HIGH(analogAxes[2].range) >= 0x64)
		analogAxes[2].calibrated = 1;
	MDUC.7 = 0;
	MDAH = analogAxes[2].range;
	MDAL = analogScale;
	NOP();
	work.word[0] = MDBL;
	work.word[1] = MDBH;
	analogAxes[2].margin = work.product >> analogScaleShift;
	analogAxes[2].margin &= ADC_RESULT_MASK;
	analogAxes[2].high = analogCalibration.axis[2].max - analogAxes[2].margin;
	if (analogAxes[2].high < analogAxes[2].position)
		analogAxes[2].position = analogAxes[2].high;
	analogAxes[2].low = analogCalibration.axis[2].min + analogAxes[2].margin;
	if (analogAxes[2].position < analogAxes[2].low)
		analogAxes[2].position = analogAxes[2].low;
	if (analogAxes[2].position != analogAxes[2].last && analogAxes[2].calibrated == 1) {
		analogAxes[2].last = analogAxes[2].position;
		analogWorkLow = analogAxes[2].last - analogAxes[2].low;
		analogWorkLow -= ANALOG_HIGH(analogWorkLow);
		analogWorkHigh = analogAxes[2].high - analogAxes[2].low;
		analogWorkLow = analogWorkLow / ANALOG_HIGH(analogWorkHigh);
		if (ANALOG_HIGH(analogWorkLow))
			analogAxes[2].value = 0xff;
		else
			analogAxes[2].value = analogWorkLow;
	}
}

/* axis 3: the median of its samples, the calibration widened to it, the position held
   between the calibration's ends less the margin, and its value 0-0xff between them */
void analogFilter3(void)
{
	union {
		unsigned long	product;
		unsigned int	word[2];
	} work;

	if (analogAxes[3].samples[0] < analogAxes[3].samples[1]) {
		if (analogAxes[3].samples[1] < analogAxes[3].samples[2])
			analogAxes[3].median = analogAxes[3].samples[1];
		else if (analogAxes[3].samples[0] < analogAxes[3].samples[2])
			analogAxes[3].median = analogAxes[3].samples[2];
		else
			analogAxes[3].median = analogAxes[3].samples[0];
	} else {
		if (analogAxes[3].samples[2] < analogAxes[3].samples[1])
			analogAxes[3].median = analogAxes[3].samples[1];
		else if (analogAxes[3].samples[2] < analogAxes[3].samples[0])
			analogAxes[3].median = analogAxes[3].samples[2];
		else
			analogAxes[3].median = analogAxes[3].samples[0];
	}
	if (analogAxes[3].median >= 0 && analogAxes[3].median <= ADC_RESULT_MASK)
		analogAxes[3].position = analogAxes[3].median;
	if (ANALOG_HIGH(analogCalibration.axis[3].max) < ANALOG_HIGH(analogAxes[3].position))
		analogCalibration.axis[3].max = analogAxes[3].position;
	if (ANALOG_HIGH(analogAxes[3].position) < ANALOG_HIGH(analogCalibration.axis[3].min))
		analogCalibration.axis[3].min = analogAxes[3].position;
	analogAxes[3].range = analogCalibration.axis[3].max - analogCalibration.axis[3].min;
	if (ANALOG_HIGH(analogAxes[3].range) >= 0x64)
		analogAxes[3].calibrated = 1;
	MDUC.7 = 0;
	MDAH = analogAxes[3].range;
	MDAL = analogScale;
	NOP();
	work.word[0] = MDBL;
	work.word[1] = MDBH;
	analogAxes[3].margin = work.product >> analogScaleShift;
	analogAxes[3].margin &= ADC_RESULT_MASK;
	analogAxes[3].high = analogCalibration.axis[3].max - analogAxes[3].margin;
	if (analogAxes[3].high < analogAxes[3].position)
		analogAxes[3].position = analogAxes[3].high;
	analogAxes[3].low = analogCalibration.axis[3].min + analogAxes[3].margin;
	if (analogAxes[3].position < analogAxes[3].low)
		analogAxes[3].position = analogAxes[3].low;
	if (analogAxes[3].position != analogAxes[3].last && analogAxes[3].calibrated == 1) {
		analogAxes[3].last = analogAxes[3].position;
		analogWorkLow = analogAxes[3].last - analogAxes[3].low;
		analogWorkLow -= ANALOG_HIGH(analogWorkLow);
		analogWorkHigh = analogAxes[3].high - analogAxes[3].low;
		analogWorkLow = analogWorkLow / ANALOG_HIGH(analogWorkHigh);
		if (ANALOG_HIGH(analogWorkLow))
			analogAxes[3].value = 0xff;
		else
			analogAxes[3].value = analogWorkLow;
	}
}
#endif

/* the median of three samples (never called) */
#if FW_SINCE(0x00090206)
unsigned int analogMedian(unsigned int *samples)
#else
unsigned int analogMedian(unsigned int __far *samples)
#endif
{
	unsigned int median;

	if (samples[0] < samples[1]) {
		if (samples[1] < samples[2])
			median = samples[1];
		else if (samples[0] < samples[2])
			median = samples[2];
		else
			median = samples[0];
	} else {
		if (samples[2] < samples[1])
			median = samples[1];
		else if (samples[2] < samples[0])
			median = samples[2];
		else
			median = samples[0];
	}
	return median;
}

/* the level of the first of count levels whose limit the input reaches; 0xff if none */
unsigned char analogLevelFind(unsigned char input, const struct AnalogLevel *levels,
			      unsigned char count)
{
	unsigned char i;

	for (i = 0; i < count; i++) {
		if (levels->limit <= input)
			return levels->level;
		levels++;
	}
	return 0xff;
}

#if BOARD_USS1001 || BOARD_DOL1001
/* the same by a whole result */
static unsigned int analogLevelFindWide(unsigned int input, const struct AnalogLevelWide *levels,
					unsigned char count)
{
	unsigned char i;

	for (i = 0; i < count; i++) {
		if (levels->limit <= input)
			return levels->level;
		levels++;
	}
	return 0xff;
}
#endif

#if BOARD_USS1001 || BOARD_DOL1001
/* the sticks on, computing set 0 (mode 1), set 1 (2) or both (3), within their limits; the set
   not computed centred */
void analogSetMode(unsigned char mode)
{
	dis_dsp();
	switch (mode) {
	case 1:
		DI();
		inputKeys.state |= 0x0002;
		EI();
		analogFlags = ANALOG_SET0;
		analogCenterSet1();
		analogResetSet1();
#if ANALOG_LIMITS
		analogLimitsSet();
#endif
		break;
	case 2:
		DI();
		inputKeys.state |= 0x0002;
		EI();
		analogFlags = ANALOG_SET1;
		analogCenterSet0();
		analogResetSet0();
#if ANALOG_LIMITS
		analogLimitsSet();
#endif
		break;
	case 3:
		DI();
		inputKeys.state |= 0x0002;
		EI();
		analogFlags = ANALOG_SET0 | ANALOG_SET1;
#if ANALOG_LIMITS
		analogLimitsSet();
#endif
		break;
	}
	ena_dsp();
}
#else
/* the sticks on, computing set 0 (mode 1), set 1 (2) or both (3); the set not computed
   centred */
void analogSetMode(unsigned char mode)
{
	dis_dsp();
	switch (mode) {
#if !FW_SINCE(0x00090206)
	case 0:
		inputKeys.state &= ~0x0002;
		analogInit();
		break;
	case 1:		/* the margin a sixth of the range */
		analogScale = 0x57;
		analogScaleShift = 9;
		inputKeys.state |= 0x0002;
		break;
	case 2:		/* a tenth */
		analogScale = 0x31;
		analogScaleShift = 9;
		inputKeys.state |= 0x0002;
		break;
#else
#if !FW_SINCE(0x0100010B)
	case 0:		/* analogStop's, before it */
		inputKeys.state &= ~0x0002;
		analogFlags = 0;
		analogInit();
		break;
#endif
	case 1:
		inputKeys.state |= 0x0002;
		analogFlags = ANALOG_SET0;
		analogCenterSet1();
		analogResetSet1();
		break;
	case 2:
		inputKeys.state |= 0x0002;
		analogFlags = ANALOG_SET1;
		analogCenterSet0();
		analogResetSet0();
		break;
	case 3:
		inputKeys.state |= 0x0002;
		analogFlags = ANALOG_SET0 | ANALOG_SET1;
		break;
#endif
	}
	ena_dsp();
}
#endif

#if BOARD_USS1001 || BOARD_DOL1001
/* the sticks off, P10.2 high (DOL-1001: their power off) */
void analogStop(void)
{
	DI();
	inputKeys.state &= ~0x0002;
	EI();
#if BOARD_DOL1001
	ANALOG_POWER = 0;
#else
	P10.2 = 1;
#endif
	analogFlags = 0;
	analogInit();
}
#else
#if FW_SINCE(0x0100010B)
void analogStop(void)
{
	inputKeys.state &= ~0x0002;
	analogFlags = 0;
	analogInit();
}
#endif
#endif

#if !BOARD_USS1001 && !BOARD_DOL1001
void analogStart80(void)
{
	inputKeys.state |= 0x0080;
	analogInput80Reset();
}
#endif

#if !BOARD_USS1001 && !BOARD_DOL1001
void analogStop80(void)
{
	inputKeys.state &= ~0x0080;
}
#endif

#if BOARD_USS1001
/* the slim's multi connector is analog input 0x0010 (analogStart10) */
void analogMultiCnStart(void)
{
}
#else
void analogMultiCnStart(void)
{
#if BOARD_DOL1001
	DI();
	inputKeys.state |= 0x0020;
	EI();
#else
	inputKeys.state |= 0x0020;
#endif
	analogMultiCnInit();
#if FW_SINCE(0x0100010B)
	adcOpenForBoard();
#endif
}
#endif

#if BOARD_USS1001
void analogMultiCnStop(void)
{
}

void analogStart10(void)
{
	DI();
	inputKeys.state |= 0x0010;
	EI();
	adcOpenForBoard();
}

void analogStop10(void)
{
	DI();
	inputKeys.state &= ~0x0010;
	EI();
	adcCloseForBoard();
}
#else
void analogMultiCnStop(void)
{
#if BOARD_DOL1001
	DI();
	inputKeys.state &= ~0x0020;
	EI();
#else
	inputKeys.state &= ~0x0020;
#endif
#if FW_SINCE(0x0100010B)
	adcCloseForBoard();
#endif
}
#endif

/* the multi connector's level (command 0x0103) */
unsigned char analogGetMultiCn(void)
{
#if BOARD_USS1001
	return analogInput10.reported;
#else
	return analogMultiCnReported;
#endif
}

/* each axis's last sample */
#if FW_SINCE(0x00090206)
void analogGetSamples(unsigned int *out)
#else
void analogGetSamples(unsigned int __far *out)
#endif
{
	unsigned char i;

	DI();
	i = analogSample;
	EI();
	i--;
	if (i >= ANALOG_SAMPLES)
		i = ANALOG_SAMPLES - 1;
	out[0] = analogAxes[0].samples[i];
	out[1] = analogAxes[1].samples[i];
	out[2] = analogAxes[2].samples[i];
	out[3] = analogAxes[3].samples[i];
}

#if SOURCE_SLIM
/* the analog sticks of each set, as host command 0x0100's read them: inputreq.c's on the other
   boards, here in Sony's link */
void controlsReadAnalog0(unsigned char *out)
{
	out[0] = analogSticks0[0];
	out[1] = analogSticks0[1];
	out[2] = analogSticks0[2];
	out[3] = analogSticks0[3];
}

void controlsReadAnalog1(unsigned char *out)
{
	out[0] = analogSticks1[0];
	out[1] = analogSticks1[1];
	out[2] = analogSticks1[2];
	out[3] = analogSticks1[3];
}

#if ANALOG_LIMITS
/* the slim's: each axis's record of its medians outside the limits (serial 0x0964, mode 1) */
void analogGetMedianFaults(unsigned char *out)
{
	memcpy(&out[0], &analogAxes[0].medianFault, sizeof(struct AnalogFault));
	memcpy(&out[8], &analogAxes[1].medianFault, sizeof(struct AnalogFault));
	memcpy(&out[0x10], &analogAxes[2].medianFault, sizeof(struct AnalogFault));
	memcpy(&out[0x18], &analogAxes[3].medianFault, sizeof(struct AnalogFault));
}

/* and of its samples (mode 0) */
void analogGetSampleFaults(unsigned char *out)
{
	memcpy(&out[0], &analogAxes[0].sampleFault, sizeof(struct AnalogFault));
	memcpy(&out[8], &analogAxes[1].sampleFault, sizeof(struct AnalogFault));
	memcpy(&out[0x10], &analogAxes[2].sampleFault, sizeof(struct AnalogFault));
	memcpy(&out[0x18], &analogAxes[3].sampleFault, sizeof(struct AnalogFault));
}
#endif
#endif

#if !BOARD_USS1001 && !BOARD_DOL1001
void analogLatchInit(void)
{
	analogLatch.value = 0;
	analogLatch.reported = 0xff;
	analogLatch.previous = 0xff;
	analogLatch.state = 0xff;
}
#endif

#if !BOARD_USS1001 && !BOARD_DOL1001
/* 0x00080504's controlsReadButtons only */
unsigned char analogLatchIsSet(void)
{
	if (analogLatch.reported == 1)
		return 1;
	return 0;
}
#endif

#if !BOARD_USS1001 && !BOARD_DOL1001
void analogLatchOn(void)
{
#if BOARD_102603
	inputKeys.state |= 1;
	analogLatchInit();
#endif
}

void analogLatchOff(void)
{
#if BOARD_102603
	inputKeys.state &= ~1;
#endif
}
#endif

