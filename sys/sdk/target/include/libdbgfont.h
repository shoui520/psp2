/*
 * Debug font library interface (SceDbgFont): 8x8 and 16x16 bitmap text overlay.
 */
#ifndef _SCE_DBGFONT_LIBDBGFONT_H
#define _SCE_DBGFONT_LIBDBGFONT_H

#include <scetypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SCE_DBGFONT_ERROR_NOT_INIT		(-2141782016)	/* 0x80570000 */
#define SCE_DBGFONT_ERROR_EXISTS		(-2141782015)	/* 0x80570001 */
#define SCE_DBGFONT_ERROR_INVALID_FONTSIZE	(-2141782014)	/* 0x80570002 */
#define SCE_DBGFONT_ERROR_INVALID_PIXELFORMAT	(-2141782013)	/* 0x80570003 */
#define SCE_DBGFONT_ERROR_INVALID_ADDRESS	(-2141782012)	/* 0x80570004 */
#define SCE_DBGFONT_ERROR_FONT_OVER		(-2141782011)	/* 0x80570005 */
#define SCE_DBGFONT_ERROR_INVALID_POINTER	(-2141782010)	/* 0x80570006 */
#define SCE_DBGFONT_ERROR_INVALID_VALUE		(-2141782009)	/* 0x80570007 */

#define SCE_DBGFONT_FONTSIZE_DEFAULT		(0)
#define SCE_DBGFONT_FONTSIZE_LARGE		(1)
#define SCE_DBGFONT_FONTCOUNT_MAX		(4096)
#define SCE_DBGFONT_PIXELFORMAT_A8B8G8R8	(0x00000000U)

typedef struct SceDbgFontConfig {
	SceInt32 fontSize;
} SceDbgFontConfig;

typedef struct SceDbgFontFrameBufInfo {
	SceUChar8 *frameBufAddr;
	SceInt32 frameBufPitch;
	SceUInt32 frameBufPixelformat;
	SceInt32 frameBufWidth;
	SceInt32 frameBufHeight;
} SceDbgFontFrameBufInfo;

SceInt32 sceDbgFontInit(const SceDbgFontConfig *config);
SceInt32 sceDbgFontExit(void);
SceInt32 sceDbgFontPrint(SceInt32 x, SceInt32 y, SceUInt32 color, const SceChar8 *string);
SceInt32 sceDbgFontFlush(const SceDbgFontFrameBufInfo *info);

#ifdef __cplusplus
}
#endif

#endif
