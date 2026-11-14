/*
 * Management side of a connection (0xfdfe..0x1069c): events and host
 * commands posted to the connection's state machines, the channel the radio
 * is tuned to and the transmit power limits that follow it, the management
 * frames the firmware builds itself (deauthentication, disassociation), the
 * events reported to the host, and the custom information elements the host
 * adds to the frames of each interface (mlan's HostCmd_CMD_MGMT_IE_LIST).
 * No Marvell source names these; the names say what the code does.
 */
#include <stddef.h>
#include "robin/adhoc.h"
#include "robin/btrom.h"
#include "robin/calibration.h"
#include "robin/conn.h"
#include "robin/mac.h"
#include "robin/mactx.h"
#include "robin/mef.h"
#include "robin/mgmt.h"
#include "robin/osa.h"
#include "robin/rf.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/txring.h"
#include "robin/wlan.h"
#include "robin/wlantask.h"

#define LE16(p)			((uint16_t)((p)[0] | (p)[1] << 8))

/* Host commands that drive the association state machine (WlanConnectionInfo.smState). */
#define WLAN_COMMAND_TRACK_BUFFER	35
/* Management subtypes built here. */
#define WLAN_SUBTYPE_PROBE_REQUEST	4
#define WLAN_SUBTYPE_PROBE_RESPONSE	5
#define WLAN_SUBTYPE_DISASSOC		10
#define WLAN_SUBTYPE_DEAUTH		12
#define WLAN_SUBTYPE_ACTION		13
/* WlanTxDescriptor.header.wire.packetLength[0] of a management frame: how it is sent. */
#define WLAN_MGMT_SEND_DIRECT		0
#define WLAN_MGMT_SEND_NO_PS		5

/* Slot times, microseconds. */
#define WLAN_SLOT_TIME_SHORT		9
#define WLAN_SLOT_TIME_LONG		20

#define WLAN_TX_POWER_NONE		255
#define WLAN_CHANNEL_SWITCH_TIMEOUT	20000
#define WLAN_MGMT_FRAME_SIZE		0x15c

#define WLAN_CUSTOM_IE_SLOTS		4
#define WLAN_CUSTOM_IE_SIZE		256
#define WLAN_CUSTOM_IE_ALL_FRAMES	0xffff	/* mask: also built into the beacon */
#define WLAN_IE_CHANNEL_SWITCH		37
/* What wlanBssBit() returns: one bit per BSS, eight per interface type. */
#define WLAN_BSS_BIT(type, number)	((1u << (number)) << (8 * (type)))

/* A message posted to a connection's state machine (20 bytes). */
struct WlanSmMessage {
	uint32_t event;
	uint32_t reserved4[2];
	uint8_t value;
	uint8_t reserved13[7];
};

/* One custom IE as the host sends it, after its slot index. */
struct __attribute__((packed)) WlanCustomIe {
	uint16_t mask;			/* management subtypes it goes into */
	uint16_t length;
	uint8_t buffer[WLAN_CUSTOM_IE_SIZE];
};

/* The custom IEs and, per slot, the interfaces using it (wlanBssBit()). */
struct __attribute__((packed)) WlanCustomIeTable {
	uint32_t owners[WLAN_CUSTOM_IE_SLOTS];
	struct WlanCustomIe entries[WLAN_CUSTOM_IE_SLOTS];
};

/* The channel power table (0x574ec): transmit power per listed channel. */
struct WlanChannelPowerTable {
	uint8_t reserved0[3];
	uint8_t count;
	uint8_t channels[64];
	uint8_t power[64];
};

/* Not converted yet; provisional names. */
int wlanEventMessageBuild(struct WlanSmMessage *message, int code, const void *data); /* 0x2a49a */
void macMgmtTrackBuffer(struct WlanConnection *connection, unsigned int value);	/* 0x38a18 */
void macMgmtPrepareContext(uint32_t *flags, void *unused);			/* 0x3566a */
void macTxNotifyConvertTable(int table);					/* 0x31dfe */
unsigned int p2pDirectDispatchTable(unsigned int queue);			/* 0x42030 */
void rfCalibScheduleDescriptor(const uint8_t *local, const uint8_t *peer);	/* 0x2a25c */
void rfCalibResolveDescriptor(void);						/* 0x2a254 */
int rfCalibNotifyDescriptor(int value);						/* 0x2a2e6 */
void rfCalibInspectDescriptor(int value);					/* 0x2a3ca */
int cbProcPrepareTable(int table, int value);					/* 0x1cdc8 */
void cbProcDispatchTable(int table, int value);					/* 0x1ce46 */
int macMgmtTrackHeader(unsigned int channel, unsigned int band, unsigned int rate); /* 0x3b784 */
int macMgmtInspectHeader(unsigned int channel, unsigned int band, unsigned int rate); /* 0x3b73e */
void macMgmtResetHeader(int unused, unsigned int channel);			/* 0x3b4b6 */
int cbProcResetPacket(void);							/* 0x1e42e */
int macTxNotifyScheduleState(unsigned int channel, uint8_t *index);		/* 0x2ff3c */
int rfCalibTrackDescriptor(const uint8_t *config, uint8_t *bandwidth);		/* 0x2a3ec */
int macTxNotifySetupTable(uint32_t timeout, uint32_t *status);			/* 0x31dd2 */
void macTxNotifyClearTable(void);						/* 0x31de8 */
unsigned int wlanPsProcessFrame(struct WlanConnection *connection, unsigned int subtype,
				int broadcast);					/* 0x3d3f0 */
int p2pDirectTrackContext(const WlanWirelessFrame *frame);			/* 0x40dd6 */
uint32_t wlanBssBit(unsigned int interfaceType, unsigned int bssNumber);	/* 0x10d10 */
struct WlanPsPeerState *wlanConnectionPsState(struct WlanConnection *connection); /* 0x1d1f6 */
int wlanP2pConnection(struct WlanConnection *connection);			/* 0x41228 */
void p2pSqramBuildContext(struct WlanConnection *connection, const uint8_t *ies,
			  unsigned int length);				/* SQRAM 0xc000cc1d */
void p2pSqramComputeContext(struct WlanConnection *connection, uint8_t *ies,
			    uint32_t *length);				/* SQRAM 0xc000cd37 */
void p2pSqramQueryContext(const uint8_t *ies, unsigned int length, void *state);	/* SQRAM 0xc000ce8d */
extern struct WlanChannelPowerTable wlanChannelPowerTable;			/* 0x000574ec */

/* Power limits in dBm; WLAN_TX_POWER_NONE: no limit. */
int32_t wlanTxPowerHostLimit __attribute__((section(".itcm.rw"))) = WLAN_TX_POWER_NONE;
int32_t wlanTxPowerLinkLimit __attribute__((section(".itcm.rw"))) = WLAN_TX_POWER_NONE;
/* Channel changes that waited for the transmit path to stop. */
uint32_t wlanChannelSwitchHolds __attribute__((section(".itcm.rw")));
/* The union of the custom IEs' subtype masks. */
uint16_t wlanCustomIeMask __attribute__((section(".itcm.rw")));
struct WlanCustomIeTable wlanCustomIes __attribute__((section(".itcm.bss")));

/*
 * Posts a message to the connection's state machine: the association state
 * machine of a station in mode 2, nothing for an access point unless it is
 * in mode 3, the connection's own one otherwise.
 */
int wlanSmPost(struct WlanConnection *connection, const struct WlanSmMessage *message)
{
	struct WlanSmRecord record;
	struct WlanConnectionInfo *info;
	void *machine;

	if (message == NULL)
		return -1;
	record.argument = 0;
	record.frame = NULL;
	record.command = NULL;
	record.message = message;
	info = wlanConnectionInfo(connection);
	if (connection->mode == 2) {
		machine = &info->smState;
	} else {
		if (connection->interfaceType == WLAN_IF_AP && connection->mode != 3)
			return 0;
		machine = &connection->smState;
	}
	mdevRomQueryState(machine, message->event, &record);
	return 0;
}

/* A host command for a connection's state machines; the buffer's data starts with it. */
void wlanSmPostCommand(OsaBuffer *buffer)
{
	struct WlanConnection *connection;
	const uint8_t *data;
	struct WlanSmRecord record;
	struct WlanConnectionInfo *info;
	void *machine;

	if (buffer == NULL)
		return;
	connection = buffer->context;
	data = (const uint8_t *)buffer + buffer->dataOffset;
	record.argument = 0;
	record.frame = NULL;
	record.message = NULL;
	record.command = data + 9;
	info = wlanConnectionInfo(connection);
	switch (data[0]) {
	case 29:
	case 31:
	case 32:
	case 33:
	case 34:
	case 36:
		machine = &info->smState;
		break;
	case 11:
	case 13:
	case 14:
		machine = &connection->smState;
		break;
	case WLAN_COMMAND_TRACK_BUFFER:
		macMgmtTrackBuffer(connection, LE16(data + 15));
		return;
	default:
		wlanSmPost(connection, (const struct WlanSmMessage *)data);
		return;
	}
	mdevRomQueryState(machine, data[0], &record);
}

/* Sets the slot time and the AIFSN of every queue. */
void wlanSetSlotTime(int shortSlot)
{
	uint32_t flags;
	unsigned int slotTime;
	uint8_t queue;

	macMgmtPrepareContext(&flags, NULL);
	if (flags & 3) {
		macTxNotifyConvertTable(2);
		slotTime = WLAN_SLOT_TIME_SHORT;
	} else {
		macTxNotifyConvertTable(1);
		slotTime = shortSlot == 1 ? WLAN_SLOT_TIME_SHORT : WLAN_SLOT_TIME_LONG;
	}
	romLibScheduleContext();
	romLibResolveContext(slotTime);
	for (queue = 0; queue < 8; queue++)
		romEdcaSetAifsn(queue, p2pDirectDispatchTable(queue));
}

/* Starts a calibration against the connection's peer. */
int wlanCalibrationStart(struct WlanConnection *connection, int value)
{
	rfCalibScheduleDescriptor(connection->localAddress, connection->interfaceAddress);
	romLibFormatContext();
	rfCalibResolveDescriptor();
	return rfCalibNotifyDescriptor(value);
}

void wlanCalibrationEnd(void)
{
	rfCalibInspectDescriptor(0);
	romLibInspectContext(cbProcPrepareTable(6, 0));
	cbProcDispatchTable(6, 1);
	romLibTrackContext();
	wlanMacSet46cBits();
}

unsigned int wlanTxPowerMinimum(unsigned int channel, unsigned int band)
{
	return wlanTxPowerMin(channel, band);
}

/* The transmit power allowed on a channel: calibration, region and the limits set. */
int wlanTxPowerAllowed(unsigned int channel, unsigned int band, unsigned int rate)
{
	int maximum = wlanTxPowerMax(channel, band);
	int power;

	if (wlanRadioConfig->regionPowerTable != 0)
		power = macMgmtTrackHeader(channel, band, rate) < maximum ?
			macMgmtTrackHeader(channel, band, rate) : maximum;
	else
		power = macMgmtInspectHeader(channel, band, rate) < maximum ?
			macMgmtInspectHeader(channel, band, rate) : maximum;
	if (power >= wlanTxPowerHostLimit)
		power = wlanTxPowerHostLimit;
	if (power >= wlanTxPowerLinkLimit)
		return wlanTxPowerLinkLimit;
	return power;
}

uint8_t wlanCurrentChannel(void)
{
	return rfGetChannel();
}

unsigned int wlanCurrentBand(void)
{
	return rfGetBand();
}

/* The channel flags (WLAN_CHANNEL_*) of the channel the radio is tuned to. */
uint8_t wlanCurrentChannelFlags(uint8_t *flags)
{
	switch (rfGetBandwidth()) {
	case 0:
		*flags = (*flags & ~WLAN_CHANNEL_WIDTH_MASK & ~WLAN_CHANNEL_OFFSET_MASK) |
			 WLAN_CHANNEL_WIDTH_20MHZ;
		break;
	case 2:
		*flags = (*flags & ~WLAN_CHANNEL_WIDTH_MASK & ~WLAN_CHANNEL_OFFSET_MASK) |
			 WLAN_CHANNEL_OFFSET_ABOVE;
		break;
	case 3:
		*flags = (*flags & ~WLAN_CHANNEL_WIDTH_MASK & ~WLAN_CHANNEL_OFFSET_MASK) |
			 WLAN_CHANNEL_OFFSET_BELOW;
		break;
	default:
		*flags &= ~WLAN_CHANNEL_WIDTH_MASK & ~WLAN_CHANNEL_OFFSET_MASK;
		break;
	}
	*flags = (*flags & ~WLAN_CHANNEL_BAND_MASK) |
		 (rfGetBand() == RF_BAND_5GHZ ? WLAN_CHANNEL_BAND_5GHZ : WLAN_CHANNEL_BAND_2GHZ);
	return *flags;
}

/* Applies the transmit power limits to the current channel. */
void wlanTxPowerApply(void)
{
	macMgmtResetHeader(0, wlanCurrentChannel());
}

void wlanTxPowerSetHostLimit(int32_t limit, int apply)
{
	if (wlanTxPowerHostLimit != limit) {
		wlanTxPowerHostLimit = limit;
		if (apply)
			wlanTxPowerApply();
	}
}

void wlanTxPowerSetLinkLimit(int32_t limit, int apply)
{
	if (wlanTxPowerLinkLimit != limit) {
		wlanTxPowerLinkLimit = limit;
		if (apply)
			wlanTxPowerApply();
	}
}

uint8_t wlanTxPowerGetLinkLimit(void)
{
	return wlanTxPowerLinkLimit;
}

/* Sets one connection's limit and recomputes the lowest over all connections. */
void wlanTxPowerSetConnectionLimit(int32_t *slot, int32_t limit, int apply)
{
	int32_t lowest;

	if (*slot != limit || limit == WLAN_TX_POWER_NONE) {
		*slot = limit;
		lowest = cbProcResetPacket();
		if (lowest != wlanTxPowerLinkLimit)
			wlanTxPowerSetLinkLimit(lowest, apply);
	}
}

/* The connection's limit from the channel power table, when it applies one. */
void wlanTxPowerUpdateConnection(struct WlanConnection *connection, unsigned int channel,
				 int apply)
{
	uint8_t index = 0;
	int32_t limit = WLAN_TX_POWER_NONE;

	if ((connection->flags2 & 1) && macTxNotifyScheduleState(channel, &index) == 1)
		limit = wlanChannelPowerTable.power[index];
	wlanTxPowerSetConnectionLimit(&connection->txPowerLimit, limit, apply);
}

/* The band and the radio bandwidth of channel flags. */
int wlanChannelFlagsDecode(const uint8_t *flags, uint8_t *band, uint8_t *bandwidth)
{
	*band = *flags & WLAN_CHANNEL_BAND_MASK;
	return rfCalibTrackDescriptor(flags, bandwidth);
}

/*
 * Tunes the radio. With wait, the transmit path is stopped first (if it
 * stops in time) and restarted afterwards.
 */
int wlanSetChannel(uint32_t channel, const uint8_t *flags, int wait)
{
	uint32_t status = 0;
	uint8_t band;
	uint8_t bandwidth;

	wlanChannelFlagsDecode(flags, &band, &bandwidth);
	romLibResolveState();
	if (wait && macTxNotifySetupTable(WLAN_CHANNEL_SWITCH_TIMEOUT, &status) == 0) {
		wlanChannelSwitchHolds++;
		if (!(status & 0x20))
			cbProcResume(0);
		if (!(status & 0x10))
			cbProcFlush();
		romLibResolveState();
		macTxNotifySetupTable(0, &status);
	}
	rfSetChannel(band, channel, bandwidth);
	wlanTxPowerApply();
	macTxNotifyClearTable();
	return romLibApplyState();
}

/* Retunes the current channel to new channel flags. */
int wlanSetChannelFlags(const uint8_t *flags)
{
	return wlanSetChannel(rfGetChannel(), flags, 1);
}

void wlanMacReleaseHold(void)
{
	wlanMacClear46cBits();
}

uint8_t wlanOperatingChannel(void)
{
	return rfGetChannel();
}

/*
 * Starts a management frame in a buffer from wlanMgmtAllocFrame(): the
 * 802.11 header where the data offset points, and the transmit data in
 * front of it. Returns the transmit data.
 */
struct WlanReplyFrame *wlanMgmtBuildFrame(OsaBuffer *buffer, const uint8_t *source,
					  const uint8_t *destination, unsigned int subtype,
					  const uint8_t *bssid)
{
	struct WlanConnection *connection = buffer->context;
	WlanWirelessFrame *frame = (WlanWirelessFrame *)((uint8_t *)buffer + buffer->dataOffset);
	struct WlanReplyFrame *request = (struct WlanReplyFrame *)buffer->data;
	int broadcast;

	buffer->dataOffset = 28;
	__aeabi_memclr4(request, offsetof(struct WlanReplyFrame, frameLength));
	wlanBuildMgmtHeader(frame, source, destination, bssid, subtype, wlanLocalState->powerSave);
	broadcast = 0;
	if (LE16(destination) == 0xffff && LE16(destination + 2) == 0xffff)
		broadcast = LE16(destination + 4) == 0xffff;
	request->descriptor.header.wire.packetLength[0] =
		wlanPsProcessFrame(connection, subtype, broadcast);
	if (connection->interfaceType == WLAN_IF_AP) {
		switch (subtype) {
		case WLAN_SUBTYPE_PROBE_REQUEST:
			if (connection->mode == 3 || !p2pDirectTrackContext(frame))
				request->descriptor.header.wire.packetLength[0] = WLAN_MGMT_SEND_NO_PS;
			else
				request->descriptor.header.wire.packetLength[0] = WLAN_MGMT_SEND_DIRECT;
			break;
		case WLAN_SUBTYPE_PROBE_RESPONSE:
			if (connection->mode == 2)
				request->descriptor.header.wire.packetLength[0] = WLAN_MGMT_SEND_NO_PS;
			break;
		case WLAN_SUBTYPE_ACTION:
			request->descriptor.header.wire.packetLength[0] = WLAN_MGMT_SEND_NO_PS;
			break;
		}
	}
	return request;
}

/* Queues a management frame; it is freed if it cannot be queued. */
int wlanMgmtSendFrame(OsaBuffer *buffer)
{
	if (macTxQueueMgmtMessage((uintptr_t)buffer) == 1) {
		osa_BufferFree(buffer);
		return -1;
	}
	return 0;
}

/* A buffer for a management frame of the connection, its transmit data cleared. */
OsaBuffer *wlanMgmtAllocFrame(struct WlanConnection *connection, uint32_t size)
{
	OsaBuffer *buffer = osa_BufferAlloc(&wlanBlockPool7, size, 0);

	if (buffer == NULL) {
		buffer = osa_BufferAlloc(&wlanReplyFramePool, size, 0);
		if (buffer == NULL)
			return NULL;
	}
	buffer->packetClass = 2;
	buffer->context = connection;
	buffer->dataOffset = 28;
	__aeabi_memclr(buffer->data, offsetof(struct WlanReplyFrame, frameLength));
	buffer->dataOffset = 28 + WLAN_REPLY_FRAME_BODY;
	return buffer;
}

/* Sends a deauthentication or disassociation frame with a reason code. */
int wlanMgmtSendLeave(struct WlanConnection *connection, const uint8_t *destination,
		      const uint8_t *bssid, uint16_t reason, uint8_t kind, int disassociate)
{
	OsaBuffer *buffer = wlanMgmtAllocFrame(connection, WLAN_MGMT_FRAME_SIZE);
	WlanWirelessFrame *frame;
	struct WlanReplyFrame *request;

	if (buffer == NULL)
		return -1;
	frame = (WlanWirelessFrame *)((uint8_t *)buffer + buffer->dataOffset);
	request = wlanMgmtBuildFrame(buffer, connection->interfaceAddress, destination,
				     disassociate ? WLAN_SUBTYPE_DISASSOC : WLAN_SUBTYPE_DEAUTH, bssid);
	frame->bodyLength[0] = 2;
	frame->bodyLength[1] = 0;
	frame->body[0] = reason;
	frame->body[1] = reason >> 8;
	/* The original reloads and stores the data offset back unchanged here. */
	request->kind = kind;
	return wlanMgmtSendFrame(buffer);
}

int wlanMgmtSendDeauth(struct WlanConnection *connection, const uint8_t *destination,
		       const uint8_t *bssid, uint16_t reason, uint8_t kind)
{
	return wlanMgmtSendLeave(connection, destination, bssid, reason, kind, 0);
}

int wlanMgmtSendDisassoc(struct WlanConnection *connection, const uint8_t *destination,
			 const uint8_t *bssid, uint16_t reason, uint8_t kind)
{
	return wlanMgmtSendLeave(connection, destination, bssid, reason, kind, 1);
}

/*
 * Reports an event of the connection through its state machine. Events
 * without a message builder are posted only when they carry no data
 * (code 30 carries one byte); others fail.
 */
int wlanReportEvent(struct WlanConnection *connection, const void *data, int code)
{
	struct WlanSmMessage message;

	if (wlanEventMessageBuild(&message, code, data) == 0) {
		switch (code) {
		case 29:
		case 32:
		case 34:
		case 37:
		case 40:
		case 41:
		case 46:
		case 47:
			break;
		case 30:
			message.value = *(const uint8_t *)data;
			break;
		default:
			return -1;
		}
	}
	wlanSmPost(connection, &message);
	return 0;
}

/* Recomputes wlanCustomIeMask. */
void wlanCustomIeUpdateMask(void)
{
	unsigned int slot;

	wlanCustomIeMask = 0;
	for (slot = 0; slot < WLAN_CUSTOM_IE_SLOTS; slot++)
		wlanCustomIeMask |= wlanCustomIes.entries[slot].mask;
}

/*
 * Sets or deletes custom IEs for the connection's interface from the host's
 * list ({slot, mask, length, IEs} records). A slot another interface set
 * can only be shared with the same IEs. An entry with a channel switch
 * element is owned by every station interface, by the IBSS and, on a P2P
 * link, by the access point. Returns 0, or 1 when the list is refused.
 */
int wlanCustomIeConfigure(struct WlanConnection *connection, const uint8_t *list,
			  int16_t remaining)
{
	uint32_t bit;
	unsigned int slot;
	const uint8_t *ie;
	unsigned int length;
	struct WlanCustomIe *entry;
	uint32_t *owners;

	if (list == NULL || remaining <= 0)
		return 1;
	bit = wlanBssBit(connection->interfaceType, connection->bssNumber);
	for (;;) {
		slot = LE16(list);
		if (slot >= WLAN_CUSTOM_IE_SLOTS)
			return 1;
		ie = list + 2;
		length = LE16(ie + 2);
		owners = &wlanCustomIes.owners[slot];
		entry = &wlanCustomIes.entries[slot];
		if (length == 0) {
			*owners &= ~bit;
			if (osaFindTlv(entry->buffer, WLAN_IE_CHANNEL_SWITCH, entry->length) != NULL)
				*owners = 0;
			if (*owners == 0)
				__aeabi_memclr(entry, sizeof(*entry));
			__aeabi_memclr(entry, sizeof(*entry));
		} else {
			if (length > WLAN_CUSTOM_IE_SIZE)
				return 1;
			if (*owners == 0 || *owners == bit) {
				__aeabi_memcpy(entry, ie, length + 4);
				if (LE16(ie + 2) < WLAN_CUSTOM_IE_SIZE)
					entry->buffer[LE16(ie + 2)] = 0;
				*owners = bit;
				if (osaFindTlv(entry->buffer, WLAN_IE_CHANNEL_SWITCH,
					       entry->length) != NULL) {
					struct WlanConnection *ap;
					uint32_t apBit = 0;

					wlanConnectionFirst(WLAN_IF_IBSS);
					ap = wlanConnectionFirst(WLAN_IF_AP);
					if (ap != NULL && wlanP2pConnection(ap))
						apBit = WLAN_BSS_BIT(WLAN_IF_AP, 0);
					*owners = apBit | (0xffu << (8 * WLAN_IF_STATION)) |
						  WLAN_BSS_BIT(WLAN_IF_IBSS, 0);
				}
				if (entry->mask == WLAN_CUSTOM_IE_ALL_FRAMES) {
					if (slot == 0) {
						p2pSqramBuildContext(connection, entry->buffer,
								     entry->length);
					} else if (slot == 1) {
						p2pSqramQueryContext(entry->buffer, entry->length,
							wlanConnectionPsState(connection)->p2pIeState);
					}
				}
				goto next;
			}
			if (memcmp(entry, ie, length + 4) != 0)
				return 1;
			*owners |= bit;
			__aeabi_memclr(entry, sizeof(*entry));
		}
next:
		list = ie + 4 + LE16(ie + 2);
		remaining = remaining - entry->length - 6;
		if (remaining <= 0) {
			wlanCustomIeUpdateMask();
			return 0;
		}
	}
}

/*
 * Copies the custom IEs the connection's interface may use into out, in the
 * host's record format: every slot, or with single the slot named by the
 * first record of out. Slot 0 with an all-frames mask comes from the P2P
 * code. Returns the length written.
 */
uint16_t wlanCustomIeGet(struct WlanConnection *connection, uint8_t *out, int single)
{
	uint16_t total = 0;
	unsigned int first = 0;
	unsigned int count;
	unsigned int end;
	unsigned int slot;
	uint32_t bit;

	if (out == NULL)
		return 0;
	if (single) {
		count = 1;
		first = LE16(out);
	} else {
		count = WLAN_CUSTOM_IE_SLOTS;
	}
	bit = wlanBssBit(connection->interfaceType, connection->bssNumber);
	end = first + count;
	for (slot = first; slot < end; slot++) {
		uint32_t owners = wlanCustomIes.owners[slot];
		const struct WlanCustomIe *entry = &wlanCustomIes.entries[slot];

		if (owners != 0 && !(owners & bit))
			continue;
		if (entry->mask == WLAN_CUSTOM_IE_ALL_FRAMES && slot == 0) {
			uint32_t length = 0;
			const uint8_t *mask = (const uint8_t *)&wlanCustomIes.entries[0].mask;

			out[0] = slot;
			out[1] = slot >> 8;
			p2pSqramComputeContext(connection, out, &length);
			out += 2;
			out[0] = mask[0];
			out[1] = mask[1];
			out[2] = length;
			out[3] = length >> 8;
			out += (uint16_t)length + 4;
			total = total + (uint16_t)length + 6;
		} else {
			out[0] = slot;
			out[1] = slot >> 8;
			out += 2;
			__aeabi_memcpy(out, entry, entry->length + 4);
			out += 4 + entry->length;
			total = entry->length + total + 6;
		}
	}
	return total;
}
