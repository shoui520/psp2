/*
 * The controller's per-link sample records (bt/lcsource.c, 0x43bfc - 0x4414e): for each of the eight
 * links two rings of four samples (a measured value, its weight and how many clock ticks it covers),
 * their averages, and the rate taken from them. The names of the firmware's placeholders are kept
 * (btLmpLc...Entry, btSource...); the roles are inferred from what the code does.
 */
#ifndef ROBIN_BTSOURCE_H
#define ROBIN_BTSOURCE_H

#include <stdint.h>

#define BT_SOURCE_LINKS		8
#define BT_SOURCE_SAMPLES	4

/* Four samples (26 bytes). */
struct BtSourceRing {
	uint16_t sample[BT_SOURCE_SAMPLES];	/* the measured values */
	uint16_t weight[BT_SOURCE_SAMPLES];	/* what each was weighted with */
	uint8_t count[BT_SOURCE_SAMPLES];	/* the ticks each covers, 0: the slot is empty */
	uint16_t average;			/* the mean of the used samples */
	uint16_t ratio;				/* the weights over the ticks, 0: not computed */
	uint8_t head;				/* the slot the next sample takes */
	uint8_t reserved;
};

/* One link's record (68 bytes). */
struct BtSourceRecord {
	struct BtSourceRing first;
	struct BtSourceRing second;
	uint32_t firstClock;			/* the event clock of the first ring's last sample */
	uint16_t value;				/* the first ring's pending weight */
	uint16_t reserved58;
	uint32_t secondClock;			/* the event clock of the second ring's last sample */
	uint8_t firstRate;			/* the rate the first ring gives, at most btSource.timer */
	uint8_t secondRate;
	uint8_t rateLimit;			/* 6: the first ring's rate is taken at most this */
	uint8_t reserved67;
};

/* 0x5a274: the limits of the records, and which of their rings hold samples. */
struct BtSourceState {
	uint8_t packet;				/* 0 */
	uint8_t frame;				/* bit n: link n's first ring is in use */
	uint8_t header;				/* bit n: link n's second ring is in use */
	uint8_t descriptor;			/* 4 */
	uint8_t queue;				/* 80: ticks after which a ring's samples are taken */
	uint8_t timer;				/* 40: the largest rate */
	uint8_t channel;			/* 3 */
	uint8_t param;				/* 1 */
};

/*
 * The ROM's profile of a device link (72 bytes, 0x400d814, index 1..7): the fields the sample code reads
 * and writes; the rest is the ROM's.
 */
struct BtDeviceProfile {
	struct BtDeviceProfile *next;		/* the next profile in its slot queue */
	uint32_t connection;			/* non-zero: the link is up */
	uint8_t reserved8[4];
	uint16_t flags;				/* the ROM's slot flags: bits 0-1 the slot's kind */
	uint16_t index;				/* its record in btSourceBuffer */
	uint16_t interval;			/* the link's own timing, in ticks */
	uint16_t span;				/* the ticks the active link is given */
	uint16_t rate;				/* the ticks the sample rates give, 1: none */
	uint8_t secondSetting;			/* the ROM's rate setting of the second ring, or 1 */
	uint8_t firstSetting;			/* the same for the first ring */
	uint8_t active;				/* non-zero: the link is active */
	uint8_t adjust;				/* 1: the rates are to be taken again */
	uint8_t reserved26[2];
	uint32_t due;				/* the event clock the link's next slot is due at */
	uint8_t reserved32[8];
	uint32_t since;				/* the event clock the link last went active at */
	uint16_t window;			/* the ticks of the link's current window */
	uint8_t reserved46[2];
	uint8_t offset;				/* the ticks from the clock to a mode 5 slot */
	uint8_t shortWindow;			/* the window of a mode 5 slot */
	uint8_t longWindow;			/* the window of a mode 9 slot */
	uint8_t hold;				/* non-zero: a mode 5 slot waits for the link's interval */
	uint8_t limit;				/* non-zero: the ticks to give the link instead */
	uint8_t reserved53;
	uint16_t peer;				/* the link a slot is shared with, 0xffff: none */
	uint32_t queued;			/* the event clock the profile was queued at */
	uint8_t reserved60[8];
	uint8_t state;				/* 5 or 9: the link's mode */
	uint8_t reserved69[3];
};

_Static_assert(sizeof(struct BtSourceRing) == 26, "sample ring");
_Static_assert(sizeof(struct BtSourceRecord) == 68, "link record");
_Static_assert(sizeof(struct BtDeviceProfile) == 72, "device profile");

extern struct BtDeviceProfile btDeviceProfile[8];		/* 0x400d814 */
extern uint32_t btDeviceRecord;					/* 0x400dde8: the event clock of the next slot */
extern uint16_t btTargetConfig;					/* 0x400de50 */
extern struct BtSourceRecord btSourceBuffer[BT_SOURCE_LINKS];	/* 0x5a054 */
extern struct BtSourceState btSource;				/* 0x5a274 */
extern uint16_t btHostWindow[];					/* 0x400c4d4: link to record index */

int btLmpLcFormatEntry(int link, uint8_t *record);		/* 0x43bfc */
int btLmpLcInspectEntry(unsigned int mask, int index);		/* 0x43c12 */
int btLmpLcTrackEntry(void);					/* 0x43c22 */
unsigned int btLmpLcManageEntry(void);				/* 0x43c34 */
unsigned int btLmpLcAcquireEntry(struct BtSourceRing *ring);	/* 0x43c6c */
unsigned int btLmpLcReleaseEntry(struct BtSourceRing *ring, unsigned int limit);	/* 0x43cca */
void btLmpLcSetupEntry(struct BtSourceRing *ring, uint16_t sample, uint16_t weight, uint8_t count);	/* 0x43d0e */
struct BtSourceState *btLmpLcClearEntry(int link, int unused, uint16_t value, uint16_t sample);	/* 0x43d30 */
struct BtSourceState *btLmpLcConvertEntry(int link, int unused, uint16_t weight, uint16_t sample);	/* 0x43d6c */
unsigned int btLmpLcVerifyEntry(int link, unsigned int clock);	/* 0x43daa */
unsigned int btLmpLcSelectEntry(int link, unsigned int clock);	/* 0x43de2 */
unsigned int btLmpLcAdjustEntry(struct BtDeviceProfile *profile, unsigned int first, unsigned int second);	/* 0x43e0e */
void btLmpLcRecordEntry(void);					/* 0x43ebe */
void btLmpLcFilterEntry(struct BtDeviceProfile *profile, unsigned int clock);	/* 0x43fd6 */

#endif /* ROBIN_BTSOURCE_H */
