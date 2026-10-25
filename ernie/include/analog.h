/*
 * The analog inputs (app/analog.c): the sticks' four axes on ANI15-ANI12 (adcResults[0-3])
 * and the multi connector's level on ANI11 (adcResults[4]). Each axis's median sample widens
 * its calibration and is scaled, in each set the mode selects, into analogSticks0 or
 * analogSticks1.
 */
#ifndef ERNIE_ANALOG_H
#define ERNIE_ANALOG_H

#define ANALOG_AXES		4
#define ANALOG_SAMPLES		3	/* an axis's median is of its last three */
#define ANALOG_SETS		2

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

struct AnalogAxis {
	unsigned int		samples[ANALOG_SAMPLES];
	unsigned int		median;
	unsigned int		range;		/* the calibration's max - min */
	unsigned char		calibrated;	/* range's high byte reached analogMinRange */
	struct AnalogOutput	output[ANALOG_SETS];
};

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
extern unsigned int analogMedian(unsigned int *samples);
extern unsigned char analogLevelFind(unsigned char input, const struct AnalogLevel *levels,
				     unsigned char count);
extern void analogSetMode(unsigned char mode);
extern void analogStop(void);
extern void analogStart80(void);
extern void analogStop80(void);
extern void analogMultiCnStart(void);
extern void analogMultiCnStop(void);
extern unsigned char analogGetMultiCn(void);
extern void analogGetSamples(unsigned int *out);
extern void analogLatchInit(void);
extern unsigned char analogLatchIsSet(void);
extern void analogLatchOn(void);
extern void analogLatchOff(void);

#endif
