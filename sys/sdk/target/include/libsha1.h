/*
 * libsha1.h: the SDK's SHA-1 context (SDK Release 03.570.011, target/include/libsha1.h); the kernel's
 * SceKernelUtilsForDriver keeps the same layout.
 */
#ifndef SCE_LIBSHA1_H
#define SCE_LIBSHA1_H

#include <scetypes.h>

#define SCE_SHA1_BLOCK_SIZE		64U
#define SCE_SHA1_DIGEST_SIZE		20U

typedef struct SceSha1Context {
	SceUInt32	h[5];
	SceUInt16	usRemains;
	SceUInt16	usComputed;
	SceUInt64	ullTotalLen;
	SceUChar8	buf[SCE_SHA1_BLOCK_SIZE];
	SceUChar8	result[SCE_SHA1_DIGEST_SIZE];
	SceUInt32	pad;
} SceSha1Context;

#ifndef __cplusplus
_Static_assert(sizeof(SceSha1Context) == 0x78, "SceSha1Context size");
#endif

#endif /* SCE_LIBSHA1_H */
