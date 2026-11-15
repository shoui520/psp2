/*
 * MD5 as the firmware has it (Marvell's common/md5.h): the Mask ROM's three
 * entry points, without the private-data argument Marvell's source passes.
 */
#ifndef ROBIN_MD5_H
#define ROBIN_MD5_H

#include <stdint.h>

typedef struct {
	uint32_t state[4];		/* state (ABCD) */
	uint32_t count[2];		/* number of bits, modulo 2^64 (lsb first) */
	uint32_t scratch[16];		/* the transform's working space */
	uint8_t buffer[64];		/* input buffer */
} Mrvl_MD5_CTX;

_Static_assert(sizeof(Mrvl_MD5_CTX) == 152, "MD5 context");
_Static_assert(__builtin_offsetof(Mrvl_MD5_CTX, buffer) == 88, "MD5 input buffer");

void wpa_MD5Init(Mrvl_MD5_CTX *context);					/* 0x03f3ae09 (veneer 0x531d4) */
void wpa_MD5Update(Mrvl_MD5_CTX *context, const uint8_t *input, uint32_t inputLen);	/* 0x03f3b511 (0x531dc) */
void wpa_MD5Final(uint8_t *digest, Mrvl_MD5_CTX *context);			/* 0x03f3b58d (0x531e4) */

void Mrvl_hmac_md5(uint8_t *text_data, int text_len, uint8_t *key, int key_len, uint8_t *digest);

#endif /* ROBIN_MD5_H */
