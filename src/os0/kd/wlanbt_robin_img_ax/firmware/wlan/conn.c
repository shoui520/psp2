/*
 * Connection control block list: lookups and release. The filename is
 * inferred; no source filename survives.
 */
#include "robin/conn.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/sys.h"

struct WlanConnection *wlanConnectionList __attribute__((section(".sqram1.rw")));
struct WlanConnection *wlanConnectionFreeList __attribute__((section(".sqram1.rw")));
struct WlanConnection *wlanConnectionSession __attribute__((section(".sqram1.rw")));
struct WlanConnection *wlanConnectionPrimary __attribute__((section(".sqram1.rw")));
uint8_t wlanConnectionCount __attribute__((section(".dtcm.bss")));
uint8_t wlanChannelInUse[1] __attribute__((section(".dtcm.bss")));

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

int wlanConnectionDecrement(void)
{
	return --wlanConnectionCount;
}

/* Address of the information pointer of a station or access point connection. */
struct WlanConnectionInfo **wlanConnectionInfoSlot(struct WlanConnection *connection)
{
	if (connection == NULL)
		return NULL;
	if (connection->interfaceType == WLAN_IF_STATION)
		return &connection->stationInfo;
	if (connection->interfaceType == WLAN_IF_AP)
		return &connection->apInfo;
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
