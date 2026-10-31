/*
 * paf::image::filter: whole-image operations on pixel buffers (blending, blurring,
 * copying, cropping, flipping, pasting, resizing, rotating, sharpening), and the
 * conversions between pixel modes, DXT compression among them (squish 1.11).
 *
 * The resize filters are Graphics Gems III's (Schumacher, "General Filtered Image
 * Rescaling"), their contributions applied as 2.14 fixed point with NEON.
 */
#include <arm_neon.h>
#include <image/image.h>
#include <image/image_attribute.h>
#include <image/image_filter.h>
#include <memory/heap_allocator.h>
#include <squish.h>
#include <std/stdlib.h>
#include <std/string.h>
extern "C" {
#include "../../../paf_types.h"
int sce_paf_abs(int value);
}
#include "../../../std/math.h"

using namespace paf;
using namespace paf::image;
using namespace paf::memory;

/*
 * The pixel operations of the copy and convert loops: what each loop does to a
 * pixel is in the loop itself, the operation only selects it.
 */
struct PixelCopy {
};

struct PixelConvert {
};

/* A palette lookup: the source's pixels index clut. */
struct PixelLookup {
	PixelLookup(const uint32_t *palette) : clut(palette)
	{
	}

	const uint32_t *clut;
};

/* One source pixel's share of a destination pixel: its offset and its weight. */
struct Contrib {
	int pixel;
	union {
		float f;	/* while the contributions are summed */
		int32_t i;	/* normalised, 2.14 fixed point */
	} weight;
};

/* The contributions to one destination pixel. */
struct ContribList {
	int n;
	Contrib *p;
};

typedef float (*FilterFunc)(float t);

static float BoxFilter(float t);
static float TriangleFilter(float t);
static float BellFilter(float t);
static float HermiteFilter(float t);
static float BSplineFilter(float t);
static float MitchellFilter(float t);
static float Lanczos3Filter(float t);
static void GetPixel(ImageMode mode, const uint8_t *src, uint32_t x, const uint8_t *clut,
		     uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a);

template <typename T>
static bool FlipImage(T *dst, const T *src, uint32_t dst_pitch, uint32_t src_pitch, int w, int h,
		      ImageMode mode, ImageFlipType type);
template <typename T>
static bool ResizeNearestNeighbor(T *dst, const T *src, uint32_t dst_pitch, uint32_t src_pitch,
				  int src_w, int src_h, ImageMode mode, int dst_w, int dst_h,
				  ImageResizeType type, HeapAllocator *allocator);
template <typename T>
static bool ConvertNearestNeighbor(uint32_t *dst, const T *src, const uint8_t *clut,
				   uint32_t dst_pitch, uint32_t src_pitch, int dst_w, int dst_h,
				   int src_w, int src_h, ImageMode src_mode, ImageMode dst_mode,
				   HeapAllocator *allocator);
template <typename T>
static bool RotateImage(T *dst, const T *src, uint32_t dst_pitch, uint32_t src_pitch, int w,
			int h, ImageMode mode, ImageRotateType type);

static void CopyPixels4(uint8_t *dst, const uint8_t *src, int dst_pitch, int src_pitch, int w,
			int h, const PixelCopy &op);
static void CopyPixels8(uint8_t *dst, const uint8_t *src, int dst_pitch, int src_pitch, int w,
			int h, const PixelCopy &op);
static void CopyPixels16(uint16_t *dst, const uint16_t *src, int dst_pitch, int src_pitch, int w,
			 int h, const PixelCopy &op);
static void CopyPixels32(uint32_t *dst, const uint32_t *src, int dst_pitch, int src_pitch, int w,
			 int h, const PixelCopy &op);
static void ConvertCLUT4(uint32_t *dst, const uint8_t *src, int dst_pitch, int src_pitch, int w,
			 int h, const PixelLookup &op);
static void ConvertCLUT8(uint32_t *dst, const uint8_t *src, int dst_pitch, int src_pitch, int w,
			 int h, const PixelLookup &op);
static void ConvertRGB565(uint8_t *dst, const uint16_t *src, int dst_pitch, int src_pitch, int w,
			  int h, const PixelConvert &op);
static void ConvertRGBA5551(uint8_t *dst, const uint16_t *src, int dst_pitch, int src_pitch,
			    int w, int h, const PixelConvert &op);
static void ConvertRGBA4444(uint8_t *dst, const uint16_t *src, int dst_pitch, int src_pitch,
			    int w, int h, const PixelConvert &op);
static void ConvertBGRA8888(uint8_t *dst, const uint8_t *src, int dst_pitch, int src_pitch, int w,
			    int h, const PixelConvert &op);
static void ConvertCLUT4WithEdge(void *dst, const uint8_t *src, int dst_pitch, int src_pitch,
				 int w, int h, const PixelLookup &op, uint32_t edge);
static void ConvertCLUT8WithEdge(void *dst, const uint8_t *src, int dst_pitch, int src_pitch,
				 int w, int h, const PixelLookup &op, uint32_t edge);
static void ConvertRGB565WithEdge(void *dst, const uint16_t *src, int dst_pitch, int src_pitch,
				  int w, int h, const PixelConvert &op, uint32_t edge);
static void ConvertRGBA5551WithEdge(void *dst, const uint16_t *src, int dst_pitch, int src_pitch,
				    int w, int h, const PixelConvert &op, uint32_t edge);
static void ConvertRGBA4444WithEdge(void *dst, const uint16_t *src, int dst_pitch, int src_pitch,
				    int w, int h, const PixelConvert &op, uint32_t edge);
static void ConvertBGRA8888WithEdge(void *dst, const uint8_t *src, int dst_pitch, int src_pitch,
				    int w, int h, const PixelConvert &op, uint32_t edge);
static void CopyPixels32WithEdge(void *dst, const uint32_t *src, int dst_pitch, int src_pitch,
				 int w, int h, const PixelCopy &op, uint32_t edge);

static inline uint8_t Clamp(int value)
{
	return value > 255 ? 255 : value < 0 ? 0 : value;
}

static inline uint8_t Clamp(double value)
{
	if (value > 255.0)
		value = 255.0;
	else if (value < 0.0)
		value = 0.0;
	return (unsigned int)value;
}

static inline void *Alloc(HeapAllocator *allocator, size_t size)
{
	if (allocator != NULL)
		return allocator->sce_paf_malloc(size);
	return sce_paf_malloc(size);
}

static inline void Free(HeapAllocator *allocator, void *ptr)
{
	if (allocator != NULL)
		allocator->sce_paf_free(ptr);
	else
		sce_paf_free(ptr);
}

/* floor(log2(value)), value > 0. */
static inline uint32_t Log2(uint32_t value)
{
	uint32_t result, shift;

	result = (value > 0xFFFF) << 4;
	value >>= result;
	shift = (value > 0xFF) << 3;
	value >>= shift;
	result |= shift;
	shift = (value > 0xF) << 2;
	value >>= shift;
	result |= shift;
	shift = (value > 0x3) << 1;
	value >>= shift;
	result |= shift;
	result |= value >> 1;
	return result;
}

bool filter::Blend(ImageBuffer *dst, const ImageBuffer *src1, const ImageBuffer *src2,
		   int dst_pitch, int src_pitch, ImageExtent extent, ImageMode mode,
		   ImageFormat format, float rate)
{
	if (dst->buff == NULL ||
	    dst->size < ImageAttribute::CalcMemSize(extent.w, extent.h, 1, 1, mode, format) ||
	    (mode != ImageMode_RGBA8888 && mode != ImageMode_BGRA8888))
		return false;

	if (rate <= 0.0f) {
		CopyPixels32((uint32_t *)dst->buff, (const uint32_t *)src1->buff, dst_pitch,
			     src_pitch, extent.w, extent.h, PixelCopy());
		return true;
	}
	if (rate >= 1.0f) {
		CopyPixels32((uint32_t *)dst->buff, (const uint32_t *)src2->buff, dst_pitch,
			     src_pitch, extent.w, extent.h, PixelCopy());
		return true;
	}

	uint8_t *d = (uint8_t *)dst->buff;
	const uint8_t *s1 = (const uint8_t *)src1->buff;
	const uint8_t *s2 = (const uint8_t *)src2->buff;

	for (int y = 0; y < extent.h; y++) {
		uint8_t *dp = d;
		const uint8_t *p1 = s1;
		const uint8_t *p2 = s2;

		for (int x = 0; x < extent.w; x++) {
			dp[0] = Clamp(p1[0] + (int)((float)(p2[0] - p1[0]) * rate));
			dp[1] = Clamp(p1[1] + (int)((float)(p2[1] - p1[1]) * rate));
			dp[2] = Clamp(p1[2] + (int)((float)(p2[2] - p1[2]) * rate));
			dp[3] = Clamp(p1[3] + (int)((float)(p2[3] - p1[3]) * rate));
			p1 += 4;
			dp += 4;
			p2 += 4;
		}
		s2 += src_pitch;
		d += dst_pitch;
		s1 += src_pitch;
	}
	return true;
}

bool filter::Blur(ImageBuffer *dst, const ImageBuffer *src, int dst_pitch, int src_pitch,
		  ImageExtent extent, ImageMode mode, ImageFormat format, float sigma,
		  HeapAllocator *allocator)
{
	int w = extent.w;
	int h = extent.h;

	if (dst->buff == NULL)
		return false;
	if (dst->size < ImageAttribute::CalcMemSize(w, h, 1, 1, mode, format) ||
	    (mode != ImageMode_RGBA8888 && mode != ImageMode_BGRA8888))
		return false;

	uint8_t *tmp = (uint8_t *)Alloc(allocator, 4 * w * h);
	if (tmp == NULL)
		return false;

	const uint8_t *s = (const uint8_t *)src->buff;
	uint8_t *d = (uint8_t *)dst->buff;
	uint8_t *t = tmp;
	int radius = (int)(sigma * 3.0f);
	double *kernel = (double *)Alloc(allocator, 8 * (radius + 1));
	if (kernel == NULL) {
		Free(allocator, tmp);
		return false;
	}

	float variance2 = sigma * 2.0f * sigma;
	float norm = 1.0f / (sigma * 2.5066283f);
	for (int i = 0; i <= radius; i++)
		kernel[i] = norm * sce_paf_powf(2.7182817f, (float)(-i * i) / variance2);

	/* Horizontally into tmp. */
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			double sum = 0.0, r = 0.0, g = 0.0, b = 0.0, a = 0.0;

			for (int k = -radius; k <= radius; k++) {
				if (x + k >= 0 && x + k <= w - 1) {
					double weight = kernel[sce_paf_abs(k)];
					const uint8_t *p = &s[4 * (y * w + x + k)];

					sum += weight;
					r += (double)p[0] * weight;
					g += (double)p[1] * weight;
					b += (double)p[2] * weight;
					a += (double)p[3] * weight;
				}
			}
			uint8_t *q = &t[4 * (y * w + x)];
			q[0] = Clamp(r / sum);
			q[1] = Clamp(g / sum);
			q[2] = Clamp(b / sum);
			q[3] = Clamp(a / sum);
		}
		t += dst_pitch - 4 * w;
		s += src_pitch - 4 * w;
	}

	/* Vertically into dst. */
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			double sum = 0.0, r = 0.0, g = 0.0, b = 0.0, a = 0.0;

			for (int k = -radius; k <= radius; k++) {
				if (y + k >= 0 && y + k <= h - 1) {
					double weight = kernel[sce_paf_abs(k)];
					const uint8_t *p = &t[4 * ((y + k) * w + x)];

					sum += weight;
					r += (double)p[0] * weight;
					g += (double)p[1] * weight;
					b += (double)p[2] * weight;
					a += (double)p[3] * weight;
				}
			}
			uint8_t *q = &d[4 * (y * w + x)];
			q[0] = Clamp(r / sum);
			q[1] = Clamp(g / sum);
			q[2] = Clamp(b / sum);
			q[3] = Clamp(a / sum);
		}
	}

	if (allocator != NULL) {
		allocator->sce_paf_free(kernel);
		allocator->sce_paf_free(tmp);
	} else {
		sce_paf_free(kernel);
		sce_paf_free(tmp);
	}
	return true;
}

/* DXT blocks to RGBA8888; the blocks' rows are padded to a power of two. */
static bool DecompressDxt(uint32_t *dst, const uint8_t *blocks, int dst_pitch, int src_pitch,
			  int w, int h, ImageMode mode, ImageConvertModeType quality)
{
	int flags;

	switch (mode) {
	case ImageMode_DXT5:
		flags = squish::kDxt5;
		break;
	case ImageMode_DXT3:
		flags = squish::kDxt3;
		break;
	case ImageMode_DXT1:
		flags = squish::kDxt1;
		break;
	default:
		return false;
	}
	switch (quality) {
	case ImageConvertModeType_Default:
	case ImageConvertModeType_MediumQuality:
		flags |= squish::kColourClusterFit;
		break;
	case ImageConvertModeType_LowQuality:
		flags |= squish::kColourRangeFit;
		break;
	case ImageConvertModeType_HighQuality:
		flags |= squish::kColourIterativeClusterFit;
		break;
	default:
		return false;
	}

	int full_w = w / 4;
	int full_h = h / 4;
	int blocks_w = (w + 3) / 4;
	int blocks_h = (h + 3) / 4;
	int block_size = 16;
	if (mode == ImageMode_DXT1)
		block_size = 8;
	uint32_t pitch = 1 << Log2(w);
	if (w > pitch)
		pitch *= 2;
	uint32_t row_size = block_size * pitch;

	for (int by = 0; by < blocks_h; by++) {
		const uint8_t *block = blocks;
		uint32_t *d = dst;

		for (int bx = 0; bx < blocks_w; bx++) {
			uint32_t rgba[16];

			squish::Decompress((squish::u8 *)rgba, block, flags);
			if (by >= full_h || bx >= full_w) {
				uint32_t *row = d;
				const uint32_t *p = rgba;

				for (int y = 0; y < 4; y++) {
					for (int x = 0; x < 4; x++) {
						if (4 * by + y < h && 4 * bx + x < w)
							row[x] = p[x];
					}
					row = (uint32_t *)((uint8_t *)row + dst_pitch);
					p += 4;
				}
			} else {
				uint32_t *row = d;
				const uint32_t *p = rgba;

				for (int y = 0; y < 4; y++) {
					row[0] = p[0];
					row[1] = p[1];
					row[2] = p[2];
					row[3] = p[3];
					row = (uint32_t *)((uint8_t *)row + dst_pitch);
					p += 4;
				}
			}
			block += block_size;
			d += 4;
		}
		dst += dst_pitch;
		blocks += row_size / 4;
	}
	return true;
}

/*
 * RGBA8888 to DXT1. The blocks cover the destination's extent; a block reaching
 * past the source repeats its last column and row.
 */
static bool CompressDxt1(ImageBuffer *dst, const ImageBuffer *src, const ImageBuffer *clut,
			 int dst_pitch, int src_pitch, const ImageExtent *dst_extent,
			 const ImageExtent *src_extent, ImageMode src_mode, ImageFormat format,
			 ImageConvertModeType quality)
{
	if (dst->buff == NULL ||
	    dst->size < ImageAttribute::CalcMemSize(dst_extent->w, dst_extent->h, 1, 1,
						      ImageMode_DXT1, ImageFormat_Dds))
		return false;

	int w = src_extent->w;
	int h = src_extent->h;
	int full_w = w / 4;
	int full_h = h / 4;
	uint32_t block_step = ImageAttribute::GetPixelSize(src_mode) * 4 / 8;
	int blocks_h = (dst_extent->h + 3) / 4;
	int blocks_w = (dst_extent->w + 3) / 4;
	const uint32_t *row = (const uint32_t *)src->buff;
	uint8_t *out = (uint8_t *)dst->buff;
	const uint8_t *last = (const uint8_t *)src->buff + src_pitch * h - src_pitch;

	for (int by = 0; by < blocks_h; by++) {
		const uint8_t *p = (const uint8_t *)row;
		uint8_t *block = out;

		for (int bx = 0; bx < blocks_w; bx++) {
			uint32_t rgba[16];

			if (by < full_h && bx < full_w) {
				const uint32_t *q = (const uint32_t *)p;

				for (int y = 0; y < 4; y++) {
					rgba[y * 4 + 0] = q[0];
					rgba[y * 4 + 1] = q[1];
					rgba[y * 4 + 2] = q[2];
					rgba[y * 4 + 3] = q[3];
					q = (const uint32_t *)((const uint8_t *)q + src_pitch);
				}
			} else {
				for (int y = 0; y < 4; y++) {
					const uint32_t *q = (const uint32_t *)(p + y * src_pitch);
					const uint32_t *edge = (const uint32_t *)((const uint8_t *)row +
										  y * src_pitch);

					for (int x = 0; x < 4; x++) {
						int py = 4 * by + y;
						int px = 4 * bx + x;

						if (py < h && px < w)
							rgba[y * 4 + x] = q[x];
						else if (py < h)
							rgba[y * 4 + x] = edge[w - 1];
						else if (px < w)
							rgba[y * 4 + x] = ((const uint32_t *)last)[px];
						else
							rgba[y * 4 + x] = ((const uint32_t *)last)[w - 1];
					}
				}
			}

			int flags;
			switch (quality) {
			case ImageConvertModeType_Default:
			case ImageConvertModeType_MediumQuality:
				flags = squish::kDxt1 | squish::kColourClusterFit;
				break;
			case ImageConvertModeType_LowQuality:
				flags = squish::kDxt1 | squish::kColourRangeFit;
				break;
			case ImageConvertModeType_HighQuality:
				flags = squish::kDxt1 | squish::kColourIterativeClusterFit;
				break;
			default:
				return false;
			}
			squish::CompressMasked((const squish::u8 *)rgba, 0xFFFF, block, flags);
			block += 8;
			p += block_step;
		}
		out += 8 * blocks_w;
		row += src_pitch;
	}
	return true;
}

/*
 * RGBA8888 to DXT3 (flags kDxt3) or DXT5 (kDxt5). The blocks cover the source's
 * extent, in a destination cleared first when the extents differ.
 */
static inline bool CompressDxtAlpha(ImageBuffer *dst, const ImageBuffer *src,
				    const ImageExtent *dst_extent, const ImageExtent *src_extent,
				    int src_pitch, ImageMode src_mode, ImageConvertModeType quality,
				    int method)
{
	int w = src_extent->w;
	int h;

	if (w != dst_extent->w || (h = src_extent->h) != dst_extent->h) {
		sce_paf_memset(dst->buff, 0, dst->size);
		w = src_extent->w;
		h = src_extent->h;
	}

	int full_w = w / 4;
	int full_h = h / 4;
	int blocks_w = (w + 3) / 4;
	int blocks_h = (h + 3) / 4;
	uint32_t block_step = ImageAttribute::GetPixelSize(src_mode) * 4 / 8;
	int dst_blocks_w = (dst_extent->w + 3) / 4;
	const uint32_t *row = (const uint32_t *)src->buff;
	uint8_t *out = (uint8_t *)dst->buff;
	const uint8_t *last = (const uint8_t *)src->buff + src_pitch * h - src_pitch;

	for (int by = 0; by < blocks_h; by++) {
		const uint8_t *p = (const uint8_t *)row;
		uint8_t *block = out;

		for (int bx = 0; bx < blocks_w; bx++) {
			uint32_t rgba[16];
			int mask;

			if (by < full_h && bx < full_w) {
				const uint32_t *q = (const uint32_t *)p;

				for (int y = 0; y < 4; y++) {
					rgba[y * 4 + 0] = q[0];
					rgba[y * 4 + 1] = q[1];
					rgba[y * 4 + 2] = q[2];
					rgba[y * 4 + 3] = q[3];
					q = (const uint32_t *)((const uint8_t *)q + src_pitch);
				}
				mask = 0xFFFF;
			} else {
				mask = 0;
				for (int y = 0; y < 4; y++) {
					const uint32_t *q = (const uint32_t *)(p + y * src_pitch);
					const uint32_t *edge = (const uint32_t *)((const uint8_t *)row +
										  y * src_pitch);

					for (int x = 0; x < 4; x++) {
						int py = 4 * by + y;
						int px = 4 * bx + x;

						if (py < h && px < w)
							rgba[y * 4 + x] = q[x];
						else if (py < h)
							rgba[y * 4 + x] = edge[w - 1];
						else if (px < w)
							rgba[y * 4 + x] = ((const uint32_t *)last)[px];
						else
							rgba[y * 4 + x] = ((const uint32_t *)last)[w - 1];
						mask |= 1 << (y * 4 + x);
					}
				}
			}

			int flags;
			switch (quality) {
			case ImageConvertModeType_Default:
			case ImageConvertModeType_MediumQuality:
				flags = method | squish::kColourClusterFit;
				break;
			case ImageConvertModeType_LowQuality:
				flags = method | squish::kColourRangeFit;
				break;
			case ImageConvertModeType_HighQuality:
				flags = method | squish::kColourIterativeClusterFit;
				break;
			default:
				return false;
			}
			squish::CompressMasked((const squish::u8 *)rgba, mask, block, flags);
			block += 16;
			p += block_step;
		}
		out += 16 * dst_blocks_w;
		row += src_pitch;
	}
	return true;
}

static bool CompressDxt3(ImageBuffer *dst, const ImageBuffer *src, const ImageBuffer *clut,
			 int dst_pitch, int src_pitch, const ImageExtent *dst_extent,
			 const ImageExtent *src_extent, ImageMode src_mode, ImageFormat format,
			 ImageConvertModeType quality)
{
	if (dst->buff == NULL ||
	    dst->size < ImageAttribute::CalcMemSize(dst_extent->w, dst_extent->h, 1, 1,
						      ImageMode_DXT3, ImageFormat_Dds))
		return false;
	return CompressDxtAlpha(dst, src, dst_extent, src_extent, src_pitch, src_mode, quality,
				squish::kDxt3);
}

static bool CompressDxt5(ImageBuffer *dst, const ImageBuffer *src, const ImageBuffer *clut,
			 int dst_pitch, int src_pitch, const ImageExtent *dst_extent,
			 const ImageExtent *src_extent, ImageMode src_mode, ImageFormat format,
			 ImageConvertModeType quality)
{
	if (dst->buff == NULL ||
	    dst->size < ImageAttribute::CalcMemSize(dst_extent->w, dst_extent->h, 1, 1,
						      ImageMode_DXT5, ImageFormat_Dds))
		return false;
	return CompressDxtAlpha(dst, src, dst_extent, src_extent, src_pitch, src_mode, quality,
				squish::kDxt5);
}

bool filter::ConvertMode(ImageBuffer *dst, const ImageBuffer *src, const ImageBuffer *clut,
			 int dst_pitch, int src_pitch, const ImageExtent &dst_extent,
			 const ImageExtent &src_extent, ImageMode src_mode, ImageFormat format,
			 ImageMode dst_mode, ImageConvertModeType quality)
{
	switch (dst_mode) {
	case ImageMode_RGBA8888: {
		if (dst->buff == NULL ||
		    dst->size < ImageAttribute::CalcMemSize(dst_extent.w, dst_extent.h, 1, 1,
							      src_mode, format))
			return false;

		const uint32_t *palette = NULL;
		if (clut != NULL)
			palette = (const uint32_t *)clut->buff;

		switch (src_mode) {
		case ImageMode_XBGR0565:
			ConvertRGB565((uint8_t *)dst->buff, (const uint16_t *)src->buff, dst_pitch,
				      src_pitch, dst_extent.w, dst_extent.h, PixelConvert());
			return true;
		case ImageMode_ABGR1555:
			ConvertRGBA5551((uint8_t *)dst->buff, (const uint16_t *)src->buff,
					dst_pitch, src_pitch, dst_extent.w, dst_extent.h,
					PixelConvert());
			return true;
		case ImageMode_ABGR4444:
			ConvertRGBA4444((uint8_t *)dst->buff, (const uint16_t *)src->buff,
					dst_pitch, src_pitch, dst_extent.w, dst_extent.h,
					PixelConvert());
			return true;
		case ImageMode_RGBA8888:
			CopyPixels32((uint32_t *)dst->buff, (const uint32_t *)src->buff, dst_pitch,
				     src_pitch, dst_extent.w, dst_extent.h, PixelCopy());
			return true;
		case ImageMode_BGRA8888:
			ConvertBGRA8888((uint8_t *)dst->buff, (const uint8_t *)src->buff,
					dst_pitch, src_pitch, dst_extent.w, dst_extent.h,
					PixelConvert());
			return true;
		case ImageMode_DXT1:
		case ImageMode_DXT3:
		case ImageMode_DXT5:
			DecompressDxt((uint32_t *)dst->buff, (const uint8_t *)src->buff, dst_pitch,
				      src_pitch, dst_extent.w, dst_extent.h, src_mode, quality);
			return true;
		case ImageMode_index4:
			ConvertCLUT4((uint32_t *)dst->buff, (const uint8_t *)src->buff, dst_pitch,
				     src_pitch, dst_extent.w, dst_extent.h, PixelLookup(palette));
			return true;
		case ImageMode_index8:
			ConvertCLUT8((uint32_t *)dst->buff, (const uint8_t *)src->buff, dst_pitch,
				     src_pitch, dst_extent.w, dst_extent.h, PixelLookup(palette));
			return true;
		default:
			return false;
		}
	}
	case ImageMode_DXT1:
		return CompressDxt1(dst, src, clut, dst_pitch, src_pitch, &dst_extent, &src_extent,
				    src_mode, format, quality);
	case ImageMode_DXT3:
		return CompressDxt3(dst, src, clut, dst_pitch, src_pitch, &dst_extent, &src_extent,
				    src_mode, format, quality);
	case ImageMode_DXT5:
		return CompressDxt5(dst, src, clut, dst_pitch, src_pitch, &dst_extent, &src_extent,
				    src_mode, format, quality);
	default:
		return false;
	}
}

bool filter::Copy(ImageBuffer *dst, const ImageBuffer *src, int dst_pitch, int src_pitch,
		  ImageExtent extent, ImageMode mode, ImageFormat format)
{
	if (dst->buff == NULL ||
	    dst->size < ImageAttribute::CalcMemSize(extent.w, extent.h, 1, 1, mode, format))
		return false;

	switch (mode) {
	case ImageMode_XBGR0565:
	case ImageMode_ABGR1555:
	case ImageMode_ABGR4444:
	case ImageMode_index16:
		CopyPixels16((uint16_t *)dst->buff, (const uint16_t *)src->buff, dst_pitch,
			     src_pitch, extent.w, extent.h, PixelCopy());
		return true;
	case ImageMode_RGBA8888:
	case ImageMode_BGRA8888:
	case ImageMode_index32:
		CopyPixels32((uint32_t *)dst->buff, (const uint32_t *)src->buff, dst_pitch,
			     src_pitch, extent.w, extent.h, PixelCopy());
		return true;
	case ImageMode_DXT1:
	case ImageMode_index4:
		CopyPixels4((uint8_t *)dst->buff, (const uint8_t *)src->buff, dst_pitch,
			    src_pitch, extent.w, extent.h, PixelCopy());
		return true;
	case ImageMode_DXT3:
	case ImageMode_DXT5:
	case ImageMode_index8:
		CopyPixels8((uint8_t *)dst->buff, (const uint8_t *)src->buff, dst_pitch,
			    src_pitch, extent.w, extent.h, PixelCopy());
		return true;
	default:
		return false;
	}
}

bool filter::CopyWithEdge(ImageBuffer *dst, const ImageBuffer *src, const ImageBuffer *clut,
			  int dst_pitch, int src_pitch, ImageExtent extent, ImageMode mode,
			  ImageFormat format, uint32_t edge)
{
	if (dst->buff == NULL ||
	    dst->size < ImageAttribute::CalcMemSize(extent.w, extent.h, 1, 1, mode, format))
		return false;

	const uint32_t *palette = NULL;
	if (clut != NULL)
		palette = (const uint32_t *)clut->buff;
	int inner_h = extent.h - 2 * edge;
	int inner_w = extent.w - 2 * edge;

	switch (mode) {
	case ImageMode_XBGR0565:
		ConvertRGB565WithEdge(dst->buff, (const uint16_t *)src->buff, dst_pitch,
				      src_pitch, inner_w, inner_h, PixelConvert(), edge);
		break;
	case ImageMode_ABGR1555:
		ConvertRGBA5551WithEdge(dst->buff, (const uint16_t *)src->buff, dst_pitch,
					src_pitch, inner_w, inner_h, PixelConvert(), edge);
		break;
	case ImageMode_ABGR4444:
		ConvertRGBA4444WithEdge(dst->buff, (const uint16_t *)src->buff, dst_pitch,
					src_pitch, inner_w, inner_h, PixelConvert(), edge);
		break;
	case ImageMode_RGBA8888:
		CopyPixels32WithEdge(dst->buff, (const uint32_t *)src->buff, dst_pitch,
				     src_pitch, inner_w, inner_h, PixelCopy(), edge);
		break;
	case ImageMode_BGRA8888:
		ConvertBGRA8888WithEdge(dst->buff, (const uint8_t *)src->buff, dst_pitch,
					src_pitch, inner_w, inner_h, PixelConvert(), edge);
		break;
	case ImageMode_index4:
		ConvertCLUT4WithEdge(dst->buff, (const uint8_t *)src->buff, dst_pitch,
				     src_pitch, inner_w, inner_h, PixelLookup(palette), edge);
		break;
	case ImageMode_index8:
		ConvertCLUT8WithEdge(dst->buff, (const uint8_t *)src->buff, dst_pitch,
				     src_pitch, inner_w, inner_h, PixelLookup(palette), edge);
		break;
	default:
		return false;
	}

	uint32_t pitch = (uint32_t)dst_pitch;
	uint32_t stride = pitch / 4;

	/* The left and right borders take their row's outermost pixel. */
	uint32_t *p = (uint32_t *)dst->buff + stride * edge;
	for (int y = 0; y < inner_h; y++) {
		uint32_t left = p[edge] & 0xFFFFFF;
		for (uint32_t x = 0; x < edge; x++)
			p[x] = left;
		p += edge + inner_w;
		uint32_t right = p[-1] & 0xFFFFFF;
		for (uint32_t x = 0; x < edge; x++)
			p[x] = right;
		p += edge;
	}

	/* The top border repeats the first row. */
	uint32_t *top = (uint32_t *)dst->buff + (stride * edge - stride);
	for (int x = 0; x < extent.w; x++, top++)
		*top = top[stride] & 0xFFFFFF;
	top -= extent.w;
	uint32_t *row = (uint32_t *)dst->buff;
	for (uint32_t y = 1; y < edge; y++) {
		sce_paf_memcpy(row, top, pitch);
		row += stride;
	}

	/* And the bottom border the last. */
	uint32_t *bottom = (uint32_t *)dst->buff + (stride * (edge + inner_h) - stride);
	for (int x = 0; x < extent.w; x++, bottom++)
		bottom[stride] = *bottom & 0xFFFFFF;
	row = bottom + stride;
	for (uint32_t y = 1; y < edge; y++) {
		sce_paf_memcpy(row, bottom, pitch);
		row += stride;
	}
	return true;
}

bool filter::Crop(ImageBuffer *dst, const ImageBuffer *src, int dst_pitch, int src_pitch,
		  ImageExtent extent, ImageMode mode, ImageFormat format, ImageRect rect)
{
	if (dst->buff == NULL ||
	    dst->size < ImageAttribute::CalcMemSize(extent.w, extent.h, 1, 1, mode, format))
		return false;

	const uint8_t *s = (const uint8_t *)src->buff + rect.origin.y * src_pitch;
	s += (int)((float)ImageAttribute::GetPixelSize(mode) * 0.125f * (float)rect.origin.x);
	CopyPixels32((uint32_t *)dst->buff, (const uint32_t *)s, dst_pitch, src_pitch, extent.w,
		     extent.h, PixelCopy());
	return true;
}

bool filter::FadeWithEdge(ImageBuffer *dst, const ImageBuffer *src, int dst_pitch, int src_pitch,
			  ImageExtent extent, ImageMode mode, ImageFormat format)
{
	if (dst->buff == NULL ||
	    dst->size < ImageAttribute::CalcMemSize(extent.w, extent.h, 1, 1, mode, format) ||
	    (mode != ImageMode_RGBA8888 && mode != ImageMode_BGRA8888))
		return false;

	CopyPixels32((uint32_t *)dst->buff, (const uint32_t *)src->buff, dst_pitch, src_pitch,
		     extent.w, extent.h, PixelCopy());

	uint32_t *p = (uint32_t *)dst->buff;
	for (int x = 0; x < extent.w; x++)
		((uint8_t *)&p[x])[3] = 0;
	p += extent.w;
	for (int y = 1; y < extent.h - 1; y++) {
		((uint8_t *)&p[0])[3] = 0;
		((uint8_t *)&p[extent.w - 1])[3] = 0;
		p += extent.w;
	}
	for (int x = 0; x < extent.w; x++)
		((uint8_t *)&p[x])[3] = 0;
	return true;
}

bool filter::Flip(ImageBuffer *dst, const ImageBuffer *src, int dst_pitch, int src_pitch,
		  ImageExtent extent, ImageMode mode, ImageFormat format, ImageFlipType type)
{
	if (dst->buff == NULL)
		return false;
	if (dst->size < ImageAttribute::CalcMemSize(extent.w, extent.h, 1, 1, mode, format))
		return false;

	switch (mode) {
	case ImageMode_XBGR0565:
	case ImageMode_ABGR1555:
	case ImageMode_ABGR4444:
	case ImageMode_index16:
		return FlipImage((uint16_t *)dst->buff, (const uint16_t *)src->buff, dst_pitch,
				 src_pitch, extent.w, extent.h, mode, type);
	case ImageMode_RGBA8888:
	case ImageMode_BGRA8888:
	case ImageMode_index32:
		return FlipImage((uint32_t *)dst->buff, (const uint32_t *)src->buff, dst_pitch,
				 src_pitch, extent.w, extent.h, mode, type);
	case ImageMode_index4:
		return false;
	case ImageMode_index8:
		return FlipImage((uint8_t *)dst->buff, (const uint8_t *)src->buff, dst_pitch,
				 src_pitch, extent.w, extent.h, mode, type);
	default:
		return false;
	}
}

bool filter::Paste(ImageBuffer *dst, const ImageBuffer *src, const ImageBuffer *overlay,
		   int dst_pitch, int src_pitch, int overlay_pitch, ImageExtent extent,
		   ImageExtent overlay_extent, ImageMode mode, ImageFormat format,
		   const ImagePoint &pos)
{
	if (dst->buff == NULL ||
	    dst->size < ImageAttribute::CalcMemSize(extent.w, extent.h, 1, 1, mode, format) ||
	    (mode != ImageMode_BGRA8888 && mode != ImageMode_RGBA8888))
		return false;

	CopyPixels32((uint32_t *)dst->buff, (const uint32_t *)src->buff, dst_pitch, src_pitch,
		     extent.w, extent.h, PixelCopy());

	uint32_t bpp = ImageAttribute::GetPixelSize(mode) >> 3;
	uint8_t *d = (uint8_t *)dst->buff;
	int left = pos.x;
	int right = pos.x + overlay_extent.w;
	const uint8_t *s = (const uint8_t *)overlay->buff;

	if (right < 0 || left >= extent.w)
		return true;
	int top = pos.y;
	int bottom = overlay_extent.h + pos.y;
	if (bottom < 0 || top >= extent.h)
		return true;

	uint32_t size;
	if (left < 0 && right <= extent.w) {
		s += -left * bpp;
		size = bpp * (overlay_extent.w - -left);
	} else if (left >= 0 && right > extent.w) {
		d += left * bpp;
		size = bpp * (overlay_extent.w - (right - extent.w));
	} else if (left < 0 && right > extent.w) {
		s += -left * bpp;
		size = bpp * (overlay_extent.w - -left + (right - extent.w));
	} else {
		d += left * bpp;
		size = overlay_extent.w * bpp;
	}

	uint32_t first, last;
	if (top < 0 && bottom < extent.h) {
		s += -top * overlay_pitch;
		first = 0;
		last = bottom;
	} else if (top >= 0 && bottom >= extent.h) {
		first = top;
		d += top * dst_pitch;
		last = extent.h;
	} else if (top < 0 && bottom >= extent.h) {
		s += -top * overlay_pitch;
		first = 0;
		last = extent.h;
	} else {
		first = top;
		d += top * dst_pitch;
		last = bottom;
	}
	for (uint32_t y = first; y < last; y++) {
		sce_paf_memcpy(d, s, size);
		d += dst_pitch;
		s += overlay_pitch;
	}
	return true;
}

void filter::ResizeHalf(void *dst, const void *src, int dst_pitch, int src_pitch, int w, int h)
{
	if (dst != NULL && src != NULL && (w & 3) == 0) {
		uint8_t *d = (uint8_t *)dst;
		const uint8_t *s = (const uint8_t *)src;

		for (int y = 0; y < h / 2; y++) {
			const uint8_t *s0 = s;
			const uint8_t *s1 = s + src_pitch;
			uint8_t *q = d;

			for (int x = 0; x < w / 4; x++) {
				uint8x8_t a = vrhadd_u8(vld1_u8(s0), vld1_u8(s1));
				uint8x8_t b = vrhadd_u8(vld1_u8(s0 + 8), vld1_u8(s1 + 8));
				int32x2x2_t t = vtrn_s32(vreinterpret_s32_u8(a), vreinterpret_s32_u8(b));

				s0 += 16;
				s1 += 16;
				vst1_u8(q, vrhadd_u8(vreinterpret_u8_s32(t.val[0]),
						     vreinterpret_u8_s32(t.val[1])));
				q += 8;
			}
			d += dst_pitch;
			s += 2 * src_pitch;
		}
	} else {
		uint8_t *d = (uint8_t *)dst;
		const uint8_t *s = (const uint8_t *)src;

		for (int y = 0; y < h / 2; y++) {
			uint8_t *q = d;
			const uint8_t *s0 = s;
			const uint8_t *s1 = s + src_pitch;

			for (int x = 0; x < w / 2; x++) {
				q[0] = (s0[0] + s0[4] + s1[0] + s1[4]) / 4;
				q[1] = (s0[1] + s0[5] + s1[1] + s1[5]) / 4;
				q[2] = (s0[2] + s0[6] + s1[2] + s1[6]) / 4;
				q[3] = (s0[3] + s0[7] + s1[3] + s1[7]) / 4;
				q += 4;
				s0 += 8;
				s1 += 8;
			}
			d += dst_pitch;
			s += 2 * src_pitch;
		}
	}
}

bool filter::ResizeHalf(ImageBuffer *dst, const ImageBuffer *src, int dst_pitch, int src_pitch,
			ImageExtent extent, ImageMode mode)
{
	if (dst->buff == NULL || dst->size < (uint32_t)(extent.w / 2 * (extent.h / 2) * 4) ||
	    (mode != ImageMode_RGBA8888 && mode != ImageMode_BGRA8888))
		return false;
	ResizeHalf(dst->buff, src->buff, dst_pitch, src_pitch, extent.w, extent.h);
	return true;
}

/* ResizeQuarter without NEON. */
static __attribute__((noinline)) void ResizeQuarterScalar(void *dst, const void *src,
							  int dst_pitch, int src_pitch, int w,
							  int h)
{
	uint8_t *d = (uint8_t *)dst;
	const uint8_t *s = (const uint8_t *)src;

	for (int y = 0; y < h / 4; y++) {
		const uint8_t *s0 = s;
		uint8_t *q = d;
		const uint8_t *s1 = s + src_pitch;
		const uint8_t *s2 = s + 2 * src_pitch;
		const uint8_t *s3 = s + 3 * src_pitch;

		for (int x = 0; x < w / 4; x++) {
			q[0] = (s0[0] + s0[4] + s0[8] + s0[12] + s1[0] + s1[4] + s1[8] + s1[12] +
				s2[0] + s2[4] + s2[8] + s2[12] + s3[0] + s3[4] + s3[8] + s3[12]) / 16;
			q[1] = (s0[1] + s0[5] + s0[9] + s0[13] + s1[1] + s1[5] + s1[9] + s1[13] +
				s2[1] + s2[5] + s2[9] + s2[13] + s3[1] + s3[5] + s3[9] + s3[13]) / 16;
			q[2] = (s0[2] + s0[6] + s0[10] + s0[14] + s1[2] + s1[6] + s1[10] + s1[14] +
				s2[2] + s2[6] + s2[10] + s2[14] + s3[2] + s3[6] + s3[10] + s3[14]) / 16;
			q[3] = (s0[3] + s0[7] + s0[11] + s0[15] + s1[3] + s1[7] + s1[11] + s1[15] +
				s2[3] + s2[7] + s2[11] + s2[15] + s3[3] + s3[7] + s3[11] + s3[15]) / 16;
			q += 4;
			s0 += 16;
			s1 += 16;
			s2 += 16;
			s3 += 16;
		}
		s += 4 * src_pitch;
		d += dst_pitch;
	}
}

void filter::ResizeQuarter(void *dst, const void *src, int dst_pitch, int src_pitch, int w,
			   int h)
{
	if (dst == NULL || src == NULL || (w & 7) != 0) {
		ResizeQuarterScalar(dst, src, dst_pitch, src_pitch, w, h);
		return;
	}

	uint8_t *d = (uint8_t *)dst;
	const uint8_t *s = (const uint8_t *)src;

	for (int y = 0; y < h / 4; y++) {
		const uint8_t *s0 = s;
		const uint8_t *s1 = s + src_pitch;
		uint8_t *q = d;
		const uint8_t *s2 = s + 2 * src_pitch;
		const uint8_t *s3 = s + 3 * src_pitch;

		for (int x = 0; x < w / 8; x++) {
			uint8x8_t p0 = vrhadd_u8(vrhadd_u8(vrhadd_u8(vld1_u8(s0), vld1_u8(s1)),
							   vld1_u8(s2)), vld1_u8(s3));
			uint8x8_t p1 = vrhadd_u8(vrhadd_u8(vrhadd_u8(vld1_u8(s0 + 8), vld1_u8(s1 + 8)),
							   vld1_u8(s2 + 8)), vld1_u8(s3 + 8));
			uint8x8_t p2 = vrhadd_u8(vrhadd_u8(vrhadd_u8(vld1_u8(s0 + 16),
								     vld1_u8(s1 + 16)),
							   vld1_u8(s2 + 16)), vld1_u8(s3 + 16));
			uint8x8_t p3 = vrhadd_u8(vrhadd_u8(vrhadd_u8(vld1_u8(s0 + 24),
								     vld1_u8(s1 + 24)),
							   vld1_u8(s2 + 24)), vld1_u8(s3 + 24));
			uint8x8_t a = vrhadd_u8(p0, p1);
			uint8x8_t b = vrhadd_u8(p2, p3);
			int32x2x2_t t = vtrn_s32(vreinterpret_s32_u8(a), vreinterpret_s32_u8(b));

			s0 += 32;
			s1 += 32;
			s2 += 32;
			s3 += 32;
			vst1_u8(q, vrhadd_u8(vreinterpret_u8_s32(t.val[0]),
					     vreinterpret_u8_s32(t.val[1])));
			q += 8;
		}
		d += dst_pitch;
		s += 4 * src_pitch;
	}
}

bool filter::ResizeQuarter(ImageBuffer *dst, const ImageBuffer *src, int dst_pitch,
			   int src_pitch, ImageExtent extent, ImageMode mode)
{
	if (dst->buff == NULL || dst->size < (uint32_t)(extent.w / 4 * (extent.h / 4) * 4) ||
	    (mode != ImageMode_RGBA8888 && mode != ImageMode_BGRA8888))
		return false;
	ResizeQuarter(dst->buff, src->buff, dst_pitch, src_pitch, extent.w, extent.h);
	return true;
}

static void FreeContribList(ContribList *contrib, int n, HeapAllocator *allocator)
{
	if (contrib == NULL)
		return;
	for (int i = 0; i < n; i++) {
		if (contrib[i].p != NULL) {
			Free(allocator, contrib[i].p);
			contrib[i].p = NULL;
		}
	}
	Free(allocator, contrib);
}

/*
 * The vertical pass: tmp holds the image transposed, each of its count rows a
 * destination column, written from the right.
 */
static void ApplyContribV(int src_n, int dst_n, int dst_pitch, const uint8_t *tmp, int count,
			  uint8_t *dst, const ContribList *contrib)
{
	const int16x4_t zero = vdup_n_s16(0);
	const int16x4_t max = vdup_n_s16(255);
	const uint8x8_t narrow = vcreate_u8(0xFFFFFFFF06040200ULL);

	for (int y = 0; y < count; y++) {
		uint32_t *out = (uint32_t *)(dst + dst_pitch - 4 * (y + 1));
		const ContribList *c = contrib;

		for (int k = 0; k < dst_n; k++, c++) {
			int16x4_t sum = zero;
			uint8_t pixel[8];

			for (int m = 0; m < c->n; m++) {
				uint8x8x2_t z = vzip_u8(vreinterpret_u8_s16(zero),
							vld1_u8(tmp + c->p[m].pixel));
				int16x4_t weight = vld1_dup_s16((const int16_t *)&c->p[m].weight.i);
				int16x4_t value = vreinterpret_s16_u16(
					vshr_n_u16(vreinterpret_u16_u8(z.val[0]), 1));

				sum = vadd_s16(vqrdmulh_s16(value, weight), sum);
			}
			sum = vmax_s16(vmin_s16(vshr_n_s16(sum, 6), max), zero);
			vst1_u8(pixel, vtbl1_u8(vreinterpret_u8_s16(sum), narrow));
			*out = *(uint32_t *)pixel;
			out = (uint32_t *)((uint8_t *)out + dst_pitch);
		}
		tmp += 4 * src_n;
	}
}

static bool GetFilter(ImageResizeType type, FilterFunc *filter, float *support)
{
	switch (type) {
	case ImageResizeType_NearestNeighbor:
	case ImageResizeType_BoxFilter:
		*filter = BoxFilter;
		*support = 0.5f;
		return true;
	case ImageResizeType_Linear:
		*filter = TriangleFilter;
		*support = 1.0f;
		return true;
	case ImageResizeType_BellFilter:
		*filter = BellFilter;
		*support = 1.0f;
		return true;
	case ImageResizeType_HermitFilter:
		*filter = HermiteFilter;
		*support = 1.0f;
		return true;
	case ImageResizeType_B_SplineFilter:
		*filter = BSplineFilter;
		*support = 2.0f;
		return true;
	case ImageResizeType_MitchellFilter:
		*filter = MitchellFilter;
		*support = 2.0f;
		return true;
	case ImageResizeType_Lanczos3Filter:
		*filter = Lanczos3Filter;
		*support = 3.0f;
		return true;
	default:
		return false;
	}
}

/*
 * Each of the dst pixels' contributions from the src pixels, by the filter of
 * type, as byte offsets of RGBA8888 pixels (index true: as pixel indices).
 */
static inline __attribute__((always_inline)) ContribList *CalcContribList(int src, int dst, ImageResizeType type,
					   HeapAllocator *allocator, bool index)
{
	FilterFunc filter = NULL;
	float support;

	GetFilter(type, &filter, &support);
	ContribList *contrib = (ContribList *)Alloc(allocator, 8 * dst);
	if (contrib == NULL) {
		Image::SetErrorType(SCE_PAF_ERROR_IMAGE_OUT_OF_MEMORY);
		return NULL;
	}

	float scale = (float)dst / (float)src;
	if (scale < 1.0f) {
		float width = support / scale;
		float fscale = 1.0f / scale;

		for (int i = 0; i < dst; i++) {
			contrib[i].n = 0;
			contrib[i].p = (Contrib *)Alloc(allocator, 8 * (int)(width * 2.0f + 2.0f));
			if (contrib[i].p == NULL) {
				Image::SetErrorType(SCE_PAF_ERROR_IMAGE_OUT_OF_MEMORY);
				return NULL;
			}

			float center = (float)(i + 0.5) / scale - 0.5;
			float left = sce_paf_ceilf(center - width);
			float right = sce_paf_floorf(center + width);
			float sum = 0.0f;

			for (int j = (int)left; j <= right; j++) {
				float weight = filter((center - (float)j) / fscale) / fscale;
				int n;

				if (j < 0)
					n = 0;
				else if (j >= src)
					n = src - 1;
				else
					n = j;
				int k = contrib[i].n++;
				contrib[i].p[k].pixel = index ? n : n * 4;
				sum += weight;
				contrib[i].p[k].weight.f = weight;
			}
			for (int k = 0; k < contrib[i].n; k++)
				contrib[i].p[k].weight.i =
					(int)(contrib[i].p[k].weight.f * 16384.0f / sum);
		}
	} else {
		for (int i = 0; i < dst; i++) {
			contrib[i].n = 0;
			contrib[i].p = (Contrib *)Alloc(allocator,
							8 * (int)(support * 2.0f + 2.0f));
			if (contrib[i].p == NULL) {
				Image::SetErrorType(SCE_PAF_ERROR_IMAGE_OUT_OF_MEMORY);
				return NULL;
			}

			float center = (float)(i + 0.5) / scale - 0.5;
			float left = sce_paf_ceilf(center - support);
			float right = sce_paf_floorf(center + support);
			float sum = 0.0f;

			for (int j = (int)left; j <= right; j++) {
				float weight = filter(center - (float)j);
				int n;

				if (j < 0)
					n = 0;
				else if (j >= src)
					n = src - 1;
				else
					n = j;
				int k = contrib[i].n++;
				sum += weight;
				contrib[i].p[k].pixel = index ? n : n * 4;
				contrib[i].p[k].weight.f = weight;
			}
			for (int k = 0; k < contrib[i].n; k++)
				contrib[i].p[k].weight.i =
					(int)(contrib[i].p[k].weight.f * 16384.0f / sum);
		}
	}
	return contrib;
}

static __attribute__((noinline)) ContribList *CalcContrib(int src, int dst, ImageResizeType type, HeapAllocator *allocator)
{
	return CalcContribList(src, dst, type, allocator, false);
}

/* The horizontal pass: one source row into a column of tmp, written upwards. */
static __attribute__((noinline)) void ApplyContribH(const uint8_t *src, uint8_t *out, int n, int pitch,
			  const ContribList *contrib)
{
	const int16x4_t zero = vdup_n_s16(0);
	const int16x4_t max = vdup_n_s16(255);
	const uint8x8_t narrow = vcreate_u8(0xFFFFFFFF06040200ULL);

	for (int i = 0; i < n; i++, out -= pitch) {
		int16x4_t sum = zero;
		uint8_t pixel[8];

		for (int m = 0; m < contrib->n; m++) {
			uint8x8x2_t z = vzip_u8(vreinterpret_u8_s16(zero),
						vld1_u8(src + contrib->p[m].pixel));
			int16x4_t weight = vld1_dup_s16((const int16_t *)&contrib->p[m].weight.i);
			int16x4_t value = vreinterpret_s16_u16(
				vshr_n_u16(vreinterpret_u16_u8(z.val[0]), 1));

			sum = vadd_s16(vqrdmulh_s16(value, weight), sum);
		}
		contrib++;
		sum = vmax_s16(vmin_s16(vshr_n_s16(sum, 6), max), zero);
		vst1_u8(pixel, vtbl1_u8(vreinterpret_u8_s16(sum), narrow));
		*(uint32_t *)out = *(uint32_t *)pixel;
	}
}

static bool ResizeFiltered(uint8_t *dst, const uint8_t *src, int dst_pitch, int src_pitch,
			   int src_w, int src_h, ImageMode mode, int dst_w, int dst_h,
			   ImageResizeType type, HeapAllocator *allocator)
{
	ContribList *contrib = CalcContrib(src_w, dst_w, type, allocator);
	if (contrib == NULL) {
		Image::SetErrorType(SCE_PAF_ERROR_IMAGE_OUT_OF_MEMORY);
		return false;
	}

	uint8_t *tmp = (uint8_t *)Alloc(allocator, 4 * src_h * dst_w);
	if (tmp == NULL) {
		Image::SetErrorType(SCE_PAF_ERROR_IMAGE_OUT_OF_MEMORY);
		FreeContribList(contrib, dst_w, allocator);
		return false;
	}

	uint8_t *column = tmp + 4 * (src_h * dst_w - src_h);
	for (int y = 0; y < src_h; y++) {
		ApplyContribH(src, column, dst_w, 4 * src_h, contrib);
		src += src_pitch;
		column += 4;
	}
	FreeContribList(contrib, dst_w, allocator);

	contrib = CalcContrib(src_h, dst_h, type, allocator);
	if (contrib == NULL) {
		Image::SetErrorType(SCE_PAF_ERROR_IMAGE_OUT_OF_MEMORY);
		Free(allocator, tmp);
		return false;
	}
	ApplyContribV(src_h, dst_h, dst_pitch, tmp, dst_w, dst, contrib);
	FreeContribList(contrib, dst_h, allocator);
	Free(allocator, tmp);
	return true;
}

bool filter::Resize(ImageBuffer *dst, const ImageBuffer *src, int dst_pitch, int src_pitch,
		    ImageExtent extent, ImageMode mode, ImageFormat format, ImageExtent dst_extent,
		    ImageResizeType type, HeapAllocator *allocator)
{
	if (dst->buff == NULL)
		return false;
	if (dst->size <
	    ImageAttribute::CalcMemSize(dst_extent.w, dst_extent.h, 1, 1, mode, format))
		return false;

	if (type != ImageResizeType_NearestNeighbor) {
		switch (mode) {
		case ImageMode_RGBA8888:
		case ImageMode_BGRA8888:
			return ResizeFiltered((uint8_t *)dst->buff, (const uint8_t *)src->buff,
					      dst_pitch, src_pitch, extent.w, extent.h, mode,
					      dst_extent.w, dst_extent.h, type, allocator);
		default:
			return false;
		}
	}

	switch (mode) {
	case ImageMode_XBGR0565:
	case ImageMode_ABGR1555:
	case ImageMode_ABGR4444:
	case ImageMode_index16:
		return ResizeNearestNeighbor((uint16_t *)dst->buff, (const uint16_t *)src->buff,
					     dst_pitch, src_pitch, extent.w, extent.h, mode,
					     dst_extent.w, dst_extent.h, type, allocator);
	case ImageMode_RGBA8888:
	case ImageMode_BGRA8888:
	case ImageMode_index32:
		return ResizeNearestNeighbor((uint32_t *)dst->buff, (const uint32_t *)src->buff,
					     dst_pitch, src_pitch, extent.w, extent.h, mode,
					     dst_extent.w, dst_extent.h, type, allocator);
	case ImageMode_index4: {
		uint8_t *d = (uint8_t *)dst->buff;
		const uint8_t *s = (const uint8_t *)src->buff;
		float scale_y = (float)extent.h / (float)dst_extent.h;
		int *xs = (int *)Alloc(allocator, 4 * dst_extent.w);
		if (xs == NULL) {
			Image::SetErrorType(SCE_PAF_ERROR_IMAGE_OUT_OF_MEMORY);
			return false;
		}

		for (int x = 0; x < dst_extent.w; x++)
			xs[x] = (int)(((float)x + 0.5f) * ((float)extent.w / (float)dst_extent.w));

		int prev = -1;
		for (int y = 0; y < dst_extent.h; y++) {
			int sy = (int)(((float)y + 0.5f) * scale_y);

			if (sy == prev) {
				sce_paf_memcpy(d, d - (uint32_t)dst_pitch, (uint32_t)dst_pitch);
			} else {
				for (int x = 0; x < dst_extent.w; x++) {
					uint8_t b = s[sy * src_pitch + (xs[x] >> 1)];
					uint8_t c = b & 0xF;

					if (xs[x] & 1)
						c = b >> 4;
					if (x & 1)
						d[x >> 1] = (d[x >> 1] & 0xF) | (c << 4);
					else
						d[x >> 1] = (d[x >> 1] & 0xF0) | c;
				}
			}
			prev = sy;
			d += (uint32_t)dst_pitch;
		}
		Free(allocator, xs);
		return true;
	}
	case ImageMode_index8:
		return ResizeNearestNeighbor((uint8_t *)dst->buff, (const uint8_t *)src->buff,
					     dst_pitch, src_pitch, extent.w, extent.h, mode,
					     dst_extent.w, dst_extent.h, type, allocator);
	default:
		return false;
	}
}

static float BoxFilter(float t)
{
	if (t > -0.5f && t <= 0.5f)
		return 1.0f;
	return 0.0f;
}

static float TriangleFilter(float t)
{
	if (t < 0.0f)
		t = -t;
	if (t < 1.0f)
		return 1.0f - t;
	return 0.0f;
}

static float BellFilter(float t)
{
	if (t < 0.0f)
		t = -t;
	/* Graphics Gems' bound, 1 / 2, is integer division: 0. */
	if (t < 1 / 2)
		return 0.75f - t * t;
	if (t < 1.5f) {
		t = t - 1.5f;
		return 0.5f * t * t;
	}
	return 0.0f;
}

static float HermiteFilter(float t)
{
	if (t < 0.0f)
		t = -t;
	if (t < 1.0f)
		return (2.0f * t - 3.0f) * t * t + 1.0f;
	return 0.0f;
}

static float BSplineFilter(float t)
{
	float tt;

	if (t < 0.0f)
		t = -t;
	if (t < 1.0f) {
		tt = t * t;
		return 0.5f * tt * t - tt + 2.0f / 3.0f;
	}
	if (t < 2.0f) {
		t = 2.0f - t;
		return 1.0f / 6.0f * (t * t * t);
	}
	return 0.0f;
}

/* Mitchell and Netravali's filter with B = C = 1/3. */
static float MitchellFilter(float t)
{
	float tt = t * t;

	if (t < 0.0f)
		t = -t;
	if (t < 1.0f) {
		t = 7.0f * (t * tt) + -12.0f * tt + 5.3333335f;
		return t / 6.0f;
	}
	if (t < 2.0f) {
		t = -2.3333333f * (t * tt) + 12.0f * tt + -20.0f * t + 10.666667f;
		return t / 6.0f;
	}
	return 0.0f;
}

static inline float Sinc(float x)
{
	x *= 3.1415927f;
	if (x != 0.0f)
		return sce_paf_sinf(x) / x;
	return 1.0f;
}

static float Lanczos3Filter(float t)
{
	if (t < 0.0f)
		t = -t;
	if (t < 3.0f)
		return Sinc(t) * Sinc(t / 3.0f);
	return 0.0f;
}

/* Pixel x of a row in mode as RGBA8888 bytes; clut is the palette of CLUT4 and CLUT8. */
static void GetPixel(ImageMode mode, const uint8_t *src, uint32_t x, const uint8_t *clut,
		     uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a)
{
	switch (mode) {
	case ImageMode_XBGR0565: {
		uint16_t c = src[2 * x] | (src[2 * x + 1] << 8);

		*r = ((c & 0x1F) << 3) | ((c >> 2) & 7);
		*g = (((c >> 5) & 0x3F) << 2) | ((c >> 9) & 3);
		*b = (((c >> 11) & 0x1F) << 3) | (c >> 13);
		*a = 0xFF;
		break;
	}
	case ImageMode_ABGR1555: {
		uint16_t c = src[2 * x] | (src[2 * x + 1] << 8);

		*r = ((c & 0x1F) << 3) | ((c >> 2) & 7);
		*g = (((c >> 5) & 0x1F) << 3) | ((c >> 7) & 7);
		*b = (((c >> 10) & 0x1F) << 3) | ((c >> 12) & 7);
		*a = (c & 0x8000) ? 0xFF : 0;
		break;
	}
	case ImageMode_ABGR4444: {
		uint16_t c = src[2 * x] | (src[2 * x + 1] << 8);

		*r = (c & 0xF) | ((c & 0xF) << 4);
		*g = (c & 0xF0) | ((c >> 4) & 0xF);
		*b = ((c >> 8) & 0xF) | (((c >> 8) & 0xF) << 4);
		*a = (c >> 12) | (((c >> 12) & 0xF) << 4);
		break;
	}
	case ImageMode_RGBA8888: {
		const uint8_t *p = &src[4 * x];

		*r = p[0];
		*g = p[1];
		*b = p[2];
		*a = p[3];
		break;
	}
	case ImageMode_BGRA8888: {
		const uint8_t *p = &src[4 * x];

		*r = p[2];
		*g = p[1];
		*b = p[0];
		*a = p[3];
		break;
	}
	case ImageMode_index4: {
		uint8_t c = src[x >> 1];
		const uint8_t *p = &clut[4 * (c & 0xF)];

		if (x & 1)
			p = &clut[4 * (c >> 4)];
		*r = p[0];
		*g = p[1];
		*b = p[2];
		*a = p[3];
		break;
	}
	case ImageMode_index8: {
		const uint8_t *p = &clut[4 * src[x]];

		*r = p[0];
		*g = p[1];
		*b = p[2];
		*a = p[3];
		break;
	}
	default:
		*r = 0;
		*g = 0;
		*b = 0;
		*a = 0;
		break;
	}
}

static __attribute__((noinline)) ContribList *CalcContribIndex(int src, int dst, ImageResizeType type,
				     HeapAllocator *allocator)
{
	return CalcContribList(src, dst, type, allocator, true);
}

/* Converts src (in any mode) to RGBA8888 while resizing it. */
static bool ConvertResize(ImageBuffer *dst, const ImageBuffer *src, const ImageBuffer *clut,
			  uint32_t dst_pitch, int src_pitch, const ImageExtent *dst_extent,
			  const ImageExtent *src_extent, ImageMode src_mode, ImageFormat format,
			  ImageMode dst_mode, ImageResizeType type, HeapAllocator *allocator)
{
	if (dst->buff == NULL ||
	    dst->size < ImageAttribute::CalcMemSize(dst_extent->w, dst_extent->h, 1, 1,
						      dst_mode, format))
		return false;

	if (type == ImageResizeType_NearestNeighbor) {
		switch (src_mode) {
		case ImageMode_XBGR0565:
		case ImageMode_ABGR1555:
		case ImageMode_ABGR4444:
			return ConvertNearestNeighbor((uint32_t *)dst->buff,
						      (const uint16_t *)src->buff, NULL,
						      dst_pitch, src_pitch, dst_extent->w,
						      dst_extent->h, src_extent->w, src_extent->h,
						      src_mode, dst_mode, allocator);
		case ImageMode_RGBA8888:
		case ImageMode_BGRA8888:
			return ConvertNearestNeighbor((uint32_t *)dst->buff,
						      (const uint32_t *)src->buff, NULL,
						      dst_pitch, src_pitch, dst_extent->w,
						      dst_extent->h, src_extent->w, src_extent->h,
						      src_mode, dst_mode, allocator);
		case ImageMode_index4: {
			const uint8_t *palette = (const uint8_t *)clut->buff;
			int dst_w = dst_extent->w;
			int dst_h = dst_extent->h;
			const uint8_t *s = (const uint8_t *)src->buff;
			uint8_t *d = (uint8_t *)dst->buff;
			float scale_x = (float)src_extent->w / (float)dst_w;
			float scale_y = (float)src_extent->h / (float)dst_h;
			uint32_t *xs = (uint32_t *)Alloc(allocator, 4 * dst_w);
			if (xs == NULL) {
				Image::SetErrorType(SCE_PAF_ERROR_IMAGE_OUT_OF_MEMORY);
				return false;
			}

			for (int x = 0; x < dst_w; x++)
				xs[x] = (int)(((float)x + 0.5f) * scale_x);

			int prev = -1;
			for (int y = 0; y < dst_h; y++) {
				int sy = (int)(((float)y + 0.5f) * scale_y);

				if (sy == prev) {
					sce_paf_memcpy(d, d - dst_pitch, dst_pitch);
				} else {
					const uint8_t *row = s + sy * src_pitch;
					uint32_t *out = (uint32_t *)d;

					for (int x = 0; x < dst_w; x++) {
						uint8_t r, g, b, a;

						GetPixel(src_mode, row, xs[x], palette, &r, &g, &b, &a);
						*out++ = r | (g << 8) | (b << 16) | (a << 24);
					}
				}
				prev = sy;
				d += dst_pitch;
			}
			Free(allocator, xs);
			return true;
		}
		case ImageMode_index8:
			return ConvertNearestNeighbor((uint32_t *)dst->buff,
						      (const uint8_t *)src->buff,
						      (const uint8_t *)clut->buff, dst_pitch,
						      src_pitch, dst_extent->w, dst_extent->h,
						      src_extent->w, src_extent->h, src_mode,
						      dst_mode, allocator);
		default:
			return false;
		}
	}

	switch (src_mode) {
	case ImageMode_XBGR0565:
	case ImageMode_ABGR1555:
	case ImageMode_ABGR4444:
	case ImageMode_RGBA8888:
	case ImageMode_BGRA8888:
	case ImageMode_index4:
	case ImageMode_index8:
		break;
	default:
		return false;
	}

	const uint8_t *s = (const uint8_t *)src->buff;
	const uint8_t *palette = NULL;
	if (clut != NULL)
		palette = (const uint8_t *)clut->buff;
	int dst_w = dst_extent->w;
	int dst_h = dst_extent->h;
	int src_w = src_extent->w;
	int src_h = src_extent->h;

	ContribList *contrib = CalcContribIndex(src_w, dst_w, type, allocator);
	if (contrib == NULL) {
		Image::SetErrorType(SCE_PAF_ERROR_IMAGE_OUT_OF_MEMORY);
		FreeContribList(contrib, dst_w, allocator);
		return false;
	}
	uint8_t *tmp = (uint8_t *)Alloc(allocator, 4 * src_h * dst_w);
	if (tmp == NULL) {
		Image::SetErrorType(SCE_PAF_ERROR_IMAGE_OUT_OF_MEMORY);
		FreeContribList(contrib, dst_w, allocator);
		return false;
	}

	/* Horizontally into tmp, transposed, as ApplyContribH does. */
	const int16x4_t zero = vdup_n_s16(0);
	const int16x4_t max = vdup_n_s16(255);
	const uint8x8_t narrow = vcreate_u8(0xFFFFFFFF06040200ULL);
	uint8_t *column = tmp + 4 * (src_h * dst_w - src_h);
	uint32x2_t value;

	for (int y = 0; y < src_h; y++) {
		uint8_t *out = column;
		const ContribList *c = contrib;

		for (int x = 0; x < dst_w; x++, c++) {
			int16x4_t sum = zero;
			uint8_t pixel[8];

			for (int m = 0; m < c->n; m++) {
				uint8_t r, g, b, a;

				GetPixel(src_mode, s, c->p[m].pixel, palette, &r, &g, &b, &a);
				value = vset_lane_u32(r | (g << 8) | (b << 16) | (a << 24), value, 0);
				uint8x8x2_t z = vzip_u8(vreinterpret_u8_s16(zero),
							vreinterpret_u8_u32(value));
				int16x4_t weight = vld1_dup_s16((const int16_t *)&c->p[m].weight.i);
				int16x4_t v = vreinterpret_s16_u16(
					vshr_n_u16(vreinterpret_u16_u8(z.val[0]), 1));

				sum = vadd_s16(vqrdmulh_s16(v, weight), sum);
			}
			sum = vmax_s16(vmin_s16(vshr_n_s16(sum, 6), max), zero);
			vst1_u8(pixel, vtbl1_u8(vreinterpret_u8_s16(sum), narrow));
			*(uint32_t *)out = *(uint32_t *)pixel;
			out -= 4 * src_h;
		}
		s += src_pitch;
		column += 4;
	}
	FreeContribList(contrib, dst_w, allocator);

	contrib = CalcContrib(src_h, dst_h, type, allocator);
	if (contrib == NULL) {
		Image::SetErrorType(SCE_PAF_ERROR_IMAGE_OUT_OF_MEMORY);
		FreeContribList(contrib, dst_h, allocator);
		Free(allocator, tmp);
		return false;
	}
	ApplyContribV(src_h, dst_h, dst_pitch, tmp, dst_w, (uint8_t *)dst->buff, contrib);
	FreeContribList(contrib, dst_h, allocator);
	Free(allocator, tmp);
	return true;
}

bool filter::ResizeWithConvertMode(ImageBuffer *dst, const ImageBuffer *src,
				   const ImageBuffer *clut, int dst_pitch, int src_pitch,
				   const ImageExtent &dst_extent, const ImageExtent &src_extent,
				   ImageMode src_mode, ImageFormat format, ImageMode dst_mode,
				   ImageResizeType type, ImageConvertModeType quality,
				   HeapAllocator *allocator)
{
	if (dst_mode != ImageMode_RGBA8888)
		return false;
	return ConvertResize(dst, src, clut, dst_pitch, src_pitch, &dst_extent, &src_extent,
			     src_mode, format, ImageMode_RGBA8888, type, allocator);
}

bool filter::Rotate(ImageBuffer *dst, const ImageBuffer *src, int dst_pitch, int src_pitch,
		    ImageExtent extent, ImageMode mode, ImageFormat format, ImageRotateType type)
{
	if (type == ImageRotateType_Degree90 || type == ImageRotateType_Degree270) {
		if (dst->buff == NULL)
			return false;
		if (dst->size <
		    ImageAttribute::CalcMemSize(extent.h, extent.w, 1, 1, mode, format))
			return false;
	} else {
		if (dst->buff == NULL)
			return false;
		if (dst->size <
		    ImageAttribute::CalcMemSize(extent.w, extent.h, 1, 1, mode, format))
			return false;
	}

	switch (mode) {
	case ImageMode_XBGR0565:
	case ImageMode_ABGR1555:
	case ImageMode_ABGR4444:
	case ImageMode_index16:
		return RotateImage((uint16_t *)dst->buff, (const uint16_t *)src->buff,
				   dst_pitch, src_pitch, extent.w, extent.h, mode, type);
	case ImageMode_RGBA8888:
	case ImageMode_BGRA8888:
	case ImageMode_index32:
		return RotateImage((uint32_t *)dst->buff, (const uint32_t *)src->buff,
				   dst_pitch, src_pitch, extent.w, extent.h, mode, type);
	case ImageMode_index4:
		return false;
	case ImageMode_index8:
		return RotateImage((uint8_t *)dst->buff, (const uint8_t *)src->buff, dst_pitch,
				   src_pitch, extent.w, extent.h, mode, type);
	default:
		return false;
	}
}

bool filter::RoundCorner(ImageBuffer *dst, const ImageBuffer *src, int dst_pitch, int src_pitch,
			 ImageExtent extent, ImageMode mode, ImageFormat format, int pitch_align,
			 int radius, int margin)
{
	if (dst->buff == NULL ||
	    dst->size < ImageAttribute::CalcMemSize(extent.w, extent.h, 1, 1, mode, format) ||
	    radius <= 0 || margin < 0 || pitch_align <= 0 ||
	    (mode != ImageMode_RGBA8888 && mode != ImageMode_BGRA8888))
		return false;

	int diameter = 2 * radius;
	if (extent.w < diameter || extent.h < diameter)
		return false;

	CopyPixels32((uint32_t *)dst->buff, (const uint32_t *)src->buff, dst_pitch, src_pitch,
		     extent.w, extent.h, PixelCopy());

	uint8_t *base = (uint8_t *)dst->buff;
	int pitch = (pitch_align + 4 * extent.w - 1) / pitch_align * pitch_align;
	base += 4 * margin + pitch * margin;
	int inner = (diameter - 3) * (diameter - 3);
	int outer = (diameter + 1) * (diameter + 1) - inner;

	for (int i = -radius; i < radius; i++) {
		int row = i < 0 ? i + radius : extent.h - 2 * margin - radius + i;
		uint8_t *line = base + row * pitch;
		int dy = (2 * i + 1) * (2 * i + 1);

		for (int j = -radius; j < radius; j++) {
			int distance = (2 * j + 1) * (2 * j + 1) + dy - inner;

			if (distance > 0) {
				int col = j < 0 ? j + radius : extent.w - 2 * margin - radius + j;
				uint8_t *p = line + 4 * col;
				uint8_t alpha = 0;

				if (distance < outer)
					alpha = 255 - 255 * distance / outer;
				if (p[3] > alpha)
					p[3] = alpha;
			}
		}
	}
	return true;
}

/* An edge index mirrored back into 0..n-1. */
static inline int Mirror(int v, int n)
{
	if (v < 0)
		return -v;
	if (v >= n)
		return 2 * n - 1 - v;
	return v;
}

static inline uint8_t SharpenChannel(int value, int blurred, float amount, int threshold)
{
	int diff = value - blurred;

	if (sce_paf_abs(diff) <= threshold)
		return value;
	return Clamp((int)((float)value + amount * (float)diff));
}

bool filter::Sharpen(ImageBuffer *dst, const ImageBuffer *src, int dst_pitch, int src_pitch,
		     ImageExtent extent, ImageMode mode, ImageFormat format, float amount,
		     int threshold, HeapAllocator *allocator)
{
	int w = extent.w;
	int h = extent.h;

	if (dst->buff == NULL)
		return false;
	if (dst->size < ImageAttribute::CalcMemSize(w, h, 1, 1, mode, format) ||
	    (mode != ImageMode_BGRA8888 && mode != ImageMode_RGBA8888))
		return false;

	uint8_t *tmp = (uint8_t *)Alloc(allocator, 4 * w * h);
	if (tmp == NULL)
		return false;

	/* tmp: the image blurred, its colour channels only. */
	uint8_t *t = tmp;
	const uint8_t *s = (const uint8_t *)src->buff;
	for (int y = 0; y < h; y++) {
		int y0 = Mirror(y - 1, h) * src_pitch;
		int y1 = Mirror(y, h) * src_pitch;
		int y2 = Mirror(y + 1, h) * src_pitch;

		for (int x = 0; x < w; x++) {
			int x0 = 4 * Mirror(x - 1, w);
			int x1 = 4 * Mirror(x, w);
			int x2 = 4 * Mirror(x + 1, w);

			for (int c = 0; c < 3; c++)
				t[c] = (s[y0 + x0 + c] + s[y0 + x1 + c] + s[y0 + x2 + c] +
					s[y1 + x0 + c] + 4 * s[y1 + x1 + c] + s[y1 + x2 + c] +
					s[y2 + x0 + c] + s[y2 + x1 + c] + s[y2 + x2 + c]) / 12;
			t += 4;
			s = (const uint8_t *)src->buff;
		}
	}

	uint8_t *d = (uint8_t *)dst->buff;
	t = tmp;
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			d[0] = SharpenChannel(s[0], t[0], amount, threshold);
			d[1] = SharpenChannel(s[1], t[1], amount, threshold);
			d[2] = SharpenChannel(s[2], t[2], amount, threshold);
			d[3] = SharpenChannel(s[3], t[3], amount, threshold);
			t += 4;
			d += 4;
			s += 4;
		}
		d += dst_pitch - 4 * w;
		s += src_pitch - 4 * w;
	}

	Free(allocator, tmp);
	return true;
}

bool filter::Transparentize(ImageBuffer *dst, const ImageBuffer *src, int dst_pitch,
			    int src_pitch, ImageExtent extent, ImageMode mode, ImageFormat format,
			    float alpha)
{
	if (dst->buff == NULL ||
	    dst->size < ImageAttribute::CalcMemSize(extent.w, extent.h, 1, 1, mode, format) ||
	    (mode != ImageMode_RGBA8888 && mode != ImageMode_BGRA8888))
		return false;

	CopyPixels32((uint32_t *)dst->buff, (const uint32_t *)src->buff, dst_pitch, src_pitch,
		     extent.w, extent.h, PixelCopy());

	uint8_t *p = (uint8_t *)dst->buff;
	for (int y = 0; y < extent.h; y++) {
		uint8_t *q = p;

		for (int x = 0; x < extent.w; x++) {
			q[3] = Clamp((int)(alpha * 255.0f));
			q += 4;
		}
		p += 4 * extent.w;
	}
	return true;
}

/*
 * The copy and convert loops: w x h pixels, each row of dst and src at its
 * pitch in bytes.
 */
static __attribute__((noinline)) void CopyPixels4(uint8_t *dst, const uint8_t *src,
						  int dst_pitch, int src_pitch, int w, int h,
						  const PixelCopy &op)
{
	int n = (w + 1) / 2;
	uint8_t *end = dst + n;

	for (int y = 0; y < h; y++) {
		while (dst < end)
			*dst++ = *src++;
		src += src_pitch - n;
		dst += dst_pitch - n;
		end += dst_pitch;
	}
}

static __attribute__((noinline)) void CopyPixels8(uint8_t *dst, const uint8_t *src,
						  int dst_pitch, int src_pitch, int w, int h,
						  const PixelCopy &op)
{
	uint8_t *end = dst + w;

	for (int y = 0; y < h; y++) {
		while (dst < end)
			*dst++ = *src++;
		src += src_pitch - w;
		dst += dst_pitch - w;
		end += dst_pitch;
	}
}

static __attribute__((noinline)) void CopyPixels16(uint16_t *dst, const uint16_t *src,
						   int dst_pitch, int src_pitch, int w, int h,
						   const PixelCopy &op)
{
	uint16_t *end = dst + w;

	for (int y = 0; y < h; y++) {
		while (dst < end)
			*dst++ = *src++;
		src += src_pitch / 2 - w;
		dst += dst_pitch / 2 - w;
		end += dst_pitch / 2;
	}
}

static __attribute__((noinline)) void CopyPixels32(uint32_t *dst, const uint32_t *src,
						   int dst_pitch, int src_pitch, int w, int h,
						   const PixelCopy &op)
{
	uint32_t *end = dst + w;

	for (int y = 0; y < h; y++) {
		while (dst < end)
			*dst++ = *src++;
		dst += dst_pitch / 4 - w;
		end += dst_pitch / 4;
		src += src_pitch / 4 - w;
	}
}

static __attribute__((noinline)) void ConvertCLUT4(uint32_t *dst, const uint8_t *src,
						   int dst_pitch, int src_pitch, int w, int h,
						   const PixelLookup &op)
{
	int n = w;
	bool odd = false;
	uint32_t *end = dst + w;

	if (w & 1) {
		odd = true;
		end--;
		n = w + 1;
	}
	for (int y = 0; y < h; y++) {
		for (; dst < end; dst += 2) {
			dst[0] = op.clut[*src & 0xF];
			dst[1] = op.clut[*src++ >> 4];
		}
		if (odd)
			*dst++ = op.clut[*src++ & 0xF];
		src += src_pitch - n / 2;
		dst += dst_pitch / 4 - w;
		end += dst_pitch / 4;
	}
}

static __attribute__((noinline)) void ConvertCLUT8(uint32_t *dst, const uint8_t *src,
						   int dst_pitch, int src_pitch, int w, int h,
						   const PixelLookup &op)
{
	uint32_t *end = dst + w;

	for (int y = 0; y < h; y++) {
		while (dst < end)
			*dst++ = op.clut[*src++];
		src += src_pitch - w;
		dst += dst_pitch / 4 - w;
		end += dst_pitch / 4;
	}
}

static inline void RGB565ToRGBA8888(uint8_t *dst, const uint16_t *src)
{
	dst[0] = ((*src & 0x1F) << 3) | ((*src >> 2) & 7);
	dst[1] = (((*src >> 5) & 0x3F) << 2) | ((*src >> 9) & 3);
	dst[2] = (((*src >> 11) & 0x1F) << 3) | (*src >> 13);
	dst[3] = 0xFF;
}

static inline void RGBA5551ToRGBA8888(uint8_t *dst, const uint16_t *src)
{
	dst[0] = ((*src & 0x1F) << 3) | ((*src >> 2) & 7);
	dst[1] = (((*src >> 5) & 0x1F) << 3) | ((*src >> 7) & 7);
	dst[2] = (((*src >> 10) & 0x1F) << 3) | ((*src >> 12) & 7);
	dst[3] = (*src & 0x8000) ? 0xFF : 0;
}

static inline void RGBA4444ToRGBA8888(uint8_t *dst, const uint16_t *src)
{
	dst[0] = (*src & 0xF) | ((*src & 0xF) << 4);
	dst[1] = (*src & 0xF0) | ((*src >> 4) & 0xF);
	dst[2] = ((*src >> 8) & 0xF) | (((*src >> 8) & 0xF) << 4);
	dst[3] = (*src >> 12) | (((*src >> 12) & 0xF) << 4);
}

static inline void BGRA8888ToRGBA8888(uint8_t *dst, const uint8_t *src)
{
	dst[0] = src[2];
	dst[1] = src[1];
	dst[2] = src[0];
	dst[3] = src[3];
}

static __attribute__((noinline)) void ConvertRGB565(uint8_t *dst, const uint16_t *src,
						    int dst_pitch, int src_pitch, int w, int h,
						    const PixelConvert &op)
{
	uint8_t *end = dst + 4 * w;

	for (int y = 0; y < h; y++) {
		for (; dst < end; dst += 4)
			RGB565ToRGBA8888(dst, src++);
		dst += dst_pitch - 4 * w;
		end += dst_pitch;
		src += src_pitch / 2 - w;
	}
}

static __attribute__((noinline)) void ConvertRGBA5551(uint8_t *dst, const uint16_t *src,
						      int dst_pitch, int src_pitch, int w, int h,
						      const PixelConvert &op)
{
	uint8_t *end = dst + 4 * w;

	for (int y = 0; y < h; y++) {
		for (; dst < end; src++) {
			RGBA5551ToRGBA8888(dst, src);
			dst += 4;
		}
		dst += dst_pitch - 4 * w;
		end += dst_pitch;
		src += src_pitch / 2 - w;
	}
}

static __attribute__((noinline)) void ConvertRGBA4444(uint8_t *dst, const uint16_t *src,
						      int dst_pitch, int src_pitch, int w, int h,
						      const PixelConvert &op)
{
	uint8_t *end = dst + 4 * w;

	for (int y = 0; y < h; y++) {
		for (; dst < end; dst += 4)
			RGBA4444ToRGBA8888(dst, src++);
		dst += dst_pitch - 4 * w;
		end += dst_pitch;
		src += src_pitch / 2 - w;
	}
}

static __attribute__((noinline)) void ConvertBGRA8888(uint8_t *dst, const uint8_t *src,
						      int dst_pitch, int src_pitch, int w, int h,
						      const PixelConvert &op)
{
	uint8_t *end = dst + 4 * w;

	for (int y = 0; y < h; y++) {
		for (; dst < end; dst += 4) {
			BGRA8888ToRGBA8888(dst, src);
			src += 4;
		}
		dst += 4 * (dst_pitch / 4 - w);
		end += dst_pitch / 4 * 4;
		src += 4 * (src_pitch / 4 - w);
	}
}

/* As the loops above, into dst inside a border of edge pixels. */
static __attribute__((noinline)) void ConvertCLUT4WithEdge(void *dst, const uint8_t *src,
							   int dst_pitch, int src_pitch, int w,
							   int h, const PixelLookup &op,
							   uint32_t edge)
{
	uint32_t *d = (uint32_t *)dst + (edge + dst_pitch / 4 * edge);
	int n = w;
	bool odd = false;
	uint32_t *end = d + w;

	if (w & 1) {
		odd = true;
		end--;
		n = w + 1;
	}
	for (int y = 0; y < h; y++) {
		for (; d < end; d += 2) {
			d[0] = op.clut[*src & 0xF];
			d[1] = op.clut[*src++ >> 4];
		}
		if (odd)
			*d++ = op.clut[*src++ & 0xF];
		d += dst_pitch / 4 - w;
		end += dst_pitch / 4;
		src += src_pitch - n / 2;
	}
}

static __attribute__((noinline)) void ConvertCLUT8WithEdge(void *dst, const uint8_t *src,
							   int dst_pitch, int src_pitch, int w,
							   int h, const PixelLookup &op,
							   uint32_t edge)
{
	uint32_t *d = (uint32_t *)dst + (edge + dst_pitch / 4 * edge);
	uint32_t *end = d + w;

	for (int y = 0; y < h; y++) {
		while (d < end)
			*d++ = op.clut[*src++];
		d += dst_pitch / 4 - w;
		end += dst_pitch / 4;
		src += src_pitch - w;
	}
}

static __attribute__((noinline)) void ConvertRGB565WithEdge(void *dst, const uint16_t *src,
							    int dst_pitch, int src_pitch, int w,
							    int h, const PixelConvert &op,
							    uint32_t edge)
{
	uint8_t *d = (uint8_t *)dst + 4 * edge + dst_pitch * edge;
	uint8_t *end = d + 4 * w;

	for (int y = 0; y < h; y++) {
		for (; d < end; d += 4)
			RGB565ToRGBA8888(d, src++);
		d += dst_pitch - 4 * w;
		end += dst_pitch;
		src += src_pitch / 2 - w;
	}
}

static __attribute__((noinline)) void ConvertRGBA5551WithEdge(void *dst, const uint16_t *src,
							      int dst_pitch, int src_pitch, int w,
							      int h, const PixelConvert &op,
							      uint32_t edge)
{
	uint8_t *d = (uint8_t *)dst + 4 * edge + dst_pitch * edge;
	uint8_t *end = d + 4 * w;

	for (int y = 0; y < h; y++) {
		for (; d < end; src++) {
			RGBA5551ToRGBA8888(d, src);
			d += 4;
		}
		d += dst_pitch - 4 * w;
		end += dst_pitch;
		src += src_pitch / 2 - w;
	}
}

static __attribute__((noinline)) void ConvertRGBA4444WithEdge(void *dst, const uint16_t *src,
							      int dst_pitch, int src_pitch, int w,
							      int h, const PixelConvert &op,
							      uint32_t edge)
{
	uint8_t *d = (uint8_t *)dst + 4 * edge + dst_pitch * edge;
	uint8_t *end = d + 4 * w;

	for (int y = 0; y < h; y++) {
		for (; d < end; d += 4)
			RGBA4444ToRGBA8888(d, src++);
		d += dst_pitch - 4 * w;
		end += dst_pitch;
		src += src_pitch / 2 - w;
	}
}

static __attribute__((noinline)) void ConvertBGRA8888WithEdge(void *dst, const uint8_t *src,
							      int dst_pitch, int src_pitch, int w,
							      int h, const PixelConvert &op,
							      uint32_t edge)
{
	uint8_t *d = (uint8_t *)((uint32_t *)dst + (edge + dst_pitch / 4 * edge));
	uint8_t *end = d + 4 * w;

	for (int y = 0; y < h; y++) {
		for (; d < end; d += 4) {
			BGRA8888ToRGBA8888(d, src);
			src += 4;
		}
		d += 4 * (dst_pitch / 4 - w);
		end += dst_pitch / 4 * 4;
		src += 4 * (src_pitch / 4 - w);
	}
}

static __attribute__((noinline)) void CopyPixels32WithEdge(void *dst, const uint32_t *src,
							   int dst_pitch, int src_pitch, int w,
							   int h, const PixelCopy &op,
							   uint32_t edge)
{
	uint32_t *d = (uint32_t *)dst + (edge + dst_pitch / 4 * edge);
	uint32_t *end = d + w;

	for (int y = 0; y < h; y++) {
		while (d < end)
			*d++ = *src++;
		d += dst_pitch / 4 - w;
		end += dst_pitch / 4;
		src += src_pitch / 4 - w;
	}
}

/* The pixel-size generic operations, by the size of a pixel (T). */
template <typename T>
static bool FlipImage(T *dst, const T *src, uint32_t dst_pitch, uint32_t src_pitch, int w, int h,
		      ImageMode mode, ImageFlipType type)
{
	uint32_t src_stride = src_pitch / (ImageAttribute::GetPixelSize(mode) >> 3);
	uint32_t dst_stride = dst_pitch / (ImageAttribute::GetPixelSize(mode) >> 3);
	T *end = dst + w;

	switch (type) {
	case ImageFlipType_LeftRight: {
		const T *s = src + w - 1;

		for (int y = 0; y < h; y++) {
			while (dst < end)
				*dst++ = *s--;
			s += src_stride + w;
			dst += dst_stride - w;
			end += dst_stride;
		}
		return true;
	}
	case ImageFlipType_TopButtom: {
		const T *s = src + (h * src_stride - src_stride);

		for (int y = 0; y < h; y++) {
			while (dst < end)
				*dst++ = *s++;
			s -= src_stride + w;
			dst += dst_stride - w;
			end += dst_stride;
		}
		return true;
	}
	default:
		return false;
	}
}

template <typename T>
static bool ResizeNearestNeighbor(T *dst, const T *src, uint32_t dst_pitch, uint32_t src_pitch,
				  int src_w, int src_h, ImageMode mode, int dst_w, int dst_h,
				  ImageResizeType type, HeapAllocator *allocator)
{
	float scale_x = (float)src_w / (float)dst_w;
	float scale_y = (float)src_h / (float)dst_h;
	uint32_t src_stride = src_pitch / (ImageAttribute::GetPixelSize(mode) >> 3);
	uint32_t dst_stride = dst_pitch / (ImageAttribute::GetPixelSize(mode) >> 3);
	int *xs = (int *)Alloc(allocator, 4 * dst_w);
	if (xs == NULL) {
		Image::SetErrorType(SCE_PAF_ERROR_IMAGE_OUT_OF_MEMORY);
		return false;
	}

	T *end = dst + dst_w;
	size_t size = dst_w * (ImageAttribute::GetPixelSize(mode) >> 3);
	for (int x = 0; x < dst_w; x++)
		xs[x] = (int)(((float)x + 0.5f) * scale_x);

	int prev = -1;
	for (int y = 0; y < dst_h; y++) {
		int sy = (int)(((float)y + 0.5f) * scale_y);

		if (sy == prev) {
			sce_paf_memcpy(dst, dst - dst_stride, size);
			dst += dst_stride;
		} else {
			const int *x = xs;

			while (dst < end)
				*dst++ = src[sy * src_stride + *x++];
			dst += dst_stride - dst_w;
		}
		prev = sy;
		end += dst_stride;
	}
	Free(allocator, xs);
	return true;
}

/*
 * Nearest-neighbour resizing to RGBA8888 from T-sized pixels. Sony's never sets
 * prev, so each row is converted anew.
 */
template <typename T>
static bool ConvertNearestNeighbor(uint32_t *dst, const T *src, const uint8_t *clut,
				   uint32_t dst_pitch, uint32_t src_pitch, int dst_w, int dst_h,
				   int src_w, int src_h, ImageMode src_mode, ImageMode dst_mode,
				   HeapAllocator *allocator)
{
	float scale_y = (float)src_h / (float)dst_h;
	uint32_t src_stride = src_pitch / (ImageAttribute::GetPixelSize(src_mode) >> 3);
	uint32_t dst_stride = dst_pitch / (ImageAttribute::GetPixelSize(dst_mode) >> 3);
	uint32_t *xs = (uint32_t *)Alloc(allocator, 4 * dst_w);
	if (xs == NULL) {
		Image::SetErrorType(SCE_PAF_ERROR_IMAGE_OUT_OF_MEMORY);
		return false;
	}

	uint32_t *end = dst + dst_w;
	size_t size = dst_w * (ImageAttribute::GetPixelSize(dst_mode) >> 3);
	for (int x = 0; x < dst_w; x++)
		xs[x] = (int)(((float)x + 0.5f) * ((float)src_w / (float)dst_w));

	int prev = -1;
	for (int y = 0; y < dst_h; y++) {
		int sy = (int)(((float)y + 0.5f) * scale_y);

		if (sy == prev) {
			sce_paf_memcpy(dst, dst - dst_stride, size);
			dst += dst_stride;
		} else {
			const uint8_t *row = (const uint8_t *)(src + sy * src_stride);
			const uint32_t *x = xs;

			while (dst < end) {
				uint8_t r, g, b, a;

				GetPixel(src_mode, row, *x++, clut, &r, &g, &b, &a);
				*dst++ = (a << 24) | (b << 16) | r | (g << 8);
			}
			dst += dst_stride - dst_w;
		}
		end += dst_stride;
	}
	Free(allocator, xs);
	return true;
}

template <typename T>
static bool RotateImage(T *dst, const T *src, uint32_t dst_pitch, uint32_t src_pitch, int w,
			int h, ImageMode mode, ImageRotateType type)
{
	uint32_t src_stride = src_pitch / (ImageAttribute::GetPixelSize(mode) >> 3);
	uint32_t dst_stride = dst_pitch / (ImageAttribute::GetPixelSize(mode) >> 3);

	switch (type) {
	case ImageRotateType_Degree90: {
		T *end = dst + w;
		const T *column = src + (w * src_stride - src_stride);

		for (int y = 0; y < h; y++) {
			const T *s = column++;

			while (dst < end) {
				*dst++ = *s;
				s -= src_stride;
			}
			dst += dst_stride - w;
			end += dst_stride;
		}
		return true;
	}
	case ImageRotateType_Degree180: {
		T *end = dst + w;
		const T *s = src + (h * src_stride - src_stride) + w - 1;

		for (int y = 0; y < h; y++) {
			while (dst < end)
				*dst++ = *s--;
			s -= src_stride - w;
			dst += dst_stride - w;
			end += dst_stride;
		}
		return true;
	}
	case ImageRotateType_Degree270: {
		T *end = dst + w;
		const T *column = src + h - 1;

		for (int y = 0; y < h; y++) {
			const T *s = column--;

			while (dst < end) {
				*dst++ = *s;
				s += src_stride;
			}
			dst += dst_stride - w;
			end += dst_stride;
		}
		return true;
	}
	default:
		return false;
	}
}
