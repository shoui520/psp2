/*
 * SceLibPgf (vs0:sys/external/libpgf.suprx): PSP-compatible PGF bitmap font library.
 */
#include <stdint.h>
#include <stddef.h>
#include <scetypes.h>
#include <font/libpgf.h>
#include <libsha1.h>

extern void *sceClibMemcpy_safe(void *dst, const void *src, SceSize len);
extern void *sceClibMemset(void *dst, int c, SceSize len);
extern int sceClibStrncmp(const char *lhs, const char *rhs, SceSize maxLen);
extern char *sceClibStrncpy(char *dst, const char *src, SceSize n);
extern char *sceClibStrncat(char *dst, const char *src, SceSize n);

typedef struct SceIoStat {
	uint32_t mode;
	uint32_t attr;
	int64_t size;
	uint8_t ctime[16];
	uint8_t atime[16];
	uint8_t mtime[16];
	uint32_t priv[6];
} SceIoStat;

extern SceUID sceIoOpen(const char *file, int flags, int mode);
extern int sceIoClose(SceUID fd);
extern int sceIoRead(SceUID fd, void *data, SceSize size);
extern int64_t sceIoLseek(SceUID fd, int64_t offset, int whence);
extern int sceIoGetstat(const char *file, SceIoStat *stat);

#define PGF_NUM_SYSTEM_FONTS		18
#define PGF_FONT_DIR_DEFAULT		"sa0:data/font/pgf/"

#define PGF_MODE_SYS_FILE		0U
#define PGF_MODE_SYS_MEM		1U
#define PGF_MODE_USER_CALLBACK		2U
#define PGF_MODE_USER_MEMFILE		3U
#define PGF_MODE_USER_MEMORY		4U

typedef struct PgfMemStream {
	uint32_t size;
	uint32_t pos;
	uintptr_t data;
} PgfMemStream;

typedef struct PgfTablePair {
	int32_t x;
	int32_t y;
} PgfTablePair;

typedef struct PgfCompRange {
	uint16_t start;
	uint16_t count;
} PgfCompRange;

typedef struct PgfLib PgfLib;
typedef struct PgfFontData PgfFontData;
typedef struct PgfFontHandle PgfFontHandle;

struct PgfFontData {
	char userFileName[64];			/* +0x000 */
	uint32_t accessMode;			/* +0x040 */
	int32_t fontIndex;			/* +0x044 */
	PgfLib *lib;				/* +0x048 */
	uint8_t inUse;				/* +0x04C */
	uint8_t refCount;			/* +0x04D */
	uint8_t loaded;				/* +0x04E */
	uint8_t loadStatePad;			/* +0x04F */
	void *streamHandle;			/* +0x050 */
	uint32_t revision;			/* +0x054 */
	uint32_t disabledFlag;			/* +0x058 */
	/* PGF file header copy (+0x05C .. +0x1F5) */
	uint8_t bigEndian;			/* +0x05C */
	uint8_t headerPad;			/* +0x05D */
	uint16_t headerSize;			/* +0x05E */
	uint8_t magic[4];			/* +0x060 */
	uint16_t version;			/* +0x064 */
	uint16_t headerVersion;			/* +0x066 */
	uint8_t headerFlags[3];			/* +0x068 */
	uint8_t headerFlagsPad;			/* +0x06B */
	uint32_t charMapLen;			/* +0x06C */
	uint32_t charPtrLen;			/* +0x070 */
	uint32_t charMapBpe;			/* +0x074 */
	uint32_t charPtrBpe;			/* +0x078 */
	uint8_t charMapAlign;			/* +0x07C */
	uint8_t charPtrAlign;			/* +0x07D */
	uint8_t bpp;				/* +0x07E */
	uint8_t bppPad;				/* +0x07F */
	uint32_t hSize64;			/* +0x080 */
	uint32_t vSize64;			/* +0x084 */
	uint32_t hRes64;			/* +0x088 */
	uint32_t vRes64;			/* +0x08C */
	uint8_t styleFlags;			/* +0x090 */
	char fontName[64];			/* +0x091 */
	char fontType[64];			/* +0x0D1 */
	uint8_t fontTypePad;			/* +0x111 */
	uint16_t firstGlyph;			/* +0x112 */
	uint16_t lastGlyph;			/* +0x114 */
	uint16_t glyphRangePad;			/* +0x116 */
	uint32_t maxMetricExtra[6];		/* +0x118 */
	int32_t maxGlyphAscender64;		/* +0x130 */
	int32_t maxGlyphDescender64;		/* +0x134 */
	int32_t maxGlyphLeftX64;		/* +0x138 */
	int32_t maxGlyphBaseY64;		/* +0x13C */
	int32_t minGlyphCenterX64;		/* +0x140 */
	int32_t maxGlyphTopY64;			/* +0x144 */
	int32_t maxGlyphAdvanceX64;		/* +0x148 */
	int32_t maxGlyphAdvanceY64;		/* +0x14C */
	uint32_t maxGlyphWidth64;		/* +0x150 */
	uint32_t maxGlyphHeight64;		/* +0x154 */
	uint16_t maxGlyphBitmapWidth;		/* +0x158 */
	uint16_t maxGlyphBitmapHeight;		/* +0x15A */
	uint16_t charPtrScale;			/* +0x15C */
	uint8_t dimTableLen;			/* +0x15E */
	uint8_t xAdjustTableLen;		/* +0x15F */
	uint8_t yAdjustTableLen;		/* +0x160 */
	uint8_t advanceTableLen;		/* +0x161 */
	uint8_t reservedHeader[100];		/* +0x162 */
	uint16_t headerTailPad;			/* +0x1C6 */
	uint32_t shadowMapLen;			/* +0x1C8 */
	uint32_t shadowMapBpe;			/* +0x1CC */
	uint8_t shadowMapAlign;			/* +0x1D0 */
	uint8_t shadowMapFlags;			/* +0x1D1 */
	uint16_t shadowMapAlignPad;		/* +0x1D2 */
	uint32_t shadowScaleX;			/* +0x1D4 */
	uint32_t shadowScaleY;			/* +0x1D8 */
	uint32_t shadowScaleX2;			/* +0x1DC */
	uint32_t shadowScaleY2;			/* +0x1E0 */
	uint32_t compCharMapBpe1;		/* +0x1E4 */
	uint16_t compCharMapLen1;		/* +0x1E8 */
	uint8_t compCharMapAlign1;		/* +0x1EA */
	uint8_t compCharMapFirstPad;		/* +0x1EB */
	uint32_t compCharMapBpe2;		/* +0x1EC */
	uint16_t compCharMapLen2;		/* +0x1F0 */
	uint8_t compCharMapAlign2;		/* +0x1F2 */
	uint8_t compCharMapSecondPad;		/* +0x1F3 */
	uint16_t compGlyphCount;		/* +0x1F4 */
	uint16_t compGlyphPad;			/* +0x1F6 */
	/* Pointers and state (+0x1F8 .. +0x233) */
	PgfTablePair *dimTable;			/* +0x1F8 */
	PgfTablePair *xAdjustTable;		/* +0x1FC */
	PgfTablePair *yAdjustTable;		/* +0x200 */
	PgfTablePair *advanceTable;		/* +0x204 */
	void *shadowMap;			/* +0x208 */
	PgfCompRange *compCharMap1;		/* +0x20C */
	PgfCompRange *compCharMap2;		/* +0x210 */
	uint8_t *charMap;			/* +0x214 */
	uint8_t *charPtrTable;			/* +0x218 */
	uint32_t charDataOffset;		/* +0x21C */
	uint32_t useExternalCache;		/* +0x220 */
	uint32_t headerTotalSize;		/* +0x224 */
	uint32_t isCompositeGlyph;		/* +0x228 */
	uint16_t compGlyphIds[4];		/* +0x22C */
};

struct PgfFontHandle {
	uint8_t inUse;				/* +0x00 */
	uint8_t handlePad;			/* +0x01 */
	uint16_t currentCharCode;		/* +0x02 */
	PgfFontData *fontData;			/* +0x04 */
	uint8_t *glyphRawBuf;			/* +0x08 */
	uint8_t *glyphBitmapBuf;		/* +0x0C */
	SceFont_t_charInfo charInfo;		/* +0x10 (60 bytes) */
};

struct PgfLib {
	SceFont_t_initRec initRec;		/* +0x00 (44 bytes) */
	PgfFontHandle *fontHandles;		/* +0x2C */
	PgfFontData *fontDataList;		/* +0x30 */
	uint16_t openHandleCount;		/* +0x34 */
	uint16_t openDataCount;			/* +0x36 */
	float hResolution;			/* +0x38 */
	float vResolution;			/* +0x3C */
	int32_t numSystemFonts;			/* +0x40 */
	void *extraAlloc;			/* +0x44 */
	uint16_t altCharCode;			/* +0x48 */
	uint16_t altCharPad;			/* +0x4A */
};

_Static_assert(sizeof(PgfFontData) == 564, "PgfFontData size");
_Static_assert(sizeof(PgfFontHandle) == 76, "PgfFontHandle size");
_Static_assert(sizeof(PgfLib) == 76, "PgfLib size");

static SceFont_t_fontStyleInfo g_pgfSystemFonts[PGF_NUM_SYSTEM_FONTS] = {
	{ 10.1f, 10.1f, 128.0f, 128.0f, 0.0f, 1, 103, 0, 1, 0, 1, "FTT-NewRodin Pro DB",    "jpn0.pgf",  0, 0 },
	{ 10.1f, 10.1f, 128.0f, 128.0f, 0.0f, 1,   1, 0, 2, 0, 1, "FTT-NewRodin Pro Latin", "ltn0.pgf",  0, 0 },
	{ 10.1f, 10.1f, 128.0f, 128.0f, 0.0f, 2,   1, 0, 2, 0, 1, "FTT-Matisse Pro Latin",  "ltn1.pgf",  0, 0 },
	{ 10.1f, 10.1f, 128.0f, 128.0f, 0.0f, 1,   2, 0, 2, 0, 1, "FTT-NewRodin Pro Latin", "ltn2.pgf",  0, 0 },
	{ 10.1f, 10.1f, 128.0f, 128.0f, 0.0f, 2,   2, 0, 2, 0, 1, "FTT-Matisse Pro Latin",  "ltn3.pgf",  0, 0 },
	{ 10.1f, 10.1f, 128.0f, 128.0f, 0.0f, 1,   5, 0, 2, 0, 1, "FTT-NewRodin Pro Latin", "ltn4.pgf",  0, 0 },
	{ 10.1f, 10.1f, 128.0f, 128.0f, 0.0f, 2,   5, 0, 2, 0, 1, "FTT-Matisse Pro Latin",  "ltn5.pgf",  0, 0 },
	{ 10.1f, 10.1f, 128.0f, 128.0f, 0.0f, 1,   6, 0, 2, 0, 1, "FTT-NewRodin Pro Latin", "ltn6.pgf",  0, 0 },
	{ 10.1f, 10.1f, 128.0f, 128.0f, 0.0f, 2,   6, 0, 2, 0, 1, "FTT-Matisse Pro Latin",  "ltn7.pgf",  0, 0 },
	{  7.0f,  7.0f, 128.0f, 128.0f, 0.0f, 1,   1, 0, 2, 0, 1, "FTT-NewRodin Pro Latin", "ltn8.pgf",  0, 0 },
	{  7.0f,  7.0f, 128.0f, 128.0f, 0.0f, 2,   1, 0, 2, 0, 1, "FTT-Matisse Pro Latin",  "ltn9.pgf",  0, 0 },
	{  7.0f,  7.0f, 128.0f, 128.0f, 0.0f, 1,   2, 0, 2, 0, 1, "FTT-NewRodin Pro Latin", "ltn10.pgf", 0, 0 },
	{  7.0f,  7.0f, 128.0f, 128.0f, 0.0f, 2,   2, 0, 2, 0, 1, "FTT-Matisse Pro Latin",  "ltn11.pgf", 0, 0 },
	{  7.0f,  7.0f, 128.0f, 128.0f, 0.0f, 1,   5, 0, 2, 0, 1, "FTT-NewRodin Pro Latin", "ltn12.pgf", 0, 0 },
	{  7.0f,  7.0f, 128.0f, 128.0f, 0.0f, 2,   5, 0, 2, 0, 1, "FTT-Matisse Pro Latin",  "ltn13.pgf", 0, 0 },
	{  7.0f,  7.0f, 128.0f, 128.0f, 0.0f, 1,   6, 0, 2, 0, 1, "FTT-NewRodin Pro Latin", "ltn14.pgf", 0, 0 },
	{  7.0f,  7.0f, 128.0f, 128.0f, 0.0f, 2,   6, 0, 2, 0, 1, "FTT-Matisse Pro Latin",  "ltn15.pgf", 0, 0 },
	{ 10.1f, 10.1f, 128.0f, 128.0f, 0.0f, 1,   1, 0, 3, 0, 3, "AsiaNHH(512Johab)",      "kr0.pgf",   0, 0 },
};

static char g_pgfEmptyName[8];
static char g_pgfFontDir[1024];
static uint32_t g_pgfCacheEmptyComp[2];
static uint32_t g_pgfCacheEmptyNormal[2];

static inline int pgfFloatApproxEq(float a, float b)
{
	if (a < b)
		return a > b * 0.999f;
	return b > a * 0.999f;
}

static inline uint32_t pgfFloatBits(float f)
{
	union { float f; uint32_t u; } v;
	v.f = f;
	return v.u;
}

static inline __attribute__((always_inline)) uint32_t pgfUdiv(uint32_t n, uint32_t d)
{
	uint32_t q = 0, r = 0;
	int i;

	if (d == 0)
		return 0;
	if ((d & (d - 1U)) == 0)
		return n >> __builtin_ctz(d);
	for (i = 31; i >= 0; i--) {
		r = (r << 1) | ((n >> i) & 1U);
		if (r >= d) {
			r -= d;
			q |= (1U << i);
		}
	}
	return q;
}

static inline int pgfIsMemMode(uint32_t mode)
{
	return mode == PGF_MODE_SYS_MEM || mode == PGF_MODE_USER_MEMFILE || mode == PGF_MODE_USER_MEMORY;
}

static inline int pgfStreamSeek(PgfLib *lib, uint32_t mode, void *handle, uint32_t pos)
{
	if (pgfIsMemMode(mode)) {
		PgfMemStream *ms = (PgfMemStream *)handle;
		if (ms->size < pos)
			return SCE_FONT_ERROR_FILESEEK;
		ms->pos = pos;
		return 0;
	}
	if (mode == PGF_MODE_USER_CALLBACK) {
		if (lib->initRec.seekFunc == NULL)
			return SCE_FONT_ERROR_FILESEEK;
		if (lib->initRec.seekFunc(lib->initRec.userData, handle, pos) != 0)
			return SCE_FONT_ERROR_FILESEEK;
		return 0;
	}
	if (sceIoLseek((SceUID)(uintptr_t)handle, (int64_t)pos, 0) < 0)
		return SCE_FONT_ERROR_FILESEEK;
	return 0;
}

static inline uint32_t pgfReadBits(const uint8_t *buf, uint32_t bitOff, uint32_t nbits)
{
	uint32_t val = 0;
	uint32_t i;
	const uint8_t *p = buf + (bitOff >> 3);
	uint32_t bit = bitOff & 7U;

	for (i = 0; i < nbits; i++) {
		if (((*p >> bit) & 1U) != 0)
			val |= (1U << i);
		if (bit == 7U) {
			p++;
			bit = 0;
		} else {
			bit++;
		}
	}
	return val;
}

/* 0x81000834 */
int pgfDrawGlyphDirect4(const uint8_t *src, int width, int height,
			SceFont_t_userImageBufferRec *img,
			int clipX, int clipY, int clipW, int clipH,
			int offX64, int offY64)
{
	int xPos = img->xPos64;
	int bpl = img->bytesPerLine;
	int byteX0 = (offX64 + xPos) >> 7;
	int curY = (offY64 + img->yPos64) >> 6;
	int pixX0 = (offX64 + xPos) >> 6;
	int subX = xPos % 64;
	int maxY = clipY + clipH;
	int maxX = clipX + clipW;
	int minY = (clipY < 0) ? 0 : clipY;
	int minX = (clipX < 0) ? 0 : clipX;
	uint8_t *rowPtr;

	if (subX < 0)
		subX += 64;
	if ((uint32_t)maxY >= img->rect.height)
		maxY = img->rect.height;
	if ((uint32_t)maxX >= img->rect.width)
		maxX = img->rect.width;

	rowPtr = img->buffer + byteX0 + curY * bpl;
	if (height > 0) {
		int endY = curY + height;
		int oddStart = pixX0 % 2;
		if (oddStart == -1)
			oddStart = 1;

		for (; curY < endY; curY++, rowPtr += img->bytesPerLine, src += width) {
			if (curY < minY) {
				if (curY >= maxY)
					return 0;
				continue;
			}
			if (curY >= maxY)
				return 0;
			if (width <= 0)
				continue;

			{
				int x = pixX0;
				int bx = byteX0;
				int odd = oddStart;
				uint8_t *dst = rowPtr;
				const uint8_t *sp = src;
				int endX = pixX0 + width;

				for (; x < endX; x++, odd ^= 1, sp++) {
					if (bx < 0 || x < minX) {
						if (x >= maxX)
							break;
						if (odd == 1) {
							if (x + 1 >= (int)img->rect.width)
								break;
							bx++;
							if (bx >= 0 && bx < (int)img->bytesPerLine) {
								uint32_t spill = (uint32_t)(subX * (*sp)) >> 6;
								uint32_t lo = (dst[1] & 0x0FU) + spill;
								if (lo > 15U)
									lo = 15U;
								dst[1] = (uint8_t)((dst[1] & 0xF0U) | lo);
							}
							dst++;
						}
						continue;
					}
					if (x >= maxX)
						break;

					{
						uint32_t pix = *sp;
						uint32_t spill = (uint32_t)(subX * pix) >> 6;
						if (odd != 1) {
							uint32_t lo = (dst[0] & 0x0FU) + pix - spill;
							uint32_t hi;
							if (lo > 15U)
								lo = 15U;
							dst[0] = (uint8_t)((dst[0] & 0xF0U) | lo);
							hi = (dst[0] >> 4) + spill;
							if (hi > 15U)
								hi = 15U;
							dst[0] = (uint8_t)((dst[0] & 0x0FU) | (hi << 4));
						} else {
							uint32_t hi = (dst[0] >> 4) + pix - spill;
							if (hi > 15U)
								hi = 15U;
							dst[0] = (uint8_t)((dst[0] & 0x0FU) | (hi << 4));
							bx++;
							if (bx < (int)img->bytesPerLine) {
								uint32_t lo = (dst[1] & 0x0FU) + spill;
								if (lo > 15U)
									lo = 15U;
								dst[1] = (uint8_t)((dst[1] & 0xF0U) | lo);
							}
							dst++;
						}
					}
				}
			}
		}
	}
	return 0;
}

/* 0x81000a40 */
int pgfDrawGlyphDirect8(const uint8_t *src, int width, int height,
			SceFont_t_userImageBufferRec *img,
			int clipX, int clipY, int clipW, int clipH,
			int offX64, int offY64)
{
	int xPos = img->xPos64;
	int pixX0 = (offX64 + xPos) >> 6;
	int curY = (offY64 + img->yPos64) >> 6;
	int subX = xPos % 64;
	int maxY = clipY + clipH;
	int maxX = clipX + clipW;
	int minY = (clipY < 0) ? 0 : clipY;
	int minX = (clipX < 0) ? 0 : clipX;
	uint8_t *rowPtr;

	if (subX < 0)
		subX += 64;
	if ((uint32_t)maxY >= img->rect.height)
		maxY = img->rect.height;
	if ((uint32_t)maxX >= img->rect.width)
		maxX = img->rect.width;

	rowPtr = img->buffer + pixX0 + curY * img->bytesPerLine;
	if (height > 0) {
		int endY = curY + height;
		for (; curY < endY; curY++, rowPtr += img->bytesPerLine, src += width) {
			if (curY < minY) {
				if (curY >= maxY)
					return 0;
				continue;
			}
			if (curY >= maxY)
				return 0;
			if (width <= 0)
				continue;

			{
				int x = pixX0;
				uint8_t *dst = rowPtr;
				const uint8_t *sp = src;
				int endX = pixX0 + width;

				for (; x < endX; x++, dst++, sp++) {
					if (x < minX) {
						if (x >= maxX || x + 1 >= (int)img->rect.width)
							break;
						if (x >= -1) {
							uint32_t pix8 = *sp | ((uint32_t)*sp << 4);
							uint32_t v = dst[1] + (((uint32_t)subX * pix8) >> 6);
							if (v > 255U)
								v = 255U;
							dst[1] = (uint8_t)v;
						}
						continue;
					}
					if (x >= maxX)
						break;

					{
						uint32_t pix8 = *sp | ((uint32_t)*sp << 4);
						uint32_t spill = ((uint32_t)subX * pix8) >> 6;
						uint32_t firstPix = dst[0] + pix8 - spill;
						if (firstPix > 255U)
							firstPix = 255U;
						dst[0] = (uint8_t)firstPix;
						if (x + 1 < (int)img->rect.width) {
							uint32_t nextPix = dst[1] + spill;
							if (nextPix > 255U)
								nextPix = 255U;
							dst[1] = (uint8_t)nextPix;
						}
					}
				}
			}
		}
	}
	return 0;
}

/* 0x81000bc8 */
int pgfStyleMask(const SceFont_t_fontStyleInfo *style)
{
	int mask = 0;

	if (style == NULL)
		return 0;
	if (style->hSize > 0.0f)
		mask |= 0x0001;
	if (style->vSize > 0.0f)
		mask |= 0x0002;
	if (style->hResolution > 0.0f)
		mask |= 0x0004;
	if (style->vResolution > 0.0f)
		mask |= 0x0008;
	if (style->weight > 0.0f)
		mask |= 0x0010;
	if (style->familyCode != 0)
		mask |= 0x0020;
	if (style->style != 0)
		mask |= 0x0040;
	if (style->subStyle != 0)
		mask |= 0x0080;
	if (style->languageCode != 0)
		mask |= 0x0100;
	if (style->regionCode != 0)
		mask |= 0x0200;
	if (style->countryCode != 0)
		mask |= 0x0400;
	if (style->fontName[0] != 0)
		mask |= 0x0800;
	if (style->fileName[0] != 0)
		mask |= 0x1000;
	return mask;
}

/* 0x81000c70 */
uint32_t pgfStreamRead(PgfLib *lib, uint32_t mode, void *handle, void *dst,
		       uint32_t elemSize, uint32_t count, SceFont_t_error *err)
{
	switch (mode) {
	case PGF_MODE_SYS_MEM:
	case PGF_MODE_USER_MEMFILE:
	case PGF_MODE_USER_MEMORY: {
		PgfMemStream *ms = (PgfMemStream *)handle;
		uint32_t total = elemSize * count;
		if (ms->size < ms->pos + total) {
			*err = SCE_FONT_ERROR_FILEREAD;
			return 0;
		}
		sceClibMemcpy_safe(dst, (const void *)(ms->data + ms->pos), total);
		ms->pos += total;
		*err = 0;
		return count;
	}
	case PGF_MODE_USER_CALLBACK:
		if (lib->initRec.readFunc == NULL) {
			*err = SCE_FONT_ERROR_FILEREAD;
			return 0;
		}
		return lib->initRec.readFunc(lib->initRec.userData, handle, dst, elemSize, count, err);
	default: {
		uint32_t req = elemSize * count;
		uint32_t got = (uint32_t)sceIoRead((SceUID)(uintptr_t)handle, dst, req);
		if (got < req) {
			*err = SCE_FONT_ERROR_FILEREAD;
			return 0;
		}
		*err = 0;
		return pgfUdiv(got, elemSize);
	}
	}
}

/* 0x81000cfc */
void pgfWriteU16(int bigEndian, uint8_t *dst, uint16_t val)
{
	uint8_t tmp[2];

	tmp[0] = (uint8_t)(val & 0xFFU);
	tmp[1] = (uint8_t)(val >> 8);
	if (bigEndian != 0) {
		dst[0] = (uint8_t)(val & 0xFFU);
		dst[1] = (uint8_t)(val >> 8);
	} else {
		dst[0] = tmp[0];
		dst[1] = tmp[1];
	}
}

/* 0x81000d38 */
void pgfWriteU32(int bigEndian, uint8_t *dst, uint32_t val)
{
	uint8_t tmp[4];

	tmp[0] = (uint8_t)(val & 0xFFU);
	tmp[1] = (uint8_t)((val >> 8) & 0xFFU);
	tmp[2] = (uint8_t)((val >> 16) & 0xFFU);
	tmp[3] = (uint8_t)(val >> 24);
	if (bigEndian != 0) {
		dst[0] = (uint8_t)(val & 0xFFU);
		dst[1] = (uint8_t)((val >> 8) & 0xFFU);
		dst[2] = (uint8_t)((val >> 16) & 0xFFU);
		dst[3] = (uint8_t)(val >> 24);
	} else {
		dst[0] = tmp[0];
		dst[1] = tmp[1];
		dst[2] = tmp[2];
		dst[3] = tmp[3];
	}
}

/* 0x81000d84 */
int pgfSwapMetricTables(PgfFontData *fd)
{
	int i;

	for (i = 0; i < (int)fd->dimTableLen; i++) {
		pgfWriteU32(fd->bigEndian, (uint8_t *)&fd->dimTable[i].x, (uint32_t)fd->dimTable[i].x);
		pgfWriteU32(fd->bigEndian, (uint8_t *)&fd->dimTable[i].y, (uint32_t)fd->dimTable[i].y);
	}
	for (i = 0; i < (int)fd->xAdjustTableLen; i++) {
		pgfWriteU32(fd->bigEndian, (uint8_t *)&fd->xAdjustTable[i].x, (uint32_t)fd->xAdjustTable[i].x);
		pgfWriteU32(fd->bigEndian, (uint8_t *)&fd->xAdjustTable[i].y, (uint32_t)fd->xAdjustTable[i].y);
	}
	for (i = 0; i < (int)fd->yAdjustTableLen; i++) {
		pgfWriteU32(fd->bigEndian, (uint8_t *)&fd->yAdjustTable[i].x, (uint32_t)fd->yAdjustTable[i].x);
		pgfWriteU32(fd->bigEndian, (uint8_t *)&fd->yAdjustTable[i].y, (uint32_t)fd->yAdjustTable[i].y);
	}
	for (i = 0; i < (int)fd->advanceTableLen; i++) {
		pgfWriteU32(fd->bigEndian, (uint8_t *)&fd->advanceTable[i].x, (uint32_t)fd->advanceTable[i].x);
		pgfWriteU32(fd->bigEndian, (uint8_t *)&fd->advanceTable[i].y, (uint32_t)fd->advanceTable[i].y);
	}
	return 0;
}

/* 0x81001038 */
int pgfCloseFontData(PgfFontData *fd)
{
	PgfLib *lib;

	if (fd == NULL || fd->inUse == 0)
		return 0;
	fd->refCount--;
	if (fd->refCount != 0)
		return 0;

	lib = fd->lib;
	if (pgfIsMemMode(fd->accessMode)) {
		pgfSwapMetricTables(fd);
		fd->charMap = NULL;
		fd->charPtrTable = NULL;
		fd->compCharMap1 = NULL;
		fd->compCharMap2 = NULL;
		fd->shadowMap = NULL;
		fd->dimTable = NULL;
		fd->xAdjustTable = NULL;
		fd->yAdjustTable = NULL;
		fd->advanceTable = NULL;
	} else {
		if (fd->charPtrTable != NULL) {
			lib->initRec.freeFunc(lib->initRec.userData, fd->charPtrTable);
			fd->charPtrTable = NULL;
		}
		if (fd->charMap != NULL) {
			lib->initRec.freeFunc(lib->initRec.userData, fd->charMap);
			fd->charMap = NULL;
		}
		if (fd->compCharMap1 != NULL) {
			lib->initRec.freeFunc(lib->initRec.userData, fd->compCharMap1);
			fd->compCharMap1 = NULL;
		}
		if (fd->compCharMap2 != NULL) {
			lib->initRec.freeFunc(lib->initRec.userData, fd->compCharMap2);
			fd->compCharMap2 = NULL;
		}
		if (fd->shadowMap != NULL) {
			lib->initRec.freeFunc(lib->initRec.userData, fd->shadowMap);
			fd->shadowMap = NULL;
		}
		if (fd->dimTable != NULL) {
			lib->initRec.freeFunc(lib->initRec.userData, fd->dimTable);
			fd->dimTable = NULL;
		}
		if (fd->xAdjustTable != NULL) {
			lib->initRec.freeFunc(lib->initRec.userData, fd->xAdjustTable);
			fd->xAdjustTable = NULL;
		}
		if (fd->yAdjustTable != NULL) {
			lib->initRec.freeFunc(lib->initRec.userData, fd->yAdjustTable);
			fd->yAdjustTable = NULL;
		}
		if (fd->advanceTable != NULL) {
			lib->initRec.freeFunc(lib->initRec.userData, fd->advanceTable);
			fd->advanceTable = NULL;
		}
	}

	switch (fd->accessMode) {
	case PGF_MODE_SYS_MEM:
	case PGF_MODE_USER_MEMFILE:
	case PGF_MODE_USER_MEMORY:
		lib->initRec.freeFunc(lib->initRec.userData, fd->streamHandle);
		break;
	case PGF_MODE_USER_CALLBACK:
		if (lib->initRec.closeFunc == NULL ||
		    lib->initRec.closeFunc(lib->initRec.userData, fd->streamHandle) != 0)
			return SCE_FONT_ERROR_FILECLOSE;
		break;
	default:
		sceIoClose((SceUID)(uintptr_t)fd->streamHandle);
		break;
	}

	fd->inUse = 0;
	fd->lib->openDataCount--;
	return 0;
}

/* 0x81001184 */
void pgfDecodeGlyphBitmap(const uint8_t *src, uint8_t *dst, int width, int height,
			  int direction, uint32_t bitOff)
{
	if (direction == 1) {
		int x = 0, y = 0;
		for (;;) {
			uint32_t code = pgfReadBits(src, bitOff, 4);
			bitOff += 4;
			if ((code & 8U) == 0) {
				uint32_t count = code + 1U;
				uint8_t val = (uint8_t)pgfReadBits(src, bitOff, 4);
				bitOff += 4;
				while (count-- != 0) {
					*dst++ = val;
					if (++x == width) {
						x = 0;
						if (++y == height)
							return;
					}
				}
			} else {
				uint32_t count = 16U - code;
				while (count-- != 0) {
					uint8_t val = (uint8_t)pgfReadBits(src, bitOff, 4);
					bitOff += 4;
					*dst++ = val;
					if (++x == width) {
						x = 0;
						if (++y == height)
							return;
					}
				}
			}
		}
	} else if (direction == 2) {
		int x = 0, y = 0;
		uint8_t *colStart = dst;
		for (;;) {
			uint32_t code = pgfReadBits(src, bitOff, 4);
			bitOff += 4;
			if ((code & 8U) == 0) {
				uint32_t count = code + 1U;
				uint8_t val = (uint8_t)pgfReadBits(src, bitOff, 4);
				bitOff += 4;
				while (count-- != 0) {
					*dst = val;
					dst += width;
					if (++y == height) {
						y = 0;
						dst = ++colStart;
						if (++x == width)
							return;
					}
				}
			} else {
				uint32_t count = 16U - code;
				while (count-- != 0) {
					uint8_t val = (uint8_t)pgfReadBits(src, bitOff, 4);
					bitOff += 4;
					*dst = val;
					if (++y == height) {
						y = 0;
						dst = ++colStart;
						if (++x == width)
							return;
					} else {
						dst += width;
					}
				}
			}
		}
	}
}

static inline __attribute__((always_inline)) PgfFontData *pgfAbortFontOpen(
	PgfLib *lib, PgfFontData *fd, int freeTables)
{
	if (freeTables) {
		if (fd->shadowMap != NULL) {
			lib->initRec.freeFunc(lib->initRec.userData, fd->shadowMap);
			fd->shadowMap = NULL;
		}
		if (fd->compCharMap1 != NULL) {
			lib->initRec.freeFunc(lib->initRec.userData, fd->compCharMap1);
			fd->compCharMap1 = NULL;
		}
		if (fd->compCharMap2 != NULL) {
			lib->initRec.freeFunc(lib->initRec.userData, fd->compCharMap2);
			fd->compCharMap2 = NULL;
		}
		if (fd->charMap != NULL) {
			lib->initRec.freeFunc(lib->initRec.userData, fd->charMap);
			fd->charMap = NULL;
		}
		if (fd->charPtrTable != NULL) {
			lib->initRec.freeFunc(lib->initRec.userData, fd->charPtrTable);
			fd->charPtrTable = NULL;
		}
	}
	fd->inUse = 1;
	lib->openDataCount++;
	fd->refCount++;
	pgfCloseFontData(fd);
	return NULL;
}

/* 0x81001554 */
PgfFontData *pgfOpenFontData(PgfLib *lib, int32_t fontIndex, const char *userFile,
			     uintptr_t userMem, uint32_t userMemSize,
			     uint32_t mode, SceFont_t_error *err)
{
	int32_t numSys;
	int32_t maxFonts, i;
	PgfFontData *fd;
	void *stream = NULL;
	SceFont_t_error ioErr = 0;
	uint8_t hdr12[12];
	uint8_t hdrBody[400];
	char pathBuf[1092];
	uint32_t hdrTotal, metricBytes, shadowBytes, comp1Bytes, comp2Bytes, charMapBytes, charPtrBytes;

	*err = 0;
	if (lib == NULL) {
		*err = SCE_FONT_ERROR_LIBID;
		return NULL;
	}
	numSys = lib->numSystemFonts;
	if (numSys < 0) {
		numSys = PGF_NUM_SYSTEM_FONTS;
		lib->numSystemFonts = PGF_NUM_SYSTEM_FONTS;
		if (*err != 0)
			return NULL;
	}

	if (mode == PGF_MODE_USER_CALLBACK || mode == PGF_MODE_USER_MEMFILE) {
		if (userFile == NULL) {
			*err = SCE_FONT_ERROR_ARG;
			return NULL;
		}
	} else if (mode == PGF_MODE_USER_MEMORY) {
		if (userMem == 0 || userMemSize == 0) {
			*err = SCE_FONT_ERROR_ARG;
			return NULL;
		}
	} else {
		if (fontIndex < 0 || fontIndex >= numSys) {
			*err = SCE_FONT_ERROR_ARG;
			return NULL;
		}
	}

	maxFonts = (int32_t)lib->initRec.maxNumFonts;
	if (maxFonts <= 0) {
		*err = SCE_FONT_ERROR_TOOMANYOPENED;
		return NULL;
	}

	/* Check if already opened */
	for (i = 0; i < maxFonts; i++) {
		fd = &lib->fontDataList[i];
		if (!fd->inUse || fd->accessMode != mode)
			continue;
		if (mode == PGF_MODE_USER_CALLBACK || mode == PGF_MODE_USER_MEMFILE) {
			if (sceClibStrncmp(userFile, fd->userFileName, 64) == 0 && fd->refCount != 255U) {
				fd->refCount++;
				return fd;
			}
		} else if (mode == PGF_MODE_USER_MEMORY) {
			PgfMemStream *ms = (PgfMemStream *)fd->streamHandle;
			if (ms->data == userMem && fd->refCount != 255U) {
				fd->refCount++;
				return fd;
			}
		} else {
			if (fd->fontIndex == fontIndex && fd->refCount != 255U) {
				fd->refCount++;
				return fd;
			}
		}
	}

	/* Find free slot */
	fd = NULL;
	for (i = 0; i < maxFonts; i++) {
		if (!lib->fontDataList[i].inUse) {
			fd = &lib->fontDataList[i];
			break;
		}
	}
	if (fd == NULL) {
		*err = SCE_FONT_ERROR_TOOMANYOPENED;
		return NULL;
	}

	/* Build path */
	if (mode == PGF_MODE_USER_CALLBACK || mode == PGF_MODE_USER_MEMFILE) {
		sceClibStrncpy(pathBuf, userFile, 1087U);
	} else if (mode != PGF_MODE_USER_MEMORY) {
		int dirLen = 0, nameLen = 0;
		const char *fn;
		while (dirLen < 64 && g_pgfFontDir[dirLen] != '\0')
			dirLen++;
		if (fontIndex >= 0 && fontIndex < lib->numSystemFonts)
			fn = (const char *)g_pgfSystemFonts[fontIndex].fileName;
		else
			fn = g_pgfEmptyName;
		while (nameLen < 64 && fn[nameLen] != '\0')
			nameLen++;
		if (dirLen + nameLen > 63) {
			*err = SCE_FONT_ERROR_NOFILE;
			return NULL;
		}
		sceClibStrncpy(pathBuf, g_pgfFontDir, 1023U);
		pathBuf[1087] = '\0';
		sceClibStrncat(pathBuf, fn, 63U);
	}
	pathBuf[1087] = '\0';

	/* Open stream */
	switch (mode) {
	case PGF_MODE_SYS_MEM:
	case PGF_MODE_USER_MEMFILE: {
		SceIoStat st;
		PgfMemStream *ms;
		SceUID iofd;
		*err = 0;
		if (sceIoGetstat(pathBuf, &st) != 0) {
			*err = SCE_FONT_ERROR_FILEOPEN;
			return NULL;
		}
		ms = (PgfMemStream *)lib->initRec.allocFunc(lib->initRec.userData, (uint32_t)st.size + 12U);
		if (ms == NULL) {
			*err = SCE_FONT_ERROR_FILEOPEN;
			return NULL;
		}
		iofd = sceIoOpen(pathBuf, 1, 0);
		if (iofd < 0) {
			lib->initRec.freeFunc(lib->initRec.userData, ms);
			*err = SCE_FONT_ERROR_FILEOPEN;
			return NULL;
		}
		if (sceIoRead(iofd, ms + 1, (SceSize)st.size) < st.size)
			*err = SCE_FONT_ERROR_FILEOPEN;
		sceIoClose(iofd);
		if (*err != 0) {
			lib->initRec.freeFunc(lib->initRec.userData, ms);
			*err = SCE_FONT_ERROR_FILEOPEN;
			return NULL;
		}
		ms->size = (uint32_t)st.size;
		ms->pos = 0;
		ms->data = (uintptr_t)(ms + 1);
		stream = ms;
		break;
	}
	case PGF_MODE_USER_CALLBACK:
		if (lib->initRec.openFunc == NULL) {
			*err = SCE_FONT_ERROR_FILEOPEN;
			return NULL;
		}
		stream = lib->initRec.openFunc(lib->initRec.userData, pathBuf, err);
		if (*err != 0) {
			*err = SCE_FONT_ERROR_FILEOPEN;
			return NULL;
		}
		break;
	case PGF_MODE_USER_MEMORY: {
		PgfMemStream *ms = (PgfMemStream *)lib->initRec.allocFunc(lib->initRec.userData, 12U);
		if (ms == NULL) {
			*err = SCE_FONT_ERROR_FILEOPEN;
			return NULL;
		}
		ms->size = userMemSize;
		ms->pos = 0;
		ms->data = userMem;
		stream = ms;
		break;
	}
	default: {
		SceUID iofd = sceIoOpen(pathBuf, 1, 0);
		if (iofd < 0) {
			*err = SCE_FONT_ERROR_FILEOPEN;
			return NULL;
		}
		stream = (void *)(uintptr_t)iofd;
		*err = 0;
		break;
	}
	}

	fd->accessMode = mode;
	fd->useExternalCache = 1;
	switch (mode) {
	case PGF_MODE_SYS_MEM:
		fd->fontIndex = fontIndex;
		fd->userFileName[0] = '\0';
		fd->useExternalCache = 0;
		break;
	case PGF_MODE_USER_CALLBACK:
		sceClibStrncpy(fd->userFileName, userFile, 63U);
		fd->userFileName[63] = '\0';
		fd->fontIndex = numSys;
		break;
	case PGF_MODE_USER_MEMFILE:
		sceClibStrncpy(fd->userFileName, userFile, 63U);
		fd->userFileName[63] = '\0';
		fd->useExternalCache = 0;
		fd->fontIndex = numSys;
		break;
	case PGF_MODE_USER_MEMORY:
		fd->userFileName[0] = '\0';
		fd->useExternalCache = 0;
		fd->fontIndex = numSys;
		break;
	default:
		fd->fontIndex = fontIndex;
		fd->userFileName[0] = '\0';
		break;
	}

	fd->lib = lib;
	fd->streamHandle = stream;
	fd->revision = 0;
	fd->disabledFlag = 0;
	fd->loaded = 0;
	fd->dimTable = NULL;
	fd->xAdjustTable = NULL;
	fd->yAdjustTable = NULL;
	fd->advanceTable = NULL;
	fd->shadowMap = NULL;
	fd->charMap = NULL;
	fd->charPtrTable = NULL;
	fd->compCharMap1 = NULL;
	fd->compCharMap2 = NULL;
	fd->charDataOffset = 0;
	fd->headerTotalSize = 0;

	if (pgfStreamSeek(lib, mode, stream, 0) != 0) {
		*err = SCE_FONT_ERROR_FILESEEK;
		return pgfAbortFontOpen(lib, fd, 0);
	}

	if (pgfStreamRead(lib, mode, stream, hdr12, 12U, 1U, &ioErr) == 0 || ioErr != 0) {
		*err = SCE_FONT_ERROR_FILEREAD;
		return pgfAbortFontOpen(lib, fd, 0);
	}

	{
		uint16_t ver = (uint16_t)(hdr12[8] | ((uint16_t)hdr12[9] << 8));
		uint32_t bodySz;
		if (ver == 3U) {
			bodySz = 400U;
		} else if (ver == 1U || ver == 2U) {
			bodySz = 380U;
		} else {
			*err = SCE_FONT_ERROR_ILLEGALVERSION;
			return pgfAbortFontOpen(lib, fd, 0);
		}

		if (pgfStreamRead(lib, mode, stream, hdrBody, bodySz, 1U, &ioErr) == 0 || ioErr != 0) {
			*err = SCE_FONT_ERROR_FILEREAD;
			return pgfAbortFontOpen(lib, fd, 0);
		}

		fd->bigEndian = hdr12[0];
		fd->headerPad = hdr12[1];
		fd->headerSize = (uint16_t)(hdr12[2] | ((uint16_t)hdr12[3] << 8));
		fd->magic[0] = hdr12[4];
		fd->magic[1] = hdr12[5];
		fd->magic[2] = hdr12[6];
		fd->magic[3] = hdr12[7];
		fd->version = ver;
		fd->headerVersion = (uint16_t)(hdr12[10] | ((uint16_t)hdr12[11] << 8));

		fd->headerFlags[0] = hdrBody[0];
		fd->headerFlags[1] = hdrBody[1];
		fd->headerFlags[2] = hdrBody[2];
		sceClibMemcpy_safe(&fd->charMapLen, hdrBody + 4, 4);
		sceClibMemcpy_safe(&fd->charPtrLen, hdrBody + 8, 4);
		sceClibMemcpy_safe(&fd->charMapBpe, hdrBody + 12, 4);
		sceClibMemcpy_safe(&fd->charPtrBpe, hdrBody + 16, 4);
		fd->charMapAlign = hdrBody[20];
		fd->charPtrAlign = hdrBody[21];
		fd->bpp = hdrBody[22];
		sceClibMemcpy_safe(&fd->hSize64, hdrBody + 24, 4);
		sceClibMemcpy_safe(&fd->vSize64, hdrBody + 28, 4);
		sceClibMemcpy_safe(&fd->hRes64, hdrBody + 32, 4);
		sceClibMemcpy_safe(&fd->vRes64, hdrBody + 36, 4);
		fd->styleFlags = hdrBody[40];
		sceClibMemcpy_safe(fd->fontName, hdrBody + 41, 64);
		sceClibMemcpy_safe(fd->fontType, hdrBody + 105, 64);
		sceClibMemcpy_safe(&fd->firstGlyph, hdrBody + 170, 2);
		sceClibMemcpy_safe(&fd->lastGlyph, hdrBody + 172, 2);
		sceClibMemcpy_safe(fd->maxMetricExtra, hdrBody + 176, 64);
		sceClibMemcpy_safe(&fd->maxGlyphBitmapWidth, hdrBody + 240, 2);
		sceClibMemcpy_safe(&fd->maxGlyphBitmapHeight, hdrBody + 242, 2);
		sceClibMemcpy_safe(&fd->charPtrScale, hdrBody + 244, 2);
		fd->dimTableLen = hdrBody[246];
		fd->xAdjustTableLen = hdrBody[247];
		fd->yAdjustTableLen = hdrBody[248];
		fd->advanceTableLen = hdrBody[249];
		sceClibMemcpy_safe(fd->reservedHeader, hdrBody + 250, 100);
		sceClibMemcpy_safe(&fd->shadowMapLen, hdrBody + 352, 4);
		sceClibMemcpy_safe(&fd->shadowMapBpe, hdrBody + 356, 4);
		fd->shadowMapAlign = hdrBody[360];
		fd->shadowMapFlags = hdrBody[361];
		sceClibMemcpy_safe(&fd->shadowScaleX, hdrBody + 364, 4);
		sceClibMemcpy_safe(&fd->shadowScaleY, hdrBody + 368, 4);
		sceClibMemcpy_safe(&fd->shadowScaleX2, hdrBody + 372, 4);
		sceClibMemcpy_safe(&fd->shadowScaleY2, hdrBody + 376, 4);

		if (ver == 3U) {
			sceClibMemcpy_safe(&fd->compCharMapBpe1, hdrBody + 380, 4);
			sceClibMemcpy_safe(&fd->compCharMapLen1, hdrBody + 384, 2);
			fd->compCharMapAlign1 = hdrBody[386];
			sceClibMemcpy_safe(&fd->compCharMapBpe2, hdrBody + 388, 4);
			sceClibMemcpy_safe(&fd->compCharMapLen2, hdrBody + 392, 2);
			fd->compCharMapAlign2 = hdrBody[394];
			sceClibMemcpy_safe(&fd->compGlyphCount, hdrBody + 396, 2);
		} else {
			fd->compCharMapBpe1 = 0;
			fd->compCharMapLen1 = 0;
			fd->compCharMapAlign1 = 0;
			fd->compCharMapBpe2 = 0;
			fd->compCharMapLen2 = 0;
			fd->compCharMapAlign2 = 0;
			fd->compGlyphCount = 0;
		}

		if (fd->shadowMapLen == 0) {
			fd->shadowMapBpe = 0;
			fd->shadowMapAlign = 1;
			fd->shadowScaleX = 32;
			fd->shadowScaleY = 32;
			fd->shadowScaleX2 = 64;
			fd->shadowScaleY2 = 64;
		}

		if (ver == 2U)
			hdrTotal = 392U;
		else if (ver == 3U)
			hdrTotal = 412U;
		else
			hdrTotal = 12U;

		fd->headerTotalSize = hdrTotal;
		fd->revision = ver;
	}

	if (pgfStreamSeek(lib, mode, stream, hdrTotal) != 0) {
		*err = SCE_FONT_ERROR_FILESEEK;
		return pgfAbortFontOpen(lib, fd, 0);
	}
	if (fd->revision <= 2U) {
		if (pgfStreamSeek(lib, mode, stream, 392U) != 0) {
			*err = SCE_FONT_ERROR_FILESEEK;
			return pgfAbortFontOpen(lib, fd, 0);
		}
	} else if (fd->revision == 3U) {
		if (pgfStreamSeek(lib, mode, stream, 412U) != 0) {
			*err = SCE_FONT_ERROR_FILESEEK;
			return pgfAbortFontOpen(lib, fd, 0);
		}
	}

	if (pgfIsMemMode(mode)) {
		PgfMemStream *ms = (PgfMemStream *)stream;
		uint8_t *base = (uint8_t *)(ms->data + ms->pos);
		fd->dimTable = (PgfTablePair *)base;
		base += 8U * fd->dimTableLen;
		fd->xAdjustTable = (PgfTablePair *)base;
		base += 8U * fd->xAdjustTableLen;
		fd->yAdjustTable = (PgfTablePair *)base;
		base += 8U * fd->yAdjustTableLen;
		fd->advanceTable = (PgfTablePair *)base;
	} else {
		fd->dimTable = (PgfTablePair *)lib->initRec.allocFunc(lib->initRec.userData, 8U * fd->dimTableLen);
		fd->xAdjustTable = (PgfTablePair *)lib->initRec.allocFunc(lib->initRec.userData, 8U * fd->xAdjustTableLen);
		fd->yAdjustTable = (PgfTablePair *)lib->initRec.allocFunc(lib->initRec.userData, 8U * fd->yAdjustTableLen);
		fd->advanceTable = (PgfTablePair *)lib->initRec.allocFunc(lib->initRec.userData, 8U * fd->advanceTableLen);
		if (fd->dimTable == NULL || fd->xAdjustTable == NULL ||
		    fd->yAdjustTable == NULL || fd->advanceTable == NULL) {
			*err = SCE_FONT_ERROR_NOMEMORY;
			return pgfAbortFontOpen(lib, fd, 0);
		}
		if (pgfStreamRead(lib, mode, stream, fd->dimTable, 8U, fd->dimTableLen, &ioErr) < fd->dimTableLen || ioErr != 0) {
			*err = SCE_FONT_ERROR_FILEREAD;
			return pgfAbortFontOpen(lib, fd, 0);
		}
		if (pgfStreamRead(lib, mode, stream, fd->xAdjustTable, 8U, fd->xAdjustTableLen, &ioErr) < fd->xAdjustTableLen || ioErr != 0) {
			*err = SCE_FONT_ERROR_FILEREAD;
			return pgfAbortFontOpen(lib, fd, 0);
		}
		if (pgfStreamRead(lib, mode, stream, fd->yAdjustTable, 8U, fd->yAdjustTableLen, &ioErr) < fd->yAdjustTableLen || ioErr != 0) {
			*err = SCE_FONT_ERROR_FILEREAD;
			return pgfAbortFontOpen(lib, fd, 0);
		}
		if (pgfStreamRead(lib, mode, stream, fd->advanceTable, 8U, fd->advanceTableLen, &ioErr) < fd->advanceTableLen || ioErr != 0) {
			*err = SCE_FONT_ERROR_FILEREAD;
			return pgfAbortFontOpen(lib, fd, 0);
		}
	}

	pgfSwapMetricTables(fd);

	metricBytes = 8U * ((uint32_t)fd->dimTableLen + fd->xAdjustTableLen +
			    fd->yAdjustTableLen + fd->advanceTableLen);
	if (pgfStreamSeek(lib, mode, stream, fd->headerTotalSize + metricBytes) != 0) {
		*err = SCE_FONT_ERROR_FILESEEK;
		return pgfAbortFontOpen(lib, fd, 0);
	}

	if (fd->revision > 1U) {
		uint32_t shadowAlign = fd->shadowMapAlign;
		shadowBytes = pgfUdiv((((fd->shadowMapLen * fd->shadowMapBpe) + 7U) >> 3) + shadowAlign - 1U, shadowAlign) * shadowAlign;
		if (fd->revision == 2U) {
			comp1Bytes = 0;
			comp2Bytes = 0;
		} else {
			uint32_t compAlignFirst = fd->compCharMapAlign1;
			uint32_t compAlignSecond = fd->compCharMapAlign2;
			comp1Bytes = pgfUdiv((2U * ((((uint32_t)fd->compCharMapLen1 * fd->compCharMapBpe1) + 7U) >> 3)) + compAlignFirst - 1U, compAlignFirst) * compAlignFirst;
			comp2Bytes = pgfUdiv((2U * ((((uint32_t)fd->compCharMapLen2 * fd->compCharMapBpe2) + 7U) >> 3)) + compAlignSecond - 1U, compAlignSecond) * compAlignSecond;
		}
	} else {
		fd->shadowMapLen = 0;
		shadowBytes = 0;
		comp1Bytes = 0;
		comp2Bytes = 0;
	}

	{
		uint32_t acm = fd->charMapAlign;
		uint32_t acp = fd->charPtrAlign;
		charMapBytes = pgfUdiv((((fd->charMapLen * fd->charMapBpe) + 7U) >> 3) + acm - 1U, acm) * acm;
		charPtrBytes = pgfUdiv((((fd->charPtrLen * fd->charPtrBpe) + 7U) >> 3) + acp - 1U, acp) * acp;
	}

	if (pgfIsMemMode(mode)) {
		PgfMemStream *ms = (PgfMemStream *)stream;
		uint8_t *base = (uint8_t *)(ms->data + ms->pos);
		fd->shadowMap = base;
		base += shadowBytes;
		fd->compCharMap1 = (PgfCompRange *)base;
		base += comp1Bytes;
		fd->compCharMap2 = (PgfCompRange *)base;
		base += comp2Bytes;
		fd->charMap = base;
		base += charMapBytes;
		fd->charPtrTable = base;
	} else {
		fd->shadowMap = lib->initRec.allocFunc(lib->initRec.userData, shadowBytes);
		if (fd->shadowMap == NULL) {
			*err = SCE_FONT_ERROR_NOMEMORY;
			return pgfAbortFontOpen(lib, fd, 1);
		}
		if ((int32_t)shadowBytes > 0) {
			if (pgfStreamRead(lib, mode, stream, fd->shadowMap, shadowBytes, 1U, &ioErr) == 0 || ioErr != 0) {
				*err = SCE_FONT_ERROR_FILEREAD;
				return pgfAbortFontOpen(lib, fd, 1);
			}
		}

		fd->compCharMap1 = (PgfCompRange *)lib->initRec.allocFunc(lib->initRec.userData, comp1Bytes);
		if (fd->compCharMap1 == NULL) {
			*err = SCE_FONT_ERROR_NOMEMORY;
			return pgfAbortFontOpen(lib, fd, 1);
		}
		if ((int32_t)comp1Bytes > 0) {
			if (pgfStreamRead(lib, mode, stream, fd->compCharMap1, comp1Bytes, 1U, &ioErr) == 0 || ioErr != 0) {
				*err = SCE_FONT_ERROR_FILEREAD;
				return pgfAbortFontOpen(lib, fd, 1);
			}
		}

		fd->compCharMap2 = (PgfCompRange *)lib->initRec.allocFunc(lib->initRec.userData, comp2Bytes);
		if (fd->compCharMap2 == NULL) {
			*err = SCE_FONT_ERROR_NOMEMORY;
			return pgfAbortFontOpen(lib, fd, 1);
		}
		if ((int32_t)comp2Bytes > 0) {
			if (pgfStreamRead(lib, mode, stream, fd->compCharMap2, comp2Bytes, 1U, &ioErr) == 0 || ioErr != 0) {
				*err = SCE_FONT_ERROR_FILEREAD;
				return pgfAbortFontOpen(lib, fd, 1);
			}
		}

		fd->charMap = (uint8_t *)lib->initRec.allocFunc(lib->initRec.userData, charMapBytes);
		if (fd->charMap == NULL) {
			*err = SCE_FONT_ERROR_NOMEMORY;
			return pgfAbortFontOpen(lib, fd, 1);
		}
		if (pgfStreamRead(lib, mode, stream, fd->charMap, charMapBytes, 1U, &ioErr) == 0 || ioErr != 0) {
			*err = SCE_FONT_ERROR_FILEREAD;
			return pgfAbortFontOpen(lib, fd, 1);
		}

		fd->charPtrTable = (uint8_t *)lib->initRec.allocFunc(lib->initRec.userData, charPtrBytes);
		if (fd->charPtrTable == NULL) {
			*err = SCE_FONT_ERROR_NOMEMORY;
			return pgfAbortFontOpen(lib, fd, 1);
		}
		if (pgfStreamRead(lib, mode, stream, fd->charPtrTable, charPtrBytes, 1U, &ioErr) == 0 || ioErr != 0) {
			*err = SCE_FONT_ERROR_FILEREAD;
			return pgfAbortFontOpen(lib, fd, 1);
		}
	}

	fd->inUse = 1;
	fd->loaded = 1;
	fd->refCount++;
	fd->charDataOffset = fd->headerTotalSize + metricBytes + shadowBytes + comp1Bytes + comp2Bytes + charMapBytes + charPtrBytes;
	lib->openDataCount++;
	return fd;
}

/* 0x810032b8 */
uint32_t pgfLookupGlyphOffset(int32_t charCode, const PgfFontData *fd)
{
	int32_t idx;
	uint32_t bpe, glyphIdx, ptrBpe, ptrVal;

	if (charCode < (int32_t)fd->firstGlyph || charCode > (int32_t)fd->lastGlyph)
		return 0;

	if (fd->revision <= 2U || fd->compCharMapLen1 == 0) {
		idx = charCode - (int32_t)fd->firstGlyph;
	} else {
		int i;
		int32_t accum = 0;
		idx = charCode;
		for (i = 0; i < (int)fd->compCharMapLen1; i++) {
			int32_t start = fd->compCharMap1[i].start;
			int32_t count = fd->compCharMap1[i].count;
			if (charCode < start)
				return 0;
			if (charCode <= start + count) {
				idx = charCode - start + accum;
				break;
			}
			accum += count;
		}
	}

	bpe = fd->charMapBpe;
	if (bpe == 0)
		return 0;
	if (bpe <= 32U)
		glyphIdx = pgfReadBits(fd->charMap, bpe * (uint32_t)idx, bpe);
	else
		glyphIdx = 0;

	if (glyphIdx == ((1U << bpe) - 1U))
		return 0;

	ptrBpe = fd->charPtrBpe;
	if (ptrBpe >= 1U && ptrBpe <= 32U)
		ptrVal = pgfReadBits(fd->charPtrTable, ptrBpe * glyphIdx, ptrBpe);
	else
		ptrVal = 0;

	return fd->charDataOffset + (uint32_t)fd->charPtrScale * ptrVal;
}

/* 0x8100344c */
int pgfLoadGlyph(PgfFontHandle *fh, uint32_t charCode, int loadBitmap)
{
	PgfFontData *fd = fh->fontData;
	PgfLib *lib = fd->lib;
	uint32_t hdrSz = (fd->revision <= 1U) ? 6U : 8U;
	uint32_t glyphOff;
	SceFont_t_cacheSystemInterface *cache = NULL;
	void *cacheEntry = NULL;
	SceFont_t_bool cacheHit = 0;
	uint8_t rawHdr[8];
	uint8_t shaBuf[20];
	SceFontCacheKey key;
	SceFont_t_error ioErr;
	uint32_t glyphSize, bmpW, bmpH, flags, direction;
	int32_t bmpLeft, bmpTop;
	uint32_t bodySize, bitOff;

	fh->currentCharCode = (uint16_t)charCode;
	glyphOff = pgfLookupGlyphOffset((int32_t)charCode, fd);
	if (glyphOff == 0 && charCode > 0x1FU) {
		fh->currentCharCode = lib->altCharCode;
		glyphOff = pgfLookupGlyphOffset((int32_t)fh->currentCharCode, fd);
	}
	if (glyphOff == 0) {
		sceClibMemset(&fh->charInfo, 0, sizeof(fh->charInfo));
		if (fh->glyphRawBuf != NULL) {
			lib->initRec.freeFunc(lib->initRec.userData, fh->glyphRawBuf);
			fh->glyphRawBuf = NULL;
		}
		if (fh->glyphBitmapBuf != NULL) {
			lib->initRec.freeFunc(lib->initRec.userData, fh->glyphBitmapBuf);
			fh->glyphBitmapBuf = NULL;
		}
		return 0;
	}

	if (fd->useExternalCache != 0 && lib->initRec.cache != NULL) {
		uint32_t hashKey;
		cache = lib->initRec.cache;
		cache->lockFunc(*cache->cacheInstance);
		key.keyValue0 = fd->fontIndex;
		key.keyValue1 = (int)fd->hSize64;
		key.keyValue2 = (int)fh->currentCharCode;
		key.keyValue3 = 0;
		sceSha1Digest(&key, sizeof(key), shaBuf);
		hashKey = ((uint32_t)shaBuf[0] << 24) | ((uint32_t)shaBuf[2] << 16) |
			  ((uint32_t)shaBuf[4] << 8) | (uint32_t)shaBuf[6];
		cacheEntry = cache->findFunc(*cache->cacheInstance, hashKey, &key, &cacheHit);
		if (!cacheHit) {
			if (cache->writeKeyValueToCacheFunc(*cache->cacheInstance, cacheEntry, &key) < 0)
				cacheEntry = NULL;
		}
	}

	if (!(cacheHit && cacheEntry != NULL)) {
		if (pgfStreamSeek(lib, fd->accessMode, fd->streamHandle, glyphOff) != 0) {
			if (cache != NULL)
				cache->unlockFunc(*cache->cacheInstance);
			return SCE_FONT_ERROR_FILESEEK;
		}
	}

	if (!(cacheHit && cacheEntry != NULL &&
	      cache->read0FromCacheFunc(*cache->cacheInstance, cacheEntry, rawHdr) >= 0)) {
		if (pgfStreamRead(lib, fd->accessMode, fd->streamHandle, rawHdr, hdrSz, 1U, &ioErr) == 0 || ioErr != 0) {
			if (cache != NULL)
				cache->unlockFunc(*cache->cacheInstance);
			return SCE_FONT_ERROR_FILEREAD;
		}
		if (cacheEntry != NULL)
			cache->write0ToCacheFunc(*cache->cacheInstance, cacheEntry, rawHdr, (SceFont_t_int)hdrSz);
	}

	/* Decode packed glyph header bits */
	glyphSize = pgfReadBits(rawHdr, 0, 14);
	bmpW = pgfReadBits(rawHdr, 14, 7);
	bmpH = pgfReadBits(rawHdr, 21, 7);
	bmpLeft = (int32_t)pgfReadBits(rawHdr, 28, 7);
	if (bmpLeft & 0x40)
		bmpLeft |= ~0x7F;
	bmpTop = (int32_t)pgfReadBits(rawHdr, 35, 7);
	if (bmpTop & 0x40)
		bmpTop |= ~0x7F;
	flags = rawHdr[5];
	direction = (flags >> 2) & 3U;

	fh->charInfo.bitmapWidth = bmpW;
	fh->charInfo.bitmapHeight = bmpH;
	fh->charInfo.bitmapLeft = bmpLeft;
	fh->charInfo.bitmapTop = bmpTop;
	if (fd->revision <= 1U) {
		fh->charInfo.reserved0[0] = 0;
		fh->charInfo.reserved0[1] = 0;
		fh->charInfo.reserved1 = 0;
	} else {
		uint8_t shadowBitsLow = rawHdr[6] & 3U;
		uint8_t shadowBitsMid = (rawHdr[6] >> 2) & 3U;
		uint8_t shadowBitsHigh = (rawHdr[6] >> 4) & 7U;
		uint8_t shadowId = (uint8_t)((rawHdr[6] >> 7) | ((rawHdr[7] & 0x7FU) << 1));
		fh->charInfo.reserved0[0] = (uint8_t)((shadowBitsLow << 5) | (shadowBitsMid << 3) | shadowBitsHigh);
		fh->charInfo.reserved0[1] = 0;
		fh->charInfo.reserved1 = shadowId;
	}

	if (fh->glyphRawBuf != NULL) {
		lib->initRec.freeFunc(lib->initRec.userData, fh->glyphRawBuf);
		fh->glyphRawBuf = NULL;
	}
	if (fh->glyphBitmapBuf != NULL) {
		lib->initRec.freeFunc(lib->initRec.userData, fh->glyphBitmapBuf);
		fh->glyphBitmapBuf = NULL;
	}

	bodySize = glyphSize - hdrSz;
	fh->glyphRawBuf = (uint8_t *)lib->initRec.allocFunc(lib->initRec.userData, bodySize);
	if (fh->glyphRawBuf == NULL) {
		if (cache != NULL)
			cache->unlockFunc(*cache->cacheInstance);
		return SCE_FONT_ERROR_NOMEMORY;
	}
	fh->glyphBitmapBuf = (uint8_t *)lib->initRec.allocFunc(lib->initRec.userData, bmpW * bmpH);
	if (fh->glyphBitmapBuf == NULL) {
		if (cache != NULL)
			cache->unlockFunc(*cache->cacheInstance);
		return SCE_FONT_ERROR_NOMEMORY;
	}

	if (!(cacheHit && cacheEntry != NULL &&
	      cache->read1FromCacheFunc(*cache->cacheInstance, cacheEntry, fh->glyphRawBuf) >= 0)) {
		if (pgfStreamRead(lib, fd->accessMode, fd->streamHandle, fh->glyphRawBuf, bodySize, 1U, &ioErr) == 0 || ioErr != 0) {
			if (cache != NULL)
				cache->unlockFunc(*cache->cacheInstance);
			return SCE_FONT_ERROR_FILEREAD;
		}
		if (cacheEntry != NULL)
			cache->write1ToCacheFunc(*cache->cacheInstance, cacheEntry, fh->glyphRawBuf, (SceFont_t_int)bodySize);
	}

	/* Unpack metrics from glyphRawBuf */
	bitOff = 0;
	if ((flags & 0x10U) != 0) {
		uint32_t idx = fh->glyphRawBuf[0];
		fh->charInfo.glyphMetrics.width64 = (uint32_t)fd->dimTable[idx].x;
		fh->charInfo.glyphMetrics.height64 = (uint32_t)fd->dimTable[idx].y;
		bitOff = 8;
	} else {
		fh->charInfo.glyphMetrics.width64 = pgfReadBits(fh->glyphRawBuf, 0, 32);
		fh->charInfo.glyphMetrics.height64 = pgfReadBits(fh->glyphRawBuf, 32, 32);
		bitOff = 64;
	}

	if ((flags & 0x20U) != 0) {
		uint32_t idx = fh->glyphRawBuf[bitOff >> 3];
		fh->charInfo.glyphMetrics.horizontalBearingX64 = fd->xAdjustTable[idx].x;
		fh->charInfo.glyphMetrics.verticalBearingX64 = fd->xAdjustTable[idx].y;
		bitOff += 8;
	} else {
		fh->charInfo.glyphMetrics.horizontalBearingX64 = (int32_t)pgfReadBits(fh->glyphRawBuf, bitOff, 32);
		fh->charInfo.glyphMetrics.verticalBearingX64 = (int32_t)pgfReadBits(fh->glyphRawBuf, bitOff + 32, 32);
		bitOff += 64;
	}

	if ((flags & 0x40U) != 0) {
		uint32_t idx = fh->glyphRawBuf[bitOff >> 3];
		fh->charInfo.glyphMetrics.horizontalBearingY64 = fd->yAdjustTable[idx].x;
		fh->charInfo.glyphMetrics.verticalBearingY64 = fd->yAdjustTable[idx].y;
		bitOff += 8;
	} else {
		fh->charInfo.glyphMetrics.horizontalBearingY64 = (int32_t)pgfReadBits(fh->glyphRawBuf, bitOff, 32);
		fh->charInfo.glyphMetrics.verticalBearingY64 = (int32_t)pgfReadBits(fh->glyphRawBuf, bitOff + 32, 32);
		bitOff += 64;
	}

	if ((flags & 0x80U) != 0) {
		uint32_t idx = fh->glyphRawBuf[bitOff >> 3];
		fh->charInfo.glyphMetrics.horizontalAdvance64 = fd->advanceTable[idx].x;
		fh->charInfo.glyphMetrics.verticalAdvance64 = fd->advanceTable[idx].y;
		bitOff += 8;
	} else {
		fh->charInfo.glyphMetrics.horizontalAdvance64 = (int32_t)pgfReadBits(fh->glyphRawBuf, bitOff, 32);
		fh->charInfo.glyphMetrics.verticalAdvance64 = (int32_t)pgfReadBits(fh->glyphRawBuf, bitOff + 32, 32);
		bitOff += 64;
	}

	if (direction == 3U) {
		int i;
		for (i = 0; i < (int)fd->compGlyphCount; i++) {
			fd->compGlyphIds[i] = (uint16_t)pgfReadBits(fh->glyphRawBuf, bitOff, 16);
			bitOff += 16;
		}
		fh->charInfo.glyphMetrics.ascender64 = fh->charInfo.glyphMetrics.horizontalBearingY64;
		fh->charInfo.glyphMetrics.descender64 = fh->charInfo.glyphMetrics.horizontalBearingY64 -
							(int32_t)fh->charInfo.glyphMetrics.height64;
		if (cache != NULL)
			cache->unlockFunc(*cache->cacheInstance);
		fd->isCompositeGlyph = 1;
		if (cacheEntry != NULL && !cacheHit) {
			cache->write2ToCacheFunc(*cache->cacheInstance, cacheEntry, g_pgfCacheEmptyComp, 0);
			cache->write3ToCacheFunc(*cache->cacheInstance, cacheEntry, g_pgfCacheEmptyComp, 0);
		}
		return 0;
	}

	fh->charInfo.glyphMetrics.ascender64 = fh->charInfo.glyphMetrics.horizontalBearingY64;
	fh->charInfo.glyphMetrics.descender64 = fh->charInfo.glyphMetrics.horizontalBearingY64 -
						(int32_t)fh->charInfo.glyphMetrics.height64;
	if (loadBitmap == 2 && fh->charInfo.bitmapWidth != 0 &&
	    fh->charInfo.bitmapHeight != 0 && fh->glyphBitmapBuf != NULL) {
		pgfDecodeGlyphBitmap(fh->glyphRawBuf, fh->glyphBitmapBuf,
				     (int)fh->charInfo.bitmapWidth,
				     (int)fh->charInfo.bitmapHeight,
				     (int)direction, bitOff);
	}
	if (cacheEntry != NULL && !cacheHit) {
		cache->write2ToCacheFunc(*cache->cacheInstance, cacheEntry, g_pgfCacheEmptyNormal, 0);
		cache->write3ToCacheFunc(*cache->cacheInstance, cacheEntry, g_pgfCacheEmptyNormal, 0);
	}
	if (cache != NULL)
		cache->unlockFunc(*cache->cacheInstance);
	return 0;
}

/* 0x81003f00 */
SceFont_t_libId sceFontNewLib(SceFont_t_initRec *initRec, SceFont_t_error *errorCode)
{
	PgfLib *lib;
	uint32_t i;

	if (g_pgfFontDir[0] == '\0') {
		sceClibStrncpy(g_pgfFontDir, PGF_FONT_DIR_DEFAULT, 1023U);
		g_pgfFontDir[1023] = '\0';
	}
	if (errorCode == NULL)
		return NULL;
	*errorCode = 0;
	if (initRec == NULL || initRec->allocFunc == NULL || initRec->freeFunc == NULL) {
		*errorCode = SCE_FONT_ERROR_ARG;
		return NULL;
	}

	lib = (PgfLib *)initRec->allocFunc(initRec->userData, sizeof(PgfLib));
	if (lib == NULL) {
		*errorCode = SCE_FONT_ERROR_NOMEMORY;
		return NULL;
	}
	if (initRec->maxNumFonts == 0 || initRec->maxNumFonts > SCE_FONT_MAX_OPEN) {
		initRec->freeFunc(initRec->userData, lib);
		*errorCode = SCE_FONT_ERROR_ARG;
		return NULL;
	}

	lib->initRec = *initRec;
	lib->hResolution = 128.0f;
	lib->vResolution = 128.0f;
	lib->altCharCode = 95;

	lib->fontHandles = (PgfFontHandle *)lib->initRec.allocFunc(lib->initRec.userData,
								   sizeof(PgfFontHandle) * lib->initRec.maxNumFonts);
	if (lib->fontHandles == NULL) {
		lib->initRec.freeFunc(initRec->userData, lib);
		*errorCode = SCE_FONT_ERROR_NOMEMORY;
		return NULL;
	}

	lib->fontDataList = (PgfFontData *)lib->initRec.allocFunc(lib->initRec.userData,
								  sizeof(PgfFontData) * lib->initRec.maxNumFonts);
	if (lib->fontDataList == NULL) {
		lib->initRec.freeFunc(initRec->userData, lib->fontHandles);
		lib->initRec.freeFunc(initRec->userData, lib);
		*errorCode = SCE_FONT_ERROR_NOMEMORY;
		return NULL;
	}

	lib->openHandleCount = 0;
	lib->openDataCount = 0;
	lib->numSystemFonts = PGF_NUM_SYSTEM_FONTS;
	lib->extraAlloc = NULL;
	*errorCode = 0;

	for (i = 0; i < lib->initRec.maxNumFonts; i++) {
		lib->fontHandles[i].inUse = 0;
		lib->fontHandles[i].fontData = NULL;
		lib->fontDataList[i].inUse = 0;
		lib->fontDataList[i].refCount = 0;
		lib->fontDataList[i].fontIndex = 0;
		lib->fontDataList[i].dimTable = NULL;
		lib->fontDataList[i].xAdjustTable = NULL;
		lib->fontDataList[i].yAdjustTable = NULL;
		lib->fontDataList[i].advanceTable = NULL;
		lib->fontDataList[i].shadowMap = NULL;
		lib->fontDataList[i].charMap = NULL;
		lib->fontDataList[i].charPtrTable = NULL;
		lib->fontDataList[i].compCharMap1 = NULL;
		lib->fontDataList[i].compCharMap2 = NULL;
	}
	return lib;
}

/* 0x81004098 */
SceFont_t_error sceFontDoneLib(SceFont_t_libId libId)
{
	PgfLib *lib = (PgfLib *)libId;
	int32_t i;

	if (lib == NULL)
		return SCE_FONT_ERROR_LIBID;

	for (i = 0; i < (int32_t)lib->initRec.maxNumFonts; i++) {
		PgfFontHandle *fh = &lib->fontHandles[i];
		if (fh->inUse != 0) {
			PgfFontData *fd = fh->fontData;
			fh->inUse = 0;
			fd->lib->openHandleCount--;
			pgfCloseFontData(fd);
			if (fh->glyphRawBuf != NULL) {
				fd->lib->initRec.freeFunc(fd->lib->initRec.userData, fh->glyphRawBuf);
				fh->glyphRawBuf = NULL;
			}
			if (fh->glyphBitmapBuf != NULL) {
				fd->lib->initRec.freeFunc(fd->lib->initRec.userData, fh->glyphBitmapBuf);
				fh->glyphBitmapBuf = NULL;
			}
		}
	}

	if (lib->extraAlloc != NULL)
		lib->initRec.freeFunc(lib->initRec.userData, lib->extraAlloc);
	lib->initRec.freeFunc(lib->initRec.userData, lib->fontHandles);
	lib->initRec.freeFunc(lib->initRec.userData, lib->fontDataList);
	lib->initRec.freeFunc(lib->initRec.userData, lib);
	return 0;
}

/* 0x81004138 */
SceFont_t_error sceFontSetResolution(SceFont_t_libId libId, SceFont_t_f32 hRes, SceFont_t_f32 vRes)
{
	PgfLib *lib = (PgfLib *)libId;

	if (lib == NULL)
		return SCE_FONT_ERROR_LIBID;
	if (hRes <= 0.0f || vRes <= 0.0f)
		return SCE_FONT_ERROR_ARG;
	lib->hResolution = hRes;
	lib->vResolution = vRes;
	return 0;
}

/* 0x8100416c */
SceFont_t_int sceFontGetNumFontList(SceFont_t_libId libId, SceFont_t_error *errorCode)
{
	PgfLib *lib = (PgfLib *)libId;

	if (errorCode == NULL)
		return 0;
	*errorCode = 0;
	if (lib == NULL) {
		*errorCode = SCE_FONT_ERROR_LIBID;
		return 0;
	}
	if (lib->numSystemFonts < 0)
		lib->numSystemFonts = PGF_NUM_SYSTEM_FONTS;
	return lib->numSystemFonts;
}

/* 0x81004194 */
SceFont_t_error sceFontGetFontList(SceFont_t_libId libId, SceFont_t_fontStyleInfo *fontStyleInfo,
				   SceFont_t_int numFonts)
{
	PgfLib *lib = (PgfLib *)libId;
	int32_t n, i;

	if (lib == NULL)
		return SCE_FONT_ERROR_LIBID;
	if (fontStyleInfo == NULL)
		return SCE_FONT_ERROR_ARG;
	n = lib->numSystemFonts;
	if (n < 0) {
		n = PGF_NUM_SYSTEM_FONTS;
		lib->numSystemFonts = PGF_NUM_SYSTEM_FONTS;
	}
	if (numFonts > n)
		numFonts = n;
	for (i = 0; i < numFonts; i++)
		sceClibMemcpy_safe(&fontStyleInfo[i], &g_pgfSystemFonts[i], sizeof(SceFont_t_fontStyleInfo));
	return 0;
}

/* 0x810041e8 */
SceFont_t_fontIndex sceFontFindOptimumFont(SceFont_t_libId libId,
					   SceFont_t_fontStyleInfo *fontStyleInfo,
					   SceFont_t_error *errorCode)
{
	PgfLib *lib = (PgfLib *)libId;
	int32_t numFonts, i, bestIdx = -1;
	int mask;
	float hTarget = 0.0f, vTarget = 0.0f;
	float bestDiff = 4294967296.0f;

	if (errorCode == NULL)
		return 0;
	*errorCode = 0;
	if (lib == NULL) {
		*errorCode = SCE_FONT_ERROR_LIBID;
		return 0;
	}
	if (fontStyleInfo == NULL) {
		*errorCode = SCE_FONT_ERROR_ARG;
		return 0;
	}
	numFonts = lib->numSystemFonts;
	if (numFonts < 0) {
		numFonts = PGF_NUM_SYSTEM_FONTS;
		lib->numSystemFonts = PGF_NUM_SYSTEM_FONTS;
		if (*errorCode != 0)
			return 0;
	}

	mask = pgfStyleMask(fontStyleInfo);
	if ((mask & 0x0001) != 0) {
		float hRes = (mask & 0x0004) ? fontStyleInfo->hResolution : lib->hResolution;
		hTarget = hRes * fontStyleInfo->hSize;
	} else {
		mask &= ~0x0004;
	}
	if ((mask & 0x0002) != 0) {
		float vRes = (mask & 0x0008) ? fontStyleInfo->hResolution : lib->vResolution;
		vTarget = vRes * fontStyleInfo->hSize;
	} else {
		mask &= ~0x0008;
		if (mask == 0)
			return 0;
	}

	for (i = 0; i < numFonts; i++) {
		const SceFont_t_fontStyleInfo *s = &g_pgfSystemFonts[i];
		if ((mask & 0x0010) && !pgfFloatApproxEq(fontStyleInfo->weight, s->weight))
			continue;
		if ((mask & 0x0020) && fontStyleInfo->familyCode != s->familyCode)
			continue;
		if ((mask & 0x0040) && fontStyleInfo->style != s->style)
			continue;
		if ((mask & 0x0080) && fontStyleInfo->subStyle != s->subStyle)
			continue;
		if ((mask & 0x0100) && fontStyleInfo->languageCode != s->languageCode)
			continue;
		if ((mask & 0x0200) && fontStyleInfo->regionCode != s->regionCode)
			continue;
		if ((mask & 0x0400) && fontStyleInfo->countryCode != s->countryCode)
			continue;
		if ((mask & 0x0800) &&
		    sceClibStrncmp((const char *)fontStyleInfo->fontName, (const char *)s->fontName, 64) != 0)
			continue;
		if ((mask & 0x1000) &&
		    sceClibStrncmp((const char *)fontStyleInfo->fileName, (const char *)s->fileName, 64) != 0)
			continue;

		if ((mask & 0x0003) == 0) {
			bestIdx = i;
			continue;
		}
		if ((mask & 0x0001) != 0) {
			float sh = s->hSize * s->hResolution;
			float diff;
			if (pgfFloatApproxEq(hTarget, sh))
				return i;
			diff = hTarget - sh;
			if (diff < 0.0f)
				diff = -diff;
			if (diff < bestDiff) {
				bestDiff = diff;
				bestIdx = i;
			}
		}
		if ((mask & 0x0002) != 0) {
			float sv = s->vSize * s->vResolution;
			float diff;
			if (pgfFloatApproxEq(vTarget, sv))
				return i;
			diff = vTarget - sv;
			if (diff < 0.0f)
				diff = -diff;
			if (diff < bestDiff) {
				bestDiff = diff;
				bestIdx = i;
			}
		}
	}

	if (bestIdx != -1)
		return bestIdx;

	if ((mask & 0x0100) != 0 && (mask & ~0x0100) != 0 && fontStyleInfo->languageCode != 0) {
		SceFont_t_fontStyleInfo fallback;
		SceFont_t_fontIndex idx;
		sceClibMemset(&fallback, 0, sizeof(fallback));
		fallback.languageCode = fontStyleInfo->languageCode;
		idx = sceFontFindOptimumFont(libId, &fallback, errorCode);
		return (idx < 0) ? 0 : idx;
	}
	return 0;
}

/* 0x8100458c */
SceFont_t_fontIndex sceFontFindFont(SceFont_t_libId libId,
				    SceFont_t_fontStyleInfo *fontStyleInfo,
				    SceFont_t_error *errorCode)
{
	PgfLib *lib = (PgfLib *)libId;
	int32_t numFonts, i;
	int mask;
	float hTarget = 0.0f, vTarget = 0.0f;

	if (errorCode == NULL)
		return 0;
	*errorCode = 0;
	if (lib == NULL) {
		*errorCode = SCE_FONT_ERROR_LIBID;
		return 0;
	}
	if (fontStyleInfo == NULL) {
		*errorCode = SCE_FONT_ERROR_ARG;
		return 0;
	}
	numFonts = lib->numSystemFonts;
	if (numFonts < 0) {
		numFonts = PGF_NUM_SYSTEM_FONTS;
		lib->numSystemFonts = PGF_NUM_SYSTEM_FONTS;
		if (*errorCode != 0)
			return 0;
	}

	mask = pgfStyleMask(fontStyleInfo);
	if ((mask & 0x0001) != 0) {
		float hRes = (mask & 0x0004) ? fontStyleInfo->hResolution : lib->hResolution;
		hTarget = hRes * fontStyleInfo->hSize;
	} else {
		mask &= ~0x0004;
	}
	if ((mask & 0x0002) != 0) {
		float vRes = (mask & 0x0008) ? fontStyleInfo->hResolution : lib->vResolution;
		vTarget = vRes * fontStyleInfo->hSize;
	} else {
		mask &= ~0x0008;
		if (mask == 0)
			return 0;
	}

	for (i = 0; i < numFonts; i++) {
		const SceFont_t_fontStyleInfo *s = &g_pgfSystemFonts[i];
		if ((mask & 0x0010) && !pgfFloatApproxEq(fontStyleInfo->weight, s->weight))
			continue;
		if ((mask & 0x0020) && fontStyleInfo->familyCode != s->familyCode)
			continue;
		if ((mask & 0x0040) && fontStyleInfo->style != s->style)
			continue;
		if ((mask & 0x0080) && fontStyleInfo->subStyle != s->subStyle)
			continue;
		if ((mask & 0x0100) && fontStyleInfo->languageCode != s->languageCode)
			continue;
		if ((mask & 0x0200) && fontStyleInfo->regionCode != s->regionCode)
			continue;
		if ((mask & 0x0400) && fontStyleInfo->countryCode != s->countryCode)
			continue;
		if ((mask & 0x0800) &&
		    sceClibStrncmp((const char *)fontStyleInfo->fontName, (const char *)s->fontName, 64) != 0)
			continue;
		if ((mask & 0x1000) &&
		    sceClibStrncmp((const char *)fontStyleInfo->fileName, (const char *)s->fileName, 64) != 0)
			continue;
		if ((mask & 0x0001) && !pgfFloatApproxEq(hTarget, s->hSize * s->hResolution))
			continue;
		if ((mask & 0x0002) && !pgfFloatApproxEq(vTarget, s->vSize * s->vResolution))
			continue;
		return i;
	}
	return -1;
}

/* 0x81004804 */
SceFont_t_error sceFontGetFontInfoByIndexNumber(SceFont_t_libId libId,
						SceFont_t_fontStyleInfo *fontStyleInfo,
						SceFont_t_fontIndex fontIndex)
{
	PgfLib *lib = (PgfLib *)libId;
	int32_t n;

	if (lib == NULL)
		return SCE_FONT_ERROR_LIBID;
	if (fontStyleInfo == NULL || fontIndex < 0)
		return SCE_FONT_ERROR_ARG;
	n = lib->numSystemFonts;
	if (n < 0) {
		n = PGF_NUM_SYSTEM_FONTS;
		lib->numSystemFonts = PGF_NUM_SYSTEM_FONTS;
	}
	if (fontIndex >= n)
		return SCE_FONT_ERROR_ARG;
	sceClibMemcpy_safe(fontStyleInfo, &g_pgfSystemFonts[fontIndex], sizeof(SceFont_t_fontStyleInfo));
	return 0;
}

/* 0x81004848 */
SceFont_t_fontId sceFontOpen(SceFont_t_libId libId, SceFont_t_fontIndex fontIndex,
			     SceFont_t_u32 mode, SceFont_t_error *errorCode)
{
	PgfLib *lib = (PgfLib *)libId;
	uint32_t openMode = (mode == SCE_FONT_MEMORYBASEDSTREAM) ? PGF_MODE_SYS_MEM : PGF_MODE_SYS_FILE;
	uint16_t prevCount;
	int32_t i;
	PgfFontHandle *fh = NULL;

	if (errorCode == NULL)
		return NULL;
	*errorCode = 0;
	if (fontIndex < 0) {
		*errorCode = SCE_FONT_ERROR_ARG;
		return NULL;
	}
	if (lib == NULL) {
		*errorCode = SCE_FONT_ERROR_LIBID;
		return NULL;
	}
	if (lib->numSystemFonts < 0)
		lib->numSystemFonts = PGF_NUM_SYSTEM_FONTS;
	if (fontIndex >= lib->numSystemFonts) {
		*errorCode = SCE_FONT_ERROR_ARG;
		return NULL;
	}

	prevCount = lib->openHandleCount;
	if ((int32_t)lib->initRec.maxNumFonts <= (int32_t)prevCount) {
		*errorCode = SCE_FONT_ERROR_TOOMANYOPENED;
		return NULL;
	}
	for (i = 0; i < (int32_t)lib->initRec.maxNumFonts; i++) {
		if (lib->fontHandles[i].inUse == 0) {
			fh = &lib->fontHandles[i];
			lib->openHandleCount = (uint16_t)(prevCount + 1U);
			break;
		}
	}

	fh->glyphRawBuf = NULL;
	fh->glyphBitmapBuf = NULL;
	fh->fontData = pgfOpenFontData(lib, fontIndex, NULL, 0, 0, openMode, errorCode);
	if (*errorCode == 0 && fh->fontData != NULL) {
		fh->inUse = 1;
		return fh;
	}
	if (*errorCode == SCE_FONT_ERROR_FILEOPEN) {
		if (lib->openHandleCount > prevCount)
			lib->openHandleCount--;
	} else {
		pgfCloseFontData(fh->fontData);
	}
	if (fh->glyphRawBuf != NULL) {
		fh->fontData->lib->initRec.freeFunc(fh->fontData->lib->initRec.userData, fh->glyphRawBuf);
		fh->glyphRawBuf = NULL;
	}
	if (fh->glyphBitmapBuf != NULL) {
		fh->fontData->lib->initRec.freeFunc(fh->fontData->lib->initRec.userData, fh->glyphBitmapBuf);
		fh->glyphBitmapBuf = NULL;
	}
	return NULL;
}

/* 0x8100498c */
SceFont_t_fontId sceFontOpenUserFile(SceFont_t_libId libId, SceFont_t_pointer fileName,
				     SceFont_t_u32 mode, SceFont_t_error *errorCode)
{
	PgfLib *lib = (PgfLib *)libId;
	uint32_t openMode;
	uint16_t prevCount;
	int32_t i;
	PgfFontHandle *fh = NULL;

	if (errorCode == NULL)
		return NULL;
	if (lib == NULL) {
		*errorCode = SCE_FONT_ERROR_LIBID;
		return NULL;
	}
	if (mode == SCE_FONT_MEMORYBASEDSTREAM) {
		openMode = PGF_MODE_USER_MEMFILE;
	} else {
		if (lib->initRec.openFunc == NULL || lib->initRec.closeFunc == NULL ||
		    lib->initRec.readFunc == NULL || lib->initRec.seekFunc == NULL) {
			*errorCode = SCE_FONT_ERROR_ARG;
			return NULL;
		}
		openMode = PGF_MODE_USER_CALLBACK;
	}
	if (fileName == NULL) {
		*errorCode = SCE_FONT_ERROR_ARG;
		return NULL;
	}

	*errorCode = 0;
	prevCount = lib->openHandleCount;
	if ((int32_t)lib->initRec.maxNumFonts <= (int32_t)prevCount) {
		*errorCode = SCE_FONT_ERROR_TOOMANYOPENED;
		return NULL;
	}
	for (i = 0; i < (int32_t)lib->initRec.maxNumFonts; i++) {
		if (lib->fontHandles[i].inUse == 0) {
			fh = &lib->fontHandles[i];
			lib->openHandleCount = (uint16_t)(prevCount + 1U);
			break;
		}
	}

	fh->glyphRawBuf = NULL;
	fh->glyphBitmapBuf = NULL;
	fh->fontData = pgfOpenFontData(lib, 0, (const char *)fileName, 0, 0, openMode, errorCode);
	if (*errorCode == 0 && fh->fontData != NULL) {
		fh->inUse = 1;
		return fh;
	}
	if (*errorCode == SCE_FONT_ERROR_FILEOPEN) {
		if (lib->openHandleCount > prevCount)
			lib->openHandleCount--;
	} else {
		pgfCloseFontData(fh->fontData);
	}
	if (fh->glyphRawBuf != NULL) {
		fh->fontData->lib->initRec.freeFunc(fh->fontData->lib->initRec.userData, fh->glyphRawBuf);
		fh->glyphRawBuf = NULL;
	}
	if (fh->glyphBitmapBuf != NULL) {
		fh->fontData->lib->initRec.freeFunc(fh->fontData->lib->initRec.userData, fh->glyphBitmapBuf);
		fh->glyphBitmapBuf = NULL;
	}
	return NULL;
}

/* 0x81004ab4 */
SceFont_t_fontId sceFontOpenUserMemory(SceFont_t_libId libId, SceFont_t_pointer addr,
				       SceFont_t_u32 size, SceFont_t_error *errorCode)
{
	PgfLib *lib = (PgfLib *)libId;
	uint16_t prevCount;
	int32_t i;
	PgfFontHandle *fh = NULL;

	if (errorCode == NULL)
		return NULL;
	if (lib == NULL) {
		*errorCode = SCE_FONT_ERROR_LIBID;
		return NULL;
	}
	if (addr == NULL || size == 0) {
		*errorCode = SCE_FONT_ERROR_ARG;
		return NULL;
	}

	*errorCode = 0;
	prevCount = lib->openHandleCount;
	if ((int32_t)lib->initRec.maxNumFonts <= (int32_t)prevCount) {
		*errorCode = SCE_FONT_ERROR_TOOMANYOPENED;
		return NULL;
	}
	for (i = 0; i < (int32_t)lib->initRec.maxNumFonts; i++) {
		if (lib->fontHandles[i].inUse == 0) {
			fh = &lib->fontHandles[i];
			lib->openHandleCount = (uint16_t)(prevCount + 1U);
			break;
		}
	}

	fh->glyphRawBuf = NULL;
	fh->glyphBitmapBuf = NULL;
	fh->fontData = pgfOpenFontData(lib, 0, NULL, (uintptr_t)addr, size, PGF_MODE_USER_MEMORY, errorCode);
	if (*errorCode == 0 && fh->fontData != NULL) {
		fh->inUse = 1;
		return fh;
	}
	if (*errorCode == SCE_FONT_ERROR_FILEOPEN) {
		if (lib->openHandleCount > prevCount)
			lib->openHandleCount--;
	} else {
		pgfCloseFontData(fh->fontData);
	}
	if (fh->glyphRawBuf != NULL) {
		fh->fontData->lib->initRec.freeFunc(fh->fontData->lib->initRec.userData, fh->glyphRawBuf);
		fh->glyphRawBuf = NULL;
	}
	if (fh->glyphBitmapBuf != NULL) {
		fh->fontData->lib->initRec.freeFunc(fh->fontData->lib->initRec.userData, fh->glyphBitmapBuf);
		fh->glyphBitmapBuf = NULL;
	}
	return NULL;
}

/* 0x81004bb8 */
SceFont_t_error sceFontClose(SceFont_t_fontId fontId)
{
	PgfFontHandle *fh = (PgfFontHandle *)fontId;
	PgfFontData *fd;

	if (fh == NULL)
		return SCE_FONT_ERROR_ARG;
	if (fh->inUse == 0)
		return 0;

	fd = fh->fontData;
	fh->inUse = 0;
	fd->lib->openHandleCount--;
	pgfCloseFontData(fd);
	if (fh->glyphRawBuf != NULL) {
		fh->fontData->lib->initRec.freeFunc(fh->fontData->lib->initRec.userData, fh->glyphRawBuf);
		fh->glyphRawBuf = NULL;
	}
	if (fh->glyphBitmapBuf != NULL) {
		fh->fontData->lib->initRec.freeFunc(fh->fontData->lib->initRec.userData, fh->glyphBitmapBuf);
		fh->glyphBitmapBuf = NULL;
	}
	return 0;
}

/* 0x81004d40 */
SceFont_t_error sceFontGetFontInfo(SceFont_t_fontId fontId, SceFont_t_fontInfo *fontInfo)
{
	PgfFontHandle *fh = (PgfFontHandle *)fontId;
	PgfFontData *fd;

	if (fh == NULL || fontInfo == NULL)
		return SCE_FONT_ERROR_ARG;
	fd = fh->fontData;
	if (fd == NULL)
		return SCE_FONT_ERROR_DATAINCONSISTENT;

	fontInfo->maxIGlyphMetrics.width64 = fd->maxGlyphWidth64;
	fontInfo->maxIGlyphMetrics.height64 = fd->maxGlyphHeight64;
	fontInfo->maxIGlyphMetrics.ascender64 = fd->maxGlyphAscender64;
	fontInfo->maxIGlyphMetrics.descender64 = fd->maxGlyphDescender64;
	fontInfo->maxIGlyphMetrics.horizontalBearingX64 = fd->maxGlyphLeftX64;
	fontInfo->maxIGlyphMetrics.horizontalBearingY64 = fd->maxGlyphBaseY64;
	fontInfo->maxIGlyphMetrics.verticalBearingX64 = fd->minGlyphCenterX64;
	fontInfo->maxIGlyphMetrics.verticalBearingY64 = fd->maxGlyphTopY64;
	fontInfo->maxIGlyphMetrics.horizontalAdvance64 = fd->maxGlyphAdvanceX64;
	fontInfo->maxIGlyphMetrics.verticalAdvance64 = fd->maxGlyphAdvanceY64;

	fontInfo->maxFGlyphMetrics.width = (float)fd->maxGlyphWidth64 * 0.015625f;
	fontInfo->maxFGlyphMetrics.height = (float)fd->maxGlyphHeight64 * 0.015625f;
	fontInfo->maxFGlyphMetrics.ascender = (float)fd->maxGlyphAscender64 * 0.015625f;
	fontInfo->maxFGlyphMetrics.descender = (float)fd->maxGlyphDescender64 * 0.015625f;
	fontInfo->maxFGlyphMetrics.horizontalBearingX = (float)fd->maxGlyphLeftX64 * 0.015625f;
	fontInfo->maxFGlyphMetrics.horizontalBearingY = (float)fd->maxGlyphBaseY64 * 0.015625f;
	fontInfo->maxFGlyphMetrics.verticalBearingX = (float)fd->minGlyphCenterX64 * 0.015625f;
	fontInfo->maxFGlyphMetrics.verticalBearingY = (float)fd->maxGlyphTopY64 * 0.015625f;
	fontInfo->maxFGlyphMetrics.horizontalAdvance = (float)fd->maxGlyphAdvanceX64 * 0.015625f;
	fontInfo->maxFGlyphMetrics.verticalAdvance = (float)fd->maxGlyphAdvanceY64 * 0.015625f;

	fontInfo->maxGlyphBitmapWidth = fd->maxGlyphBitmapWidth;
	fontInfo->maxGlyphBitmapHeight = fd->maxGlyphBitmapHeight;
	fontInfo->numChars = fd->charPtrLen;
	fontInfo->numSubChars = 0;

	fontInfo->fontStyleInfo.hSize = (float)fd->hSize64 * 0.015625f;
	fontInfo->fontStyleInfo.vSize = (float)fd->vSize64 * 0.015625f;
	fontInfo->fontStyleInfo.hResolution = (float)fd->hRes64 * 0.015625f;
	fontInfo->fontStyleInfo.vResolution = (float)fd->vRes64 * 0.015625f;
	fontInfo->pixelDepth = fd->bpp;

	switch (fd->accessMode) {
	case PGF_MODE_SYS_FILE:
	case PGF_MODE_SYS_MEM: {
		const SceFont_t_fontStyleInfo *s;
		if (fd->fontIndex >= fd->lib->numSystemFonts)
			return SCE_FONT_ERROR_ARG;
		s = &g_pgfSystemFonts[fd->fontIndex];
		fontInfo->fontStyleInfo.weight = s->weight;
		fontInfo->fontStyleInfo.familyCode = s->familyCode;
		fontInfo->fontStyleInfo.style = s->style;
		fontInfo->fontStyleInfo.subStyle = s->subStyle;
		fontInfo->fontStyleInfo.languageCode = s->languageCode;
		fontInfo->fontStyleInfo.regionCode = s->regionCode;
		fontInfo->fontStyleInfo.countryCode = s->countryCode;
		sceClibMemcpy_safe(fontInfo->fontStyleInfo.fontName, s->fontName, SCE_FONT_FONTNAME_LENGTH);
		sceClibMemcpy_safe(fontInfo->fontStyleInfo.fileName, s->fileName, SCE_FONT_FONTFILENAME_LENGTH);
		fontInfo->fontStyleInfo.extraAttributes = s->extraAttributes;
		fontInfo->fontStyleInfo.expireDate = s->expireDate;
		break;
	}
	case PGF_MODE_USER_CALLBACK:
	case PGF_MODE_USER_MEMFILE:
	case PGF_MODE_USER_MEMORY:
		fontInfo->fontStyleInfo.weight = 0.0f;
		fontInfo->fontStyleInfo.familyCode = 0;
		fontInfo->fontStyleInfo.style = 0;
		fontInfo->fontStyleInfo.subStyle = 0;
		fontInfo->fontStyleInfo.languageCode = 0;
		fontInfo->fontStyleInfo.regionCode = 0;
		fontInfo->fontStyleInfo.countryCode = 0;
		fontInfo->fontStyleInfo.fontName[0] = 0;
		fontInfo->fontStyleInfo.fileName[0] = 0;
		fontInfo->fontStyleInfo.extraAttributes = 0;
		fontInfo->fontStyleInfo.expireDate = 0;
		break;
	default:
		break;
	}
	return 0;
}

static inline uint32_t pgfCheckCompRange2(const PgfFontData *fd, uint32_t charCode)
{
	int i;

	for (i = 0; i < (int)fd->compCharMapLen2; i++) {
		uint32_t start = fd->compCharMap2[i].start;
		if (charCode >= start && (int32_t)charCode < (int32_t)(start + fd->compCharMap2[i].count))
			return fd->lib->altCharCode;
	}
	return charCode;
}

/* 0x81004f74 */
SceFont_t_error sceFontGetCharInfo(SceFont_t_fontId fontId, SceFont_t_charCode charCode,
				   SceFont_t_charInfo *charInfo)
{
	PgfFontHandle *fh = (PgfFontHandle *)fontId;
	PgfFontData *fd;
	int ret;

	if (fh == NULL || charInfo == NULL)
		return SCE_FONT_ERROR_ARG;
	fd = fh->fontData;
	if (fd->disabledFlag != 0)
		return 0;

	fd->isCompositeGlyph = 0;
	ret = pgfLoadGlyph(fh, pgfCheckCompRange2(fd, charCode), 1);
	if (ret != 0)
		return ret;
	*charInfo = fh->charInfo;
	return 0;
}

/* 0x81005004 */
SceFont_t_error sceFontGetCharImageRect(SceFont_t_fontId fontId, SceFont_t_charCode charCode,
					SceFont_t_irect *rect)
{
	PgfFontHandle *fh = (PgfFontHandle *)fontId;
	PgfFontData *fd;
	int ret;

	if (fh == NULL || rect == NULL)
		return SCE_FONT_ERROR_ARG;
	fd = fh->fontData;
	if (fd->disabledFlag != 0) {
		rect->width = 0;
		rect->height = 0;
		return 0;
	}

	fd->isCompositeGlyph = 0;
	ret = pgfLoadGlyph(fh, pgfCheckCompRange2(fd, charCode), 1);
	if (ret != 0)
		return ret;

	fd = fh->fontData;
	if (fd->isCompositeGlyph != 0) {
		int minX = 0x7FFF, maxX = -32768;
		int minY = 0x7FFF, maxY = -32768;
		int i;

		if (fd->compGlyphCount == 0) {
			rect->width = 0;
			rect->height = (uint16_t)(maxY - minY);
			return 0;
		}
		for (i = 0; i < (int)fd->compGlyphCount; i++) {
			int left, top, right, bottom;
			ret = pgfLoadGlyph(fh, fd->compGlyphIds[i], 1);
			if (ret != 0)
				return ret;
			left = fh->charInfo.bitmapLeft;
			top = fh->charInfo.bitmapTop;
			right = left + (int)fh->charInfo.bitmapWidth;
			bottom = (int)fh->charInfo.bitmapHeight - top;
			if (left < minX)
				minX = left;
			if (right > maxX)
				maxX = right;
			if (-top < minY)
				minY = -top;
			if (bottom > maxY)
				maxY = bottom;
			fd = fh->fontData;
		}
		rect->width = (maxX - minX > 0) ? (uint16_t)(maxX - minX) : 0;
		rect->height = (maxY == minY) ? 0 : (uint16_t)(maxY - minY);
		return 0;
	}

	rect->width = (uint16_t)fh->charInfo.bitmapWidth;
	rect->height = (uint16_t)fh->charInfo.bitmapHeight;
	return 0;
}

/* 0x81005138 */
SceFont_t_error sceFontGetCharGlyphImage(SceFont_t_fontId fontId, SceFont_t_charCode charCode,
					 SceFont_t_userImageBufferRec *imageBuffer)
{
	PgfFontHandle *fh = (PgfFontHandle *)fontId;
	PgfFontData *fd;
	int ret;

	if (fh == NULL || imageBuffer == NULL)
		return SCE_FONT_ERROR_ARG;
	fd = fh->fontData;
	fd->isCompositeGlyph = 0;
	ret = pgfLoadGlyph(fh, pgfCheckCompRange2(fd, charCode), 2);
	if (ret != 0)
		return ret;

	fd = fh->fontData;
	if (fd->isCompositeGlyph != 0) {
		int32_t baseLeft = fh->charInfo.bitmapLeft;
		int32_t baseTop = fh->charInfo.bitmapTop;
		int i;

		for (i = 0; i < (int)fd->compGlyphCount; i++) {
			int w, h, offX64, offY64;
			ret = pgfLoadGlyph(fh, fd->compGlyphIds[i], 2);
			if (ret != 0)
				return ret;
			w = (int)fh->charInfo.bitmapWidth;
			h = (int)fh->charInfo.bitmapHeight;
			if (w != 0 && h != 0) {
				if (fh->glyphBitmapBuf == NULL)
					return SCE_FONT_ERROR_NOMEMORY;
				offX64 = (fh->charInfo.bitmapLeft << 6) - (baseLeft << 6);
				offY64 = (baseTop << 6) - (fh->charInfo.bitmapTop << 6);
				if (imageBuffer->pixelFormat == SCE_FONT_USERIMAGE_DIRECT4_L)
					pgfDrawGlyphDirect4(fh->glyphBitmapBuf, w, h, imageBuffer,
							    0, 0, imageBuffer->rect.width, imageBuffer->rect.height,
							    offX64, offY64);
				else if (imageBuffer->pixelFormat == SCE_FONT_USERIMAGE_DIRECT8)
					pgfDrawGlyphDirect8(fh->glyphBitmapBuf, w, h, imageBuffer,
							    0, 0, imageBuffer->rect.width, imageBuffer->rect.height,
							    offX64, offY64);
				else
					return SCE_FONT_ERROR_ARG;
			}
			fd = fh->fontData;
		}
		return 0;
	}

	if (fh->charInfo.bitmapWidth == 0 || fh->charInfo.bitmapHeight == 0)
		return 0;
	if (fh->glyphBitmapBuf == NULL)
		return SCE_FONT_ERROR_NOMEMORY;
	if (imageBuffer->pixelFormat == SCE_FONT_USERIMAGE_DIRECT4_L) {
		pgfDrawGlyphDirect4(fh->glyphBitmapBuf,
				    (int)fh->charInfo.bitmapWidth, (int)fh->charInfo.bitmapHeight,
				    imageBuffer, 0, 0, imageBuffer->rect.width, imageBuffer->rect.height, 0, 0);
		return 0;
	}
	if (imageBuffer->pixelFormat == SCE_FONT_USERIMAGE_DIRECT8) {
		pgfDrawGlyphDirect8(fh->glyphBitmapBuf,
				    (int)fh->charInfo.bitmapWidth, (int)fh->charInfo.bitmapHeight,
				    imageBuffer, 0, 0, imageBuffer->rect.width, imageBuffer->rect.height, 0, 0);
		return 0;
	}
	return SCE_FONT_ERROR_ARG;
}

/* 0x810052b0 */
SceFont_t_error sceFontGetCharGlyphImage_Clip(SceFont_t_fontId fontId, SceFont_t_charCode charCode,
					      SceFont_t_userImageBufferRec *imageBuffer,
					      SceFont_t_s32 clipX, SceFont_t_s32 clipY,
					      SceFont_t_u32 clipWidth, SceFont_t_u32 clipHeight)
{
	PgfFontHandle *fh = (PgfFontHandle *)fontId;
	PgfFontData *fd;
	int ret;

	if (fh == NULL || imageBuffer == NULL)
		return SCE_FONT_ERROR_ARG;
	if (clipHeight == 0 || clipWidth == 0)
		return 0;
	if ((int32_t)(clipWidth + (uint32_t)clipX) == 0 ||
	    (int32_t)(clipHeight + (uint32_t)clipY) == 0 ||
	    clipX >= (int32_t)imageBuffer->rect.width ||
	    clipY >= (int32_t)imageBuffer->rect.height)
		return 0;

	fd = fh->fontData;
	fd->isCompositeGlyph = 0;
	ret = pgfLoadGlyph(fh, pgfCheckCompRange2(fd, charCode), 2);
	if (ret != 0)
		return ret;

	fd = fh->fontData;
	if (fd->isCompositeGlyph != 0) {
		int32_t baseLeft64 = fh->charInfo.bitmapLeft << 6;
		int32_t baseTop64 = fh->charInfo.bitmapTop << 6;
		int i;

		for (i = 0; i < (int)fd->compGlyphCount; i++) {
			int w, h, offX64, offY64;
			ret = pgfLoadGlyph(fh, fd->compGlyphIds[i], 2);
			if (ret != 0)
				return ret;
			w = (int)fh->charInfo.bitmapWidth;
			h = (int)fh->charInfo.bitmapHeight;
			if (w != 0 && h != 0) {
				if (fh->glyphBitmapBuf == NULL)
					return SCE_FONT_ERROR_NOMEMORY;
				offX64 = (fh->charInfo.bitmapLeft << 6) - baseLeft64;
				offY64 = baseTop64 - (fh->charInfo.bitmapTop << 6);
				if (imageBuffer->pixelFormat == SCE_FONT_USERIMAGE_DIRECT4_L)
					pgfDrawGlyphDirect4(fh->glyphBitmapBuf, w, h, imageBuffer,
							    clipX, clipY, (int)clipWidth, (int)clipHeight,
							    offX64, offY64);
				else if (imageBuffer->pixelFormat == SCE_FONT_USERIMAGE_DIRECT8)
					pgfDrawGlyphDirect8(fh->glyphBitmapBuf, w, h, imageBuffer,
							    clipX, clipY, (int)clipWidth, (int)clipHeight,
							    offX64, offY64);
				else
					return SCE_FONT_ERROR_ARG;
			}
			fd = fh->fontData;
		}
		return 0;
	}

	if (fh->charInfo.bitmapWidth == 0 || fh->charInfo.bitmapHeight == 0)
		return 0;
	if (fh->glyphBitmapBuf == NULL)
		return SCE_FONT_ERROR_NOMEMORY;
	if (imageBuffer->pixelFormat == SCE_FONT_USERIMAGE_DIRECT4_L) {
		pgfDrawGlyphDirect4(fh->glyphBitmapBuf,
				    (int)fh->charInfo.bitmapWidth, (int)fh->charInfo.bitmapHeight,
				    imageBuffer, clipX, clipY, (int)clipWidth, (int)clipHeight, 0, 0);
		return 0;
	}
	if (imageBuffer->pixelFormat == SCE_FONT_USERIMAGE_DIRECT8) {
		pgfDrawGlyphDirect8(fh->glyphBitmapBuf,
				    (int)fh->charInfo.bitmapWidth, (int)fh->charInfo.bitmapHeight,
				    imageBuffer, clipX, clipY, (int)clipWidth, (int)clipHeight, 0, 0);
		return 0;
	}
	return SCE_FONT_ERROR_ARG;
}

/* 0x81005450 */
SceFont_t_f32 sceFontPixelToPointH(SceFont_t_libId libId, SceFont_t_f32 pixel,
				   SceFont_t_error *errorCode)
{
	PgfLib *lib = (PgfLib *)libId;
	float res, pt;
	uint32_t bits;

	if (errorCode == NULL)
		return 0.0f;
	*errorCode = 0;
	if (lib == NULL) {
		*errorCode = SCE_FONT_ERROR_LIBID;
		return 0.0f;
	}
	res = lib->hResolution;
	if (res < 0.0f ? (res > 0.0f) : (res * 0.999f < 0.0f)) {
		*errorCode = SCE_FONT_ERROR_NOSUPPORT;
		return 0.0f;
	}
	pt = (pixel * 72.0f) / res;
	bits = pgfFloatBits(pt);
	if ((bits & 0x7FFFFFFFU) == 0x7F800000U || bits == 0xFF80FFFFU) {
		*errorCode = SCE_FONT_ERROR_NOSUPPORT;
		return 0.0f;
	}
	return pt;
}

/* 0x810054cc */
SceFont_t_f32 sceFontPixelToPointV(SceFont_t_libId libId, SceFont_t_f32 pixel,
				   SceFont_t_error *errorCode)
{
	PgfLib *lib = (PgfLib *)libId;
	float res, pt;
	uint32_t bits;

	if (errorCode == NULL)
		return 0.0f;
	*errorCode = 0;
	if (lib == NULL) {
		*errorCode = SCE_FONT_ERROR_LIBID;
		return 0.0f;
	}
	res = lib->vResolution;
	if (res < 0.0f ? (res > 0.0f) : (res * 0.999f < 0.0f)) {
		*errorCode = SCE_FONT_ERROR_NOSUPPORT;
		return 0.0f;
	}
	pt = (pixel * 72.0f) / res;
	bits = pgfFloatBits(pt);
	if ((bits & 0x7FFFFFFFU) == 0x7F800000U || bits == 0xFF80FFFFU) {
		*errorCode = SCE_FONT_ERROR_NOSUPPORT;
		return 0.0f;
	}
	return pt;
}

/* 0x81005548 */
SceFont_t_f32 sceFontPointToPixelH(SceFont_t_libId libId, SceFont_t_f32 point,
				   SceFont_t_error *errorCode)
{
	PgfLib *lib = (PgfLib *)libId;
	float px;
	uint32_t bits;

	if (errorCode == NULL)
		return 0.0f;
	*errorCode = 0;
	if (lib == NULL) {
		*errorCode = SCE_FONT_ERROR_LIBID;
		return 0.0f;
	}
	px = (point * lib->hResolution) * (1.0f / 72.0f);
	bits = pgfFloatBits(px);
	if ((bits & 0x7FFFFFFFU) == 0x7F800000U || bits == 0xFF80FFFFU) {
		*errorCode = SCE_FONT_ERROR_NOSUPPORT;
		return 0.0f;
	}
	return px;
}

/* 0x810055a8 */
SceFont_t_f32 sceFontPointToPixelV(SceFont_t_libId libId, SceFont_t_f32 point,
				   SceFont_t_error *errorCode)
{
	PgfLib *lib = (PgfLib *)libId;
	float px;
	uint32_t bits;

	if (errorCode == NULL)
		return 0.0f;
	*errorCode = 0;
	if (lib == NULL) {
		*errorCode = SCE_FONT_ERROR_LIBID;
		return 0.0f;
	}
	px = (point * lib->vResolution) * (1.0f / 72.0f);
	bits = pgfFloatBits(px);
	if ((bits & 0x7FFFFFFFU) == 0x7F800000U || bits == 0xFF80FFFFU) {
		*errorCode = SCE_FONT_ERROR_NOSUPPORT;
		return 0.0f;
	}
	return px;
}

/* 0x81005608 */
SceFont_t_error sceFontSetAltCharacterCode(SceFont_t_libId libId, SceFont_t_charCode charCode)
{
	PgfLib *lib = (PgfLib *)libId;

	if (lib == NULL)
		return SCE_FONT_ERROR_LIBID;
	lib->altCharCode = charCode;
	return 0;
}

/* 0x8100561c */
SceFont_t_error sceFontFlush(SceFont_t_fontId fontId)
{
	PgfFontHandle *fh = (PgfFontHandle *)fontId;

	if (fh == NULL)
		return SCE_FONT_ERROR_ARG;
	sceClibMemset(&fh->charInfo, 0, sizeof(fh->charInfo));
	if (fh->glyphRawBuf != NULL) {
		fh->fontData->lib->initRec.freeFunc(fh->fontData->lib->initRec.userData, fh->glyphRawBuf);
		fh->glyphRawBuf = NULL;
	}
	if (fh->glyphBitmapBuf != NULL) {
		fh->fontData->lib->initRec.freeFunc(fh->fontData->lib->initRec.userData, fh->glyphBitmapBuf);
		fh->glyphBitmapBuf = NULL;
	}
	return 0;
}
