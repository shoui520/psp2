/*
 * WMM (Wi-Fi Multimedia): the access category parameters of the BSS a
 * station joined (from the WMM parameter element) and how they map to the
 * four hardware queues, the TSPEC admission state, and the host commands
 * that query and tune them (Marvell's HostCmd_CMD_WMM_QUEUE_CONFIG and
 * HostCmd_CMD_WMM_GET_STATUS). The names are inferred from use.
 */
#ifndef ROBIN_WMM_H
#define ROBIN_WMM_H

#include <stddef.h>
#include <stdint.h>
#include "robin/buffer.h"
#include "robin/conn.h"

#define WLAN_WMM_QUEUES		4
#define WLAN_WMM_SLOTS		8

/* A transmit queue's state in the MAC: only the part the WMM code sets (96 bytes, one per TID). */
struct WlanWmmSlot {
	uint8_t tspec[53];		/* the TSPEC: TS Info first (wlanTspecApplyBits() reads the user priority and U-APSD from it) */
	uint8_t mediumTime[2];		/* the TSPEC's medium time (unaligned) */
	uint8_t state;			/* 0: free; 1: to be requested; 2: requested; 4: admitted; 5: admitted, suspended */
	uint8_t tailLength;		/* the bytes of tail the ADDTS request carries after the TSPEC */
	uint8_t tail[27];		/* what follows the TSPEC in the request (the host's elements) */
	uint8_t directions;		/* the directions the TSPEC covers (WLAN_TSPEC_UPLINK, WLAN_TSPEC_DOWNLINK) */
	uint8_t ownQueue;		/* the queue that normally uses the slot */
	uint8_t fallbackQueue;		/* the queue it falls back to when admission control rules the queue out */
	uint8_t activeQueue;		/* the queue using it now */
	uint8_t activeRing;		/* the TID/ring that goes with the active queue (wlanWmmRings) */
	uint8_t reserved89;
	uint16_t nextSequence;		/* the sequence number of the next frame of the TID */
	uint16_t lastSequence;		/* the sequence number of the last frame sent */
	uint8_t statsRing;		/* the ring whose statistics count the slot's frames */
	uint8_t reserved95;
};

/* EDCA parameters of an access category as the MAC takes them (the first two swapped). */
struct WlanWmmEdca {
	uint8_t info;			/* bits 1-2: access category, bit 3: admission control mandatory, bits 4-7: AIFSN */
	uint8_t contention;		/* ECWmin << 4 | ECWmax */
	uint16_t txop;			/* TXOP limit */
};

/* The parameter record of one hardware queue. */
struct WlanWmmQueue {
	struct WlanWmmEdca edca;
	struct WlanWmmSlot *slots[2];
	uint16_t admittedTime;		/* the medium time a TSPEC was admitted for; 0: none */
	uint16_t usedTime;		/* the medium time its frames used since the admission */
	uint8_t exhausted;		/* the used time went over the admitted time: use the fallback */
	uint8_t fallback;		/* the queue it falls back to without admission: 255: none */
	uint8_t reserved18[2];
};

_Static_assert(sizeof(struct WlanWmmSlot) == 96, "WMM slot");
_Static_assert(offsetof(struct WlanWmmSlot, state) == 55, "WMM slot state");
_Static_assert(offsetof(struct WlanWmmSlot, directions) == 84, "WMM slot directions");
_Static_assert(sizeof(struct WlanWmmEdca) == 4, "EDCA parameters");
_Static_assert(sizeof(struct WlanWmmQueue) == 20, "WMM queue");


/* One access category's record in a WMM parameter element (four of them from byte 10). */
struct __attribute__((packed)) WlanWmmAcRecord {
	uint8_t aifsn : 4;
	uint8_t acm : 1;		/* admission control mandatory */
	uint8_t aci : 2;		/* the access category: 0 AC_BE, 1 AC_BK, 2 AC_VI, 3 AC_VO */
	uint8_t reserved : 1;
	uint8_t ecwMin : 4;
	uint8_t ecwMax : 4;
	uint16_t txopLimit;		/* units of 32 microseconds */
};

_Static_assert(sizeof(struct WlanWmmAcRecord) == 4, "WMM access category record");

/* The byte of a WMM information element (as kept here: ID, length, then the body) that holds its QoS Info. */
#define WLAN_WMM_IE_QOS_INFO	8

/*
 * All WMM state but the per-queue timing and statistics: one object in the
 * original, cleared as a whole (wlanWmmClear() clears 0x3cc bytes from
 * 0x04008c4c, and the other modules address its parts from wlanWmm).
 */
struct WlanWmm {
	uint8_t enabled;		/* the BSS is a WMM one */
	uint8_t parameterSetCount;	/* the parameter set count of the parameter element in use */
	uint8_t reserved2[2];
	struct WlanWmmQueue queues[WLAN_WMM_QUEUES];
	struct WlanWmmSlot slots[WLAN_WMM_SLOTS];		/* 0x04008ca0 */
	struct WlanWmmSlot *sessions[WLAN_WMM_SLOTS];		/* 0x04008fa0: the TSPECs admitted, by TSID */
	uint8_t joinIe[9];			/* 0x04008fc0: the WMM information element of the join command */
	uint8_t bssIe[9];			/* 0x04008fc9: the BSS's element header (or its QoS capability) */
	uint8_t infoIe[3][9];			/* 0x04008fd2: the information elements in use; their QoS Info
						   bytes: 0 the delivery-enabled (downlink) access categories,
						   1 the reference, 2 the trigger-enabled (uplink) ones */
	uint8_t element[26];			/* 0x04008fed: the parameter element, ID 221 */
	uint8_t currentSlot;			/* 0x04009007 */
	uint8_t padding[2];
	uint16_t policed;			/* 0x0400900a */
	int validatedSeen[2];			/* 0x0400900c: each ring's validated frame count when last read
						   (macTxStatsAddValidated()) */
	uint32_t policingTimer;			/* 0x04009014: osaCreateTimer() handle of the medium time's second */
};

_Static_assert(offsetof(struct WlanWmm, slots) == 0x54, "WMM slots");
_Static_assert(offsetof(struct WlanWmm, sessions) == 0x354, "WMM sessions");
_Static_assert(offsetof(struct WlanWmm, joinIe) == 0x374, "WMM join element");
_Static_assert(offsetof(struct WlanWmm, element) == 0x3a1, "WMM parameter element");
_Static_assert(offsetof(struct WlanWmm, policed) == 0x3be, "WMM policed time");
_Static_assert(sizeof(struct WlanWmm) == 0x3cc, "WMM state");

/* What a queue's frames cost: one bin of Marvell's HostCmd_CMD_WMM_QUEUE_STATS (32 bytes). */
struct WlanWmmStats {
	uint16_t packets;
	uint16_t lost;
	uint16_t usedTime;
	uint16_t policedTime;
	uint32_t queueDelay;		/* sum, microseconds */
	uint32_t transmitDelay;		/* sum, microseconds */
	uint16_t histogram[7];		/* delays of 5, 10, 20, 30, 40 and 50 ms and over */
	uint16_t reserved30;
};

/* The frames a queue has in flight and when its last one finished. */
struct WlanWmmTiming {
	uint32_t lastTime;
	uint8_t pending;
	uint8_t reserved5[3];
};

_Static_assert(sizeof(struct WlanWmmStats) == 32, "WMM statistics bin");
_Static_assert(sizeof(struct WlanWmmTiming) == 8, "WMM queue timing");

struct WlanConnection;
struct WlanIePointers;
struct WlanTspecRequest;

extern struct WlanWmm wlanWmm;					/* 0x04008c4c */
extern struct WlanWmmTiming wlanWmmTiming[WLAN_WMM_QUEUES];	/* 0x04009018 */
extern struct WlanWmmStats wlanWmmStats[WLAN_WMM_QUEUES];	/* 0x04009038 */
extern uint32_t wlanWmmDefaultTime;				/* 0x00054ec0 */
extern const uint8_t wlanWmmRings[8];				/* SQRAM 0xc000fa9b: two rings per access category */

int wlanWmmEnable(void);					/* 0x1e570 */
int wlanWmmEnabled(void);					/* 0x41cb8 */
int wlanWmmApplyQueue(unsigned int queue);			/* 0x1e578 */
int wlanWmmSetParameters(const uint8_t *element, uint8_t *count);	/* 0x1e63c */
int wlanWmmQueueStatus(const struct WlanConnection *connection, uint8_t *tlvs);	/* 0x1e6c8 */
int wlanWmmReportStatus(struct WlanConnection *connection);	/* 0x1e79a */
int wlanWmmUpdate(struct WlanConnection *connection, const uint8_t *info, const uint8_t *parameter);	/* 0x1e7cc */
void wlanWmmSet(struct WlanConnection *connection, int enable, int report);	/* 0x1e868 */
void wlanWmmJoin(struct WlanConnection *connection, struct WlanIePointers *elements, int sameBss,
		 int report);	/* 0x1e8ba */
int wlanWmmQueueBit(int queue);					/* 0x2d1c6 */
int wlanWmmSetEdca(unsigned int queue, const struct WlanWmmEdca *edca);	/* 0x2d1e2 */
int wlanWmmParseParameter(const uint8_t *element, uint8_t *count);	/* 0x2d2e2 */
struct WlanWmmEdca *wlanWmmStoreQueue(struct WlanWmmEdca *edca, const uint8_t *record);	/* 0x2d338 */
int wlanWmmQueueConfig(uint8_t *body, uint16_t *table, uint32_t *length);	/* 0x2d38c */
int wlanWmmAppendIe(uint8_t *body, const uint8_t *element);	/* 0x2d4b0 */
unsigned int hostCmd0070(uint8_t *body);			/* 0x41af8..: WMM_QUEUE_CONFIG's length */
int hostCmd0081(uint8_t *body);					/* 0x1e83c: HostCmd_CMD_WMM_QUEUE_STATS */
void wlanWmmResetCounters(void);				/* 0x1e84a */
int wlanWmmHasElement(void);					/* 0x1e9ac */
int wlanWmmCurrentSlotNumber(void);				/* 0x1e9ba */
int wlanWmmSetBaWindow(unsigned int tid, unsigned int queue, unsigned int ring);	/* 0x1e9c2 */
void wlanWmmClear(void);					/* 0x1e9f2 */
void wlanWmmInitDefaults(void);					/* 0x1eb4e */
void wlanWmmSetupQueue(unsigned int queue);			/* 0x1eb5e */
int wlanWmmApplyApEdca(struct WlanConnection *connection);	/* 0x1ea08 */
void wlanWmmSetApsdSupport(uint8_t *ie, int enable);		/* 0x1ea54 */
int wlanWmmSendQosNull(struct WlanConnection *connection, int moreData, unsigned int tid);	/* 0x1ea6c */
unsigned int wlanWmmTid(unsigned int priority);			/* 0x41e76 */
unsigned int wlanGroupNextSequence(unsigned int tid);		/* 0x4204c */
unsigned int wlanWmmLastSequence(unsigned int tid);		/* 0x4205a */
unsigned int wlanWmmAifsn(unsigned int queue);			/* 0x42030 */
unsigned int wlanNextSequence(struct WlanConnection *connection, unsigned int tid);	/* 0x42068 */
void wlanWmmCountLost(unsigned int queue);			/* 0x41cbe */
void wlanWmmRecordDelay(struct WlanWmmStats *bin, unsigned int queueDelay, unsigned int transmitDelay);	/* 0x420f4 */
void wlanWmmFrameDone(uint8_t *data, unsigned int queue, int delivered, unsigned int usedTime,
		      unsigned int policedTime);		/* 0x41cd2 */
struct WlanTxStatus;
uint32_t wlanTxAirtime(struct WlanConnection *connection, struct WlanTxStatus *status);	/* 0x41db2 */
int wlanTxAirtimeValidated(struct WlanConnection *connection, struct WlanTxStatus *status);	/* 0x41e22 */
int wlanWmmSetQueue(struct WlanConnection *connection, uint8_t *data, unsigned int priority,
		    unsigned int delay);			/* 0x41e84 */
void wlanTxCountDropped(OsaBuffer *buffer);		/* 0x41f2e */
void wlanWmmTxDone(struct WlanConnection *connection, struct WlanTxStatus *status);	/* 0x41f6a */
void wlanWmmBuildParameterElement(uint8_t *ie);			/* 0x2d220 */
void wlanWmmFillParameterElement(uint8_t *ie);			/* 0x2d248 */
unsigned int wlanTspecDirection(const uint8_t *tsInfo, uint8_t *directions);	/* 0x2cfbc */
int wlanTspecUapsdBit(int userPriority);			/* 0x2cfde */
int wlanTspecApplyBits(int action, uint8_t *uplink, uint8_t *downlink, const uint8_t *reference,
		       const uint8_t *tspec, const uint8_t *directions);	/* 0x2d000 */
uint8_t *wlanTspecBuildAddts(OsaBuffer *buffer, const uint8_t *tspec, unsigned int length,
			     unsigned int dialogToken);				/* 0x2d092 */
void wlanWmmAddtsResult(uint8_t *command, const uint8_t *response, unsigned int length);	/* 0x2d0ca */
void wlanTspecBuildDelts(OsaBuffer *buffer, const uint8_t *tspecElement);			/* 0x2d10c */
unsigned int wlanWmmTsStatus(uint8_t *body, const uint8_t *tsInfo, const uint8_t *direction, int valid);	/* 0x2d164 */
int wlanWmmStatsCommand(uint8_t *body, struct WlanWmmStats *bins, uint16_t *policed);	/* 0x2d3d0 */
int wlanWmmResetDefaults(void *info1, void *info2, void *info3, void *element, struct WlanWmmEdca *edca,
			 uint16_t *times, unsigned int cwMin, unsigned int cwMax);	/* 0x2d4e4 */

#endif /* ROBIN_WMM_H */
