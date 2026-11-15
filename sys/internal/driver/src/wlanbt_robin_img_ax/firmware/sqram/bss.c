/*
 * The slots of the BSSs the device runs at once, in SQRAM (0xc0007200 - 0xc00074e8): a slot's
 * release, the beacon times of the slots, the hooks around a change of the connections, the
 * EAPOL frames of the supplicant and authenticator, and the supplicant's start. The names are
 * the firmware's placeholders; the filename is inferred.
 */
#include <stddef.h>
#include "robin/conn.h"
#include "robin/esa.h"
#include "robin/join.h"
#include "robin/keyMgmtAp.h"
#include "robin/macentry.h"
#include "robin/mactx.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/sys.h"
#include "robin/txring.h"
#include "robin/uapbss.h"
#include "robin/wlan.h"
#include "robin/wlantask.h"

/* The beacon and the probe response transmit requests of each slot (wlanConnectionSlotAcquire()). */
uint8_t wlanBssBeaconRequests[WLAN_CONNECTION_SLOTS][48] __attribute__((section(".sqram1.bss")));		/* 0xc0031d64 */
uint8_t wlanBssProbeResponseRequests[WLAN_CONNECTION_SLOTS][48] __attribute__((section(".sqram1.bss")));	/* 0xc0031e24 */

/* The stage that starts the supplicant again after a reset (uapSqramParseState()). */
struct WlanInitStage esaInitStage __attribute__((section(".itcm.rw")));				/* 0x547d4 */

int romLibApplyContext(unsigned int count);							/* 0x03f3a94e */

/*
 * 0xc0007200: gives the connection's slot back. The slots above it move down one, with their
 * template requests, so that the slots in use stay the first ones.
 */
int uapSqramHandleState(struct WlanConnection *connection)
{
	UINT posture = sysIrqDisable();
	unsigned int slot;

	if (connection == NULL || (slot = connection->groupRing) == 0xff)
		for (;;)
			;
	if (connection->interfaceType != WLAN_IF_IBSS) {
		macTxNotifyClearBuffer(slot, connection->interfaceIndex, 0, connection->dtimPeriod,
				       connection->dtimPeriod);
		while (wlanConnectionCount > slot) {
			struct WlanConnection *next = wlanConnectionSlots[slot + 1];

			wlanConnectionSlots[slot] = next;
			next->groupRing = slot;
			__aeabi_memcpy(wlanBssBeaconRequests[slot], next->beaconRequest, 48);
			__aeabi_memcpy(wlanBssProbeResponseRequests[slot], next->probeResponseRequest, 48);
			next->beaconRequest = (struct WlanMacTxRequest *)wlanBssBeaconRequests[slot];
			next->probeResponseRequest = (struct WlanMacTxRequest *)wlanBssProbeResponseRequests[slot];
			slot = (uint8_t)(slot + 1);
		}
		if (wlanConnectionCount == 0)
			macTxNotifySetupBuffer();
	}
	wlanConnectionSlotMask &= ~(1 << slot);
	wlanConnectionSlots[slot] = NULL;
	sysIrqRestore(posture);
	return 0;
}

/*
 * 0xc00072ae: spreads the beacons of the slots over their common period, the greatest common
 * divisor of their beacon periods: slot n sends at n times the period over the slots (in
 * 1024 us), and counts its DTIM in common periods.
 */
void uapSqramProcessState(void)
{
	unsigned int slot, step, period;

	wlanBeaconPeriodGcd = 0;
	for (slot = 0; slot < WLAN_CONNECTION_SLOTS; slot = (uint8_t)(slot + 1)) {
		struct WlanConnection *connection = wlanConnectionSlots[slot];
		unsigned int a, b;

		if (connection == NULL)
			continue;
		a = connection->beaconPeriod;
		for (b = wlanBeaconPeriodGcd; b != 0; ) {
			unsigned int remainder = a % b;

			a = b;
			b = remainder;
		}
		wlanBeaconPeriodGcd = a;
	}
	period = wlanBeaconPeriodGcd;
	step = period / wlanConnectionCount;
	for (slot = 0; slot < WLAN_CONNECTION_SLOTS; slot = (uint8_t)(slot + 1)) {
		struct WlanConnection *connection = wlanConnectionSlots[slot];
		struct WlanMacTxRequest *beacon;
		uint8_t flags;

		if (connection == NULL)
			continue;
		connection->beaconsPerDtim = connection->beaconPeriod / period;
		connection->beaconsSinceDtim = 0;
		__aeabi_uwrite4((connection->groupRing * step) << 10, connection->beaconRequest->expiry);
		__aeabi_uwrite4(0, connection->beaconRequest->expiry + 4);
		__aeabi_uwrite4(__aeabi_uread4(connection->beaconRequest->expiry),
				connection->probeResponseRequest->expiry);
		__aeabi_uwrite4(__aeabi_uread4(connection->beaconRequest->expiry + 4),
				connection->probeResponseRequest->expiry + 4);
		beacon = connection->beaconRequest;
		flags = beacon->txFlags | MAC_TX_HAS_LIFETIME;
		beacon->txFlags = flags;
		connection->probeResponseRequest->txFlags = flags;
		macTxNotifyClearBuffer(connection->groupRing, connection->interfaceIndex, 1, connection->dtimPeriod,
				       connection->dtimPeriod);
	}
	romLibApplyContext(step);
	macTxNotifyConvertBuffer(wlanConnectionCount - 1);
}

/* 0xc000738e: before a slot's connection goes; the value its callers pass is not read. */
int uapSqramUpdateState(int value)
{
	macTxNotifyAdjustBuffer();
	return 0;
}

/* 0xc0007398: after the slots changed: new beacon times, and the next beacon. */
int uapSqramConfigureState(int value)
{
	uapSqramProcessState();
	macTxNotifySelectBuffer();
	return 0;
}

/*
 * 0xc00073a6: starts the connections again after a reset: a station or access point runs
 * uapBssDefaults(), any other gets the device's address back. Returns 1 when a
 * connection was not set up.
 */
int uapSqramCheckState(void)
{
	int pending = 0;
	UINT posture = sysIrqDisable();
	struct WlanConnection *connection;

	for (connection = wlanConnectionList; connection != NULL; connection = connection->next) {
		if (!(connection->flags2 & 4)) {
			pending = 1;
			continue;
		}
		if (connection->interfaceType == WLAN_IF_STATION || connection->interfaceType == WLAN_IF_AP) {
			uapBssDefaults(connection);
		} else {
			__aeabi_memcpy4(connection->interfaceAddress, wlanDeviceAddress->address, 6);
			macTxNotifyAcquireBuffer(2, connection->interfaceAddress, connection->interfaceIndex, 1);
		}
	}
	sysIrqRestore(posture);
	return pending;
}

/* The EAPOL header (802.1X): its packet type, and the code of an EAP packet's body. */
#define EAPOL_TYPE_EAP_PACKET	0
#define EAPOL_TYPE_KEY		3
#define EAP_CODE_REQUEST	1

/*
 * 0xc0007478: an EAPOL frame for the device. A key frame goes to the AMP key management, the
 * authenticator (access point) or the supplicant, and is taken (1); an EAP request to a
 * station or IBSS calls the join hook.
 */
int wlanDispatchSecurityFrame(OsaBuffer *buffer, const uint8_t *source, const uint8_t *destination)
{
	struct WlanConnection *connection = buffer->context;
	const uint8_t *eapol = (const uint8_t *)buffer + buffer->dataOffset;

	if (eapol[1] == EAPOL_TYPE_EAP_PACKET) {
		if ((connection->mode == 3 || connection->mode == 1) && eapol[4] == EAP_CODE_REQUEST)
			wlanJoinHook();
		return 0;
	}
	if (eapol[1] != EAPOL_TYPE_KEY)
		return 0;
	if (connection->interfaceType == WLAN_IF_AMP)
		btampProcessKeyMgmtData(buffer);
	else if (connection->mode == 2)
		AuthenticatorProcessEapolPacket(buffer);
	else
		ProcessKeyMgmtDataSta(buffer, (IEEEtypes_MacAddr_t *)source, (IEEEtypes_MacAddr_t *)destination);
	return 1;
}

/* 0xc00074c6: the PMK cache and the supplicant from the start. */
__attribute__((noinline)) int uapSqramDispatchState(void)
{
	pmkCacheInit();
	supplicantInit();
	return 0;
}

/* 0xc00074d4: starts the supplicant, and again after every reset. */
int uapSqramParseState(void)
{
	uapSqramDispatchState();
	esaInitStage.run = uapSqramDispatchState;
	return wlanInitStageRegister(&esaInitStage);
}
