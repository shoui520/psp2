/*
 * libSceHmac (the SDK's static library, libhmac.o): HMAC over any of the SDK's hashes. The
 * key is padded to the hash's block (hashed first when it is longer) and XORed with the inner
 * pad; XORing that with 0x6a gives the outer pad (0x36 ^ 0x5c).
 */
#include <libkernel/libkernel.h>
#include <stddef.h>
#include "priv.h"

#define HMAC_IPAD	0x36
#define HMAC_IPAD_TO_OPAD	0x6a	/* the inner pad's key to the outer pad's */

SceInt32 _sceHmacCommonDigest(const void *secretkey, SceUInt32 keylen, const void *plain,
			      SceUInt32 datalen, SceUChar8 *digest,
			      const SceHmacHashAlgorithm *pHashAlg)
{
	SceUChar8 inner[SCE_HMAC_DIGEST_SIZE_MAX];
	SceUChar8 keybuf[SCE_HMAC_BLOCK_SIZE_MAX];
	SceUChar8 context[SCE_HMAC_CONTEXT_SIZE_MAX] __attribute__((aligned(8)));
	SceUInt32 blockSize;
	SceUInt32 i;

	blockSize = pHashAlg->pGetBlockSize(NULL);
	if (plain == NULL || secretkey == NULL || digest == NULL)
		return SCE_HMAC_ERROR_INVALID_POINTER;
	sceClibMemset(keybuf, 0, sizeof(keybuf));
	if (blockSize < keylen)
		pHashAlg->pDigest(secretkey, keylen, keybuf);
	else
		sceClibMemcpy_safe(keybuf, secretkey, keylen);
	for (i = 0; i < blockSize; i++)
		keybuf[i] ^= HMAC_IPAD;
	pHashAlg->pBlockInit(context);
	pHashAlg->pBlockUpdate(context, keybuf, blockSize);
	pHashAlg->pBlockUpdate(context, plain, datalen);
	pHashAlg->pBlockResult(context, inner);
	for (i = 0; i < blockSize; i++)
		keybuf[i] ^= HMAC_IPAD_TO_OPAD;
	pHashAlg->pBlockInit(context);
	pHashAlg->pBlockUpdate(context, keybuf, blockSize);
	pHashAlg->pBlockUpdate(context, inner, pHashAlg->pGetDigestSize(context));
	pHashAlg->pBlockResult(context, digest);
	return 0;
}

SceInt32 _sceHmacCommonBlockInit(void *pContext, const void *secretkey, SceUInt32 keylen,
				 const SceHmacHashAlgorithm *pHashAlg)
{
	SceUInt32 offset = pHashAlg->uiContextSize;
	SceUInt32 blockSize;
	SceUChar8 *keybuf;
	SceUInt32 i;

	blockSize = pHashAlg->pGetBlockSize(pContext);
	if (secretkey == NULL || pContext == NULL)
		return SCE_HMAC_ERROR_INVALID_POINTER;
	keybuf = (SceUChar8 *)pContext + offset;
	sceClibMemset(keybuf, 0, blockSize);
	if (blockSize < keylen)
		pHashAlg->pDigest(secretkey, keylen, keybuf);
	else
		sceClibMemcpy_safe(keybuf, secretkey, keylen);
	for (i = 0; i < blockSize; i++)
		keybuf[i] ^= HMAC_IPAD;
	pHashAlg->pBlockInit(pContext);
	pHashAlg->pBlockUpdate(pContext, keybuf, blockSize);
	return 0;
}

SceInt32 _sceHmacCommonBlockUpdate(void *pContext, const void *plain, SceUInt32 len,
				   const SceHmacHashAlgorithm *pHashAlg)
{
	if (plain == NULL || pContext == NULL)
		return SCE_HMAC_ERROR_INVALID_POINTER;
	pHashAlg->pBlockUpdate(pContext, plain, len);
	return 0;
}

SceInt32 _sceHmacCommonBlockResult(void *pContext, SceUChar8 *digest,
				   const SceHmacHashAlgorithm *pHashAlg)
{
	SceUChar8 inner[SCE_HMAC_DIGEST_SIZE_MAX];
	SceUChar8 keybuf[SCE_HMAC_BLOCK_SIZE_MAX];
	SceUChar8 context[SCE_HMAC_CONTEXT_SIZE_MAX] __attribute__((aligned(8)));
	SceUInt32 offset = pHashAlg->uiContextSize;
	SceUInt32 blockSize;
	SceUInt32 i;

	blockSize = pHashAlg->pGetBlockSize(pContext);
	if (digest == NULL || pContext == NULL)
		return SCE_HMAC_ERROR_INVALID_POINTER;
	pHashAlg->pBlockResult(pContext, inner);
	sceClibMemcpy_safe(keybuf, (SceUChar8 *)pContext + offset, blockSize);
	for (i = 0; i < blockSize; i++)
		keybuf[i] ^= HMAC_IPAD_TO_OPAD;
	pHashAlg->pBlockInit(context);
	pHashAlg->pBlockUpdate(context, keybuf, blockSize);
	pHashAlg->pBlockUpdate(context, inner, pHashAlg->pGetDigestSize(pContext));
	pHashAlg->pBlockResult(context, digest);
	return 0;
}
