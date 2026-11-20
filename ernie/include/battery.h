/*
 * The battery (app/battery.c): its gauge on the I2C bus at 0xaa (0xa4 where the settings move
 * the Cookie's registers), and its bootloader's commands (0x09b2-0x09b4).
 */
#ifndef ERNIE_BATTERY_H
#define ERNIE_BATTERY_H

#include "ernie.h"
#include "board.h"
#if BOARD_USS1001 && SOURCE_0104
#include "analog.h"
#endif

/* the slim's 0x0105 (USS-1002): the syscon is the gauge, from the battery's voltage (ANI7), its
   current (ANI8) and its thermistor (ANI9), averaged by the input scan (input.h's
   INPUT_MEASURE); no gauge on the bus, no bootloader commands */
#define BATTERY_GAUGE		(BOARD_USS1001 && SOURCE_0104)

#if BATTERY_GAUGE
/* the gauge's record in the NVS (0xbc, 4 bytes) */
struct BatteryRecord {
	unsigned int	cycleCount;	/* full charges counted */
	unsigned char	percent;	/* the percents discharged toward the next cycle */
	unsigned char	reserved;
};

/* the gauge's flags (batteryReadFlags answers the word) */
union BatteryFlags {
	unsigned int	word;
	struct {
		unsigned int	estimated:1;	/* the estimate started */
		unsigned int	blLine:1;	/* batteryBLLineOn's */
		unsigned int	hibernated:1;	/* batteryHibernate's: no update */
		unsigned int	charging:1;	/* the current at least 40 mA */
		unsigned int	flowing:1;	/* the current 40 mA or more either way */
		unsigned int	currentReached:1;	/* the charge current reached the band's */
		unsigned int	fullPending:1;	/* the band's full voltage at a small current */
		unsigned int	full:1;		/* 20 seconds fullPending */
		unsigned int	voltOffsetValid:1;
		unsigned int	currentOffsetValid:1;
		unsigned int	resting:1;	/* suspended: rest counts */
		unsigned int	resumed:1;	/* the rest ends at the next current */
		unsigned int	idle10:1;	/* 10 minutes idle */
		unsigned int	rest60:1;	/* an hour of rest */
		unsigned int	discharging:1;	/* tte from the current */
	} bits;
};

/* the gauge's state: capacities in mAh, states of charge in 0.01 % (10000 full) */
struct BatteryGauge {
	unsigned int	volt;		/* mV */
	int		current;	/* mA, above 0 charging */
	unsigned int	temp;		/* 0.1 K, 0xffff out of range */
	unsigned int	interval;	/* seconds from the update before */
	unsigned int	voltOffsetSaved;	/* the settings' batteryCalibration0 */
	unsigned int	currentOffsetSaved;	/* the settings' batteryCalibration1 */
	unsigned int	remainCap;	/* batteryReadRemainCap's */
	unsigned int	bicTemp;	/* batteryReadBicTemp's: temp */
	unsigned char	lifePercent;	/* batteryReadLifePercent's */
	unsigned char	soh;		/* percent of fullCap left after the cycles */
	unsigned int	cycleCount;	/* the record's */
	unsigned int	fullCap2;	/* usable at the temperature */
	unsigned int	fullCap;	/* the design's, 2320 */
	unsigned int	tte;		/* minutes to empty */
	unsigned int	info1A;		/* answered, never set */
	int		voltOffset;	/* added to ANI7, 0 where not valid */
	int		currentOffset;	/* added to ANI8 */
	long		charge;		/* mA s from full: 0 full, below 0 discharged */
	int		chargeMah;	/* charge in mAh */
	int		chargeFull;	/* chargeMah where a charge ends: minus the band's curve */
	unsigned int	capacity;	/* fullCap less the fade, at least 1160 */
	unsigned int	resistance;	/* mohm */
	unsigned int	resistanceBase;	/* mohm at the temperature */
	unsigned int	resistanceAge;	/* percent of the base with the cycles: 100-200 */
	int		irDrop;		/* mV: minus the current times the resistance */
	unsigned int	ocv;		/* volt plus irDrop: the open circuit's */
	unsigned int	ocvEmpty;	/* the curve's empty voltage plus irDrop */
	unsigned int	tempFactor;	/* 0.01 % of capacity usable at the temperature */
	unsigned int	usable;		/* capacity at the temperature */
	unsigned int	fade;		/* lost to the cycles */
	unsigned int	remain;		/* usable less what was discharged */
	unsigned int	remainShown;	/* usable at socShown */
	unsigned int	socVolt;	/* from ocv, above the empty voltage */
	unsigned int	socEmpty;	/* of ocvEmpty */
	unsigned int	socOcv;		/* of ocv */
	unsigned int	socCharge;	/* from remain */
	unsigned int	soc;		/* socVolt and socCharge weighted */
	unsigned int	socBefore;	/* soc of the update before */
	unsigned int	reserved;
	unsigned int	weight;		/* 0.01 % of socVolt in soc */
	int		weightVolt;	/* from the voltage */
	unsigned int	weightSoc;	/* from socVolt */
	int		weightTemp;	/* from the temperature */
	unsigned int	socShown;	/* lifePercent's, never up while discharging */
	unsigned char	percentBefore;	/* lifePercent of the update before */
	struct BatteryRecord record;
	unsigned int	reserved2[2];
	unsigned int	updateTicks;	/* the RTC's at the update */
	unsigned int	fullTicks;	/* the RTC's when full was first seen */
	unsigned int	idle;		/* seconds without current */
	unsigned int	rest;		/* seconds of the rest (suspended) */
	unsigned int	band;		/* the charge current's: 0-2 */
	unsigned int	info6C;		/* answered, never set */
	union BatteryFlags flags;
};

extern struct BatteryGauge batteryGauge;
extern struct AnalogMeasure batteryMeasure;	/* the input scan's averages, read at each update */

/* the offsets (batteryCalibrate, batteryGetOffsets) */
struct BatteryOffsets {
	unsigned int	volt;
	unsigned int	current;
};

/* 0x0904's: the offsets used and the settings' */
struct BatteryCalibration {
	unsigned int	volt;
	unsigned int	voltSaved;
	unsigned int	current;
	unsigned int	currentSaved;
};

extern void batteryPowerOn(void);
extern unsigned char batteryCalibrate(unsigned char mode, struct BatteryOffsets *offsets);
extern void batteryLoadOffsets(void);
extern void batteryPowerOff(void);
extern void batteryUpdate(void);
extern unsigned char batterySaveRecord(void);
extern unsigned char batteryIsCharging(void);
extern unsigned char batteryOffsetsValid(void);
extern void batteryResume(int woken);
extern void batterySuspend(void);
extern void batteryGetOffsets(struct BatteryOffsets *offsets);
extern void batteryGetCalibration(struct BatteryCalibration *calibration);
extern void batteryClearFull(void);
extern unsigned char batteryReadSOH(unsigned int *value);
extern unsigned char batteryReadCycleCount(unsigned int *value);
extern unsigned char batteryReadFullCap2(unsigned int *value);
extern unsigned char batteryReadElec(unsigned int *value);
extern unsigned char batteryReadFullCap(unsigned int *value);
extern unsigned char batteryReadTTE(unsigned int *value);
extern unsigned char batteryReadInfo1A(unsigned int *value);
extern unsigned char batteryReadInfo6C(unsigned int *value);
extern unsigned char batteryReadFlags(unsigned int *value);
#endif

/* a command to the battery's bootloader (0x09b2-0x09b4) */
struct BatteryBLCommand {
	unsigned int	command;
	unsigned char	length;
	unsigned char	*buffer;
};

#define BATTERY_BL_SIZE		0x63	/* the most the buffer holds */

extern struct BatteryBLCommand batteryBLCommand;

extern void batteryInit(void);
extern unsigned char batteryWrite(unsigned char *data, unsigned char length);
extern unsigned char batteryRead(unsigned char *reg, unsigned char *data, unsigned char length);
extern unsigned char batteryControl(unsigned int command, unsigned char *data,
				    unsigned char length);

extern unsigned char batteryReadInfo(unsigned int *info);
extern unsigned char batteryReadRemainCap(unsigned int *value);
extern unsigned char batteryReadVolt(unsigned int *value);
extern unsigned char batteryReadTemp(unsigned int *value);
extern unsigned char batteryReadBicTemp(unsigned int *value);
extern unsigned char batteryReadLifePercent(unsigned int *value);
extern unsigned char batteryReadAt(unsigned char *packet, unsigned char *data, unsigned char length);
extern unsigned char batteryWriteAt(unsigned char *packet, unsigned char length);
extern unsigned char batteryStartBLMode(void);
extern unsigned char batteryStopBLMode(void);
extern unsigned char batterySWReset(void);
extern void batteryBLLineOn(void);
extern void batteryBLLineOff(void);
extern unsigned char batteryHibernate(void);
extern unsigned char batteryControl0015(void);
#if SOURCE_0103
/* the slim's faults record, logged as event 5, 0x10 (its first four bytes) at each change:
   byte 0 the power-on's (bits 0-3) and the charge's (4, 5), byte 1 the bootloader line (bit 0)
   and the hibernation (bit 1) */
union BatteryFaults {
	unsigned char	byte[8];
	unsigned int	word[4];
};
extern union BatteryFaults batteryFaults;
extern void batteryGetFaults(unsigned char *faults);
extern void batteryInfoRead(void);	/* after batteryReadInfo answered: nothing */
#endif

#endif
