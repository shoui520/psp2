/*
 * libsha256.h: the SDK's SHA-256 context (SDK Release 03.570.011, target/include/libsha256.h); the kernel's
 * SceKernelUtilsForDriver keeps the same layout.
 */
#ifndef SCE_LIBSHA256_H
#define SCE_LIBSHA256_H

#include <scetypes.h>

#define SCE_SHA256_BLOCK_SIZE		64U
#define SCE_SHA256_DIGEST_SIZE		32U

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

#endif /* SCE_LIBSHA256_H */
