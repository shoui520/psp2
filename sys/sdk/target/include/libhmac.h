/*
 * libhmac.h: the SDK's HMAC contexts and the HMAC-SHA1 and HMAC-SHA256 calls (SDK Release
 * 03.570.011, target/include/libhmac.h), using the SDK's SHA headers.
 */
#ifndef SCE_HMAC_H
#define SCE_HMAC_H

#include <libsha1.h>
#include <libsha256.h>
#include <stddef.h>

#define SCE_HMAC_ERROR_INVALID_POINTER		-2142760960	/* 0x80481000 */
#define SCE_HMAC_ERROR_INVALID_STATE		-2142760959	/* 0x80481001 */
#define SCE_HMAC_ERROR_INVALID_DIGEST_SIZE	-2142760958	/* 0x80481002 */

#define SCE_HMAC_SHA1_DIGEST_SIZE	20U
#define SCE_HMAC_SHA256_DIGEST_SIZE	32U

typedef struct SceHmacSha1Context {
	SceSha1Context sha1;
	SceUChar8 keybuf[SCE_SHA1_BLOCK_SIZE];
} SceHmacSha1Context;

typedef struct SceHmacSha256Context {
	SceSha256Context sha256;
	SceUChar8 keybuf[SCE_SHA256_BLOCK_SIZE];
} SceHmacSha256Context;

#ifndef __cplusplus
_Static_assert(offsetof(SceHmacSha1Context, sha1) == 0, "HMAC SHA1 state");
_Static_assert(offsetof(SceHmacSha1Context, keybuf) == 120, "HMAC SHA1 key");
_Static_assert(sizeof(SceHmacSha1Context) == 184, "HMAC SHA1 context size");
_Static_assert(sizeof(SceHmacSha256Context) == 208, "HMAC SHA256 context size");
#endif

#ifndef PSP2_KERNEL
#if defined(_LANGUAGE_C_PLUS_PLUS)||defined(__cplusplus)||defined(c_plusplus)
extern "C" {
#endif

SceInt32 sceHmacSha1Digest(const void *secretkey, SceUInt32 keylen, const void *plain,
			   SceUInt32 datalen, SceUChar8 *digest);
SceInt32 sceHmacSha1BlockInit(SceHmacSha1Context *pContext, const void *secretkey,
			      SceUInt32 keylen);
SceInt32 sceHmacSha1BlockUpdate(SceHmacSha1Context *pContext, const void *plain,
				SceUInt32 len);
SceInt32 sceHmacSha1BlockResult(SceHmacSha1Context *pContext, SceUChar8 *digest);

SceInt32 sceHmacSha256Digest(const void *secretkey, SceUInt32 keylen, const void *plain,
			     SceUInt32 datalen, SceUChar8 *digest);
SceInt32 sceHmacSha256BlockInit(SceHmacSha256Context *pContext, const void *secretkey,
				SceUInt32 keylen);
SceInt32 sceHmacSha256BlockUpdate(SceHmacSha256Context *pContext, const void *plain,
				  SceUInt32 len);
SceInt32 sceHmacSha256BlockResult(SceHmacSha256Context *pContext, SceUChar8 *digest);

#if defined(_LANGUAGE_C_PLUS_PLUS)||defined(__cplusplus)||defined(c_plusplus)
}
#endif
#endif

#endif
