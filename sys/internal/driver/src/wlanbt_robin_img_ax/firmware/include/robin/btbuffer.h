/*
 * The link controller's host link buffers (bt/lcbuffer.c), and the ROM's host link state they use.
 * The ROM's names are provisional; their addresses are in boot/rom_syms.ld.
 */
#ifndef ROBIN_BTBUFFER_H
#define ROBIN_BTBUFFER_H

#include <stdint.h>

/* The host link's timing in the ROM (0x400c5f0), scaled with the radio's rate. */
struct BtHostTiming {
	uint8_t enabled;			/* 1: the timing follows the rate */
	uint8_t scale;				/* the rate bits of the link controller */
	uint8_t policy;
	uint8_t reserved3[5];
	uint32_t ticks[5];
	uint8_t slots[2];
	uint8_t reserved30[2];
};
_Static_assert(sizeof(struct BtHostTiming) == 32, "BtHostTiming");

/* A link's slots, as the last host request was given them (0x400cd10). */
struct BtSlotRequest {
	uint8_t active;
	uint8_t reserved1;
	uint16_t link;				/* the host link */
	uint8_t offset;				/* the first slot, in half slots */
	uint8_t period;				/* in half slots */
	uint8_t wrap;				/* 2: the clock had wrapped past 0x2000000 */
	uint8_t reserved7;
	uint16_t packet;			/* the packet kinds */
	uint8_t mode;
	uint8_t extra;				/* the extra slots, in halves */
};
_Static_assert(sizeof(struct BtSlotRequest) == 12, "BtSlotRequest");

/* A link's last eight rate classes, in a ring (0x59f08). */
struct BtRateHistory {
	uint8_t head;
	uint8_t samples[8];
};

/* The ROM's host link policy (0x400bb80). */
struct BtHostPolicy {
	uint8_t reserved0[16];
	uint8_t state;
	uint8_t reserved17[11];
	uint8_t link;
	uint8_t role;
	uint8_t reserved30[2];
	uint32_t flags;				/* bit n: step n of the role switch is done here */
	uint8_t reserved36;
	int8_t step;				/* the step of the role switch */
	uint8_t reserved38;
	uint8_t pending;			/* 1: a step is being handled */
	uint8_t busy;
	uint8_t reserved41[3];
};
_Static_assert(sizeof(struct BtHostPolicy) == 44, "BtHostPolicy");

/* The timing of one of the device context's two slot kinds. */
struct BtSlotTiming {
	uint16_t buffer;
	uint16_t packet;			/* the ticks of the kind's window */
	uint8_t frame;
	uint8_t reserved5;
};
_Static_assert(sizeof(struct BtSlotTiming) == 6, "BtSlotTiming");

/* The steps one of the slot kinds has taken (btLmpLcHandleDescriptor() takes one). */
struct BtStepRecord {
	uint8_t reserved0[20];
	uint32_t steps;				/* 9 or more: the kind's mode 7 slot may move */
	uint8_t reserved24[4];
};
_Static_assert(sizeof(struct BtStepRecord) == 28, "BtStepRecord");

/*
 * The ROM's device context (0x400c7b4) whose bitmap event btLmpLcNotifyBuffer() passes on. Its two
 * slot kinds: 0 for most links, 1 for a profile whose stepKind is 1.
 */
struct BtDeviceContext {
	uint8_t reserved0[4];
	uint8_t entry;
	uint8_t reserved5;
	struct BtSlotTiming timings[2];		/* 6 */
	uint8_t reserved18[2];
	struct BtStepRecord steps[2];		/* 20 */
	uint32_t queue;				/* 76 */
};
_Static_assert(sizeof(struct BtDeviceContext) == 80, "BtDeviceContext");

extern struct BtRateHistory btRateHistory[8];			/* 0x59f08 */
extern struct BtHostPolicy btHostPolicy;			/* 0x400bb80 */
extern struct BtDeviceContext btDeviceContext;			/* 0x400c7b4 */
extern uint32_t btPeerPolicy;					/* 0x400f62c: the peer's flags of the role switch */
extern uint32_t (*btRoleSource)(uint8_t *source);		/* 0x400f03c */
extern int (*btRoleCommand)(int8_t factor, int allow);		/* 0x400f08c */
extern int (*btLinkLimitQuery)(void);				/* 0x400f0d0 */
extern void (*btLinkLimitPacket)(void);				/* 0x400f0e0 */
extern void (*btLinkLimitFrame)(void);				/* 0x400f0e4 */
extern uint8_t btPeerPending;					/* 0x400f614 */
extern uint8_t btPeerLimit;					/* 0x400f616 */
extern uint8_t btPeerLimitPending;				/* 0x400f615: a link did not settle at the limit */
extern unsigned int (*btLinkPolicyHook)(unsigned int link, unsigned int limit);	/* 0x400f080 */
extern void (*btLinkProfileHook)(unsigned int link);		/* 0x400f084 */
extern unsigned int (*btLinkEventHook)(unsigned int policy);	/* 0x400f088 */
extern void (*btLinkPayloadHook)(void);				/* 0x400f0a0 */
extern void (*btLinkRegisterHook)(unsigned int link);		/* 0x400f0a8 */
extern int (*btLinkRecordHook)(unsigned int link);		/* 0x400f0ac */
extern void (*btLinkStepHook)(struct BtStepRecord *step, const struct BtSlotTiming *timing, uint32_t clock,
			      int flags);				/* 0x400f250 */
extern uint8_t btTargetRepeat;					/* 0x400de4f: cleared when a mode 11 slot repeats */
extern uint8_t btHostSetting;					/* 0x400bd43 */
extern uint8_t btHostRoleNow;					/* 0x400bd4e */
extern uint8_t btHostRoleWanted;				/* 0x400bd4f */
extern uint32_t (*btRoleRecord)(uint8_t *record);		/* 0x400f038 */
extern void (*btRoleUpdate)(uint32_t from, uint32_t to);	/* 0x400f050 */
extern int (*btRoleAllowed)(void);				/* 0x400f014 */
extern uint16_t btLinkPendingRate;				/* 0x400f812: 0xffff: none */
extern void (*btNextRate)(int kind, int value);			/* 0x400ed34 */
extern void (*btNextBitmap)(int kind, uint32_t queue, int value, uint8_t frame, uint16_t packet,
			    uint16_t buffer);			/* 0x400ed30 */
extern struct BtHostTiming btHostTiming;			/* 0x400c5f0 */
extern uint8_t btLinkChannelState;				/* 0x400f72f */
extern uint8_t btLinkPendingSlot;				/* 0x400f72c: a slot to give back, 0: none */
extern uint8_t btLinkHostState[];				/* 0x400f740: the ROM's host link lists */
extern uint8_t *btLinkSlotTable;				/* 0x400f914: 8 bytes per slot */
extern uint16_t *btLinkSlotMask;				/* 0x400f904 */
extern void (*btLinkEndHook)(void);				/* 0x400edac */
extern void (*btLinkLimitHook)(uint8_t first, uint8_t second);	/* 0x400f1d0 */
extern void (*btNextWindow)(int link, uint32_t clock, uint32_t interval);	/* 0x400ed24 */
extern void (*btNextControl)(int kind, int link);		/* 0x400ecfc */
extern int (*btLinkSessionCheck)(int link);			/* 0x400f268 */
extern int (*btNextSession)(int link);				/* 0x400ed04: the clock of the link's next slot */
extern uint8_t btLinkSlotFactor;				/* 0x400f617 */
extern uint8_t btLinkSpanEnabled;				/* 0x400f7f2 */
extern uint8_t btLinkPayloadRegister;				/* 0x400f7fc */
extern int (*btLinkWrapCheck)(unsigned int wrap);		/* 0x400f264 */
extern struct BtSlotRequest btSlotRequest;			/* 0x400cd10 */
extern struct BtSlotRequest *btTableSlotRequest;		/* 0x552c0 */
extern const uint32_t romQueueLcp;				/* ROM 0x03f41a40 */
extern const uint32_t romQueueLc;				/* ROM 0x03f41a44 */
extern const uint32_t romQueueLsc;				/* ROM 0x03f41a4c */

void btLmpLcHandleBuffer(void);					/* 0x44ca6 */
void btLmpLcProcessBuffer(void);				/* 0x44cb2 */
void btLmpLcUpdateBuffer(void);					/* 0x44db8 */
void btLmpLcConfigureBuffer(void);				/* 0x44ddc */
void btLmpLcCheckBuffer(unsigned int link);			/* 0x44ece */
int btLmpLcValidateBuffer(uint8_t *message);			/* 0x44f2c */
void btLmpLcPrepareBuffer(void);				/* 0x45066 */
int btLmpLcDispatchBuffer(uint8_t *command, const uint16_t *count, uint32_t a2, uint32_t a3, uint32_t a4,
			  uint32_t a5);				/* 0x4507e */
struct BtDeviceProfile;
void btLmpLcParseBuffer(struct BtDeviceProfile *profile, const struct BtDeviceProfile *source);	/* 0x45116 */
void btLmpLcBuildBuffer(int link);				/* 0x4514a */
void btLmpLcComputeBuffer(int link, uint32_t clock, unsigned int next, uint32_t interval);	/* 0x45220 */
void btLmpLcQueryBuffer(int link, uint32_t held);		/* 0x45246 */
void btLmpLcResetBuffer(uint8_t *command);			/* 0x452c2 */
void btLmpLcSyncBuffer(uint16_t *linkOut, uint8_t *slotOut, int clock, const uint8_t *request);	/* 0x45300 */
void btLmpLcApplyBuffer(uint8_t *request);			/* 0x456da */
void btLmpLcResolveBuffer(int link, unsigned int mode);		/* 0x458b2 */
void btLmpLcScheduleBuffer(int link, uint16_t interval);	/* 0x4591a */
int btLmpLcNotifyBuffer(uint8_t *message);			/* 0x45926 */
int btLmpLcFormatBuffer(const uint8_t *flags);			/* 0x45a56 */
void btLmpLcInspectBuffer(unsigned int link, unsigned int role);	/* 0x45ba4 */
int btLmpLcTrackBuffer(void);					/* 0x45c00 */
void btLmpLcManageBuffer(int step);				/* 0x45dd4 */
int btLmpLcAcquireBuffer(int limit, int apply);			/* 0x45ed4 */
struct BtSlotEvent;
void btLmpLcReleaseBuffer(struct BtSlotEvent *event, uint32_t clock);	/* 0x46100 */
struct BtDeviceProfile;
void btLmpLcSetupBuffer(struct BtDeviceProfile *profile, uint32_t clock, struct BtStepRecord *step,
			const struct BtSlotTiming *timing, unsigned int kind);	/* 0x4618c */
void btLmpLcClearBuffer(struct BtSlotEvent *event, uint32_t clock);	/* 0x461f0 */
void btLmpLcHandleDescriptor(struct BtStepRecord *step);		/* 0x49254 */

#endif /* ROBIN_BTBUFFER_H */
