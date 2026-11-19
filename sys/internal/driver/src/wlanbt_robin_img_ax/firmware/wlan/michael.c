/*
 * Michael, TKIP's message integrity code (IEEE 802.11i, 8.3.2.3), over a received
 * frame: its destination and source addresses, a zero priority and the data. The
 * filename is inferred; no source filename survives.
 */
#include "robin/wlan.h"
#include "robin/wlanrx.h"

/* The message's end: 0x5a, then at least four zero bytes up to a word boundary. Returns the padded length. */
static __attribute__((noinline)) unsigned int wlanMichaelPad(uint8_t *data, unsigned int length)
{
	data[length++] = 0x5a;
	data[length++] = 0;
	data[length++] = 0;
	data[length++] = 0;
	do {
		data[length++] = 0;
	} while (length & 3);
	return length;
}

static inline uint32_t rotateRight(uint32_t value, unsigned int bits)
{
	return (value >> bits) | (value << (32 - bits));
}

/* One word of the message through the Michael block function. */
static inline void wlanMichaelBlock(uint32_t *left, uint32_t *right, uint32_t word)
{
	uint32_t l = *left ^ word;
	uint32_t r = *right;

	r ^= rotateRight(l, 15);			/* rotate left by 17 */
	l += r;
	r ^= ((l & 0xff00ff00) >> 8) | ((l & 0x00ff00ff) << 8);
	l += r;
	r ^= rotateRight(l, 29);			/* rotate left by 3 */
	l += r;
	r ^= rotateRight(l, 2);
	l += r;
	*left = l;
	*right = r;
}

/*
 * 0x2e8d4: the MIC of a frame's data (mic[0], mic[1]) under the key's two words. The data
 * is the frame's body after its 96 bytes of header and security fields; the padding is
 * written after it. The addresses are taken as 802.11 says: the destination is address 1
 * (address 3 to the distribution system), the source address 2 (address 3 from the
 * distribution system, address 4 between two of them).
 */
void wlanMichaelMic(const uint8_t *frame, const uint8_t *key, const uint8_t *data, uint32_t *mic)
{
	const WlanWirelessFrame *wireless = (const WlanWirelessFrame *)frame;
	const uint16_t *destination;
	const uint16_t *source;
	const uint32_t *keyWords = (const uint32_t *)key;
	const uint32_t *dataWords = (const uint32_t *)data;
	uint32_t header[4];
	uint16_t *addresses = (uint16_t *)header;
	uint32_t left;
	uint32_t right;
	unsigned int words;
	unsigned int i;
	uint8_t control = wireless->frameControl[1];
	uint32_t length;

	destination = (const uint16_t *)((control & 1) ? wireless->address3 : wireless->address1);
	if (!(control & 2))
		source = (const uint16_t *)wireless->address2;
	else if (control & 1)
		source = (const uint16_t *)wireless->address4;
	else
		source = (const uint16_t *)wireless->address3;
	addresses[0] = destination[0];
	addresses[1] = destination[1];
	addresses[2] = destination[2];
	addresses[3] = source[0];
	addresses[4] = source[1];
	addresses[5] = source[2];
	header[3] = 0;				/* the priority, and three reserved zero bytes */

	length = (wireless->bodyLength[1] << 8 | wireless->bodyLength[0]) - 96;
	words = wlanMichaelPad((uint8_t *)data, length) >> 2;
	left = keyWords[0];
	right = keyWords[1];
	for (i = 0; i < 4; i++)
		wlanMichaelBlock(&left, &right, header[i]);
	for (i = 0; i < words; i++)
		wlanMichaelBlock(&left, &right, dataWords[i]);
	mic[0] = left;
	mic[1] = right;
}
