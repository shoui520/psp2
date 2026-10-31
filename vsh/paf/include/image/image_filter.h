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

/* A pixel buffer: its address and size in bytes. */
class ImageBuffer {
public:
	void *m_addr;
	uint32_t m_size;
};

namespace filter {

/* src1 at rate 0, src2 at rate 1, each channel interpolated in between. */
bool Blend(ImageBuffer *dst, const ImageBuffer *src1, const ImageBuffer *src2, int dst_pitch,
	   int src_pitch, ImageExtent extent, ImageMode mode, ImageFormat format, float rate);
/* A Gaussian blur of standard deviation sigma. */
bool Blur(ImageBuffer *dst, const ImageBuffer *src, int dst_pitch, int src_pitch,
	  ImageExtent extent, ImageMode mode, ImageFormat format, float sigma,
	  memory::HeapAllocator *allocator);
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
 * Exports whose Sony names no NID search has found: converting between modes
 * (DXT compression at the given quality), and converting to RGBA8888 while
 * resizing (quality is unused). clut is the source's palette.
 */
extern "C" {
bool ScePafMisc_CA674A19(paf::image::ImageBuffer *dst, const paf::image::ImageBuffer *src,
			 const paf::image::ImageBuffer *clut, int dst_pitch, int src_pitch,
			 const paf::image::ImageExtent &dst_extent,
			 const paf::image::ImageExtent &src_extent, ImageMode src_mode,
			 paf::image::ImageFormat format, ImageMode dst_mode,
			 paf::image::ImageCompressQuality quality);
bool ScePafMisc_D94BEC9F(paf::image::ImageBuffer *dst, const paf::image::ImageBuffer *src,
			 const paf::image::ImageBuffer *clut, uint32_t dst_pitch, int src_pitch,
			 const paf::image::ImageExtent &dst_extent,
			 const paf::image::ImageExtent &src_extent, ImageMode src_mode,
			 paf::image::ImageFormat format, ImageMode dst_mode,
			 paf::image::ImageResizeType type, paf::image::ImageCompressQuality quality,
			 paf::memory::HeapAllocator *allocator);
}

#endif
