/*
 * libmd5.h: under Sony's names and with Sony's definitions
 * (SDK Release 03.570.011, target/include/libmd5.h).
 */
#ifndef _SCE_MD5_LIBMD5_H
#define _SCE_MD5_LIBMD5_H

#include <scetypes.h>

#if defined(_LANGUAGE_C_PLUS_PLUS)||defined(__cplusplus)||defined(c_plusplus)
#if !defined(__LANGUAGE_ASSEMBLY)
extern "C" {
#endif
#endif

#define SCE_MD5_ERROR_INVALID_POINTER			-2142764800	/* 0x80480100 */

#define SCE_MD5_ERROR_INVALID_STATE				-2142764799	/* 0x80480101 */

#define SCE_MD5_BLOCK_SIZE		64U
#define SCE_MD5_DIGEST_SIZE		16U

#if !defined(__LANGUAGE_ASSEMBLY)

typedef struct SceMd5Context {
	SceUInt32		h[4];
	SceUInt32		pad;
	SceUInt16		usRemains;
	SceUInt16		usComputed;
	SceUInt64		ullTotalLen;
	SceUChar8		buf[SCE_MD5_BLOCK_SIZE];
	SceUChar8		result[SCE_MD5_DIGEST_SIZE];
} SceMd5Context;

SceInt32 sceMd5Digest(const void *plain, SceUInt32 len, unsigned char *digest);

SceInt32 sceMd5BlockInit(SceMd5Context *pContext);

SceInt32 sceMd5BlockUpdate(SceMd5Context *pContext, const void *plain, SceUInt32 len);

SceInt32 sceMd5BlockResult(SceMd5Context *pContext, SceUChar8 *digest);

#endif

#if defined(_LANGUAGE_C_PLUS_PLUS)||defined(__cplusplus)||defined(c_plusplus)
#if !defined(__LANGUAGE_ASSEMBLY)
}
#endif
#endif

#endif
