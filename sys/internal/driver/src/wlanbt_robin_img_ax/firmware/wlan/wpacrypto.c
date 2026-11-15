/*
 * RC4 (rc4.c) and the PBKDF2 block function of the pass phrase hash
 * (pass_phrase.c) of Marvell's ESA: RC4 decrypts the key data of EAPOL-Key
 * frames of the old key descriptor, PBKDF2-HMAC-SHA1 derives the
 * pre-shared key from a pass phrase and the SSID. The names are Marvell's
 * (agent/ref/marvell-esa-8977/esa/common); the two files are one here, the
 * linker having put their objects together.
 */
#include <stddef.h>
#include <stdint.h>
#include "robin/esa.h"
#include "robin/rom.h"
#include "robin/runtime.h"

#define A_SHA_DIGEST_LEN	20

typedef struct rc4_key {
	uint8_t state[256];
	uint8_t x;
	uint8_t y;
} rc4_key;

static rc4_key rc4key __attribute__((section(".itcm.bss")));			/* 0x57324 */

static void swap_byte(uint8_t *a, uint8_t *b);

void prepare_key(uint8_t *key_data_ptr, int key_data_len, rc4_key *key)
{
	uint8_t index1;
	uint8_t index2;
	uint8_t *state;
	int16_t counter;

	state = &key->state[0];
	for (counter = 0; counter < 256; counter++)
		state[counter] = counter;
	key->x = 0;
	key->y = 0;
	index1 = 0;
	index2 = 0;
	for (counter = 0; counter < 256; counter++) {
		index2 = (key_data_ptr[index1] + state[counter] + index2) % 256;
		swap_byte(&state[counter], &state[index2]);
		index1 = (index1 + 1) % key_data_len;
	}
}

void rc4(uint8_t *buffer_ptr, int buffer_len, int skip, rc4_key *key)
{
	uint8_t x;
	uint8_t y;
	uint8_t *state;
	uint8_t xorIndex;
	int16_t counter;

	x = key->x;
	y = key->y;
	state = &key->state[0];
	for (counter = 0; counter < skip; counter++) {
		x = (x + 1) % 256;
		y = (state[x] + y) % 256;
		swap_byte(&state[x], &state[y]);
	}
	for (counter = 0; counter < buffer_len; counter++) {
		x = (x + 1) % 256;
		y = (state[x] + y) % 256;
		swap_byte(&state[x], &state[y]);
		xorIndex = (state[x] + state[y]) % 256;
		buffer_ptr[counter] ^= state[xorIndex];
	}
	key->x = x;
	key->y = y;
}

static __attribute__((noinline)) void swap_byte(uint8_t *a, uint8_t *b)
{
	uint8_t swapByte;

	swapByte = *a;
	*a = *b;
	*b = swapByte;
}

void RC4_Encrypt(uint8_t *Encr_Key, uint8_t *IV, uint16_t iv_length, uint8_t *Data,
		 uint16_t data_length, uint16_t skipBytes)
{
	uint8_t key[32];

	if (iv_length + 16 > (int)sizeof(key))
		return;
	__aeabi_memcpy(key, IV, iv_length);
	__aeabi_memcpy(key + iv_length, Encr_Key, 16);
	prepare_key(key, iv_length + 16, &rc4key);
	rc4(Data, data_length, skipBytes, &rc4key);
}

/*
 * F(P, S, c, i) = U1 xor U2 xor ... Uc
 * U1 = PRF(P, S || Int(i))
 * U2 = PRF(P, U1)
 * Uc = PRF(P, Uc-1)
 */
void Mrvl_F(char *password, uint8_t *ssid, int ssidlength, int iterations, int count, uint8_t *output)
{
	static uint8_t digest[36] __attribute__((section(".itcm.bss")));		/* 0x57426 */
	static uint8_t digest1[A_SHA_DIGEST_LEN] __attribute__((section(".itcm.bss")));	/* 0x5744a */
	int i, j;
	int len = strlen(password);
	int tmpLen = ssidlength + 4;
	uint8_t *pTemp = digest;

	/* U1 = PRF(P, S || int(i)) */
	__aeabi_memcpy(digest, ssid, ssidlength);
	digest[ssidlength] = (uint8_t)((count >> 24) & 0xff);
	digest[ssidlength + 1] = (uint8_t)((count >> 16) & 0xff);
	digest[ssidlength + 2] = (uint8_t)((count >> 8) & 0xff);
	digest[ssidlength + 3] = (uint8_t)(count & 0xff);
	Mrvl_hmac_sha1(&pTemp, &tmpLen, 1, (uint8_t *)password, len, digest1, A_SHA_DIGEST_LEN);

	/* output = U1 */
	__aeabi_memcpy(output, digest1, A_SHA_DIGEST_LEN);
	pTemp = digest1;
	for (i = 1; i < iterations; i++) {
		tmpLen = A_SHA_DIGEST_LEN;

		/* Un = PRF(P, Un-1) */
		Mrvl_hmac_sha1(&pTemp, &tmpLen, 1, (uint8_t *)password, len, digest, A_SHA_DIGEST_LEN);
		__aeabi_memcpy(digest1, digest, A_SHA_DIGEST_LEN);

		/* output = output xor Un */
		for (j = 0; j < A_SHA_DIGEST_LEN; j++)
			output[j] ^= digest[j];
	}
}

/*
 * password - ascii string up to 63 characters in length
 * ssid - octet string up to 32 octets
 * ssidlength - length of ssid in octets
 * output must be 32 octets in length and outputs 256 bits of key
 */
int Mrvl_PasswordHash(char *password, uint8_t *ssid, int ssidlength, uint8_t *output)
{
	if (strlen(password) > 63 || ssidlength > 32)
		return 0;

	Mrvl_F(password, ssid, ssidlength, 4096, 2, output);
	__aeabi_memcpy(output + A_SHA_DIGEST_LEN, output, 12);
	Mrvl_F(password, ssid, ssidlength, 4096, 1, output);
	return 1;
}
