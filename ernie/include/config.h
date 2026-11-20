/*
 * The settings (app/config.c): two banks of 1 KiB in the flash at 0x3b800, each a header and
 * the 0x3f0 bytes of a ConfigBlock; the newer valid one is used. The host edits a copy in RAM
 * by records, checked with SHA-1, and the copy is written to the other bank.
 */
#ifndef ERNIE_CONFIG_H
#define ERNIE_CONFIG_H

#include "ernie.h"
#include "analog.h"
#include "board.h"
#include "charge.h"
#include "watchdog.h"

#define CONFIG_SIZE		0x3f0	/* a bank's block */
#if BOARD_USS1001 && SOURCE_0104
#define CONFIG_VERSION		0x74	/* the slim's 0x0105 layout */
#elif BOARD_DOL1001 && SOURCE_SLIM
#define CONFIG_VERSION		0x67	/* DOL-1002's layout */
#elif BOARD_DOL1001
#define CONFIG_VERSION		0x50	/* DOL-1001's layout */
#elif SOURCE_0103 && FW_SINCE(0x01030511)
#define CONFIG_VERSION		0x58	/* the slim's layout */
#elif SOURCE_0103
#define CONFIG_VERSION		0x4d	/* the slim's 0x01030505 layout */
#elif FW_SINCE(0x0100060B)
#define CONFIG_VERSION		0x38	/* this build's layout */
#elif FW_SINCE(0x01000306)
#define CONFIG_VERSION		0x31	/* (no charge alarm adjusts or off information) */
#elif FW_SINCE(0x01000109)
#define CONFIG_VERSION		0x29	/* (no touch delays, host link limits or off transition 0x18) */
#elif FW_SINCE(0x01000107)
#define CONFIG_VERSION		0x28	/* (the charge timer's delay 0x168) */
#elif FW_SINCE(0x01000104)
#define CONFIG_VERSION		0x26	/* (the charge LED's counted blinks) */
#elif FW_SINCE(0x00090903)
#define CONFIG_VERSION		0x12	/* (nothing after chargeAlarm2Setting) */
#else
#define CONFIG_VERSION		0x10
#endif
#define CONFIG_WRITTEN		0x5a5a

/* the wake factors unmasked, and masked, at the start and in the off state (stateInit,
   stateOffHandle), and in the suspend state */
struct ConfigWakeMasks {
	unsigned long	off;
	unsigned long	on;
	unsigned long	suspendOff;
	unsigned long	suspendOn;
};

/* the WLAN line's setting the code reads: 0x00102603 and 0x00315200 before 0x01000104 read the
   byte after wlanLine (wlanLineExtra, 0x129) */
#if BOARD_102603 && !FW_SINCE(0x01000104)
#define CONFIG_WLAN_LINE	wlanLineExtra
#else
#define CONFIG_WLAN_LINE	wlanLine
#endif

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
			unsigned int	chargeLedOffReleased:1;	/* before 0x01000104: the
								   charge LED off while
								   input1Bit7Active does not hold
								   (task5ChargeLed) */
			unsigned int	chargeLedOff:1;	/* where layouts before 0x31 kept
							   chargeLedOff (configMerge); before
							   0x01000104 the charge LED's off while
							   input1Bit7Active holds */
			unsigned int	wakeUnmask15:1;	/* set: wake factor 15 unmasked */
			unsigned int	coldStartOn:1;	/* DOL-1001, set: a cold start (cause 0x32)
							   with P4.6 high turns on (stateOffEnter),
							   wake factor 20 unmasked (stateInit) */
			unsigned int	reserved26:10;
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
#if BOARD_IRS1001 || (SOURCE_0103 && !BOARD_DOL1001) || BOARD_DOL1002
	unsigned char	reserved41[0xf8 - 0xee];	/* IRS-1001 keeps its levels at 0x1bc,
							   USS-1001 at 0x1e0 */
#else
	unsigned char	voltage81;		/* the Elmo's regulators 0x81-0x84: their levels at a
						   resume */
	unsigned char	voltage82;
	unsigned char	voltage83;
	unsigned char	voltage84;
#endif
#if BOARD_IRS1001 || (SOURCE_0103 && !BOARD_DOL1001) || BOARD_DOL1002
#elif BOARD_DOL1001
	unsigned char	reserved7[0xf8 - 0xf2];	/* DOL-1001: its trims at 0x21a */
#elif ERNIE_HW == 0x00315200
	unsigned char	reserved22[3];		/* 0x00315200's layout: the trim two bytes later */
	unsigned char	voltageTrim;		/* passed with a regulator's level (0x088e) */
	unsigned char	reserved7[0xf8 - 0xf6];
#elif ERNIE_BOARD == 0x00406000
	unsigned char	reserved22[2];		/* PCH-1000's layout: the trim a byte later */
	unsigned char	voltageTrim;		/* passed with a regulator's level (0x088e) */
	unsigned char	reserved7[0xf8 - 0xf5];
#elif BOARD_IS(0x00411000)
	unsigned char	voltageTrim;		/* passed with a regulator's level (0x088e) */
	unsigned char	reserved7[0xf8 - 0xf3];
#else
	unsigned char	reserved22;
	unsigned char	voltageTrim;		/* passed with a regulator's level (0x088e) */
	unsigned char	reserved7[0xf8 - 0xf4];
#endif
	struct ConfigWakeMasks wakeMasks0;	/* the sets each board reads (BOARD_WAKE_MASKS): */
	struct ConfigWakeMasks wakeMasks1;	/* DEM-3000L's */
	struct ConfigWakeMasks wakeMasks2;	/* PCH-1000's */
	unsigned char	wlanLine;		/* the updater's: P3.1, the WLAN's line, driven */
	unsigned char	wlanLineExtra;		/* before 0x01000104 kept with wlanLine at a merge */
	unsigned char	suspendLedKeep:1;	/* before 0x01000104 set: powerSeqSuspend leaves
						   the power LED's pattern as it is */
	unsigned char	reserved40:7;
	unsigned char	suspendLedMask;		/* before 0x01000104: the LED status bits that
						   make powerSeqSuspend start pattern 1 */
	struct ChargeAlarmLimit chargeAlarmLimits[4];	/* the battery temperature's alarms */
	unsigned int	chargeAlarm0Delay;	/* timer 0x1d's, when alarm 0 comes on */
	unsigned int	chargeTempInterval;	/* the ticks between two chargeAlarmUpdate */
	unsigned char	chargeAlarm2Setting;	/* cookieCtrlSupplyCurrentAc's while alarm 2 is on (else 0xc0) */
	unsigned char	reserved24;
#if FW_SINCE(0x01000104)
	unsigned int	bbmcPowerDelay;		/* the 3G modem's power line's hold at a power-on
						   when its power input is up (powerBbmcOn) */
	struct ChargeAlarmLimit chargeAlarmLimits3G[4];	/* the alarms with a 3G modem (bbmcDetect) */
	union {
		unsigned char	value;
		struct {
			unsigned char	startCheckOff:1;	/* set: mode 1's command 0x0105
								   starts state 9 although
								   serial2TestAreaUsed and
								   serial2InTime refuse */
			unsigned char	testDisarm:1;		/* set: notification 0x0211 runs
								   transition 0 once armed
								   (serial2Arm) */
			unsigned char	reserved4:6;
		} bits;
	} test;					/* the serial2 test mode's; kept across a new
						   layout in the retail boards' 0x010001xx */
	unsigned char	startClockLimit;	/* the clock's low byte up to which serial2InTime
						   holds */
	unsigned char	chargeLedBlink1On;	/* before 0x01000107 the charge LED's first counted
						   blink (CHARGE_LED_BLINK1): its on and off ticks
						   and its blinks */
	unsigned char	chargeLedBlink1Off;
	unsigned char	chargeLedBlink1Count;
	unsigned char	chargeLedBlink2Count;	/* the second's (CHARGE_LED_BLINK2; its ticks are
						   paramSetting1 and 2) */
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
								   off state; on PCH-1000 also
								   transition 0x16 with the AC on */
			unsigned int	chargeTimer1f:1;	/* PCH-1000: set: in the system
								   states with bit 5, timer 0x1f runs
								   while the AC is off
								   (chargeTimer1fStart) */
			unsigned int	chargeLedOff:1;	/* the charge LED off at a reset, and while
							   input1Bit7Active holds (chargeLedSetOff) */
			unsigned int	chargeHold:1;	/* set: no transition 0x0f in chargeUpdate */
			unsigned int	chargeLedStateOff:1;	/* the charge LED off in the system
									   states with bit 5 (chargeLedSetStateOff) */
			unsigned int	reserved13:11;
		} bits;
	} state;				/* kept across a new layout */
	unsigned int	chargeTimer1fDelay;	/* PCH-1000: timer 0x1f's, less 120 ticks */
	unsigned char	batterySetting4x;	/* cookieCtrlReg7's for a battery version[2] 0x40-0x4f */
	unsigned char	batterySetting5x;	/* and 0x50 or above */
#if FW_SINCE(0x01000306)
	unsigned char	touchDelaySelect;	/* Touch's delays, in NOPs */
	unsigned char	touchDelayByte;
	unsigned char	touchDelayDeselect;
	unsigned char	touchWriteTries;	/* a verified write's tries */
	unsigned char	touchWriteReads;	/* and the reads each try compares */
#if SOURCE_0103
	unsigned char	touchDelayReadByte;	/* USS-1001's delay after each byte panel 0 sends
						   back */
#else
	unsigned char	reserved6;
#endif
	unsigned int	hostBusyLimit;		/* the host link's busy time before a pause */
	unsigned int	hostIdleLimit;		/* the idle time that clears the busy time */
	unsigned int	hostPause;		/* the pause, in ticks */
#if BOARD_IRS1001
	unsigned char	voltage81;		/* IRS-1001's regulators 0x81-0x84: their levels at a
						   resume */
	unsigned char	voltage82;
	unsigned char	voltage83;
	unsigned char	voltage84;
	unsigned char	reserved42;
	unsigned char	voltageTrim;		/* passed with a regulator's level (0x088e) */
#else
	unsigned char	reserved28[0x1c2 - 0x1bc];
#endif
	unsigned int	offTransitionBlock;	/* not 0: no transition 0x18 (offTransition18) */
	unsigned char	offTransition18:1;	/* set: the off state entered by transition 0x31
						   runs transition 0x18 */
	unsigned char	reserved29:7;
#if FW_SINCE(0x0100060B)
	unsigned char	reserved33;
	int		chargeAlarmAdjust[3][4];	/* added to each alarm's limits
							   (chargeAlarmUpdate), by the
							   parameter block's alarm level */
	unsigned char	offInfo2;		/* sleepFactorGetOffInfo's bytes 2 and 3 */
	unsigned char	offInfo3;
#if SOURCE_0103
#if BOARD_DOL1001 && !BOARD_DOL1002
	unsigned char	reserved50[5];		/* DOL-1001 keeps its levels at 0xee */
#else
	unsigned char	voltage81;		/* USS-1001's regulators 0x81-0x84: their levels at a
						   resume */
	unsigned char	voltage82;
	unsigned char	voltage83;
	unsigned char	voltage84;
	unsigned char	reserved34;
#endif
	unsigned char	voltageTrim;		/* USS-1001: passed with a regulator's level
						   (0x088e) */
	unsigned char	chargeUsbCurrents[8][6];	/* USS-1001: the USB supply's settings by the
							   USB status and chargeUsbMode
							   (chargeUsbCurrent); 0xff none */
	unsigned int	analogInput10Interval;	/* USS-1001: the RTC's ticks between starts of
						   analog input 0x0010 (input.c's scan) */
#if BOARD_DOL1001
	unsigned char	reserved44[0x21a - 0x218];
	unsigned char	resumeTrim;		/* DOL-1001: the trim of regulators 0x81-0x83 at a
						   resume (powerSeqLevels) */
	unsigned char	resumeTrim84;		/* and of regulator 0x84 */
#else
	unsigned char	reserved44[0x21c - 0x218];
#endif
	union {
		unsigned char	value;
		struct {
			unsigned char	manualOffCold:1;	/* USS-1001: set: the manual charge
								   mode off when the cold range stops
								   the charge (chargeTempUpdate) */
			unsigned char	manualCharge:1;	/* and set: transition 0x0f also in the
							   manual charge mode (chargeUpdate) */
			unsigned char	reserved47:6;
		} bits;
	} charge;
	unsigned char	reserved48;
	unsigned char	manualChargingCurrent;	/* USS-1001: the manual charge mode's Cookie
						   settings (chargeSetManualMode) */
	unsigned char	manualReg8;
	unsigned char	manualSupplyCurrent;
	unsigned char	manualSupplyCurrentUsb;
	unsigned int	chargeBoostTime;	/* USS-1001: in seconds, the longest the boost runs
						   after a battery goes in charging (chargeUpdate) */
	unsigned int	chargeBoostVolt;	/* and the battery's voltage that ends it sooner */
	unsigned char	touchDelayWriteByte;	/* USS-1001: the delay after each byte panel 0 is
						   written */
	unsigned char	touchDelayRelease;	/* and after its deselect */
#if BOARD_DOL1001 || SOURCE_0104
	unsigned char	thermalPeriod;		/* DOL-1001: the temperature statistics' ticks
						   between samples (thermalStatsInit) */
	unsigned char	reserved49;
	unsigned int	thermalMaxSamples;	/* and their samples' maximum */
	unsigned int	thermalStart0;		/* each mode's ticks before its first sample, */
	unsigned int	thermalLimit0;		/* its last tick and the samples before it is */
	unsigned int	thermalThreshold0;	/* answered (struct ThermalStats) */
	unsigned int	thermalStart1;
	unsigned int	thermalLimit1;
	unsigned int	thermalThreshold1;
#else
	unsigned char	reserved43[0x238 - 0x228];
#endif
	unsigned char	chargeDongleCurrent;	/* USS-1001: the USB supply's setting with the
						   multi-connector's dongle or debug board */
#if SOURCE_0104
	unsigned char	batterySetting;		/* DOL-1002: cookieCtrlReg7's for a battery version
						   it does not know */
	unsigned char	reserved51[0x25c - 0x23a];
	struct ConfigWakeMasks wakeMasksHigh;	/* 0x0104: wake factors 0x20-0x3f's */
	unsigned char	reserved52;
#endif
	unsigned char	analogLimitScale;	/* USS-1001: a calibrated axis's limits are its
						   calibration's widened by range * this / 100 */
	unsigned int	analogLimitMargin;	/* plus this (analogLimitsSet) */
	unsigned int	analogLimitMin;		/* and kept within these, an axis's limits */
	unsigned int	analogLimitMax;		/* before it is calibrated */
#if BOARD_USS1001 && SOURCE_0104
	unsigned char	reserved53[0x276 - 0x274];
	unsigned int	batteryCalibration0;	/* the slim's 0x0105: the battery's two
						   calibration words, 0x7fff none (kept across a
						   new layout) */
	unsigned int	batteryCalibration1;
	unsigned char	reserved46[0x3e0 - 0x27a];
#elif SOURCE_0104
	unsigned char	reserved46[0x3e0 - 0x274];
#else
	unsigned char	reserved46[0x3e0 - 0x240];
#endif
#else
	unsigned char	reserved34[0x3e0 - 0x1e0];
#endif
	unsigned char	offInfoBit0;		/* not 0: sleepFactorGetOffInfo's bit 0 */
	unsigned char	offInfoBit1;		/* and its bit 1 */
	unsigned char	reserved30[CONFIG_SIZE - 0x3e2];
#else
	unsigned char	reserved30[CONFIG_SIZE - 0x1c5];
#endif
#else
	unsigned char	reserved30[CONFIG_SIZE - 0x1b0];
#endif
#else
	unsigned char	reserved32[CONFIG_SIZE - 0x15a];
#endif
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

extern struct ConfigBank configBuffer;

extern int configInit(void);
extern struct ConfigBlock *configGet(void);
extern unsigned char configGetVersions(struct ConfigVersions *versions);
extern unsigned char configRead(unsigned int offset, unsigned char size, unsigned char *data);
extern unsigned char configWrite(unsigned int offset, unsigned char size, unsigned char *data);
extern unsigned char configOpen(void);
extern unsigned char configCommit(void);
#if FW_SINCE(0x00090804)
extern unsigned char configWriteRecords(unsigned int offset, unsigned char size,
					unsigned char *data, unsigned char check);
#else
/* 0x00090206: write records applied to the open copy as they come, no digest */
extern unsigned char configWriteRecords(unsigned char *records, int length);
#endif
extern unsigned char configClose(void);
extern unsigned char configPrepareUpdate(void);
extern unsigned int configUpdateSegment(void);
extern unsigned char configMergeUpdate(void);
extern unsigned char configTakeChanged(void);
extern unsigned char configFlashWrite(unsigned long address);
extern unsigned char configRestoreDefaults(void);

#endif
