/*
 * The frame builder of the transmit task (macTxTaskMain() calls it for each
 * host data frame): the frame in the buffer is made into the frames the MAC
 * sends. It chooses the cipher the connection's key calls for, cuts the
 * payload into fragments at the fragmentation threshold, lays the fragments
 * out in the buffer over the host's frame (a payload that is not cut stays
 * where it is), puts a copy of the header and of the cipher's header in front
 * of each, sets the durations the MAC announces and encrypts the frames the
 * firmware encrypts itself. The file name is inferred.
 */
#include <stddef.h>
#include "robin/conn.h"
#include "robin/dma.h"
#include "robin/esa.h"
#include "robin/ieee80211.h"
#include "robin/macentry.h"
#include "robin/mactx.h"
#include "robin/phy.h"
#include "robin/protection.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/soc.h"
#include "robin/swcrypto.h"
#include "robin/txbuild.h"
#include "robin/txring.h"
#include "robin/wep.h"

_Static_assert(offsetof(struct WlanConnectionInfo, rateFlags) == 592, "info rate flags");
_Static_assert(offsetof(struct WlanConnectionInfo, grpKeyData.KeyIndex) == 850, "group key index");
_Static_assert(offsetof(struct WlanConnection, fragThreshold) == 160, "fragmentation threshold");

/* Not converted yet; provisional names. */
int romLibCheckState(int a, int b, int c, int d, int e);		/* 0x03f3a347 */

/* WlanConnection.hostFlags: the host's WEP key is used. */
#define WLAN_CONN_HOST_WEP	WLAN_CONN_HOST_FLAG_8

/* WlanConnection.flags: the connection's frames are protected by an RTS or a CTS-to-self. */
#define WLAN_CONN_PROTECTED	0x08

/* The bytes of a key: its direction, its type and its state (cipher_key_hdr_t). */
#define TX_KEY_STATE(key)	((key)[10])

/* WlanMacTxRequest.flags: the bits that say the frame has a QoS header. */
#define MAC_TX_FLAG_HEADER	0x00000003u
#define MAC_TX_FLAG_NEED_DURATION	0x00000400u

/* Frame control: the frame is protected, more fragments follow. */
#define WLAN_TX_FC1_MORE_FRAG	0x04
#define WLAN_TX_FC1_PROTECTED	0x40

/* The bytes a frame's header has in the descriptor's copy (a QoS header and the FCS are longer). */
#define WLAN_TX_HEADER_LENGTH		24
#define WLAN_TX_HEADER_LENGTH_QOS	30

/* Where in the transmit data the frame starts, and the descriptor's copy of its header (32 bytes). */
#define WLAN_TX_DATA_FRAME	116
#define WLAN_TX_FRAME_HEADER	32

/* Encrypts a fragment with WEP: the initialisation vector is the MAC timer, the key the default key. */
int wlanTxEncryptWepFragment(uint8_t *fragment, uint32_t length, const WlanWepHostConfig *config)
{
	uint8_t seed[16];
	unsigned int keyIndex = config->keys.defaultKey;
	const WlanWepHostKey *key = &config->keys.key[keyIndex];
	unsigned int size = 8;
	unsigned int is104 = 0;
	unsigned int i;

	seed[0] = SOC_MAC_TIMER >> 16;
	seed[1] = SOC_MAC_TIMER >> 8;
	seed[2] = SOC_MAC_TIMER;
	if (key->isWep104) {
		size = 16;
		is104 = 1;
	}
	for (i = 0; i < size - 3; i++)
		seed[3 + i] = key->key[i];
	wlanWepEncrypt(fragment + 32, fragment + 36, length - 8, seed, is104, keyIndex);
	return 1;
}

/* What the cipher adds to the frame (total, in front and after) and which one it is. */
__attribute__((noinline)) int wlanTxCipherOverhead(int wep, int tkip, int aes, int wapi, uint8_t *cipher,
			 struct WlanTxCipherOverhead *overhead)
{
	if (tkip != 0) {
		*cipher = WLAN_TX_CIPHER_TKIP;
		overhead->total = 12;
		overhead->header = 8;
		overhead->trailer = 4;
		return 12;
	}
	if (aes != 0) {
		*cipher = WLAN_TX_CIPHER_AES;
		overhead->total = 16;
		overhead->header = 8;
		overhead->trailer = 8;
		return 3;
	}
	if (wep != 0) {
		*cipher = WLAN_TX_CIPHER_WEP;
		overhead->total = 8;
		overhead->header = 4;
		overhead->trailer = 4;
		return 1;
	}
	if (wapi != 0) {
		*cipher = 4;
		overhead->total = 34;
		overhead->header = 18;
		overhead->trailer = 16;
		return 18;
	}
	overhead->total = 0;
	overhead->header = 0;
	overhead->trailer = 0;
	return 0;
}

/*
 * What the payload is: whether it is an EAPOL, WAPI or other key frame (1), and what has to go
 * in front of it (the 802.2 header and SNAP of RFC 1042, or of the bridge tunnel for AppleTalk
 * ARP and IPX). A payload that starts with one already, or with a length, is told by its first
 * two bytes.
 */
__attribute__((noinline)) int wlanTxClassifyPayload(unsigned int packetType, const uint8_t *payload, uint8_t *offset, int *drop,
			  uint16_t *length, uint8_t *snap, unsigned int *snapLength)
{
	uint32_t eapol = 0x8e88;
	uint32_t wapi = 0xb488;
	uint32_t other = 0x300;
	unsigned int skip = 0;
	int found = 0;
	unsigned int type;
	const uint8_t *protocol;

	if (packetType == 230)
		goto compare;
	type = (payload[0] << 8) | payload[1];
	if (packetType != 5) {
		if (type == 0xaaaa) {
			skip = 6;
		} else {
			if (type >= 0x600)
				goto lookup;
			*offset = 2;
			skip = 8;
			*length -= 2;
		}
		*snapLength = 0;
		goto lookup;
	}
	*snapLength = 0;
	*offset = 0;
	*drop = 0;
lookup:
	if (payload[skip] == 0x80) {
		if (payload[skip + 1] != 0xf3)
			goto compare;
	} else if (payload[skip] != 0x81 || payload[skip + 1] != 0x37) {
		goto compare;
	}
	snap[5] = 0xf8;
compare:
	protocol = &payload[skip];
	if (memcmp(protocol, &eapol, 2) == 0)
		found = 1;
	if (memcmp(protocol, &wapi, 2) == 0)
		found = 1;
	if (memcmp(protocol, &other, 2) == 0)
		return 1;
	return found;
}

/* How many frames the payload is cut into at the threshold (which is cut down to what a frame can carry). */
unsigned int wlanTxFragmentCount(uint16_t *threshold, int qos, unsigned int cipherOverhead,
				 unsigned int payloadLength, unsigned int snapLength)
{
	unsigned int size;
	unsigned int first;
	unsigned int count;
	unsigned int rest;

	if (qos != 0)
		*threshold -= 30;
	else
		*threshold -= 28;
	size = (uint16_t)(*threshold - cipherOverhead - ((*threshold - cipherOverhead) & 3));
	*threshold = size;
	count = 1;
	first = size - snapLength;
	if (first < payloadLength) {
		rest = payloadLength - first;
		count = rest / size + 1;
		if (rest % size != 0)
			++count;
	}
	return count;
}

/*
 * Where each fragment goes: the buffer from *cursor on holds, for every fragment, the 32 bytes of
 * the descriptor's copy of the header, the cipher's header (and, in the first, the 802.2 header),
 * the payload and the cipher's trailer. destinations gets each one's start and lengths how much it
 * holds after the 32 bytes; the payload's parts go to fragments, to be moved into place.
 */
void wlanTxFragmentLayout(unsigned int count, struct WlanTxFragment *fragments, uint32_t *destinations,
			  uint32_t *lengths, uintptr_t *cursor, unsigned int snapLength,
			  const struct WlanTxCipherOverhead *overhead, uint16_t *remaining,
			  unsigned int offset, unsigned int threshold, const uint8_t **payload)
{
	unsigned int i;

	for (i = 0; i < count; i++) {
		unsigned int first = 0;
		unsigned int size;

		destinations[i] = *cursor;
		lengths[i] = 0;
		*cursor += WLAN_TX_FRAME_HEADER;
		if (i == 0) {
			*cursor += snapLength;
			lengths[0] = snapLength;
			first = (uint8_t)snapLength;
		}
		*cursor += overhead->header;
		lengths[i] += overhead->header;
		fragments[i].source = *payload + offset;
		fragments[i].destination = (uint8_t *)*cursor;
		size = threshold - first;
		if (*remaining <= size || count <= 1)
			size = *remaining;
		fragments[i].length = size;
		*payload += size;
		lengths[i] += size;
		*cursor = 4 * ((size + *cursor + 3) >> 2);
		*cursor += overhead->trailer;
		lengths[i] += overhead->trailer;
		*remaining -= fragments[i].length;
	}
}

/* Turns a request's flags to protection by CTS-to-self (for a frame sent without an acknowledgement) or by RTS. */
__attribute__((noinline)) uint32_t wlanTxFlagsProtect(uint32_t flags)
{
	if (flags & MAC_TX_FLAG_NO_ACK)
		return (flags & ~(MAC_TX_FLAG_RTS | MAC_TX_FLAG_CTS_TO_SELF)) | MAC_TX_FLAG_CTS_TO_SELF;
	return (flags & ~(MAC_TX_FLAG_RTS | MAC_TX_FLAG_CTS_TO_SELF)) | MAC_TX_FLAG_RTS;
}

/*
 * Moves the payloads to where wlanTxFragmentLayout() put them: forward the ones that go below
 * where they are (from the first), then backward from the last the ones that go above.
 */
void wlanTxFragmentsMove(const struct WlanTxFragment *fragments, unsigned int last)
{
	signed char first = 0;
	signed char index = (signed char)last;

	for (;;) {
		const struct WlanTxFragment *fragment = &fragments[first];

		if (fragment->destination > fragment->source || (unsigned int)first > last)
			break;
		if (fragment->destination < fragment->source)
			wlanDmaMoveForward(fragment->destination, fragment->source, fragment->length);
		first = (signed char)(first + 1);
	}
	while (first <= index) {
		const struct WlanTxFragment *fragment = &fragments[index];

		wlanDmaMoveBackward(fragment->destination, fragment->source, fragment->length);
		index = (signed char)(index - 1);
	}
}

/* The index of the rate the request is to be sent at. */
static inline unsigned int wlanTxRequestRate(const struct WlanMacTxRequest *request)
{
	return MAC_TX_RATE_INDEX(request->rate);
}

/*
 * Builds the frames of a host data frame. The result is where the first one starts in the buffer
 * (NULL when the frame is not sent: *drop is set when it is to be dropped); *eapol tells whether the
 * payload is a key frame, *key is the key to send it with (or NULL). packetType 230 is a frame the
 * firmware relays, ampdu 1 one that goes into an aggregate, qos one with a QoS header.
 */
uint8_t *wlanTxBuildFrame(struct MacTxFrameBuild *build, OsaBuffer *buffer, int *drop, int *eapol,
			  int reserved, unsigned int packetType, int ampdu,
			  const uint8_t **key, int qos)
{
	struct WlanTxFragment fragments[WLAN_TX_FRAGMENTS_MAX];
	uint32_t destinations[WLAN_TX_FRAGMENTS_MAX];
	uint32_t lengths[WLAN_TX_FRAGMENTS_MAX];
	struct WlanTxCipherOverhead overhead;
	struct WlanTxFragmentHeader header;
	uint8_t snap[8] = { 0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00 };
	uint8_t *data = (uint8_t *)buffer + buffer->dataOffset;
	struct WlanConnection *connection = buffer->context;
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);
	struct WlanMacTxRequest *request = TX_DATA_REQUEST(build->data);
	const uint8_t *payload = build->payload;
	uint8_t *frame;
	uintptr_t cursor;
	uint8_t *result = NULL;
	unsigned int snapLength = 6;
	unsigned int offset = 0;
	unsigned int wep = 0;
	unsigned int wapi = 0;
	int tkip = 0;
	int aes = 0;
	uint8_t cipher = WLAN_TX_CIPHER_NONE;
	uint16_t threshold = 0;
	struct WlanKeyBuffer *block;
	uint8_t flags;
	unsigned int i;

	frame = (uint8_t *)__aeabi_uread4(request->frame);
	if (packetType == 230)
		snapLength = 0;
	block = (frame[6] & 1) ? connection->keyBlocks[0] : connection->keyBlocks[1];
	if (block != NULL)
		*key = (const uint8_t *)&block->key;
	flags = connection->flags;
	if ((flags & WLAN_CONN_ENCRYPTED) && *key == NULL &&
	    ((connection->hostFlags & WLAN_CONN_HOST_WEP) || (flags & WLAN_CONN_PRIVACY)))
		goto dropped;
	if ((flags & WLAN_CONN_JOIN_OPEN) && reserved != 3) {
		wep = (connection->hostFlags >> 3) & 1;
		if (wep == 0) {
			if (flags & WLAN_CONN_PRIVACY) {
				unsigned int type = TX_KEY_TYPE(*key);

				tkip = type == KEY_TYPE_TKIP;
				aes = type == KEY_TYPE_AES;
			} else if ((flags & WLAN_CONN_WAPI) && *key != NULL && (TX_KEY_STATE(*key) & 1)) {
				wapi = 1;
			} else if (connection->interfaceType == 1) {
				const uint8_t *k = *key;

				if (k != NULL && TX_KEY_TYPE(k) == KEY_TYPE_AES && (TX_KEY_STATE(k) & 1))
					aes = 1;
			}
		}
	}
	*eapol = wlanTxClassifyPayload(packetType, payload, (uint8_t *)&offset, drop,
				       &build->payloadLength, snap, &snapLength);
	if (((!(flags & WLAN_CONN_JOIN_OPEN) ||
	      (*key != NULL && (TX_KEY_STATE(*key) & KEY_STATE_FORCE_EAPOL_UNENCRYPTED) &&
	       (TX_KEY_STATE(*key) & KEY_STATE_ENABLE)) ||
	      (connection->hostFlags & WLAN_CONN_HOST_WEP) || macTxBuildOverride != 0)) &&
	    reserved != 3) {
		if (*eapol != 0) {
			if ((*key != NULL && (TX_KEY_STATE(*key) & KEY_STATE_FORCE_EAPOL_UNENCRYPTED) &&
			     (TX_KEY_STATE(*key) & KEY_STATE_ENABLE)) ||
			    (connection->hostFlags & WLAN_CONN_HOST_WEP)) {
				wep = 0;
				aes = 0;
				tkip = 0;
				*key = NULL;
			}
		} else if (!(flags & WLAN_CONN_JOIN_OPEN)) {
			goto dropped;
		}
	}
	if (wapi != 0 && *eapol != 0) {
		wapi = 0;
		*key = NULL;
	}
	*drop = 0;
	wlanTxCipherOverhead(wep, tkip, aes, wapi, &cipher, &overhead);
	if (tkip || aes || wapi || wep) {
		frame = (uint8_t *)__aeabi_uread4(request->frame);
		frame[3] |= WLAN_TX_FC1_PROTECTED;
		if (*key == NULL || !(TX_KEY_STATE(*key) & KEY_STATE_ENABLE))
			goto dropped;
	}
	if (packetType == 230 || ampdu == 1) {
		build->fragments = 1;
	} else {
		threshold = connection->fragThreshold[0] | (connection->fragThreshold[1] << 8);
		build->fragments = wlanTxFragmentCount(&threshold, qos, overhead.total,
						       build->payloadLength, snapLength);
	}
	if (__aeabi_uread4(request->flags) & MAC_TX_FLAG_NO_ACK) {
		build->fragments = 1;
	} else {
		frame = (uint8_t *)__aeabi_uread4(request->frame);
		frame[3] |= WLAN_TX_FC1_MORE_FRAG;
		if (((wlanProtection.flags & WLAN_PROTECTION_USE_PROTECTION) ||
		     (connection->mode == 2 && (info->rateFlags & WLAN_CONN_PROTECTED))) &&
		    wlanTxRequestRate(request) > 3 && !(wlanTxRateFlags & 0x40)) {
			unsigned int rate = (8 * bbpTxHook()) & 0x3ff;

			request->rate = (request->rate & ~MAC_TX_RATE_MASK) | rate;
		}
		cursor = 4 * (((uintptr_t)data + WLAN_TX_DATA_FRAME) >> 2);
		if (build->fragments != 1)
			goto layout;
	}
	cursor = 4 * ((uintptr_t)(payload + (uint8_t)offset - snapLength - overhead.header -
				  WLAN_TX_FRAME_HEADER) >> 2);
	if (cursor < (uintptr_t)data + WLAN_TX_DATA_FRAME)
		cursor = (uintptr_t)data + WLAN_TX_DATA_FRAME;
layout:
	result = (uint8_t *)cursor;
	wlanTxFragmentLayout(build->fragments, fragments, destinations, lengths, &cursor, snapLength,
			     &overhead, &build->payloadLength, (uint8_t)offset, threshold, &payload);
	wlanTxFragmentsMove(fragments, build->fragments - 1);
	if (snapLength != 0)
		__aeabi_memcpy(fragments[0].destination - snapLength, snap, snapLength);
	__aeabi_memcpy(&header, (const void *)__aeabi_uread4(request->frame), sizeof(header));
	__aeabi_uwrite4((int)destinations[0], request->frame);
	if (packetType != 230 && ampdu != 1 &&
	    (wlanTxUsesRts(connection, (uint16_t)lengths[0], header.address1) ||
	     (connection->flags & WLAN_CONN_PROTECTED))) {
		__aeabi_uwrite4((int)wlanTxFlagsProtect(__aeabi_uread4(request->flags)), request->flags);
	}
	__aeabi_uwrite4((int)(__aeabi_uread4(request->flags) & ~MAC_TX_FLAG_NEED_DURATION), request->flags);
	for (i = 0; i < build->fragments; i++) {
		unsigned int last = (unsigned int)(build->fragments - 1) == i;
		uint32_t requestFlags;

		if (tkip && last)
			lengths[i] += 8;
		header.length = lengths[i];
		if (last) {
			unsigned int length = header.length;
			unsigned int hardware;

			header.frameControl[1] &= ~WLAN_TX_FC1_MORE_FRAG;
			requestFlags = __aeabi_uread4(request->flags);
			hardware = length + ((requestFlags & MAC_TX_FLAG_HEADER) ? WLAN_TX_HEADER_LENGTH_QOS :
									  WLAN_TX_HEADER_LENGTH) + 4;
			request->length = hardware;
			if (qos != 0)
				request->length = hardware + 2;
			if (requestFlags & MAC_TX_FLAG_NEED_DURATION)
				header.duration = wlanConnectionAckTime(connection, wlanTxRequestRate(request),
									requestFlags);
			requestFlags = __aeabi_uread4(request->flags);
			if (requestFlags & MAC_TX_FLAG_RTS)
				request->duration[1] = wlanFrameDurationMacCall(wlanTxRequestRate(request),
										requestFlags, (uint16_t)lengths[i],
										header.duration, 0);
			else if (requestFlags & MAC_TX_FLAG_CTS_TO_SELF)
				request->duration[1] = romLibCheckState(wlanTxRequestRate(request), requestFlags,
									(uint16_t)lengths[i], header.duration, qos);
			else
				request->duration[1] = 0;
		} else {
			requestFlags = __aeabi_uread4(request->flags);
			if (requestFlags & MAC_TX_FLAG_NEED_DURATION)
				header.duration = wlanConnectionFrameDuration(connection, wlanTxRequestRate(request),
									      requestFlags, (uint16_t)lengths[i + 1],
									      qos);
			if (i == 0) {
				uint16_t duration;

				requestFlags = __aeabi_uread4(request->flags);
				if (requestFlags & MAC_TX_FLAG_RTS) {
					unsigned int ack = wlanConnectionAckTime(connection,
										 wlanTxRequestRate(request),
										 requestFlags);

					duration = wlanFrameDurationMacCall(wlanTxRequestRate(request),
									    __aeabi_uread4(request->flags),
									    (uint16_t)lengths[0], ack, 0);
				} else if (requestFlags & MAC_TX_FLAG_CTS_TO_SELF) {
					unsigned int ack = wlanConnectionAckTime(connection,
										 wlanTxRequestRate(request),
										 requestFlags);

					duration = romLibCheckState(wlanTxRequestRate(request),
								    __aeabi_uread4(request->flags),
								    (uint16_t)lengths[0], ack, qos);
				} else {
					duration = 0;
				}
				request->duration[0] = duration;
			}
		}
		header.sequenceControl = (build->sequenceControl & 0xfff0) | (i & 0xf);
		__aeabi_memcpy((void *)destinations[i], &header, sizeof(header));
		if (cipher == WLAN_TX_CIPHER_WEP) {
			wlanTxEncryptWepFragment((uint8_t *)destinations[i], lengths[i],
						 (const WlanWepHostConfig *)*key);
		} else if (cipher == WLAN_TX_CIPHER_TKIP) {
			uint8_t *fragment = (uint8_t *)destinations[i];
			uint8_t keyId = 0;
			unsigned int length = (uint16_t)lengths[i] - overhead.total;

			*(uint16_t *)fragment = last ? length - 8 : length;
			if (connection->mode == 2 && (fragment[6] & 1))
				keyId = info->grpKeyData.KeyIndex;
			wlanTxEncryptCcmp(fragment, build->tid, (cipher_key_t *)*key, keyId);
			if (last)
				*(uint16_t *)fragment = *(uint16_t *)fragment + 8;
		}
	}
	return result;

dropped:
	*drop = 1;
	return NULL;
}
