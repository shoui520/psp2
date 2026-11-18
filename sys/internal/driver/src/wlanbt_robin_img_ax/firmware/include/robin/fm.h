/*
 * The FM radio of the 88W8787's Bluetooth side (0x494f4..0x49d3c): an mdev
 * driver named "fm" whose task takes the FM commands the host sends as
 * Bluetooth vendor commands, answers each through the controller's event
 * hook, and turns the FM block's interrupt (line 27) into events for the host.
 *
 * Marvell published no source; names are inferred from use.
 */
#ifndef ROBIN_FM_H
#define ROBIN_FM_H

#include <stdint.h>

/* The FM interrupt line and its bit in the SoC interrupt registers. */
#define FM_IRQ_LINE		27
#define FM_IRQ_MASK		(1u << FM_IRQ_LINE)
#define FM_IRQ_SECONDARY_MASK	(1u << 26)

/* The FM block's registers. */
#define FM_CONTROL		REG16(0x9000d804)	/* bit 14: the interrupt is armed */
#define FM_STATUS		REG16(0x9000d84c)	/* what the block reports */
#define FM_INTERRUPT_ENABLE	REG16(0x9000d854)

#define FM_CONTROL_ARMED	0x4000

/* FM_STATUS bits; their names follow the handler each one runs and are not confirmed. */
#define FM_STATUS_TUNED		0x0002
#define FM_STATUS_SEARCH	0x0010
#define FM_STATUS_RDS		0x0020
#define FM_STATUS_LEVEL		0x0cc0
#define FM_STATUS_STEREO	0x8000

/* The answer to a command, or an event, as the controller's event hook sends it. */
struct FmReply {
	int8_t status;		/* 0, or -1 when result holds an error */
	uint8_t command;	/* the command answered, or the event code */
	uint8_t result;
	uint8_t data[249];
};

_Static_assert(sizeof(struct FmReply) == 252, "FM reply");

/* The kinds of FmReply the event hook sends. */
#define FM_REPLY_EVENT		2
#define FM_REPLY_COMMAND	3

/* Events the FM block raises (FmReply.command). */
#define FM_EVENT_RDS		0x81
#define FM_EVENT_SEARCH		0x82
#define FM_EVENT_LEVEL		0x83
#define FM_EVENT_SEARCH_DONE	0x86

/* The RDS buffer's read and write positions (FmSettings.rdsIndex). */
struct FmRdsIndex {	/* one word of bit fields: Sony reads the positions with a word load */
	uint32_t read : 8;
	uint32_t write : 8;
	uint32_t reserved : 16;
};

/* FmSettings.rdsFlags. */
#define FM_RDS_RECEIVING	0x80
#define FM_RDS_PENDING		0x100		/* the buffer holds groups */
#define FM_RDS_MODE(flags)	((flags) >> 4 & 3)
#define FM_RDS_SIZE(flags)	((flags) >> 9 & 0xff)	/* in blocks */
#define FM_RDS_TIMEOUT		0x199a		/* event clock ticks between groups */

/*
 * The FM configuration the host gives. Only the words this file reads are
 * named; the others belong to the FM settings code (0x4f514..), named when it
 * is written.
 */
struct FmSettings {
	uint32_t referenceClock;	/* btSbcInspectEntry() */
	uint32_t vendorSetting;	/* vendor command 0x5a's value (btSbcInspectTable()) */
	uint32_t reserved0;
	uint32_t spacing;	/* kHz between the channels a search steps through */
	uint32_t frequency;	/* kHz, the frequency tuned to (btSbcAcquireEntry()) */
	uint32_t deviation;	/* the transmitter's total deviation, 22500..100000 (btSbcFormatBuffer()) */
	uint32_t eventStatus;	/* bit 7: an RDS group matched block A (btSbcClearTable()) */
	uint32_t eventMask;	/* events the host wants (fmCoexApplyState()) */
	uint32_t frequencyOffset;	/* kHz, added before the local oscillator is set */
	uint16_t searchThreshold;	/* 15: how far a channel must stand above a neighbour */
	uint16_t reserved10;
	uint32_t features;	/* bits 4, 5 and 13 are read here */
	/* one word of bit fields: Sony reads the level with a word load */
	uint32_t audioLimit : 8;	/* the highest level btSbcScheduleBuffer() raises audioSetting to */
	uint32_t audioSetting : 8;	/* the transmit level, 99 once the radio is started: what btFmApplyHook() is given */
	uint32_t reserved18 : 16;
	uint32_t tuneFlags;	/* FM_TUNE_* */
	uint32_t levelFlags;	/* bit 14: the level reads 56 high (btSbcParseTable()) */
	uint32_t reserved17;
	uint32_t policy;	/* bits 7..4: when the interrupt is rearmed, bit 17: level events */
	uint32_t reserved3[2];
	uint32_t audioLevel;	/* bits 24..17: the level btSbcPrepareTable() sets, given to the ROM from bit 1 */
	uint32_t rdsBlockBMatch;	/* bits 15..0 the value, 31..16 the mask */
	uint32_t rdsBlockAMatch;	/* the same, for block A */
	uint32_t rdsFlags;		/* FM_RDS_* */
	struct FmRdsIndex rdsIndex;
};

_Static_assert(sizeof(struct FmSettings) == 0x5c, "FM settings");

/* A handler of the FM task's event flags, and the flags it takes. */
struct FmEventHandler {
	int (*handler)(unsigned int index);
	uint32_t mask;
};

struct OsaBufferPool;

extern struct FmReply fmReply;					/* SQRAM 0xc0033328 */
extern void *fmDevice;						/* 0x5534c: the "fm" device the vendor commands write to */
extern struct OsaBufferPool fmPool;				/* 0x55360: the FM commands' buffers */
extern uint32_t fmFlags[22];					/* SQRAM 0xc0033438: the FM task's event flags */
extern struct FmSettings fmSettings;				/* SQRAM 0xc0033490 */
/* The controller's event sender, set by btSbcQueryEntry(). */
extern int (*fmEventSend)(struct FmReply *reply, unsigned int length, unsigned int kind); /* DTCM 0x400f2f4 */

/* bt/fm.c */
int btLmpLcDispatchDescriptor(int line, uint32_t pending);	/* 0x494f4 */
void btLmpLcParseDescriptor(uint32_t status);			/* 0x4952a */
int fmCoexTaskMain(void);					/* 0x4957c */
int fmCoexHandleState(void);					/* 0x49600 */
int fmCoexProcessState(void *device, void *message, uint32_t argument, uint32_t type); /* 0x4960a */
void fmCoexUpdateState(void);					/* 0x49624 */
int fmInit(void);						/* 0x4966a */
int fmCoexConfigureState(unsigned int index);			/* 0x496e6 */
int fmCoexCheckState(void);					/* 0x49b9c */
int fmCoexParseState(unsigned int index);			/* 0x49bea */
int fmCoexBuildState(unsigned int index);			/* 0x49c22 */
int fmCoexComputeState(unsigned int index);			/* 0x49ca0 */
int fmCoexQueryState(unsigned int index);			/* 0x49ce2 */
int fmCoexResetState(unsigned int index);			/* 0x49d06 */
int fmCoexSyncState(unsigned int index);			/* 0x49d1a */
int fmCoexApplyState(uint32_t mask);				/* 0x49d26 */

/*
 * The FM tuner's state (bt/fmtune.c, 0x55694). The fields whose readers are
 * not converted yet are "reserved" until they are.
 */
struct FmTuner {
	uint8_t quietSearch;		/* 1: a search looks for the quietest channel */
	uint8_t savedRate;		/* the policy's bits 7..4 while transmitting */
	uint8_t savedLevel;		/* its bit 18 */
	uint8_t savedMode;		/* its bits 20..19 */
	uint8_t mode;			/* FM_MODE_*, btSbcFormatTable() */
	uint8_t measureGain;		/* 2: the gain register 0x23c takes while btSbcUpdateTable() measures */
	uint8_t measureTime;		/* 10: the time of one measurement, in ms */
	uint8_t reserved12;
	uint16_t savedLevels[2];	/* two level registers while btSbcProcessTable() overrides them */
	uint16_t savedOutput;		/* register 0x318 while transmitting */
	int16_t measureTolerance;	/* 20: how far the calibration may be off */
	uint16_t measureSpread;		/* 15: how far a measurement may be from the others */
	int16_t calibratedTolerance;	/* 10: the tolerance once a search has calibrated */
	uint8_t filterCodes[4];		/* 0x0f, 0x17, 0x1f: the PCM port's code, by the policy's bits 24..23 */
	uint32_t idle;			/* 1 while the FM task waits for a command (btSbcConvertTable()) */
	uint32_t tuneDone;		/* set when a tune or search completes */
	const uint32_t (*filters)[3];	/* ROM: 16 sets of filter words, by frequency */
	uint32_t rdsLastClock;		/* the event clock at the last RDS group */
	uint32_t searchCalibrate;	/* 1: a search calibrates the offset at the first station */
	uint32_t measureScale;		/* 1 after btSbcClearEntry(): which scale btSbcUpdateTable() uses */
	uint8_t levelBase;		/* 30 after btSbcClearEntry(): subtracted from register 0x2b0, at least 2 */
	uint8_t levelMode;		/* 1: the gain is fixed at its highest (btSbcValidateBuffer()) */
	uint8_t antennaScan;		/* 1 after btSbcClearEntry(): btSbcCheckBuffer() scans register 0x160 */
	uint8_t antennaOffset;		/* at most 17: added to the frequency step's setting */
	uint32_t searchCount;
	uint32_t rdsMissed;		/* searches that saw no RDS group */
	uint32_t rdsReceived;		/* searches that saw one */
	uint32_t searchLimit;
};

_Static_assert(sizeof(struct FmTuner) == 0x44, "FM tuner state");

extern struct FmTuner fmTuner;					/* 0x55694 */

/* The FM block's registers, by their offset from 0x9000d800. */
#define FM_REGISTER(offset)	REG16(0x9000d800 + (offset))
/* The event clock (SOC_EVENT_CLOCK): the FM code reads it at its fixed address, not through sysEventRegisterBase. */
#define FM_EVENT_CLOCK		REG32(0x80002434)
#define FM_TUNE_POLLS		500	/* of 66 clock ticks */
#define FM_TUNE_TIMEOUT		42

/* bt/fmtune.c */
int btSbcComputeEntry(struct FmReply *reply, unsigned int length, unsigned int kind);	/* 0x4f4fc */
void btSbcSyncEntry(void);					/* 0x4f544 */
int btSbcApplyEntry(uint32_t ticks);				/* 0x4f5d0 */
int btSbcResolveEntry(int search);				/* 0x4f5e4 */
void btSbcScheduleEntry(int enable);				/* 0x4f686 */
int btSbcNotifyEntry(int enable);				/* 0x4f734 */
int btSbcFormatEntry(uint32_t frequency);			/* 0x4f752 */
uint16_t btSbcInspectEntry(uint32_t value);			/* 0x4f7a6 */
void btSbcTrackEntry(uint32_t value);				/* 0x4f7ca */
int btSbcManageEntry(uint32_t vco);				/* 0x4f804 */
int btSbcAcquireEntry(uint32_t frequency);			/* 0x4f87c */
uint32_t btSbcSetupEntry(uint32_t clock);			/* 0x4fa7e */
void btSbcConvertEntry(void);					/* 0x4fb62 */
void btSbcVerifyEntry(void);					/* 0x4fc02 */
extern uint16_t fmRdsBlocks[256];				/* SQRAM 0xc00334f8: not converted yet */
void btSbcFilterTable(void);					/* 0x50e62 */
int btSbcRomConvertEntry(uint32_t event);
void btSbcCheckBuffer(uint32_t frequency);			/* 0x50f56 */
void btSbcApplyBuffer(uint32_t value, int swap);		/* 0x511d6 */
extern uint8_t btFmLevelThreshold;				/* 0x55740: the level btSbcResolveBuffer() finds */
void btSbcRomSetupContext(uint32_t clock);
void btSbcRomAcquireEntry(unsigned int band, uint8_t *out);
void btSbcRomReleaseContext(void);
extern uint16_t (*btFmClockHook)(uint32_t value);		/* DTCM 0x400ef08 */
extern void (*btFmTimerHook)(uint32_t value);			/* DTCM 0x400ef24 */
extern int (*btFmStereoHook)(int enable);			/* DTCM 0x400eef4 */
extern int (*btFmWaitHook)(int search);				/* DTCM 0x400ef28 */
extern void (*btFmApplyHook)(uint32_t value, int swap);			/* DTCM 0x400eee4 */
extern int (*btFmFrequencyHook)(uint32_t frequency);		/* DTCM 0x400eef8: sets the oscillator */

/* The intermediate frequency (Hz) the local oscillator sits above or below the station. */
#define FM_INTERMEDIATE		132300
#define FM_VCO_LIMIT		1550000000u

void btSbcUpdateBuffer(uint32_t frequency);			/* 0x50f2c */
void btSbcRecordTable(int enable);				/* 0x50e4c */
void btSbcRomHandleEntry(uint32_t value);
int btSbcRomCheckEntry(uint32_t vco);
extern void (*btAudioPresetHook)(int enable, unsigned int mode);			/* DTCM 0x400f2f8 */
extern void (*btAudioClockHook)(int enable, unsigned int mode);			/* DTCM 0x400f2fc */
extern void (*btFmEnableHook)(int enable, unsigned int kind);			/* DTCM 0x400f300 */

/* The audio path (sqram/hooks.c, SQRAM 0xc000e274..0xc000e5a8). */
void btAudioPathModesClear(void);						/* via the veneer at 0x533d4 */
void btAudioPathPresetHook(int enable, unsigned int mode);
void btAudioPathClockHook(int enable, unsigned int mode);			/* via the veneer at 0x5306c */
void btAudioPathEnable(int enable, unsigned int kind);
void btAudioPathHooksInit(void);						/* via the veneer at 0x533dc */
uint8_t btAudioPathModesSet(unsigned int length, const uint8_t *parameters);
void btAudioPathDataInvert(unsigned int clear);					/* via the veneer at 0x53c04 */
void btAudioPathPresetApply(unsigned int mode);
extern void (*btFmInjectionHook)(uint32_t frequency, uint32_t *config);	/* DTCM 0x400eeec */
extern int (*btFmPllSetupHook)(uint32_t vco, unsigned int highSide, uint32_t *tuned);	/* DTCM 0x400ef18 */
extern int (*btFmPllDividerHook)(uint32_t tuned);				/* DTCM 0x400ef10 */
extern int (*btFmPllFractionHook)(uint32_t tuned);				/* DTCM 0x400ef14 */
extern int (*btFmFilterHook)(uint32_t tuned);					/* DTCM 0x400ef0c: btSbcFormatEntry() */
extern int (*btFmPllLockHook)(uint32_t tuned);					/* DTCM 0x400ef1c */
extern void (*btFmMixerHook)(uint32_t oscillator, unsigned int mode);		/* DTCM 0x400eef0 */
int btSbcRomNotifyContext(struct FmReply *reply, unsigned int length, unsigned int kind);
uint32_t btSbcRomAdjustContext(uint32_t mask, int set);
extern void (*btFmSearchHook)(uint32_t frequency);		/* DTCM 0x400eedc: btSbcCheckBuffer() */
extern int (*btFmTuneHook)(uint32_t frequency);		/* DTCM 0x400ef20: btSbcAcquireEntry() */
extern const uint8_t romFmTable[];			/* ROM 0x03f42070 */

/*
 * The FM settings code, not converted yet; the names are provisional until
 * its file is written. Addresses are those of the 4.00 image.
 */
void btSbcQueryEntry(void);					/* 0x4f514 */
int btSbcResetEntry(void);					/* 0x4f524 */
int btSbcReleaseEntry(void);					/* 0x4fa4a */
int btSbcClearEntry(uint8_t clock);				/* 0x4faae */
void btSbcSelectEntry(void);					/* 0x4fd42 */
void btSbcAdjustEntry(uint32_t status);				/* 0x4fd7c */
void btSbcRecordEntry(void);					/* 0x4fe84 */
void btSbcFilterEntry(void);					/* 0x4fea4 */
int btSbcHandleTable(uint32_t frequency, int up);		/* 0x4fed8 */
int btSbcUpdateTable(uint32_t frequency, int tolerance, int16_t *mean);	/* 0x4ff82 */
int btSbcConfigureTable(uint32_t frequency);			/* 0x500a4 */
int btSbcCheckTable(void);					/* 0x500fc */
int btSbcValidateTable(uint32_t frequency);			/* 0x5014c */
int btSbcPrepareTable(int mode);				/* 0x501fc */
int btSbcDispatchTable(uint32_t value);				/* 0x502d8 */
uint16_t btSbcParseTable(void);					/* 0x502fc */
void btSbcBuildTable(unsigned int value);			/* 0x50314 */
int btSbcComputeTable(uint32_t frequency);			/* 0x50328 */
void btSbcQueryTable(int16_t tolerance, int16_t calibratedTolerance, uint16_t spread, uint32_t calibrate);	/* 0x50346 */
void btSbcResetTable(uint8_t *tolerance, uint8_t *calibratedTolerance, uint8_t *spread, uint8_t *calibrate);	/* 0x50354 */
void btSbcSyncTable(uint8_t gain, uint8_t time, uint32_t scale);	/* 0x5036a */
void btSbcApplyTable(uint8_t *gain, uint8_t *time, uint8_t *scale);	/* 0x50374 */
void btSbcResolveTable(int enable);				/* 0x50386 */
void btSbcBuildBuffer(void);					/* 0x510ba */

/* fmSettings.tuneFlags */
#define FM_TUNE_OFFSET		0x80000000u	/* the frequency offset is calibrated */

/* The FM bands' limits in kHz (ROM), by bits 3..0 of fmSettings.features. */
struct FmBand {
	uint32_t low;
	uint32_t high;
};
extern const struct FmBand romFmBands[];			/* ROM 0x03f42050 */

#define FM_EVENT_VALUE		0x88	/* btSbcDispatchTable()'s event */

void btSbcRomConfigureEntry(unsigned int mode);
void btSbcRomProcessEntry(unsigned int level);
void btSbcProcessTable(int enable);				/* 0x4ff0e */
void btSbcResolveBuffer(void);					/* 0x51264 */
void btSbcScheduleBuffer(void);					/* 0x512c2 */
void btSbcRomTrackEntry(void);
int btSbcRomFilterContext(void);
int btSbcScheduleTable(uint32_t frequency, uint32_t *station, uint8_t mode);	/* 0x503b4 */

/* fmSettings.levelFlags: bits 30..15, the lowest level a station may have. */
#define FM_LEVEL_FLOOR(flags)	((flags) << 1 >> 16)
#define FM_SEARCH_RING		9	/* the levels a search keeps around the channel */
#define FM_SEARCH_PROBES	4

extern int (*btFmResetHook)(void);					/* DTCM 0x400eefc */
extern int (*btFmTuneModeHook)(unsigned int mode, uint32_t frequency);	/* DTCM 0x400ef00 */
struct SbcPcmStream;
void btSbcNotifyTable(struct SbcPcmStream *config);		/* 0x508be */
int btSbcFormatTable(uint8_t mode);				/* 0x5096a */

/* fmTuner.mode: what btSbcFormatTable() starts the radio for; 0 stops it. */
#define FM_MODE_SCHEDULED	0x01	/* the ROM schedules the receiver (btSbcRomScheduleEntry()) */
#define FM_MODE_RECEIVE		0x04
#define FM_MODE_TRANSMIT	0x08

/* fmSettings.levelFlags bit 31: vendor command 0x5a's flag. */
#define FM_LEVEL_VENDOR		0x80000000u

/* What btSbcSelectTable() returns (in r0: a 4-byte struct). */
struct FmClockSelection {
	uint8_t code;
	uint8_t flags;
	uint8_t unset[2];	/* never written */
};

/* The PCM port's codes by frequency (btSbcSelectTable()): the first entry whose limit is not below it. */
struct FmClockCode {
	uint16_t limit;		/* the frequency in 8 kHz units */
	uint8_t code;
	uint8_t reserved;
};
#define FM_CLOCK_CODES		43
extern const struct FmClockCode fmClockCodes[FM_CLOCK_CODES];	/* 0x54342 */

void btSbcRomResetEntry(void);
void btSbcRomScheduleEntry(int enable);
void btSbcRomReleaseEntry(unsigned int scheduled, uint32_t mode);
void btSbcRomClearEntry(void);
void btSbcSyncBuffer(uint32_t value, int raise);		/* 0x511ac */
int btSbcInspectTable(uint32_t value, uint8_t mode);		/* 0x50be2 */
int btSbcTrackTable(uint16_t *value, uint8_t *active);		/* 0x50c16 */
int btSbcAcquireTable(uint16_t *value, uint8_t *active);	/* 0x50c84 */
int btSbcReleaseTable(uint32_t *value);				/* 0x50cac */
int btSbcSetupTable(uint32_t *value);				/* 0x50ce8 */
int btSbcClearTable(uint32_t value, uint32_t *result, uint8_t mode); /* 0x50d24 */
void btSbcConvertTable(int enable);				/* 0x50dfc */
int btSbcVerifyTable(void);					/* 0x50e22 */
struct FmClockSelection btSbcSelectTable(uint32_t frequency);			/* 0x50e28 */
uint16_t btSbcManageTable(void);				/* 0x50c60 */
void btSbcAdjustTable(uint8_t value);				/* 0x50e46 */
int btSbcHandleBuffer(int mode);				/* 0x50ea4 */
void btSbcConfigureBuffer(uint8_t scan, uint8_t offset);	/* 0x50f44 */
int btSbcValidateBuffer(uint8_t value);			/* 0x51036 */
uint8_t btSbcPrepareBuffer(void);				/* 0x5106e */
int btSbcDispatchBuffer(uint8_t value);			/* 0x51086 */
uint8_t btSbcParseBuffer(void);					/* 0x510b4 */
void btSbcComputeBuffer(uint32_t limit);			/* 0x510f2 */
int btSbcQueryBuffer(uint8_t *data, uint8_t *length);		/* 0x51100 */
int btSbcNotifyBuffer(uint8_t value);				/* 0x5135e */
int btSbcFormatBuffer(uint32_t first, uint32_t second, uint32_t third, uint32_t tone);	/* 0x513f0 */

/*
 * The gain steps (0x556d8): the gain register 0x2b0's level, less
 * fmTuner.levelBase, picks the first step whose high end is not below it.
 */
struct FmGainStep {
	uint32_t low : 8;
	uint32_t high : 8;
	uint32_t gain : 7;		/* given to the ROM */
	uint32_t reserved : 9;
};
#define FM_GAIN_STEPS		17
extern struct FmGainStep fmGainSteps[FM_GAIN_STEPS];		/* 0x556d8 */

/* The frequency steps (0x5571c): the first whose limit is not below the frequency. */
struct FmFrequencyStep {
	uint16_t limit : 14;		/* the frequency in 8 kHz units */
	uint16_t reserved0 : 2;
	uint8_t setting : 5;		/* what btSbcResetBuffer() sets register 0x160 to: Sony reads its byte */
	uint8_t reserved1 : 3;
	uint8_t reserved2;
};
#define FM_FREQUENCY_STEPS	9
extern struct FmFrequencyStep fmFrequencySteps[FM_FREQUENCY_STEPS];	/* 0x5571c */

/*
 * The transmit levels 125 down to 99 (0x543f0, btSbcInspectBuffer()): four
 * output bytes for registers 0x398 and 0x39c, and the settings of registers
 * 0x394 (bits 4..0), 0x390 (bits 9..5 and 18..16) and 0x38c (bits 15..10).
 */
struct FmLevelStep {
	uint32_t outputs;
	uint32_t settings;
};
#define FM_LEVEL_HIGH		125
#define FM_LEVEL_LOW		99
extern const struct FmLevelStep fmLevelSteps[FM_LEVEL_HIGH - FM_LEVEL_LOW + 1];	/* 0x543f0 */

const struct FmFrequencyStep *btSbcProcessBuffer(uint32_t frequency);	/* 0x50f14 */
void btSbcResetBuffer(unsigned int setting);			/* 0x51164 */
const struct FmLevelStep *btSbcInspectBuffer(uint32_t level);	/* 0x51552 */
void btSbcRomPrepareEntry(uint32_t low, uint32_t high, uint32_t base);
void btSbcRomDispatchEntry(uint32_t first, uint32_t second);

#endif /* ROBIN_FM_H */
