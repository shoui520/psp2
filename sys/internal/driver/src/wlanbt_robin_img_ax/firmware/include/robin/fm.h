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
	uint32_t reserved0[7];
	uint32_t eventMask;	/* events the host wants (fmCoexApplyState()) */
	uint32_t reserved1[2];
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
extern void (*fmEventSend)(struct FmReply *reply, unsigned int length, unsigned int kind); /* DTCM 0x400f2f4 */

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
 * The FM settings code, not converted yet; the names are provisional until
 * its file is written. Addresses are those of the 4.00 image.
 */
extern uint32_t btSbcSearchCount;				/* 0x556c8 */
extern uint32_t btSbcSearchLimit;				/* 0x556d4 */
extern uint8_t btSbcLevelMode;					/* 0x556c5 */
void btSbcQueryEntry(void);					/* 0x4f514 */
void btSbcResetEntry(void);					/* 0x4f524 */
void btSbcReleaseEntry(void);					/* 0x4fa4a */
int btSbcClearEntry(uint8_t mode);				/* 0x4faae */
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
