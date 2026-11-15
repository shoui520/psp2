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

/*
 * The FM configuration the host gives. Only the words this file reads are
 * named; the others belong to the FM settings code (0x4f514..), named when it
 * is written.
 */
struct FmSettings {
	uint32_t referenceClock;	/* btSbcInspectEntry() */
	uint32_t reserved0[3];
	uint32_t frequency;	/* kHz, the frequency tuned to (btSbcAcquireEntry()) */
	uint32_t reserved9[2];
	uint32_t eventMask;	/* events the host wants (fmCoexApplyState()) */
	uint32_t frequencyOffset;	/* kHz, added before the local oscillator is set */
	uint16_t reserved1;		/* 15 after btSbcClearEntry() */
	uint16_t reserved10;
	uint32_t features;	/* bits 4, 5 and 13 are read here */
	uint32_t reserved2[4];
	uint32_t policy;	/* bits 7..4: when the interrupt is rearmed, bit 17: level events */
	uint32_t reserved3[7];
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
	uint8_t reserved0;		/* 0 after btSbcClearEntry() */
	uint8_t reserved9[4];
	uint8_t reserved10;		/* 2 after btSbcClearEntry() */
	uint8_t reserved11;		/* 10 after btSbcClearEntry() */
	uint8_t reserved12;
	uint16_t reserved1[3];
	uint16_t reserved13[3];		/* 20, 15 and 10 after btSbcClearEntry() */
	uint8_t reserved2[4];		/* 0x0f, 0x17, 0x1f */
	uint32_t reserved3;		/* 1 */
	uint32_t tuneDone;		/* set when a tune or search completes */
	const uint32_t (*filters)[3];	/* ROM: 16 sets of filter words, by frequency */
	uint32_t reserved5;
	uint32_t reserved14;		/* 1 after btSbcClearEntry() */
	uint32_t reserved15;		/* 1 after btSbcClearEntry() */
	uint8_t band;			/* 30 after btSbcClearEntry(), given to the ROM */
	uint8_t levelMode;
	uint8_t reserved7[2];		/* 1 and 0 after btSbcClearEntry() */
	uint32_t searchCount;
	uint32_t reserved8[2];
	uint32_t searchLimit;
};

_Static_assert(sizeof(struct FmTuner) == 0x44, "FM tuner state");

extern struct FmTuner fmTuner;					/* 0x55694 */

/* The FM block's registers, by their offset from 0x9000d800. */
#define FM_REGISTER(offset)	REG16(0x9000d800 + (offset))
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
int btSbcCheckBuffer(uint32_t frequency);			/* 0x50f56: not converted yet */
int btSbcApplyBuffer(uint32_t value);				/* 0x511d6: not converted yet */
extern uint8_t btFmLevelThreshold;				/* 0x55740: not converted yet */
void btSbcRomSetupContext(uint32_t clock);
void btSbcRomAcquireEntry(unsigned int band, uint8_t *out);
void btSbcRomReleaseContext(void);
extern uint16_t (*btFmClockHook)(uint32_t value);		/* DTCM 0x400ef08 */
extern void (*btFmTimerHook)(uint32_t value);			/* DTCM 0x400ef24 */
extern int (*btFmStereoHook)(int enable);			/* DTCM 0x400eef4 */
extern int (*btFmWaitHook)(int search);				/* DTCM 0x400ef28 */
extern int (*btFmApplyHook)(uint32_t value);			/* DTCM 0x400eee4 */

/* The intermediate frequency (Hz) the local oscillator sits above or below the station. */
#define FM_INTERMEDIATE		132300
#define FM_VCO_LIMIT		1550000000u

int btSbcUpdateBuffer(uint32_t frequency);			/* 0x50f2c: not converted yet */
void btSbcRecordTable(int enable);				/* 0x50e4c: not converted yet */
void btSbcRomHandleEntry(uint32_t value);
int btSbcRomCheckEntry(uint32_t vco);
extern void (*btFmEnableHook)(int enable, unsigned int kind);			/* DTCM 0x400f300 */
extern void (*btFmInjectionHook)(uint32_t frequency, uint32_t *config);	/* DTCM 0x400eeec */
extern int (*btFmPllSetupHook)(uint32_t vco, unsigned int highSide, uint32_t *tuned);	/* DTCM 0x400ef18 */
extern int (*btFmPllDividerHook)(uint32_t tuned);				/* DTCM 0x400ef10 */
extern int (*btFmPllFractionHook)(uint32_t tuned);				/* DTCM 0x400ef14 */
extern int (*btFmFilterHook)(uint32_t tuned);					/* DTCM 0x400ef0c: btSbcFormatEntry() */
extern int (*btFmPllLockHook)(uint32_t tuned);					/* DTCM 0x400ef1c */
extern void (*btFmMixerHook)(uint32_t oscillator, unsigned int mode);		/* DTCM 0x400eef0 */
int btSbcRomNotifyContext(struct FmReply *reply, unsigned int length, unsigned int kind);
void btSbcRomAdjustContext(uint32_t mask, int set);
extern int (*btFmSearchHook)(uint32_t frequency);		/* DTCM 0x400eedc: btSbcCheckBuffer() */
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
void btSbcConvertEntry(void);					/* 0x4fb62 */
void btSbcSelectEntry(void);					/* 0x4fd42 */
void btSbcAdjustEntry(void);					/* 0x4fd7c */
void btSbcRecordEntry(void);					/* 0x4fe84 */
void btSbcFilterEntry(void);					/* 0x4fea4 */
void btSbcCheckTable(void);					/* 0x500fc */
int btSbcValidateTable(uint32_t value);				/* 0x5014c */
void btSbcQueryTable(uint8_t first, uint8_t second, uint8_t third, uint8_t fourth); /* 0x50346 */
void btSbcResetTable(uint8_t *first, uint8_t *second, uint8_t *third, uint8_t *fourth); /* 0x50354 */
void btSbcSyncTable(uint8_t first, uint8_t second, uint8_t third); /* 0x5036a */
void btSbcApplyTable(uint8_t *first, uint8_t *second, uint8_t *third); /* 0x50374 */
int btSbcScheduleTable(uint32_t value, uint32_t *result, uint8_t mode); /* 0x503b4 */
int btSbcFormatTable(uint8_t value);				/* 0x5096a */
int btSbcInspectTable(uint32_t value, uint8_t mode);		/* 0x50be2 */
int btSbcTrackTable(uint16_t *value, uint8_t *active);		/* 0x50c16 */
int btSbcAcquireTable(uint16_t *value, uint8_t *active);	/* 0x50c84 */
int btSbcReleaseTable(uint32_t *value);				/* 0x50cac */
int btSbcSetupTable(uint32_t *value);				/* 0x50ce8 */
int btSbcClearTable(uint32_t value, uint32_t *result, uint8_t mode); /* 0x50d24 */
void btSbcConvertTable(int enable);				/* 0x50dfc */
int btSbcVerifyTable(void);					/* 0x50e22 */
uint32_t btSbcSelectTable(uint32_t value);			/* 0x50e28 */
void btSbcAdjustTable(uint8_t value);				/* 0x50e46 */
void btSbcHandleBuffer(int mode);				/* 0x50ea4 */
void btSbcConfigureBuffer(uint8_t first, uint8_t second);	/* 0x50f44 */
void btSbcValidateBuffer(uint8_t value);			/* 0x51036 */
uint8_t btSbcPrepareBuffer(void);				/* 0x5106e */
void btSbcDispatchBuffer(uint8_t value);			/* 0x51086 */
uint8_t btSbcParseBuffer(void);					/* 0x510b4 */
void btSbcComputeBuffer(uint32_t value);			/* 0x510f2 */
int btSbcQueryBuffer(uint8_t *data, uint8_t *length);		/* 0x51100 */
int btSbcNotifyBuffer(uint8_t value);				/* 0x5135e */
void btSbcFormatBuffer(uint32_t first, uint32_t second, uint32_t third, uint32_t fourth); /* 0x513f0 */

#endif /* ROBIN_FM_H */
