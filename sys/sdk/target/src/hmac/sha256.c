/* libSceHmac's HMAC-SHA256 (the SDK's sha256.o) */
#include <libsha256.h>
#include "priv.h"

static SceUInt32 _sceHmacSha256GetBlockSize(const void *pContext)
{
	(void)pContext;
	return SCE_SHA256_BLOCK_SIZE;
}

static SceUInt32 _sceHmacSha256GetDigestSize(const void *pContext)
{
	(void)pContext;
	return SCE_SHA256_DIGEST_SIZE;
}

static const SceHmacHashAlgorithm s_hashSha256 = {
	sizeof(SceSha256Context),
	_sceHmacSha256GetBlockSize,
	_sceHmacSha256GetDigestSize,
	sceSha256Digest,
	(SceInt32 (*)(void *))sceSha256BlockInit,
	(SceInt32 (*)(void *, const void *, SceUInt32))sceSha256BlockUpdate,
	(SceInt32 (*)(void *, SceUChar8 *))sceSha256BlockResult,
};

SceInt32 sceHmacSha256Digest(const void *secretkey, SceUInt32 keylen, const void *plain,
			     SceUInt32 datalen, SceUChar8 *digest)
{
	return _sceHmacCommonDigest(secretkey, keylen, plain, datalen, digest, &s_hashSha256);
}

SceInt32 sceHmacSha256BlockInit(SceHmacSha256Context *pContext, const void *secretkey,
				SceUInt32 keylen)
{
	return _sceHmacCommonBlockInit(pContext, secretkey, keylen, &s_hashSha256);
}

SceInt32 sceHmacSha256BlockUpdate(SceHmacSha256Context *pContext, const void *plain,
				  SceUInt32 len)
{
	return _sceHmacCommonBlockUpdate(pContext, plain, len, &s_hashSha256);
}

SceInt32 sceHmacSha256BlockResult(SceHmacSha256Context *pContext, SceUChar8 *digest)
{
	return _sceHmacCommonBlockResult(pContext, digest, &s_hashSha256);
}
