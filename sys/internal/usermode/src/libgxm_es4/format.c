/*
 * SceGxm: format tables.
 *
 * Every texture, color surface and transfer format indexes a table entry
 * that says what the format supports and how big its texels are. The
 * entry names are empty in the release library.
 */

#include "gxm_internal.h"

/* --- texture formats --- */

typedef struct GxmTextureSwizzle {
	SceUInt32 componentCount;	/* components a query returns, 0 if invalid */
	const char *name;
} GxmTextureSwizzle;

typedef struct GxmTextureFormatInfo {
	const char *name;
	SceUInt32 queryPrecisions;	/* texture query precisions allowed (mask) */
	SceUInt16 flags;		/* GXM_FORMAT_FLAG_* */
	SceUInt16 level;		/* format level needed, 2 for no format */
	SceUInt16 alignment;		/* data alignment in bytes */
	SceUInt8 bitsPerPixel;
	SceUInt8 bytesPerPixel;
	const GxmTextureSwizzle *swizzles;
} GxmTextureFormatInfo;

#define GXM_FORMAT_LEVEL_NONE	2

#define GXM_TEXTURE_FORMAT_NONE	{ "", 0, 0, GXM_FORMAT_LEVEL_NONE, 0, 0, 0, gxmSwizzlesNone }

static const GxmTextureSwizzle gxmSwizzlesNone[8] = {
	{ 0, "" }, { 0, "" }, { 0, "" }, { 0, "" }, { 0, "" }, { 0, "" }, { 0, "" }, { 0, "" }
};

static const GxmTextureSwizzle gxmSwizzlesYuv420[8] = {
	{ 4, "" }, { 4, "" }, { 4, "" }, { 4, "" }, { 0, "" }, { 0, "" }, { 0, "" }, { 0, "" }
};

static const GxmTextureSwizzle gxmSwizzlesBlock[8] = {
	{ 4, "" }, { 0, "" }, { 0, "" }, { 0, "" }, { 4, "" }, { 0, "" }, { 0, "" }, { 0, "" }
};

static const GxmTextureSwizzle gxmSwizzlesYuv422[8] = {
	{ 4, "" }, { 4, "" }, { 4, "" }, { 4, "" }, { 4, "" }, { 4, "" }, { 4, "" }, { 4, "" }
};

static const GxmTextureSwizzle gxmSwizzles1[8] = {
	{ 1, "" }, { 4, "" }, { 4, "" }, { 4, "" }, { 4, "" }, { 4, "" }, { 4, "" }, { 4, "" }
};

static const GxmTextureSwizzle gxmSwizzles2[8] = {
	{ 2, "" }, { 4, "" }, { 4, "" }, { 4, "" }, { 4, "" }, { 4, "" }, { 0, "" }, { 0, "" }
};

static const GxmTextureSwizzle gxmSwizzles3[8] = {
	{ 4, "" }, { 4, "" }, { 0, "" }, { 0, "" }, { 0, "" }, { 0, "" }, { 0, "" }, { 0, "" }
};

static const GxmTextureSwizzle gxmSwizzlesX8U24[8] = {
	{ 1, "" }, { 1, "" }, { 0, "" }, { 0, "" }, { 0, "" }, { 0, "" }, { 0, "" }, { 0, "" }
};

static const GxmTextureSwizzle gxmSwizzles4Argb[8] = {
	{ 4, "" }, { 0, "" }, { 0, "" }, { 0, "" }, { 0, "" }, { 0, "" }, { 0, "" }, { 0, "" }
};

static const GxmTextureSwizzle gxmSwizzles4[8] = {
	{ 4, "" }, { 4, "" }, { 4, "" }, { 4, "" }, { 4, "" }, { 4, "" }, { 4, "" }, { 4, "" }
};

static const GxmTextureFormatInfo gxmTextureFormats[2][32] = {
	{
		[ 0] = { "", 0xb, 0x41, 0, 4, 8, 1, gxmSwizzles1 },	/* U8 */
		[ 1] = { "", 0x7, 0x40, 0, 4, 8, 1, gxmSwizzles1 },	/* S8 */
		[ 2] = { "", 0xb, 0x1, 0, 4, 16, 2, gxmSwizzles4 },	/* U4U4U4U4 */
		[ 3] = { "", 0xb, 0x1, 0, 4, 16, 2, gxmSwizzles4Argb },	/* U8U3U3U2 */
		[ 4] = { "", 0xb, 0x1, 0, 4, 16, 2, gxmSwizzles4 },	/* U1U5U5U5 */
		[ 5] = { "", 0xb, 0x1, 0, 4, 16, 2, gxmSwizzles3 },	/* U5U6U5 */
		[ 6] = { "", 0x3, 0x0, 0, 4, 16, 2, gxmSwizzles3 },	/* S5S5U6 */
		[ 7] = { "", 0xb, 0x1, 0, 4, 16, 2, gxmSwizzles2 },	/* U8U8 */
		[ 8] = { "", 0x7, 0x0, 0, 4, 16, 2, gxmSwizzles2 },	/* S8S8 */
		[ 9] = { "", 0x21, 0x40, 0, 4, 16, 2, gxmSwizzles1 },	/* U16 */
		[10] = { "", 0x11, 0x40, 0, 4, 16, 2, gxmSwizzles1 },	/* S16 */
		[11] = { "", 0x3, 0x40, 0, 4, 16, 2, gxmSwizzles1 },	/* F16 */
		[12] = { "", 0xb, 0x1, 0, 4, 32, 4, gxmSwizzles4 },	/* U8U8U8U8 */
		[13] = { "", 0x7, 0x0, 0, 4, 32, 4, gxmSwizzles4 },	/* S8S8S8S8 */
		[14] = { "", 0x3, 0x0, 0, 4, 32, 4, gxmSwizzles4 },	/* U2U10U10U10 */
		[15] = { "", 0x21, 0x0, 0, 4, 32, 4, gxmSwizzles2 },	/* U16U16 */
		[16] = { "", 0x11, 0x0, 0, 4, 32, 4, gxmSwizzles2 },	/* S16S16 */
		[17] = { "", 0x3, 0x0, 0, 4, 32, 4, gxmSwizzles2 },	/* F16F16 */
		[18] = { "", 0x1, 0x40, 0, 4, 32, 4, gxmSwizzles1 },	/* F32 */
		[19] = { "", 0x1, 0x40, 0, 4, 32, 4, gxmSwizzles1 },	/* F32M */
		[20] = { "", 0x3, 0x0, 0, 4, 32, 4, gxmSwizzles3 },	/* X8S8S8U8 */
		[21] = { "", 0x81, 0x40, 0, 4, 32, 4, gxmSwizzlesX8U24 },	/* X8U24 */
		[22] = GXM_TEXTURE_FORMAT_NONE,
		[23] = { "", 0x80, 0x0, 0, 4, 32, 4, gxmSwizzles1 },	/* U32 */
		[24] = { "", 0x40, 0x0, 0, 4, 32, 4, gxmSwizzles1 },	/* S32 */
		[25] = { "", 0x3, 0x0, 0, 4, 32, 4, gxmSwizzles3 },	/* SE5M9M9M9 */
		[26] = { "", 0x3, 0x0, 0, 4, 32, 4, gxmSwizzles3 },	/* F11F11F10 */
		[27] = { "", 0x3, 0x0, 0, 8, 64, 8, gxmSwizzles4 },	/* F16F16F16F16 */
		[28] = { "", 0x21, 0x0, 0, 8, 64, 8, gxmSwizzles4 },	/* U16U16U16U16 */
		[29] = { "", 0x11, 0x0, 0, 8, 64, 8, gxmSwizzles4 },	/* S16S16S16S16 */
		[30] = { "", 0x1, 0x0, 0, 8, 64, 8, gxmSwizzles2 },	/* F32F32 */
		[31] = { "", 0x80, 0x0, 0, 8, 64, 8, gxmSwizzles2 },	/* U32U32 */
	},
	{
		[ 0] = { "", 0xb, 0x3, 0, 8, 2, 4, gxmSwizzlesBlock },	/* PVRT2BPP */
		[ 1] = { "", 0xb, 0x3, 0, 8, 4, 4, gxmSwizzlesBlock },	/* PVRT4BPP */
		[ 2] = { "", 0xb, 0x3, 0, 8, 2, 4, gxmSwizzlesBlock },	/* PVRTII2BPP */
		[ 3] = { "", 0xb, 0x3, 0, 8, 4, 4, gxmSwizzlesBlock },	/* PVRTII4BPP */
		[ 4] = { "", 0xb, 0x3, 1, 8, 4, 0, gxmSwizzlesBlock },	/* ETC1 */
		[ 5] = { "", 0xb, 0x3, 0, 8, 4, 4, gxmSwizzlesBlock },	/* UBC1 */
		[ 6] = { "", 0xb, 0x3, 0, 16, 8, 4, gxmSwizzlesBlock },	/* UBC2 */
		[ 7] = { "", 0xb, 0x3, 0, 16, 8, 4, gxmSwizzlesBlock },	/* UBC3 */
		[ 8] = { "", 0xb, 0x43, 0, 8, 4, 4, gxmSwizzles1 },	/* UBC4 */
		[ 9] = { "", 0x7, 0x42, 0, 8, 4, 4, gxmSwizzles1 },	/* SBC4 */
		[10] = { "", 0xb, 0x3, 0, 16, 8, 4, gxmSwizzles2 },	/* UBC5 */
		[11] = { "", 0x7, 0x2, 0, 16, 8, 4, gxmSwizzles2 },	/* SBC5 */
		[12] = GXM_TEXTURE_FORMAT_NONE,
		[13] = GXM_TEXTURE_FORMAT_NONE,
		[14] = GXM_TEXTURE_FORMAT_NONE,
		[15] = GXM_TEXTURE_FORMAT_NONE,
		[16] = { "", 0xb, 0x8, 0, 4, 12, 4, gxmSwizzlesYuv420 },	/* YUV420P2 */
		[17] = { "", 0xb, 0x8, 0, 4, 12, 4, gxmSwizzlesYuv420 },	/* YUV420P3 */
		[18] = { "", 0xb, 0x4, 0, 4, 16, 4, gxmSwizzlesYuv422 },	/* YUV422 */
		[19] = GXM_TEXTURE_FORMAT_NONE,
		[20] = { "", 0xb, 0x11, 0, 4, 4, 4, gxmSwizzles4 },	/* P4 */
		[21] = { "", 0xb, 0x11, 0, 4, 8, 4, gxmSwizzles4 },	/* P8 */
		[22] = GXM_TEXTURE_FORMAT_NONE,
		[23] = GXM_TEXTURE_FORMAT_NONE,
		[24] = { "", 0xb, 0x21, 0, 4, 24, 3, gxmSwizzles3 },	/* U8U8U8 */
		[25] = { "", 0x7, 0x20, 0, 4, 24, 3, gxmSwizzles3 },	/* S8S8S8 */
		[26] = { "", 0x3, 0x0, 0, 4, 32, 4, gxmSwizzles4 },	/* U2F10F10F10 */
		[27] = GXM_TEXTURE_FORMAT_NONE,
		[28] = GXM_TEXTURE_FORMAT_NONE,
		[29] = GXM_TEXTURE_FORMAT_NONE,
		[30] = GXM_TEXTURE_FORMAT_NONE,
		[31] = GXM_TEXTURE_FORMAT_NONE,
	},
};

/* --- color surface formats --- */

#define GXM_SWIZZLE_NONE	0xffffffff

typedef struct GxmColorSwizzle {
	SceUInt32 textureSwizzle;	/* swizzle bits of the matching texture format */
	const char *name;
} GxmColorSwizzle;

/* outputFlags, per output register size */
#define GXM_COLOR_OUTPUT_NATIVE		0x01
#define GXM_COLOR_OUTPUT_CONVERT	0x02
#define GXM_COLOR_OUTPUT_F16		0x08
#define GXM_COLOR_OUTPUT_F32		0x10
#define GXM_COLOR_OUTPUT_GAMMA		0x20

/* flags */
#define GXM_COLOR_FLAG_DOWNSCALE	0x0001
#define GXM_COLOR_FLAG_DITHER		0x0002
#define GXM_COLOR_FLAG_RESTRICTED	0x0004	/* only with GXM_COMPAT_COLOR_RESTRICTED */

typedef struct GxmColorFormatInfo {
	const char *name;
	SceUInt32 outputFlags[2];
	SceUInt16 flags;
	SceUInt16 level;
	SceUInt8 bitsPerPixel;
	SceUInt8 bytesPerPixel;
	SceUInt16 swizzleGroup;		/* row of gxmColorSwizzles plus one */
	SceGxmTextureFormat textureFormat;
} GxmColorFormatInfo;

#define GXM_COLOR_FORMAT_NONE	{ "", { 0, 0 }, 0, GXM_FORMAT_LEVEL_NONE, 0, 0, 0, 0 }

static const GxmColorSwizzle gxmColorSwizzles[4][4] = {
	{ { 0x1000, "" }, { 0x3000, "" }, { GXM_SWIZZLE_NONE, "" }, { GXM_SWIZZLE_NONE, "" } },
	{ { 0x1000, "" }, { 0x5000, "" }, { 0x3000, "" }, { 0x4000, "" } },
	{ { 0x0, "" }, { 0x1000, "" }, { GXM_SWIZZLE_NONE, "" }, { GXM_SWIZZLE_NONE, "" } },
	{ { 0x0, "" }, { 0x1000, "" }, { 0x2000, "" }, { 0x3000, "" } },
};

static const GxmColorFormatInfo gxmColorFormats[3][16] = {
	{
		[ 0] = { "", { 0x5, 0x2a }, 0x3, 0, 32, 4, 4, SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8 },	/* U8U8U8U8 */
		[ 1] = { "", { 0x5, 0x2a }, 0x3, 0, 24, 3, 3, SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8 },	/* U8U8U8 */
		[ 2] = GXM_COLOR_FORMAT_NONE,
		[ 3] = { "", { 0x5, 0x2a }, 0x3, 0, 16, 2, 4, SCE_GXM_TEXTURE_BASE_FORMAT_U5U6U5 },	/* U5U6U5 */
		[ 4] = { "", { 0x5, 0x2a }, 0x3, 0, 16, 2, 4, SCE_GXM_TEXTURE_BASE_FORMAT_U1U5U5U5 },	/* U1U5U5U5 */
		[ 5] = { "", { 0x5, 0x2a }, 0x3, 0, 16, 2, 4, SCE_GXM_TEXTURE_BASE_FORMAT_U4U4U4U4 },	/* U4U4U4U4 */
		[ 6] = { "", { 0x5, 0x2a }, 0x3, 0, 16, 4, 4, SCE_GXM_TEXTURE_BASE_FORMAT_U8U3U3U2 },	/* U8U3U3U2 */
		[ 7] = GXM_COLOR_FORMAT_NONE,
		[ 8] = GXM_COLOR_FORMAT_NONE,
		[ 9] = GXM_COLOR_FORMAT_NONE,
		[10] = GXM_COLOR_FORMAT_NONE,
		[11] = GXM_COLOR_FORMAT_NONE,
		[12] = GXM_COLOR_FORMAT_NONE,
		[13] = GXM_COLOR_FORMAT_NONE,
		[14] = GXM_COLOR_FORMAT_NONE,
		[15] = { "", { 0x9, 0x0 }, 0x1, 0, 16, 2, 1, SCE_GXM_TEXTURE_BASE_FORMAT_F16 },	/* F16 */
	},
	{
		[ 0] = { "", { 0x9, 0x0 }, 0x1, 0, 32, 4, 2, SCE_GXM_TEXTURE_BASE_FORMAT_F16F16 },	/* F16F16 */
		[ 1] = { "", { 0x11, 0x0 }, 0x0, 0, 32, 4, 1, SCE_GXM_TEXTURE_BASE_FORMAT_F32 },	/* F32 */
		[ 2] = { "", { 0x5, 0x0 }, 0x1, 0, 16, 2, 1, SCE_GXM_TEXTURE_BASE_FORMAT_S16 },	/* S16 */
		[ 3] = { "", { 0x5, 0x0 }, 0x1, 0, 32, 4, 2, SCE_GXM_TEXTURE_BASE_FORMAT_S16S16 },	/* S16S16 */
		[ 4] = { "", { 0x5, 0x0 }, 0x1, 0, 16, 2, 1, SCE_GXM_TEXTURE_BASE_FORMAT_U16 },	/* U16 */
		[ 5] = { "", { 0x5, 0x0 }, 0x1, 0, 32, 4, 2, SCE_GXM_TEXTURE_BASE_FORMAT_U16U16 },	/* U16U16 */
		[ 6] = { "", { 0x0, 0xa }, 0x3, 0, 32, 4, 4, SCE_GXM_TEXTURE_BASE_FORMAT_U2U10U10U10 },	/* U2U10U10U10 */
		[ 7] = GXM_COLOR_FORMAT_NONE,
		[ 8] = { "", { 0x25, 0x0 }, 0x1, 0, 8, 1, 1, SCE_GXM_TEXTURE_BASE_FORMAT_U8 },	/* U8 */
		[ 9] = { "", { 0x5, 0x0 }, 0x1, 0, 8, 1, 1, SCE_GXM_TEXTURE_BASE_FORMAT_S8 },	/* S8 */
		[10] = { "", { 0x5, 0xa }, 0x1, 0, 16, 2, 3, SCE_GXM_TEXTURE_BASE_FORMAT_S5S5U6 },	/* S5S5U6 */
		[11] = { "", { 0x25, 0x0 }, 0x1, 0, 16, 2, 2, SCE_GXM_TEXTURE_BASE_FORMAT_U8U8 },	/* U8U8 */
		[12] = { "", { 0x5, 0x0 }, 0x1, 0, 16, 2, 2, SCE_GXM_TEXTURE_BASE_FORMAT_S8S8 },	/* S8S8 */
		[13] = { "", { 0x5, 0xa }, 0x5, 0, 32, 4, 4, SCE_GXM_TEXTURE_BASE_FORMAT_X8S8S8U8 },	/* U8S8S8U8 */
		[14] = { "", { 0x5, 0x0 }, 0x1, 0, 32, 4, 4, SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8S8 },	/* S8S8S8S8 */
		[15] = GXM_COLOR_FORMAT_NONE,
	},
	{
		[ 0] = { "", { 0x0, 0x9 }, 0x1, 0, 64, 8, 4, SCE_GXM_TEXTURE_BASE_FORMAT_F16F16F16F16 },	/* F16F16F16F16 */
		[ 1] = { "", { 0x0, 0x11 }, 0x0, 0, 64, 8, 2, SCE_GXM_TEXTURE_BASE_FORMAT_F32F32 },	/* F32F32 */
		[ 2] = { "", { 0x0, 0xa }, 0x1, 0, 32, 4, 3, SCE_GXM_TEXTURE_BASE_FORMAT_F11F11F10 },	/* F11F11F10 */
		[ 3] = { "", { 0x0, 0xa }, 0x1, 0, 32, 4, 3, SCE_GXM_TEXTURE_BASE_FORMAT_SE5M9M9M9 },	/* SE5M9M9M9 */
		[ 4] = { "", { 0x0, 0xa }, 0x1, 0, 32, 4, 4, SCE_GXM_TEXTURE_BASE_FORMAT_U2F10F10F10 },	/* U2F10F10F10 */
		[ 5] = GXM_COLOR_FORMAT_NONE,
		[ 6] = GXM_COLOR_FORMAT_NONE,
		[ 7] = GXM_COLOR_FORMAT_NONE,
		[ 8] = GXM_COLOR_FORMAT_NONE,
		[ 9] = GXM_COLOR_FORMAT_NONE,
		[10] = GXM_COLOR_FORMAT_NONE,
		[11] = GXM_COLOR_FORMAT_NONE,
		[12] = GXM_COLOR_FORMAT_NONE,
		[13] = GXM_COLOR_FORMAT_NONE,
		[14] = GXM_COLOR_FORMAT_NONE,
		[15] = GXM_COLOR_FORMAT_NONE,
	},
};

/* --- transfer formats --- */

#define GXM_TRANSFER_FLAG_COLOR		0x0001
#define GXM_TRANSFER_FLAG_RAW		0x0002
#define GXM_TRANSFER_FLAG_YUV		0x0004

typedef struct GxmTransferFormatInfo {
	SceUInt16 flags;
	SceUInt16 level;
	SceUInt16 alignment;
	SceUInt8 bytesPerPixel;
	SceUInt8 bitsPerPixel;
	const char *name;
} GxmTransferFormatInfo;

#define GXM_TRANSFER_FORMAT_NONE	{ 0, GXM_FORMAT_LEVEL_NONE, 0, 0, 0, "" }

static const GxmTransferFormatInfo gxmTransferFormats[20] = {
	[ 0] = { GXM_TRANSFER_FLAG_COLOR, 0, 1, 1, 8, "" },	/* U8_R */
	[ 1] = { GXM_TRANSFER_FLAG_COLOR, 0, 2, 2, 16, "" },	/* U4U4U4U4_ABGR */
	[ 2] = { GXM_TRANSFER_FLAG_COLOR, 0, 2, 2, 16, "" },	/* U1U5U5U5_ABGR */
	[ 3] = { GXM_TRANSFER_FLAG_COLOR, 0, 2, 2, 16, "" },	/* U5U6U5_BGR */
	[ 4] = { GXM_TRANSFER_FLAG_COLOR, 0, 2, 2, 16, "" },	/* U8U8_GR */
	[ 5] = { GXM_TRANSFER_FLAG_COLOR, 0, 4, 3, 24, "" },	/* U8U8U8_BGR */
	[ 6] = { GXM_TRANSFER_FLAG_COLOR, 0, 4, 4, 32, "" },	/* U8U8U8U8_ABGR */
	[ 7] = { GXM_TRANSFER_FLAG_YUV, 0, 4, 4, 16, "" },	/* VYUY422 */
	[ 8] = { GXM_TRANSFER_FLAG_YUV, 0, 4, 4, 16, "" },	/* YVYU422 */
	[ 9] = { GXM_TRANSFER_FLAG_YUV, 0, 4, 4, 16, "" },	/* UYVY422 */
	[10] = { GXM_TRANSFER_FLAG_YUV, 0, 4, 4, 16, "" },	/* YUYV422 */
	[11] = GXM_TRANSFER_FORMAT_NONE,
	[12] = GXM_TRANSFER_FORMAT_NONE,
	[13] = { GXM_TRANSFER_FLAG_COLOR, 0, 4, 4, 32, "" },	/* U2U10U10U10_ABGR */
	[14] = GXM_TRANSFER_FORMAT_NONE,
	[15] = { GXM_TRANSFER_FLAG_RAW, 0, 2, 2, 16, "" },	/* RAW16 */
	[16] = GXM_TRANSFER_FORMAT_NONE,
	[17] = { GXM_TRANSFER_FLAG_RAW, 0, 4, 4, 32, "" },	/* RAW32 */
	[18] = { GXM_TRANSFER_FLAG_RAW, 0, 8, 8, 64, "" },	/* RAW64 */
	[19] = { GXM_TRANSFER_FLAG_RAW, 0, 16, 16, 128, "" },	/* RAW128 */
};

/* --- lookups --- */

static inline const GxmColorFormatInfo *gxmColorFormatInfo(SceGxmColorFormat format)
{
	return &gxmColorFormats[(format >> 23) & 3][format >> 28];
}

_Bool gxmColorFormatIsSupported(SceGxmColorFormat format)
{
	const GxmColorFormatInfo *info = gxmColorFormatInfo(format);

	if (!(gxmGlobals.compatFlags & GXM_COMPAT_COLOR_RESTRICTED) &&
	    (info->flags & GXM_COLOR_FLAG_RESTRICTED))
		return 0;
	return info->level <= gxmGlobals.formatLevel;
}

_Bool gxmColorFormatIsGammaCapable(SceGxmColorFormat format, SceUInt32 outputRegisterSize)
{
	return (gxmColorFormatInfo(format)->outputFlags[outputRegisterSize] &
		GXM_COLOR_OUTPUT_GAMMA) != 0;
}

_Bool gxmColorFormatSupportsDownscale(SceGxmColorFormat format)
{
	return gxmColorFormatInfo(format)->flags & GXM_COLOR_FLAG_DOWNSCALE;
}

_Bool gxmColorFormatSupportsDither(SceGxmColorFormat format)
{
	return (gxmColorFormatInfo(format)->flags & GXM_COLOR_FLAG_DITHER) != 0;
}

SceUInt8 gxmColorFormatGetBitsPerPixel(SceGxmColorFormat format)
{
	return gxmColorFormatInfo(format)->bitsPerPixel;
}

SceUInt8 gxmColorFormatGetBytesPerPixel(SceGxmColorFormat format)
{
	return gxmColorFormatInfo(format)->bytesPerPixel;
}

/* the texture format reading a surface of this format */
SceGxmTextureFormat gxmColorFormatGetTextureFormat(SceGxmColorFormat format)
{
	const GxmColorFormatInfo *info = gxmColorFormatInfo(format);

	return info->textureFormat |
	       gxmColorSwizzles[info->swizzleGroup - 1][(format >> 20) & 3].textureSwizzle;
}

/* the surface word bit selecting format conversion; left alone if neither applies */
int gxmColorFormatGetOutputMode(SceGxmColorFormat format, SceUInt32 outputRegisterSize,
				SceUInt32 *mode)
{
	SceUInt32 flags = gxmColorFormatInfo(format)->outputFlags[outputRegisterSize];

	if (flags == 0)
		return SCE_GXM_ERROR_UNSUPPORTED;
	if (flags & GXM_COLOR_OUTPUT_NATIVE) {
		*mode = 0;
		return 0;
	}
	if (flags & GXM_COLOR_OUTPUT_CONVERT)
		*mode = 0x2000000;
	return 0;
}

int gxmColorFormatGetFloatMode4(SceGxmColorFormat format, SceUInt32 outputRegisterSize,
				SceUInt32 *mode)
{
	SceUInt32 flags = gxmColorFormatInfo(format)->outputFlags[outputRegisterSize];

	if (flags == 0)
		return SCE_GXM_ERROR_UNSUPPORTED;
	if (flags & GXM_COLOR_OUTPUT_F16) {
		*mode = 0x20;
		return 0;
	}
	if (flags & GXM_COLOR_OUTPUT_F32)
		*mode = 0x30;
	else
		*mode = 0;
	return 0;
}

int gxmColorFormatGetFloatMode14(SceGxmColorFormat format, SceUInt32 outputRegisterSize,
				 SceUInt32 *mode)
{
	SceUInt32 flags = gxmColorFormatInfo(format)->outputFlags[outputRegisterSize];

	if (flags == 0)
		return SCE_GXM_ERROR_UNSUPPORTED;
	if (flags & GXM_COLOR_OUTPUT_F16) {
		*mode = 0x8000;
		return 0;
	}
	if (flags & GXM_COLOR_OUTPUT_F32)
		*mode = 0xc000;
	else
		*mode = 0;
	return 0;
}

static inline const GxmTextureFormatInfo *gxmTextureFormatInfo(SceGxmTextureFormat format)
{
	return &gxmTextureFormats[format >> 31][(format >> 24) & 0x1f];
}

_Bool gxmTextureFormatIsSupported(SceGxmTextureFormat format)
{
	return gxmTextureFormatInfo(format)->level <= gxmGlobals.formatLevel;
}

_Bool gxmTextureFormatIsGammaCapable(SceGxmTextureFormat format)
{
	return gxmTextureFormatInfo(format)->flags & GXM_FORMAT_FLAG_GAMMA;
}

_Bool gxmTextureFormatIsCompressed(SceGxmTextureFormat format)
{
	return (gxmTextureFormatInfo(format)->flags & GXM_FORMAT_FLAG_COMPRESSED) != 0;
}

_Bool gxmTextureFormatIsYuv(SceGxmTextureFormat format)
{
	return (gxmTextureFormatInfo(format)->flags &
		(GXM_FORMAT_FLAG_YUV422 | GXM_FORMAT_FLAG_YUV_PLANAR)) != 0;
}

_Bool gxmTextureFormatIsYuvPlanar(SceGxmTextureFormat format)
{
	return (gxmTextureFormatInfo(format)->flags & GXM_FORMAT_FLAG_YUV_PLANAR) != 0;
}

_Bool gxmTextureFormatIsYuv422(SceGxmTextureFormat format)
{
	return (gxmTextureFormatInfo(format)->flags & GXM_FORMAT_FLAG_YUV422) != 0;
}

_Bool gxmTextureFormatIsPalettized(SceGxmTextureFormat format)
{
	return (gxmTextureFormatInfo(format)->flags & GXM_FORMAT_FLAG_PALETTE) != 0;
}

_Bool gxmTextureFormatIs24Bit(SceGxmTextureFormat format)
{
	return (gxmTextureFormatInfo(format)->flags & GXM_FORMAT_FLAG_24BIT) != 0;
}

SceUInt16 gxmTextureFormatGetAlignment(SceGxmTextureFormat format)
{
	return gxmTextureFormatInfo(format)->alignment;
}

SceUInt8 gxmTextureFormatGetBitsPerPixel(SceGxmTextureFormat format)
{
	return gxmTextureFormatInfo(format)->bitsPerPixel;
}

SceUInt8 gxmTextureFormatGetBytesPerPixel(SceGxmTextureFormat format)
{
	return gxmTextureFormatInfo(format)->bytesPerPixel;
}

/* the components a query of this swizzle returns, 0 for an invalid swizzle */
SceUInt32 gxmTextureFormatGetSwizzleComponents(SceGxmTextureFormat format)
{
	return gxmTextureFormatInfo(format)->swizzles[(format >> 12) & 7].componentCount;
}

static inline const GxmTransferFormatInfo *gxmTransferFormatInfo(SceGxmTransferFormat format)
{
	return &gxmTransferFormats[(format >> 16) & 0x3f];
}

_Bool gxmTransferFormatIsSupported(SceGxmTransferFormat format)
{
	return gxmTransferFormatInfo(format)->level <= gxmGlobals.formatLevel;
}

SceUInt16 gxmTransferFormatGetAlignment(SceGxmTransferFormat format)
{
	return gxmTransferFormatInfo(format)->alignment;
}

SceUInt8 gxmTransferFormatGetBytesPerPixel(SceGxmTransferFormat format)
{
	return gxmTransferFormatInfo(format)->bytesPerPixel;
}

SceUInt8 gxmTransferFormatGetBitsPerPixel(SceGxmTransferFormat format)
{
	return gxmTransferFormatInfo(format)->bitsPerPixel;
}

_Bool gxmTransferFormatIsColor(SceGxmTransferFormat format)
{
	return gxmTransferFormatInfo(format)->flags & GXM_TRANSFER_FLAG_COLOR;
}

_Bool gxmTransferFormatIsRaw(SceGxmTransferFormat format)
{
	return (gxmTransferFormatInfo(format)->flags & GXM_TRANSFER_FLAG_RAW) != 0;
}

_Bool gxmTransferFormatIsYuv(SceGxmTransferFormat format)
{
	return (gxmTransferFormatInfo(format)->flags & GXM_TRANSFER_FLAG_YUV) != 0;
}
