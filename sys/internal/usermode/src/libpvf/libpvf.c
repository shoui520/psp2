/*
 * SceLibPvf: the PVF (and other FreeType) font library, on libSceFt2's FreeType.
 *
 * No debug record or SDK library names its files or internal functions; the names below are
 * ours, from the SDK's libpvf.h vocabulary (agent/namecheck records them).
 */

#include <kernel/iofilemgr.h>
#include <kernel/sysmem/memblock.h>
#include <libkernel/libkernel.h>
#include <libsha1.h>
#include <sysmem/memblock.h>
#include <sysmem/sysmem.h>
#include <font/libpvf.h>
#include <libSceFt2/libSceFt2.h>
#include "libpvf_internal.h"

#ifdef SCE_PVF_2014
#define sceClibMemcpy_safe	sceClibMemcpy
#endif

/* the system fonts: sa0:data/font/pvf/<fileName> */
static const ScePvf_t_fontStyleInfo pvfSystemFonts[PVF_NUM_SYSTEM_FONTS] = {
	{ .familyCode = SCE_PVF_FAMILY_SANSERIF, .style = SCE_PVF_STYLE_REGULAR,
	  .languageCode = SCE_PVF_LANGUAGE_J,
	  .countryCode = SCE_PVF_COUNTRY_JAPAN, .fileName = "jpn0.pvf" },
	{ .familyCode = SCE_PVF_FAMILY_SERIF, .style = SCE_PVF_STYLE_REGULAR,
	  .languageCode = SCE_PVF_LANGUAGE_J,
	  .countryCode = SCE_PVF_COUNTRY_JAPAN, .fileName = "jpn1.pvf" },
	{ .familyCode = SCE_PVF_FAMILY_SANSERIF, .style = SCE_PVF_STYLE_BOLD,
	  .languageCode = SCE_PVF_LANGUAGE_J,
	  .countryCode = SCE_PVF_COUNTRY_JAPAN, .fileName = "jpn2.pvf" },
	{ .familyCode = SCE_PVF_FAMILY_SERIF, .style = SCE_PVF_STYLE_BOLD,
	  .languageCode = SCE_PVF_LANGUAGE_J,
	  .countryCode = SCE_PVF_COUNTRY_JAPAN, .fileName = "jpn3.pvf" },
	{ .familyCode = SCE_PVF_FAMILY_SANSERIF, .style = SCE_PVF_STYLE_REGULAR,
	  .languageCode = SCE_PVF_LANGUAGE_LATIN,
	  .countryCode = SCE_PVF_COUNTRY_JAPAN, .fileName = "ltn0.pvf" },
	{ .familyCode = SCE_PVF_FAMILY_SERIF, .style = SCE_PVF_STYLE_REGULAR,
	  .languageCode = SCE_PVF_LANGUAGE_LATIN,
	  .countryCode = SCE_PVF_COUNTRY_JAPAN, .fileName = "ltn1.pvf" },
	{ .familyCode = SCE_PVF_FAMILY_SANSERIF, .style = SCE_PVF_STYLE_OBLIQUE,
	  .languageCode = SCE_PVF_LANGUAGE_LATIN,
	  .countryCode = SCE_PVF_COUNTRY_JAPAN, .fileName = "ltn2.pvf" },
	{ .familyCode = SCE_PVF_FAMILY_SERIF, .style = SCE_PVF_STYLE_OBLIQUE,
	  .languageCode = SCE_PVF_LANGUAGE_LATIN,
	  .countryCode = SCE_PVF_COUNTRY_JAPAN, .fileName = "ltn3.pvf" },
	{ .familyCode = SCE_PVF_FAMILY_SANSERIF, .style = SCE_PVF_STYLE_BOLD,
	  .languageCode = SCE_PVF_LANGUAGE_LATIN,
	  .countryCode = SCE_PVF_COUNTRY_JAPAN, .fileName = "ltn4.pvf" },
	{ .familyCode = SCE_PVF_FAMILY_SERIF, .style = SCE_PVF_STYLE_BOLD,
	  .languageCode = SCE_PVF_LANGUAGE_LATIN,
	  .countryCode = SCE_PVF_COUNTRY_JAPAN, .fileName = "ltn5.pvf" },
	{ .familyCode = SCE_PVF_FAMILY_SANSERIF, .style = SCE_PVF_STYLE_BOLD_OBLIQUE,
	  .languageCode = SCE_PVF_LANGUAGE_LATIN,
	  .countryCode = SCE_PVF_COUNTRY_JAPAN, .fileName = "ltn6.pvf" },
	{ .familyCode = SCE_PVF_FAMILY_SERIF, .style = SCE_PVF_STYLE_BOLD_OBLIQUE,
	  .languageCode = SCE_PVF_LANGUAGE_LATIN,
	  .countryCode = SCE_PVF_COUNTRY_JAPAN, .fileName = "ltn7.pvf" },
	{ .familyCode = SCE_PVF_FAMILY_SANSERIF, .style = SCE_PVF_STYLE_REGULAR,
	  .languageCode = SCE_PVF_LANGUAGE_K,
	  .countryCode = SCE_PVF_COUNTRY_KOREA, .fileName = "kr0.pvf" },
	{ .familyCode = SCE_PVF_FAMILY_SERIF, .style = SCE_PVF_STYLE_REGULAR,
	  .languageCode = SCE_PVF_LANGUAGE_K,
	  .countryCode = SCE_PVF_COUNTRY_KOREA, .fileName = "kr1.pvf" },
	{ .familyCode = SCE_PVF_FAMILY_SANSERIF, .style = SCE_PVF_STYLE_BOLD,
	  .languageCode = SCE_PVF_LANGUAGE_K,
	  .countryCode = SCE_PVF_COUNTRY_KOREA, .fileName = "kr2.pvf" },
	{ .familyCode = SCE_PVF_FAMILY_SERIF, .style = SCE_PVF_STYLE_BOLD,
	  .languageCode = SCE_PVF_LANGUAGE_K,
	  .countryCode = SCE_PVF_COUNTRY_KOREA, .fileName = "kr3.pvf" },
	{ .familyCode = SCE_PVF_FAMILY_SANSERIF, .style = SCE_PVF_STYLE_REGULAR,
	  .languageCode = SCE_PVF_LANGUAGE_C,
	  .countryCode = SCE_PVF_COUNTRY_JAPAN, .fileName = "cn0.pvf" },
	{ .familyCode = SCE_PVF_FAMILY_SERIF, .style = SCE_PVF_STYLE_REGULAR,
	  .languageCode = SCE_PVF_LANGUAGE_C,
	  .countryCode = SCE_PVF_COUNTRY_JAPAN, .fileName = "cn1.pvf" },
};

/* the init record's memory functions, for the rasterizer */
static void pvfFree(void *userData, void *ptr)
{
	PvfLib *lib = userData;

	lib->init.freeFunc(lib->init.userData, ptr);
}

static void *pvfAlloc(void *userData, ScePvf_t_u32 size)
{
	PvfLib *lib = userData;

	return lib->init.allocFunc(lib->init.userData, size);
}

static void *pvfRealloc(void *userData, void *ptr, ScePvf_t_u32 size)
{
	PvfLib *lib = userData;

	return lib->init.reallocFunc(lib->init.userData, ptr, size);
}

/* a 4-bit pixel value, saturated */
#define PVF_PIXEL4_MAX	0xF
/* an 8-bit pixel value, saturated */
#define PVF_PIXEL8_MAX	0xFF

/*
 * Adds a glyph's 4-bit image (two pixels a byte, the left one in the low bits) into the user's
 * 4-bit buffer at the buffer's 1/64-pixel reference point, inside the clip rectangle. Each
 * pixel is split between the two buffer pixels it covers, by the point's fraction of a pixel.
 */
__attribute__((used))
static ScePvf_t_s32 pvfDrawGlyphDirect4(const ScePvf_t_u8 *src, ScePvf_t_s32 left,
	ScePvf_t_s32 top, ScePvf_t_s32 width, ScePvf_t_s32 pitch, ScePvf_t_s32 rows,
	const ScePvf_t_userImageBufferRec *imageBuffer, ScePvf_t_s32 clipX, ScePvf_t_s32 clipY,
	ScePvf_t_u32 clipWidth, ScePvf_t_u32 clipHeight)
{
	ScePvf_t_s32 xPos = imageBuffer->xPos64;
	ScePvf_t_s32 y = (imageBuffer->yPos64 >> 6) - top;
	ScePvf_t_s32 x0 = left + (xPos >> 6);
	ScePvf_t_s32 frac = xPos % 64;
	ScePvf_t_u32 yEnd = clipY + clipHeight;
	ScePvf_t_u32 xEnd = clipX + clipWidth;
	ScePvf_t_u8 *line;
	ScePvf_t_s32 odd0, yLast;

	if (frac < 0)
		frac += 64;
	line = imageBuffer->buffer + (x0 >> 1) + imageBuffer->bytesPerLine * y;
	if (yEnd >= imageBuffer->rect.height)
		yEnd = imageBuffer->rect.height;
	if (xEnd >= imageBuffer->rect.width)
		xEnd = imageBuffer->rect.width;
	if (rows <= 0)
		return 0;
	yLast = rows + y;
	odd0 = x0 % 2;
	if (odd0 == -1)
		odd0 = 1;
	for (;; y++, line += imageBuffer->bytesPerLine, src += pitch) {
		const ScePvf_t_u8 *s;
		ScePvf_t_u8 *d;
		ScePvf_t_s32 x, col, odd, high;

		if (y < (clipY < 0 ? 0 : clipY)) {
			if (y >= (ScePvf_t_s32)yEnd)
				return 0;
			goto next;
		}
		if (y >= (ScePvf_t_s32)yEnd)
			return 0;
		if (width <= 0)
			goto next;
		s = src;
		d = line;
		x = x0;
		col = x0 >> 1;
		odd = odd0;
		high = 0;
		do {
			ScePvf_t_u32 v, carry, sum;

			if (high == 1)
				v = *s++ >> 4;
			else
				v = *s & 0xF;
			carry = (frac * v) >> 6;
			if (x < (clipX < 0 ? 0 : clipX)) {
				if (x >= (ScePvf_t_s32)xEnd)
					break;
				if (odd == 1) {
					if (++x >= imageBuffer->rect.width)
						break;
					if (col >= -1) {
						if (++col < imageBuffer->bytesPerLine) {
							sum = (d[1] & 0xF) + carry;
							d[1] = (d[1] & 0xF0) |
								(sum > PVF_PIXEL4_MAX ? PVF_PIXEL4_MAX : sum);
						}
					} else {
						col++;
					}
					d++;
				} else {
					x++;
				}
			} else {
				if (x >= (ScePvf_t_s32)xEnd)
					break;
				if (odd != 1) {
					ScePvf_t_u32 b;

					x++;
					sum = v + (*d & 0xF) - carry;
					b = (*d & 0xF0) | (sum > PVF_PIXEL4_MAX ? PVF_PIXEL4_MAX : sum);
					sum = carry + (b >> 4);
					if (sum > PVF_PIXEL4_MAX)
						sum = PVF_PIXEL4_MAX;
					*d = (b & 0xF) | (sum << 4);
				} else {
					col++;
					sum = v + (*d >> 4) - carry;
					if (sum > PVF_PIXEL4_MAX)
						sum = PVF_PIXEL4_MAX;
					*d = (*d & 0xF) | (sum << 4);
					if (col < imageBuffer->bytesPerLine) {
						x++;
						sum = (d[1] & 0xF) + carry;
						d[1] = (d[1] & 0xF0) |
							(sum > PVF_PIXEL4_MAX ? PVF_PIXEL4_MAX : sum);
						d++;
					} else {
						d++;
						x++;
					}
				}
			}
			odd ^= 1;
			high ^= 1;
		} while (x != width + x0);
next:
		if (y + 1 == yLast)
			return 0;
	}
}

/*
 * Adds a glyph's 8-bit image into the user's 8-bit buffer at the buffer's 1/64-pixel reference
 * point, inside the clip rectangle, each pixel split between the two buffer pixels it covers.
 */
__attribute__((used))
static ScePvf_t_s32 pvfDrawGlyphDirect8(const ScePvf_t_u8 *src, ScePvf_t_s32 left,
	ScePvf_t_s32 top, ScePvf_t_s32 width, ScePvf_t_s32 rows,
	const ScePvf_t_userImageBufferRec *imageBuffer, ScePvf_t_s32 clipX, ScePvf_t_s32 clipY,
	ScePvf_t_u32 clipWidth, ScePvf_t_u32 clipHeight)
{
	ScePvf_t_s32 xPos = imageBuffer->xPos64;
	ScePvf_t_s32 y = (imageBuffer->yPos64 >> 6) - top;
	ScePvf_t_s32 x0 = left + (xPos >> 6);
	ScePvf_t_s32 frac = xPos % 64;
	ScePvf_t_u32 yEnd = clipY + clipHeight;
	ScePvf_t_s32 xEnd, yLast;
	ScePvf_t_u8 *line;

	if (frac < 0)
		frac += 64;
	if (yEnd >= imageBuffer->rect.height)
		yEnd = imageBuffer->rect.height;
	line = imageBuffer->buffer + x0 + imageBuffer->bytesPerLine * y;
	if ((ScePvf_t_u32)(clipX + clipWidth) >= imageBuffer->rect.width)
		xEnd = imageBuffer->rect.width;
	else
		xEnd = clipX + clipWidth;
	if (rows <= 0)
		return 0;
	yLast = rows + y;
	for (;; y++, line += imageBuffer->bytesPerLine, src += width) {
		const ScePvf_t_u8 *s;
		ScePvf_t_u8 *d;
		ScePvf_t_s32 x;

		if (y < (clipY < 0 ? 0 : clipY)) {
			if (y >= (ScePvf_t_s32)yEnd)
				return 0;
			goto next;
		}
		if (y >= (ScePvf_t_s32)yEnd)
			return 0;
		if (width <= 0)
			goto next;
		s = src;
		d = line;
		x = x0;
		do {
			ScePvf_t_u32 carry, sum;

			if (x < (clipX < 0 ? 0 : clipX)) {
				if (x >= xEnd || x + 1 >= imageBuffer->rect.width)
					break;
				if (x >= -1) {
					sum = d[1] + ((frac * *s) >> 6);
					d[1] = sum >= PVF_PIXEL8_MAX ? PVF_PIXEL8_MAX : sum;
				}
			} else {
				if (x >= xEnd)
					break;
				carry = (frac * *s) >> 6;
				sum = *s + d[0] - carry;
				d[0] = sum >= PVF_PIXEL8_MAX ? PVF_PIXEL8_MAX : sum;
				if (x + 1 < imageBuffer->rect.width) {
					sum = d[1] + carry;
					d[1] = sum >= PVF_PIXEL8_MAX ? PVF_PIXEL8_MAX : sum;
				}
			}
			x++;
			s++;
			d++;
		} while (x != width + x0);
next:
		if (y + 1 == yLast)
			return 0;
	}
}

/* where a system font index has no file */
static char pvfNoFileName[8];

/* the length of a name, up to max */
static inline ScePvf_t_s32 pvfStrnlen(const ScePvf_t_s8 *s, ScePvf_t_s32 max)
{
	ScePvf_t_s32 n = 0;

	while (n != max && s[n] != 0)
		n++;
	return n;
}

/* the file name of system font index (the first font's for an index out of range) */
static inline const char *pvfGetSystemFontFileName(PvfLib *lib, ScePvf_t_s32 index,
	ScePvf_t_error *errorCode)
{
	*errorCode = 0;
	if (index >= 0) {
		if (index < scePvfGetNumFontList(lib, errorCode))
			return (const char *)pvfSystemFonts[index].fileName;
		if (*errorCode != 0)
			return pvfNoFileName;
	}
	return (const char *)pvfSystemFonts[0].fileName;
}

/* path: the file of system font index, in the library's font folder */
static ScePvf_t_error pvfGetSystemFontPath(PvfLib *lib, char *path, ScePvf_t_s32 index)
{
	ScePvf_t_error error;
	ScePvf_t_s32 length = pvfStrnlen(lib->fontPath, SCE_PVF_FONTFILENAME_LENGTH);

	length += pvfStrnlen((const ScePvf_t_s8 *)pvfGetSystemFontFileName(lib, index, &error),
		SCE_PVF_FONTFILENAME_LENGTH);
	if (length > SCE_PVF_FONTFILENAME_LENGTH - 1)
		return SCE_PVF_ERROR_NOFILE;
	sceClibStrncpy(path, (const char *)lib->fontPath, PVF_FONT_PATH_SIZE - 2);
	path[PVF_FONT_PATH_SIZE - 2] = '\0';
	path[PVF_PATH_SIZE - 1] = '\0';
	sceClibStrncat(path, pvfGetSystemFontFileName(lib, index, &error),
		SCE_PVF_FONTFILENAME_LENGTH - 1);
	return 0;
}

/*
 * The font a handle opens: one already open the same way gets another reference, else a free
 * slot opens it (a system font by index, a user file by name, or a font image in memory).
 */
#ifdef SCE_PVF_2014
static PvfFont *pvfOpenFont(PvfLib *lib, ScePvf_t_s32 index, const char *fileName,
	const void *buffer, ScePvf_t_s32 size, ScePvf_t_u32 openMode,
	ScePvf_t_error *errorCode)
#else
static PvfFont *pvfOpenFont(PvfLib *lib, ScePvf_t_s32 index, const char *fileName,
	const void *buffer, ScePvf_t_s32 size, ScePvf_t_s32 subFontIndex, ScePvf_t_u32 openMode,
	ScePvf_t_error *errorCode)
#endif
{
	char path[PVF_PATH_SIZE];
	PvfFaceInfo info;
	PvfFont *font;
	ScePvf_t_s32 i, face;

	*errorCode = 0;
	if (openMode == PVF_OPEN_USER_STREAM || openMode == PVF_OPEN_USER_MEMORY) {
		if (fileName == NULL)
			goto badArgument;
	} else if (openMode == PVF_OPEN_USER_BUFFER) {
		if (size == 0 || buffer == NULL)
			goto badArgument;
	} else {
		if (index < 0)
			goto badArgument;
		if (lib == NULL) {
			*errorCode = SCE_PVF_ERROR_LIBID;
			return NULL;
		}
		if (index >= scePvfGetNumFontList(lib, errorCode)) {
			if (*errorCode != 0)
				return NULL;
			goto badArgument;
		}
		if (*errorCode != 0)
			return NULL;
	}

	for (i = 0; i < (ScePvf_t_s32)lib->init.maxNumFonts; i++) {
		font = &lib->fonts[i];
		if (!font->inUse || font->openMode != openMode)
			continue;
		if (openMode == PVF_OPEN_USER_STREAM || openMode == PVF_OPEN_USER_MEMORY) {
			if (sceClibStrncmp(fileName, font->fileName, SCE_PVF_FONTFILENAME_LENGTH) != 0)
				continue;
#ifndef SCE_PVF_2014
			if (font->subFontIndex != subFontIndex)
				continue;
#endif
		} else if (openMode == PVF_OPEN_USER_BUFFER) {
			continue;
		} else if (index != font->fontIndex) {
			continue;
		}
		if (font->refCount == 0xFF)
			continue;
		font->refCount++;
		return &lib->fonts[i];
	}

	for (i = 0; i < (ScePvf_t_s32)lib->init.maxNumFonts; i++) {
		if (!lib->fonts[i].inUse)
			break;
	}
	if (i >= (ScePvf_t_s32)lib->init.maxNumFonts) {
		*errorCode = SCE_PVF_ERROR_TOOMANYOPENED;
		return NULL;
	}
	font = &lib->fonts[i];

	font->systemFile = 0;
	if (openMode == PVF_OPEN_USER_STREAM || openMode == PVF_OPEN_USER_MEMORY) {
		sceClibStrncpy(path, fileName, sizeof(path) - 1);
	} else if (openMode != PVF_OPEN_USER_BUFFER) {
		ScePvf_t_s32 length = pvfStrnlen(lib->fontPath, SCE_PVF_FONTFILENAME_LENGTH);

		length += pvfStrnlen((const ScePvf_t_s8 *)pvfGetSystemFontFileName(lib, index,
			errorCode), SCE_PVF_FONTFILENAME_LENGTH);
		if (length > SCE_PVF_FONTFILENAME_LENGTH - 1) {
			*errorCode = SCE_PVF_ERROR_NOFILE;
			return NULL;
		}
		if (*errorCode != 0)
			return NULL;
		sceClibStrncpy(path, (const char *)lib->fontPath, PVF_FONT_PATH_SIZE - 2);
		path[PVF_FONT_PATH_SIZE - 2] = '\0';
		path[PVF_PATH_SIZE - 1] = '\0';
		sceClibStrncat(path, pvfGetSystemFontFileName(lib, index, errorCode),
			SCE_PVF_FONTFILENAME_LENGTH - 1);
		if (*errorCode != 0)
			return NULL;
		font->systemFile = 1;
	}
	path[sizeof(path) - 1] = '\0';

	switch (openMode) {
	case PVF_OPEN_SYSTEM_MEMORY:
	case PVF_OPEN_USER_MEMORY:
#ifdef SCE_PVF_2014
		face = PvfRasterizerOpenFile(lib->rasterizer, path, 0, errorCode);
#else
		face = PvfRasterizerOpenFile(lib->rasterizer, path, 0, subFontIndex, errorCode);
#endif
		break;
	case PVF_OPEN_USER_BUFFER:
#ifdef SCE_PVF_2014
		face = PvfRasterizerOpenMemory(lib->rasterizer, buffer, size, errorCode);
#else
		face = PvfRasterizerOpenMemory(lib->rasterizer, buffer, size, subFontIndex,
			errorCode);
#endif
		break;
	default:
#ifdef SCE_PVF_2014
		face = PvfRasterizerOpenFile(lib->rasterizer, path, 1, errorCode);
#else
		face = PvfRasterizerOpenFile(lib->rasterizer, path, 1, subFontIndex, errorCode);
#endif
		break;
	}
	if (*errorCode != 0)
		return NULL;
	if (face < 0)
		goto openFailed;

	font->openMode = openMode;
	font->cacheable = 1;
#ifndef SCE_PVF_2014
	font->subFontIndex = subFontIndex;
#endif
	switch (openMode) {
	case PVF_OPEN_USER_STREAM:
	case PVF_OPEN_USER_MEMORY:
		if (lib->nextUserFont == -1)
			goto openFailed;
		sceClibStrncpy(font->fileName, fileName, SCE_PVF_FONTFILENAME_LENGTH - 1);
		font->fileName[SCE_PVF_FONTFILENAME_LENGTH - 1] = '\0';
		font->fontIndex = lib->nextUserFont++;
		break;
	case PVF_OPEN_USER_BUFFER:
		if (lib->nextUserFont == -1)
			goto openFailed;
		font->fileName[0] = '\0';
		font->fontIndex = lib->nextUserFont++;
		break;
	default:
		font->fileName[0] = '\0';
		font->fontIndex = index;
		break;
	}
	font->face = face;
	font->infoValid = 0;
	font->lib = lib;
	PvfRasterizerGetFaceInfo(lib->rasterizer, face, &info);
	font->unitsPerEM = info.unitsPerEM;
	font->numGlyphs = info.numGlyphs;
#ifndef SCE_PVF_2014
	font->numFaces = info.numFaces;
#endif
	sceClibStrncpy(font->familyName, info.familyName, sizeof(font->familyName) - 1);
	font->familyName[sizeof(font->familyName) - 1] = '\0';
	sceClibStrncpy(font->styleName, info.styleName, sizeof(font->styleName) - 1);
	font->styleName[sizeof(font->styleName) - 1] = '\0';
	font->refCount++;
	font->inUse = 1;
	font->infoValid = 1;
	font->metrics.ascender64 = info.ascender << 6;
	font->metrics.descender64 = info.descender << 6;
	font->metrics.horizontalBearingX64 = 0;
	font->metrics.horizontalBearingY64 = 0;
	font->metrics.verticalBearingX64 = 0;
	font->metrics.verticalBearingY64 = 0;
	font->metrics.horizontalAdvance64 = info.maxAdvanceWidth << 6;
	font->metrics.verticalAdvance64 = info.maxAdvanceHeight << 6;
	font->metrics.width64 = 0;
	font->metrics.height64 = info.height << 6;
	lib->numFonts++;
	return font;

badArgument:
	*errorCode = SCE_PVF_ERROR_ARG;
	return NULL;
openFailed:
	*errorCode = SCE_PVF_ERROR_FILEOPEN;
	return NULL;
}

/* forgets the handle's glyph and frees its bitmap */
static inline void pvfClearGlyph(PvfFontHandle *handle)
{
	handle->charInfo.bitmapWidth = 0;
	handle->charInfo.bitmapHeight = 0;
	handle->charInfo.bitmapLeft = 0;
	handle->charInfo.bitmapTop = 0;
	handle->charInfo.glyphMetrics.width64 = 0;
	handle->charInfo.glyphMetrics.height64 = 0;
	handle->charInfo.glyphMetrics.ascender64 = 0;
	handle->charInfo.glyphMetrics.descender64 = 0;
	handle->charInfo.glyphMetrics.horizontalBearingX64 = 0;
	handle->charInfo.glyphMetrics.horizontalBearingY64 = 0;
	handle->charInfo.glyphMetrics.verticalBearingX64 = 0;
	handle->charInfo.glyphMetrics.verticalBearingY64 = 0;
	handle->charInfo.glyphMetrics.horizontalAdvance64 = 0;
	handle->charInfo.glyphMetrics.verticalAdvance64 = 0;
	handle->charInfo.reserved0[1] = 0;
	handle->charInfo.reserved0[0] = 0;
	handle->charInfo.reserved1 = 0;
	if (handle->glyphImage != NULL) {
		PvfLib *lib = handle->font->lib;

		lib->init.freeFunc(lib->init.userData, handle->glyphImage);
		handle->glyphImage = NULL;
	}
}

/* drops the handle's reference to its font, closing the font with the last one */
static inline void pvfReleaseFont(PvfFontHandle *handle)
{
	PvfFont *font = handle->font;

	if (font != NULL && font->inUse) {
		if (--font->refCount == 0 && PvfRasterizerClose(font->lib->rasterizer,
		    font->lib->fonts[font->face].face) == 0) {
			font->inUse = 0;
			font->lib->numFonts--;
		}
		font = handle->font;
		if (handle->glyphImage != NULL)
			font->lib->init.freeFunc(font->lib->init.userData, handle->glyphImage);
	}
	handle->glyphImage = NULL;
}

/* the cache key's hash: four bytes of the key's SHA-1, and the character */
static inline ScePvf_t_u32 pvfCacheHash(const ScePvfCacheKey *key)
{
	ScePvf_t_u8 digest[20];

	sceSha1Digest(key, sizeof(*key), digest);
	return ((digest[7] << 24) | (digest[11] << 16) | (digest[13] << 8) | digest[17]) ^
		key->keyValue1;
}

/* whether the handle's font has charCode, made the handle's character (scePvfIsElement) */
static inline ScePvf_t_bool pvfHasChar(PvfFontHandle *handle, ScePvf_t_charCode charCode)
{
	PvfFont *font;

	if (handle == NULL || charCode == 0xFFFF)
		return 0;
	font = handle->font;
	handle->charCode = charCode;
	return PvfRasterizerHasChar(font->lib->rasterizer, font->face, charCode) != 0;
}

/* sets the rasterizer up for the handle: its resolution, size, skew and emboldening */
static inline void pvfSetRasterizer(PvfFontHandle *handle)
{
	PvfLib *lib = handle->font->lib;

#ifdef SCE_PVF_2014
	PvfRasterizerSetResolution(lib->rasterizer, (FT_UInt)lib->resolution.h,
		(FT_UInt)lib->resolution.v);
#else
	PvfRasterizerSetResolution(lib->rasterizer, lib->resolution.h, lib->resolution.v);
#endif
	PvfRasterizerSetCharSize(handle->font->lib->rasterizer,
		(FT_F26Dot6)(handle->hCharSize * 64.0f), (FT_F26Dot6)(handle->vCharSize * 64.0f));
	PvfRasterizerSetSkew(handle->font->lib->rasterizer, handle->skewAngleX,
		handle->skewAngleY);
	PvfRasterizerSetEmbolden(handle->font->lib->rasterizer,
		(FT_Pos)(handle->emboldenRate * handle->hCharSize * 64.0f / 100.0f));
	lib = handle->font->lib;
	if (PVF_IS_NONZERO(lib->emboldenRate)) {
		PvfRasterizerSetEmbolden(lib->rasterizer, (FT_Pos)(lib->emboldenRate * 64.0f));
		lib = handle->font->lib;
	}
	if (PVF_IS_NONZERO(lib->skewValue))
		PvfRasterizerSetSlant(lib->rasterizer, (FT_Fixed)(lib->skewValue * 65536.0f));
}

/* the handle's glyph metrics from FreeType's */
static inline void pvfSetGlyphMetrics(PvfFontHandle *handle, const FT_Glyph_Metrics *metrics)
{
	handle->charInfo.glyphMetrics.width64 = metrics->width;
	handle->charInfo.glyphMetrics.height64 = metrics->height;
	handle->charInfo.glyphMetrics.ascender64 = metrics->horiBearingY;
	handle->charInfo.glyphMetrics.descender64 = metrics->horiBearingY - metrics->height;
	handle->charInfo.glyphMetrics.horizontalBearingX64 = metrics->horiBearingX;
	handle->charInfo.glyphMetrics.horizontalBearingY64 = metrics->horiBearingY;
	handle->charInfo.reserved0[0] = 0;
	handle->charInfo.reserved0[1] = 0;
	handle->charInfo.reserved1 = 0;
	handle->charInfo.glyphMetrics.verticalBearingX64 = metrics->vertBearingX;
	handle->charInfo.glyphMetrics.verticalBearingY64 = metrics->vertBearingY;
	handle->charInfo.glyphMetrics.horizontalAdvance64 = metrics->horiAdvance;
	handle->charInfo.glyphMetrics.verticalAdvance64 = metrics->vertAdvance;
}

/* the cache key of the handle's glyph, rendered for glyphMode, vertical or not */
static inline void pvfCacheKey(const PvfFontHandle *handle, ScePvfCacheKey *key,
	ScePvf_t_s32 glyphMode, ScePvf_t_s32 vertical)
{
#ifdef SCE_PVF_2014
	(void)vertical;
	key->keyValue0 = handle->font->fontIndex;
#else
	key->keyValue0 = (handle->font->fontIndex & 0x1F) |
		((ScePvf_t_u8)handle->font->subFontIndex << 5);
#endif
	key->keyValue1 = handle->charCode;
	key->keyValue2 = handle->resolution.h * handle->hCharSize;
	key->keyValue3 = handle->resolution.v * handle->vCharSize;
	key->keyValue4 = (float)glyphMode;
#ifdef SCE_PVF_2014
	key->keyValue5 = 0.0f;
#else
	key->keyValue5 = vertical ? 1.0f : 0.0f;
#endif
	key->keyValue6 = handle->skewAngleX;
	key->keyValue7 = handle->skewAngleY;
	key->keyValue8 = handle->emboldenRate;
}

/*
 * Loads the handle's glyph for charCode (the library's alternative character where the font
 * has none), from the library's cache or the rasterizer: its metrics, and its bitmap for depth4
 * (4-bit) or 8-bit pixels. vertical loads the vertical layout's; isVert says whether the glyph
 * has a vertical form.
 */
#ifdef SCE_PVF_2014
static ScePvf_t_error pvfLoadGlyph(PvfFontHandle *handle, ScePvf_t_charCode charCode,
	ScePvf_t_s32 depth4)
#else
static ScePvf_t_error pvfLoadGlyph(PvfFontHandle *handle, ScePvf_t_charCode charCode,
	ScePvf_t_s32 depth4, ScePvf_t_s32 vertical, ScePvf_t_s32 *isVert)
#endif
{
	ScePvf_t_cacheSystemInterface *cache;
	ScePvf_t_bool found;
	ScePvf_t_pointer slot;
	ScePvf_t_error result;
	PvfLib *lib;
#ifdef SCE_PVF_2014
	ScePvf_t_s32 width, rows, top, left, pitch;
#else
	ScePvf_t_s32 width, rows, top, left, pitch, vert;
#endif
	ScePvf_t_u8 *buffer;

	if (!pvfHasChar(handle, charCode)) {
		charCode = handle->font->lib->altCharCode;
		if (charCode == 0)
			return SCE_PVF_ERROR_NOGLYPH;
	}
	found = 0;
	pvfClearGlyph(handle);
	lib = handle->font->lib;
	handle->charCode = charCode;
	handle->resolution = lib->resolution;
	slot = NULL;
	if (handle->font->cacheable && (cache = lib->init.cache) != NULL) {
		ScePvfCacheKey key;

		cache->lockFunc(cache->cacheInstance);
#ifdef SCE_PVF_2014
		pvfCacheKey(handle, &key, depth4, 0);
#else
		pvfCacheKey(handle, &key, depth4, vertical);
#endif
		cache = handle->font->lib->init.cache;
		slot = cache->findFunc(cache->cacheInstance, pvfCacheHash(&key), &key, &found);
		if (!found) {
			cache = handle->font->lib->init.cache;
			if (cache->writeKeyValueToCacheFunc(cache->cacheInstance, slot, &key) < 0)
				slot = NULL;
		}
		if (slot != NULL && found) {
			cache = handle->font->lib->init.cache;
			if (cache->read0FromCacheFunc(cache->cacheInstance, slot,
			    &handle->charInfo) < 0)
				found = 0;
		}
		if (slot != NULL && found) {
			lib = handle->font->lib;
			handle->glyphImage = lib->init.allocFunc(lib->init.userData,
				handle->charInfo.bitmapHeight * handle->charInfo.bitmapPitch);
			if (handle->glyphImage == NULL)
				found = 0;
		}
		if (slot != NULL && found) {
			cache = handle->font->lib->init.cache;
			if (cache->read1FromCacheFunc(cache->cacheInstance, slot,
			    handle->glyphImage) < 0) {
				lib = handle->font->lib;
				lib->init.freeFunc(lib->init.userData, handle->glyphImage);
				handle->glyphImage = NULL;
				found = 0;
			}
		}
		if (slot != NULL && found) {
			result = 0;
			goto unlock;
		}
	}

	pvfSetRasterizer(handle);
	lib = handle->font->lib;
#ifndef SCE_PVF_2014
	if (isVert != NULL)
		*isVert = 0;
#endif
	{
		FT_Glyph_Metrics metrics;

#ifdef SCE_PVF_2014
		result = PvfRasterizerGetGlyphMetrics(lib->rasterizer, handle->font->face,
			handle->charCode, &metrics);
#else
		if (vertical)
			result = PvfRasterizerGetVertGlyphMetrics(lib->rasterizer,
				handle->font->face, handle->charCode, &metrics, &vert);
		else
			result = PvfRasterizerGetGlyphMetrics(lib->rasterizer, handle->font->face,
				handle->charCode, &metrics);
#endif
		if (result != 0)
			return result;
		buffer = NULL;
		pvfSetGlyphMetrics(handle, &metrics);
	}
#ifdef SCE_PVF_2014
	result = PvfRasterizerRenderGlyph(handle->font->lib->rasterizer,
		handle->font->face, handle->charCode, depth4 != 0, &width, &rows, &top,
		&left, &pitch, &buffer);
#else
	if (vertical) {
		result = PvfRasterizerRenderVertGlyph(handle->font->lib->rasterizer,
			handle->font->face, handle->charCode, depth4 != 0, &width, &rows, &top,
			&left, &pitch, &buffer, &vert);
		if (isVert != NULL)
			*isVert = vert != 0;
	} else {
		result = PvfRasterizerRenderGlyph(handle->font->lib->rasterizer,
			handle->font->face, handle->charCode, depth4 != 0, &width, &rows, &top,
			&left, &pitch, &buffer);
	}
#endif
	if (result != 0)
		return result;
	lib = handle->font->lib;
	handle->charInfo.bitmapHeight = rows;
	handle->charInfo.bitmapPitch = pitch;
	handle->charInfo.bitmapWidth = width;
	handle->charInfo.bitmapLeft = left;
	handle->charInfo.bitmapTop = top;
	handle->glyphImage = lib->init.allocFunc(lib->init.userData, rows * pitch);
	if (handle->glyphImage != NULL) {
		sceClibMemcpy_safe(handle->glyphImage, buffer, rows * pitch);
		if (handle->glyphImage != NULL && slot != NULL && !found) {
			handle->glyphMode = depth4;
			cache = handle->font->lib->init.cache;
			cache->write0ToCacheFunc(cache->cacheInstance, slot, &handle->charInfo,
				sizeof(handle->charInfo));
			cache = handle->font->lib->init.cache;
			cache->write1ToCacheFunc(cache->cacheInstance, slot, handle->glyphImage,
				pitch * rows);
		}
	} else {
		result = SCE_PVF_ERROR_NOMEMORY;
	}
	PvfRasterizerFreeBitmap(handle->font->lib->rasterizer, buffer);
unlock:
	if (handle->font->cacheable && (cache = handle->font->lib->init.cache) != NULL)
		cache->unlockFunc(cache->cacheInstance);
	return result;
}

/* a free handle of the library's, counted; NULL when all are in use */
static inline PvfFontHandle *pvfAllocHandle(PvfLib *lib)
{
	ScePvf_t_s32 i;

	for (i = 0; i < (ScePvf_t_s32)lib->init.maxNumFonts; i++) {
		if (!lib->handles[i].inUse) {
			lib->numHandles++;
			return &lib->handles[i];
		}
	}
	return NULL;
}

/* opens a handle on a font (pvfOpenFont's), with the library's sizes */
#ifdef SCE_PVF_2014
static __attribute__((noinline)) PvfFontHandle *pvfOpenHandle(PvfLib *lib, ScePvf_t_s32 index,
	const char *fileName, const void *buffer, ScePvf_t_s32 size, ScePvf_t_u32 openMode,
	ScePvf_t_error *errorCode)
#else
static PvfFontHandle *pvfOpenHandle(PvfLib *lib, ScePvf_t_s32 index,
	const char *fileName, const void *buffer, ScePvf_t_s32 size, ScePvf_t_u32 openMode,
	ScePvf_t_s32 subFontIndex, ScePvf_t_error *errorCode)
#endif
{
	PvfFontHandle *handle;
	ScePvf_t_u32 numHandles;

	if (lib == NULL) {
		*errorCode = SCE_PVF_ERROR_LIBID;
		return NULL;
	}
	numHandles = lib->numHandles;
	*errorCode = 0;
	if ((ScePvf_t_s32)lib->init.maxNumFonts <= (ScePvf_t_s32)numHandles) {
		*errorCode = SCE_PVF_ERROR_TOOMANYOPENED;
		return NULL;
	}
	handle = pvfAllocHandle(lib);
	handle->memBlockServer = 0;
	handle->memBlockClient = 0;
	handle->glyphImage = NULL;
	handle->memBlockBase = NULL;
	handle->glyphMode = 2;
	handle->memBlock = -1;
#ifdef SCE_PVF_2014
	handle->font = pvfOpenFont(lib, index, fileName, buffer, size, openMode, errorCode);
#else
	handle->font = pvfOpenFont(lib, index, fileName, buffer, size, subFontIndex, openMode,
		errorCode);
#endif
	if (*errorCode != 0 || handle->font == NULL) {
		if (*errorCode == SCE_PVF_ERROR_FILEOPEN) {
			if ((ScePvf_t_s32)numHandles < lib->numHandles)
				lib->numHandles--;
			if (handle->glyphImage != NULL) {
				PvfLib *fontLib = handle->font->lib;

				fontLib->init.freeFunc(fontLib->init.userData, handle->glyphImage);
			}
			handle->glyphImage = NULL;
		} else {
			pvfClearGlyph(handle);
			pvfReleaseFont(handle);
		}
		return NULL;
	}
	handle->inUse = 1;
	/* (the comparisons keep either value) */
	handle->hCharSize = PVF_IS_ZERO(lib->hCharSize - 10.125f) ? 10.125f : lib->hCharSize;
	handle->vCharSize = PVF_IS_NONZERO(lib->vCharSize - 10.125f) ? lib->vCharSize : 10.125f;
	handle->libEmboldenRate = PVF_IS_NONZERO(lib->emboldenRate) ? lib->emboldenRate : 0.0f;
	handle->emboldenRate = 0.0f;
	handle->skewAngleX = 0.0f;
	handle->skewAngleY = 0.0f;
	handle->libSkewValue = PVF_IS_NONZERO(lib->skewValue) ? lib->skewValue : 0.0f;
	return handle;
}

/* the system's default font of a language: Japanese (0), or Latin (4) */
#define PVF_DEFAULT_FONT_LATIN	4

static inline const char *pvfDefaultFontBlockName(ScePvf_t_s32 language)
{
	return language == PVF_DEFAULT_FONT_LATIN ? "SceSysDefaultLtnFont" :
		"SceSysDefaultJpnFont";
}

static inline void pvfDefaultFontStyle(ScePvf_t_fontStyleInfo *style, ScePvf_t_s32 language)
{
	sceClibMemset(style, 0, sizeof(*style));
	style->familyCode = SCE_PVF_FAMILY_SANSERIF;
	style->style = SCE_PVF_STYLE_REGULAR;
	style->regionCode = SCE_PVF_GENERIC_REGION_CODE;
	style->languageCode = language == PVF_DEFAULT_FONT_LATIN ? SCE_PVF_LANGUAGE_LATIN :
		SCE_PVF_LANGUAGE_J;
}

/* opens a default font as its server: reads its file into a shared block other processes open */
static PvfFontHandle *pvfOpenDefaultFontServer(PvfLib *lib, ScePvf_t_s32 language,
	ScePvf_t_error *errorCode)
{
	const char *name = pvfDefaultFontBlockName(language);
	char path[PVF_PATH_SIZE];
	union {
		ScePvf_t_fontStyleInfo style;
		SceIoStat stat;
		MemBlockUserOpt opt;
	} u;
	PvfFontHandle *handle;
	ScePvf_t_error error;
	ScePvf_t_fontIndex index;
	ScePvf_t_s32 size, i;
	SceUID uid, fd;
	void *base;

	*errorCode = 0;
	error = 0;
	pvfDefaultFontStyle(&u.style, language);
	index = scePvfFindFont(lib, &u.style, errorCode);
	if (*errorCode != 0)
		return NULL;
	error = pvfGetSystemFontPath(lib, path, index);
	if (error != 0 || sceIoGetstat(path, &u.stat) != 0 || (SceInt32)u.stat.st_size <= 0) {
		*errorCode = SCE_PVF_ERROR_FILEOPEN;
		return NULL;
	}
	size = (ScePvf_t_s32)u.stat.st_size;
	sceClibMemset(&u.opt, 0, sizeof(u.opt));
	u.opt.size = sizeof(u.opt);
	u.opt.attr = ALLOC_ATTR_CLIENT_INFO | ALLOC_ATTR_HAS_CLIENT_INFO;
	u.opt.openFlags = MEMBLOCK_OPEN_RO;
	for (i = 0; i < 8; i++)
		u.opt.capabilities[i] = 0;
	uid = sceKernelAllocMemBlock(name, SCE_KERNEL_MEMBLOCK_TYPE_USER_RW,
		(size + 0xFFF) >> 12 << 12, (const SceKernelAllocMemBlockOpt *)&u.opt);
	if (uid < 0)
		return NULL;
	if (sceKernelGetMemBlockBase(uid, &base) != 0)
		goto failed;
	fd = sceIoOpen(path, SCE_O_RDONLY, 0);
	if (fd < 0)
		goto failed;
	if (size != sceIoRead(fd, base, size)) {
		sceIoClose(fd);
		goto failed;
	}
	if (sceIoClose(fd) < 0)
		goto failed;
	if (base != NULL)
#ifdef SCE_PVF_2014
		handle = pvfOpenHandle(lib, 0, NULL, base, size, PVF_OPEN_USER_BUFFER, &error);
#else
		handle = pvfOpenHandle(lib, 0, NULL, base, size, PVF_OPEN_USER_BUFFER, 0,
			&error);
#endif
	else
		handle = NULL;
	handle->memBlock = uid;
	handle->memBlockBase = base;
	handle->memBlockServer = 1;
	handle->memBlockClient = 0;
	return handle;

failed:
	sceKernelFreeMemBlock(uid);
	*errorCode = SCE_PVF_ERROR_NOMEMORY;
	return NULL;
}

/* opens a default font as a client: the block its server made */
static __attribute__((noclone)) PvfFontHandle *pvfOpenDefaultFontClient(PvfLib *lib, ScePvf_t_s32 language,
	ScePvf_t_error *errorCode)
{
	const char *name = pvfDefaultFontBlockName(language);
	char path[PVF_PATH_SIZE];
	union {
		ScePvf_t_fontStyleInfo style;
		SceIoStat stat;
	} u;
	PvfFontHandle *handle;
	ScePvf_t_fontIndex index;
	ScePvf_t_s32 size;
	SceUID uid;
	void *base;

	*errorCode = 0;
	pvfDefaultFontStyle(&u.style, language);
	index = scePvfFindFont(lib, &u.style, errorCode);
	if (*errorCode != 0)
		return NULL;
	if (pvfGetSystemFontPath(lib, path, index) != 0 || sceIoGetstat(path, &u.stat) != 0 ||
	    (size = (ScePvf_t_s32)u.stat.st_size) <= 0) {
		*errorCode = SCE_PVF_ERROR_FILEOPEN;
		return NULL;
	}
	base = NULL;
#ifdef SCE_PVF_2014
	{
		SceKernelAllocMemBlockOpt opt;

		sceClibMemset(&opt, 0, sizeof(opt));
		opt.size = sizeof(opt);
		opt.attr = 0x01000200;
		opt.strBaseBlockName = name;
		uid = sceKernelAllocMemBlock(name, SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_RW,
					     (size + 0xfff) & ~0xfff, &opt);
	}
#else
	uid = sceKernelOpenMemBlock(name, MEMBLOCK_OPEN_RO);
#endif
	if (uid < 0) {
		*errorCode = SCE_PVF_ERROR_FILEOPEN;
		return NULL;
	}
	if (sceKernelGetMemBlockBase(uid, &base) != 0) {
		sceKernelFreeMemBlock(uid);
		*errorCode = SCE_PVF_ERROR_NOMEMORY;
		return NULL;
	}
	if (base != NULL) {
#ifdef SCE_PVF_2014
		handle = pvfOpenHandle(lib, 0, NULL, base, size, PVF_OPEN_USER_BUFFER, errorCode);
#else
		handle = pvfOpenHandle(lib, 0, NULL, base, size, PVF_OPEN_USER_BUFFER, 0,
			errorCode);
#endif
	} else {
		handle = NULL;
		*errorCode = SCE_PVF_ERROR_ARG;
	}
	handle->memBlock = uid;
	handle->memBlockBase = base;
	handle->memBlockServer = 0;
	handle->memBlockClient = 1;
	return handle;
}

void __scePvfSetFt2LibCHook(void)
{
	PvfRasterizerSetLibCHook();
}

void __scePvfSetFt2DoneLibCHook(void)
{
	PvfRasterizerDoneLibCHook();
}

/* the system fonts' count, counted at the first use */
static inline ScePvf_t_s32 pvfNumFontList(PvfLib *lib)
{
	if (lib->numFontList < 0)
		lib->numFontList = PVF_NUM_SYSTEM_FONTS;
	return lib->numFontList;
}

ScePvf_t_libId scePvfNewLib(ScePvf_t_initRec *initParam, ScePvf_t_error *errorCode)
{
	char path[PVF_FONT_PATH_SIZE];
	PvfLib *lib;
	PvfRasterizer *rasterizer;
	ScePvf_t_u32 i;

	path[0] = '\0';
	if (errorCode == NULL)
		return NULL;
	if (initParam == NULL || initParam->reserved != NULL)
		goto badArgument;
	sceClibStrncpy(path, PVF_FONT_PATH, sizeof(path) - 1);
	path[sizeof(path) - 1] = '\0';
	*errorCode = 0;
	if (initParam->allocFunc == NULL || initParam->reallocFunc == NULL ||
	    initParam->freeFunc == NULL)
		goto badArgument;
	lib = initParam->allocFunc(initParam->userData, sizeof(*lib));
	if (lib == NULL)
		goto noMemory;
	sceClibStrncpy((char *)lib->fontPath, path, sizeof(lib->fontPath) - 1);
	lib->fontPath[sizeof(lib->fontPath) - 1] = 0;
	if (initParam->maxNumFonts - 1 > SCE_PVF_MAX_OPEN - 1) {
		initParam->freeFunc(initParam->userData, lib);
		goto badArgument;
	}
	lib->init.userData = initParam->userData;
	lib->init.maxNumFonts = initParam->maxNumFonts;
	lib->init.cache = initParam->cache;
	lib->init.reserved = initParam->reserved;
	lib->init.allocFunc = initParam->allocFunc;
	lib->init.reallocFunc = initParam->reallocFunc;
	lib->init.freeFunc = initParam->freeFunc;
	lib->autoHintMode = 0;
	lib->resolution.h = PVF_DEFAULT_RESOLUTION;
	lib->resolution.v = PVF_DEFAULT_RESOLUTION;
	lib->hCharSize = PVF_DEFAULT_CHAR_SIZE;
	lib->vCharSize = PVF_DEFAULT_CHAR_SIZE;
	lib->emboldenRate = 0.0f;
	lib->skewValue = 0.0f;
	lib->altCharCode = PVF_DEFAULT_ALT_CHAR;
	lib->handles = lib->init.allocFunc(lib->init.userData,
		lib->init.maxNumFonts * sizeof(PvfFontHandle));
	if (lib->handles == NULL) {
		lib->init.freeFunc(initParam->userData, lib);
		goto noMemory;
	}
	lib->fonts = lib->init.allocFunc(lib->init.userData,
		lib->init.maxNumFonts * sizeof(PvfFont));
	if (lib->fonts == NULL) {
		lib->init.freeFunc(initParam->userData, lib->handles);
		lib->init.freeFunc(initParam->userData, lib);
		goto noMemory;
	}
	lib->numFontList = -1;
	lib->fontListBuffer = NULL;
	lib->numFonts = 0;
	lib->numHandles = 0;
	lib->em = PVF_DEFAULT_EM;
	*errorCode = 0;
	pvfNumFontList(lib);
	for (i = 0; i < lib->init.maxNumFonts; i++) {
		lib->handles[i].font = NULL;
		lib->handles[i].inUse = 0;
		lib->fonts[i].inUse = 0;
		lib->fonts[i].refCount = 0;
		lib->fonts[i].fontIndex = 0;
#ifndef SCE_PVF_2014
		lib->fonts[i].subFontIndex = 0;
#endif
		lib->fonts[i].reserved = -1;
	}
	rasterizer = PvfRasterizerNew(lib->init.maxNumFonts, lib, lib, pvfAlloc, pvfRealloc,
		pvfFree);
	lib->rasterizer = rasterizer;
	if (rasterizer != NULL) {
#ifdef SCE_PVF_2014
		PvfRasterizerSetResolution(rasterizer, (FT_UInt)lib->resolution.h,
			(FT_UInt)lib->resolution.v);
#else
		PvfRasterizerSetResolution(rasterizer, lib->resolution.h, lib->resolution.v);
#endif
		PvfRasterizerSetCharSize(lib->rasterizer, (FT_F26Dot6)(lib->hCharSize * 64.0f),
			(FT_F26Dot6)(lib->vCharSize * 64.0f));
		if (PVF_IS_NONZERO(lib->emboldenRate))
			PvfRasterizerSetEmbolden(lib->rasterizer,
				(FT_Pos)(lib->emboldenRate * 64.0f));
		if (PVF_IS_NONZERO(lib->skewValue))
			PvfRasterizerSetSlant(lib->rasterizer,
				(FT_Fixed)(lib->skewValue * 65536.0f));
	}
	lib->nextUserFont = lib->numFontList;
	return lib;

badArgument:
	*errorCode = SCE_PVF_ERROR_ARG;
	return NULL;
noMemory:
	*errorCode = SCE_PVF_ERROR_NOMEMORY;
	return NULL;
}

/* closes a handle: its default font block, its glyph and its reference to the font */
static inline void pvfCloseHandle(PvfFontHandle *handle)
{
	if (handle->memBlockServer || handle->memBlockClient) {
		if (handle->memBlockServer) {
			sceKernelFreeMemBlock(handle->memBlock);
			handle->memBlockServer = 0;
		}
		if (handle->memBlockClient) {
			sceKernelFreeMemBlock(handle->memBlock);
			handle->memBlockClient = 0;
		}
		handle->memBlockBase = NULL;
	}
	handle->inUse = 0;
	handle->font->lib->numHandles--;
	pvfClearGlyph(handle);
	pvfReleaseFont(handle);
}

ScePvf_t_error scePvfDoneLib(ScePvf_t_libId libID)
{
	PvfLib *lib = libID;
	ScePvf_t_s32 i;

	if (lib == NULL)
		return SCE_PVF_ERROR_LIBID;
	for (i = 0; i < (ScePvf_t_s32)lib->init.maxNumFonts; i++) {
		PvfFontHandle *handle = &lib->handles[i];

		if (handle != NULL && handle->inUse)
			pvfCloseHandle(handle);
	}
	if (lib->fontListBuffer != NULL)
		lib->init.freeFunc(lib->init.userData, lib->fontListBuffer);
	PvfRasterizerDelete(lib->rasterizer);
	lib->init.freeFunc(lib->init.userData, lib->handles);
	lib->init.freeFunc(lib->init.userData, lib->fonts);
	lib->init.freeFunc(lib->init.userData, lib);
	return 0;
}

ScePvf_t_error scePvfSetResolution(ScePvf_t_libId libID, ScePvf_t_f32 hResolution,
	ScePvf_t_f32 vResolution)
{
	PvfLib *lib = libID;

	if (lib == NULL)
		return SCE_PVF_ERROR_LIBID;
	if (hResolution <= 0.0f || vResolution <= 0.0f)
		return SCE_PVF_ERROR_ARG;
	lib->resolution.h = hResolution;
	lib->resolution.v = vResolution;
	return 0;
}

ScePvf_t_error scePvfSetCharSize(ScePvf_t_fontId fontID, ScePvf_t_f32 hSize, ScePvf_t_f32 vSize)
{
	PvfFontHandle *handle = fontID;

	if (handle == NULL || hSize <= 0.0f || vSize <= 0.0f)
		return SCE_PVF_ERROR_ARG;
	handle->hCharSize = hSize;
	handle->vCharSize = vSize;
	return 0;
}

#ifdef SCE_PVF_2014
ScePvf_t_error pvfSetDefaultCharSize(ScePvf_t_libId libID, ScePvf_t_f32 hSize, ScePvf_t_f32 vSize)
{
	PvfLib *lib = libID;
	ScePvf_t_s32 i;

	if (lib == NULL)
		return SCE_PVF_ERROR_LIBID;
	if (hSize <= 0.0f || vSize <= 0.0f)
		return SCE_PVF_ERROR_ARG;
	lib->hCharSize = hSize;
	lib->vCharSize = vSize;
	for (i = 0; i < (ScePvf_t_s32)lib->init.maxNumFonts; i++) {
		lib->handles[i].hCharSize = hSize;
		lib->handles[i].vCharSize = vSize;
	}
	return 0;
}
#endif

ScePvf_t_error scePvfSetEmboldenRate(ScePvf_t_fontId fontID, ScePvf_t_f32 emboldenRate)
{
	PvfFontHandle *handle = fontID;

	if (handle == NULL || emboldenRate < SCE_PVF_MIN_EMBOLDEN_RATE ||
	    emboldenRate > SCE_PVF_MAX_EMBOLDEN_RATE)
		return SCE_PVF_ERROR_ARG;
	handle->emboldenRate = emboldenRate;
	return 0;
}

ScePvf_t_error scePvfSetSkewValue(ScePvf_t_fontId fontID, ScePvf_t_f32 angleX,
	ScePvf_t_f32 angleY)
{
	PvfFontHandle *handle = fontID;

	if (handle == NULL || angleX < SCE_PVF_MIN_SKEW_VALUE || angleX > SCE_PVF_MAX_SKEW_VALUE ||
	    angleY < SCE_PVF_MIN_SKEW_VALUE || angleY > SCE_PVF_MAX_SKEW_VALUE)
		return SCE_PVF_ERROR_ARG;
	handle->skewAngleX = angleX;
	handle->skewAngleY = angleY;
	return 0;
}

#ifdef SCE_PVF_2014
ScePvf_t_error pvfSetDefaultEmboldenRate(ScePvf_t_libId libID, ScePvf_t_f32 emboldenRate)
{
	PvfLib *lib = libID;
	ScePvf_t_s32 i;

	if (lib == NULL)
		return SCE_PVF_ERROR_LIBID;
	lib->emboldenRate = emboldenRate;
	for (i = 0; i < (ScePvf_t_s32)lib->init.maxNumFonts; i++)
		lib->handles[i].libEmboldenRate = emboldenRate;
	return 0;
}

ScePvf_t_error pvfSetDefaultSkewValue(ScePvf_t_libId libID, ScePvf_t_f32 skewValue)
{
	PvfLib *lib = libID;
	ScePvf_t_s32 i;

	if (lib == NULL)
		return SCE_PVF_ERROR_LIBID;
	lib->skewValue = skewValue;
	for (i = 0; i < (ScePvf_t_s32)lib->init.maxNumFonts; i++)
		lib->handles[i].libSkewValue = skewValue;
	return 0;
}

ScePvf_t_error pvfSetAutoHintMode(ScePvf_t_libId libID, ScePvf_t_u32 mode)
{
	PvfLib *lib = libID;

	if (lib == NULL)
		return SCE_PVF_ERROR_LIBID;
	if (mode > 1)
		return SCE_PVF_ERROR_ARG;
	lib->autoHintMode = (ScePvf_t_s32)mode;
	return 0;
}
#endif

ScePvf_t_int scePvfGetNumFontList(ScePvf_t_libId libID, ScePvf_t_error *errorCode)
{
	if (errorCode == NULL)
		return 0;
	*errorCode = 0;
	if (libID == NULL) {
		*errorCode = SCE_PVF_ERROR_LIBID;
		return 0;
	}
	return pvfNumFontList(libID);
}

ScePvf_t_error scePvfGetFontList(ScePvf_t_libId libID, ScePvf_t_fontStyleInfo *fontStyleInfo,
	ScePvf_t_int arraySize)
{
	ScePvf_t_s32 num;

	if (libID == NULL)
		return SCE_PVF_ERROR_LIBID;
	if (fontStyleInfo == NULL)
		return SCE_PVF_ERROR_ARG;
	num = pvfNumFontList(libID);
	if (num < arraySize)
		arraySize = num;
	if (arraySize > 0)
		sceClibMemcpy_safe(fontStyleInfo, pvfSystemFonts,
			sizeof(pvfSystemFonts[0]) * arraySize);
	return 0;
}

/* what a font style asks for (pvfMatchFont) */
#define PVF_MATCH_WEIGHT	0x0010
#define PVF_MATCH_FAMILY	0x0020
#define PVF_MATCH_STYLE		0x0040
#define PVF_MATCH_SUBSTYLE	0x0080
#define PVF_MATCH_LANGUAGE	0x0100
#define PVF_MATCH_REGION	0x0200
#define PVF_MATCH_COUNTRY	0x0400
#define PVF_MATCH_FONTNAME	0x0800
#define PVF_MATCH_FILENAME	0x1000

/* what of style is asked for: its fields that are set */
static inline ScePvf_t_u16 pvfMatchFlags(const ScePvf_t_fontStyleInfo *style)
{
	ScePvf_t_u16 flags = 0;

	if (style->weight > 0.0f)
		flags |= PVF_MATCH_WEIGHT;
	if (style->familyCode != 0)
		flags |= PVF_MATCH_FAMILY;
	if (style->style != 0)
		flags |= PVF_MATCH_STYLE;
	if (style->subStyle != 0)
		flags |= PVF_MATCH_SUBSTYLE;
	if (style->languageCode != 0)
		flags |= PVF_MATCH_LANGUAGE;
	if (style->regionCode != 0)
		flags |= PVF_MATCH_REGION;
	if (style->countryCode != 0)
		flags |= PVF_MATCH_COUNTRY;
	if (style->fontName[0] != 0)
		flags |= PVF_MATCH_FONTNAME;
	if (style->fileName[0] != 0)
		flags |= PVF_MATCH_FILENAME;
	return flags;
}

/* the first of num system fonts with what flags asks of style (its weight within 0.1%) */
static inline ScePvf_t_fontIndex pvfMatchFont(ScePvf_t_s32 num,
	const ScePvf_t_fontStyleInfo *style, ScePvf_t_u16 flags)
{
	ScePvf_t_f32 weight = style->weight;
	ScePvf_t_u16 familyCode = style->familyCode;
	ScePvf_t_u16 fontStyle = style->style;
	ScePvf_t_u16 subStyle = style->subStyle;
	ScePvf_t_u16 languageCode = style->languageCode;
	ScePvf_t_u16 regionCode = style->regionCode;
	ScePvf_t_u16 countryCode = style->countryCode;
	ScePvf_t_s32 i;

	for (i = 0; i < num; i++) {
		const ScePvf_t_fontStyleInfo *font = &pvfSystemFonts[i];

		if (flags & PVF_MATCH_WEIGHT) {
			if (weight < font->weight) {
				if (weight <= font->weight * 0.999f)
					continue;
			} else if (weight * 0.999f >= font->weight) {
				continue;
			}
		}
		if ((flags & PVF_MATCH_FAMILY) && font->familyCode != familyCode)
			continue;
		if ((flags & PVF_MATCH_STYLE) && font->style != fontStyle)
			continue;
		if ((flags & PVF_MATCH_SUBSTYLE) && font->subStyle != subStyle)
			continue;
		if ((flags & PVF_MATCH_LANGUAGE) && font->languageCode != languageCode)
			continue;
		if ((flags & PVF_MATCH_REGION) && font->regionCode != regionCode)
			continue;
		if ((flags & PVF_MATCH_COUNTRY) && font->countryCode != countryCode)
			continue;
		if ((flags & PVF_MATCH_FONTNAME) && sceClibStrncmp((const char *)style->fontName,
		    (const char *)font->fontName, SCE_PVF_FONTNAME_LENGTH) != 0)
			continue;
		if ((flags & PVF_MATCH_FILENAME) && sceClibStrncmp((const char *)style->fileName,
		    (const char *)font->fileName, SCE_PVF_FONTFILENAME_LENGTH) != 0)
			continue;
		return i;
	}
	return -1;
}

ScePvf_t_fontIndex scePvfFindOptimumFont(ScePvf_t_libId libID,
	ScePvf_t_fontStyleInfo *fontStyleInfo, ScePvf_t_error *errorCode)
{
	ScePvf_t_fontStyleInfo languageOnly;
	ScePvf_t_fontIndex index;
	ScePvf_t_u16 languageCode;
	ScePvf_t_s32 num;
	ScePvf_t_u16 flags;

	if (errorCode == NULL)
		return 0;
	*errorCode = 0;
	if (libID == NULL) {
		*errorCode = SCE_PVF_ERROR_LIBID;
		return 0;
	}
	if (fontStyleInfo == NULL) {
		*errorCode = SCE_PVF_ERROR_ARG;
		return 0;
	}
	num = pvfNumFontList(libID);
	if (*errorCode != 0)
		return 0;
	languageCode = fontStyleInfo->languageCode;
	flags = pvfMatchFlags(fontStyleInfo);
	if (flags == 0)
		return 0;
	index = pvfMatchFont(num, fontStyleInfo, flags);
	if (index >= 0)
		return index;
	/* else the language's first font */
	if (!(flags & PVF_MATCH_LANGUAGE) || (flags & ~PVF_MATCH_LANGUAGE) == 0 ||
	    languageCode == 0)
		return 0;
	sceClibMemset(&languageOnly, 0, sizeof(languageOnly));
	languageOnly.languageCode = fontStyleInfo->languageCode;
	index = scePvfFindOptimumFont(libID, &languageOnly, errorCode);
	return index < 0 ? 0 : index;
}

ScePvf_t_fontIndex scePvfFindFont(ScePvf_t_libId libID, ScePvf_t_fontStyleInfo *fontStyleInfo,
	ScePvf_t_error *errorCode)
{
	ScePvf_t_s32 num;
	ScePvf_t_u16 flags;

	if (errorCode == NULL)
		return 0;
	*errorCode = 0;
	if (libID == NULL) {
		*errorCode = SCE_PVF_ERROR_LIBID;
		return 0;
	}
	if (fontStyleInfo == NULL) {
		*errorCode = SCE_PVF_ERROR_ARG;
		return 0;
	}
	num = pvfNumFontList(libID);
	if (*errorCode != 0)
		return 0;
	flags = pvfMatchFlags(fontStyleInfo);
	if (flags == 0)
		return 0;
	return pvfMatchFont(num, fontStyleInfo, flags);
}

ScePvf_t_error scePvfGetFontInfoByIndexNumber(ScePvf_t_libId libID,
	ScePvf_t_fontStyleInfo *fontStyleInfo, ScePvf_t_fontIndex fontIndex)
{
	if (libID == NULL)
		return SCE_PVF_ERROR_LIBID;
	if (fontStyleInfo == NULL)
		return SCE_PVF_ERROR_ARG;
	if (fontIndex >= 0 && fontIndex < pvfNumFontList(libID)) {
		sceClibMemcpy_safe(fontStyleInfo, &pvfSystemFonts[fontIndex],
			sizeof(*fontStyleInfo));
		return 0;
	}
	return SCE_PVF_ERROR_ARG;
}

ScePvf_t_fontId scePvfOpenDefaultJapaneseFontOnSharedMemory(ScePvf_t_libId libID,
	ScePvf_t_error *errorCode)
{
	ScePvf_t_error error;
	PvfFontHandle *handle;

	if (errorCode == NULL)
		return NULL;
	if (libID == NULL) {
		*errorCode = SCE_PVF_ERROR_LIBID;
		return NULL;
	}
	error = 0;
	handle = pvfOpenDefaultFontClient(libID, 0, &error);
	if (error == 0) {
		*errorCode = 0;
		return handle;
	}
	handle = pvfOpenDefaultFontServer(libID, 0, errorCode);
	if (*errorCode != 0)
		return NULL;
	return handle;
}

ScePvf_t_fontId scePvfOpenDefaultLatinFontOnSharedMemory(ScePvf_t_libId libID,
	ScePvf_t_error *errorCode)
{
	ScePvf_t_error error;
	PvfFontHandle *handle;

	if (errorCode == NULL)
		return NULL;
	if (libID == NULL) {
		*errorCode = SCE_PVF_ERROR_LIBID;
		return NULL;
	}
	error = 0;
	handle = pvfOpenDefaultFontClient(libID, PVF_DEFAULT_FONT_LATIN, &error);
	if (error == 0) {
		*errorCode = 0;
		return handle;
	}
	handle = pvfOpenDefaultFontServer(libID, PVF_DEFAULT_FONT_LATIN, errorCode);
	if (*errorCode != 0)
		return NULL;
	return handle;
}

ScePvf_t_fontId scePvfOpen(ScePvf_t_libId libID, ScePvf_t_fontIndex fontIndex,
	ScePvf_t_u32 mode, ScePvf_t_error *errorCode)
{
	if (errorCode == NULL)
		return NULL;
	*errorCode = 0;
	if (mode > SCE_PVF_MEMORYBASEDSTREAM || fontIndex < 0) {
		*errorCode = SCE_PVF_ERROR_ARG;
		return NULL;
	}
	if (libID == NULL) {
		*errorCode = SCE_PVF_ERROR_LIBID;
		return NULL;
	}
	if (fontIndex >= scePvfGetNumFontList(libID, errorCode)) {
		if (*errorCode == 0)
			*errorCode = SCE_PVF_ERROR_ARG;
		return NULL;
	}
	if (*errorCode != 0)
		return NULL;
#ifdef SCE_PVF_2014
	return pvfOpenHandle(libID, fontIndex, NULL, NULL, 0, mode, errorCode);
#else
	return pvfOpenHandle(libID, fontIndex, NULL, NULL, 0, mode, 0, errorCode);
#endif
}

/* the open mode of a user file read as mode says; 0 for a bad mode */
static inline ScePvf_t_u32 pvfUserFileMode(ScePvf_t_u32 mode)
{
	if (mode == SCE_PVF_FILEBASEDSTREAM)
		return PVF_OPEN_USER_STREAM;
	if (mode == SCE_PVF_MEMORYBASEDSTREAM)
		return PVF_OPEN_USER_MEMORY;
	return 0;
}

ScePvf_t_fontId scePvfOpenUserFile(ScePvf_t_libId libID, ScePvf_t_pointer filename,
	ScePvf_t_u32 mode, ScePvf_t_error *errorCode)
{
	ScePvf_t_u32 openMode;

	if (errorCode == NULL)
		return NULL;
	openMode = pvfUserFileMode(mode);
	if (openMode == 0 || filename == NULL) {
		*errorCode = SCE_PVF_ERROR_ARG;
		return NULL;
	}
	if (libID == NULL) {
		*errorCode = SCE_PVF_ERROR_LIBID;
		return NULL;
	}
#ifdef SCE_PVF_2014
	return pvfOpenHandle(libID, 0, filename, NULL, 0, openMode, errorCode);
#else
	return pvfOpenHandle(libID, 0, filename, NULL, 0, openMode, 0, errorCode);
#endif
}

#ifndef SCE_PVF_2014
ScePvf_t_fontId scePvfOpenUserFileWithSubfontIndex(ScePvf_t_libId libID,
	ScePvf_t_pointer filename, ScePvf_t_u32 mode, ScePvf_t_u32 subFontIndex,
	ScePvf_t_error *errorCode)
{
	ScePvf_t_u32 openMode;

	if (errorCode == NULL)
		return NULL;
	openMode = pvfUserFileMode(mode);
	if (openMode == 0 || filename == NULL) {
		*errorCode = SCE_PVF_ERROR_ARG;
		return NULL;
	}
	if (libID == NULL) {
		*errorCode = SCE_PVF_ERROR_LIBID;
		return NULL;
	}
	return pvfOpenHandle(libID, 0, filename, NULL, 0, openMode, subFontIndex, errorCode);
}
#endif

ScePvf_t_fontId scePvfOpenUserMemory(ScePvf_t_libId libID, ScePvf_t_pointer addr,
	ScePvf_t_u32 size, ScePvf_t_error *errorCode)
{
	if (errorCode == NULL)
		return NULL;
	if (addr == NULL || size == 0) {
		*errorCode = SCE_PVF_ERROR_ARG;
		return NULL;
	}
	if (libID == NULL) {
		*errorCode = SCE_PVF_ERROR_LIBID;
		return NULL;
	}
#ifdef SCE_PVF_2014
	return pvfOpenHandle(libID, 0, NULL, addr, size, PVF_OPEN_USER_BUFFER, errorCode);
#else
	return pvfOpenHandle(libID, 0, NULL, addr, size, PVF_OPEN_USER_BUFFER, 0, errorCode);
#endif
}

#ifndef SCE_PVF_2014
ScePvf_t_fontId scePvfOpenUserMemoryWithSubfontIndex(ScePvf_t_libId libID,
	ScePvf_t_pointer addr, ScePvf_t_u32 size, ScePvf_t_u32 subFontIndex,
	ScePvf_t_error *errorCode)
{
	if (errorCode == NULL)
		return NULL;
	if (addr == NULL || size == 0) {
		*errorCode = SCE_PVF_ERROR_ARG;
		return NULL;
	}
	if (libID == NULL) {
		*errorCode = SCE_PVF_ERROR_LIBID;
		return NULL;
	}
	return pvfOpenHandle(libID, 0, NULL, addr, size, PVF_OPEN_USER_BUFFER, subFontIndex,
		errorCode);
}
#endif

ScePvf_t_bool scePvfIsElement(ScePvf_t_fontId fontID, ScePvf_t_charCode charCode)
{
	return pvfHasChar(fontID, charCode);
}

#ifndef SCE_PVF_2014
ScePvf_t_bool scePvfIsVertElement(ScePvf_t_fontId fontID, ScePvf_t_charCode charCode)
{
	PvfFontHandle *handle = fontID;
	PvfFont *font;

	if (handle == NULL || charCode == 0xFFFF)
		return 0;
	font = handle->font;
	handle->charCode = charCode;
	return PvfRasterizerHasVertChar(font->lib->rasterizer, font->face, charCode) != 0;
}
#endif

ScePvf_t_error scePvfClose(ScePvf_t_fontId fontID)
{
	PvfFontHandle *handle = fontID;

	if (handle == NULL || !handle->inUse)
		return SCE_PVF_ERROR_ARG;
	pvfCloseHandle(handle);
	return 0;
}

/* x rounded up to a whole number (x within the int range, else as it is; for x >= 0) */
static inline ScePvf_t_f32 pvfCeil(ScePvf_t_f32 x)
{
	if (x >= -2147483648.0f && x <= 2147483648.0f) {
		ScePvf_t_f32 whole = (ScePvf_t_f32)(ScePvf_t_s32)x;

		if (x < 0.0f || x - whole <= 0.0f)
			x = (ScePvf_t_f32)(ScePvf_t_s32)x;
		else
			x = whole + 1.0f;
	}
	return x;
}

/* x rounded away from zero */
static inline ScePvf_t_f32 pvfRoundOut(ScePvf_t_f32 x)
{
	if (x < 0.0f)
		return -pvfCeil(-x);
	return pvfCeil(x);
}

ScePvf_t_error scePvfGetFontInfo(ScePvf_t_fontId fontID, ScePvf_t_fontInfo *fontInfo)
{
	PvfFontHandle *handle = fontID;
	ScePvf_t_iGlyphMetricsInfo *imetrics;
	ScePvf_t_fGlyphMetricsInfo *fmetrics;
	PvfFont *font;
	ScePvf_t_f32 scale;

	if (handle == NULL || fontInfo == NULL)
		return SCE_PVF_ERROR_ARG;
	font = handle->font;
	if (font == NULL)
		return SCE_PVF_ERROR_DATAINCONSISTENT;
	imetrics = &fontInfo->maxIGlyphMetrics;
	fmetrics = &fontInfo->maxFGlyphMetrics;
	scale = 1.0f / ((ScePvf_t_f32)font->unitsPerEM * font->lib->em);
	imetrics->width64 = pvfRoundOut(font->metrics.width64 * scale);
	imetrics->height64 = pvfRoundOut(font->metrics.height64 * scale);
	imetrics->ascender64 = pvfRoundOut(font->metrics.ascender64 * scale);
	imetrics->descender64 = pvfRoundOut(font->metrics.descender64 * scale);
	imetrics->horizontalBearingX64 = pvfRoundOut(font->metrics.horizontalBearingX64 * scale);
	imetrics->horizontalBearingY64 = pvfRoundOut(font->metrics.horizontalBearingY64 * scale);
	imetrics->verticalBearingX64 = pvfRoundOut(font->metrics.verticalBearingX64 * scale);
	imetrics->verticalBearingY64 = pvfRoundOut(font->metrics.verticalBearingY64 * scale);
	imetrics->horizontalAdvance64 = pvfRoundOut(font->metrics.horizontalAdvance64 * scale);
	imetrics->verticalAdvance64 = pvfRoundOut(font->metrics.verticalAdvance64 * scale);
	fmetrics->width = imetrics->width64 / 64.0f;
	fontInfo->numChars = font->numGlyphs;
	fmetrics->height = imetrics->height64 / 64.0f;
	fmetrics->ascender = imetrics->ascender64 / 64.0f;
	fmetrics->descender = imetrics->descender64 / 64.0f;
	fmetrics->horizontalBearingX = imetrics->horizontalBearingX64 / 64.0f;
	fmetrics->horizontalBearingY = imetrics->horizontalBearingY64 / 64.0f;
	fmetrics->verticalBearingX = imetrics->verticalBearingX64 / 64.0f;
	fmetrics->verticalBearingY = imetrics->verticalBearingY64 / 64.0f;
	fmetrics->horizontalAdvance = imetrics->horizontalAdvance64 / 64.0f;
	fmetrics->verticalAdvance = imetrics->verticalAdvance64 / 64.0f;
	sceClibStrncpy((char *)fontInfo->fontStyleInfo.fontName, font->familyName,
		SCE_PVF_FONTNAME_LENGTH - 1);
	fontInfo->fontStyleInfo.fontName[SCE_PVF_FONTNAME_LENGTH - 1] = 0;
	sceClibStrncpy((char *)fontInfo->fontStyleInfo.styleName, font->styleName,
		SCE_PVF_STYLENAME_LENGTH - 1);
	fontInfo->fontStyleInfo.styleName[SCE_PVF_STYLENAME_LENGTH - 1] = 0;
	font = handle->font;
	switch (font->openMode) {
	case PVF_OPEN_SYSTEM_STREAM:
	case PVF_OPEN_SYSTEM_MEMORY: {
		const ScePvf_t_fontStyleInfo *system;

		if (font->fontIndex >= font->lib->numFontList)
			return SCE_PVF_ERROR_ARG;
		system = &pvfSystemFonts[font->fontIndex];
		fontInfo->fontStyleInfo.weight = system->weight;
		fontInfo->fontStyleInfo.familyCode = system->familyCode;
		fontInfo->fontStyleInfo.style = system->style;
		fontInfo->fontStyleInfo.subStyle = system->subStyle;
		fontInfo->fontStyleInfo.languageCode = system->languageCode;
		fontInfo->fontStyleInfo.regionCode = system->regionCode;
		fontInfo->fontStyleInfo.countryCode = system->countryCode;
		sceClibMemcpy_safe(fontInfo->fontStyleInfo.fileName, system->fileName,
			SCE_PVF_FONTFILENAME_LENGTH);
		fontInfo->fontStyleInfo.extraAttributes = system->extraAttributes;
		fontInfo->fontStyleInfo.expireDate = system->expireDate;
		return 0;
	}
	case PVF_OPEN_USER_STREAM:
	case PVF_OPEN_USER_MEMORY:
	case PVF_OPEN_USER_BUFFER:
		fontInfo->fontStyleInfo.weight = 0.0f;
		fontInfo->fontStyleInfo.familyCode = 0;
		fontInfo->fontStyleInfo.style = 0;
		fontInfo->fontStyleInfo.subStyle = 0;
		fontInfo->fontStyleInfo.languageCode = 0;
		fontInfo->fontStyleInfo.regionCode = 0;
		fontInfo->fontStyleInfo.countryCode = 0;
		fontInfo->fontStyleInfo.fileName[0] = 0;
		fontInfo->fontStyleInfo.extraAttributes = 0;
		fontInfo->fontStyleInfo.expireDate = 0;
		return 0;
	default:
		return 0;
	}
}

/*
 * The handle's metrics for charCode (the library's alternative character where the font has
 * none), from the library's cache or the rasterizer, vertical layout's or not, into charInfo.
 */
static inline ScePvf_t_error pvfGetCharInfo(PvfFontHandle *handle, ScePvf_t_charCode charCode,
	ScePvf_t_charInfo *charInfo, ScePvf_t_s32 vertical)
{
	ScePvf_t_cacheSystemInterface *cache;
	union {
		ScePvfCacheKey key;
		FT_Glyph_Metrics metrics;
	} u;
	ScePvf_t_bool found;
	ScePvf_t_pointer slot;
	ScePvf_t_error error;
	ScePvf_t_s32 isVert;
	PvfLib *lib;

	if (handle == NULL || charInfo == NULL)
		return SCE_PVF_ERROR_ARG;
	if (!pvfHasChar(handle, charCode)) {
		charCode = handle->font->lib->altCharCode;
		if (charCode == 0)
			return SCE_PVF_ERROR_NOGLYPH;
	}
	found = 0;
	pvfClearGlyph(handle);
	lib = handle->font->lib;
	handle->charCode = charCode;
	handle->resolution = lib->resolution;
	slot = NULL;
	if (handle->font->cacheable && (cache = lib->init.cache) != NULL) {
		cache->lockFunc(cache->cacheInstance);
		pvfCacheKey(handle, &u.key, handle->glyphMode, vertical);
		cache = handle->font->lib->init.cache;
		slot = cache->findFunc(cache->cacheInstance, pvfCacheHash(&u.key), &u.key, &found);
		if (!found) {
			cache = handle->font->lib->init.cache;
			if (cache->writeKeyValueToCacheFunc(cache->cacheInstance, slot, &u.key) < 0)
				slot = NULL;
		}
		if (slot != NULL && found) {
			cache = handle->font->lib->init.cache;
			if (cache->read0FromCacheFunc(cache->cacheInstance, slot,
			    &handle->charInfo) < 0)
				found = 0;
		}
		if (slot != NULL && found)
			goto unlock;
	}
	pvfSetRasterizer(handle);
	lib = handle->font->lib;
#ifdef SCE_PVF_2014
	(void)isVert;
	error = PvfRasterizerGetGlyphMetrics(lib->rasterizer, handle->font->face,
		handle->charCode, &u.metrics);
#else
	if (vertical)
		error = PvfRasterizerGetVertGlyphMetrics(lib->rasterizer, handle->font->face,
			handle->charCode, &u.metrics, &isVert);
	else
		error = PvfRasterizerGetGlyphMetrics(lib->rasterizer, handle->font->face,
			handle->charCode, &u.metrics);
#endif
	if (error != 0)
		return error;
	pvfSetGlyphMetrics(handle, &u.metrics);
	if (slot != NULL) {
		cache = handle->font->lib->init.cache;
		cache->write0ToCacheFunc(cache->cacheInstance, slot, &handle->charInfo,
			sizeof(handle->charInfo));
	}
unlock:
	if (handle->font->cacheable && (cache = handle->font->lib->init.cache) != NULL)
		cache->unlockFunc(cache->cacheInstance);
	*charInfo = handle->charInfo;
	return 0;
}

ScePvf_t_error scePvfGetCharInfo(ScePvf_t_fontId fontID, ScePvf_t_charCode charCode,
	ScePvf_t_charInfo *charInfo)
{
	return pvfGetCharInfo(fontID, charCode, charInfo, 0);
}

#ifdef SCE_PVF_2014
ScePvf_t_error pvfGetCharMetricsDirect(ScePvf_t_fontId fontID, ScePvf_t_charCode charCode,
	ScePvf_t_charInfo *charInfo)
{
	PvfFontHandle *handle = fontID;
	FT_Glyph_Metrics metrics;
	ScePvf_t_error error;
	PvfLib *lib;

	if (handle == NULL || charInfo == NULL)
		return SCE_PVF_ERROR_ARG;
	if (!pvfHasChar(handle, charCode)) {
		charCode = handle->font->lib->altCharCode;
		if (charCode == 0)
			return SCE_PVF_ERROR_NOGLYPH;
	}
	lib = handle->font->lib;
	handle->resolution = lib->resolution;
	handle->charCode = charCode;
	pvfSetRasterizer(handle);
	lib = handle->font->lib;
	error = PvfRasterizerGetGlyphMetrics(lib->rasterizer, handle->font->face,
		handle->charCode, &metrics);
	if (error == 0) {
		pvfSetGlyphMetrics(handle, &metrics);
		*charInfo = handle->charInfo;
	}
	return error;
}
#else
ScePvf_t_error scePvfGetVertCharInfo(ScePvf_t_fontId fontID, ScePvf_t_charCode charCode,
	ScePvf_t_charInfo *charInfo)
{
	return pvfGetCharInfo(fontID, charCode, charInfo, 1);
}

ScePvf_t_error scePvfGetKerningInfo(ScePvf_t_fontId fontID, ScePvf_t_charCode leftCharCode,
	ScePvf_t_charCode rightCharCode, ScePvf_t_kerningInfo *pKerningInfo)
{
	PvfFontHandle *handle = fontID;
	ScePvf_t_iKerningInfo kerning;
	ScePvf_t_error error;
	PvfLib *lib;

	if (handle == NULL || pKerningInfo == NULL)
		return SCE_PVF_ERROR_ARG;
	lib = handle->font->lib;
	PvfRasterizerSetResolution(lib->rasterizer, lib->resolution.h, lib->resolution.v);
	PvfRasterizerSetCharSize(handle->font->lib->rasterizer,
		(FT_F26Dot6)(handle->hCharSize * 64.0f), (FT_F26Dot6)(handle->vCharSize * 64.0f));
	error = PvfRasterizerGetKerning(handle->font->lib->rasterizer, handle->font->face,
		leftCharCode, rightCharCode, &kerning);
	if (error == 0) {
		pKerningInfo->iKerningInfo = kerning;
		pKerningInfo->fKerningInfo.xOffset = kerning.xOffset64 / 64.0f;
		pKerningInfo->fKerningInfo.yOffset = kerning.yOffset64 / 64.0f;
	}
	return error;
}
#endif

/* the handle's glyph image rectangle, vertical layout's or not */
static inline ScePvf_t_error pvfGetCharImageRect(PvfFontHandle *handle,
	ScePvf_t_charCode charCode, ScePvf_t_irect *rect, ScePvf_t_s32 vertical)
{
	ScePvf_t_error error;

	if (handle == NULL || rect == NULL)
		return SCE_PVF_ERROR_ARG;
#ifdef SCE_PVF_2014
	(void)vertical;
	error = pvfLoadGlyph(handle, charCode, handle->glyphMode);
#else
	error = pvfLoadGlyph(handle, charCode, handle->glyphMode, vertical, NULL);
#endif
	if (error != 0)
		return error;
	rect->width = handle->charInfo.bitmapWidth;
	rect->height = handle->charInfo.bitmapHeight;
	return 0;
}

ScePvf_t_error scePvfGetCharImageRect(ScePvf_t_fontId fontID, ScePvf_t_charCode charCode,
	ScePvf_t_irect *rect)
{
	return pvfGetCharImageRect(fontID, charCode, rect, 0);
}

#ifndef SCE_PVF_2014
ScePvf_t_error scePvfGetVertCharImageRect(ScePvf_t_fontId fontID, ScePvf_t_charCode charCode,
	ScePvf_t_irect *rect)
{
	return pvfGetCharImageRect(fontID, charCode, rect, 1);
}
#endif

/* draws the handle's loaded glyph into imageBuffer, within the clip rectangle */
static inline ScePvf_t_error pvfDrawGlyph(PvfFontHandle *handle,
	const ScePvf_t_userImageBufferRec *imageBuffer, ScePvf_t_s32 clipX, ScePvf_t_s32 clipY,
	ScePvf_t_u32 clipWidth, ScePvf_t_u32 clipHeight)
{
	if (handle->charInfo.bitmapWidth == 0 || handle->charInfo.bitmapHeight == 0)
		return 0;
	if (handle->glyphImage == NULL)
		return SCE_PVF_ERROR_NOMEMORY;
	switch (imageBuffer->pixelFormat) {
	case SCE_PVF_USERIMAGE_DIRECT4_L:
		pvfDrawGlyphDirect4(handle->glyphImage, handle->charInfo.bitmapLeft,
			handle->charInfo.bitmapTop, handle->charInfo.bitmapWidth,
			handle->charInfo.bitmapPitch, handle->charInfo.bitmapHeight, imageBuffer,
			clipX, clipY, clipWidth, clipHeight);
		return 0;
	case SCE_PVF_USERIMAGE_DIRECT8:
		pvfDrawGlyphDirect8(handle->glyphImage, handle->charInfo.bitmapLeft,
			handle->charInfo.bitmapTop, handle->charInfo.bitmapWidth,
			handle->charInfo.bitmapHeight, imageBuffer, clipX, clipY, clipWidth,
			clipHeight);
		return 0;
	default:
		return SCE_PVF_ERROR_ARG;
	}
}

ScePvf_t_error scePvfGetCharGlyphImage(ScePvf_t_fontId fontID, ScePvf_t_charCode charCode,
	ScePvf_t_userImageBufferRec *imageBuffer)
{
	PvfFontHandle *handle = fontID;
	ScePvf_t_error error;

	if (handle == NULL || imageBuffer == NULL)
		return SCE_PVF_ERROR_ARG;
#ifdef SCE_PVF_2014
	error = pvfLoadGlyph(handle, charCode, imageBuffer->pixelFormat);
#else
	error = pvfLoadGlyph(handle, charCode, imageBuffer->pixelFormat, 0, NULL);
#endif
	if (error != 0)
		return error;
	return pvfDrawGlyph(handle, imageBuffer, 0, 0, imageBuffer->rect.width,
		imageBuffer->rect.height);
}

#ifndef SCE_PVF_2014
ScePvf_t_error scePvfGetVertCharGlyphImage(ScePvf_t_fontId fontID, ScePvf_t_charCode charCode,
	ScePvf_t_userImageBufferRec *imageBuffer)
{
	PvfFontHandle *handle = fontID;
	ScePvf_t_error error;
	ScePvf_t_s32 isVert;

	if (handle == NULL || imageBuffer == NULL)
		return SCE_PVF_ERROR_ARG;
	error = pvfLoadGlyph(handle, charCode, imageBuffer->pixelFormat, 1, &isVert);
	if (error != 0)
		return error;
	return pvfDrawGlyph(handle, imageBuffer, 0, 0, imageBuffer->rect.width,
		imageBuffer->rect.height);
}
#endif

ScePvf_t_error scePvfGetCharGlyphImage_Clip(ScePvf_t_fontId fontID, ScePvf_t_charCode charCode,
	ScePvf_t_userImageBufferRec *imageBuffer, ScePvf_t_s32 clipX, ScePvf_t_s32 clipY,
	ScePvf_t_u32 clipWidth, ScePvf_t_u32 clipHeight)
{
	PvfFontHandle *handle = fontID;
	ScePvf_t_error error;

	if (handle == NULL || imageBuffer == NULL)
		return SCE_PVF_ERROR_ARG;
	if (clipWidth == 0 || clipHeight == 0 || clipX + clipWidth == 0 ||
	    clipY + clipHeight == 0)
		return 0;
	if (clipX >= imageBuffer->rect.width || clipY >= imageBuffer->rect.height)
		return 0;
#ifdef SCE_PVF_2014
	error = pvfLoadGlyph(handle, charCode, imageBuffer->pixelFormat);
#else
	error = pvfLoadGlyph(handle, charCode, imageBuffer->pixelFormat, 0, NULL);
#endif
	if (error != 0)
		return error;
	return pvfDrawGlyph(handle, imageBuffer, clipX, clipY, clipWidth, clipHeight);
}

#ifndef SCE_PVF_2014
ScePvf_t_error scePvfGetVertCharGlyphImage_Clip(ScePvf_t_fontId fontID,
	ScePvf_t_charCode charCode, ScePvf_t_userImageBufferRec *imageBuffer, ScePvf_t_s32 clipX,
	ScePvf_t_s32 clipY, ScePvf_t_u32 clipWidth, ScePvf_t_u32 clipHeight)
{
	PvfFontHandle *handle = fontID;
	ScePvf_t_error error;
	ScePvf_t_s32 isVert;

	if (handle == NULL || imageBuffer == NULL)
		return SCE_PVF_ERROR_ARG;
	if (clipWidth == 0 || clipHeight == 0 || clipX + clipWidth == 0 ||
	    clipY + clipHeight == 0)
		return 0;
	if (clipX >= imageBuffer->rect.width || clipY >= imageBuffer->rect.height)
		return 0;
	error = pvfLoadGlyph(handle, charCode, imageBuffer->pixelFormat, 1, &isVert);
	if (error != 0)
		return error;
	return pvfDrawGlyph(handle, imageBuffer, clipX, clipY, clipWidth, clipHeight);
}

/* the glyph outline of charCode, vertical layout's or not (a user font's only) */
static inline ScePvf_t_error pvfGetCharGlyphOutline(PvfFontHandle *handle,
	ScePvf_t_charCode charCode, FT_Outline *outline, ScePvf_t_s32 vertical)
{
	FT_Glyph_Metrics metrics;
	ScePvf_t_error error;
	ScePvf_t_s32 isVert;
	PvfLib *lib;

	if (handle == NULL || outline == NULL)
		return SCE_PVF_ERROR_ARG;
	if (handle->font->systemFile)
		return SCE_PVF_ERROR_NOSUPPORT;
	if (!pvfHasChar(handle, charCode)) {
		charCode = handle->font->lib->altCharCode;
		if (charCode == 0)
			return SCE_PVF_ERROR_NOGLYPH;
	}
	pvfClearGlyph(handle);
	pvfSetRasterizer(handle);
	lib = handle->font->lib;
	if (vertical)
		error = PvfRasterizerGetVertGlyphMetrics(lib->rasterizer, handle->font->face,
			handle->charCode, &metrics, &isVert);
	else
		error = PvfRasterizerGetGlyphMetrics(lib->rasterizer, handle->font->face,
			handle->charCode, &metrics);
	if (error != 0)
		return error;
	pvfSetGlyphMetrics(handle, &metrics);
	lib = handle->font->lib;
	if (vertical)
		return PvfRasterizerGetVertOutline(lib->rasterizer, handle->font->face, charCode,
			&outline->flags, &outline->n_contours, &outline->n_points,
			&outline->contours, &outline->points, &outline->tags, &isVert);
	return PvfRasterizerGetOutline(lib->rasterizer, handle->font->face, charCode,
		&outline->flags, &outline->n_contours, &outline->n_points, &outline->contours,
		&outline->points, &outline->tags);
}

ScePvf_t_error scePvfGetCharGlyphOutline(ScePvf_t_fontId fontID, ScePvf_t_charCode charCode,
	FT_Outline *outline)
{
	return pvfGetCharGlyphOutline(fontID, charCode, outline, 0);
}

ScePvf_t_error scePvfGetVertCharGlyphOutline(ScePvf_t_fontId fontID,
	ScePvf_t_charCode charCode, FT_Outline *outline)
{
	return pvfGetCharGlyphOutline(fontID, charCode, outline, 1);
}

ScePvf_t_error scePvfReleaseCharGlyphOutline(ScePvf_t_fontId fontID, FT_Outline *outline)
{
	PvfFontHandle *handle = fontID;

	if (handle == NULL || outline == NULL)
		return SCE_PVF_ERROR_ARG;
	return PvfRasterizerFreeOutline(handle->font->lib->rasterizer, outline->contours,
		outline->points, outline->tags);
}
#endif

/* a float no conversion can use: infinite, or the library's NaN */
static inline ScePvf_t_bool pvfIsBadFloat(ScePvf_t_f32 x)
{
	union {
		ScePvf_t_f32 f;
		ScePvf_t_u32 u;
	} v;

	v.f = x;
	return (v.u & 0x7FFFFFFF) == 0x7F800000 || v.u == 0xFF80FFFF;
}

/* pixel at resolution dpi, in points */
static inline ScePvf_t_f32 pvfPixelToPoint(ScePvf_t_f32 resolution, ScePvf_t_f32 pixel,
	ScePvf_t_error *errorCode)
{
	ScePvf_t_f32 point;

	if (resolution < 0.0f) {
		if (resolution > 0.0f)
			goto bad;
	} else if (resolution * 0.999f < 0.0f) {
		goto bad;
	}
	point = pixel * 72.0f / resolution;
	if (!pvfIsBadFloat(point))
		return point;
bad:
	*errorCode = SCE_PVF_ERROR_NOSUPPORT;
	return 0.0f;
}

/* point at resolution dpi, in pixels */
static inline ScePvf_t_f32 pvfPointToPixel(ScePvf_t_f32 resolution, ScePvf_t_f32 point,
	ScePvf_t_error *errorCode)
{
	ScePvf_t_f32 pixel = point * resolution * (1.0f / 72.0f);

	if (!pvfIsBadFloat(pixel))
		return pixel;
	*errorCode = SCE_PVF_ERROR_NOSUPPORT;
	return 0.0f;
}

ScePvf_t_f32 scePvfPixelToPointH(ScePvf_t_libId libID, ScePvf_t_f32 pixel,
	ScePvf_t_error *errorCode)
{
	PvfLib *lib = libID;

	if (errorCode == NULL)
		return 0.0f;
	*errorCode = 0;
	if (lib == NULL) {
		*errorCode = SCE_PVF_ERROR_LIBID;
		return 0.0f;
	}
	return pvfPixelToPoint(lib->resolution.h, pixel, errorCode);
}

ScePvf_t_f32 scePvfPixelToPointV(ScePvf_t_libId libID, ScePvf_t_f32 pixel,
	ScePvf_t_error *errorCode)
{
	PvfLib *lib = libID;

	if (errorCode == NULL)
		return 0.0f;
	*errorCode = 0;
	if (lib == NULL) {
		*errorCode = SCE_PVF_ERROR_LIBID;
		return 0.0f;
	}
	return pvfPixelToPoint(lib->resolution.v, pixel, errorCode);
}

ScePvf_t_f32 scePvfPointToPixelH(ScePvf_t_libId libID, ScePvf_t_f32 point,
	ScePvf_t_error *errorCode)
{
	PvfLib *lib = libID;

	if (errorCode == NULL)
		return 0.0f;
	*errorCode = 0;
	if (lib == NULL) {
		*errorCode = SCE_PVF_ERROR_LIBID;
		return 0.0f;
	}
	return pvfPointToPixel(lib->resolution.h, point, errorCode);
}

ScePvf_t_f32 scePvfPointToPixelV(ScePvf_t_libId libID, ScePvf_t_f32 point,
	ScePvf_t_error *errorCode)
{
	PvfLib *lib = libID;

	if (errorCode == NULL)
		return 0.0f;
	*errorCode = 0;
	if (lib == NULL) {
		*errorCode = SCE_PVF_ERROR_LIBID;
		return 0.0f;
	}
	return pvfPointToPixel(lib->resolution.v, point, errorCode);
}

ScePvf_t_error scePvfSetEM(ScePvf_t_libId libID, ScePvf_t_f32 emValue)
{
	PvfLib *lib = libID;

	if (lib == NULL)
		return SCE_PVF_ERROR_LIBID;
	if (emValue < 0.0f || emValue * 0.999f < 0.0f || emValue < 0.0f)
		return SCE_PVF_ERROR_ARG;
	lib->em = emValue;
	return 0;
}

ScePvf_t_error scePvfSetAltCharacterCode(ScePvf_t_libId libID, ScePvf_t_charCode charCode)
{
	PvfLib *lib = libID;

	if (lib == NULL)
		return SCE_PVF_ERROR_LIBID;
	lib->altCharCode = charCode;
	return 0;
}

ScePvf_t_error scePvfFlush(ScePvf_t_fontId fontID)
{
	PvfFontHandle *handle = fontID;

	if (handle == NULL)
		return SCE_PVF_ERROR_ARG;
	pvfClearGlyph(handle);
	return 0;
}
