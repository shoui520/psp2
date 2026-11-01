/*
 * SceGxm: format tables.
 *
 * Every texture, color surface and transfer format indexes a table entry
 * that says what the format supports and how big its texels are. The
 * entry names are empty in the release library.
 */

#include "gxm_internal.h"

/* the debug build names every entry and swizzle; the release build keeps "" */
#if SCE_DBG_ASSERTS_ENABLED
#define GXM_NAME(name)	name
#else
#define GXM_NAME(name)	""
#endif

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

#define GXM_TEXTURE_FORMAT_NONE	{ GXM_NAME("reserved"), 0, 0, GXM_FORMAT_LEVEL_NONE, 0, 0, 0, gxmSwizzlesNone }

static const GxmTextureSwizzle gxmSwizzlesNone[8] = {
	{ 0, GXM_NAME("reserved") },
	{ 0, GXM_NAME("reserved") },
	{ 0, GXM_NAME("reserved") },
	{ 0, GXM_NAME("reserved") },
	{ 0, GXM_NAME("reserved") },
	{ 0, GXM_NAME("reserved") },
	{ 0, GXM_NAME("reserved") },
	{ 0, GXM_NAME("reserved") },
};

static const GxmTextureSwizzle gxmSwizzlesYuv420[8] = {
	{ 4, GXM_NAME("SCE_GXM_TEXTURE_SWIZZLE_YUV_CSC0") },
	{ 4, GXM_NAME("SCE_GXM_TEXTURE_SWIZZLE_YVU_CSC0") },
	{ 4, GXM_NAME("SCE_GXM_TEXTURE_SWIZZLE_YUV_CSC1") },
	{ 4, GXM_NAME("SCE_GXM_TEXTURE_SWIZZLE_YVU_CSC1") },
	{ 0, GXM_NAME("reserved") },
	{ 0, GXM_NAME("reserved") },
	{ 0, GXM_NAME("reserved") },
	{ 0, GXM_NAME("reserved") },
};

static const GxmTextureSwizzle gxmSwizzlesBlock[8] = {
	{ 4, GXM_NAME("SCE_GXM_TEXTURE_SWIZZLE4_ABGR") },
	{ 0, GXM_NAME("reserved") },
	{ 0, GXM_NAME("reserved") },
	{ 0, GXM_NAME("reserved") },
	{ 4, GXM_NAME("SCE_GXM_TEXTURE_SWIZZLE4_1BGR") },
	{ 0, GXM_NAME("reserved") },
	{ 0, GXM_NAME("reserved") },
	{ 0, GXM_NAME("reserved") },
};

static const GxmTextureSwizzle gxmSwizzlesYuv422[8] = {
	{ 4, GXM_NAME("SCE_GXM_TEXTURE_SWIZZLE_YUYV_CSC0") },
	{ 4, GXM_NAME("SCE_GXM_TEXTURE_SWIZZLE_YVYU_CSC0") },
	{ 4, GXM_NAME("SCE_GXM_TEXTURE_SWIZZLE_UYVY_CSC0") },
	{ 4, GXM_NAME("SCE_GXM_TEXTURE_SWIZZLE_VYUY_CSC0") },
	{ 4, GXM_NAME("SCE_GXM_TEXTURE_SWIZZLE_YUYV_CSC1") },
	{ 4, GXM_NAME("SCE_GXM_TEXTURE_SWIZZLE_YVYU_CSC1") },
	{ 4, GXM_NAME("SCE_GXM_TEXTURE_SWIZZLE_UYVY_CSC1") },
	{ 4, GXM_NAME("SCE_GXM_TEXTURE_SWIZZLE_VYUY_CSC1") },
};

static const GxmTextureSwizzle gxmSwizzles1[8] = {
	{ 1, GXM_NAME("SCE_GXM_TEXTURE_SWIZZLE1_R") },
	{ 4, GXM_NAME("SCE_GXM_TEXTURE_SWIZZLE1_000R") },
	{ 4, GXM_NAME("SCE_GXM_TEXTURE_SWIZZLE1_111R") },
	{ 4, GXM_NAME("SCE_GXM_TEXTURE_SWIZZLE1_RRRR") },
	{ 4, GXM_NAME("SCE_GXM_TEXTURE_SWIZZLE1_0RRR") },
	{ 4, GXM_NAME("SCE_GXM_TEXTURE_SWIZZLE1_1RRR") },
	{ 4, GXM_NAME("SCE_GXM_TEXTURE_SWIZZLE1_R000") },
	{ 4, GXM_NAME("SCE_GXM_TEXTURE_SWIZZLE1_R111") },
};

static const GxmTextureSwizzle gxmSwizzles2[8] = {
	{ 2, GXM_NAME("SCE_GXM_TEXTURE_SWIZZLE2_GR") },
	{ 4, GXM_NAME("SCE_GXM_TEXTURE_SWIZZLE2_00GR") },
	{ 4, GXM_NAME("SCE_GXM_TEXTURE_SWIZZLE2_GRRR") },
	{ 4, GXM_NAME("SCE_GXM_TEXTURE_SWIZZLE2_RGGG") },
	{ 4, GXM_NAME("SCE_GXM_TEXTURE_SWIZZLE2_GRGR") },
	{ 4, GXM_NAME("SCE_GXM_TEXTURE_SWIZZLE2_00RG") },
	{ 0, GXM_NAME("reserved") },
	{ 0, GXM_NAME("reserved") },
};

static const GxmTextureSwizzle gxmSwizzles3[8] = {
	{ 4, GXM_NAME("SCE_GXM_TEXTURE_SWIZZLE3_BGR") },
	{ 4, GXM_NAME("SCE_GXM_TEXTURE_SWIZZLE3_RGB") },
	{ 0, GXM_NAME("reserved") },
	{ 0, GXM_NAME("reserved") },
	{ 0, GXM_NAME("reserved") },
	{ 0, GXM_NAME("reserved") },
	{ 0, GXM_NAME("reserved") },
	{ 0, GXM_NAME("reserved") },
};

static const GxmTextureSwizzle gxmSwizzlesX8U24[8] = {
	{ 1, GXM_NAME("SCE_GXM_TEXTURE_SWIZZLE2_SD") },
	{ 1, GXM_NAME("SCE_GXM_TEXTURE_SWIZZLE2_DS") },
	{ 0, GXM_NAME("reserved") },
	{ 0, GXM_NAME("reserved") },
	{ 0, GXM_NAME("reserved") },
	{ 0, GXM_NAME("reserved") },
	{ 0, GXM_NAME("reserved") },
	{ 0, GXM_NAME("reserved") },
};

static const GxmTextureSwizzle gxmSwizzles4Argb[8] = {
	{ 4, GXM_NAME("SCE_GXM_TEXTURE_SWIZZLE4_ARGB") },
	{ 0, GXM_NAME("reserved") },
	{ 0, GXM_NAME("reserved") },
	{ 0, GXM_NAME("reserved") },
	{ 0, GXM_NAME("reserved") },
	{ 0, GXM_NAME("reserved") },
	{ 0, GXM_NAME("reserved") },
	{ 0, GXM_NAME("reserved") },
};

static const GxmTextureSwizzle gxmSwizzles4[8] = {
	{ 4, GXM_NAME("SCE_GXM_TEXTURE_SWIZZLE4_ABGR") },
	{ 4, GXM_NAME("SCE_GXM_TEXTURE_SWIZZLE4_ARGB") },
	{ 4, GXM_NAME("SCE_GXM_TEXTURE_SWIZZLE4_RGBA") },
	{ 4, GXM_NAME("SCE_GXM_TEXTURE_SWIZZLE4_BGRA") },
	{ 4, GXM_NAME("SCE_GXM_TEXTURE_SWIZZLE4_1BGR") },
	{ 4, GXM_NAME("SCE_GXM_TEXTURE_SWIZZLE4_1RGB") },
	{ 4, GXM_NAME("SCE_GXM_TEXTURE_SWIZZLE4_RGB1") },
	{ 4, GXM_NAME("SCE_GXM_TEXTURE_SWIZZLE4_BGR1") },
};

static const GxmTextureFormatInfo gxmTextureFormats[2][32] = {
	{
		[ 0] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_U8"), 0xb, 0x41, 0, 4, 8, 1, gxmSwizzles1 },
		[ 1] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_S8"), 0x7, 0x40, 0, 4, 8, 1, gxmSwizzles1 },
		[ 2] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_U4U4U4U4"), 0xb, 0x1, 0, 4, 16, 2, gxmSwizzles4 },
		[ 3] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_U8U3U3U2"), 0xb, 0x1, 0, 4, 16, 2, gxmSwizzles4Argb },
		[ 4] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_U1U5U5U5"), 0xb, 0x1, 0, 4, 16, 2, gxmSwizzles4 },
		[ 5] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_U5U6U5"), 0xb, 0x1, 0, 4, 16, 2, gxmSwizzles3 },
		[ 6] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_S5S5U6"), 0x3, 0x0, 0, 4, 16, 2, gxmSwizzles3 },
		[ 7] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_U8U8"), 0xb, 0x1, 0, 4, 16, 2, gxmSwizzles2 },
		[ 8] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_S8S8"), 0x7, 0x0, 0, 4, 16, 2, gxmSwizzles2 },
		[ 9] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_U16"), 0x21, 0x40, 0, 4, 16, 2, gxmSwizzles1 },
		[10] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_S16"), 0x11, 0x40, 0, 4, 16, 2, gxmSwizzles1 },
		[11] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_F16"), 0x3, 0x40, 0, 4, 16, 2, gxmSwizzles1 },
		[12] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8"), 0xb, 0x1, 0, 4, 32, 4, gxmSwizzles4 },
		[13] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8S8"), 0x7, 0x0, 0, 4, 32, 4, gxmSwizzles4 },
		[14] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_U2U10U10U10"), 0x3, 0x0, 0, 4, 32, 4, gxmSwizzles4 },
		[15] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_U16U16"), 0x21, 0x0, 0, 4, 32, 4, gxmSwizzles2 },
		[16] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_S16S16"), 0x11, 0x0, 0, 4, 32, 4, gxmSwizzles2 },
		[17] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_F16F16"), 0x3, 0x0, 0, 4, 32, 4, gxmSwizzles2 },
		[18] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_F32"), 0x1, 0x40, 0, 4, 32, 4, gxmSwizzles1 },
		[19] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_F32M"), 0x1, 0x40, 0, 4, 32, 4, gxmSwizzles1 },
		[20] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_X8S8S8U8"), 0x3, 0x0, 0, 4, 32, 4, gxmSwizzles3 },
		[21] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_X8U24"), 0x81, 0x40, 0, 4, 32, 4, gxmSwizzlesX8U24 },
		[22] = GXM_TEXTURE_FORMAT_NONE,
		[23] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_U32"), 0x80, 0x0, 0, 4, 32, 4, gxmSwizzles1 },
		[24] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_S32"), 0x40, 0x0, 0, 4, 32, 4, gxmSwizzles1 },
		[25] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_SE5M9M9M9"), 0x3, 0x0, 0, 4, 32, 4, gxmSwizzles3 },
		[26] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_F11F11F10"), 0x3, 0x0, 0, 4, 32, 4, gxmSwizzles3 },
		[27] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_F16F16F16F16"), 0x3, 0x0, 0, 8, 64, 8, gxmSwizzles4 },
		[28] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_U16U16U16U16"), 0x21, 0x0, 0, 8, 64, 8, gxmSwizzles4 },
		[29] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_S16S16S16S16"), 0x11, 0x0, 0, 8, 64, 8, gxmSwizzles4 },
		[30] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_F32F32"), 0x1, 0x0, 0, 8, 64, 8, gxmSwizzles2 },
		[31] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_U32U32"), 0x80, 0x0, 0, 8, 64, 8, gxmSwizzles2 },
	},
	{
		[ 0] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_PVRT2BPP"), 0xb, 0x3, 0, 8, 2, 4, gxmSwizzlesBlock },
		[ 1] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_PVRT4BPP"), 0xb, 0x3, 0, 8, 4, 4, gxmSwizzlesBlock },
		[ 2] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII2BPP"), 0xb, 0x3, 0, 8, 2, 4, gxmSwizzlesBlock },
		[ 3] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII4BPP"), 0xb, 0x3, 0, 8, 4, 4, gxmSwizzlesBlock },
		[ 4] = { GXM_NAME("reserved"), 0xb, 0x3, 1, 8, 4, 0, gxmSwizzlesBlock },	/* ETC1 */
		[ 5] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_UBC1"), 0xb, 0x3, 0, 8, 4, 4, gxmSwizzlesBlock },
		[ 6] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_UBC2"), 0xb, 0x3, 0, 16, 8, 4, gxmSwizzlesBlock },
		[ 7] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_UBC3"), 0xb, 0x3, 0, 16, 8, 4, gxmSwizzlesBlock },
		[ 8] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_UBC4"), 0xb, 0x43, 0, 8, 4, 4, gxmSwizzles1 },
		[ 9] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_SBC4"), 0x7, 0x42, 0, 8, 4, 4, gxmSwizzles1 },
		[10] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_UCB5"), 0xb, 0x3, 0, 16, 8, 4, gxmSwizzles2 },
		[11] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_SBC5"), 0x7, 0x2, 0, 16, 8, 4, gxmSwizzles2 },
		[12] = GXM_TEXTURE_FORMAT_NONE,
		[13] = GXM_TEXTURE_FORMAT_NONE,
		[14] = GXM_TEXTURE_FORMAT_NONE,
		[15] = GXM_TEXTURE_FORMAT_NONE,
		[16] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P2"), 0xb, 0x8, 0, 4, 12, 4, gxmSwizzlesYuv420 },
		[17] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P3"), 0xb, 0x8, 0, 4, 12, 4, gxmSwizzlesYuv420 },
		[18] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_YUV422"), 0xb, 0x4, 0, 4, 16, 4, gxmSwizzlesYuv422 },
		[19] = GXM_TEXTURE_FORMAT_NONE,
		[20] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_P4"), 0xb, 0x11, 0, 4, 4, 4, gxmSwizzles4 },
		[21] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_P8"), 0xb, 0x11, 0, 4, 8, 4, gxmSwizzles4 },
		[22] = GXM_TEXTURE_FORMAT_NONE,
		[23] = GXM_TEXTURE_FORMAT_NONE,
		[24] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8"), 0xb, 0x21, 0, 4, 24, 3, gxmSwizzles3 },
		[25] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8"), 0x7, 0x20, 0, 4, 24, 3, gxmSwizzles3 },
		[26] = { GXM_NAME("SCE_GXM_TEXTURE_BASE_FORMAT_U2F10F10F10"), 0x3, 0x0, 0, 4, 32, 4, gxmSwizzles4 },
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

#define GXM_COLOR_FORMAT_NONE	{ GXM_NAME("reserved"), { 0, 0 }, 0, GXM_FORMAT_LEVEL_NONE, 0, 0, 0, 0 }

static const GxmColorSwizzle gxmColorSwizzles[4][4] = {
	{
		{ 0x1000, GXM_NAME("SCE_GXM_COLOR_SWIZZLE1_R") },
		{ 0x3000, GXM_NAME("SCE_GXM_COLOR_SWIZZLE1_G") },
		{ GXM_SWIZZLE_NONE, GXM_NAME("reserved") },
		{ GXM_SWIZZLE_NONE, GXM_NAME("reserved") },
	},
	{
		{ 0x1000, GXM_NAME("SCE_GXM_COLOR_SWIZZLE_GR") },
		{ 0x5000, GXM_NAME("SCE_GXM_COLOR_SWIZZLE_RG") },
		{ 0x3000, GXM_NAME("SCE_GXM_COLOR_SWIZZLE_RA") },
		{ 0x4000, GXM_NAME("SCE_GXM_COLOR_SWIZZLE_AR") },
	},
	{
		{ 0x0, GXM_NAME("SCE_GXM_COLOR_SWIZZLE_BGR") },
		{ 0x1000, GXM_NAME("SCE_GXM_COLOR_SWIZZLE_RGB") },
		{ GXM_SWIZZLE_NONE, GXM_NAME("reserved") },
		{ GXM_SWIZZLE_NONE, GXM_NAME("reserved") },
	},
	{
		{ 0x0, GXM_NAME("SCE_GXM_COLOR_SWIZZLE4_ABGR") },
		{ 0x1000, GXM_NAME("SCE_GXM_COLOR_SWIZZLE4_ARGB") },
		{ 0x2000, GXM_NAME("SCE_GXM_COLOR_SWIZZLE4_RGBA") },
		{ 0x3000, GXM_NAME("SCE_GXM_COLOR_SWIZZLE4_BGRA") },
	},
};

static const GxmColorFormatInfo gxmColorFormats[3][16] = {
	{
		[ 0] = { GXM_NAME("SCE_GXM_COLOR_BASE_FORMAT_U8U8U8U8"), { 0x5, 0x2a }, 0x3, 0, 32, 4, 4, SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8 },
		[ 1] = { GXM_NAME("SCE_GXM_COLOR_BASE_FORMAT_U8U8U8"), { 0x5, 0x2a }, 0x3, 0, 24, 3, 3, SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8 },
		[ 2] = GXM_COLOR_FORMAT_NONE,
		[ 3] = { GXM_NAME("SCE_GXM_COLOR_BASE_FORMAT_U5U6U5"), { 0x5, 0x2a }, 0x3, 0, 16, 2, 4, SCE_GXM_TEXTURE_BASE_FORMAT_U5U6U5 },
		[ 4] = { GXM_NAME("SCE_GXM_COLOR_BASE_FORMAT_U1U5U5U5"), { 0x5, 0x2a }, 0x3, 0, 16, 2, 4, SCE_GXM_TEXTURE_BASE_FORMAT_U1U5U5U5 },
		[ 5] = { GXM_NAME("SCE_GXM_COLOR_BASE_FORMAT_U4U4U4U4"), { 0x5, 0x2a }, 0x3, 0, 16, 2, 4, SCE_GXM_TEXTURE_BASE_FORMAT_U4U4U4U4 },
		[ 6] = { GXM_NAME("SCE_GXM_COLOR_BASE_FORMAT_U8U3U3U2"), { 0x5, 0x2a }, 0x3, 0, 16, 4, 4, SCE_GXM_TEXTURE_BASE_FORMAT_U8U3U3U2 },
		[ 7] = GXM_COLOR_FORMAT_NONE,
		[ 8] = GXM_COLOR_FORMAT_NONE,
		[ 9] = GXM_COLOR_FORMAT_NONE,
		[10] = GXM_COLOR_FORMAT_NONE,
		[11] = GXM_COLOR_FORMAT_NONE,
		[12] = GXM_COLOR_FORMAT_NONE,
		[13] = GXM_COLOR_FORMAT_NONE,
		[14] = GXM_COLOR_FORMAT_NONE,
		[15] = { GXM_NAME("SCE_GXM_COLOR_BASE_FORMAT_F16"), { 0x9, 0x0 }, 0x1, 0, 16, 2, 1, SCE_GXM_TEXTURE_BASE_FORMAT_F16 },
	},
	{
		[ 0] = { GXM_NAME("SCE_GXM_COLOR_BASE_FORMAT_F16F16"), { 0x9, 0x0 }, 0x1, 0, 32, 4, 2, SCE_GXM_TEXTURE_BASE_FORMAT_F16F16 },
		[ 1] = { GXM_NAME("SCE_GXM_COLOR_BASE_FORMAT_F32"), { 0x11, 0x0 }, 0x0, 0, 32, 4, 1, SCE_GXM_TEXTURE_BASE_FORMAT_F32 },
		[ 2] = { GXM_NAME("SCE_GXM_COLOR_BASE_FORMAT_S16"), { 0x5, 0x0 }, 0x1, 0, 16, 2, 1, SCE_GXM_TEXTURE_BASE_FORMAT_S16 },
		[ 3] = { GXM_NAME("SCE_GXM_COLOR_BASE_FORMAT_S16S16"), { 0x5, 0x0 }, 0x1, 0, 32, 4, 2, SCE_GXM_TEXTURE_BASE_FORMAT_S16S16 },
		[ 4] = { GXM_NAME("SCE_GXM_COLOR_BASE_FORMAT_U16"), { 0x5, 0x0 }, 0x1, 0, 16, 2, 1, SCE_GXM_TEXTURE_BASE_FORMAT_U16 },
		[ 5] = { GXM_NAME("SCE_GXM_COLOR_BASE_FORMAT_U16U16"), { 0x5, 0x0 }, 0x1, 0, 32, 4, 2, SCE_GXM_TEXTURE_BASE_FORMAT_U16U16 },
		[ 6] = { GXM_NAME("SCE_GXM_COLOR_BASE_FORMAT_U2U10U10U10"), { 0x0, 0xa }, 0x3, 0, 32, 4, 4, SCE_GXM_TEXTURE_BASE_FORMAT_U2U10U10U10 },
		[ 7] = GXM_COLOR_FORMAT_NONE,
		[ 8] = { GXM_NAME("SCE_GXM_COLOR_BASE_FORMAT_U8"), { 0x25, 0x0 }, 0x1, 0, 8, 1, 1, SCE_GXM_TEXTURE_BASE_FORMAT_U8 },
		[ 9] = { GXM_NAME("SCE_GXM_COLOR_BASE_FORMAT_S8"), { 0x5, 0x0 }, 0x1, 0, 8, 1, 1, SCE_GXM_TEXTURE_BASE_FORMAT_S8 },
		[10] = { GXM_NAME("SCE_GXM_COLOR_BASE_FORMAT_S5S5U6"), { 0x5, 0xa }, 0x1, 0, 16, 2, 3, SCE_GXM_TEXTURE_BASE_FORMAT_S5S5U6 },
		[11] = { GXM_NAME("SCE_GXM_COLOR_BASE_FORMAT_U8U8"), { 0x25, 0x0 }, 0x1, 0, 16, 2, 2, SCE_GXM_TEXTURE_BASE_FORMAT_U8U8 },
		[12] = { GXM_NAME("SCE_GXM_COLOR_BASE_FORMAT_S8S8"), { 0x5, 0x0 }, 0x1, 0, 16, 2, 2, SCE_GXM_TEXTURE_BASE_FORMAT_S8S8 },
		[13] = { GXM_NAME("SCE_GXM_COLOR_BASE_FORMAT_U8S8S8U8"), { 0x5, 0xa }, 0x5, 0, 32, 4, 4, SCE_GXM_TEXTURE_BASE_FORMAT_X8S8S8U8 },
		[14] = { GXM_NAME("SCE_GXM_COLOR_BASE_FORMAT_S8S8S8S8"), { 0x5, 0x0 }, 0x1, 0, 32, 4, 4, SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8S8 },
		[15] = GXM_COLOR_FORMAT_NONE,
	},
	{
		[ 0] = { GXM_NAME("SCE_GXM_COLOR_BASE_FORMAT_F16F16F16F16"), { 0x0, 0x9 }, 0x1, 0, 64, 8, 4, SCE_GXM_TEXTURE_BASE_FORMAT_F16F16F16F16 },
		[ 1] = { GXM_NAME("SCE_GXM_COLOR_BASE_FORMAT_F32F32"), { 0x0, 0x11 }, 0x0, 0, 64, 8, 2, SCE_GXM_TEXTURE_BASE_FORMAT_F32F32 },
		[ 2] = { GXM_NAME("SCE_GXM_COLOR_BASE_FORMAT_F11F11F10"), { 0x0, 0xa }, 0x1, 0, 32, 4, 3, SCE_GXM_TEXTURE_BASE_FORMAT_F11F11F10 },
		[ 3] = { GXM_NAME("SCE_GXM_COLOR_BASE_FORMAT_SE5M9M9M9"), { 0x0, 0xa }, 0x1, 0, 32, 4, 3, SCE_GXM_TEXTURE_BASE_FORMAT_SE5M9M9M9 },
		[ 4] = { GXM_NAME("SCE_GXM_COLOR_BASE_FORMAT_U2F10F10F10"), { 0x0, 0xa }, 0x1, 0, 32, 4, 4, SCE_GXM_TEXTURE_BASE_FORMAT_U2F10F10F10 },
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

#define GXM_TRANSFER_FORMAT_NONE	{ 0, GXM_FORMAT_LEVEL_NONE, 0, 0, 0, GXM_NAME("reserved") }

static const GxmTransferFormatInfo gxmTransferFormats[20] = {
	[ 0] = { GXM_TRANSFER_FLAG_COLOR, 0, 1, 1, 8, GXM_NAME("SCE_GXM_TRANSFER_FORMAT_U8_R") },
	[ 1] = { GXM_TRANSFER_FLAG_COLOR, 0, 2, 2, 16, GXM_NAME("SCE_GXM_TRANSFER_FORMAT_U4U4U4U4_ABGR") },
	[ 2] = { GXM_TRANSFER_FLAG_COLOR, 0, 2, 2, 16, GXM_NAME("SCE_GXM_TRANSFER_FORMAT_U1U5U5U5_ABGR") },
	[ 3] = { GXM_TRANSFER_FLAG_COLOR, 0, 2, 2, 16, GXM_NAME("SCE_GXM_TRANSFER_FORMAT_U5U5U5_BGR") },
	[ 4] = { GXM_TRANSFER_FLAG_COLOR, 0, 2, 2, 16, GXM_NAME("SCE_GXM_TRANSFER_FORMAT_U8U8_GR") },
	[ 5] = { GXM_TRANSFER_FLAG_COLOR, 0, 4, 3, 24, GXM_NAME("SCE_GXM_TRANSFER_FORMAT_U8U8U8_BGR") },
	[ 6] = { GXM_TRANSFER_FLAG_COLOR, 0, 4, 4, 32, GXM_NAME("SCE_GXM_TRANSFER_FORMAT_U8U8U8U8_ABGR") },
	[ 7] = { GXM_TRANSFER_FLAG_YUV, 0, 4, 4, 16, GXM_NAME("SCE_GXM_TRANSFER_FORMAT_VYUY422") },
	[ 8] = { GXM_TRANSFER_FLAG_YUV, 0, 4, 4, 16, GXM_NAME("SCE_GXM_TRANSFER_FORMAT_YVYU422") },
	[ 9] = { GXM_TRANSFER_FLAG_YUV, 0, 4, 4, 16, GXM_NAME("SCE_GXM_TRANSFER_FORMAT_UYVY422") },
	[10] = { GXM_TRANSFER_FLAG_YUV, 0, 4, 4, 16, GXM_NAME("SCE_GXM_TRANSFER_FORMAT_YUYV422") },
	[11] = GXM_TRANSFER_FORMAT_NONE,
	[12] = GXM_TRANSFER_FORMAT_NONE,
	[13] = { GXM_TRANSFER_FLAG_COLOR, 0, 4, 4, 32, GXM_NAME("SCE_GXM_TRANSFER_FORMAT_U2U10U10U10_ABGR") },
	[14] = GXM_TRANSFER_FORMAT_NONE,
	[15] = { GXM_TRANSFER_FLAG_RAW, 0, 2, 2, 16, GXM_NAME("SCE_GXM_TRANSFER_FORMAT_RAW16") },
	[16] = GXM_TRANSFER_FORMAT_NONE,
	[17] = { GXM_TRANSFER_FLAG_RAW, 0, 4, 4, 32, GXM_NAME("SCE_GXM_TRANSFER_FORMAT_RAW32") },
	[18] = { GXM_TRANSFER_FLAG_RAW, 0, 8, 8, 64, GXM_NAME("SCE_GXM_TRANSFER_FORMAT_RAW64") },
	[19] = { GXM_TRANSFER_FLAG_RAW, 0, 16, 16, 128, GXM_NAME("SCE_GXM_TRANSFER_FORMAT_RAW128") },
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

#if SCE_DBG_ASSERTS_ENABLED
const char *gxmColorFormatGetName(SceGxmColorFormat format)
{
	return gxmColorFormatInfo(format)->name;
}

const char *gxmColorFormatGetSwizzleName(SceGxmColorFormat format)
{
	return gxmColorSwizzles[gxmColorFormatInfo(format)->swizzleGroup - 1][(format >> 20) & 3].name;
}
#endif

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

/* a color format has no output mode for this register size (the debug build says which) */
#define GXM_DBG_ERROR_OUTPUT_REGISTER_SIZE(line, format, outputRegisterSize)			\
	GXM_DBG_ERROR(line, "Parameter outputRegisterSize is %s, which is not allowed for color "	\
		      "format [%s|%s].", (outputRegisterSize) ? "64-bit" : "32-bit",		\
		      gxmColorFormatGetName(format), gxmColorFormatGetSwizzleName(format))

/* the surface word bit selecting format conversion; left alone if neither applies */
int gxmColorFormatGetOutputMode(SceGxmColorFormat format, SceUInt32 outputRegisterSize,
				SceUInt32 *mode)
{
	SceUInt32 flags = gxmColorFormatInfo(format)->outputFlags[outputRegisterSize];

	if (flags == 0) {
		GXM_DBG_ERROR_OUTPUT_REGISTER_SIZE(1377, format, outputRegisterSize);
		return SCE_GXM_ERROR_UNSUPPORTED;
	}
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

	if (flags == 0) {
		GXM_DBG_ERROR_OUTPUT_REGISTER_SIZE(1399, format, outputRegisterSize);
		return SCE_GXM_ERROR_UNSUPPORTED;
	}
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

	if (flags == 0) {
		GXM_DBG_ERROR_OUTPUT_REGISTER_SIZE(1423, format, outputRegisterSize);
		return SCE_GXM_ERROR_UNSUPPORTED;
	}
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

#if SCE_DBG_ASSERTS_ENABLED
const char *gxmTextureFormatGetName(SceGxmTextureFormat format)
{
	return gxmTextureFormatInfo(format)->name;
}

const char *gxmTextureFormatGetSwizzleName(SceGxmTextureFormat format)
{
	return gxmTextureFormatInfo(format)->swizzles[(format >> 12) & 7].name;
}

/* every precision asked for is one the format can be queried at */
_Bool gxmTextureFormatIsQueryPrecisionSupported(SceGxmTextureFormat format, SceUInt32 precisions)
{
	return (precisions & ~gxmTextureFormatInfo(format)->queryPrecisions) == 0;
}
#endif

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

#if SCE_DBG_ASSERTS_ENABLED
_Bool gxmTextureFormatSupportsSampleCoefficients(SceGxmTextureFormat format)
{
	return (gxmTextureFormatInfo(format)->flags & GXM_FORMAT_FLAG_SAMPLE_COEFFICIENTS) != 0;
}
#endif

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

#if SCE_DBG_ASSERTS_ENABLED
const char *gxmTransferFormatGetName(SceGxmTransferFormat format)
{
	return gxmTransferFormatInfo(format)->name;
}
#endif

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
