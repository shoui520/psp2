/*
 * The beacon and probe response templates of an access point: built from the BSS's settings (SSID,
 * rates, channel, TIM, country, ERP, HT, RSN/WPA, WAPI, WMM and P2P elements) in the buffers the
 * MAC sends them from, and kept up to date as the BSS changes (the ERP flags, the HT operation
 * element, the capability field and single elements). Marvell published no source for it; the
 * names are the firmware's own placeholder names, kept, and the file name is inferred.
 *
 * A template's transmit request (WlanMacTxRequest) says where in the frame the CF Parameter Set and
 * the TIM element start and how long the frame is; the frame is the buffer's data, behind the
 * transmit data, where the data offset points.
 */
#include <stddef.h>
#include "robin/p2pgo.h"
#include "robin/adhoc.h"
#include "robin/apsta.h"
#include "robin/conn.h"
#include "robin/domain.h"
#include "robin/ie.h"
#include "robin/keyMgmtAp.h"
#include "robin/macentry.h"
#include "robin/mef.h"
#include "robin/mgmt.h"
#include "robin/protection.h"
#include "robin/ratectl.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/sys.h"
#include "robin/txpower.h"
#include "robin/txring.h"
#include "robin/wlan.h"
#include "robin/wapiie.h"
#include "robin/scan.h"

#define U16(bytes)		((uint16_t)((bytes)[0] | (bytes)[1] << 8))
#define PUT_U16(bytes, value)	((bytes)[0] = (uint8_t)(value), (bytes)[1] = (uint8_t)((value) >> 8))

#define WLAN_TEMPLATE_FRAME_OFFSET	176	/* the data offset of a template's buffer: past the transmit data */
#define WLAN_TEMPLATE_BODY_FIXED	12	/* timestamp, beacon interval and capability */
#define WLAN_IE_ERP			42
#define WLAN_IE_HT_OPERATION		61

/* The frame a template's transmit request points to. */
static inline WlanWirelessFrame *templateFrame(const struct WlanMacTxRequest *request)
{
	return (WlanWirelessFrame *)__aeabi_uread4(request->frame);
}

/* The template's header, from the SSID on: the SSID element is empty in a beacon of a hidden one. */
uint8_t *fmCoexSelectEntry(OsaBuffer *buffer, const uint8_t *source, const uint8_t *bssid,
			   unsigned int subtype, int broadcastSsid, const uint8_t *ssid)
{
	WlanWirelessFrame *frame = (WlanWirelessFrame *)((uint8_t *)buffer + buffer->dataOffset);
	uint8_t *cursor = frame->body + WLAN_TEMPLATE_BODY_FIXED;

	wlanBuildMgmtHeader(frame, source, wlanScanAnyBssid, bssid, subtype, 0);
	if (subtype != WLAN_FRAME_BEACON || broadcastSsid != 0)
		return cursor + wlanPutSsidIe(ssid, cursor);
	frame->body[12] = 0;
	frame->body[13] = 0;
	return cursor + 2;
}

/* The supported rates element and, unless the BSS is 802.11b only, the DS parameter set. */
uint8_t *fmCoexAdjustEntry(uint8_t *cursor, const uint8_t *rates, int dsParameter, uint8_t channel)
{
	uint8_t *end = cursor + wlanPutRatesIe(rates, cursor, 0);

	if (dsParameter != 0)
		end += wlanPutDsParameterIe(end, channel);
	return end;
}

/*
 * A TIM element with no traffic bits for as many stations as the BSS admits (one bit each),
 * kept in copy too (the beacon's TIM is rebuilt from it); returns the end of the element.
 */
uint8_t *fmCoexRecordEntry(uint8_t *tim, unsigned int stations, uint8_t dtimPeriod, uint8_t *copy)
{
	unsigned int bytes = stations >> 3;
	unsigned int i;

	tim[0] = 5;
	tim[3] = dtimPeriod;
	tim[2] = 0;
	tim[4] = 0;
	if (stations & 7)
		bytes++;
	tim[1] = bytes + 3;
	for (i = 0; i < bytes; i++)
		tim[5 + i] = 0;
	if (copy != NULL)
		__aeabi_memcpy(copy, tim, tim[1] + 2);
	return &tim[tim[1] + 2];
}

/* The request is made to send the template: the frame, where the TIM starts (a beacon) and its length. */
void fmCoexFilterEntry(struct WlanMacTxRequest *request, unsigned int subtype, WlanWirelessFrame *frame,
		       uint8_t *tim)
{
	unsigned int length;

	__aeabi_uwrite4((uint32_t)frame, request->frame);
	if (subtype == WLAN_FRAME_BEACON)
		request->timOffset = (uint8_t)((uintptr_t)tim - (uintptr_t)frame - 8);
	length = U16(frame->bodyLength) + 28;
	request->templateLength[0] = (uint8_t)length;
	request->templateLength[1] = length >> 8;
	__aeabi_memcpy(frame->address1, wlanScanAnyBssid, 6);
}

/* The ERP element of a template says what the BSS's rate flags do. */
void fmCoexHandleTable(struct WlanMacTxRequest *request, int nonErp, int protection, int barker)
{
	WlanWirelessFrame *frame;
	uint8_t *erp;

	if (request == NULL)
		return;
	frame = templateFrame(request);
	if (frame == NULL)
		return;
	erp = (uint8_t *)wlanIeFind(WLAN_IE_ERP, frame->body + WLAN_TEMPLATE_BODY_FIXED,
					  U16(frame->bodyLength) - WLAN_TEMPLATE_BODY_FIXED);
	if (erp != NULL)
		erp[2] = (protection << 1) | nonErp | (barker << 2);
}

/* The HT operation element of a template takes the BSS's operating mode, non-greenfield and OBSS bits. */
void fmCoexProcessTable(struct WlanMacTxRequest *request, const uint8_t *htOperation)
{
	WlanWirelessFrame *frame = templateFrame(request);
	uint8_t *element = (uint8_t *)wlanIeFind(WLAN_IE_HT_OPERATION, frame->body + WLAN_TEMPLATE_BODY_FIXED,
						       (uint16_t)(U16(frame->bodyLength) - WLAN_TEMPLATE_BODY_FIXED));
	unsigned int value;

	if (element == NULL)
		return;
	value = (element[4] & ~3u) | (htOperation[4] & 3);
	element[4] = value;
	value = (value & 0xfb) | (((htOperation[4] >> 2) & 1) << 2);
	element[4] = value;
	element[4] = (value & 0xef) | (((htOperation[4] & 0x10) != 0) << 4);
}

/*
 * Builds the beacon (subtype 8) or probe response (subtype 5) template of the BSS: the elements in
 * the order the BSS sends them, the transmit request's flags, rate and length, and the P2P elements
 * a group owner's or client's connection adds. The first call for a beacon also gives the TIM
 * element its place (WlanConnectionInfo.tim, timPosition).
 */
void macMgmtScheduleFrame(struct WlanConnection *connection, unsigned int subtype)
{
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);
	struct WlanMacTxRequest *request;
	OsaBuffer *buffer;
	struct WlanReplyFrame *transmit;
	WlanWirelessFrame *frame;
	uint8_t *cursor;
	uint32_t value = 0;
	unsigned int rate;
	uint32_t word;

	wlanConnectionInfoSlot(connection);
	if (wlanP2pConnection(connection) != 0)
		rate = 5;
	else
		rate = wlanTxControlRate(connection);
	if (subtype == WLAN_FRAME_BEACON) {
		request = connection->beaconRequest;
		buffer = connection->beaconBuffer;
	} else if (subtype == WLAN_FRAME_PROBE_RESPONSE) {
		request = connection->probeResponseRequest;
		buffer = connection->probeResponseBuffer;
	} else {
		return;
	}
	if (buffer == NULL || request == NULL)
		return;
	frame = (WlanWirelessFrame *)((uint8_t *)buffer + WLAN_TEMPLATE_FRAME_OFFSET);
	transmit = (struct WlanReplyFrame *)buffer->data;
	if (subtype == WLAN_FRAME_BEACON)
		buffer->dataOffset = WLAN_TEMPLATE_FRAME_OFFSET;
	cursor = fmCoexSelectEntry(buffer, connection->localAddress, connection->localAddress, subtype,
				   (info->protectionFlags >> 1) & 1, connection->networkSsid);
	word = connection->beaconPeriod;
	PUT_U16(frame->body + 8, word);
	frame->body[10] = info->capability[0];
	frame->body[11] = info->capability[1];
	cursor = fmCoexAdjustEntry(cursor, info->rates, info->phyMode != 2, connection->channel);
	if (subtype == WLAN_FRAME_BEACON) {
		unsigned int stations = 1;

		info->timPosition = cursor;
		if (info->maxStations > 1)
			stations = info->maxStations;
		cursor = fmCoexRecordEntry(cursor, stations, connection->dtimPeriod, info->tim);
	}
	if (wlanDomainApplies(connection))
		cursor += wlanDomainBuildIe(connection, cursor);
	if (info->phyMode == 1)
		cursor += wlanPutErpIe(cursor, (info->rateFlags & WLAN_RATE_FLAG_ERP) != 0,
				       (info->rateFlags & WLAN_RATE_FLAG_LOW_NULL) != 0,
				       (info->rateFlags & WLAN_RATE_FLAG_PROTECTION) != 0);
	if (info->htEnabled == 1)
		cursor += fmCoexSyncTable(cursor, info->htCapability);
	if (info->securityMode.wpa2)
		cursor += keyMgmtAp_FormatWPARSN_IE((IEEEtypes_InfoElementHdr_t *)cursor, 1,
						    &info->apRsn.wpa2UcstCipher, info->apRsn.wpa2UcstCipherCount,
						    &info->apRsn.mcstCipher, info->apRsn.AuthKey,
						    info->apRsn.AuthKeyCount);
	cursor += wlanPutRatesIe(info->rates, cursor, 1);
	if (info->htEnabled == 1)
		cursor += fmCoexApplyTable(cursor, info->htOperation);
	if (info->securityMode.wapi)
		cursor += fmCoexSelectBuffer(info, cursor);
	if (info->htEnabled == 1 && (info->htCapability[2] & 0x02)) {
		if (info->phyMode != 2) {
			__aeabi_memcpy(cursor, &info->obssScan, 16);
			cursor += 16;
		}
		__aeabi_memcpy(cursor, &info->extCapability, 6);
		cursor += 6;
	}
	if (info->securityMode.wpa)
		cursor += keyMgmtAp_FormatWPARSN_IE((IEEEtypes_InfoElementHdr_t *)cursor, 0,
						    &info->apRsn.wpaUcstCipher, info->apRsn.wpaUcstCipherCount,
						    &info->apRsn.mcstCipher, info->apRsn.AuthKey,
						    info->apRsn.AuthKeyCount);
	if (info->qosEnabled == 1)
		cursor += fmCoexResolveTable(cursor, info->wmmElement);
	if (subtype == WLAN_FRAME_BEACON) {
		wlanBeaconTemplateEnd = cursor;
		value = 256;
	} else {
		wlanProbeTemplateEnd = cursor;
	}
	wlanTxRequestReset(request);
	__aeabi_uwrite4(value, request->flags);
	transmit->descriptor.header.wire.packetLength[0] = rate;
	word = __aeabi_uread4(request->flags);
	__aeabi_uwrite4(word & 0xffffefff, request->flags);
	__aeabi_uwrite4((uint32_t)wlanRateTableLookup(rate), request->rateInfo);
	PUT_U16(frame->bodyLength, (uint16_t)((uintptr_t)cursor - (uintptr_t)frame - 32));
	wlanCustomIeApply(connection, 1, frame, 12);
	if (connection->interfaceType == 3 && connection->mode != 0) {
		if (subtype == WLAN_FRAME_PROBE_RESPONSE)
			p2pDirectAdjustState(connection, frame);
		else
			p2pDirectVerifyState(connection, frame);
		if (connection->mode == 2 && subtype == WLAN_FRAME_BEACON)
			p2pDirectValidateContext(connection, frame);
	}
	fmCoexFilterEntry(request, subtype, frame, info->timPosition);
}

/* The ERP element of a template follows the BSS's rate flags. */
void macMgmtNotifyFrame(struct WlanConnection *connection, struct WlanMacTxRequest *request)
{
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);

	fmCoexHandleTable(request, (info->rateFlags & WLAN_RATE_FLAG_ERP) != 0,
			  (info->rateFlags & WLAN_RATE_FLAG_LOW_NULL) != 0,
			  (info->rateFlags & WLAN_RATE_FLAG_PROTECTION) != 0);
}

void macMgmtFormatFrame(struct WlanConnection *connection)
{
	wlanConnectionInfo(connection);
	macMgmtNotifyFrame(connection, connection->beaconRequest);
	macMgmtNotifyFrame(connection, connection->probeResponseRequest);
}

void macMgmtInspectFrame(struct WlanConnection *connection)
{
	wlanConnectionInfo(connection);
	macMgmtFormatFrame(connection);
}

/*
 * A template's HT operation element takes the BSS's: RIFS is not allowed in operating modes 1 and 3,
 * and the channel width and secondary channel offset follow.
 */
void macMgmtTrackFrame(struct WlanMacTxRequest *request, uint8_t *htOperation)
{
	WlanWirelessFrame *frame = templateFrame(request);
	unsigned int posture = sysIrqDisable();
	uint8_t *element = (uint8_t *)wlanIeFind(WLAN_IE_HT_OPERATION, frame->body + WLAN_TEMPLATE_BODY_FIXED,
						 (uint16_t)(U16(frame->bodyLength) - WLAN_TEMPLATE_BODY_FIXED));

	if (element != NULL) {
		unsigned int mode = htOperation[4];
		unsigned int value;

		if (~mode << 30 == 0 || (mode & 3) == 1) {
			htOperation[3] &= ~8u;
			element[3] &= ~8u;
		} else {
			htOperation[3] |= 8;
			element[3] |= 8;
		}
		value = (element[3] & 0xfb) | (((htOperation[3] >> 2) & 1) << 2);
		element[3] = value;
		element[3] = ((value >> 2) << 2) | (htOperation[3] & 3);
	}
	sysIrqRestore(posture);
}

void macMgmtManageFrame(struct WlanConnection *connection, struct WlanMacTxRequest *request)
{
	uint8_t *htOperation = wlanConnectionInfo(connection)->htOperation;

	fmCoexProcessTable(request, htOperation);
	macMgmtTrackFrame(request, htOperation);
}

void macMgmtAcquireFrame(struct WlanConnection *connection)
{
	wlanConnectionInfo(connection);
	macMgmtManageFrame(connection, connection->beaconRequest);
	macMgmtManageFrame(connection, connection->probeResponseRequest);
}

/* The HT operation element of both templates follows the BSS's. */
void macMgmtReleaseFrame(struct WlanConnection *connection)
{
	wlanConnectionInfo(connection);
	macMgmtAcquireFrame(connection);
}

/* The capability field of both templates. */
void macMgmtSetupFrame(uint16_t capability, struct WlanMacTxRequest *beacon, struct WlanMacTxRequest *probe)
{
	WlanWirelessFrame *frame = templateFrame(beacon);

	PUT_U16(frame->body + 10, capability);
	frame = templateFrame(probe);
	PUT_U16(frame->body + 10, capability);
}

void macMgmtClearFrame(struct WlanConnection *connection)
{
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);

	macMgmtSetupFrame((uint16_t)(info->capability[0] | info->capability[1] << 8), connection->beaconRequest,
			  connection->probeResponseRequest);
}

void macMgmtConvertFrame(struct WlanConnection *connection)
{
	wlanConnectionInfo(connection);
	macMgmtClearFrame(connection);
}

/* An element is replaced in both templates. */
void macMgmtVerifyFrame(const uint8_t *ie, struct WlanMacTxRequest *beacon, struct WlanMacTxRequest *probe)
{
	wlanTemplateSetIe(WLAN_FRAME_BEACON, templateFrame(beacon), beacon, ie);
	wlanTemplateSetIe(WLAN_FRAME_PROBE_RESPONSE, templateFrame(probe), probe, ie);
}

/* The TIM element of the beacon is rebuilt in its place. */
void macMgmtSelectFrame(struct WlanConnection *connection)
{
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);

	__aeabi_memcpy(info->timPosition, info->tim, info->tim[1] + 2);
}

/* The hooks of the template code: the TIM element is moved, an element is replaced in the templates. */
void macMgmtAdjustFrame(struct WlanConnection *connection)
{
	wlanConnectionInfo(connection);
	macMgmtSelectFrame(connection);
}

void macMgmtRecordFrame(struct WlanConnection *connection)
{
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);

	macMgmtVerifyFrame(info->wmmElement, connection->beaconRequest, connection->probeResponseRequest);
}

void macMgmtFilterFrame(struct WlanConnection *connection)
{
	wlanConnectionInfo(connection);
	macMgmtRecordFrame(connection);
}

void macMgmtHandleHeader(void)
{
	wlanSqramFrameHook = macMgmtFilterFrame;
	wlanHtOperationHook = macMgmtReleaseFrame;
	wlanApTemplateHook = macMgmtAdjustFrame;
}
