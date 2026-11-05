/*
 * libsha512.h: the SDK's SHA-512 context and interface (SDK Release 03.570.011, target/include/libsha512.h).
 */
#ifndef _SCE_SHA512_LIBSHA512_H
#define _SCE_SHA512_LIBSHA512_H

#include <scetypes.h>

#define SCE_SHA512_BLOCK_SIZE		128U
#define SCE_SHA512_DIGEST_SIZE		64U

#define SCE_SHA512_ERROR_INVALID_POINTER	-2142763264	/* 0x80480700 */
#define SCE_SHA512_ERROR_INVALID_STATE		-2142763263	/* 0x80480701 */

typedef struct SceSha512Context {
	SceUInt64	h[8];
	SceUInt32	pad;
	SceUInt16	usRemains;
	SceUInt16	usComputed;
	SceUInt64	ullTotalLen;
	SceUChar8	buf[SCE_SHA512_BLOCK_SIZE];
	SceUChar8	result[SCE_SHA512_DIGEST_SIZE];
} SceSha512Context;

#ifndef __cplusplus
_Static_assert(sizeof(SceSha512Context) == 0x110, "SceSha512Context size");
#endif

SceInt32 sceSha512Digest(const void *plain, SceUInt32 len, SceUChar8 *digest);
int sceSha512BlockInit(SceSha512Context *pContext);
SceInt32 sceSha512BlockUpdate(SceSha512Context *pContext, const void *plain, SceUInt32 len);
SceInt32 sceSha512BlockResult(SceSha512Context *pContext, SceUChar8 *digest);

#endif /* _SCE_SHA512_LIBSHA512_H */
