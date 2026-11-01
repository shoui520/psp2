/*
 * libbase64.h: under Sony's names and with Sony's definitions
 * (SDK Release 03.570.011, target/include/libbase64.h).
 */
#ifndef _SCE_LIBBASE64_H_
#define _SCE_LIBBASE64_H_

#include <scetypes.h>

#if defined(_LANGUAGE_C_PLUS_PLUS)||defined(__cplusplus)||defined(c_plusplus)
extern "C" {
#endif

#define SCE_BASE64_ENC_BUF_SIZE(_size) (((_size) + 2) / 3 * 4 + 1)
#define SCE_BASE64_DEC_BUF_SIZE(_size) ((_size) / 4 * 3 + 1)

SceInt32 sceBase64Encoder(char *pDst, const void *pSrc, SceUInt32 uiSiz );
SceInt32 sceBase64Decoder(void *pDst, const char *pSrc, SceUInt32 uiSiz );

#define SCE_BASE64_ERROR_INVALID_FORMAT		-2138898431	/* 0x80830001 */
#define SCE_BASE64_ERROR_INVALID_VALUE		-2138898430	/* 0x80830002 */

#if defined(_LANGUAGE_C_PLUS_PLUS)||defined(__cplusplus)||defined(c_plusplus)
}
#endif

#endif
