/*
 * WAPI (WPI-SMS4) for frames the firmware encrypts or decrypts itself: the
 * keys, the 48-byte AAD and the packet number go into the CCM engine's
 * register block (robin/ccm.h), which does the cipher. A WAPI frame carries
 * a two-byte header (key index, reserved) and a 16-byte packet number
 * before the data, and a 16-byte MIC after it: 34 bytes in all.
 */
#ifndef ROBIN_WAPI_H
#define ROBIN_WAPI_H

#include <stdint.h>
#include "robin/buffer.h"
#include "robin/esa.h"
#include "robin/wlan.h"
#include "robin/wlanrx.h"

#define WLAN_WAPI_OVERHEAD		34

/* Counts kept for the receive path (0x0400105c..0x04001070). */
struct WlanWapiCounters {
	uint32_t replays;		/* frames whose packet number was not newer */
	uint32_t replacementKeys;	/* frames that switched to the replacement key */
	uint32_t unknownKeys;		/* frames with a key index no key has */
	uint32_t frames;
	uint32_t qosFrames;
};

extern struct WlanWapiCounters wlanWapiCounters;

int wlanAddressIsBroadcast(const uint8_t *address);			/* 0x3f402 */
void wlanWapiEncryptFrame(int qos, unsigned int priority, uint8_t *frame, unsigned int length,
			  cipher_key_wapi_t *key);			/* 0x3f428 */
int wlanWapiDecryptFrame(unsigned int priority, uint8_t *frame, unsigned int length,
			 cipher_key_wapi_t *key);			/* 0x3f494 */
int wlanWapiSend(uint8_t *frame, int unused, unsigned int priority, cipher_key_t *key);	/* 0x3f514 */
int wlanWapiReceive(OsaBuffer *buffer, struct WlanRxStatus *status, cipher_key_t *key);	/* 0x3f58c */
int wlanWapiLoadEngine(int decrypt, int qos, const uint8_t *packetNumber, const uint8_t *aad,
		       void *source, unsigned int length, const uint8_t *dataKey,
		       const uint8_t *micKey, void *destination, uint32_t block);	/* 0x3f684 */
void wlanWapiBuildAad(int qos, unsigned int priority, unsigned int length, uint8_t *aad,
		      const uint8_t *frame, uint8_t keyIndex);		/* 0x3f726 */
void wlanWapiNextPacketNumber(cipher_key_wapi_t *key, uint8_t *output);	/* 0x3f84a */
int wlanWapiCheckPacketNumber(int notStation, cipher_key_wapi_t *key, const uint8_t *packetNumber);	/* 0x3f87c */
void wlanReverseBytes(uint8_t *output, const uint8_t *input, uint8_t length);	/* 0x3f8ba */

/* SQRAM 0xc0008292, not converted: applies and frees a replacement key. */
void wlanSqramKeyApply(uint32_t key);

#endif /* ROBIN_WAPI_H */
