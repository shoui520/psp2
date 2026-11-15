/*
 * Connection control block list: lookups and release. The filename is
 * inferred; no source filename survives.
 */
#include "robin/btpower.h"
#include "robin/ratesel.h"
#include "robin/mgmtentry.h"
#include "robin/btrom.h"
#include "robin/calibration.h"
#include "robin/conn.h"
#include "robin/hostps.h"
#include "robin/ie.h"
#include "robin/mac.h"
#include "robin/mactx.h"
#include "robin/mgmt.h"
#include "robin/osa.h"
#include "robin/signal.h"
#include "robin/stream.h"
#include "robin/subscribe.h"
#include "robin/txring.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/sys.h"
#include "robin/htba.h"
#include "robin/peerrate.h"
#include "robin/stamachine.h"

struct WlanConnection *wlanConnectionList __attribute__((section(".sqram1.rw")));
struct WlanConnection *wlanConnectionFreeList __attribute__((section(".sqram1.rw")));
struct WlanConnection *wlanConnectionSession __attribute__((section(".sqram1.rw")));
struct WlanConnection *wlanConnectionPrimary __attribute__((section(".sqram1.rw")));
/* The DTCM block at 0x040088dc: the channel in use, the slots (rings) taken, the counts and the slot table. */
uint8_t wlanChannelInUse[1] __attribute__((section(".dtcm.bss")));				/* 0x040088dc */
uint8_t wlanConnectionSlotMask __attribute__((section(".dtcm.bss")));				/* 0x040088dd */
uint32_t wlanConnectionCount __attribute__((section(".dtcm.bss")));				/* 0x040088e4 */
uint32_t wlanConnectionIndexMask __attribute__((section(".dtcm.bss")));			/* 0x040088e0: interface indices taken */
uint32_t wlanConnectionDtimHolds __attribute__((section(".dtcm.bss")));			/* 0x040088e8: connections holding the DTIM */
struct WlanConnection *wlanConnectionSlots[WLAN_CONNECTION_SLOTS] __attribute__((section(".dtcm.bss")));	/* 0x040088f0 */

/* The beacon and the probe response of each slot: their buffers, pools and the requests the MAC reads. */
extern uint8_t wlanBssBeaconRequests[WLAN_CONNECTION_SLOTS][48];				/* SQRAM 0xc0031d64 */
extern uint8_t wlanBssProbeResponseRequests[WLAN_CONNECTION_SLOTS][48];			/* SQRAM 0xc0031e24 */
void uapSqramHandleState(struct WlanConnection *connection);					/* SQRAM 0xc0007200 */
uint64_t wlanSignalTimeNow(void);								/* 0x2a4fc */
void clockHostSelectPreset0(void);								/* 0x26b78 */
void clockHostSelectPreset1(void);								/* 0x26baa */
void clockHostSelectPreset2(void);									/* 0x26bdc */
void clockHostSelectPreset3(void);								/* 0x26c0e */
int macMgmtSelectContext(struct WlanConnection *connection);					/* 0x35f7c */
void macTxNotifyReleaseBuffer(void);								/* 0x3239c */
void macTxNotifyClearBuffer(unsigned int ring, unsigned int index, int enable, unsigned int beaconPeriodA,
			    unsigned int beaconPeriodB);						/* 0x323f8 */

/* Not converted yet; provisional names. */
struct WlanConnection *p2pDirectProcessEntry(int kind);					/* 0x41250 */
int macTxNotifyAcquireBuffer(int kind, const uint8_t *address, unsigned int index, int fresh);	/* 0x322f2 */
void uapSqramUpdateBuffer(struct WlanConnection *connection);					/* SQRAM 0xc000a9ba */
/* The pools: of connections, of beacon and probe response templates, of information blocks. */
#define POOL(name, size, count)									\
	struct OsaBufferPool name __attribute__((section(".dtcm.rw"))) = { { { 1, 0, 0 } } };	\
	static struct OsaPoolConfig name##Config __attribute__((section(".dtcm.rw"))) = { 0, size, count }

POOL(wlanConnectionPool, 0x3cc, 12);								/* 0x04000e98 */
POOL(wlanBlockPool0, 0x41c, 4);									/* 0x04000ec0 */
POOL(wlanBlockPool1, 0x41c, 4);									/* 0x04000ed4 */
POOL(wlanBlockPool2, 0x4fc, 2);									/* 0x04000ee8 */
extern uint8_t wlanConnectionPoolControl[], wlanConnectionPoolMemory[];				/* 0x04004f88, 0x04005000 */
extern uint8_t wlanBlockPool0Control[], wlanBlockPool0Memory[];					/* 0x04004fe0, SQRAM 0xc002fa40 */
extern uint8_t wlanBlockPool1Control[], wlanBlockPool1Memory[];					/* 0x04004ff0, SQRAM 0xc0030ac0 */
extern uint8_t wlanBlockPool2Control[], wlanBlockPool2Memory[];					/* 0x04000f08, 0x04007dc0 */
uint32_t wlanConnectionSlotsFree __attribute__((section(".dtcm.bss")));			/* 0x040088ec */
struct OsaIdleHook wlanConnectionIdleHook __attribute__((section(".sqram1.rw")));		/* 0xc0011b00 */
/* The device's address and the SNMP defaults a connection starts with (the pointer is at 0x54a7c). */
struct WlanDeviceConfig {
	uint8_t address[6];
	uint8_t rtsThreshold[2];
	uint8_t shortRetryLimit;
	uint8_t longRetryLimit;
	uint8_t fragThreshold[2];
};
extern const struct WlanDeviceConfig *wlanDeviceAddress;					/* 0x54a7c */
void ampPalCopySupportedRates(struct WlanConnection *connection);				/* 0x2372a */
int wlanAmpRxManagement(OsaBuffer *buffer, uintptr_t argument);					/* 0xfa20 */
int wlanStaRxManagement(OsaBuffer *buffer, uintptr_t argument);					/* 0xf96c */
int wlanStaRxControl(const WlanWirelessFrame *frame, OsaBuffer *buffer);					/* 0xfdb2 */
int wlanApRxManagement(OsaBuffer *buffer, uintptr_t argument);					/* 0xfb70 */
int wlanApRxControl(const WlanWirelessFrame *frame, OsaBuffer *buffer);					/* 0xfb1c */

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

uint32_t wlanConnectionDtimHoldCount(void)
{
	return wlanConnectionDtimHolds;
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

/* Releases the information buffer of an access point or station connection that owns one. */
int wlanConnectionTeardownBss(struct WlanConnection *connection)
{
	struct WlanConnectionRole *role;

	if (connection == NULL)
		return 1;
	role = wlanConnectionInfoSlot(connection);
	if (connection->flags2 & 4) {
		if (role->infoBuffer != NULL) {
			osa_BufferFree(role->infoBuffer);
			role->infoBuffer = NULL;
			role->info = NULL;
		}
	}
	return 0;
}

/* 1: a connection of the type has its BSS up (an access point; a station: one acting as one). */
int wlanConnectionAnyStarted(unsigned int interfaceType)
{
	int started = 0;
	UINT posture = sysIrqDisable();
	struct WlanConnection *connection = wlanConnectionFirst(interfaceType);

	do {
		if (connection == NULL)
			break;
		if ((interfaceType == WLAN_IF_STATION ||
		     (interfaceType == WLAN_IF_AP && connection->mode == 2)) &&
		    wlanConnectionInfo(connection)->bssStarted == 1)
			started = 1;
		connection = wlanConnectionNextFlagged(connection, interfaceType);
	} while (started == 0);
	sysIrqRestore(posture);
	return started;
}

/*
 * Whether the radio may take the command (a host command, an event code): not
 * while an access point and a station run together, and not with an AMP link.
 * If it may, the ROM's library is told.
 */
int wlanConnectionCommandPrepare(unsigned int command, uintptr_t argument)
{
	int allowed = 1;
	UINT posture = sysIrqDisable();

	switch (command) {
	case 1: case 5: case 6: case 13:
		allowed = wlanConnectionAnyStarted(WLAN_IF_AP) != 1;
		if (wlanConnectionAnyStarted(WLAN_IF_STATION) == 1)
			allowed = 0;
		if (command == 13)
			break;
		if (wlanConnectionExists(WLAN_IF_AMP) != 0) {
			allowed = 0;
			goto done;
		}
		break;
	case 3: {
		struct WlanConnection *connection = p2pDirectProcessEntry(3);

		if ((connection != NULL || (connection = wlanConnectionFirst(0)) != NULL) &&
		    (connection->flags2 & 0x10))
			allowed = 0;
		if (wlanConnectionExists(WLAN_IF_AMP) != 0) {
			allowed = 0;
			goto done;
		}
		break;
	}
	default:
		break;
	}
	if (allowed == 1)
		romLibDispatchContext(argument);
done:
	sysIrqRestore(posture);
	return allowed;
}

/* The same decision for a command the ROM's library only checks. */
int wlanConnectionCommandCheck(unsigned int command, uintptr_t argument)
{
	int allowed = 1;
	UINT posture = sysIrqDisable();

	switch (command) {
	case 1: case 4: case 5: case 6: case 7: case 8: case 9: case 13: case 14: case 15:
		allowed = wlanConnectionAnyStarted(WLAN_IF_AP) != 1;
		if (wlanConnectionAnyStarted(WLAN_IF_STATION) == 1)
			allowed = 0;
		if (command != 13 && wlanConnectionExists(WLAN_IF_AMP) != 0) {
			allowed = 0;
			goto done;
		}
		if (allowed != 1)
			goto done;
		break;
	case 2: case 12:
		if (allowed != 1)
			goto done;
		break;
	case 3:
		if (wlanConnectionExists(WLAN_IF_AMP) != 0) {
			allowed = 0;
			goto done;
		}
		break;
	default:
		for (;;)
			;
	}
	romLibCheckContext(argument);
done:
	sysIrqRestore(posture);
	return allowed;
}

/* Gives the interface index back when the last connection of the BSS goes. */
int wlanConnectionIndexRelease(struct WlanConnection *connection)
{
	int result = 0;

	if (connection->interfaceType == WLAN_IF_AMP) {
		wlanConnectionCommandCheck(13, 1);
		wlanConnectionCommandPrepare(13, 0);
	}
	if (connection->flags2 & 4) {
		result = macTxNotifyAcquireBuffer(0, connection->interfaceAddress, connection->interfaceIndex, 0);
		if (result == 0) {
			wlanConnectionIndexMask &= ~(1 << connection->interfaceIndex);
			connection->interfaceIndex = 0xff;
		}
	}
	return result;
}

/* A connection that has an information block of its own takes a buffer for it; the others share the BSS's. */
int wlanConnectionInfoAttach(struct WlanConnection *connection)
{
	if (connection->flags2 & 4) {
		struct WlanConnectionRole *role = wlanConnectionInfoSlot(connection);

		if (role->infoBuffer == NULL) {
			OsaBuffer *buffer = osa_BufferAlloc(&wlanBlockPool2, 0, 0);

			if (buffer == NULL)
				return 1;
			role->infoBuffer = buffer;
			role->info = (struct WlanConnectionInfo *)((uint8_t *)buffer + buffer->dataOffset);
			uapSqramUpdateBuffer(connection);
		}
	} else {
		struct WlanConnection *first = wlanConnectionFind(connection->bssType, connection->bssNumber);
		struct WlanConnectionRole *role = wlanConnectionInfoSlot(connection);
		struct WlanConnectionRole *shared;

		(void)first;
		shared = wlanConnectionInfoSlot(first);
		role->infoBuffer = shared->infoBuffer;
		role->info = shared->info;
	}
	return 0;
}

/* Takes an interface index (a transmit notification slot) for each address of the connection. */
int wlanConnectionIndexAcquire(struct WlanConnection *connection)
{
	int result = 0;

	if (connection->flags2 & 4) {
		unsigned int index;
		int fresh = connection->interfaceType != WLAN_IF_STATION;

		result = 1;
		for (index = 0; index < 4 && result != 0; ++index) {
			if (!((1 << index) & wlanConnectionIndexMask)) {
				result = macTxNotifyAcquireBuffer(1, connection->interfaceAddress, index, fresh);
				if (result == 0) {
					connection->interfaceIndex = index;
					wlanConnectionIndexMask |= 1 << index;
				}
			}
		}
	}
	return result;
}

/* Returns a connection to its state before the host configured it. */
void wlanConnectionReset(struct WlanConnection *connection)
{
	__aeabi_memclr4(&connection->bssCapability, 60);
	connection->networkSsidLength = 0;
	__aeabi_memclr4(connection->networkSsid, 32);
	__aeabi_memclr4(connection->localAddress, 6);
	connection->flags2 &= ~0x10;
	connection->state = 0;
	connection->dataRate = 254;
	connection->groupRate = 254;
	macTxNotifyConfigureFrame(connection);
}

/* Forgets what was heard of the peer's security. */
void wlanConnectionClearSecurity(struct WlanConnection *connection)
{
	wlanSoftTimerStop(&connection->softTimer);
	macTxNotifyParseFrame(connection);
	__aeabi_memclr(connection->peerAddress, 6);
}

/* Sets the connection's transmit power limit (255: none) when the BSS has spectrum management. */
void wlanConnectionTxPowerUpdate(struct WlanConnection *connection, int enable)
{
	int32_t limit;

	if (!(connection->bssCapability[1] & 1))
		return;
	if (enable) {
		if (connection->interfaceType != WLAN_IF_IBSS)
			return;
		limit = macMgmtApplyEntry();
	} else {
		limit = 255;
	}
	wlanTxPowerSetConnectionLimit(&connection->txPowerLimit, limit, 1);
}

/* An IBSS connection goes. */
void wlanConnectionTeardownIbss(struct WlanConnection *connection)
{
	wlanConnectionTxPowerUpdate(connection, 0);
	wlanConnectionClearSecurity(connection);
	wlanConnectionReset(connection);
}

/*
 * A connection goes: what its type holds is given back, and for the last
 * connection of the BSS its interface index and slot as well. 1: it was the last.
 */
int wlanConnectionTeardownWds(struct WlanConnection *connection)
{
	struct WlanConnection *first = wlanConnectionFind(connection->bssType, connection->bssNumber);
	int last = wlanConnectionNextByBss(first, connection->bssType, connection->bssNumber) == NULL;

	switch (connection->interfaceType) {
	case 0:
		wlanConnectionTeardownType0(connection);
		goto finish;
	case 1:
		wlanConnectionTeardownIbss(connection);
		if (last)
			wlanConnectionIndexRelease(connection);
		return 0;
	case 2:
		wlanConnectionTeardownBss(connection);
		goto finish;
	case 3:
		if (connection->mode == 3)
			wlanConnectionTeardownStation(connection);
		else if (connection->mode == 2)
			wlanConnectionTeardownBss(connection);
finish:
		if (last)
			wlanConnectionIndexRelease(connection);
		return last;
	case 4:
		if (last) {
			connection->probeResponseBuffer->allocatorTag = 0;
			wlanConnectionSlotDetach(connection);
			wlanConnectionIndexRelease(connection);
		}
		connection->state = 0;
		if (wlanConnectionsIdle() != 0) {
			unsigned int role = 0;

			btLmpLcHandleState(0);
			if (wlanHasApRole())
				role = 2;
			wlanPsPhaseResume(wlanConnectionByRole(role));
		}
		return last;
	default:
		return last;
	}
}

/* Sets up an access point connection: the info block's back pointers, its handlers, its rate and association context. */
void wlanConnectionSetupType3Info(struct WlanConnection *connection)
{
	struct WlanConnectionInfo *info;
	struct WlanUapContext *context;

	wlanConnectionInfoSlot(connection);
	info = wlanConnectionInfo(connection);
	if (connection->flags2 & 4) {
		info->psMachine.connection = connection;
		info->connection = connection;
	}
	connection->rxManagement = wlanApRxManagement;
	connection->rxControl = wlanApRxControl;
	connection->dataRate = info->unicastRate != 0 ? romRateIndex(info->unicastRate) : 254;
	if (info->uapContext == NULL) {
		context = uapSqramUpdateContext(0);
		info->uapContext = context;
		if (context == NULL)
			for (;;)
				;
		__aeabi_memclr4(&context->params, 84);
		context->params.mode = 3;
		context->params.options = 0;
		context->params.control = context->params.control >> 1 << 1;
	}
}

/* The connection becomes an access point's: its mode is 2, and the first of them is activated. */
void wlanConnectionSetupType2(struct WlanConnection *connection)
{
	struct WlanConnectionInfo *info;
	struct WlanUapContext *context;

	connection->mode = 2;
	wlanConnectionInfoSlot(connection);
	info = wlanConnectionInfo(connection);
	if (connection->flags2 & 4) {
		info->psMachine.connection = connection;
		info->connection = connection;
		if ((connection->flags2 & 4) && wlanRoleOtherOnly() == 0)
			wlanConnectionActivate(connection);
	}
	connection->rxManagement = wlanApRxManagement;
	connection->rxControl = wlanApRxControl;
	connection->dataRate = info->unicastRate != 0 ? romRateIndex(info->unicastRate) : 254;
	if (info->uapContext == NULL) {
		context = uapSqramUpdateContext(0);
		info->uapContext = context;
		if (context == NULL)
			for (;;)
				;
		__aeabi_memclr4(&context->params, 84);
		context->params.mode = 3;
		context->params.options = 0;
		context->params.control = context->params.control >> 1 << 1;
	}
}

/* A connection holds the DTIM: its group frames wait. */
void wlanConnectionDtimHold(struct WlanConnection *connection)
{
	UINT posture;

	wlanConnectionInfo(connection);
	posture = sysIrqDisable();
	++wlanConnectionDtimHolds;
	connection->dtimHold = 1;
	if (connection->groupRing == MAC_2E8_RING)
		MAC_2F4_RINGS = (MAC_2F4_RINGS & ~(1u << connection->groupRing)) | 0x100;
	sysIrqRestore(posture);
}

void wlanConnectionDtimRelease(struct WlanConnection *connection)
{
	UINT posture;

	wlanConnectionInfo(connection);
	posture = sysIrqDisable();
	connection->dtimHold = 0;
	--wlanConnectionDtimHolds;
	sysIrqRestore(posture);
}

/* The block a connection of the type keeps its link in (type 0: the local name's; type 3: the second timer's). */
struct WlanHtElements *wlanConnectionTypeBlock(struct WlanConnection *connection)
{
	if (connection != NULL) {
		if (connection->interfaceType == 0)
			return (struct WlanHtElements *)(void *)&connection->localName;
		if (connection->interfaceType == 3)
			return (struct WlanHtElements *)(void *)&connection->secondTimerHandle;
	}
	return NULL;
}

/* A station connection takes its handlers and the pointers to its SQRAM state. */
void wlanConnectionSetupType3(struct WlanConnection *connection)
{
	struct WlanHtElements *state;

	if (connection == NULL)
		return;
	if (connection->interfaceType == 3) {
		state = wlanConnectionTypeBlock(connection);
		if (state != NULL) {
			connection->owner = connection;
			macMgmtSetupContext(connection);
			wlanConnectionActivate(connection);
			connection->rxManagement = wlanStaRxManagement;
			connection->rxControl = wlanStaRxControl;
			state->htOperation = wlanHtOperationIe;
			state->htCoexistence = wlanHtCoexIe;
			state->overlapScan = wlanHtOverlapScanIe;
			wlanSubscribeApplyThresholds(connection);
		}
	}
}

/* The same for a connection that becomes a station's (mode 3). */
void wlanConnectionSetupType0(struct WlanConnection *connection)
{
	struct WlanHtElements *state = wlanConnectionTypeBlock(connection);

	if (state != NULL) {
		connection->mode = 3;
		connection->owner = connection;
		wlanConnectionActivate(connection);
		connection->rxManagement = wlanStaRxManagement;
		connection->rxControl = wlanStaRxControl;
		state->htOperation = wlanHtOperationIe;
		state->htCoexistence = wlanHtCoexIe;
		state->overlapScan = wlanHtOverlapScanIe;
		wlanSubscribeApplyThresholds(connection);
	}
}

/* A connection becomes an IBSS's (mode 4): it keeps the SQRAM state's pointers and merges with other networks. */
void wlanConnectionSetupType1(struct WlanConnection *connection)
{
	struct WlanHtElements *state = (struct WlanHtElements *)(void *)&connection->localName;

	connection->mode = 4;
	if (connection->flags2 & 4)
		wlanConnectionActivate(connection);
	state->htOperation = wlanHtOperationIe;
	state->htCoexistence = wlanHtCoexIe;
	state->overlapScan = wlanHtOverlapScanIe;
	connection->stationFlags = (connection->stationFlags & 0xffffff3c) | 1;
	wlanSubscribeApplyThresholds(connection);
}

int wlanConnectionIdle(struct WlanConnection *connection)
{
	return wlanTxInFlightHasConnection(connection) == 0;
}

/* The idle hook: connections that were released are freed once the transmit side is done with them. */
int wlanConnectionFreeListClean(void)
{
	struct WlanConnection *previous = NULL;
	struct WlanConnection *connection;
	UINT posture;

	if (wlanConnectionFreeList == NULL)
		return 0;
	posture = sysIrqDisable();
	connection = wlanConnectionFreeList;
	while (connection != NULL) {
		if (wlanConnectionIdle(connection)) {
			struct WlanConnection *freed = connection;

			if (connection == wlanConnectionFreeList)
				wlanConnectionFreeList = wlanConnectionFreeList->next;
			else
				previous->next = connection->next;
			connection = connection->next;
			osa_BufferFree((OsaBuffer *)freed - 1);
		} else {
			previous = connection;
			connection = connection->next;
		}
	}
	sysIrqRestore(posture);
	return 0;
}

/* The connections' state, their buffer pools and the idle hook that frees released connections. */
int wlanConnectionsInit(void)
{
	unsigned int slot;

	wlanChannelInUse[0] = 0;
	wlanConnectionSlotMask = 0;
	wlanConnectionIndexMask = 0;
	wlanConnectionSlotsFree = 0;
	for (slot = 0; slot < WLAN_CONNECTION_SLOTS; slot = (uint8_t)(slot + 1))
		wlanConnectionSlots[slot] = NULL;
	if (osa_BlockPoolCreate(&wlanConnectionPool, &wlanConnectionPoolConfig, wlanConnectionPoolControl,
				wlanConnectionPoolMemory, osaPoolEntryCreate) != 0)
		for (;;)
			;
	if (osa_BlockPoolCreate(&wlanBlockPool0, &wlanBlockPool0Config, wlanBlockPool0Control,
				wlanBlockPool0Memory, osaPoolEntryCreate) != 0)
		for (;;)
			;
	if (osa_BlockPoolCreate(&wlanBlockPool1, &wlanBlockPool1Config, wlanBlockPool1Control,
				wlanBlockPool1Memory, osaPoolEntryCreate) != 0)
		for (;;)
			;
	if (osa_BlockPoolCreate(&wlanBlockPool2, &wlanBlockPool2Config, wlanBlockPool2Control,
				wlanBlockPool2Memory, osaPoolEntryCreate) != 0)
		for (;;)
			;
	wlanConnectionIdleHook.function = wlanConnectionFreeListClean;
	osaAddIdleHook(&wlanConnectionIdleHook);
	return 0;
}

/* The connections of the BSS, this one included, counting the one that is not set up yet. */
int wlanConnectionBssCount(struct WlanConnection *connection)
{
	int count = 0;

	if (connection->peerAddress[0] != 0 || connection->peerAddress[1] != 0 ||
	    connection->peerAddress[2] != 0 || connection->peerAddress[3] != 0 ||
	    connection->peerAddress[4] != 0 || connection->peerAddress[5] != 0)
		count = 1;
	while ((connection = wlanConnectionNextByBss(connection, connection->bssType, connection->bssNumber)) != NULL)
		++count;
	return count;
}

/*
 * Makes a connection of the interface type for a BSS: it comes from the
 * connection pool (the BSS's access point role may take a transmit buffer
 * instead), is cleared and set up for its type, and is put at the end of the
 * list. NULL: nothing was free, or no index or buffer could be had.
 */
struct WlanConnection *wlanConnectionConfigure(int interfaceType, int bssType, int bssNumber,
					       const uint8_t *local, const uint8_t *peer,
					       int channel, void *driver)
{
	struct WlanConnection *first = wlanConnectionFind(bssType, bssNumber);
	OsaBuffer *buffer = osa_BufferAlloc(&wlanConnectionPool, 0, 0);
	struct WlanConnection *connection;
	struct WlanConnection **tail;
	UINT posture;

	if (buffer == NULL) {
		if (interfaceType != 5)
			return NULL;
		if (first == NULL)
			return NULL;
		if ((unsigned int)wlanConnectionBssCount(first) >= 17)
			return NULL;
		buffer = wlanTxBufferAlloc(&wlanBlockPool28, 0, 0, 3);
		if (buffer == NULL)
			return NULL;
	}
	connection = (struct WlanConnection *)((uint8_t *)buffer + buffer->dataOffset);
	__aeabi_memclr4(connection, 944);
	connection->interfaceType = interfaceType;
	connection->bssType = bssType;
	connection->bssNumber = bssNumber;
	connection->device = driver;
	connection->channel = channel;
	connection->flags2 = (connection->flags2 & ~4) | (4 * (first == NULL));
	connection->beaconRssiLast = -98;
	connection->noiseLast = -98;
	connection->txPowerLimit = 255;
	if (interfaceType != 5) {
		if ((interfaceType == 2 || interfaceType == 3) && wlanConnectionInfoAttach(connection) != 0)
			goto fail;
		if (first != NULL)
			__aeabi_memcpy4(connection->interfaceAddress, first->interfaceAddress, 6);
		else if (connection->interfaceType != 4)
			__aeabi_memcpy4(connection->interfaceAddress, wlanDeviceAddress->address, 6);
		else
			__aeabi_memcpy(connection->interfaceAddress, local, 6);
		if (wlanConnectionIndexAcquire(connection) != 0) {
			if (connection->interfaceType == 2 || connection->interfaceType == 3)
				wlanConnectionTeardownBss(connection);
			goto fail;
		}
		if (peer != NULL)
			__aeabi_memcpy(connection->peerAddress, peer, 6);
		connection->radioParams = wlanRadioConfig->params;
		__aeabi_memclr(wlanRadioConfig->params, 15);
		connection->subscribeTriggered = 0;
		connection->lastSequence = 0xffff;
		connection->lastFragment = 0;
		connection->state = 0;
		connection->dataRate = 254;
		connection->groupRate = 254;
		connection->flags2 &= ~0x10;
		connection->keyBlocks[1] = NULL;
		connection->reserved664 = 0;
		connection->reserved668 = 0;
		connection->keyBlocks[0] = NULL;
		__aeabi_memclr4(&connection->bssCapability, 60);
		__aeabi_memclr4(connection->localAddress, 6);
		__aeabi_memclr4(connection->networkSsid, 32);
		connection->networkSsidLength = 0;
		connection->dtimPeriod = 0;
		connection->beaconPeriod = 0;
		if (local != NULL)
			__aeabi_memcpy(connection->localAddress, local, 6);
		connection->flags &= 0x3f;
		wlanSignalInit(connection);
		wlanRateChannelWidthSet(connection, 1, wlanHtChannelWidth(connection));
	}
	connection->rtsThreshold[0] = wlanDeviceAddress->rtsThreshold[0];
	connection->rtsThreshold[1] = wlanDeviceAddress->rtsThreshold[1];
	connection->fragThreshold[0] = wlanDeviceAddress->fragThreshold[0];
	connection->fragThreshold[1] = wlanDeviceAddress->fragThreshold[1];
	connection->shortRetryLimit = wlanDeviceAddress->shortRetryLimit;
	connection->longRetryLimit = wlanDeviceAddress->longRetryLimit;
	connection->beaconPeriod = 100;
	connection->dtimPeriod = 0;
	switch (interfaceType) {
	case 0:
		connection->owner = connection;
		macMgmtSetupContext(connection);
		wlanConnectionSetupType0(connection);
		break;
	case 1:
		wlanConnectionSetupType1(connection);
		break;
	case 2:
		wlanConnectionSetupType2(connection);
		break;
	case 3:
		((struct WlanHtElements *)(void *)&connection->localName)->htCoexistence = wlanApHtCoexIe;
		wlanConnectionSetupType3(connection);
		wlanConnectionSetupType3Info(connection);
		connection->rxManagement = wlanStaRxManagement;
		connection->rxControl = wlanStaRxControl;
		break;
	case 4: {
		struct WlanHtElements *state;

		if (wlanConnectionSlotAttach(connection) != 0) {
			wlanConnectionIndexRelease(connection);
			goto fail;
		}
		connection->probeResponseBuffer->allocatorTag = 1;
		connection->state = 2;
		connection->owner = connection;
		macMgmtSetupContext(connection);
		connection->mode = 4;
		wlanConnectionActivate(connection);
		connection->rxManagement = wlanAmpRxManagement;
		connection->flags |= 4;
		wlanConnectionSetActiveFlag(connection, 1);
		state = (struct WlanHtElements *)(void *)&connection->rsnElement[47];
		state->htOperation = wlanHtOperationIe;
		state->htCoexistence = wlanHtCoexIe;
		state->overlapScan = wlanHtOverlapScanIe;
		connection->transmitBusy = 0;
		ampPalCopySupportedRates(connection);
		break;
	}
	default:
		break;
	}
	posture = sysIrqDisable();
	tail = &wlanConnectionList;
	if (wlanConnectionList != NULL) {
		struct WlanConnection *last = wlanConnectionList;

		while (last->next != NULL)
			last = last->next;
		tail = &last->next;
	}
	*tail = connection;
	sysIrqRestore(posture);
	return connection;
fail:
	osa_BufferFree(buffer);
	return NULL;
}

/*
 * A connection takes another interface type (or, for a station, another
 * mode) and starts over: the radio parameters, the counters, the state and
 * the signal measurements are reset, the addresses replaced.
 */
int wlanConnectionReconfigure(struct WlanConnection *connection, int interfaceType,
			      const uint8_t *local, const uint8_t *peer)
{
	int mode = 0;

	if (connection->interfaceType == 3)
		mode = interfaceType;
	else
		connection->interfaceType = interfaceType;
	if (peer != NULL)
		__aeabi_memcpy(connection->peerAddress, peer, 6);
	connection->radioParams = wlanRadioConfig->params;
	__aeabi_memclr(wlanRadioConfig->params, 15);
	connection->subscribeTriggered = 0;
	connection->lastSequence = 0xffff;
	connection->lastFragment = 0;
	connection->state = 0;
	connection->dataRate = 254;
	connection->groupRate = 254;
	connection->flags2 &= ~0x10;
	__aeabi_memclr4(&connection->bssCapability, 60);
	if (local != NULL)
		__aeabi_memcpy(connection->localAddress, local, 6);
	wlanSignalInit(connection);
	wlanRateChannelWidthSet(connection, 1, wlanHtChannelWidth(connection));
	switch (connection->interfaceType) {
	case 0:
		wlanConnectionSetupType0(connection);
		break;
	case 1:
		wlanConnectionSetupType1(connection);
		break;
	case 3:
		if (mode == 0) {
			struct WlanHtElements *state = (struct WlanHtElements *)(void *)&connection->secondTimerHandle;

			wlanConnectionActivate(connection);
			state->htOperation = wlanHtOperationIe;
			state->htCoexistence = wlanHtCoexIe;
			state->overlapScan = wlanHtOverlapScanIe;
			connection->rxManagement = wlanStaRxManagement;
			connection->rxControl = wlanStaRxControl;
			wlanSubscribeApplyThresholds(connection);
		} else if (mode == 2) {
			wlanConnectionSetupType3Info(connection);
		}
		break;
	default:
		break;
	}
	return 0;
}

/* The access point (a station's connection in mode 2) is copied into a new connection. */
struct WlanConnection *wlanConnectionDuplicateAp(const uint8_t *peer, const struct WlanConnection *source)
{
	struct WlanConnection *copy = wlanConnectionConfigure(source->interfaceType, source->bssType,
							     source->bssNumber, NULL, NULL, 0, source->device);

	__aeabi_memcpy4(copy->networkSsid, source->networkSsid, 32);
	copy->networkSsidLength = source->networkSsidLength;
	copy->dtimPeriod = source->dtimPeriod;
	copy->beaconPeriod = source->beaconPeriod;
	copy->mode = source->mode;
	copy->localName.length = source->localName.length;
	copy->rxManagement = wlanApRxManagement;
	copy->rxControl = wlanApRxControl;
	return copy;
}

/* The same for a connection of interface type 2. */
struct WlanConnection *wlanConnectionDuplicateType2(const uint8_t *peer, const struct WlanConnection *source)
{
	struct WlanConnection *copy = wlanConnectionConfigure(source->interfaceType, source->bssType,
							     source->bssNumber, NULL, NULL, 0, source->device);

	__aeabi_memcpy4(copy->networkSsid, source->networkSsid, 32);
	copy->mode = source->mode;
	copy->networkSsidLength = source->networkSsidLength;
	copy->dtimPeriod = source->dtimPeriod;
	copy->beaconPeriod = source->beaconPeriod;
	return copy;
}

#define COPY_BITS(dst, src, mask)	((dst) = ((dst) & ~(mask)) | ((src) & (mask)))

/*
 * An IBSS's connection is copied into a connection of interface type 5
 * (one more member of the network) that then takes its interface type, state
 * machine, addresses, rates and flags; the peer is the one of the copy.
 */
void wlanConnectionDuplicateIbss(const uint8_t *peer, struct WlanConnection *source)
{
	struct WlanConnection *copy;

	if (source->interfaceType == 1 && (source->stationFlags & 0x80))
		return;
	copy = wlanConnectionConfigure(5, source->bssType, source->bssNumber, NULL, NULL, 0, source->device);
	if (copy == NULL)
		return;
	copy->mode = source->mode;
	copy->interfaceType = source->interfaceType;
	macMgmtFilterState(&copy->machine);
	copy->owner = copy;
	copy->smState = copy->smStates + 36;
	__aeabi_memcpy4(&copy->bssCapability, &source->bssCapability, 60);
	__aeabi_memcpy4(copy->localAddress, source->localAddress, 6);
	__aeabi_memcpy4(copy->networkSsid, source->networkSsid, 32);
	copy->networkSsidLength = source->networkSsidLength;
	copy->dtimPeriod = source->dtimPeriod;
	copy->beaconPeriod = source->beaconPeriod;
	copy->rtsThreshold[0] = source->rtsThreshold[0];
	copy->rtsThreshold[1] = source->rtsThreshold[1];
	copy->fragThreshold[0] = source->fragThreshold[0];
	copy->fragThreshold[1] = source->fragThreshold[1];
	copy->shortRetryLimit = source->shortRetryLimit;
	copy->longRetryLimit = source->longRetryLimit;
	__aeabi_memcpy(copy->peerAddress, peer, 6);
	__aeabi_memcpy4(copy->interfaceAddress, wlanDeviceAddress->address, 6);
	__aeabi_memclr4(&copy->peerLink, sizeof(copy->peerLink));
	copy->rxManagement = source->rxManagement;
	COPY_BITS(copy->txOptions, source->txOptions, 0x10);
	COPY_BITS(copy->hostFlags, source->hostFlags, 0x08);
	COPY_BITS(copy->hostFlags, source->hostFlags, 0x10);
	copy->beaconBuffer = source->beaconBuffer;
	copy->probeResponseBuffer = source->probeResponseBuffer;
	copy->dataRate = source->dataRate;
	copy->groupRate = source->groupRate;
	COPY_BITS(copy->flags, source->flags, 0x40);
	COPY_BITS(copy->flags, source->flags, 0x80);
	COPY_BITS(copy->flags2, source->flags2, 0x01);
	COPY_BITS(copy->flags, source->flags, 0x04);
	copy->stationFlags = (copy->stationFlags & ~0x41u) | (source->stationFlags & 1);
	if (!(source->flags & 4)) {
		copy->reserved664 = source->reserved664;
		copy->reserved668 = source->reserved668;
		copy->keyBlocks[0] = source->keyBlocks[0];
		copy->keyBlocks[1] = source->keyBlocks[1];
	}
}

/* A connection is copied by its interface type. */
void wlanConnectionDuplicate(const uint8_t *peer, struct WlanConnection *source)
{
	if (source->interfaceType == 1)
		wlanConnectionDuplicateIbss(peer, source);
	else if (source->interfaceType == 2)
		wlanConnectionDuplicateType2(peer, source);
	else if (source->interfaceType == 3 && source->mode == 2)
		wlanConnectionDuplicateAp(peer, source);
}

/* The IBSS connection whose SSID is the one of the SSID element (ID, length, SSID). */
struct WlanConnection *wlanConnectionFindIbssBySsid(const uint8_t *ssidElement)
{
	struct WlanConnection *connection;
	UINT posture;

	if (ssidElement[1] == 0)
		return NULL;
	posture = sysIrqDisable();
	for (connection = wlanConnectionList; connection != NULL; connection = connection->next) {
		if (connection->interfaceType == 1 && ssidElement[1] == connection->networkSsidLength &&
		    memcmp(connection->networkSsid, ssidElement + 2, ssidElement[1]) == 0) {
			sysIrqRestore(posture);
			return connection;
		}
	}
	sysIrqRestore(posture);
	return NULL;
}

/* 1: an IBSS (one we did not start) has its key handshake state in the second stage with the first bit set. */
int wlanIbssSecurityActive(void)
{
	struct WlanConnection *connection;
	UINT posture = sysIrqDisable();

	for (connection = wlanConnectionList; connection != NULL; connection = connection->next) {
		if (connection->interfaceType == 1 && !(connection->flags & 4)) {
			uint8_t *block = connection->keyBlocks[0];

			if (block != NULL && block[28 + 7] == 2 && (block[28 + 10] & 1)) {
				sysIrqRestore(posture);
				return 1;
			}
		}
	}
	sysIrqRestore(posture);
	return 0;
}

/*
 * A Block Ack session was added or removed (or a role changed): the radio's
 * mode follows whether any connection carries host data and whether a Block
 * Ack session is open.
 */
void wlanBaTablesChangedHandler(void)
{
	int data = wlanAnyConnectionHostFlag8() != 0 || wlanAnyConnectionFlag4() != 0 ||
		   wlanAnyConnectionFlag10() != 0 || wlanIbssSecurityActive() != 0;
	int active = wlanBaAnyActive(wlanTxBaSessions, wlanRxBaSessions);

	if (data) {
		if (active != 0)
			clockHostSelectPreset3();
		else
			clockHostSelectPreset1();
	} else if (active != 0) {
		clockHostSelectPreset2();
	} else {
		clockHostSelectPreset0();
	}
}

/* 1: a connection may transmit now. */
int wlanAnyConnectionTxAllowed(void)
{
	struct WlanConnection *connection;
	UINT posture = sysIrqDisable();

	for (connection = wlanConnectionList; connection != NULL; connection = connection->next) {
		if (macMgmtSelectContext(connection) == 1) {
			sysIrqRestore(posture);
			return 1;
		}
	}
	sysIrqRestore(posture);
	return 0;
}

/*
 * The BSS address registers are programmed for the stage a join or a
 * start reaches (kind): from the descriptor, or from the peer of the
 * station connection. 1: allowed (no access point or station BSS is
 * already up).
 */
int wlanBssSelectAddress(unsigned int kind, const void *descriptor)
{
	int allowed = 1;
	UINT posture = sysIrqDisable();

	switch (kind) {
	case 0:
	case 1:
	case 7:
	case 8:
	case 9:
	case 10:
		if (wlanConnectionAnyStarted(3) == 1)
			allowed = 0;
		if (wlanConnectionAnyStarted(2) == 1) {
			allowed = 0;
			break;
		}
		/* fall through */
	case 2:
		if (allowed == 1)
			wlanMacAddressSet(descriptor);
		break;
	case 3: {
		struct WlanConnection *connection = p2pDirectProcessEntry(3);

		if (connection != NULL || (connection = wlanConnectionFirst(0)) != NULL) {
			if (connection->flags2 & 0x10)
				descriptor = connection->peerAddress;
		}
		wlanMacAddressSet(descriptor);
		break;
	}
	default:
		for (;;)
			;
	}
	sysIrqRestore(posture);
	return allowed;
}

/* The same check for the stages that only need to know whether the BSS may come up. */
int wlanBssAllowStart(unsigned int stage)
{
	int allowed = 1;
	UINT posture = sysIrqDisable();

	if (stage != 0 && stage != 3) {
		for (;;)
			;
	}
	if (wlanConnectionAnyStarted(3) == 1)
		allowed = 0;
	if (wlanConnectionAnyStarted(2) == 1)
		allowed = 0;
	else if (allowed == 1)
		romLibNotifyState();
	sysIrqRestore(posture);
	return allowed;
}

/*
 * A BSS timing record (the beacon time and the next target time) is
 * brought up to date for the stage that reached it.
 */
int wlanBssRecordTiming(unsigned int kind, struct WlanBssTiming *timing)
{
	int allowed = 1;
	UINT posture = sysIrqDisable();

	switch (kind) {
	case 0:
		if (wlanConnectionCount != 0) {
			allowed = 0;
			goto done;
		} else {
			uint32_t interval = timing->interval;
			uint64_t next = ((uint64_t)timing->next[1] << 32) | timing->next[0];
			uint64_t now = romLibRecordState(timing->next[0]);

			if (next == 0 || next + interval < now || now < next) {
				uint64_t quotient = next / interval;

				next = romMul64x32(quotient + 1, interval);
				timing->next[1] = next >> 32;
				timing->next[0] = next;
			}
		}
		break;
	case 2: {
		struct WlanConnection *connection = p2pDirectProcessEntry(3);

		if (connection != NULL || (connection = wlanConnectionFirst(0)) != NULL) {
			if (connection->flags2 & 0x10) {
				uint64_t now = wlanSignalTimeNow();

				timing->timestamp[1] = now >> 32;
				timing->timestamp[0] = now;
			}
		}
		break;
	}
	case 3: {
		struct WlanConnection *connection = wlanConnectionFirst(0);

		if (connection != NULL && (connection->flags2 & 0x10)) {
			uint64_t now = wlanSignalTimeNow();
			uint64_t timer;

			timing->timestamp[1] = now >> 32;
			timing->timestamp[0] = now;
			timer = romGetTimerTimestamp();
			timing->next[1] = timer >> 32;
			timing->next[0] = timer;
		}
		break;
	}
	default:
		break;
	}
	romLibProcessContext(timing);
done:
	sysIrqRestore(posture);
	return allowed;
}

/* The peer's address and state of the link check are cleared and the connection's peer state gets the argument. */
int wlanConnectionUpdatePeer(struct WlanConnection *connection, int argument)
{
	wlanPsSetupPacket(&connection->peerLink, argument);
	return macTxNotifyApplyFrame(connection);
}

/* 1: the peer is not a group address and the connection's peers are QoS stations. */
int wlanPeerQosEnabled(struct WlanConnection *connection, const uint8_t *peer)
{
	if (peer[0] & 1)
		return 0;
	return (connection->flags2 >> 3) & 1;
}

/* The next sequence number of a TID towards the peer (advanced when asked). */
unsigned int wlanPeerNextSequence(struct WlanConnection *connection, unsigned int tid, int advance)
{
	unsigned int sequence = connection->txSequence[tid];

	if (advance == 1)
		connection->txSequence[tid] = (sequence + 1) & 0xfff;
	return sequence;
}

/* An IBSS is left: the connection goes (when it was not the first of its BSS) or is torn down or its security cleared. */
void wlanIbssLeave(struct WlanConnection *connection, int teardown)
{
	connection->stationFlags &= ~0x40u;
	if (!(connection->flags2 & 4)) {
		wlanConnectionRelease(connection);
		return;
	}
	if (teardown != 0) {
		wlanConnectionTeardownIbss(connection);
		return;
	}
	wlanConnectionTxPowerUpdate(connection, 1);
	wlanConnectionClearSecurity(connection);
}

void wlanIbssLeaveAll(int teardown)
{
	struct WlanConnection *connection;
	UINT posture = sysIrqDisable();

	for (connection = wlanConnectionList; connection != NULL;) {
		struct WlanConnection *next = connection->next;

		if (connection->interfaceType == 1)
			wlanIbssLeave(connection, teardown);
		connection = next;
	}
	sysIrqRestore(posture);
}

/*
 * The link check of the IBSS connections: a peer silent for 2 s counts a miss,
 * three misses leave the IBSS (and tell the host, unless it is the merge state).
 */
void wlanIbssCheckPeers(void)
{
	uint8_t none[8] = { 0 };
	struct WlanConnection *connection = wlanConnectionList;
	uint32_t now = romGetMicroseconds();

	while (connection != NULL) {
		if (connection->interfaceType == 1) {
			if (connection->stationFlags & 0x80) {
				if ((unsigned int)wlanTimeDelta(now, connection->peerCheckTime) >= 2000000) {
					if (++connection->peerCheckMisses < 3) {
						connection->peerCheckTime = now;
					} else {
						connection->peerCheckMisses = 0;
						wlanIbssLeave(connection, 0);
					}
				}
				return;
			}
			if (memcmp(connection->peerAddress, none, 6) != 0) {
				if ((unsigned int)wlanTimeDelta(now, connection->peerCheckTime) >= 2000000) {
					if (++connection->peerCheckMisses < 3) {
						connection->peerCheckTime = now;
					} else {
						connection->peerCheckMisses = 0;
						wlanHostEventSendAddress(connection, WLAN_HOST_EVENT_IBSS_PEER_SILENT,
									 connection->peerAddress);
						wlanIbssLeave(connection, 0);
					}
				}
			}
		}
		connection = connection->next;
	}
}

int wlanIbssResetPeerCheck(struct WlanConnection *connection)
{
	connection->peerCheckTime = romGetMicroseconds();
	connection->peerCheckMisses = 0;
	return 0;
}

/* The power-save frames queued on the connections of one BSS. */
int wlanBssQueuedFrames(const struct WlanConnection *bss)
{
	unsigned int bssNumber = bss->bssNumber;
	unsigned int bssType = bss->bssType;
	struct WlanConnection *connection;
	int total = 0;
	UINT posture = sysIrqDisable();

	for (connection = wlanConnectionList; connection != NULL; connection = connection->next) {
		if (connection->bssNumber == bssNumber && connection->bssType == bssType) {
			struct WlanConnectionRole *role = wlanConnectionInfoSlot(connection);

			if (role != NULL)
				total += role->psQueue.count;
		}
	}
	sysIrqRestore(posture);
	return total;
}

/* The BSS count over the access points and stations in mode 2. */
int wlanConnectionCountMode2(int stage)
{
	unsigned int types = 0x302;	/* interface types 2, 3 */
	int total = 0;

	if (stage == 2) {
		unsigned int i;

		for (i = 0; i < 2; i++) {
			struct WlanConnection *connection = wlanConnectionFirst((types >> (8 * i)) & 0xff);

			while (connection != NULL) {
				if (connection->mode == 2)
					total += wlanConnectionBssCount(connection);
				connection = wlanConnectionNextFlagged(connection, connection->interfaceType);
			}
		}
	}
	return total;
}

/* The lowest transmit power limit of the connections (255: none set). */
int wlanTxPowerLimitMin(void)
{
	struct WlanConnection *connection;
	int limit = 255;
	UINT posture = sysIrqDisable();

	for (connection = wlanConnectionList; connection != NULL; connection = connection->next) {
		if (connection->txPowerLimit < limit)
			limit = connection->txPowerLimit;
	}
	sysIrqRestore(posture);
	return limit;
}

/* The hardware address slot 4 takes (or gives back) the address: its bit in the index mask follows. */
int wlanConnectionIndexAssign(const uint8_t *address)
{
	int result = macTxNotifyAcquireBuffer(1, address, 4, 1);

	if (result == 0)
		wlanConnectionIndexMask |= 0x10;
	return result;
}

int wlanConnectionIndexUnassign(const uint8_t *address)
{
	int result = macTxNotifyAcquireBuffer(0, address, 4, 1);

	if (result == 0)
		wlanConnectionIndexMask &= ~0x10u;
	return result;
}

struct WlanConnection *wlanConnectionSlotGet(unsigned int index)
{
	return wlanConnectionSlots[index];
}

/* A connection of type 0 is given back: its link check stops and the supplicant state is cleared. */
int wlanConnectionTeardownType0(struct WlanConnection *connection)
{
	osaCancelTimer(connection->link.timer);
	__aeabi_memclr4(&connection->link, 24);
	return wlanSoftTimerStop(&connection->softTimer);
}

/* The same for a station connection in mode 3. */
int wlanConnectionTeardownStation(struct WlanConnection *connection)
{
	osaCancelTimer(connection->link.timer);
	__aeabi_memclr4(&connection->link, 24);
	return wlanSoftTimerStop(&connection->softTimer);
}
