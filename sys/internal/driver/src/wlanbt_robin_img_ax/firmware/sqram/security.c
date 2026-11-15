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

static ROBIN_PATCH_SLOT(wlanAesEncryptOverride);			/* 0x55b74 */
static ROBIN_PATCH_SLOT(wlanAesWrapOverride);			/* 0x55b80 */
static ROBIN_PATCH_SLOT(wlanAesUnwrapOverride);			/* 0x55b84 */

/* The initial value of RFC 3394 (0xc000fa60). */
static const uint8_t aesWrapIv[8] __attribute__((section(".sqram1.rw"))) = {
	0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6
};

/* Compares two byte strings without stopping at the first difference: 0 when they are the same, else -1 (0xc0007648). */
__attribute__((noinline)) int sqramBytesEqual(const uint8_t *first, const uint8_t *second, int length)
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
__attribute__((noinline)) uint8_t *sqramMemset(uint8_t *destination, uint8_t value, int length)
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

/* Encrypts one 128-bit block (data) into ret with the key kek, kekLen of 64 bits (0xc00076a6). */
int MRVL_AesEncrypt(uint8_t *kek, uint8_t kekLen, uint8_t *data, uint8_t *ret)
{
	int result;
	OsaBuffer *buffer;
	uint8_t *context;

	if (wlanAesEncryptOverride(kek, kekLen, data, ret, &result) != 0)
		return result;
	buffer = osa_BufferAlloc(ramHook_encrPoolConfig, 400, 0xffffffff);
	context = (uint8_t *)buffer + buffer->dataOffset;
	romAesKeySet(context, kek, kekLen << 6, 1);
	romAesEncryptBlock(context, data, ret);
	osa_BufferFree(buffer);
	return 0;
}

/*
 * Wraps n 64-bit blocks of key data (plain) with the key-encryption key, kekLen of 64 bits, into
 * the n + 1 blocks of cipher; keyIv (or the initial value of RFC 3394) starts the first block
 * (0xc00076fc). -1 when there is nothing to wrap.
 */
int MRVL_AesWrap(uint8_t *kek, uint8_t kekLen, uint32_t n, uint8_t *plain, uint8_t *keyIv, uint8_t *cipher)
{
	int result;
	uint8_t block[16];
	uint8_t a[8];
	uint8_t *blocks;
	int step;

	step = 0;
	if (wlanAesWrapOverride(kek, kekLen, n, plain, keyIv, cipher, &result) != 0)
		return result;
	if (n == 0)
		return -1;
	blocks = cipher + 8;
	sqramMemset(block, 0, 16);
	sqramMemmove(a, keyIv != NULL ? keyIv : aesWrapIv, 8);
	sqramMemmove(cipher + 8, plain, 8 * n);
	do {
		uint8_t *r = blocks;
		uint32_t i;

		for (i = 1; i <= n; i++) {
			sqramMemmove(block, a, 8);
			sqramMemmove(block + 8, r, 8);
			MRVL_AesEncrypt(kek, kekLen, block, block);
			sqramMemmove(a, block, 8);
			a[7] ^= (uint8_t)(n * step + i);
			sqramMemmove(r, block + 8, 8);
			r += 8;
		}
	} while (++step <= 5);
	sqramMemmove(cipher, a, 8);
	return 0;
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
