/*
 * Block Ack sessions: the transmit sessions (this station aggregates toward a
 * peer) and the receive sessions (a peer aggregates toward this station), both
 * keyed by TID and peer address, plus the hardware window numbers the transmit
 * sessions use. The tables live in DTCM; the counts and the notification
 * callbacks below are set by wlanBaTablesInit().
 */
#ifndef ROBIN_STREAM_H
#define ROBIN_STREAM_H

#include <stdint.h>
#include "robin/patch.h"

/* Transmit session (20 bytes). */
struct WlanTxBaSession {
	uint8_t active;
	uint8_t tid;
	uint8_t hwSlot;
	uint8_t windowId;
	uint8_t peer[6];
	uint8_t reserved[2];
	uint32_t idleCount;
	uint32_t timeout;
};

/* Receive session (16 bytes). */
struct WlanRxBaSession {
	uint8_t state;
	uint8_t tid;
	uint8_t hwSlot;
	uint8_t peer[6];
	uint8_t reserved[3];
	uint32_t frameCount;
};

/* Parameters of an add-Block-Ack request; other bytes are not read here. */
struct WlanBaRequest {
	uint8_t flags;
	uint8_t reserved0[3];
	uint8_t tid[4];		/* unaligned word, low byte used */
	uint8_t timeout[4];	/* unaligned word, in Block Ack timeout units */
	uint8_t reserved1[8];
	uint8_t peer[6];
	uint8_t startSequence[2];
};

/* Free list of hardware window numbers (2 bytes per transmit session). */
struct WlanWindowId {
	uint8_t inUse;
	int8_t id;
};

_Static_assert(sizeof(struct WlanTxBaSession) == 20, "transmit session");
_Static_assert(sizeof(struct WlanRxBaSession) == 16, "receive session");
_Static_assert(sizeof(struct WlanBaRequest) == 28, "Block Ack request");

enum {
	WLAN_TX_BA_MAX = 2,
	WLAN_RX_BA_MAX = 10,
	WLAN_WINDOW_ID_BASE = 5,
};

extern struct WlanTxBaSession wlanTxBaSessions[WLAN_TX_BA_MAX];
extern struct WlanRxBaSession wlanRxBaSessions[WLAN_RX_BA_MAX];

extern uint32_t wlanTxBaCount;
extern uint32_t wlanRxBaCount;
extern void (*wlanBaTablesChanged)(void);
extern uint32_t (*wlanBaTimeoutShift)(uintptr_t owner);
extern uint32_t (*wlanBaTimeoutValue)(uintptr_t owner, uint32_t timeout);
extern void (*wlanBaWindowReset)(unsigned int window);

void wlanBaTablesInit(void);
/* Not converted yet; defined by the MAC transmit and coprocessor code. */
void wlanBaTablesChangedHandler(void);
uint32_t wlanTxBaTimeoutShift(uintptr_t owner);
uint32_t wlanTxBaTimeoutRegister(uintptr_t owner, uint32_t timeout);
void wlanTxWindowReset(unsigned int window);
int wlanBaAnyActive(struct WlanTxBaSession *txSessions,
		    struct WlanRxBaSession *rxSessions);
struct WlanTxBaSession *wlanTxBaCountFrame(unsigned int index,
					   struct WlanTxBaSession *sessions);
void wlanTxBaTouch(unsigned int tid, const uint8_t *peer,
		   struct WlanTxBaSession *sessions);
unsigned int wlanRxBaFind(const uint8_t *peer, unsigned int tid,
			  struct WlanRxBaSession *sessions);
void wlanRxBaResetCount(unsigned int tid, const uint8_t *peer,
			struct WlanRxBaSession *sessions);
int wlanTxBaIdleCount(unsigned int tid, const uint8_t *peer,
		      struct WlanTxBaSession *sessions);
int wlanRxBaFrameCount(unsigned int tid, const uint8_t *peer,
		       struct WlanRxBaSession *sessions);
void wlanRxBaAdd(const struct WlanBaRequest *request, unsigned int hwSlot,
		 struct WlanRxBaSession *sessions);
void wlanTxBaArmTimeout(uintptr_t owner, unsigned int index,
			struct WlanTxBaSession *sessions);
void wlanTxBaAdd(uintptr_t owner, const struct WlanBaRequest *request,
		 unsigned int hwSlot, unsigned int windowId,
		 struct WlanTxBaSession *sessions);
int wlanRxBaHwSlot(unsigned int tid, const uint8_t *peer, uint8_t *hwSlot,
		   struct WlanRxBaSession *sessions);
void wlanTxBaProbe(unsigned int tid, const uint8_t *peer,
		   struct WlanTxBaSession *sessions);
int wlanTxBaFind(unsigned int tid, const uint8_t *peer, uint8_t *index,
		 struct WlanTxBaSession *sessions);
void wlanTxBaArmTimeouts(uintptr_t owner, struct WlanTxBaSession *sessions);
void wlanWindowFree(unsigned int id, uintptr_t *owners,
		    struct WlanWindowId *pool);
uintptr_t wlanWindowOwner(unsigned int id, const uintptr_t *owners);
void wlanWindowPoolInit(struct WlanWindowId *pool);
int wlanWindowAlloc(struct WlanWindowId *pool);
int wlanTxBaWindowId(unsigned int index, uint8_t *windowId,
		     const struct WlanTxBaSession *sessions);
void wlanTxBaRemove(const uint8_t *peer, unsigned int tid, uint8_t *windowId,
		    uint8_t *hwSlot, struct WlanTxBaSession *sessions);
unsigned int wlanTxBaFindFreeHwSlot(void);
unsigned int wlanRxBaFindFreeHwSlot(void);
int macTxQueuesIdle(void);
void bbpSetWindowTimeout(uint32_t value, unsigned int windowId);

#endif /* ROBIN_STREAM_H */
