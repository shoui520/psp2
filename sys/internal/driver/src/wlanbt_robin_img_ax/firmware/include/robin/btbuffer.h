/*
 * The link controller's host link buffers (bt/lcbuffer.c), and the ROM's host link state they use.
 * The ROM's names are provisional; their addresses are in boot/rom_syms.ld.
 */
#ifndef ROBIN_BTBUFFER_H
#define ROBIN_BTBUFFER_H

#include <stdint.h>

struct BtDeviceProfile;
struct BtSlotEvent;
struct BtSlotQueue;

/* The parameters of HCI Setup Synchronous Connection, as the host controller passes them on. */
struct BtSyncSetup {
	uint8_t reserved0[4];
	uint16_t handle;			/* the ACL link */
	uint8_t reserved6[2];
	uint32_t transmitBandwidth;		/* bytes per second */
	uint32_t receiveBandwidth;
	uint16_t maxLatency;			/* milliseconds */
	uint16_t voiceSetting;			/* bits 0-1: the air coding format */
	uint16_t packetType;			/* the eSCO packet types allowed */
	uint8_t retransmissionEffort;		/* 0, 1, 2 or 0xff: don't care */
	uint8_t reserved23;
};

/* The synchronous frames btLmpLcHandleFrame() could not take, per synchronous link. */
struct BtSyncFrameCounts {
	uint8_t dropped[3];			/* no room in the ring, or no data */
	uint8_t partial[3];			/* taken, but too short to be handed on */
};

/* An eSCO packet type (12 bytes). */
struct BtEscoPacket {
	uint16_t type;				/* its bit in the HCI packet types */
	uint16_t length;			/* its payload, in bytes */
	uint32_t bandwidth;			/* the most it carries, bytes per second */
	uint8_t code;				/* the LMP packet type */
	uint8_t retransmission;			/* the slots a retransmission takes */
	uint8_t interval;			/* the shortest eSCO interval, in slots */
	uint8_t reserved11;
};

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
extern uint32_t (*btLinkPlaceHook)(struct BtDeviceProfile *profile, uint32_t clock, int flags);	/* 0x400ede8 */
extern uint32_t btLinkTakenClock[8];				/* 0x59ee8: the due clock each link's slot was taken at */
extern uint8_t btLinkReleaseSeen;				/* 0x400f6cb: a link record reached state 3 or 4 */
extern uint8_t btLinkReleaseHeld;				/* 0x400f6cc */
extern int (*btLinkAddressHook)(const uint8_t *address, uint16_t *link);	/* 0x400ea70 */
extern uint8_t btLinkMatchEnabled;				/* 0x400f6d2 */
extern uint8_t btLinkMatchFirst;				/* 0x400f6d3 */
extern uint8_t btLinkMatchSecond;				/* 0x400f6d4 */
extern const uint8_t *btLinkMatchPeer;				/* 0x400f6e8: its address at 4 */
extern uint32_t (*btLinkClockHook)(void);			/* 0x400eb38 */
extern int (*btLinkValidateHook)(unsigned int link, uint32_t first, uint32_t second, uint8_t *out);	/* 0x400eaa4 */
extern uint8_t btPeerReport;					/* 0x400b557: 1: report event 0x32 once */

/* The ROM's mode control, whose header byte btLmpLcParsePacket() sets. */
struct BtModeControl {
	uint8_t reserved0[3];
	uint8_t sessions;			/* the ring's size */
	uint8_t header;				/* the session written next */
	uint8_t current;			/* the session read next */
};
extern struct BtModeControl *btLinkModeControl;			/* 0x400f8f4 */

/* One of the ROM's 14 link sessions. */
struct BtLinkSession {
	uint16_t active;
	uint8_t reserved2[2];
	uint16_t flags;
	uint8_t reserved6[2];
};
extern struct BtLinkSession *btLinkSessions;			/* 0x400f8fc */
extern struct BtSyncFrameCounts btSyncFrameCounts;		/* 0x552ac */
extern void (*btSyncFrameDoneHook)(unsigned int link, uint32_t buffer);	/* 0x400ece8 */
extern uint8_t btSyncMode;					/* 0x400f714: 1 and 3: synchronous links are set up here */
extern void (*btSyncLinkUpHook)(unsigned int link);		/* 0x400f184 */
extern const struct BtEscoPacket btEscoPackets[7];		/* 0x5424c */
extern uint8_t btSyncLatency;					/* 0x400bb02: the latency allowed, in slots (126 at most) */
extern uint8_t btSyncRetransmission;				/* 0x400bb03 */
extern uint16_t btSyncPacketTypes;				/* 0x400bb04: the packet types left to try */
extern uint8_t btSyncRetryPending;				/* 0x400f5e5: 1: btSyncRetryRequest is to be tried again */
extern struct BtSyncSetup *btSyncRetryRequest;			/* 0x400f60c: the request with the types left */
extern uint8_t *btLinkSlotRegisters;				/* 0x400f93c: 32 bytes of registers per link */
extern uint16_t *btLinkControlWord;				/* 0x400f940 */
extern uint8_t *btLinkSlotStatus;				/* 0x400f944: 32 bytes of status registers per link */
extern uint8_t btLinkSyncPending;				/* 0x400f8ce: a bit per link */
extern uint32_t btQueueSyncReport;				/* 0x400f720: where message 0x1f8 goes */
extern const uint8_t romSlotKindTable[8];			/* ROM 0x3f41f18 */
extern uint16_t btTargetHold;					/* 0x400de10: non-zero: no target slot is taken (set by the ROM) */
extern uint8_t btTargetFits;					/* 0x400de59: 1: the queued links' intervals fit the slots */
extern uint16_t btTargetLongest;				/* 0x400de5a: the longest interval of the queued links */
extern uint32_t btTargetDeadline;				/* 0x400de5c: the clock target state 5 ends at */
extern void (*btTargetExpireHook)(void);			/* 0x400edbc */

/* A link controller message about a link handle. */
struct BtHandleMessage {
	uint32_t id;
	uint16_t handle;
	uint8_t value;
};
extern uint16_t btLinkRateBase;				/* 0x400f7e8: the ticks a rate step is counted on */
extern uint16_t btLinkRateExtra;				/* 0x400f7ea: the ticks added to a link's rate slots */
extern uint8_t btHostRate;					/* 0x400c4f1: the host link's rate state, 1 to 3 */
extern uint8_t btHandlePending[];				/* 0x400f586: per handle */
extern uint8_t btTargetSessions;				/* 0x400de55 */
extern void (*btSlotNeedHook)(struct BtDeviceProfile *profile, struct BtDeviceProfile *previous, uint8_t *before,
			      uint8_t *setup, uint8_t *after, uint8_t *hold);	/* 0x400edb0 */
extern void (*btTargetEnterHook)(int kind);			/* 0x400ea04 */
extern void (*btTargetLeaveHook)(int kind);			/* 0x400ea00 */
extern unsigned int (*btSlotFillHook)(uint32_t clock, unsigned int limit, unsigned int remaining,
				      uint8_t *stop);			/* 0x400edd0 */
extern void (*btSlotDoneHook)(uint32_t clock);			/* 0x400eddc */
extern int (*btDeviceWaitHook)(uint32_t clock);			/* 0x400dde4: cleared once it returns non-zero */
extern uint32_t btTargetStart;					/* 0x400de00 */
extern uint16_t btTargetWindow;					/* 0x400ddfa */
extern uint16_t btTargetIndex;					/* 0x400de04 */
extern uint8_t btTargetMode;					/* 0x400de06 */
extern uint8_t btSlotSessionFlags;				/* 0x400cd29 */
extern struct BtLinkSession *btTargetSession;			/* 0x400ddec */
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
extern const uint32_t romQueueSlot;				/* ROM 0x03f41a48: the slot requests made in interrupts */
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
void btLmpLcReleaseBuffer(struct BtSlotEvent *event, uint32_t clock);	/* 0x46100 */
void btLmpLcSetupBuffer(struct BtDeviceProfile *profile, uint32_t clock, struct BtStepRecord *step,
			const struct BtSlotTiming *timing, unsigned int kind);	/* 0x4618c */
void btLmpLcClearBuffer(struct BtSlotEvent *event, uint32_t clock);	/* 0x461f0 */
void btLmpLcHandleDescriptor(struct BtStepRecord *step);		/* 0x49254 */
void btLmpLcConvertBuffer(struct BtSlotQueue *queue, struct BtDeviceProfile *profile);	/* 0x462d2 */
void btLmpLcVerifyBuffer(struct BtSlotQueue *queue, struct BtDeviceProfile *profile, uint32_t clock);	/* 0x462f4 */
void btLmpLcSelectBuffer(struct BtSlotQueue *queue, uint32_t clock);	/* 0x46436 */
void btLmpLcAdjustBuffer(struct BtSlotQueue *queue, struct BtDeviceProfile *profile);	/* 0x46494 */
int btLmpLcRecordBuffer(uint32_t clock);				/* 0x464ca */
void btLmpLcFilterBuffer(struct BtSlotEvent *event, uint32_t clock);	/* 0x46516 */
void btLmpLcHandlePacket(void);					/* 0x465d8 */
void btLmpLcProcessPacket(unsigned int link, uint32_t argument);	/* 0x46660 */
void btLmpLcUpdatePacket(void);					/* 0x466e8 */
int btLmpLcConfigurePacket(uint32_t *command);			/* 0x466f8 */
void btLmpLcCheckPacket(unsigned int link, uint32_t clock, unsigned int kind, uint32_t argument,
			unsigned int interval, uint32_t extra);		/* 0x46776 */
int btLmpLcValidatePacket(unsigned int link, uint32_t first, uint32_t second, uint8_t *out);	/* 0x46830 */
void btLmpLcPreparePacket(unsigned int value);			/* 0x4687a */
void btLmpLcDispatchPacket(uint8_t *request);			/* 0x46926 */
unsigned int btLmpLcParsePacket(uint32_t clock, unsigned int count, unsigned int limit, uint8_t *flag);	/* 0x4699c */
int btLmpLcBuildPacket(uint16_t handle);			/* 0x46a90 */
void btLmpLcComputePacket(void);				/* 0x46ac8 */
unsigned int btLmpLcQueryPacket(unsigned int link, unsigned int rate);	/* 0x46b02 */
void btLmpLcResetPacket(unsigned int link, unsigned int kind);	/* 0x46b7e */
void btLmpLcSyncPacket(void);					/* 0x46ba2 */
void btLmpLcApplyPacket(void);					/* 0x46bc2 */
void btLmpLcResolvePacket(unsigned int mode);			/* 0x46bd6 */
void btLmpLcSchedulePacket(uint8_t *request);			/* 0x46c06 */
void btLmpLcNotifyPacket(void);					/* 0x46c66 */
void btLmpLcFormatPacket(void);					/* 0x46c74 */
int btLmpLcInspectPacket(const uint8_t *request);		/* 0x46c82 */
unsigned int btLmpLcTrackPacket(void);				/* 0x46cb6 */
uint32_t btLmpLcManagePacket(unsigned int argument);		/* 0x46cf4 */
int btLmpLcAcquirePacket(uint32_t unused, uint16_t flags, struct BtDeviceProfile *profile, uint8_t entry);	/* 0x46f7c */
void btLmpLcReleasePacket(void);				/* 0x47066 */
void btLmpLcSetupPacket(uint32_t clock);			/* 0x470d4 */
void btLmpLcClearPacket(uint32_t clock, uint32_t extra);	/* 0x4712e */
void btLmpLcConvertPacket(uint32_t first, uint32_t second, unsigned int slots, uint32_t fourth,
			  uint32_t fifth);				/* 0x471f2 */
void btLmpLcVerifyPacket(unsigned int slot, uint32_t first, uint32_t second);	/* 0x47264 */
void btLmpLcSelectPacket(unsigned int link);			/* 0x47336 */
void btLmpLcAdjustPacket(uint32_t first, uint8_t *record, uint32_t third, uint32_t fourth,
			 uint32_t fifth);				/* 0x473ea */
void btLmpLcRecordPacket(uint8_t *record, const uint16_t *step);	/* 0x47402 */
int btLmpLcFilterPacket(struct BtSyncSetup *setup);		/* 0x47414 */
void btLmpLcHandleFrame(const uint8_t *message);		/* 0x475e0 */
void btLmpLcProcessFrame(uint8_t *pdu);				/* 0x476dc */
int btLmpLcUpdateFrame(void *message);				/* 0x47706 */
void btLmpLcVerifyHeader(unsigned int handle, int enable, unsigned int reason);	/* 0x48eb2 */

#endif /* ROBIN_BTBUFFER_H */
