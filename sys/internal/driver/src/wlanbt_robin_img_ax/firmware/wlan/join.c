/*
 * Joining a BSS as a station (0x11a60..0x12900): the host's associate
 * command takes channel time from the slot scheduler, authenticates
 * (open, shared key, network EAP, or open then shared key), associates or
 * reassociates, and leaves the result and the AP's response for the host.
 * The filename is inferred; no source filename survives.
 */
#include <stddef.h>
#include <stdint.h>
#include "robin/calibration.h"
#include "robin/stapssm.h"
#include "robin/coex.h"
#include "robin/conn.h"
#include "robin/hostps.h"
#include "robin/esa.h"
#include "robin/ie.h"
#include "robin/join.h"
#include "robin/joinframe.h"
#include "robin/mactx.h"
#include "robin/mgmt.h"
#include "robin/protection.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/signal.h"
#include "robin/slot.h"
#include "robin/stream.h"
#include "robin/sys.h"
#include "robin/txring.h"
#include "robin/wlan.h"
#include "robin/wlantask.h"
#include "robin/htba.h"
#include "robin/stamachine.h"

#define LE16(bytes)	((uint16_t)((bytes)[0] | ((bytes)[1] << 8)))

/* WlanConnection.state */
#define WLAN_CONN_STATE_JOINING		3
#define WLAN_CONN_STATE_ASSOCIATED	4

/* Capability information, low byte: Privacy; high byte: Spectrum Management,
   Short Slot Time, Radio Measurement. */
#define WLAN_CAPABILITY0_PRIVACY	0x10
#define WLAN_CAPABILITY1_SPECTRUM_MGMT	0x01
#define WLAN_CAPABILITY1_SHORT_SLOT	0x04
#define WLAN_CAPABILITY1_RADIO_MEASURE	0x10

/* The connection event wlanPortOpenNotify() reports when the port opens. */
#define WLAN_CONN_EVENT_PORT_OPEN	64

/* The host command response posted when a join ends without a result. */
#define HostCmd_RET_802_11_ASSOCIATE	0x8012

/* HT Capabilities Information, low byte: 20/40 MHz supported. */
#define WLAN_HT_CAP_40MHZ		0x02

/* The 802.11 frame types and subtypes the join handles. */
#define WLAN_FC_TYPE(fc)		(((fc) >> 2) & 3)
#define WLAN_FC_SUBTYPE(fc)		((fc) >> 4)
#define WLAN_SUBTYPE_ASSOC_REQUEST	0
#define WLAN_SUBTYPE_ASSOC_RESPONSE	1
#define WLAN_SUBTYPE_REASSOC_REQUEST	2
#define WLAN_SUBTYPE_REASSOC_RESPONSE	3
#define WLAN_SUBTYPE_DISASSOC		10
#define WLAN_SUBTYPE_AUTH		11
#define WLAN_SUBTYPE_DEAUTH		12

/* The fixed fields ahead of an association response's elements. */
#define WLAN_ASSOC_RESPONSE_FIXED	6

/* The MAC's filters while joining and otherwise. */
#define WLAN_JOIN_FILTER_SAME_CHANNEL	40
#define WLAN_JOIN_FILTER_OTHER_CHANNEL	46
#define WLAN_JOIN_FILTER_IDLE		47
#define WLAN_JOIN_FILTER_IDLE_ENTRY	41

/* mdevIoctl() on the SDIO device: the MAC is busy joining. */
#define MDEV_IOCTL_SDIO_JOIN_BUSY	0x48

/* What romLibUpdateContext() fills for cbProcRecordBuffer(). */
struct WlanTsfRecord {
	uint64_t timestamp;		/* the BSS's TSF */
	uint8_t reserved8[16];
	uint32_t beaconInterval;	/* microseconds */
	uint8_t reserved28[20];
};

_Static_assert(sizeof(struct WlanTsfRecord) == 48, "TSF record");

struct WlanJoin wlanJoin __attribute__((section(".itcm.bss")));

/* Not converted yet; provisional names. */
extern IEEEtypes_MacAddr_t *wlanDeviceAddress;				/* 0x54a7c */
extern uint8_t wlanJoinWmmIe[9];					/* DTCM 0x04008fc0 */
extern uint8_t wlanBssWmmIe[9];						/* DTCM 0x04008fc9 */
void p2pDirectVerifyEntry(struct WlanConnection *connection);		/* 0x41bf2 */
void wpaSuppScheduleDescriptor(struct WlanConnection *connection);	/* 0x18c0a */
void cbProcFilterPacket(struct WlanConnection *connection, const WlanIePointers *ies,
			uint32_t sameBss, int unused);			/* 0x1e8ba */
void macMgmtClearContext(struct WlanConnection *connection);		/* 0x35dc0 */
int ampPalOwnsConnection(struct WlanConnection *connection);		/* 0x236f8 */
int cbProcSetupEntry(void);						/* 0x1cb00 */
int cbProcVerifyEntry(void);						/* 0x1cb1a */
void macMgmtFilterPacket(int filter, int kind);				/* 0x3a5aa */
void cbProcClearPacket(struct WlanConnection *connection);		/* 0x1e79a */
void wlanPsPrepareTable(struct WlanConnection *connection);		/* 0x172f0 */

/* 0x11a60: authenticate with the algorithm the host asked for. */
int wlanJoinSendAuth(int shared)
{
	struct WlanJoinAuthRequest request = { .algorithm = 0 };

	__aeabi_memcpy(request.peer, wlanJoin.bss.bssid, sizeof(request.peer));
	switch (wlanJoin.authType) {
	case WLAN_JOIN_AUTH_OPEN:
		request.algorithm = WLAN_AUTH_ALG_OPEN;
		break;
	case WLAN_JOIN_AUTH_SHARED:
		request.algorithm = WLAN_AUTH_ALG_SHARED;
		break;
	case WLAN_JOIN_AUTH_NETWORK_EAP:
		request.algorithm = WLAN_AUTH_ALG_NETWORK_EAP;
		break;
	default:
		request.algorithm = shared ? WLAN_AUTH_ALG_SHARED : WLAN_AUTH_ALG_OPEN;
		break;
	}
	wlanJoin.state = WLAN_JOIN_AUTH_SENT;
	wlanJoin.authAlgorithm = request.algorithm;
	return macTxNotifyFilterFrame(wlanJoin.connection, &request);
}

/* 0x11aba: associate, or reassociate when the connection was associated. */
int wlanJoinSendAssoc(void)
{
	struct WlanJoinAssocRequest request;
	uint8_t *ies;
	unsigned int i;
	int result;

	__aeabi_memclr4(&request, sizeof(request));
	__aeabi_memcpy(request.peer, wlanJoin.bss.bssid, sizeof(request.peer));
	__aeabi_memcpy(request.ssid, wlanJoin.bss.ssid, wlanJoin.bss.ssidLength);
	request.capability = wlanJoin.capability[0] | (wlanJoin.capability[1] << 8);
	request.listenInterval = wlanAssocListenInterval;
	for (i = 0; i < wlanJoin.rateCount; i = (uint8_t)(i + 1))
		request.rates[i] = wlanJoin.rates[i] & 0x7f;
	wlanJoin.state = WLAN_JOIN_ASSOC_SENT;
	wlanJoin.connection->flags2 = (wlanJoin.connection->flags2 & ~WLAN_CONN2_ASSOCIATED) |
				      ((wlanJoin.reassociation << 4) & 0x1f);
	request.iesLength = wlanIeBufferGet(&wlanJoin.ieBuffer, &ies);
	request.ies = ies;
	if ((wlanJoin.connection->flags2 & WLAN_CONN2_ASSOCIATED) || wlanJoin.reassociationRequest)
		result = macTxNotifySetupFrame(wlanJoin.connection, &request, &wlanJoin.previous);
	else
		result = macTxNotifyReleaseFrame(wlanJoin.connection, &request);
	if (result == 0)
		wlanJoin.connection->flags2 |= WLAN_CONN2_ASSOCIATED;
	return result;
}

/* 0x11b78 */
void wlanJoinClosePort(void)
{
	wlanJoin.portOpen = 0;
}

/* 0x11b82 */
uint32_t wlanJoinPortIsOpen(void)
{
	return wlanJoin.portOpen;
}

/* 0x11b8a: the key handshake (or an open network) lets data through. */
void keyMgmtControlledPortOpen(void)
{
	struct WlanConnection *connection;

	supplicantStopSessionTimer();
	if (wlanJoin.portOpen) {
		wlanPortOpenNotify(wlanJoin.connection, WLAN_CONN_EVENT_PORT_OPEN);
		return;
	}
	wlanJoin.portOpen = 1;
	wlanJoinSetBusy(wlanJoin.connection, 0);
	wlanPortOpenNotify(wlanJoin.connection, WLAN_CONN_EVENT_PORT_OPEN);
	p2pDirectVerifyEntry(wlanJoin.connection);
	if (wlanJoin.newBss && wlanJoin.portTimeout)
		macTxNotifyBuildHeader(wlanJoin.connection, wlanJoin.previous.bssid, 3, NULL,
				       1000 * wlanJoin.portTimeout);
	if (wlanSessionId && wlanRoleOtherOnly()) {
		connection = wlanJoin.connection;
		if (connection->interfaceType != WLAN_IF_AMP)
			wlanPsStartHostModeTimer(connection);
	}
}

/* 0x11c08: take the BSS's address and SSID for the connection. */
void wlanJoinSetBss(const struct WlanJoinBss *bss)
{
	struct WlanConnection *connection = wlanJoin.connection;

	__aeabi_memcpy(connection->localAddress, bss->bssid, sizeof(connection->localAddress));
	__aeabi_memclr4(connection->networkSsid, sizeof(connection->networkSsid));
	__aeabi_memcpy(connection->networkSsid, bss->ssid, bss->ssidLength);
	connection->networkSsidLength = bss->ssidLength;
	cbProcSelectBuffer(0, bss->bssid);
	wlanTxSetSharedRadio(1);
	wlanMacAddressApply();
}

/* 0x11c56: end the join with a result (0: associated). */
void wlanJoinFinish(uint8_t result)
{
	struct WlanConnection *connection;
	struct WlanTsfRecord record;
	uint64_t now;
	uint8_t *response;
	unsigned int length;
	unsigned int flags;
	UINT posture;

	wlanSlotClearWindow(wlanJoin.slot);
	wlanJoin.state = WLAN_JOIN_FINISHED;
	wlanJoin.result = result;
	macTxSetPowerSave(wlanJoin.savedPowerSave);
	now = romGetTimerTimestamp();
	if (wlanJoin.result == WLAN_JOIN_OK || wlanJoin.result == WLAN_JOIN_REFUSED) {
		length = wlanIeBufferGet(&wlanJoin.ieBuffer, &response);
		wlanParseIes(response + WLAN_ASSOC_RESPONSE_FIXED,
			     (uint16_t)(length - WLAN_ASSOC_RESPONSE_FIXED), &wlanJoin.ies);
	}
	if (wlanJoin.result == WLAN_JOIN_OK) {
		wlanJoinClosePort();
		wlanBaSessionsDelete(wlanJoin.connection, wlanJoin.previous.bssid);
		wlanPsPhaseSuspend();
		if (wlanJoin.reassociationRequest)
			wlanJoin.associatedAt = 0;
		else
			wlanJoin.associatedAt = (now - wlanJoin.initTime) / 1000000;
		wlanJoin.connection->flags2 |= WLAN_CONN2_ASSOCIATED;
		wlanJoin.connection->state = WLAN_CONN_STATE_ASSOCIATED;

		posture = sysIrqDisable();
		romLibUpdateContext(&record);
		record.timestamp = now + wlanJoin.tsfOffset;
		record.beaconInterval = 102400;
		cbProcRecordBuffer(0, (int)(uintptr_t)&record);
		sysIrqRestore(posture);

		wlanPsVerifyFrame(3);
		if (wlanJoin.connection->interfaceType != WLAN_IF_AMP)
			wlanLinkMonitorStart(wlanJoin.connection);
		if (wlanJoin.capability[1] & WLAN_CAPABILITY1_SPECTRUM_MGMT)
			wlanTxPowerUpdateConnection(wlanJoin.connection, wlanJoin.channel, 0);
		wlanTxPowerApply();
		wpaSuppScheduleDescriptor(wlanJoin.connection);
		wlanJoin.connection->bssCapability[0] = wlanJoin.capability[0];
		wlanJoin.connection->bssCapability[1] = wlanJoin.capability[1];
		wlanJoin.connection->dtimPeriod = 0;
		wlanPsHostMode = 0;
		wlanJoin.connection->flags = (wlanJoin.connection->flags & ~WLAN_CONN_PRIVACY) |
					     ((wlanJoin.privacy << 2) & 7);
		flags = (wlanJoin.connection->flags & ~WLAN_CONN_WAPI) | ((wlanJoin.wapi << 4) & 0x1f);
		wlanJoin.connection->flags = flags;
		if (!wlanJoin.portPending) {
			if (!(wlanJoin.connection->flags2 & WLAN_CONN2_ASSOCIATED) ||
			    (!(flags & WLAN_CONN_WAPI) && !(flags & WLAN_CONN_PRIVACY) &&
			     (!(wlanJoin.connection->bssCapability[0] & WLAN_CAPABILITY0_PRIVACY) ||
			      (wlanJoin.connection->hostFlags & WLAN_CONN_HOST_FLAG_8))))
				wlanJoin.portPending = 1;
			macMgmtResolveState(wlanJoin.connection, wlanJoin.privacy | wlanJoin.wapi);
			if (wlanJoin.privacy && wlanJoin.connection->interfaceType != WLAN_IF_AMP)
				supplicantInitSession(wlanJoin.connection, wlanJoin.bss.ssid,
						      wlanJoin.bss.ssidLength,
						      (IEEEtypes_MacAddr_t *)wlanJoin.bss.bssid,
						      wlanDeviceAddress);
		}
		cbProcFilterPacket(wlanJoin.connection, &wlanJoin.ies, wlanJoin.sameBss, 0);
		wlanSetSlotTime((wlanJoin.connection->bssCapability[1] >> 2) & 1);
		wlanSlotAbort(wlanJoin.slot, WLAN_SLOT_EVENT_JOIN_DONE);
		connection = wlanJoin.connection;
		__aeabi_memcpy4(wlanJoin.previous.bssid, connection->localAddress,
				sizeof(wlanJoin.previous.bssid));
		__aeabi_memcpy(wlanJoin.previous.ssid, connection->networkSsid,
			       sizeof(wlanJoin.previous.ssid));
		wlanJoin.previous.ssidLength = connection->networkSsidLength;
	} else {
		if (wlanJoin.reassociation) {
			wlanJoinSetBss(&wlanJoin.previous);
		} else {
			__aeabi_memclr4(&wlanJoin.previous, sizeof(wlanJoin.previous));
			wlanJoin.connection->state = 0;
		}
		wlanJoin.connection->flags2 = (wlanJoin.connection->flags2 & ~WLAN_CONN2_ASSOCIATED) |
					      ((wlanJoin.reassociation << 4) & 0x1f);
		__aeabi_memcpy(&wlanProtection, &wlanJoin.savedProtection, sizeof(wlanProtection));
		macMgmtClearContext(wlanJoin.connection);
		wlanJoinSetBusy(wlanJoin.connection, 0);
		wlanSlotAbort(wlanJoin.slot, WLAN_SLOT_EVENT_JOIN_FAILED);
	}
	wlanBaTablesChangedHandler();
	if (!(wlanJoin.connection->flags & WLAN_CONN_PRIVACY))
		wlanJoin.connection->flags |= WLAN_CONN_JOIN_OPEN;
}

/* 0x11ece: the join's slot request: started, polled, timed out, ended. */
int wlanJoinSlotEvent(uint32_t handle, int event, uint32_t argument)
{
	struct WlanConnection *connection;
	OsaBuffer *buffer;
	uint8_t *data;
	int result = WLAN_SLOT_OK;

	(void)argument;
	if (wlanJoin.slot != handle) {
		if (wlanJoin.slot)
			wlanJoinFinish(WLAN_JOIN_PREEMPTED);
		return WLAN_SLOT_FAILED;
	}
	connection = wlanJoin.connection;
	switch (event) {
	case WLAN_SLOT_EVENT_JOIN_START:
		wlanJoin.starts++;
		if ((connection->flags2 & WLAN_CONN2_ASSOCIATED) && wlanPsEnabled() &&
		    (wlanCoexUseP2pMode() || wlanCoexUseStationMode()))
			break;
		if (!macTxPoolsIdle())
			result = 2;
		break;
	case WLAN_SLOT_EVENT_POLL:
		if (wlanJoin.state == WLAN_JOIN_FINISHED)
			break;
		if (wlanJoin.state > WLAN_JOIN_AUTH_DONE) {
			wlanJoinSendAssoc();
			break;
		}
		wlanJoin.retries++;
		wlanJoinSendAuth(0);
		break;
	case WLAN_SLOT_EVENT_START:
		wlanJoinSetBusy(connection, 1);
		wlanJoin.savedPowerSave = macTxGetPowerSave();
		macTxSetPowerSave(0);
		connection = wlanJoin.connection;
		if ((connection->flags2 & WLAN_CONN2_ASSOCIATED) &&
		    connection->interfaceType != WLAN_IF_AMP) {
			wlanJoin.sameBss = wlanConnectionHasLocalAddress(connection, wlanJoin.bss.bssid);
			wlanJoin.newBss = !wlanJoin.sameBss;
		}
		macMgmtQueryContext(wlanJoin.connection, wlanJoin.rates, wlanJoin.rateCount, 0);
		wlanJoinSetBss(&wlanJoin.bss);
		connection = wlanJoin.connection;
		wlanJoin.reassociation = (connection->flags2 & WLAN_CONN2_ASSOCIATED) >> 4;
		wlanJoin.retries++;
		if ((wlanJoin.sameBss || wlanJoin.authType == WLAN_JOIN_AUTH_FT) &&
		    connection->interfaceType != WLAN_IF_AMP)
			wlanJoinSendAssoc();
		else
			wlanJoinSendAuth(0);
		break;
	case WLAN_SLOT_EVENT_TICK:
		if (wlanJoin.retries == 0 || wlanJoin.retries >= 2)
			break;
		wlanJoin.retries++;
		wlanMacFlush();
		wlanJoinSendAuth(0);
		break;
	case WLAN_SLOT_EVENT_JOIN_EXPIRED:
		if (wlanJoin.state == WLAN_JOIN_FINISHED)
			break;
		wlanJoinFinish(wlanJoin.state <= WLAN_JOIN_AUTH_DONE ? WLAN_JOIN_AUTH_TIMEOUT :
								       WLAN_JOIN_ASSOC_TIMEOUT);
		break;
	case WLAN_SLOT_EVENT_JOIN_FAILED:
	case WLAN_SLOT_EVENT_JOIN_DONE:
		wlanJoin.slot = 0;
		wlanJoin.active = 0;
		if (!ampPalOwnsConnection(connection)) {
			buffer = wlanHostMessageAlloc(wlanJoin.connection);
			if (buffer != NULL) {
				data = (uint8_t *)buffer + buffer->dataOffset;
				data[0] = HostCmd_RET_802_11_ASSOCIATE & 0xff;
				data[1] = HostCmd_RET_802_11_ASSOCIATE >> 8;
				data[6] = 0;
				data[7] = 0;
				wlanHostMessagePost(buffer);
			}
		}
		wlanJoinFilterIdle();
		break;
	default:
		break;
	}
	return result;
}

/*
 * 0x12032: frames of the join: the AP's authentication and (re)association
 * responses and its deauthentication, and the transmit status of our requests.
 */
int wlanJoinHandleFrame(uint32_t handle, const struct WlanTxStatus *status, OsaBuffer *rx,
			uint32_t unused, int argument)
{
	const WlanWirelessFrame *frame;
	const WlanWirelessFrame *sent;
	const struct WlanConnection *owner;
	unsigned int frameControl;
	int handled = 0;

	(void)handle;
	(void)unused;
	if (rx != NULL) {
		frame = (const WlanWirelessFrame *)((uint8_t *)rx + rx->dataOffset);
		if (memcmp(wlanJoin.bss.bssid, frame->address3, 6) != 0) {
			handled = 1;
		} else {
			switch (WLAN_FC_SUBTYPE(frame->frameControl[0])) {
			case WLAN_SUBTYPE_ASSOC_RESPONSE:
			case WLAN_SUBTYPE_REASSOC_RESPONSE:
				wlanSlotClearWindow(wlanJoin.slot);
				if (wlanJoin.state != WLAN_JOIN_ASSOC_ACKED) {
					if (wlanJoin.state != WLAN_JOIN_ASSOC_SENT) {
						handled = 1;
						break;
					}
					wlanMacFlush();
				}
				wlanSignalJoinFrame(wlanJoin.connection, (const struct WlanRxStatus *)argument);
				wlanIeBufferReset(&wlanJoin.ieBuffer);
				wlanIeBufferAppend(&wlanJoin.ieBuffer, frame->body,
						   LE16(frame->bodyLength));
				macTxNotifyClearFrame(rx);
				handled = 1;
				break;
			case WLAN_SUBTYPE_DISASSOC:
			case WLAN_SUBTYPE_DEAUTH:
				wlanSlotClearWindow(wlanJoin.slot);
				if (wlanJoin.retries != 0 && wlanJoin.retries < 2) {
					wlanJoin.retries++;
					wlanMacFlush();
					wlanJoinSendAuth(0);
				}
				handled = 1;
				break;
			case WLAN_SUBTYPE_AUTH:
				wlanSlotClearWindow(wlanJoin.slot);
				if (wlanJoin.state != WLAN_JOIN_AUTH_ACKED) {
					if (wlanJoin.state != WLAN_JOIN_AUTH_SENT) {
						handled = 1;
						break;
					}
					wlanMacFlush();
				}
				wlanSignalJoinFrame(wlanJoin.connection, (const struct WlanRxStatus *)argument);
				macTxNotifyUpdateHeader(rx);
				handled = 1;
				break;
			default:
				break;
			}
		}
	}
	if (status == NULL)
		return handled;

	owner = ((const OsaBuffer *)(status->data - offsetof(OsaBuffer, data)))->context;
	sent = (const WlanWirelessFrame *)TX_DATA_FRAME(status->data);
	frameControl = sent->frameControl[0];
	if (WLAN_FC_TYPE(frameControl) != 0)
		return handled;
	switch (WLAN_FC_SUBTYPE(frameControl)) {
	case WLAN_SUBTYPE_ASSOC_REQUEST:
	case WLAN_SUBTYPE_REASSOC_REQUEST:
		if (status->result == 0) {
			if (wlanJoin.state == WLAN_JOIN_ASSOC_SENT)
				wlanJoin.state = WLAN_JOIN_ASSOC_ACKED;
		} else if (wlanJoin.state == WLAN_JOIN_ASSOC_SENT &&
			   !(owner->flags2 & WLAN_CONN2_ASSOCIATED)) {
			wlanJoinFinish(WLAN_JOIN_ASSOC_TX_FAILED);
		}
		return 1;
	case WLAN_SUBTYPE_AUTH:
		if (wlanJoin.authAlgorithm != WLAN_AUTH_ALG_SHARED) {
			wlanSlotClearWindow(wlanJoin.slot);
			if (wlanJoin.authTimeout)
				wlanSlotSetWindow(wlanJoin.slot, 1000 * wlanJoin.authTimeout);
		}
		if (status->result == 0) {
			if (wlanJoin.state == WLAN_JOIN_AUTH_SENT)
				wlanJoin.state = WLAN_JOIN_AUTH_ACKED;
		} else if (wlanJoin.state == WLAN_JOIN_AUTH_SENT &&
			   !(owner->flags2 & WLAN_CONN2_ASSOCIATED)) {
			wlanJoinFinish(WLAN_JOIN_AUTH_TX_FAILED);
		}
		return 1;
	default:
		return handled;
	}
}

/*
 * 0x12196: take the BSS and the host's elements from the associate command
 * and ask for channel time. Returns 0 when the join cannot start.
 */
int wlanJoinSetup(HostCmd_DS_802_11_ASSOCIATE *command, unsigned int length)
{
	struct WlanSlotParams params;
	IEEEtypes_RSNCapability_t rsnCapabilityValue = { 0 };
	IEEEtypes_RSNCapability_t *rsnCapability = &rsnCapabilityValue;
	SecurityMode_t wpaType = { 0 };
	SecurityMode_t rsnType = { 0 };
	Cipher_t wpaMulticast, wpaUnicast, rsnMulticast, rsnUnicast;
	AkmSuite_t wpaAkm[5], rsnAkm[5];
	uint8_t channels[8] = { 0 };
	uint8_t powerCapability[4];
	uint64_t local, remote;
	uint8_t *tlv;
	uint8_t *rsnIe;
	unsigned int remaining, tlvLength, copy;
	uint16_t listenInterval;

	__aeabi_memcpy(wlanJoin.bss.bssid, command->peerStaAddr, sizeof(wlanJoin.bss.bssid));
	wlanJoin.capability[0] = command->capInfo[0];
	wlanJoin.capability[1] = command->capInfo[1];
	__aeabi_memclr4(wlanJoinWmmIe, sizeof(wlanJoinWmmIe));
	__aeabi_memclr(wlanJoinHtCapability, sizeof(wlanJoinHtCapability));
	__aeabi_memclr4(wlanJoin.rates, sizeof(wlanJoin.rates));
	remaining = length - offsetof(HostCmd_DS_802_11_ASSOCIATE, tlvBuffer);
	tlv = command->tlvBuffer;
	listenInterval = LE16(command->listenInterval);
	if (listenInterval)
		wlanAssocListenInterval = listenInterval;
	wlanJoin.privacy = 0;
	wlanJoin.tsfOffset = 0;
	wlanJoin.wapi = 0;
	wlanJoin.authType = WLAN_JOIN_AUTH_AUTO;
	wlanIeBufferAlloc(&wlanJoin.ieBuffer);
	__aeabi_memcpy(wlanJoin.bss.bssid, command->peerStaAddr, sizeof(wlanJoin.bss.bssid));
	wlanJoin.capability[0] = command->capInfo[0];
	wlanJoin.capability[1] = command->capInfo[1];
	wlanJoin.beaconPeriod[0] = command->beaconPeriod[0];
	wlanJoin.beaconPeriod[1] = command->beaconPeriod[1];
	wlanJoin.dtimPeriod = command->dtimPeriod;

	while (remaining >= 4) {
		tlvLength = LE16(tlv + 2);
		if (tlvLength > remaining - 4)
			break;
		switch (LE16(tlv)) {
		case TLV_TYPE_SSID:
			copy = tlvLength <= 32 ? tlvLength : 32;
			wlanJoin.bss.ssidLength = copy;
			__aeabi_memcpy(wlanJoin.bss.ssid, tlv + 4, copy);
			break;
		case TLV_TYPE_RATES:
			copy = tlvLength < 14 ? tlvLength : 14;
			wlanJoin.rateCount = copy;
			__aeabi_memcpy(wlanJoin.rates, tlv + 4, copy);
			break;
		case TLV_TYPE_PHY_DS:
			wlanJoin.channel = tlv[4];
			break;
		case TLV_TYPE_HT_CAPABILITY:
			/* the host's TLV becomes the element it carries */
			tlv[2] = 26;
			tlv[3] = 0;
			tlv[0] = IE_HT_CAPABILITY;
			tlv[1] = 0;
			/* fall through */
		case TLV_TYPE_HT_CAPABILITY_IE:
			wlanJoinHtCapability[0] = IE_HT_CAPABILITY;
			wlanJoinHtCapability[1] = tlv[2];
			__aeabi_memcpy(&wlanJoinHtCapability[2], tlv + 4, LE16(tlv + 2));
			break;
		case TLV_TYPE_RSN_IE:
			wlanJoin.privacy = 1;
			if (!supplicantIsEnabled()) {
				wlanIeBufferAppendTlv(&wlanJoin.ieBuffer, tlv);
				break;
			}
			supplicantParseRsnIe((IEEEtypes_RSNElement_t *)(tlv + 2), &rsnType,
					     &rsnMulticast, &rsnUnicast, rsnAkm, 5, rsnCapability,
					     NULL);
			break;
		case TLV_TYPE_2040BSS_COEX_IE:
		case TLV_TYPE_EXTCAP_IE:
			wlanIeBufferAppendTlv(&wlanJoin.ieBuffer, tlv);
			break;
		case TLV_TYPE_VENDOR_IE:
			if (wlanWmmElementType(tlv + 4) == WLAN_VENDOR_WMM_INFORMATION) {
				wlanJoinWmmIe[0] = tlv[0];
				wlanJoinWmmIe[1] = tlv[2];
				__aeabi_memcpy(&wlanJoinWmmIe[2], tlv + 4, LE16(tlv + 2));
			}
			if (wlanWpaElementType(tlv + 4) != WLAN_VENDOR_WPA)
				break;
			wlanJoin.privacy = 1;
			if (!supplicantIsEnabled()) {
				wlanIeBufferAppendTlv(&wlanJoin.ieBuffer, tlv);
				break;
			}
			supplicantParseWpaIe((IEEEtypes_WPAElement_t *)(tlv + 2), &wpaType,
					     &wpaMulticast, &wpaUnicast, wpaAkm, 5);
			break;
		case TLV_TYPE_CHANLIST:
			wlanTlvChannelListParse(tlv, channels, NULL);
			wlanJoin.channel = channels[1];
			wlanJoin.band = channels[0];
			break;
		case TLV_TYPE_PASSTHROUGH:
			wlanIeBufferAppend(&wlanJoin.ieBuffer, tlv + 4, tlvLength);
			break;
		case TLV_TYPE_TSFTIMESTAMP:
			__aeabi_memcpy(&remote, tlv + 12, sizeof(remote));
			if (remote == 0)
				break;
			__aeabi_memcpy(&local, tlv + 4, sizeof(local));
			if (local == 0)
				break;
			__aeabi_memcpy(&local, tlv + 4, sizeof(local));
			__aeabi_memcpy(&remote, tlv + 12, sizeof(remote));
			wlanJoin.tsfOffset = remote - local;
			break;
		case TLV_TYPE_AUTH_TYPE:
			wlanJoin.authType = tlv[4];
			break;
		case TLV_TYPE_2040BSS_COEX:
			tlv[0] = TLV_TYPE_2040BSS_COEX_IE;
			tlv[1] = 0;
			wlanIeBufferAppendTlv(&wlanJoin.ieBuffer, tlv);
			break;
		case TLV_TYPE_EXTCAP:
			tlv[0] = TLV_TYPE_EXTCAP_IE;
			tlv[1] = 0;
			wlanIeBufferAppendTlv(&wlanJoin.ieBuffer, tlv);
			break;
		case TLV_TYPE_WAPI_IE:
			wlanJoin.wapi = 1;
			wlanIeBufferAppend(&wlanJoin.ieBuffer, tlv + 4, LE16(tlv + 2));
			break;
		default:
			break;
		}
		remaining = remaining - 4 - tlvLength;
		tlv += tlvLength + 4;
	}

	if (channels[1] == 0)
		macMgmtPrepareContext(&wlanJoin.band, NULL);
	if ((wlanJoin.capability[1] & WLAN_CAPABILITY1_RADIO_MEASURE) ||
	    (wlanJoin.capability[1] & WLAN_CAPABILITY1_SPECTRUM_MGMT)) {
		powerCapability[0] = IE_POWER_CAPABILITY;
		powerCapability[1] = 2;
		powerCapability[2] = wlanTxPowerMin(wlanJoin.channel, wlanJoin.band & 3);
		powerCapability[3] = wlanTxPowerMax(wlanJoin.channel, wlanJoin.band & 3);
		wlanIeBufferAppend(&wlanJoin.ieBuffer, powerCapability, sizeof(powerCapability));
	}
	if (supplicantIsEnabled() && wlanJoin.privacy) {
		if (supplicantIsCounterMeasuresActive())
			return 0;
		/* the element is built in the command buffer, past the command */
		rsnIe = (uint8_t *)command + length + 100;
		__aeabi_memclr(rsnIe, 44);
		if (rsnType.wpa2)
			supplicantSetAssocRsn(rsnType, &rsnMulticast, &rsnUnicast, rsnAkm,
					      rsnCapability, NULL);
		else if (wpaType.wpa)
			supplicantSetAssocRsn(wpaType, &wpaMulticast, &wpaUnicast, wpaAkm,
					      rsnCapability, NULL);
		if (keyMgmtFormatWpaRsnIe(rsnIe, (IEEEtypes_MacAddr_t *)command->peerStaAddr,
					  wlanDeviceAddress, NULL, 0)) {
			wlanJoin.privacy = 1;
			wlanIeBufferAppend(&wlanJoin.ieBuffer, rsnIe, rsnIe[1] + 2);
		}
	}
	__aeabi_memcpy(wlanBssWmmIe, wlanJoinWmmIe, sizeof(wlanBssWmmIe));

	__aeabi_memclr4(&params, sizeof(params));
	params.callback = wlanJoinSlotEvent;
	params.context = (uint32_t)(uintptr_t)wlanJoinHandleFrame;
	params.connection = wlanJoin.connection;
	wlanJoin.reassociationRequest = 0;
	if (((wlanJoin.connection->flags2 & WLAN_CONN2_ASSOCIATED) &&
	     wlanJoin.connection->interfaceType != WLAN_IF_AMP) ||
	    (memcmp(wlanJoin.previous.ssid, wlanJoin.bss.ssid, wlanJoin.bss.ssidLength) == 0 &&
	     wlanJoin.connection->interfaceType != WLAN_IF_AMP &&
	     wlanJoin.reassociationWindow >=
		     (romGetTimerTimestamp() - wlanJoin.initTime) / 1000000))
		wlanJoin.reassociationRequest = 1;
	wlanSignalJoinReset(wlanJoin.connection);
	macMgmtTrackState(wlanJoin.connection);
	params.mode = wlanJoin.channel;
	params.bssTypeLow = wlanJoin.band & 3;
	params.bssTypeMiddle = (wlanJoin.band >> 2) & 3;
	params.bssTypeHigh = 0;
	if ((wlanJoin.connection->flags2 & WLAN_CONN2_ASSOCIATED) &&
	    wlanJoin.connection->interfaceType != WLAN_IF_AMP) {
		params.duration = 1000 * wlanJoin.reassociationTime;
		params.flags |= 1;
	} else {
		params.duration = 1000 * wlanJoin.associationTime;
		params.flags &= ~1;
	}
	wlanJoin.newBss = 0;
	wlanJoin.sameBss = 0;
	params.tag = params.duration + 10000;
	params.flags |= 1;
	params.delay = 0;
	wlanJoinFilterActive();
	wlanJoin.slot = wlanSlotSubmit(&params);
	return wlanJoin.slot != 0;
}

/* 0x12678: HostCmd_CMD_802_11_ASSOCIATE. Returns 0 when a join is under way or fails to start. */
int wlanJoinAssociate(struct WlanConnection *connection, HostCmd_DS_802_11_ASSOCIATE *command,
		      unsigned int length)
{
	if (wlanJoin.active)
		return 0;
	wlanJoin.connection = connection;
	wlanJoin.active = 1;
	wlanJoin.portPending = 0;
	wlanJoin.retries = 0;
	wlanJoin.state = WLAN_JOIN_IDLE;
	wlanJoin.starts = 0;
	wlanIeBufferAlloc(&wlanJoin.ieBuffer);
	__aeabi_memcpy(&wlanJoin.savedProtection, &wlanProtection, sizeof(wlanJoin.savedProtection));
	if (!wlanJoinSetup(command, length)) {
		__aeabi_memcpy(&wlanProtection, &wlanJoin.savedProtection, sizeof(wlanProtection));
		macMgmtClearContext(wlanJoin.connection);
		wlanIeBufferFree(&wlanJoin.ieBuffer);
		wlanJoin.active = 0;
		wlanJoinFilterIdle();
		return 0;
	}
	wlanJoin.connection->state = WLAN_CONN_STATE_JOINING;
	return 1;
}

/* 0x126fa: the AP answered the authentication. */
void wlanJoinAuthResult(int result, uint16_t statusCode)
{
	wlanJoin.statusCode = statusCode;
	wlanJoin.state = WLAN_JOIN_AUTH_DONE;
	if (result == 5) {
		wlanJoinSendAssoc();
		return;
	}
	if (result == 7 && statusCode == WLAN_STATUS_UNSUPPORTED_AUTH_ALG &&
	    wlanJoin.authType == WLAN_JOIN_AUTH_AUTO && wlanJoin.authAlgorithm == WLAN_AUTH_ALG_OPEN) {
		wlanJoinSendAuth(1);
		return;
	}
	wlanJoinFinish(result);
}

/* 0x1275e: the AP answered the (re)association. */
void wlanJoinAssocResult(int result, uint16_t statusCode)
{
	wlanJoin.state = WLAN_JOIN_ASSOC_DONE;
	wlanJoin.statusCode = statusCode;
	wlanJoinFinish(result);
}

/* 0x12770 */
void wlanJoinInit(void)
{
	__aeabi_memclr4(&wlanJoin, offsetof(struct WlanJoin, savedProtection));
	wlanJoin.reassociationTime = 200;
	wlanJoin.associationTime = 1000;
	wlanJoin.authTimeout = 50;
	wlanJoin.portTimeout = 2000;
	wlanJoin.reassociationWindow = 30;
	wlanJoin.initTime = romGetTimerTimestamp();
}

/* 0x127a8: the host has the join's result. */
int wlanJoinReported(OsaBuffer *buffer)
{
	if (wlanJoin.result == WLAN_JOIN_OK) {
		cbProcClearPacket(buffer->context);
		if (wlanJoin.portPending)
			keyMgmtControlledPortOpen();
		wlanJoin.portPending = 0;
		wlanJoinSetBusy(buffer->context, 0);
	}
	wlanPsPrepareTable(buffer->context);
	return 0;
}

/* 0x127e0 */
uint16_t wlanJoinListenInterval(void)
{
	return wlanAssocListenInterval;
}

/*
 * 0x127e6: the associate command's result for the host: the AP's response
 * (capability, status, AID and elements), or a failure in its shape.
 */
unsigned int wlanJoinGetResponse(uint8_t *response)
{
	struct WlanJoinFailure *failure = (struct WlanJoinFailure *)response;
	uint8_t *data;
	unsigned int length;
	uint16_t value;

	if (wlanJoin.result == WLAN_JOIN_OK || wlanJoin.result == WLAN_JOIN_REFUSED) {
		length = wlanIeBufferGet(&wlanJoin.ieBuffer, &data);
		__aeabi_memcpy(response, data, length);
	} else {
		failure->aid[0] = 0xff;
		failure->aid[1] = 0xff;
		length = sizeof(*failure);
		/* 6, 7 and 9 come from the callers of wlanJoinAuthResult() and
		   wlanJoinAssocResult(), not converted yet. */
		switch (wlanJoin.result) {
		case WLAN_JOIN_ASSOC_TX_FAILED:
		case WLAN_JOIN_ASSOC_TIMEOUT:
			failure->capability = 0xfffc;
			value = 1;
			break;
		case 6:
			failure->capability = 0xfffb;
			value = wlanJoin.statusCode;
			break;
		case 7:
			failure->capability = 0xfffd;
			value = 0xffff;
			break;
		case WLAN_JOIN_AUTH_TX_FAILED:
		case WLAN_JOIN_AUTH_TIMEOUT:
			failure->capability = 0xfffc;
			value = 2;
			break;
		case 9:
			failure->capability = 0xfffe;
			value = 0;
			break;
		default:
			failure->capability = 0xffff;
			value = wlanJoin.statusCode;
			break;
		}
		failure->statusCode[0] = value;
		failure->statusCode[1] = value >> 8;
	}
	wlanIeBufferFree(&wlanJoin.ieBuffer);
	return length;
}

/* 0x12868: a hook the SQRAM code calls; nothing to do. */
void wlanJoinHook(void)
{
}

/* 0x1286a: the MAC's filters once no join holds the channel. */
int wlanJoinFilterIdle(void)
{
	int filter = WLAN_JOIN_FILTER_IDLE;

	if (cbProcVerifyEntry())
		filter = WLAN_JOIN_FILTER_IDLE_ENTRY;
	macMgmtFilterPacket(filter, 2);
	macMgmtFilterPacket(filter, 3);
	return 1;
}

/* 0x1288c: tell the SDIO device the MAC is (not) busy joining. */
int wlanJoinSetBusy(struct WlanConnection *connection, uintptr_t busy)
{
	if (connection == NULL)
		return 0;
	if (connection->bssType == WLAN_BSS_TYPE_AMP)
		return WLAN_BSS_TYPE_AMP;
	return mdevIoctl(wlanSdioDev, MDEV_IOCTL_SDIO_JOIN_BUSY, busy);
}

/* 0x128a6: the MAC's filters while the join holds the channel. */
int wlanJoinFilterActive(void)
{
	uint8_t flags;
	int filter;

	if (cbProcSetupEntry()) {
		filter = WLAN_JOIN_FILTER_SAME_CHANNEL;
		wlanCurrentChannelFlags(&flags);
		if (wlanOperatingChannel() != wlanJoin.channel ||
		    (flags & 3) != (wlanJoin.band & 3) ||
		    (wlanJoinHtCapability[0] != 0 && (wlanJoinHtCapability[2] & WLAN_HT_CAP_40MHZ) &&
		     ((flags >> 4) & 3) != 0))
			filter = WLAN_JOIN_FILTER_OTHER_CHANNEL;
		macMgmtFilterPacket(filter, 2);
		macMgmtFilterPacket(filter, 3);
	}
	return 1;
}
