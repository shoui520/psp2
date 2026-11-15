/*
 * The rate selection (wlan/ratesel.c, 0x3c740 to 0x3d4c6): what decides the rate a peer is sent
 * at. A connection keeps its state in WlanConnection.peerLink.rateControl (robin/ratectl.h); what
 * is common to the connections, the host's wishes and the aggregation limits, is in
 * wlanRateSelection. wlanPsHandlePacket() runs every 10 ms (macTxNotifyUpdateFrame()) and moves the
 * connection's rate up or down from the transmit statistics the window holds. The firmware's
 * placeholder names are kept ("wlanPs*Buffer": the decisions, "wlanPs*Packet": the rate masks,
 * "wlanPs*Frame": the window); the descriptive names of the fields are inferred from use.
 *
 * A rate index is the one of robin/ratectl.h: 0 to 13 legacy, 14 to 21 HT20 MCS 0 to 7, 22 MCS 32,
 * 23 to 30 HT40 MCS 0 to 7. A rate mask has one bit per rate index.
 */
#ifndef ROBIN_RATESEL_H
#define ROBIN_RATESEL_H

#include <stddef.h>
#include <stdint.h>
#include "robin/ratectl.h"

struct WlanConnection;
struct WlanPeerLink;
struct WlanRxStatus;
struct WlanTxStatus;

/* What the host and the configuration allow of all connections (DTCM 0x04000ff4, 98 bytes). */
struct WlanRateSelection {
	uint8_t fixedRate;		/* the rate index that picks the set of rates wlanRateModeMask[] allows */
	uint8_t amsduRate;		/* the rate wlanPsFormatBuffer() last chose the A-MSDU size for */
	uint8_t previousRate;		/* the rate wlanPsNotifyBuffer() was last asked about */
	uint8_t eventRate;		/* the fixed rate the connection takes, while mode is 2 */
	uint16_t amsduBufferSize;	/* the A-MSDU size the firmware reports (HostCmd_CMD_11N_AMSDU_AGGR_CTRL's
					   curr_buf_size, event 0x42); 0xffff: none chosen yet */
	uint16_t amsduLarge;		/* the three sizes wlanPsQueryPacket() sets, from the largest */
	uint16_t amsduMedium;
	uint16_t amsduSmall;		/* 0: no aggregation */
	uint16_t mode;			/* 0: no rate selection, 1: the firmware chooses, 2: a rate is fixed */
	uint16_t reserved0e;
	uint16_t amsduEnable;		/* the host's A-MSDU aggregation control (0: off) */
	uint16_t reserved12;
	uint32_t channelWidth;		/* the last width wlanRateChannelWidthSet() was given */
	uint32_t txRateMask;		/* the rates the host allows (HostCmd_CMD_TX_RATE_CFG's rate scope) */
	uint32_t allowedRates;		/* the rates of the connection wlanPsAcquirePacket() found */
	/*
	 * Thresholds on the bytes a rate carries in one burst (wlanRateBurstBytes[]), for 20 MHz and
	 * legacy rates, then for 40 MHz. A rate raised through them takes the larger A-MSDU size above
	 * raiseLarge, the middle one above raiseMedium; one that was lowered takes the smallest below
	 * lowerSmall, the middle one below lowerMedium.
	 */
	uint32_t raiseLarge[2];
	uint32_t raiseMedium[2];
	uint32_t lowerMedium[2];
	uint32_t lowerSmall[2];
	uint16_t burstBytes[17];	/* what wlanPsReleaseBuffer() computes: the bytes of one burst per HT rate */
};

_Static_assert(offsetof(struct WlanRateSelection, amsduBufferSize) == 4, "amsdu size");
_Static_assert(offsetof(struct WlanRateSelection, mode) == 12, "selection mode");
_Static_assert(offsetof(struct WlanRateSelection, amsduEnable) == 16, "amsdu enable");
_Static_assert(offsetof(struct WlanRateSelection, channelWidth) == 20, "selection width");
_Static_assert(offsetof(struct WlanRateSelection, txRateMask) == 24, "tx rate mask");
_Static_assert(offsetof(struct WlanRateSelection, raiseLarge) == 32, "raise large");
_Static_assert(offsetof(struct WlanRateSelection, lowerSmall) == 56, "lower small");
_Static_assert(offsetof(struct WlanRateSelection, burstBytes) == 64, "burst bytes");

extern struct WlanRateSelection wlanRateSelection;		/* DTCM 0x04000ff4 */
extern uint16_t *wlanRateBurstTable;				/* DTCM 0x04000f7c: wlanRateSelection.burstBytes */
extern const uint32_t *wlanRateWeightTable;			/* DTCM 0x04000f80: wlanRateWeight */

/* The tables of the rates, by rate index (0x54084 to 0x54240). */
extern const uint16_t wlanRateBurstBytes[17];			/* 0x54084: bytes in 1.504 ms, HT20 MCS 0 to 7, MCS 32, HT40 */
extern const uint32_t wlanRateKbps[17];				/* 0x540a8: the same rates, kbit/s */
extern const uint8_t wlanRateNext[WLAN_RATE_COUNT];		/* 0x54174: the next rate, slowest first */
extern const uint8_t wlanRatePrevious[WLAN_RATE_COUNT];		/* 0x54193: the one before */
extern const uint32_t wlanRateModeMask[WLAN_RATE_COUNT];	/* 0x541b4: the rates a fixedRate allows */
extern const uint8_t wlanRateModeFixed[28];			/* 0x54230: the rate of each frame class (254: by the
								   connection); from index 14 on another table, see
								   wlanRateModeHtMcs */

/* The rates a connection has no hardware rate table slot for (SQRAM 0xc00329bc, 19 slots). */
extern struct WlanRateSlot wlanRateSlots[19];
extern uint8_t wlanRateResponseContext;				/* SQRAM 0xc0011d70: the highest rate of the last connection */

int wlanPsInspectBuffer(struct WlanConnection *connection);			/* 0x3c800 */
int wlanPsSetupBuffer(struct WlanConnection *connection, unsigned int index);	/* 0x3c9f8 */
int wlanPsClearBuffer(struct WlanConnection *connection);			/* 0x3ca26 */
int wlanPsConvertBuffer(struct WlanConnection *connection);			/* 0x3ca78 */
int wlanPsVerifyBuffer(struct WlanConnection *connection);			/* 0x3ca8e */
int wlanPsSelectBuffer(struct WlanConnection *connection);			/* 0x3ca98 */
int wlanPsAdjustBuffer(struct WlanConnection *connection);			/* 0x3cacc */
void wlanPsRecordBuffer(struct WlanConnection *connection, unsigned int decision); /* 0x3cada */
int wlanPsFilterBuffer(struct WlanConnection *connection);			/* 0x3cb08 */
int wlanPsTrackBuffer(struct WlanConnection *connection);			/* 0x3c922 */
int wlanPsAcquireBuffer(struct WlanConnection *connection);			/* 0x3c97c */
int wlanPsManageBuffer(struct WlanConnection *connection);			/* 0x3c97a */
unsigned int wlanPsReleaseBuffer(void);						/* 0x3c9ba */
unsigned int wlanPsNotifyBuffer(unsigned int rate);				/* 0x3c740 */
void wlanPsFormatBuffer(struct WlanConnection *connection, unsigned int rate);	/* 0x3c7b8 */
int wlanPsHandlePacket(struct WlanConnection *connection);			/* 0x3cc48 */
int wlanPsProcessPacket(struct WlanRateControl *rateControl, struct WlanTxStatus *status); /* 0x3cd54 */

/* The host's TX rate configuration (HostCmd_CMD_TX_RATE_CFG). */
int hostCmd00d6Apply(struct WlanConnection *connection, uint8_t *rateScope, uint8_t *dropPattern,
		     uint8_t *fixedRate);					/* 0x3ce42 */
unsigned int hostCmd00d6Get(struct WlanConnection *connection, uint8_t *data, uint32_t rateMask); /* 0x3cea6 */

void wlanPsConfigurePacket(const uint8_t *tlv);					/* 0x3cd86 */
void wlanPsCheckPacket(struct WlanConnection *connection, const uint8_t *tlv);		/* 0x3cdf8 */
int wlanPsUpdatePacket(void);								/* 0x3cd82 */
void wlanRateChannelWidthSet(struct WlanConnection *connection, int update, int width);	/* 0x3cfb2 */
void wlanTxRateAttempts(const struct WlanTxStatus *status, uint8_t *attempts);		/* 0x3cfd2 */
void wlanPsBuildPacket(void);								/* 0x3cfe4 */
unsigned int wlanPsComputePacket(void);						/* 0x3cff4 */
void wlanPsQueryPacket(void);								/* 0x3d006 */
void wlanPsApplyPacket(void);								/* 0x3d06c */
int wlanPsResolvePacket(struct WlanConnection *connection, struct WlanRxStatus *status);	/* 0x3d08c */
int wlanPsSchedulePacket(struct WlanConnection *connection, struct WlanTxStatus *status); /* 0x3d09c */
int wlanPsNotifyPacket(struct WlanConnection *connection);				/* 0x3d0c6 */
int wlanPsFormatPacket(struct WlanConnection *connection);				/* 0x3d0e4 */
int wlanPsInspectPacket(struct WlanConnection *connection, uint8_t *rate);		/* 0x3d118 */
int wlanPsTrackPacket(uint8_t *rate);							/* 0x3d156 */
int wlanPsManagePacket(struct WlanConnection *connection, uint8_t *rate, int direction); /* 0x3d16e */
int wlanPsAcquirePacket(struct WlanConnection *connection);				/* 0x3d1dc */
int wlanPsReleasePacket(struct WlanConnection *connection, uint32_t rateMask);		/* 0x3d286 */
int wlanPsSetupPacket(struct WlanPeerLink *link, uint32_t peerMask);			/* 0x3d29e */
int wlanPsClearPacket(unsigned int index);						/* 0x3d2a2 */
uint32_t wlanPsConvertPacket(unsigned int index);					/* 0x3d2aa */
unsigned int wlanPsVerifyPacket(struct WlanConnection *connection);			/* 0x3d2b4 */
int wlanRateAmpduAllowed(unsigned int rate);						/* 0x3d2e2 */
int wlanPsAdjustPacket(void);								/* 0x3d2f4 */
unsigned int wlanTxDataRate(struct WlanConnection *connection, int offset);		/* 0x3d2f8 */
int wlanPsFilterPacket(struct WlanConnection *connection);				/* 0x3d34c */
unsigned int wlanPsHandleFrame(struct WlanConnection *connection, unsigned int mode);	/* 0x3d3a8 */
unsigned int wlanPsProcessFrame(struct WlanConnection *connection, unsigned int mode,
				int broadcast);					/* 0x3d3f0 */
int wlanPsUpdateFrame(unsigned int rate, uint8_t *wide, uint8_t *index);		/* 0x3d402 */
int wlanPsConfigureFrame(const struct WlanRateLevel *level);				/* 0x3d45c */
int wlanPsCheckFrame(const uint8_t *level);						/* 0x3d472 */
void wlanPsValidateFrame(struct WlanRateWindow *window);				/* 0x3d480 */
int wlanPsPrepareFrame(const struct WlanRateWindow *window);				/* 0x3d490 */
void wlanPsDispatchFrame(const struct WlanRateWindow *window, unsigned int *exchanges,
			 unsigned int *excess);					/* 0x3d4a2 */
int wlanPsParseFrame(struct WlanRateWindow *window, int *percent, uint8_t *level,
		     int ampdu);						/* 0x3d4c6 */

/* The window of the transmissions counted (wlan/ratewin.c). */
void fmCoexAcquireTable(struct WlanRateWindow *window, unsigned int rate);		/* 0x4c0d8 */
int fmCoexReleaseTable(struct WlanRateWindow *window, int *percent, int ampdu); /* 0x4c0ec */
void fmCoexAdjustTable(struct WlanRateControl *rc);					/* 0x4c3e4 */

const uint8_t *wlanTlvNextOfType(unsigned int type, const uint8_t *tlv);		/* 0x2be78 */

/*
 * The rate words the MAC sends with (SQRAM 0xc0011d70 to 0xc0011f24, 0xc00324fc to 0xc0032f00,
 * wlan/ratetab.c). A rate's words are a chain of up to 8, the first used at the first attempt;
 * wlanRateChains.byRate[] points at the chain of each rate index (wlanRateTable points at it).
 */
struct WlanRateChains {
	uint32_t defaultUnicast[8];	/* what wlanPsComputeFrame() puts in every unicast row */
	uint32_t *byRate[WLAN_RATE_COUNT];
};

extern struct WlanRateChains wlanRateChains;			/* SQRAM 0xc0011e88 */
extern uint32_t wlanRateWordsCck[4];				/* SQRAM 0xc0011e3c: rates 0 to 3, last first */
extern uint32_t wlanRateWordsOfdm[8];				/* SQRAM 0xc0011e00: rates 5 to 12 */
extern uint32_t wlanRateWordsHt20[9];				/* SQRAM 0xc0011dc0: rates 14 to 21, from word 8 down */
extern uint32_t wlanRateWordsHt40[9];				/* SQRAM 0xc0011d7c: rates 23 to 30 */
extern uint32_t wlanRateWordsMcs32[1];				/* SQRAM 0xc0011da0: rate 22 */
extern uint32_t wlanRateDefaultGroup[8];			/* SQRAM 0xc0011e68: what every group row starts as */
extern uint32_t wlanRateChainLength;				/* SQRAM 0xc0011d78: 32 */
extern uint32_t wlanRateGroupChains[19][8];			/* SQRAM 0xc00324fc: the rows of the slots */
extern uint32_t wlanRateUnicastChains[19][8];			/* SQRAM 0xc003275c */
extern uint8_t wlanRateBlocks[0x45c];				/* SQRAM 0xc0032aa0: wlanRateBlockBuild()'s blocks */

void wlanPsBuildFrame(void);								/* 0x3d4ea */
void wlanPsComputeFrame(void);								/* 0x3d510 */
unsigned int wlanPsQueryFrame(uint8_t mode);						/* 0x3d570 */
uint32_t *wlanPsResetFrame(unsigned int rate, uint32_t rateMask);			/* 0x3d580 */
void wlanPsSyncFrame(struct WlanConnection *connection, unsigned int rate,
		     struct WlanRateControl *rc);					/* 0x3d5b0 */
void wlanPsApplyFrame(struct WlanConnection *connection, int update, unsigned int shortPreamble); /* 0x3d6d0 */
void wlanPsScheduleFrame(struct WlanConnection *connection);				/* 0x3d758 */
void wlanPsNotifyFrame(const uint8_t *items, int count);				/* 0x3d77c */
uint32_t *wlanPsFormatFrame(struct WlanConnection *connection, unsigned int multicast); /* 0x3d78c */
unsigned int wlanPsInspectFrame(void);							/* 0x3d7c4 */
void wlanPsTrackFrame(void);								/* 0x3d852 */
void wlanPsManageFrame(void *destination, const void *source, unsigned int length);	/* 0x3d860 */
void wlanPsAcquireFrame(void);								/* 0x3d896 */

/* The window's counting hooks (wlan/ratewin.c). */
struct WlanRateWindowState {
	uint32_t ageLimit;		/* milliseconds a window may age before its few samples decide */
	uint32_t reportEnabled;		/* fmCoexReleaseTable() reports the counts */
	uint32_t *reportDelivered;
	uint32_t *reportSent;
	void (*count)(const struct WlanRateWindow *window, unsigned int *sent, unsigned int *delivered);
};
extern struct WlanRateWindowState wlanRateWindowState;		/* 0x553f8 */

int fmCoexSetupTable(const struct WlanTxStatus *status);				/* 0x4c1fc */
void fmCoexClearTable(struct WlanRateWindow *window, const struct WlanTxStatus *status); /* 0x4c218 */
unsigned int fmCoexVerifyTable(struct WlanRateWindow *window, const struct WlanTxStatus *status); /* 0x4c374 */
unsigned int fmCoexSelectTable(struct WlanRateWindow *window, const struct WlanTxStatus *status); /* 0x4c3be */
void fmCoexConvertTable(struct WlanTxStatus *status);					/* 0x4c246 */

#endif /* ROBIN_RATESEL_H */
