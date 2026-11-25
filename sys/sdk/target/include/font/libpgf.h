/*
 * font/libpgf.h: Sony PSP-compatible PGF font library public header
 * (SDK target/include/font/libpgf.h).
 */
#ifndef _SCE_FONT_LIBPGF_H
#define _SCE_FONT_LIBPGF_H

#ifdef __cplusplus
extern "C" {
#endif

#define SCE_FONT_MAX_OPEN			(9)

#define SCE_FONT_ERROR_NOMEMORY			-2142896127	/* 0x80460001 */
#define SCE_FONT_ERROR_LIBID			-2142896126	/* 0x80460002 */
#define SCE_FONT_ERROR_ARG			-2142896125	/* 0x80460003 */
#define SCE_FONT_ERROR_NOFILE			-2142896124	/* 0x80460004 */
#define SCE_FONT_ERROR_FILEOPEN			-2142896123	/* 0x80460005 */
#define SCE_FONT_ERROR_FILECLOSE		-2142896122	/* 0x80460006 */
#define SCE_FONT_ERROR_FILEREAD			-2142896121	/* 0x80460007 */
#define SCE_FONT_ERROR_FILESEEK			-2142896120	/* 0x80460008 */
#define SCE_FONT_ERROR_TOOMANYOPENED		-2142896119	/* 0x80460009 */
#define SCE_FONT_ERROR_ILLEGALVERSION		-2142896118	/* 0x8046000A */
#define SCE_FONT_ERROR_DATAINCONSISTENT		-2142896117	/* 0x8046000B */
#define SCE_FONT_ERROR_EXPIRED			-2142896116	/* 0x8046000C */
#define SCE_FONT_ERROR_REGISTRY			-2142896115	/* 0x8046000D */
#define SCE_FONT_ERROR_NOSUPPORT		-2142896114	/* 0x8046000E */
#define SCE_FONT_ERROR_UNKNOWN			-2142830593	/* 0x8046FFFF */

enum SceFontFamilyCode {
	SCE_FONT_DEFAULT_FAMILY_CODE = 0,
	SCE_FONT_FAMILY_SANSERIF = 1,
	SCE_FONT_FAMILY_SERIF = 2,
	SCE_FONT_FAMILY_ROUNDED = 3
};

enum SceFontStyleCode {
	SCE_FONT_DEFAULT_STYLE_CODE = 0,
	SCE_FONT_STYLE_REGULAR = 1,
	SCE_FONT_STYLE_OBLIQUE = 2,
	SCE_FONT_STYLE_NARROW = 3,
	SCE_FONT_STYLE_NARROW_OBLIQUE = 4,
	SCE_FONT_STYLE_BOLD = 5,
	SCE_FONT_STYLE_BOLD_OBLIQUE = 6,
	SCE_FONT_STYLE_BLACK = 7,
	SCE_FONT_STYLE_BLACK_OBLIQUE = 8,
	SCE_FONT_STYLE_L = 101,
	SCE_FONT_STYLE_M = 102,
	SCE_FONT_STYLE_DB = 103,
	SCE_FONT_STYLE_B = 104,
	SCE_FONT_STYLE_EB = 105,
	SCE_FONT_STYLE_UB = 106
};

enum SceFontImageByfferPixelFormatType {
	SCE_FONT_USERIMAGE_DIRECT4_L = 0,
	SCE_FONT_USERIMAGE_DIRECT8 = 2
};

enum SceFontLanguageCode {
	SCE_FONT_DEFAULT_LANGUAGE_CODE = 0,
	SCE_FONT_LANGUAGE_J = 1,
	SCE_FONT_LANGUAGE_LATIN = 2,
	SCE_FONT_LANGUAGE_K = 3,
	SCE_FONT_LANGUAGE_CJK = 5
};

enum SceFontRegionCode {
	SCE_FONT_GENERIC_REGION_CODE = 0,
	SCE_FONT_REGION_001 = 1,
	SCE_FONT_REGION_002 = 2,
	SCE_FONT_REGION_003 = 3,
	SCE_FONT_REGION_004 = 4,
	SCE_FONT_REGION_005 = 5,
	SCE_FONT_REGION_006 = 6,
	SCE_FONT_REGION_007 = 7
};

enum SceFontFontVendorCountryCode {
	SCE_FONT_GENERIC_COUNTRY_CODE = 0,
	SCE_FONT_COUNTRY_JAPAN = 1,
	SCE_FONT_COUNTRY_USA = 2,
	SCE_FONT_COUNTRY_KOREA = 3
};

enum SceFontBoolValue {
	SCE_FONT_FALSE = 0,
	SCE_FONT_TRUE = 1
};

#define SCE_FONT_SUBSTYLE_VERTICALLAYOUT	(0x0001)
#define SCE_FONT_SUBSTYLE_PSEUDO_BOLD		(0x0002)
#define SCE_FONT_SUBSTYLE_PSEUDO_SLANT		(0x0004)

#define SCE_FONT_FONTNAME_LENGTH		(64)
#define SCE_FONT_FONTFILENAME_LENGTH		(64)

enum SceFontDataAccessMode {
	SCE_FONT_FILEBASEDSTREAM = 0,
	SCE_FONT_MEMORYBASEDSTREAM = 1
};

#if defined(__GNUC__)
__extension__ typedef unsigned long long SceFont_t_u64;
__extension__ typedef signed long long SceFont_t_s64;
#else
typedef unsigned long long SceFont_t_u64;
typedef signed long long SceFont_t_s64;
#endif
typedef unsigned int SceFont_t_u32;
typedef signed int SceFont_t_s32;
typedef unsigned short SceFont_t_u16;
typedef signed short SceFont_t_s16;
typedef unsigned char SceFont_t_u8;
typedef signed char SceFont_t_s8;
typedef float SceFont_t_f32;
typedef double SceFont_t_f64;
typedef SceFont_t_u32 SceFont_t_bool;

typedef void *SceFont_t_libId;
typedef void *SceFont_t_fontId;
typedef void *SceFont_t_pointer;
typedef void *SceFont_t_handle;

typedef SceFont_t_s32 SceFont_t_error;
typedef SceFont_t_s32 SceFont_t_int;
typedef SceFont_t_u16 SceFont_t_charCode;
typedef SceFont_t_charCode *SceFont_t_string;
typedef SceFont_t_s32 SceFont_t_fontIndex;

typedef struct SceFont_t_irect {
	SceFont_t_u16 width;
	SceFont_t_u16 height;
} SceFont_t_irect;

typedef struct SceFont_t_rect {
	SceFont_t_u32 width;
	SceFont_t_u32 height;
} SceFont_t_rect;

typedef struct SceFont_t_cacheSystemInterface {
	SceFont_t_pointer *cacheInstance;
	SceFont_t_s32 (*lockFunc)(SceFont_t_pointer);
	SceFont_t_s32 (*unlockFunc)(SceFont_t_pointer);
	SceFont_t_pointer (*findFunc)(SceFont_t_pointer, SceFont_t_u32, SceFont_t_pointer, SceFont_t_bool *);
	SceFont_t_s32 (*writeKeyValueToCacheFunc)(SceFont_t_pointer, SceFont_t_pointer, SceFont_t_pointer);
	SceFont_t_s32 (*write0ToCacheFunc)(SceFont_t_pointer, SceFont_t_pointer, SceFont_t_pointer, SceFont_t_int);
	SceFont_t_s32 (*write1ToCacheFunc)(SceFont_t_pointer, SceFont_t_pointer, SceFont_t_pointer, SceFont_t_int);
	SceFont_t_s32 (*write2ToCacheFunc)(SceFont_t_pointer, SceFont_t_pointer, SceFont_t_pointer, SceFont_t_int);
	SceFont_t_s32 (*write3ToCacheFunc)(SceFont_t_pointer, SceFont_t_pointer, SceFont_t_pointer, SceFont_t_int);
	SceFont_t_s32 (*read0FromCacheFunc)(SceFont_t_pointer, SceFont_t_pointer, SceFont_t_pointer);
	SceFont_t_s32 (*read1FromCacheFunc)(SceFont_t_pointer, SceFont_t_pointer, SceFont_t_pointer);
	SceFont_t_s32 (*read2FromCacheFunc)(SceFont_t_pointer, SceFont_t_pointer, SceFont_t_pointer);
	SceFont_t_s32 (*read3FromCacheFunc)(SceFont_t_pointer, SceFont_t_pointer, SceFont_t_pointer);
} SceFont_t_cacheSystemInterface;

typedef struct SceFont_t_initRec {
	SceFont_t_pointer userData;
	SceFont_t_u32 maxNumFonts;
	SceFont_t_cacheSystemInterface *cache;
	SceFont_t_pointer (*allocFunc)(SceFont_t_pointer, SceFont_t_u32);
	void (*freeFunc)(SceFont_t_pointer, SceFont_t_pointer);
	SceFont_t_handle (*openFunc)(SceFont_t_pointer, SceFont_t_pointer, SceFont_t_error *);
	SceFont_t_error (*closeFunc)(SceFont_t_pointer, SceFont_t_handle);
	SceFont_t_u32 (*readFunc)(SceFont_t_pointer, SceFont_t_handle, SceFont_t_pointer, SceFont_t_u32, SceFont_t_u32, SceFont_t_error *);
	SceFont_t_error (*seekFunc)(SceFont_t_pointer, SceFont_t_handle, SceFont_t_u32);
	SceFont_t_s32 (*onErrorFunc)(SceFont_t_pointer, SceFont_t_s32);
	SceFont_t_s32 (*whenDoneReadFunc)(SceFont_t_pointer, SceFont_t_s32);
} SceFont_t_initRec;

typedef struct SceFont_t_fontStyleInfo {
	SceFont_t_f32 hSize;
	SceFont_t_f32 vSize;
	SceFont_t_f32 hResolution;
	SceFont_t_f32 vResolution;
	SceFont_t_f32 weight;
	SceFont_t_u16 familyCode;
	SceFont_t_u16 style;
	SceFont_t_u16 subStyle;
	SceFont_t_u16 languageCode;
	SceFont_t_u16 regionCode;
	SceFont_t_u16 countryCode;
	SceFont_t_u8 fontName[SCE_FONT_FONTNAME_LENGTH];
	SceFont_t_u8 fileName[SCE_FONT_FONTFILENAME_LENGTH];
	SceFont_t_u32 extraAttributes;
	SceFont_t_u32 expireDate;
} SceFont_t_fontStyleInfo;

typedef struct SceFont_t_userImageBufferRec {
	SceFont_t_u32 pixelFormat;
	SceFont_t_s32 xPos64;
	SceFont_t_s32 yPos64;
	SceFont_t_irect rect;
	SceFont_t_u16 bytesPerLine;
	SceFont_t_u16 reserved;
	SceFont_t_u8 *buffer;
} SceFont_t_userImageBufferRec;

typedef struct SceFont_t_iGlyphMetricsInfo {
	SceFont_t_u32 width64;
	SceFont_t_u32 height64;
	SceFont_t_s32 ascender64;
	SceFont_t_s32 descender64;
	SceFont_t_s32 horizontalBearingX64;
	SceFont_t_s32 horizontalBearingY64;
	SceFont_t_s32 verticalBearingX64;
	SceFont_t_s32 verticalBearingY64;
	SceFont_t_s32 horizontalAdvance64;
	SceFont_t_s32 verticalAdvance64;
} SceFont_t_iGlyphMetricsInfo;

typedef struct SceFont_t_charInfo {
	SceFont_t_u32 bitmapWidth;
	SceFont_t_u32 bitmapHeight;
	SceFont_t_s32 bitmapLeft;
	SceFont_t_s32 bitmapTop;
	SceFont_t_iGlyphMetricsInfo glyphMetrics;
	SceFont_t_u8 reserved0[2];
	SceFont_t_u16 reserved1;
} SceFont_t_charInfo;

typedef struct SceFont_t_fGlyphMetricsInfo {
	SceFont_t_f32 width;
	SceFont_t_f32 height;
	SceFont_t_f32 ascender;
	SceFont_t_f32 descender;
	SceFont_t_f32 horizontalBearingX;
	SceFont_t_f32 horizontalBearingY;
	SceFont_t_f32 verticalBearingX;
	SceFont_t_f32 verticalBearingY;
	SceFont_t_f32 horizontalAdvance;
	SceFont_t_f32 verticalAdvance;
} SceFont_t_fGlyphMetricsInfo;

typedef struct SceFont_t_fontInfo {
	SceFont_t_iGlyphMetricsInfo maxIGlyphMetrics;
	SceFont_t_fGlyphMetricsInfo maxFGlyphMetrics;
	SceFont_t_u16 maxGlyphBitmapWidth;
	SceFont_t_u16 maxGlyphBitmapHeight;
	SceFont_t_u32 numChars;
	SceFont_t_u32 numSubChars;
	SceFont_t_fontStyleInfo fontStyleInfo;
	SceFont_t_u8 pixelDepth;
	SceFont_t_u8 reserved[3];
} SceFont_t_fontInfo;

typedef struct SceFontCacheKey {
	int keyValue0;
	int keyValue1;
	int keyValue2;
	int keyValue3;
} SceFontCacheKey;

extern SceFont_t_libId sceFontNewLib(SceFont_t_initRec *initRec, SceFont_t_error *errorCode);
extern SceFont_t_error sceFontDoneLib(SceFont_t_libId libId);
extern SceFont_t_error sceFontSetResolution(SceFont_t_libId libId, SceFont_t_f32 hRes, SceFont_t_f32 vRes);
extern SceFont_t_int sceFontGetNumFontList(SceFont_t_libId libId, SceFont_t_error *errorCode);
extern SceFont_t_error sceFontGetFontList(SceFont_t_libId libId, SceFont_t_fontStyleInfo *fontStyleInfo, SceFont_t_int numFonts);
extern SceFont_t_fontIndex sceFontFindOptimumFont(SceFont_t_libId libId, SceFont_t_fontStyleInfo *fontStyleInfo, SceFont_t_error *errorCode);
extern SceFont_t_fontIndex sceFontFindFont(SceFont_t_libId libId, SceFont_t_fontStyleInfo *fontStyleInfo, SceFont_t_error *errorCode);
extern SceFont_t_error sceFontGetFontInfoByIndexNumber(SceFont_t_libId libId, SceFont_t_fontStyleInfo *fontStyleInfo, SceFont_t_fontIndex fontIndex);
extern SceFont_t_fontId sceFontOpen(SceFont_t_libId libId, SceFont_t_fontIndex fontIndex, SceFont_t_u32 mode, SceFont_t_error *errorCode);
extern SceFont_t_fontId sceFontOpenUserFile(SceFont_t_libId libId, SceFont_t_pointer fileName, SceFont_t_u32 mode, SceFont_t_error *errorCode);
extern SceFont_t_fontId sceFontOpenUserMemory(SceFont_t_libId libId, SceFont_t_pointer addr, SceFont_t_u32 size, SceFont_t_error *errorCode);
extern SceFont_t_error sceFontClose(SceFont_t_fontId fontId);
extern SceFont_t_error sceFontGetFontInfo(SceFont_t_fontId fontId, SceFont_t_fontInfo *fontInfo);
extern SceFont_t_error sceFontGetCharInfo(SceFont_t_fontId fontId, SceFont_t_charCode charCode, SceFont_t_charInfo *charInfo);
extern SceFont_t_error sceFontGetCharImageRect(SceFont_t_fontId fontId, SceFont_t_charCode charCode, SceFont_t_irect *rect);
extern SceFont_t_error sceFontGetCharGlyphImage(SceFont_t_fontId fontId, SceFont_t_charCode charCode, SceFont_t_userImageBufferRec *imageBuffer);
extern SceFont_t_error sceFontGetCharGlyphImage_Clip(SceFont_t_fontId fontId, SceFont_t_charCode charCode, SceFont_t_userImageBufferRec *imageBuffer, SceFont_t_s32 clipX, SceFont_t_s32 clipY, SceFont_t_u32 clipWidth, SceFont_t_u32 clipHeight);
extern SceFont_t_f32 sceFontPixelToPointH(SceFont_t_libId libId, SceFont_t_f32 pixel, SceFont_t_error *errorCode);
extern SceFont_t_f32 sceFontPixelToPointV(SceFont_t_libId libId, SceFont_t_f32 pixel, SceFont_t_error *errorCode);
extern SceFont_t_f32 sceFontPointToPixelH(SceFont_t_libId libId, SceFont_t_f32 point, SceFont_t_error *errorCode);
extern SceFont_t_f32 sceFontPointToPixelV(SceFont_t_libId libId, SceFont_t_f32 point, SceFont_t_error *errorCode);
extern SceFont_t_error sceFontSetAltCharacterCode(SceFont_t_libId libId, SceFont_t_charCode charCode);
extern SceFont_t_error sceFontFlush(SceFont_t_fontId fontId);

#ifdef __cplusplus
}
#endif

#endif /* _SCE_FONT_LIBPGF_H */
