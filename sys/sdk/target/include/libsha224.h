/*
 * libsha224.h: the SDK's SHA-224 context (SDK Release 03.570.011, target/include/libsha224.h); the kernel's
 * SceKernelUtilsForDriver keeps the same layout.
 */
#ifndef SCE_LIBSHA224_H
#define SCE_LIBSHA224_H

#include <scetypes.h>

#define SCE_SHA224_BLOCK_SIZE		64U
#define SCE_SHA224_DIGEST_SIZE		28U

#define SCE_SHA224_ERROR_INVALID_POINTER	-2142764032	/* 0x80480400 */
#define SCE_SHA224_ERROR_INVALID_STATE		-2142764031	/* 0x80480401 */

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

/* the SDK's user library; the kernel declares its own in sysmem/utils.h */
#ifndef PSP2_KERNEL
SceInt32 sceSha224Digest(const void *plain, SceUInt32 len, SceUChar8 *digest);
SceInt32 sceSha224BlockInit(SceSha224Context *pContext);
SceInt32 sceSha224BlockUpdate(SceSha224Context *pContext, const void *plain, SceUInt32 len);
SceInt32 sceSha224BlockResult(SceSha224Context *pContext, SceUChar8 *digest);
#endif

#endif /* SCE_LIBSHA224_H */
