/*
 * Small receive-path helpers: 802.11 header builders and the frame filters
 * applied before delivery. The filename is inferred; no source filename
 * survives.
 */
#include "robin/wlanrx.h"
#include "robin/ieee80211.h"
#include "robin/rom.h"
#include "robin/runtime.h"

/* Builds a PS-Poll control frame in front of the data offset of buffer. */
void wlanBuildPsPoll(OsaBuffer *buffer, const uint8_t *transmitter, const uint8_t *bssid,
		     int powerSave, uint16_t aid)
{
	WlanWirelessFrame *frame = (WlanWirelessFrame *)((uint8_t *)buffer + buffer->dataOffset);
	uint16_t control = powerSave ? 0x1000 : 0;

	__aeabi_memclr(frame, 0x24);
	/* Frame control 0x00a4 (PS-Poll), power management flag as asked. */
	frame->frameControl[0] = 0xa4;
	frame->frameControl[1] = control >> 8;
	/* Duration/ID carries the association ID with the two top bits set. */
	frame->duration[0] = aid;
	frame->duration[1] = (aid >> 8) | 0xc0;
	__aeabi_memcpy(frame->address2, transmitter, 6);
	__aeabi_memcpy(frame->address1, bssid, 6);
}

void wlanSetHeaderAddresses(WlanWirelessFrame *frame, const uint8_t *address1,
			    const uint8_t *address2, const uint8_t *address3)
{
	__aeabi_memcpy(frame->address1, address1, 6);
	__aeabi_memcpy(frame->address2, address2, 6);
	__aeabi_memcpy(frame->address3, address3, 6);
}

/* Clears a management header and fills in type, subtype and the addresses. */
void wlanInitMgmtHeader(WlanWirelessFrame *frame, unsigned int type, unsigned int subtype,
			const uint8_t *address1, const uint8_t *address2, const uint8_t *address3)
{
	__aeabi_memclr(frame, 0x20);
	frame->frameControl[0] = (frame->frameControl[0] & 3) | ((type << 2) & 0xf) | (subtype << 4);
	wlanSetHeaderAddresses(frame, address1, address2, address3);
}

/* Frame control of a data frame as an unsigned halfword, little endian. */
unsigned int wlanDataFrameControl(int qos, int nullData, int moreData)
{
	unsigned int control = IEEE80211_FC0_TYPE_DATA;

	if (nullData)
		control = qos ? 0xc8 : 0x48;
	else if (qos)
		control = 0x88;
	if (moreData)
		control |= IEEE80211_FC1_MORE_DATA << 8;
	return control;
}

/* Keeps only frames sent to the distribution system; drops the others. */
int wlanRxFilterToDs(int enabled, OsaBuffer *buffer, const WlanWirelessFrame *frame)
{
	unsigned int flags = frame->frameControl[1];

	if (!enabled || (flags & IEEE80211_FC1_FROMDS) || !(flags & IEEE80211_FC1_TODS)) {
		buffer->length = 0;
		return 0;
	}
	return flags & IEEE80211_FC1_TODS;
}

/* IBSS filter: neither DS bit set and the BSSID (address 3) must match. */
int wlanRxFilterIbss(int enabled, OsaBuffer *buffer, const WlanWirelessFrame *frame,
		     const uint8_t *bssid)
{
	unsigned int flags = frame->frameControl[1];
	unsigned int i;

	if (enabled && !(flags & IEEE80211_FC1_FROMDS) && !(flags & IEEE80211_FC1_TODS)) {
		for (i = 0; i < 6; i++) {
			if (frame->address3[i] != bssid[i])
				goto drop;
		}
		return frame->address3[5];
	}
drop:
	buffer->length = 0;
	return 0;
}

/* Station filter: from the distribution system with the transmitter equal to the BSSID. */
int wlanRxFilterStation(int enabled, OsaBuffer *buffer, const WlanWirelessFrame *frame,
			const uint8_t *bssid)
{
	unsigned int flags = frame->frameControl[1];
	int result;

	if (enabled && (flags & IEEE80211_FC1_FROMDS) && !(flags & IEEE80211_FC1_TODS)) {
		result = memcmp(frame->address2, bssid, 6);
		if (result == 0)
			return result;
	}
	buffer->length = 0;
	return 0;
}

/* Null frames are never protected; returns 1, or 2 when the frame is protected. */
int wlanRxClearProtectedOnNull(WlanWirelessFrame *frame)
{
	unsigned int subtype = frame->frameControl[0] >> IEEE80211_FC0_SUBTYPE_SHIFT;

	if (subtype == IEEE80211_SUBTYPE_NULL || subtype == IEEE80211_SUBTYPE_QOS_NULL)
		frame->frameControl[1] &= ~IEEE80211_FC1_PROTECTED;
	return ((frame->frameControl[1] & IEEE80211_FC1_PROTECTED) != 0) + 1;
}

/* Drops a frame relayed back from the distribution system with our own source address. */
void wlanRxDropLoopback(OsaBuffer *buffer, const WlanWirelessFrame *frame, const uint8_t *own)
{
	unsigned int flags = frame->frameControl[1];

	if (!(flags & IEEE80211_FC1_TODS) && (flags & IEEE80211_FC1_FROMDS) &&
	    memcmp(frame->address3, own, 6) == 0)
		buffer->length = 0;
}

/*
 * Fragment sequencing: a fragment must carry the next fragment number of the
 * current sequence number, and a retried whole frame is a duplicate.
 */
void wlanRxCheckFragment(OsaBuffer *buffer, const WlanWirelessFrame *frame,
			 uint16_t *lastSequence, uint16_t *lastFragment, unsigned int maxFragments)
{
	unsigned int sequenceControl = frame->sequenceControl[0] | (frame->sequenceControl[1] << 8);
	unsigned int sequence = sequenceControl >> 4;
	unsigned int fragment = sequenceControl & 0xf;
	unsigned int flags;

	if (*lastSequence != sequence) {
		if (fragment != 0)
			goto drop;
		*lastSequence = sequence;
		goto accept;
	}
	flags = frame->frameControl[1];
	if ((flags & IEEE80211_FC1_RETRY) && !(flags & IEEE80211_FC1_MORE_FRAG)) {
		if (fragment == 0)
			goto drop;
		if ((unsigned int)*lastFragment + 1 != fragment)
			goto drop;
	} else if (fragment != 0) {
		if ((unsigned int)*lastFragment + 1 != fragment)
			goto drop;
	}
	if (fragment >= maxFragments)
		goto drop;
accept:
	*lastFragment = fragment;
	return;
drop:
	buffer->length = 0;
}

/* Decodes the rate and the HT information (bit 0 HT, bit 1 40 MHz, bit 2 short GI). */
void wlanRxDecodeRate(const struct WlanRxStatus *status, uint8_t *htInfo, uint8_t *rate)
{
	unsigned int phyMode = (status->phyInfo >> 3) & 3;
	unsigned int info;
	unsigned int value;

	if (phyMode == 2 || phyMode == 3) {
		info = 1;
		if (status->flags & 0x80)
			info = 5;
		if (status->mcs & 0x80)
			info |= 2;
		value = status->mcs & 0x7f;
	} else {
		value = status->legacyRate;
		info = 0;
	}
	*htInfo = info;
	*rate = value;
}

/* True for a Null function frame, with or without QoS. */
int wlanIsNullData(const WlanWirelessFrame *frame)
{
	return ieee80211IsNullData(frame->frameControl[0]);
}

/* True for any fragment of a fragmented frame, including the last one. */
int wlanIsFragment(const WlanWirelessFrame *frame)
{
	if (frame->frameControl[1] & IEEE80211_FC1_MORE_FRAG)
		return 1;
	return (frame->sequenceControl[0] & 0xf) != 0;
}
