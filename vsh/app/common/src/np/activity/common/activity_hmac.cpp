/*
 * HMAC-SHA1 (RFC 2104) on the SDK's libsha1. No binary names the file; the name is ours.
 */
#include <libkernel/libkernel.h>
#include <libsha1.h>
#include "activity_hmac.h"

/* the pads' bytes; the outer pad is made from the inner one */
#define ACTIVITY_HMAC_IPAD		0x36
#define ACTIVITY_HMAC_OPAD_FROM_IPAD	(0x36 ^ 0x5c)

void activityHmacSha1(const void *key, SceSize keySize, const void *data, SceSize size,
	SceUInt8 *digest)
{
	SceUInt8 inner[SCE_SHA1_DIGEST_SIZE];
	SceUInt8 pad[SCE_SHA1_BLOCK_SIZE];
	SceSha1Context context;
	int i;

	sceClibMemset(pad, 0, sizeof(pad));
	if (keySize > SCE_SHA1_BLOCK_SIZE)
		sceSha1Digest(key, keySize, pad);
	else
		sceClibMemcpy_safe(pad, key, keySize);

	for (i = 0; i < SCE_SHA1_BLOCK_SIZE; i++)
		pad[i] ^= ACTIVITY_HMAC_IPAD;
	sceSha1BlockInit(&context);
	sceSha1BlockUpdate(&context, pad, SCE_SHA1_BLOCK_SIZE);
	sceSha1BlockUpdate(&context, data, size);
	sceSha1BlockResult(&context, inner);

	for (i = 0; i < SCE_SHA1_BLOCK_SIZE; i++)
		pad[i] ^= ACTIVITY_HMAC_OPAD_FROM_IPAD;
	sceSha1BlockInit(&context);
	sceSha1BlockUpdate(&context, pad, SCE_SHA1_BLOCK_SIZE);
	sceSha1BlockUpdate(&context, inner, SCE_SHA1_DIGEST_SIZE);
	sceSha1BlockResult(&context, digest);
}
