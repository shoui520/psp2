/*
 * Security code kept in SQRAM (0xc0007200..): the byte helpers it is written
 * with, the AES key unwrap of RFC 3394 (the ROM has the same function; the
 * patch slot below lets an image replace it), the key lookup in the PMK
 * cache, and the commands that reach it. The file name is inferred.
 */
#include <stddef.h>
#include <stdint.h>
#include "robin/btrom.h"
#include "robin/buffer.h"
#include "robin/conn.h"
#include "robin/esa.h"
#include "robin/patch.h"
#include "robin/rom.h"
#include "robin/sys.h"

static ROBIN_PATCH_SLOT(wlanAesUnwrapOverride);			/* 0x55b84 */

/* The ROM's AES: sets the key up (bits long, for decryption when encrypt is 0), and runs one block. */
void romAesKeySet(void *context, const uint8_t *key, unsigned int bits, int encrypt);	/* 0x03f3be3e */
void romAesDecryptBlock(void *context, const uint8_t *input, uint8_t *output);		/* 0x03f3be6e */
void romAesEncryptBlock(void *context, const uint8_t *input, uint8_t *output);		/* 0x03f3be7e */

/* The initial value of RFC 3394 (0xc000fa60). */
static const uint8_t aesWrapIv[8] __attribute__((section(".sqram1.rw"))) = {
	0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6
};

/* Compares two byte strings without stopping at the first difference: 0 when they are the same, else -1 (0xc0007648). */
int sqramBytesEqual(const uint8_t *first, const uint8_t *second, int length)
{
	int equal = length;

	while (length-- != 0) {
		int a = *first;
		int b = *second;

		++first;
		++second;
		if (a == b)
			--equal;
	}
	if (equal != 0)
		return -1;
	return 0;
}

/* Fills a byte string (0xc000766e). */
uint8_t *sqramMemset(uint8_t *destination, uint8_t value, int length)
{
	while (length-- != 0)
		*destination++ = value;
	return destination;
}

/* Copies a byte string, from the end when the destination is above the source (0xc000767a). */
uint8_t *sqramMemmove(uint8_t *destination, const uint8_t *source, int length)
{
	if (destination >= source) {
		while (length != 0) {
			destination[length - 1] = source[length - 1];
			--length;
		}
	} else {
		while (length != 0) {
			*destination++ = *source++;
			--length;
		}
	}
	return destination;
}

/*
 * Unwraps n 64-bit blocks of key data with the key-encryption key (kekLen of 64 bits): plain gets the
 * blocks, keyIv (or the initial value of RFC 3394) is what the first block of the wrapped data has to
 * have become. 0 when it has, -1 when not.
 */
int MRVL_AesUnWrap(uint8_t *kek, uint8_t kekLen, uint32_t n, uint8_t *cipher, uint8_t *keyIv, uint8_t *plain)
{
	int result;
	uint8_t a[8];
	uint8_t block[16];
	OsaBuffer *buffer;
	uint8_t *context;
	int step;

	if (wlanAesUnwrapOverride(kek, kekLen, n, cipher, keyIv, plain, &result) != 0)
		return result;
	if (n == 0)
		return -1;
	sqramMemset(a, 0, 8);
	sqramMemset(block, 0, 16);
	sqramMemmove(a, cipher, 8);
	sqramMemmove(plain, cipher + 8, 8 * n);
	buffer = osa_BufferAlloc(ramHook_encrPoolConfig, 400, 0xffffffff);
	context = (uint8_t *)buffer + buffer->dataOffset;
	romAesKeySet(context, kek, kekLen << 6, 0);
	for (step = 5; step >= 0; step--) {
		uint8_t *last = &plain[8 * n - 8];
		int i;

		for (i = n; i >= 1; i--) {
			sqramMemmove(block, a, 8);
			block[7] ^= (uint8_t)(n * step + i);
			sqramMemmove(block + 8, last, 8);
			romAesDecryptBlock(context, block, block);
			sqramMemmove(a, block, 8);
			sqramMemmove(last, block + 8, 8);
			last -= 8;
		}
	}
	osa_BufferFree(buffer);
	if (sqramBytesEqual(keyIv != NULL ? keyIv : aesWrapIv, a, 8) != 0)
		return -1;
	return 0;
}

/* Sets the bit of the IBSS connections of the BSS (type and number) that makes them merge with other networks (HostCmd 0x83). */
int hostCmd0083(unsigned int bssType, unsigned int bssNumber, unsigned int merge)
{
	struct WlanConnection *connection = wlanConnectionList;
	int found = 0;
	unsigned int posture = sysIrqDisable();

	while (connection != NULL) {
		if (connection->interfaceType == 1 && connection->bssType == bssType &&
		    connection->bssNumber == bssNumber) {
			found = 1;
			connection->stationFlags = (connection->stationFlags & ~1u) | (merge & 1);
		}
		connection = connection->next;
	}
	sysIrqRestore(posture);
	return found != 0;
}
