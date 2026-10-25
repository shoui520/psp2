/*
 * The settings block (1 KiB) that sub_2C658 returns: its owner is not written yet.
 */
#ifndef ERNIE_CONFIG_H
#define ERNIE_CONFIG_H

#include "analog.h"
#include "charge.h"

/* PLACEHOLDER: only the fields read so far */
struct ConfigBlock {
	struct AnalogCalibration analogCalibration;	/* the sticks' default calibration */
	unsigned char	analogScale0;		/* set 0's margin, in 1/1000 of the range */
	unsigned char	analogScale1;		/* set 1's */
	unsigned char	wakeOff22:1;		/* set: wake factor 22 off (stateInit) */
	unsigned char	batteryNoRetry:1;	/* set: the battery is read once at the power-on
						   (powerSeqOn) */
	unsigned char	reserved20:2;
	unsigned char	wakeOff15:1;		/* set: wake factor 15 off */
	unsigned char	reserved26:3;
	unsigned char	reserved21;
	unsigned char	bbmcDetect:1;		/* set: P3.2 low at the start means a 3G modem
						   (bbmcInit) */
	unsigned char	reserved17:7;
	unsigned char	reserved18[0x1a - 0x15];
	struct ChargeTempLevel chargeTempLevels[8];	/* the battery temperature's ranges */
	struct ChargeTempLevel chargeTempLevelsCharge[8];	/* and in system state 0x20 */
	unsigned char	reserved25[0xcd - 0x7a];
	unsigned char	pressAnyState:1;	/* a button's presses go on being counted when the
						   system state changes */
	unsigned char	reserved2:7;
	unsigned int	analogMin;		/* a calibration's min at a reset, and the most a
						   loaded one's may be */
	unsigned int	analogMax;		/* its max at a reset, the least a loaded one's */
	unsigned char	analogMinRange;		/* the least high byte of a calibration's range */
	unsigned char	reserved3;
	unsigned int	cookieRegHigh;		/* the Cookie's registers are numbered from 0xa0 */
	unsigned int	batteryVolt;		/* the battery's readings with no Cookie (chargeInit) */
	unsigned int	batteryRemainCap;
	unsigned int	batteryLifePercent;
	unsigned int	batteryTemp;		/* the battery's answers when none answered: */
	unsigned int	batteryBicTemp;		/* 0x0981, 0x098b, 0x0982, 0x0983, 0x0984, */
	unsigned int	batteryFullCap;		/* 0x0985 and 0x098a */
	unsigned int	batteryElec;
	unsigned int	batteryTte;
	unsigned int	batterySoh;
	unsigned int	batteryFullCap2;
	unsigned char	reserved10[0xee - 0xea];
	unsigned char	voltage81;		/* the Elmo's regulators 0x81-0x84: their levels at a
						   resume */
	unsigned char	voltage82;
	unsigned char	voltage83;
	unsigned char	voltage84;
	unsigned char	reserved22;
	unsigned char	voltageTrim;		/* passed with a regulator's level (0x088e) */
	unsigned char	reserved7[0x108 - 0xf4];
	unsigned long	wakeMaskOff;		/* the wake factors off, and on, at the start */
	unsigned long	wakeMaskOn;		/* (stateInit) */
	unsigned char	reserved27[0x12c - 0x110];
	struct ChargeAlarmLimit chargeAlarmLimits[4];	/* the battery temperature's alarms */
	unsigned int	chargeAlarm0Delay;	/* timer 0x1d's, when alarm 0 comes on */
	unsigned int	chargeTempInterval;	/* the ticks between two chargeAlarmUpdate */
	unsigned char	chargeAlarm2Setting;	/* cookieCtrlSupplyCurrentAc's while alarm 2 is on (else 0xc0) */
	unsigned char	reserved24;
	unsigned int	bbmcPowerDelay;		/* the 3G modem's power line's hold at a power-on
						   when its power input is up (powerBbmcOn) */
	struct ChargeAlarmLimit chargeAlarmLimits3G[4];	/* the alarms with a 3G modem (bbmcDetect) */
	unsigned char	startCheckOff:1;	/* set: mode 1's command 0x0105 starts state 9
						   although serial2TestAreaUsed and serial2InTime
						   refuse */
	unsigned char	testDisarm:1;		/* set: notification 0x0211 runs transition 0 once
						   armed (serial2Arm) */
	unsigned char	reserved4:6;
	unsigned char	startClockLimit;	/* the clock's low byte up to which serial2InTime
						   holds */
	unsigned char	reserved5[0x18a - 0x186];
	unsigned int	batteryLowVolt;		/* below it, chargeUpdate calls sub_2E03A(0) */
	unsigned char	chargeLedHost;		/* the host may hand the charge LED over (0x089e
						   mode 2) */
	unsigned char	reserved8;
	unsigned char	chargeCurrents[4][6];	/* cookieCtrlSupplyCurrentUsb's charge settings by the USB status
						   (the power status's bits 8-9) and chargeUsbMode;
						   0xff: no charge */
	unsigned char	suspendCheck:1;		/* set: a suspend runs powerBbmcCycle also when
						   the 3G modem has no mode (bbmcModeIsNone) */
	unsigned char	reserved15:7;
	unsigned char	reserved16;
	unsigned int	hardwareInfoSelect;	/* power-off's hardware info flags: 1 sets 0x80,
						   3 sets 0x40, others clear both */
	unsigned char	reserved11:2;
	unsigned char	indicatorOn0:1;		/* sub_2DF86's indicator on at a reset, and while
						   sub_126C9 holds */
	unsigned char	chargeHold:1;		/* set: no transition 0x0f in chargeUpdate */
	unsigned char	indicatorOn1:1;		/* sub_2DFAA's indicator on at a reset */
	unsigned char	reserved13:3;
	unsigned char	reserved14[0x1ae - 0x1ab];
	unsigned char	batterySetting4x;	/* cookieCtrlReg7's for a battery version[2] 0x40-0x4f */
	unsigned char	batterySetting5x;	/* and 0x50 or above */
	unsigned char	touchDelaySelect;	/* Touch's delays, in NOPs */
	unsigned char	touchDelayByte;
	unsigned char	touchDelayDeselect;
	unsigned char	touchWriteTries;	/* a verified write's tries */
	unsigned char	touchWriteReads;	/* and the reads each try compares */
	unsigned char	reserved6;
	unsigned int	hostBusyLimit;		/* the host link's busy time before a pause */
	unsigned int	hostIdleLimit;		/* the idle time that clears the busy time */
	unsigned int	hostPause;		/* the pause, in ticks */
};

extern struct ConfigBlock *sub_2C658(void);

#endif
