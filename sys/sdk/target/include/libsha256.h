/*
 * libsha256.h: the SDK's SHA-256 context (SDK Release 03.570.011, target/include/libsha256.h); the kernel's
 * SceKernelUtilsForDriver keeps the same layout.
 */
#ifndef SCE_LIBSHA256_H
#define SCE_LIBSHA256_H

#include <scetypes.h>

#define SCE_SHA256_BLOCK_SIZE		64U
#define SCE_SHA256_DIGEST_SIZE		32U

#define SCE_SHA256_ERROR_INVALID_POINTER	-2142763776	/* 0x80480500 */
#define SCE_SHA256_ERROR_INVALID_STATE		-2142763775	/* 0x80480501 */

typedef struct SceSha256Context {
	SceUInt32	h[8];
	SceUInt32	pad;
	SceUInt16	usRemains;
	SceUInt16	usComputed;
	SceUInt64	ullTotalLen;
	SceUChar8	buf[SCE_SHA256_BLOCK_SIZE];
	SceUChar8	result[SCE_SHA256_DIGEST_SIZE];
} SceSha256Context;

#ifndef __cplusplus
_Static_assert(sizeof(SceSha256Context) == 0x90, "SceSha256Context size");
#endif

/* the SDK's user library; the kernel declares its own in sysmem/utils.h */
#ifndef PSP2_KERNEL
SceInt32 sceSha256Digest(const void *plain, SceUInt32 len, SceUChar8 *digest);
SceInt32 sceSha256BlockInit(SceSha256Context *pContext);
SceInt32 sceSha256BlockUpdate(SceSha256Context *pContext, const void *plain, SceUInt32 len);
SceInt32 sceSha256BlockResult(SceSha256Context *pContext, SceUChar8 *digest);
#endif

#endif /* SCE_LIBSHA256_H */
