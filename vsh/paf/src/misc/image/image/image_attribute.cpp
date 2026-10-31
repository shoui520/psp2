/* paf::image::ImageAttribute: pixel sizes and the memory layout of mipmap levels. */
#include <image/image_attribute.h>

using namespace paf;
using namespace paf::image;

size_t ImageAttribute::GetPixelSize(ImageMode mode)
{
	switch (mode) {
	case ImageMode_NONE:
		return 0;
	case ImageMode_XBGR0565:
	case ImageMode_ABGR1555:
	case ImageMode_ABGR4444:
	case ImageMode_index16:
		return 16;
	case ImageMode_ABGR8888:
	case ImageMode_RGBA8888:
	case ImageMode_BGRA8888:
	case ImageMode_index32:
		return 32;
	case ImageMode_A8:
	case ImageMode_L8:
	case ImageMode_DXT3:
	case ImageMode_DXT5:
	case ImageMode_index8:
	case ImageMode_PVRT2BPP:
	case ImageMode_PVRT4BPP:
	case ImageMode_PVRTII2BPP:
	case ImageMode_PVRTII4BPP:
		return 8;
	case ImageMode_DXT1:
	case ImageMode_index4:
		return 4;
	case ImageMode_BGR888:
		return 24;
	default:
		return 0;
	}
}

size_t ImageAttribute::GetPixelSizeInBits() const
{
	return GetPixelSize(m_mode);
}

size_t ImageAttribute::GetCLUTPixelSizeInBits() const
{
	return GetPixelSize(m_clutmode);
}

size_t ImageAttribute::GetPixelSizeInBytes() const
{
	return GetPixelSize(m_mode) / 8;
}

size_t ImageAttribute::GetCLUTPixelSizeInBytes() const
{
	return GetPixelSize(m_clutmode) / 8;
}

/* GetExtent, inlined: Sony's memory size functions inline it, SNC here inlines it only from a static function. */
static inline ImageExtent LevelExtent(const ImageExtent &extent, ImageXType xtype, uint32_t index)
{
	if (xtype == ImageXType_HalfSize) {
		int w = extent.w;
		int h = extent.h;
		for (; index != 0; index--) {
			w = (w + 1) / 2;
			h = (h + 1) / 2;
		}
		return ImageExtent(w, h);
	}
	return extent;
}

ImageExtent ImageAttribute::GetExtent(uint32_t index) const
{
	if (m_xtype == ImageXType_HalfSize) {
		int w = m_extent.w;
		int h = m_extent.h;
		for (; index != 0; index--) {
			w = (w + 1) / 2;
			h = (h + 1) / 2;
		}
		return ImageExtent(w, h);
	}
	return m_extent;
}

ImageRect ImageAttribute::GetClip(uint32_t index) const
{
	ImageRect clip;

	if (m_xtype == ImageXType_HalfSize) {
		int x = m_clip.origin.x;
		int y = m_clip.origin.y;
		int w = m_clip.extent.w;
		int h = m_clip.extent.h;
		for (; index != 0; index--) {
			x = (x + 1) / 2;
			y = (y + 1) / 2;
			w = (w + 1) / 2;
			h = (h + 1) / 2;
		}
		clip.origin.x = x;
		clip.origin.y = y;
		clip.extent.w = w;
		clip.extent.h = h;
	} else {
		clip = m_clip;
	}
	return clip;
}

/* A row of width pixels in bytes, padded to align bytes. */
static inline size_t AlignedPitch(ImageMode mode, uint16_t align, int width)
{
	return (align - 1 + ((ImageAttribute::GetPixelSize(mode) * width + 7) >> 3)) / align * align;
}

static inline int AlignedHeight(uint16_t align, int height)
{
	return (height + align - 1) / align * align;
}

size_t ImageAttribute::GetMemPitch(uint32_t index) const
{
	size_t bits = GetPixelSize(m_mode);
	uint16_t align = m_pitchalign;

	return (align - 1 + ((bits * LevelExtent(m_extent, m_xtype, index).w + 7) >> 3)) / align *
	       align;
}

uint32_t ImageAttribute::GetMemHeight(uint32_t index) const
{
	return AlignedHeight(m_heightalign, LevelExtent(m_extent, m_xtype, index).h);
}

/*
 * The size of mipmap level index, or 0xFFFFFFFF when it, with a row and a
 * palette to spare, passes 4 GiB.
 */
uint32_t ImageAttribute::GetMemSize(uint32_t index) const
{
	ImageExtent extent = LevelExtent(m_extent, m_xtype, index);

	if ((uint64_t)(AlignedPitch(m_mode, m_pitchalign, extent.w) + 1) *
			    AlignedHeight(m_heightalign, extent.h) +
		    (4 << GetPixelSize(m_mode)) > 0xFFFFFFFF)
		return 0xFFFFFFFF;
	return AlignedPitch(m_mode, m_pitchalign, extent.w) * AlignedHeight(m_heightalign, extent.h);
}

uint32_t ImageAttribute::CalcMemSize(uint32_t width, uint32_t height, uint16_t pitchalign,
				     uint16_t heightalign, ImageMode mode, ImageFormat format)
{
	if ((uint64_t)(AlignedPitch(mode, pitchalign, width) + 1) *
			    ((height + heightalign - 1) / heightalign * heightalign) +
		    (4 << GetPixelSize(mode)) > 0xFFFFFFFF)
		return 0xFFFFFFFF;
	return AlignedPitch(mode, pitchalign, width) *
	       ((height + heightalign - 1) / heightalign * heightalign);
}

ImageAttribute::ImageAttribute()
	: m_mode(ImageMode_NONE), m_clutmode(ImageMode_NONE), m_order((ImageOrder)0), m_clutsize(0),
	  m_format(ImageFormat_None), m_xtype(ImageXType_None), m_ytype(ImageYType_None),
	  m_pitchalign(0), m_heightalign(0), m_resizetype(ImageResizeType_NearestNeighbor)
{
}

ImageAttribute::~ImageAttribute()
{
}
