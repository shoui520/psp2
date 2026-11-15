/*
 * SHA-1 and HMAC-SHA1 as the firmware has them (Marvell's common/sha1.h,
 * sha1.c and hmac_sha1.c, without the private-data argument).
 */
#ifndef ROBIN_SHA1_H
#define ROBIN_SHA1_H

#include <stdint.h>

#ifndef A_SHA_DIGEST_LEN
#define A_SHA_DIGEST_LEN 20
#endif

enum {
	shaSuccess = 0,
	shaNull,		/* Null pointer parameter */
	shaInputTooLong,	/* input data too long */
	shaStateError		/* called Input after Result */
};

typedef struct {
	uint32_t Intermediate_Hash[A_SHA_DIGEST_LEN / 4];	/* Message Digest */
	uint32_t Length_Low;		/* Message length in bits */
	uint32_t Length_High;		/* Message length in bits */
	uint32_t Scratch[16];		/* the transform's working space */
	uint8_t Message_Block[64];	/* 512-bit message blocks */
	int16_t Message_Block_Index;	/* Index into message block array */
	uint8_t Computed;		/* Is the digest computed? */
	uint8_t Corrupted;		/* Is the message digest corrupted? */
} Mrvl_SHA1_CTX;

_Static_assert(sizeof(Mrvl_SHA1_CTX) == 160, "SHA-1 context");

int Mrvl_SHA1Init(Mrvl_SHA1_CTX *context);					/* 0x4c410 */
int Mrvl_SHA1Update(Mrvl_SHA1_CTX *context, const uint8_t *message, unsigned int length);	/* 0x4c76c */
int Mrvl_SHA1Final(Mrvl_SHA1_CTX *context, uint8_t *digest);			/* 0x4c704 */

void Mrvl_hmac_sha1(uint8_t **ppText, int *pTextLen, int textNum, uint8_t *key,
		    int key_len, uint8_t *output, int outputLen);		/* 0x2db58 */
void Mrvl_PRF(uint8_t *key, int key_len, uint8_t *prefix, int prefix_len,
	      uint8_t *data, int data_len, uint8_t *output, int len);		/* 0x2dc5c */

#endif /* ROBIN_SHA1_H */
