/*
 * SceLibPvf's own objects: the library, its fonts and the FreeType rasterizer under them.
 * Laid out from the firmware; no Sony record names them (agent/namecheck).
 */
#ifndef LIBPVF_INTERNAL_H
#define LIBPVF_INTERNAL_H

#include <font/libpvf.h>
#include <ft2build.h>
#include FT_FREETYPE_H

/* the system fonts (sa0:data/font/pvf/) */
#define PVF_NUM_SYSTEM_FONTS	SCE_PVF_MAX_OPEN

/* how a font is opened (PvfFont.openMode) */
#define PVF_OPEN_SYSTEM_STREAM	0	/* a system font by index, read as needed */
#define PVF_OPEN_SYSTEM_MEMORY	1	/* a system font by index, read whole */
#define PVF_OPEN_USER_STREAM	2	/* a user file, read as needed */
#define PVF_OPEN_USER_MEMORY	3	/* a user file, read whole */
#define PVF_OPEN_USER_BUFFER	4	/* a font image in the caller's memory */

/* the size the library starts with, in points (hCharSize, vCharSize) */
#define PVF_DEFAULT_CHAR_SIZE	10.0f
/* the resolution it starts with, in dpi */
#define PVF_DEFAULT_RESOLUTION	128.0f
/* the em size it starts with (scePvfSetEM) */
#define PVF_DEFAULT_EM		1000.0f
/* the character drawn for one the font has none of ('_') */
#define PVF_DEFAULT_ALT_CHAR	0x5F
/* the system fonts' folder */
#define PVF_FONT_PATH		"sa0:data/font/pvf/"
#define PVF_FONT_PATH_SIZE	1024
/* a font file's path: the folder and a file name */
#define PVF_PATH_SIZE		(PVF_FONT_PATH_SIZE + SCE_PVF_FONTFILENAME_LENGTH)

/* a float the library treats as zero */
#define PVF_IS_ZERO(x)		(__builtin_fabsf(x) <= 0.0f)
/* and one it does not (a NaN is neither) */
#define PVF_IS_NONZERO(x)	(__builtin_fabsf(x) > 0.0f)

struct PvfLib;
struct PvfRasterizer;

/* a resolution, in dpi */
typedef struct PvfResolution {
	ScePvf_t_f32 h;
	ScePvf_t_f32 v;
} PvfResolution;

/* a face's largest glyph metrics, in 26.6 (scePvfGetFontInfo's maxIGlyphMetrics) */
typedef struct PvfFontMetrics {
	ScePvf_t_s32 ascender64;
	ScePvf_t_s32 descender64;
	ScePvf_t_s32 horizontalBearingX64;
	ScePvf_t_s32 horizontalBearingY64;
	ScePvf_t_s32 verticalBearingX64;
	ScePvf_t_s32 verticalBearingY64;
	ScePvf_t_s32 horizontalAdvance64;
	ScePvf_t_s32 verticalAdvance64;
	ScePvf_t_u32 width64;
	ScePvf_t_u32 height64;
} PvfFontMetrics;

/* a font file the library opened, shared by the handles that use it (276 bytes) */
typedef struct PvfFont {
	char fileName[SCE_PVF_FONTFILENAME_LENGTH];	/* a user file's name */
	ScePvf_t_s32 subFontIndex;
	ScePvf_t_u32 openMode;				/* PVF_OPEN_* */
	ScePvf_t_s32 fontIndex;		/* the system font's index, or the user font's number */
	struct PvfLib *lib;
	ScePvf_t_u8 inUse;
	ScePvf_t_u8 refCount;		/* the handles open on it */
	ScePvf_t_u8 infoValid;
	ScePvf_t_u8 systemFile;		/* the path was made from the system font folder */
	ScePvf_t_s32 face;		/* the rasterizer's face */
	ScePvf_t_u32 unitsPerEM;
	ScePvf_t_s32 numGlyphs;
	ScePvf_t_s32 numFaces;
	char familyName[64];
	char styleName[64];
	PvfFontMetrics metrics;		/* the face's, in 26.6 */
	ScePvf_t_s32 cacheable;		/* glyphs go through the library's cache */
	ScePvf_t_s32 reserved;
} PvfFont;

/* a font handle (ScePvf_t_fontId, 132 bytes) */
typedef struct PvfFontHandle {
	ScePvf_t_u8 inUse;
	ScePvf_t_u8 padding;
	ScePvf_t_charCode charCode;	/* the glyph loaded */
	PvfResolution resolution;	/* the library's, when the glyph was loaded */
	ScePvf_t_f32 hCharSize;
	ScePvf_t_f32 vCharSize;
	ScePvf_t_f32 emboldenRate;	/* scePvfSetEmboldenRate's */
	ScePvf_t_f32 skewAngleX;	/* scePvfSetSkewValue's */
	ScePvf_t_f32 skewAngleY;
	ScePvf_t_f32 libEmboldenRate;	/* the library's, when the handle was opened */
	ScePvf_t_f32 libSkewValue;
	PvfFont *font;
	ScePvf_t_u8 *glyphImage;	/* the loaded glyph's bitmap */
	ScePvf_t_charInfo charInfo;	/* the loaded glyph's */
	SceUID memBlock;		/* the default font's shared memory */
	ScePvf_t_s32 memBlockServer;	/* we made it */
	ScePvf_t_s32 memBlockClient;	/* we opened another's */
	void *memBlockBase;
	ScePvf_t_s32 glyphMode;		/* what the loaded glyph was rendered for */
} PvfFontHandle;

/* the library (ScePvf_t_libId, 1120 bytes) */
typedef struct PvfLib {
	struct PvfRasterizer *rasterizer;
	ScePvf_t_f32 em;
	ScePvf_t_initRec init;
	PvfFontHandle *handles;		/* init.maxNumFonts of them */
	PvfFont *fonts;			/* init.maxNumFonts of them */
	ScePvf_t_u16 numHandles;
	ScePvf_t_u16 numFonts;
	PvfResolution resolution;
	ScePvf_t_f32 hCharSize;
	ScePvf_t_f32 vCharSize;
	ScePvf_t_f32 emboldenRate;
	ScePvf_t_f32 skewValue;
	ScePvf_t_s32 reserved0;
	ScePvf_t_s32 numFontList;	/* the system fonts; -1 until counted */
	void *fontListBuffer;
	ScePvf_t_charCode altCharCode;
	ScePvf_t_s8 fontPath[PVF_FONT_PATH_SIZE];
	ScePvf_t_u8 reserved1[6];
	ScePvf_t_s32 nextUserFont;	/* the number the next user font gets */
} PvfLib;

/* the rasterizer's FreeType objects (28 bytes) */
typedef struct PvfFaceTable {
	FT_Library library;
	FT_Memory memory;
	FT_Face *faces;			/* maxFaces of them */
	void **buffers;			/* the files read whole */
	struct PvfGsub **vertTables;	/* the faces' vertical substitutions */
	ScePvf_t_s32 maxFaces;
	ScePvf_t_s32 numFaces;
} PvfFaceTable;

/* what the rasterizer renders with (72 bytes) */
typedef struct PvfRasterizer {
	void *userData;
	PvfFaceTable *table;
	ScePvf_t_s32 forceAutohint;	/* FT_LOAD_FORCE_AUTOHINT, else FT_LOAD_NO_HINTING */
	ScePvf_t_s32 antialias;		/* FT_RENDER_MODE_NORMAL, else _MONO; 1 */
	ScePvf_t_f32 hResolution;	/* dpi */
	ScePvf_t_f32 vResolution;
	FT_F26Dot6 charWidth;
	FT_F26Dot6 charHeight;
	FT_Pos embolden;		/* 26.6 */
	FT_Matrix skew;			/* 16.16 */
	FT_Fixed slant;			/* 16.16 */
	void *memUserData;
	void *(*alloc)(void *userData, ScePvf_t_u32 size);
	void *(*realloc)(void *userData, void *ptr, ScePvf_t_u32 size);
	void (*free)(void *userData, void *ptr);
} PvfRasterizer;

/* a face's information (PvfRasterizerGetFaceInfo's) */
typedef struct PvfFaceInfo {
	FT_Long styleFlags;
	FT_Long faceFlags;
	FT_Long numFaces;
	FT_Long faceIndex;
	FT_Long numGlyphs;
	FT_Int numCharmaps;
	char familyName[128];
	char styleName[128];
	FT_Int unitsPerEM;		/* the face's, widened */
	FT_Int ascender;
	FT_Int descender;
	FT_Int height;
	FT_Int maxAdvanceWidth;
	FT_Int maxAdvanceHeight;
	FT_Int underlinePosition;
	FT_Int underlineThickness;
} PvfFaceInfo;

/* the rasterizer (pvf_rasterizer.c), libpvf's layer over libSceFt2's FreeType */
/* a FreeType error as libpvf's */
ScePvf_t_error PvfRasterizerError(FT_Error error);
void PvfRasterizerSetLibCHook(void);
void PvfRasterizerDoneLibCHook(void);
PvfRasterizer *PvfRasterizerNew(ScePvf_t_s32 maxFaces, void *userData, void *memUserData,
	void *(*alloc)(void *userData, ScePvf_t_u32 size),
	void *(*realloc)(void *userData, void *ptr, ScePvf_t_u32 size),
	void (*free)(void *userData, void *ptr));
void PvfRasterizerDelete(PvfRasterizer *rasterizer);
ScePvf_t_s32 PvfRasterizerOpenFile(PvfRasterizer *rasterizer, const char *path,
	ScePvf_t_s32 stream, ScePvf_t_s32 subFontIndex, ScePvf_t_error *errorCode);
ScePvf_t_s32 PvfRasterizerOpenMemory(PvfRasterizer *rasterizer, const void *buffer,
	ScePvf_t_s32 size, ScePvf_t_s32 subFontIndex, ScePvf_t_error *errorCode);
ScePvf_t_error PvfRasterizerClose(PvfRasterizer *rasterizer, ScePvf_t_s32 face);
void PvfRasterizerSetResolution(PvfRasterizer *rasterizer, ScePvf_t_f32 hResolution,
	ScePvf_t_f32 vResolution);
void PvfRasterizerSetCharSize(PvfRasterizer *rasterizer, FT_F26Dot6 width, FT_F26Dot6 height);
void PvfRasterizerSetEmbolden(PvfRasterizer *rasterizer, FT_Pos embolden);
void PvfRasterizerSetSkew(PvfRasterizer *rasterizer, ScePvf_t_f32 angleX, ScePvf_t_f32 angleY);
void PvfRasterizerSetSlant(PvfRasterizer *rasterizer, FT_Fixed slant);
void PvfRasterizerGetFaceInfo(PvfRasterizer *rasterizer, ScePvf_t_s32 face, PvfFaceInfo *info);
ScePvf_t_bool PvfRasterizerHasChar(PvfRasterizer *rasterizer, ScePvf_t_s32 face,
	ScePvf_t_charCode charCode);
ScePvf_t_bool PvfRasterizerHasVertChar(PvfRasterizer *rasterizer, ScePvf_t_s32 face,
	ScePvf_t_charCode charCode);
ScePvf_t_error PvfRasterizerGetKerning(PvfRasterizer *rasterizer, ScePvf_t_s32 face,
	ScePvf_t_charCode left, ScePvf_t_charCode right, ScePvf_t_iKerningInfo *kerning);
ScePvf_t_error PvfRasterizerGetGlyphMetrics(PvfRasterizer *rasterizer, ScePvf_t_s32 face,
	ScePvf_t_charCode charCode, FT_Glyph_Metrics *metrics);
ScePvf_t_error PvfRasterizerGetVertGlyphMetrics(PvfRasterizer *rasterizer, ScePvf_t_s32 face,
	ScePvf_t_charCode charCode, FT_Glyph_Metrics *metrics, ScePvf_t_s32 *vertical);
ScePvf_t_error PvfRasterizerRenderGlyph(PvfRasterizer *rasterizer, ScePvf_t_s32 face,
	ScePvf_t_charCode charCode, ScePvf_t_s32 depth8, ScePvf_t_s32 *width,
	ScePvf_t_s32 *rows, ScePvf_t_s32 *top, ScePvf_t_s32 *left, ScePvf_t_s32 *pitch,
	ScePvf_t_u8 **buffer);
ScePvf_t_error PvfRasterizerRenderVertGlyph(PvfRasterizer *rasterizer, ScePvf_t_s32 face,
	ScePvf_t_charCode charCode, ScePvf_t_s32 depth8, ScePvf_t_s32 *width,
	ScePvf_t_s32 *rows, ScePvf_t_s32 *top, ScePvf_t_s32 *left, ScePvf_t_s32 *pitch,
	ScePvf_t_u8 **buffer, ScePvf_t_s32 *vertical);
ScePvf_t_error PvfRasterizerFreeBitmap(PvfRasterizer *rasterizer, ScePvf_t_u8 *buffer);
ScePvf_t_error PvfRasterizerGetOutline(PvfRasterizer *rasterizer, ScePvf_t_s32 face,
	ScePvf_t_charCode charCode, int *flags, short *numContours, short *numPoints,
	short **contours, FT_Vector **points, char **tags);
ScePvf_t_error PvfRasterizerGetVertOutline(PvfRasterizer *rasterizer, ScePvf_t_s32 face,
	ScePvf_t_charCode charCode, int *flags, short *numContours, short *numPoints,
	short **contours, FT_Vector **points, char **tags, ScePvf_t_s32 *vertical);
ScePvf_t_error PvfRasterizerFreeOutline(PvfRasterizer *rasterizer, short *contours,
	FT_Vector *points, char *tags);

/* the vertical-substitution table's memory (PvfGsubNew copies it) */
typedef struct PvfGsubMemory {
	void *(*alloc)(void *user, ScePvf_t_u32 size);
	void (*free)(void *user, void *ptr);
	void *user;
} PvfGsubMemory;

/* a face's vertical substitutions (pvf_gsub.c), from its GSUB table's 'vert' and 'vrt2' */
struct PvfGsub;
struct PvfGsub *PvfGsubNew(const PvfGsubMemory *memory, FT_Bytes table);
void PvfGsubDelete(struct PvfGsub *gsub);
/* the vertical form of a glyph, or -1 */
FT_Int PvfGsubGetVertGlyph(struct PvfGsub *gsub, FT_UInt glyph);

/*
 * SceLibPvf's exports the SDK's libpvf.h does not declare (the SDK's stub library names them):
 * a glyph's outline, FreeType's.
 */
ScePvf_t_error scePvfGetCharGlyphOutline(ScePvf_t_fontId fontID, ScePvf_t_charCode charCode,
	FT_Outline *outline);
ScePvf_t_error scePvfGetVertCharGlyphOutline(ScePvf_t_fontId fontID,
	ScePvf_t_charCode charCode, FT_Outline *outline);
ScePvf_t_error scePvfReleaseCharGlyphOutline(ScePvf_t_fontId fontID, FT_Outline *outline);
void __scePvfSetFt2LibCHook(void);
void __scePvfSetFt2DoneLibCHook(void);

#endif
