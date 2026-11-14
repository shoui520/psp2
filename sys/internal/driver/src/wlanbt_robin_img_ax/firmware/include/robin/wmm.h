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
#include "robin/conn.h"

#define WLAN_WMM_QUEUES		4
#define WLAN_WMM_SLOTS		8

/* A transmit queue's state in the MAC: only the part the WMM code sets (96 bytes). */
struct WlanWmmSlot {
	uint8_t reserved0[85];
	uint8_t ownQueue;		/* the queue that normally uses the slot */
	uint8_t fallbackQueue;		/* the queue it falls back to when admission control rules the queue out */
	uint8_t activeQueue;		/* the queue using it now */
	uint8_t activeRing;		/* the transmit ring that goes with the active queue */
	uint8_t reserved89[7];
};

/* The parameter record of one hardware queue (an access category, the first two swapped). */
struct WlanWmmQueue {
	uint8_t info;			/* bits 1-2: access category, bit 3: admission control mandatory, bits 4-7: AIFSN */
	uint8_t contention;		/* ECWmin << 4 | ECWmax */
	uint16_t txop;			/* TXOP limit */
	struct WlanWmmSlot *slots[2];
	uint16_t admittedTime;		/* the medium time a TSPEC was admitted for; 0: none */
	uint8_t reserved14[2];
	uint8_t streams;		/* admitted streams in the queue */
	uint8_t fallback;		/* the queue it falls back to without admission: 255: none */
	uint8_t reserved18[2];
};

_Static_assert(sizeof(struct WlanWmmSlot) == 96, "WMM slot");
_Static_assert(sizeof(struct WlanWmmQueue) == 20, "WMM queue");

struct WlanWmm {
	uint8_t enabled;		/* the BSS is a WMM one */
	uint8_t parameterSetCount;	/* the parameter set count of the parameter element in use */
	uint8_t reserved2[2];
	struct WlanWmmQueue queues[WLAN_WMM_QUEUES];
};

extern struct WlanWmm wlanWmm;					/* 0x04008c4c */
extern struct WlanWmmSlot wlanWmmSlots[WLAN_WMM_SLOTS];		/* 0x04008ca0 */
extern uint8_t wlanWmmCurrentSlot;				/* 0x04009007 */
extern uint8_t wlanWmmFlowBits;					/* 0x04008fda */
extern uint8_t wlanWmmSessionBits;				/* 0x04008fec */
extern uint8_t wlanWmmElement[26];				/* 0x04008fed: the parameter element, ID 221 */
extern uint16_t wlanWmmDefaultTime;				/* 0x00054ec0 */
extern const uint8_t wlanWmmRings[];				/* SQRAM 0xc000fa9b */

int wlanWmmEnable(void);					/* 0x1e570 */
int wlanWmmEnabled(void);					/* 0x41cb8 */
int wlanWmmApplyQueue(unsigned int queue);			/* 0x1e578 */
int wlanWmmSetParameters(const uint8_t *element, uint8_t *count);	/* 0x1e63c */
int wlanWmmQueueStatus(const struct WlanConnection *connection, uint8_t *tlvs);	/* 0x1e6c8 */
int wlanWmmReportStatus(struct WlanConnection *connection);	/* 0x1e79a */
int wlanWmmUpdate(struct WlanConnection *connection, const uint8_t *info, const uint8_t *parameter);	/* 0x1e7cc */
int wlanWmmQueueBit(int queue);					/* 0x2d1c6 */
int wlanWmmSetEdca(unsigned int queue, const struct WlanWmmQueue *record);	/* 0x2d1e2 */
int wlanWmmParseParameter(const uint8_t *element, uint8_t *count);	/* 0x2d2e2 */
struct WlanWmmQueue *wlanWmmStoreQueue(struct WlanWmmQueue *queue, const uint8_t *record);	/* 0x2d338 */
int wlanWmmQueueConfig(uint8_t *body, uint16_t *table, uint32_t *length);	/* 0x2d38c */
int wlanWmmAppendIe(uint8_t *body, const uint8_t *element);	/* 0x2d4b0 */

#endif /* ROBIN_WMM_H */
