/*
 * Connection control block list: lookups and release. The filename is
 * inferred; no source filename survives.
 */
#include "robin/btrom.h"
#include "robin/conn.h"
#include "robin/mactx.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/sys.h"

struct WlanConnection *wlanConnectionList __attribute__((section(".sqram1.rw")));
struct WlanConnection *wlanConnectionFreeList __attribute__((section(".sqram1.rw")));
struct WlanConnection *wlanConnectionSession __attribute__((section(".sqram1.rw")));
struct WlanConnection *wlanConnectionPrimary __attribute__((section(".sqram1.rw")));
/* The DTCM block at 0x040088dc: the channel in use, the slots (rings) taken, the counts and the slot table. */
uint8_t wlanChannelInUse[1] __attribute__((section(".dtcm.bss")));				/* 0x040088dc */
uint8_t wlanConnectionSlotMask __attribute__((section(".dtcm.bss")));				/* 0x040088dd */
uint32_t wlanConnectionCount __attribute__((section(".dtcm.bss")));				/* 0x040088e4 */
uint32_t wlanConnectionTable __attribute__((section(".dtcm.bss")));				/* 0x040088e8 */
struct WlanConnection *wlanConnectionSlots[WLAN_CONNECTION_SLOTS] __attribute__((section(".dtcm.bss")));	/* 0x040088f0 */

/* The beacon and the probe response of each slot: their buffers, pools and the requests the MAC reads. */
extern struct OsaBufferPool wlanBlockPool0;							/* 0x04000ec0 */
extern struct OsaBufferPool wlanBlockPool1;							/* 0x04000ed4 */
extern uint8_t wlanBssBeaconRequests[WLAN_CONNECTION_SLOTS][48];				/* SQRAM 0xc0031d64 */
extern uint8_t wlanBssProbeResponseRequests[WLAN_CONNECTION_SLOTS][48];			/* SQRAM 0xc0031e24 */
void uapSqramHandleState(struct WlanConnection *connection);					/* SQRAM 0xc0007200 */
void macTxNotifyReleaseBuffer(void);								/* 0x3239c */
void macTxNotifyClearBuffer(unsigned int ring, unsigned int index, int enable, unsigned int beaconPeriodA,
			    unsigned int beaconPeriodB);						/* 0x323f8 */

/* Type-specific teardown, not converted yet. */
int wlanConnectionTeardownWds(struct WlanConnection *connection);	/* 0x1d146 */
void wlanConnectionTeardownIbss(struct WlanConnection *connection);	/* 0x1d0ea */
void wlanConnectionTeardownBss(struct WlanConnection *connection);	/* 0x1cc8e */

/* Finds the connection of a BSS type and number. */
struct WlanConnection *wlanConnectionFind(unsigned int bssType, unsigned int bssNumber)
{
	struct WlanConnection *connection;
	UINT posture = sysIrqDisable();

	for (connection = wlanConnectionList; connection; connection = connection->next) {
		if (connection->bssType == bssType && connection->bssNumber == bssNumber) {
			sysIrqRestore(posture);
			return connection;
		}
	}
	sysIrqRestore(posture);
	return NULL;
}

int wlanConnectionExists(unsigned int interfaceType)
{
	struct WlanConnection *connection;
	UINT posture = sysIrqDisable();

	for (connection = wlanConnectionList; connection; connection = connection->next) {
		if (connection->interfaceType == interfaceType) {
			sysIrqRestore(posture);
			return 1;
		}
	}
	sysIrqRestore(posture);
	return 0;
}

struct WlanConnection *wlanConnectionFirst(unsigned int interfaceType)
{
	struct WlanConnection *connection;
	UINT posture = sysIrqDisable();

	for (connection = wlanConnectionList; connection; connection = connection->next) {
		if (connection->interfaceType == interfaceType) {
			sysIrqRestore(posture);
			return connection;
		}
	}
	sysIrqRestore(posture);
	return NULL;
}

/* Finds the connection whose peer (BSSID) address matches. */
struct WlanConnection *wlanConnectionFindByPeer(const uint8_t *peer)
{
	struct WlanConnection *connection;
	UINT posture = sysIrqDisable();

	for (connection = wlanConnectionList; connection; connection = connection->next) {
		if (memcmp(connection->peerAddress, peer, 6) == 0) {
			sysIrqRestore(posture);
			return connection;
		}
	}
	sysIrqRestore(posture);
	return NULL;
}

/* Finds the connection of an interface type with the given local address. */
struct WlanConnection *wlanConnectionFindByLocal(unsigned int interfaceType, const uint8_t *local)
{
	struct WlanConnection *connection;
	UINT posture = sysIrqDisable();

	for (connection = wlanConnectionList; connection; connection = connection->next) {
		if (connection->interfaceType == interfaceType &&
		    memcmp(connection->localAddress, local, 6) == 0) {
			sysIrqRestore(posture);
			return connection;
		}
	}
	sysIrqRestore(posture);
	return NULL;
}

/* Tears the connection down, unlinks it and returns it to the free list. */
void wlanConnectionRelease(struct WlanConnection *connection)
{
	struct WlanConnection *previous;
	struct WlanConnection *other;
	UINT posture;
	UINT inner;

	if (connection->flags2 & 4) {
		if (!wlanConnectionTeardownWds(connection))
			return;
	} else if (connection->interfaceType == 1) {
		wlanConnectionTeardownIbss(connection);
	} else if (connection->interfaceType == 2 ||
		   (connection->interfaceType == 3 && connection->mode == 2)) {
		wlanConnectionTeardownBss(connection);
	}
	posture = sysIrqDisable();
	/* The original spins forever when the connection is not on the list. */
	if (!connection)
		for (;;)
			;
	if (wlanConnectionList == connection) {
		wlanConnectionList = wlanConnectionList->next;
	} else {
		if (!wlanConnectionList)
			for (;;)
				;
		previous = wlanConnectionList;
		while (previous->next != connection) {
			if (!previous->next)
				for (;;)
					;
			previous = previous->next;
		}
		previous->next = connection->next;
	}
	inner = sysIrqDisable();
	for (other = wlanConnectionList; other; other = other->next) {
		if (other->channel) {
			sysIrqRestore(inner);
			goto linked;
		}
	}
	sysIrqRestore(inner);
	wlanChannelInUse[0] = 0;
linked:
	connection->next = wlanConnectionFreeList;
	wlanConnectionFreeList = connection;
	sysIrqRestore(posture);
}

struct WlanConnection *wlanConnectionGetSession(void)
{
	return wlanConnectionSession;
}

int wlanConnectionSetSession(struct WlanConnection *connection)
{
	wlanConnectionSession = connection;
	return 0;
}

int wlanConnectionGetCount(void)
{
	return wlanConnectionCount;
}

int wlanConnectionIncrement(void)
{
	return ++wlanConnectionCount;
}

int wlanConnectionDecrement(void)
{
	return --wlanConnectionCount;
}

uint32_t wlanConnectionGetTable(void)
{
	return wlanConnectionTable;
}

/*
 * Gives the connection the first free slot: its group ring, the beacon and
 * probe response buffers, and the requests the MAC reads. 1: no slot is free.
 * A buffer that cannot be had stops the processor.
 */
int wlanConnectionSlotAcquire(struct WlanConnection *connection)
{
	unsigned int slot;
	OsaBuffer *buffer;

	for (slot = 0; (1 << slot) & wlanConnectionSlotMask; ) {
		slot = (uint8_t)(slot + 1);
		if (slot >= WLAN_CONNECTION_SLOTS)
			return 1;
	}
	connection->groupRing = slot;
	wlanConnectionSlotMask |= 1 << slot;
	wlanConnectionSlots[slot] = connection;
	buffer = osa_BufferAlloc(&wlanBlockPool0, 100, 0);
	connection->beaconBuffer = buffer;
	if (buffer == NULL)
		for (;;)
			;
	__aeabi_memclr4(wlanBssBeaconRequests[slot], 48);
	connection->beaconRequest = (struct WlanMacTxRequest *)wlanBssBeaconRequests[slot];
	buffer = osa_BufferAlloc(&wlanBlockPool1, 100, 0);
	connection->probeResponseBuffer = buffer;
	if (buffer == NULL)
		for (;;)
			;
	__aeabi_memclr4(wlanBssProbeResponseRequests[slot], 48);
	connection->probeResponseRequest = (struct WlanMacTxRequest *)wlanBssProbeResponseRequests[slot];
	if (connection->interfaceType != WLAN_IF_IBSS) {
		if (wlanConnectionCount == 1)
			macTxNotifyReleaseBuffer();
		macTxNotifyClearBuffer(connection->groupRing, connection->interfaceIndex, 1,
				       connection->dtimPeriod, connection->dtimPeriod);
	}
	return 0;
}

/*
 * Takes the connection's slot unless a slot already belongs to its BSS (the
 * same type and number); the first connection also names the channel in use.
 */
int wlanConnectionSlotAttach(struct WlanConnection *connection)
{
	unsigned int slot;

	if (wlanChannelInUse[0] == 0)
		wlanChannelInUse[0] = connection->channel;
	if (connection->interfaceType == WLAN_IF_IBSS || connection->interfaceType == WLAN_IF_STATION ||
	    connection->interfaceType == WLAN_IF_AP || connection->interfaceType == WLAN_IF_AMP) {
		for (slot = 0; slot < WLAN_CONNECTION_SLOTS; slot = (uint8_t)(slot + 1)) {
			struct WlanConnection *other;

			if (!((1 << slot) & wlanConnectionSlotMask))
				continue;
			other = wlanConnectionSlots[slot];
			if (other->bssNumber == connection->bssNumber && other->bssType == connection->bssType)
				return 0;
		}
		return wlanConnectionSlotAcquire(connection);
	}
	return 0;
}

/* Gives the connection's slot buffers back; the slot itself is released by wlanConnectionRelease(). */
int wlanConnectionSlotDetach(struct WlanConnection *connection)
{
	if (connection->interfaceType == WLAN_IF_IBSS || connection->interfaceType == WLAN_IF_STATION ||
	    connection->interfaceType == WLAN_IF_AP || connection->interfaceType == WLAN_IF_AMP) {
		if (connection->beaconBuffer != NULL)
			osa_BufferFree(connection->beaconBuffer);
		if (connection->probeResponseBuffer != NULL)
			osa_BufferFree(connection->probeResponseBuffer);
		connection->beaconRequest = NULL;
		connection->beaconBuffer = NULL;
		connection->probeResponseRequest = NULL;
		connection->probeResponseBuffer = NULL;
		uapSqramHandleState(connection);
		connection->groupRing = 0xff;
	}
	return 0;
}

/* The role block of a station or access point connection, at its information pointer. */
struct WlanConnectionRole *wlanConnectionInfoSlot(struct WlanConnection *connection)
{
	if (connection == NULL)
		return NULL;
	if (connection->interfaceType == WLAN_IF_STATION)
		return (struct WlanConnectionRole *)&connection->stationInfo;
	if (connection->interfaceType == WLAN_IF_AP)
		return (struct WlanConnectionRole *)&connection->apInfo;
	return NULL;
}

struct WlanConnectionInfo *wlanConnectionInfo(struct WlanConnection *connection)
{
	if (connection != NULL) {
		if (connection->interfaceType == WLAN_IF_STATION)
			return connection->stationInfo;
		if (connection->interfaceType == WLAN_IF_AP)
			return connection->apInfo;
	}
	return NULL;
}

/* Everything but a station role is ready; a station role needs its information. */
int wlanConnectionIsReady(struct WlanConnection *connection)
{
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);

	return connection->mode != 2 || (info != NULL && info->ready != 0);
}

/* Bit 3 of the second flag byte: the connection is the active one. */
void wlanConnectionSetActiveFlag(struct WlanConnection *connection, int active)
{
	connection->flags2 = (connection->flags2 & ~8) | ((8 * active) & 0xf);
}

/* True when the frame belongs to an AMP (Bluetooth high speed) link. */
int wlanBufferUsesAmp(OsaBuffer *buffer)
{
	struct WlanConnection *connection = buffer->context;

	if (connection == NULL)
		return 0;
	return connection->interfaceType == WLAN_IF_AMP;
}

void *wlanGetBufferDevice(OsaBuffer *buffer)
{
	struct WlanConnection *connection = buffer->context;

	return connection->device;
}

/*
 * Next connection after one in the list that has bit 2 of the second flag
 * byte set and the given interface type; interrupts are masked while the
 * list is walked.
 */
struct WlanConnection *wlanConnectionNextFlagged(struct WlanConnection *after,
						 unsigned int interfaceType)
{
	struct WlanConnection *found = NULL;
	struct WlanConnection *connection;
	struct WlanConnection *candidate;
	unsigned int posture = sysIrqDisable();

	for (connection = wlanConnectionList; connection != NULL; connection = connection->next) {
		if (connection != after)
			continue;
		for (candidate = connection->next; candidate != NULL && found == NULL;
		     candidate = candidate->next) {
			if ((candidate->flags2 & 4) && candidate->interfaceType == interfaceType)
				found = candidate;
		}
		break;
	}
	sysIrqRestore(posture);
	return found;
}

int wlanConnectionsIdle(void)
{
	struct WlanConnection *connection;
	unsigned int posture = sysIrqDisable();

	for (connection = wlanConnectionList; connection != NULL; connection = connection->next) {
		if (connection->state != 0) {
			sysIrqRestore(posture);
			return 0;
		}
	}
	sysIrqRestore(posture);
	return 1;
}

int wlanNoConnectionActive(void)
{
	struct WlanConnection *connection;
	unsigned int posture = sysIrqDisable();

	for (connection = wlanConnectionList; connection != NULL; connection = connection->next) {
		if (connection->state != 0) {
			sysIrqRestore(posture);
			return 0;
		}
	}
	sysIrqRestore(posture);
	return 1;
}

/* Next connection after one in the list that belongs to the given BSS. */
struct WlanConnection *wlanConnectionNextByBss(struct WlanConnection *after, unsigned int bssType,
					       unsigned int bssNumber)
{
	struct WlanConnection *connection;
	struct WlanConnection *candidate;
	unsigned int posture = sysIrqDisable();

	for (connection = wlanConnectionList; connection != NULL; connection = connection->next) {
		if (connection != after)
			continue;
		for (candidate = connection->next; candidate != NULL; candidate = candidate->next) {
			if (candidate->bssType == bssType && candidate->bssNumber == bssNumber) {
				sysIrqRestore(posture);
				return candidate;
			}
		}
		break;
	}
	sysIrqRestore(posture);
	return NULL;
}

int wlanAnyConnectionHostFlag8(void)
{
	struct WlanConnection *connection;
	unsigned int posture = sysIrqDisable();

	for (connection = wlanConnectionList; connection != NULL; connection = connection->next) {
		if (connection->hostFlags & WLAN_CONN_HOST_FLAG_8) {
			sysIrqRestore(posture);
			return 1;
		}
	}
	sysIrqRestore(posture);
	return 0;
}

int wlanAnyConnectionFlag4(void)
{
	struct WlanConnection *connection;
	unsigned int posture = sysIrqDisable();

	for (connection = wlanConnectionList; connection != NULL; connection = connection->next) {
		if (connection->flags & 4) {
			sysIrqRestore(posture);
			return 1;
		}
	}
	sysIrqRestore(posture);
	return 0;
}

int wlanAnyConnectionFlag10(void)
{
	struct WlanConnection *connection;
	unsigned int posture = sysIrqDisable();

	for (connection = wlanConnectionList; connection != NULL; connection = connection->next) {
		if (connection->flags & 0x10) {
			sysIrqRestore(posture);
			return 1;
		}
	}
	sysIrqRestore(posture);
	return 0;
}

/* A connection that is up and has bit 0 of the second flag byte. */
int wlanAnyActiveConnectionFlags2Bit0(void)
{
	struct WlanConnection *connection;
	unsigned int posture = sysIrqDisable();

	for (connection = wlanConnectionList; connection != NULL; connection = connection->next) {
		if (connection->state != 0 && (connection->flags2 & 1)) {
			sysIrqRestore(posture);
			return 1;
		}
	}
	sysIrqRestore(posture);
	return 0;
}

int wlanAnyConnectionFlags2Bit4(void)
{
	struct WlanConnection *connection;
	unsigned int posture = sysIrqDisable();

	for (connection = wlanConnectionList; connection != NULL; connection = connection->next) {
		if (connection->flags2 & 0x10) {
			sysIrqRestore(posture);
			return 1;
		}
	}
	sysIrqRestore(posture);
	return 0;
}

struct WlanConnection *wlanConnectionFindByTypeNumber(unsigned int interfaceType,
						      unsigned int bssNumber)
{
	struct WlanConnection *connection;
	unsigned int posture = sysIrqDisable();

	for (connection = wlanConnectionList; connection != NULL; connection = connection->next) {
		if (connection->interfaceType == interfaceType && connection->bssNumber == bssNumber) {
			sysIrqRestore(posture);
			return connection;
		}
	}
	sysIrqRestore(posture);
	return NULL;
}

int wlanConnectionHasLocalAddress(const struct WlanConnection *connection, const void *address)
{
	return memcmp(connection->localAddress, address, 6) == 0;
}

int wlanContextUsesEthernetFrames(const void *context)
{
	return (((const struct WlanConnection *)context)->hostFlags >> 4) & 1;
}

/* Number of different interface types (below 6) among the connections. */
int wlanConnectionTypeCount(void)
{
	struct WlanConnection *connection;
	unsigned int posture = sysIrqDisable();
	unsigned int seen = 0;
	int count = 0;

	for (connection = wlanConnectionList; connection != NULL; connection = connection->next) {
		unsigned int type = connection->interfaceType;

		if (type < 6 && ((1 << type) & seen) == 0) {
			++count;
			seen |= 1 << type;
		}
	}
	sysIrqRestore(posture);
	return count;
}

/* First connection with bit 2 of the second flag byte and the given local address. */
struct WlanConnection *wlanConnectionFindFlaggedByLocal(const void *address)
{
	struct WlanConnection *found = NULL;
	struct WlanConnection *connection;
	unsigned int posture = sysIrqDisable();

	for (connection = wlanConnectionList; connection != NULL && found == NULL;
	     connection = connection->next) {
		if ((connection->flags2 & 4) && memcmp(connection->localAddress, address, 6) == 0)
			found = connection;
	}
	sysIrqRestore(posture);
	return found;
}
