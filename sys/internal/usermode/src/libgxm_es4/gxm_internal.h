/*
 * SceGxm: library internals shared by the source files.
 *
 * The library is ARM code built with GCC; exported functions taking
 * floating-point values receive them in VFP registers (the user ABI).
 */

#ifndef _GXM_INTERNAL_H
#define _GXM_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <kernel/sysmem.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include <gpu/pvrsrv_client.h>

int sceKernelIsGameBudget(void);

#define GXM_VFP		__attribute__((pcs("aapcs-vfp")))

/* a function Sony's build kept out of line although it could be inlined */
#define GXM_NOINLINE	__attribute__((noinline))

#define GXM_ASSERT_OFFSET(type, field, offset) \
	_Static_assert(offsetof(type, field) == (offset), #type "." #field)

/* --- error codes --- */

/* error codes (Sony gxm/error.h) */
#define SCE_GXM_ERROR_UNINITIALIZED			0x805b0000
#define SCE_GXM_ERROR_ALREADY_INITIALIZED		0x805b0001
#define SCE_GXM_ERROR_OUT_OF_MEMORY			0x805b0002
#define SCE_GXM_ERROR_INVALID_VALUE			0x805b0003
#define SCE_GXM_ERROR_INVALID_POINTER			0x805b0004
#define SCE_GXM_ERROR_INVALID_ALIGNMENT			0x805b0005
#define SCE_GXM_ERROR_NOT_WITHIN_SCENE			0x805b0006
#define SCE_GXM_ERROR_WITHIN_SCENE			0x805b0007
#define SCE_GXM_ERROR_NULL_PROGRAM			0x805b0008
#define SCE_GXM_ERROR_UNSUPPORTED			0x805b0009
#define SCE_GXM_ERROR_PATCHER_INTERNAL			0x805b000a
#define SCE_GXM_ERROR_RESERVE_FAILED			0x805b000b
#define SCE_GXM_ERROR_PROGRAM_IN_USE			0x805b000c
#define SCE_GXM_ERROR_INVALID_INDEX_COUNT		0x805b000d
#define SCE_GXM_ERROR_INVALID_POLYGON_MODE		0x805b000e
#define SCE_GXM_ERROR_INVALID_SAMPLER_RESULT_TYPE_PRECISION	0x805b000f
#define SCE_GXM_ERROR_INVALID_SAMPLER_RESULT_TYPE_COMPONENT_COUNT	0x805b0010
#define SCE_GXM_ERROR_UNIFORM_BUFFER_NOT_RESERVED	0x805b0011
#define SCE_GXM_ERROR_INVALID_PRECOMPUTED_DRAW		0x805b0014
#define SCE_GXM_ERROR_INVALID_PRECOMPUTED_VERTEX_STATE	0x805b0015
#define SCE_GXM_ERROR_INVALID_PRECOMPUTED_FRAGMENT_STATE	0x805b0016
#define SCE_GXM_ERROR_DRIVER				0x805b0017
#define SCE_GXM_ERROR_INVALID_THREAD			0x805b0018
#define SCE_GXM_ERROR_INVALID_TEXTURE_DATA_POINTER	0x805b0019
#define SCE_GXM_ERROR_INVALID_TEXTURE_PALETTE_POINTER	0x805b001a
#define SCE_GXM_ERROR_INVALID_OUTPUT_REGISTER_SIZE	0x805b001b
#define SCE_GXM_ERROR_INVALID_FRAGMENT_MSAA_MODE	0x805b001c
#define SCE_GXM_ERROR_INVALID_VISIBILITY_BUFFER_POINTER	0x805b001d
#define SCE_GXM_ERROR_INVALID_VISIBILITY_INDEX		0x805b001e
#define SCE_GXM_ERROR_INVALID_DEPTH_STENCIL_CONFIGURATION	0x805b001f
#define SCE_GXM_ERROR_INVALID_TEXTURE			0x805b0020
#define SCE_GXM_ERROR_OUT_OF_HOST_MEMORY		0x805b0021
#define SCE_GXM_ERROR_OUT_OF_BUFFER_MEMORY		0x805b0022
#define SCE_GXM_ERROR_OUT_OF_VERTEX_USSE_MEMORY		0x805b0023
#define SCE_GXM_ERROR_OUT_OF_FRAGMENT_USSE_MEMORY	0x805b0024
#define SCE_GXM_ERROR_INVALID_PRIMITIVE_TYPE		0x805b0025
#define SCE_GXM_ERROR_INVALID_MAPPING			0x805b0026
#define SCE_GXM_ERROR_OUT_OF_RENDER_TARGETS		0x805b0027
#define SCE_GXM_ERROR_INVALID_VISIBILITY_OP		0x805b0028
#define SCE_GXM_ERROR_RAZOR				0x805b0029
#define SCE_GXM_ERROR_INVALID_SAMPLER_FILTER_MODE	0x805b002a
#define SCE_GXM_ERROR_INVALID_REGION_CLIP_IN_COMMAND_LIST	0x805b002b
#define SCE_GXM_ERROR_WITHIN_COMMAND_LIST		0x805b002c
#define SCE_GXM_ERROR_NOT_WITHIN_COMMAND_LIST		0x805b002d
#define SCE_GXM_ERROR_BUFFER_OVERRUN			0x805b002e

/* SceGxmMultisampleMode */
#define SCE_GXM_MULTISAMPLE_NONE	0
#define SCE_GXM_MULTISAMPLE_2X		1
#define SCE_GXM_MULTISAMPLE_4X		2

/* --- texture formats --- */

typedef SceUInt32 SceGxmTextureFormat;

/* the format word is spread over the control words; these bits carry it */
#define GXM_FORMAT_BASE_EXT		0x80000000	/* control word 0 */
#define GXM_FORMAT_BASE			0x1f000000	/* control word 1 */
#define GXM_FORMAT_SWIZZLE		0x00007000	/* control word 3, >> 16 */

#define SCE_GXM_TEXTURE_BASE_FORMAT_U8	0x00000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_S8	0x01000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_U4U4U4U4	0x02000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_U8U3U3U2	0x03000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_U1U5U5U5	0x04000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_U5U6U5	0x05000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_S5S5U6	0x06000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_U8U8	0x07000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_S8S8	0x08000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_U16	0x09000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_S16	0x0a000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_F16	0x0b000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8U8	0x0c000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8S8	0x0d000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_U2U10U10U10	0x0e000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_U16U16	0x0f000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_S16S16	0x10000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_F16F16	0x11000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_F32	0x12000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_F32M	0x13000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_X8S8S8U8	0x14000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_X8U24	0x15000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_U32	0x17000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_S32	0x18000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_SE5M9M9M9	0x19000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_F11F11F10	0x1a000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_F16F16F16F16	0x1b000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_U16U16U16U16	0x1c000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_S16S16S16S16	0x1d000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_F32F32	0x1e000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_U32U32	0x1f000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_PVRT2BPP	0x80000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_PVRT4BPP	0x81000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII2BPP	0x82000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_PVRTII4BPP	0x83000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_ETC1	0x84000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_UBC1	0x85000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_UBC2	0x86000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_UBC3	0x87000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_UBC4	0x88000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_SBC4	0x89000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_UBC5	0x8a000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_SBC5	0x8b000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P2	0x90000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_YUV420P3	0x91000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_YUV422	0x92000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_P4	0x94000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_P8	0x95000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_U8U8U8	0x98000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_S8S8S8	0x99000000
#define SCE_GXM_TEXTURE_BASE_FORMAT_U2F10F10F10	0x9a000000

/* format table entry flags */
#define GXM_FORMAT_FLAG_GAMMA		0x0001	/* supports gamma correction */
#define GXM_FORMAT_FLAG_COMPRESSED	0x0002	/* block-compressed */
#define GXM_FORMAT_FLAG_YUV422		0x0004	/* packed YUV */
#define GXM_FORMAT_FLAG_YUV_PLANAR	0x0008	/* planar YUV */
#define GXM_FORMAT_FLAG_PALETTE		0x0010	/* palettized */
#define GXM_FORMAT_FLAG_24BIT		0x0020	/* three bytes per texel */

_Bool gxmTextureFormatIsSupported(SceGxmTextureFormat format);
_Bool gxmTextureFormatIsGammaCapable(SceGxmTextureFormat format);
_Bool gxmTextureFormatIsCompressed(SceGxmTextureFormat format);
_Bool gxmTextureFormatIsYuv(SceGxmTextureFormat format);
_Bool gxmTextureFormatIsYuvPlanar(SceGxmTextureFormat format);
_Bool gxmTextureFormatIsYuv422(SceGxmTextureFormat format);
_Bool gxmTextureFormatIsPalettized(SceGxmTextureFormat format);
_Bool gxmTextureFormatIs24Bit(SceGxmTextureFormat format);
SceUInt16 gxmTextureFormatGetAlignment(SceGxmTextureFormat format);
SceUInt8 gxmTextureFormatGetBitsPerPixel(SceGxmTextureFormat format);
SceUInt8 gxmTextureFormatGetBytesPerPixel(SceGxmTextureFormat format);
SceUInt32 gxmTextureFormatGetSwizzleComponents(SceGxmTextureFormat format);

typedef SceUInt32 SceGxmColorFormat;

_Bool gxmColorFormatIsSupported(SceGxmColorFormat format);
_Bool gxmColorFormatIsGammaCapable(SceGxmColorFormat format, SceUInt32 outputRegisterSize);
_Bool gxmColorFormatSupportsDownscale(SceGxmColorFormat format);
_Bool gxmColorFormatSupportsDither(SceGxmColorFormat format);
SceUInt8 gxmColorFormatGetBitsPerPixel(SceGxmColorFormat format);
SceUInt8 gxmColorFormatGetBytesPerPixel(SceGxmColorFormat format);
SceGxmTextureFormat gxmColorFormatGetTextureFormat(SceGxmColorFormat format);
int gxmColorFormatGetOutputMode(SceGxmColorFormat format, SceUInt32 outputRegisterSize,
				SceUInt32 *mode);
int gxmColorFormatGetFloatMode4(SceGxmColorFormat format, SceUInt32 outputRegisterSize,
				SceUInt32 *mode);
int gxmColorFormatGetFloatMode14(SceGxmColorFormat format, SceUInt32 outputRegisterSize,
				 SceUInt32 *mode);

typedef SceUInt32 SceGxmTransferFormat;

_Bool gxmTransferFormatIsSupported(SceGxmTransferFormat format);
SceUInt16 gxmTransferFormatGetAlignment(SceGxmTransferFormat format);
SceUInt8 gxmTransferFormatGetBytesPerPixel(SceGxmTransferFormat format);
SceUInt8 gxmTransferFormatGetBitsPerPixel(SceGxmTransferFormat format);
_Bool gxmTransferFormatIsColor(SceGxmTransferFormat format);
_Bool gxmTransferFormatIsRaw(SceGxmTransferFormat format);
_Bool gxmTransferFormatIsYuv(SceGxmTransferFormat format);

/* --- textures --- */

typedef struct SceGxmTexture {
	SceUInt32 controlWords[4];
} SceGxmTexture;

SceGxmTextureFormat sceGxmTextureGetFormat(const SceGxmTexture *texture);

/* control word 0 as bound to a context: bits 29 and 30 are left out */
#define GXM_TEX0_BOUND_MASK	0x9fffffff

/* control word 0 */
#define GXM_TEX0_STRIDE_EXT		0x00000006	/* linear strided: stride bits 12..13 */
#define GXM_TEX0_VADDR_MODE_SHIFT	3
#define GXM_TEX0_VADDR_MODE		0x00000038
#define GXM_TEX0_UADDR_MODE_SHIFT	6
#define GXM_TEX0_UADDR_MODE		0x000001c0
#define GXM_TEX0_MIP_FILTER		0x00000200
#define GXM_TEX0_STRIDE_LOW		0x00000e00	/* linear strided: stride bits 0..2 */
#define GXM_TEX0_MIN_FILTER_SHIFT	10
#define GXM_TEX0_MIN_FILTER		0x00000c00
#define GXM_TEX0_MAG_FILTER_SHIFT	12
#define GXM_TEX0_MAG_FILTER		0x00003000
#define GXM_TEX0_MIP_COUNT_SHIFT	17
#define GXM_TEX0_MIP_COUNT		0x001e0000
#define GXM_TEX0_STRIDE			0x07fe0000	/* linear strided: stride bits 3..12 */
#define GXM_TEX0_LOD_BIAS_SHIFT		21
#define GXM_TEX0_LOD_BIAS		0x07e00000
#define GXM_TEX0_GAMMA_MODE		0x18000000

/* control word 1 */
#define GXM_TEX1_TYPE			0xe0000000
#define GXM_TEX1_WIDTH_SHIFT		12
#define GXM_TEX1_WIDTH			0x00fff000
#define GXM_TEX1_HEIGHT			0x00000fff
#define GXM_TEX1_WIDTH_LOG2_SHIFT	16
#define GXM_TEX1_WIDTH_LOG2		0x000f0000
#define GXM_TEX1_HEIGHT_LOG2		0x0000000f

/* control word 2 */
#define GXM_TEX2_LOD_MIN_HIGH		0x00000003
#define GXM_TEX2_DATA			0xfffffffc

/* control word 3 */
#define GXM_TEX3_PALETTE		0x03ffffff
#define GXM_TEX3_LOD_MIN_LOW_SHIFT	26
#define GXM_TEX3_LOD_MIN_LOW		0x0c000000
#define GXM_TEX3_SWIZZLE		0x70000000
#define GXM_TEX3_NORMALIZE_MODE		0x80000000

#define SCE_GXM_TEXTURE_SWIZZLED		0x00000000
#define SCE_GXM_TEXTURE_CUBE			0x40000000
#define SCE_GXM_TEXTURE_LINEAR			0x60000000
#define SCE_GXM_TEXTURE_TILED			0x80000000
#define SCE_GXM_TEXTURE_SWIZZLED_ARBITRARY	0xa0000000
#define SCE_GXM_TEXTURE_LINEAR_STRIDED		0xc0000000
#define SCE_GXM_TEXTURE_CUBE_ARBITRARY		0xe0000000

/* the arbitrary-size types store width and height minus one, the others log2 */
#define GXM_TEXTURE_TYPE_ARBITRARY_SIZE		0xa0000000

#define SCE_GXM_TEXTURE_ADDR_REPEAT		0
#define SCE_GXM_TEXTURE_ADDR_MIRROR		1
#define SCE_GXM_TEXTURE_ADDR_CLAMP		2
#define SCE_GXM_TEXTURE_ADDR_MIRROR_CLAMP	3
#define SCE_GXM_TEXTURE_ADDR_REPEAT_IGNORE_BORDER	4
#define SCE_GXM_TEXTURE_ADDR_CLAMP_FULL_BORDER	5
#define SCE_GXM_TEXTURE_ADDR_CLAMP_IGNORE_BORDER	6
#define SCE_GXM_TEXTURE_ADDR_CLAMP_HALF_BORDER	7

int sceGxmTextureInitSwizzled(SceGxmTexture *texture, const void *data,
			      SceGxmTextureFormat texFormat, SceUInt32 width,
			      SceUInt32 height, SceUInt32 mipCount);
int sceGxmTextureInitLinear(SceGxmTexture *texture, const void *data,
			    SceGxmTextureFormat texFormat, SceUInt32 width,
			    SceUInt32 height, SceUInt32 mipCount);
int sceGxmTextureInitTiled(SceGxmTexture *texture, const void *data,
			   SceGxmTextureFormat texFormat, SceUInt32 width,
			   SceUInt32 height, SceUInt32 mipCount);
int sceGxmTextureInitLinearStrided(SceGxmTexture *texture, const void *data,
				   SceGxmTextureFormat texFormat, SceUInt32 width,
				   SceUInt32 height, SceUInt32 byteStride);
int sceGxmTextureSetFormat(SceGxmTexture *texture, SceGxmTextureFormat texFormat);
int sceGxmTextureSetData(SceGxmTexture *texture, const void *data);
int sceGxmTextureSetGammaMode(SceGxmTexture *texture, SceUInt32 gammaMode);
int sceGxmTextureValidate(const SceGxmTexture *texture);

#define GXM_TEXTURE_MAX_SIZE		4096
#define GXM_TEXTURE_MAX_MIP_COUNT	13
#define GXM_TEXTURE_MAX_LOD_BIAS	64
#define GXM_TEXTURE_TILE_SIZE		32
#define GXM_TEXTURE_MAX_STRIDE		0x20000

/* --- programs --- */

/* a self-relative offset field, 0 for none */
#define GXM_RELATIVE(field)	((field) ? (const void *)((const char *)&(field) + (field)) : NULL)

#define GXM_PROGRAM_FRAGMENT		0x00000001
#define GXM_PROGRAM_DISCARD		0x00000008
#define GXM_PROGRAM_DEPTH_REPLACE	0x00000010
#define GXM_PROGRAM_SPRITE_COORD	0x00000020
#define GXM_PROGRAM_NATIVE_COLOR	0x00000040
#define GXM_PROGRAM_FRAG_COLOR		0x00000080
#define GXM_PROGRAM_UNK2		0x00000002
#define GXM_PROGRAM_UNK4		0x00000004	/* needs one instruction after the program */
#define GXM_PROGRAM_MSAA_2X		0x00000100	/* built for 2x MSAA translucency */
#define GXM_PROGRAM_MSAA_4X		0x00000200	/* built for 4x MSAA translucency */
#define GXM_PROGRAM_UNK400		0x00000400
#define GXM_PROGRAM_UNK800		0x00000800
#define GXM_PROGRAM_UNK1000		0x00001000
#define GXM_PROGRAM_UNK2000		0x00002000
#define GXM_PROGRAM_UNK4000		0x00004000
#define GXM_PROGRAM_UNK8000		0x00008000	/* phases run even when blending keeps the colour */
#define GXM_PROGRAM_UNK10000		0x00010000	/* output written as declared */
#define GXM_PROGRAM_UNK80000		0x00080000

typedef struct SceGxmProgram {
	SceUInt32 magic;
	union {
		struct {
			SceUInt8 majorVersion;
			SceUInt8 minorVersion;
			SceUInt16 sdkVersion;
		};
		SceUInt32 version;
	};
	SceUInt32 size;
	SceUInt32 binaryGuid;
	SceUInt32 sourceGuid;
	SceUInt32 flags;			/* 0x14 GXM_PROGRAM_* */
	SceUInt32 bufferFlags;
	SceUInt32 texUnitFlags[2];
	SceUInt32 parameterCount;		/* 0x24 */
	SceInt32 parametersOffset;
	SceInt32 varyingsOffset;
	SceUInt16 primaryRegCount;		/* 0x30 */
	SceUInt16 secondaryRegCount;
	SceUInt16 tempRegCount;
	SceUInt16 reserved;
	SceUInt16 tempRegCount2;		/* 0x38 */
	SceUInt16 primaryPhaseCount;
	SceUInt32 primaryInstructionCount;	/* 0x3c */
	SceInt32 primaryProgramOffset;
	SceUInt32 secondaryInstructionCount;	/* 0x44 */
	SceInt32 secondaryProgramOffset;
	SceInt32 secondaryProgramEndOffset;
	SceUInt32 scratchBufferCount;		/* 0x50 */
	SceUInt32 threadBufferCount;
	SceUInt32 literalBufferCount;
	SceUInt32 dataBufferCount;
	SceUInt32 textureBufferCount;
	SceUInt32 defaultUniformBufferCount;	/* 0x64, in words */
	SceInt32 literalBufferOffset;		/* 0x68 */
	SceUInt32 reserved2;			/* cleared in a built program */
	SceUInt32 literalCount;			/* 0x70: GxmProgramLiteral */
	SceInt32 literalsOffset;
	SceUInt32 bufferRefCount;		/* 0x78: GxmProgramBufferRef */
	SceInt32 bufferRefsOffset;
	SceUInt32 constantTextureCount;		/* 0x80: GxmProgramTextureRef */
	SceInt32 constantTexturesOffset;
	SceUInt32 textureRefCount;		/* 0x88: GxmProgramTextureRef */
	SceInt32 textureRefsOffset;
	SceUInt32 secondaryPdsDataCount;	/* 0x90: GxmProgramDmaEntry */
	SceInt32 secondaryPdsDataOffset;
} SceGxmProgram;

GXM_ASSERT_OFFSET(SceGxmProgram, secondaryPdsDataOffset, 0x94);

/*
 * The tables a program's state blocks are written from. The constant block
 * (dataBufferCount words) takes literals, uniform buffer addresses and texture
 * words; the texture block (textureBufferCount words) texture words only.
 */
typedef struct GxmProgramLiteral {
	SceUInt16 dataWord;		/* the word of the constant block */
	SceUInt16 reserved;
	SceUInt32 value;
} GxmProgramLiteral;

typedef struct GxmProgramBufferRef {
	SceUInt16 bufferIndex;		/* the uniform buffer */
	SceUInt16 dataWord;
	SceUInt32 offset;		/* in bytes, added to its address */
} GxmProgramBufferRef;

typedef struct GxmProgramTextureRef {
	SceUInt16 textureWord;		/* the word of the context's textures */
	SceUInt16 dataWord;
} GxmProgramTextureRef;

/* a DMA of a uniform buffer into the secondary attribute registers */
typedef struct GxmProgramDmaEntry {
	SceUInt16 bufferIndex;
	SceUInt16 offset;		/* in words */
	SceUInt16 destination;		/* the first register */
	SceUInt16 size;			/* in words */
} GxmProgramDmaEntry;

/* the self-relative word at 0x4c: the program's entry, in instructions */
#define entryOffset	secondaryProgramEndOffset

/* the words after a program's code in USSE memory */
#define GXM_PROGRAM_TRAILER		0xdeadc0de
#define GXM_PROGRAM_TRAILER_VERTEX	0xa50c0001	/* also unblended fragment programs */
#define GXM_PROGRAM_TRAILER_BLEND	0xa50c0002

/* parameter info word */
#define GXM_PARAMETER_CATEGORY		0x0000000f
#define GXM_PARAMETER_TYPE		0x000000f0
#define GXM_PARAMETER_COMPONENT_COUNT	0x00000f00
#define GXM_PARAMETER_CONTAINER_INDEX	0x0000f000
#define GXM_PARAMETER_SEMANTIC		0x00ff0000
#define GXM_PARAMETER_SEMANTIC_INDEX	0x0f000000
#define GXM_PARAMETER_SAMPLER_CUBE	0x10000000

#define SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE	0
#define SCE_GXM_PARAMETER_CATEGORY_UNIFORM	1

/* SceGxmParameterType */
#define SCE_GXM_PARAMETER_TYPE_F32		0
#define SCE_GXM_PARAMETER_TYPE_F16		1
#define SCE_GXM_PARAMETER_TYPE_C10		2
#define SCE_GXM_PARAMETER_TYPE_U32		3
#define SCE_GXM_PARAMETER_TYPE_S32		4
#define SCE_GXM_PARAMETER_TYPE_U16		5
#define SCE_GXM_PARAMETER_TYPE_S16		6
#define SCE_GXM_PARAMETER_TYPE_U8		7
#define SCE_GXM_PARAMETER_TYPE_S8		8
#define SCE_GXM_PARAMETER_TYPE_AGGREGATE	9

typedef struct SceGxmProgramParameter {
	SceInt32 nameOffset;
	SceUInt32 info;
	SceUInt32 arraySize;
	SceUInt32 resourceIndex;
} SceGxmProgramParameter;

typedef struct GxmProgramFragmentInput {
	SceUInt32 word;			/* texture coordinate and varying selectors */
	SceUInt32 textureIndex;
	SceUInt32 iteratorBits;		/* OR'ed into the PDS iterate instruction */
	SceUInt32 samplerBits;		/* OR'ed into the PDS texture fetch */
} GxmProgramFragmentInput;

typedef struct GxmProgramVaryings {
	union {
		struct {
			SceUInt32 inputMask[2];
			SceUInt32 regFormatMask[2];
			SceUInt32 outputs;	/* bits 24..31: output registers */
			SceUInt32 texCoordOutputs;
			SceUInt32 outputs2;
			SceUInt16 instanceData;
			SceUInt16 indexData;
		} vertex;
		struct {
			SceUInt8 reserved[8];
			SceUInt16 outputRegister;	/* the output's primary register */
			SceUInt8 outputRegisterType;
			SceUInt8 outputComponentCount;
			SceUInt16 inputCount;
			SceUInt16 textureIndexCount;
			SceInt32 inputsOffset;
		} fragment;
	};
} GxmProgramVaryings;

static inline const GxmProgramVaryings *gxmProgramGetVaryings(const SceGxmProgram *program)
{
	return GXM_RELATIVE(program->varyingsOffset);
}

static inline const SceGxmProgramParameter *gxmProgramGetParameters(const SceGxmProgram *program)
{
	return GXM_RELATIVE(program->parametersOffset);
}

static inline SceUInt32 gxmParameterGetCategory(const SceGxmProgramParameter *parameter)
{
	return parameter->info & GXM_PARAMETER_CATEGORY;
}

static inline SceUInt32 gxmParameterGetSemantic(const SceGxmProgramParameter *parameter)
{
	return (parameter->info & GXM_PARAMETER_SEMANTIC) >> 16;
}

static inline SceUInt32 gxmParameterGetSemanticIndex(const SceGxmProgramParameter *parameter)
{
	return (parameter->info & GXM_PARAMETER_SEMANTIC_INDEX) >> 24;
}

int gxmStrcmp(const char *a, const char *b);

/* --- chunk buffers --- */

typedef void *GxmDeferredBufferCallback(void *userData, SceSize minSize, SceSize *size);

/* where a deferred context's buffers come from */
typedef struct GxmDeferredBuffer {
	GxmDeferredBufferCallback *callback;
	void *userData;
	SceUInt8 linked;		/* chunks end in a link to the next one */
} GxmDeferredBuffer;

/* a linked chunk keeps this much at its end for the link */
#define GXM_CHUNK_LINK_RESERVE	0x104

typedef struct GxmChunkBuffer GxmChunkBuffer;

struct GxmChunkBuffer {
	SceUInt32 cur;
	SceUInt32 end;
	SceUInt32 gpuEnd;		/* the GPU offset of end */
	void (*refill)(GxmChunkBuffer *buffer, SceSize size, SceSize alignment);
	union {
	GxmDeferredBuffer *source;	/* a deferred context's */
	struct GxmRingBuffer *ring;	/* an immediate context's */
	};
};

void *gxmChunkBufferReserveSlow(GxmChunkBuffer *buffer, SceSize size, SceSize alignment);
void gxmChunkBufferInitFixed(GxmChunkBuffer *buffer, void *memory, SceUInt32 gpuOffset, SceSize size);
void gxmChunkBufferInitDeferred(GxmDeferredBuffer *source, GxmChunkBuffer *buffer, void *memory,
				SceSize size, GxmDeferredBufferCallback *callback, void *userData,
				SceUInt8 linked);
void gxmChunkBufferReset(const GxmDeferredBuffer *source, GxmChunkBuffer *buffer, void *memory,
			 SceSize size);

/* aligned space for size bytes; the caller advances cur past what it writes */
static inline void *gxmChunkBufferReserve(GxmChunkBuffer *buffer, SceSize size, SceSize alignment)
{
	SceUInt32 block = (buffer->cur + alignment - 1) & ~(alignment - 1);

	if (buffer->end < block + size)
		return gxmChunkBufferReserveSlow(buffer, size, alignment);
	buffer->cur = block;
	return (void *)block;
}

/* --- GPU memory, code and command blocks --- */

/* where a block of USSE code or a command block sits */
typedef struct GxmCodeRef {
	SceUInt32 address;		/* gxmCodeRefSetAddress() */
	SceUInt32 taskControl;		/* the USSE task control word sent with the address */
} GxmCodeRef;

/* a command block written into a chunk buffer */
typedef struct GxmCommandBlock {
	void *data;
	SceUInt16 dataSize;		/* the PDS data, in bytes */
	SceUInt16 size;
} GxmCommandBlock;

/* code offset of the chunk buffer's write position */
static inline SceUInt32 gxmChunkBufferOffset(const GxmChunkBuffer *buffer)
{
	return buffer->cur + buffer->gpuEnd - buffer->end;
}

void gxmCodeRefSetAddress(GxmCodeRef *ref, SceUInt32 offset);
SceUInt32 gxmPdsDataWord(SceUInt32 a, SceUInt32 b, SceUInt32 c);
int gxmWriteProgramBlock(GxmChunkBuffer *buffer, const GxmCodeRef *program, GxmCommandBlock *block);
int gxmBuildFenceProgram(GxmChunkBuffer *buffer, GxmCodeRef *program, SceUInt32 fenceAddress);
SceSize gxmMteGetCopyProgramsSize(void);
int gxmMteBuildCopyPrograms(GxmCodeRef *programs, GxmChunkBuffer *buffer);
SceUInt32 *gxmUsseWriteHeader(SceUInt32 *code);
SceUInt32 *gxmUsseWriteOldHeader(SceUInt32 *code);
SceSize gxmUsseGetEmptyProgramSize(void);

int gxmUsseBuildEmptyProgram(GxmChunkBuffer *buffer, GxmCodeRef *program);

/* 64 slots of a GPU-visible block, one 16-byte record each */
typedef struct GxmSlotTable {
	PVRSRV_CLIENT_MEM_INFO *block;
	SceUInt32 used[2];
	void *items[64];
} GxmSlotTable;

int gxmSlotTableCreate(GxmSlotTable *table);
void *gxmSlotTableAdd(GxmSlotTable *table, void *item);
void gxmSlotTableRemove(GxmSlotTable *table, void *record);
void gxmSlotTableForEach(GxmSlotTable *table, void (*callback)(void *item));
int gxmSlotTableDestroy(GxmSlotTable *table);
void gxmWaitForValue(const PVRSRV_CLIENT_MEM_INFO *block, const volatile SceUInt32 *address,
		     SceUInt32 value);

typedef struct GxmDmsInfo {
	SceUInt8 tasks;
	SceUInt8 outputTasks;
	SceUInt8 padding[2];
	SceUInt32 word;
} GxmDmsInfo;

/* --- range heaps --- */

typedef struct GxmHeapNode {
	struct GxmHeapNode *next;
	SceUInt32 base;
	SceSize size;
} GxmHeapNode;

typedef GxmHeapNode *GxmHeapNodeAlloc(void *userData);
typedef void GxmHeapNodeFree(void *userData, GxmHeapNode *node);

typedef struct GxmHeap {
	void *userData;
	GxmHeapNodeAlloc *allocNode;
	GxmHeapNodeFree *freeNode;
	GxmHeapNode *used;
	GxmHeapNode *free;		/* sorted by base */
} GxmHeap;

void gxmHeapInit(GxmHeap *heap, void *userData, GxmHeapNodeAlloc *allocNode,
		 GxmHeapNodeFree *freeNode);
int gxmHeapAddRange(GxmHeap *heap, SceUInt32 base, SceSize size);
void gxmHeapClear(GxmHeap *heap);
SceUInt32 gxmHeapAlloc(GxmHeap *heap, SceSize size, SceSize alignment);
void gxmHeapFree(GxmHeap *heap, SceUInt32 base);
int gxmQueryHeaps(void);

/* --- PDS programs --- */

typedef struct SceGxmVertexAttribute {
	SceUInt16 streamIndex;
	SceUInt16 offset;
	SceUInt8 format;
	SceUInt8 componentCount;
	SceUInt16 regIndex;
} SceGxmVertexAttribute;

typedef struct SceGxmVertexStream {
	SceUInt16 stride;
	SceUInt16 indexSource;
} SceGxmVertexStream;

/* where an attribute's address goes in the PDS data */
typedef struct GxmPdsAttributePatch {
	SceUInt16 streamIndex;
	SceUInt16 offset;
	SceUInt32 dataWord;
} GxmPdsAttributePatch;

typedef struct GxmPdsState {
	void *code;
	SceUInt32 dataSize;		/* in 16-byte units */
	SceUInt32 flags;
} GxmPdsState;

uint32_t gxmAttributeFormatGetSize(SceUInt8 format);
SceSize gxmPdsVertexProgramSize(SceUInt32 attributeCount, const SceGxmVertexStream *streams,
				SceUInt32 streamCount, SceSize *dataSize, SceUInt8 instanced,
				SceUInt8 indexed);
int gxmPdsBuildVertexProgram(GxmChunkBuffer *buffer, const SceGxmVertexAttribute *attributes,
			     SceUInt32 attributeCount, const SceGxmVertexStream *streams,
			     SceUInt32 streamCount, const GxmCodeRef *program,
			     GxmCommandBlock *block, GxmPdsAttributePatch *patches,
			     SceUInt8 instanced, SceUInt8 indexed, SceUInt32 instanceData,
			     SceUInt32 indexData);
void gxmPdsPatchAttributes(SceUInt32 *data, const GxmPdsAttributePatch *patches, SceUInt32 count,
			   const SceUInt32 *streamAddresses);
void gxmPdsPatchAttributesPrecomputed(SceUInt32 *data, const GxmPdsAttributePatch *patches,
				      SceUInt32 count, const SceUInt32 *streamAddresses);
void gxmPdsPatchStream(SceUInt32 *data, const GxmPdsAttributePatch *patches, SceUInt32 count,
		       SceUInt32 streamIndex, SceUInt32 address);
SceSize gxmPdsFragmentProgramSize(const GxmProgramFragmentInput *inputs, SceUInt32 inputCount,
				  SceSize *dataSize);
int gxmPdsBuildFragmentProgram(GxmChunkBuffer *buffer, const SceGxmTexture *textures,
			       const GxmProgramFragmentInput *inputs, SceUInt32 inputCount,
			       const SceUInt8 *texCoordRemap, const GxmCodeRef *program,
			       GxmCommandBlock *block, SceUInt32 *textureIndices);
SceSize gxmPdsSimpleProgramSize(SceUInt32 count, SceSize *dataSize, SceSize unused);
int gxmPdsBuildSimpleProgram(GxmChunkBuffer *buffer, SceUInt32 value, SceUInt32 count,
			     const GxmCodeRef *program, GxmCommandBlock *block);
struct SceGxmColorSurface;
/* the surface's control and the six words after it are the PBE's sideband and emit words */
int gxmPdsBuildDoutProgram(GxmChunkBuffer *buffer, const struct SceGxmColorSurface *surface,
			   GxmCodeRef *program);
int gxmPdsBuildNopProgram(GxmChunkBuffer *buffer, GxmCodeRef *program);
int gxmPdsBuildKickProgram(GxmChunkBuffer *buffer, GxmCodeRef *program);
int gxmPdsBuildPixelProgram(GxmChunkBuffer *buffer, SceUInt32 samples, SceUInt32 mode,
			    const GxmCodeRef *first, const GxmCodeRef *second, const GxmCodeRef *third,
			    GxmCommandBlock *block, GxmPdsState *state);

/* --- USSE register budgets --- */

void gxmDmsVertexInfo(SceUInt32 primaryRegs, SceUInt32 secondaryRegs, SceUInt32 tempRegs,
			SceUInt32 outputRegs, SceUInt8 phased, GxmDmsInfo *info);
void gxmDmsFragmentInfo(SceUInt32 primaryRegs, SceUInt32 secondaryRegs, SceUInt32 tempRegs,
		      SceBool phased, SceUInt32 *word);

/* --- vertex attribute unpacking --- */

SceUInt32 gxmVertexUnpackSize(const SceGxmProgram *program, const SceGxmVertexAttribute *attributes,
			      SceUInt32 attributeCount);
int gxmVertexUnpackBuild(GxmChunkBuffer *buffer, const SceGxmProgram *program,
			 const SceGxmVertexAttribute *attributes, SceUInt32 attributeCount,
			 SceUInt32 *offset, SceUInt32 **codeOut);

/* --- program code --- */

#define SCE_GXM_ATTRIBUTE_FORMAT_UNTYPED_REG	10

SceUInt32 gxmProgramGetFragmentDms(const SceGxmProgram *program, SceBool phased, SceUInt32 samples);
SceSize gxmProgramGetVertexCodeSize(const SceGxmProgram *program,
				    const SceGxmVertexAttribute *attributes, SceUInt32 attributeCount);
SceSize gxmProgramGetSecondaryCodeSize(const SceGxmProgram *program);
int gxmProgramBuildSecondaryCode(GxmChunkBuffer *buffer, const SceGxmProgram *program,
				 GxmCodeRef *ref);
int gxmProgramBuildVertexCode(GxmChunkBuffer *buffer, const SceGxmProgram *program,
			      const SceGxmVertexAttribute *attributes, SceUInt32 attributeCount,
			      GxmCodeRef *ref);

/* --- fragment outputs --- */

SceBool gxmOutputIsWide(SceUInt32 type, SceUInt32 componentCount);
int gxmOutputFormatResolve(SceUInt32 format, SceUInt32 *type, SceUInt32 *componentCount,
			   SceUInt32 *mode);
SceUInt32 *gxmOutputFormatWriteConversion(SceUInt32 *code, SceUInt32 reg, SceUInt32 format,
					  SceUInt32 *type, SceUInt32 *componentCount,
					  SceUInt8 secondary);
SceUInt32 *gxmUsseWriteOutputMove(SceUInt32 *code, SceUInt32 reg, SceBool wide);
void gxmCopyWords(SceUInt32 *dest, const SceUInt32 *src, SceSize size);
void gxmPdsWriteStateWords(SceUInt32 *words, const GxmCommandBlock *vertex, SceUInt32 flags,
			   const GxmCommandBlock *fragment);
int gxmCommandBlockCopy(GxmChunkBuffer *buffer, const GxmCommandBlock *source,
			GxmCommandBlock *copy);

#define GXM_PDS_DMA_CONTROL	0x07000003
#define GXM_PDS_DMA_END		0x070001a5
#define GXM_PDS_HALT		0xaf000000

/*
 * The secondary PDS program, from 16-byte aligned memory: one DMA per uniform
 * buffer range the program reads, then the secondary program's address and task
 * control. Returns the end of what it wrote.
 */
static inline SceUInt32 *gxmProgramWriteDmaBlock(SceUInt32 *data, GxmCommandBlock *dmaBlock,
						 const SceGxmProgram *program,
						 const void *const *uniformBuffers,
						 const GxmCodeRef *secondaryCode)
{
	const GxmProgramDmaEntry *entries;
	SceUInt32 *control, *code, count, controlOffset, i;

	data = (SceUInt32 *)(((SceUInt32)data + 15) & ~15);
	entries = GXM_RELATIVE(program->secondaryPdsDataOffset);
	count = program->secondaryPdsDataCount;
	dmaBlock->data = data;
	controlOffset = ((count * 2 + 3) & ~3) + 4;
	dmaBlock->dataSize = controlOffset * 4;
	control = data + controlOffset;
	dmaBlock->size = (count + 2 + controlOffset) * 4;
	for (i = 0; i < count; i++) {
		data[0] = (SceUInt32)uniformBuffers[entries[i].bufferIndex] + entries[i].offset * 4;
		data[1] = (entries[i].size - 1) | entries[i].destination << 8;
		*control++ = (i >> 1) << 12 | (i >> 1) << 18 | GXM_PDS_DMA_CONTROL |
			     (i & 1) << 11 | ((i & 1) * 2 | 1) << 8;
		data += 2;
	}
	code = (SceUInt32 *)(((SceUInt32)data + 15) & ~15);
	code[0] = secondaryCode->address;
	code[1] = secondaryCode->taskControl;
	code[2] = 0;
	control[0] = ((count + 1) >> 1) << 12 | ((count + 1) >> 1) << 18 | GXM_PDS_DMA_END;
	control[1] = GXM_PDS_HALT;
	return control + 2;
}

/* --- VDM commands --- */

int gxmVdmWriteFragmentState(GxmChunkBuffer *buffer, SceSize size, const GxmCommandBlock *block,
			     SceBool secondary);
int gxmVdmWriteVertexState(GxmChunkBuffer *buffer, const GxmCommandBlock *block, SceSize size,
			   SceBool secondary);
int gxmVdmWritePrimitive(GxmChunkBuffer *buffer, const GxmCommandBlock *block,
			 const SceUInt8 *programInfo, SceUInt32 primitive, SceUInt32 indexWord,
			 SceUInt32 count, SceUInt32 flags, SceUInt32 indexAddress, SceUInt8 wordBits);
int gxmVdmWriteFence(GxmChunkBuffer *buffer, const GxmCommandBlock *block);
int gxmVdmWriteTerminate(GxmChunkBuffer *buffer, const GxmCommandBlock *block);
int gxmVdmWriteLink(GxmChunkBuffer *buffer, SceUInt32 address, const SceUInt32 *returnWords);

/* --- contexts --- */

#define SCE_GXM_CONTEXT_TYPE_IMMEDIATE	0
#define SCE_GXM_CONTEXT_TYPE_DEFERRED	1

typedef enum SceGxmYuvProfile {
	SCE_GXM_YUV_PROFILE_BT601_STANDARD,
	SCE_GXM_YUV_PROFILE_BT709_STANDARD,
	SCE_GXM_YUV_PROFILE_BT601_FULL_RANGE,
	SCE_GXM_YUV_PROFILE_BT709_FULL_RANGE
} SceGxmYuvProfile;

/* SceGxmContext.dirty: state words to send with the next draw */
#define GXM_DIRTY_FRONT_ISP		0x00000001
#define GXM_DIRTY_FRONT_BIAS		0x00000002
#define GXM_DIRTY_FRONT_STENCIL		0x00000004
#define GXM_DIRTY_BACK_ISP		0x00000008
#define GXM_DIRTY_BACK_BIAS		0x00000010
#define GXM_DIRTY_BACK_STENCIL		0x00000020
#define GXM_DIRTY_PDS_STATE		0x00000040
#define GXM_DIRTY_REGION_CLIP		0x00000080
#define GXM_DIRTY_VIEWPORT		0x00000100
#define GXM_DIRTY_RESERVED0		0x00000200
#define GXM_DIRTY_VERTEX_OUTPUTS	0x00000400
#define GXM_DIRTY_W_CLAMP_VALUE		0x00000800
#define GXM_DIRTY_CULL			0x00001000
#define GXM_DIRTY_RESERVED1		0x00002000
#define GXM_DIRTY_TEXCOORD_OUTPUTS	0x00004000
#define GXM_DIRTY_VERTEX_OUTPUTS2	0x00008000

/* SceGxmContext.flags: what gxmContextWriteState writes again before the next draw */
#define GXM_CONTEXT_FLAG_BACKGROUND		0x00000001	/* the background programs are to be built */
#define GXM_CONTEXT_FLAG_FRAGMENT_TEXTURES	0x00000002	/* the texture block */
#define GXM_CONTEXT_FLAG_FRAGMENT_CONSTANTS	0x00000004	/* the constant block */
#define GXM_CONTEXT_FLAG_FRAGMENT_DATA		0x00000008	/* the uniform buffers' DMA list */
#define GXM_CONTEXT_FLAG_FRAGMENT_TEXTURE_STATE	0x00000010	/* the texture state block */
#define GXM_CONTEXT_FLAG_FRAGMENT_PROGRAM	0x00000020	/* the PDS state words */
#define GXM_CONTEXT_FLAG_FRAGMENT_ALL		0x0000003e
#define GXM_CONTEXT_FLAG_VERTEX_TEXTURES	0x00000040
#define GXM_CONTEXT_FLAG_VERTEX_CONSTANTS	0x00000080
#define GXM_CONTEXT_FLAG_VERTEX_DATA		0x00000100
#define GXM_CONTEXT_FLAG_VERTEX_PROGRAM		0x00000200	/* the vertex state in the VDM stream */
#define GXM_CONTEXT_FLAG_VERTEX_ALL		0x000003c0
#define GXM_CONTEXT_FLAG_VERTEX_STREAMS		0x00000400	/* a stream the vertex program reads changed */
#define GXM_CONTEXT_FLAG_DEPTH_STENCIL_USED	0x00000800	/* the depth/stencil modes changed */

/* a buffer the DMA list reads changed; one the constant block reads, too */
#define GXM_CONTEXT_FLAG_FRAGMENT_DMA_UPDATE \
	(GXM_CONTEXT_FLAG_FRAGMENT_DATA | GXM_CONTEXT_FLAG_FRAGMENT_PROGRAM)
#define GXM_CONTEXT_FLAG_FRAGMENT_CONSTANT_UPDATE \
	(GXM_CONTEXT_FLAG_FRAGMENT_CONSTANTS | GXM_CONTEXT_FLAG_FRAGMENT_DMA_UPDATE)
#define GXM_CONTEXT_FLAG_VERTEX_DMA_UPDATE \
	(GXM_CONTEXT_FLAG_VERTEX_DATA | GXM_CONTEXT_FLAG_VERTEX_PROGRAM)
#define GXM_CONTEXT_FLAG_VERTEX_CONSTANT_UPDATE \
	(GXM_CONTEXT_FLAG_VERTEX_CONSTANTS | GXM_CONTEXT_FLAG_VERTEX_DMA_UPDATE)

/* ISP control word (front and back) */
#define GXM_ISP_STENCIL_REF		0x000000ff
#define GXM_ISP_STENCIL_ENABLE		0x00000100
#define GXM_ISP_TWO_SIDED		0x00000800
#define GXM_ISP_VISIBILITY_INDEX_HIGH	0x00003000
#define GXM_ISP_VISIBILITY_ENABLE	0x00004000
#define GXM_ISP_POLYGON_MODE		0x00038000
#define GXM_ISP_VISIBILITY_OP		0x00040000
#define GXM_ISP_LINE_FILL_LAST_PIXEL	0x00080000
#define GXM_ISP_DEPTH_WRITE_DISABLE	0x00100000
#define GXM_ISP_FRAGMENT_PROGRAM_DISABLE	0x00200000
#define GXM_ISP_DEPTH_FUNC		0x01c00000
#define GXM_ISP_PASS_TYPE		0x0e000000
#define GXM_ISP_FRAGMENT_PROGRAM_MASK	0x0e200000	/* bits the fragment program sets */
#define GXM_ISP_POINT_LINE_WIDTH_SHIFT	28
#define GXM_ISP_POINT_LINE_WIDTH	0xf0000000
#define GXM_ISP_DEPTH_FUNC_ALWAYS	0x01c00000	/* depth funcs NEVER (0) and ALWAYS read no depth */
#define GXM_ISP_PASS_TYPE_MASK_UPDATE	0x06000000

/* stencil word (front and back) */
#define GXM_STENCIL_FUNC		0x0e000000
#define GXM_STENCIL_FUNC_ALWAYS		0x0e000000	/* funcs NEVER (0) and ALWAYS read no stencil */

/* bias word */
#define GXM_BIAS_VISIBILITY_INDEX	0x00000fff
#define GXM_BIAS_UNITS_SHIFT		16
#define GXM_BIAS_SLOPE_SHIFT		21
#define GXM_BIAS_MASK			0x03ff0000

/* cull word */
#define GXM_CULL_MODE			0x00000003
#define GXM_CULL_W_BUFFER		0x00004000
#define GXM_CULL_W_CLAMP		0x00008000
#define GXM_CULL_VIEWPORT_DISABLE	0x00010000

struct SceGxmShaderPatcher;
struct SceGxmRegisteredProgram;

typedef struct SceGxmFragmentProgram {
	const SceGxmProgram *programHeader;
	GxmCodeRef code;		/* 0x04: the program with output and blending */
	SceUInt32 reserved0;
	GxmCodeRef secondaryCode;	/* 0x10 */
	SceUInt32 reserved1;
	SceUInt32 pdsFlags;		/* 0x1c */
	SceUInt32 ispFlags;		/* ISP control bits of the program */
	SceUInt32 dms;			/* 0x24: gxmProgramGetFragmentDms() */
	GxmCommandBlock pds;		/* 0x28: the PDS program feeding the inputs */
	SceSize secondaryPdsSize;	/* 0x30 */
	SceUInt32 textureIndexCount;
	SceUInt32 *textureIndices;	/* 0x38 */
	void *literals;			/* 0x3c: the registered program's buffers */
	void *scratch;
	void *thread;
	SceUInt32 passFlags;		/* 0x48 */
	SceSize hostSize;		/* 0x4c */
	void *usseMem;
	SceSize usseSize;
	SceUInt8 texCoordRemap[10];	/* 0x58: vertex texture coordinates by fragment input */
	SceUInt8 padding[2];
	SceUInt32 outputFormat;		/* 0x64 */
	struct SceGxmBlendInfo *blendInfo;
	SceUInt32 multisampleMode;	/* 0x6c */
	SceUInt32 refCount;		/* 0x70 */
	struct SceGxmShaderPatcher *patcher;
	struct SceGxmRegisteredProgram *registered;
	struct SceGxmFragmentProgram *next;	/* in the registered program's list */
	/* then textureIndices[textureIndexCount], the blend info and the PDS program */
} SceGxmFragmentProgram;

_Static_assert(sizeof(SceGxmFragmentProgram) == 0x80, "SceGxmFragmentProgram size");

SceUInt32 sceGxmFragmentProgramGetPassType(const SceGxmFragmentProgram *fragmentProgram);

typedef struct SceGxmVertexProgram {
	const SceGxmProgram *programHeader;
	GxmCodeRef code;		/* 0x04: unpack code, then the program */
	SceUInt32 reserved0;
	GxmCodeRef secondaryCode;	/* 0x10 */
	SceUInt32 reserved1;
	SceUInt32 pdsFlags;		/* 0x1c */
	GxmCommandBlock pds;		/* 0x20 */
	SceSize secondaryPdsSize;	/* 0x28 */
	SceUInt32 streamMask;		/* streams the attributes use */
	GxmPdsAttributePatch *patches;	/* 0x30 */
	SceUInt32 patchCount;
	void *literals;			/* 0x38: the registered program's buffers */
	void *scratch;
	void *thread;
	GxmDmsInfo dms;			/* 0x44 */
	SceSize hostSize;		/* 0x4c */
	void *usseMem;
	SceSize usseSize;
	SceGxmVertexAttribute *attributes;	/* 0x58 */
	SceUInt32 attributeCount;
	SceGxmVertexStream *streams;	/* 0x60 */
	SceUInt32 streamCount;
	SceUInt32 refCount;		/* 0x68 */
	struct SceGxmShaderPatcher *patcher;
	struct SceGxmRegisteredProgram *registered;
	struct SceGxmVertexProgram *next;	/* in the registered program's list */
} SceGxmVertexProgram;

_Static_assert(sizeof(SceGxmVertexProgram) == 0x78, "SceGxmVertexProgram size");

/*
 * The render state the GPU's MTE takes, one word per dirty bit group;
 * a state update sends the dirty mask followed by the dirty words.
 */
typedef struct GxmMteState {
	SceUInt32 dirty;		/* GXM_DIRTY_* */
	SceUInt32 frontIsp;
	SceUInt32 frontBias;
	SceUInt32 frontStencil;
	SceUInt32 backIsp;
	SceUInt32 backBias;
	SceUInt32 backStencil;
	SceUInt32 pdsState[3];		/* GXM_DIRTY_PDS_STATE: gxmPdsWriteStateWords */
	SceUInt32 regionClip[2];
	SceUInt32 viewport[6];		/* x/y/z offset and scale, float bits */
	SceUInt32 reserved0;		/* GXM_DIRTY_RESERVED0: only ever cleared */
	SceUInt32 vertexOutputs;
	SceUInt32 wClampValue;		/* float bits */
	SceUInt32 cull;
	SceUInt32 reserved1;		/* GXM_DIRTY_RESERVED1: never set */
	SceUInt32 texCoordOutputs;
	SceUInt32 vertexOutputs2;
} GxmMteState;

void gxmMteStateInvalidate(GxmMteState *state);
void gxmMteStateInit(GxmMteState *state);
int gxmMteWriteStateWords(GxmChunkBuffer *buffer, const GxmMteState *state, SceUInt32 **words,
			  SceUInt32 *count);
int gxmMteWriteUpdateBlock(GxmChunkBuffer *buffer, SceUInt32 stateWords, SceUInt32 count,
			   const GxmCodeRef *copyProgram, GxmCommandBlock *block);
const GxmCodeRef *gxmMteGetCopyProgram(const GxmCodeRef *programs, SceUInt32 count);
int gxmMteWriteRegionBlock(GxmChunkBuffer *buffer, const GxmCodeRef *program, SceUInt32 xMin,
			   SceUInt32 yMin, SceUInt32 xMax, SceUInt32 yMax, GxmCommandBlock *block);


/*
 * An immediate context's ring buffer (sceGxmCreateContext sets one up for each chunk
 * buffer, and the chunk buffer's refill waits on it).
 */
typedef struct GxmRingBuffer {
	SceUInt32 memory;
	SceUInt32 gpuOffset;
	SceSize size;
	SceSize usableSize;		/* size less GXM_CHUNK_LINK_RESERVE when linked */
	SceSize halfSize;		/* half the usable size */
	SceUInt8 linked;
	SceUInt32 threshold;		/* memory + halfSize at first; the refill moves it */
	volatile SceUInt32 *readOffset;	/* the GPU's position, in the context's slot */
	SceUInt32 lastOffset;		/* the refill measures from it; 0 at first */
} GxmRingBuffer;

void gxmRingBufferInit(GxmRingBuffer *ring, GxmChunkBuffer *buffer, void *memory,
		       SceUInt32 gpuOffset, SceSize size, SceUInt8 linked,
		       volatile SceUInt32 *readOffset);
void gxmRingBufferKicked(GxmRingBuffer *ring, GxmChunkBuffer *buffer);

/* --- surfaces --- */

typedef struct SceGxmColorSurface {
	SceUInt32 control;
	SceUInt32 format;
	SceUInt32 data;
	SceUInt32 stride;
	SceUInt32 reserved;		/* pbeEmitWords[3], always 0 */
	SceUInt32 clipMin;
	SceUInt32 clipMax;
	SceUInt32 outputRegisterSize;
	SceGxmTexture backgroundTex;
} SceGxmColorSurface;

typedef struct SceGxmDepthStencilSurface {
	SceUInt32 zlsControl;
	void *depthData;
	void *stencilData;
	float backgroundDepth;
	SceUInt32 backgroundControl;
} SceGxmDepthStencilSurface;

#define SCE_GXM_COLOR_SURFACE_SCALE_NONE		0
#define SCE_GXM_COLOR_SURFACE_SCALE_MSAA_DOWNSCALE	1

SceBool sceGxmColorSurfaceIsEnabled(const SceGxmColorSurface *surface);
SceGxmColorFormat sceGxmColorSurfaceGetFormat(const SceGxmColorSurface *surface);

/* the background object's programs (background.c) */
int gxmBackgroundBuildProgram(GxmChunkBuffer *buffer, const SceGxmColorSurface *surface,
			      SceUInt32 samples, GxmCodeRef *program, SceUInt32 *dms);
int gxmBackgroundBuildTextureProgram(GxmChunkBuffer *buffer, const SceGxmColorSurface *surface,
				     SceUInt32 samples, const GxmCodeRef *program,
				     GxmCommandBlock *block);
int gxmBackgroundBuildFilterProgram(GxmChunkBuffer *buffer, const SceGxmColorSurface *surface,
				    SceUInt32 width, SceUInt32 height, SceUInt32 samples,
				    const GxmCodeRef *program, GxmCommandBlock *block);
int sceGxmColorSurfaceInitDisabled(SceGxmColorSurface *surface);
SceUInt32 sceGxmColorSurfaceGetScaleMode(const SceGxmColorSurface *surface);
void *sceGxmColorSurfaceGetData(const SceGxmColorSurface *surface);
SceBool sceGxmDepthStencilSurfaceIsEnabled(const SceGxmDepthStencilSurface *surface);
SceUInt32 sceGxmDepthStencilSurfaceGetStrideInSamples(const SceGxmDepthStencilSurface *surface);
SceUInt32 sceGxmTextureGetWidth(const SceGxmTexture *texture);
SceUInt32 sceGxmTextureGetHeight(const SceGxmTexture *texture);

/* --- scenes --- */

#define SCE_GXM_SCENE_FRAGMENT_SET_DEPENDENCY		0x00000001U
#define SCE_GXM_SCENE_VERTEX_WAIT_FOR_DEPENDENCY	0x00000002U
#define SCE_GXM_SCENE_FRAGMENT_TRANSFER_SYNC		0x00000004U
#define SCE_GXM_SCENE_VERTEX_TRANSFER_SYNC		0x00000008U
#define GXM_SCENE_FLAGS		0x0000000fU

typedef struct SceGxmValidRegion {
	SceUInt32 xMax;
	SceUInt32 yMax;
} SceGxmValidRegion;

/* the region _sceGxmBeginScene (the older export) takes: the maximum after the minimum */
typedef struct GxmValidRegionWithMin {
	SceUInt32 xMin;
	SceUInt32 yMin;
	SceGxmValidRegion max;
} GxmValidRegionWithMin;

struct SceGxmRenderTarget;
struct SceGxmSyncObject;
struct GxmTopContext;

/* what libgpu_es4 reports for a render target */
typedef struct GxmRenderTargetInfo {
	SceUInt32 width;
	SceUInt32 height;
	SceUInt16 samplesX;		/* multisampling: samples per pixel across */
	SceUInt16 samplesY;		/* and down */
	SceUInt32 wholeRegion;		/* the valid region must be the whole target */
} GxmRenderTargetInfo;

int SceGpuEs4User_83D972DF(struct GxmTopContext *top, const struct SceGxmRenderTarget *renderTarget,
			   GxmRenderTargetInfo *info);	/* the render target's size */
int SceGpuEs4User_F0CCE9DB(struct GxmTopContext *top, const SGX_KICKTA *kick,
			   SGX_KICKTA_OUTPUT *output);	/* kick a scene */

/*
 * The scene's depth/stencil state, from the load and store surfaces of
 * sceGxmBeginSceneEx (sceGxmBeginScene passes one surface as both).
 */
typedef struct GxmSceneDepthStencil {
	SceUInt32 zlsControl;		/* the store surface's, with the load surface's load bits */
	void *loadDepthData;
	void *loadStencilData;
	void *storeDepthData;
	void *storeStencilData;
	float backgroundDepth;
	SceUInt32 backgroundControl;
} GxmSceneDepthStencil;

/*
 * Scene parameters the context's setters store between scenes and
 * sceGxmBeginScene takes over, in two parts it copies separately. The
 * SceGxmInternalForTest setters (unnamed exports) set most of them.
 */
typedef struct GxmSceneTuning {
	SceUInt32 option;		/* C65CED80: 0 or 0x80 */
	SceUInt16 limit;		/* 65FF05BC: 1..0x7fff */
	SceUInt16 count;		/* 65FF05BC: 0..16 */
} GxmSceneTuning;

typedef struct GxmSceneSettings {
	SceUInt32 offsets;		/* 46D6C306: two signed bytes */
	SceUInt32 mode;			/* D352F90A */
	SceUInt32 modeValue;
	void *visibilityBuffer;		/* sceGxmSetVisibilityBuffer */
	SceUInt32 visibilityStride;	/* per core */
	SceUInt32 yuvProfile[2];	/* sceGxmSetYuvProfile */
} GxmSceneSettings;

typedef struct GxmSceneParams {
	GxmSceneTuning tuning;
	GxmSceneSettings settings;
} GxmSceneParams;

/* a VDM state command, as gxmVdmWriteVertexState and gxmVdmWriteFragmentState write it */
typedef struct GxmVdmState {
	SceUInt32 words[2];
} GxmVdmState;

/*
 * The SDK's opaque precomputed states (SCE_GXM_PRECOMPUTED_*_STATE_WORD_COUNT
 * words): a program's blocks, written once into the caller's memory block.
 */
typedef struct SceGxmPrecomputedFragmentState {
	const SceGxmFragmentProgram *program;
	void *textureStateBlock;	/* the memory block: the PDS program with the textures */
	void *dmaBlock;			/* the uniform buffers' DMA list */
	const void *defaultUniformBuffer;
	void *constantBlock;
	void *textureBlock;
	SceUInt32 pdsState[3];		/* gxmPdsWriteStateWords */
} SceGxmPrecomputedFragmentState;

typedef struct SceGxmPrecomputedVertexState {
	const SceGxmVertexProgram *program;
	void *dmaBlock;			/* the memory block */
	const void *defaultUniformBuffer;
	void *constantBlock;
	void *textureBlock;
	GxmVdmState vdmState;		/* gxmVdmWriteVertexState */
} SceGxmPrecomputedVertexState;

_Static_assert(sizeof(SceGxmPrecomputedFragmentState) == 9 * 4, "SceGxmPrecomputedFragmentState size");
_Static_assert(sizeof(SceGxmPrecomputedVertexState) == 7 * 4, "SceGxmPrecomputedVertexState size");

/* a VDM primitive command, as gxmVdmWritePrimitive writes it */
typedef struct GxmVdmPrimitive {
	SceUInt32 words[6];
} GxmVdmPrimitive;

/* the SDK's opaque SceGxmPrecomputedDraw (SCE_GXM_PRECOMPUTED_DRAW_WORD_COUNT words) */
typedef struct SceGxmPrecomputedDraw {
	const SceGxmVertexProgram *vertexProgram;
	GxmCommandBlock streamBlock;	/* the vertex program's PDS block, streams patched in */
	SceUInt32 reserved[2];
	GxmVdmPrimitive primitive;
} SceGxmPrecomputedDraw;

_Static_assert(sizeof(SceGxmPrecomputedDraw) == 11 * 4, "SceGxmPrecomputedDraw size");
struct SceGxmRenderTarget;

/*
 * The default uniform buffer modes (fragmentDefaultUniformBufferMode,
 * vertexDefaultUniformBufferMode): a program change clears all but SET.
 */
#define GXM_DEFAULT_UNIFORM_BUFFER_NONE		0
#define GXM_DEFAULT_UNIFORM_BUFFER_RESERVED	1	/* sceGxmReserve*DefaultUniformBuffer */
#define GXM_DEFAULT_UNIFORM_BUFFER_USED		2	/* a draw used the reserved one */
#define GXM_DEFAULT_UNIFORM_BUFFER_SET		3	/* sceGxmSet*DefaultUniformBuffer */

typedef struct SceGxmContext {
	SceUInt32 type;			/* SCE_GXM_CONTEXT_TYPE_* */
	SceUInt32 flags;		/* GXM_CONTEXT_FLAG_* */
	GxmChunkBuffer vdmBuffer;	/* 0x008 */
	GxmChunkBuffer vertexBuffer;	/* 0x01c */
	GxmChunkBuffer fragmentBuffer;	/* 0x030 */

	/* the fragment stage */
	const SceGxmFragmentProgram *fragmentProgram;	/* 0x044 */
	SceUInt32 fragmentDefaultUniformBufferMode;
	const void *fragmentUniformBuffers[15];		/* 0x04c: [14] the default one */
	void *fragmentTextureBlock;	/* 0x088: the program's texture words, per draw */
	SceUInt32 fragmentProgramWords[3];		/* the program's state words */
	void *fragmentConstantBlock;	/* 0x098: buffer addresses, literals, texture words */
	const void *fragmentDefaultUniformBuffer;
	SceGxmTexture fragmentTextures[16];		/* 0x0a0 */
	const struct SceGxmPrecomputedFragmentState *precomputedFragmentState;	/* 0x1a0 */
	const struct SceGxmPrecomputedFragmentState *appliedFragmentState;	/* last written */
	GxmCommandBlock fragmentDmaBlock;	/* 0x1a8: the uniform buffers' DMA list */
	GxmCommandBlock fragmentTextureStateBlock;
	SceUInt32 frontFragmentProgramMode;
	SceUInt32 backFragmentProgramMode;
	GxmMteState state;		/* 0x1c0 */

	/* the vertex stage */
	const SceGxmVertexProgram *vertexProgram;	/* 0x224 */
	SceUInt32 vertexDefaultUniformBufferMode;
	const void *vertexUniformBuffers[15];		/* 0x22c: [14] the default one */
	void *vertexTextureBlock;	/* 0x268 */
	SceUInt32 vertexProgramWords[3];
	void *vertexConstantBlock;	/* 0x278 */
	const void *vertexDefaultUniformBuffer;
	const void *vertexStreams[16];			/* 0x280 */
	SceGxmTexture vertexTextures[16];		/* 0x2c0 */
	const struct SceGxmPrecomputedVertexState *precomputedVertexState;	/* 0x3c0 */
	const struct SceGxmPrecomputedVertexState *appliedVertexState;
	GxmCommandBlock vertexDmaBlock;	/* 0x3c8 */
	GxmCommandBlock vertexStreamBlock;	/* 0x3d0: the streams' descriptors */

	SceUInt8 insideScene;		/* 0x3d8: in a scene, or (deferred) in a command list */
	union {
	struct {			/* SCE_GXM_CONTEXT_TYPE_IMMEDIATE */
	volatile SceUInt32 *slot;	/* 0x3dc: its slot table slot: the rings' read offsets */
	GxmRingBuffer vdmRing;		/* 0x3e0 */
	GxmRingBuffer vertexRing;	/* 0x404 */
	GxmRingBuffer fragmentRing;	/* 0x428 */
	GxmChunkBuffer fragmentUsseBuffer;	/* 0x44c */
	GxmRingBuffer fragmentUsseRing;	/* 0x460 */
	GxmSceneParams scene;		/* 0x484: the scene's, from nextScene at its beginning */
	SceUInt32 sceneFlags;		/* 0x4a8: sceGxmBeginScene's flags */
	const struct SceGxmRenderTarget *renderTarget;
	SceUInt32 tileLimitX;		/* 0x4b0: the valid region's maximum */
	SceUInt32 tileLimitY;		/* 0x4b4: immediate contexts clip the region to it */
	SceUInt32 colorWidth;		/* 0x4b8: the color surface's size, 0 when disabled */
	SceUInt32 colorHeight;
	struct SceGxmSyncObject *fragmentSyncObject;	/* 0x4c0 */
	SceGxmColorSurface colorSurface;	/* 0x4c4 */
	GxmSceneDepthStencil depthStencil;	/* 0x4f4 */
	SceUInt32 sampleCount;		/* 0x510: the render target's samples per pixel */
	SceUInt32 backgroundState[3];	/* 0x514: the background object's PDS state words */
	GxmPdsState pixelState;		/* 0x520: the end-of-tile pixel program */
	SceUInt8 lastKickVertexOnly;	/* 0x52c: the last kick left the fragment part out */
	SceUInt8 padding[3];
	GxmSceneParams nextScene;	/* 0x530: set between scenes */
	};
	struct {			/* SCE_GXM_CONTEXT_TYPE_DEFERRED */
	GxmDeferredBuffer vdmSource;	/* 0x3dc: where each buffer's next chunk comes from */
	GxmDeferredBuffer vertexSource;
	GxmDeferredBuffer fragmentSource;
	SceUInt32 *listStart;		/* 0x400: the command list's first VDM word */
	SceUInt32 listDepthStencilUsage;	/* GXM_LIST_USES_*, from the draws' depth/stencil modes */
	SceUInt32 listTileYMax;		/* 0x408: the lowest tile row a region clip reached */
	SceUInt32 listSerial;		/* 0x40c: gxmCounterIncrement() at creation */
	};
	};
} SceGxmContext;

GXM_ASSERT_OFFSET(SceGxmContext, state, 0x1c0);
GXM_ASSERT_OFFSET(SceGxmContext, tileLimitY, 0x4b4);
GXM_ASSERT_OFFSET(SceGxmContext, fragmentProgram, 0x44);
GXM_ASSERT_OFFSET(SceGxmContext, fragmentTextures, 0xa0);
GXM_ASSERT_OFFSET(SceGxmContext, frontFragmentProgramMode, 0x1b8);
GXM_ASSERT_OFFSET(SceGxmContext, vertexProgram, 0x224);
GXM_ASSERT_OFFSET(SceGxmContext, vertexStreams, 0x280);
GXM_ASSERT_OFFSET(SceGxmContext, vertexTextures, 0x2c0);
GXM_ASSERT_OFFSET(SceGxmContext, insideScene, 0x3d8);
GXM_ASSERT_OFFSET(SceGxmContext, fragmentUsseRing, 0x460);
GXM_ASSERT_OFFSET(SceGxmContext, sceneFlags, 0x4a8);
GXM_ASSERT_OFFSET(SceGxmContext, colorSurface, 0x4c4);
GXM_ASSERT_OFFSET(SceGxmContext, depthStencil, 0x4f4);
GXM_ASSERT_OFFSET(SceGxmContext, nextScene, 0x530);
_Static_assert(sizeof(SceGxmContext) == 0x554, "SceGxmContext size");
GXM_ASSERT_OFFSET(SceGxmContext, vdmSource, 0x3dc);
GXM_ASSERT_OFFSET(SceGxmContext, listStart, 0x400);
GXM_ASSERT_OFFSET(SceGxmContext, listSerial, 0x40c);

/* a deferred context uses only the memory up to its own fields */
#define GXM_DEFERRED_CONTEXT_SIZE	0x410

/* the depth/stencil state a command list's draws use (listDepthStencilUsage) */
#define GXM_LIST_USES_DEPTH_FRONT	0x00000001
#define GXM_LIST_USES_DEPTH_BACK	0x00000002
#define GXM_LIST_USES_STENCIL_FRONT	0x00000004
#define GXM_LIST_USES_STENCIL_BACK	0x00000008
#define GXM_LIST_USES_MASK		0x00000010
#define GXM_LIST_USES_ALL		0x0000001f

/* the scene's ZLS control (depthStencil.zlsControl) */
#define GXM_ZLS_LOAD_STENCIL		0x00002000
#define GXM_ZLS_LOAD_DEPTH		0x00004000
#define GXM_ZLS_LOAD_MASK		0x00008000
#define GXM_ZLS_STORE_STENCIL		0x00020000
#define GXM_ZLS_STORE_DEPTH		0x00040000
#define GXM_ZLS_STORE_MASK		0x00080000
#define GXM_ZLS_LOAD_CONTROL		0x00e0f002	/* taken from the load surface */
#define GXM_ZLS_STORE_CONTROL		0xff1f0ffd	/* and the rest from the store surface */

#define SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE		2048

/* sceGxmCreateContext's smallest ring buffers (no SDK name) */
#define GXM_MINIMUM_VDM_RING_BUFFER_SIZE		0x1000
#define GXM_MINIMUM_VERTEX_RING_BUFFER_SIZE		0x8000
#define GXM_MINIMUM_FRAGMENT_RING_BUFFER_SIZE		0x8000
#define GXM_MINIMUM_FRAGMENT_USSE_RING_BUFFER_SIZE	0x1000

typedef struct SceGxmContextParams {
	void *hostMem;
	uint32_t hostMemSize;
	void *vdmRingBufferMem;
	uint32_t vdmRingBufferMemSize;
	void *vertexRingBufferMem;
	uint32_t vertexRingBufferMemSize;
	void *fragmentRingBufferMem;
	uint32_t fragmentRingBufferMemSize;
	void *fragmentUsseRingBufferMem;
	uint32_t fragmentUsseRingBufferMemSize;
	uint32_t fragmentUsseRingBufferOffset;
} SceGxmContextParams;

/* until a stream or uniform buffer is set, its address points at unmapped memory */
#define GXM_UNSET_VERTEX_STREAM(i)	((const void *)((0x3d0 + (i)) << 20))
#define GXM_UNSET_UNIFORM_BUFFER(i)	((const void *)((0x3e00 + (i)) << 16))

/* whether two buffers given to a context share a byte */
static inline SceBool gxmBuffersOverlap(const char *a, SceSize aSize, const char *b, SceSize bSize)
{
	const char *start = b < a ? a : b;
	const char *end = a + aSize;

	if (b + bSize < end)
		end = b + bSize;
	return start < end;
}

int sceGxmFinish(SceGxmContext *context);
int sceGxmPadHeartbeat(const SceGxmColorSurface *displaySurface,
		       struct SceGxmSyncObject *displaySyncObject);
int sceGxmWaitEvent(void);
#define SCE_GXM_MINIMUM_DEFERRED_CONTEXT_BUFFER_SIZE	1024

typedef void *(*SceGxmDeferredContextCallback)(void *userData, uint32_t minSize, uint32_t *size);

typedef struct SceGxmDeferredContextParams {
	void *hostMem;
	uint32_t hostMemSize;
	SceGxmDeferredContextCallback vdmCallback;
	SceGxmDeferredContextCallback vertexCallback;
	SceGxmDeferredContextCallback fragmentCallback;
	void *userData;
	void *vdmBufferMem;
	uint32_t vdmBufferMemSize;
	void *vertexBufferMem;
	uint32_t vertexBufferMemSize;
	void *fragmentBufferMem;
	uint32_t fragmentBufferMemSize;
} SceGxmDeferredContextParams;

/* the SDK's opaque SceGxmCommandList (SCE_GXM_COMMAND_LIST_WORD_COUNT words) */
typedef struct SceGxmCommandList {
	SceUInt32 *start;		/* the first VDM word, linked to by sceGxmExecuteCommandList */
	SceUInt32 *returnLink;		/* the last block: patched with the address to return to */
	SceUInt32 depthStencilUsage;	/* GXM_LIST_USES_* */
	SceUInt32 tileYMax;		/* the scene's valid region must reach this tile row */
	SceUInt32 reserved[3];
	SceUInt32 serial;		/* the deferred context's listSerial */
} SceGxmCommandList;

_Static_assert(sizeof(SceGxmCommandList) == 8 * 4, "SceGxmCommandList size");

static inline SceUInt32 gxmFloatBits(float value)
{
	union { float f; SceUInt32 u; } bits = { .f = value };

	return bits.u;
}

void gxmContextUpdateVertexOutputs(SceGxmContext *context, const SceGxmVertexProgram *program);
void gxmContextUpdateFrontFragmentProgram(SceGxmContext *context,
					  const SceGxmFragmentProgram *program);
void gxmContextUpdateBackFragmentProgram(SceGxmContext *context,
					 const SceGxmFragmentProgram *program);

void sceGxmSetDefaultRegionClipAndViewport(SceGxmContext *context, SceUInt32 xMax, SceUInt32 yMax);

/* context_flush.c */
int gxmContextCheckCommandList(const SceGxmContext *context, const SceGxmCommandList *list);
void gxmContextResetState(SceGxmContext *context, SceBool vertexOnly,
			  SceBool preserveDefaultUniformBuffers);
int gxmContextWriteVertexStreams(SceGxmContext *context);
int gxmContextWriteState(SceGxmContext *context);

/* context_kick.c */
#define SCE_GXM_MIDSCENE_PRESERVE_DEFAULT_UNIFORM_BUFFERS	0x00000001U

#define GXM_KICK_FRAGMENT	0x00000002	/* the fragment part too: the scene's last kick */
#define GXM_KICK_PARTIAL	0x00000004	/* a partial render: the scene goes on */
#define GXM_KICK_VDM_FULL	0x00000008	/* gxmContextKickIfFull: the ring that was full */
#define GXM_KICK_VERTEX_FULL	0x00000010
#define GXM_KICK_FRAGMENT_FULL	0x00000020
#define GXM_KICK_USSE_FULL	0x00000040
#define GXM_KICK_MID_SCENE	0x00000080	/* a flush inside the scene */

/* what the capture tool is given of a kick (GxmInitCallbacks.kickCaptureInfo) */
typedef struct GxmKickCaptureInfo {
	SceGxmContext *context;
	SceUInt32 frame;
	SceBool fragment;
	SceUInt32 vdmStart;		/* the kick's first VDM word */
	SceUInt32 reserved;
	const SGX_KICKTA *kick;
	SceUInt32 usseBases[16];	/* the USSE code heaps, in 8 MiB steps */
} GxmKickCaptureInfo;

struct SceGxmNotification;
int gxmContextKick(SceGxmContext *context, SceUInt32 flags,
		   const struct SceGxmNotification *vertexNotification,
		   const struct SceGxmNotification *fragmentNotification);
int gxmContextKickIfFull(SceGxmContext *context);

/* --- shader patcher --- */

typedef void *SceGxmShaderPatcherHostAllocCallback(void *userData, SceSize size);
typedef void SceGxmShaderPatcherHostFreeCallback(void *userData, void *mem);
typedef void *SceGxmShaderPatcherBufferAllocCallback(void *userData, SceSize size);
typedef void SceGxmShaderPatcherBufferFreeCallback(void *userData, void *mem);
typedef void *SceGxmShaderPatcherUsseAllocCallback(void *userData, SceSize size,
						   SceUInt32 *usseOffset);
typedef void SceGxmShaderPatcherUsseFreeCallback(void *userData, void *mem);

typedef struct SceGxmShaderPatcherParams {
	void *userData;
	SceGxmShaderPatcherHostAllocCallback *hostAllocCallback;
	SceGxmShaderPatcherHostFreeCallback *hostFreeCallback;
	SceGxmShaderPatcherBufferAllocCallback *bufferAllocCallback;
	SceGxmShaderPatcherBufferFreeCallback *bufferFreeCallback;
	void *bufferMem;
	SceSize bufferMemSize;
	SceGxmShaderPatcherUsseAllocCallback *vertexUsseAllocCallback;
	SceGxmShaderPatcherUsseFreeCallback *vertexUsseFreeCallback;
	void *vertexUsseMem;
	SceSize vertexUsseMemSize;
	SceUInt32 vertexUsseOffset;
	SceGxmShaderPatcherUsseAllocCallback *fragmentUsseAllocCallback;
	SceGxmShaderPatcherUsseFreeCallback *fragmentUsseFreeCallback;
	void *fragmentUsseMem;
	SceSize fragmentUsseMemSize;
	SceUInt32 fragmentUsseOffset;
} SceGxmShaderPatcherParams;

typedef struct SceGxmRegisteredProgram {
	const SceGxmProgram *programHeader;
	void *literals;
	void *scratch;
	void *thread;
	SceGxmVertexProgram *vertexPrograms;
	SceGxmFragmentProgram *fragmentPrograms;
	struct SceGxmShaderPatcher *patcher;
	struct SceGxmRegisteredProgram *next;
	struct SceGxmRegisteredProgram *prev;
} SceGxmRegisteredProgram;

typedef SceGxmRegisteredProgram *SceGxmShaderPatcherId;

typedef struct SceGxmShaderPatcher {
	void *userData;
	SceGxmShaderPatcherHostAllocCallback *hostAlloc;
	SceGxmShaderPatcherHostFreeCallback *hostFree;
	void *bufferUserData;
	SceGxmShaderPatcherBufferAllocCallback *bufferAlloc;
	SceGxmShaderPatcherBufferFreeCallback *bufferFree;
	void *vertexUsseUserData;
	SceGxmShaderPatcherUsseAllocCallback *vertexUsseAlloc;
	SceGxmShaderPatcherUsseFreeCallback *vertexUsseFree;
	void *fragmentUsseUserData;
	SceGxmShaderPatcherUsseAllocCallback *fragmentUsseAlloc;
	SceGxmShaderPatcherUsseFreeCallback *fragmentUsseFree;
	SceGxmRegisteredProgram *programs;	/* 0x30: the list head */
	GxmHeap bufferHeap;
	GxmHeap vertexUsseHeap;
	GxmHeap fragmentUsseHeap;
	void *bufferMem;			/* 0x70 */
	void *vertexUsseMem;
	SceUInt32 vertexUsseOffset;
	void *fragmentUsseMem;
	SceUInt32 fragmentUsseOffset;
	SceSize hostMemAllocated;		/* 0x84 */
	SceSize bufferMemAllocated;
	SceSize vertexUsseMemAllocated;
	SceSize fragmentUsseMemAllocated;
	SceGxmRegisteredProgram programList;	/* 0x94 */
} SceGxmShaderPatcher;

int sceGxmProgramCheck(const SceGxmProgram *program);
void SceGxmInternalForTest_631E95AA(const void *mem, SceSize size, SceUInt32 a, SceUInt32 b,
				    const char *name);	/* names a block for tools */

/* --- memory --- */

int sceGxmMapMemory(void *base, SceSize size, SceUInt32 attr);
int sceGxmUnmapMemory(void *base);
int sceGxmMapVertexUsseMemory(void *base, SceSize size, SceUInt32 *offset);
int sceGxmUnmapVertexUsseMemory(void *base);
int sceGxmMapFragmentUsseMemory(void *base, SceSize size, SceUInt32 *offset);
int sceGxmUnmapFragmentUsseMemory(void *base);

/* --- notifications --- */

/* a notification as the GPU takes it */
typedef struct GxmGpuNotification {
	SceUInt32 gpuAddress;
	SceUInt32 value;
	SceUInt32 handle;
} GxmGpuNotification;

typedef struct SceGxmNotification {
	volatile SceUInt32 *address;
	SceUInt32 value;
} SceGxmNotification;

int gxmNotificationCheckAddress(const volatile SceUInt32 *address);
int sceGpuSignalWait(void *signal, SceUInt32 timeout);

/* --- display queue --- */

typedef void SceGxmDisplayQueueCallback(const void *callbackData);

typedef struct GxmDisplayQueueEntry {
	SceUInt32 syncHandle;		/* signalled when the frame is done */
	void *callbackData;
} GxmDisplayQueueEntry;

typedef struct GxmDisplayQueue {
	SceUInt32 readIndex;
	SceUInt32 writeIndex;
	SceUInt32 maxPending;
	SceUInt32 mask;
	GxmDisplayQueueEntry *entries;
	SceGxmDisplayQueueCallback *callback;
	SceSize callbackDataSize;
	SceUInt32 exit;
	SceUID thread;
	SceUID readyEvent;		/* an entry was added */
	SceUID completeEvent;		/* an entry was flipped */
	SceUInt32 threadState;
	GxmDisplayQueueEntry storage[];	/* then the callback data */
} GxmDisplayQueue;

int gxmDisplayQueueCreate(SceUInt32 maxPending, SceGxmDisplayQueueCallback *callback,
			  SceSize callbackDataSize, GxmDisplayQueue **queue, SceUInt32 cpuAffinity);
int gxmDisplayQueueDestroy(GxmDisplayQueue *queue);
SceUInt32 gxmDisplayQueueGetMaxPending(const GxmDisplayQueue *queue);
SceBool gxmDisplayQueueIsCurrentThread(void);

int gxmThreadStateInit(SceUInt32 *state, SceUInt8 *flag);

/* --- global state --- */

/*
 * Behaviour switches chosen at start-up from the application's SDK version:
 * a set bit keeps the behaviour of older SDKs.
 */
#define GXM_COMPAT_FINISH_ON_UNMAP	0x00000001	/* UnmapMemory finishes every context */
#define GXM_COMPAT_FORMAT_BYTES		0x00000002	/* SetFormat compares bytes, not bits, per pixel */
#define GXM_COMPAT_COLOR_BYTES		0x00000004	/* the same for color surfaces */
#define GXM_COMPAT_USSE_OLD_HEADER	0x00000010	/* 8-byte aligned programs, short header */
#define GXM_COMPAT_VERTEX_LAYOUT_ANY	0x00000020	/* vertex layouts are not checked */
#define GXM_COMPAT_PATCHER_UNALIGNED	0x00000008	/* patcher memory need not be word aligned */
#define GXM_COMPAT_TEXCOORD_PACKED	0x00000040	/* texture coordinates are not compacted */
#define GXM_COMPAT_YUV_ANY_TYPE		0x00000080
#define GXM_COMPAT_24BIT_STRIDED	0x00000100
#define GXM_COMPAT_YUV_TILED		0x00000200
#define GXM_COMPAT_PALETTE_OPTIONAL	0x00000800
#define GXM_COMPAT_GAMMA_INVERTED	0x00001000
#define GXM_COMPAT_SURFACE_ANY		0x00002000
#define GXM_COMPAT_DRAW_STREAMS_UNSET	0x00004000	/* PrecomputedDrawInit leaves the streams unpatched */
#define GXM_COMPAT_DEPTH_STRIDE_PIXELS	0x00000400	/* the depth stride need only cover the region */
#define GXM_COMPAT_COLOR_ENABLE_DATA	0x00008000	/* a surface is enabled when it has data */
#define GXM_COMPAT_FRAGMENT_CALLBACK_VALUE 0x00040000	/* no fragment callback: INVALID_VALUE */
#define GXM_COMPAT_COLOR_RESTRICTED	0x00010000
#define GXM_COMPAT_USSE_ANY_SIZE	0x00020000	/* USSE maps may exceed 8 MiB */
#define GXM_COMPAT_UNK80000		0x00080000	/* MSAA blending: no single-instruction phase flag */

/* the USSE code heaps: vertex then fragment */
typedef struct GxmUsseHeaps {
	SceUInt32 heap[2];
	SceUInt32 base[2];
} GxmUsseHeaps;

/* library state shared with the system (returned by sceGxmGetTopContextInternal) */
typedef struct GxmTopContext {	/* libgpu_es4's device context */
	SceUInt32 conn;				/* 0x0a4: PVRSRV_DEV_DATA, then the rest */
	SceUInt32 devCookie;			/* 0x0a8 */
	SceUInt32 driverState[45];		/* 0x0ac; [42] at 0x154: USSE registers */
	SceUInt32 memContext;			/* 0x160 */
	SceUInt32 generalHeap;			/* heap 0 */
	GxmUsseHeaps usse[2];			/* 0x168: for applications, then internal */
	SceUInt32 slotHeap;			/* 0x188: heap 8 */
	SceUInt32 heap14;			/* heap 0xe */
	SceUInt32 reserved;
	SceUInt32 hMemBlockProcRef;		/* 0x194 */
	SceUID driverBlock;			/* 0x198 */
	SceUID parameterBufferBlock;		/* 0x19c */
	SceUInt32 renderContext;		/* 0x1a0 */
	SceUInt32 transferQueue;		/* 0x1a4 */
	GxmSlotTable contexts;			/* 0x1a8 */
	PVRSRV_CLIENT_MEM_INFO *notifications;	/* 0x2b4: notification words, then the region */
	PVRSRV_CLIENT_MEM_INFO *codeBlock;			/* 0x2b8: the library's own USSE code */
	PVRSRV_CLIENT_MEM_INFO *fenceBlock;		/* 0x2bc */
	GxmCodeRef copyPrograms[25];		/* 0x2c0: copy 1..25 registers */
	GxmCodeRef emptyProgram;		/* 0x388 */
	GxmCodeRef fenceProgram;		/* 0x390 */
} GxmTopContext;

typedef struct GxmGlobals {
	SceUInt8 initialized;
	SceUInt8 initDone;
	SceUInt8 padding[2];
	SceUInt32 initFlags;		/* SCE_GXM_INITIALIZE_FLAG_* */
	SceUInt32 gpuDevice;		/* the libgpu_es4 connection */
	SceUInt32 deviceInfo;		/* 0x00c */
	SceUInt8 reserved0[0xa4 - 0x10];
	GxmTopContext top;			/* 0x0a4 */
	volatile SceUInt32 sceneActive;		/* 0x398 */
	struct GxmDisplayQueue *displayQueue;	/* 0x39c */
	SceUInt32 compatFlags;		/* GXM_COMPAT_* */
	SceUInt32 parameterBufferSize;	/* 0x3a4 */
	SceUInt32 formatLevel;		/* highest format table level available */
	SceUInt32 reserved1;
	SceUInt32 frameIndex;		/* 0x3b0, 24 bits, tags capture markers */
	SceUInt32 sceneIndex;		/* 0x3b4: scenes this frame, tags capture markers */
	volatile SceUInt32 counter;	/* 0x3b8 */
	SceUInt32 reserved2;
} GxmGlobals;

extern GxmGlobals gxmGlobals;

/* USSE programs are 16-byte aligned, 8 for older SDKs */
static inline SceSize gxmUsseAlignment(void)
{
	return (gxmGlobals.compatFlags & GXM_COMPAT_USSE_OLD_HEADER) ? 8 : 16;
}

GXM_ASSERT_OFFSET(GxmGlobals, top, 0x0a4);
GXM_ASSERT_OFFSET(GxmGlobals, top.memContext, 0x160);
GXM_ASSERT_OFFSET(GxmGlobals, top.contexts, 0x1a8);
GXM_ASSERT_OFFSET(GxmGlobals, top.notifications, 0x2b4);
GXM_ASSERT_OFFSET(GxmGlobals, top.copyPrograms, 0x2c0);
GXM_ASSERT_OFFSET(GxmGlobals, top.slotHeap, 0x188);
GXM_ASSERT_OFFSET(GxmGlobals, top.usse[1].base[1], 0x184);
GXM_ASSERT_OFFSET(GxmGlobals, top.transferQueue, 0x1a4);
GXM_ASSERT_OFFSET(GxmGlobals, top.fenceProgram, 0x390);
GXM_ASSERT_OFFSET(GxmGlobals, parameterBufferSize, 0x3a4);
GXM_ASSERT_OFFSET(GxmGlobals, sceneActive, 0x398);
GXM_ASSERT_OFFSET(GxmGlobals, displayQueue, 0x39c);
GXM_ASSERT_OFFSET(GxmGlobals, counter, 0x3b8);
GXM_ASSERT_OFFSET(GxmGlobals, compatFlags, 0x3a0);
GXM_ASSERT_OFFSET(GxmGlobals, formatLevel, 0x3a8);
GXM_ASSERT_OFFSET(GxmGlobals, frameIndex, 0x3b0);
_Static_assert(sizeof(GxmGlobals) == 0x3c0, "GxmGlobals size");

#define SCE_GXM_INITIALIZE_FLAG_SHARED_SYNC	0x00000002

/* exclusive access (no barriers, as Sony's code) */
static inline SceUInt32 gxmLoadExclusive(volatile SceUInt32 *address)
{
	SceUInt32 value;

	__asm__ volatile("ldrex %0, [%1]" : "=r"(value) : "r"(address) : "memory");
	return value;
}

static inline int gxmStoreExclusive(volatile SceUInt32 *address, SceUInt32 value)
{
	int failed;

	__asm__ volatile("strex %0, %2, [%1]" : "=&r"(failed) : "r"(address), "r"(value) : "memory");
	return failed;
}

int gxmSceneStateInit(void);
int gxmSceneBegin(void);
int gxmSceneEnd(void);
SceBool gxmSceneIsActive(void);
int gxmSceneStateTerm(void);
SceUInt32 gxmCounterIncrement(void);

/* --- tool callbacks (Razor capture, replay) --- */

#define GXM_CALLBACKS_MAGIC	0x00010005

typedef struct GxmRazorCallbacks {
	void (*parameterBufferThreshold)(SceUInt32 threshold);
	void (*terminate)(void);
	void (*parameterBuffer)(SceSize size, SceSize reserved);
	void (*displayQueueAddEntry)(void);	/* after a flip is queued */
	void (*padHeartbeat)(const SceGxmColorSurface *displaySurface,
			     struct SceGxmSyncObject *displaySyncObject);
	void (*kickIfFull)(SceBool vdmFull, SceBool vertexFull, SceBool fragmentFull,
			   SceBool fragmentUsseFull);	/* a ring filled up: a partial kick */
	void (*beginScene)(SceSize vdmSize, SceSize vertexSize, SceSize fragmentSize,
			   SceSize fragmentUsseSize);	/* the rings' sizes */
	void (*kick)(SceSize vdmUsed, SceSize vertexUsed, SceSize fragmentUsed,
		     SceSize fragmentUsseUsed);	/* each kick: the rings' bytes since the last */
	void (*reserved)(void);			/* never called */
	SceUInt32 *volatile *captureMarker;	/* word 8 of *captureMarker takes markers */
} GxmRazorCallbacks;

typedef struct GxmInitCallbacks {
	void (*initialize)(void);
	void (*terminate)(void);
	void (*endScene)(void);
	void (*beginScene)(SceUInt32 frame);
	void (*displayQueueAddEntry)(SceUInt32);	/* 0x10 */
	void (*contextWritten)(SceGxmContext *context);	/* after a draw or a kick */
	int (*userMarker)(SceGxmContext *context, const char *tag);	/* its result picks the marker word */
	void (*memoryMapped)(SceUInt32 type, void *base, SceUInt32 gpuAddress, SceSize size);
	void (*memoryUnmapped)(SceUInt32 type, void *base);
	int (*isCapturing)(void);		/* gates the marker and capture hooks */
	int (*kickCapture)(void);
	void (*colorSurfaceCaptured)(int capture);	/* takes captureColorSurface's result */
	void (*drawCapture)(void);		/* a draw or a kick while capturing */
	int (*captureColorSurface)(SceUInt32 control, const SceUInt32 *words,
				   SceUInt32 outputRegisterSize, const SceGxmTexture *backgroundTex);
	void (*kickCaptureInfo)(const struct GxmKickCaptureInfo *info);
	int (*addRazorGpuCaptureBuffer)(void *base, SceSize size);	/* 0x3c */
	int (*removeRazorGpuCaptureBuffer)(void *base);
	void (*parameterBuffer)(SceSize size, SceSize reserved);	/* 0x44 */
} GxmInitCallbacks;

GXM_ASSERT_OFFSET(GxmInitCallbacks, memoryMapped, 0x1c);
GXM_ASSERT_OFFSET(GxmInitCallbacks, addRazorGpuCaptureBuffer, 0x3c);
_Static_assert(sizeof(GxmInitCallbacks) == 0x48, "GxmInitCallbacks size");
_Static_assert(sizeof(GxmRazorCallbacks) == 0x28, "GxmRazorCallbacks size");

extern SceUInt32 gxmRazorMagic;			/* GXM_CALLBACKS_MAGIC when set */
extern GxmRazorCallbacks gxmRazorCallbacks;
extern SceUInt32 gxmInitCallbacksMagic;
extern GxmInitCallbacks gxmInitCallbacks;

/* tag the capture with an event marker when Razor is attached */
static inline void gxmRazorMarker(SceUInt32 event, SceUInt32 frame)
{
	if (gxmRazorMagic == GXM_CALLBACKS_MAGIC && *gxmRazorCallbacks.captureMarker != NULL)
		(*gxmRazorCallbacks.captureMarker)[8] = frame | event;
}

/* --- SceGpuEs4User (libgpu_es4), ID-named until that library is converted --- */

void *SceGpuEs4User_7AF40138(SceSize size);		/* allocate host memory */
void SceGpuEs4User_1BE5985A(void *memory);		/* free host memory */
int SceGpuEs4User_416C8813(SceUInt32 device, SceUInt32 *syncHandle);	/* create a sync */
int SceGpuEs4User_7DCCBCAF(SceUInt32 device, SceUInt32 syncHandle);	/* destroy a sync */
int SceGpuEs4User_5C3A6DFE(SceUInt32 syncHandle, SceUInt32 timeout);	/* wait on a sync */
int SceGpuEs4User_116449D2(SceUInt32 device, SceUInt32 syncHandle);	/* release a sync */
int SceGpuEs4User_94286660(GxmTopContext *top, void **syncObject);	/* create */
int SceGpuEs4User_B26DF3D2(GxmTopContext *top, void *syncObject);	/* destroy */
int SceGpuEs4UserForVsh_3C09BFCA(GxmTopContext *top, SceUInt32 key, void *syncObject);
int SceGpuEs4UserForVsh_8A27AB0A(GxmTopContext *top, SceUInt32 key, void **syncObject);
int SceGpuEs4UserForVsh_0A9556D9(GxmTopContext *top, SceUInt32 key, void *syncObject);
int SceGpuEs4UserForVsh_010DA2CB(GxmTopContext *top, SceUInt32 key);
int SceGpuEs4User_56893E98(GxmTopContext *top, SceUInt32 memContext, SceUInt32 heap,
			   SceSize size, SceSize alignment, void *base, SceUInt32 flags,
			   SceUInt32 *gpuAddress);			/* map memory */
int SceGpuEs4User_AD20D1A0(GxmTopContext *top, void *base, SceUInt32 heap,
			   SceUInt32 flags);				/* unmap memory */
int SceGpuEs4UserForVsh_3DC96093(GxmTopContext *top, SceUInt32 memContext, SceUInt32 a,
				 SceUInt32 b, SceUInt32 c);
int SceGpuEs4User_071E62F0(GxmTopContext *top, SceUInt32 heap, SceUInt32 attr, SceSize size,
			   SceSize alignment, SceUInt32 flags, PVRSRV_CLIENT_MEM_INFO **block);	/* allocate */
int SceGpuEs4User_B6CFD213(GxmTopContext *top, PVRSRV_CLIENT_MEM_INFO *block);	/* free */
SceUInt32 SceGpuEs4User_A5E331FA(void *params, void *code);	/* write a PDS program */
int SceGpuEs4User_5564C1BE(const SceUInt32 *words, SceUInt32 count, GxmTopContext *top,
			   SceUInt32 queue, void *syncObject, SceUInt32 waitFlag, SceUInt32 signalFlag,
			   const GxmGpuNotification *notification);	/* submit a transfer */
int SceGpuEs4User_7E965369(GxmTopContext *top, SceUInt32 queue);	/* wait for transfers */
int SceGpuEs4User_228252A2(SceUInt32 *device, SceUInt32 flags);		/* connect */
int SceGpuEs4User_73E945A1(SceUInt32 device, SceUInt32 *info);
int SceGpuEs4User_4447D217(SceUInt32 device, SceUInt32 *count, SceUInt32 *devices);
int SceGpuEs4User_70800B46(SceUInt32 device, SceUInt32 handle, GxmTopContext *top);
int SceGpuEs4User_B000CA65(GxmTopContext *top, void *state);
int SceGpuEs4User_5273D097(GxmTopContext *top, void *state);
int SceGpuEs4User_F4735A47(SceUInt32 device, SceUInt32 *info);
int SceGpuEs4User_A5B14369(SceUInt32 device);					/* disconnect */
int SceGpuEs4User_0EA7458D(GxmTopContext *top, SceUID block, SceUInt32 *hMemBlockProcRef,
			   SceUInt32 which);					/* register memory */
int SceGpuEs4User_156A6B70(GxmTopContext *top, SceUID block);		/* unregister memory */
int SceGpuEs4User_0162A405(GxmTopContext *top, const SGX_ADDRENDTARG *target,
			   struct SceGxmRenderTarget **renderTarget);		/* add a render target */
int SceGpuEs4User_D90B0707(GxmTopContext *top, SceUInt32 renderContext,
			   struct SceGxmRenderTarget *renderTarget);		/* remove a render target */
int SceGpuEs4User_804BA13D(GxmTopContext *top, const struct SceGxmRenderTarget *renderTarget,
			   SceUID *driverMemBlock, SceUInt32 *allocated);	/* its driver memory */
int SceGpuEs4User_1E0A9C4A(GxmTopContext *top, SceUInt32 *memContext, SceUInt32 *count,
			   PVRSRV_HEAP_INFO *heaps);			/* heap info */
int SceGpuEs4User_8A997DBB(GxmTopContext *top, const SGX_CREATERENDERCONTEXT *params,
			   SceUInt32 *renderContext, void *out);			/* render context */
int SceGpuEs4User_0BFE1685(GxmTopContext *top, SceUInt32 renderContext, SceUInt32 unused,
			   SceUInt32 forceCleanup);
int SceGpuEs4User_B9178277(GxmTopContext *top, SceUInt32 memContext);
int SceGpuEs4User_9D5C4326(GxmTopContext *top);				/* reset sync waits */
int SceGpuEs4User_BDE2B9E1(GxmTopContext *top, SceUInt32 reserved, const SceUInt32 *params,
			   SceUInt32 *queue);					/* transfer queue */
int SceGpuEs4User_8ED7D698(GxmTopContext *top, SceUInt32 queue, SceUInt32 forceCleanup);
SceUInt32 SceGpuEs4User_53CACDAB(void *code, const SceUInt32 *words, SceUInt32 count);
SceUInt32 SceGpuEs4User_1A575DFD(void *code);
SceUInt32 SceGpuEs4User_86FC3202(void *code);
SceUInt32 SceGpuEs4User_E98914E4(void *params, void *code, SceUInt32 textured,
				 SceUInt32 perspective, SceUInt32 count);
int SceGpuEs4User_0D27286F(SceUInt32 device, SceUInt32 syncHandle, void *const *syncObjects,
			   SceUInt32 count, SceUInt32 flags, SceUInt32 *writeOps, SceUInt32 *readOps);

#endif /* _GXM_INTERNAL_H */
