/*
 * paf::image::ConvertTo*Operation: an image converted to RGBA8888, or
 * compressed to DXT1, DXT3 or DXT5.
 */
#include <image/image.h>
#include <image/image_filter.h>
#include "image_operation.h"

using namespace paf;
using namespace paf::image;

/* The power of two at or above v, at least 4: the extent of a DXT image. */
static inline int32_t DxtExtent(int32_t v)
{
	uint32_t current;
	uint32_t step8;
	uint32_t step4;
	uint32_t step2;
	int32_t high;

	if (v < 4)
		return 4;
	if ((v & (v - 1)) == 0)
		return v;
	step8 = ((int32_t)((uint32_t)v - 0x100) >> 16) & 8;
	current = (uint32_t)v << step8;
	step4 = ((int32_t)(current - 0x1000) >> 16) & 4;
	current <<= step4;
	step2 = ((int32_t)(current - 0x4000) >> 16) & 2;
	current <<= step2;
	high = (int32_t)current >> 14;
	return 1 << ((high & ~(high >> 1)) - (int32_t)(step8 + step4 + step2) + 14);
}

bool ConvertToRGBA8888Operation::Init(const ImageAttribute *srcattr, const ImageArray *srcimages,
				      const ImageArray *srccluts, ImageAttribute *dstattr,
				      ImageArray *dstimages, ImageArray *dstcluts)
{
	if (!ImageOperation::Init(srcattr, srcimages, srccluts, dstattr, dstimages, dstcluts))
		return false;

	if (srcimages->GetArraySize() == ImagePoint(0, 0))
		return false;
	if (srcattr->m_mode == ImageMode_index16 || srcattr->m_mode == ImageMode_index32)
		return false;
	if (srcattr->m_mode == ImageMode_index4 &&
	    (srcattr->m_clutsize != 16 || srcattr->m_clutmode != ImageMode_RGBA8888))
		return false;
	if (srcattr->m_mode == ImageMode_index8 &&
	    (srcattr->m_clutsize != 256 || srcattr->m_clutmode != ImageMode_RGBA8888))
		return false;
	if (srcattr->m_order != ImageOrder_Linear) {
		ImageExtent extent = srcattr->m_extent;
		ImageExtent clip = srcattr->m_clip.extent;
		if (!(extent == clip))
			return false;
	}

	dstattr->m_mode = ImageMode_RGBA8888;
	dstattr->m_clutmode = ImageMode_NONE;
	dstattr->m_clutsize = 0;

	dstimages->Init(srcimages->GetArraySize());
	return true;
}

bool ConvertToRGBA8888Operation::OperateImage(const ImagePoint &index,
					      const ImageAttribute *srcattr,
					      const ImageBuffer *srcimage, const ImageBuffer *srcclut,
					      ImageAttribute *dstattr, ImageBuffer *dstimage,
					      ImageArray *dstclut)
{
	if (dstimage->buff == NULL || dstimage->size < dstattr->GetMemSize(index.x))
		return false;

	ImageRect clip = srcattr->GetClip(index.x);
	ImageBuffer src = *srcimage;
	src.buff = (uint8_t *)src.buff + clip.origin.y * srcattr->GetMemPitch(index.x) +
		   (int)(srcattr->GetPixelSizeInBits() * 0.125f * clip.origin.x);
	int src_pitch = srcattr->GetMemPitch(index.x);
	int dst_pitch = dstattr->GetMemPitch(index.x);
	ImageExtent dst_extent = dstattr->GetExtent(index.x);
	ImageExtent src_extent = srcattr->GetExtent(index.x);

	return filter::ConvertMode(dstimage, &src, srcclut, dst_pitch, src_pitch, dst_extent,
				   src_extent, srcattr->m_mode, srcattr->m_format,
				   ImageMode_RGBA8888, m_type);
}

ConvertToRGBA8888Operation::ConvertToRGBA8888Operation(ImageConvertModeType type,
						       memory::HeapAllocator *allocator)
	: ImageOperation(allocator), m_type(type)
{
}

ConvertToRGBA8888Operation::~ConvertToRGBA8888Operation()
{
}

bool ConvertToDXT1Operation::Init(const ImageAttribute *srcattr, const ImageArray *srcimages,
				  const ImageArray *srccluts, ImageAttribute *dstattr,
				  ImageArray *dstimages, ImageArray *dstcluts)
{
	if (!ImageOperation::Init(srcattr, srcimages, srccluts, dstattr, dstimages, dstcluts))
		return false;

	if (srcimages->GetArraySize() == ImagePoint(0, 0))
		return false;
	if (srcattr->m_mode != ImageMode_RGBA8888 || srcattr->m_order != ImageOrder_Linear)
		return false;
	ImageExtent extent = srcattr->m_extent;
	ImageExtent clip = srcattr->m_clip.extent;
	if (!(extent == clip))
		return false;

	dstattr->m_clutsize = 0;
	dstattr->m_mode = ImageMode_DXT1;
	dstattr->m_clutmode = ImageMode_NONE;
	dstattr->m_pitchalign = 1;
	dstattr->m_heightalign = 1;
	dstattr->m_extent.w = DxtExtent(dstattr->m_originalextent.w);
	dstattr->m_extent.h = DxtExtent(dstattr->m_originalextent.h);

	dstimages->Init(srcimages->GetArraySize());
	return true;
}

bool ConvertToDXT1Operation::OperateImage(const ImagePoint &index, const ImageAttribute *srcattr,
					  const ImageBuffer *srcimage, const ImageBuffer *srcclut,
					  ImageAttribute *dstattr, ImageBuffer *dstimage,
					  ImageArray *dstclut)
{
	if (dstimage->buff == NULL || dstimage->size < dstattr->GetMemSize(index.x))
		return false;

	int src_pitch = srcattr->GetMemPitch(index.x);
	int dst_pitch = dstattr->GetMemPitch(index.x);
	ImageExtent dst_extent = dstattr->GetExtent(index.x);
	ImageExtent src_extent = srcattr->GetExtent(index.x);

	return filter::ConvertMode(dstimage, srcimage, srcclut, dst_pitch, src_pitch, dst_extent,
				   src_extent, srcattr->m_mode, srcattr->m_format, ImageMode_DXT1,
				   m_type);
}

ConvertToDXT1Operation::ConvertToDXT1Operation(ImageConvertModeType type,
					       memory::HeapAllocator *allocator)
	: ImageOperation(allocator), m_type(type)
{
}

ConvertToDXT1Operation::~ConvertToDXT1Operation()
{
}

bool ConvertToDXT3Operation::Init(const ImageAttribute *srcattr, const ImageArray *srcimages,
				  const ImageArray *srccluts, ImageAttribute *dstattr,
				  ImageArray *dstimages, ImageArray *dstcluts)
{
	if (!ImageOperation::Init(srcattr, srcimages, srccluts, dstattr, dstimages, dstcluts))
		return false;

	if (srcimages->GetArraySize() == ImagePoint(0, 0))
		return false;
	if (srcattr->m_mode != ImageMode_RGBA8888 || srcattr->m_order != ImageOrder_Linear)
		return false;
	ImageExtent extent = srcattr->m_extent;
	ImageExtent clip = srcattr->m_clip.extent;
	if (!(extent == clip))
		return false;

	dstattr->m_clutsize = 0;
	dstattr->m_mode = ImageMode_DXT3;
	dstattr->m_clutmode = ImageMode_NONE;
	dstattr->m_pitchalign = 1;
	dstattr->m_heightalign = 1;
	dstattr->m_extent.w = DxtExtent(dstattr->m_originalextent.w);
	dstattr->m_extent.h = DxtExtent(dstattr->m_originalextent.h);

	dstimages->Init(srcimages->GetArraySize());
	return true;
}

bool ConvertToDXT3Operation::OperateImage(const ImagePoint &index, const ImageAttribute *srcattr,
					  const ImageBuffer *srcimage, const ImageBuffer *srcclut,
					  ImageAttribute *dstattr, ImageBuffer *dstimage,
					  ImageArray *dstclut)
{
	if (dstimage->buff == NULL || dstimage->size < dstattr->GetMemSize(index.x))
		return false;

	int src_pitch = srcattr->GetMemPitch(index.x);
	int dst_pitch = dstattr->GetMemPitch(index.x);
	ImageExtent dst_extent = dstattr->GetExtent(index.x);
	ImageExtent src_extent = srcattr->GetExtent(index.x);

	return filter::ConvertMode(dstimage, srcimage, srcclut, dst_pitch, src_pitch, dst_extent,
				   src_extent, srcattr->m_mode, srcattr->m_format, ImageMode_DXT3,
				   m_type);
}

ConvertToDXT3Operation::ConvertToDXT3Operation(ImageConvertModeType type,
					       memory::HeapAllocator *allocator)
	: ImageOperation(allocator), m_type(type)
{
}

ConvertToDXT3Operation::~ConvertToDXT3Operation()
{
}

bool ConvertToDXT5Operation::Init(const ImageAttribute *srcattr, const ImageArray *srcimages,
				  const ImageArray *srccluts, ImageAttribute *dstattr,
				  ImageArray *dstimages, ImageArray *dstcluts)
{
	if (!ImageOperation::Init(srcattr, srcimages, srccluts, dstattr, dstimages, dstcluts))
		return false;

	if (srcimages->GetArraySize() == ImagePoint(0, 0))
		return false;
	if (srcattr->m_mode != ImageMode_RGBA8888 || srcattr->m_order != ImageOrder_Linear)
		return false;
	ImageExtent extent = srcattr->m_extent;
	ImageExtent clip = srcattr->m_clip.extent;
	if (!(extent == clip))
		return false;

	dstattr->m_clutsize = 0;
	dstattr->m_mode = ImageMode_DXT5;
	dstattr->m_clutmode = ImageMode_NONE;
	dstattr->m_pitchalign = 1;
	dstattr->m_heightalign = 1;
	dstattr->m_extent.w = DxtExtent(dstattr->m_originalextent.w);
	dstattr->m_extent.h = DxtExtent(dstattr->m_originalextent.h);

	dstimages->Init(srcimages->GetArraySize());
	return true;
}

bool ConvertToDXT5Operation::OperateImage(const ImagePoint &index, const ImageAttribute *srcattr,
					  const ImageBuffer *srcimage, const ImageBuffer *srcclut,
					  ImageAttribute *dstattr, ImageBuffer *dstimage,
					  ImageArray *dstclut)
{
	if (dstimage->buff == NULL || dstimage->size < dstattr->GetMemSize(index.x))
		return false;

	int src_pitch = srcattr->GetMemPitch(index.x);
	int dst_pitch = dstattr->GetMemPitch(index.x);
	ImageExtent dst_extent = dstattr->GetExtent(index.x);
	ImageExtent src_extent = srcattr->GetExtent(index.x);

	return filter::ConvertMode(dstimage, srcimage, srcclut, dst_pitch, src_pitch, dst_extent,
				   src_extent, srcattr->m_mode, srcattr->m_format, ImageMode_DXT5,
				   m_type);
}

ConvertToDXT5Operation::ConvertToDXT5Operation(ImageConvertModeType type,
					       memory::HeapAllocator *allocator)
	: ImageOperation(allocator), m_type(type)
{
}

ConvertToDXT5Operation::~ConvertToDXT5Operation()
{
}
