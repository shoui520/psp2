/*
 * The pixel layout of an image or surface (ImageMode, ImageOrder: global
 * types, as Sony's mangled names show), how it is stored (ImageFormat), and
 * how paf::image resizes, flips, rotates and converts.
 */
#ifndef PAF_IMAGE_IMAGE_TYPES_H
#define PAF_IMAGE_IMAGE_TYPES_H

#include <stddef.h>
#include <stdint.h>

enum ImageMode {
	ImageMode_NONE = -1,
	ImageMode_XBGR0565 = 0,
	ImageMode_ABGR1555 = 1,
	ImageMode_ABGR4444 = 2,
	ImageMode_ABGR8888 = 3,
	ImageMode_RGBA8888 = 4,
	ImageMode_ARGB8888 = 5,
	ImageMode_BGRA8888 = 6,
	ImageMode_WZYX16FP = 7,
	ImageMode_WZYX32FP = 8,
	ImageMode_XCRYCB8888 = 9,
	ImageMode_CRYCBX8888 = 10,
	ImageMode_XCRYCB16FP = 11,
	ImageMode_RGBX16INT = 12,
	ImageMode_CRYCBX16INT = 13,
	ImageMode_A8 = 14,
	ImageMode_L8 = 15,
	ImageMode_DXT1 = 16,
	ImageMode_DXT2 = 17,
	ImageMode_DXT3 = 18,
	ImageMode_DXT4 = 19,
	ImageMode_DXT5 = 20,
	ImageMode_index4 = 21,
	ImageMode_index8 = 22,
	ImageMode_index16 = 23,
	ImageMode_index32 = 24,
	ImageMode_BGR888 = 25,
	ImageMode_RGB888 = 26,
	ImageMode_YUV420P2 = 27,
	ImageMode_YUV420P3 = 28,
	ImageMode_PVRT2BPP = 29,
	ImageMode_PVRT4BPP = 30,
	ImageMode_PVRTII2BPP = 31,
	ImageMode_PVRTII4BPP = 32,
	ImageMode_depth16 = 100,
	ImageMode_depth32 = 101,
	ImageMode_A16FP = 102,
	ImageMode_A32FP = 103
};

enum ImageOrder {
	ImageOrder_NONE = -1,
	ImageOrder_Linear = 1,
	ImageOrder_Normal = 1,
	ImageOrder_Swizzle = 2,
	ImageOrder_Tiled = 3
};

namespace paf {
namespace image {

enum ImageFormat {
	ImageFormat_None = 0,
	ImageFormat_Png = 1,
	ImageFormat_Jpg = 2,
	ImageFormat_Tif = 3,
	ImageFormat_Gif = 4,
	ImageFormat_Bmp = 5,
	ImageFormat_Gim = 6,
	ImageFormat_Dds = 7,
	ImageFormat_Gxt = 8
};

/* How the mipmap levels shrink (HalfSize: each level half the one before). */
enum ImageXType {
	ImageXType_None = 0,
	ImageXType_HalfSize = 1,
	ImageXType_ConstantSize = 2
};

enum ImageYType {
	ImageYType_None = 0,
	ImageYType_Sequence = 1
};

enum ImageResizeType {
	ImageResizeType_None = 0,
	ImageResizeType_NearestNeighbor = 1,
	ImageResizeType_Linear = 2,
	ImageResizeType_BoxFilter = 3,
	ImageResizeType_BellFilter = 4,
	ImageResizeType_HermitFilter = 5,
	ImageResizeType_B_SplineFilter = 6,
	ImageResizeType_MitchellFilter = 7,
	ImageResizeType_Lanczos3Filter = 8
};

enum ImageFlipType {
	ImageFlipType_None = 0,
	ImageFlipType_LeftRight = 1,
	ImageFlipType_TopButtom = 2
};

/* Clockwise quarter turns. */
enum ImageRotateType {
	ImageRotateType_None = 0,
	ImageRotateType_Degree90 = 1,
	ImageRotateType_Degree180 = 2,
	ImageRotateType_Degree270 = 3
};

enum ImageDecoderType {
	ImageDecoderType_None = 0,
	ImageDecoderType_SSDecoder = 1,
	ImageDecoderType_DefaultDecoder = 2,
	ImageDecoderType_OldDecoder = 3
};

/*
 * How DXT images are compressed, as squish's colour fit: Default and
 * MediumQuality the cluster fit, LowQuality the range fit, HighQuality the
 * iterative cluster fit.
 */
enum ImageConvertModeType {
	ImageConvertModeType_Default = 0,
	ImageConvertModeType_LowQuality = 1,
	ImageConvertModeType_MediumQuality = 2,
	ImageConvertModeType_HighQuality = 3
};

/* A pixel buffer: its address and size in bytes. */
class ImageBuffer {
public:
	ImageBuffer() : buff(NULL), size(0) {}

	void *buff;
	size_t size;
};

}
}

#endif
