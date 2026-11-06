/*
 * ces/error.h: the character encoding library's error codes, under Sony's names and with Sony's
 * definitions (SDK Release 03.570.011, target/include/ces/error.h).
 */
#ifndef _SCE_CES_ERROR_H
#define _SCE_CES_ERROR_H

#include <sceerror.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SCE_CES_OK				SCE_OK

#define SCE_CES_ERROR_INVALID_PARAMETER		-2141454335	/* (0x805C0001) */
#define SCE_CES_ERROR_INVALID_PROFILE		-2141454332	/* (0x805C0004) */
#define SCE_CES_ERROR_INVALID_SRC_BUFFER	-2141454320	/* (0x805C0010) */
#define SCE_CES_ERROR_SRC_BUFFER_END		-2141454319	/* (0x805C0011) */
#define SCE_CES_ERROR_INVALID_ENCODE		-2141454316	/* (0x805C0014) */
#define SCE_CES_ERROR_ILLEGAL_CODE		-2141454315	/* (0x805C0015) */
#define SCE_CES_ERROR_UNASSIGNED_CODE		-2141454304	/* (0x805C0020) */
#define SCE_CES_ERROR_OUT_OF_CODE_RANGE		-2141454300	/* (0x805C0024) */
#define SCE_CES_ERROR_INVALID_DST_BUFFER	-2141454288	/* (0x805C0030) */
#define SCE_CES_ERROR_DST_BUFFER_END		-2141454287	/* (0x805C0031) */

#ifdef __cplusplus
}
#endif

#endif /* _SCE_CES_ERROR_H */
