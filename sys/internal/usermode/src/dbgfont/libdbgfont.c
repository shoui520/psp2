/*
 * Debug font library (sys/internal/usermode/src/dbgfont/libdbgfont.c):
 * 8x8 and 16x16 bitmap text overlay rendered directly to an A8B8G8R8 framebuffer.
 */
#include <stddef.h>
#include <scetypes.h>
#include <sceerror.h>
#include <kernel/threadmgr.h>
#include <libdbgfont.h>
#include "dbgfont.h"

#define DBGFONT_GLYPH_SIZE_DEFAULT	8
#define DBGFONT_GLYPH_SIZE_LARGE	16
#define DBGFONT_ASCII_FIRST		0x20

typedef struct SceDbgFont {
	SceInt32 x;
	SceInt32 y;
	SceUInt32 color;
	SceChar8 c;
	SceUChar8 pad[3];
} SceDbgFont;

static SceInt32 s_dbgfont_size;
static SceDbgFont s_dbgfont_buf[SCE_DBGFONT_FONTCOUNT_MAX];
static SceUInt32 s_dbgfont_cnt;
static SceUInt32 s_dbgfont_initialized;
static SceUID s_dbgfont_mutex_id;

SceInt32 sceDbgFontInit(const SceDbgFontConfig *config)
{
	SceInt32 ret;

	if (config == NULL)
		return SCE_DBGFONT_ERROR_INVALID_POINTER;
	if (s_dbgfont_initialized != 0)
		return SCE_DBGFONT_ERROR_EXISTS;

	s_dbgfont_cnt = 0;
	if (config->fontSize == SCE_DBGFONT_FONTSIZE_DEFAULT)
		s_dbgfont_size = DBGFONT_GLYPH_SIZE_DEFAULT;
	else if (config->fontSize == SCE_DBGFONT_FONTSIZE_LARGE)
		s_dbgfont_size = DBGFONT_GLYPH_SIZE_LARGE;
	else
		return SCE_DBGFONT_ERROR_INVALID_FONTSIZE;

	s_dbgfont_mutex_id = -1;
	ret = sceKernelCreateMutex("SceDbgFontMutex", SCE_KERNEL_MUTEX_ATTR_TH_PRIO, 0, NULL);
	if (ret < 0)
		return ret;

	s_dbgfont_mutex_id = ret;
	s_dbgfont_initialized = 1;
	return SCE_OK;
}

SceInt32 sceDbgFontExit(void)
{
	SceInt32 ret;

	s_dbgfont_cnt = 0;
	s_dbgfont_size = DBGFONT_GLYPH_SIZE_DEFAULT;
	ret = sceKernelDeleteMutex(s_dbgfont_mutex_id);
	if (ret < 0)
		return ret;

	s_dbgfont_mutex_id = -1;
	s_dbgfont_initialized = 0;
	return SCE_OK;
}

SceInt32 sceDbgFontPrint(SceInt32 x, SceInt32 y, SceUInt32 color, const SceChar8 *string)
{
	SceInt32 ret;
	SceUInt32 strlength;
	SceUInt32 fontcnt;
	const SceChar8 *straddr;
	SceDbgFont *fontbuf;
	SceInt32 fx;
	SceInt32 fy;
	SceChar8 code;

	if (string == NULL)
		return SCE_DBGFONT_ERROR_INVALID_POINTER;
	if (s_dbgfont_initialized == 0)
		return SCE_DBGFONT_ERROR_NOT_INIT;

	straddr = string;
	strlength = 0;
	while (*straddr != '\0') {
		if (*straddr != '\n')
			strlength++;
		straddr++;
	}

	fontcnt = s_dbgfont_cnt;
	if (strlength > SCE_DBGFONT_FONTCOUNT_MAX || fontcnt >= SCE_DBGFONT_FONTCOUNT_MAX)
		return SCE_DBGFONT_ERROR_FONT_OVER;

	ret = sceKernelLockMutex(s_dbgfont_mutex_id, 1, NULL);
	if (ret < 0)
		return ret;

	fx = x;
	fy = y;
	straddr = string;
	fontbuf = &s_dbgfont_buf[fontcnt];

	while ((code = *straddr++) != '\0') {
		if (code == '\n') {
			fy += s_dbgfont_size;
			fx = x;
		} else {
			fontbuf->x = fx;
			fontbuf->y = fy;
			fontbuf->color = color;
			fontbuf->c = (SceChar8)(code - DBGFONT_ASCII_FIRST);
			fontbuf++;
			fontcnt++;
			fx += s_dbgfont_size;
			if (fontcnt == SCE_DBGFONT_FONTCOUNT_MAX)
				break;
		}
	}

	s_dbgfont_cnt = fontcnt;
	ret = sceKernelUnlockMutex(s_dbgfont_mutex_id, 1);
	if (ret < 0)
		return ret;

	return (SceInt32)fontcnt;
}

SceInt32 sceDbgFontFlush(const SceDbgFontFrameBufInfo *info)
{
	SceInt32 ret;
	SceUInt32 fontcnt;
	SceDbgFont *fontbuf;
	SceInt32 fx;
	SceInt32 fy;
	SceUInt32 color;
	SceChar8 code;
	SceUInt32 data;
	SceUInt32 mask;
	SceInt32 u;
	SceInt32 v;
	SceUShort16 *pixel;
	SceUShort16 *nextRowPixel;

	if (info == NULL)
		return SCE_DBGFONT_ERROR_INVALID_POINTER;
	if (s_dbgfont_initialized == 0)
		return SCE_DBGFONT_ERROR_NOT_INIT;
	if (info->frameBufAddr == NULL)
		return SCE_DBGFONT_ERROR_INVALID_ADDRESS;
	if (info->frameBufWidth <= 0 || info->frameBufHeight <= 0 || info->frameBufPitch <= 0)
		return SCE_DBGFONT_ERROR_INVALID_VALUE;

	fontcnt = s_dbgfont_cnt;
	if (fontcnt == 0)
		return SCE_OK;

	ret = sceKernelLockMutex(s_dbgfont_mutex_id, 1, NULL);
	if (ret < 0)
		return ret;

	fontbuf = s_dbgfont_buf;
	data = 0;
	mask = 0;

	while (fontcnt != 0) {
		fx = fontbuf->x;
		fy = fontbuf->y;
		if (fy + s_dbgfont_size <= info->frameBufHeight &&
		    fx + s_dbgfont_size <= info->frameBufWidth &&
		    fy >= 0 && fx >= 0) {
			color = fontbuf->color;
			code = fontbuf->c;
			for (v = 0; v < s_dbgfont_size; v++) {
				if (v == 0) {
					data = fonttex[code * 2];
					mask = 1;
				} else if (v == s_dbgfont_size / 2) {
					data = fonttex[code * 2 + 1];
					mask = 1;
				}

				for (u = 0; u < s_dbgfont_size; u++) {
					pixel = (SceUShort16 *)info->frameBufAddr +
						((fy + v) * info->frameBufPitch + fx + u) * 2;
					if (s_dbgfont_size == DBGFONT_GLYPH_SIZE_DEFAULT) {
						if ((data & mask) != 0) {
							pixel[0] = (SceUShort16)color;
							pixel[1] = (SceUShort16)(color >> 16);
						}
						mask <<= 1;
					} else {
						if ((data & mask) != 0) {
							nextRowPixel = (SceUShort16 *)info->frameBufAddr +
								((fy + v + 1) * info->frameBufPitch + fx + u) * 2;
							pixel[0] = (SceUShort16)color;
							pixel[1] = (SceUShort16)(color >> 16);
							nextRowPixel[0] = (SceUShort16)color;
							nextRowPixel[1] = (SceUShort16)(color >> 16);
						}
						if ((v % 2) == 1 && (u % 2) == 1)
							mask <<= 1;
					}
				}
			}
		}
		fontbuf++;
		fontcnt--;
	}

	s_dbgfont_cnt = 0;
	ret = sceKernelUnlockMutex(s_dbgfont_mutex_id, 1);
	if (ret < 0)
		return ret;

	return SCE_OK;
}
