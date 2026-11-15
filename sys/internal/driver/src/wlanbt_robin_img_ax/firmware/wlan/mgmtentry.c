/*
 * The IBSS start and join entry code of the management layer (0x3600a..0x36bf0 and what follows):
 * the beacon and probe response templates of a Bluetooth AMP link, the TLVs and the security
 * elements the host gives an IBSS. Marvell published no source for it; the names are the
 * firmware's own placeholder names (macMgmt*Entry), kept, and the file name is inferred.
 */
#include <stddef.h>
#include "robin/adhoc.h"
#include "robin/hostps.h"
#include "robin/htba.h"
#include "robin/amp.h"
#include "robin/buffer.h"
#include "robin/conn.h"
#include "robin/esa.h"
#include "robin/hostcmd.h"
#include "robin/ie.h"
#include "robin/apsta.h"
#include "robin/macentry.h"
#include "robin/mactx.h"
#include "robin/mgmt.h"
#include "robin/mgmtentry.h"
#include "robin/peerrate.h"
#include "robin/protection.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/soc.h"
#include "robin/stamachine.h"
#include "robin/txring.h"

/* Marvell's name for this element is not known; it is the EDCA Parameter Set (ID 12) of 802.11e. */
static const uint8_t wlanAmpEdcaParameterSet[20] = {
	12, 18, 0x80, 0x00,
	0x03, 0xa4, 0x00, 0x00,		/* AC_BE */
	0x27, 0xa4, 0x00, 0x00,		/* AC_BK */
	0x42, 0x43, 0x5e, 0x00,		/* AC_VI */
	0x62, 0x32, 0x2f, 0x00,		/* AC_VO */
};

/* The ROM's. */
void romLibInspectState(unsigned int mask);
void romLibTrackState(unsigned int mask);

struct WlanHtTlvElements wlanIbssTlvElements __attribute__((section(".itcm.rw")));	/* 0x57890 */
struct WlanIeBuffer wlanIbssIeBuffer __attribute__((section(".itcm.bss")));		/* 0x55170 */
int (*wlanSecurityQueryHook)(void) __attribute__((section(".itcm.bss")));		/* 0x54e74 */
void (*wlanSecurityRecordHook)(SecurityMode_t mode, Cipher_t *mcast, Cipher_t *ucast, AkmSuite_t *akmList,
			       IEEEtypes_RSNCapability_t *rsnCapability,
			       Cipher_t *groupManagementCipher) __attribute__((section(".itcm.bss"))); /* 0x54e78 */
uint32_t wlanIbssPowerLimit __attribute__((section(".itcm.bss")));			/* 0x5516c */
extern uint8_t ampScanChannel;							/* 0x54cc1 */
extern OsaBuffer *wlanIbssBeaconBuffer;						/* 0x54c20 */
extern OsaBuffer *wlanIbssProbeResponseBuffer;					/* 0x54c24 */
extern WlanWirelessFrame *wlanIbssProbeResponseFrame;				/* 0x54c28 */
extern WlanWirelessFrame *wlanIbssBeaconFrame;					/* 0x54c2c */
extern struct WlanMacTxRequest *wlanIbssBeaconRequest;				/* 0x54bc4 */

void wlanPsFilterFrame(uint16_t beaconPeriod);					/* 0x3dc4e */
int wlanPsVerifyFrame(int reason);						/* 0x3dac6 */
void wlanMacFlush(void);								/* 0x1c840 */
int btLmpLcHandleState(int state);							/* 0x42488 */
void uapSqramConfigureState(int value);						/* SQRAM */

/* 0x3600a: the beacon (subtype 8) or probe response (5) template of an AMP link and its MAC request. */
void macMgmtProcessEntry(struct WlanConnection *connection, unsigned int subtype, OsaBuffer *buffer,
			 struct WlanMacTxRequest *request)
{
	uint8_t edca[sizeof(wlanAmpEdcaParameterSet)];
	WlanWirelessFrame *frame;
	uint8_t *body;
	uint8_t *cursor;
	uint8_t *rsn;
	unsigned int count;
	unsigned int extended = 0;
	unsigned int rate;
	unsigned int length;
	uint32_t word;
	int hasExtended = 0;

	__aeabi_memcpy4(edca, wlanAmpEdcaParameterSet, sizeof(edca));
	wlanMgmtBuildFrame(buffer, connection->interfaceAddress, wlanBroadcastAddress, subtype,
			   connection->interfaceAddress);
	frame = (WlanWirelessFrame *)((uint8_t *)buffer + 176);
	buffer->dataOffset = 176;
	wlanSetSlotTime(1);
	body = frame->body;
	body[8] = connection->beaconPeriod;
	body[9] = connection->beaconPeriod >> 8;
	body[10] = connection->bssCapability[0];
	body[11] = connection->bssCapability[1];
	cursor = body + 12;

	cursor[0] = 0;			/* SSID: the link's name */
	cursor[1] = connection->localName.length;
	__aeabi_memcpy(cursor + 2, connection->localName.name, connection->localName.length);
	cursor += 2 + connection->localName.length;

	count = romStringLength(wlanLocalState->rates, 14);
	if (count > 4) {
		extended = (uint8_t)(count - 4);
		hasExtended = 1;
	}
	cursor[0] = 1;			/* Supported Rates: the first four */
	cursor[1] = romStringCopy(cursor + 2, wlanLocalState->rates, 4);
	cursor += 2 + cursor[1];
	cursor[0] = 3;			/* DS Parameter Set */
	cursor[1] = 1;
	cursor[2] = connection->channel;
	cursor += 3;
	if ((wlanChannelContext.savedFlags & 3) == 0 && !(wlanProtection.flags & WLAN_PROTECTION_DSSS_BSS)) {
		int nonErp = 0;
		int useProtection = 0;
		int barker = 0;

		if (__aeabi_uread4(wlanProtection.timers[WLAN_PROTECTION_DSSS_ONLY].remaining)) {
			nonErp = 1;
			useProtection = 1;
		}
		if ((connection->txOptions & 0x20) ||
		    __aeabi_uread4(wlanProtection.timers[WLAN_PROTECTION_USE_ERP].remaining))
			useProtection = 1;
		if (__aeabi_uread4(wlanProtection.timers[WLAN_PROTECTION_NO_SHORT_PREAMBLE].remaining))
			barker = 1;
		cursor += wlanPutErpIe(cursor, nonErp, useProtection, barker);
	}
	if (hasExtended) {
		cursor[0] = 50;		/* Extended Supported Rates: the rest */
		cursor[1] = romStringCopy(cursor + 2, wlanLocalState->rates + 4, extended);
		cursor += 2 + cursor[1];
	}
	rsn = cursor;
	cursor += btampAddRsnIe((IEEEtypes_RSNElement_t *)cursor);
	__aeabi_memcpy(connection->rsnElement, rsn, rsn[1] + 2);
	if (ampScanChannel != 0) {
		__aeabi_memcpy(cursor, edca, edca[1] + 2);
		cursor += edca[1] + 2;
	}
	if (subtype == WLAN_FRAME_BEACON) {
		cursor[0] = 5;		/* TIM: no traffic, DTIM period 1 */
		cursor[1] = 4;
		cursor[2] = 0;
		cursor[3] = 1;
		cursor[4] = 0;
		cursor[5] = 0;
		cursor += 6;
	}

	rate = wlanTxControlRate(connection);
	wlanTxRequestReset(request);
	__aeabi_uwrite4(0, request->flags);
	if (subtype == WLAN_FRAME_BEACON)
		__aeabi_uwrite4(256, request->flags);
	word = __aeabi_uread4(request->flags);
	__aeabi_uwrite4(word & 0xffffefff, request->flags);
	__aeabi_uwrite4((uint32_t)wlanRateTableLookup(rate), request->rateInfo);
	__aeabi_uwrite4((uint32_t)frame, request->frame);
	length = (uint16_t)(cursor - (uint8_t *)frame - 32);
	frame->bodyLength[0] = length;
	frame->bodyLength[1] = length >> 8;
	request->timOffset = cursor - (uint8_t *)frame - 8;
	length = (frame->bodyLength[0] | (frame->bodyLength[1] << 8)) + 28;
	request->templateLength[0] = length;
	request->templateLength[1] = length >> 8;
	if (subtype == WLAN_FRAME_BEACON)
		wlanBeaconTemplateEnd = cursor;
	else
		wlanProbeTemplateEnd = cursor;
	__aeabi_memcpy(frame->address1, wlanBroadcastAddress, 6);
}

/* 0x36236: the host started an AMP link's IBSS: the radio, the timing and the templates are set. */
void macMgmtUpdateEntry(uintptr_t argument)
{
	struct WlanConnection *connection = (struct WlanConnection *)argument;
	const uint8_t *rates = wlanLocalState->rates;
	struct WlanBssTiming timing;
	uint8_t flags[2];
	unsigned int count;
	OsaBuffer *buffer;

	wlanMacFlush();
	flags[0] = 0;
	flags[1] = connection->channel;
	macMgmtProcessContext(connection, &flags[0], flags[1]);
	macMgmtUpdateContext(flags[0], connection->channel);
	macMgmtCheckContext();
	count = (uint8_t)romStringLength(rates, 14);
	macMgmtQueryContext(connection, rates, count,
			    wlanRateBitmapAll(rates, count, 0, 0,
					      (const WlanHtCapabilityIe *)connection->htCapability));
	macTxNotifyApplyFrame(connection);
	wlanTxSetSharedRadio(2);
	macTxNotifyHandlePacket();
	romLibUpdateContext(&timing);
	timing.interval = connection->beaconPeriod;
	timing.next[1] = 0;
	timing.dtimPeriod = connection->dtimPeriod;
	timing.next[0] = connection->beaconPeriod << 11;
	timing.timestamp[0] = 0;
	timing.timestamp[1] = 0;
	wlanBssRecordTiming(4, &timing);
	MAC_REG(0x824) |= 4;
	MAC_REG(0x300) |= 0x2000;
	SOC_MAC_EVENT_CONTROL |= 0x20;
	connection->bssCapability[0] = 0x31;
	connection->bssCapability[1] = 0x04;

	buffer = connection->beaconBuffer;
	wlanIbssBeaconBuffer = buffer;
	buffer->dataOffset = 176;
	wlanIbssBeaconFrame = (WlanWirelessFrame *)((uint8_t *)buffer + 176);
	macMgmtProcessEntry(connection, WLAN_FRAME_BEACON, buffer, wlanIbssBeaconRequest);
	buffer = connection->probeResponseBuffer;
	wlanIbssProbeResponseBuffer = buffer;
	buffer->dataOffset = 176;
	wlanIbssProbeResponseFrame = (WlanWirelessFrame *)((uint8_t *)buffer + 176);
	macMgmtProcessEntry(connection, WLAN_FRAME_PROBE_RESPONSE, buffer, connection->probeResponseRequest);
	wlanPsFilterFrame(connection->beaconPeriod);
	wlanPsVerifyFrame(3);
	wlanMacFilterMasksUpdate();
	uapSqramConfigureState(wlanConnectionIncrement());
	wlanConnectionCommandCheck(12, 0);
	wlanConnectionCommandPrepare(12, 1);
	btampKeyMgmtInit();
	if ((wlanChannelContext.savedFlags & 3) == 0)
		btLmpLcHandleState(macMgmtQueryState(wlanChannelContext.savedFlags,
						     wlanChannelContext.savedChannel));
}

/* 0x36420: the elements the host's TLVs became are kept. */
__attribute__((noinline)) void macMgmtConfigureEntry(uint8_t *capability, uint8_t *information, uint8_t *coexistence,
			   uint8_t *obssScan, uint8_t *reserved)
{
	wlanIbssTlvElements.capability = capability;
	wlanIbssTlvElements.information = information;
	wlanIbssTlvElements.coexistence = coexistence;
	wlanIbssTlvElements.obssScan = obssScan;
	wlanIbssTlvElements.reserved16 = reserved;
}

/* 0x3642a: a TLV of the host's start or join request: the HT elements, else the rest. */
int macMgmtCheckEntry(struct WlanConnection *connection, uint8_t *tlv, uint8_t *channels)
{
	if (wlanHtTlvApply(&wlanIbssTlvElements, &wlanIbssIeBuffer, tlv) == 0)
		return wlanHostTlvResolve(&wlanIbssIeBuffer, tlv, channels);
	connection->flags |= 0x40;
	if (connection->htCapability[2] & 2)	/* HT Capabilities: 40 MHz */
		connection->flags |= 0x80;
	return connection->flags;
}

/* 0x36464: the host's request is answered with a failure and what it gave is dropped. */
OsaBuffer *macMgmtValidateEntry(struct WlanConnection *connection, const uint8_t *request,
				unsigned int command)
{
	OsaBuffer *message;

	wlanIeBufferFree(&wlanIbssIeBuffer);
	macMgmtConfigureEntry(NULL, NULL, NULL, NULL, NULL);
	message = wlanHostMessageAlloc(connection);
	if (message != NULL) {
		wlanHostResponseBuild(message, command, 1, request[4]);
		wlanHostMessagePost(message);
	}
	return message;
}

/* 0x3649e: the MAC is quiesced around the flush, unless it already is. */
void macMgmtPrepareEntry(void)
{
	unsigned int pending = MAC_REG(0x2d4) & 8;

	if (pending == 0)
		romLibInspectState(8);
	wlanMacFlush();
	if (pending == 0)
		romLibTrackState(8);
}

/* 0x364be: the security a beacon's or the host's RSN or WPA element offers. */
__attribute__((noinline)) void macMgmtDispatchEntry(const uint8_t *rsn, const uint8_t *wpa, uint8_t *ucast, uint8_t *mcast,
			  SecurityMode_t *mode, int unused, int skipHeader)
{
	SecurityMode_t modeBuffer;
	AkmSuite_t akm[5];

	if (rsn == NULL && wpa == NULL)
		return;
	if (mode == NULL)
		mode = &modeBuffer;
	*ucast = 0;
	*mcast = 0;
	((uint8_t *)mode)[0] = 0;
	((uint8_t *)mode)[1] = 0;
	if (rsn != NULL) {
		if (skipHeader)
			rsn += 2;
		supplicantParseRsnIe((IEEEtypes_RSNElement_t *)rsn, &modeBuffer,
				     (Cipher_t *)mcast, (Cipher_t *)ucast, akm, 5, NULL, NULL);
	} else if (wpa != NULL) {
		if (skipHeader)
			wpa += 2;
		supplicantParseWpaIe((IEEEtypes_WPAElement_t *)wpa, &modeBuffer,
				     (Cipher_t *)mcast, (Cipher_t *)ucast, akm, 5);
	}
}

/* 0x3652a: the host's WPA and RSN elements are kept and their security is worked out. */
__attribute__((noinline)) void macMgmtParseEntry(int unused, const uint8_t *wpa, const uint8_t *rsn, struct WlanIeBuffer *ies,
		       uint8_t *present, uint8_t *mcast, uint8_t *ucast)
{
	SecurityMode_t mode;
	AkmSuite_t akm[5];

	if (rsn == NULL && wpa == NULL)
		return;
	*present = 1;
	if (rsn != NULL)
		wlanIeBufferAppendTlv(ies, rsn);
	if (wpa != NULL)
		wlanIeBufferAppendTlv(ies, wpa);
	if (wlanSecurityQueryHook() != 0) {
		macMgmtDispatchEntry(rsn, wpa, ucast, mcast, &mode, (int)(intptr_t)akm, 1);
		wlanSecurityRecordHook(mode, (Cipher_t *)mcast, (Cipher_t *)ucast, akm, NULL, NULL);
	} else {
		macMgmtDispatchEntry(rsn, wpa, ucast, mcast, NULL, 0, 1);
	}
}

/* 0x3691c: the IBSS the host started is up: the supplicant's keys, the state and the answer to the host. */
int macMgmtQueryEntry(struct WlanConnection *connection, const struct WlanHostCompletion *completion)
{
	OsaBuffer *message;
	int result;

	wlanIeBufferFree(&wlanIbssIeBuffer);
	if (completion->status != 0) {
		result = -1;
	} else {
		wlanPsPhaseSuspend();
		macMgmtResolveState(connection, (connection->flags & 4) != 0 && supplicantIsEnabled() == 0);
		supplicantInstallWpaNoneKeys(connection);
		connection->flags2 |= 0x10;
		connection->state = 6;
		if (wlanSessionId != 0)
			wlanPsStartHostModeTimer(connection);
		result = 0;
	}
	message = wlanHostMessageAlloc(connection);
	if (message != NULL) {
		wlanHostResponseBuild(message, 0x802b, completion->status, completion->sequence);
		wlanHostMessagePost(message);
	}
	return result;
}

/* 0x3699a: the same for the IBSS the host joined, which is dropped when it did not work. */
int macMgmtResetEntry(struct WlanConnection *connection, const struct WlanHostCompletion *completion)
{
	OsaBuffer *message;
	int result = -1;

	wlanIeBufferFree(&wlanIbssIeBuffer);
	if (completion->status != 0) {
		macMgmtScheduleState(connection);
	} else {
		wlanPsPhaseSuspend();
		macMgmtResolveState(connection, (connection->flags & 4) != 0 && supplicantIsEnabled() == 0);
		supplicantInstallWpaNoneKeys(connection);
		connection->flags2 |= 0x10;
		connection->state = 6;
		if (wlanSessionId != 0)
			wlanPsStartHostModeTimer(connection);
		result = 0;
	}
	message = wlanHostMessageAlloc(connection);
	if (message != NULL) {
		wlanHostResponseBuild(message, 0x802c, completion->status, completion->sequence);
		wlanHostMessagePost(message);
	}
	return result;
}

/* 0x36a1c: the security elements go to the supplicant. */
void macMgmtSyncEntry(void)
{
	wlanSecurityQueryHook = supplicantIsEnabled;
	wlanSecurityRecordHook = supplicantSetAssocRsn;
}

/* 0x36a2a */
int macMgmtApplyEntry(void)
{
	return wlanIbssPowerLimit;
}

/* 0x36a32: the transmit power of the connection is updated; the IBSS is held to its limit when it has one. */
void macMgmtResolveEntry(struct WlanConnection *connection, unsigned int channel, int limited)
{
	wlanTxPowerUpdateConnection(connection, channel, 1);
	wlanIbssPowerLimit = limited ? connection->txPowerLimit : 255;
}

/* 0x36a56: the IBSS is left; the host is told. */
void macMgmtScheduleEntry(struct WlanConnection *connection)
{
	OsaBuffer *message;

	macMgmtScheduleState(connection);
	message = wlanHostMessageAlloc(connection);
	if (message != NULL) {
		wlanHostResponse8040Build(message, 0);
		wlanHostMessagePost(message);
	}
}

/* 0x36a78: the host's request to stop the IBSS goes to the state machine; if it cannot, the IBSS is dropped here. */
void macMgmtNotifyEntry(struct WlanConnection *connection)
{
	OsaBuffer *message = macMgmtHandleState(connection);
	int sent = 0;

	if (message != NULL) {
		((uint8_t *)message)[message->dataOffset + offsetof(OsaBuffer, allocationMetadata)] = WLAN_STA_EVENT_STOP;
		if (macMgmtProcessState(message) == 1)
			osa_BufferFree(message);
		else
			sent = 1;
	}
	if (!sent) {
		macMgmtSetupContext(connection);
		macMgmtScheduleEntry(connection);
	}
}

/*
 * The TLVs after the fixed part of an AD_HOC_START or AD_HOC_JOIN command: the WPA and RSN
 * elements are kept for macMgmtParseEntry(), the elements it passes on go into the IBSS's
 * element buffer and the HT ones are checked. Join takes the vendor and the HT TLVs
 * differently than start does.
 */
struct WlanIbssTlvs {
	const uint8_t *wpa;
	const uint8_t *rsn;
};

static inline __attribute__((always_inline)) void wlanIbssParseTlvs(struct WlanConnection *connection, const uint8_t *cursor, unsigned int length,
			      uint8_t *channels, int start, struct WlanIbssTlvs *found)
{
	for (;;) {
		uint8_t *tlv = (uint8_t *)cursor;
		unsigned int size;
		unsigned int type;

		if ((uint16_t)length < 4)
			break;
		length = (uint16_t)(length - 4);
		size = tlv[2] | (tlv[3] << 8);
		if (size > length)
			break;
		type = tlv[0] | (tlv[1] << 8);
		if (type == 221) {
			if (wlanWpaElementType(tlv + 4) == WLAN_VENDOR_WPA)
				found->wpa = tlv;
			else
				wlanIeBufferAppendTlv(&wlanIbssIeBuffer, tlv);
			if (start)
				macMgmtCheckEntry(connection, tlv, channels);
		} else if (type <= 221) {
			switch (type) {
			case 33:
				break;
			case 48:
				found->rsn = tlv;
				break;
			case 127:
				wlanIeBufferAppendTlv(&wlanIbssIeBuffer, tlv);
				break;
			default:
				macMgmtCheckEntry(connection, tlv, channels);
				break;
			}
		} else if (type == 335) {
			tlv[0] = 127;	/* the host's TLV 335 is the Extended Capabilities element */
			tlv[1] = 0;
			wlanIeBufferAppendTlv(&wlanIbssIeBuffer, tlv);
		} else if (type == 448) {
			if (start ? tlv[4] == 1 : tlv[4] != 0)
				connection->stationFlags |= 0x80;
			else
				connection->stationFlags &= ~0x80u;
		} else {
			macMgmtCheckEntry(connection, tlv, channels);
		}
		length = (uint16_t)length - size;
		cursor = tlv + size + 4;
	}
}

/* The message the state machine is sent for the host's start or join request: the event, the command's header and the parameters. */
struct __attribute__((packed)) WlanIbssStartMessage {
	uint8_t event;			/* WLAN_STA_EVENT_START */
	uint8_t command[8];
	WlanAdHocStart start;
};

struct __attribute__((packed)) WlanIbssJoinMessage {
	uint8_t event;			/* WLAN_STA_EVENT_JOIN */
	uint8_t command[8];
	WlanAdHocBssDesc join;
};

/* 0x365aa: HostCmd_CMD_802_11_AD_HOC_START: the request, with its elements, goes to the state machine. */
OsaBuffer *hostCmd002bStart(struct WlanConnection *connection, uint8_t *command)
{
	struct WlanIbssTlvs found = { NULL, NULL };
	struct WlanIbssStartMessage *message;
	OsaBuffer *buffer;
	uint16_t tlvLength = 0;
	uint8_t *extraIes;
	uint8_t present = 0;
	int failed = 0;

	macMgmtPrepareEntry();
	if (connection->state != 0 || romStringLength(command + 8, 32) == 0)
		failed = 1;
	macMgmtConfigureEntry(connection->htCapability, wlanHtOperationIe, wlanHtCoexIe, wlanHtOverlapScanIe,
			      NULL);
	if (failed)
		return macMgmtValidateEntry(connection, command, 0x802b);
	wlanIeBufferAlloc(&wlanIbssIeBuffer);
	wlanIbssPowerLimit = 255;
	buffer = macMgmtHandleState(connection);
	if (buffer == NULL)
		return macMgmtValidateEntry(connection, command, 0x802b);
	message = (struct WlanIbssStartMessage *)((uint8_t *)buffer + buffer->dataOffset);
	wlanAdHocStartParse(&message->start, command, wlanLocalState->rates, &tlvLength,
			    connection->beaconPeriod);
	message->event = WLAN_STA_EVENT_START;
	__aeabi_memcpy(message->command, command, 8);
	wlanIbssParseTlvs(connection, command + 77, tlvLength, message->start.reserved2, 1, &found);
	macMgmtParseEntry(0, found.wpa, found.rsn, &wlanIbssIeBuffer, &present, &connection->securitySuites[0],
			  &connection->securitySuites[1]);
	connection->flags = (connection->flags & 0xfb) | ((4 * present) & 7);
	message->start.extraIesLength = wlanIeBufferGet(&wlanIbssIeBuffer, &extraIes);
	message->start.extraIes = extraIes;
	if (macMgmtProcessState(buffer) == 1) {
		osa_BufferFree(buffer);
		return macMgmtValidateEntry(connection, command, 0x802b);
	}
	return NULL;
}

/* 0x3675e: HostCmd_CMD_802_11_AD_HOC_JOIN, the same for an IBSS to join. */
OsaBuffer *hostCmd002cJoin(struct WlanConnection *connection, uint8_t *command)
{
	struct WlanIbssTlvs found = { NULL, NULL };
	struct WlanIbssJoinMessage *message;
	OsaBuffer *buffer;
	uint16_t tlvLength = 0;
	uint8_t *extraIes;
	uint8_t present = 0;

	macMgmtPrepareEntry();
	macMgmtConfigureEntry(connection->htCapability, wlanHtOperationIe, wlanHtCoexIe, wlanHtOverlapScanIe,
			      NULL);
	if (connection->state != 0)
		return macMgmtValidateEntry(connection, command, 0x802c);
	wlanIeBufferAlloc(&wlanIbssIeBuffer);
	buffer = macMgmtHandleState(connection);
	if (buffer == NULL)
		return macMgmtValidateEntry(connection, command, 0x802c);
	message = (struct WlanIbssJoinMessage *)((uint8_t *)buffer + buffer->dataOffset);
	wlanAdHocJoinParse(&message->join, command, &tlvLength);
	wlanIbssPowerLimit = 255;
	message->event = WLAN_STA_EVENT_JOIN;
	wlanIbssParseTlvs(connection, command + 101, tlvLength, message->join.reserved, 0, &found);
	macMgmtParseEntry(0, found.wpa, found.rsn, &wlanIbssIeBuffer, &present, &connection->securitySuites[0],
			  &connection->securitySuites[1]);
	connection->flags = (connection->flags & 0xfb) | ((4 * present) & 7);
	message->join.extraIesLength = wlanIeBufferGet(&wlanIbssIeBuffer, &extraIes);
	message->join.extraIes = extraIes;
	if (macMgmtProcessState(buffer) == 1) {
		osa_BufferFree(buffer);
		return macMgmtValidateEntry(connection, command, 0x802c);
	}
	connection->state = 2;
	return NULL;
}
