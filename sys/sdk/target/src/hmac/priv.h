/*
 * libSceHmac's private header (the SDK's target/src/hmac/priv.h): the hash an HMAC runs on,
 * one descriptor per algorithm (md5.c, sha1.c, ...), and the common code they share.
 */
#ifndef SCE_HMAC_PRIV_H
#define SCE_HMAC_PRIV_H

#include <libhmac.h>

typedef struct SceHmacHashAlgorithm {
	SceUInt32 uiContextSize;	/* the hash's context: the key buffer follows it */
	SceUInt32 (*pGetBlockSize)(const void *pContext);
	SceUInt32 (*pGetDigestSize)(const void *pContext);
	SceInt32 (*pDigest)(const void *plain, SceUInt32 len, SceUChar8 *digest);
	SceInt32 (*pBlockInit)(void *pContext);
	SceInt32 (*pBlockUpdate)(void *pContext, const void *plain, SceUInt32 len);
	SceInt32 (*pBlockResult)(void *pContext, SceUChar8 *digest);
} SceHmacHashAlgorithm;

/* the largest block and digest, and hash context, of the algorithms */
#define SCE_HMAC_BLOCK_SIZE_MAX		128
#define SCE_HMAC_DIGEST_SIZE_MAX	64
#define SCE_HMAC_CONTEXT_SIZE_MAX	276

SceInt32 _sceHmacCommonDigest(const void *secretkey, SceUInt32 keylen, const void *plain,
			      SceUInt32 datalen, SceUChar8 *digest,
			      const SceHmacHashAlgorithm *pHashAlg);
SceInt32 _sceHmacCommonBlockInit(void *pContext, const void *secretkey, SceUInt32 keylen,
				 const SceHmacHashAlgorithm *pHashAlg);
SceInt32 _sceHmacCommonBlockUpdate(void *pContext, const void *plain, SceUInt32 len,
				   const SceHmacHashAlgorithm *pHashAlg);
SceInt32 _sceHmacCommonBlockResult(void *pContext, SceUChar8 *digest,
				   const SceHmacHashAlgorithm *pHashAlg);

#endif
