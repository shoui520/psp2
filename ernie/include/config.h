/*
 * The settings (app/config.c): two banks of 1 KiB in the flash at 0x3b800, each a header and
 * the 0x3f0 bytes of a ConfigBlock; the newer valid one is used. The host edits a copy in RAM
 * by records, checked with SHA-1, and the copy is written to the other bank.
 */
#ifndef ERNIE_CONFIG_H
#define ERNIE_CONFIG_H

#include "analog.h"
#include "charge.h"
#include "watchdog.h"

#define CONFIG_SIZE		0x3f0	/* a bank's block */
#define CONFIG_VERSION		0x31	/* this build's layout */
#define CONFIG_WRITTEN		0x5a5a

/* PLACEHOLDER: only the fields read so far */
struct ConfigBlock {
	struct AnalogCalibration analogCalibration;	/* the sticks' default calibration */
	unsigned char	analogScale0;		/* set 0's margin, in 1/1000 of the range */
	unsigned char	analogScale1;		/* set 1's */
	union {
		unsigned int	value;
		struct {
			unsigned int	wakeUnmask22:1;	/* set: wake factor 22 unmasked at the start
							   (stateInit) */
			unsigned int	batteryNoRetry:1;	/* set: the battery is read once at the
								   power-on (powerSeqOn) */
			unsigned int	reserved20:1;
			unsigned int	chargeLedOff:1;	/* where layouts before 0x31 kept
							   chargeLedOff (configMerge) */
			unsigned int	wakeUnmask15:1;	/* set: wake factor 15 unmasked */
			unsigned int	reserved26:11;
		} bits;
	} start;				/* kept across a new layout */
	union {
		unsigned int	value;
		struct {
			unsigned int	bbmcDetect:1;	/* set: P3.2 low at the start means a 3G
							   modem (bbmcInit) */
			unsigned int	reserved17:15;
		} bits;
	} board;				/* kept across a new layout */
	union Watchdog	watchdog;		/* the host's watchdog at a power-on (watchdogSetup) */
	union ConfigWatchdogKeep {
		unsigned int	value;
		struct {
			unsigned int	keepOff:1;	/* set: the watchdog's setting is kept at a
							   power-on whose wakeup factor has no notify */
			unsigned int	keepNotify:1;	/* and at one that has */
			unsigned int	reserved19:14;
		} bits;
	} watchdogKeep;
	struct ChargeTempLevel chargeTempLevels[8];	/* the battery temperature's ranges */
	struct ChargeTempLevel chargeTempLevelsCharge[8];	/* and in system state 0x20 */
	struct ChargeLevel chargeLevels[10];	/* the battery's levels (paramLoad), with
						   bbmcDetect set */
	struct ChargeLevel chargeLevelsNoDetect[10];	/* and without */
	unsigned char	pressTime1;		/* the parameter block's button times */
	unsigned char	pressTime2;
	unsigned char	hangLimit;		/* the hang counter's count that resets the system
						   (at least 8) */
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
	union {
		unsigned int	value;
		struct {
			unsigned int	pressTimesSet:1;	/* set: pressTime1 and 2 are kept across
								   a new layout */
			unsigned int	reserved10:15;
		} bits;
	} keep;					/* kept across a new layout */
	unsigned char	paramSetting1;		/* the parameter block's, for the host */
	unsigned char	paramSetting2;
	unsigned char	voltage81;		/* the Elmo's regulators 0x81-0x84: their levels at a
						   resume */
	unsigned char	voltage82;
	unsigned char	voltage83;
	unsigned char	voltage84;
	unsigned char	reserved22;
	unsigned char	voltageTrim;		/* passed with a regulator's level (0x088e) */
	unsigned char	reserved7[0x108 - 0xf4];
	unsigned long	wakeMaskOff;		/* the wake factors unmasked, and masked, at the */
	unsigned long	wakeMaskOn;		/* start and in the off state (stateInit,
						   stateOffHandle) */
	unsigned long	suspendWakeMaskOff;	/* and in the suspend state */
	unsigned long	suspendWakeMaskOn;
	unsigned char	reserved27[0x12c - 0x118];
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
	unsigned int	batteryLowVolt;		/* below it, chargeUpdate calls chargeLedSetReleased(0) */
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
	union {
		unsigned int	value;
		struct {
			unsigned int	offWakeUnmask22:1;	/* set: wake factor 22 unmasked in the
								   off state */
			unsigned int	reserved11:1;
			unsigned int	chargeLedOff:1;	/* the charge LED off at a reset, and while
							   sub_126C9 holds (chargeLedSetOff) */
			unsigned int	chargeHold:1;	/* set: no transition 0x0f in chargeUpdate */
			unsigned int	chargeLedStateOff:1;	/* the charge LED off in the system
									   states with bit 5 (chargeLedSetStateOff) */
			unsigned int	reserved13:11;
		} bits;
	} state;				/* kept across a new layout */
	unsigned char	reserved14[0x1ae - 0x1ac];
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
	unsigned char	reserved28[0x1c2 - 0x1bc];
	unsigned int	offTransitionBlock;	/* not 0: no transition 0x18 (offTransition18) */
	unsigned char	offTransition18:1;	/* set: the off state entered by transition 0x31
						   runs transition 0x18 */
	unsigned char	reserved29:7;
	unsigned char	reserved30[CONFIG_SIZE - 0x1c5];
};

/* a bank */
struct ConfigBank {
	unsigned char	magic[4];		/* "Conf" */
	unsigned int	state;			/* CONFIG_WRITTEN (or 0x5050) */
	unsigned int	size;			/* CONFIG_SIZE */
	unsigned int	version;		/* the layout's: below CONFIG_VERSION the defaults'
						   are merged in at the start */
	unsigned int	count;			/* the writes since the defaults (1) */
	unsigned long	checksum;		/* the sum of block's bytes, complemented */
	struct ConfigBlock block;
};

/* configGetVersions's: the bank used's, then the other's */
struct ConfigVersions {
	unsigned int	version;
	unsigned int	count;
	unsigned int	otherVersion;
	unsigned int	otherCount;
};

extern int configInit(void);
extern struct ConfigBlock *configGet(void);
extern unsigned char configGetVersions(struct ConfigVersions *versions);
extern unsigned char configRead(unsigned int offset, unsigned char size, unsigned char *data);
extern unsigned char configWrite(unsigned int offset, unsigned char size, unsigned char *data);
extern unsigned char configOpen(void);
extern unsigned char configCommit(void);
extern unsigned char configWriteRecords(unsigned int offset, unsigned char size,
					unsigned char *data, unsigned char check);
extern unsigned char configClose(void);
extern unsigned char configPrepareUpdate(void);
extern unsigned int configUpdateSegment(void);
extern unsigned char configMergeUpdate(void);
extern unsigned char configTakeChanged(void);
extern unsigned char configFlashWrite(unsigned long address);
extern unsigned char configRestoreDefaults(void);

#endif
