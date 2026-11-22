/*
 * The battery: its gauge's registers over the I2C bus, while one answered at the power-on
 * (deviceVersions.battery.id not BATTERY_NONE), and its bootloader's commands.
 */
#pragma sfr
#pragma NOP

#include <stddef.h>
#include <string.h>

#include "ernie.h"
#include "board.h"
#include "battery.h"
#include "charge.h"
#include "chip.h"
#include "config.h"
#include "cookie.h"
#include "eventlog.h"
#include "iica.h"
#include "kernel.h"
#include "log.h"
#include "powerseq.h"
#include "powerwait.h"

#if SOURCE_0103
/* the slim's: the id, the type twice (the second word from 0x0416 on: 0xa8 for a 0x0802
   battery of type 0xa7 or 0xa8), the version, then the bootloader's (0xfffd before 0x0416); with
   none answered all 0xffff */
/* the words as Sony's sets them here: a struct's, not the array's. The gauge's control
   commands 1 (the device's type), 3 (the hardware's version), 2 (the firmware's) and 6; the
   data flash's class 0x39 */
struct BatteryInfoWords {
	unsigned int	id;		/* command 1 */
	unsigned int	type;		/* the hardware's, or 0xa8 */
	unsigned int	firmware;	/* command 2 */
	unsigned int	dataFlash;	/* class 0x39's first word */
	unsigned int	command6;	/* from firmware 0x0416 */
	unsigned int	hardware;	/* command 3 */
};
#define BATTERY_INFO(info)	((struct BatteryInfoWords *)(info))
#endif

#if BATTERY_GAUGE
#include <math.h>

#include "analog.h"
#include "input.h"
#include "nvsreq.h"
#include "sleepfactor.h"
#include "state.h"
#include "timer.h"
#ifdef ERNIE_UPDATER
#include "nvs.h"
#endif

/* the gauge's record in the NVS */
#define BATTERY_RECORD_OFFSET	0xbc

/* the ANI7 and ANI8 an offset takes: the offsets calibrated at 0xcdc0 and 0x8000 */
#define BATTERY_VOLT_ZERO	0xcdc0
#define BATTERY_VOLT_MIN	0xc8a6UL	/* above */
#define BATTERY_VOLT_MAX	0xd2d9UL	/* to */
#define BATTERY_CURRENT_ZERO	0x8000
#define BATTERY_CURRENT_MIN	0x4deeL		/* from */
#define BATTERY_CURRENT_MAX	0xb213L		/* below */

/* a curve's point: a temperature (0.1 K, from the highest down to 0) and its value */
struct BatteryPoint {
	unsigned int	temp;
	unsigned int	value;
};

/* where a charge of the band ends: at least the voltage, the current under the current */
struct BatteryFull {
	unsigned int	volt;
	unsigned int	current;
};

struct AnalogMeasure batteryMeasure;
struct BatteryGauge batteryGauge;
union BatteryFaults batteryFaults;
static unsigned char batterySaveResult;
static unsigned char batteryLoadResult;

static const struct BatteryFull batteryFull[3] = {
	{ 3900, 200 }, { 4000, 200 }, { 4100, 200 }
};

/* the charge (mAh) left when a charge ends, by the band */
static const struct BatteryPoint batteryTaper0[] = {
	{ 0xffff, 581 }, { 3182, 581 }, { 0, 581 }
};
static const struct BatteryPoint batteryTaper1[] = {
	{ 0xffff, 254 }, { 3182, 254 }, { 3082, 259 }, { 2982, 263 }, { 2882, 312 },
	{ 2782, 398 }, { 2682, 626 }, { 2582, 1462 }, { 0, 1462 }
};
static const struct BatteryPoint batteryTaper2[] = {
	{ 0xffff, 15 }, { 3182, 15 }, { 3082, 17 }, { 2982, 19 }, { 2882, 73 },
	{ 2782, 157 }, { 2682, 297 }, { 2582, 683 }, { 0, 683 }
};
static const struct BatteryPoint * const batteryTapers[3] = {
	batteryTaper0, batteryTaper1, batteryTaper2
};

/* 0.01 % of the capacity usable at the temperature */
static const struct BatteryPoint batteryTempFactors[] = {
	{ 0xffff, 9700 }, { 3182, 9700 }, { 3082, 9690 }, { 2982, 9680 }, { 2882, 9520 },
	{ 2782, 9370 }, { 2682, 8890 }, { 2582, 6910 }, { 0, 6910 }
};

/* the resistance (mohm) at the temperature */
static const struct BatteryPoint batteryResistances[] = {
	{ 0xffff, 96 }, { 3182, 96 }, { 3082, 98 }, { 2982, 100 }, { 2882, 116 },
	{ 2782, 210 }, { 2682, 325 }, { 2582, 570 }, { 0, 570 }
};

/* the open circuit voltage (mV) at each percent: under load, then at rest */
static const unsigned int batteryOcvLoad[101] = {
	3000, 3294, 3418, 3499, 3555, 3594, 3622, 3642, 3658, 3669,
	3678, 3687, 3694, 3700, 3707, 3713, 3718, 3722, 3727, 3731,
	3736, 3740, 3744, 3747, 3751, 3754, 3757, 3761, 3764, 3767,
	3770, 3773, 3775, 3778, 3781, 3784, 3787, 3790, 3793, 3796,
	3799, 3802, 3805, 3808, 3812, 3816, 3820, 3823, 3827, 3831,
	3835, 3839, 3844, 3848, 3853, 3857, 3862, 3867, 3871, 3876,
	3882, 3887, 3892, 3898, 3904, 3909, 3915, 3921, 3928, 3935,
	3941, 3948, 3955, 3962, 3969, 3976, 3984, 3991, 3999, 4007,
	4015, 4023, 4031, 4040, 4048, 4057, 4066, 4075, 4084, 4093,
	4102, 4111, 4120, 4129, 4138, 4147, 4156, 4165, 4174, 4183,
	4192
};
static const unsigned int batteryOcvRest[101] = {
	3000, 3281, 3403, 3484, 3544, 3593, 3631, 3656, 3666, 3671,
	3674, 3677, 3680, 3683, 3691, 3698, 3704, 3709, 3714, 3720,
	3725, 3729, 3733, 3736, 3739, 3742, 3747, 3751, 3755, 3759,
	3763, 3767, 3770, 3772, 3774, 3776, 3778, 3780, 3782, 3784,
	3786, 3788, 3791, 3793, 3796, 3798, 3801, 3804, 3807, 3810,
	3813, 3817, 3821, 3825, 3830, 3835, 3840, 3846, 3853, 3859,
	3867, 3874, 3881, 3887, 3893, 3899, 3905, 3911, 3917, 3923,
	3929, 3935, 3941, 3947, 3954, 3960, 3966, 3973, 3979, 3986,
	3993, 4000, 4007, 4014, 4021, 4029, 4037, 4045, 4053, 4061,
	4070, 4078, 4087, 4096, 4105, 4114, 4123, 4132, 4141, 4150,
	4159
};

#ifndef ERNIE_UPDATER	/* the updater's powerSeqOn loads it (powerseq.c) */
static unsigned char batteryLoadRecord(void);
#endif
static void batteryRecordSaved(unsigned char command, unsigned char result);
static void batteryRecordLoaded(unsigned char command, unsigned char result);
static unsigned int batteryVolt(unsigned int raw);
static int batteryCurrent(unsigned int raw);
static unsigned int batteryTemperature(unsigned int raw);
static unsigned char batteryBand(void);
static void batteryCount(void);
static void batteryChargeFromRest(void);
static void batteryEstimate(void);
static void batteryCapacityUpdate(void);
static void batterySocVoltUpdate(void);
static void batterySocChargeUpdate(void);
static void batterySocUpdate(void);
static void batteryShownReset(void);
static void batteryShownUpdate(void);
static void batterySetFull(void);
static void batteryRemainUpdate(void);
static void batteryRemainCapUpdate(void);
static void batterySetCharge(int mah);
static void batteryTteUpdate(void);
static void batteryCycleUpdate(void);
static void batterySohUpdate(void);
static unsigned int batteryOcvPercent(unsigned int volt, const unsigned int *curve);
static unsigned int batteryCurve(unsigned int temp, const struct BatteryPoint *curve);

void batteryInit(void)
{
	memset(&batteryMeasure, 0, sizeof(batteryMeasure));
	memset(&batteryGauge, 0, sizeof(batteryGauge));
	memset(batteryFaults.byte, 0, sizeof(batteryFaults));
	timerGetRtcTicks(&batteryMeasure.ticks);
	return;
}

#ifdef ERNIE_UPDATER
/* the updater's power-on: the record and the offsets, the averages measured at once with the
   charge stopped, the first update, then register 0xaa bit 15 off */
void batteryPowerOn(void)
{
	batteryLoadRecord();
	batteryLoadOffsets();
	cookieUpdateBatteryCharge(0);
	powerWaitLong(13);
	analogMeasureNow();
	batteryUpdate();
	P14.4 = 1;
	powerRegAA15Off();
}
#else
/* at the power-on: the record and the offsets, one measure with the charge stopped, the first
   update, then register 0xaa bit 15 off */
void batteryPowerOn(void)
{
	ER ercd;
	FLGPTN ptn;
	unsigned char info[4];

	batteryLoadRecord();
	batteryLoadOffsets();
	cookieUpdateBatteryCharge(0);
	dly_tsk(13);
	clr_flg(4, ~0x40);
	inputMeasure.ask1 = 1;
	ercd = twai_flg(4, 0x40, TWF_ORW, &ptn, 250);
	if (ercd == E_OK) {
		if (ptn & 0x40)
			clr_flg(4, ~0x40);
		batteryUpdate();
		sleepFactorGetOffInfo(info);
		switch (info[0]) {
		case 0:
		case 0x20:
			break;
		default:
			dly_tsk(0x175);
		}
		P14.4 = 1;
		powerRegAA15Off();
	}
}

/* the offsets calibrated at the zero current (mode 0 both, 1 the voltage's, 2 the current's),
   answered in offsets[0] and [1]: 0x62 the voltage, 0x63 the current out of range, 0x64 no
   measure, 0x65 not in state 4 */
unsigned char batteryCalibrate(unsigned char mode, struct BatteryOffsets *offsets)
{
	ER ercd;
	FLGPTN ptn;
	unsigned char result = 0;

	if (stateGet() != 4)
		return 0x65;
	switch (mode) {
	case 0:
	case 1:
	case 2:
		break;
	default:
		return 0x33;
	}
	cookieUpdateBatteryCharge(0);
	dly_tsk(13);
	inputMeasure.only5 = 0;
	clr_flg(4, ~0x40);
	inputMeasure.ask3 = 1;
	inputScanMode1(1);
	ercd = twai_flg(4, 0x40, TWF_ORW, &ptn, 250);
	if (ercd == E_OK) {
		if (ptn & 0x40)
			clr_flg(4, ~0x40);
	} else {
		result = 0x64;
		goto done;
	}
	analogGetMeasure(&batteryMeasure);
	if (mode == 0 || mode == 1) {
		if (batteryMeasure.ani7 > BATTERY_VOLT_MIN && batteryMeasure.ani7 <= BATTERY_VOLT_MAX) {
			batteryGauge.voltOffset = BATTERY_VOLT_ZERO - batteryMeasure.ani7;
			batteryGauge.flags.bits.voltOffsetValid = 1;
			offsets->volt = batteryGauge.voltOffset;
		} else {
			result = 0x62;
			goto done;
		}
	}
	if (mode == 0 || mode == 2) {
		if (batteryMeasure.ani8 >= BATTERY_CURRENT_MIN &&
		    batteryMeasure.ani8 < BATTERY_CURRENT_MAX) {
			batteryGauge.currentOffset = BATTERY_CURRENT_ZERO - batteryMeasure.ani8;
			batteryGauge.flags.bits.currentOffsetValid = 1;
			offsets->current = batteryGauge.currentOffset;
		} else {
			result = 0x63;
			goto done;
		}
	}
	batteryGauge.flags.bits.estimated = 0;
	batteryUpdate();
	chargeUpdate();
done:
	inputScanMode2(1);
	inputMeasure.only5 = 1;
	return result;
}
#endif

/* the settings' offsets (0x7fff none), each used where it is in range */
void batteryLoadOffsets(void)
{
	batteryGauge.voltOffset = batteryGauge.voltOffsetSaved = configGet()->batteryCalibration0;
	batteryGauge.currentOffset = batteryGauge.currentOffsetSaved =
		configGet()->batteryCalibration1;
	if (batteryGauge.voltOffsetSaved == 0x7fff)
		batteryGauge.voltOffset = 0;
	if (batteryGauge.currentOffsetSaved == 0x7fff)
		batteryGauge.currentOffset = 0;
	if ((unsigned int)(batteryGauge.voltOffset + BATTERY_VOLT_ZERO) > BATTERY_VOLT_MIN &&
	    (unsigned int)(batteryGauge.voltOffset + BATTERY_VOLT_ZERO) <= BATTERY_VOLT_MAX) {
		batteryGauge.flags.bits.voltOffsetValid = 1;
	} else {
		batteryGauge.voltOffset = 0;
		batteryGauge.flags.bits.voltOffsetValid = 0;
	}
	if ((unsigned int)(batteryGauge.currentOffset + BATTERY_CURRENT_ZERO) >= BATTERY_CURRENT_MIN &&
	    (unsigned int)(batteryGauge.currentOffset + BATTERY_CURRENT_ZERO) < BATTERY_CURRENT_MAX) {
		batteryGauge.flags.bits.currentOffsetValid = 1;
	} else {
		batteryGauge.currentOffset = 0;
		batteryGauge.flags.bits.currentOffsetValid = 0;
	}
	batteryGauge.fullCap = 2320;
}

/* at the power-off: the record saved, register 0xaa bit 15 on while the estimate runs */
void batteryPowerOff(void)
{
	batterySaveRecord();
	if (batteryGauge.flags.bits.estimated)
		powerRegAA15On();
	else
		powerRegAA15Off();
}

/* a measure: the voltage, the current, the temperature, then the estimate */
void batteryUpdate(void)
{
	unsigned int ticks;

	ticks = batteryMeasure.ticks;
	if (batteryGauge.flags.bits.hibernated)
		return;
	analogGetMeasure(&batteryMeasure);
	batteryGauge.interval = (batteryMeasure.ticks - ticks) >> 1;
	batteryGauge.volt = batteryVolt(batteryMeasure.ani7);
	batteryGauge.current = batteryCurrent(batteryMeasure.ani8);
	batteryGauge.bicTemp = batteryGauge.temp = batteryTemperature(batteryMeasure.ani9);
	batteryGauge.band = batteryBand();
	batteryCapacityUpdate();
	batteryCount();
	batteryEstimate();
	batteryCycleUpdate();
	batterySohUpdate();
	batteryInfoRead();
}

#ifdef ERNIE_UPDATER
/* the updater's: written directly */
unsigned char batterySaveRecord(void)
{
	unsigned char result;

	result = nvsWrite(BATTERY_RECORD_OFFSET, sizeof(batteryGauge.record),
			  (unsigned char *)&batteryGauge.record, 1);
	return result;
}
#else
unsigned char batterySaveRecord(void)
{
	unsigned char result;

	result = nvsJobWrite(BATTERY_RECORD_OFFSET, sizeof(batteryGauge.record),
			     (unsigned char *)&batteryGauge.record, 1, 0, batteryRecordSaved);
	if (result != 0)
		return result;
	return batterySaveResult;
}
#endif

static void batteryRecordSaved(unsigned char command, unsigned char result)
{
	batterySaveResult = result;
}

#ifdef ERNIE_UPDATER
/* the updater's: read directly */
unsigned char batteryLoadRecord(void)
{
	unsigned char result;

	result = nvsRead(BATTERY_RECORD_OFFSET, sizeof(batteryGauge.record),
			 (unsigned char *)&batteryGauge.record, 1);
	return result;
}
#else
static unsigned char batteryLoadRecord(void)
{
	unsigned char result;

	result = nvsJobRead(BATTERY_RECORD_OFFSET, sizeof(batteryGauge.record),
			    (unsigned char *)&batteryGauge.record, 1, 0, batteryRecordLoaded);
	if (result != 0)
		return result;
	return batteryLoadResult;
}
#endif

unsigned char batteryIsCharging(void)
{
	return batteryGauge.flags.bits.charging;
}

unsigned char batteryOffsetsValid(void)
{
	return batteryGauge.flags.bits.voltOffsetValid & batteryGauge.flags.bits.currentOffsetValid;
}

/* a blank record (0xffff) starts at 0 */
static void batteryRecordLoaded(unsigned char command, unsigned char result)
{
	batteryLoadResult = result;
	if (batteryGauge.record.cycleCount == 0xffff) {
		batteryGauge.record.cycleCount = 0;
		batteryGauge.record.percent = 0;
	}
}

/* ANI7 to mV: 0xffc7 is 5010 mV */
static unsigned int batteryVolt(unsigned int raw)
{
	unsigned int volt;
	long value;

	value = (long)raw + batteryGauge.voltOffset;
	if (value >= 0xffc8L)
		value = 0xffc7L;
	else if (value < 0)
		value = 0;
	volt = 5010 * value / 0xffc7L;
	return volt;
}

/* ANI8 to mA: 0 is -2556 mA, 0xffff 2556 */
static int batteryCurrent(unsigned int raw)
{
	int current;
	long value;

	value = (long)raw + batteryGauge.currentOffset;
	if (value >= 0x10000L)
		value = 0xffffL;
	else if (value < 0)
		value = 0;
	current = 2556 - 5112 * value / 0xffffL;
	return current;
}

/* ANI9 to 0.1 K: the thermistor (10 kohm at 25 C, B 3380) against 10 kohm, 0xffff out of
   range */
static unsigned int batteryTemperature(unsigned int raw)
{
	unsigned int temp;
	float resistance = 0;
	float inverse = 0;
	float kelvin;

	if (raw >= 0xf5c0 || raw < 0xdc1) {
		temp = 0xffff;
	} else if (0xffc0 - raw != 0) {
		resistance = 10000UL * raw / (0xffc0 - raw);
		if (resistance > 0)
			inverse = log(resistance / 10000.0f) / 3380.0f + 1.0f / 298.15f;
		if (inverse > 0) {
			kelvin = 1.0f / inverse;
			temp = 10.0f * kelvin + 0.5f;
			if (temp < 2332 || temp >= 3983)
				temp = 0xffff;
		} else {
			temp = 0xffff;
		}
	} else {
		temp = 0xffff;
	}
	return temp;
}

/* the charge current's band from the Cookie's register 8 */
static unsigned char batteryBand(void)
{
	unsigned char band = 0;

	switch (cookieRegs[5].byte[0] & 0xe0) {
	case 0x00:
		band = 2;
		break;
	case 0x40:
		band = 1;
		break;
	case 0x80:
	default:
		band = 0;
	}
	return band;
}

#ifndef ERNIE_UPDATER	/* the updater's: no resume, no calibration */
/* at the resume: the rest ends (woken 0: it starts) */
void batteryResume(int woken)
{
	batteryGauge.idle = 0;
	if (woken != 0) {
		batteryGauge.rest = 0;
	} else {
		batteryGauge.flags.bits.resting = 1;
		batteryGauge.flags.bits.resumed = 0;
	}
}

/* a state other than the resume entered (powerSequence): the rest ends at the next current */
void batteryStateDone(void)
{
	batteryGauge.flags.bits.resumed = 1;
}

void batteryGetOffsets(struct BatteryOffsets *offsets)
{
	offsets->volt = batteryGauge.voltOffset;
	offsets->current = batteryGauge.currentOffset;
}

#endif

/* 0x0904's: the offsets used and the settings' */
void batteryGetCalibration(struct BatteryCalibration *calibration)
{
	calibration->volt = batteryGauge.voltOffset;
	calibration->voltSaved = configGet()->batteryCalibration0;
	calibration->current = batteryGauge.currentOffset;
	calibration->currentSaved = configGet()->batteryCalibration1;
}

void batteryClearFull(void)
{
	batteryGauge.flags.bits.currentReached = 0;
	batteryGauge.flags.bits.fullPending = 0;
	batteryGauge.flags.bits.full = 0;
}

/* the charge counted while a current flows, the idle and rest times while none does */
static void batteryCount(void)
{
	if (batteryGauge.current >= 40 || batteryGauge.current <= -40) {
		batteryGauge.flags.bits.flowing = 1;
		batteryGauge.charge += (long)batteryGauge.current * batteryGauge.interval;
		batteryGauge.chargeMah = batteryGauge.charge / 3600;
		if (batteryGauge.chargeMah > 0)
			batterySetCharge(0);
		if (batteryGauge.chargeMah < -(int)batteryGauge.usable)
			batterySetCharge(-(int)batteryGauge.usable);
		batteryGauge.idle = 0;
		if (batteryGauge.flags.bits.resting)
			batteryGauge.rest += batteryGauge.interval;
		else
			batteryGauge.rest = 0;
	} else {
		batteryGauge.flags.bits.flowing = 0;
		if (batteryGauge.flags.bits.resumed) {
			batteryGauge.flags.bits.resting = 0;
			batteryGauge.flags.bits.resumed = 0;
		}
		batteryGauge.idle += batteryGauge.interval;
		batteryGauge.rest += batteryGauge.interval;
	}
	batteryGauge.flags.bits.idle10 = batteryGauge.idle >= 600 ? 1 : 0;
	batteryGauge.flags.bits.rest60 = batteryGauge.rest >= 3600 ? 1 : 0;
}

/* the charge from the voltage at rest */
static void batteryChargeFromRest(void)
{
	batteryGauge.socVolt = batteryOcvPercent(batteryGauge.volt, batteryOcvRest);
	batterySetCharge(-(int)((unsigned long)batteryGauge.capacity * (10000 - batteryGauge.socVolt) /
				10000));
}

#ifndef ERNIE_UPDATER
/* the charge from the open circuit's voltage */
void batteryChargeFromLoad(void)
{
	batteryGauge.socVolt = batteryOcvPercent(batteryGauge.ocv, batteryOcvRest);
	batterySetCharge(-(int)((unsigned long)batteryGauge.capacity * (10000 - batteryGauge.socVolt) /
				10000));
}
#endif

static void batteryEstimate(void)
{
	timerGetRtcTicks(&batteryGauge.updateTicks);
	if (!batteryGauge.flags.bits.estimated) {
		batteryChargeFromRest();
		batterySocChargeUpdate();
		batteryShownReset();
		batteryGauge.flags.bits.estimated = 1;
	} else if (batteryGauge.flags.bits.idle10 && batteryGauge.flags.bits.rest60) {
		batteryChargeFromRest();
		batterySocChargeUpdate();
		batteryShownReset();
		batteryGauge.idle = 0;
		batteryGauge.rest = 0;
		batteryGauge.flags.bits.idle10 = 0;
		batteryGauge.flags.bits.rest60 = 0;
	} else {
		batterySocVoltUpdate();
		batterySocChargeUpdate();
		batterySocUpdate();
		if (batteryGauge.current >= 40) {
			batteryGauge.flags.bits.charging = 1;
			if (!batteryGauge.flags.bits.full &&
			    batteryGauge.chargeMah > batteryGauge.chargeFull)
				batterySetCharge(batteryGauge.chargeFull);
			batteryShownReset();
			if (batteryGauge.flags.bits.fullPending &&
			    batteryGauge.updateTicks - batteryGauge.fullTicks >= 20 &&
			    !batteryGauge.flags.bits.full) {
				if (chargeUsbPower() || stateGet() != 0x20)
					batterySetFull();
				batteryGauge.flags.bits.full = 1;
			}
			if (batteryGauge.current >= batteryFull[batteryGauge.band].current)
				batteryGauge.flags.bits.currentReached = 1;
			if (batteryGauge.current < batteryFull[batteryGauge.band].current &&
			    batteryGauge.volt >= batteryFull[batteryGauge.band].volt &&
			    batteryGauge.flags.bits.currentReached) {
				if (!batteryGauge.flags.bits.fullPending) {
					batteryGauge.flags.bits.fullPending = 1;
					timerGetRtcTicks(&batteryGauge.fullTicks);
				}
			} else {
				batteryGauge.flags.bits.fullPending = 0;
			}
		} else {
			batteryGauge.flags.bits.charging = 0;
			batteryClearFull();
			batteryShownUpdate();
		}
	}
	batteryRemainUpdate();
	batteryRemainCapUpdate();
	batteryTteUpdate();
}

/* the capacity less the cycles' fade, at the temperature, and where a charge ends */
static void batteryCapacityUpdate(void)
{
	batteryGauge.fade = batteryGauge.record.cycleCount / 50 * 21;
	if (batteryGauge.fade + 1160 < batteryGauge.fullCap)
		batteryGauge.capacity = batteryGauge.fullCap - batteryGauge.fade;
	else
		batteryGauge.capacity = 1160;
	batteryGauge.resistanceAge = batteryGauge.record.cycleCount / 50 * 3 + 100;
	if (batteryGauge.resistanceAge >= 201)
		batteryGauge.resistanceAge = 200;
	batteryGauge.tempFactor = batteryCurve(batteryGauge.temp, batteryTempFactors);
	batteryGauge.fullCap2 = batteryGauge.usable =
		(unsigned long)batteryGauge.capacity * batteryGauge.tempFactor / 10000;
	batteryGauge.chargeFull = -(int)batteryCurve(batteryGauge.temp,
						      batteryTapers[batteryGauge.band]);
}

/* the state of charge from the open circuit's voltage, above the empty voltage */
static void batterySocVoltUpdate(void)
{
	batteryGauge.resistanceBase = batteryCurve(batteryGauge.temp, batteryResistances);
	batteryGauge.resistance = (unsigned long)180 * batteryGauge.resistanceBase *
				  batteryGauge.resistanceAge / 100000;
	batteryGauge.irDrop = (long)batteryGauge.resistance * -batteryGauge.current / 100;
	batteryGauge.ocv = batteryGauge.volt + batteryGauge.irDrop;
	batteryGauge.socOcv = batteryOcvPercent(batteryGauge.ocv, batteryOcvLoad);
	batteryGauge.ocvEmpty = batteryOcvLoad[0] + batteryGauge.irDrop;
	batteryGauge.socEmpty = batteryOcvPercent(batteryGauge.ocvEmpty, batteryOcvLoad);
	if (batteryGauge.socEmpty < 10000) {
		if (batteryGauge.socEmpty < batteryGauge.socOcv) {
			batteryGauge.socVolt = (unsigned long)(batteryGauge.socOcv - batteryGauge.socEmpty) *
					       10000 / (10000 - batteryGauge.socEmpty);
			if (batteryGauge.socVolt >= 10001)
				batteryGauge.socVolt = 10000;
		} else {
			batteryGauge.socVolt = 0;
		}
	} else {
		batteryGauge.socVolt = 10000;
	}
}

/* the state of charge from what is left of the usable */
static void batterySocChargeUpdate(void)
{
	if ((int)batteryGauge.usable > -batteryGauge.chargeMah)
		batteryGauge.remain = batteryGauge.usable + batteryGauge.chargeMah;
	else
		batteryGauge.remain = 0;
	if (batteryGauge.remain != 0 && batteryGauge.usable != 0) {
		batteryGauge.socCharge = (unsigned long)10000 * batteryGauge.remain /
					 batteryGauge.usable;
		if (batteryGauge.socCharge >= 10001)
			batteryGauge.socCharge = 10000;
	} else {
		batteryGauge.socCharge = 0;
	}
}

/* the voltage's and the charge's states of charge weighted by the voltage, the state of
   charge and the temperature */
static void batterySocUpdate(void)
{
	long value;

	batteryGauge.weightVolt = 0xffff * batteryGauge.volt + 0x1004;
	if (batteryGauge.weightVolt < 0)
		batteryGauge.weightVolt = 0;
	if (batteryGauge.socVolt != 0)
		value = (long)((unsigned long)10000 * (10000 - batteryGauge.socVolt)) /
			(long)((unsigned long)batteryGauge.socVolt * 120) + 10;
	else
		value = 0x7fffffffL;
	if (value >= 10001)
		batteryGauge.weightSoc = 10000;
	else
		batteryGauge.weightSoc = value;
	if ((int)batteryGauge.weightSoc < 0)
		batteryGauge.weightSoc = 0;
	batteryGauge.weightTemp = (long)(((unsigned long)batteryGauge.temp - 2732) << 4) / 10 + -560L;
	if ((int)(batteryGauge.weightVolt + batteryGauge.weightTemp) < 100) {
		batteryGauge.weight = batteryGauge.weightSoc;
	} else {
		value = (long)(batteryGauge.weightVolt + batteryGauge.weightTemp) *
			(int)batteryGauge.weightSoc / 100;
		if (value >= 10001)
			batteryGauge.weight = 10000;
		else
			batteryGauge.weight = value;
	}
	batteryGauge.socBefore = batteryGauge.soc;
	batteryGauge.soc = ((unsigned long)batteryGauge.socVolt * (int)batteryGauge.weight +
			    (unsigned long)batteryGauge.socCharge * (int)(10000 - batteryGauge.weight)) /
			   10000;
	if (batteryGauge.soc >= 10001)
		batteryGauge.soc = 10000;
}

static void batteryShownReset(void)
{
	batteryGauge.socShown = batteryGauge.socCharge;
	batteryGauge.lifePercent = (batteryGauge.socShown + 50) / 100;
}

/* discharging: the shown state of charge only down, and only after two updates below it */
static void batteryShownUpdate(void)
{
#ifdef ERNIE_UPDATER
	/* the updater's: none shown yet (0xffff), the state of charge at once */
	if (batteryGauge.socShown == 0xffff)
		batteryGauge.socShown = batteryGauge.soc;
#endif
	if (batteryGauge.soc < batteryGauge.socShown && batteryGauge.socBefore < batteryGauge.socShown)
		batteryGauge.socShown = batteryGauge.soc;
	batteryGauge.lifePercent = (batteryGauge.socShown + 50) / 100;
}

static void batterySetFull(void)
{
	batterySetCharge(batteryGauge.chargeFull);
}

static void batteryRemainUpdate(void)
{
	if (!batteryGauge.flags.bits.charging && batteryGauge.socShown == 0)
		batterySetCharge(-(int)batteryGauge.usable);
	batteryGauge.remainShown = (unsigned long)batteryGauge.usable * batteryGauge.socShown / 10000;
}

static void batteryRemainCapUpdate(void)
{
	batteryGauge.remainCap = batteryGauge.remainShown;
}

static void batterySetCharge(int mah)
{
	batteryGauge.chargeMah = mah;
	batteryGauge.charge = (long)mah * 3600;
}

/* minutes to empty: at the current while discharging, else at 500 mA */
static void batteryTteUpdate(void)
{
	unsigned int current;

	if (-batteryGauge.current >= 40) {
		batteryGauge.flags.bits.discharging = 1;
		current = -batteryGauge.current;
	} else {
		batteryGauge.flags.bits.discharging = 0;
		current = 500;
	}
	batteryGauge.tte = (unsigned long)batteryGauge.remainCap * 60 / current;
}

/* a cycle for every 100 percents discharged */
static void batteryCycleUpdate(void)
{
	if (batteryGauge.lifePercent < batteryGauge.percentBefore) {
		batteryGauge.record.percent += batteryGauge.percentBefore - batteryGauge.lifePercent;
		if (batteryGauge.record.percent >= 101) {
			batteryGauge.record.percent -= 100;
			if (batteryGauge.record.cycleCount < 0xffff)
				batteryGauge.record.cycleCount++;
			batterySaveRecord();
		}
	}
	batteryGauge.percentBefore = batteryGauge.lifePercent;
	batteryGauge.cycleCount = batteryGauge.record.cycleCount;
}

static void batterySohUpdate(void)
{
	batteryGauge.soh = 100 - (unsigned char)((unsigned long)100 * batteryGauge.fade /
						 batteryGauge.fullCap);
}

/* the percent (0.01 %) of a voltage on a curve of 101 */
static unsigned int batteryOcvPercent(unsigned int volt, const unsigned int *curve)
{
	unsigned int percent;
	unsigned char low = 0;
	unsigned char high = 100;
	unsigned char middle;

	if (curve[low] >= volt) {
		percent = 0;
	} else if (volt >= curve[high]) {
		percent = 10000;
	} else {
		do {
			middle = (low + high) >> 1;
			if (volt < curve[middle])
				high = middle;
			else
				low = middle;
		} while (high - low >= 2);
		percent = 100 * (unsigned int)low +
			  100 * (volt - curve[low]) / (curve[low + 1] - curve[low]);
	}
	return percent;
}

/* a curve's value at the temperature, between its points */
static unsigned int batteryCurve(unsigned int temp, const struct BatteryPoint *curve)
{
	unsigned int value;
	unsigned char i = 0;
	unsigned int temp1;
	unsigned int temp0;
	unsigned int value1;
	unsigned int value0;

	while (temp < curve[i].temp)
		i++;
	temp1 = curve[i].temp;
	temp0 = curve[i - 1].temp;
	value1 = curve[i].value;
	value0 = curve[i - 1].value;
	value = value0 - (int)((long)(((unsigned long)value0 - (unsigned long)value1) *
				      (temp0 - temp)) / (temp0 - temp1));
	return value;
}

/* after batteryReadInfo answered (charge.c): nothing */
void batteryInfoRead(void)
{
}

/* the battery's identity: the gauge's own */
unsigned char batteryReadInfo(unsigned int *info)
{
	BATTERY_INFO(info)->id = 0xffe2;
	BATTERY_INFO(info)->firmware = 0x11;
	BATTERY_INFO(info)->dataFlash = 7;
	BATTERY_INFO(info)->type = 0xff01;
	BATTERY_INFO(info)->hardware = 0xfffd;
	BATTERY_INFO(info)->command6 = 0xfffd;
	return 0;
}

unsigned char batteryReadRemainCap(unsigned int *value)
{
	*value = batteryGauge.remainCap;
	return 0;
}

unsigned char batteryReadVolt(unsigned int *value)
{
	*value = batteryGauge.volt;
	return 0;
}

unsigned char batteryReadTemp(unsigned int *value)
{
	*value = batteryGauge.temp;
	return 0;
}

unsigned char batteryReadBicTemp(unsigned int *value)
{
	*value = batteryGauge.bicTemp;
	return 0;
}

unsigned char batteryReadLifePercent(unsigned int *value)
{
	*value = (unsigned int)batteryGauge.lifePercent;
	return 0;
}

/* the SOH, its high byte 1 */
unsigned char batteryReadSOH(unsigned int *value)
{
	union {
		unsigned int	word;
		unsigned char	byte[2];
	} soh;

	soh.byte[0] = batteryGauge.soh;
	soh.byte[1] = 1;
	*value = soh.word;
	return 0;
}

unsigned char batteryReadCycleCount(unsigned int *value)
{
	*value = batteryGauge.cycleCount;
	return 0;
}

unsigned char batteryReadFullCap2(unsigned int *value)
{
	*value = batteryGauge.fullCap2;
	return 0;
}

unsigned char batteryReadElec(unsigned int *value)
{
	*value = batteryGauge.current;
	return 0;
}

unsigned char batteryReadFullCap(unsigned int *value)
{
	*value = batteryGauge.fullCap;
	return 0;
}

unsigned char batteryReadTTE(unsigned int *value)
{
	*value = batteryGauge.tte;
	return 0;
}

unsigned char batteryReadInfo1A(unsigned int *value)
{
	*value = batteryGauge.info1A;
	return 0;
}

unsigned char batteryReadInfo6C(unsigned int *value)
{
	*value = batteryGauge.info6C;
	return 0;
}

unsigned char batteryReadFlags(unsigned int *value)
{
	*value = batteryGauge.flags.word;
	return 0;
}

#ifndef ERNIE_UPDATER	/* the updater's: none of the bootloader line's, nor the faults' */
/* 0x08b0: the bootloader line, now only recorded */
void batteryBLLineOn(void)
{
	batteryGauge.flags.bits.blLine = 1;
	batteryFaults.byte[1] |= 0x01;
	eventLogRecord(5, 0x10, batteryFaults.byte, 4);
}

void batteryBLLineOff(void)
{
	batteryGauge.flags.bits.blLine = 0;
	batteryFaults.byte[1] &= ~0x01;
	eventLogRecord(5, 0x10, batteryFaults.byte, 4);
}

/* at the power-off: the manual charge mode left, the faults recorded and cleared; where the
   bootloader line is on, register 0xaa bit 15 off and the gauge hibernated (2 where it is not) */
unsigned char batteryHibernate(void)
{
	unsigned char result = 0;

	chargeSetManualMode(0);
	batteryFaults.byte[1] |= 0x02;
	eventLogRecord(5, 0x10, batteryFaults.byte, 4);
	batteryFaults.byte[0] = 0;
	batteryFaults.word[2] = 0;
	batteryFaults.word[3] = 0;
	if (batteryGauge.flags.bits.blLine) {
		result = powerRegAA15Off();
		if (result == 0) {
			P14.4 = 0;
			batteryGauge.flags.bits.hibernated = 1;
			batteryFaults.byte[1] |= 0x04;
			eventLogRecord(5, 0x10, batteryFaults.byte, 4);
		}
	} else {
		result = 2;
	}
	return result;
}

/* 0x098d: the cycles counted from 0 */
unsigned char batteryStopBLMode(void)
{
	batteryGauge.record.percent = 0;
	batteryGauge.record.cycleCount = 0;
	batterySaveRecord();
	batteryUpdate();
	chargeUpdate();
	return 0;
}

/* the faults record's eight bytes */
void batteryGetFaults(unsigned char *faults)
{
	memcpy(faults, batteryFaults.byte, sizeof(batteryFaults));
}
#endif
#else

/* the battery's address on the I2C bus: the Cookie's where the settings move it (DOL-1001 moves
   the battery only, its Cookie stays at 0xa8) */
#if BOARD_COOKIE_REG_HIGH || BOARD_DOL1001
#define BATTERY_ADDRESS		(configGet()->cookieRegHigh ? 0xa4 : 0xaa)
#else
#define BATTERY_ADDRESS		0xaa
#endif

/* the charge stopped while the bootloader runs: on the slim out of the manual charge mode */
#if SOURCE_0103
#define BATTERY_CHARGE(on)	cookieUpdateBatteryCharge(on)
#else
#define BATTERY_CHARGE(on)	cookieCtrlBatteryCharge(on)
#endif

/* the battery's id when none answered at the power-on */
#if SOURCE_0103
#define BATTERY_NONE		0xffff
#else
#define BATTERY_NONE		0x00ff
#endif

#if BOARD_DOL1002
/* DOL-1002 has no battery: what the other files call answers 0 (a read 0 0); the gauge's
   transfers stay behind it, logging a failure (events 0x25 and 0x26), for the control commands
   and the bootloader mode's start */
#ifdef ERNIE_UPDATER	/* no event log in DOL-1002's updater */
#define BATTERY_LOG_FAILURE(status)	if ((status) != 0) { \
					}
#else
#define BATTERY_LOG_FAILURE(status)	if ((status) != 0) { \
						logEvent(0x25); \
						logEventValue(0x26, (status)); \
					}
#endif
#define BATTERY_BUS_WRITE		batteryWriteBus
#define BATTERY_BUS_READ		batteryReadBus
#define BATTERY_BUS_READ_AT		batteryReadAtBus
#else
#define BATTERY_BUS_WRITE		batteryWrite
#define BATTERY_BUS_READ		batteryRead
#define BATTERY_BUS_READ_AT		batteryReadAt
#endif

static struct IicaRequest batteryIica;
static unsigned char batteryBLBuffer[BATTERY_BL_SIZE - 1];
struct BatteryBLCommand batteryBLCommand;
#if SOURCE_0103
union BatteryFaults batteryFaults;
#endif

void batteryInit(void)
{
	batteryBLCommand.command = 0;
	batteryBLCommand.length = 0;
	batteryBLCommand.buffer = batteryBLBuffer;
#if SOURCE_0103
	memset(batteryFaults.byte, 0, sizeof(batteryFaults));
#endif
}

#if SOURCE_0103
/* after batteryReadInfo answered (charge.c): nothing */
void batteryInfoRead(void)
{
}
#endif

#if BOARD_DOL1002
unsigned char batteryWrite(unsigned char *data, unsigned char length)
{
	return 0;
}

/* data[0] is the register */
static unsigned char batteryWriteBus(unsigned char *data, unsigned char length)
#else
/* data[0] is the register */
unsigned char batteryWrite(unsigned char *data, unsigned char length)
#endif
{
	unsigned char status;
#if !UPD_0103
	ER ercd;
	FLGPTN ptn;
#endif
#if UPD_0103
	unsigned char wait;
#endif

#if !BOARD_DOL1002
	if (deviceVersions.battery.id == BATTERY_NONE)
		return 0;
#endif
	batteryIica.rxData = NULL;
	batteryIica.rxCount = 0;
	batteryIica.txData = data;
	batteryIica.txCount = length;
	batteryIica.address = BATTERY_ADDRESS;
	batteryIica.status = &status;
#if UPD_0103	/* the PS TV's updater: no kernel, the transfer waited for in iicaTransfer, then a
		   short delay */
	iicaTransfer(&batteryIica);
	wait = 65;
	while (wait != 0) {
		NOP();
		wait--;
	}
#else
#if FW_SINCE(0x0100060B)
	clr_flg(4, ~0x10);
#endif
	iicaStart(&batteryIica);
	ercd = twai_flg(4, 0x10, TWF_ORW, &ptn, 0x4b);
	if (ercd == E_OK) {
		if (ptn & 0x10)
			clr_flg(4, ~0x10);
	} else if (ercd == E_TMOUT) {
		status = 0x10;
	}
#endif
#if BOARD_DOL1002
	BATTERY_LOG_FAILURE(status)
#endif
	return status;
}

#if BOARD_DOL1002
unsigned char batteryRead(unsigned char *reg, unsigned char *data, unsigned char length)
{
	*data++ = 0;
	*data = 0;
	return 0;
}

static unsigned char batteryReadBus(unsigned char *reg, unsigned char *data, unsigned char length)
#else
/* none answered: data reads 0 0 */
unsigned char batteryRead(unsigned char *reg, unsigned char *data, unsigned char length)
#endif
{
#if !UPD_0103
	ER ercd;
	FLGPTN ptn;
#endif
	unsigned char status;
#if UPD_0103
	unsigned char wait;
#endif

#if !BOARD_DOL1002
	if (deviceVersions.battery.id == BATTERY_NONE) {
		*data++ = 0;
		*data = 0;
		return 0;
	}
#endif
	batteryIica.rxData = data;
	batteryIica.rxCount = length;
	batteryIica.txData = reg;
	batteryIica.txCount = 1;
	batteryIica.address = BATTERY_ADDRESS;
	batteryIica.status = &status;
#if UPD_0103	/* the PS TV's updater: no kernel, the transfer waited for in iicaTransfer, then a
		   short delay */
	iicaTransfer(&batteryIica);
	wait = 65;
	while (wait != 0) {
		NOP();
		wait--;
	}
#else
#if FW_SINCE(0x0100060B)
	clr_flg(4, ~0x10);
#endif
	iicaStart(&batteryIica);
	ercd = twai_flg(4, 0x10, TWF_ORW, &ptn, 0x4b);
	if (ercd == E_OK) {
		if (ptn & 0x10)
			clr_flg(4, ~0x10);
	} else if (ercd == E_TMOUT) {
		status = 0x10;
	}
#endif
#if BOARD_DOL1002
	BATTERY_LOG_FAILURE(status)
#endif
	return status;
}

#if BOARD_DOL1002
unsigned char batteryControl(unsigned int command, unsigned char *data, unsigned char length)
{
	*data++ = 0;
	*data = 0;
	return 0;
}

/* a control command (register 0), then its answer */
static unsigned char batteryControlBus(unsigned int command, unsigned char *data,
				       unsigned char length)
#else
/* a control command (register 0), then its answer */
unsigned char batteryControl(unsigned int command, unsigned char *data, unsigned char length)
#endif
{
	unsigned char result;

#if !BOARD_DOL1002
	if (deviceVersions.battery.id == BATTERY_NONE) {
		*data++ = 0;
		*data = 0;
		return 0;
	}
#endif
	elmoCommand[0] = 0;
	elmoCommand[1] = command & 0xff;
	elmoCommand[2] = command >> 8;
	result = BATTERY_BUS_WRITE(elmoCommand, 3);
	if (result == 0) {
		elmoCommand[0] = 0;
		result = BATTERY_BUS_READ(elmoCommand, data, length);
		if (result != 0)
			;
	}
	return result;
}

#if !FW_SINCE(0x00090203)	/* 0x00090009's order */
/* a write to the device at packet[0]: the rest of the length bytes */
unsigned char batteryWriteAt(unsigned char *packet, unsigned char length)
{
	ER ercd;
	FLGPTN ptn;
	unsigned char status;

	if (deviceVersions.battery.id == BATTERY_NONE)
		return 0;
	if (length != 0) {
		batteryIica.txCount = length - 1;
		batteryIica.rxData = NULL;
		batteryIica.rxCount = 0;
		batteryIica.txData = packet + 1;
		batteryIica.address = *packet;
		batteryIica.status = &status;
		iicaStart(&batteryIica);
		ercd = twai_flg(4, 0x10, TWF_ORW, &ptn, 0x4b);
		if (ercd == E_OK) {
			if (ptn & 0x10)
				clr_flg(4, ~0x10);
		} else if (ercd == E_TMOUT) {
			iicaInit();
			status = 0x10;
		}
	} else {
		status = 0;
	}
	return status;
}

/* a register of the device at packet[0] (packet[1] the register); a timeout resets the bus */
unsigned char batteryReadAt(unsigned char *packet, unsigned char *data, unsigned char length)
{
	ER ercd;
	FLGPTN ptn;
	unsigned char status;

	if (deviceVersions.battery.id == BATTERY_NONE) {
		*data++ = 0;
		*data = 0;
		return 0;
	}
	batteryIica.rxData = data;
	batteryIica.rxCount = length;
	batteryIica.txData = packet + 1;
	batteryIica.txCount = 1;
	batteryIica.address = *packet;
	batteryIica.status = &status;
	iicaStart(&batteryIica);
	ercd = twai_flg(4, 0x10, TWF_ORW, &ptn, 0x4b);
	if (ercd == E_OK) {
		if (ptn & 0x10)
			clr_flg(4, ~0x10);
	} else if (ercd == E_TMOUT) {
		iicaInit();
		status = 0x10;
	}
	return status;
}
#endif

/* the battery's identity (struct BatteryInfo's four words): 0xfffe for each where a battery
   without a gauge answered */
#if SOURCE_0103

#if BOARD_DOL1002
/* DOL-1002's: every word 0xfffc */
unsigned char batteryReadInfo(unsigned int *info)
{
	BATTERY_INFO(info)->id = BATTERY_INFO(info)->type = BATTERY_INFO(info)->firmware =
		BATTERY_INFO(info)->dataFlash = BATTERY_INFO(info)->command6 =
		BATTERY_INFO(info)->hardware = 0xfffc;
	return 0;
}
#else
unsigned char batteryReadInfo(unsigned int *info)
{
	unsigned char result;
	unsigned char status;

	if (deviceVersions.battery.id == BATTERY_NONE) {
		BATTERY_INFO(info)->id = BATTERY_INFO(info)->type = BATTERY_INFO(info)->firmware =
			BATTERY_INFO(info)->dataFlash = BATTERY_INFO(info)->command6 =
			BATTERY_INFO(info)->hardware = 0xffff;
		return 0;
	}
	result = batteryControl(1, (unsigned char *)&BATTERY_INFO(info)->id, 2);
	if (result == 0) {
		result = batteryControl(3,
				(unsigned char *)&BATTERY_INFO(info)->hardware, 2);
		if (result == 0) {
			result = batteryControl(2,
					(unsigned char *)&BATTERY_INFO(info)->firmware, 2);
			if (result == 0) {
				BATTERY_INFO(info)->type = BATTERY_INFO(info)->hardware;
				if (BATTERY_INFO(info)->firmware >= 0x0416) {
					result = batteryControl(6,
							(unsigned char *)&BATTERY_INFO(info)->command6, 2);
					if (result != 0)
						goto failed;
					if ((BATTERY_INFO(info)->hardware == 0xa7 ||
					     BATTERY_INFO(info)->hardware == 0xa8) &&
					    BATTERY_INFO(info)->command6 == 0x0802)
						BATTERY_INFO(info)->type = 0xa8;
				} else
					BATTERY_INFO(info)->command6 = 0xfffd;
				elmoCommand[0] = 0x3e;
				elmoCommand[1] = 0x39;
				result = batteryWrite(elmoCommand, 2);
				if (result == 0) {
					elmoCommand[0] = 0x3f;
					elmoCommand[1] = 0;
					result = batteryWrite(elmoCommand, 2);
					if (result == 0) {
						powerSleep2(4);
						elmoCommand[0] = 0x40;
						result = batteryRead(elmoCommand,
							(unsigned char *)&BATTERY_INFO(info)->dataFlash, 2);
					}
				}
			}
		}
	}
failed:
#if !UPD_0103 /* the PS TV's updater: no status read */
	if (result == 1) {
		elmoCommand[0] = 0x16;
		elmoCommand[1] = 0;
		result = batteryReadAt(elmoCommand, &status, 1);
		if (result == 0) {
			BATTERY_INFO(info)->id = 0xfffe;
			BATTERY_INFO(info)->type = 0xfffe;
			BATTERY_INFO(info)->firmware = 0xfffe;
			BATTERY_INFO(info)->dataFlash = 0xfffe;
			BATTERY_INFO(info)->command6 = 0xfffe;
			BATTERY_INFO(info)->hardware = 0xfffe;
		}
	}
#endif
	return result;
}
#endif /* BOARD_DOL1002 */
#else
unsigned char batteryReadInfo(unsigned int *info)
{
	unsigned char result;
	unsigned char status;

	result = batteryControl(1, (unsigned char *)info, 2);
	if (result == 0) {
		result = batteryControl(3, (unsigned char *)(info + 1), 2);
		if (result == 0) {
			result = batteryControl(2, (unsigned char *)(info + 2), 2);
			if (result == 0) {
				elmoCommand[0] = 0x3e;
				elmoCommand[1] = 0x39;
				result = batteryWrite(elmoCommand, 2);
				if (result == 0) {
					elmoCommand[0] = 0x3f;
					elmoCommand[1] = 0;
					result = batteryWrite(elmoCommand, 2);
					if (result == 0) {
#if UPD_0103	/* the PS TV's updater: no kernel to sleep in */
						powerWaitMs(4);
#else
						powerSleep2(4);
#endif
						elmoCommand[0] = 0x40;
						result = batteryRead(elmoCommand,
								     (unsigned char *)(info + 3), 2);
					}
				}
			}
		}
	}
	if (result == 1) {
#if !UPD_0103	/* the PS TV's updater: the test kept, with no gauge to ask */
		elmoCommand[0] = 0x16;
		elmoCommand[1] = 0;
		result = batteryReadAt(elmoCommand, &status, 1);
		if (result == 0) {
			info[0] = 0xfffe;
			info[1] = 0xfffe;
			info[2] = 0xfffe;
			info[3] = 0xfffe;
		}
#endif
	}
	return result;
}
#endif

/* the gauge's remaining capacity (register 0x10) */
unsigned char batteryReadRemainCap(unsigned int *value)
{
	unsigned char reg;

	reg = 0x10;
	return batteryRead(&reg, (unsigned char *)value, 2);
}

/* its voltage (0x08) */
unsigned char batteryReadVolt(unsigned int *value)
{
	unsigned char reg;

	reg = 0x08;
	return batteryRead(&reg, (unsigned char *)value, 2);
}

/* its temperature (0x06) */
unsigned char batteryReadTemp(unsigned int *value)
{
	unsigned char reg;

	reg = 0x06;
	return batteryRead(&reg, (unsigned char *)value, 2);
}

#if FW_SINCE(0x00090203)	/* 0x00090009 has no internal temperature */
/* its internal temperature (0x20), from version 0x0404 on; before it 0x0aac (0 degrees) */
unsigned char batteryReadBicTemp(unsigned int *value)
{
	unsigned char reg;

#if SOURCE_0103
	reg = 0x20;					/* the slim's: whatever the version */
	return batteryRead(&reg, (unsigned char *)value, 2);
#else
	if (deviceVersions.battery.version[1] >= 0x0404) {
		reg = 0x20;
		return batteryRead(&reg, (unsigned char *)value, 2);
	}
	*value = 0x0aac;
	return 0;
#endif
}
#endif

#if FW_SINCE(0x00090006)	/* 0x00080504 reads no state of charge */
/* its state of charge in percent (0x2c) */
unsigned char batteryReadLifePercent(unsigned int *value)
{
	unsigned char reg;

	reg = 0x2c;
	return batteryRead(&reg, (unsigned char *)value, 2);
}
#endif

#if !UPD_0103	/* the PS TV's updater: none of the rest */
#if FW_SINCE(0x00090203)
#if BOARD_DOL1002
unsigned char batteryReadAt(unsigned char *packet, unsigned char *data, unsigned char length)
{
	*data++ = 0;
	*data = 0;
	return 0;
}

/* a register of the device at packet[0] (packet[1] the register); a timeout resets the bus */
static unsigned char batteryReadAtBus(unsigned char *packet, unsigned char *data,
				      unsigned char length)
#else
/* a register of the device at packet[0] (packet[1] the register); a timeout resets the bus */
unsigned char batteryReadAt(unsigned char *packet, unsigned char *data, unsigned char length)
#endif
{
#if !UPD_0103
	ER ercd;
	FLGPTN ptn;
#endif
	unsigned char status;

#if !BOARD_DOL1002
	if (deviceVersions.battery.id == BATTERY_NONE) {
		*data++ = 0;
		*data = 0;
		return 0;
	}
#endif
	batteryIica.rxData = data;
	batteryIica.rxCount = length;
	batteryIica.txData = packet + 1;
	batteryIica.txCount = 1;
	batteryIica.address = *packet;
	batteryIica.status = &status;
#if FW_SINCE(0x0100060B)
	clr_flg(4, ~0x10);
#endif
	iicaStart(&batteryIica);
	ercd = twai_flg(4, 0x10, TWF_ORW, &ptn, 0x4b);
	if (ercd == E_OK) {
		if (ptn & 0x10)
			clr_flg(4, ~0x10);
	} else if (ercd == E_TMOUT) {
		iicaInit();
		status = 0x10;
	}
#if BOARD_DOL1002
	BATTERY_LOG_FAILURE(status)
#endif
	return status;
}

#if BOARD_DOL1002
unsigned char batteryWriteAt(unsigned char *packet, unsigned char length)
{
	return 0;
}

/* a write to the device at packet[0]: the rest of the length bytes; nothing calls it */
static unsigned char batteryWriteAtBus(unsigned char *packet, unsigned char length)
#else
/* a write to the device at packet[0]: the rest of the length bytes */
unsigned char batteryWriteAt(unsigned char *packet, unsigned char length)
#endif
{
	ER ercd;
	FLGPTN ptn;
	unsigned char status;

#if !BOARD_DOL1002
	if (deviceVersions.battery.id == BATTERY_NONE)
		return 0;
#endif
	if (length != 0) {
		batteryIica.txCount = length - 1;
		batteryIica.rxData = NULL;
		batteryIica.rxCount = 0;
		batteryIica.txData = packet + 1;
		batteryIica.address = *packet;
		batteryIica.status = &status;
#if FW_SINCE(0x0100060B)
		clr_flg(4, ~0x10);
#endif
		iicaStart(&batteryIica);
		ercd = twai_flg(4, 0x10, TWF_ORW, &ptn, 0x4b);
		if (ercd == E_OK) {
			if (ptn & 0x10)
				clr_flg(4, ~0x10);
		} else if (ercd == E_TMOUT) {
			iicaInit();
			status = 0x10;
		}
	} else {
		status = 0;
	}
#if BOARD_DOL1002
	BATTERY_LOG_FAILURE(status)
#endif
	return status;
}
#else	/* 0x00090009's order */
#if FW_SINCE(0x00090006)	/* 0x00080504 has no bootloader line and no hibernation */
/* 0x08b0: the battery's bootloader line */
void batteryBLLineOn(void)
{
#if !BOARD_102603
	P96 = 1;
#endif
}

void batteryBLLineOff(void)
{
#if !BOARD_102603
	P96 = 0;
#endif
}

/* at the power-off: the gauge's control command 0x0011 (its hibernation), from version 0x0408 */
unsigned char batteryHibernate(void)
{
	if (deviceVersions.battery.version[1] >= 0x0408) {
		elmoCommand[0] = 0;
		elmoCommand[1] = 0x11;
		elmoCommand[2] = 0;
		return batteryWrite(elmoCommand, 3);
	}
	return 0;
}
#endif
#endif

/* 0x09b0: the gauge's control command 0x0f00 (its bootloader), checked at 0x16 where a battery
   without a gauge answered */
unsigned char batteryStartBLMode(void)
{
	unsigned char result;
	unsigned char data;

	elmoCommand[0] = 0;
	elmoCommand[1] = 0;
	elmoCommand[2] = 0x0f;
	result = BATTERY_BUS_WRITE(elmoCommand, 3);
	if (result == 1) {
		elmoCommand[0] = 0x16;
		elmoCommand[1] = 0;
		result = BATTERY_BUS_READ_AT(elmoCommand, &data, 1);
		if (result != 0)
			;
	}
#if BOARD_DOL1002
	if (result != 0)
		;
#endif
#if FW_SINCE(0x00090502)
	if (result == 0) {
		BATTERY_CHARGE(0);
		chargeTimerStop();
	}
#endif
	return result;
}

/* 0x09b1: the bootloader's commands 0x00 0x0f, 0x64 0x0f and 0x65 0x00 */
unsigned char batteryStopBLMode(void)
{
	unsigned char result;

#if FW_SINCE(0x00090502)
	chargeTimerStart(0x0a);
#endif
	elmoCommand[0] = 0x16;
	elmoCommand[1] = 0;
	elmoCommand[2] = 0x0f;
	result = batteryWriteAt(elmoCommand, 3);
	if (result == 0) {
		elmoCommand[0] = 0x16;
		elmoCommand[1] = 0x64;
		elmoCommand[2] = 0x0f;
		result = batteryWriteAt(elmoCommand, 3);
		if (result == 0) {
			elmoCommand[0] = 0x16;
			elmoCommand[1] = 0x65;
			elmoCommand[2] = 0;
			result = batteryWriteAt(elmoCommand, 3);
#if BOARD_DOL1002
			if (result != 0)
				;
#endif
		}
	}
#if BOARD_DOL1002
	/* DOL-1002's: the charge is not restarted */
	if (result == 0)
		;
#elif FW_SINCE(0x00090502)
	BATTERY_CHARGE(1);
#endif
	return result;
}

/* 0x0989: the gauge's control command 0x0041 (its reset) */
unsigned char batterySWReset(void)
{
#if BOARD_DOL1002
	/* DOL-1002's: the charge stopped too */
	unsigned char result;

	chargeTimerStart(0x0a);
	BATTERY_CHARGE(0);
	elmoCommand[0] = 0;
	elmoCommand[1] = 0x41;
	elmoCommand[2] = 0;
	result = batteryWrite(elmoCommand, 3);
	return result;
#else
#if FW_SINCE(0x00090502)
	chargeTimerStart(0x0a);
#endif
	elmoCommand[0] = 0;
	elmoCommand[1] = 0x41;
	elmoCommand[2] = 0;
	return batteryWrite(elmoCommand, 3);
#endif
}

#if SOURCE_0103
/* 0x08b0: the battery's bootloader line, P6.4 on the slim, recorded in the faults */
void batteryBLLineOn(void)
{
#if !BOARD_DOL1001	/* DOL-1001 has no bootloader line */
	P6.4 = 1;
	batteryFaults.byte[1] |= 0x01;
#if BOARD_EVENT_LOG
	eventLogRecord(5, 0x10, batteryFaults.byte, 4);
#endif
#endif
}

void batteryBLLineOff(void)
{
#if !BOARD_DOL1001
	P6.4 = 0;
	batteryFaults.byte[1] &= ~0x01;
#if BOARD_EVENT_LOG
	eventLogRecord(5, 0x10, batteryFaults.byte, 4);
#endif
#endif
}

/* at the power-off: the manual charge mode left, the faults recorded and cleared, then the
   gauge's control command 0x0011 (its hibernation) whatever the version */
unsigned char batteryHibernate(void)
{
#if BOARD_DOL1002
	unsigned char result;

#endif
	chargeSetManualMode(0);
	batteryFaults.byte[1] |= 0x02;
#if BOARD_EVENT_LOG
	eventLogRecord(5, 0x10, batteryFaults.byte, 4);
#endif
	batteryFaults.byte[0] = 0;
	batteryFaults.word[2] = 0;
	batteryFaults.word[3] = 0;
	elmoCommand[0] = 0;
	elmoCommand[1] = 0x11;
	elmoCommand[2] = 0;
#if BOARD_DOL1002
	result = batteryWrite(elmoCommand, 3);
	return result;
#else
	return batteryWrite(elmoCommand, 3);
#endif
}
#elif FW_SINCE(0x00090203)
/* 0x08b0: the battery's bootloader line */
void batteryBLLineOn(void)
{
#if !BOARD_102603
	P96 = 1;
#endif
}

void batteryBLLineOff(void)
{
#if !BOARD_102603
	P96 = 0;
#endif
}

/* at the power-off: the gauge's control command 0x0011 (its hibernation), from version 0x0408 */
unsigned char batteryHibernate(void)
{
	if (deviceVersions.battery.version[1] >= 0x0408) {
		elmoCommand[0] = 0;
		elmoCommand[1] = 0x11;
		elmoCommand[2] = 0;
		return batteryWrite(elmoCommand, 3);
	}
	return 0;
}
#endif

#if FW_SINCE(0x00090903)
/* 0x098d: the gauge's control command 0x0015 */
unsigned char batteryControl0015(void)
{
	chargeTimerStart(0x0a);
	elmoCommand[0] = 0;
	elmoCommand[1] = 0x15;
	elmoCommand[2] = 0;
	return batteryWrite(elmoCommand, 3);
}
#endif

#if SOURCE_0103
/* the faults record's eight bytes */
void batteryGetFaults(unsigned char *faults)
{
	memcpy(faults, batteryFaults.byte, sizeof(batteryFaults));
}
#endif
#endif /* !UPD_0103 */
#endif /* BATTERY_GAUGE */
