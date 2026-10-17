/* SCE SDK-backed HMAC adapters; original source filename is unavailable.
 * Unlike the SDK's generic libhmac implementation, this owner retains the
 * outer key pad and reuses its SHA1 state when producing the final digest.
 */

#include <libhmac.h>
#include <string.h>
#include <sysmem/utils.h>
#if FW_VERSION >= 0x04000000
#include <sysmem/fortify.h>
#endif

int sceNetPsHmacSha1Init(SceHmacSha1Context *pContext,
		       const void *secretkey, unsigned int keylen)
{
	unsigned char digest[SCE_SHA1_DIGEST_SIZE];
	unsigned char innerPad[SCE_SHA1_BLOCK_SIZE + 1];
	SceSha1Context temporary;
	unsigned int i;

	if (keylen > SCE_SHA1_BLOCK_SIZE) {
		SceKernelUtilsForDriver_E4390FFA(&temporary);
		SceKernelUtilsForDriver_478A6F3C(&temporary, secretkey, keylen);
		SceKernelUtilsForDriver_48F24106(&temporary, digest);
		secretkey = digest;
		keylen = sizeof(digest);
	}
	memset(innerPad, 0, sizeof(innerPad));
	memset(pContext->keybuf, 0, sizeof(pContext->keybuf));
	memcpy(innerPad, secretkey, keylen);
	memcpy(pContext->keybuf, secretkey, keylen);
	for (i = 0; i < SCE_SHA1_BLOCK_SIZE; i++) {
		innerPad[i] ^= 0x36;
		pContext->keybuf[i] ^= 0x5c;
	}
	SceKernelUtilsForDriver_E4390FFA(&pContext->sha1);
	SceKernelUtilsForDriver_478A6F3C(&pContext->sha1, innerPad, SCE_SHA1_BLOCK_SIZE);
	return 0;
}

int sceNetPsHmacSha1Update(SceHmacSha1Context *pContext,
			 const void *plain, unsigned int len)
{
	SceKernelUtilsForDriver_478A6F3C(&pContext->sha1, plain, len);
	return 0;
}

int sceNetPsHmacSha1Result(unsigned char *digest,
			 SceHmacSha1Context *pContext)
{
	unsigned char innerDigest[SCE_SHA1_DIGEST_SIZE];

	SceKernelUtilsForDriver_48F24106(&pContext->sha1, innerDigest);
	SceKernelUtilsForDriver_E4390FFA(&pContext->sha1);
	SceKernelUtilsForDriver_478A6F3C(&pContext->sha1, pContext->keybuf,
			   SCE_SHA1_BLOCK_SIZE);
	SceKernelUtilsForDriver_478A6F3C(&pContext->sha1, innerDigest, sizeof(innerDigest));
	SceKernelUtilsForDriver_48F24106(&pContext->sha1, digest);
	return 0;
}
