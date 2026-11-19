/* The frame builder of the transmit task: a host frame becomes the frames the MAC sends. */
#ifndef ROBIN_TXBUILD_H
#define ROBIN_TXBUILD_H

#include <stdint.h>
#include "robin/buffer.h"
#include "robin/mactx.h"
#include "robin/wep.h"

/* The most frames one host frame is cut into. */
#define WLAN_TX_FRAGMENTS_MAX	16

/* The ciphers wlanTxCipherOverhead() chooses between (what wlanTxBuildFrame() does to each fragment). */
enum {
	WLAN_TX_CIPHER_NONE = 0,
	WLAN_TX_CIPHER_WEP = 1,		/* encrypted here with the default key */
	WLAN_TX_CIPHER_TKIP = 2,	/* encrypted here, with the Michael MIC after the last fragment */
	WLAN_TX_CIPHER_AES = 3,		/* the crypto engine encrypts it */
	WLAN_TX_CIPHER_WAPI = 4,
};

/* What a cipher adds to a frame: all of it, what goes in front of the payload and what comes after. */
struct WlanTxCipherOverhead {
	uint8_t total;
	uint8_t header;
	uint8_t trailer;
};

/* Where one fragment's payload comes from and goes to (the buffer is made over in place);
 * packed: Sony reads and writes its fields with __aeabi_uread4 and __aeabi_uwrite4. */
struct __attribute__((packed)) WlanTxFragment {
	uint32_t length;
	const uint8_t *source;
	uint8_t *destination;
};

/* The first 32 bytes of a frame as the transmit descriptor holds them: its length and the header. */
struct WlanTxFragmentHeader {
	uint16_t length;
	uint8_t frameControl[2];
	uint16_t duration;
	uint8_t address1[6];
	uint8_t reserved12[12];
	uint16_t sequenceControl;
	uint8_t reserved26[6];
};

_Static_assert(sizeof(struct WlanTxFragmentHeader) == 32, "frame header");

uint8_t *wlanTxBuildFrame(struct MacTxFrameBuild *build, OsaBuffer *buffer, int *drop, int *eapol,
			  int reserved, unsigned int packetType, int ampdu,
			  const uint8_t **key, int qos);			/* 0x2ed44 */
int wlanTxEncryptWepFragment(uint8_t *fragment, uint32_t length, const WlanWepHostConfig *config); /* 0x4a828 */
int wlanTxCipherOverhead(int wep, int tkip, int aes, int wapi, uint8_t *cipher,
			 struct WlanTxCipherOverhead *overhead);		/* 0x4a89c */
int wlanTxClassifyPayload(unsigned int packetType, const uint8_t *payload, uint8_t *offset, int *drop,
			  uint16_t *length, uint8_t *snap, unsigned int *snapLength); /* 0x4a8f4 */
unsigned int wlanTxFragmentCount(uint16_t *threshold, int qos, unsigned int cipherOverhead,
				 unsigned int payloadLength, unsigned int snapLength); /* 0x4a9aa */
void wlanTxFragmentLayout(unsigned int count, struct WlanTxFragment *fragments, uint32_t *destinations,
			  uint32_t *lengths, uintptr_t *cursor, unsigned int snapLength,
			  const struct WlanTxCipherOverhead *overhead, uint16_t *remaining,
			  unsigned int offset, unsigned int threshold, const uint8_t **payload); /* 0x4a9f8 */
uint32_t wlanTxFlagsProtect(uint32_t flags);					/* 0x4aad4 */
void wlanTxFragmentsMove(const struct WlanTxFragment *fragments, unsigned int last); /* 0x4aaea */

#endif /* ROBIN_TXBUILD_H */
