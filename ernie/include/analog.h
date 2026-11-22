/*
 * The analog inputs (app/analog.c): the sticks' four axes on ANI15-ANI12 (adcResults[0-3])
 * and the multi connector's level on ANI11 (adcResults[4]). Each axis's median sample widens
 * its calibration and is scaled, in each set the mode selects, into analogSticks0 or
 * analogSticks1.
 */
#ifndef ERNIE_ANALOG_H
#define ERNIE_ANALOG_H

#include "board.h"

#define ANALOG_AXES		4
#define ANALOG_SAMPLES		3	/* an axis's median is of its last three */
#define ANALOG_SETS		2

/* the slim's limits of each axis's readings and its records of those outside them: not in
   0x01030505; the PS TV from 0x0104 (DOL-1002) */
#define ANALOG_LIMITS		(SOURCE_0511)
/* 0x0104: a median's fault keeps the samples it came from (struct AnalogSnapshot) */
#define ANALOG_SNAPSHOT		FW_SINCE(0x01040000)
/* the slim's 0x0105 (USS-1002): each set's positions checked against the limits too, with a
   record and the samples of those outside them */
#define ANALOG_OUTPUT_LIMITS	(BOARD_USS1001 && SOURCE_0104)
/* DOL-1002: the temperature from a thermistor on ANI10 (adcResults[5], inputKeys' bit 0x0040),
   in place of DOL-1001's sensor on the I2C bus (thermal.c) */
#define ANALOG_THERMISTOR	BOARD_DOL1002

#define ANALOG_SAMPLE_CENTER	0x8000
#define ANALOG_VALUE_CENTER	0x80

/* analogFlags, the mode: the sets computed */
#define ANALOG_SET0		0x01
#define ANALOG_SET1		0x02

/* the ends of an axis's travel the calibration has seen */
struct AnalogLimits {
	unsigned int	min;
	unsigned int	max;
};

struct AnalogCalibration {
	struct AnalogLimits	axis[ANALOG_AXES];
};

/* an axis in one set: its position held between low and high, as a value 0-0xff */
struct AnalogOutput {
	unsigned int	position;
	unsigned int	last;		/* the position the value was computed from */
	unsigned int	margin;		/* the calibration's range * the set's scale / 1000 */
	unsigned int	high;		/* the calibration's max less the margin */
	unsigned int	low;		/* its min plus the margin */
	unsigned char	value;
};

#if ANALOG_LIMITS
/* the slim's record of an axis's readings outside its limits: the lowest at or below limitMin,
   the highest at or above limitMax, the last with its low six bits set, and how many were below
   and above (up to 0xff) */
struct AnalogFault {
	unsigned int	low;
	unsigned int	high;
	unsigned int	noise;
	unsigned char	below;
	unsigned char	above;
};

#if ANALOG_SNAPSHOT
/* 0x0104's: the samples a median outside the limits was taken from, when it was the lowest or
   the highest yet, and 0xffff less their sum */
struct AnalogSnapshot {
	unsigned int	samples[ANALOG_SAMPLES];
	unsigned int	check;
};
#endif

#if ANALOG_OUTPUT_LIMITS
/* the slim's 0x0105: a set's positions outside the axis's limits, and the samples of the lowest
   or the highest */
struct AnalogOutputFault {
	struct AnalogFault	fault;
	struct AnalogSnapshot	snapshot;
};
#endif

struct AnalogAxis {
	unsigned int		samples[ANALOG_SAMPLES];
	unsigned int		median;
	unsigned int		range;		/* the calibration's max - min */
	unsigned int		limitMin;	/* the readings taken: above limitMin, below */
	unsigned int		limitMax;	/* limitMax (analogLimitsSet) */
	unsigned char		calibrated;	/* range's high byte reached analogMinRange */
	struct AnalogOutput	output[ANALOG_SETS];
	struct AnalogFault	sampleFault;	/* the samples outside the limits */
	struct AnalogFault	medianFault;	/* and the medians */
#if ANALOG_SNAPSHOT
	struct AnalogSnapshot	medianSnapshot;	/* the samples of medianFault's low or high */
#endif
#if ANALOG_OUTPUT_LIMITS
	struct AnalogOutputFault outputFault[ANALOG_SETS];
#endif
};
#elif defined(ERNIE_UPDATER) || FW_SINCE(0x00090203)
struct AnalogAxis {
	unsigned int		samples[ANALOG_SAMPLES];
	unsigned int		median;
	unsigned int		range;		/* the calibration's max - min */
	unsigned char		calibrated;	/* range's high byte reached analogMinRange */
	struct AnalogOutput	output[ANALOG_SETS];
};
#else
/* 0x00090009: one set, its position held between low and high, as a value 0-0xff */
struct AnalogAxis {
	unsigned int	samples[ANALOG_SAMPLES];
	unsigned int	median;
	unsigned char	value;
	unsigned char	calibrated;	/* range's high byte reached 0x64 */
	unsigned int	position;
	unsigned int	last;		/* the position the value was computed from */
	unsigned int	range;		/* the calibration's max - min */
	unsigned int	margin;		/* range * analogScale >> analogScaleShift */
	unsigned int	high;		/* the calibration's max less the margin */
	unsigned int	low;		/* its min plus the margin */
};
#endif

/* a level of the multi connector and the lowest input that reads as it */
struct AnalogLevel {
	unsigned char	limit;
	unsigned char	level;
};

extern __near unsigned char analogSticks0[ANALOG_AXES];	/* set 0's values, as reported */
extern __near unsigned char analogSticks1[ANALOG_AXES];	/* set 1's */
extern __near unsigned char analogFlags;		/* ANALOG_SET0, ANALOG_SET1 */

extern void analogInit(void);
extern void analogCenterSet0(void);
extern void analogResetSet0(void);
extern void analogCenterSet1(void);
extern void analogResetSet1(void);
extern void analogCenter(void);
extern void analogCalibrationReset(void);
extern void analogCalibrationDefault(void);
extern int analogStoreRead(unsigned char *data, unsigned char command,
			   void (*done)(unsigned char command, unsigned char result));
extern int analogStoreWrite(unsigned char *data, unsigned char command,
			    void (*done)(unsigned char command, unsigned char result));
extern void analogCalibrationLoad(void);
extern void analogCalibrationSave(void);
extern void analogCalibrationGet(unsigned char *out);
extern void analogInput80Reset(void);
extern void analogMultiCnInit(void);
extern unsigned char analogStart(unsigned int inputs);
extern void analogEvent(unsigned int inputs);
extern void analogLatchUpdate(unsigned char value);
extern void analogFilter0(void);
extern void analogFilter1(void);
extern void analogFilter2(void);
extern void analogFilter3(void);
extern void analogReport(void);
#if defined(ERNIE_UPDATER) || FW_SINCE(0x00090203)
extern unsigned int analogMedian(unsigned int *samples);
#else
extern unsigned int analogMedian(unsigned int __far *samples);
#endif
extern unsigned char analogLevelFind(unsigned char input, const struct AnalogLevel *levels,
				     unsigned char count);
extern void analogSetMode(unsigned char mode);
extern void analogStop(void);
extern void analogStart80(void);
extern void analogStop80(void);
extern void analogMultiCnStart(void);
extern void analogMultiCnStop(void);
extern unsigned char analogGetMultiCn(void);
#if defined(ERNIE_UPDATER) || FW_SINCE(0x00090203)
extern void analogGetSamples(unsigned int *out);
#else
extern void analogGetSamples(unsigned int __far *out);
#endif
#if ANALOG_THERMISTOR
extern void analogThermistorInit(void);
extern void analogThermistorStart(void);
extern void analogThermistorStop(void);
extern int analogThermistorRead(signed char *temperature);
extern void analogThermistorGet(unsigned char *out);
#endif
/* the slim's analog input 0x0010 (inputKeys' bit) */
#if ANALOG_LIMITS
extern void analogLimitsSet(void);
extern void analogLimitsGet(unsigned int *out);
extern void analogGetMedianFaults(unsigned char *out);
extern void analogGetSampleFaults(unsigned char *out);
#if ANALOG_SNAPSHOT
extern void analogGetMedianSnapshots(unsigned char *out);
#endif
#if ANALOG_OUTPUT_LIMITS
extern void analogGetSet0Faults(unsigned char *out);
extern void analogGetSet0Snapshots(unsigned char *out);
extern void analogGetSet1Faults(unsigned char *out);
extern void analogGetSet1Snapshots(unsigned char *out);
#endif
#endif
#if BOARD_USS1001 && SOURCE_0104	/* input.h's INPUT_MEASURE */
/* analogGetMeasure's: input 0x0080's averages and their RTC ticks */
struct AnalogMeasure {
	unsigned int	ani7;
	unsigned int	ani8;
	unsigned int	ani9;
	unsigned int	ticks;
};

extern void analogGetMeasure(struct AnalogMeasure *out);
#endif
extern void analogInput10Init(void);
extern void analogStart10(void);
extern void analogStop10(void);
extern void analogAxesReset(void);
extern void analogLatchInit(void);
extern unsigned char analogLatchIsSet(void);
extern void analogLatchOn(void);
extern void analogLatchOff(void);

#if (defined(ERNIE_UPDATER)) && BOARD_USS1001 && SOURCE_0104
/* the slim's 0x0105 updater (updater/analog.c): the battery's measures, polled */
extern void analogMeasureStep(void);
extern void analogMeasureNow(void);
#endif

#endif
