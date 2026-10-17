/*
 * libsha224.h: the SDK's SHA-224 context (SDK Release 03.570.011, target/include/libsha224.h); the kernel's
 * SceKernelUtilsForDriver keeps the same layout.
 */
#ifndef SCE_LIBSHA224_H
#define SCE_LIBSHA224_H

#include <scetypes.h>

#define SCE_SHA224_BLOCK_SIZE		64U
#define SCE_SHA224_DIGEST_SIZE		28U

typedef struct SceSha224Context {
	SceUInt32	h[8];
	SceUInt32	pad;
	SceUInt16	usRemains;
	SceUInt16	usComputed;
	SceUInt64	ullTotalLen;
	SceUChar8	buf[SCE_SHA224_BLOCK_SIZE];
	SceUChar8	result[SCE_SHA224_DIGEST_SIZE];
	SceUInt32	pad2;
} SceSha224Context;

#ifndef __cplusplus
_Static_assert(sizeof(SceSha224Context) == 0x90, "SceSha224Context size");
#endif

#endif /* SCE_LIBSHA224_H */
