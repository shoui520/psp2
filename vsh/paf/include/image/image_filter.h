/*
 * paf::image::filter: whole-image operations on pixel buffers. Each takes the
 * destination and source buffers with their pitches in bytes, the image's size
 * and mode, and returns false when the destination is too small or the mode is
 * not supported.
 */
#ifndef PAF_IMAGE_IMAGE_FILTER_H
#define PAF_IMAGE_IMAGE_FILTER_H

#include <stddef.h>
#include <stdint.h>
#include <image/image_types.h>
#include <image/image_region.h>

namespace paf {

namespace memory {
class HeapAllocator;
}

namespace image {

namespace filter {

/* src1 at rate 0, src2 at rate 1, each channel interpolated in between. */
bool Blend(ImageBuffer *dst, const ImageBuffer *src1, const ImageBuffer *src2, int dst_pitch,
	   int src_pitch, ImageExtent extent, ImageMode mode, ImageFormat format, float rate);
/* A Gaussian blur of standard deviation sigma. */
bool Blur(ImageBuffer *dst, const ImageBuffer *src, int dst_pitch, int src_pitch,
	  ImageExtent extent, ImageMode mode, ImageFormat format, float sigma,
	  memory::HeapAllocator *allocator);
/*
 * Converts src (palette clut) from src_mode to dst_mode, DXT modes compressed as
 * quality says.
 */
bool ConvertMode(ImageBuffer *dst, const ImageBuffer *src, const ImageBuffer *clut, int dst_pitch,
		 int src_pitch, const ImageExtent &dst_extent, const ImageExtent &src_extent,
		 ImageMode src_mode, ImageFormat format, ImageMode dst_mode,
		 ImageConvertModeType quality = ImageConvertModeType_Default);
bool Copy(ImageBuffer *dst, const ImageBuffer *src, int dst_pitch, int src_pitch,
	  ImageExtent extent, ImageMode mode, ImageFormat format);
/*
 * Converts src to RGBA8888 inside a border of edge pixels, then fills the border
 * with the nearest pixel's colour at alpha 0.
 */
bool CopyWithEdge(ImageBuffer *dst, const ImageBuffer *src, const ImageBuffer *clut,
		  int dst_pitch, int src_pitch, ImageExtent extent, ImageMode mode,
		  ImageFormat format, uint32_t edge);
bool Crop(ImageBuffer *dst, const ImageBuffer *src, int dst_pitch, int src_pitch,
	  ImageExtent extent, ImageMode mode, ImageFormat format, ImageRect rect);
/* Copies the image with the alpha of its outermost pixels cleared. */
bool FadeWithEdge(ImageBuffer *dst, const ImageBuffer *src, int dst_pitch, int src_pitch,
		  ImageExtent extent, ImageMode mode, ImageFormat format);
bool Flip(ImageBuffer *dst, const ImageBuffer *src, int dst_pitch, int src_pitch,
	  ImageExtent extent, ImageMode mode, ImageFormat format, ImageFlipType type);
/* Copies src, then overlay at pos, clipped to the image. */
bool Paste(ImageBuffer *dst, const ImageBuffer *src, const ImageBuffer *overlay, int dst_pitch,
	   int src_pitch, int overlay_pitch, ImageExtent extent, ImageExtent overlay_extent,
	   ImageMode mode, ImageFormat format, const ImagePoint &pos);
/* Halves or quarters an RGBA image in both directions; extent is the source's. */
void ResizeHalf(void *dst, const void *src, int dst_pitch, int src_pitch, int w, int h);
bool ResizeHalf(ImageBuffer *dst, const ImageBuffer *src, int dst_pitch, int src_pitch,
		ImageExtent extent, ImageMode mode);
void ResizeQuarter(void *dst, const void *src, int dst_pitch, int src_pitch, int w, int h);
bool ResizeQuarter(ImageBuffer *dst, const ImageBuffer *src, int dst_pitch, int src_pitch,
		   ImageExtent extent, ImageMode mode);
bool Resize(ImageBuffer *dst, const ImageBuffer *src, int dst_pitch, int src_pitch,
	    ImageExtent extent, ImageMode mode, ImageFormat format, ImageExtent dst_extent,
	    ImageResizeType type, memory::HeapAllocator *allocator);
/* Converts src to RGBA8888 while resizing it from src_extent to dst_extent; quality is unused. */
bool ResizeWithConvertMode(ImageBuffer *dst, const ImageBuffer *src, const ImageBuffer *clut,
			   int dst_pitch, int src_pitch, const ImageExtent &dst_extent,
			   const ImageExtent &src_extent, ImageMode src_mode, ImageFormat format,
			   ImageMode dst_mode, ImageResizeType type,
			   ImageConvertModeType quality, memory::HeapAllocator *allocator);
bool Rotate(ImageBuffer *dst, const ImageBuffer *src, int dst_pitch, int src_pitch,
	    ImageExtent extent, ImageMode mode, ImageFormat format, ImageRotateType type);
/*
 * Copies the image and fades out its corners: a quarter circle of the radius in
 * each corner of the image less margin, rows padded to pitch_align bytes.
 */
bool RoundCorner(ImageBuffer *dst, const ImageBuffer *src, int dst_pitch, int src_pitch,
		 ImageExtent extent, ImageMode mode, ImageFormat format, int pitch_align,
		 int radius, int margin);
/* An unsharp mask: channels that differ from the blurred image by more than threshold move by amount. */
bool Sharpen(ImageBuffer *dst, const ImageBuffer *src, int dst_pitch, int src_pitch,
	     ImageExtent extent, ImageMode mode, ImageFormat format, float amount,
	     int threshold, memory::HeapAllocator *allocator);
/* Copies the image with every pixel's alpha set to alpha (0 to 1). */
bool Transparentize(ImageBuffer *dst, const ImageBuffer *src, int dst_pitch, int src_pitch,
		    ImageExtent extent, ImageMode mode, ImageFormat format, float alpha);

}
}
}

/*
 * The resampling the filters share with the codecs' line loading: the
 * contributions of src pixels to each of dst pixels, the horizontal pass of a
 * source row into a column of a transposed image, and the vertical pass back.
 */
struct ContribList;

void FreeContribList(ContribList *contrib, int n, paf::memory::HeapAllocator *allocator);
void ApplyContribV(int src_n, int dst_n, int dst_pitch, const uint8_t *tmp, int count,
		   uint8_t *dst, const ContribList *contrib);
ContribList *CalcContrib(int src, int dst, paf::image::ImageResizeType type,
			 paf::memory::HeapAllocator *allocator);
void ApplyContribH(const uint8_t *src, uint8_t *out, int n, int pitch, const ContribList *contrib);

#endif
