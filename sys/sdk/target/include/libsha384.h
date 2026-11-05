/*
 * libsha384.h: the SDK's SHA-384 context and interface (SDK Release 03.570.011, target/include/libsha384.h).
 */
#ifndef _SCE_SHA384_LIBSHA384_H
#define _SCE_SHA384_LIBSHA384_H

#include <scetypes.h>

#define SCE_SHA384_BLOCK_SIZE		128U
#define SCE_SHA384_DIGEST_SIZE		48U

#define SCE_SHA384_ERROR_INVALID_POINTER	-2142763520	/* 0x80480600 */
#define SCE_SHA384_ERROR_INVALID_STATE		-2142763519	/* 0x80480601 */

typedef struct SceSha384Context {
	SceUInt64	h[8];
	SceUInt32	pad;
	SceUInt16	usRemains;
	SceUInt16	usComputed;
	SceUInt64	ullTotalLen;
	SceUChar8	buf[SCE_SHA384_BLOCK_SIZE];
	SceUChar8	result[SCE_SHA384_DIGEST_SIZE];
} SceSha384Context;

#ifndef __cplusplus
_Static_assert(sizeof(SceSha384Context) == 0x100, "SceSha384Context size");
#endif

SceInt32 sceSha384Digest(const void *plain, SceUInt32 len, SceUChar8 *digest);
int sceSha384BlockInit(SceSha384Context *pContext);
SceInt32 sceSha384BlockUpdate(SceSha384Context *pContext, const void *plain, SceUInt32 len);
SceInt32 sceSha384BlockResult(SceSha384Context *pContext, SceUChar8 *digest);

#endif /* _SCE_SHA384_LIBSHA384_H */
