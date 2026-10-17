/* Sony libhmac.h context, using the SDK's canonical SHA1 owner. */
#ifndef SCE_HMAC_H
#define SCE_HMAC_H

#include <libsha1.h>
#include <stddef.h>

typedef struct SceHmacSha1Context {
	SceSha1Context sha1;
	SceUChar8 keybuf[SCE_SHA1_BLOCK_SIZE];
} SceHmacSha1Context;

_Static_assert(offsetof(SceHmacSha1Context, sha1) == 0, "HMAC SHA1 state");
_Static_assert(offsetof(SceHmacSha1Context, keybuf) == 120, "HMAC SHA1 key");
_Static_assert(sizeof(SceHmacSha1Context) == 184, "HMAC SHA1 context size");

#endif
