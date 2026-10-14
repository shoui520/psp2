/*
 * Block Ack session tables and the hardware window numbers of the transmit
 * sessions. The filename is inferred; no source filename survives.
 */
#include "robin/stream.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/soc.h"
#include "robin/threadx.h"

struct WlanTxBaSession wlanTxBaSessions[WLAN_TX_BA_MAX]
	__attribute__((section(".dtcm.bss")));
struct WlanRxBaSession wlanRxBaSessions[WLAN_RX_BA_MAX]
	__attribute__((section(".dtcm.bss")));

uint32_t wlanTxBaCount __attribute__((section(".itcm.rw"))) = 0;
uint32_t wlanRxBaCount __attribute__((section(".itcm.rw"))) = 0;
void (*wlanBaTablesChanged)(void) __attribute__((section(".itcm.rw"))) = NULL;
uint32_t (*wlanBaTimeoutShift)(uintptr_t owner)
	__attribute__((section(".itcm.rw"))) = NULL;
uint32_t (*wlanBaTimeoutValue)(uintptr_t owner, uint32_t timeout)
	__attribute__((section(".itcm.rw"))) = NULL;
void (*wlanBaWindowReset)(unsigned int window)
	__attribute__((section(".itcm.rw"))) = NULL;

/* Sets the table sizes and the callbacks the tables use. */
void wlanBaTablesInit(void)
{
	wlanTxBaCount = WLAN_TX_BA_MAX;
	wlanRxBaCount = WLAN_RX_BA_MAX;
	wlanBaTablesChanged = wlanBaTablesChangedHandler;
	wlanBaTimeoutShift = wlanTxBaTimeoutShift;
	wlanBaTimeoutValue = wlanTxBaTimeoutRegister;
	wlanBaWindowReset = wlanTxWindowReset;
}

static ROBIN_PATCH_SLOT(wlanBaActiveOverride);
static ROBIN_PATCH_SLOT(wlanTxBaTouchOverride);
static ROBIN_PATCH_SLOT(wlanRxBaFindOverride);
static ROBIN_PATCH_SLOT(wlanRxBaResetOverride);
static ROBIN_PATCH_SLOT(wlanTxBaIdleOverride);
static ROBIN_PATCH_SLOT(wlanRxBaFrameCountOverride);
static ROBIN_PATCH_SLOT(wlanRxBaAddOverride);
static ROBIN_PATCH_SLOT(wlanTxBaAddOverride);
static ROBIN_PATCH_SLOT(wlanRxBaHwSlotOverride);
static ROBIN_PATCH_SLOT(wlanTxBaProbeOverride);
static ROBIN_PATCH_SLOT(wlanTxBaFindOverride);
static ROBIN_PATCH_SLOT(wlanWindowPoolOverride);
static ROBIN_PATCH_SLOT(wlanTxBaRemoveOverride);

int wlanBaAnyActive(struct WlanTxBaSession *txSessions,
		    struct WlanRxBaSession *rxSessions)
{
	unsigned int i;
	int result = 0;

	if (wlanBaActiveOverride((uintptr_t)txSessions, (uintptr_t)rxSessions,
				 (uintptr_t)&result, 0))
		return result;
	for (i = 0; i < wlanTxBaCount; i = (uint8_t)(i + 1)) {
		if (txSessions[i].active)
			return 1;
	}
	for (i = 0; i < wlanRxBaCount; i = (uint8_t)(i + 1)) {
		if (rxSessions[i].state)
			return 1;
	}
	return 0;
}

struct WlanTxBaSession *wlanTxBaCountFrame(unsigned int index,
					   struct WlanTxBaSession *sessions)
{
	struct WlanTxBaSession *session = &sessions[index];

	++session->idleCount;
	return session;
}

/* A frame arrived on the session: it is no longer idle. */
void wlanTxBaTouch(unsigned int tid, const uint8_t *peer,
		   struct WlanTxBaSession *sessions)
{
	unsigned int i;

	if (wlanTxBaTouchOverride(tid, (uintptr_t)peer, (uintptr_t)sessions, 0))
		return;
	for (i = 0; i < wlanTxBaCount; i = (uint8_t)(i + 1)) {
		if (sessions[i].active && sessions[i].tid == tid &&
		    memcmp(peer, sessions[i].peer, 6) == 0) {
			sessions[i].idleCount = 0;
			return;
		}
	}
}

/* Returns the index of the receive session, or the table size when absent. */
unsigned int wlanRxBaFind(const uint8_t *peer, unsigned int tid,
			  struct WlanRxBaSession *sessions)
{
	unsigned int i;
	int result = 0;

	if (wlanRxBaFindOverride((uintptr_t)peer, tid, (uintptr_t)sessions,
				 (uintptr_t)&result))
		return (uint8_t)result;
	for (i = 0; i < wlanRxBaCount; i = (uint8_t)(i + 1)) {
		if (sessions[i].state == 1 && sessions[i].tid == tid &&
		    memcmp(sessions[i].peer, peer, 6) == 0)
			break;
	}
	return i;
}

void wlanRxBaResetCount(unsigned int tid, const uint8_t *peer,
			struct WlanRxBaSession *sessions)
{
	unsigned int index;

	if (wlanRxBaResetOverride(tid, (uintptr_t)peer, (uintptr_t)sessions, 0))
		return;
	index = wlanRxBaFind(peer, tid, sessions);
	if (index < wlanRxBaCount)
		sessions[index].frameCount = 0;
}

/* Returns the idle frame count of the transmit session, or -1 when absent. */
int wlanTxBaIdleCount(unsigned int tid, const uint8_t *peer,
		      struct WlanTxBaSession *sessions)
{
	unsigned int i;
	int result = 0;

	if (wlanTxBaIdleOverride(tid, (uintptr_t)peer, (uintptr_t)sessions,
				 (uintptr_t)&result))
		return result;
	for (i = 0; i < wlanTxBaCount; i = (uint8_t)(i + 1)) {
		if (sessions[i].active == 1 && sessions[i].tid == tid &&
		    memcmp(peer, sessions[i].peer, 6) == 0)
			break;
	}
	if (i == wlanTxBaCount)
		return -1;
	return sessions[i].idleCount;
}

/* Returns the frame count of the receive session, or -1 when absent. */
int wlanRxBaFrameCount(unsigned int tid, const uint8_t *peer,
		       struct WlanRxBaSession *sessions)
{
	unsigned int index;
	int result = 0;

	if (wlanRxBaFrameCountOverride(tid, (uintptr_t)peer, (uintptr_t)sessions,
				       (uintptr_t)&result))
		return result;
	index = wlanRxBaFind(peer, tid, sessions);
	if (index >= wlanRxBaCount)
		return -1;
	return sessions[index].frameCount;
}

void wlanRxBaAdd(const struct WlanBaRequest *request, unsigned int hwSlot,
		 struct WlanRxBaSession *sessions)
{
	struct WlanRxBaSession *session;
	unsigned int i;

	if (wlanRxBaAddOverride((uintptr_t)request, hwSlot, (uintptr_t)sessions, 0))
		return;
	for (i = 0; i < wlanRxBaCount; i = (uint8_t)(i + 1)) {
		if (sessions[i].state)
			continue;
		session = &sessions[i];
		session->tid = __aeabi_uread4(request->tid);
		session->hwSlot = hwSlot;
		__aeabi_memcpy(session->peer, request->peer, 6);
		session->state = 1;
		session->frameCount = 0;
		break;
	}
	wlanBaTablesChanged();
}

/*
 * Programs the Block Ack timeout of a transmit session into its hardware
 * window, when the timeout callback supplies a register value.
 */
void wlanTxBaArmTimeout(uintptr_t owner, unsigned int index,
			struct WlanTxBaSession *sessions)
{
	struct WlanTxBaSession *session = &sessions[index];
	uint32_t value;

	value = wlanBaTimeoutValue(owner, session->timeout);
	if (value)
		bbpSetWindowTimeout(value, session->windowId);
}

void wlanTxBaAdd(uintptr_t owner, const struct WlanBaRequest *request,
		 unsigned int hwSlot, unsigned int windowId,
		 struct WlanTxBaSession *sessions)
{
	struct WlanTxBaSession *session;
	uint32_t limit;
	uint32_t timeout;
	unsigned int i;

	if (wlanTxBaAddOverride(owner, (uintptr_t)request, hwSlot, windowId))
		return;
	limit = 1u << (wlanBaTimeoutShift(owner) + 13);
	for (i = 0; i < wlanTxBaCount; i = (uint8_t)(i + 1)) {
		if (sessions[i].active)
			continue;
		session = &sessions[i];
		session->tid = __aeabi_uread4(request->tid);
		session->hwSlot = hwSlot;
		__aeabi_memcpy(session->peer, request->peer, 6);
		session->active = 1;
		session->windowId = windowId;
		timeout = __aeabi_uread4(request->timeout);
		if (timeout)
			session->timeout = (timeout * 9) << 8;
		else
			session->timeout = 0x900;
		if (session->timeout > limit)
			session->timeout = limit;
		session->idleCount = 0;
		wlanTxBaArmTimeout(owner, i, sessions);
		break;
	}
	wlanBaTablesChanged();
}

/* Returns 1 and the hardware slot of the receive session when one exists. */
int wlanRxBaHwSlot(unsigned int tid, const uint8_t *peer, uint8_t *hwSlot,
		   struct WlanRxBaSession *sessions)
{
	unsigned int i;
	int found = 0;
	int result = 0;

	if (wlanRxBaHwSlotOverride(tid, (uintptr_t)peer, (uintptr_t)hwSlot, 0))
		return result;
	for (i = 0; i < wlanRxBaCount; i = (uint8_t)(i + 1)) {
		if (!sessions[i].state)
			continue;
		if (memcmp(peer, sessions[i].peer, 6) == 0 && sessions[i].tid == tid) {
			found = 1;
			*hwSlot = sessions[i].hwSlot;
		}
	}
	return found;
}

/* Compares the peer against every active session and keeps no result. */
void wlanTxBaProbe(unsigned int tid, const uint8_t *peer,
		   struct WlanTxBaSession *sessions)
{
	unsigned int i;

	if (wlanTxBaProbeOverride(tid, (uintptr_t)peer, 0, 0))
		return;
	for (i = 0; i < wlanTxBaCount; i = (uint8_t)(i + 1)) {
		if (sessions[i].active)
			memcmp(peer, sessions[i].peer, 6);
	}
}

/* Returns 1 and the index of the active transmit session of the peer. */
int wlanTxBaFind(unsigned int tid, const uint8_t *peer, uint8_t *index,
		 struct WlanTxBaSession *sessions)
{
	unsigned int i;
	int found = 0;
	int result = 0;

	if (wlanTxBaFindOverride(tid, (uintptr_t)peer, (uintptr_t)index, 0))
		return result;
	for (i = 0; i < wlanTxBaCount; i = (uint8_t)(i + 1)) {
		if (!sessions[i].active)
			continue;
		if (memcmp(peer, sessions[i].peer, 6) == 0 && sessions[i].tid == tid) {
			found = 1;
			*index = i;
		}
	}
	return found;
}

void wlanTxBaArmTimeouts(uintptr_t owner, struct WlanTxBaSession *sessions)
{
	unsigned int i;

	for (i = 0; i < wlanTxBaCount; i = (uint8_t)(i + 1)) {
		if (sessions[i].active)
			wlanTxBaArmTimeout(owner, i, sessions);
	}
}

/* Releases a window number and the owner slot it was assigned to. */
void wlanWindowFree(unsigned int id, uintptr_t *owners,
		    struct WlanWindowId *pool)
{
	unsigned int i;

	owners[id] = 0;
	for (i = 0; i < wlanTxBaCount; i = (uint8_t)(i + 1)) {
		if (pool[i].id == (int)id)
			pool[i].inUse = 0;
	}
}

uintptr_t wlanWindowOwner(unsigned int id, const uintptr_t *owners)
{
	return owners[id];
}

/* Window numbers start at WLAN_WINDOW_ID_BASE; one per transmit session. */
void wlanWindowPoolInit(struct WlanWindowId *pool)
{
	unsigned int i;

	if (wlanWindowPoolOverride(0, 0, 0, 0))
		return;
	for (i = 0; i < wlanTxBaCount; i = (uint8_t)(i + 1)) {
		pool[i].id = i + WLAN_WINDOW_ID_BASE;
		pool[i].inUse = 0;
	}
}

/* Takes the first free window number, or returns -1. */
int wlanWindowAlloc(struct WlanWindowId *pool)
{
	unsigned int i;

	for (i = 0; i < wlanTxBaCount; i = (uint8_t)(i + 1)) {
		if (!pool[i].inUse) {
			pool[i].inUse = 1;
			return pool[i].id;
		}
	}
	return -1;
}

int wlanTxBaWindowId(unsigned int index, uint8_t *windowId,
		     const struct WlanTxBaSession *sessions)
{
	if (index < wlanTxBaCount && sessions[index].active) {
		*windowId = sessions[index].windowId;
		return 1;
	}
	return 0;
}

/* Deletes the transmit session of a peer and returns its window and slot. */
void wlanTxBaRemove(const uint8_t *peer, unsigned int tid, uint8_t *windowId,
		    uint8_t *hwSlot, struct WlanTxBaSession *sessions)
{
	struct WlanTxBaSession *session;
	UINT posture;
	unsigned int i;

	if (wlanTxBaRemoveOverride((uintptr_t)peer, tid, (uintptr_t)windowId,
				   (uintptr_t)hwSlot))
		return;
	posture = tx_interrupt_control(TX_INT_DISABLE);
	for (i = 0; i < wlanTxBaCount; i = (uint8_t)(i + 1)) {
		session = &sessions[i];
		if (!session->active || session->tid != tid ||
		    memcmp(session->peer, peer, 6) != 0)
			continue;
		*windowId = session->windowId;
		*hwSlot = session->hwSlot;
		session->active = 0;
		session->idleCount = 0;
		break;
	}
	tx_interrupt_control(posture);
	wlanBaTablesChanged();
}

/* True when the read and write pointers of all eight transmit queues agree. */
int macTxQueuesIdle(void)
{
	return MAC_REG(0x040) == MAC_REG(0x044) &&
	       MAC_REG(0x048) == MAC_REG(0x04c) &&
	       MAC_REG(0x050) == MAC_REG(0x054) &&
	       MAC_REG(0x058) == MAC_REG(0x05c) &&
	       MAC_REG(0x060) == MAC_REG(0x064) &&
	       MAC_REG(0x068) == MAC_REG(0x06c) &&
	       MAC_REG(0x070) == MAC_REG(0x074) &&
	       MAC_REG(0x078) == MAC_REG(0x07c);
}

void bbpSetWindowTimeout(uint32_t value, unsigned int windowId)
{
	MAC_REG(0xe30 + 4 * windowId) = value;
}

/* First transmit session number whose hardware slot bit is free. */
unsigned int wlanTxBaFindFreeHwSlot(void)
{
	unsigned int slot;

	for (slot = 0; slot < wlanTxBaCount && (MAC_REG(0xac0) & (1u << slot)); slot++)
		;
	return slot;
}

/* First receive session number whose hardware slot bit is free. */
unsigned int wlanRxBaFindFreeHwSlot(void)
{
	unsigned int slot;

	for (slot = 0; slot < wlanRxBaCount && (MAC_REG(0xac0) & (16u << slot)); slot++)
		;
	return slot;
}
