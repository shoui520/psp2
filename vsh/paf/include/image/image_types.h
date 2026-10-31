/*
 * The pixel layout of an image or surface (ImageMode, ImageOrder: global
 * types, as Sony's mangled names show), how it is stored (ImageFormat), and
 * how paf::image resizes, flips and rotates.
 */
#ifndef PAF_IMAGE_IMAGE_TYPES_H
#define PAF_IMAGE_IMAGE_TYPES_H

#include <stdint.h>

/* The enumerator names are ours: no Sony source names them. */
enum ImageMode {
	ImageMode_NONE = -1,
	ImageMode_RGB565 = 0,
	ImageMode_RGBA5551 = 1,
	ImageMode_RGBA4444 = 2,
	ImageMode_RGBA8888 = 4,
	ImageMode_BGRA8888 = 6,
	ImageMode_DXT1 = 16,
	ImageMode_DXT3 = 18,
	ImageMode_DXT5 = 20,
	ImageMode_CLUT4 = 21,
	ImageMode_CLUT8 = 22,
	ImageMode_CLUT16 = 23,
	ImageMode_CLUT32 = 24
};

enum ImageOrder {
	ImageOrder_NONE = -1
};

namespace paf {
namespace image {

/* How an image's memory is laid out; the filters pass it to CalcMemSize, which ignores it. */
enum ImageFormat {
	ImageFormat_NONE = -1,
	ImageFormat_DXT = 7		/* what the DXT compressors check their output with */
};

enum ImageResizeType {
	ImageResizeType_None = 0,
	ImageResizeType_NearestNeighbor = 1,
	ImageResizeType_Triangle = 2,
	ImageResizeType_Box = 3,
	ImageResizeType_Bell = 4,
	ImageResizeType_Hermite = 5,
	ImageResizeType_BSpline = 6,
	ImageResizeType_Mitchell = 7,
	ImageResizeType_Lanczos3 = 8
};

enum ImageFlipType {
	ImageFlipType_Horizontal = 1,
	ImageFlipType_Vertical = 2
};

/* Clockwise quarter turns. */
enum ImageRotateType {
	ImageRotateType_90 = 1,
	ImageRotateType_180 = 2,
	ImageRotateType_270 = 3
};

/* How DXT images are compressed: squish's colour fit. */
enum ImageCompressQuality {
	ImageCompressQuality_ClusterFit = 0,
	ImageCompressQuality_RangeFit = 1,
	ImageCompressQuality_Default = 2,
	ImageCompressQuality_IterativeClusterFit = 3
};

}
}

#endif
