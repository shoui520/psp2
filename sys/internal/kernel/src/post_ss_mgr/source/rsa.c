/*
 * RSA-2048 (file name unknown): signatures made and checked in the kernel,
 * on a bignum library of 32-bit words.
 */

#include "post_ss_mgr_internal.h"

#define RSA2048_WORDS	0x40	/* names unknown */
#define RSA2048_BYTES	0x100

/* the source of random bytes the RSA code uses */
int rsaNullRandom(void *buf, SceSize size)	/* name unknown */
{
	(void)buf;
	(void)size;
	return 0;
}

int (*g_rsaRandom)(void *buf, SceSize size) = rsaNullRandom;	/* name unknown */

/* PLACEHOLDER: cryptographic routine, deferred: makes an RSA
 * signature of a hash with a private key (with the bignum routines at 0xc8bc and
 * 0xcc78-0xd7a4) */
POST_SS_DEFERRED int rsaCreateSignature(void *signature, const void *hash, const void *key,
					int words, SceSize size, int type)	/* name unknown */
{
	(void)signature;
	(void)hash;
	(void)key;
	(void)words;
	(void)size;
	(void)type;
	return -1;
}

int ksceSblRSA2048CreateSignature(void *signature, const void *hash, const void *key, int type)
{
	return rsaCreateSignature(signature, hash, key, RSA2048_WORDS, RSA2048_BYTES, type);
}

/* PLACEHOLDER: cryptographic routine, deferred: checks an RSA
 * signature of a hash with a public key */
POST_SS_DEFERRED int rsaVerifySignature(const void *signature, const void *hash, const void *key,
					int words, SceSize size, int type)	/* name unknown */
{
	(void)signature;
	(void)hash;
	(void)key;
	(void)words;
	(void)size;
	(void)type;
	return -1;
}

int ksceSblRSA2048VerifySignature(const void *signature, const void *hash, const void *key,
				  int type)
{
	return rsaVerifySignature(signature, hash, key, RSA2048_WORDS, RSA2048_BYTES, type);
}

int sceSblRsaSetRandom(int (*random)(void *buf, SceSize size))
{
	g_rsaRandom = random;
	return 0;
}

/* a big-endian number of size bytes as words, lowest first, zero-padded to count words */
int postSsCopyKey(void *dst, SceSize count, const void *src, SceSize size)
{
	SceUInt32 *word = (SceUInt32 *)dst - 1;
	const SceUInt8 *start = src;
	const SceUInt8 *end = start + size;
	SceUInt32 shift = 32;

	if ((int)(count * 4) < (int)size)
		start = end - count * 4;
	for (; start < end; end--) {
		if (shift == 32) {
			shift = 0;
			*++word = 0;
		}
		*word |= (SceUInt32)end[-1] << (shift & 0xff);
		shift += 8;
	}
	while (++word < (SceUInt32 *)dst + count)
		*word = 0;
	return 0;
}
