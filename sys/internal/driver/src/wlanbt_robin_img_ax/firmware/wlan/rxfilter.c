/*
 * Small receive-path helpers: 802.11 header builders and the frame filters
 * applied before delivery. The filename is inferred; no source filename
 * survives.
 */
#include "robin/wlanrx.h"
#include "robin/ieee80211.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/mef.h"

/*
 * Fills the QoS control byte of a data frame's transmit data: the TID, the
 * no-acknowledgement policy when the host asked for it (control bits 13 and
 * 14, which also tell the MAC not to wait for an ACK), and the A-MSDU bit
 * for frames the host built itself.
 */
void wlanTxSetQosControl(struct WlanMacTxRequest *request, struct WlanReplyFrame *data,
			 uint32_t control, unsigned int tid, int amsdu)
{
	uint8_t *qos = &data->descriptor.header.bssType;

	*qos = ((*qos >> 3 << 3) | (tid & 7)) & 0x9f;
	if ((control & 0x4000) && (control & 0x2000)) {
		__aeabi_uwrite4((int)(__aeabi_uread4(request->flags) | MAC_TX_FLAG_NO_ACK),
				request->flags);
		*qos = (*qos & 0x9f) + 0x20;
	}
	if (amsdu)
		*qos |= 0x80;
}

/* Sets the retry limit (bits 8-11) of a transmit control word and enables it. */
uint32_t wlanTxControlSetRetryLimit(uint32_t control, unsigned int limit)
{
	return ((control | 0x1000) & ~0xf00u) | limit << 8;
}

/*
 * Chooses the protection of a frame sent above the lowest rates: RTS/CTS
 * when the connection prefers it and the frame is not group-addressed,
 * CTS-to-self otherwise. Returns 0 at rate indexes 0-3, which need none.
 */
int wlanTxSetProtection(struct WlanMacTxRequest *request, int protect, int preferRts)
{
	uint32_t flags;

	if ((((unsigned int)request->rate << 22) >> 25) <= 3)
		return 0;
	if (protect) {
		flags = __aeabi_uread4(request->flags);
		if (preferRts && !(flags & MAC_TX_FLAG_NO_ACK))
			flags = (flags | MAC_TX_FLAG_RTS) & ~MAC_TX_FLAG_CTS_TO_SELF;
		else
			flags = (flags | MAC_TX_FLAG_CTS_TO_SELF) & ~MAC_TX_FLAG_RTS;
		__aeabi_uwrite4((int)flags, request->flags);
	}
	return 1;
}

/* Builds a PS-Poll control frame at the data offset of buffer and returns it. */
WlanWirelessFrame *wlanBuildPsPoll(OsaBuffer *buffer, const uint8_t *transmitter, const uint8_t *bssid,
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
	return frame;
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

/*
 * Strips the 12-byte security trailer the MAC leaves on a decrypted frame.
 * A status with bits 2-3 set is a decryption error, reported and dropped
 * (returns 0); bit 1 says the frame was TKIP-decrypted, so the last fragment
 * carries the 8-byte Michael MIC, copied to mic. *tkip tells which.
 */
int wlanRxTkipTrailer(unsigned int status, uint8_t *frame, const uint8_t *end, OsaBuffer *buffer,
		      int *tkip, void *mic)
{
	int checked = 0;
	unsigned int length;

	if (status != 0) {
		if ((status >> 2) & 3) {
			wlanReceiveErrorHandler(buffer->context);
			return 0;
		}
		if (!(status & 2))
			return 0;
		checked = 1;
	}
	length = (frame[1] << 8 | frame[0]) - 12;
	frame[0] = (uint8_t)length;
	frame[1] = (uint8_t)(length >> 8);
	buffer->length = (uint16_t)(length - 64);
	if (!(frame[3] & 4) && (frame[24] & 0xf)) {
		if (checked)
			__aeabi_memcpy(mic, end + (frame[1] << 8 | frame[0]) - 104, 8);
		else
			buffer->length = (uint16_t)(length - 64) - 8;
	}
	*tkip = checked;
	return 1;
}

/* Removes the Michael MIC from a TKIP frame and checks it; returns 1 when it matches. */
int wlanRxTkipMicCheck(uint8_t *frame, const uint8_t *data, OsaBuffer *buffer,
		       const cipher_key_t *key, const uint32_t *expected)
{
	unsigned int length = (frame[1] << 8 | frame[0]) - 8;
	uint32_t mic[2];

	frame[0] = (uint8_t)length;
	frame[1] = (uint8_t)(length >> 8);
	buffer->length -= 8;
	wlanMichaelMic(frame, key->ckd.aes.rxMicKey, data, mic);
	return mic[0] == expected[0] && mic[1] == expected[1];
}

/* Stores the receive sequence counter of the last frame accepted with a key. */
void wlanRxKeySetReplayCounter(cipher_key_t *key, uint16_t low, uint32_t high)
{
	key->ckd.aes.rxIv16 = low;
	key->ckd.aes.rxIv32 = high;
}

/*
 * Reads the packet number from the TKIP or CCMP header of a received frame
 * and returns 1 if it is not newer than the key's last one (a replay).
 * Otherwise returns 0 with the number in *low and *high.
 */
int wlanRxReplayCheck(const uint8_t *iv, const cipher_key_t *key, uint16_t *low, uint32_t *high)
{
	uint16_t counterLow;
	uint32_t counterHigh;

	if (key->hdr.keyType == KEY_TYPE_TKIP)
		counterLow = (uint16_t)((iv[0] << 8) + iv[2]);
	else
		counterLow = (uint16_t)((iv[1] << 8) + iv[0]);
	counterHigh = ((((iv[7] << 8) + iv[6]) << 8) + iv[5]) << 8;
	counterHigh += iv[4];
	if (key->ckd.aes.rxIv32 == counterHigh && key->ckd.aes.rxIv16 >= counterLow)
		return 1;
	if (key->ckd.aes.rxIv32 != 0xffffffff && key->ckd.aes.rxIv32 > counterHigh)
		return 1;
	*low = counterLow;
	*high = counterHigh;
	return 0;
}

/* Keeps only frames sent to the distribution system; drops the others. */
void wlanRxFilterToDs(int enabled, OsaBuffer *buffer, const WlanWirelessFrame *frame)
{
	if (!enabled || (frame->frameControl[1] & IEEE80211_FC1_FROMDS) ||
	    !(frame->frameControl[1] & IEEE80211_FC1_TODS))
		buffer->length = 0;
}

/* IBSS filter: neither DS bit set and the BSSID (address 3) must match. */
void wlanRxFilterIbss(int enabled, OsaBuffer *buffer, const WlanWirelessFrame *frame,
		      const uint8_t *bssid)
{
	if (!enabled || (frame->frameControl[1] & IEEE80211_FC1_FROMDS) ||
	    (frame->frameControl[1] & IEEE80211_FC1_TODS) ||
	    frame->address3[0] != bssid[0] || frame->address3[1] != bssid[1] ||
	    frame->address3[2] != bssid[2] || frame->address3[3] != bssid[3] ||
	    frame->address3[4] != bssid[4] || frame->address3[5] != bssid[5])
		buffer->length = 0;
}

/* Station filter: from the distribution system with the transmitter equal to the BSSID. */
void wlanRxFilterStation(int enabled, OsaBuffer *buffer, const WlanWirelessFrame *frame,
			 const uint8_t *bssid)
{
	if (!enabled || !(frame->frameControl[1] & IEEE80211_FC1_FROMDS) ||
	    (frame->frameControl[1] & IEEE80211_FC1_TODS) || memcmp(frame->address2, bssid, 6) != 0)
		buffer->length = 0;
}

/* Null frames are never protected; returns 1 when the frame is not protected, 0 when it is. */
int wlanRxClearProtectedOnNull(WlanWirelessFrame *frame)
{
	unsigned int subtype = frame->frameControl[0] >> IEEE80211_FC0_SUBTYPE_SHIFT;

	if (subtype == IEEE80211_SUBTYPE_NULL || subtype == IEEE80211_SUBTYPE_QOS_NULL)
		frame->frameControl[1] &= ~IEEE80211_FC1_PROTECTED;
	return !(frame->frameControl[1] & IEEE80211_FC1_PROTECTED);
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
		if (__aeabi_uread4(&status->flags) & 0x80)	/* Sony reads the word */
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
