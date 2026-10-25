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

#include "adc.h"
#include "analog.h"
#include "chip.h"
#include "config.h"
#include "input.h"
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

/* the levels of an older table, never read */
const struct AnalogLevel analogLevels5[5] = {
	{ 0xdf, 0 }, { 0xae, 1 }, { 0x7d, 1 }, { 0x4b, 2 }, { 0x1a, 3 },
};

const struct AnalogLevel analogMultiCnLevels[7] = {
	{ 0xe3, 6 }, { 0xc4, 5 }, { 0x99, 4 }, { 0x69, 3 }, { 0x40, 2 }, { 0x1d, 1 }, { 0x00, 0 },
};

static __near struct AnalogAxis analogAxes[ANALOG_AXES];	/* ANI15 first */
static __near unsigned int analogWorkLow;	/* the margin's product, then the value's quotient */
static __near unsigned int analogWorkHigh;	/* and its divisor */
static __near unsigned char analogSample;	/* the samples[] the next conversions go to */
static __near unsigned char analogMinRange;	/* the high byte of a range that calibrates */
static __near unsigned int analogScales[ANALOG_SETS];	/* each set's margin, 1/1000 of the range */
static __near unsigned char analogMultiCnInput;	/* the high byte of ANI11's result */
static __near unsigned char analogMultiCnReserved;	/* cleared by analogMultiCnInit, otherwise
							   unused */
static __near unsigned char analogMultiCn;		/* the level of the last reading */
static __near unsigned char analogMultiCnPrevious;	/* of the one before */
static __near unsigned char analogMultiCnReported;	/* the level the host reads; 0xff: none */
__near unsigned char analogSticks0[ANALOG_AXES];
__near unsigned char analogSticks1[ANALOG_AXES];
static __near struct AnalogCalibration analogCalibration;
__near unsigned char analogFlags;

/* inputKeys.state's bit 0x0080 (analogStart80, never called): only reset */
static struct {
	unsigned char	input;
	unsigned char	previous;
	unsigned char	reported;
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
	analogFlags = 0;
	analogMinRange = sub_2C658()->analogMinRange;
	if (analogMinRange < 0x32)
		analogMinRange = 0x32;
	analogScales[0] = sub_2C658()->analogScale0;
	if (analogScales[0] > 400)
		analogScales[0] = 400;
	analogScales[1] = sub_2C658()->analogScale1;
	if (analogScales[1] > 400)
		analogScales[1] = 400;
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

void analogCenter(void)
{
	analogCenterSet0();
	analogCenterSet1();
}

/* every axis's calibration back to the settings' limits, not yet calibrated */
void analogCalibrationReset(void)
{
	unsigned int limit;

	limit = sub_2C658()->analogMin;
	analogCalibration.axis[0].min = limit;
	analogCalibration.axis[1].min = limit;
	analogCalibration.axis[2].min = limit;
	analogCalibration.axis[3].min = limit;
	limit = sub_2C658()->analogMax;
	analogCalibration.axis[0].max = limit;
	analogCalibration.axis[1].max = limit;
	analogCalibration.axis[2].max = limit;
	analogCalibration.axis[3].max = limit;
	analogAxes[0].calibrated = 0;
	analogAxes[1].calibrated = 0;
	analogAxes[2].calibrated = 0;
	analogAxes[3].calibrated = 0;
}

/* the settings' calibration */
void analogCalibrationDefault(void)
{
	analogCalibration = sub_2C658()->analogCalibration;
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
			range = analogCalibration.axis[i].max - analogCalibration.axis[i].min;
			if (analogCalibration.axis[i].min > sub_2C658()->analogMin ||
			    analogCalibration.axis[i].max < sub_2C658()->analogMax ||
			    sub_2C658()->analogMinRange > (unsigned char)(range >> 8)) {
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

void analogInput80Reset(void)
{
	analogSuspend = 0xff;
	analogInput80.previous = 0xff;
	analogInput80.reported = 0xff;
	analogInput80.input = 0;
}

void analogMultiCnInit(void)
{
	sleepFactorSet6(0);
	analogMultiCnReported = 0xff;
	analogMultiCn = 0xff;
	analogMultiCnPrevious = 0xff;
	analogMultiCnInput = 0;
	analogMultiCnReserved = 0;
}

/* converts the inputs (inputKeys' bits) asked: the sticks (0x0002, powered by P9.7), the multi
   connector (0x0020); 1 if the converter is busy */
unsigned char analogStart(unsigned int inputs)
{
	unsigned char mask;
	unsigned char skip;

	mask = 0;
	skip = 1;
	if (inputs & 0x0002) {
		mask |= 0xf0;
		P97 = 1;
		skip = 2;
	}
	if (inputs & 0x0020)
		mask |= 0x08;
	if (inputs & 0x0001)
		;	/* Sony's tests bit 0 and does nothing */
	if (adcStart(mask, skip))
		return 1;
	return 0;
}

/* the conversions of the inputs asked ended: the sticks' samples taken and scaled, the multi
   connector's level reported when two readings agree on a new one */
void analogEvent(unsigned int inputs)
{
	if ((inputs & 0x0002) && analogSuspend != 0xff && (inputKeys.state & 0x0002)) {
		P97 = 0;
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
		analogReport();
	}
	if ((inputs & 0x0020) && (inputKeys.state & 0x0020)) {
		analogMultiCnInput = adcResults[4] >> 8;
		analogMultiCn = analogLevelFind(analogMultiCnInput, analogMultiCnLevels,
						LENGTH(analogMultiCnLevels));
		if (analogMultiCn == analogMultiCnPrevious && analogMultiCn != analogMultiCnReported) {
			analogMultiCnReported = analogMultiCn;
			analogMultiCnStop();
			sleepFactorSet6(1);
			sleepFactorSet5(1);
			task2SetFlag(3, 0x0001);
		}
		analogMultiCnPrevious = analogMultiCn;
	}
	if (inputs & 0x0001)
		;	/* Sony's tests bit 0 and does nothing */
}

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
		if ((analogCalibration.axis[0].max >> 8) < (median >> 8))
			analogCalibration.axis[0].max = median;
		if ((median >> 8) < (analogCalibration.axis[0].min >> 8))
			analogCalibration.axis[0].min = median;
		if (analogCalibration.axis[0].min < analogCalibration.axis[0].max) {
			analogAxes[0].range = analogCalibration.axis[0].max -
					     analogCalibration.axis[0].min;
			if ((analogAxes[0].range >> 8) >= analogMinRange)
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
		analogWorkLow -= analogWorkLow >> 8;
		analogWorkHigh = analogAxes[0].output[0].high - analogAxes[0].output[0].low;
		analogWorkLow = analogWorkLow / (unsigned char)(analogWorkHigh >> 8);
		if (analogWorkLow >> 8)
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
		analogWorkLow -= analogWorkLow >> 8;
		analogWorkHigh = analogAxes[0].output[1].high - analogAxes[0].output[1].low;
		analogWorkLow = analogWorkLow / (unsigned char)(analogWorkHigh >> 8);
		if (analogWorkLow >> 8)
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
		if ((analogCalibration.axis[1].max >> 8) < (median >> 8))
			analogCalibration.axis[1].max = median;
		if ((median >> 8) < (analogCalibration.axis[1].min >> 8))
			analogCalibration.axis[1].min = median;
		if (analogCalibration.axis[1].min < analogCalibration.axis[1].max) {
			analogAxes[1].range = analogCalibration.axis[1].max -
					     analogCalibration.axis[1].min;
			if ((analogAxes[1].range >> 8) >= analogMinRange)
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
		analogWorkLow -= analogWorkLow >> 8;
		analogWorkHigh = analogAxes[1].output[0].high - analogAxes[1].output[0].low;
		analogWorkLow = analogWorkLow / (unsigned char)(analogWorkHigh >> 8);
		if (analogWorkLow >> 8)
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
		analogWorkLow -= analogWorkLow >> 8;
		analogWorkHigh = analogAxes[1].output[1].high - analogAxes[1].output[1].low;
		analogWorkLow = analogWorkLow / (unsigned char)(analogWorkHigh >> 8);
		if (analogWorkLow >> 8)
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
		if ((analogCalibration.axis[2].max >> 8) < (median >> 8))
			analogCalibration.axis[2].max = median;
		if ((median >> 8) < (analogCalibration.axis[2].min >> 8))
			analogCalibration.axis[2].min = median;
		if (analogCalibration.axis[2].min < analogCalibration.axis[2].max) {
			analogAxes[2].range = analogCalibration.axis[2].max -
					     analogCalibration.axis[2].min;
			if ((analogAxes[2].range >> 8) >= analogMinRange)
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
		analogWorkLow -= analogWorkLow >> 8;
		analogWorkHigh = analogAxes[2].output[0].high - analogAxes[2].output[0].low;
		analogWorkLow = analogWorkLow / (unsigned char)(analogWorkHigh >> 8);
		if (analogWorkLow >> 8)
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
		analogWorkLow -= analogWorkLow >> 8;
		analogWorkHigh = analogAxes[2].output[1].high - analogAxes[2].output[1].low;
		analogWorkLow = analogWorkLow / (unsigned char)(analogWorkHigh >> 8);
		if (analogWorkLow >> 8)
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
		if ((analogCalibration.axis[3].max >> 8) < (median >> 8))
			analogCalibration.axis[3].max = median;
		if ((median >> 8) < (analogCalibration.axis[3].min >> 8))
			analogCalibration.axis[3].min = median;
		if (analogCalibration.axis[3].min < analogCalibration.axis[3].max) {
			analogAxes[3].range = analogCalibration.axis[3].max -
					     analogCalibration.axis[3].min;
			if ((analogAxes[3].range >> 8) >= analogMinRange)
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
		analogWorkLow -= analogWorkLow >> 8;
		analogWorkHigh = analogAxes[3].output[0].high - analogAxes[3].output[0].low;
		analogWorkLow = analogWorkLow / (unsigned char)(analogWorkHigh >> 8);
		if (analogWorkLow >> 8)
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
		analogWorkLow -= analogWorkLow >> 8;
		analogWorkHigh = analogAxes[3].output[1].high - analogAxes[3].output[1].low;
		analogWorkLow = analogWorkLow / (unsigned char)(analogWorkHigh >> 8);
		if (analogWorkLow >> 8)
			analogAxes[3].output[1].value = 0xff;
		else
			analogAxes[3].output[1].value = analogWorkLow;
	}
}

/* the sets' values as reported: axis 1 inverted, axis 0, axis 3, axis 2 inverted */
void analogReport(void)
{
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
}

/* the median of three samples (never called) */
unsigned int analogMedian(unsigned int *samples)
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

/* the sticks on, computing set 0 (mode 1), set 1 (2) or both (3); the set not computed
   centred */
void analogSetMode(unsigned char mode)
{
	dis_dsp();
	switch (mode) {
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
	}
	ena_dsp();
}

void analogStop(void)
{
	inputKeys.state &= ~0x0002;
	analogFlags = 0;
	analogInit();
}

void analogStart80(void)
{
	inputKeys.state |= 0x0080;
	analogInput80Reset();
}

void analogStop80(void)
{
	inputKeys.state &= ~0x0080;
}

void analogMultiCnStart(void)
{
	inputKeys.state |= 0x0020;
	analogMultiCnInit();
	adcOpenForBoard();
}

void analogMultiCnStop(void)
{
	inputKeys.state &= ~0x0020;
	adcCloseForBoard();
}

/* the multi connector's level (command 0x0103) */
unsigned char analogGetMultiCn(void)
{
	return analogMultiCnReported;
}

/* each axis's last sample */
void analogGetSamples(unsigned int *out)
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

void analogLatchInit(void)
{
	analogLatch.value = 0;
	analogLatch.reported = 0xff;
	analogLatch.previous = 0xff;
	analogLatch.state = 0xff;
}

/* never called */
unsigned char analogLatchIsSet(void)
{
	if (analogLatch.reported == 1)
		return 1;
	return 0;
}

void analogLatchOn(void)
{
}

void analogLatchOff(void)
{
}
