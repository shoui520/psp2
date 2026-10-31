/*
 * libsha1.h: the SDK's SHA-1 context (SDK Release 03.570.011, target/include/libsha1.h); the kernel's
 * SceKernelUtilsForDriver keeps the same layout.
 */
#ifndef SCE_LIBSHA1_H
#define SCE_LIBSHA1_H

#include <scetypes.h>

#define SCE_SHA1_BLOCK_SIZE		64U
#define SCE_SHA1_DIGEST_SIZE		20U

#define SCE_SHA1_ERROR_INVALID_POINTER	-2142764288	/* 0x80480300 */
#define SCE_SHA1_ERROR_INVALID_STATE	-2142764287	/* 0x80480301 */

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

/* the SDK's user library; the kernel declares its own in sysmem/utils.h */
#ifndef PSP2_KERNEL
SceInt32 sceSha1Digest(const void *plain, SceUInt32 len, SceUChar8 *digest);
SceInt32 sceSha1BlockInit(SceSha1Context *pContext);
SceInt32 sceSha1BlockUpdate(SceSha1Context *pContext, const void *plain, SceUInt32 len);
SceInt32 sceSha1BlockResult(SceSha1Context *pContext, SceUChar8 *digest);
#endif

#endif /* SCE_LIBSHA1_H */
