/*
 * SceNpCommonPs4: the SHA-256 and the HMAC over it (crypto/sha256.c, crypto/hmac.c).
 */
#ifndef NP_COMMON_PS4_CRYPTO_H
#define NP_COMMON_PS4_CRYPTO_H

#include "../np_common_ps4_internal.h"

/* the functions of a hash an HMAC is built on, with the offset of the key block in the HMAC context */
struct NpCommonPs4HashAlgorithm {
	SceSize contextOffset;
	SceSize (*getBlockSize)();
	SceSize (*getDigestSize)(void *context);
	int (*hashKey)(const void *data, SceSize size, void *digest);
	int (*init)(void *context);
	int (*update)(void *context, const void *data, SceSize size);
	int (*final)(void *context, void *digest);
};

/* the SHA-256 context (144 bytes): the state, the number of bytes hashed, the block being filled (its last byte holds
   the number of bytes in it) and the digest made by the last block */
struct NpCommonPs4Sha256Context {
	SceUInt32 state[8];
	SceUInt8 reserved[8];
	SceUInt64 length;
	SceUInt8 buffer[64];
	SceUInt32 digest[8];
};

extern const struct NpCommonPs4HashAlgorithm g_npCommonPs4Sha256Algorithm;	/* 0x81029ca0 */
extern SceUInt32 g_npCommonPs4Sha256Init[8];				/* 0x8102abd8 */
extern const SceUInt32 g_npCommonPs4Sha256K[64];				/* 0x8102abf8 */

SceSize npCommonPs4Sha256GetBlockSize(void);
SceSize npCommonPs4Sha256GetDigestSize(void);
int npCommonPs4Sha256HashKey(const void *data, SceSize size, void *digest);
int npCommonPs4Sha256Init(struct NpCommonPs4Sha256Context *context);
int npCommonPs4Sha256Update(struct NpCommonPs4Sha256Context *context, const void *data, SceSize size);
int npCommonPs4Sha256Final(struct NpCommonPs4Sha256Context *context, void *digest);
const void *npCommonPs4Sha256Transform(const void *data, SceUInt32 blocks, SceUInt32 *state);

int npCommonPs4Hmac(const void *key, SceSize keySize, const void *data, SceSize dataSize, void *digest,
	const struct NpCommonPs4HashAlgorithm *algorithm);
int npCommonPs4HmacInit(void *context, const void *key, SceSize keySize, const struct NpCommonPs4HashAlgorithm *algorithm);
int npCommonPs4HmacUpdate(void *context, const void *data, SceSize size, const struct NpCommonPs4HashAlgorithm *algorithm);
int npCommonPs4HmacFinal(void *context, void *digest, const struct NpCommonPs4HashAlgorithm *algorithm);
int npAsmHmacSha256(const void *key, SceSize keySize, const void *data, SceSize dataSize, void *digest);
int npAsmHmacSha256Init(void *context, const void *key, SceSize keySize);
int npAsmHmacSha256Update(void *context, const void *data, SceSize size);
int npAsmHmacSha256Final(void *context, void *digest);

#endif
