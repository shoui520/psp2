/* libSceHmac's HMAC-SHA1 (the SDK's sha1.o) */
#include <libsha1.h>
#include "priv.h"

static SceUInt32 _sceHmacSha1GetBlockSize(const void *pContext)
{
	(void)pContext;
	return SCE_SHA1_BLOCK_SIZE;
}

static SceUInt32 _sceHmacSha1GetDigestSize(const void *pContext)
{
	(void)pContext;
	return SCE_SHA1_DIGEST_SIZE;
}

static const SceHmacHashAlgorithm s_hashSha1 = {
	sizeof(SceSha1Context),
	_sceHmacSha1GetBlockSize,
	_sceHmacSha1GetDigestSize,
	sceSha1Digest,
	(SceInt32 (*)(void *))sceSha1BlockInit,
	(SceInt32 (*)(void *, const void *, SceUInt32))sceSha1BlockUpdate,
	(SceInt32 (*)(void *, SceUChar8 *))sceSha1BlockResult,
};

SceInt32 sceHmacSha1Digest(const void *secretkey, SceUInt32 keylen, const void *plain,
			   SceUInt32 datalen, SceUChar8 *digest)
{
	return _sceHmacCommonDigest(secretkey, keylen, plain, datalen, digest, &s_hashSha1);
}

SceInt32 sceHmacSha1BlockInit(SceHmacSha1Context *pContext, const void *secretkey,
			      SceUInt32 keylen)
{
	return _sceHmacCommonBlockInit(pContext, secretkey, keylen, &s_hashSha1);
}

SceInt32 sceHmacSha1BlockUpdate(SceHmacSha1Context *pContext, const void *plain,
				SceUInt32 len)
{
	return _sceHmacCommonBlockUpdate(pContext, plain, len, &s_hashSha1);
}

SceInt32 sceHmacSha1BlockResult(SceHmacSha1Context *pContext, SceUChar8 *digest)
{
	return _sceHmacCommonBlockResult(pContext, digest, &s_hashSha1);
}
